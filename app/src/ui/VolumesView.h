#pragma once
#include "../rendering/Renderer.h"
#include "../docker/DockerClient.h"
#include <vector>
#include <functional>
#include <string>

class VolumesView {
public:
    VolumesView();
    void Render(Renderer& renderer, float startX, float windowWidth, float windowHeight, 
                const std::vector<DockerVolume>& volumes,
                float mouseX, float mouseY, bool mouseClicked,
                std::function<void(const std::string&, const std::string&)> onAction,
                std::wstring& searchQuery, bool& searchFocused, float& scrollOffset,
                bool isLoading, const std::wstring& loadingText,
                std::wstring& createQuery, bool& createFocused);
};
