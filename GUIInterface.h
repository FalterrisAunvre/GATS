#pragma once
#pragma comment(lib, "shlwapi.lib")

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
#include <FL/Fl_Input_Choice.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Select_Browser.H>

#include <Shlwapi.h>
#include <ximage.h>
#include <mutex>
#include <thread>
#include <memory>
#include <sstream>
#include <string>
#include <filesystem>

#include "mongoose.h"
#include "Server.h"
#include "sqlite3.h"
#include "tinyfiledialogs.h"
// 0f05947f75e5d442420790fd15380dd8
#define GATS_VERSION "GATS v0.1.02 alpha"

class GUIInterface
{
private:
	void logMessage(const char *str);
	void addFile(const char *_path);
	int findFileInDatabase(const char *filename);
	bool isFileInDatabase(const char *filename);
	void queryDatabase(std::string query, int callback(void* param, int argc, char *argv[], char *colname[]), void *userData);
	int getTopImageId();

	Fl_Window *_window;
	Fl_Tabs *_tabs;
	Fl_Group *_tabServer, *_tabEntries, *_tabTags, *_tabSql;

	// Server tab
	Fl_Button *_updateWhitelist;
	Fl_Text_Buffer *_logBuffer, *_editWhitelistBuffer;
	Fl_Text_Display *_log;
	Fl_Text_Editor *_editWhitelist;
	Fl_Light_Button *_serverButton;
	Fl_Int_Input *_portInput;

	// Entries tab
	Fl_Button *_openFiles, *_openDirectory, *_updateFilter, *_openEntry, *_modifyTags;
	Fl_Input *_filter;
	Fl_Select_Browser *_filterResults;
	
	// SQL tab
	Fl_Input *_sqlQuery;
	Fl_Text_Display *_sqlQueryResults;
	Fl_Text_Buffer *_sqlQueryResultsBuffer;

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

	static void ToggleServer(Fl_Widget* widget, void* data);
	static void ChangeServerPort(Fl_Widget* widget, void* data);
	static void UpdateWhitelist(Fl_Widget* widget, void* data);
	static void AddFiles(Fl_Widget* widget, void* data);
	static void AddDirectory(Fl_Widget* widget, void* data);
	static void EntriesFilter(Fl_Widget* widget, void* data);
	static void AnyQuery(Fl_Widget* widget, void* data);
	static void UpdateEntriesList(Fl_Widget* widget, void* data);
};