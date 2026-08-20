#pragma once
#include "../rendering/Renderer.h"
#include <vector>
#include <string>

class Sidebar {
public:
    Sidebar();
    void Render(Renderer& renderer, float windowHeight, float mouseX = -1.0f, float mouseY = -1.0f);
    
    int GetWidth() const { return 250; }
    int GetSelectedIndex() const { return m_selectedIndex; }
    void SetSelectedIndex(int index) { m_selectedIndex = index; }
    bool HandleClick(float mouseX, float mouseY);
    
private:
    struct SidebarItem {
        std::wstring name;
        wchar_t icon;
    };
    std::vector<SidebarItem> m_items;
    int m_selectedIndex = 0;
};

