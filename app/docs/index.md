# Dockless Application – Comprehensive Documentation

## Table of Contents
1. [Project Overview](#project-overview)
2. [Directory Structure](#directory-structure)
3. [Build Instructions](#build-instructions)
4. [Run & Usage](#run--usage)
5. [Configuration (`config.xml`)](#configuration-configxml)
6. [VM Lifecycle](#vm-lifecycle)
7. [User Interface Overview](#user-interface-overview)
   - Sidebar
   - Containers View
   - Images View
   - Logs Overlay
   - System‑Tray Menu
   - Create‑Container Dialog
8. [Key Implementation Details](#key-implementation-details)
   - Single‑Instance Guard
   - Clipboard Paste (Ctrl+V) & Select‑All (Ctrl+A)
   - Thread‑Safe Container Run
   - Autostart Toggle
9. [Common Issues & Troubleshooting](#common-issues--troubleshooting)
10. [Extending the Application](#extending-the-application)
11. [Useful Source Links](#useful-source-links)

---

## Project Overview
**Dockless** is a native Windows C++ application that provides a lightweight graphical interface for managing Docker containers **inside a QEMU‑based Alpine Linux VM**. It abstracts away the VM handling, exposing Docker functionality directly from the UI.

Key features:
- Automatic VM start on first launch (single‑core to avoid Docker daemon start failures).
- System‑tray integration with options to **Show Apps**, **Restart VM**, **Stop VM**, and **Exit**.
- Real‑time log view floating above the activity bar.
- Create‑container dialog supporting container name, port mapping, volume mapping, environment variables, and autostart.
- Clipboard paste support (Ctrl+V) for all text fields.
- Single‑instance guarantee – subsequent launches focus the existing window.

---

## Directory Structure
```
app/
├─ src/                # C++ source code
│   ├─ app/           # Core application logic
│   │   ├─ Window.h               // Window class declaration
│   │   └─ Window.cpp             // Message handling, VM start/stop, UI rendering
│   ├─ ui/            # UI widgets
│   │   ├─ Sidebar.h / Sidebar.cpp    // Sidebar + refresh button + logs overlay
│   │   ├─ ImagesView.h / ImagesView.cpp // Image list, pull, delete, run dialog
│   │   ├─ ContainersView.h / ContainersView.cpp // Container list & actions
│   │   └─ ... (other view components)
│   ├─ docker/        // Thin Docker HTTP client wrapper
│   │   ├─ DockerClient.h / DockerClient.cpp // API calls: list, run, stop, etc.
│   └─ main.cpp       // Entry point, creates named mutex and launches Window
├─ resources/          # Assets & scripts
│   ├─ scripts/start_vm.bat        // Starts QEMU VM with -display none -serial file:vm_log.txt
│   └─ scripts/poweroff_vm.bat    // Calls `poweroff` inside the VM
│   └─ icon.ico                    // Application icon (added to resources)
├─ CMakeLists.txt      # Build configuration (adds resource.rc)
├─ docs/               # Documentation (this file)
└─ config.xml          # VM configuration (RAM, CPU, VMDK size)
```

### Notable Files
- **`Window.cpp`** – Handles Windows messages (`WM_CHAR`, `WM_TRAYICON`, `WM_CLOSE`), starts/stops the VM, renders the overlay UI, and contains the paste‑support logic.
- **`DockerClient.cpp`** – Implements Docker Engine HTTP API calls (`RunImage`, `PullImage`, `CreateContainer`, etc.).
- **`ImagesView.cpp`** – Renders image cards, manages the *Run Container* dialog, and forwards actions to `Window::ExecuteImageAction`.
- **`Sidebar.cpp`** – Draws the navigation sidebar, handles the refresh button, and displays the last two log lines.
- **`config.xml`** – Determines VM resources. After fixing Docker start failures the `<Cpu>` value is set to `1`.

---

## Build Instructions
```powershell
# From the project root (C:\Users\YohanesOktanio\Documents\VM\app)
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Debug
cmake --build . --config Debug
```
The resulting executable is placed at:
```
app\build\Debug\Dockless.exe
```

The CMake configuration includes `src/resource.rc` to embed `icon.ico`.

---

## Run & Usage
1. **First launch** – The app automatically starts the Alpine VM using `scripts/start_vm.bat`. The Docker daemon becomes reachable on `192.168.100.2:2375`.
2. **System tray** – Right‑click the icon for: 
   - **Show Apps** – Restores the window.
   - **Restart VM** – Stops then starts the VM.
   - **Stop VM** – Powers off the VM (`poweroff_vm.bat`).
   - **Exit** – Calls `StopDockerVM(true)` (blocking) before exiting.
3. **Sidebar** – Click *Dockless* to view containers, *Images* to pull/run images, *Volumes* and *Networks* for their respective lists. A refresh icon appears next to the title to force a status update.
4. **Create‑Container Dialog** – Accessed via the *Run* button on an image card.
   - Fill fields (container name, ports, volumes, env vars).
   - Toggle *Autostart*.
   - **Ctrl+V** works in any field; **Ctrl+A** clears the field.
   - Press *Run* – the dialog closes and the container is created & started.
5. **Logs overlay** – The last two lines of `vm_log.txt` float at the top‑right corner, updating in real time.

---

## Configuration (`config.xml`)
Located two levels above the `app` folder:
```xml
<Configuration>
    <Docker/>
    <Vm>
        <Ram>2G</Ram>
        <Cpu>1</Cpu>  <!-- Must be 1 core for Docker daemon to start reliably -->
        <Vmdksize>50G</Vmdksize>
    </Vm>
</Configuration>
```
- **Ram** – Memory allocated to the VM.
- **Cpu** – Number of virtual cores. `max` caused Docker daemon start failures; `1` works reliably.
- **Vmdksize** – Size of the virtual disk.

After editing `config.xml`, rebuild the project to apply changes.

---

## VM Lifecycle
- **StartDockerVM()** – Called on first UI interaction or when the user clicks *Start VM*; spawns `scripts/start_vm.bat` via `CreateProcessW` with hidden window.
- **StopDockerVM(bool wait)** – Executes `scripts/poweroff_vm.bat`. When `wait` is `true` (used on **Exit**) the call blocks up to 15 seconds for the process to finish, ensuring a clean shutdown.
- **Process detection** – `IsQemuProcessRunning()` checks for `qemu-system-x86_64.exe` to determine VM status.

---

## User Interface Overview
### Sidebar
- Navigation items: **Containers**, **Images**, **Volumes**, **Networks**, **Logs**.
- Refresh button (🔄) forces immediate Docker status polling.
- Two‑line VM log overlay (transparent text) displayed at the top‑right.

### Containers View
- List of running/stopped containers with actions: **Start**, **Stop**, **Restart**, **Delete**, **More** (popup menu).
- *More* offers a terminal (`docker exec -it`) and Autostart toggle.

### Images View
- Grid of Docker images. Each card includes **Pull**, **Delete**, and **Run** buttons.
- *Run* opens the dialog described earlier.

### Logs Overlay
- Implemented in `Window::Render` – draws the last two lines from `vm_log.txt` using a semi‑transparent brush.

### System‑Tray Menu
Implemented in `Window.cpp` under `WM_TRAYICON`. Uses `CreatePopupMenu` and `TrackPopupMenu`.

### Create‑Container Dialog
Fields are stored as `std::wstring` members:
- `m_dialogContainerName`
- `m_dialogPortRouting`
- `m_dialogVolumeMapping`
- `m_dialogEnvVars`
- `m_dialogAutostart` (bool)
The dialog is drawn in `Window::Render` when `m_isRunDialogOpen` is `true`.

---

## Key Implementation Details
### Single‑Instance Guard
```cpp
HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"Global\\DocklessAppSingleInstanceMutex");
if (GetLastError() == ERROR_ALREADY_EXISTS) {
    HWND hwndExisting = FindWindowW(L"DocklessWindowClass", nullptr);
    ShowWindow(hwndExisting, SW_SHOW);
    SetForegroundWindow(hwndExisting);
    return 0; // exit duplicate instance
}
```
Ensures only one process runs; later instances just focus the existing window.

### Clipboard Paste (Ctrl+V) & Select‑All (Ctrl+A)
Implemented in the `WM_CHAR` handler. A helper lambda reads the Unicode clipboard and appends it to the focused `std::wstring`. `Ctrl+A` clears the field:
```cpp
if (ch == 22) { // Ctrl+V
    pasteClipboard(target);
} else if (ch == 1) { // Ctrl+A
    target.clear();
}
```
All input fields (search, pull, create, network create, and dialog fields) use this shared logic via `handleCharInput`.

### Thread‑Safe Container Run
Before launching the background thread for `RunImage`, the dialog values are captured on the UI thread:
```cpp
std::string capturedContainerName = WindowWideToUtf8(m_dialogContainerName);
// ... other captures ...
std::thread([this, target, action, capturedContainerName, ...]() {
    m_dockerClient.RunImage(target, capturedContainerName, ...);
}).detach();
```
Prevents race conditions where the dialog fields are cleared before the thread reads them.

### Autostart Toggle
Stored as `bool m_dialogAutostart`. When the user toggles the checkbox, the value is passed to `DockerClient::ToggleAutostart` (via `RunImage`'s `RestartPolicy`).

---

## Common Issues & Troubleshooting
| Symptom | Likely Cause | Fix |
|---------|--------------|-----|
| Docker daemon refuses connections (`dial tcp 192.168.100.2:2375: connectex` ) | `<Cpu>` set to `max` (multiple cores) causing race condition during VM boot. | Edit `config.xml` → set `<Cpu>1</Cpu>` and rebuild. |
| No logs appear in the overlay | `vm_log.txt` not created or script lacks write permission. | Ensure `scripts/start_vm.bat` uses `-serial file:vm_log.txt` and the executable directory is writable. |
| Paste (Ctrl+V) does nothing | Clipboard not opened correctly. | Verify Windows clipboard contains Unicode text and that the `OpenClipboard`/`GetClipboardData` calls succeed (they now return silently on failure). |
| Application exits without shutting down VM | Exit path called `StopDockerVM()` without `wait=true`. | Updated Exit menu to call `StopDockerVM(true)` – already applied. |
| UI freezes after clicking *Restart VM* | `StopDockerVM` was asynchronous. | `StopDockerVM(true)` blocks until poweroff completes before starting VM again. |

---

## Extending the Application
1. **Add new UI pages** – Create a new view class in `src/ui/`, expose a handler in `Window::HandleClick`, and render it in `Window::Render`.
2. **New Docker actions** – Extend `DockerClient` with the corresponding HTTP endpoint (e.g., `network prune`). Add UI triggers similar to existing actions.
3. **Theme customization** – Colors are defined in `Window::Render` via `D2D1::ColorF`. Expose a settings dialog to modify them.
4. **Persist user preferences** – Store UI state (last selected sidebar item, window size) in a JSON file under `AppData` and load on startup.

---

## Useful Source Links
- [CMakeLists.txt](file:///C:/Users/YohanesOktanio/Documents/VM/app/CMakeLists.txt)
- [main.cpp](file:///C:/Users/YohanesOktanio/Documents/VM/app/src/main.cpp)
- [Window.h](file:///C:/Users/YohanesOktanio/Documents/VM/app/src/app/Window.h)
- [Window.cpp](file:///C:/Users/YohanesOktanio/Documents/VM/app/src/app/Window.cpp)
- [DockerClient.h](file:///C:/Users/YohanesOktanio/Documents/VM/app/src/docker/DockerClient.h)
- [DockerClient.cpp](file:///C:/Users/YohanesOktanio/Documents/VM/app/src/docker/DockerClient.cpp)
- [ImagesView.h](file:///C:/Users/YohanesOktanio/Documents/VM/app/src/ui/ImagesView.h)
- [ImagesView.cpp](file:///C:/Users/YohanesOktanio/Documents/VM/app/src/ui/ImagesView.cpp)
- [Sidebar.h](file:///C:/Users/YohanesOktanio/Documents/VM/app/src/ui/Sidebar.h)
- [Sidebar.cpp](file:///C:/Users/YohanesOktanio/Documents/VM/app/src/ui/Sidebar.cpp)
- [scripts/start_vm.bat](file:///C:/Users/YohanesOktanio/Documents/VM/app/resources/scripts/start_vm.bat)
- [scripts/poweroff_vm.bat](file:///C:/Users/YohanesOktanio/Documents/VM/app/resources/scripts/poweroff_vm.bat)

---

*End of documentation.*
