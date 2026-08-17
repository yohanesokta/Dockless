#Requires -RunAsAdministrator

$ErrorActionPreference = "Stop"

# ============================================================
# QEMU TAP Network Installer
# ============================================================

$TapIP       = "192.168.100.1"
$TapPrefix   = 24
$Network     = "192.168.100.0/24"
$NatName     = "QEMU-NAT"

$DriverDir   = Join-Path $PSScriptRoot "driver"
$DevCon      = Join-Path $DriverDir "devcon.exe"
$Inf         = Join-Path $DriverDir "OemVista.inf"

# ============================================================
# Helpers
# ============================================================

function Write-Step {
    param(
        [string]$Message
    )

    Write-Host ""
    Write-Host $Message -ForegroundColor Yellow
}

function Write-OK {
    param(
        [string]$Message
    )

    Write-Host "[OK] $Message" -ForegroundColor Green
}

function Write-Fail {
    param(
        [string]$Message
    )

    Write-Host "[ERROR] $Message" -ForegroundColor Red
}

function Fail {
    param(
        [string]$Message
    )

    Write-Fail $Message
    exit 1
}

# ============================================================
# Header
# ============================================================

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host " QEMU TAP Network Installer" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# ============================================================
# 1. Administrator
# ============================================================

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()

$principal = New-Object Security.Principal.WindowsPrincipal($identity)

if (-not $principal.IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator
)) {
    Fail "Run this installer as Administrator."
}

Write-OK "Administrator privileges"

# ============================================================
# 2. Validate driver files
# ============================================================

Write-Step "[1/8] Checking TAP driver package..."

if (-not (Test-Path $DriverDir)) {
    Fail "Driver directory not found: $DriverDir"
}

if (-not (Test-Path $DevCon)) {
    Fail "devcon.exe not found: $DevCon"
}

if (-not (Test-Path $Inf)) {
    Fail "OemVista.inf not found: $Inf"
}

Write-OK "Driver directory found"
Write-OK "DevCon found"
Write-OK "TAP INF found"

# ============================================================
# 3. Detect existing TAP adapter
# ============================================================

Write-Step "[2/8] Checking existing TAP adapter..."

$tap = Get-NetAdapter -ErrorAction SilentlyContinue |
    Where-Object {
        $_.InterfaceDescription -like "*TAP-Windows Adapter V9*"
    } |
    Select-Object -First 1

if ($tap) {

    Write-Host "Existing TAP adapter detected:"
    Write-Host "  Name : $($tap.Name)"
    Write-Host "  Index: $($tap.ifIndex)"

    $TapName = $tap.Name

}
else {

    Write-Host "No existing TAP adapter found."
    Write-Host "Creating TAP device..."
    
    # ========================================================
    # 4. Install TAP device using DevCon
    # ========================================================

    Write-Step "[3/8] Installing TAP device with DevCon..."

    & $DevCon install $Inf tap0901

    if ($LASTEXITCODE -ne 0) {
        Fail "DevCon failed to install TAP device."
    }

    Write-OK "DevCon installation completed"

    # ========================================================
    # Wait for adapter
    # ========================================================

    Write-Host "Waiting for TAP adapter..."

    $tap = $null

    for ($i = 0; $i -lt 30; $i++) {

        $tap = Get-NetAdapter -ErrorAction SilentlyContinue |
            Where-Object {
                $_.InterfaceDescription -like "*TAP-Windows Adapter V9*"
            } |
            Select-Object -First 1

        if ($tap) {
            break
        }

        Start-Sleep -Seconds 1
    }

    if (-not $tap) {

        Write-Host ""
        Write-Fail "TAP adapter was not detected."

        Write-Host ""
        Write-Host "DevCon TAP devices:" -ForegroundColor Yellow

        & $DevCon find "*tap*"

        Write-Host ""
        Write-Host "Network adapters:" -ForegroundColor Yellow

        Get-NetAdapter |
            Format-Table ifIndex,Name,InterfaceDescription,Status

        exit 1
    }

    $TapName = $tap.Name

    Write-OK "TAP adapter created"
}

# ============================================================
# Refresh adapter information
# ============================================================

$tap = Get-NetAdapter -Name $TapName -ErrorAction Stop

$TapIndex = $tap.ifIndex

Write-Host ""
Write-Host "TAP adapter:"
Write-Host "  Name : $TapName"
Write-Host "  Index: $TapIndex"

# ============================================================
# 5. Enable TAP
# ============================================================

Write-Step "[4/8] Enabling TAP adapter..."

if ($tap.Status -eq "Disabled") {

    Enable-NetAdapter -Name $TapName -Confirm:$false -ErrorAction SilentlyContinue

    Start-Sleep -Seconds 2
}

$tap = Get-NetAdapter -Name $TapName

if ($tap.Status -eq "Disabled") {
    Fail "Failed to enable TAP adapter."
}

Write-OK "TAP adapter enabled"

# ============================================================
# 6. Configure TAP IP
# ============================================================

Write-Step "[5/8] Configuring TAP IP..."

# Check if our IP already exists.
$existingIP = Get-NetIPAddress -InterfaceIndex $TapIndex -AddressFamily IPv4 -ErrorAction SilentlyContinue |
    Where-Object {
        $_.IPAddress -eq $TapIP
    }

if (-not $existingIP) {

    # Remove only automatic APIPA addresses.
    Get-NetIPAddress -InterfaceIndex $TapIndex -AddressFamily IPv4 -ErrorAction SilentlyContinue |
        Where-Object {
            $_.IPAddress -like "169.254.*"
        } |
        Remove-NetIPAddress -Confirm:$false -ErrorAction SilentlyContinue

    $mask = if ($TapPrefix -eq 24) { "255.255.255.0" } else { "255.255.255.0" }
    netsh interface ip set address name=$TapName static $TapIP $mask | Out-Null
}

Write-OK "TAP IP configured: $TapIP/$TapPrefix"

# ============================================================
# 7. Remove TAP default route
# ============================================================

Write-Host "Removing TAP default route..."

Get-NetRoute -InterfaceIndex $TapIndex -AddressFamily IPv4 -DestinationPrefix "0.0.0.0/0" -ErrorAction SilentlyContinue |
    Remove-NetRoute -Confirm:$false -ErrorAction SilentlyContinue

Write-OK "TAP default route cleared"

# ============================================================
# 8. Configure Windows NAT
# ============================================================

Write-Step "[6/8] Configuring Windows NAT..."

$nat = Get-NetNat -Name $NatName -ErrorAction SilentlyContinue

if (-not $nat) {

    New-NetNat -Name $NatName -InternalIPInterfaceAddressPrefix $Network -ErrorAction Stop | Out-Null

    Write-OK "NAT created: $NatName"
}
else {

    if ($nat.InternalIPInterfaceAddressPrefix -ne $Network) {

        Write-Host "Existing NAT has incorrect configuration."
        Write-Host "Recreating NAT..."

        Remove-NetNat -Name $NatName -Confirm:$false

        New-NetNat -Name $NatName -InternalIPInterfaceAddressPrefix $Network -ErrorAction Stop | Out-Null
    }

    Write-OK "NAT already configured: $NatName"
}

# ============================================================
# 9. Verify configuration
# ============================================================

Write-Step "[7/8] Verifying network configuration..."

$verifyIP = Get-NetIPAddress -InterfaceIndex $TapIndex -AddressFamily IPv4 -ErrorAction SilentlyContinue |
    Where-Object {
        $_.IPAddress -eq $TapIP -and
        $_.PrefixLength -eq $TapPrefix
    }

if (-not $verifyIP) {
    Fail "TAP IP verification failed."
}

Write-OK "TAP IP verified"

$verifyNat = Get-NetNat -Name $NatName -ErrorAction SilentlyContinue

if (-not $verifyNat) {
    Fail "NAT verification failed."
}

if ($verifyNat.InternalIPInterfaceAddressPrefix -ne $Network) {
    Fail "NAT network prefix is incorrect."
}

Write-OK "NAT verified"

# ============================================================
# 10. Final configuration
# ============================================================

Write-Step "[8/8] Installation complete"

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host " QEMU Network Ready" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""

Write-Host "Windows TAP"
Write-Host "  Adapter : $TapName"
Write-Host "  Index   : $TapIndex"
Write-Host "  IP      : $TapIP"
Write-Host "  Network : $Network"

Write-Host ""

Write-Host "QEMU / Alpine"
Write-Host "  IP      : 192.168.100.2"
Write-Host "  Netmask : 255.255.255.0"
Write-Host "  Gateway : 192.168.100.1"
Write-Host "  DNS     : 1.1.1.1"

Write-Host ""
Write-Host "NAT"
Write-Host "  Name    : $NatName"
Write-Host "  Prefix  : $Network"

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host " Installation successful!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""