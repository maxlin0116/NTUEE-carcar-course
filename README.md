# NTUEE CarCar Course

2026 Spring project repository for the line-following car, Bluetooth bridge, and Python control tools.

## Overview

This repo has three main parts:

- `src/` and `lib/`: Arduino Mega firmware for the car
- `esp32-hm10/`: ESP32 BLE bridge firmware
- `server/`: Python tools for routing, Bluetooth control, and UID submission

The most useful entry point for daily testing is:

```powershell
python server\main.py --help
```

## Repository Layout

```text
NTUEE-carcar-course/
├─ src/                 Arduino Mega main firmware
├─ lib/                 Car control, tracking, RFID, Bluetooth helpers
├─ server/              Python CLI for routing and testing
├─ esp32-hm10/          ESP32 BLE bridge firmware
├─ init_hm10/           HM-10 initialization sketch
├─ README.bluetooth.md  BLE bridge background and setup notes
└─ platformio.ini       PlatformIO project for Arduino Mega
```

## Hardware Setup

Before using the Python commands below, make sure:

1. The Arduino Mega car firmware is flashed.
2. The ESP32 BLE bridge is flashed and connected to your PC by USB.
3. The HM-10 is powered on and paired with the ESP32 bridge target name.
4. The car firmware sends `EVENT:NODE` when the car reaches a node.

If you need HM-10 and ESP32 setup details, see `README.bluetooth.md`.

## Software Setup

### 1. Python dependencies

From the repo root:

```powershell
pip install -r server\requirements.txt
```

### 2. Arduino Mega firmware

Build and upload with PlatformIO:

```powershell
pio run
pio run --target upload
```

After opening the serial monitor at `115200`, you can test the RFID reader directly:
`i` prints MFRC522 status, `u` tries one UID read, `r` reinitializes the reader, `?` shows help.

### 3. Check the Python CLI

```powershell
python server\main.py --help
```

## Quick Start

### Dry-run a route without moving the car

This checks the maze file and route generation only.

```powershell
python server\main.py bfs --from-node 1 --to-node 10 --start-dir south
```

Expected output is similar to:

```text
Path: 1 -> 2 -> 3 -> 9 -> 10
Actions: ffrl
Car commands: FFRL
```

### Manually control the car over Bluetooth

```powershell
python server\main.py manual --bt-port COM11 --expected-bt-name HM10_G10
```

During manual driving, any `UID:XXXXXXXX` event from the car is printed in the terminal and
forwarded to the scoreboard. Add `--fake-scoreboard` if you want to test without the live server.

Commands inside manual mode:

- `G`: run
- `H`: halt
- `F`: go forward through the next node
- `L`: turn left at the next node
- `R`: turn right at the next node
- `B`: u-turn at the next node
- `S`: stop at the next node
- `quit`: leave manual mode

### Drive the car from node A to node B

This is the main point-to-point auto-drive command:

```powershell
python server\main.py go --from-node A --to-node B --start-dir south --bt-port COM11 --expected-bt-name HM10_G10
```

Example:

```powershell
python server\main.py go --from-node 1 --to-node 10 --start-dir south --bt-port COM11 --expected-bt-name HM10_G10
```

What happens:

1. Python connects to the ESP32 bridge.
2. Python computes the BFS shortest path from node `A` to node `B`.
3. Python prints the path and the generated car commands.
4. Python starts a scoreboard session and forwards any `UID:XXXXXXXX` messages seen during the drive.
5. The script asks for `Start command>`.
6. Type `G` and press Enter to begin.
7. The script sends one node command at a time as the car reports `EVENT:NODE`.

For local testing without the live server, add `--fake-scoreboard`.

### Drive a full BFS traversal of the map

This mode visits reachable nodes in BFS order and stitches together a drivable path.
When `--drive-bt` is enabled, the script also starts a scoreboard session and forwards any
`UID:XXXXXXXX` events reported by the car during traversal.

```powershell
python server\main.py map --start-node 1 --start-dir south --drive-bt --bt-port COM11 --expected-bt-name HM10_G10
```

If you only want to preview the plan without moving the car, remove `--drive-bt`.
For local testing without the live server, add `--fake-scoreboard`.

## How Auto-Drive Works

In A-to-B route mode:

1. Python sends `G` to start the car.
2. Python sends the first node command, such as `F`, `L`, `R`, or `B`.
3. The car follows the line until it reaches a node.
4. The car firmware reports `EVENT:NODE`.
5. Python sends the next node command.
6. After the last step, Python sends `H`.

Important: if the firmware does not send `EVENT:NODE`, the Python controller will send only the first node command and then wait forever.

## Common Commands

### Self-test mode

```powershell
python server\main.py self-test
```

### UID manual input mode

```powershell
python server\main.py uid --fake-scoreboard
```

### Submit one UID directly

```powershell
python server\main.py uid --uid 10BA617E --fake-scoreboard
```

### Listen for UID from Bluetooth and forward to the scoreboard

```powershell
python server\main.py uid --listen-bt --bt-port COM11 --expected-bt-name HM10_G10
```

## Command Reference

Main CLI file:

```text
server/main.py
```

Useful mode names:

- `manual`
- `bfs`
- `go`
- `map`
- `uid`
- `self-test`

Useful arguments:

- `--from-node` or `--start-node`
- `--to-node` or `--goal-node`
- `--start-dir north|south|west|east`
- `--bt-port COM11`
- `--expected-bt-name HM10_G10`
- `--drive-bt`

## Troubleshooting

### The dry run works, but the real car does not move

Check:

- The correct serial port is used, such as `COM11`
- The ESP32 bridge is connected and powered
- The HM-10 target name matches `--expected-bt-name`
- No other program is using the serial port

### The car moves once, then stops forever

This usually means the Python controller is waiting for `EVENT:NODE`.

Check:

- The car firmware is running the current code in `src/` and `lib/`
- Bluetooth communication is working
- `EVENT:NODE` is actually printed from the car side

### The path command looks wrong

Check:

- The `maze.csv` contents
- The `--start-dir` value
- The start node and goal node values

### `COM11` is wrong on your computer

Use the actual serial port of your ESP32 bridge instead.

## Notes

- Root-level commands are supported. The default maze path already points to `server/maze.csv`.
- `go` is the easiest command for real point-to-point driving.
- `bfs` is the safest command for route previewing.
- `manual` is the best first Bluetooth test if auto-drive is not working yet.

## Related Files

- `server/main.py`: Python CLI entry point
- `server/maze.py`: BFS path generation
- `src/main.cpp`: main Arduino loop
- `lib/control.h`: node-event and drive logic
- `README.bluetooth.md`: BLE bridge guide
