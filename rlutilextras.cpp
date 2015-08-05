#include "rlutilextras.h"

namespace rlutil
{
	void mvprintf(int x, int y, const char* str, ...)
	{
		va_list args;
		va_start(args, str);
		gotoxy(x, y);
		vprintf(str, args);
		//char buffer[256];
		//sprintf(buffer, str, args);
		//printf("%s", buffer);
		va_end(args);
	}

	int mvreadInt(int x, int y)
	{
		gotoxy(x, y);
		int i;
		scanf("%d", &i);
		return i;
	}

	std::string mvreadString(int x, int y)
	{
		gotoxy(x, y);
		char buf[256];
		fgets(buf, 256, stdin);
		return std::string(buf);
	}
}