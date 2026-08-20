#pragma once

#include <windows.h>

class DwmManager {
public:
    static void EnableDarkMode(HWND hwnd);
    static void EnableRoundedCorners(HWND hwnd);
    static void EnableMica(HWND hwnd);
};
