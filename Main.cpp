#include <iostream>

#include "Interface.h"

Interface gInterface;

// Generic Art Tagging Server
int main(int argc, char *argv[])
{
	while (gInterface.isGood())
	{
		gInterface.draw();
		gInterface.getInput();
	}
	// Susmaster
	return 0;
}