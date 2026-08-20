#include "DwmManager.h"
#include <dwmapi.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif

#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif
#define DWMWA_COLOR_NONE 0xFFFFFFFE

#ifndef DWMWA_TEXT_COLOR
#define DWMWA_TEXT_COLOR 36
#endif

void DwmManager::EnableDarkMode(HWND hwnd) {
    BOOL value = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
    
    // Set title bar to DWMWA_COLOR_NONE to make it transparent and follow the acrylic effect
    COLORREF color = DWMWA_COLOR_NONE;
    DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &color, sizeof(color));

    // Ensure title text is white
    COLORREF textColor = RGB(255, 255, 255);
    DwmSetWindowAttribute(hwnd, DWMWA_TEXT_COLOR, &textColor, sizeof(textColor));
}

void DwmManager::EnableRoundedCorners(HWND hwnd) {
    int value = 2; // DWMWCP_ROUND
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &value, sizeof(value));
}

void DwmManager::EnableMica(HWND hwnd) {
    int value = 3; // DWMSBT_TRANSIENTWINDOW (Acrylic effect - very transparent glass)
    DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &value, sizeof(value));
}
