#include "Server.h"

void *Server::serve(serveArgs args)
{
	//serveArgs *args = (serveArgs*)argsl;

	while (!(*args.stop)) mg_poll_server(args.server, 200);
	return nullptr;
}

int Server::eventHandler(mg_connection *conn, mg_event event)
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
			generateHtml(conn, static_cast<serveArgs*>(conn->server_param)->inter, static_cast<serveArgs*>(conn->server_param)->sql, "post", html);

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

void Server::splitString(const std::string str, char delimiter, std::vector<std::string> &list)
{
	std::stringstream ss(str);
	std::string item;
	while (std::getline(ss, item, delimiter)) {
		list.push_back(item);
	}
}

void Server::getExtension(const std::string str, std::string &ext)
{
	ext = str.substr(str.find_last_of('.'));
	ext = ext.substr(1);
}

const char *Server::getMimeType(std::string ext)
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

void Server::generateTagSearchQuery(sqlite3 *sql, std::string &queryStr, std::vector<std::string> &tags)
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
void Server::tagIdLookup(sqlite3 *sql, std::vector<std::string> &tags, std::vector<int> &ids)
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

void Server::tagIdLookup(sqlite3 *sql, std::string &tag, int &id)
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

void Server::generateHtml(mg_connection *conn, GUIInterface *inter, sqlite3 *sql, std::string sub, std::string &html)
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
			htmlBuf << "<img src=\"preview/" << str.substr(0, 2) << "/" << str.substr(2, 2) << "/" << str << ".jpg/>";
		}
		htmlBuf << "</html>";
	}

	html = htmlBuf.str();
}