#pragma once

#include <windows.h>
#include <mutex>
#include <atomic>
#include <thread>
#include <d2d1_1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include "../ui/Sidebar.h"
#include "../ui/Sidebar.h"
#include "../ui/ContainersView.h"
#include "../ui/ImagesView.h"
#include "../ui/VolumesView.h"
#include "../ui/NetworksView.h"
#include "../rendering/Renderer.h"
#include "../docker/DockerClient.h"
#include "Config.h"

#include <d3d11.h>
#include <dxgi1_3.h>
#include <dcomp.h>
using Microsoft::WRL::ComPtr;

#include "../app/Config.h"

class Window {
public:
    Window(int width, int height, const wchar_t* title, const AppConfig& config);
    ~Window();

    bool ProcessMessages();

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void InitializeDirect2D();
    void Render();
    void Resize(UINT width, UINT height);

    HWND m_hwnd;
    int m_width;
    int m_height;

    ComPtr<ID3D11Device> m_d3dDevice;
    ComPtr<IDXGISwapChain1> m_swapChain;
    ComPtr<ID2D1Factory1> m_d2dFactory1;
    ComPtr<ID2D1Device> m_d2dDevice;
    ComPtr<ID2D1DeviceContext> m_d2dContext;
    ComPtr<ID2D1Bitmap1> m_d2dTargetBitmap;
    ComPtr<IDCompositionDevice> m_dcompDevice;
    ComPtr<IDCompositionTarget> m_dcompTarget;
    ComPtr<IDCompositionVisual> m_dcompVisual;
    ComPtr<IDWriteFactory> m_dwriteFactory;

    Sidebar m_sidebar;
    ContainersView m_containersView;
    ImagesView m_imagesView;
    VolumesView m_volumesView;
    NetworksView m_networksView;
    DockerClient m_dockerClient;
    AppConfig m_config;
    bool m_isCheckingStatus = true;
    
    std::vector<DockerContainer> m_cachedContainers;
    std::mutex m_containersMutex;
    
    std::vector<DockerImage> m_cachedImages;
    std::mutex m_imagesMutex;
    
    std::vector<DockerVolume> m_cachedVolumes;
    std::mutex m_volumesMutex;
    
    std::vector<DockerNetwork> m_cachedNetworks;
    std::mutex m_networksMutex;
    
    std::atomic<bool> m_isFetching{false};

    // Mouse interaction states
    float m_mouseX = -1.0f;
    float m_mouseY = -1.0f;
    bool m_mouseClicked = false;

    // Search and scroll states
    std::wstring m_searchQuery;
    bool m_searchFocused = false;
    float m_scrollOffset = 0.0f;

    // Pull query and focus for images
    std::wstring m_pullQuery;
    bool m_pullFocused = false;

    // Create volume query and focus
    std::wstring m_createQuery;
    bool m_createFocused = false;

    // Create network query and focus
    std::wstring m_networkCreateQuery;
    bool m_networkCreateFocused = false;

    // Run Dialog States
    bool m_isRunDialogOpen = false;
    std::string m_dialogImageName;
    std::wstring m_dialogContainerName;
    bool m_dialogContainerNameFocused = false;
    std::wstring m_dialogPortRouting;
    bool m_dialogPortRoutingFocused = false;
    std::wstring m_dialogVolumeMapping;
    bool m_dialogVolumeMappingFocused = false;
    std::wstring m_dialogEnvVars;
    bool m_dialogEnvVarsFocused = false;
    bool m_dialogAutostart = false;

    // Loading states
    bool m_isLoading = false;
    std::wstring m_loadingText;

    // VM and Docker status
    bool m_isVmRunning = false;
    bool m_isDockerConnected = false;
    std::wstring m_lastLogLines;
    bool m_isStartingVmDialogActive = false;

    void ExecuteContainerAction(const std::string& containerId, const std::string& action);
    void ExecuteImageAction(const std::string& target, const std::string& action);
    void ExecuteVolumeAction(const std::string& target, const std::string& action);
    void ExecuteNetworkAction(const std::string& target, const std::string& action);
    void StartDockerVM();
    void StopDockerVM(bool wait = false);
    std::wstring GetLatestLogs();
};

#define WM_TRAYICON (WM_APP + 2)
