﻿#define WIN32_LEAN_AND_MEAN  // Exclude rarely-used stuff from Windows headers
#include "httplib.h"
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include <atomic>
#include <d3d9.h>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <tchar.h>
#include <thread>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

struct WeatherData {
	std::string cityName;
	std::string condition;
	int temperature;
	int humidity;
	double windSpeed;
	bool isFavorate = FALSE;
};

// Data
static LPDIRECT3D9              g_pD3D = nullptr;
static LPDIRECT3DDEVICE9        g_pd3dDevice = nullptr;
static bool                     g_DeviceLost = false;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static D3DPRESENT_PARAMETERS    g_d3dpp = {};
using json = nlohmann::json;

// Shared data for the weather fetching thread
std::string city = "Enter City Name...";
std::string weatherInfo = "Weather data will appear here.";
std::atomic<bool> fetchingWeather{ false };
std::vector<WeatherData> favorates;
WeatherData data;
std::vector<std::string> cityWeatherInfo;
bool showFavoritesWindow = FALSE;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void ResetDevice();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

void FetchWeatherData(const std::string& city) {


	cityWeatherInfo.clear();

	//Fetching data
	httplib::Client client("http://api.openweathermap.org");
	std::string apiKey = "1b47309fcd7ed75fb6306aa00625578f";
	std::string url = "/data/2.5/weather?q=" + city + "&appid=" + apiKey + "&units=metric";

	auto res = client.Get(url.c_str());

	if (res && res->status == 200) {
		auto jsonData = json::parse(res->body);


		data.cityName = jsonData["name"];
		data.condition = jsonData["weather"][0]["main"];
		data.temperature = jsonData["main"]["temp"];
		data.humidity = jsonData["main"]["humidity"];
		data.windSpeed = jsonData["wind"]["speed"];

		std::ostringstream oss;
		//Dividing data to enter the vector
		oss << data.cityName;
		cityWeatherInfo.push_back(oss.str());
		oss.str("");  // Clear the stream content
		oss.clear();  // Clear any error flags

		oss << data.condition;
		cityWeatherInfo.push_back(oss.str());
		oss.str("");
		oss.clear();


		oss << data.temperature << "c";
		cityWeatherInfo.push_back(oss.str());
		oss.str("");
		oss.clear();


		oss << "Humadity: " << data.humidity << "%";
		cityWeatherInfo.push_back(oss.str());
		oss.str("");
		oss.clear();


		oss << "Wind Speed: " << data.windSpeed << " m/s";
		cityWeatherInfo.push_back(oss.str());
		oss.str("");
		oss.clear();


	}
	else {
		weatherInfo = "Failed to fetch data for " + city;
		cityWeatherInfo.push_back(weatherInfo.c_str());
	}

	fetchingWeather = false;  // Mark fetching as complete


}

void addToFavorates(std::vector<WeatherData>& favorate, const WeatherData& data) {
	// Check if the weather data is already in the favorites list
	auto it = std::find_if(favorate.begin(), favorate.end(), [&data](const WeatherData& wd) {
		return wd.cityName == data.cityName &&
			wd.condition == data.condition &&
			wd.temperature == data.temperature &&
			wd.humidity == data.humidity &&
			wd.windSpeed == data.windSpeed;
		});

	// If it's not in the list, add it
	if (it == favorate.end()) {
		favorate.push_back(data);
	}
}

void removeFromFavorates(std::vector<WeatherData>& favorate, const WeatherData& data) {
	// Find the weather data in the favorites list by matching the whole WeatherData object
	auto it = std::remove_if(favorate.begin(), favorate.end(), [&data](const WeatherData& wd) {
		return wd.cityName == data.cityName &&
			wd.condition == data.condition &&
			wd.temperature == data.temperature &&
			wd.humidity == data.humidity &&
			wd.windSpeed == data.windSpeed;
		});

	// If found, remove it from the vector
	if (it != favorate.end()) {
		favorate.erase(it, favorate.end());
	}
}


// Main code
int main(int, char**)
{
	// Create application window
	WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
	::RegisterClassExW(&wc);
	HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Dear ImGui DirectX9 Example", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, nullptr, nullptr, wc.hInstance, nullptr);

	// Initialize Direct3D
	if (!CreateDeviceD3D(hwnd))
	{
		CleanupDeviceD3D();
		::UnregisterClassW(wc.lpszClassName, wc.hInstance);
		return 1;
	}

	// Show the window
	::ShowWindow(hwnd, SW_SHOWDEFAULT);
	::UpdateWindow(hwnd);

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX9_Init(g_pd3dDevice);

	//Load Font
	ImFont* defaultFont = io.Fonts->AddFontFromFileTTF("../testing\\vendor\\Fonts\\MADE TOMMY Black_PERSONAL USE.otf", 15.0f);
	ImFont* smallFont = io.Fonts->AddFontFromFileTTF("../testing\\vendor\\Fonts\\MADE TOMMY Black_PERSONAL USE.otf", 20.0f);
	ImFont* mediumFont = io.Fonts->AddFontFromFileTTF("../testing\\vendor\\Fonts\\MADE TOMMY Black_PERSONAL USE.otf", 32.0f);
	ImFont* largeFont = io.Fonts->AddFontFromFileTTF("../testing\\vendor\\Fonts\\MADE TOMMY Black_PERSONAL USE.otf", 70.0f);

	// Main loop
	bool done = false;
	while (!done)
	{
		// Poll and handle messages (inputs, window resize, etc.)
		MSG msg;
		while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
				done = true;
		}
		if (done)
			break;

		// Handle lost D3D9 device
		if (g_DeviceLost)
		{
			HRESULT hr = g_pd3dDevice->TestCooperativeLevel();
			if (hr == D3DERR_DEVICELOST)
			{
				::Sleep(10);
				continue;
			}
			if (hr == D3DERR_DEVICENOTRESET)
				ResetDevice();
			g_DeviceLost = false;
		}

		// Handle window resize (we don't resize directly in the WM_SIZE handler)
		if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
		{
			g_d3dpp.BackBufferWidth = g_ResizeWidth;
			g_d3dpp.BackBufferHeight = g_ResizeHeight;
			g_ResizeWidth = g_ResizeHeight = 0;
			ResetDevice();
		}


		ImGui_ImplDX9_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		// Calculate the size of the window
		ImVec2 windowSize = ImGui::GetWindowSize();
		float windowWidth = windowSize.x;
		float windowHeight = windowSize.y;

		// Begin the window
		ImGui::SetNextWindowPos(ImVec2(450, 100));
		ImGui::SetNextWindowSize(ImVec2(347, 492));
		ImGui::Begin("Weather Application");

		// Input field for entering a city name
		ImGui::SetCursorPos(ImVec2(12, 20));
		char cityBuffer[128];
		strncpy_s(cityBuffer, city.c_str(), sizeof(cityBuffer));

		if (ImGui::InputTextMultiline("##multiline", cityBuffer, sizeof(cityBuffer), ImVec2(256, 24))) {
			city = cityBuffer;
		}

		// Fetch Weather Button (right-aligned)
		ImGui::SetCursorPos(ImVec2(280, 20));
		if (ImGui::Button("Search", ImVec2(48, 24)) && !fetchingWeather) {
			fetchingWeather = true;
			std::thread(FetchWeatherData, city).detach();  // Start fetching in a new thread and detach it
		}

		// Centering function with Y position adjustment
		auto CenterTextAtY = [&](const std::string& text, ImFont* font, float yPos) {
			ImGui::PushFont(font);
			float textWidth = ImGui::CalcTextSize(text.c_str()).x;
			float textX = (windowWidth - textWidth) / 2.5f;
			ImGui::SetCursorPos(ImVec2(textX, yPos));
			ImGui::Text("%s", text.c_str());
			ImGui::PopFont();
			};

		// Display the city name
		if (!cityWeatherInfo.empty()) {
			CenterTextAtY(cityWeatherInfo[0], mediumFont, 190.0f);  
		}

		// Display weather condition
		if (cityWeatherInfo.size() >= 2) {
			CenterTextAtY(cityWeatherInfo[1], smallFont, 160.0f);  
		}

		// Display the temperature
		if (cityWeatherInfo.size() >= 3) {
			CenterTextAtY(cityWeatherInfo[2], largeFont, 100.0f);  
		}

		// Display humidity and wind speed on the same line
		if (cityWeatherInfo.size() >= 4) {
			ImGui::PushFont(smallFont);
			ImGui::SetCursorPos(ImVec2(25, 250));  // Position for the humidity
			ImGui::Text("%s", cityWeatherInfo[3].c_str());
			ImGui::SameLine(0.0f, 10.0f);  // Add some spacing between humidity and wind speed
			ImGui::Text("%s", cityWeatherInfo[4].c_str());
			ImGui::PopFont();
		}

		if (fetchingWeather) {
			CenterTextAtY("Fetching weather data...", smallFont, 280.0f); 
		}

		// Favorite Button 
		ImGui::SetCursorPos(ImVec2(0, 450));
		if (ImGui::Button("Favorite", ImVec2(91, 34))) {
			if (data.isFavorate) {
				data.isFavorate = false;
				removeFromFavorates(favorates, data);
			}
			else {
				data.isFavorate = true;
				addToFavorates(favorates, data);
			}
		}

		// Favorites List Button 
		ImGui::SetCursorPos(ImVec2(237, 450));
		if (ImGui::Button("Favorites List", ImVec2(110, 34))) {
			showFavoritesWindow = true;  // Toggle the display of the favorites window
		}

		ImGui::End();


		if (showFavoritesWindow) {
			ImGui::SetNextWindowPos(ImVec2(797, 100));
			ImGui::Begin("Favorites", &showFavoritesWindow);  // Window will automatically close if the 'X' is clicked

			ImGui::Text("Favorite Weather Data:");

			if (favorates.empty()) {
				ImGui::Text("No favorites yet.");
			}
			else {
				char c = '%';
				for (const auto& data : favorates) {
					ImGui::Text("City: %s", data.cityName.c_str());
					ImGui::Text("Temperature: %dc", data.temperature);
					ImGui::Text("Condition: %s", data.condition.c_str());
					ImGui::Text("Humidity: %d%c", data.humidity, c);
					ImGui::Text("Wind Speed: %.2f m/s", data.windSpeed);

					ImGui::SameLine();  // Position the remove button on the same line as the last text
					if (ImGui::Button(("X##" + data.cityName).c_str())) {
						removeFromFavorates(favorates, data);  // Remove the entry and update the iterator
					}

					ImGui::Separator();  // Separate each favorite entry
				}
			}

			ImGui::End();
		}

		// Rendering
		ImGui::EndFrame();
		g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
		g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
		g_pd3dDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
		D3DCOLOR clear_col_dx = D3DCOLOR_RGBA((int)(0.45f * 255.0f), (int)(0.55f * 255.0f), (int)(0.60f * 255.0f), (int)(1.0f * 255.0f));
		g_pd3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clear_col_dx, 1.0f, 0);
		if (g_pd3dDevice->BeginScene() >= 0)
		{
			ImGui::Render();
			ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
			g_pd3dDevice->EndScene();
		}
		HRESULT result = g_pd3dDevice->Present(nullptr, nullptr, nullptr, nullptr);
		if (result == D3DERR_DEVICELOST)
			g_DeviceLost = true;
	}

	// Cleanup
	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	CleanupDeviceD3D();
	::DestroyWindow(hwnd);
	::UnregisterClassW(wc.lpszClassName, wc.hInstance);

	return 0;
}

// Helper functions
bool CreateDeviceD3D(HWND hWnd)
{
	if ((g_pD3D = Direct3DCreate9(D3D_SDK_VERSION)) == nullptr)
		return false;

	// Create the D3DDevice
	ZeroMemory(&g_d3dpp, sizeof(g_d3dpp));
	g_d3dpp.Windowed = TRUE;
	g_d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	g_d3dpp.BackBufferFormat = D3DFMT_UNKNOWN; // Need to use an explicit format with alpha if needing per-pixel alpha composition.
	g_d3dpp.EnableAutoDepthStencil = TRUE;
	g_d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
	g_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;           // Present with vsync
	if (g_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &g_d3dpp, &g_pd3dDevice) < 0)
		return false;

	return true;
}

void CleanupDeviceD3D()
{
	if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
	if (g_pD3D) { g_pD3D->Release(); g_pD3D = nullptr; }
}

void ResetDevice()
{
	ImGui_ImplDX9_InvalidateDeviceObjects();
	HRESULT hr = g_pd3dDevice->Reset(&g_d3dpp);
	if (hr == D3DERR_INVALIDCALL)
		IM_ASSERT(0);
	ImGui_ImplDX9_CreateDeviceObjects();
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_SIZE:
		if (wParam == SIZE_MINIMIZED)
			return 0;
		g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
		g_ResizeHeight = (UINT)HIWORD(lParam);
		return 0;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;
	case WM_DESTROY:
		::PostQuitMessage(0);
		return 0;
	}
	return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

