#include "Sidebar.h"

Sidebar::Sidebar() {
    m_items = {
        {L"Containers", 0xE7B8}, // Package
        {L"Images", 0xE8B9},     // Photo
        {L"Volumes", 0xE18C},    // HardDrive
        {L"Networks", 0xE17B}    // Network
    };
}

void Sidebar::Render(Renderer& renderer, float windowHeight, float mouseX, float mouseY) {
    float width = static_cast<float>(GetWidth());
    float margin = 12.0f; // Gap for the floating effect
    
    // Title
    renderer.DrawTextW(L"Dockless", 20.0f + margin, 20.0f + margin, width - (margin * 2) - 24.0f, 40.0f, 24.0f, D2D1::ColorF(D2D1::ColorF::White));
    
    // Draw Refresh Icon next to Title
    float refreshX = 20.0f + margin + 115.0f;
    float refreshY = 20.0f + margin + 6.0f;
    bool isRefreshHovered = (mouseX >= refreshX - 6.0f && mouseX <= refreshX + 22.0f && mouseY >= refreshY - 6.0f && mouseY <= refreshY + 22.0f);
    if (isRefreshHovered) {
        renderer.FillRoundedRectangle(refreshX - 6.0f, refreshY - 4.0f, 28.0f, 28.0f, 14.0f, 14.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.08f));
    }
    renderer.DrawIcon((wchar_t)0xE72C, refreshX, refreshY, 16.0f, D2D1::ColorF(0.8f, 0.8f, 0.8f));
    
    // Items
    float startY = 80.0f + margin;
    float itemHeight = 40.0f;
    float itemMargin = margin + 10.0f;
    float itemWidth = width - (margin * 2) - 20.0f;
    
    for (size_t i = 0; i < m_items.size(); ++i) {
        float y = startY + (i * itemHeight);
        bool isSelected = (i == m_selectedIndex);
        
        if (isSelected) {
            // Win11 Settings style active state
            renderer.FillRoundedRectangle(itemMargin, y, itemWidth, itemHeight, 6.0f, 6.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.06f)); // Light subtle bg
            
            // Blue pill indicator
            renderer.FillRoundedRectangle(itemMargin + 2.0f, y + (itemHeight - 16.0f) / 2.0f, 3.0f, 16.0f, 1.5f, 1.5f, D2D1::ColorF(0.2f, 0.6f, 1.0f, 1.0f));
            
            // Icon
            renderer.DrawIcon(m_items[i].icon, itemMargin + 16.0f, y + 10.0f, 16.0f, D2D1::ColorF(D2D1::ColorF::White));
            
            // Text
            renderer.DrawTextW(m_items[i].name, itemMargin + 42.0f, y + 8.0f, itemWidth, itemHeight, 16.0f, D2D1::ColorF(D2D1::ColorF::White), DWRITE_FONT_WEIGHT_SEMI_BOLD);
        } else {
            // Normal text & icon
            renderer.DrawIcon(m_items[i].icon, itemMargin + 16.0f, y + 10.0f, 16.0f, D2D1::ColorF(0.7f, 0.7f, 0.7f));
            renderer.DrawTextW(m_items[i].name, itemMargin + 42.0f, y + 8.0f, itemWidth, itemHeight, 16.0f, D2D1::ColorF(0.7f, 0.7f, 0.7f));
        }
    }
}

bool Sidebar::HandleClick(float mouseX, float mouseY) {
    float width = static_cast<float>(GetWidth());
    float margin = 12.0f;
    float startY = 80.0f + margin;
    float itemHeight = 40.0f;
    float itemMargin = margin + 10.0f;
    float itemWidth = width - (margin * 2) - 20.0f;
    
    for (size_t i = 0; i < m_items.size(); ++i) {
        float y = startY + (i * itemHeight);
        if (mouseX >= itemMargin && mouseX <= itemMargin + itemWidth &&
            mouseY >= y && mouseY <= y + itemHeight) {
            m_selectedIndex = static_cast<int>(i);
            return true;
        }
    }
    return false;
}


