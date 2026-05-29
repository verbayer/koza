# Koza

Koza is a minimal Linux container runtime written in C, built from scratch as an educational project to understand how containers work at the kernel level.


## How It Works

Koza uses Linux kernel primitives directly. Each container is isolated using:

- **Namespaces** (`clone(2)`) — PID, mount, UTS, IPC, network, and optionally user namespaces
- **Cgroups v2** — memory, CPU, and PID limits enforced via `/sys/fs/cgroup`
- **Overlay filesystem** — containers run on a copy-on-write layer over a shared image, keeping the base image intact
- **`pivot_root(2)`** — switches the container's root filesystem after mounting the overlay
- **Capabilities** — dropped via `libcap` after setup; rootless mode supported
- **Virtual networking** — a `koza0` bridge is created on startup; each container gets a veth pair connected to it
- **PTY** — interactive containers get a pseudo-terminal via `openpty(3)`

Container state is persisted to `/var/lib/koza/containers/<id>/state.json` using `json-c`.

## Features

- Create, run, stop, and delete containers
- JSON-based container configuration
- Overlay filesystem (shared image, per-container writable layer)
- cgroup v2 resource limits (memory, CPU quota/period, max PIDs)
- Linux capability management (drop all or set specific caps)
- Rootless container support
- Virtual network with NAT via a host bridge
- Interactive mode with PTY support
- Image export/import (`.koza` archive format using libarchive)
- Container state persistence

## Dependencies

**Dependencies:** `libcap`, `json-c`, `libnl3`, `libnl-route-3`, `libarchive`

On Arch Linux:

```sh
sudo pacman -S libcap json-c libnl libarchive
```

## Building

```sh
git clone https://github.com/verbayer/koza.git
cd koza
make
```

The binary is output as `./koza`.

## Usage

Most commands require `root` (for namespace and cgroup setup). Rootless mode (`--rootless`) requires a user namespace-capable kernel.

### Create a container

```sh
sudo ./koza create --name mybox --rootfs ./alpine --cmd /bin/sh
```

With resource limits:

```sh
sudo ./koza create \
  --name mybox \
  --rootfs ./alpine \
  --cmd /bin/sh \
  --memory 104857600 \
  --cpu-quota 50000 \
  --cpu-period 100000 \
  --pids 32
```

With a JSON config file:

```sh
sudo ./koza create -f demo_config.json
```

### Run a container

```sh
sudo ./koza run <id>
```

Interactive shell:

```sh
sudo ./koza run -i <id>
```

### Other commands

```sh
sudo ./koza list              # List all containers
sudo ./koza stop <id>         # Stop a running container
sudo ./koza delete <id>       # Delete a container
sudo ./koza export <id>       # Export container as <id>.koza archive
sudo ./koza import <file.koza> # Import a .koza archive as an image
sudo ./koza config init <name> # Generate a template config JSON
```

## Configuration File Format

```json
{
  "name": "demo",
  "rootfs": "./alpine",
  "command": "/bin/sh",
  "hostname": "koza-demo",
  "rootless": 0,
  "capabilities": "",
  "uid": 0,
  "gid": 0,
  "cgroup": {
    "memory_limit": 104857600,
    "cpu_quota": 50000,
    "cpu_period": 100000,
    "pids_limit": 32
  }
}
```

## Project Structure

```
koza/
├── src/
│   ├── main.c          # Entry point, bridge initialization
│   ├── cli.c           # Command-line interface
│   ├── container.c     # Container lifecycle management
│   ├── namespaces.c    # Linux namespace isolation
│   ├── rootfs.c        # Filesystem setup and pivot_root
│   ├── cgroups.c       # cgroup v2 resource limits
│   ├── caps.c          # Linux capability management
│   ├── network.c       # Virtual network (bridge + veth pairs)
│   ├── state.c         # Container state persistence
│   ├── config.c        # JSON configuration loading
│   ├── pty.c           # Pseudo-terminal support
│   ├── image.c         # Image export and import
│   └── utils.c         # Common helpers
├── include/            # Header files
├── demo_config.json    # Example configuration
└── Makefile
```

## Getting a Root Filesystem

A minimal Alpine Linux rootfs works well for testing:

```sh
mkdir alpine
curl -L https://dl-cdn.alpinelinux.org/alpine/v3.23/releases/x86_64/alpine-minirootfs-3.23.0-x86_64.tar.gz \
  | tar -xz -C alpine
```

## License

MIT
