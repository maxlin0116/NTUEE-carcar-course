# BLE GATT Client Implementation Notes

## Overview

This implementation follows the ESP-IDF GATT Client API pattern for connecting to and interacting with a BLE device.

## Key Implementation Details

### 1. Device Discovery (GAP Layer)

The application uses the GAP (Generic Access Profile) callback to scan for devices:

```c
esp_gap_cb() -> ESP_GAP_BLE_SCAN_RESULT_EVT
```

When a device is found, it checks the advertised name against `DEVICE_NAME` ("sallen_hm10"). If matched, it stops scanning and initiates a connection.

### 2. Connection Flow

```
1. esp_ble_gattc_app_register(PROFILE_A_APP_ID)
   ↓
2. ESP_GATTC_REG_EVT → Start scanning
   ↓
3. Device found → esp_ble_gattc_open()
   ↓
4. ESP_GATTC_CONNECT_EVT → Send MTU request
   ↓
5. ESP_GATTC_CFG_MTU_EVT
   ↓
6. ESP_GATTC_DIS_SRVC_CMPL_EVT → Search services
```

### 3. Service and Characteristic Discovery

After connection, the code:
1. Searches for the service with UUID `0xFFE0`
2. Gets all characteristics in that service
3. Finds the characteristic with UUID `0xFFE1`
4. Stores the characteristic handle for future operations

### 4. Notification Setup

To receive notifications:
1. Register for notifications: `esp_ble_gattc_register_for_notify()`
2. Find the CCCD (Client Characteristic Configuration Descriptor)
3. Write value `0x0001` to CCCD to enable notifications
4. Handle incoming notifications in `ESP_GATTC_NOTIFY_EVT`

### 5. Read/Write Operations

**Read**: 
```c
esp_ble_gattc_read_char(gattc_if, conn_id, char_handle, auth_req)
→ ESP_GATTC_READ_CHAR_EVT (response with data)
```

**Write**:
```c
esp_ble_gattc_write_char(gattc_if, conn_id, char_handle, value_len, value, write_type, auth_req)
→ ESP_GATTC_WRITE_CHAR_EVT (confirmation)
```

## Message Length and MTU Limitations

### BLE MTU Configuration

The ESP32 requests a large MTU to support longer messages:

```c
// In app_main() - line 540
esp_ble_gatt_set_local_mtu(500);  // Request 500-byte MTU
```

**MTU Negotiation:**
1. ESP32 requests 500-byte MTU via `esp_ble_gatt_set_local_mtu()`
2. Upon connection, sends MTU request: `esp_ble_gattc_send_mtu_req()` (line 159)
3. Negotiated MTU is logged in `ESP_GATTC_CFG_MTU_EVT` (line 187)

**HM-10 Limitation:**

Despite the ESP32's 500-byte MTU request, the **HM-10 module is limited to 20-byte packets** due to:
- Default BLE 4.0 ATT MTU of 23 bytes (20 usable after 3-byte overhead)
- HM-10 does not support MTU negotiation
- Each BLE notification is limited to 20 bytes

**Impact:**
- Messages longer than 20 bytes are fragmented by the HM-10
- Each 20-byte fragment arrives as a separate `ESP_GATTC_NOTIFY_EVT`
- The Python client must reassemble fragments

**Buffer Sizes:**
- UART buffer: 1024 bytes (can receive long messages from computer)
- BLE packet: 20 bytes (HM-10 limitation)
- Each fragment logged separately with `bt_com` tag

## UART VCP Layer

### 6. UART Configuration

UART uses the default USB-Serial port (UART0) with the following settings:

```c
#define UART_PORT_NUM              UART_NUM_0
#define UART_BAUD_RATE             115200
#define UART_BUF_SIZE              1024        // Can receive long messages
```

Configuration: 8 data bits, no parity, 1 stop bit, no flow control.

**Buffer capacity:**
- RX buffer: 2048 bytes (UART_BUF_SIZE × 2, set in `uart_driver_install()`)
- Static buffer: 1024 bytes (`uart_data[UART_BUF_SIZE]`)
- Reads up to 1023 bytes per cycle (UART_BUF_SIZE - 1)

### 7. UART RX Task

The `uart_rx_task()` function runs as a FreeRTOS task that:

1. Reads bytes from UART with 20ms timeout
2. Checks if BLE is connected (`connect` flag and valid `char_handle`)
3. Calls `esp_ble_gattc_write_char()` to send data to HM-10
4. Delays 100ms between read cycles

```c
uart_rx_task() -> uart_read_bytes() -> esp_ble_gattc_write_char()
```

### 8. Log TAG Convention

Two TAGs are used to separate system logs from BLE data:

```c
static const char* TAG = "GATTC_HM10";      // System/debug logs
static const char* TAG_BT_COM = "bt_com";   // BLE communication data
```

**Usage:**
- `ESP_LOGI(TAG_BT_COM, ...)` - Data sent to BLE (TX)
- `ESP_LOGE(TAG_BT_COM, ...)` - Data received from BLE (RX)

The `ESP_LOGE` level is used for RX to make received data visually distinct (red color in terminal).

### 9. BLE Notification Handling

When `ESP_GATTC_NOTIFY_EVT` is received, the data is output using:

```c
ESP_LOGE(TAG_BT_COM, "%.*s", p_data->notify.value_len, p_data->notify.value);
```

This outputs the raw notification data with the `bt_com` tag.

### 10. Initialization Order

In `app_main()`:

```c
1. uart_init()           // UART driver and RX task
2. nvs_flash_init()      // NVS for BLE stack
3. esp_bt_controller_*   // BT controller
4. esp_bluedroid_*       // Bluedroid stack
5. Register callbacks    // GAP and GATTC
6. esp_ble_gattc_app_register()
```

UART is initialized first to display the welcome banner before BLE initialization logs.

### 11. Dependencies

CMakeLists.txt requires the `driver` component for UART:

```cmake
PRIV_REQUIRES bt nvs_flash driver
```

## UUID Format Note

The HM-10 uses 128-bit UUIDs, but the code uses 16-bit shortcuts:
- Service: `0xFFE0` = `0000FFE0-0000-1000-8000-00805F9B34FB`
- Characteristic: `0xFFE1` = `0000FFE1-0000-1000-8000-00805F9B34FB`

ESP-IDF automatically converts between 16-bit and 128-bit UUIDs using the Bluetooth Base UUID.

## Event Sequence (Normal Operation)

```
ESP_GATTC_REG_EVT
ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT
ESP_GAP_BLE_SCAN_START_COMPLETE_EVT
ESP_GAP_BLE_SCAN_RESULT_EVT (multiple times)
ESP_GATTC_CONNECT_EVT
ESP_GATTC_OPEN_EVT
ESP_GATTC_CFG_MTU_EVT
ESP_GATTC_DIS_SRVC_CMPL_EVT
ESP_GATTC_SEARCH_RES_EVT
ESP_GATTC_SEARCH_CMPL_EVT
ESP_GATTC_REG_FOR_NOTIFY_EVT
ESP_GATTC_WRITE_DESCR_EVT (CCCD write)
ESP_GATTC_READ_CHAR_EVT (initial read)
ESP_GATTC_WRITE_CHAR_EVT (write confirmation)
ESP_GATTC_NOTIFY_EVT (when notifications arrive)
```

## Memory Management

The code allocates memory dynamically for:
- Characteristic element arrays (`esp_gattc_char_elem_t`)
- Descriptor element arrays (`esp_gattc_descr_elem_t`)

Always `free()` these after use to prevent memory leaks.

## Customization Points

1. **Device Name**: Change `DEVICE_NAME` macro
2. **Service UUID**: Change `REMOTE_SERVICE_UUID` macro
3. **Characteristic UUID**: Change `REMOTE_NOTIFY_CHAR_UUID` macro
4. **Write Data**: Modify the write data in `ESP_GATTC_READ_CHAR_EVT` case
5. **Notification Handling**: Add custom logic in `ESP_GATTC_NOTIFY_EVT` case

## Common Issues and Solutions

### Issue: Service not found
**Solution**: Verify the UUID matches exactly. Use a BLE scanner app to confirm the service UUID.

### Issue: Notifications not working
**Solution**: Ensure:
1. The characteristic has notify property
2. CCCD write succeeded
3. The server is sending notifications

### Issue: Write fails
**Solution**: Check:
1. The characteristic has write property
2. MTU is large enough for the data
3. The connection is still active

## Testing Tips

1. Use nRF Connect or similar BLE scanner apps to verify the HM-10 is advertising
2. Check the characteristic properties (read/write/notify) match expectations
3. Monitor the ESP32 logs to see the event sequence
4. Test with small data packets first before sending larger payloads
