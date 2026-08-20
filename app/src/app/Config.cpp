#include "Config.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <iostream>

AppConfig AppConfig::Load(const std::wstring& configPath) {
    AppConfig config;
    
    std::ifstream file(configPath);
    if (!file.is_open()) return config;
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    std::smatch match;
    
    // Parse Docker Endpoint
    if (std::regex_search(content, match, std::regex("<Endpoint>http://([^:]+):(\\d+)</Endpoint>"))) {
        std::string ip = match[1].str();
        config.dockerEndpoint = std::wstring(ip.begin(), ip.end());
        config.dockerPort = std::stoi(match[2].str());
    }
    
    // Parse VM Settings
    if (std::regex_search(content, match, std::regex("<Ram>([^<]+)</Ram>"))) {
        std::string val = match[1].str();
        config.vmRam = std::wstring(val.begin(), val.end());
    }
    if (std::regex_search(content, match, std::regex("<Cpu>([^<]+)</Cpu>"))) {
        std::string val = match[1].str();
        config.vmCpu = std::wstring(val.begin(), val.end());
    }
    if (std::regex_search(content, match, std::regex("<Vmdksize>([^<]+)</Vmdksize>"))) {
        std::string val = match[1].str();
        config.vmDiskSize = std::wstring(val.begin(), val.end());
    }
    
    return config;
}
