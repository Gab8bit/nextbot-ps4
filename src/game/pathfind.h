// Grid pathfinding: A* (8 directions, no corner cutting) and BFS helpers.
#pragma once
#include "map.h"

// Finds a path from (sx, sy) to (gx, gy). Writes tile indices (y * w + x), start
// included, into outPath. Returns the number of tiles, or -1 if there is no path.
int pathfind_astar(const Map& m, int sx, int sy, int gx, int gy, int* outPath, int maxLen);

// Free tile with the largest walking distance from (sx, sy). Returns false if none.
bool pathfind_farthest(const Map& m, int sx, int sy, int* fx, int* fy);
