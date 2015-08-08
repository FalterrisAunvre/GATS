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
					_serverButton->callback(toggleServer, this);
					_serverButton->color2(FL_GREEN);
				_portInput = new Fl_Int_Input(20, 80, 90, 30, "Server Port");
					_portInput->callback(changeServerPort, this);
					_portInput->when(FL_WHEN_CHANGED);
					_portInput->maximum_size(5);
					_portInput->value("8008");
					_portInput->align(FL_ALIGN_RIGHT);
				_editWhitelistBuffer = new Fl_Text_Buffer;
				_editWhitelist = new Fl_Text_Editor(20, 160, 280, 280, "Whitelisted IPs (blank for no whitelist)");
					_editWhitelist->buffer(_editWhitelistBuffer);
					_editWhitelist->cursor_style(Fl_Text_Editor::SIMPLE_CURSOR);
				_updateWhitelist = new Fl_Button(20, 440, 280, 30, "Update Whitelist");
					_updateWhitelist->callback(updateWhitelist, this);
			}
			_tabServer->end();

			_tabFiles = new Fl_Group(20, 30, 600, 430, "Files");
			{
				
			}
			_tabFiles->end();
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

	delete _tabFiles;

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

void GUIInterface::toggleServer(Fl_Widget *widget, void *data)
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

void GUIInterface::changeServerPort(Fl_Widget* widget, void* data)
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

void GUIInterface::updateWhitelist(Fl_Widget* widget, void* data)
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

void GUIInterface::queryDatabase(GUIInterface *inter, std::string query, int callback(void* param, int argc, char *argv[], char *colname[]), void *userData = nullptr)
{
	std::lock_guard<std::mutex> lockGuard(inter->_databaseMutex);
	sqlite3_exec(inter->_database, query.c_str(), callback, userData, nullptr);
}