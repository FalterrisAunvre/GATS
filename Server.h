#pragma once

#include <string>
#include "Interface.h"
#include "mongoose.h"

namespace Server
{
	struct serveArgs
	{
		bool *stop;
		mg_server *server;
	};
	void *serve(serveArgs argsl);
	int eventHandler(mg_connection *conn, mg_event event);
	void splitString(const std::string str, char delimiter, std::vector<std::string> &list);
	void getExtension(const std::string str, std::string &ext);
	const char *getMimeType(std::string ext);
}