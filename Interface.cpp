#include "Interface.h"

Interface::Interface()
{
	rlutil::hidecursor();
	_page = 1;

	_states.push(State_Main);
	_message = "Welcome to " GATS_VERSION "!";

	_server = nullptr;
	_bServerThreadStop = false;
	_bServerIsUp = false;
	_serverPort = 8008;

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
		_getch();
		exit(-1);
	}
}

Interface::~Interface()
{
	sqlite3_close(_database);
	if (_bServerIsUp)
		stopServer();
}

int Interface::_countTags()
{
	struct predResults
	{
		int count;
		predResults()
		{
			count = 0U;
		}
	}results;

	struct predicate
	{
		static int func(void *param, int argc, char **argv, char **colname)
		{
			predResults *results = static_cast<predResults*>(param);
			results->count = argc;
			return 0;
		}
	};

	std::string sqlString = "SELECT DISTINCT id FROM tags";

	sqlite3_exec(_database, sqlString.c_str(), predicate::func, &results, nullptr);
	return results.count;
}

unsigned int Interface::_nextEntryId()
{
	struct predResults
	{
		unsigned int id;
		predResults()
		{
			id = 1U;
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

	std::string sqlString = "SELECT DISTINCT id FROM entries ORDER BY id DESC LIMIT 1";

	sqlite3_exec(_database, sqlString.c_str(), predicate::func, &results, nullptr);
	return results.id;
}

void Interface::_splitString(const std::string str, char delimiter, std::vector<std::string> &list)
{
	std::stringstream ss(str);
	std::string item;
	while (std::getline(ss, item, delimiter)) {
		list.push_back(item);
	}
}

void Interface::getInput()
{
	std::string str;
	//std::cin >> str;
	str += _getch();

	if (str == "0" && _states.size() > 1)
		_states.pop();
	else if (str == "X")
		_states.push(State_Exit);
	else
	{
		bool error = false;
		switch (_states.top())
		{
		case State_Main:
			if (str == "1") _states.push(State_Database);
			else if (str == "2") _states.push(State_Server);
			else if (str == "3") _states.push(State_Lua);
			else error = true;
			break;
		case State_Database:
			if (str == "1") _states.push(State_Files);
			else if (str == "2") _states.push(State_Collections);
			else if (str == "3") _states.push(State_Tags);
			else error = true;
			break;
		case State_Server:
			if (str == "1")
			{
				if (!_bServerIsUp)
				{
					_bServerIsUp = startServer();
					if (!_bServerIsUp)
						_message = "Unable to start server!";
				}
				else
				{
					_bServerIsUp = !stopServer();
					if (_bServerIsUp)
						_message = "Unable to stop server!";
				}
			}
			else if (str == "2")
			{
				if (!_bServerIsUp)
				{
					std::string newPort;
					std::cout << "New server port: ";
					//rlutil::mvreadString()
					std::cin >> newPort;
					bool good = true;
					for (int i = 0; i < newPort.length(); i++)
					{
						if (!isdigit(newPort[i]))
						{
							_message = "'" + newPort + "' is not a valid port number.";
							good = false;
							break;
						}
					}
					if (good)
					{
						int newPortInt = atoi(newPort.c_str());
						if (newPortInt < 1 || newPortInt > 65535)
						{
							_message = "'" + newPort + "' is outside the port range (1-65535).";
						}
						else
						{
							_serverPort = newPortInt;
						}
					}
				}
			}
			else error = true;
			break;
		case State_Files:
			if (str == "1")
			{
				int tags = tinyfd_messageBox("Add tags?",
					"Would you like to add tags after selecting the entries?",
					"yesno",
					"question",
					0);

				const char* const formats[] = { "*.bmp", "*.jpg", "*.jpeg", "*.png", "*.gif" };
				const char* dialog = tinyfd_openFileDialog("Select File(s)",
					"",
					5,
					formats,
					"All Media Types",
					1);
				if (dialog)
				{
					std::string files = dialog;
					std::vector<std::string> filelist;
					_splitString(files, '|', filelist);
					int errorCount = 0;
					for (auto i : filelist)
					{
						if (addEntry(i.c_str()) != Error_None)
							errorCount++;
					}
					if (errorCount > 0)
					{
						system("CLS");
						printf("%d entries could not be added!\n");
						system("PAUSE");
					}
				}
			}
			else if (str == "2")
			{
				const char* dialog = tinyfd_selectFolderDialog("Select Folder", "");
				if (dialog)
				{
					addDirectory(dialog);
				}
			}
			else error = true;
			break;
		case State_Collections:
			if (str == "1")
			{
				rlutil::cls();
				int i = 0, input = -2;
				rlutil::mvprintf(1, 1, "Enter a name for the new collection:");
				rlutil::showcursor();
				std::string name = rlutil::mvreadString(1, 2);
				rlutil::mvprintf(1, 3, "Enter the IDs of each entry in the collection (0 to finish, -1 to cancel):");
				std::vector<int> ids;
				while (input != 0 && input != -1)
				{
					input = rlutil::mvreadInt(1, 4 + i);
					if (input != 0 && input != -1)
					{
						ids.push_back(input);
						i++;
					}
				}
				rlutil::hidecursor();
			}
		}
		if (error) _message = "Invalid option '" + str + "'.";
	}
}

void Interface::draw()
{
	rlutil::cls();
	if (_message.length() > 0)
	{
		rlutil::mvprintf(1, rlutil::trows(), "%s", _message.c_str());
		_message = "";
	}

	rlutil::mvprintf((rlutil::tcols() / 2) - (strlen(GATS_VERSION) / 2), 2, GATS_VERSION);

	switch (_states.top())
	{
	case State_Main:
		rlutil::mvprintf((rlutil::tcols() / 2) - (strlen(GATS_VERSION) / 2), 2, GATS_VERSION);
		rlutil::mvprintf(1, 4, "Main Menu");
		rlutil::mvprintf(3, 5, "1. Database Management");
		rlutil::mvprintf(3, 6, "2. Server Management");
		rlutil::mvprintf(3, 7, "3. Lua Scripts");
		rlutil::mvprintf(3, 9, "X. Exit");
		break;
	case State_Database:
		rlutil::mvprintf(1, 4, "Main Menu > Database Management");
		rlutil::mvprintf(3, 5, "1. Add/Remove Files");
		rlutil::mvprintf(3, 6, "2. Create/Delete Collections");
		rlutil::mvprintf(3, 7, "3. Add/Remove Tags");
		rlutil::mvprintf(3, 8, "4. View File List");
		rlutil::mvprintf(3, 10, "0. Back");
		rlutil::mvprintf(3, 11, "X. Exit");
		break;
	case State_Server:
		rlutil::mvprintf(1, 4, "Main Menu > Server Management");
		rlutil::mvprintf(3, 5, "1. Toggle Server (Server Status: %s)", (_bServerIsUp ? "Up" : "Offline"));
		if (_bServerIsUp) rlutil::setColor(rlutil::DARKGREY);
		rlutil::mvprintf(3, 6, "2. Set Server Port (Current: %d)", _serverPort);
		if (_bServerIsUp) rlutil::setColor(rlutil::GREY);
		rlutil::mvprintf(3, 8, "0. Back");
		rlutil::mvprintf(3, 9, "X. Exit");
		break;
	case State_Lua:
		rlutil::mvprintf(1, 4, "Main Menu > Lua Scripts");
		rlutil::mvprintf(3, 5, "Feature under construction!");
		rlutil::mvprintf(3, 7, "0. Back");
		rlutil::mvprintf(3, 8, "X. Exit");
		break;
	case State_Files:
		rlutil::mvprintf(1, 4, "Main Menu > Database Management > Add/Remove Files");
		rlutil::mvprintf(3, 5, "1. Add Individual Files");
		rlutil::mvprintf(3, 6, "2. Add File Directory");
		rlutil::mvprintf(3, 7, "3. Remove Files by ID");
		rlutil::mvprintf(3, 8, "4. Remove Files by Tag");
		rlutil::mvprintf(3, 10, "0. Back");
		rlutil::mvprintf(3, 11, "X. Exit");
		break;
	case State_Collections:
		rlutil::mvprintf(1, 4, "Main Menu > Database Management > Create/Delete Collection");
		rlutil::mvprintf(3, 5, "1. Create New Collection");
		rlutil::mvprintf(3, 6, "2. Delete Collection");
		rlutil::mvprintf(3, 8, "0. Back");
		rlutil::mvprintf(3, 9, "X. Exit");
		break;
	}
}

bool Interface::isGood()
{
	return _states.size() > 0 && _states.top() != State_Exit;
}

bool Interface::startServer()
{
	_server = mg_create_server(_database, Server::eventHandler);
	if (!_server)
		return false;

	char buf[8];
	sprintf(buf, "%u", _serverPort);
	const char *msg = mg_set_option(_server, "listening_port", buf);
	if (msg)
	{
		_message = "Unable to start server! Message: ";
		_message += msg;

		return false;
	}

	Server::serveArgs args{ &_bServerThreadStop, _server };
	_serverThread = new std::thread(Server::serve, args);

	return true;
}

bool Interface::stopServer()
{
	_bServerThreadStop = true;	// Thread will cease when this is set to true
	_serverThread->join();		// Wait up to 200ms for the thread to finish
	delete _serverThread;		// Delete the thread
	_bServerThreadStop = false;

	mg_destroy_server(&_server);
	return _server == nullptr;
}

bool Interface::addTag(const char* name)
{
	std::string sqlString = "INSERT INTO tags(name) VALUES(\"";
	sqlString += name;
	sqlString += "\");";

	bool ret = (sqlite3_exec(_database, sqlString.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK);
	if (!ret)
	{
		_message = "Couldn't add tag '";
		_message += name;
		_message += "'.";
	}
	return ret;
}

// Returns false if tag doesn't exist, true if it does.
// retId holds the tag's ID.
bool Interface::findTag(const char *name, int *retId)
{
	struct predResults
	{
		bool bSuccess;
		int id;
		predResults()
		{
			bSuccess = false;
			id = -1;
		}
	}results;

	struct predicate
	{
		static int func(void *param, int argc, char **argv, char **colname)
		{
			for (int i = 0; i < argc; i++)
			{
				printf("#%d: %s = %s\n", i, colname[i], argv[i]);
			}
			predResults *results = static_cast<predResults*>(param);
			results->bSuccess = (argc == 1);
			return 0;
		}
	};

	std::string sqlString = "SELECT id FROM tags WHERE name = '";
	sqlString += name;
	sqlString += "'";

	sqlite3_exec(_database, sqlString.c_str(), predicate::func, &results, nullptr);
	//std::string err = sqlite3_errmsg(_database);

	if (retId) *retId = results.id;
	return results.bSuccess;
}

bool Interface::findImage(const char *name, int *retId)
{
	struct predResults
	{
		bool bSuccess;
		int id;
		predResults()
		{
			bSuccess = false;
			int id = -1;
		}
	}results;

	struct predicate
	{
		static int func(void *param, int argc, char **argv, char **colname)
		{
			predResults *results = static_cast<predResults*>(param);
			results->bSuccess = (argc == 1);
			results->id = atoi(argv[0]);
			return 0;
		}
	};

	std::string sqlString = "SELECT DISTINCT id FROM images WHERE filename = '";
	sqlString += name;
	sqlString += "'";

	sqlite3_exec(_database, sqlString.c_str(), predicate::func, &results, nullptr);

	if (retId) *retId = results.id;
	return results.bSuccess;
}

bool Interface::addDirectory(const char* _path)
{
	using namespace std::tr2;
	sys::path path = _path;
	std::vector<std::string> entryList;
	if (sys::exists(path) && sys::is_directory(path))
	{
		sys::recursive_directory_iterator end;
		for (sys::recursive_directory_iterator iter(path); iter != end; iter++)
		{
			if (sys::is_regular_file(iter->status()))
			{
				std::vector<std::string> formats = { ".bmp", ".jpg", ".jpeg", ".png", ".gif" };
				std::string ext = sys::extension(iter->path());
				for (auto i : formats)
				{
					if (i == ext)
					{
						entryList.push_back(iter->path());
						break;
					}
				}
			}
		}
	}
	for (int i = 0; i < entryList.size(); i++)
	{
		system("CLS");
		printf("Entry %d/%d\n+%s\n", i+1, entryList.size(), entryList[i].c_str());
		addEntry(entryList[i].c_str());
	}
	system("PAUSE");
	return true;
}

unsigned int Interface::addEntry(const char* _path)
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
				/*CryptoPP::MD5 md5;
				byte digest[CryptoPP::MD5::DIGESTSIZE];
				md5.CalculateDigest(digest, loadedFile.get(), size);

				CryptoPP::HexEncoder hex;
				hex.Attach(new CryptoPP::StringSink(hashString));
				hex.Put(digest, sizeof(digest));
				hex.MessageEnd();
				//delete sink;

				for (int i = 0; i < hashString.length(); i++)
				{
					hashString[i] = tolower(hashString[i]);
				}*/
				md5wrapper *MD5 = new md5wrapper;
				hashString = MD5->getHashFromFile(_path);
				delete MD5;
			}
			//delete loadedFile;

			if (!findImage(hashString.c_str()))
			{
				if (false /*Check for swf+video magic numbers*/)
				{
					return Error_UnsupportedFormat;
				}
				else
				{
					std::string ext = "";
					CxImage image;

					/*int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, _path, strlen(_path), NULL, 0);
					std::wstring _wpath(sizeNeeded, 0);
					MultiByteToWideChar(CP_UTF8, 0, _path, strlen(_path), &_wpath[0], sizeNeeded);
					*/
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
							image.Resample(width, height);
							image.Save(thumbPath.c_str(), CXIMAGE_FORMAT_JPG);

							std::string sqlString = "INSERT INTO images(filename, extension) VALUES(\"";
							sqlString += hashString;
							sqlString += "\",\"";
							sqlString += ext;
							sqlString += "\");";
							if (sqlite3_exec(_database, sqlString.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK)
								return Error_None;
							else
								return Error_SQLiteError;
					}
					else // Unknown/unsupported file type
					{
						return Error_UnsupportedFormat;
					}
				}
			}
			else // Hash collision found
			{
				return Error_HashCollision;
			}
		}
		else // File doesn't exist
		{
			return Error_FileDoesntExist;
		}
	}
	else // File deosn't exist
	{
		return Error_FileDoesntExist;
	}
}

unsigned int Interface::addEntry(const char* _path, std::vector<std::string> &_tags)
{
	return 0;
}