#pragma once

#include <FL/Fl.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Int_Input.H>
#include <FL/Fl_Light_Button.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_File_Browser.H>
#include <FL/Fl_Tabs.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Editor.H>

#include <mutex>
#include <thread>

#include <string>

#include "mongoose.h"
#include "Server.h"
#include "sqlite3.h"

#define GATS_VERSION "GATS v0.0.01 alpha"

class GUIInterface
{
private:
	void logMessage(const char *str);

	Fl_Window *_window;
	Fl_Tabs *_tabs;
	Fl_Group *_tabServer, *_tabFiles, *_tabEntries, *_tabTags;

	Fl_Button *_updateWhitelist;
	Fl_Text_Buffer *_logBuffer, *_editWhitelistBuffer;
	Fl_Text_Display *_log;
	Fl_Text_Editor *_editWhitelist;
	Fl_Light_Button *_serverButton;
	Fl_Int_Input *_portInput;

	sqlite3 *_database;
	std::mutex _databaseMutex;

	mg_server *_server;
	std::thread *_serverThread;
	std::mutex _serverMutex;
	std::vector<std::string> _serverWhitelist;
	bool _bServerIsUp, _bServerThreadStop;
	int _serverPort;
public:
	GUIInterface();
	~GUIInterface();
	int show();

	static void toggleServer(Fl_Widget* widget, void* data);
	static void changeServerPort(Fl_Widget* widget, void* data);
	static void updateWhitelist(Fl_Widget* widget, void* data);
	static void queryDatabase(GUIInterface *inter, std::string query, int callback(void* param, int argc, char *argv[], char *colname[]), void *userData);
};