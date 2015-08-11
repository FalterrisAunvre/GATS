#include "GUIInterface.h"

GUIInterface::GUIInterface()
{
	if (sqlite3_open("database.db", &_database) == SQLITE_OK)
	{
		sqlite3_exec(_database, "CREATE TABLE IF NOT EXISTS entries(id INTEGER, tag INTEGER);", nullptr, NULL, NULL);
		sqlite3_exec(_database, "CREATE TABLE IF NOT EXISTS tags(id INTEGER PRIMARY KEY, name TEXT);", nullptr, NULL, NULL);
		sqlite3_exec(_database, "CREATE TABLE IF NOT EXISTS files(id INTEGER PRIMARY KEY, filename TEXT, extension TEXT);", nullptr, NULL, NULL);
		sqlite3_exec(_database, "CREATE TABLE IF NOT EXISTS collections(id INTEGER, name TEXT, entry INTEGER);", nullptr, NULL, NULL);
	}
	else
	{
		system("CLS");
		printf("Could not open database 'database.db'.\nReason:'%s'.\n\nPress any key to terminate program...\n", sqlite3_errmsg(_database));
		system("PAUSE");
		exit(-1);
	}

	//_filterResultsValues.second = nullptr;

	_server = nullptr;
	_serverThread = nullptr;
	_bServerIsUp = false;
	_bServerThreadStop = false;
	_serverPort = 8008;

	_window = new Fl_Window(640, 480, GATS_VERSION);
	_window->resizable(false);
	_window->begin();
	{
		_tabs = new Fl_Tabs(10, 10, 300, 460);
		{
			_tabServer = new Fl_Group(20, 30, 290, 430, "Server");
			{
				_serverButton = new Fl_Light_Button(20, 40, 160, 30, "Toggle Server");
					_serverButton->callback(ToggleServer, this);
					_serverButton->color2(FL_GREEN);
				_portInput = new Fl_Int_Input(20, 80, 90, 30, "Server Port");
					_portInput->callback(ChangeServerPort, this);
					_portInput->when(FL_WHEN_CHANGED);
					_portInput->maximum_size(5);
					_portInput->value("8008");
					_portInput->align(FL_ALIGN_RIGHT);
				_editWhitelistBuffer = new Fl_Text_Buffer;
				_editWhitelist = new Fl_Text_Editor(20, 160, 280, 280, "Whitelisted IPs (blank for no whitelist)");
					_editWhitelist->buffer(_editWhitelistBuffer);
					_editWhitelist->cursor_style(Fl_Text_Editor::SIMPLE_CURSOR);
				_updateWhitelist = new Fl_Button(20, 440, 280, 30, "Update Whitelist");
					_updateWhitelist->callback(UpdateWhitelist, this);
			}
			_tabServer->end();

			_tabEntries = new Fl_Group(20, 30, 600, 430, "Entries");
			{
				_openFiles = new Fl_Button(20, 40, 280, 30, "Add Individual File(s)");
					_openFiles->callback(AddFiles, this);
				_openDirectory = new Fl_Button(20, 80, 280, 30, "Add Directory");
					_openDirectory->callback(AddDirectory, this);
				_filter = new Fl_Input(20, 120, 200, 30, "Filter");
					_filter->align(FL_ALIGN_RIGHT);
					_filter->when(FL_WHEN_ENTER_KEY_ALWAYS);
					_filter->callback(EntriesFilter, this);
				_filterResults = new Fl_Select_Browser(20, 160, 110, 300);
					_filterResults->callback(UpdateEntriesList, this);
				//_filterResults->when(FL_WHEN_)
				_openEntry = new Fl_Button(140, 160, 160, 30, "Open Entry");
					_openEntry->deactivate();
					_openEntry->callback(OpenFileWithDefaultProgram, this);
				_modifyTags = new Fl_Button(140, 200, 160, 30, "Modify Tags");
					_modifyTags->deactivate();
			}
			_tabEntries->end();

			_tabTags = new Fl_Group(20, 30, 600, 430, "Tags");
			{

			}
			_tabTags->end();

			_tabSql = new Fl_Group(20, 30, 600, 430, "SQL");
			{
				_sqlQueryResultsBuffer = new Fl_Text_Buffer;
				_sqlQueryResults = new Fl_Text_Display(20, 80, 280, 380);
					_sqlQueryResults->buffer(_sqlQueryResultsBuffer);
				_sqlQuery = new Fl_Input(20, 40, 280, 30);
					_sqlQuery->callback(AnyQuery, this);
					_sqlQuery->when(FL_WHEN_ENTER_KEY_ALWAYS);
			}
			_tabSql->end();
		}
		_tabs->end();

		_logBuffer = new Fl_Text_Buffer(1024 * 100);
		_log = new Fl_Text_Display(330, 30, 300, 440, "Log");
			_log->buffer(_logBuffer);
	}
	_window->end();
}

GUIInterface::~GUIInterface()
{
	sqlite3_close(_database);

	delete _portInput;
	delete _serverButton;
	delete _tabServer;

	delete _tabs;
	delete _window;
}

int GUIInterface::show()
{
	_window->show();
	return Fl::run();
}

void GUIInterface::logMessage(const char *str)
{
	_logBuffer->append(str);
	_log->scroll(10000000000, 0);
}

// Returns id of image in database, or -1 if it doesn't exist.
int GUIInterface::findFileInDatabase(const char *filename)
{
	auto callback = [](void* param, int argc, char *argv[], char *colname[]) -> int {
		int *ret = static_cast<int*>(param);
		*ret = atoi(argv[0]);
		return 0;
	};

	int id = -1;
	std::string sql = "SELECT DISTINCT id FROM files WHERE filename = '";
	sql += filename;
	sql += "'";

	queryDatabase(sql, callback, &id);
	return id;
}

bool GUIInterface::isFileInDatabase(const char *filename)
{
	return (findFileInDatabase(filename) != -1);
}

void GUIInterface::queryDatabase(std::string query, int callback(void* param, int argc, char *argv[], char *colname[]), void *userData = nullptr)
{
	std::lock_guard<std::mutex> lockGuard(_databaseMutex);
	printf("%s\n", query.c_str());
	sqlite3_exec(_database, query.c_str(), callback, userData, nullptr);
}

// Returns -1 if there are no entries.
int GUIInterface::getTopImageId()
{
	auto callback = [](void* param, int argc, char *argv[], char *colname[]) -> int {
		int *ret = static_cast<int*>(param);
		*ret = atoi(argv[0]);
		return 0;
	};

	int id = -1;
	queryDatabase("SELECT DISTINCT id FROM files ORDER BY id DESC LIMIT 1", callback, &id);
	return id;
}

void GUIInterface::addFile(const char *_path)
{
	using namespace std::tr2;
	sys::path path = _path;
	if (sys::exists(path))
	{
		// Load the file header into memory.
		size_t size = sys::file_size(path);
		FILE *file = std::fopen(_path, "r");
		if (file)
		{
			std::unique_ptr<byte[]> loadedFile(new byte[size]);
			std::fread(loadedFile.get(), size, 1, file);
			std::fclose(file);

			// Get the MD5 hash.
			std::string hashString;
			{
				md5wrapper *MD5 = new md5wrapper;
				hashString = MD5->getHashFromFile(_path);
				delete MD5;
			}
			//delete loadedFile;

			int findFile = findFileInDatabase(hashString.c_str());
			if (findFile == -1)
			{
				if (false /*Check for swf+video magic numbers*/)
				{
					std::stringstream ss;
					ss << "File '" << _path << "' is of an unsupported format.\n";
					logMessage(ss.str().c_str());
				}
				else
				{
					std::string ext = "";
					CxImage image;
					image.Load(_path);
					if (image.GetType() != CXIMAGE_FORMAT_UNKNOWN)
					{
						switch (image.GetType())
						{
						case CXIMAGE_FORMAT_BMP: ext = "bmp"; break;
						case CXIMAGE_FORMAT_GIF: ext = "gif"; break;
						case CXIMAGE_FORMAT_JP2: ext = "jp2"; break;
						case CXIMAGE_FORMAT_JPG: ext = "jpg"; break;
						case CXIMAGE_FORMAT_MNG: ext = "mng"; break;
						case CXIMAGE_FORMAT_PCX: ext = "pcx"; break;
						case CXIMAGE_FORMAT_PNG: ext = "png"; break;
						case CXIMAGE_FORMAT_PNM: ext = "pnm"; break;
						case CXIMAGE_FORMAT_PSD: ext = "psd"; break;
						case CXIMAGE_FORMAT_RAS: ext = "ras"; break;
						case CXIMAGE_FORMAT_RAW: ext = "raw"; break;
						case CXIMAGE_FORMAT_TGA: ext = "tga"; break;
						case CXIMAGE_FORMAT_TIF: ext = "tif"; break;
						case CXIMAGE_FORMAT_WBMP: ext = "wbmp"; break;
						case CXIMAGE_FORMAT_WMF: ext = "wmf"; break;
						}

						std::string basePath = hashString.substr(0, 2) + "/" + hashString.substr(2, 2) + "/" + hashString;
						std::string thumbPath = "preview/" + basePath + ".jpg";
						std::string imgPath = "images/" + basePath + "." + ext;
						// Creates the necessary directories
						{
							TCHAR thispath[512];
							GetModuleFileName(NULL, thispath, 512);
							PathRemoveFileSpec(thispath);
							sys::current_path(sys::path(thispath));
							sys::path tpath = "preview/" + hashString.substr(0, 2) + "/" + hashString.substr(2, 2) + "/", ipath = "images/" + hashString.substr(0, 2) + "/" + hashString.substr(2, 2) + "/";
							char absolute[_MAX_PATH];
							_fullpath(absolute, tpath.string().c_str(), _MAX_PATH);
							sys::create_directories(sys::path(absolute));
							_fullpath(absolute, ipath.string().c_str(), _MAX_PATH);
							sys::create_directories(sys::path(ipath));
						}
						const float FULLSIZE = 150.f;
						unsigned int width = image.GetWidth(), height = image.GetHeight();
						if (width > height)
						{
							height = (((float)height / (float)width)) * FULLSIZE;
							width = FULLSIZE;
						}
						else
						{
							width = ((float)(width / (float)height)) * FULLSIZE;
							height = FULLSIZE;
						}
						char absolute[_MAX_PATH];
						_fullpath(absolute, imgPath.c_str(), _MAX_PATH);
						image.Save(absolute, image.GetType());
						image.Resample(width, height, 0);
						_fullpath(absolute, thumbPath.c_str(), _MAX_PATH);
						image.Save(thumbPath.c_str(), CXIMAGE_FORMAT_JPG);

						std::stringstream sqlString;
						sqlString << "INSERT INTO files(filename, extension) VALUES('";
						sqlString << hashString << "','" << ext << "');";

						int newId = getTopImageId();
						if (newId == -1) newId = 1;
						else newId++;
						std::stringstream sqlString2;
						sqlString2 << "INSERT INTO entries(id, tag) VALUES(";
						sqlString2 << newId << ", -1);";

						std::lock_guard<std::mutex> lockGuard(_databaseMutex);
						char *err1 = new char[512], *err2 = new char[512];
						if (sqlite3_exec(_database, sqlString.str().c_str(), nullptr, nullptr, &err1) == SQLITE_OK)
						{
							if (sqlite3_exec(_database, sqlString2.str().c_str(), nullptr, nullptr, &err2) == SQLITE_OK)
							{
								std::stringstream ss;
								ss << "Added file '" << _path << "' with entry ID " << newId << ".\n";
								logMessage(ss.str().c_str());
								printf("%s\n", hashString.c_str());
							}
							else
							{
								std::stringstream ss;
								ss << "SQL error! Unable to add file '" << _path << "' to 'entries' database.\n\tSQL query: '" << sqlString2.str() << "'\n\tSQL error: '" << err2 << "'\n";
								logMessage(ss.str().c_str());
							}
						}
						else
						{
							std::stringstream ss;
							ss << "SQL error! Unable to add file '" << _path << "' to 'images' database.\n\tSQL query: '" << sqlString.str() << "'\n\tSQL error: '" << err1 << "'\n";
							logMessage(ss.str().c_str());
						}
					}
					else // Unknown/unsupported file type
					{
						std::stringstream ss;
						ss << "File '" << _path << "' is of an unsupported format.\n";
						logMessage(ss.str().c_str());
					}
				}
			}
			else // Hash collision found
			{
				std::stringstream ss;
				ss << "File '" << _path << "' already exists in the database with id " << findFile << ".\n";
				logMessage(ss.str().c_str());
			}
		}
		else // File doesn't exist
		{
			std::stringstream ss;
			ss << "File '" << _path << "' doesn't exist.\n";
			logMessage(ss.str().c_str());
		}
	}
	else // File doesn't exist
	{
		std::stringstream ss;
		ss << "File '" << _path << "' doesn't exist.\n";
		logMessage(ss.str().c_str());
	}
}

void GUIInterface::ToggleServer(Fl_Widget *widget, void *data)
{
	GUIInterface *inter = static_cast<GUIInterface*>(data);
	Fl_Light_Button *button = static_cast<Fl_Light_Button*>(widget);

	inter->_serverButton->deactivate();
	if (!inter->_bServerIsUp)
	{
		inter->_server = mg_create_server(inter, eventHandler);
		if (!inter->_server)
		{
			std::stringstream ss;
			ss << "Unable to start server.\n";
			inter->logMessage(ss.str().c_str());
			inter->_bServerIsUp = false;
		}
		else
		{
			char buf[8];
			sprintf(buf, "%u", inter->_serverPort);
			const char *msg = mg_set_option(inter->_server, "listening_port", buf);
			if (msg)
			{
				std::stringstream ss;
				ss << "Unable to set port to " << inter->_serverPort << ".\n";
				inter->logMessage(ss.str().c_str());
				inter->_bServerIsUp = false;
			}

			//serveArgs args{ &inter->_bServerThreadStop, inter->_server, inter, inter->_database };
			inter->_serverThread = new std::thread(serve, inter);
			
			std::stringstream ss;
			ss << "Server started on port " << inter->_serverPort << ".\n";
			inter->logMessage(ss.str().c_str());
			inter->_bServerIsUp = true;
		}
	}
	else
	{
		inter->_bServerThreadStop = true;	// Thread will cease when this is set to true
		inter->_serverThread->join();		// Wait up to 200ms for the thread to finish
		delete inter->_serverThread;		// Delete the thread
		inter->_bServerThreadStop = false;

		mg_destroy_server(&inter->_server);
		inter->_bServerIsUp = !(inter->_server == nullptr);

		std::stringstream ss;
		ss << "Server stopped.\n";
		inter->logMessage(ss.str().c_str());
	}
	inter->_serverButton->activate();

	if (inter->_bServerIsUp)
	{
		inter->_portInput->deactivate();
	}
	else
	{
		inter->_portInput->activate();
	}

	button->value(inter->_bServerIsUp);
}

void GUIInterface::ChangeServerPort(Fl_Widget* widget, void* data)
{
	GUIInterface *inter = static_cast<GUIInterface*>(data);
	Fl_Int_Input *input = static_cast<Fl_Int_Input*>(widget);

	int port = atoi(input->value());
	if (port >= 0 && port <= 65535)
	{
		inter->_serverPort = port;
	}
	else if (port < 0)
	{
		inter->_serverPort = 0;
		input->value("0");
	}
	else if (port > 65535)
	{
		inter->_serverPort = 65535;
		input->value("65535");
	}
}

void GUIInterface::UpdateWhitelist(Fl_Widget* widget, void* data)
{
	GUIInterface *inter = static_cast<GUIInterface*>(data);
	std::lock_guard<std::mutex>(inter->_serverMutex);

	char *text = inter->_editWhitelistBuffer->text();
	std::vector<std::string> wl;
	std::stringstream ss(text);
	free(text);

	std::string item;
	while (std::getline(ss, item, '\n')) {
		wl.push_back(item);
	}

	if (wl.size())
	{
		std::stringstream ss;
		ss << "Whitelist updated (" << wl.size() << " entries):\n";
		for (auto i : wl)
			ss << "\t- " << i << '\n';
		inter->logMessage(ss.str().c_str());
	}
	else
	{
		std::stringstream ss;
		ss << "Whitelist cleared.\n";
		inter->logMessage(ss.str().c_str());
	}
}

void GUIInterface::AddFiles(Fl_Widget* widget, void* data)
{
	GUIInterface *inter = static_cast<GUIInterface*>(data);
	const char* const formats[] = { "*.bmp", "*.jpg", "*.jpeg", "*.png", "*.gif" };
	const char* dialog = tinyfd_openFileDialog("Select File(s)",
		"",
		5,
		formats,
		"All Accepted Media Types",
		1);
	if (dialog)
	{
		std::vector<std::string> files;
		std::stringstream ss(dialog);

		std::string item;
		while (std::getline(ss, item, '|')) {
			files.push_back(item);
		}

		for (auto i : files)
			inter->addFile(i.c_str());
	}
}

void GUIInterface::AddDirectory(Fl_Widget* widget, void* data)
{

}

void GUIInterface::queryForTags(std::string text, std::vector<int> &results)
{
	unsigned int count = 0U;
	std::stringstream sqlQuery;

	std::vector<std::string> parameters;
	std::stringstream ss(text);
	std::string item;
	while (std::getline(ss, item, ' ')) {
		parameters.push_back(item);
	}

	bool bDescResults = true;
	for (auto i : parameters)
	{
		std::string str = i;
		if (str.find("id:") == 0)
		{
			str = str.substr(3);

			if (count > 0U) sqlQuery << " AND id IN (SELECT id FROM entries WHERE ";
			else sqlQuery << "SELECT DISTINCT id FROM entries WHERE ";

			if (str.find("all") == 0)
			{
				sqlQuery << "id >= 0";
			}
			else if (str.find(">=") == 0)
			{
				sqlQuery << "id >= " << str.substr(2);
			}
			else if (str.find(">") == 0)
			{
				sqlQuery << "id > " << str.substr(1);
			}
			else if (str.find("<=") == 0)
			{
				sqlQuery << "id <= " << str.substr(2);
			}
			else if (str.find("<") == 0)
			{
				sqlQuery << "id < " << str.substr(1);
			}
			else
			{
				sqlQuery << "id = " << str;
			}
			count++;
		}
		else if (str.find("order:") == 0)
		{
			str = str.substr(6);

			if (str.find("id_desc"))
			{
				bDescResults = false;
			}
			else if (str.find("id"))
			{
				bDescResults = true;
			}
		}
		else if (str.find("md5:") == 0)
		{
			str = str.substr(4);

			if (count > 0U) sqlQuery << " AND id IN (SELECT id FROM entries WHERE ";
			else sqlQuery << "SELECT DISTINCT id FROM entries WHERE ";

			sqlQuery << "id IN (SELECT id FROM files WHERE filename = '" << str << "')";
			count++;
		}
		else
		{
			if (count > 0U) sqlQuery << " AND id IN (SELECT id FROM entries WHERE ";
			else sqlQuery << "SELECT DISTINCT id FROM entries WHERE ";

			sqlQuery << "id IN (SELECT id FROM tags WHERE name = '" << str << "')";
			count++;
		}
	}

	if (count > 0U)
	{
		for (unsigned int i = 0; i < count - 1; i++)
		{
			sqlQuery << ")";
		}
		if (bDescResults) sqlQuery << " ORDER BY id DESC";
		else sqlQuery << " ORDER BY id ASC";

		auto callback = [](void* param, int argc, char *argv[], char *colname[]) -> int {
			std::vector<int> *ret = static_cast<std::vector<int>*>(param);
			for (int i = 0; i < argc; i++)
				ret->push_back(atoi(argv[i]));
			return 0;
		};

		// Populates the results list,
		//std::vector<int> results;
		printf("%s\n", sqlQuery.str().c_str());
		char *msg = nullptr;
		sqlite3_exec(_database, sqlQuery.str().c_str(), callback, &results, &msg);
		printf("%s\n", msg);
		delete[] msg;
	}
	else
	{
		sqlQuery << "SELECT id FROM entries ORDER BY id DESC";

		auto callback = [](void* param, int argc, char *argv[], char *colname[]) -> int {
			std::vector<int> *ret = static_cast<std::vector<int>*>(param);
			for (int i = 0; i < argc; i++)
				ret->push_back(atoi(argv[i]));
			return 0;
		};

		// Populates the results list,
		//std::vector<int> _results;
		sqlite3_exec(_database, sqlQuery.str().c_str(), callback, &results, nullptr);
	}
}

void GUIInterface::queryForEntryInfo(std::vector<int> &ids, std::vector<std::string> &filename, std::vector<std::string> &exts)
{
	struct res {
		std::vector<std::string> filename, ext;
	}results;

	auto callback = [](void* param, int argc, char *argv[], char *colname[]) -> int {
		res *r = static_cast<res*>(param);
		for (int i = 0; i < argc; i++)
		{
			if (strcmp("filename", colname[i]) == 0)
				r->filename.push_back(argv[i]);
			else if (strcmp("extension", colname[i]) == 0)
				r->ext.push_back(argv[i]);
		}
		return 0;
	};

	std::stringstream sql;
	sql << "SELECT filename,extension FROM files WHERE id IN (";
	for (int i = 0; i < ids.size(); i++)
	{
		sql << std::to_string(ids[i]);
		if (i != ids.size() - 1)
			sql << ", ";
		else
			sql << ")";
	}

	sqlite3_exec(_database, sql.str().c_str(), callback, &results, nullptr);
	filename = results.filename;
	exts = results.ext;
}

void GUIInterface::EntriesFilter(Fl_Widget* widget, void* data)
{
	GUIInterface *inter = static_cast<GUIInterface*>(data);
	Fl_Input *input = static_cast<Fl_Input*>(widget);

	std::string text = input->value();

	/*unsigned int count = 0U;
	std::stringstream sqlQuery;

	std::vector<std::string> parameters;
	std::stringstream ss(text);
	std::string item;
	while (std::getline(ss, item, ' ')) {
		parameters.push_back(item);
	}

	bool bDescResults = true;
	for (auto i : parameters)
	{
		std::string str = i;
		if (str.find("id:") == 0)
		{
			str = str.substr(3);

			if (count > 0U) sqlQuery << " AND id IN (SELECT id FROM entries WHERE ";
			else sqlQuery << "SELECT DISTINCT id FROM entries WHERE ";

			if (str.find("all") == 0)
			{
				sqlQuery << "id >= 0";
			}
			else if (str.find(">=") == 0)
			{
				sqlQuery << "id >= " << str.substr(2);
			}
			else if (str.find(">") == 0)
			{
				sqlQuery << "id > " << str.substr(1);
			}
			else if (str.find("<=") == 0)
			{
				sqlQuery << "id <= " << str.substr(2);
			}
			else if (str.find("<") == 0)
			{
				sqlQuery << "id < " << str.substr(1);
			}
			else
			{
				sqlQuery << "id = " << str;
			}
			count++;
		}
		else if (str.find("order:") == 0)
		{
			str = str.substr(6);

			if (str.find("id_desc"))
			{
				bDescResults = false;
			}
			else if (str.find("id"))
			{
				bDescResults = true;
			}
		}
		else if (str.find("md5:") == 0)
		{
			str = str.substr(4);

			if (count > 0U) sqlQuery << " AND id IN (SELECT id FROM entries WHERE ";
			else sqlQuery << "SELECT DISTINCT id FROM entries WHERE ";

			sqlQuery << "id IN (SELECT id FROM files WHERE filename = '" << str << "')";
			count++;
		}
		else
		{
			if (count > 0U) sqlQuery << " AND id IN (SELECT id FROM entries WHERE ";
			else sqlQuery << "SELECT DISTINCT id FROM entries WHERE ";

			sqlQuery << "id IN (SELECT id FROM tags WHERE name = '" << str << "')";
			count++;
		}
	}

	inter->_filterResults->clear();
	unsigned int resultCount = 0U;
	if (count > 0U)
	{
		for (unsigned int i = 0; i < count - 1; i++)
		{
			sqlQuery << ")";
		}
		if (bDescResults) sqlQuery << " ORDER BY id DESC";
		else sqlQuery << " ORDER BY id ASC";

		auto callback = [](void* param, int argc, char *argv[], char *colname[]) -> int {
			std::vector<int> *ret = static_cast<std::vector<int>*>(param);
			for (int i = 0; i < argc; i++)
				ret->push_back(atoi(argv[i]));
			return 0;
		};

		// Populates the results list,
		std::vector<int> results;
		inter->queryDatabase(sqlQuery.str(), callback, &results);
		resultCount = results.size();
		for (unsigned int i = 0U; i < results.size(); i++)
		{
			inter->_filterResults->add(std::to_string(results[i]).c_str());
		}
	}
	else
	{
		sqlQuery << "SELECT id FROM entries ORDER BY id DESC";

		auto callback = [](void* param, int argc, char *argv[], char *colname[]) -> int {
			std::vector<int> *ret = static_cast<std::vector<int>*>(param);
			for (int i = 0; i < argc; i++)
				ret->push_back(atoi(argv[i]));
			return 0;
		};

		// Populates the results list,
		std::vector<int> results;
		inter->queryDatabase(sqlQuery.str(), callback, &results);
		resultCount = results.size();
		for (unsigned int i = 0U; i < results.size(); i++)
		{
			inter->_filterResults->add(std::to_string(results[i]).c_str());
		}
	}
	*/

	std::vector<int> results;
	inter->queryForTags(text, results);
	
	inter->_filterResults->clear();
	for (unsigned int i = 0U; i < results.size(); i++)
	{
		inter->_filterResults->add(std::to_string(results[i]).c_str());
	}

	std::stringstream msg;
	msg << results.size() << " results returned from query '" << text << "'.\n";
	inter->logMessage(msg.str().c_str());
}

void GUIInterface::AnyQuery(Fl_Widget* widget, void* data)
{
	GUIInterface *inter = static_cast<GUIInterface*>(data);
	Fl_Input *input = static_cast<Fl_Input*>(widget);

	inter->_sqlQueryResultsBuffer->text("");
	auto callback = [](void* param, int argc, char *argv[], char *colname[]) -> int {
		Fl_Text_Buffer *inter = static_cast<Fl_Text_Buffer*>(param);
		for (int i = 0; i < argc; i++)
		{
			std::stringstream ss;
			ss << "Col: '" << colname[i] << "', Value: " << argv[i] << "\n";
			inter->append(ss.str().c_str());
		}
		return 0;
	};

	std::string text = input->value();
	inter->queryDatabase(text.c_str(), callback, inter->_sqlQueryResultsBuffer);
}

void GUIInterface::UpdateEntriesList(Fl_Widget* widget, void* data)
{
	GUIInterface *inter = static_cast<GUIInterface*>(data);
	int line = inter->_filterResults->value();
	if (line == 0)
	{
		inter->_openEntry->deactivate();
		inter->_modifyTags->deactivate();
	}
	else
	{
		inter->_openEntry->activate();
		inter->_modifyTags->activate();

		int entryID = atoi(inter->_filterResults->text(line));
	}
}

void GUIInterface::OpenFileWithDefaultProgram(Fl_Widget* widget, void* data)
{
	GUIInterface *inter = static_cast<GUIInterface*>(data);
	//std::string *str = static_cast<std::string*>(data);
	std::pair<std::string, std::string> params;

	auto callback = [](void* param, int argc, char *argv[], char *colname[]) -> int {
		std::pair<std::string, std::string> *params = static_cast<std::pair<std::string, std::string>*>(param);
		for (int i = 0; i < argc; i++)
		{
			if (strcmp(colname[i], "filename") == 0) params->first = argv[i];
			else if (strcmp(colname[i], "extension") == 0) params->second = argv[i];
		}
		return 0;
	};

	std::stringstream ss;
	ss << "SELECT filename, extension FROM files WHERE id = " << atoi(inter->_filterResults->text(inter->_filterResults->value()));

	inter->queryDatabase(ss.str(), callback, &params);

	// A double-check to make sure that it got both of the parameters were found.
	if (params.first.size() > 0 && params.second.size() > 0)
	{
		std::stringstream ss2;
		ss2 << "images/" << params.first.substr(0, 2) << "/" << params.first.substr(2, 2) << "/" << params.first << "." << params.second;
		char absolute[_MAX_PATH];
		if (_fullpath(absolute, ss2.str().c_str(), _MAX_PATH) != nullptr)
		{
			printf("%s\n", absolute);
			/*HINSTANCE res =*/ ShellExecute(0, "open", absolute, 0, 0, SW_SHOW);
		}
	}
}

void *GUIInterface::serve(GUIInterface *inter)
{
	//serveArgs *args = (serveArgs*)argsl;

	while (!(inter->_bServerThreadStop)) mg_poll_server(inter->_server, 200);
	return nullptr;
}

int GUIInterface::eventHandler(mg_connection *conn, mg_event event)
{
	std::vector<std::string> path;
	switch (event)
	{
	case MG_POLL: return MG_FALSE;
	case MG_AUTH: return MG_TRUE;
	case MG_REQUEST:
		byte buffer[1024];
		int length;
		std::string newUri = conn->uri;
		newUri = newUri.substr(1); // Get rid of the first slash

		std::vector<std::string> path;
		splitString(newUri, '/', path);
		if (path.size() == 0)
		{
			mg_send_header(conn, "Content-Type", "text/plain");
			std::stringstream html;
			html << "This is going to be the home screen.\nIncoming IP: " << conn->remote_ip << "\nLocal IP: " << conn->local_ip;
			mg_printf_data(conn, html.str().c_str());
			return MG_TRUE;
		}
		else if (path[0] == "admin")
		{
			if (!(strcmp(conn->remote_ip, "127.0.0.1") == 0))
			{
				mg_send_header(conn, "Content-Type", "text/plain");
				mg_printf_data(conn, "You are not authorized to view this page.");
				return MG_TRUE;
			}
			mg_send_header(conn, "Content-Type", "text/plain");
			mg_printf_data(conn, "Admin page here.");
			return MG_TRUE;
		}
		else if (path[0] == "post")
		{
			mg_send_header(conn, "Content-Type", "text/html");
			std::string html;
			generateHtml(conn, (GUIInterface*)(conn->server_param), "post", html);

			mg_printf_data(conn, html.c_str());
			return MG_TRUE;
		}
		else if (path[0] == "images" || path[0] == "preview" || path[0] == "movies")
		{
			FILE *f = fopen(newUri.c_str(), "rb");
			if (f)
			{
				std::string ext;
				getExtension(newUri, ext);
				std::string mime = getMimeType(ext.c_str());
				mg_send_header(conn, "Content-Type", mime.c_str());
				length = fread(buffer, 1, sizeof(buffer), f);
				while (length > 0)
				{
					mg_send_data(conn, buffer, length);
					length = fread(buffer, 1, sizeof(buffer), f);
				}
				fclose(f);
				return MG_TRUE;
			}
			else
			{
				mg_send_header(conn, "Content-Type", "text/plain");
				mg_printf_data(conn, "Error: couldn't find file [%s].\n", newUri.c_str());
				return MG_TRUE;
			}
		}
	}
}

void GUIInterface::splitString(const std::string str, char delimiter, std::vector<std::string> &list)
{
	std::stringstream ss(str);
	std::string item;
	while (std::getline(ss, item, delimiter)) {
		list.push_back(item);
	}
}

void GUIInterface::getExtension(const std::string str, std::string &ext)
{
	ext = str.substr(str.find_last_of('.'));
	ext = ext.substr(1);
}

const char *GUIInterface::getMimeType(std::string ext)
{
	if (ext == "bmp") return "image/bmp";
	else if (ext == "gif") return "image/gif";
	else if (ext == "jp2") return "image/jp2";
	else if (ext == "jpg") return "image/jpeg";
	else if (ext == "mng") return "video/x-mng";
	else if (ext == "pcx") return "image/x-pcx";
	else if (ext == "png") return "image/png";
	else if (ext == "pnm") return "image/x-portable-bitmap";
	else if (ext == "psd") return "image/vnd.adobe.photoshop";
	else if (ext == "ras") return "image/x-cmu-raster";
	else if (ext == "raw") return "image/raw";
	else if (ext == "tga") return "image/x-targa";
	else if (ext == "tif") return "image/tiff";
	else if (ext == "wbmp") return "image/vnd.wap.wbmp";
	else if (ext == "wmf") return "application/x-msmetafile";
	else return "text/plain";
}

void GUIInterface::generateTagSearchQuery(sqlite3 *sql, std::string &queryStr, std::vector<std::string> &tags)
{
	std::vector<int> ids;
	tagIdLookup(sql, tags, ids);

	int count = tags.size();
	queryStr = "";
	if (count >= 1)
	{
		while (count > 0)
		{
			queryStr += "(SELECT DISTINCT id FROM entries WHERE entries.tag = ";
			queryStr += ids[count - 1];
			count--;
			if (count > 0) queryStr += "AND entries.id IN ";
		}
		for (int i = 0; i < tags.size(); i++) queryStr += ")";
	}
}

// If an id is -1, that means it doesn't exist in the database.
void GUIInterface::tagIdLookup(sqlite3 *sql, std::vector<std::string> &tags, std::vector<int> &ids)
{
	struct predResults
	{
		int id;
		predResults()
		{
			id = -1;
		}
	}results;

	struct predicate
	{
		static int func(void *param, int argc, char **argv, char **colname)
		{
			predResults *results = static_cast<predResults*>(param);
			results->id = static_cast<unsigned int>(atoi(argv[0]));
			return 0;
		}
	};

	for (auto i : tags)
	{
		std::string str = "SELECT id FROM tags WHERE name = ";
		str += i;
		sqlite3_exec(sql, str.c_str(), predicate::func, &results, nullptr);
		ids.push_back(results.id);
	}
}

void GUIInterface::tagIdLookup(sqlite3 *sql, std::string &tag, int &id)
{
	auto callback = [](void* param, int argc, char *argv[], char *colname[]) -> int {
		int *i = static_cast<int*>(param);
		*i = atoi(argv[0]);
		return 0;
	};

	int toReturn = -1;
	std::stringstream query;
	query << "SELECT id FROM tags WHERE name = '" << tag << "'";
	sqlite3_exec(sql, query.str().c_str(), callback, &toReturn, nullptr);
	id = toReturn;
}

void GUIInterface::generateHtml(mg_connection *conn, GUIInterface *inter, std::string sub, std::string &html)
{
	std::stringstream htmlBuf;

	if (sub == "post")
	{
		char get[256];
		std::vector<int> results;
		// -2 if buffer too small, -1 if not found
		int getResult = mg_get_var(conn, "tags", get, 256);
		if (getResult == -1)
		{
			inter->queryForTags("", results);
		}
		else
		{
			inter->queryForTags(get, results);
		}

		std::vector<std::string> filenames, extensions;
		inter->queryForEntryInfo(results, filenames, extensions);

		htmlBuf << "<html>";
		for (int i = 0; i < filenames.size(); i++)
		{
			std::string &str = filenames[i];
			htmlBuf << "sus <img src=\"../preview/" << str.substr(0, 2) << "/" << str.substr(2, 2) << "/" << str << ".jpg\"><a href=\"../images/" << str.substr(0, 2) << "/" << str.substr(2, 2) << "/" << str << "." << extensions[i] << "\"/></img>";
		}
		htmlBuf << "</html>";
	}

	html = htmlBuf.str();
}