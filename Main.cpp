#include <iostream>

#include "Interface.h"

Interface gInterface;

// Generic Art Taggin Server
int main(int argc, char *argv[])
{
	while (gInterface.isGood())
	{
		gInterface.draw();
		gInterface.getInput();
	}
	return 0;
}