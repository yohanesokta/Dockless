#include "DockerClient.h"
#include "../third_party/json.hpp"
#include "../third_party/httplib.h"
#include <iostream>
#include <fstream>
#include <windows.h>

#include <unordered_map>

using json = nlohmann::json;

void DebugLog(const std::string& msg) {
    std::ofstream ofs("docker_debug.txt", std::ios::app);
    ofs << msg << std::endl;
}

static std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string str(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], size, nullptr, nullptr);
    return str;
}

DockerClient::DockerClient(const std::wstring& endpoint, int port) 
    : m_endpoint(endpoint), m_port(port) {
}

DockerClient::~DockerClient() {
}

bool DockerClient::Connect() {
    std::string host = WideToUtf8(m_endpoint);
    
    DebugLog("Connecting to host: " + host + " port: " + std::to_string(m_port));
    
    m_client = std::make_unique<httplib::Client>(host, m_port);
    m_client->set_connection_timeout(5, 0);
    m_client->set_read_timeout(5, 0);
    m_client->set_write_timeout(5, 0);
    m_client->set_follow_location(false);
    
    // Test connection immediately
    auto res = m_client->Get("/_ping");
    if (res) {
        DebugLog("Connect test OK - status: " + std::to_string(res->status));
        return true;
    } else {
        auto err = res.error();
        DebugLog("Connect test FAILED - error: " + std::to_string(static_cast<int>(err)));
        
        // Retry with URL-style constructor
        std::string url = "http://" + host + ":" + std::to_string(m_port);
        DebugLog("Retrying with URL: " + url);
        m_client = std::make_unique<httplib::Client>(url);
        m_client->set_connection_timeout(5, 0);
        m_client->set_read_timeout(5, 0);
        m_client->set_write_timeout(5, 0);
        
        res = m_client->Get("/_ping");
        if (res) {
            DebugLog("Retry OK - status: " + std::to_string(res->status));
            return true;
        } else {
            err = res.error();
            DebugLog("Retry FAILED - error: " + std::to_string(static_cast<int>(err)));
            return false;
        }
    }
}

bool DockerClient::Ping() {
    if (!m_client) return false;
    
    auto res = m_client->Get("/_ping");
    if (res) {
        DebugLog("Ping status: " + std::to_string(res->status) + " body: " + res->body);
        return res->status == 200;
    } else {
        auto err = res.error();
        DebugLog("Ping failed with httplib error: " + std::to_string(static_cast<int>(err)));
    }
    return false;
}

std::vector<DockerContainer> DockerClient::GetContainers() {
    std::vector<DockerContainer> containers;
    if (!m_client) {
        DebugLog("m_client is null");
        return containers;
    }

    // Build a map of Image ID -> Tag from /images/json
    std::unordered_map<std::string, std::string> imageIdToTag;
    auto imgRes = m_client->Get("/images/json");
    if (imgRes && imgRes->status == 200) {
        try {
            auto imgJ = json::parse(imgRes->body);
            if (imgJ.is_array()) {
                for (const auto& img : imgJ) {
                    std::string imgId = img.value("Id", "");
                    if (img.contains("RepoTags") && img["RepoTags"].is_array() && !img["RepoTags"].empty()) {
                        std::string firstTag = img["RepoTags"][0].get<std::string>();
                        if (firstTag != "<none>:<none>") {
                            imageIdToTag[imgId] = firstTag;
                        }
                    }
                }
            }
        } catch (...) {}
    }

    auto res = m_client->Get("/containers/json?all=true");
    
    if (res) {
        DebugLog("Status code: " + std::to_string(res->status));
        if (res->status == 200) {
            try {
                auto j = json::parse(res->body);
                if (j.is_array()) {
                    DebugLog("Parsed JSON array with size: " + std::to_string(j.size()));
                    for (const auto& item : j) {
                        DockerContainer container;
                        container.id = item.value("Id", "");
                        if (container.id.length() > 12) {
                            container.id = container.id.substr(0, 12);
                        }
                        
                        if (item.contains("Names") && item["Names"].is_array() && !item["Names"].empty()) {
                            container.name = item["Names"][0].get<std::string>();
                            if (!container.name.empty() && container.name[0] == '/') {
                                container.name = container.name.substr(1);
                            }
                        }
                        
                        std::string imageId = item.value("ImageID", "");
                        std::string imgName = item.value("Image", "");
                        
                        // If the image name is a raw SHA or missing a tag (no colon), try to look up RepoTags
                        if ((imgName.find("sha256:") == 0 || imgName.find(":") == std::string::npos) && imageIdToTag.count(imageId)) {
                            container.image = imageIdToTag[imageId];
                        } else {
                            container.image = imgName;
                        }
                        
                        container.state = item.value("State", "");
                        container.status = item.value("Status", "");

                        if (item.contains("Ports") && item["Ports"].is_array()) {
                            std::vector<std::string> portStrings;
                            for (const auto& p : item["Ports"]) {
                                int privatePort = p.value("PrivatePort", 0);
                                int publicPort = p.value("PublicPort", 0);
                                std::string type = p.value("Type", "tcp");
                                
                                if (publicPort != 0) {
                                    portStrings.push_back(std::to_string(publicPort) + "->" + std::to_string(privatePort) + "/" + type);
                                } else if (privatePort != 0) {
                                    portStrings.push_back(std::to_string(privatePort) + "/" + type);
                                }
                            }
                            
                            std::string portsCombined;
                            for (size_t i = 0; i < portStrings.size(); ++i) {
                                if (i > 0) portsCombined += ", ";
                                portsCombined += portStrings[i];
                            }
                            container.ports = portsCombined.empty() ? "-" : portsCombined;
                        } else {
                            container.ports = "-";
                        }
                        
                        containers.push_back(container);
                    }
                } else {
                    DebugLog("Response is not a JSON array.");
                }
            } catch (const json::exception& e) {
                DebugLog(std::string("JSON parse error: ") + e.what());
            }
        } else {
            DebugLog("HTTP error: " + std::to_string(res->status));
        }
    } else {
        auto err = res.error();
        DebugLog("HTTP request failed. httplib error code: " + std::to_string(static_cast<int>(err)));
    }

    return containers;
}

bool DockerClient::StopContainer(const std::string& id) {
    if (!m_client) return false;
    auto res = m_client->Post("/containers/" + id + "/stop");
    return res && (res->status == 204 || res->status == 304 || res->status == 200);
}

bool DockerClient::StartContainer(const std::string& id) {
    if (!m_client) return false;
    auto res = m_client->Post("/containers/" + id + "/start");
    return res && (res->status == 204 || res->status == 304 || res->status == 200);
}

bool DockerClient::RestartContainer(const std::string& id) {
    if (!m_client) return false;
    auto res = m_client->Post("/containers/" + id + "/restart");
    return res && (res->status == 204 || res->status == 200);
}

bool DockerClient::DeleteContainer(const std::string& id) {
    if (!m_client) return false;
    // v=true to remove volumes, force=true to force stop and remove
    auto res = m_client->Delete("/containers/" + id + "?v=true&force=true");
    return res && (res->status == 204 || res->status == 200);
}

bool DockerClient::ToggleAutostart(const std::string& id, bool enable) {
    if (!m_client) return false;
    json body = {
        {"RestartPolicy", {
            {"Name", enable ? "always" : "no"}
        }}
    };
    auto res = m_client->Post("/containers/" + id + "/update", body.dump(), "application/json");
    return res && (res->status == 200);
}

std::vector<DockerImage> DockerClient::GetImages() {
    std::vector<DockerImage> images;
    if (!m_client) return images;

    auto res = m_client->Get("/images/json");
    if (res && res->status == 200) {
        try {
            auto j = json::parse(res->body);
            if (j.is_array()) {
                for (const auto& item : j) {
                    DockerImage img;
                    img.id = item.value("Id", "");
                    
                    // Format short ID
                    if (img.id.rfind("sha256:", 0) == 0 && img.id.length() > 19) {
                        img.id = img.id.substr(7, 12);
                    } else if (img.id.length() > 12) {
                        img.id = img.id.substr(0, 12);
                    }

                    if (item.contains("RepoTags") && item["RepoTags"].is_array()) {
                        for (const auto& tag : item["RepoTags"]) {
                            img.repoTags.push_back(tag.get<std::string>());
                        }
                    }

                    img.created = item.value("Created", 0LL);
                    img.size = item.value("Size", 0LL);

                    // Display size
                    double sizeInMb = img.size / 1024.0 / 1024.0;
                    char sizeBuf[64];
                    sprintf_s(sizeBuf, "%.2f MB", sizeInMb);
                    img.displaySize = sizeBuf;

                    // Parse repository and tag
                    if (!img.repoTags.empty() && img.repoTags[0] != "<none>:<none>") {
                        std::string firstTag = img.repoTags[0];
                        size_t colonPos = firstTag.find_last_of(':');
                        if (colonPos != std::string::npos) {
                            img.repository = firstTag.substr(0, colonPos);
                            img.tag = firstTag.substr(colonPos + 1);
                        } else {
                            img.repository = firstTag;
                            img.tag = "<none>";
                        }
                    } else {
                        img.repository = "<none>";
                        img.tag = "<none>";
                    }

                    images.push_back(img);
                }
            }
        } catch (...) {}
    }
    return images;
}

bool DockerClient::PullImage(const std::string& imageName) {
    if (!m_client) return false;
    // URL encode the image name
    std::string path = "/images/create?fromImage=" + imageName;
    auto res = m_client->Post(path);
    return res && (res->status == 200);
}

bool DockerClient::DeleteImage(const std::string& id, bool force) {
    if (!m_client) return false;
    std::string path = "/images/" + id + (force ? "?force=true" : "?force=false");
    auto res = m_client->Delete(path);
    return res && (res->status == 200);
}

bool DockerClient::RunImage(const std::string& imageName,
                              const std::string& containerName,
                              const std::string& portRouting,
                              const std::string& volumeMapping,
                              const std::string& envVars,
                              bool autostart) {
    if (!m_client) return false;
    
    std::string path = "/containers/create";
    if (!containerName.empty()) {
        path += "?name=" + containerName;
    }
    
    json body = {
        {"Image", imageName}
    };
    
    body["HostConfig"] = json::object();
    
    // Parse Port Routing (e.g. "8080:80")
    if (!portRouting.empty()) {
        size_t colonPos = portRouting.find(':');
        if (colonPos != std::string::npos) {
            std::string hostPort = portRouting.substr(0, colonPos);
            std::string containerPort = portRouting.substr(colonPos + 1);
            if (containerPort.find('/') == std::string::npos) {
                containerPort += "/tcp";
            }
            body["HostConfig"]["PortBindings"] = {
                {containerPort, json::array({{{"HostPort", hostPort}}})}
            };
            body["ExposedPorts"] = {
                {containerPort, json::object()}
            };
        }
    }
    
    // Parse Volume Mapping (e.g. "/host/path:/container/path")
    if (!volumeMapping.empty()) {
        body["HostConfig"]["Binds"] = json::array({ volumeMapping });
    }
    
    // Parse Environment Variables (e.g. "KEY1=VAL1,KEY2=VAL2")
    if (!envVars.empty()) {
        std::vector<std::string> envs;
        std::string current;
        for (char c : envVars) {
            if (c == ',') {
                if (!current.empty()) {
                    envs.push_back(current);
                }
                current.clear();
            } else {
                current.push_back(c);
            }
        }
        if (!current.empty()) {
            envs.push_back(current);
        }
        body["Env"] = envs;
    }
    
    // Parse Autostart
    body["HostConfig"]["RestartPolicy"] = {
        {"Name", autostart ? "always" : "no"}
    };
    
    auto res = m_client->Post(path, body.dump(), "application/json");
    if (res && (res->status == 201 || res->status == 200)) {
        try {
            auto j = json::parse(res->body);
            std::string newId = j.value("Id", "");
            if (!newId.empty()) {
                auto startRes = m_client->Post("/containers/" + newId + "/start");
                return startRes && (startRes->status == 204 || startRes->status == 200);
            }
        } catch (...) {}
    }
    return false;
}

std::vector<DockerVolume> DockerClient::GetVolumes() {
    std::vector<DockerVolume> volumes;
    if (!m_client) return volumes;

    auto res = m_client->Get("/volumes");
    if (res && res->status == 200) {
        try {
            auto j = json::parse(res->body);
            if (j.contains("Volumes") && j["Volumes"].is_array()) {
                for (const auto& item : j["Volumes"]) {
                    DockerVolume vol;
                    vol.name = item.value("Name", "");
                    vol.driver = item.value("Driver", "");
                    vol.mountpoint = item.value("Mountpoint", "");
                    vol.createdAt = item.value("CreatedAt", "");
                    volumes.push_back(vol);
                }
            }
        } catch (...) {}
    }
    return volumes;
}

bool DockerClient::CreateVolume(const std::string& name) {
    if (!m_client) return false;
    json body = {
        {"Name", name}
    };
    auto res = m_client->Post("/volumes/create", body.dump(), "application/json");
    return res && (res->status == 201 || res->status == 200);
}

bool DockerClient::DeleteVolume(const std::string& name) {
    if (!m_client) return false;
    auto res = m_client->Delete("/volumes/" + name + "?force=true");
    return res && (res->status == 204 || res->status == 200);
}

std::vector<DockerNetwork> DockerClient::GetNetworks() {
    std::vector<DockerNetwork> networks;
    if (!m_client) return networks;

    auto res = m_client->Get("/networks");
    if (res && res->status == 200) {
        try {
            auto j = json::parse(res->body);
            if (j.is_array()) {
                for (const auto& item : j) {
                    DockerNetwork net;
                    net.id = item.value("Id", "");
                    
                    // Format short ID
                    if (net.id.length() > 12) {
                        net.id = net.id.substr(0, 12);
                    }
                    
                    net.name = item.value("Name", "");
                    net.driver = item.value("Driver", "");
                    net.scope = item.value("Scope", "");
                    networks.push_back(net);
                }
            }
        } catch (...) {}
    }
    return networks;
}

bool DockerClient::CreateNetwork(const std::string& name, const std::string& driver) {
    if (!m_client) return false;
    json body = {
        {"Name", name},
        {"Driver", driver}
    };
    auto res = m_client->Post("/networks/create", body.dump(), "application/json");
    return res && (res->status == 201 || res->status == 200);
}

bool DockerClient::DeleteNetwork(const std::string& id) {
    if (!m_client) return false;
    auto res = m_client->Delete("/networks/" + id);
    return res && (res->status == 204 || res->status == 200);
}


