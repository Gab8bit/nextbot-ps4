#include "pathfind.h"

#include <string.h>

static const int N = MAP_MAX_W * MAP_MAX_H;
static const int COST_STRAIGHT = 10, COST_DIAG = 14;

// scratch buffers (the game is single threaded)
static int g_cost[N];
static int g_parent[N];
static unsigned g_stamp[N];   // node belongs to the current search when g_stamp == s_search
static unsigned char g_closed[N];
static unsigned s_search = 0;

// binary min-heap of (f, node)
static int h_f[N * 2], h_node[N * 2], h_size;

static void heap_push(int f, int node) {
    int i = h_size++;
    while (i > 0) {
        int p = (i - 1) / 2;
        if (h_f[p] <= f) break;
        h_f[i] = h_f[p]; h_node[i] = h_node[p];
        i = p;
    }
    h_f[i] = f; h_node[i] = node;
}

static int heap_pop() {
    int top = h_node[0];
    int f = h_f[--h_size], node = h_node[h_size];
    int i = 0;
    for (;;) {
        int c = i * 2 + 1;
        if (c >= h_size) break;
        if (c + 1 < h_size && h_f[c + 1] < h_f[c]) c++;
        if (f <= h_f[c]) break;
        h_f[i] = h_f[c]; h_node[i] = h_node[c];
        i = c;
    }
    h_f[i] = f; h_node[i] = node;
    return top;
}

static int heuristic(int x, int y, int gx, int gy) { // octile distance
    int dx = x > gx ? x - gx : gx - x, dy = y > gy ? y - gy : gy - y;
    int mn = dx < dy ? dx : dy, mx = dx < dy ? dy : dx;
    return COST_DIAG * mn + COST_STRAIGHT * (mx - mn);
}

int pathfind_astar(const Map& m, int sx, int sy, int gx, int gy, int* outPath, int maxLen) {
    if (map_wall(m, sx, sy) || map_wall(m, gx, gy) || maxLen <= 0) return -1;
    if (++s_search == 0) { memset(g_stamp, 0, sizeof(g_stamp)); s_search = 1; }
    int w = m.w;
    int start = sy * w + sx, goal = gy * w + gx;
    h_size = 0;
    g_stamp[start] = s_search; g_cost[start] = 0; g_parent[start] = -1; g_closed[start] = 0;
    heap_push(heuristic(sx, sy, gx, gy), start);

    static const int DX[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
    static const int DY[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
    bool found = false;
    while (h_size > 0) {
        int cur = heap_pop();
        if (g_closed[cur]) continue; // stale heap entry
        g_closed[cur] = 1;
        if (cur == goal) { found = true; break; }
        int cx = cur % w, cy = cur / w;
        for (int k = 0; k < 8; k++) {
            int nx = cx + DX[k], ny = cy + DY[k];
            if (map_wall(m, nx, ny)) continue;
            // diagonal moves only if both side tiles are free (no squeezing past corners)
            if (k >= 4 && (map_wall(m, cx + DX[k], cy) || map_wall(m, cx, cy + DY[k]))) continue;
            int n = ny * w + nx;
            int cost = g_cost[cur] + (k >= 4 ? COST_DIAG : COST_STRAIGHT);
            if (g_stamp[n] != s_search) {
                g_stamp[n] = s_search; g_closed[n] = 0;
            } else if (g_closed[n] || cost >= g_cost[n]) {
                continue;
            }
            g_cost[n] = cost; g_parent[n] = cur;
            if (h_size < N * 2) heap_push(cost + heuristic(nx, ny, gx, gy), n);
        }
    }
    if (!found) return -1;

    int len = 0;
    for (int c = goal; c != -1; c = g_parent[c]) len++;
    if (len > maxLen) {
        // keep only the first maxLen steps from the start
        int skip = len - maxLen, c = goal;
        while (skip-- > 0) c = g_parent[c];
        for (int i = maxLen - 1; i >= 0; i--) { outPath[i] = c; c = g_parent[c]; }
        return maxLen;
    }
    int i = len - 1;
    for (int c = goal; c != -1; c = g_parent[c]) outPath[i--] = c;
    return len;
}

bool pathfind_farthest(const Map& m, int sx, int sy, int* fx, int* fy) {
    if (map_wall(m, sx, sy)) return false;
    static int dist[N], queue[N];
    for (int i = 0; i < m.w * m.h; i++) dist[i] = -1;
    int head = 0, tail = 0, best = sy * m.w + sx;
    queue[tail++] = best; dist[best] = 0;
    while (head < tail) {
        int c = queue[head++], cx = c % m.w, cy = c / m.w;
        if (dist[c] > dist[best]) best = c;
        const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };
        for (int k = 0; k < 4; k++) {
            int nx = cx + dx[k], ny = cy + dy[k];
            if (map_wall(m, nx, ny)) continue;
            int n = ny * m.w + nx;
            if (dist[n] < 0) { dist[n] = dist[c] + 1; queue[tail++] = n; }
        }
    }
    *fx = best % m.w; *fy = best / m.w;
    return true;
}
