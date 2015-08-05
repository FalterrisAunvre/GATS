#pragma once

#include <cstdarg>
#include <string>
#include "rlutil.h"

namespace rlutil
{
	void mvprintf(int x, int y, const char* str, ...);
	int mvreadInt(int x, int y);
	std::string mvreadString(int x, int y);
}