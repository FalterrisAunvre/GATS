#pragma once

#include <hashlibpp.h>
#include <string>
#include <sstream>
#include <vector>
#include <Windows.h>
#include "GUIInterface.h"
#include "mongoose.h"
#include "sqlite3.h"

namespace Server
{
	struct serveArgs
	{
		bool *stop;
		mg_server *server;
		GUIInterface *inter;
		sqlite3 *sql;
	};
	void *serve(serveArgs argsl);
	int eventHandler(mg_connection *conn, mg_event event);
	void splitString(const std::string str, char delimiter, std::vector<std::string> &list);
	void getExtension(const std::string str, std::string &ext);
	const char *getMimeType(std::string ext);
	void generateTagSearchQuery(sqlite3 *sql, std::string &queryStr, std::vector<std::string> &tags);
	void tagIdLookup(sqlite3 *sql, std::vector<std::string> &tags, std::vector<int> &ids);
	void tagIdLookup(sqlite3 *sql, std::string &tag, int &ids);
	void generateHtml(mg_connection *conn, GUIInterface *inter, sqlite3 *sql, std::string sub, std::string &html);
}