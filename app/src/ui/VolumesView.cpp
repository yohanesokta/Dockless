#include "VolumesView.h"
#include <cwchar>

VolumesView::VolumesView() {}

static std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string str(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size, nullptr, nullptr);
    return str;
}

void VolumesView::Render(Renderer& renderer, float startX, float windowWidth, float windowHeight, 
                        const std::vector<DockerVolume>& volumes,
                        float mouseX, float mouseY, bool mouseClicked,
                        std::function<void(const std::string&, const std::string&)> onAction,
                        std::wstring& searchQuery, bool& searchFocused, float& scrollOffset,
                        bool isLoading, const std::wstring& loadingText,
                        std::wstring& createQuery, bool& createFocused) {
    
    float padding = 40.0f;
    float contentWidth = windowWidth - startX;
    
    // 1. Heading
    renderer.DrawTextW(L"Volumes", startX + padding, padding, contentWidth - padding * 2, 40.0f, 32.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f), DWRITE_FONT_WEIGHT_SEMI_BOLD);
    
    // Theme colors
    D2D1::ColorF cardBg(0.12f, 0.17f, 0.28f, 0.75f); // Distinct slate glass background for high contrast
    D2D1::ColorF cardStroke(1.0f, 1.0f, 1.0f, 0.10f); // Translucent bright top reflection stroke
    D2D1::ColorF textPrimary(1.0f, 1.0f, 1.0f);
    D2D1::ColorF textSecondary(0.6f, 0.6f, 0.6f); // More muted secondary text
    D2D1::ColorF separator(1.0f, 1.0f, 1.0f, 0.06f);
    D2D1::ColorF hoverBg(1.0f, 1.0f, 1.0f, 0.06f);
    D2D1::ColorF accentColor(0.0f, 0.47f, 0.83f);
    
    // Shadow helper lambda using filled rounded rects shifted down for realistic depth
    auto drawShadow = [&](float x, float y, float w, float h, float r, float size = 12.0f) {
        for (int i = static_cast<int>(size); i >= 1; --i) {
            float offset = static_cast<float>(i);
            float alpha = (1.0f - (offset / size)) * 0.012f; // Soft ambient shadow accumulation
            renderer.FillRoundedRectangle(x - offset, y - offset + 4.0f, w + offset * 2.0f, h + offset * 2.0f,
                                          r + offset, r + offset, D2D1::ColorF(0.0f, 0.0f, 0.0f, alpha));
        }
    };
    
    // 2. Create Volume Section
    float createY = padding + 60.0f;
    float createInputLeft = startX + padding;
    float createInputWidth = 300.0f;
    float createInputHeight = 32.0f;
    float createBtnLeft = createInputLeft + createInputWidth + 12.0f;
    float createBtnWidth = 100.0f;
    float createBtnHeight = 32.0f;
    
    // Handle create input box focus
    if (mouseClicked && !isLoading) {
        if (mouseX >= createInputLeft && mouseX <= createInputLeft + createInputWidth &&
            mouseY >= createY && mouseY <= createY + createInputHeight) {
            createFocused = true;
            searchFocused = false;
        }
    }
    
    // Draw Create Input Box
    renderer.FillRoundedRectangle(createInputLeft, createY, createInputWidth, createInputHeight, 4.0f, 4.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.08f));
    renderer.DrawRoundedRectangle(createInputLeft, createY, createInputWidth, createInputHeight, 4.0f, 4.0f, 
                                  createFocused ? accentColor : separator, 1.5f);
    
    if (createQuery.empty() && !createFocused) {
        renderer.DrawTextW(L"Create volume, e.g. my-data-vol", createInputLeft + 12.0f, createY + 6.0f, createInputWidth - 24.0f, 20.0f, 14.0f, textSecondary);
    } else {
        std::wstring displayStr = createQuery + (createFocused ? L"|" : L"");
        renderer.DrawTextW(displayStr, createInputLeft + 12.0f, createY + 6.0f, createInputWidth - 24.0f, 20.0f, 14.0f, textPrimary);
    }
    
    // Draw Create Button
    bool isCreateBtnHovered = !isLoading && (mouseX >= createBtnLeft && mouseX <= createBtnLeft + createBtnWidth &&
                                             mouseY >= createY && mouseY <= createY + createBtnHeight);
    
    D2D1::ColorF btnBg = isCreateBtnHovered ? D2D1::ColorF(0.0f, 0.54f, 0.93f) : accentColor;
    renderer.FillRoundedRectangle(createBtnLeft, createY, createBtnWidth, createBtnHeight, 4.0f, 4.0f, btnBg);
    renderer.DrawTextW(L"Create", createBtnLeft, createY, createBtnWidth, createBtnHeight, 14.0f, textPrimary, DWRITE_FONT_WEIGHT_SEMI_BOLD, true);
    
    if (isCreateBtnHovered && mouseClicked && onAction && !createQuery.empty()) {
        std::string volName = WideToUtf8(createQuery);
        onAction(volName, "create");
    }
    
    // 3. Main List Card Background
    float listY = createY + 55.0f;
    float listHeight = windowHeight - listY - padding;
    if (listHeight < 250.0f) listHeight = 250.0f;
    
    drawShadow(startX + padding, listY, contentWidth - padding * 2, listHeight, 8.0f);
    renderer.FillRoundedRectangle(startX + padding, listY, contentWidth - padding * 2, listHeight, 8.0f, 8.0f, cardBg);
    renderer.DrawRoundedRectangle(startX + padding, listY, contentWidth - padding * 2, listHeight, 8.0f, 8.0f, cardStroke, 1.0f);
    
    // Search Box Inside List Card
    float searchY = listY + 15.0f;
    float searchBoxLeft = startX + padding + 20.0f;
    float searchBoxWidth = 250.0f;
    float searchBoxHeight = 32.0f;
    
    if (mouseClicked && !isLoading) {
        if (mouseX >= searchBoxLeft && mouseX <= searchBoxLeft + searchBoxWidth &&
            mouseY >= searchY && mouseY <= searchY + searchBoxHeight) {
            searchFocused = true;
            createFocused = false;
        } else if (!(mouseX >= createInputLeft && mouseX <= createInputLeft + createInputWidth &&
                     mouseY >= createY && mouseY <= createY + createInputHeight)) {
            searchFocused = false;
            createFocused = false;
        }
    }
    
    renderer.FillRoundedRectangle(searchBoxLeft, searchY, searchBoxWidth, searchBoxHeight, 4.0f, 4.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.08f));
    renderer.DrawRoundedRectangle(searchBoxLeft, searchY, searchBoxWidth, searchBoxHeight, 4.0f, 4.0f, 
                                  searchFocused ? accentColor : separator, 1.5f);
    
    if (searchQuery.empty() && !searchFocused) {
        renderer.DrawTextW(L"Search volumes...", searchBoxLeft + 15.0f, searchY + 6.0f, searchBoxWidth - 30.0f, 20.0f, 14.0f, textSecondary);
    } else {
        std::wstring displayStr = searchQuery + (searchFocused ? L"|" : L"");
        renderer.DrawTextW(displayStr, searchBoxLeft + 15.0f, searchY + 6.0f, searchBoxWidth - 30.0f, 20.0f, 14.0f, textPrimary);
    }
    
    renderer.FillRectangle(startX + padding, searchY + 45.0f, contentWidth - padding * 2, 1.0f, separator);
    
    // Filter Volumes
    std::vector<DockerVolume> filteredVolumes;
    if (searchQuery.empty()) {
        filteredVolumes = volumes;
    } else {
        std::string searchStr = WideToUtf8(searchQuery);
        auto toLower = [](std::string s) {
            for (char &c : s) c = static_cast<char>(tolower(c));
            return s;
        };
        std::string lowerSearch = toLower(searchStr);
        for (const auto& vol : volumes) {
            if (toLower(vol.name).find(lowerSearch) != std::string::npos ||
                toLower(vol.driver).find(lowerSearch) != std::string::npos) {
                filteredVolumes.push_back(vol);
            }
        }
    }
    
    // Columns Layout
    float colIconX       = startX + padding + 20.0f;
    float colNameX       = startX + padding + 60.0f;
    float colDriverX     = startX + padding + 280.0f;
    float colMountpointX = startX + padding + 400.0f;
    float colActionX     = startX + padding + 670.0f;
    
    float headerY = searchY + 55.0f;
    renderer.DrawTextW(L"Name", colNameX, headerY, 200.0f, 20.0f, 14.0f, D2D1::ColorF(0.8f, 0.8f, 0.8f), DWRITE_FONT_WEIGHT_SEMI_BOLD);
    renderer.DrawTextW(L"Driver", colDriverX, headerY, 100.0f, 20.0f, 14.0f, D2D1::ColorF(0.8f, 0.8f, 0.8f), DWRITE_FONT_WEIGHT_SEMI_BOLD);
    renderer.DrawTextW(L"Mountpoint", colMountpointX, headerY, 250.0f, 20.0f, 14.0f, D2D1::ColorF(0.8f, 0.8f, 0.8f), DWRITE_FONT_WEIGHT_SEMI_BOLD);
    renderer.DrawTextW(L"Action", colActionX, headerY, 100.0f, 20.0f, 14.0f, D2D1::ColorF(0.8f, 0.8f, 0.8f), DWRITE_FONT_WEIGHT_SEMI_BOLD);
    
    // Scroll limits
    float clipTop = headerY + 25.0f;
    float clipBottom = listY + listHeight - 10.0f;
    float visibleHeight = clipBottom - clipTop;
    float totalRowsHeight = filteredVolumes.size() * 50.0f;
    float maxScroll = (totalRowsHeight > visibleHeight) ? (totalRowsHeight - visibleHeight) : 0.0f;
    if (scrollOffset > maxScroll) scrollOffset = maxScroll;
    if (scrollOffset < 0.0f) scrollOffset = 0.0f;
    
    float rowY = headerY + 30.0f - scrollOffset;
    
    ID2D1RenderTarget* rt = renderer.GetRenderTarget();
    D2D1_RECT_F clipRect = D2D1::RectF(startX + padding, clipTop, windowWidth - padding, clipBottom);
    rt->PushAxisAlignedClip(clipRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    
    if (filteredVolumes.empty()) {
        renderer.DrawTextW(L"No volumes to display.", startX + padding + 20.0f, rowY + 15.0f, contentWidth - padding * 2, 30.0f, 14.0f, textSecondary);
    } else {
        for (const auto& vol : filteredVolumes) {
            if (rowY + 50.0f < clipTop || rowY > clipBottom) {
                rowY += 50.0f;
                continue;
            }
            
            std::wstring name(vol.name.begin(), vol.name.end());
            std::wstring driver(vol.driver.begin(), vol.driver.end());
            std::wstring mount(vol.mountpoint.begin(), vol.mountpoint.end());
            
            float textBaseline = rowY + 15.0f;
            float iconY = rowY + 15.0f;
            float actionIconY = rowY + 13.0f;
            
            // Volume icon (glyph 0xE18C - HardDrive)
            renderer.DrawIcon((wchar_t)0xE18C, colIconX, iconY, 18.0f, textSecondary);
            
            // Name (ellipsized in DrawTextW if it overflows, but for now 200px limit)
            renderer.DrawTextW(name, colNameX, textBaseline, 200.0f, 20.0f, 14.0f, textPrimary, DWRITE_FONT_WEIGHT_SEMI_BOLD);
            renderer.DrawTextW(driver, colDriverX, textBaseline, 100.0f, 20.0f, 12.0f, textSecondary);
            renderer.DrawTextW(mount, colMountpointX, textBaseline, 250.0f, 20.0f, 12.0f, textSecondary);
            
            // Delete action
            float actionIconSize = 20.0f;
            float hitBoxPadding = 6.0f;
            float hitBoxSize = actionIconSize + hitBoxPadding * 2.0f;
            
            float hitLeft = colActionX - hitBoxPadding;
            float hitTop = actionIconY - hitBoxPadding;
            float hitRight = hitLeft + hitBoxSize;
            float hitBottom = hitTop + hitBoxSize;
            
            bool isHovered = !isLoading && (mouseX >= hitLeft && mouseX <= hitRight && mouseY >= hitTop && mouseY <= hitBottom);
            
            if (isHovered) {
                renderer.FillRoundedRectangle(hitLeft, hitTop, hitBoxSize, hitBoxSize, 4.0f, 4.0f, hoverBg);
                if (mouseClicked && onAction) {
                    onAction(vol.name, "delete");
                }
            }
            
            D2D1::ColorF iconColor = isHovered ? textPrimary : textSecondary;
            renderer.DrawIcon((wchar_t)0xE74D, colActionX, actionIconY, actionIconSize, iconColor); // Trash glyph
            
            rowY += 50.0f;
            renderer.FillRectangle(startX + padding, rowY, contentWidth - padding * 2, 1.0f, separator);
        }
    }
    
    rt->PopAxisAlignedClip();
    
    // 4. Render Loading Overlay over the List Card
    if (isLoading) {
        renderer.FillRoundedRectangle(startX + padding, listY, contentWidth - padding * 2, listHeight, 8.0f, 8.0f, D2D1::ColorF(0.04f, 0.04f, 0.06f, 0.75f));
        renderer.DrawRoundedRectangle(startX + padding, listY, contentWidth - padding * 2, listHeight, 8.0f, 8.0f, cardStroke, 1.0f);
        
        float centerX = startX + padding + (contentWidth - padding * 2) / 2.0f;
        float centerY = listY + listHeight / 2.0f;
        
        float angle = (GetTickCount() % 1200) / 1200.0f * 2.0f * 3.14159f;
        
        renderer.DrawRoundedRectangle(centerX - 18.0f, centerY - 18.0f, 36.0f, 36.0f, 18.0f, 18.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.1f), 2.0f);
        
        float dotX = centerX + cos(angle) * 18.0f;
        float dotY = centerY + sin(angle) * 18.0f;
        renderer.FillRoundedRectangle(dotX - 4.0f, dotY - 4.0f, 8.0f, 8.0f, 4.0f, 4.0f, accentColor);
        
        renderer.DrawTextW(loadingText, centerX - 200.0f, centerY + 28.0f, 400.0f, 30.0f, 14.0f, textPrimary, DWRITE_FONT_WEIGHT_SEMI_BOLD, true);
    }
}
