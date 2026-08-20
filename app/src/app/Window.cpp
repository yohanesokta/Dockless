#include "Window.h"
#include "../system/DwmManager.h"
#include <dwmapi.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <stdexcept>

static std::string WindowWideToUtf8(const std::wstring& wstr);
static std::wstring WindowUtf8ToWide(const std::string& str);

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

static bool IsQemuProcessRunning() {
    bool exists = false;
    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(PROCESSENTRY32W);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        if (Process32FirstW(snapshot, &entry)) {
            do {
                if (wcscmp(entry.szExeFile, L"qemu-system-x86_64.exe") == 0) {
                    exists = true;
                    break;
                }
            } while (Process32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
    }
    return exists;
}


Window::Window(int width, int height, const wchar_t* title, const AppConfig& config) 
    : m_width(width), m_height(height), m_hwnd(nullptr), m_dockerClient(config.dockerEndpoint, config.dockerPort), m_config(config), m_isCheckingStatus(true) {
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"DocklessWindowClass";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    RegisterClassExW(&wc);

    m_hwnd = CreateWindowExW(
        WS_EX_NOREDIRECTIONBITMAP,      // Required for DirectComposition
        wc.lpszClassName,               // Window class
        title,                          // Window text
        WS_OVERLAPPEDWINDOW,            // Window style
        CW_USEDEFAULT, CW_USEDEFAULT, width, height, // Size and position
        nullptr,       // Parent window    
        nullptr,       // Menu
        wc.hInstance,  // Instance handle
        this           // Additional application data
    );

    if (m_hwnd == nullptr) {
        throw std::runtime_error("Failed to create window");
    }

    DwmManager::EnableDarkMode(m_hwnd);
    DwmManager::EnableRoundedCorners(m_hwnd);
    DwmManager::EnableMica(m_hwnd);

    // Extend the frame to client area for Acrylic glass effect
    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(m_hwnd, &margins);

    // Force frame calculation to trigger WM_NCCALCSIZE and correct layout sizes immediately
    SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

    InitializeDirect2D();

    m_dockerClient.Connect();
    
    // Auto-start VM if not running
    if (!IsQemuProcessRunning()) {
        m_isStartingVmDialogActive = true;
        SetTimer(m_hwnd, 2, 16, nullptr); // Start 60fps spinner animation
        StartDockerVM();
    }

    SetTimer(m_hwnd, 1, 1000, nullptr); // Timer ID 1, 1000ms
    // Post message to run first fetch immediately
    PostMessage(m_hwnd, WM_TIMER, 1, 0);

    // Initialize System Tray Icon
    NOTIFYICONDATAW nid = { sizeof(nid) };
    nid.hWnd = m_hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIconW(GetModuleHandle(nullptr), L"IDI_APP_ICON");
    wcscpy_s(nid.szTip, L"Dockless");
    Shell_NotifyIconW(NIM_ADD, &nid);

    ShowWindow(m_hwnd, SW_SHOWDEFAULT);
}

Window::~Window() {
    DestroyWindow(m_hwnd);
}

void Window::InitializeDirect2D() {
    HRESULT hr = S_OK;

    // Initialize D3D11 Device
    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
    ComPtr<ID3D11DeviceContext> d3dContext;
    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, 
        featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION, &m_d3dDevice, nullptr, &d3dContext);
    
    if (FAILED(hr)) {
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, 
            featureLevels, ARRAYSIZE(featureLevels), D3D11_SDK_VERSION, &m_d3dDevice, nullptr, &d3dContext);
        if (FAILED(hr)) throw std::runtime_error("D3D11CreateDevice failed");
    }

    // Get DXGI Factory
    ComPtr<IDXGIDevice> dxgiDevice;
    hr = m_d3dDevice.As(&dxgiDevice);
    if (FAILED(hr)) throw std::runtime_error("Failed to get IDXGIDevice");

    ComPtr<IDXGIAdapter> dxgiAdapter;
    hr = dxgiDevice->GetAdapter(&dxgiAdapter);
    if (FAILED(hr)) throw std::runtime_error("Failed to get IDXGIAdapter");

    ComPtr<IDXGIFactory2> dxgiFactory;
    hr = dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory));
    if (FAILED(hr)) throw std::runtime_error("Failed to get IDXGIFactory2");

    // Create D2D Factory
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), reinterpret_cast<void**>(m_d2dFactory1.GetAddressOf()));
    if (FAILED(hr)) throw std::runtime_error("D2D1CreateFactory failed");
    
    hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(m_dwriteFactory.GetAddressOf()));
    if (FAILED(hr)) throw std::runtime_error("DWriteCreateFactory failed");

    // Create D2D Device & Context
    hr = m_d2dFactory1->CreateDevice(dxgiDevice.Get(), &m_d2dDevice);
    if (FAILED(hr)) throw std::runtime_error("CreateDevice failed");
    
    hr = m_d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_d2dContext);
    if (FAILED(hr)) throw std::runtime_error("CreateDeviceContext failed");

    // Create DXGI SwapChain for Composition
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {0};
    swapChainDesc.Width = rc.right - rc.left;
    swapChainDesc.Height = rc.bottom - rc.top;
    swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED; // True transparency!
    swapChainDesc.Flags = 0;

    hr = dxgiFactory->CreateSwapChainForComposition(m_d3dDevice.Get(), &swapChainDesc, nullptr, &m_swapChain);
    if (FAILED(hr)) throw std::runtime_error("CreateSwapChainForComposition failed");

    // Initialize DirectComposition
    hr = DCompositionCreateDevice(dxgiDevice.Get(), IID_PPV_ARGS(&m_dcompDevice));
    if (FAILED(hr)) throw std::runtime_error("DCompositionCreateDevice failed");

    hr = m_dcompDevice->CreateTargetForHwnd(m_hwnd, TRUE, &m_dcompTarget);
    if (FAILED(hr)) throw std::runtime_error("CreateTargetForHwnd failed");

    hr = m_dcompDevice->CreateVisual(&m_dcompVisual);
    if (FAILED(hr)) throw std::runtime_error("CreateVisual failed");

    m_dcompVisual->SetContent(m_swapChain.Get());
    m_dcompTarget->SetRoot(m_dcompVisual.Get());
    m_dcompDevice->Commit();

    // Link SwapChain to D2D Context
    ComPtr<IDXGISurface> dxgiBackBuffer;
    hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&dxgiBackBuffer));
    if (FAILED(hr)) throw std::runtime_error("GetBuffer failed");
    
    D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
    );
    hr = m_d2dContext->CreateBitmapFromDxgiSurface(dxgiBackBuffer.Get(), &bitmapProperties, &m_d2dTargetBitmap);
    if (FAILED(hr)) throw std::runtime_error("CreateBitmapFromDxgiSurface failed");
    
    m_d2dContext->SetTarget(m_d2dTargetBitmap.Get());
}

void Window::Render() {
    if (!m_d2dContext) return;

    m_d2dContext->BeginDraw();
    
    // Clear target with transparent background to allow DWM acrylic to serve as base
    m_d2dContext->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

    RECT rc;
    GetClientRect(m_hwnd, &rc);
    float width = static_cast<float>(rc.right - rc.left);
    float height = static_cast<float>(rc.bottom - rc.top);

    // Draw a beautiful WinUI-style dark slate gradient background
    ID2D1GradientStopCollection* pGradientStops = nullptr;
    D2D1_GRADIENT_STOP gradientStops[2];
    gradientStops[0].position = 0.0f;
    gradientStops[0].color = D2D1::ColorF(0.07f, 0.11f, 0.19f, 0.85f); // WinUI Deep Slate Blue (RGB 17, 28, 48)
    gradientStops[1].position = 1.0f;
    gradientStops[1].color = D2D1::ColorF(0.12f, 0.16f, 0.23f, 0.90f); // WinUI Dark Slate Grey (RGB 30, 41, 59)

    HRESULT hres = m_d2dContext->CreateGradientStopCollection(
        gradientStops,
        2,
        D2D1_GAMMA_2_2,
        D2D1_EXTEND_MODE_CLAMP,
        &pGradientStops
    );

    if (SUCCEEDED(hres)) {
        ID2D1LinearGradientBrush* pLinearGradientBrush = nullptr;
        hres = m_d2dContext->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(
                D2D1::Point2F(0.0f, 0.0f),
                D2D1::Point2F(width, height)
            ),
            pGradientStops,
            &pLinearGradientBrush
        );

        if (SUCCEEDED(hres)) {
            m_d2dContext->FillRectangle(D2D1::RectF(0.0f, 0.0f, width, height), pLinearGradientBrush);
            pLinearGradientBrush->Release();
        }
        pGradientStops->Release();
    }

    float statusBarHeight = 28.0f;
    float titlebarHeight = 40.0f;
    float mainContentHeight = height - statusBarHeight - titlebarHeight;

    // Apply translation to shift all views and sidebar down to leave space for custom titlebar
    m_d2dContext->SetTransform(D2D1::Matrix3x2F::Translation(0, titlebarHeight));
    
    // Adjusted mouse Y for the translated context
    float adjMouseY = m_mouseY - titlebarHeight;

    Renderer renderer(m_d2dContext.Get(), m_dwriteFactory.Get());
    m_sidebar.Render(renderer, mainContentHeight, m_mouseX, adjMouseY);

    // Draw main content area
    float sidebarWidth = static_cast<float>(m_sidebar.GetWidth());
    
    if (m_sidebar.GetSelectedIndex() == 0) {
        std::vector<DockerContainer> currentContainers;
        {
            std::lock_guard<std::mutex> lock(m_containersMutex);
            currentContainers = m_cachedContainers;
        }
        m_containersView.Render(renderer, sidebarWidth, width, mainContentHeight, currentContainers,
                                 m_mouseX, adjMouseY, m_mouseClicked,
                                 [this](const std::string& cid, const std::string& act) {
                                     this->ExecuteContainerAction(cid, act);
                                  },
                                 m_searchQuery, m_searchFocused, m_scrollOffset,
                                 m_isLoading, m_loadingText);
    } else if (m_sidebar.GetSelectedIndex() == 1) {
        std::vector<DockerImage> currentImages;
        {
            std::lock_guard<std::mutex> lock(m_imagesMutex);
            currentImages = m_cachedImages;
        }
        m_imagesView.Render(renderer, sidebarWidth, width, mainContentHeight, currentImages,
                             m_mouseX, adjMouseY, m_mouseClicked,
                             [this](const std::string& target, const std::string& act) {
                                 this->ExecuteImageAction(target, act);
                             },
                             m_searchQuery, m_searchFocused, m_scrollOffset,
                             m_isLoading, m_loadingText,
                             m_pullQuery, m_pullFocused,
                             m_isRunDialogOpen, m_dialogImageName,
                             m_dialogContainerName, m_dialogContainerNameFocused,
                             m_dialogPortRouting, m_dialogPortRoutingFocused,
                             m_dialogVolumeMapping, m_dialogVolumeMappingFocused,
                             m_dialogEnvVars, m_dialogEnvVarsFocused,
                             m_dialogAutostart);
    } else if (m_sidebar.GetSelectedIndex() == 2) {
        std::vector<DockerVolume> currentVolumes;
        {
            std::lock_guard<std::mutex> lock(m_volumesMutex);
            currentVolumes = m_cachedVolumes;
        }
        m_volumesView.Render(renderer, sidebarWidth, width, mainContentHeight, currentVolumes,
                             m_mouseX, adjMouseY, m_mouseClicked,
                             [this](const std::string& target, const std::string& act) {
                                 this->ExecuteVolumeAction(target, act);
                             },
                             m_searchQuery, m_searchFocused, m_scrollOffset,
                             m_isLoading, m_loadingText,
                             m_createQuery, m_createFocused);
    } else if (m_sidebar.GetSelectedIndex() == 3) {
        std::vector<DockerNetwork> currentNetworks;
        {
            std::lock_guard<std::mutex> lock(m_networksMutex);
            currentNetworks = m_cachedNetworks;
        }
        m_networksView.Render(renderer, sidebarWidth, width, mainContentHeight, currentNetworks,
                              m_mouseX, adjMouseY, m_mouseClicked,
                              [this](const std::string& target, const std::string& act) {
                                  this->ExecuteNetworkAction(target, act);
                              },
                              m_searchQuery, m_searchFocused, m_scrollOffset,
                              m_isLoading, m_loadingText,
                              m_networkCreateQuery, m_networkCreateFocused);
    } else {
        // Other tabs placeholder
        renderer.DrawTextW(L"Coming Soon...", sidebarWidth + 40.0f, 40.0f, width - sidebarWidth - 80.0f, 40.0f, 24.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f));
    }

    // --- Render Bottom Status Bar (drawn relative to translated origin) ---
    float barY = mainContentHeight;
    // Fill background
    renderer.FillRectangle(0.0f, barY + 1.0f, width, statusBarHeight - 1.0f, D2D1::ColorF(0.08f, 0.08f, 0.1f, 0.95f));
    // Draw top separator
    renderer.FillRectangle(0.0f, barY, width, 1.0f, D2D1::ColorF(0.2f, 0.2f, 0.23f, 0.5f));

    // VM Status Item
    D2D1::ColorF vmDotColor = m_isCheckingStatus ? D2D1::ColorF(1.0f, 0.7f, 0.0f) : (m_isVmRunning ? D2D1::ColorF(0.4f, 0.85f, 0.6f) : D2D1::ColorF(0.8f, 0.3f, 0.3f));
    renderer.DrawTextW(L"●", 16.0f, barY + 5.0f, 15.0f, 18.0f, 12.0f, vmDotColor);
    
    std::wstring vmText;
    if (m_isCheckingStatus) {
        vmText = L"VM: Connecting...";
    } else {
        vmText = L"VM: " + std::wstring(m_isVmRunning ? L"Running" : L"Stopped [Start]");
    }
    
    // Draw subtle hover for VM status (if stopped and clickable)
    if (!m_isVmRunning && !m_isCheckingStatus) {
        bool isVmHovered = (m_mouseX >= 12.0f && m_mouseX <= 145.0f && adjMouseY >= barY && adjMouseY <= barY + statusBarHeight);
        if (isVmHovered) {
            renderer.FillRectangle(12.0f, barY + 2.0f, 133.0f, 24.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.08f));
        }
    }
    renderer.DrawTextW(vmText, 32.0f, barY + 5.0f, 130.0f, 18.0f, 12.0f, D2D1::ColorF(0.8f, 0.8f, 0.8f));

    // Docker Status Item
    D2D1::ColorF dockerDotColor = m_isCheckingStatus ? D2D1::ColorF(1.0f, 0.7f, 0.0f) : (m_isDockerConnected ? D2D1::ColorF(0.2f, 0.6f, 1.0f) : D2D1::ColorF(0.8f, 0.3f, 0.3f));
    renderer.DrawTextW(L"●", 160.0f, barY + 5.0f, 15.0f, 18.0f, 12.0f, dockerDotColor);
    
    std::wstring dockerText;
    if (m_isCheckingStatus) {
        dockerText = L"Docker: Connecting...";
    } else {
        dockerText = L"Docker: " + std::wstring(m_isDockerConnected ? L"Connected" : L"Disconnected");
    }
    renderer.DrawTextW(dockerText, 176.0f, barY + 5.0f, 180.0f, 18.0f, 12.0f, D2D1::ColorF(0.8f, 0.8f, 0.8f));

    // Floating log text above the status bar (aligned to the right side, no background/border, real-time)
    std::wstring latestLog = GetLatestLogs();
    if (!latestLog.empty()) {
        float logW = 400.0f;
        float logH = 36.0f;
        float logX = width - logW - 40.0f;
        float logY = mainContentHeight - logH - 12.0f; // Float 12px above status bar
        renderer.DrawTextW(latestLog, logX, logY + 4.0f, logW, logH - 8.0f, 9.0f, D2D1::ColorF(0.70f, 0.75f, 0.80f, 0.70f));
    }

    // Logs Button (Far Right)
    bool isLogHovered = (m_mouseX >= width - 104.0f && m_mouseX <= width - 12.0f && adjMouseY >= barY && adjMouseY <= barY + statusBarHeight);
    if (isLogHovered) {
        renderer.FillRectangle(width - 104.0f, barY + 2.0f, 92.0f, 24.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.08f));
    }
    renderer.DrawIcon((wchar_t)0xE756, width - 96.0f, barY + 6.0f, 14.0f, D2D1::ColorF(0.8f, 0.8f, 0.8f));
    renderer.DrawTextW(L"VM Logs", width - 76.0f, barY + 5.0f, 60.0f, 18.0f, 12.0f, D2D1::ColorF(0.8f, 0.8f, 0.8f));

    m_mouseClicked = false; // Reset click state after rendering

    // Reset transform to draw absolute custom titlebar buttons at top-right
    m_d2dContext->SetTransform(D2D1::Matrix3x2F::Identity());

    // Draw Custom Titlebar Buttons
    float btnWidth = 46.0f;
    float btnHeight = 40.0f;
    
    // Close button
    float closeLeft = width - btnWidth;
    bool isCloseHovered = (m_mouseX >= closeLeft && m_mouseX <= width && m_mouseY >= 0 && m_mouseY <= btnHeight);
    if (isCloseHovered) {
        renderer.FillRectangle(closeLeft, 0.0f, btnWidth, btnHeight, D2D1::ColorF(0.85f, 0.15f, 0.15f, 0.9f));
    }
    renderer.DrawIcon((wchar_t)0xE711, closeLeft + 17.0f, 14.0f, 10.0f, D2D1::ColorF(D2D1::ColorF::White));

    // Maximize button
    float maxLeft = closeLeft - btnWidth;
    bool isMaxHovered = (m_mouseX >= maxLeft && m_mouseX < closeLeft && m_mouseY >= 0 && m_mouseY <= btnHeight);
    if (isMaxHovered) {
        renderer.FillRectangle(maxLeft, 0.0f, btnWidth, btnHeight, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.08f));
    }
    wchar_t maxIcon = IsZoomed(m_hwnd) ? (wchar_t)0xE923 : (wchar_t)0xE922; // Restore vs Maximize
    renderer.DrawIcon(maxIcon, maxLeft + 17.0f, 14.0f, 10.0f, D2D1::ColorF(D2D1::ColorF::White));

    // Minimize button
    float minLeft = maxLeft - btnWidth;
    bool isMinHovered = (m_mouseX >= minLeft && m_mouseX < maxLeft && m_mouseY >= 0 && m_mouseY <= btnHeight);
    if (isMinHovered) {
        renderer.FillRectangle(minLeft, 0.0f, btnWidth, btnHeight, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.08f));
    }
    renderer.DrawIcon((wchar_t)0xE921, minLeft + 17.0f, 14.0f, 10.0f, D2D1::ColorF(D2D1::ColorF::White));

    // Draw Starting VM Large Popup Modal if active
    if (m_isStartingVmDialogActive) {
        // Dark translucent overlay
        renderer.FillRectangle(0.0f, 0.0f, width, height, D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.65f));

        float modalW = 400.0f;
        float modalH = 240.0f;
        float modalX = (width - modalW) / 2.0f;
        float modalY = (height - modalH) / 2.0f;

        // Draw shadow under the modal card
        for (int i = 12; i >= 1; --i) {
            float offset = static_cast<float>(i);
            float alpha = (1.0f - (offset / 12.0f)) * 0.02f;
            renderer.FillRoundedRectangle(modalX - offset, modalY - offset + 4.0f, modalW + offset * 2.0f, modalH + offset * 2.0f,
                                          12.0f + offset, 12.0f + offset, D2D1::ColorF(0.0f, 0.0f, 0.0f, alpha));
        }

        // Draw card background & border
        renderer.FillRoundedRectangle(modalX, modalY, modalW, modalH, 12.0f, 12.0f, D2D1::ColorF(0.12f, 0.17f, 0.28f, 0.95f));
        renderer.DrawRoundedRectangle(modalX, modalY, modalW, modalH, 12.0f, 12.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.15f), 1.0f);

        // Title
        renderer.DrawTextW(L"Starting Docker Machine", modalX, modalY + 35.0f, modalW, 30.0f, 20.0f, D2D1::ColorF(D2D1::ColorF::White), DWRITE_FONT_WEIGHT_SEMI_BOLD, true);

        // Subtitle/Description
        renderer.DrawTextW(L"Please wait while the virtual machine\nand Docker engine initialize...", modalX + 20.0f, modalY + 75.0f, modalW - 40.0f, 40.0f, 12.0f, D2D1::ColorF(0.7f, 0.75f, 0.8f), DWRITE_FONT_WEIGHT_NORMAL, true);

        // Spinning orbit indicator
        float spinnerCenterX = width / 2.0f;
        float spinnerCenterY = modalY + 165.0f;
        float angle = (GetTickCount() % 1200) / 1200.0f * 2.0f * 3.14159f;

        // Track ring
        renderer.DrawRoundedRectangle(spinnerCenterX - 18.0f, spinnerCenterY - 18.0f, 36.0f, 36.0f, 18.0f, 18.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.1f), 2.0f);

        // Orbiting accent dot
        float dotX = spinnerCenterX + cos(angle) * 18.0f;
        float dotY = spinnerCenterY + sin(angle) * 18.0f;
        renderer.FillRoundedRectangle(dotX - 4.0f, dotY - 4.0f, 8.0f, 8.0f, 4.0f, 4.0f, D2D1::ColorF(0.2f, 0.6f, 1.0f));
    }

    HRESULT hr = m_d2dContext->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        m_d2dContext.Reset();
        m_swapChain.Reset();
        InitializeDirect2D();
    } else {
        m_swapChain->Present(1, 0);
    }
}

void Window::Resize(UINT width, UINT height) {
    if (m_swapChain && width > 0 && height > 0) {
        m_d2dContext->SetTarget(nullptr);
        m_d2dTargetBitmap.Reset();
        m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
        
        ComPtr<IDXGISurface> dxgiBackBuffer;
        m_swapChain->GetBuffer(0, IID_PPV_ARGS(&dxgiBackBuffer));
        D2D1_BITMAP_PROPERTIES1 bitmapProperties = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
        );
        m_d2dContext->CreateBitmapFromDxgiSurface(dxgiBackBuffer.Get(), &bitmapProperties, &m_d2dTargetBitmap);
        m_d2dContext->SetTarget(m_d2dTargetBitmap.Get());
    }
}

LRESULT CALLBACK Window::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    Window* pThis = nullptr;

    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (Window*)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
        pThis->m_hwnd = hwnd;
    } else {
        pThis = (Window*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }

    if (pThis) {
        return pThis->HandleMessage(uMsg, wParam, lParam);
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT Window::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_NCCALCSIZE:
        if (wParam == TRUE) {
            return 0; // Remove standard frame/caption
        }
        break;

    case WM_NCACTIVATE:
        return TRUE; // Prevent standard caption button rendering on activation

    case WM_NCPAINT:
        return 0; // Prevent standard frame painting (ghost buttons)

    case WM_GETMINMAXINFO: {
        MINMAXINFO* pMinMax = reinterpret_cast<MINMAXINFO*>(lParam);
        pMinMax->ptMinTrackSize.x = 1220;
        pMinMax->ptMinTrackSize.y = 770;
        return 0;
    }

    case WM_NCHITTEST: {
        POINT pt = { static_cast<LONG>(static_cast<short>(LOWORD(lParam))), static_cast<LONG>(static_cast<short>(HIWORD(lParam))) };
        ScreenToClient(m_hwnd, &pt);

        RECT rc;
        GetClientRect(m_hwnd, &rc);

        int borderWidth = 8; // Resize borders

        if (pt.y < borderWidth) {
            if (pt.x < borderWidth) return HTTOPLEFT;
            if (pt.x > rc.right - borderWidth) return HTTOPRIGHT;
            return HTTOP;
        }
        if (pt.y > rc.bottom - borderWidth) {
            if (pt.x < borderWidth) return HTBOTTOMLEFT;
            if (pt.x > rc.right - borderWidth) return HTBOTTOMRIGHT;
            return HTBOTTOM;
        }
        if (pt.x < borderWidth) return HTLEFT;
        if (pt.x > rc.right - borderWidth) return HTRIGHT;

        // Custom Titlebar height is 40.0f
        if (pt.y < 40) {
            if (pt.x < rc.right - 138) {
                return HTCAPTION; // Draggable titlebar
            }
        }
        return HTCLIENT;
    }

    case WM_TRAYICON: {
        if (lParam == WM_LBUTTONDBLCLK) {
            ShowWindow(m_hwnd, SW_SHOW);
            SetForegroundWindow(m_hwnd);
            m_isCheckingStatus = true;
            PostMessage(m_hwnd, WM_TIMER, 1, 0); // Force immediate refresh
        } else if (lParam == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            if (hMenu) {
                AppendMenuW(hMenu, MF_STRING, 1001, L"Show Apps");
                AppendMenuW(hMenu, MF_STRING, 1002, L"Restart VM");
                AppendMenuW(hMenu, MF_STRING, 1003, L"Stop VM");
                AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
                AppendMenuW(hMenu, MF_STRING, 1004, L"Exit");

                SetForegroundWindow(m_hwnd);
                int trackId = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hwnd, nullptr);
                DestroyMenu(hMenu);

                if (trackId == 1001) {
                    ShowWindow(m_hwnd, SW_SHOW);
                    SetForegroundWindow(m_hwnd);
                    m_isCheckingStatus = true;
                    PostMessage(m_hwnd, WM_TIMER, 1, 0); // Force immediate refresh
                } else if (trackId == 1002) {
                    StopDockerVM();
                    m_isCheckingStatus = true;
                    std::thread([this]() {
                        Sleep(5000); // Wait for poweroff command to execute
                        StartDockerVM();
                    }).detach();
                } else if (trackId == 1003) {
                    StopDockerVM();
                    m_isCheckingStatus = true;
                } else if (trackId == 1004) {
                    if (m_isVmRunning) {
                        StopDockerVM(true); // Block until VM is powered off
                    }
                    NOTIFYICONDATAW nid = { sizeof(nid) };
                    nid.hWnd = m_hwnd;
                    nid.uID = 1;
                    Shell_NotifyIconW(NIM_DELETE, &nid);
                    DestroyWindow(m_hwnd);
                }
            }
        }
        return 0;
    }

    case WM_CLOSE: {
        if (m_isVmRunning) {
            ShowWindow(m_hwnd, SW_HIDE);
        } else {
            DestroyWindow(m_hwnd);
        }
        return 0;
    }

    case WM_DESTROY: {
        NOTIFYICONDATAW nid = { sizeof(nid) };
        nid.hWnd = m_hwnd;
        nid.uID = 1;
        Shell_NotifyIconW(NIM_DELETE, &nid);
        PostQuitMessage(0);
        return 0;
    }

    case WM_PAINT:
        Render();
        ValidateRect(m_hwnd, nullptr);
        return 0;

    case WM_TIMER:
        if (wParam == 1) {
            if (!m_isFetching) {
                m_isFetching = true;
                std::thread([this]() {
                    auto newContainers = m_dockerClient.GetContainers();
                    auto newImages = m_dockerClient.GetImages();
                    auto newVolumes = m_dockerClient.GetVolumes();
                    auto newNetworks = m_dockerClient.GetNetworks();
                    bool isVmRunning = IsQemuProcessRunning();
                    bool isDockerConnected = m_dockerClient.Ping();
                    
                    {
                        std::lock_guard<std::mutex> lock(m_containersMutex);
                        m_cachedContainers = newContainers;
                    }
                    {
                        std::lock_guard<std::mutex> lock(m_imagesMutex);
                        m_cachedImages = newImages;
                    }
                    {
                        std::lock_guard<std::mutex> lock(m_volumesMutex);
                        m_cachedVolumes = newVolumes;
                    }
                    {
                        std::lock_guard<std::mutex> lock(m_networksMutex);
                        m_cachedNetworks = newNetworks;
                    }
                    
                    m_isVmRunning = isVmRunning;
                    m_isDockerConnected = isDockerConnected;
                    m_isCheckingStatus = false;

                    if (isDockerConnected && m_isStartingVmDialogActive) {
                        m_isStartingVmDialogActive = false;
                        KillTimer(m_hwnd, 2);
                    }

                    // Read the last 2 lines of vm_log.txt safely
                    std::wstring lastLog = L"";
                    {
                        wchar_t exePath[MAX_PATH];
                        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
                        std::wstring logPath = exePath;
                        size_t lastSlash = logPath.find_last_of(L"\\/");
                        if (lastSlash != std::wstring::npos) {
                            logPath = logPath.substr(0, lastSlash) + L"\\vm_log.txt";
                        }

                        HANDLE hFile = CreateFileW(logPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
                        if (hFile != INVALID_HANDLE_VALUE) {
                            DWORD fileSize = GetFileSize(hFile, nullptr);
                            if (fileSize > 0) {
                                DWORD readSize = fileSize > 2048 ? 2048 : fileSize;
                                std::vector<char> buffer(readSize + 1, 0);
                                LARGE_INTEGER li;
                                li.QuadPart = fileSize - readSize;
                                SetFilePointerEx(hFile, li, nullptr, FILE_BEGIN);
                                DWORD bytesRead = 0;
                                if (ReadFile(hFile, buffer.data(), readSize, &bytesRead, nullptr) && bytesRead > 0) {
                                    std::string content(buffer.data(), bytesRead);
                                    std::vector<std::string> logLines;
                                    std::string currentLine;
                                    for (char c : content) {
                                        if (c == '\n') {
                                            if (!currentLine.empty()) {
                                                logLines.push_back(currentLine);
                                                currentLine.clear();
                                            }
                                        } else if (c != '\r') {
                                            currentLine += c;
                                        }
                                    }
                                    if (!currentLine.empty()) {
                                        logLines.push_back(currentLine);
                                    }
                                    
                                    if (logLines.size() >= 2) {
                                        lastLog = WindowUtf8ToWide(logLines[logLines.size() - 2]) + L"\n" + WindowUtf8ToWide(logLines.back());
                                    } else if (logLines.size() == 1) {
                                        lastLog = WindowUtf8ToWide(logLines[0]);
                                    } else {
                                        lastLog = L"Waiting for VM logs...";
                                    }
                                }
                            } else {
                                lastLog = L"Log file empty";
                            }
                            CloseHandle(hFile);
                        } else {
                            lastLog = L"No VM logs found";
                        }
                    }
                    m_lastLogLines = lastLog;
                    
                    m_isFetching = false;
                    PostMessage(m_hwnd, WM_APP + 1, 0, 0);
                }).detach();
            }
        } else if (wParam == 2) {
            InvalidateRect(m_hwnd, nullptr, FALSE); // Drive spinner animation frame
        }
        return 0;

    case WM_APP + 1:
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return 0;

    case WM_MOUSEMOVE:
        m_mouseX = static_cast<float>(static_cast<short>(LOWORD(lParam)));
        m_mouseY = static_cast<float>(static_cast<short>(HIWORD(lParam)));
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return 0;

    case WM_LBUTTONDOWN:
        m_mouseX = static_cast<float>(static_cast<short>(LOWORD(lParam)));
        m_mouseY = static_cast<float>(static_cast<short>(HIWORD(lParam)));
        m_mouseClicked = true;
        {
            RECT rc;
            GetClientRect(m_hwnd, &rc);
            float width = static_cast<float>(rc.right - rc.left);
            float height = static_cast<float>(rc.bottom - rc.top);
            float barY = height - 28.0f;

            // Check custom title bar button clicks (top 40px)
            if (m_mouseY < 40.0f) {
                if (m_mouseX >= width - 46.0f && m_mouseX <= width) {
                    PostMessage(m_hwnd, WM_CLOSE, 0, 0);
                    return 0;
                }
                else if (m_mouseX >= width - 92.0f && m_mouseX < width - 46.0f) {
                    ShowWindow(m_hwnd, IsZoomed(m_hwnd) ? SW_RESTORE : SW_MAXIMIZE);
                    return 0;
                }
                else if (m_mouseX >= width - 138.0f && m_mouseX < width - 92.0f) {
                    ShowWindow(m_hwnd, SW_MINIMIZE);
                    return 0;
                }
            }

            // Check Refresh button click (next to title "Dockless")
            float adjMouseY = m_mouseY - 40.0f;
            if (m_mouseX >= 141.0f && m_mouseX <= 169.0f && adjMouseY >= 32.0f && adjMouseY <= 60.0f) {
                m_isCheckingStatus = true;
                PostMessage(m_hwnd, WM_TIMER, 1, 0); // Force immediate refresh
                return 0;
            }

            if (m_mouseY >= barY && m_mouseY <= height) {
                // Clicked in the Status Bar!
                if (!m_isVmRunning && !m_isCheckingStatus && m_mouseX >= 16.0f && m_mouseX <= 130.0f) {
                    // Clicked Start VM!
                    StartDockerVM();
                    m_isCheckingStatus = true;
                    // Force an immediate refresh
                    PostMessage(m_hwnd, WM_TIMER, 1, 0);
                }
                else if (m_mouseX >= width - 100.0f && m_mouseX <= width - 16.0f) {
                    // Clicked Logs!
                    std::thread([]() {
                        // Create empty vm_log.txt if not exists
                        FILE* f = fopen("vm_log.txt", "a");
                        if (f) fclose(f);

                        std::string cmd = "cmd.exe /c start cmd.exe /k \"title VM Log && powershell -NoProfile -ExecutionPolicy Bypass -Command \\\"Get-Content -Path vm_log.txt -Wait -Tail 100 -ErrorAction SilentlyContinue\\\"\"";
                        system(cmd.c_str());
                    }).detach();
                }
            }
        }

        if (m_sidebar.HandleClick(m_mouseX, m_mouseY - 40.0f)) {
            // Clear page state
            m_searchQuery = L"";
            m_searchFocused = false;
            m_scrollOffset = 0.0f;
            m_pullQuery = L"";
            m_pullFocused = false;
            m_createQuery = L"";
            m_createFocused = false;
            m_networkCreateQuery = L"";
            m_networkCreateFocused = false;
            
            // Trigger fetch immediately
            if (!m_isFetching) {
                m_isFetching = true;
                std::thread([this]() {
                    auto newContainers = m_dockerClient.GetContainers();
                    auto newImages = m_dockerClient.GetImages();
                    auto newVolumes = m_dockerClient.GetVolumes();
                    auto newNetworks = m_dockerClient.GetNetworks();
                    bool isVmRunning = IsQemuProcessRunning();
                    bool isDockerConnected = m_dockerClient.Ping();
                    {
                        std::lock_guard<std::mutex> lock(m_containersMutex);
                        m_cachedContainers = newContainers;
                    }
                    {
                        std::lock_guard<std::mutex> lock(m_imagesMutex);
                        m_cachedImages = newImages;
                    }
                    {
                        std::lock_guard<std::mutex> lock(m_volumesMutex);
                        m_cachedVolumes = newVolumes;
                    }
                    {
                        std::lock_guard<std::mutex> lock(m_networksMutex);
                        m_cachedNetworks = newNetworks;
                    }
                    m_isVmRunning = isVmRunning;
                    m_isDockerConnected = isDockerConnected;
                    m_isCheckingStatus = false;
                    m_isFetching = false;
                    PostMessage(m_hwnd, WM_APP + 1, 0, 0);
                }).detach();
            }
        }
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return 0;

    case WM_CHAR:
    {
        wchar_t ch = static_cast<wchar_t>(wParam);

        // Helper lambda to paste clipboard text into a wstring
        auto pasteClipboard = [this](std::wstring& target) {
            if (OpenClipboard(m_hwnd)) {
                HANDLE hData = GetClipboardData(CF_UNICODETEXT);
                if (hData) {
                    wchar_t* pszText = static_cast<wchar_t*>(GlobalLock(hData));
                    if (pszText) {
                        target += pszText;
                        GlobalUnlock(hData);
                    }
                }
                CloseClipboard();
            }
        };

        // Helper lambda to handle char input for a text field
        auto handleCharInput = [&](std::wstring& target, bool& focused) -> bool {
            if (!focused) return false;
            if (ch == 22) { // Ctrl+V (paste)
                pasteClipboard(target);
            } else if (ch == 1) { // Ctrl+A (select all = clear)
                target.clear();
            } else if (ch == L'\b') {
                if (!target.empty()) target.pop_back();
            } else if (ch == L'\r' || ch == L'\n') {
                focused = false;
            } else if (ch >= 32) {
                target.push_back(ch);
            }
            InvalidateRect(m_hwnd, nullptr, FALSE);
            return true;
        };

        if (handleCharInput(m_searchQuery, m_searchFocused)) return 0;
        if (handleCharInput(m_pullQuery, m_pullFocused)) return 0;
        if (handleCharInput(m_createQuery, m_createFocused)) return 0;
        if (handleCharInput(m_networkCreateQuery, m_networkCreateFocused)) return 0;

        if (m_isRunDialogOpen) {
            if (handleCharInput(m_dialogContainerName, m_dialogContainerNameFocused)) return 0;
            if (handleCharInput(m_dialogPortRouting, m_dialogPortRoutingFocused)) return 0;
            if (handleCharInput(m_dialogVolumeMapping, m_dialogVolumeMappingFocused)) return 0;
            if (handleCharInput(m_dialogEnvVars, m_dialogEnvVarsFocused)) return 0;
        }
        break;
    }

    case WM_MOUSEWHEEL:
        {
            int delta = GET_WHEEL_DELTA_WPARAM(wParam);
            m_scrollOffset -= (delta / 120.0f) * 40.0f;
            if (m_scrollOffset < 0.0f) m_scrollOffset = 0.0f;
            InvalidateRect(m_hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_SIZE:
        Resize(LOWORD(lParam), HIWORD(lParam));
        return 0;
    }
    return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
}

void Window::ExecuteContainerAction(const std::string& containerId, const std::string& action) {
    if (action == "more") {
        POINT pt;
        GetCursorPos(&pt);
        
        HMENU hMenu = CreatePopupMenu();
        AppendMenuW(hMenu, MF_STRING, 1001, L"Terminal (docker exec -it)");
        AppendMenuW(hMenu, MF_STRING, 1002, L"Toggle Autostart");
        
        int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, m_hwnd, nullptr);
        DestroyMenu(hMenu);
        
        if (cmd == 1001) {
            std::thread([containerId]() {
                std::string cmdStr = "cmd.exe /c start cmd.exe /k \"docker exec -it " + containerId + " sh || docker exec -it " + containerId + " bash || docker exec -it " + containerId + " cmd\"";
                system(cmdStr.c_str());
            }).detach();
        } else if (cmd == 1002) {
            ExecuteContainerAction(containerId, "autostart");
        }
        return;
    }

    // Map action to status message
    if (action == "stop") {
        m_loadingText = L"Stopping container...";
    } else if (action == "start") {
        m_loadingText = L"Starting container...";
    } else if (action == "restart") {
        m_loadingText = L"Restarting container...";
    } else if (action == "delete") {
        m_loadingText = L"Deleting container...";
    } else if (action == "autostart") {
        m_loadingText = L"Updating autostart configuration...";
    } else {
        m_loadingText = L"Processing request...";
    }

    m_isLoading = true;
    SetTimer(m_hwnd, 2, 16, nullptr); // Start 60FPS animation timer (16ms)
    InvalidateRect(m_hwnd, nullptr, FALSE);

    std::thread([this, containerId, action]() {
        bool success = false;
        if (action == "stop") {
            success = m_dockerClient.StopContainer(containerId);
        } else if (action == "start") {
            success = m_dockerClient.StartContainer(containerId);
        } else if (action == "restart") {
            success = m_dockerClient.RestartContainer(containerId);
        } else if (action == "delete") {
            success = m_dockerClient.DeleteContainer(containerId);
        } else if (action == "autostart") {
            success = m_dockerClient.ToggleAutostart(containerId, true);
        }

        // Fetch new state immediately
        auto newContainers = m_dockerClient.GetContainers();
        auto newImages = m_dockerClient.GetImages();
        auto newVolumes = m_dockerClient.GetVolumes();
        {
            std::lock_guard<std::mutex> lock(m_containersMutex);
            m_cachedContainers = newContainers;
        }
        {
            std::lock_guard<std::mutex> lock(m_imagesMutex);
            m_cachedImages = newImages;
        }
        {
            std::lock_guard<std::mutex> lock(m_volumesMutex);
            m_cachedVolumes = newVolumes;
        }

        // Reset loading status
        m_isLoading = false;
        KillTimer(m_hwnd, 2); // Stop animation timer
        PostMessage(m_hwnd, WM_APP + 1, 0, 0);
    }).detach();
}

static std::string WindowWideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string str(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size, nullptr, nullptr);
    return str;
}

static std::wstring WindowUtf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring wstr(size - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size);
    return wstr;
}

void Window::ExecuteImageAction(const std::string& target, const std::string& action) {
    if (action == "run") {
        m_isRunDialogOpen = true;
        m_dialogImageName = target;
        m_dialogContainerName = L"";
        m_dialogContainerNameFocused = false;
        m_dialogPortRouting = L"";
        m_dialogPortRoutingFocused = false;
        m_dialogVolumeMapping = L"";
        m_dialogVolumeMappingFocused = false;
        m_dialogEnvVars = L"";
        m_dialogEnvVarsFocused = false;
        m_dialogAutostart = false;
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    } else if (action == "cancel_run") {
        m_isRunDialogOpen = false;
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }

    if (action == "pull") {
        m_loadingText = L"Pulling image: " + std::wstring(target.begin(), target.end()) + L"...";
    } else if (action == "delete") {
        m_loadingText = L"Deleting image...";
    } else if (action == "submit_run") {
        m_isRunDialogOpen = false;
        m_loadingText = L"Running container from image...";
    } else {
        m_loadingText = L"Processing request...";
    }

    m_isLoading = true;
    SetTimer(m_hwnd, 2, 16, nullptr); // Start 60FPS animation timer (16ms)
    InvalidateRect(m_hwnd, nullptr, FALSE);

    // Capture dialog values on the main thread before spawning background thread
    std::string capturedContainerName = WindowWideToUtf8(m_dialogContainerName);
    std::string capturedPortRouting = WindowWideToUtf8(m_dialogPortRouting);
    std::string capturedVolumeMapping = WindowWideToUtf8(m_dialogVolumeMapping);
    std::string capturedEnvVars = WindowWideToUtf8(m_dialogEnvVars);
    bool capturedAutostart = m_dialogAutostart;

    std::thread([this, target, action, capturedContainerName, capturedPortRouting, capturedVolumeMapping, capturedEnvVars, capturedAutostart]() {
        bool success = false;
        if (action == "pull") {
            success = m_dockerClient.PullImage(target);
        } else if (action == "delete") {
            success = m_dockerClient.DeleteImage(target, true); // force delete
        } else if (action == "submit_run") {
            success = m_dockerClient.RunImage(target, 
                                             capturedContainerName,
                                             capturedPortRouting,
                                             capturedVolumeMapping,
                                             capturedEnvVars,
                                             capturedAutostart);
        }

        // Fetch new states immediately
        auto newContainers = m_dockerClient.GetContainers();
        auto newImages = m_dockerClient.GetImages();
        auto newVolumes = m_dockerClient.GetVolumes();
        {
            std::lock_guard<std::mutex> lock(m_containersMutex);
            m_cachedContainers = newContainers;
        }
        {
            std::lock_guard<std::mutex> lock(m_imagesMutex);
            m_cachedImages = newImages;
        }
        {
            std::lock_guard<std::mutex> lock(m_volumesMutex);
            m_cachedVolumes = newVolumes;
        }

        // Reset loading status
        m_isLoading = false;
        KillTimer(m_hwnd, 2); // Stop animation timer
        if (action == "pull" && success) {
            m_pullQuery = L"";
        }
        PostMessage(m_hwnd, WM_APP + 1, 0, 0);
    }).detach();
}

void Window::ExecuteVolumeAction(const std::string& target, const std::string& action) {
    if (action == "create") {
        m_loadingText = L"Creating volume: " + std::wstring(target.begin(), target.end()) + L"...";
    } else if (action == "delete") {
        m_loadingText = L"Deleting volume: " + std::wstring(target.begin(), target.end()) + L"...";
    } else {
        m_loadingText = L"Processing request...";
    }

    m_isLoading = true;
    SetTimer(m_hwnd, 2, 16, nullptr); // Start 60FPS animation timer (16ms)
    InvalidateRect(m_hwnd, nullptr, FALSE);

    std::thread([this, target, action]() {
        bool success = false;
        if (action == "create") {
            success = m_dockerClient.CreateVolume(target);
        } else if (action == "delete") {
            success = m_dockerClient.DeleteVolume(target);
        }

        // Fetch new states immediately
        auto newContainers = m_dockerClient.GetContainers();
        auto newImages = m_dockerClient.GetImages();
        auto newVolumes = m_dockerClient.GetVolumes();
        auto newNetworks = m_dockerClient.GetNetworks();
        {
            std::lock_guard<std::mutex> lock(m_containersMutex);
            m_cachedContainers = newContainers;
        }
        {
            std::lock_guard<std::mutex> lock(m_imagesMutex);
            m_cachedImages = newImages;
        }
        {
            std::lock_guard<std::mutex> lock(m_volumesMutex);
            m_cachedVolumes = newVolumes;
        }
        {
            std::lock_guard<std::mutex> lock(m_networksMutex);
            m_cachedNetworks = newNetworks;
        }

        // Reset loading status
        m_isLoading = false;
        KillTimer(m_hwnd, 2); // Stop animation timer
        if (action == "create" && success) {
            m_createQuery = L"";
        }
        PostMessage(m_hwnd, WM_APP + 1, 0, 0);
    }).detach();
}

void Window::ExecuteNetworkAction(const std::string& target, const std::string& action) {
    if (action == "create") {
        m_loadingText = L"Creating network: " + std::wstring(target.begin(), target.end()) + L"...";
    } else if (action == "delete") {
        m_loadingText = L"Deleting network...";
    } else {
        m_loadingText = L"Processing request...";
    }

    m_isLoading = true;
    SetTimer(m_hwnd, 2, 16, nullptr); // Start 60FPS animation timer (16ms)
    InvalidateRect(m_hwnd, nullptr, FALSE);

    std::thread([this, target, action]() {
        bool success = false;
        if (action == "create") {
            success = m_dockerClient.CreateNetwork(target);
        } else if (action == "delete") {
            success = m_dockerClient.DeleteNetwork(target);
        }

        // Fetch new states immediately
        auto newContainers = m_dockerClient.GetContainers();
        auto newImages = m_dockerClient.GetImages();
        auto newVolumes = m_dockerClient.GetVolumes();
        auto newNetworks = m_dockerClient.GetNetworks();
        {
            std::lock_guard<std::mutex> lock(m_containersMutex);
            m_cachedContainers = newContainers;
        }
        {
            std::lock_guard<std::mutex> lock(m_imagesMutex);
            m_cachedImages = newImages;
        }
        {
            std::lock_guard<std::mutex> lock(m_volumesMutex);
            m_cachedVolumes = newVolumes;
        }
        {
            std::lock_guard<std::mutex> lock(m_networksMutex);
            m_cachedNetworks = newNetworks;
        }

        // Reset loading status
        m_isLoading = false;
        KillTimer(m_hwnd, 2); // Stop animation timer
        if (action == "create" && success) {
            m_networkCreateQuery = L"";
        }
        PostMessage(m_hwnd, WM_APP + 1, 0, 0);
    }).detach();
}

bool Window::ProcessMessages() {
    MSG msg = { 0 };
    if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return true;
}

void Window::StartDockerVM() {
    std::thread([this]() {
        // Get absolute path of current executable
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        std::wstring exeDir = exePath;
        size_t lastSlash = exeDir.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            exeDir = exeDir.substr(0, lastSlash);
        }

        // Run start_vm.bat headless
        STARTUPINFOW si = { sizeof(si) };
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi = { 0 };

        // Construct absolute paths (script is directly under scripts/ inside the build output folder)
        std::wstring scriptPath = exeDir + L"\\scripts\\start_vm.bat";
        std::wstring logPath = exeDir + L"\\vm_log.txt";

        // Command to run start_vm.bat without shell redirection (QEMU writes natively to vm_log.txt)
        std::wstring cmd = L"cmd.exe /c \"\"" + scriptPath + L"\" " + 
                           m_config.vmRam + L" " + 
                           m_config.vmCpu + L" " + 
                           m_config.vmDiskSize + L"\"";

        if (CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, CREATE_NEW_CONSOLE | ABOVE_NORMAL_PRIORITY_CLASS, nullptr, exeDir.c_str(), &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }).detach();
}

void Window::StopDockerVM(bool wait) {
    auto stopFn = [this]() {
        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        std::wstring exeDir = exePath;
        size_t lastSlash = exeDir.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            exeDir = exeDir.substr(0, lastSlash);
        }

        STARTUPINFOW si = { sizeof(si) };
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi = { 0 };

        std::wstring scriptPath = exeDir + L"\\scripts\\poweroff_vm.bat";
        std::wstring cmd = L"cmd.exe /c \"\"" + scriptPath + L"\"\"";

        if (CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, exeDir.c_str(), &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 15000); // Wait up to 15 seconds
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    };

    if (wait) {
        stopFn();
    } else {
        std::thread(stopFn).detach();
    }
}

std::wstring Window::GetLatestLogs() {
    std::wstring lastLog = L"";
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring logPath = exePath;
    size_t lastSlash = logPath.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        logPath = logPath.substr(0, lastSlash) + L"\\vm_log.txt";
    }

    HANDLE hFile = CreateFileW(logPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD fileSize = GetFileSize(hFile, nullptr);
        if (fileSize > 0) {
            DWORD readSize = fileSize > 2048 ? 2048 : fileSize;
            std::vector<char> buffer(readSize + 1, 0);
            LARGE_INTEGER li;
            li.QuadPart = fileSize - readSize;
            SetFilePointerEx(hFile, li, nullptr, FILE_BEGIN);
            DWORD bytesRead = 0;
            if (ReadFile(hFile, buffer.data(), readSize, &bytesRead, nullptr) && bytesRead > 0) {
                std::string content(buffer.data(), bytesRead);
                std::vector<std::string> logLines;
                std::string currentLine;
                for (char c : content) {
                    if (c == '\n') {
                        if (!currentLine.empty()) {
                            logLines.push_back(currentLine);
                            currentLine.clear();
                        }
                    } else if (c != '\r') {
                         currentLine += c;
                    }
                }
                if (!currentLine.empty()) {
                    logLines.push_back(currentLine);
                }
                
                if (logLines.size() >= 2) {
                    lastLog = WindowUtf8ToWide(logLines[logLines.size() - 2]) + L"\n" + WindowUtf8ToWide(logLines.back());
                } else if (logLines.size() == 1) {
                    lastLog = WindowUtf8ToWide(logLines[0]);
                } else {
                    lastLog = L"Waiting for VM logs...";
                }
            }
        } else {
            lastLog = L"Log file empty";
        }
        CloseHandle(hFile);
    } else {
        lastLog = L"No VM logs found";
    }
    return lastLog;
}
