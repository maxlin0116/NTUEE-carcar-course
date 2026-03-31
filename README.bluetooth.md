# CarCarImprove Wireless Communication System

A complete wireless communication system using HM-10 BLE modules with ESP32 bridge capabilities for seamless PC-to-BLE data transfer.

## Overview

This project provides a full-stack solution for wireless communication between a PC and HM-10 Bluetooth Low Energy modules via an ESP32 bridge. The system consists of firmware, Python libraries, and configuration tools that work together to enable transparent bidirectional communication.

### System Architecture

```
┌──────────┐     USB/UART       ┌──────────┐       BLE        ┌──────────┐
│    PC    │ ◄────────────────► │  ESP32   │ ◄──────────────► │  HM-10   │
│ (Python) │                    │ (Bridge) │                  │ (Module) │
└──────────┘                    └──────────┘                  └──────────┘
```

**Communication Flow:**
1. PC sends data via USB serial to ESP32
2. ESP32 acts as BLE GATT client and forwards data to HM-10
3. HM-10 receives data via BLE characteristic writes
4. HM-10 sends data back via BLE notifications
5. ESP32 receives notifications and forwards to PC via USB serial

## Project Components

### 1. `esp32-hm10/` - ESP32 Firmware

**Purpose:** BLE GATT Client firmware that turns the ESP32 into a transparent UART-to-BLE bridge.

**Key Features:**
- Automatic BLE scanning and connection to configured HM-10 devices
- UART interface at 115200 baud for PC communication
- Bidirectional data transfer (UART ↔ BLE)
- AT command support for runtime configuration
- Persistent device name storage (NVS)
- Automatic reconnection on disconnect

**Technology:**
- Platform: ESP-IDF (Espressif IoT Development Framework)
- Language: C
- BLE Stack: Bluedroid

**Documentation:** See [esp32-hm10/README.md](esp32-hm10/README.md) for detailed build, flash, and usage instructions.

**Quick Start:**
```bash
cd esp32-hm10
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

---

### 2. `chat_hm10/hm10_esp32/` - Python Bridge Module

**Purpose:** High-level Python interface for communicating with the ESP32 bridge, abstracting away low-level serial communication and log filtering.

**Key Features:**
- AT command parsing and handling
- Automatic log filtering (strips ESP32 system logs and ANSI codes)
- Data packet concatenation (merges fragmented HM-10 packets)
- Status checking (connection state queries)
- Device name configuration (NVS interface)

**API Highlights:**
```python
from hm10_esp32 import ESP32HM10Bridge

bridge = ESP32HM10Bridge('/dev/ttyUSB2')
bridge.get_status()              # Check connection state
bridge.get_hm10_name()           # Query target device name
bridge.set_hm10_name('MyDevice') # Change target device
bridge.send('Hello HM-10!')      # Send data to HM-10
message = bridge.listen()        # Receive data from HM-10
```

**Documentation:** See [chat_hm10/hm10_esp32/README.md](chat_hm10/hm10_esp32/README.md)

**Installation:**
```bash
pip install pyserial
```

---

### 3. `chat_hm10/chat_hm10-esp32.py` - Example Chat Application

**Purpose:** Working example demonstrating real-time bidirectional communication using the Python bridge module.

**Features:**
- Threaded architecture for concurrent send/receive
- Connection verification before starting
- Live chat interface with clear message formatting
- Graceful exit handling

**Usage:**
```bash
cd chat_hm10
python chat_hm10-esp32.py
```

**Requirements:**
- ESP32 flashed with `esp32-hm10` firmware
- HM-10 powered on and within range
- ESP32 connected via USB (e.g., `/dev/ttyUSB2`)

**Example Output:**
```
✅ Ready! Connected to HM10_Mega
--- Start Chatting (Type 'exit' to quit) ---
You: Hello from PC!
[HM10]: Hello from Arduino!
You: How are you?
[HM10]: Working great!
```

---

### 4. `init_hm10/` - HM-10 Configuration Utility

**Purpose:** Arduino sketch for initializing and configuring HM-10 modules to work with the ESP32 bridge.

**Features:**
- Automatic baud rate detection (tests 9600-230400 baud)
- Force disconnection from existing connections
- Factory reset capability
- Custom device name configuration
- Connection notification setup
- MAC address query

**Configuration Steps:**
1. Auto-detect HM-10 baud rate
2. Force disconnect any active connections
3. Restore factory defaults
4. Set custom device name (configurable via macro)
5. Enable connection notifications
6. Reset module to apply changes
7. Query and display Bluetooth MAC address

**Usage:**
1. Connect HM-10 to Arduino Mega Serial3 (3.3V logic level!)
2. Edit `CUSTOM_NAME` in the sketch:
   ```arduino
   #define CUSTOM_NAME "HM10_Mega"  // Max 12 characters
   ```
3. Upload sketch and open Serial Monitor (115200 baud)
4. Initialization will run automatically on startup
5. Use Serial Monitor for manual AT commands if needed

**Hardware Connections (Arduino Mega):**
- HM-10 TX → Arduino RX3 (Pin 15)
- HM-10 RX → Arduino TX3 (Pin 14)
- HM-10 VCC → 3.3V (NOT 5V!)
- HM-10 GND → GND

---

## Getting Started

### Prerequisites

**Hardware:**
- ESP32 development board with USB
- HM-10 BLE module (properly configured)
- Arduino Mega or compatible (for HM-10 initialization)
- USB cables for programming and communication

**Software:**
- [ESP-IDF v5.0+](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/)
- [Arduino IDE](https://www.arduino.cc/en/software) (for HM-10 setup)
- Python 3.7+ with `pyserial`

### Setup Workflow

#### Step 1: Initialize HM-10
```bash
# 1. Open init_hm10/init_hm10.ino in Arduino IDE
# 2. Set your desired device name in CUSTOM_NAME
# 3. Upload to Arduino Mega with HM-10 connected to Serial3
# 4. Open Serial Monitor to verify initialization
```

#### Step 2: Flash ESP32
```bash
cd esp32-hm10
idf.py build
idf.py -p /dev/ttyUSB0 flash

# Configure target device name (must match HM-10 name)
idf.py -p /dev/ttyUSB0 monitor
# In monitor, type: AT+NAMEHM10_Mega
# Verify: AT+NAME?
```

#### Step 3: Test Communication
```bash
# Install Python dependencies
pip install pyserial

# Run chat application
cd chat_hm10
python chat_hm10-esp32.py
```

---

## Configuration

### Changing Target HM-10 Device

**Method 1 - Runtime (via AT commands):**
```bash
# Connect to ESP32 serial port
screen /dev/ttyUSB0 115200

# Change target device
AT+NAMEMyDevice

# Verify
AT+NAME?

# Reset to apply
AT+RESET
```

**Method 2 - Python API:**
```python
from hm10_esp32 import ESP32HM10Bridge

bridge = ESP32HM10Bridge('/dev/ttyUSB2')
bridge.set_hm10_name('MyDevice')
# Reset ESP32 manually after this
```

**Method 3 - Firmware default:**
Edit `esp32-hm10/main/main.c`:
```c
static char target_device_name[MAX_DEVICE_NAME_LEN] = "MyDevice";
```

### Serial Port Configuration

All components use 115200 baud by default:
- **ESP32 UART:** 115200 baud (USB Virtual COM Port)
- **Arduino Serial Monitor:** 115200 baud
- **Python Bridge:** 115200 baud (configurable)

---

## Troubleshooting

### ESP32 Can't Find HM-10

**Symptoms:** ESP32 continuously scans but never connects

**Solutions:**
1. Verify HM-10 is powered on and advertising
2. Check device name matches: `AT+NAME?` on ESP32
3. Use nRF Connect app to verify HM-10 visibility
4. Ensure HM-10 is not connected to another device
5. Check HM-10 is within BLE range (< 10 meters)

### Data Not Received

**Symptoms:** Sent data doesn't appear on the other side

**Solutions:**
1. Check connection status: `AT+STATUS?`
2. Verify both devices are connected
3. Check for transmission errors in logs
4. Ensure data doesn't exceed MTU limits
5. Filter logs: Look for `bt_com:` prefix

### HM-10 Initialization Fails

**Symptoms:** Arduino can't detect HM-10

**Solutions:**
1. Verify 3.3V power supply (NOT 5V!)
2. Check TX/RX connections (not swapped)
3. Ensure Serial3 is available on your Arduino
4. Try different baud rates manually
5. Check HM-10 has stable power

### Python Script Connection Timeout

**Symptoms:** Script exits with "Status is DISCONNECTED"

**Solutions:**
1. Wait for ESP32 to complete connection (check monitor)
2. Verify correct serial port in script
3. Check ESP32 is flashed and running
4. Ensure no other programs are using the serial port

---

## BLE Technical Details

### Service and Characteristic UUIDs

- **Service UUID:** `0xFFE0` (HM-10 default UART service)
- **Characteristic UUID:** `0xFFE1` (HM-10 UART characteristic)
- **Properties:** Read, Write, Notify

### MTU and Data Limitations

- **HM-10 MTU:** 23 bytes (20 bytes usable payload)
- **ESP32 UART Buffer:** 1024 bytes
- **Fragmentation:** Messages > 20 bytes are split by HM-10
- **Python Module:** Automatically concatenates fragments

### Connection Parameters

- **Scan Interval:** 80 ms (0x50)
- **Scan Window:** 48 ms (0x30)
- **Connection Interval:** Negotiated by BLE stack
- **MTU Request:** 500 bytes (negotiated down to HM-10 limit)

---

## Development

### Repository Structure

```
CarCarImprove/
├── README.md                      # This file
├── environment.yml                # Conda environment (if applicable)
├── esp32-hm10/                    # ESP32 firmware
│   ├── main/
│   │   └── main.c                 # Main application
│   ├── CMakeLists.txt
│   └── README.md
├── chat_hm10/                     # Python interface
│   ├── hm10_esp32/               # Python module
│   │   ├── __init__.py
│   │   ├── hm10_esp32_bridge.py
│   │   └── README.md
│   └── chat_hm10-esp32.py        # Example script
├── init_hm10/                     # HM-10 setup utility
│   └── init_hm10.ino             # Arduino sketch
└── resource/                      # Resources/documentation
```

### Building from Source

**ESP32 Firmware:**
```bash
cd esp32-hm10
idf.py set-target esp32
idf.py build
```

**Python Module:**
```bash
cd chat_hm10
# Module is ready to use without building
# Just ensure pyserial is installed
pip install pyserial
```

---

## Use Cases

### 1. Wireless Serial Communication
Replace wired serial connections with BLE for wireless debugging, logging, or control.

### 2. IoT Data Collection
Collect sensor data from Arduino/HM-10 devices wirelessly on your PC.

### 3. Remote Control
Send commands from PC to embedded devices via BLE.

### 4. Multi-Device Bridge
Connect multiple HM-10 devices by configuring multiple ESP32 bridges with different target names.

---

## Related Documentation

- [ESP32-HM10 Firmware README](esp32-hm10/README.md) - Detailed ESP32 documentation
- [ESP32-HM10 Implementation Notes](esp32-hm10/IMPLEMENTATION_NOTES.md) - Technical details
- [HM10_ESP32 Python Module README](chat_hm10/hm10_esp32/README.md) - Python API reference
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
- [HM-10 Datasheet](http://www.jnhuamao.cn/bluetooth.asp) - Official HM-10 documentation

---

## License

This project is provided as-is for educational and development purposes.

## Contributing

Contributions are welcome! Please ensure:
- Code follows existing style conventions
- Documentation is updated for any new features
- Testing is performed on actual hardware

---

**CarCarImprove Wireless** - Complete BLE communication solution for ESP32 and HM-10 modules
