import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent
VM = ROOT / "kernel"
DISK = ROOT / "vm"
QEMU_ROOT = ROOT / "bin" / "qemu"
QEMU = str(QEMU_ROOT / "qemu-system-x86_64.exe")

cmd = [
    QEMU,
    "-L", str(QEMU_ROOT),

    "-machine", "q35",
    "-cpu", "max",

    "-smp", "1",
    "-m", "512M",

    "-kernel", str(VM / "vmlinuz-virt"),
    "-initrd", str(VM / "initramfs-virt"),

    "-append", "root=/dev/vda rw rootfstype=ext4 console=ttyS0",

    "-drive", f"file={DISK / 'alpine.qcow2'},if=virtio,format=qcow2",

    # Network
    #"-netdev", "user,id=net0,hostfwd=tcp:127.0.0.1:9000-10.0.2.15:9000,",
    #"-device", "virtio-net-pci,netdev=net0",
    "-netdev", "tap,id=net0,ifname=Local Area Connection",
    "-device", "virtio-net-pci,netdev=net0",
    # Terminal
    # "-serial", "stdio",
    # "-display", "none",
     "-nographic"
]

print("Starting Docker VM...")
subprocess.run(cmd)