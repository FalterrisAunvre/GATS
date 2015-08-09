#include "GUIInterface.h"

GUIInterface::GUIInterface()
{
	if (sqlite3_open("database.db", &_database) == SQLITE_OK)
	{
		sqlite3_exec(_database, "CREATE TABLE IF NOT EXISTS entries(id INTEGER, tag INTEGER);", nullptr, NULL, NULL);
		sqlite3_exec(_database, "CREATE TABLE IF NOT EXISTS tags(id INTEGER PRIMARY KEY, name TEXT);", nullptr, NULL, NULL);
		sqlite3_exec(_database, "CREATE TABLE IF NOT EXISTS images(id INTEGER PRIMARY KEY, filename TEXT, extension TEXT);", nullptr, NULL, NULL);
		sqlite3_exec(_database, "CREATE TABLE IF NOT EXISTS collections(id INTEGER, name TEXT, entry INTEGER);", nullptr, NULL, NULL);
	}
	else
	{
		system("CLS");
		printf("Could not open database 'database.db'.\nReason:'%s'.\n\nPress any key to terminate program...\n", sqlite3_errmsg(_database));
		system("PAUSE");
		exit(-1);
	}

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
			}
			_tabEntries->end();

			_tabTags = new Fl_Group(20, 30, 600, 430, "Tags");
			{

			}
			_tabTags->end();

			_tabSql = new Fl_Group(20, 30, 600, 430, "SQL");
			{

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
	std::string sql = "SELECT DISTINCT id FROM images WHERE filename = '";
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
	sqlite3_exec(_database, query.c_str(), callback, userData, nullptr);
}

int GUIInterface::getTopImageId()
{
	auto callback = [](void* param, int argc, char *argv[], char *colname[]) -> int {
		int *ret = static_cast<int*>(param);
		*ret = atoi(argv[0]);
		return 0;
	};

	int id = 0;
	queryDatabase("SELECT DISTINCT id FROM images ORDER BY id DESC LIMIT 1", callback, &id);
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
					ss << "File '" << _path << "' is of an unsupported format.";
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
							sys::create_directories(tpath);
							sys::create_directories(ipath);
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
							width = ((float)(width / height)) * FULLSIZE;
							height = FULLSIZE;
						}
						image.Save(imgPath.c_str(), image.GetType());
						image.Resample(width, height, 0);
						image.Save(thumbPath.c_str(), CXIMAGE_FORMAT_JPG);

						std::stringstream sqlString;
						sqlString << "INSERT INTO images(filename, extension) VALUES('";
						sqlString << hashString << "','" << ext << "');";

						int newId = getTopImageId();
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
		inter->_server = mg_create_server(inter->_database, Server::eventHandler);
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
				ss << "Unable to set port to " << inter->_serverPort << "!\n";
				inter->logMessage(ss.str().c_str());
				inter->_bServerIsUp = false;
			}

			Server::serveArgs args{ &inter->_bServerThreadStop, inter->_server };
			inter->_serverThread = new std::thread(Server::serve, args);

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