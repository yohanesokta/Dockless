#pragma once

#include <string>
#include <windows.h>

class QemuManager {
public:
    QemuManager(const std::wstring& qemuPath);
    ~QemuManager();

    bool StartVM();
    bool StopVM();
    bool IsRunning() const;

private:
    std::wstring m_qemuPath;
    HANDLE m_hProcess;
};
