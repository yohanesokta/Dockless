#include "ContainersView.h"

ContainersView::ContainersView() {}

void ContainersView::Render(Renderer& renderer, float startX, float windowWidth, float windowHeight, 
                            const std::vector<DockerContainer>& containers,
                            float mouseX, float mouseY, bool mouseClicked,
                            std::function<void(const std::string&, const std::string&)> onAction,
                            std::wstring& searchQuery, bool& searchFocused, float& scrollOffset,
                            bool isLoading, const std::wstring& loadingText) {
    UNREFERENCED_PARAMETER(windowHeight);
    
    float padding = 40.0f;
    float contentWidth = windowWidth - startX;
    
    // 1. Heading
    renderer.DrawTextW(L"Containers", startX + padding, padding, contentWidth - padding * 2, 40.0f, 32.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f), DWRITE_FONT_WEIGHT_SEMI_BOLD);
    
    // Win11 Settings colors (Glass cards)
    D2D1::ColorF cardBg(0.12f, 0.17f, 0.28f, 0.75f); // Distinct slate glass background for high contrast
    D2D1::ColorF cardStroke(1.0f, 1.0f, 1.0f, 0.10f); // Translucent bright top reflection stroke
    D2D1::ColorF textPrimary(1.0f, 1.0f, 1.0f);
    D2D1::ColorF textSecondary(0.6f, 0.6f, 0.6f); // More muted secondary text
    D2D1::ColorF separator(1.0f, 1.0f, 1.0f, 0.06f);
    D2D1::ColorF hoverBg(1.0f, 1.0f, 1.0f, 0.06f);
    D2D1::ColorF activeHoverBg(1.0f, 1.0f, 1.0f, 0.12f);
    
    // Shadow helper lambda using filled rounded rects shifted down for realistic depth
    auto drawShadow = [&](float x, float y, float w, float h, float r, float size = 12.0f) {
        for (int i = static_cast<int>(size); i >= 1; --i) {
            float offset = static_cast<float>(i);
            float alpha = (1.0f - (offset / size)) * 0.012f; // Soft ambient shadow accumulation
            renderer.FillRoundedRectangle(x - offset, y - offset + 4.0f, w + offset * 2.0f, h + offset * 2.0f,
                                          r + offset, r + offset, D2D1::ColorF(0.0f, 0.0f, 0.0f, alpha));
        }
    };
    
    // 2. Summary Cards (CPU & Memory)
    float cardsY = padding + 60.0f;
    float cardWidth = (contentWidth - padding * 2 - 20.0f) / 2.0f;
    
    // CPU Card
    drawShadow(startX + padding, cardsY, cardWidth, 80.0f, 8.0f);
    renderer.FillRoundedRectangle(startX + padding, cardsY, cardWidth, 80.0f, 8.0f, 8.0f, cardBg);
    renderer.DrawRoundedRectangle(startX + padding, cardsY, cardWidth, 80.0f, 8.0f, 8.0f, cardStroke, 1.0f);
    renderer.DrawTextW(L"Container CPU usage", startX + padding + 20.0f, cardsY + 15.0f, cardWidth, 20.0f, 14.0f, textPrimary, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    renderer.DrawTextW(L"Data unavailable at this time", startX + padding + 20.0f, cardsY + 45.0f, cardWidth, 20.0f, 13.0f, textSecondary);
    
    // Memory Card
    float memCardX = startX + padding + cardWidth + 20.0f;
    drawShadow(memCardX, cardsY, cardWidth, 80.0f, 8.0f);
    renderer.FillRoundedRectangle(memCardX, cardsY, cardWidth, 80.0f, 8.0f, 8.0f, cardBg);
    renderer.DrawRoundedRectangle(memCardX, cardsY, cardWidth, 80.0f, 8.0f, 8.0f, cardStroke, 1.0f);
    renderer.DrawTextW(L"Container memory usage", memCardX + 20.0f, cardsY + 15.0f, cardWidth, 20.0f, 14.0f, textPrimary, DWRITE_FONT_WEIGHT_SEMI_BOLD);
    renderer.DrawTextW(L"Data unavailable at this time", memCardX + 20.0f, cardsY + 45.0f, cardWidth, 20.0f, 13.0f, textSecondary);
    
    // 3. Main List Card Background
    float listY = cardsY + 110.0f;
    float listHeight = 350.0f; // Fixed height to handle scrolling
    drawShadow(startX + padding, listY, contentWidth - padding * 2, listHeight, 8.0f);
    renderer.FillRoundedRectangle(startX + padding, listY, contentWidth - padding * 2, listHeight, 8.0f, 8.0f, cardBg);
    renderer.DrawRoundedRectangle(startX + padding, listY, contentWidth - padding * 2, listHeight, 8.0f, 8.0f, cardStroke, 1.0f);
    
    // Search inside the list card (Top item)
    float searchY = listY + 15.0f;
    float searchBoxLeft = startX + padding + 20.0f;
    float searchBoxRight = searchBoxLeft + 250.0f;
    float searchBoxTop = searchY;
    float searchBoxBottom = searchY + 32.0f;

    // Handle search box focus click (only when not loading)
    if (mouseClicked && !isLoading) {
        if (mouseX >= searchBoxLeft && mouseX <= searchBoxRight && mouseY >= searchBoxTop && mouseY <= searchBoxBottom) {
            searchFocused = true;
        } else {
            searchFocused = false;
        }
    }

    renderer.FillRoundedRectangle(searchBoxLeft, searchY, 250.0f, 32.0f, 4.0f, 4.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.08f));
    renderer.DrawRoundedRectangle(searchBoxLeft, searchY, 250.0f, 32.0f, 4.0f, 4.0f, 
                                  searchFocused ? D2D1::ColorF(0.0f, 0.47f, 0.83f) : separator, 1.5f);
    
    // Render text or placeholder
    if (searchQuery.empty() && !searchFocused) {
        renderer.DrawTextW(L"Search...", searchBoxLeft + 15.0f, searchY + 6.0f, 200.0f, 20.0f, 14.0f, textSecondary);
    } else {
        std::wstring displayStr = searchQuery + (searchFocused ? L"|" : L"");
        renderer.DrawTextW(displayStr, searchBoxLeft + 15.0f, searchY + 6.0f, 200.0f, 20.0f, 14.0f, textPrimary);
    }
    
    // Separator line
    renderer.FillRectangle(startX + padding, searchY + 45.0f, contentWidth - padding * 2, 1.0f, separator);
 
    // Filter containers list based on search query
    std::vector<DockerContainer> filteredContainers;
    if (searchQuery.empty()) {
        filteredContainers = containers;
    } else {
        std::string searchStr;
        for (wchar_t wc : searchQuery) {
            searchStr.push_back(static_cast<char>(wc));
        }
        auto toLower = [](std::string s) {
            for (char &c : s) c = static_cast<char>(tolower(c));
            return s;
        };
        std::string lowerSearch = toLower(searchStr);

        for (const auto& c : containers) {
            if (toLower(c.name).find(lowerSearch) != std::string::npos ||
                toLower(c.image).find(lowerSearch) != std::string::npos) {
                filteredContainers.push_back(c);
            }
        }
    }

    // Define column X positions (used by both header and rows)
    float colIconX   = startX + padding + 20.0f;
    float colNameX   = startX + padding + 55.0f;
    float colImageX  = startX + padding + 240.0f;
    float colStatusX = startX + padding + 420.0f;
    float colPortX   = startX + padding + 550.0f;
    float colActionX = startX + padding + 670.0f;
 
    // Header for list columns
    float headerY = searchY + 55.0f;
    renderer.DrawTextW(L"Name", colNameX, headerY, 150.0f, 20.0f, 14.0f, D2D1::ColorF(0.8f, 0.8f, 0.8f), DWRITE_FONT_WEIGHT_SEMI_BOLD);
    renderer.DrawTextW(L"Image", colImageX, headerY, 150.0f, 20.0f, 14.0f, D2D1::ColorF(0.8f, 0.8f, 0.8f), DWRITE_FONT_WEIGHT_SEMI_BOLD);
    renderer.DrawTextW(L"Status", colStatusX, headerY, 80.0f, 20.0f, 14.0f, D2D1::ColorF(0.8f, 0.8f, 0.8f), DWRITE_FONT_WEIGHT_SEMI_BOLD);
    renderer.DrawTextW(L"Port(s)", colPortX, headerY, 80.0f, 20.0f, 14.0f, D2D1::ColorF(0.8f, 0.8f, 0.8f), DWRITE_FONT_WEIGHT_SEMI_BOLD);
    renderer.DrawTextW(L"Action", colActionX, headerY, 150.0f, 20.0f, 14.0f, D2D1::ColorF(0.8f, 0.8f, 0.8f), DWRITE_FONT_WEIGHT_SEMI_BOLD);
 
    // Calculate scroll limits
    float clipTop = headerY + 25.0f;
    float clipBottom = listY + listHeight - 10.0f;
    float visibleHeight = clipBottom - clipTop;
    float totalRowsHeight = filteredContainers.size() * 50.0f;
    float maxScroll = (totalRowsHeight > visibleHeight) ? (totalRowsHeight - visibleHeight) : 0.0f;
    if (scrollOffset > maxScroll) scrollOffset = maxScroll;
    if (scrollOffset < 0.0f) scrollOffset = 0.0f;

    // 4. Render Rows with clipping and scroll offset
    float rowY = headerY + 30.0f - scrollOffset;

    // Push clipping rectangle to prevent rows drawing outside the card client area
    ID2D1RenderTarget* rt = renderer.GetRenderTarget();
    D2D1_RECT_F clipRect = D2D1::RectF(startX + padding, clipTop, windowWidth - padding, clipBottom);
    rt->PushAxisAlignedClip(clipRect, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    if (filteredContainers.empty()) {
        renderer.DrawTextW(L"No containers to display.", startX + padding + 20.0f, rowY + 15.0f, contentWidth - padding * 2, 30.0f, 14.0f, textSecondary);
    } else {
        for (const auto& container : filteredContainers) {
            // Skip rendering if row is fully out of bounds
            if (rowY + 50.0f < clipTop || rowY > clipBottom) {
                rowY += 50.0f;
                continue;
            }

            std::wstring name(container.name.begin(), container.name.end());
            std::wstring image(container.image.begin(), container.image.end());
            std::wstring state(container.state.begin(), container.state.end());
            
            bool isRunning = (container.state == "running");

            // Vertical baseline/centering for elements in a 50px high row space
            float textBaseline = rowY + 15.0f;
            float iconY = rowY + 13.0f;
            float actionIconY = rowY + 13.0f;

            // Container icon (far left)
            renderer.DrawImage(L"icons/container.png", colIconX, iconY, 24.0f, 24.0f);

            // Name column
            renderer.DrawTextW(name, colNameX, textBaseline, 170.0f, 20.0f, 14.0f,
                                textPrimary, DWRITE_FONT_WEIGHT_SEMI_BOLD);
            // Image column
            renderer.DrawTextW(image, colImageX, textBaseline, 160.0f, 20.0f, 12.0f, textSecondary);
            // Status column
            renderer.DrawTextW(state, colStatusX, textBaseline, 100.0f, 20.0f, 14.0f,
                                isRunning ? D2D1::ColorF(0.4f, 0.85f, 0.6f) : textSecondary);
            std::wstring ports(container.ports.begin(), container.ports.end());
            // Ports column
            renderer.DrawTextW(ports, colPortX, textBaseline, 110.0f, 20.0f, 12.0f, textSecondary);
            
            // Action column - Windows style neutral action buttons (monochrome, proportional)
            float actionIconSize = 20.0f;
            float hitBoxPadding = 6.0f;
            float hitBoxSize = actionIconSize + hitBoxPadding * 2.0f; // 32px hit box

            // Define button render details (spaced farther: 40px)
            struct ActionButton {
                std::string action;
                wchar_t glyph;
                float x;
            };

            std::vector<ActionButton> buttons = {
                { isRunning ? "stop" : "start", isRunning ? (wchar_t)0xE71A : (wchar_t)0xE768, colActionX },
                { "restart", (wchar_t)0xE72C, colActionX + 40.0f },
                { "delete", (wchar_t)0xE74D, colActionX + 80.0f },
                { "more", (wchar_t)0xE712, colActionX + 120.0f }
            };

            for (const auto& btn : buttons) {
                float hitLeft = btn.x - hitBoxPadding;
                float hitTop = actionIconY - hitBoxPadding;
                float hitRight = hitLeft + hitBoxSize;
                float hitBottom = hitTop + hitBoxSize;

                // Disable button hover/clicks during loading
                bool isHovered = !isLoading && (mouseX >= hitLeft && mouseX <= hitRight && mouseY >= hitTop && mouseY <= hitBottom);

                if (isHovered) {
                    renderer.FillRoundedRectangle(hitLeft, hitTop, hitBoxSize, hitBoxSize, 4.0f, 4.0f, hoverBg);
                    if (mouseClicked && onAction) {
                        onAction(container.id, btn.action);
                    }
                }

                D2D1::ColorF iconColor = isHovered ? textPrimary : textSecondary;
                renderer.DrawIcon(btn.glyph, btn.x, actionIconY, actionIconSize, iconColor);
            }

            // Advance to next row
            rowY += 50.0f;
            renderer.FillRectangle(startX + padding, rowY, contentWidth - padding * 2, 1.0f, separator);
        }
    }

    rt->PopAxisAlignedClip();

    // 5. Render Loading Overlay over the List Card
    if (isLoading) {
        // Dark semi-transparent card backdrop
        renderer.FillRoundedRectangle(startX + padding, listY, contentWidth - padding * 2, listHeight, 8.0f, 8.0f, D2D1::ColorF(0.04f, 0.04f, 0.06f, 0.75f));
        renderer.DrawRoundedRectangle(startX + padding, listY, contentWidth - padding * 2, listHeight, 8.0f, 8.0f, cardStroke, 1.0f);

        float centerX = startX + padding + (contentWidth - padding * 2) / 2.0f;
        float centerY = listY + listHeight / 2.0f;

        // Smooth modern spinning orbit animation using GetTickCount
        float angle = (GetTickCount() % 1200) / 1200.0f * 2.0f * 3.14159f;

        // Orbit ring track
        renderer.DrawRoundedRectangle(centerX - 18.0f, centerY - 38.0f, 36.0f, 36.0f, 18.0f, 18.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.1f), 2.0f);

        // Orbiting accent dot
        float dotX = centerX + cos(angle) * 18.0f;
        float dotY = (centerY - 20.0f) + sin(angle) * 18.0f;
        renderer.FillRoundedRectangle(dotX - 4.0f, dotY - 4.0f, 8.0f, 8.0f, 4.0f, 4.0f, D2D1::ColorF(0.0f, 0.47f, 0.83f)); // Fluent Accent Blue

        // Status text centered below the spinner
        renderer.DrawTextW(loadingText, centerX - 200.0f, centerY + 18.0f, 400.0f, 30.0f, 14.0f, textPrimary, DWRITE_FONT_WEIGHT_SEMI_BOLD, true);
    }
}
