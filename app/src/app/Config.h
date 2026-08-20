#pragma once
#include <string>

struct AppConfig {
    std::wstring dockerEndpoint = L"192.168.100.2";
    int dockerPort = 2375;
    std::wstring vmRam = L"2G";
    std::wstring vmCpu = L"max";
    std::wstring vmDiskSize = L"50G";
    
    static AppConfig Load(const std::wstring& configPath);
};