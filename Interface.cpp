#include "Interface.h"

Interface::Interface()
{
	_states.push(State_Main);
	_message = "Welcome to GATS " GATS_VERSION "!";

	_bServerIsUp = false;
	_serverPort = 8008;

	if (sqlite3_open("database.db", &_database) == SQLITE_OK)
	{
		sqlite3_exec(_database, "CREATE TABLE IF NOT EXISTS entries(id INTEGER, tag INTEGER);", nullptr, NULL, NULL);
		sqlite3_exec(_database, "CREATE TABLE IF NOT EXISTS tags(id INTEGER PRIMARY KEY, name TEXT);", nullptr, NULL, NULL);
		sqlite3_exec(_database, "CREATE TABLE IF NOT EXISTS images(id INTEGER PRIMARY KEY, filename TEXT, extension TEXT);", nullptr, NULL, NULL);
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
			if (str == "1") _bServerIsUp = !_bServerIsUp;
			else if (str == "2")
			{
				std::string newPort;
				std::cout << "New server port: ";
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
			break;
		case State_Files:
			const char* const formats[] = { "*.bmp", "*.jpg", "*.jpeg", "*.png", "*.gif" };
			std::string files = tinyfd_openFileDialog("Select File(s)",
				"",
				5,
				formats,
				"All Media Types",
				1);
			std::vector<std::string> filelist;
			_splitString(files, '|', filelist);
			int errorCount = 0;
			for (auto i : filelist)
			{
				if (!addEntry(i.c_str()))
					errorCount++;
			}
			if (errorCount > 0)
			{
				system("CLS");
				printf("%d entries could not be added!\n");
				system("PAUSE");
			}
			break;
		}
		if (error) _message = "Invalid option '" + str + "'.";
	}
}

void Interface::draw()
{
	system("CLS");
	
	if (_message.length() > 0)
	{
		std::cout << _message << std::endl;
		_message = "";
	}
	else std::cout << std::endl;

	switch (_states.top())
	{
	case State_Main:
		std::cout << "Main Menu" << std::endl
			<< "Options:" << std::endl
			<< "  1. Database Management" << std::endl
			<< "  2. Server Management" << std::endl
			<< "  3. Lua Scripts" << std::endl
			<< std::endl << "  X. Exit" << std::endl;
		break;
	case State_Database:
		std::cout << "Database Management" << std::endl
			<< "Options:" << std::endl
			<< "  1. Add/Remove Files" << std::endl
			<< "  2. Create/Delete Collections" << std::endl
			<< "  3. Add/Remove Tags" << std::endl
			<< "  4. Dump File List To Clipboard" << std::endl
			<< "  5. Dump Tag List To Clipboard" << std::endl
			<< "  6. Dump Collection List To Clipboard" << std::endl
			<< std::endl << "  0. Back" << std::endl << "  X. Exit" << std::endl;
		break;
	case State_Server:
		std::cout << "Server Management" << std::endl
			<< "Options:" << std::endl
			<< "  1. Toggle Server (Server Status: " << (_bServerIsUp ? "Up" : "Offline") << ")" << std::endl
			<< "  2. Set Server Port (Current: " << _serverPort << ")" << std::endl
			<< std::endl << "  0. Back" << std::endl << "  X. Exit" << std::endl;
		break;
	case State_Lua:
		std::cout << "Lua Scripts" << std::endl
			<< "Feature under construction!" << std::endl
			<< std::endl << "  0. Back" << std::endl << "  X. Exit" << std::endl;
		break;
	case State_Files:
		std::cout << "Add/Remove Files" << std::endl
			<< "Options:" << std::endl
			<< "  1. Add Individual Files" << std::endl
			<< "  2. Add File Directory" << std::endl
			<< "  3. Remove Files by ID" << std::endl
			<< "  4. Remove Files by Tag" << std::endl
			<< std::endl << "  0. Back" << std::endl << "  X. Exit" << std::endl;
	}
}

bool Interface::isGood()
{
	return _states.size() > 0 && _states.top() != State_Exit;
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
			int id = -1;
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
			for (int i = 0; i < argc; i++)
			{
				printf("#%d: %s = %s\n", i, colname[i], argv[i]);
			}
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

bool Interface::addEntry(const char* _path)
{
	using namespace std::tr2;
	sys::path path = _path;
	if (sys::exists(path))
	{
		// Load the file header into memory.
		size_t size = sys::file_size(path);
		byte *loadedFile = reinterpret_cast<byte*>(::operator new(size));
		FILE *file = nullptr;
		fopen_s(&file, _path, "r");
		fread_s(loadedFile, size, size, 1, file);
		fclose(file);

		// Get the MD5 hash.
		std::string hashString;
		{
			CryptoPP::Weak1::MD5 md5;
			byte digest[CryptoPP::MD5::DIGESTSIZE];
			md5.CalculateDigest(digest, loadedFile, size);

			CryptoPP::HexEncoder hex;
			hex.Attach(new CryptoPP::StringSink(hashString));
			hex.Put(digest, sizeof(digest));
			hex.MessageEnd();
			//delete sink;

			for (int i = 0; i < hashString.length(); i++)
			{
				hashString[i] = tolower(hashString[i]);
			}
		}
		delete loadedFile;

		/*
		// Determine the file's type based on it's magic number.
		std::string ext = "";
		uint16_t *r1 = reinterpret_cast<uint16_t*>(loadedFile);
		uint32_t *r2 = reinterpret_cast<uint32_t*>(loadedFile);
		uint64_t *r3 = reinterpret_cast<uint64_t*>(loadedFile);

		if (*r1 == 0x424D) ext = "bmp";
		else if (*r3 == 0x474946383761 || static_cast<uint64_t>(*loadedFile) == 0x474946383961) ext = "gif";
		else if (loadedFile[0] == 0xFF && loadedFile[1] == 0xD8 && loadedFile[2] == 0xFF && loadedFile[3] == 0xE0) ext = "jpg";
		else if (*r3 == 0x89504E470D0A1A0A) ext = "png";*/

		if (false /*Check for swf+video magic numbers*/)
		{

		}
		else
		{
			CxImage image;
			std::string ext = "";
			if (image.Load(_path))
			{
				uint32_t imgType = image.GetType();
				if (imgType != CXIMAGE_FORMAT_UNKNOWN)
				{
					switch (imgType)
					{
					case CXIMAGE_FORMAT_BMP: ext = "bmp"; break;
					case CXIMAGE_FORMAT_GIF: ext = "gif"; break;
					case CXIMAGE_FORMAT_JP2: ext = "jp2"; break;
					case CXIMAGE_FORMAT_JPC: ext = "jpc"; break;
					case CXIMAGE_FORMAT_JPG: ext = "jpg"; break;
					case CXIMAGE_FORMAT_MNG: ext = "mng"; break;
					case CXIMAGE_FORMAT_PCX: ext = "pcx"; break;
					case CXIMAGE_FORMAT_PGX: ext = "pgx"; break;
					case CXIMAGE_FORMAT_PNG: ext = "png"; break;
					case CXIMAGE_FORMAT_PNM: ext = "pnm"; break;
					case CXIMAGE_FORMAT_PSD: ext = "psd"; break;
					case CXIMAGE_FORMAT_RAS: ext = "ras"; break;
					case CXIMAGE_FORMAT_RAW: ext = "raw"; break;
					case CXIMAGE_FORMAT_SKA: ext = "ska"; break;
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
						char thispath[512];
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
					sqlite3_exec(_database, sqlString.c_str(), nullptr, nullptr, nullptr);

					/*std::string qry = hashString;
					qry += ".";
					qry += ext;*/
					return findImage(hashString.c_str());
				}
				else // Image is of unknown file format
				{

				}
			}
			else // Failure to load image (shouldn't happen)
			{

			}

			if (ext != "")
			{
				// Generate save directory.
				

				// Copy the image to the main directory.


					// Create the thumbnail and save it to the thumbnail directory.
				{
					
					//unsigned int width = img.getWidth(), height = img.getHeight();
					

				}

				std::string sqlString = "INSERT INTO images(filename, extension) VALUES(\"";
				sqlString += hashString;
				sqlString += "\",\"";
				sqlString += ext;
				sqlString += "\");";
				sqlite3_exec(_database, sqlString.c_str(), nullptr, nullptr, nullptr);

				int id = -1;
				findImage(_path, &id);

				return true;
			}
		}
	}
}