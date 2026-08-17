import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent
VM = ROOT / "vm"

QEMU = "qemu-system-x86_64.exe"

ISO = ROOT / "alpine-standard-3.24.1-x86_64.iso"
DISK = VM / "alpine.qcow2"

cmd = [
    QEMU,
    "-machine", "q35",
    "-cpu", "max",
    "-smp", "1",
    "-m", "256M",
    "-drive", f"file={DISK},if=virtio,format=qcow2",
    "-netdev", "user,id=net0",
    "-device", "e1000,netdev=net0",
    "-cdrom", str(ISO),
    "-boot", "d",
    "-nographic",
    "-serial", "mon:stdio",
    
]

print("Starting Alpine VM...")
print()

subprocess.run(cmd)