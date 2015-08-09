#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1

#include <iostream>

#include "GUIInterface.h"
#include "Interface.h"

Interface gInterface;

// Generic Art Tagging Server
int main(int argc, char *argv[])
{
	GUIInterface inter;
	inter.show();
	return 0;
}