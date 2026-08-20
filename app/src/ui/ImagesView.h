#pragma once
#include "../rendering/Renderer.h"
#include "../docker/DockerClient.h"
#include <vector>
#include <functional>
#include <string>

class ImagesView {
public:
    ImagesView();
    void Render(Renderer& renderer, float startX, float windowWidth, float windowHeight, 
                const std::vector<DockerImage>& images,
                float mouseX, float mouseY, bool mouseClicked,
                std::function<void(const std::string&, const std::string&)> onAction,
                std::wstring& searchQuery, bool& searchFocused, float& scrollOffset,
                bool isLoading, const std::wstring& loadingText,
                std::wstring& pullQuery, bool& pullFocused,
                bool isRunDialogOpen, const std::string& dialogImageName,
                std::wstring& dialogContainerName, bool& dialogContainerNameFocused,
                std::wstring& dialogPortRouting, bool& dialogPortRoutingFocused,
                std::wstring& dialogVolumeMapping, bool& dialogVolumeMappingFocused,
                std::wstring& dialogEnvVars, bool& dialogEnvVarsFocused,
                bool& dialogAutostart);
};
