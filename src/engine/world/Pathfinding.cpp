#include "engine/world/Pathfinding.h"

#include "engine/world/GridMap.h"

#include <algorithm>
#include <cstdlib>
#include <queue>
#include <vector>

namespace Mood::Pathfinding {

namespace {

struct Node {
    u32 idx;       // y * width + x
    u32 g;         // costo desde start
    u32 f;         // g + h
};

struct NodeGreater {
    bool operator()(const Node& a, const Node& b) const { return a.f > b.f; }
};

inline u32 manhattan(TileCoord a, TileCoord b) {
    const u32 dx = (a.x > b.x) ? (a.x - b.x) : (b.x - a.x);
    const u32 dy = (a.y > b.y) ? (a.y - b.y) : (b.y - a.y);
    return dx + dy;
}

} // namespace

std::vector<TileCoord> findPath(const GridMap& map,
                                 TileCoord start,
                                 TileCoord goal) {
    const u32 W = map.width();
    const u32 H = map.height();

    // Validacion de bounds. Goal solido = sin path; start fuera = sin
    // path. Start solido se permite (agente atrapado puede salir).
    if (start.x >= W || start.y >= H) return {};
    if (goal.x  >= W || goal.y  >= H) return {};
    if (map.isSolid(goal.x, goal.y))  return {};

    if (start == goal) return {start};

    const u32 N = W * H;
    auto idxOf = [W](u32 x, u32 y) { return y * W + x; };

    // closed[i] = true si ya expandimos el nodo i.
    std::vector<bool> closed(N, false);
    // bestG[i] = mejor g conocido al nodo i (UINT32_MAX = sin descubrir).
    std::vector<u32> bestG(N, 0xFFFFFFFFu);
    // parent[i] = idx del padre en el path optimo descubierto hasta ahora.
    // 0xFFFFFFFFu = sin padre (es el start).
    std::vector<u32> parent(N, 0xFFFFFFFFu);

    std::priority_queue<Node, std::vector<Node>, NodeGreater> open;

    const u32 startIdx = idxOf(start.x, start.y);
    const u32 goalIdx  = idxOf(goal.x,  goal.y);
    bestG[startIdx] = 0;
    open.push(Node{startIdx, 0, manhattan(start, goal)});

    // 4-connected, sin diagonales.
    constexpr int kDx[4] = {+1, -1,  0,  0};
    constexpr int kDy[4] = { 0,  0, +1, -1};

    while (!open.empty()) {
        const Node n = open.top();
        open.pop();

        if (closed[n.idx]) continue;
        closed[n.idx] = true;

        if (n.idx == goalIdx) break;

        const u32 nx = n.idx % W;
        const u32 ny = n.idx / W;

        for (int k = 0; k < 4; ++k) {
            const i64 sx = static_cast<i64>(nx) + kDx[k];
            const i64 sy = static_cast<i64>(ny) + kDy[k];
            if (sx < 0 || sx >= static_cast<i64>(W)) continue;
            if (sy < 0 || sy >= static_cast<i64>(H)) continue;
            const u32 vx = static_cast<u32>(sx);
            const u32 vy = static_cast<u32>(sy);
            if (map.isSolid(vx, vy)) continue;

            const u32 vIdx = idxOf(vx, vy);
            if (closed[vIdx]) continue;

            const u32 tentativeG = n.g + 1;
            if (tentativeG >= bestG[vIdx]) continue; // ya teniamos mejor

            bestG[vIdx]  = tentativeG;
            parent[vIdx] = n.idx;
            const u32 h = manhattan(TileCoord{vx, vy}, goal);
            open.push(Node{vIdx, tentativeG, tentativeG + h});
        }
    }

    if (parent[goalIdx] == 0xFFFFFFFFu && goalIdx != startIdx) {
        // Nunca llegamos al goal — sin path posible.
        return {};
    }

    // Reconstruir path goal -> start, despues invertir.
    std::vector<TileCoord> path;
    u32 cur = goalIdx;
    while (cur != 0xFFFFFFFFu) {
        path.push_back(TileCoord{cur % W, cur / W});
        if (cur == startIdx) break;
        cur = parent[cur];
    }
    std::reverse(path.begin(), path.end());
    return path;
}

} // namespace Mood::Pathfinding
