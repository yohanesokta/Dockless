#pragma once

#include <string>
#include <vector>
#include <memory>

struct DockerContainer {
    std::string id;
    std::string name;
    std::string image;
    std::string state;
    std::string status;
    std::string ports;
};

struct DockerImage {
    std::string id;
    std::vector<std::string> repoTags;
    long long created = 0;
    long long size = 0;
    std::string displaySize;
    std::string repository;
    std::string tag;
};

struct DockerVolume {
    std::string name;
    std::string driver;
    std::string mountpoint;
    std::string createdAt;
};

struct DockerNetwork {
    std::string id;
    std::string name;
    std::string driver;
    std::string scope;
};

// Forward declare httplib::Client
namespace httplib { class Client; }

class DockerClient {
public:
    DockerClient(const std::wstring& endpoint = L"127.0.0.1", int port = 2375);
    ~DockerClient();

    bool Connect();
    bool Ping();
    std::vector<DockerContainer> GetContainers();
    
    bool StopContainer(const std::string& id);
    bool StartContainer(const std::string& id);
    bool RestartContainer(const std::string& id);
    bool DeleteContainer(const std::string& id);
    bool ToggleAutostart(const std::string& id, bool enable);
    
    std::vector<DockerImage> GetImages();
    bool PullImage(const std::string& imageName);
    bool DeleteImage(const std::string& id, bool force = false);
    bool RunImage(const std::string& imageName,
                  const std::string& containerName,
                  const std::string& portRouting,
                  const std::string& volumeMapping,
                  const std::string& envVars,
                  bool autostart);
    
    std::vector<DockerVolume> GetVolumes();
    bool CreateVolume(const std::string& name);
    bool DeleteVolume(const std::string& name);
    
    std::vector<DockerNetwork> GetNetworks();
    bool CreateNetwork(const std::string& name, const std::string& driver = "bridge");
    bool DeleteNetwork(const std::string& id);
    
private:
    std::wstring m_endpoint;
    int m_port;
    
    std::unique_ptr<httplib::Client> m_client;
};

