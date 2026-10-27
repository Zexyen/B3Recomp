// Witch Time: the launcher (see app.h). witch-time [--returned <exit code>]: --returned is how the launcher starts
// itself again after the game ended in its window (game_host.cpp).
#include "app.h"

int main(int argc, char** argv)
{
	App app;
	if (!app.init(argc, argv))
		return 1;
	return app.run();
}
