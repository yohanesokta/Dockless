#Requires -RunAsAdministrator

$ErrorActionPreference = "Stop"

# ============================================================
# QEMU TAP Network Uninstaller
# ============================================================

$NatName     = "QEMU-NAT"
$DriverDir   = Join-Path $PSScriptRoot "driver"
$DevCon      = Join-Path $DriverDir "devcon.exe"

# ============================================================
# Helpers
# ============================================================

function Write-Step {
    param([string]$Message)
    Write-Host ""
    Write-Host $Message -ForegroundColor Yellow
}

function Write-OK {
    param([string]$Message)
    Write-Host "[OK] $Message" -ForegroundColor Green
}

function Write-Fail {
    param([string]$Message)
    Write-Host "[ERROR] $Message" -ForegroundColor Red
}

function Fail {
    param([string]$Message)
    Write-Fail $Message
    exit 1
}

# ============================================================
# Header
# ============================================================

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host " QEMU TAP Network Uninstaller" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# ============================================================
# 1. Administrator Check
# ============================================================

$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)

if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Fail "Run this uninstaller as Administrator."
}

Write-OK "Administrator privileges"

# ============================================================
# 2. Remove Windows NAT
# ============================================================

Write-Step "[1/3] Removing Windows NAT ($NatName)..."

$nat = Get-NetNat -Name $NatName -ErrorAction SilentlyContinue

if ($nat) {
    Remove-NetNat -Name $NatName -Confirm:$false -ErrorAction Stop
    Write-OK "NAT '$NatName' removed"
} else {
    Write-OK "NAT '$NatName' not found (already removed)"
}

# ============================================================
# 3. Remove TAP Adapter using devcon
# ============================================================

Write-Step "[2/3] Removing TAP Adapter Device..."

if (Test-Path $DevCon) {
    Write-Host "Running DevCon to remove tap0901 devices..."
    & $DevCon remove tap0901
    Write-OK "DevCon remove command executed"
} else {
    Write-Fail "devcon.exe not found at $DevCon."
    Write-Host "Please remove the adapter manually from Device Manager." -ForegroundColor Yellow
}

# Verify if adapter is gone
Start-Sleep -Seconds 2
$tap = Get-NetAdapter -ErrorAction SilentlyContinue | Where-Object { $_.InterfaceDescription -like "*TAP-Windows Adapter V9*" }

if ($tap) {
    Write-Fail "TAP adapter still exists in system. You may need to remove it manually from Device Manager."
} else {
    Write-OK "TAP adapter verified removed"
}

# ============================================================
# 4. Final Output
# ============================================================

Write-Step "[3/3] Uninstallation complete"

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host " Uninstallation successful!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
