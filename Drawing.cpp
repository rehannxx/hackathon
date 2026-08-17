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
// RANDOM CITY MAP
// ============================================================

struct RoadPoint
{
    ImVec2 pos;
};

struct Road
{
    ImVec2 start;
    ImVec2 end;
};

static std::vector<RoadPoint> cityPoints;
static std::vector<Road> cityRoads;

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

// Carrier position
static int carrierRoad = 0;
static float carrierProgress = 0.0f;

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
// GENERATE CITY
// ============================================================

static void GenerateCity()
{
    cityPoints.clear();
    cityRoads.clear();

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
    // Generate intersections
    // --------------------------------------------------------

    for (int y = 0; y < rows; y++)
    {
        for (int x = 0; x < columns; x++)
        {
            // Slight random distortion makes every map different
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
    // Horizontal roads
    // --------------------------------------------------------

    for (int y = 0; y < rows; y++)
    {
        for (int x = 0; x < columns - 1; x++)
        {
            int index1 = y * columns + x;
            int index2 = y * columns + x + 1;

            Road road;

            road.start = cityPoints[index1].pos;
            road.end = cityPoints[index2].pos;

            cityRoads.push_back(road);
        }
    }

    // --------------------------------------------------------
    // Vertical roads
    // --------------------------------------------------------

    for (int y = 0; y < rows - 1; y++)
    {
        for (int x = 0; x < columns; x++)
        {
            int index1 = y * columns + x;
            int index2 = (y + 1) * columns + x;

            Road road;

            road.start = cityPoints[index1].pos;
            road.end = cityPoints[index2].pos;

            cityRoads.push_back(road);
        }
    }

    // --------------------------------------------------------
    // Add some random diagonal roads
    // --------------------------------------------------------

    int diagonalRoads = RandomInt(4, 10);

    for (int i = 0; i < diagonalRoads; i++)
    {
        int index1 = RandomInt(0, static_cast<int>(cityPoints.size()) - 1);
        int index2 = RandomInt(0, static_cast<int>(cityPoints.size()) - 1);

        if (index1 == index2)
            continue;

        Road road;

        road.start = cityPoints[index1].pos;
        road.end = cityPoints[index2].pos;

        cityRoads.push_back(road);
    }

    // Start carrier on a random road
    carrierRoad = RandomInt(
        0,
        static_cast<int>(cityRoads.size()) - 1
    );

    carrierProgress = RandomFloat(0.0f, 1.0f);

    cityGenerated = true;
}

// ============================================================
// GET CARRIER POSITION
// ============================================================

static ImVec2 GetCarrierPosition()
{
    if (cityRoads.empty())
        return ImVec2(0, 0);

    if (carrierRoad >= static_cast<int>(cityRoads.size()))
        carrierRoad = 0;

    Road& road = cityRoads[carrierRoad];

    float x =
        road.start.x +
        (road.end.x - road.start.x) * carrierProgress;

    float y =
        road.start.y +
        (road.end.y - road.start.y) * carrierProgress;

    return ImVec2(x, y);
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

static void DrawCityMap()
{
    ImGui::BeginChild(
        "CityMap",
        ImVec2(760, 550),
        true
    );

    ImDrawList* draw =
        ImGui::GetWindowDrawList();

    ImVec2 canvasPos =
        ImGui::GetWindowPos();

    // --------------------------------------------------------
    // Generate city once
    // --------------------------------------------------------

    if (!cityGenerated)
        GenerateCity();

    // --------------------------------------------------------
    // Background
    // --------------------------------------------------------

    ImVec2 bgMin =
        ImVec2(
            canvasPos.x + 5,
            canvasPos.y + 5
        );

    ImVec2 bgMax =
        ImVec2(
            canvasPos.x + 755,
            canvasPos.y + 545
        );

    draw->AddRectFilled(
        bgMin,
        bgMax,
        IM_COL32(22, 25, 30, 255),
        4.0f
    );

    // --------------------------------------------------------
    // Roads
    // --------------------------------------------------------

    for (const auto& road : cityRoads)
    {
        ImVec2 start =
            ImVec2(
                canvasPos.x + road.start.x,
                canvasPos.y + road.start.y - 65
            );

        ImVec2 end =
            ImVec2(
                canvasPos.x + road.end.x,
                canvasPos.y + road.end.y - 65
            );

        // Road shadow
        draw->AddLine(
            start,
            end,
            IM_COL32(10, 10, 10, 255),
            9.0f
        );

        // Road
        draw->AddLine(
            start,
            end,
            IM_COL32(75, 75, 80, 255),
            6.0f
        );

        // Road center
        draw->AddLine(
            start,
            end,
            IM_COL32(150, 150, 150, 255),
            1.0f
        );
    }

    // --------------------------------------------------------
    // Intersections
    // --------------------------------------------------------

    for (const auto& point : cityPoints)
    {
        ImVec2 p =
            ImVec2(
                canvasPos.x + point.pos.x,
                canvasPos.y + point.pos.y - 65
            );

        draw->AddCircleFilled(
            p,
            4.0f,
            IM_COL32(180, 180, 180, 255)
        );
    }

    // --------------------------------------------------------
    // Buildings / city blocks
    // --------------------------------------------------------

    for (const auto& point : cityPoints)
    {
        // Random-looking building placement around
        // intersections.
        float offsetX =
            RandomFloat(-22.0f, 22.0f);

        float offsetY =
            RandomFloat(-22.0f, 22.0f);

        ImVec2 buildingPos =
            ImVec2(
                canvasPos.x +
                point.pos.x +
                offsetX,

                canvasPos.y +
                point.pos.y -
                65 +
                offsetY
            );

        float width =
            RandomFloat(10.0f, 22.0f);

        float height =
            RandomFloat(8.0f, 18.0f);

        draw->AddRectFilled(
            ImVec2(
                buildingPos.x - width,
                buildingPos.y - height
            ),
            ImVec2(
                buildingPos.x + width,
                buildingPos.y + height
            ),
            IM_COL32(
                RandomInt(45, 85),
                RandomInt(45, 85),
                RandomInt(50, 95),
                255
            ),
            2.0f
        );
    }

    // --------------------------------------------------------
    // Carrier
    // --------------------------------------------------------

    ImVec2 carrier =
        GetCarrierPosition();

    carrier.x += canvasPos.x;
    carrier.y += canvasPos.y - 65;

    if (payload == 1)
    {
        DrawBike(draw, carrier);
    }
    else if (payload == 2)
    {
        DrawCar(draw, carrier);
    }
    else
    {
        DrawTruck(draw, carrier);
    }

    // --------------------------------------------------------
    // Move carrier
    // --------------------------------------------------------

    carrierProgress +=
        ImGui::GetIO().DeltaTime * 0.08f;

    if (carrierProgress >= 1.0f)
    {
        carrierProgress = 0.0f;

        carrierRoad =
            RandomInt(
                0,
                static_cast<int>(
                    cityRoads.size()
                    ) - 1
            );
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

            // ------------------------------------------------
            // Generate new city button
            // ------------------------------------------------

            if (ImGui::Button("Generate New City"))
            {
                GenerateCity();
            }

            ImGui::SameLine();

            ImGui::Text(
                "Roads: %d",
                static_cast<int>(
                    cityRoads.size()
                    )
            );

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