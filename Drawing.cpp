#include "Drawing.h"
#include <string>
#include <vector>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdio>
#include <windows.h>
#include <tlhelp32.h>
#include <fstream>
#include <processthreadsapi.h>
#include <random>
#include <queue>
#include <set>
#include <functional>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <ctime>

LPCSTR Drawing::lpWindowName = "Launcher";
ImVec2 Drawing::vWindowSize = { 450, 450 };
ImGuiWindowFlags Drawing::WindowFlags = 0;
bool Drawing::bDraw = true;
bool checkval = false;
bool file_not_found = false;
int a = 0;
char input;
std::string input2 = "";
ImVec4 colorpicker = ImVec4(0.0f, 1.0f, 1.0f, 1.0f);

namespace globals
{
	const char* killsteamservice = "taskkill/f /im SteamService.exe";
	const char* killsteamwebhelper = "taskkill/f /im steamwebhelper.exe";
	const char* killsteam = "taskkill/f /im Steam.exe";
	const char* killcsgo = "taskkill /f /im csgo.exe";
	const char* startcsgo = "START \"Steam\" \"C:\\Program Files (x86)\\Steam\\steam.exe\" /min -applaunch 730";
	const char* startsteamcustomid = "START \"Steam\" \"C:\\Program Files (x86)\\Steam\\steam.exe\" /min -login ";
	const char* startsteam = "START \"Steam\" \"C:\\Program Files (x86)\\Steam\\steam.exe\" /min ";
}

void Drawing::Active()
{
	bDraw = true;
}

bool Drawing::isActive()
{
	return bDraw == true;
}

DWORD GetProcessIdByName(const std::string& processName) {
	DWORD processId = 0;
	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot != INVALID_HANDLE_VALUE) {
		PROCESSENTRY32 pe32;
		pe32.dwSize = sizeof(PROCESSENTRY32);
		if (Process32First(hSnapshot, &pe32)) {
			do {
				// Convert processName to a TCHAR string
#ifdef UNICODE
				std::wstring wideProcessName = std::wstring(processName.begin(), processName.end());
				if (_tcsicmp(pe32.szExeFile, wideProcessName.c_str()) == 0) {
#else
				if (_tcsicmp(pe32.szExeFile, processName.c_str()) == 0) {
#endif
					processId = pe32.th32ProcessID;
					break;
				}
				} while (Process32Next(hSnapshot, &pe32));
			}
		CloseHandle(hSnapshot);
		}
	return processId;
	}

bool IsProcessRunning(const TCHAR* const executableName) {
	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);

	const auto snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

	if (!Process32First(snapshot, &entry)) {
		CloseHandle(snapshot);
		return false;
	}

	do {
		if (!_tcsicmp(entry.szExeFile, executableName)) {
			CloseHandle(snapshot);
			return true;
		}
	} while (Process32Next(snapshot, &entry));

	CloseHandle(snapshot);
	return false;
}

int Login()
{
	std::ifstream account_file("credentials.txt");
	std::ostringstream buffer;
	static int account_index = 0;
	if (!account_file)
	{
		file_not_found = true;
		return 1;
	}
	buffer << account_file.rdbuf();
	account_file.close();
	std::string accounts = buffer.str();
	std::vector<std::string> split;
	std::stringstream ss(accounts);
	std::string word;
	while (ss >> word) {
		split.push_back(word);
	}
	std::string user_name = split[account_index * 3]; //magic numbers behenchod.
	std::string pass_word = split[account_index * 3 + 1];
	std::string steamid = split[account_index * 3 + 2];

	buffer.str("");
	buffer.clear();
	buffer << globals::startsteamcustomid << user_name << " " << pass_word << '\n';
	system(buffer.str().c_str());
	account_index++; // next account
	return 0;
}

int num_lines()
{
	std::ifstream myFile;
	std::string line;
	int lines = 0;
	myFile.open("credentials.txt");
	while (std::getline(myFile, line))
	{
		lines++;
	}
	myFile.close();
	return lines;
}
// xp
//switch accnt after weekly target
// one key at a time

// ============================================================
// RANDOM CITY MAP  +  DYNAMIC ROUTING
// ============================================================
//
// The city is a GRAPH:
//   * nodes  = intersections  (cityPoints)
//   * edges  = roads          (cityRoads, referencing node indices)
//
// The carrier plans a real path from its current node to a delivery
// destination using Dijkstra, then drives that path segment by
// segment. When roads close or get congested (traffic events, or the
// user clicking), the path is recomputed on the fly -> dynamic route
// changes.
// ============================================================

struct RoadPoint
{
    ImVec2 pos;
};

struct Road
{
    int a = 0;              // node index (endpoint A)
    int b = 0;              // node index (endpoint B)
    bool blocked = false;   // closed road -> impassable
    float congestion = 1.0f;// >1 = traffic, slower + higher path cost
};

struct Neighbor
{
    int node;   // adjacent node index
    int road;   // index into cityRoads for that connection
};

struct Building
{
    ImVec2 pos; // map-space position (pre y-offset)
    float w;
    float h;
    ImU32 col;
};

static std::vector<RoadPoint> cityPoints;
static std::vector<Road> cityRoads;
static std::vector<std::vector<Neighbor>> cityAdj; // adjacency list per node
static std::vector<Building> cityBuildings;

static bool cityGenerated = false;

static std::mt19937 cityRng(
    static_cast<unsigned int>(
        std::time(nullptr)
        )
);

// Payload:
// 1 = Bike
// 2 = Car
// 3 = Truck
static int payload = 1;

// --- Carrier / routing state ---------------------------------
static std::vector<int> carrierRoute;   // node sequence: current .. destination
static int   routeStep = 0;             // traversing route[step] -> route[step+1]
static float carrierProgress = 0.0f;    // 0..1 along current segment
static int   destinationNode = 0;

// --- Dynamic-route-change controls ---------------------------
static bool  autoTraffic = true;        // periodic traffic events
static float trafficTimer = 3.0f;       // countdown to next event
static float rerouteFlash = 0.0f;       // >0 shows "REROUTING" pulse
static int   rerouteCount = 0;          // stat: how many times rerouted

// ============================================================
// RANDOM NUMBER HELPERS
// ============================================================

static int RandomInt(int min, int max)
{
    std::uniform_int_distribution<int> dist(min, max);
    return dist(cityRng);
}

static float RandomFloat(float min, float max)
{
    std::uniform_real_distribution<float> dist(min, max);
    return dist(cityRng);
}

// ============================================================
// GRAPH HELPERS
// ============================================================

static float NodeDist(int a, int b)
{
    ImVec2 pa = cityPoints[a].pos;
    ImVec2 pb = cityPoints[b].pos;
    float dx = pa.x - pb.x;
    float dy = pa.y - pb.y;
    return std::sqrt(dx * dx + dy * dy);
}

static void BuildAdjacency()
{
    cityAdj.assign(cityPoints.size(), {});
    for (int i = 0; i < static_cast<int>(cityRoads.size()); i++)
    {
        const Road& r = cityRoads[i];
        cityAdj[r.a].push_back({ r.b, i });
        cityAdj[r.b].push_back({ r.a, i });
    }
}

// Road index connecting two adjacent nodes (-1 if none).
static int FindRoadIndex(int a, int b)
{
    if (a < 0 || a >= static_cast<int>(cityAdj.size()))
        return -1;
    for (const Neighbor& nb : cityAdj[a])
        if (nb.node == b)
            return nb.road;
    return -1;
}

// Dijkstra shortest path (cost = length * congestion, skip blocked).
// Returns node sequence src..dst, or empty if unreachable.
static std::vector<int> FindPath(int src, int dst)
{
    std::vector<int> path;
    int n = static_cast<int>(cityPoints.size());
    if (n == 0 || src < 0 || dst < 0 || src >= n || dst >= n)
        return path;

    std::vector<float> dist(n, FLT_MAX);
    std::vector<int> prev(n, -1);

    typedef std::pair<float, int> PFI;
    std::priority_queue<PFI, std::vector<PFI>, std::greater<PFI>> pq;

    dist[src] = 0.0f;
    pq.push({ 0.0f, src });

    while (!pq.empty())
    {
        PFI top = pq.top();
        pq.pop();

        float d = top.first;
        int u = top.second;

        if (d > dist[u])
            continue;
        if (u == dst)
            break;

        for (const Neighbor& nb : cityAdj[u])
        {
            const Road& r = cityRoads[nb.road];
            if (r.blocked)
                continue;

            float w = NodeDist(u, nb.node) * r.congestion;
            if (dist[u] + w < dist[nb.node])
            {
                dist[nb.node] = dist[u] + w;
                prev[nb.node] = u;
                pq.push({ dist[nb.node], nb.node });
            }
        }
    }

    if (dist[dst] == FLT_MAX)
        return path; // unreachable

    for (int at = dst; at != -1; at = prev[at])
        path.push_back(at);
    std::reverse(path.begin(), path.end());
    return path;
}

// Node the carrier is currently departing from.
static int CarrierNode()
{
    if (carrierRoute.empty())
        return 0;
    if (routeStep >= static_cast<int>(carrierRoute.size()))
        return carrierRoute.back();
    return carrierRoute[routeStep];
}

// Guarantee a path exists (clears closures if the city deadlocks so
// the demo never gets permanently stuck).
static void EnsureReachable(int from, int to)
{
    if (!FindPath(from, to).empty())
        return;
    for (auto& r : cityRoads)
        r.blocked = false;
}

// Hard reroute: recompute from the current node, snap to it.
static void HardReroute(bool announce)
{
    int from = CarrierNode();
    EnsureReachable(from, destinationNode);

    std::vector<int> p = FindPath(from, destinationNode);
    if (p.size() >= 2)
    {
        carrierRoute = p;
        routeStep = 0;
        carrierProgress = 0.0f;
    }
    if (announce)
    {
        rerouteFlash = 1.6f;
        rerouteCount++;
    }
}

// Soft reroute: keep driving the current segment, recompute the tail.
// Used for congestion changes so the carrier doesn't snap backwards.
static void SoftReroute(bool announce)
{
    if (carrierRoute.size() < 2 ||
        routeStep + 1 >= static_cast<int>(carrierRoute.size()))
    {
        HardReroute(announce);
        return;
    }

    int segStart = carrierRoute[routeStep];
    int heading = carrierRoute[routeStep + 1];

    // Can't keep the current segment if that road just closed.
    int rIdx = FindRoadIndex(segStart, heading);
    if (rIdx >= 0 && cityRoads[rIdx].blocked)
    {
        HardReroute(announce);
        return;
    }

    EnsureReachable(heading, destinationNode);
    std::vector<int> tail = FindPath(heading, destinationNode);
    if (tail.empty())
    {
        HardReroute(announce);
        return;
    }

    std::vector<int> route;
    route.push_back(segStart);
    for (int nd : tail)   // tail[0] == heading
        route.push_back(nd);

    carrierRoute = route;
    routeStep = 0;        // progress preserved on segStart -> heading

    if (announce)
    {
        rerouteFlash = 1.6f;
        rerouteCount++;
    }
}

static void PickNewDestination()
{
    int n = static_cast<int>(cityPoints.size());
    if (n < 2)
        return;

    int from = CarrierNode();
    int dst = from;
    int guard = 0;
    do { dst = RandomInt(0, n - 1); } while (dst == from && ++guard < 50);

    destinationNode = dst;
    HardReroute(false);
}

// ============================================================
// TRAFFIC EVENTS  (the source of dynamic route changes)
// ============================================================

static void TriggerTrafficEvent(bool reroute)
{
    if (cityRoads.empty())
        return;

    // Ease some existing jams / reopen some roads.
    for (auto& r : cityRoads)
    {
        if (r.blocked && RandomFloat(0.0f, 1.0f) < 0.5f)
            r.blocked = false;
        if (!r.blocked && r.congestion > 1.0f && RandomFloat(0.0f, 1.0f) < 0.5f)
            r.congestion = 1.0f;
    }

    // New road closures.
    int nBlock = RandomInt(1, 3);
    for (int i = 0; i < nBlock; i++)
        cityRoads[RandomInt(0, static_cast<int>(cityRoads.size()) - 1)].blocked = true;

    // New congestion pockets.
    int nJam = RandomInt(2, 5);
    for (int i = 0; i < nJam; i++)
    {
        Road& r = cityRoads[RandomInt(0, static_cast<int>(cityRoads.size()) - 1)];
        if (!r.blocked)
            r.congestion = RandomFloat(1.6f, 3.5f);
    }

    if (reroute)
        SoftReroute(true); // adapt to the new conditions, live
}

// ============================================================
// GENERATE CITY
// ============================================================

static void GenerateCity()
{
    cityPoints.clear();
    cityRoads.clear();
    cityBuildings.clear();

    // City area
    const float mapX = 20.0f;
    const float mapY = 70.0f;

    const float mapWidth = 740.0f;
    const float mapHeight = 530.0f;

    // Random grid dimensions
    int columns = RandomInt(7, 11);
    int rows = RandomInt(6, 9);

    float columnSpacing = mapWidth / (columns - 1);
    float rowSpacing = mapHeight / (rows - 1);

    // --------------------------------------------------------
    // Generate intersections (nodes)
    // --------------------------------------------------------

    for (int y = 0; y < rows; y++)
    {
        for (int x = 0; x < columns; x++)
        {
            float offsetX = 0.0f;
            float offsetY = 0.0f;

            if (x != 0 && x != columns - 1)
                offsetX = RandomFloat(-12.0f, 12.0f);

            if (y != 0 && y != rows - 1)
                offsetY = RandomFloat(-12.0f, 12.0f);

            RoadPoint point;
            point.pos = ImVec2(
                mapX + x * columnSpacing + offsetX,
                mapY + y * rowSpacing + offsetY
            );
            cityPoints.push_back(point);
        }
    }

    // --------------------------------------------------------
    // Roads (edges) store node indices so we can path over them
    // --------------------------------------------------------

    // Horizontal
    for (int y = 0; y < rows; y++)
        for (int x = 0; x < columns - 1; x++)
        {
            Road road;
            road.a = y * columns + x;
            road.b = y * columns + x + 1;
            cityRoads.push_back(road);
        }

    // Vertical
    for (int y = 0; y < rows - 1; y++)
        for (int x = 0; x < columns; x++)
        {
            Road road;
            road.a = y * columns + x;
            road.b = (y + 1) * columns + x;
            cityRoads.push_back(road);
        }

    // Random diagonals (shortcuts that make routing interesting)
    int diagonalRoads = RandomInt(4, 10);
    for (int i = 0; i < diagonalRoads; i++)
    {
        int i1 = RandomInt(0, static_cast<int>(cityPoints.size()) - 1);
        int i2 = RandomInt(0, static_cast<int>(cityPoints.size()) - 1);
        if (i1 == i2)
            continue;
        Road road;
        road.a = i1;
        road.b = i2;
        cityRoads.push_back(road);
    }

    BuildAdjacency();

    // --------------------------------------------------------
    // Buildings generated ONCE (previously re-randomized every
    // frame, which made them flicker).
    // --------------------------------------------------------
    for (const auto& point : cityPoints)
    {
        Building bld;
        bld.pos = ImVec2(
            point.pos.x + RandomFloat(-22.0f, 22.0f),
            point.pos.y + RandomFloat(-22.0f, 22.0f)
        );
        bld.w = RandomFloat(10.0f, 22.0f);
        bld.h = RandomFloat(8.0f, 18.0f);
        bld.col = IM_COL32(
            RandomInt(45, 85),
            RandomInt(45, 85),
            RandomInt(50, 95),
            255
        );
        cityBuildings.push_back(bld);
    }

    // --------------------------------------------------------
    // Place carrier + first delivery, then plan the route.
    // --------------------------------------------------------
    int n = static_cast<int>(cityPoints.size());
    int start = RandomInt(0, n - 1);

    int dst = start;
    int guard = 0;
    do { dst = RandomInt(0, n - 1); } while (dst == start && ++guard < 50);

    destinationNode = dst;
    carrierRoute = FindPath(start, dst);
    if (carrierRoute.size() < 2)
        carrierRoute = { start };

    routeStep = 0;
    carrierProgress = 0.0f;
    trafficTimer = RandomFloat(2.0f, 4.0f);
    rerouteFlash = 0.0f;
    rerouteCount = 0;

    cityGenerated = true;
}

// ============================================================
// CARRIER MOVEMENT
// ============================================================

static ImVec2 CarrierMapPos()
{
    if (carrierRoute.empty())
        return ImVec2(0, 0);
    if (carrierRoute.size() < 2)
        return cityPoints[carrierRoute[0]].pos;

    int s = carrierRoute[routeStep];
    int e = carrierRoute[
        (routeStep + 1 < static_cast<int>(carrierRoute.size()))
            ? routeStep + 1 : routeStep];

    ImVec2 a = cityPoints[s].pos;
    ImVec2 b = cityPoints[e].pos;
    return ImVec2(
        a.x + (b.x - a.x) * carrierProgress,
        a.y + (b.y - a.y) * carrierProgress
    );
}

static void UpdateCarrier(float dt)
{
    // Arrived / degenerate route -> get a new delivery.
    if (carrierRoute.size() < 2)
    {
        PickNewDestination();
        return;
    }

    int s = carrierRoute[routeStep];
    int e = carrierRoute[routeStep + 1];
    int rIdx = FindRoadIndex(s, e);

    // Current road closed mid-drive -> must reroute now.
    if (rIdx >= 0 && cityRoads[rIdx].blocked)
    {
        HardReroute(true);
        return;
    }

    float cong = (rIdx >= 0) ? cityRoads[rIdx].congestion : 1.0f;
    const float baseSpeed = 0.7f; // segments per second at no traffic

    carrierProgress += dt * baseSpeed / cong;

    while (carrierProgress >= 1.0f)
    {
        carrierProgress -= 1.0f;
        routeStep++;

        // Reached the destination.
        if (routeStep + 1 >= static_cast<int>(carrierRoute.size()))
        {
            carrierProgress = 0.0f;
            PickNewDestination();
            return;
        }

        // Peek the next segment; reroute if it's closed.
        int ns = carrierRoute[routeStep];
        int ne = carrierRoute[routeStep + 1];
        int nr = FindRoadIndex(ns, ne);
        if (nr >= 0 && cityRoads[nr].blocked)
        {
            HardReroute(true);
            return;
        }
    }
}

// ============================================================
// PICKING HELPERS (click interactions)
// ============================================================

static int NearestNode(ImVec2 mapPt)
{
    int best = -1;
    float bd = FLT_MAX;
    for (int i = 0; i < static_cast<int>(cityPoints.size()); i++)
    {
        float dx = cityPoints[i].pos.x - mapPt.x;
        float dy = cityPoints[i].pos.y - mapPt.y;
        float d = dx * dx + dy * dy;
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

static float PointSegDist(ImVec2 p, ImVec2 a, ImVec2 b)
{
    float vx = b.x - a.x, vy = b.y - a.y;
    float wx = p.x - a.x, wy = p.y - a.y;
    float c1 = vx * wx + vy * wy;
    if (c1 <= 0.0f)
        return std::sqrt(wx * wx + wy * wy);
    float c2 = vx * vx + vy * vy;
    if (c2 <= c1)
    {
        float dx = p.x - b.x, dy = p.y - b.y;
        return std::sqrt(dx * dx + dy * dy);
    }
    float t = c1 / c2;
    float px = a.x + t * vx, py = a.y + t * vy;
    float dx = p.x - px, dy = p.y - py;
    return std::sqrt(dx * dx + dy * dy);
}

static int NearestRoad(ImVec2 mapPt)
{
    int best = -1;
    float bd = FLT_MAX;
    for (int i = 0; i < static_cast<int>(cityRoads.size()); i++)
    {
        const Road& r = cityRoads[i];
        float d = PointSegDist(mapPt, cityPoints[r.a].pos, cityPoints[r.b].pos);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

// ============================================================
// DRAW BIKE
// ============================================================

static void DrawBike(
    ImDrawList* draw,
    ImVec2 pos
)
{
    // Wheels
    draw->AddCircle(
        ImVec2(pos.x - 8, pos.y + 5),
        5.0f,
        IM_COL32(220, 220, 220, 255),
        12,
        2.0f
    );

    draw->AddCircle(
        ImVec2(pos.x + 8, pos.y + 5),
        5.0f,
        IM_COL32(220, 220, 220, 255),
        12,
        2.0f
    );

    // Frame
    draw->AddLine(
        ImVec2(pos.x - 8, pos.y + 5),
        ImVec2(pos.x, pos.y - 5),
        IM_COL32(50, 200, 255, 255),
        2.0f
    );

    draw->AddLine(
        ImVec2(pos.x, pos.y - 5),
        ImVec2(pos.x + 8, pos.y + 5),
        IM_COL32(50, 200, 255, 255),
        2.0f
    );

    draw->AddLine(
        ImVec2(pos.x - 8, pos.y + 5),
        ImVec2(pos.x + 8, pos.y + 5),
        IM_COL32(50, 200, 255, 255),
        2.0f
    );

    // Rider
    draw->AddCircleFilled(
        ImVec2(pos.x, pos.y - 10),
        3.0f,
        IM_COL32(255, 200, 100, 255)
    );
}

// ============================================================
// DRAW CAR
// ============================================================

static void DrawCar(ImDrawList* draw, ImVec2 pos)
{
    ImVec2 pMin(
        pos.x - 12.0f,
        pos.y - 7.0f
    );

    ImVec2 pMax(
        pos.x + 12.0f,
        pos.y + 7.0f
    );

    // Car body
    draw->AddRectFilled(
        pMin,
        pMax,
        IM_COL32(60, 150, 255, 255),
        3.0f
    );

    // Front window
    draw->AddRectFilled(
        ImVec2(
            pos.x - 7.0f,
            pos.y - 5.0f
        ),
        ImVec2(
            pos.x + 2.0f,
            pos.y + 1.0f
        ),
        IM_COL32(100, 180, 220, 255),
        1.0f
    );

    // Rear window
    draw->AddRectFilled(
        ImVec2(
            pos.x + 3.0f,
            pos.y - 5.0f
        ),
        ImVec2(
            pos.x + 8.0f,
            pos.y + 1.0f
        ),
        IM_COL32(100, 180, 220, 255),
        1.0f
    );

    // Wheels
    draw->AddCircleFilled(
        ImVec2(
            pos.x - 7.0f,
            pos.y + 7.0f
        ),
        3.0f,
        IM_COL32(30, 30, 30, 255)
    );

    draw->AddCircleFilled(
        ImVec2(
            pos.x + 7.0f,
            pos.y + 7.0f
        ),
        3.0f,
        IM_COL32(30, 30, 30, 255)
    );
}

// ============================================================
// DRAW TRUCK
// ============================================================

static void DrawTruck(
    ImDrawList* draw,
    ImVec2 pos
)
{
    // Cargo container
    draw->AddRectFilled(
        ImVec2(pos.x - 17, pos.y - 8),
        ImVec2(pos.x + 5, pos.y + 8),
        IM_COL32(220, 150, 50, 255),
        2.0f
    );

    // Cab
    draw->AddRectFilled(
        ImVec2(pos.x + 5, pos.y - 6),
        ImVec2(pos.x + 16, pos.y + 8),
        IM_COL32(70, 150, 220, 255),
        2.0f
    );

    // Window
    draw->AddRectFilled(
        ImVec2(pos.x + 8, pos.y - 4),
        ImVec2(pos.x + 14, pos.y + 1),
        IM_COL32(100, 190, 230, 255)
    );

    // Wheels
    draw->AddCircleFilled(
        ImVec2(pos.x - 9, pos.y + 9),
        3.5f,
        IM_COL32(30, 30, 30, 255)
    );

    draw->AddCircleFilled(
        ImVec2(pos.x + 10, pos.y + 9),
        3.5f,
        IM_COL32(30, 30, 30, 255)
    );
}

// ============================================================
// DRAW CITY MAP
// ============================================================

// Map-space -> screen-space (roads/points are drawn with a -65 y shift).
static inline ImVec2 ToScreen(ImVec2 canvasPos, ImVec2 mapPt)
{
    return ImVec2(canvasPos.x + mapPt.x, canvasPos.y + mapPt.y - 65.0f);
}

// Screen-space -> map-space (inverse of ToScreen), for click picking.
static inline ImVec2 ToMap(ImVec2 canvasPos, ImVec2 screenPt)
{
    return ImVec2(screenPt.x - canvasPos.x, screenPt.y - canvasPos.y + 65.0f);
}

static void DrawCityMap()
{
    ImGui::BeginChild(
        "CityMap",
        ImVec2(760, 550),
        true
    );

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetWindowPos();

    // --------------------------------------------------------
    // Generate city once
    // --------------------------------------------------------
    if (!cityGenerated)
        GenerateCity();

    float dt = ImGui::GetIO().DeltaTime;

    // --------------------------------------------------------
    // SIMULATION UPDATE (dynamic route changes happen here)
    // --------------------------------------------------------
    if (rerouteFlash > 0.0f)
        rerouteFlash -= dt;

    if (autoTraffic)
    {
        trafficTimer -= dt;
        if (trafficTimer <= 0.0f)
        {
            trafficTimer = RandomFloat(2.0f, 4.5f);
            TriggerTrafficEvent(true);
        }
    }

    UpdateCarrier(dt);

    // --------------------------------------------------------
    // Interaction: left-click sets destination, right-click
    // closes/opens the nearest road -> carrier reroutes live.
    // --------------------------------------------------------
    if (ImGui::IsWindowHovered())
    {
        ImVec2 m = ImGui::GetIO().MousePos;
        ImVec2 mapPt = ToMap(canvasPos, m);

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            int node = NearestNode(mapPt);
            if (node >= 0)
            {
                destinationNode = node;
                HardReroute(true);
            }
        }
        else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            int road = NearestRoad(mapPt);
            if (road >= 0)
            {
                cityRoads[road].blocked = !cityRoads[road].blocked;
                if (cityRoads[road].blocked)
                    SoftReroute(true);
            }
        }
    }

    // --------------------------------------------------------
    // Background
    // --------------------------------------------------------
    draw->AddRectFilled(
        ImVec2(canvasPos.x + 5, canvasPos.y + 5),
        ImVec2(canvasPos.x + 755, canvasPos.y + 545),
        IM_COL32(22, 25, 30, 255),
        4.0f
    );

    // --------------------------------------------------------
    // Buildings (generated once, no more per-frame flicker)
    // --------------------------------------------------------
    for (const auto& bld : cityBuildings)
    {
        ImVec2 c = ToScreen(canvasPos, bld.pos);
        draw->AddRectFilled(
            ImVec2(c.x - bld.w, c.y - bld.h),
            ImVec2(c.x + bld.w, c.y + bld.h),
            bld.col,
            2.0f
        );
    }

    // --------------------------------------------------------
    // Roads, colored by live state
    //   closed     -> red (dashed X marker)
    //   congested  -> amber, thicker
    //   clear      -> grey
    // --------------------------------------------------------
    for (const auto& road : cityRoads)
    {
        ImVec2 start = ToScreen(canvasPos, cityPoints[road.a].pos);
        ImVec2 end = ToScreen(canvasPos, cityPoints[road.b].pos);

        // Shadow
        draw->AddLine(start, end, IM_COL32(10, 10, 10, 255), 9.0f);

        ImU32 col;
        float thickness;
        if (road.blocked)
        {
            col = IM_COL32(200, 45, 45, 255);
            thickness = 6.0f;
        }
        else if (road.congestion > 1.2f)
        {
            // amber -> deep orange as congestion rises
            float t = (road.congestion - 1.2f) / 2.3f; // ~0..1
            if (t > 1.0f) t = 1.0f;
            col = IM_COL32(
                235,
                static_cast<int>(200 - 120 * t),
                40,
                255
            );
            thickness = 6.0f + 2.0f * t;
        }
        else
        {
            col = IM_COL32(75, 75, 80, 255);
            thickness = 6.0f;
        }

        draw->AddLine(start, end, col, thickness);

        if (road.blocked)
        {
            // Little X in the middle to read as "closed".
            ImVec2 mid((start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f);
            draw->AddLine(ImVec2(mid.x - 5, mid.y - 5),
                          ImVec2(mid.x + 5, mid.y + 5),
                          IM_COL32(255, 230, 230, 255), 2.0f);
            draw->AddLine(ImVec2(mid.x - 5, mid.y + 5),
                          ImVec2(mid.x + 5, mid.y - 5),
                          IM_COL32(255, 230, 230, 255), 2.0f);
        }
        else
        {
            // Lane centerline
            draw->AddLine(start, end, IM_COL32(150, 150, 150, 90), 1.0f);
        }
    }

    // --------------------------------------------------------
    // The carrier's CURRENT ROUTE, glowing on top of the roads.
    // This is what visibly snaps to a new path on a reroute.
    // --------------------------------------------------------
    if (carrierRoute.size() >= 2)
    {
        ImVec2 prev = ToScreen(canvasPos, CarrierMapPos());
        for (int i = routeStep + 1; i < static_cast<int>(carrierRoute.size()); i++)
        {
            ImVec2 np = ToScreen(canvasPos, cityPoints[carrierRoute[i]].pos);
            draw->AddLine(prev, np, IM_COL32(60, 255, 140, 90), 7.0f); // glow
            draw->AddLine(prev, np, IM_COL32(90, 255, 160, 230), 2.5f);// core
            prev = np;
        }
    }

    // --------------------------------------------------------
    // Intersections
    // --------------------------------------------------------
    for (const auto& point : cityPoints)
    {
        draw->AddCircleFilled(
            ToScreen(canvasPos, point.pos),
            4.0f,
            IM_COL32(180, 180, 180, 255)
        );
    }

    // --------------------------------------------------------
    // Destination marker (pulsing ring)
    // --------------------------------------------------------
    if (destinationNode >= 0 && destinationNode < static_cast<int>(cityPoints.size()))
    {
        ImVec2 d = ToScreen(canvasPos, cityPoints[destinationNode].pos);
        float pulse = 8.0f + 3.0f * sinf(static_cast<float>(ImGui::GetTime()) * 4.0f);
        draw->AddCircle(d, pulse, IM_COL32(60, 255, 140, 255), 16, 2.5f);
        draw->AddCircleFilled(d, 3.5f, IM_COL32(60, 255, 140, 255));
        draw->AddText(ImVec2(d.x + 8, d.y - 8),
                      IM_COL32(60, 255, 140, 255), "DEST");
    }

    // --------------------------------------------------------
    // Carrier
    // --------------------------------------------------------
    ImVec2 carrier = ToScreen(canvasPos, CarrierMapPos());

    if (payload == 1)      DrawBike(draw, carrier);
    else if (payload == 2) DrawCar(draw, carrier);
    else                   DrawTruck(draw, carrier);

    // --------------------------------------------------------
    // "REROUTING" pulse over the carrier
    // --------------------------------------------------------
    if (rerouteFlash > 0.0f)
    {
        int alpha = static_cast<int>(255 * (rerouteFlash / 1.6f));
        if (alpha < 0) alpha = 0; if (alpha > 255) alpha = 255;
        draw->AddText(ImVec2(carrier.x + 10, carrier.y - 22),
                      IM_COL32(255, 220, 80, alpha), "REROUTING");
    }

    ImGui::EndChild();
}

// ============================================================
// DRAW
// ===================================


void Drawing::Draw()
{
    if (!isActive())
        return;

    ImGui::SetNextWindowSize(
        vWindowSize,
        ImGuiCond_Once
    );

    ImGui::SetNextWindowBgAlpha(1.0f);

    ImGui::Begin(
        lpWindowName,
        &bDraw,
        WindowFlags
    );

    // ========================================================
    // ONE TAB ONLY
    // ========================================================

    if (ImGui::BeginTabBar("MainTabs"))
    {
        if (ImGui::BeginTabItem("City Map"))
        {
            // ------------------------------------------------
            // Header
            // ------------------------------------------------

            ImGui::Text(
                "Random City Delivery Simulator"
            );

            ImGui::Separator();

            // ------------------------------------------------
            // Payload slider
            // ------------------------------------------------

            ImGui::Text("Payload");

            ImGui::SliderInt(
                "##Payload",
                &payload,
                1,
                3
            );

            // ------------------------------------------------
            // Payload information
            // ------------------------------------------------

            if (payload == 1)
            {
                ImGui::TextColored(
                    ImVec4(
                        0.2f,
                        0.8f,
                        1.0f,
                        1.0f
                    ),
                    "Stage 1 - BIKE"
                );
            }
            else if (payload == 2)
            {
                ImGui::TextColored(
                    ImVec4(
                        1.0f,
                        0.8f,
                        0.2f,
                        1.0f
                    ),
                    "Stage 2 - CAR"
                );
            }
            else
            {
                ImGui::TextColored(
                    ImVec4(
                        1.0f,
                        0.3f,
                        0.2f,
                        1.0f
                    ),
                    "Stage 3 - TRUCK"
                );
            }

            ImGui::Separator();

            // ------------------------------------------------
            // Dynamic routing controls
            // ------------------------------------------------

            if (ImGui::Button("Generate New City"))
                GenerateCity();

            ImGui::SameLine();
            if (ImGui::Button("New Delivery"))
                PickNewDestination();

            ImGui::SameLine();
            if (ImGui::Button("Trigger Traffic"))
                TriggerTrafficEvent(true);

            ImGui::SameLine();
            if (ImGui::Button("Reopen All Roads"))
            {
                for (auto& r : cityRoads) { r.blocked = false; r.congestion = 1.0f; }
                SoftReroute(false);
            }

            ImGui::Checkbox("Auto traffic events", &autoTraffic);

            // Live routing stats
            int blocked = 0;
            for (const auto& r : cityRoads)
                if (r.blocked) blocked++;

            int hops = carrierRoute.empty()
                ? 0 : static_cast<int>(carrierRoute.size()) - 1 - routeStep;
            if (hops < 0) hops = 0;

            ImGui::Text("Roads: %d   Closed: %d   Reroutes: %d",
                static_cast<int>(cityRoads.size()), blocked, rerouteCount);
            ImGui::Text("Destination: node %d   Hops left: %d",
                destinationNode, hops);
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f),
                "Left-click map = set destination   |   Right-click road = close/open");

            ImGui::Separator();

            // ------------------------------------------------
            // MAP
            // ------------------------------------------------

            DrawCityMap();

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    // ========================================================
    // FILE ERROR
    // ========================================================

    if (file_not_found)
    {
        ImGui::Begin(
            "Error",
            &bDraw,
            WindowFlags
        );

        ImGui::TextColored(
            ImVec4(
                1.0f,
                0.0f,
                0.0f,
                1.0f
            ),
            "File not found."
        );

        ImGui::End();
    }

    ImGui::End();

#ifdef _WINDLL

    if (GetAsyncKeyState(VK_INSERT) & 1)
        bDraw = !bDraw;

#endif
}