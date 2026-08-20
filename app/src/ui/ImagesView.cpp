#include "ImagesView.h"
#include <cwchar>

ImagesView::ImagesView() {}

static std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string str(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size, nullptr, nullptr);
    return str;
}

void ImagesView::Render(Renderer& renderer, float startX, float windowWidth, float windowHeight, 
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
                        bool& dialogAutostart) {
    
    float padding = 40.0f;
    float contentWidth = windowWidth - startX;
    
    // 1. Heading
    renderer.DrawTextW(L"Images", startX + padding, padding, contentWidth - padding * 2, 40.0f, 32.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f), DWRITE_FONT_WEIGHT_SEMI_BOLD);
    
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
    
    // 2. Pull Image Section
    float pullY = padding + 60.0f;
    float pullInputLeft = startX + padding;
    float pullInputWidth = 300.0f;
    float pullInputHeight = 32.0f;
    float pullBtnLeft = pullInputLeft + pullInputWidth + 12.0f;
    float pullBtnWidth = 100.0f;
    float pullBtnHeight = 32.0f;
    
    // Handle pull input box focus (only if dialog is not open)
    if (mouseClicked && !isLoading && !isRunDialogOpen) {
        if (mouseX >= pullInputLeft && mouseX <= pullInputLeft + pullInputWidth &&
            mouseY >= pullY && mouseY <= pullY + pullInputHeight) {
            pullFocused = true;
            searchFocused = true; // Wait, actually set searchFocused false
            searchFocused = false;
        }
    }
    
    // Draw Pull Input Box
    renderer.FillRoundedRectangle(pullInputLeft, pullY, pullInputWidth, pullInputHeight, 4.0f, 4.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.08f));
    renderer.DrawRoundedRectangle(pullInputLeft, pullY, pullInputWidth, pullInputHeight, 4.0f, 4.0f, 
                                  pullFocused ? accentColor : separator, 1.5f);
    
    if (pullQuery.empty() && !pullFocused) {
        renderer.DrawTextW(L"Pull image, e.g. nginx:latest", pullInputLeft + 12.0f, pullY + 6.0f, pullInputWidth - 24.0f, 20.0f, 14.0f, textSecondary);
    } else {
        std::wstring displayStr = pullQuery + (pullFocused ? L"|" : L"");
        renderer.DrawTextW(displayStr, pullInputLeft + 12.0f, pullY + 6.0f, pullInputWidth - 24.0f, 20.0f, 14.0f, textPrimary);
    }
    
    // Draw Pull Button (Centering pull text corrected: y is pullY, not pullY + 6.0f)
    bool isPullBtnHovered = !isLoading && !isRunDialogOpen && (mouseX >= pullBtnLeft && mouseX <= pullBtnLeft + pullBtnWidth &&
                                           mouseY >= pullY && mouseY <= pullY + pullBtnHeight);
    
    D2D1::ColorF btnBg = isPullBtnHovered ? D2D1::ColorF(0.0f, 0.54f, 0.93f) : accentColor;
    renderer.FillRoundedRectangle(pullBtnLeft, pullY, pullBtnWidth, pullBtnHeight, 4.0f, 4.0f, btnBg);
    renderer.DrawTextW(L"Pull", pullBtnLeft, pullY, pullBtnWidth, pullBtnHeight, 14.0f, textPrimary, DWRITE_FONT_WEIGHT_SEMI_BOLD, true);
    
    if (isPullBtnHovered && mouseClicked && onAction && !pullQuery.empty()) {
        std::string imageName = WideToUtf8(pullQuery);
        onAction(imageName, "pull");
    }
    
    // 3. Main List Card Background
    float listY = pullY + 55.0f;
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
    
    if (mouseClicked && !isLoading && !isRunDialogOpen) {
        if (mouseX >= searchBoxLeft && mouseX <= searchBoxLeft + searchBoxWidth &&
            mouseY >= searchY && mouseY <= searchY + searchBoxHeight) {
            searchFocused = true;
            pullFocused = false;
        } else if (!(mouseX >= pullInputLeft && mouseX <= pullInputLeft + pullInputWidth &&
                     mouseY >= pullY && mouseY <= pullY + pullInputHeight)) {
            searchFocused = false;
            pullFocused = false;
        }
    }
    
    renderer.FillRoundedRectangle(searchBoxLeft, searchY, searchBoxWidth, searchBoxHeight, 4.0f, 4.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.08f));
    renderer.DrawRoundedRectangle(searchBoxLeft, searchY, searchBoxWidth, searchBoxHeight, 4.0f, 4.0f, 
                                  searchFocused ? accentColor : separator, 1.5f);
    
    if (searchQuery.empty() && !searchFocused) {
        renderer.DrawTextW(L"Search images...", searchBoxLeft + 15.0f, searchY + 6.0f, searchBoxWidth - 30.0f, 20.0f, 14.0f, textSecondary);
    } else {
        std::wstring displayStr = searchQuery + (searchFocused ? L"|" : L"");
        renderer.DrawTextW(displayStr, searchBoxLeft + 15.0f, searchY + 6.0f, searchBoxWidth - 30.0f, 20.0f, 14.0f, textPrimary);
    }
    
    renderer.FillRectangle(startX + padding, searchY + 45.0f, contentWidth - padding * 2, 1.0f, separator);
    
    // Filter Images
    std::vector<DockerImage> filteredImages;
    if (searchQuery.empty()) {
        filteredImages = images;
    } else {
        std::string searchStr = WideToUtf8(searchQuery);
        auto toLower = [](std::string s) {
            for (char &c : s) c = static_cast<char>(tolower(c));
            return s;
        };
        std::string lowerSearch = toLower(searchStr);
        for (const auto& img : images) {
            if (toLower(img.repository).find(lowerSearch) != std::string::npos ||
                toLower(img.tag).find(lowerSearch) != std::string::npos ||
                toLower(img.id).find(lowerSearch) != std::string::npos) {
                filteredImages.push_back(img);
            }
        }
    }
    
    // Columns Layout
    float colIconX       = startX + padding + 20.0f;
    float colRepositoryX = startX + padding + 60.0f;
    float colTagX        = startX + padding + 280.0f;
    float colIdX         = startX + padding + 400.0f;
    float colSizeX       = startX + padding + 550.0f;
    float colActionX     = startX + padding + 670.0f;
    
    float headerY = searchY + 55.0f;
    renderer.DrawTextW(L"Repository", colRepositoryX, headerY, 200.0f, 20.0f, 14.0f, D2D1::ColorF(0.8f, 0.8f, 0.8f), DWRITE_FONT_WEIGHT_SEMI_BOLD);
    renderer.DrawTextW(L"Tag", colTagX, headerY, 100.0f, 20.0f, 14.0f, D2D1::ColorF(0.8f, 0.8f, 0.8f), DWRITE_FONT_WEIGHT_SEMI_BOLD);
    renderer.DrawTextW(L"Image ID", colIdX, headerY, 120.0f, 20.0f, 14.0f, D2D1::ColorF(0.8f, 0.8f, 0.8f), DWRITE_FONT_WEIGHT_SEMI_BOLD);
    renderer.DrawTextW(L"Size", colSizeX, headerY, 100.0f, 20.0f, 14.0f, D2D1::ColorF(0.8f, 0.8f, 0.8f), DWRITE_FONT_WEIGHT_SEMI_BOLD);
    renderer.DrawTextW(L"Action", colActionX, headerY, 120.0f, 20.0f, 14.0f, D2D1::ColorF(0.8f, 0.8f, 0.8f), DWRITE_FONT_WEIGHT_SEMI_BOLD);
    
    // Scroll limits
    float clipTop = headerY + 25.0f;
    float clipBottom = listY + listHeight - 10.0f;
    float visibleHeight = clipBottom - clipTop;
    float totalRowsHeight = filteredImages.size() * 50.0f;
    float maxScroll = (totalRowsHeight > visibleHeight) ? (totalRowsHeight - visibleHeight) : 0.0f;
    if (scrollOffset > maxScroll) scrollOffset = maxScroll;
    if (scrollOffset < 0.0f) scrollOffset = 0.0f;
    
    float rowY = headerY + 30.0f - scrollOffset;
    
    ID2D1RenderTarget* rt = renderer.GetRenderTarget();
    D2D1_RECT_F clipRect = D2D1::RectF(startX + padding, clipTop, windowWidth - padding, clipBottom);
    rt->PushAxisAlignedClip(clipRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    
    if (filteredImages.empty()) {
        renderer.DrawTextW(L"No images to display.", startX + padding + 20.0f, rowY + 15.0f, contentWidth - padding * 2, 30.0f, 14.0f, textSecondary);
    } else {
        for (const auto& img : filteredImages) {
            if (rowY + 50.0f < clipTop || rowY > clipBottom) {
                rowY += 50.0f;
                continue;
            }
            
            std::wstring repo(img.repository.begin(), img.repository.end());
            std::wstring tag(img.tag.begin(), img.tag.end());
            std::wstring shortId(img.id.begin(), img.id.end());
            std::wstring displaySize(img.displaySize.begin(), img.displaySize.end());
            
            float textBaseline = rowY + 15.0f;
            float iconY = rowY + 15.0f;
            float actionIconY = rowY + 13.0f;
            
            // Image/Photo icon (glyph 0xE8B9)
            renderer.DrawIcon((wchar_t)0xE8B9, colIconX, iconY, 18.0f, textSecondary);
            
            renderer.DrawTextW(repo, colRepositoryX, textBaseline, 200.0f, 20.0f, 14.0f, textPrimary, DWRITE_FONT_WEIGHT_SEMI_BOLD);
            renderer.DrawTextW(tag, colTagX, textBaseline, 100.0f, 20.0f, 12.0f, textSecondary);
            renderer.DrawTextW(shortId, colIdX, textBaseline, 120.0f, 20.0f, 12.0f, textSecondary);
            renderer.DrawTextW(displaySize, colSizeX, textBaseline, 100.0f, 20.0f, 12.0f, textSecondary);
            
            // Actions
            float actionIconSize = 20.0f;
            float hitBoxPadding = 6.0f;
            float hitBoxSize = actionIconSize + hitBoxPadding * 2.0f;
            
            struct ActionButton {
                std::string action;
                wchar_t glyph;
                float x;
            };
            
            // Run container (Play glyph 0xE768), Delete image (Trash glyph 0xE74D)
            std::vector<ActionButton> buttons = {
                { "run", (wchar_t)0xE768, colActionX },
                { "delete", (wchar_t)0xE74D, colActionX + 45.0f }
            };
            
            for (const auto& btn : buttons) {
                float hitLeft = btn.x - hitBoxPadding;
                float hitTop = actionIconY - hitBoxPadding;
                float hitRight = hitLeft + hitBoxSize;
                float hitBottom = hitTop + hitBoxSize;
                
                bool isHovered = !isLoading && !isRunDialogOpen && (mouseX >= hitLeft && mouseX <= hitRight && mouseY >= hitTop && mouseY <= hitBottom);
                
                if (isHovered) {
                    renderer.FillRoundedRectangle(hitLeft, hitTop, hitBoxSize, hitBoxSize, 4.0f, 4.0f, hoverBg);
                    if (mouseClicked && onAction) {
                        std::string target = (btn.action == "run") ? (img.repository + ":" + img.tag) : img.id;
                        onAction(target, btn.action);
                    }
                }
                
                D2D1::ColorF iconColor = isHovered ? textPrimary : textSecondary;
                renderer.DrawIcon(btn.glyph, btn.x, actionIconY, actionIconSize, iconColor);
            }
            
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
    
    // 5. Render Run Dialog/Overlay Form
    if (isRunDialogOpen) {
        // Draw dark full-screen overlay backdrop
        renderer.FillRectangle(0, 0, windowWidth, windowHeight, D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.6f));
        
        float dialogWidth = 460.0f;
        float dialogHeight = 420.0f;
        float dialogX = (windowWidth - dialogWidth) / 2.0f;
        float dialogY = (windowHeight - dialogHeight) / 2.0f;
        
        // Fill and border dialog card
        renderer.FillRoundedRectangle(dialogX, dialogY, dialogWidth, dialogHeight, 8.0f, 8.0f, D2D1::ColorF(0.12f, 0.12f, 0.15f, 0.98f));
        renderer.DrawRoundedRectangle(dialogX, dialogY, dialogWidth, dialogHeight, 8.0f, 8.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.15f), 1.0f);
        
        // Header
        renderer.DrawTextW(L"Run Container", dialogX + 24.0f, dialogY + 20.0f, dialogWidth - 48.0f, 30.0f, 18.0f, textPrimary, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        std::wstring imgWName(dialogImageName.begin(), dialogImageName.end());
        renderer.DrawTextW(imgWName, dialogX + 24.0f, dialogY + 50.0f, dialogWidth - 48.0f, 20.0f, 12.0f, textSecondary);
        
        // Handle input click/focus
        if (mouseClicked) {
            dialogContainerNameFocused = false;
            dialogPortRoutingFocused = false;
            dialogVolumeMappingFocused = false;
            dialogEnvVarsFocused = false;
            
            float inputX = dialogX + 160.0f;
            float inputW = 276.0f;
            float inputH = 28.0f;
            
            if (mouseX >= inputX && mouseX <= inputX + inputW && mouseY >= dialogY + 90.0f && mouseY <= dialogY + 90.0f + inputH) {
                dialogContainerNameFocused = true;
            } else if (mouseX >= inputX && mouseX <= inputX + inputW && mouseY >= dialogY + 130.0f && mouseY <= dialogY + 130.0f + inputH) {
                dialogPortRoutingFocused = true;
            } else if (mouseX >= inputX && mouseX <= inputX + inputW && mouseY >= dialogY + 170.0f && mouseY <= dialogY + 170.0f + inputH) {
                dialogVolumeMappingFocused = true;
            } else if (mouseX >= inputX && mouseX <= inputX + inputW && mouseY >= dialogY + 210.0f && mouseY <= dialogY + 210.0f + inputH) {
                dialogEnvVarsFocused = true;
            } else if (mouseX >= inputX && mouseX <= inputX + 60.0f && mouseY >= dialogY + 250.0f && mouseY <= dialogY + 250.0f + inputH) {
                dialogAutostart = !dialogAutostart;
            } else if (mouseX >= dialogX + 228.0f && mouseX <= dialogX + 328.0f && mouseY >= dialogY + 360.0f && mouseY <= dialogY + 392.0f) {
                onAction(dialogImageName, "submit_run");
            } else if (mouseX >= dialogX + 338.0f && mouseX <= dialogX + 428.0f && mouseY >= dialogY + 360.0f && mouseY <= dialogY + 392.0f) {
                onAction("", "cancel_run");
            }
        }
        
        // Helper to draw label and input box
        auto drawDialogInput = [&](const std::wstring& label, float y, const std::wstring& text, bool focused, const std::wstring& placeholder) {
            renderer.DrawTextW(label, dialogX + 24.0f, y + 4.0f, 130.0f, 20.0f, 13.0f, textPrimary, DWRITE_FONT_WEIGHT_SEMI_BOLD);
            float inputX = dialogX + 160.0f;
            renderer.FillRoundedRectangle(inputX, y, 276.0f, 28.0f, 4.0f, 4.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.08f));
            renderer.DrawRoundedRectangle(inputX, y, 276.0f, 28.0f, 4.0f, 4.0f, focused ? accentColor : separator, 1.5f);
            
            if (text.empty() && !focused) {
                renderer.DrawTextW(placeholder, inputX + 10.0f, y + 4.0f, 256.0f, 20.0f, 13.0f, textSecondary);
            } else {
                std::wstring displayStr = text + (focused ? L"|" : L"");
                renderer.DrawTextW(displayStr, inputX + 10.0f, y + 4.0f, 256.0f, 20.0f, 13.0f, textPrimary);
            }
        };
        
        drawDialogInput(L"Container Name", dialogY + 90.0f, dialogContainerName, dialogContainerNameFocused, L"e.g. my-nginx");
        drawDialogInput(L"Port Routing", dialogY + 130.0f, dialogPortRouting, dialogPortRoutingFocused, L"e.g. 8080:80");
        drawDialogInput(L"Volume Mapping", dialogY + 170.0f, dialogVolumeMapping, dialogVolumeMappingFocused, L"e.g. /host/path:/container/path");
        drawDialogInput(L"Env Variables", dialogY + 210.0f, dialogEnvVars, dialogEnvVarsFocused, L"e.g. KEY=VAL,DEBUG=1");
        
        // Autostart Policy
        renderer.DrawTextW(L"Autostart", dialogX + 24.0f, dialogY + 254.0f, 130.0f, 20.0f, 13.0f, textPrimary, DWRITE_FONT_WEIGHT_SEMI_BOLD);
        float toggleX = dialogX + 160.0f;
        float toggleY = dialogY + 250.0f;
        renderer.FillRoundedRectangle(toggleX, toggleY, 60.0f, 28.0f, 4.0f, 4.0f, dialogAutostart ? accentColor : D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.08f));
        renderer.DrawRoundedRectangle(toggleX, toggleY, 60.0f, 28.0f, 4.0f, 4.0f, separator, 1.0f);
        renderer.DrawTextW(dialogAutostart ? L"Yes" : L"No", toggleX, toggleY + 4.0f, 60.0f, 20.0f, 13.0f, textPrimary, DWRITE_FONT_WEIGHT_NORMAL, true);
        
        // Buttons
        // Run button
        bool isRunBtnHovered = mouseX >= dialogX + 228.0f && mouseX <= dialogX + 328.0f && mouseY >= dialogY + 360.0f && mouseY <= dialogY + 392.0f;
        renderer.FillRoundedRectangle(dialogX + 228.0f, dialogY + 360.0f, 100.0f, 32.0f, 4.0f, 4.0f, isRunBtnHovered ? D2D1::ColorF(0.0f, 0.54f, 0.93f) : accentColor);
        renderer.DrawTextW(L"Run", dialogX + 228.0f, dialogY + 360.0f, 100.0f, 32.0f, 13.0f, textPrimary, DWRITE_FONT_WEIGHT_SEMI_BOLD, true);
        
        // Cancel button
        bool isCancelBtnHovered = mouseX >= dialogX + 338.0f && mouseX <= dialogX + 428.0f && mouseY >= dialogY + 360.0f && mouseY <= dialogY + 392.0f;
        renderer.FillRoundedRectangle(dialogX + 338.0f, dialogY + 360.0f, 90.0f, 32.0f, 4.0f, 4.0f, isCancelBtnHovered ? D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.12f) : D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.06f));
        renderer.DrawRoundedRectangle(dialogX + 338.0f, dialogY + 360.0f, 90.0f, 32.0f, 4.0f, 4.0f, separator, 1.0f);
        renderer.DrawTextW(L"Cancel", dialogX + 338.0f, dialogY + 360.0f, 90.0f, 32.0f, 13.0f, textPrimary, DWRITE_FONT_WEIGHT_NORMAL, true);
    }
}
