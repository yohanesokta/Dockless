@echo off
setlocal

REM ==========================================
REM Dockless Docker VM Launcher
REM Usage:
REM   start.bat <RAM> <CPU> <VMDK_SIZE>
REM
REM Example:
REM   start.bat 2G 2 50G
REM ==========================================

set "ROOT=%~dp0..\..\..\..\"
set "VM=%ROOT%kernel"
set "DISK=%ROOT%vm"
set "QEMU_ROOT=%ROOT%bin\qemu"

set "QEMU=%QEMU_ROOT%\qemu-system-x86_64.exe"
set "QEMU_IMG=%QEMU_ROOT%\qemu-img.exe"

REM ==========================================
REM Arguments
REM ==========================================

set "RAM=%~1"
set "CPU=%~2"
set "VMDK_SIZE=%~3"

REM Defaults
if "%RAM%"=="" set "RAM=512M"
if "%CPU%"=="" set "CPU=1"
if "%VMDK_SIZE%"=="" set "VMDK_SIZE=50G"

if /I "%CPU%"=="max" set "CPU=%NUMBER_OF_PROCESSORS%"

echo.
echo ==========================================
echo          Dockless Docker VM
echo ==========================================
echo RAM       : %RAM%
echo CPU       : %CPU%
echo Disk size : %VMDK_SIZE%
echo ==========================================
echo.

REM ==========================================
REM Validate QEMU
REM ==========================================

if not exist "%QEMU%" (
    echo ERROR: QEMU not found:
    echo %QEMU%
    exit /b 1
)

if not exist "%QEMU_IMG%" (
    echo ERROR: qemu-img not found:
    echo %QEMU_IMG%
    exit /b 1
)

REM ==========================================
REM Create VM directory
REM ==========================================

if not exist "%DISK%" (
    mkdir "%DISK%"
)

REM ==========================================
REM Docker disk
REM ==========================================

set "DOCKER_DISK=%DISK%\docker.vmdk"

if not exist "%DOCKER_DISK%" (
    echo Creating dynamic Docker disk...
    echo Size: %VMDK_SIZE%
    echo.

    "%QEMU_IMG%" create ^
        -f vmdk ^
        "%DOCKER_DISK%" ^
        "%VMDK_SIZE%"

    if errorlevel 1 (
        echo ERROR: Failed to create Docker disk.
        exit /b 1
    )

    echo Docker disk created successfully.
    echo.
)

REM ==========================================
REM Start QEMU
REM ==========================================

echo Starting Docker VM...
echo.

"%QEMU%" ^
    -L "%QEMU_ROOT%" ^
    -machine q35 ^
    -accel whpx ^
    -cpu qemu64,+cx16,+popcnt,+ssse3,+sse4.1,+sse4.2 ^
    -smp %CPU% ^
    -m %RAM% ^
    -kernel "%VM%\vmlinuz-virt" ^
    -initrd "%VM%\initramfs-virt" ^
    -append "root=/dev/vda rw rootfstype=ext4 console=ttyS0" ^
    -drive "file=%DISK%\alpine.qcow2,if=virtio,format=qcow2" ^
    -drive "file=%DOCKER_DISK%,if=virtio,format=vmdk" ^
    -netdev "tap,id=net0,ifname=Local Area Connection" ^
    -device "virtio-net-pci,netdev=net0" ^
    -display none ^
    -serial file:vm_log.txt

set "EXIT_CODE=%ERRORLEVEL%"

echo.
echo Docker VM stopped.
echo Exit code: %EXIT_CODE%

exit /b %EXIT_CODE%