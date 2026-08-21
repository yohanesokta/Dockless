# Dockless

> **A lightweight Docker Desktop alternative for Windows.**

Dockless is an experimental, lightweight Docker environment designed to run containers through a minimal virtual machine with as little overhead as possible.

Inspired by [OrbStack](https://orbstack.dev/), Dockless focuses on reducing the unnecessary resource usage commonly associated with traditional container and VM solutions.

## Story

Aku menemukan salah satu postingan di Facebook tentang penggunaan alternatif Docker yaitu [OrbStack](https://orbstack.dev/) yang menarik. Yang membuatku tertarik adalah ide marketing mereka yaitu **"Say goodbye to slow, clunky containers and VMs"** wkwkwk.

> [translate:id]
> I came across a Facebook post about an alternative to Docker called [OrbStack](https://orbstack.dev/), and I found it really interesting. What caught my attention was their marketing idea: **"Say goodbye to slow, clunky containers and VMs."** Haha.

Dockless terinspirasi oleh project [OrbStack](https://orbstack.dev/) yang memangkas penggunaan resource berlebih agar penggunaan dapat jauh lebih efisien. Ini dapat meninggalkan kesan lambat di Docker Desktop.

> [translate:id]
> Dockless was inspired by the same idea: **containers shouldn't require excessive resources just to run.**

Dockless aims to achieve this by using a **minimal virtual machine**, keeping its disk image as small as possible while still providing a fully functional Docker environment.

The goal is simple:

**Minimal VM. Minimal overhead. Full Docker experience.**

> [!IMPORTANT]
> Dockless is currently under active development. It is experimental software and should be used for testing and experimentation until an official stable release is available.

## Preview

![Dockless Client Preview](https://github.com/user-attachments/assets/b6128eae-c2ac-4f27-b68b-6491c4483889)

## Features

* [x] Minimal Linux VM
* [x] QEMU-based virtualization
* [x] Docker Engine inside the VM
* [x] Windows Docker client
* [x] Docker Engine API support
* [x] TAP-based networking
* [x] Port forwarding
* [x] VM status monitoring
* [x] VM logs
* [x] Docker container management
* [x] Docker image management
* [x] Docker volume management
* [x] Docker Compose management
* [x] SSH access


## Why Dockless?

Traditional Docker Desktop solutions can introduce additional layers between the host system and Docker Engine.

Dockless takes a different approach:

```text
Windows
   │
   ▼
Minimal QEMU VM
   │
   ▼
Docker Engine
   │
   ├── Containers
   ├── Images
   ├── Volumes
   └── Networks
```

The VM is intentionally kept minimal so that most of the available resources can be dedicated to the workloads running inside Docker.

## Networking

Dockless uses a virtual network between Windows and the Linux VM.

```text
Windows
   │
   │ TAP Adapter
   │
   ▼
192.168.100.1
   │
   │
   ▼
192.168.100.2
Alpine Linux VM
   │
   ▼
Docker Engine
```

This allows the Windows client to communicate directly with the Docker Engine running inside the VM.

## Docker API

Dockless can communicate with the Docker Engine through the Docker Engine API.

For example:

```text
http://192.168.100.2:2375
```

This means the client does not need to manage Docker containers directly. Instead, it can act as a graphical frontend for the Docker Engine.

## VM

The VM is designed around a minimal Linux environment.

Current development focuses on:

* Small disk footprint
* Low memory usage
* Fast boot time
* Minimal userspace
* QEMU virtualization
* Docker Engine compatibility
* Simple networking

The objective is not to create another general-purpose Linux VM, but a **purpose-built environment for running containers**.

## Requirements

### Windows

* Windows 10 / Windows 11
* x86-64 CPU
* QEMU
* TAP-Windows adapter
* Hardware virtualization recommended

### Virtualization

Dockless is designed to work with available Windows virtualization acceleration where supported, including:

* WHPX
* CPU virtualization extensions
* Generic x86-64 QEMU configuration

## Status

Dockless is currently **experimental**.

The architecture, VM image, networking implementation, and client application are still subject to change.

Do not use Dockless for production workloads yet.

## Roadmap

* [ ] Faster VM boot
* [ ] Smaller VM image
* [ ] Lower memory footprint
* [ ] Automatic VM provisioning
* [ ] Automatic Docker installation
* [ ] Better Windows networking
* [ ] Container dashboard
* [ ] Image management
* [ ] Volume management
* [ ] Network management
* [ ] Compose management
* [ ] Integrated terminal
* [ ] Resource monitoring
* [ ] Application auto-update
* [ ] Installer
* [ ] Stable release

## Philosophy

Dockless is built around a simple idea:

> **Docker should feel native without requiring a heavyweight environment.**

Instead of trying to reproduce an entire Linux desktop or development environment, Dockless provides only what is necessary to run Docker workloads.

**Small VM. Fast startup. Low overhead. Docker.**

## Inspiration

Dockless is inspired by projects that focus on making container development faster and more efficient, especially:

* [OrbStack](https://orbstack.dev/)
* Docker Desktop
* QEMU
* Alpine Linux

## Disclaimer

Dockless is an independent project and is **not affiliated with, endorsed by, or sponsored by Docker or OrbStack**.

The project is experimental and provided as-is.

## License

See the [`LICENSE`](LICENSE) file for licensing information.
