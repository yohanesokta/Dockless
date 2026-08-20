#include "QemuManager.h"

QemuManager::QemuManager(const std::wstring& qemuPath) : m_qemuPath(qemuPath), m_hProcess(nullptr) {
}

QemuManager::~QemuManager() {
    StopVM();
}

bool QemuManager::StartVM() {
    if (IsRunning()) return true;

    // We can call start_vm.bat instead of raw qemu here
    // For now, this is a scaffold
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };

    std::wstring cmd = L"cmd.exe /c start_vm.bat";
    if (CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, L"..", &si, &pi)) {
        m_hProcess = pi.hProcess;
        CloseHandle(pi.hThread);
        return true;
    }
    return false;
}

bool QemuManager::StopVM() {
    if (m_hProcess) {
        TerminateProcess(m_hProcess, 0);
        CloseHandle(m_hProcess);
        m_hProcess = nullptr;

        // Optionally call poweroff_vm.bat here
    }
    return true;
}

bool QemuManager::IsRunning() const {
    if (!m_hProcess) return false;
    
    DWORD exitCode = 0;
    if (GetExitCodeProcess(m_hProcess, &exitCode)) {
        return exitCode == STILL_ACTIVE;
    }
    return false;
}
