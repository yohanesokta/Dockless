import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent
VM = ROOT / "kernel"
DISK = ROOT / "vm"
QEMU_ROOT = ROOT / "bin" / "qemu"
QEMU = str(QEMU_ROOT / "qemu-system-x86_64.exe")

QEMU_IMG = str(QEMU_ROOT / "qemu-img.exe")

DOCKER_DISK = DISK / "docker.vmdk"
if not DOCKER_DISK.exists():
    print("Creating dynamic 50GB Docker disk (this is a one-time operation)...")
    subprocess.run([QEMU_IMG, "create", "-f", "vmdk", str(DOCKER_DISK), "50G"], check=True)

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
    "-drive", f"file={DISK / 'docker.vmdk'},if=virtio,format=vmdk",

    "-netdev", "tap,id=net0,ifname=Local Area Connection",
    "-device", "virtio-net-pci,netdev=net0",

     "-nographic"
]

print("Starting Docker VM...")
subprocess.run(cmd)