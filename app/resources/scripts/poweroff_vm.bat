@echo off
setlocal

set "IP=%~1"
if "%IP%"=="" set "IP=192.168.100.2"


echo Sending poweroff command to %IP%...

ssh ^
    -i .\scripts\sshkeys_vm ^
    -o StrictHostKeyChecking=no ^
    -o UserKnownHostsFile=NUL ^
    -o ConnectTimeout=5 ^
    root@%IP% "poweroff"

if errorlevel 1 (
    echo Failed to power off VM.
    exit /b 1
)

echo Poweroff command sent successfully.

endlocal