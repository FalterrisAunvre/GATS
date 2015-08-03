#pragma once
#pragma comment(lib, "shlwapi.lib")

#include <array>
#include <ximage.h>
#include <conio.h>
#include <filesystem>
#include <functional>
#include <iostream>
#include <hashlibpp.h>
#include <memory>
#include <stack>
#include <cstdint>
#include <sstream>
#include <Shlwapi.h>
#include <string>
#include <vector>
#include <Windows.h>

#include "mongoose.h"
#include "rlutil.h"
#include "rlutilextras.h"
#include "Server.h"
#include "sqlite3.h"
#include "tinyfiledialogs.h"

#define GATS_VERSION "GATS v0.0.01 alpha"

class Interface
{
private:
	int _countTags();
	unsigned int _nextEntryId();
	void _splitString(const std::string str, char delimiter, std::vector<std::string> &list);

	unsigned int _page;

	std::stack<int> _states;
	std::string _message;

	mg_server *_server;
	bool _bServerIsUp;
	unsigned int _serverPort;

	sqlite3 *_database;
public:
	enum StateEnum
	{
		State_Exit,
		State_Main,
		State_Database,
		State_Server,
		State_Lua,
		State_Files,
		State_Collections,
		State_Tags,
		State_TagList,
		STATECOUNT
	};
	enum ErrorEnum
	{
		Error_None,
		Error_FileDoesntExist,
		Error_HashCollision,
		Error_FailedToLoad,
		Error_UnsupportedFormat,
		Error_SQLiteError,
		ERRORCOUNT
	};
	Interface();
	~Interface();
	void getInput();
	void draw();
	bool isGood();
	bool startServer();
	bool stopServer();
	bool addTag(const char* name);
	bool findTag(const char* name, int *retId = nullptr);
	bool addTagCheckIfExists(const char* name);
	bool findImage(const char* name, int *retId = nullptr);
	unsigned int addEntry(const char* _path);
	bool addDirectory(const char* _path);
};