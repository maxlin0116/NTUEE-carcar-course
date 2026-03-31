# ESP32-HM10 BLE Bridge

A BLE GATT Client implementation for ESP32 that connects to HM-10 Bluetooth modules and provides a UART interface for bidirectional communication.

## Overview

This project enables an ESP32 to act as a BLE bridge between a computer (via USB serial) and an HM-10 BLE module. It automatically discovers and connects to a configured HM-10 device, allowing transparent data transfer between UART and BLE.

### Key Features

- ✅ Automatic BLE scanning and connection to HM-10 modules
- ✅ UART interface at 115200 baud for computer communication
- ✅ Bidirectional data transfer (UART ↔ BLE)
- ✅ AT command support for device configuration
- ✅ Persistent storage of target device name (NVS)
- ✅ Automatic reconnection on disconnect
- ✅ BLE notification handling

## Hardware Requirements

- **ESP32 Development Board** (any ESP32 module with USB serial)
- **HM-10 BLE Module** (configured and powered on)
- **USB Cable** for programming and serial communication

## Software Requirements

- [ESP-IDF v5.0+](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/)
- Python 3.7+ (for ESP-IDF tools)

## Installation

### 1. Set up ESP-IDF

Follow the [ESP-IDF Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) to install the ESP-IDF framework.

### 2. Clone and Navigate to Project

```bash
cd esp32-hm10
```

### 3. Configure the Project (Optional)

```bash
idf.py menuconfig
```

### 4. Build the Project

```bash
idf.py build
```

### 5. Flash to ESP32

Connect your ESP32 board via USB and run:

```bash
idf.py -p /dev/ttyUSB0 flash
```

*(Replace `/dev/ttyUSB0` with your ESP32's serial port)*

### 6. Monitor Output

```bash
idf.py -p /dev/ttyUSB0 monitor
```

To exit the monitor, press `Ctrl+]`.

**Combined flash and monitor:**
```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

## Usage

### Initial Setup

1. **Power on your HM-10 module** - Ensure it's advertising and within range
2. **Flash the ESP32** - The default target device name is `sallen_hm10`
3. **Monitor the serial output** - You'll see scanning and connection logs

### Changing Target Device

If you want to connect to a different HM-10 device:

```
AT+NAME<device_name>
```

**Example:**
```
AT+NAMEmy_hm10
```

**Response:**
```
OK+SETmy_hm10
```

The device name is saved to non-volatile storage and persists across reboots.

### Checking Configuration

**Query current target device name:**
```
AT+NAME?
```

**Response:**
```
OK+NAMEHM10_Mega
```

**Query connection status:**
```
AT+STATUS?
```

**Response:**
- `OK+CONN` - Connected to HM-10
- `OK+UNCONN` - Not connected

### Resetting the ESP32

```
AT+RESET
```

**Response:**
```
OK+RESET
```

The ESP32 will restart after a short delay.

### Sending Data to HM-10

Once connected, any data you send through the serial terminal (except AT commands) will be forwarded to the HM-10 module:

```
Hello from computer!
```

The data is transmitted via BLE Write operations to the HM-10 characteristic.

### Receiving Data from HM-10

Any notifications from the HM-10 module are printed to the serial monitor with the `bt_com` tag:

```
I (12345) bt_com: Hello from HM-10!
```

## Communication Protocol

### BLE Service and Characteristic

- **Service UUID:** `0xFFE0` (HM-10 default service)
- **Characteristic UUID:** `0xFFE1` (HM-10 default characteristic)
- **Characteristic Properties:** Read, Write, Notify

### Data Limitations

- **HM-10 MTU:** 23 bytes (20 bytes usable payload per packet)
- **UART Buffer:** 1024 bytes
- **Messages longer than 20 bytes** are fragmented by the HM-10 and received as multiple notifications

## AT Commands Reference

| Command | Description | Example | Response |
|---------|-------------|---------|----------|
| `AT+NAME<name>` | Set target device name | `AT+NAMEmy_device` | `OK+SETmy_device` |
| `AT+NAME?` | Query target device name | `AT+NAME?` | `OK+NAMEsallen_hm10` |
| `AT+STATUS?` | Query connection status | `AT+STATUS?` | `OK+CONN` or `OK+UNCONN` |
| `AT+RESET` | Reset ESP32 | `AT+RESET` | `OK+RESET` |

**Note:** AT commands are processed locally by the ESP32 and not forwarded to the HM-10.

## Configuration

### Default Settings

```c
// Target device name (can be changed via AT+NAME command)
"sallen_hm10"

// UART Configuration
UART_PORT_NUM   = UART_NUM_0 (USB Serial)
UART_BAUD_RATE  = 115200
UART_DATA_BITS  = 8
UART_PARITY     = None
UART_STOP_BITS  = 1
UART_FLOW_CTRL  = None

// BLE Service UUIDs
SERVICE_UUID    = 0xFFE0
CHAR_UUID       = 0xFFE1
```

### Modifying Default Device Name

Edit the default device name in [main/main.c](main/main.c):

```c
static char target_device_name[MAX_DEVICE_NAME_LEN] = "sallen_hm10";
```

### Changing UUIDs

If your HM-10 uses different UUIDs, modify these macros:

```c
#define REMOTE_SERVICE_UUID        0xFFE0
#define REMOTE_NOTIFY_CHAR_UUID    0xFFE1
```

## Project Structure

```
esp32-hm10/
├── CMakeLists.txt                 # Top-level CMake configuration
├── README.md                      # This file
├── IMPLEMENTATION_NOTES.md        # Detailed technical notes
├── sdkconfig                      # ESP-IDF configuration
├── sdkconfig.defaults             # Default configuration values
├── main/
│   ├── CMakeLists.txt             # Main component CMake file
│   ├── Kconfig.projbuild          # Project configuration menu
│   └── main.c                     # Application entry point
└── build/                         # Build output directory
```

## How It Works

### Connection Flow

1. **Initialization** → ESP32 initializes BLE stack and UART
2. **Scanning** → Scans for BLE devices advertising nearby
3. **Discovery** → Finds HM-10 device matching target name
4. **Connection** → Connects to HM-10 and negotiates MTU
5. **Service Discovery** → Searches for service 0xFFE0
6. **Characteristic Discovery** → Finds characteristic 0xFFE1
7. **Enable Notifications** → Writes to CCCD to enable notifications
8. **Data Transfer** → Bidirectional communication established

### Data Flow

**Computer → ESP32 → HM-10:**
```
UART RX → uart_rx_task() → esp_ble_gattc_write_char() → HM-10
```

**HM-10 → ESP32 → Computer:**
```
HM-10 → ESP_GATTC_NOTIFY_EVT → ESP_LOGI() → UART TX
```

### Event-Driven Architecture

The application uses ESP-IDF's event-driven BLE stack:
- **GAP events** handle scanning, connection, and disconnection
- **GATT events** handle service discovery, reads, writes, and notifications
- **UART task** runs continuously to read user input

## Troubleshooting

### ESP32 doesn't find the HM-10

**Possible causes:**
- HM-10 is not powered on or advertising
- Device name doesn't match target name
- HM-10 is already connected to another device

**Solutions:**
1. Verify HM-10 is powered and advertising (check with nRF Connect app)
2. Check device name: `AT+NAME?`
3. Set correct device name: `AT+NAME<correct_name>`
4. Ensure HM-10 is not connected elsewhere

### Connection drops frequently

**Possible causes:**
- Weak signal / distance too far
- Power supply issues
- RF interference

**Solutions:**
1. Move devices closer together
2. Check power supply stability
3. Reduce interference from other 2.4 GHz devices

### Data is garbled or incomplete

**Possible causes:**
- Baud rate mismatch
- Buffer overflow
- Character encoding issues

**Solutions:**
1. Verify UART is set to 115200 baud
2. Reduce data transmission rate
3. Ensure data is valid UTF-8

### No notifications received

**Possible causes:**
- HM-10 not sending data
- Notifications not properly enabled
- Connection lost

**Solutions:**
1. Check connection status: `AT+STATUS?`
2. Verify characteristic has notify property
3. Check ESP32 logs for GATTC events

## Logging

The application uses two log tags:

- **`GATTC_HM10`** - System logs (initialization, connection, errors)
- **`bt_com`** - BLE communication data (sent/received messages)

### Filtering Logs with ESP-IDF Monitor

To filter logs by tag, use the `idf.py monitor` filter feature:

```bash
idf.py monitor --print_filter bt_com:I
```

### Filtering Raw COM Port Output

When using external serial terminals (PuTTY, minicom, screen, etc.), the raw output contains many system logs. To filter out unrelated messages and see only BLE communication data, look for lines with the prefix:

```
I (<timestamp>) bt_com: 
```

**Example:**
```
I (12345) bt_com: Hello from HM-10!
```

## Development Notes

For detailed implementation information, including event sequences, memory management, and customization points, see [IMPLEMENTATION_NOTES.md](IMPLEMENTATION_NOTES.md).

## License

This project is provided as-is for educational and development purposes.

## Contributing

Contributions are welcome! Please ensure code follows ESP-IDF coding standards and includes appropriate documentation.

## Support

For issues or questions:
1. Check the [IMPLEMENTATION_NOTES.md](IMPLEMENTATION_NOTES.md) for technical details
2. Review ESP-IDF documentation for BLE GATT Client APIs
3. Use nRF Connect or similar tools to debug BLE communication

---
