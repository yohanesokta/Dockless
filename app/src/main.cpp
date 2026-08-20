#include <windows.h>
#include "app/Window.h"
#include "app/Config.h"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(pCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);
    
    // Initialize COM for WIC and Direct2D
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return -1;

    // Prevent multiple instances of the app
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"Global\\DocklessAppSingleInstanceMutex");
    if (hMutex != nullptr && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND hwndExisting = FindWindowW(L"DocklessWindowClass", nullptr);
        if (hwndExisting) {
            ShowWindow(hwndExisting, SW_SHOW);
            ShowWindow(hwndExisting, SW_RESTORE);
            SetForegroundWindow(hwndExisting);
        }
        CloseHandle(hMutex);
        CoUninitialize();
        return 0;
    }
    
    try {
        // Load configuration
        AppConfig config = AppConfig::Load(L"../../config.xml");

        Window window(1220, 770, L"Dockless", config);

        while (window.ProcessMessages()) {
            Sleep(1);
        }
    } catch (const std::exception& e) {
        // Convert char* to wchar_t* for MessageBoxW
        int len = MultiByteToWideChar(CP_UTF8, 0, e.what(), -1, nullptr, 0);
        std::wstring wmsg(len, 0);
        MultiByteToWideChar(CP_UTF8, 0, e.what(), -1, &wmsg[0], len);
        MessageBoxW(nullptr, wmsg.c_str(), L"Application Crash", MB_ICONERROR | MB_OK);
    } catch (...) {
        MessageBoxW(nullptr, L"Unknown application error occurred", L"Error", MB_ICONERROR | MB_OK);
    }

    if (hMutex) {
        CloseHandle(hMutex);
    }
    CoUninitialize();
    return 0;
}
