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
- **Web Remote Desktop** - HTML5 + WebSocket based viewer (browser access)
- **Account System** - Login with username/password, Developer/User modes
- **Developer Mode** - Detailed log panel with one-click copy for debugging
- **User Mode** - Simplified interface for end users
- **File Transfer** - Drag & drop files, bidirectional transfer, save to chosen directory
- **Voice Call** - Audio communication with mute/unmute (can work with or without video)
- **Device List** - Save frequently connected devices for one-click reconnect

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
| **mydesk** | Unified client (ToDesk-like GUI, bidirectional control, web service) |
| host | CLI host (be controlled via LAN or P2P) |
| viewer | CLI viewer (control remote via LAN or P2P, SDL2 display) |
| gui_viewer | Qt5 viewer-only GUI |
| signal_server | Signaling relay for P2P pairing |
| capture_demo | Screen capture test |
| codec_demo | Encode/decode pipeline test |

## Usage: mydesk (Unified Client)

Launch `mydesk` on both machines:

1. **Login** - Enter username/password (or skip for dev mode)
2. **Machine A** (to be controlled): Note the Device ID and Temporary Password shown on the left panel
3. **Machine B** (controller): Enter Machine A's IP (LAN mode) or Device ID (P2P mode) + password, click Connect

### Command Line Options

```bash
mydesk                          # GUI mode (default)
mydesk --headless               # Headless service mode (no GUI)
mydesk --headless --port 9000   # Headless on custom port
mydesk --web                    # Enable web server (port 8080)
mydesk --web --web-port 9090    # Web server on custom port
```

### Web Remote Desktop

When web server is enabled (via GUI toggle or `--web` flag), open a browser to:
```
http://<host-ip>:8080
```
Enter the password to start remote control via browser.

### Developer vs User Mode

- **Developer Mode** (default): Full interface with log panel, signal server config, detailed error messages, one-click log copy
- **User Mode**: Simplified interface - just Device ID, password, and connect button

### File Transfer

- **Drag & Drop**: Drag files directly onto the File Transfer panel to send
- **Browse**: Click "Send File" to select files
- **Receive**: When remote sends a file, choose where to save it
- **Bidirectional**: Both sides can send and receive

### Voice Call

- Click "Voice Call" button to start audio communication
- Audio works independently of video - can have voice-only or voice+video
- Mute/unmute supported

### Saved Devices

- Successfully connected devices are automatically saved
- Double-click a saved device for one-click reconnect
- Device list is stored locally in `~/.config/myDesk/devices.json`

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
│   │   ├── signal/             # Signal client
│   │   ├── file_transfer/      # File transfer protocol
│   │   └── audio/              # Audio capture/playback
│   └── apps/
│       ├── mydesk/             # ★ Unified client (Qt5, bidirectional)
│       │   ├── account_manager # Login/logout, mode management
│       │   ├── device_list_manager # Saved devices persistence
│       │   ├── log_panel       # Developer log display
│       │   ├── login_dialog    # Login/register UI
│       │   ├── file_transfer_widget # File transfer UI (drag & drop)
│       │   └── web_server      # HTTP + WebSocket server
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
- File (30-35): File transfer (offer/accept/reject/data/complete/cancel)
- Audio (40-41): Voice data and control

## Account System

The account system currently uses local validation for development. The architecture is ready for server integration:

- **Login API**: `POST /api/login { username, password }` → `{ token, mode }`
- **Register API**: `POST /api/register { username, password, email }`
- **Logout API**: `POST /api/logout { token }`

Set server URL via `AccountManager::setServerUrl()`. The login dialog supports both login and registration, with a "Skip Login" option for development.

## TODO

- [x] Web remote desktop (HTML5 + WebSocket)
- [x] Account login system
- [x] Developer/User mode
- [x] Developer log panel with copy
- [x] File transfer with drag & drop
- [x] Voice call framework
- [x] Device list with one-click connect
- [ ] Server-side account API deployment
- [ ] Opus audio codec integration
- [ ] PortAudio cross-platform audio capture
- [ ] WebCodecs H.264 for web viewer (replace JPEG)
- [ ] TURN relay fallback for symmetric NAT
- [ ] TLS on signaling + device authentication
- [ ] Clipboard sync
- [ ] Multi-monitor support
- [ ] DXGI Desktop Duplication (Windows high-perf capture)
- [ ] PipeWire capture (Wayland)
- [ ] Android/iOS viewer app
