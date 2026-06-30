# myDesk - Open-Source Cross-Platform Remote Desktop

A cross-platform remote desktop tool built from scratch, inspired by ToDesk / TeamViewer / RustDesk.
Features P2P direct connection, bidirectional control, and multi-platform support.

Tech stack: **C++17 + CMake + Qt5**, video codec via FFmpeg, NAT traversal via WebRTC (libdatachannel).

## Features

- **Bidirectional control** - Same app acts as both host (be controlled) and viewer (control others)
- **Multi-platform** - Windows (GDI), Linux (X11), macOS (CoreGraphics)
- **Device ID + Password** - ToDesk-like pairing model with 9-digit ID and temp password
- **LAN Direct** - TCP direct connection on local network
- **P2P NAT Traversal** - WebRTC/ICE/STUN via libdatachannel for public internet
- **H.264 Streaming** - Low latency video with libx264 zerolatency tuning
- **Input Injection** - Full mouse + keyboard remote control
- **Authentication** - Password verification before allowing control

## Quick Start

### Build (Windows)

Requires: Visual Studio 2022 + vcpkg (with ffmpeg, qt5, sdl2 installed)

```powershell
.\dev.ps1 build        # Configure + build all
.\dev.ps1 run mydesk   # Build + launch the unified client
```

### Build (Linux)

```bash
sudo apt install cmake g++ libx11-dev libxtst-dev libavcodec-dev libavutil-dev libswscale-dev libsdl2-dev qtbase5-dev
cmake -S . -B build
cmake --build build -j
./build/bin/mydesk
```

### Build (macOS)

```bash
brew install cmake ffmpeg qt@5 sdl2
cmake -S . -B build -DCMAKE_PREFIX_PATH=$(brew --prefix qt@5)
cmake --build build -j
./build/bin/mydesk
```

## Applications

| App | Description |
|-----|-------------|
| **mydesk** | Unified client (ToDesk-like GUI, bidirectional control) |
| host | CLI host (be controlled via LAN or P2P) |
| viewer | CLI viewer (control remote via LAN or P2P, SDL2 display) |
| gui_viewer | Qt5 viewer-only GUI |
| signal_server | Signaling relay for P2P pairing |
| capture_demo | Screen capture test |
| codec_demo | Encode/decode pipeline test |

## Usage: mydesk (Unified Client)

Launch `mydesk` on both machines:

1. **Machine A** (to be controlled): Note the Device ID and Temporary Password shown on the left panel
2. **Machine B** (controller): Enter Machine A's IP (LAN mode) or Device ID (P2P mode) + password on the right panel, click Connect

Both machines run the same program - each is simultaneously a potential host and viewer.

## Usage: CLI Mode

### LAN (direct TCP)

```bash
# On machine to be controlled:
./host 9000

# On controlling machine:
./viewer <host-ip> 9000
```

### Public P2P

```bash
# Deploy signal server on a VPS:
./signal_server 7000

# Host registers:
./host --p2p <signal-server-ip> 7000 ROOM1

# Viewer connects:
./viewer --p2p <signal-server-ip> 7000 ROOM1
```

## Architecture

```
myDesk/
├── src/
│   ├── core/                    # Cross-platform core library
│   │   ├── capture/             # Screen capture (X11/GDI/CoreGraphics)
│   │   ├── codec/              # H.264 encode/decode (FFmpeg)
│   │   ├── input/              # Input injection (XTest/SendInput/CGEvent)
│   │   ├── net/                # TCP + message framing
│   │   ├── p2p/                # WebRTC transport (libdatachannel)
│   │   └── signal/             # Signal client
│   └── apps/
│       ├── mydesk/             # ★ Unified client (Qt5, bidirectional)
│       ├── host/               # CLI host
│       ├── viewer/             # CLI viewer (SDL2)
│       ├── gui_viewer/         # Qt5 viewer-only
│       └── signal_server/      # Signaling server
└── CMakeLists.txt
```

## Protocol

Wire format: `[type: 1 byte][length: 4 bytes BE][payload]`

Message types:
- Video (1): H.264 Annex-B bitstream
- Input (2): Mouse/keyboard events (13 bytes fixed format)
- Auth (20/21/22): Password authentication handshake
- Signal (10-14): P2P pairing and relay

## TODO

- [ ] TURN relay fallback for symmetric NAT
- [ ] TLS on signaling + device authentication
- [ ] File transfer
- [ ] Clipboard sync
- [ ] Multi-monitor support
- [ ] DXGI Desktop Duplication (Windows high-perf capture)
- [ ] PipeWire capture (Wayland)
- [ ] Android/iOS viewer app
