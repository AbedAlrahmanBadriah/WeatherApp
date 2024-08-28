
#define WIN32_LEAN_AND_MEAN  // Exclude rarely-used stuff from Windows headers
#include "httplib.h"
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include <d3d9.h>
#include <tchar.h>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include "httplib.h"
#include <nlohmann/json.hpp>

struct WeatherData {
    std::string cityName;
    double temperature;
    double humidity;
    double windSpeed;
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
std::mutex weatherMutex;
std::atomic<bool> fetchingWeather{ false };

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void ResetDevice();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

void FetchWeatherData(const std::string& city) {
    httplib::Client client("http://api.openweathermap.org");
    std::string apiKey = "1b47309fcd7ed75fb6306aa00625578f"; // Replace with your actual OpenWeather API key
    std::string url = "/data/2.5/weather?q=" + city + "&appid=" + apiKey + "&units=metric";

    auto res = client.Get(url.c_str());
    if (res && res->status == 200) {
        auto jsonData = json::parse(res->body);

        WeatherData data;
        data.cityName = jsonData["name"];
        data.temperature = jsonData["main"]["temp"];
        data.humidity = jsonData["main"]["humidity"];
        data.windSpeed = jsonData["wind"]["speed"];

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);  // Set the precision to 2 decimal places

        oss << "City: " << data.cityName << "\n";
        oss << "Temp: " << data.temperature << "C\n";
        oss << "Humidity: " << data.humidity << "%\n";
        oss << "Wind Speed: " << data.windSpeed << " m/s";

        std::lock_guard<std::mutex> guard(weatherMutex);
        weatherInfo = oss.str();  // Assign the formatted string to weatherInfo

    }
    else {
        std::lock_guard<std::mutex> guard(weatherMutex);
        weatherInfo = "Failed to fetch data for " + city;
    }

    fetchingWeather = false;  // Mark fetching as complete
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

        // Start the Dear ImGui frame
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // UI code
        
        float windowHeight = ImGui::GetWindowSize().y;  // Get the height of the window
        float windowWidth = ImGui::GetWindowSize().x;   //Get the width of the window
        float widgetHeight = ImGui::GetFrameHeight();  // Get the height of a standard ImGui widget
        float widgetWidth = ImGui::CalcTextSize("Weather Application").x + ImGui::GetStyle().FramePadding.x * 2;


        ImVec2 windowSize = ImGui::GetWindowSize();  // Get the current window size

        // Calculate widget positions based on window size
        float buttonX = windowSize.x * 0.5f - 75;  // Center horizontally (assuming a button width of 150 pixels)
        float buttonY = windowSize.y * 0.7f;
        

        //ImGui::SetNextWindowPos(ImVec2(windowHeight * 0.5f, windowWidth * 0.5f));
        ImGui::SetNextWindowPos(ImVec2(windowHeight/2, windowWidth/2));
        ImGui::SetNextWindowSize(ImVec2(275, 500));
        
        ImGui::Begin("Weather Application");




















        // Input field for entering a city name
        ImGui::SetNextItemWidth(widgetWidth);
        ImGui::SetCursorPos(ImVec2(widgetWidth/4,20));
        char cityBuffer[128];
        strncpy_s(cityBuffer, city.c_str(), sizeof(cityBuffer));

        if (ImGui::InputText("##City", cityBuffer, sizeof(cityBuffer))) {
            city = cityBuffer;
        }



















        // Fetch Weather Button
        ImGui::SetCursorPos(ImVec2(widgetWidth+40, 20));
        if (ImGui::Button("Search") && !fetchingWeather)
        {
            fetchingWeather = true;
            std::thread(FetchWeatherData, city).detach();  // Start fetching in a new thread and detach it
        }














        ImGui::SetCursorPos(ImVec2(widgetWidth /2, 60));
         //Display the weather information
        {
            std::lock_guard<std::mutex> lock(weatherMutex);
            ImGui::Text("%s", weatherInfo.c_str());
        }

        if (fetchingWeather) {
            ImGui::Text("Fetching weather data...");
        }











        ImGui::SetCursorPos(ImVec2(10,470));
        ImGui::Button("Favorite");

        std::cout << "hello";


        ImGui::SetCursorPos(ImVec2(widgetWidth + 20, 470));
        ImGui::Button("Favorites List");

        ImGui::Button("Favorites List");

        ImGui::End();

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
