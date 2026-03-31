/*
 * BLE GATT Client for HM-10 Module
 * Connects to device "sallen_hm10" and interacts with its characteristic
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>
#include "nvs.h"
#include "nvs_flash.h"

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"

#define REMOTE_SERVICE_UUID        0xFFE0
#define REMOTE_NOTIFY_CHAR_UUID    0xFFE1
#define PROFILE_NUM                1
#define PROFILE_A_APP_ID           0
#define INVALID_HANDLE             0
#define NVS_NAMESPACE              "config"
#define NVS_DEVICE_NAME_KEY        "device_name"
#define MAX_DEVICE_NAME_LEN        32
#define AT_CMD_PREFIX              "AT+NAME"

// UART Configuration
#define UART_PORT_NUM              UART_NUM_0
#define UART_BAUD_RATE             115200
#define UART_TX_PIN                UART_PIN_NO_CHANGE
#define UART_RX_PIN                UART_PIN_NO_CHANGE
#define UART_BUF_SIZE              1024
#define UART_RX_TASK_STACK_SIZE    2048
#define UART_RX_TASK_PRIORITY      10

static const char* TAG = "GATTC_HM10";      // TAG for general logs
static const char* TAG_BT_COM = "bt_com";   // TAG for BLE communication data

// GATT client profile instance
struct gattc_profile_inst {
    esp_gattc_cb_t gattc_cb;
    uint16_t gattc_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_start_handle;
    uint16_t service_end_handle;
    uint16_t char_handle;
    esp_bd_addr_t remote_bda;
};

// Declare callback
static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);

// Profile array
static struct gattc_profile_inst gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_A_APP_ID] = {
        .gattc_cb = gattc_profile_event_handler,
        .gattc_if = ESP_GATT_IF_NONE,
    },
};

static bool connect = false;
static bool get_service = false;
// static esp_gatt_srvc_id_t remote_service_id;
// static esp_gatt_id_t remote_char_id;

// Target device address (will be filled during scan)
static esp_bd_addr_t target_device_addr;
// static bool target_device_found = false;

// Target device name (loaded from NVS)
static char target_device_name[MAX_DEVICE_NAME_LEN] = "sallen_hm10";  // Default value

// UART data buffer
static uint8_t uart_data[UART_BUF_SIZE];

// BLE scan parameters
static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x50,
    .scan_window            = 0x30,
    .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
};

// Function to save device name to NVS
static esp_err_t save_device_name_to_nvs(const char* device_name)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_str(nvs_handle, NVS_DEVICE_NAME_KEY, device_name);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error writing device name: %s", esp_err_to_name(err));
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error committing NVS: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Device name saved to NVS: %s", device_name);
    }

    nvs_close(nvs_handle);
    return err;
}

// Function to load device name from NVS
static esp_err_t load_device_name_from_nvs(char* device_name, size_t max_len)
{
    nvs_handle_t nvs_handle;
    esp_err_t err;

    err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        // Namespace doesn't exist yet - this is normal on first boot
        ESP_LOGI(TAG, "NVS namespace not found, using default device name: %s", device_name);
        return ESP_OK;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        return err;
    }

    size_t required_size = 0;
    err = nvs_get_str(nvs_handle, NVS_DEVICE_NAME_KEY, NULL, &required_size);
    
    if (err == ESP_OK && required_size > 0) {
        if (required_size > max_len) {
            ESP_LOGE(TAG, "Device name too long in NVS: %d bytes", required_size);
            nvs_close(nvs_handle);
            return ESP_ERR_NVS_INVALID_LENGTH;
        }

        err = nvs_get_str(nvs_handle, NVS_DEVICE_NAME_KEY, device_name, &required_size);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Device name loaded from NVS: %s", device_name);
        } else {
            ESP_LOGE(TAG, "Error reading device name: %s", esp_err_to_name(err));
        }
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "Device name not found in NVS, using default: %s", device_name);
        err = ESP_OK;  // Not an error - just use default
    } else {
        ESP_LOGE(TAG, "Error getting device name size: %s", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
    return err;
}

// UART receive task - reads user input and sends to BLE
static void uart_rx_task(void *arg)
{
    static char mes_buff[11];
    while (1) {
        // MTU of HM-10 is only 23bytes. The string can be at most 20bytes in UTF-8. So we read max 10 chars here.
        int len = uart_read_bytes(UART_PORT_NUM, uart_data, 10, 10 / portTICK_PERIOD_MS);

        if (len > 0) {
            memcpy(mes_buff, uart_data, len);
            mes_buff[len] = '\0';  // Null terminate for safety

            // Check if this is an AT+NAME command (needs more data)
            if (strncmp(mes_buff, AT_CMD_PREFIX, strlen(AT_CMD_PREFIX)) == 0) {
                // Read the rest of the device name (up to MAX_DEVICE_NAME_LEN + prefix length)
                int additional_len = uart_read_bytes(UART_PORT_NUM, uart_data + len, 
                                                    MAX_DEVICE_NAME_LEN - len, 
                                                    10 / portTICK_PERIOD_MS);
                if (additional_len > 0) {
                    len += additional_len;
                    memcpy(mes_buff, uart_data, len);
                    mes_buff[len] = '\0';
                }
            }

            // Handle AT+RESET (reply then reset)
            if (strcmp(mes_buff, "AT+RESET") == 0) {
                ESP_LOGI(TAG_BT_COM, "OK+RESET");
                vTaskDelay(100 / portTICK_PERIOD_MS);
                esp_restart();
                continue; // not reached, but keep for clarity
            }

            // Handle AT+NAME? query
            if (strcmp(mes_buff, "AT+NAME?") == 0) {
                ESP_LOGI(TAG_BT_COM, "OK+NAME%s", target_device_name);
                continue;
            }

            // Handle AT+STATUS? query
            if (strcmp(mes_buff, "AT+STATUS?") == 0) {
                if (connect) {
                    ESP_LOGI(TAG_BT_COM, "OK+CONN");
                } else {
                    ESP_LOGI(TAG_BT_COM, "OK+UNCONN");
                }
                continue;
            }

            // Check if this is an AT command
            if (strncmp(mes_buff, AT_CMD_PREFIX, strlen(AT_CMD_PREFIX)) == 0) {
                // Extract device name after AT+NAME
                char* new_name = (char*)mes_buff + strlen(AT_CMD_PREFIX);
                size_t name_len = strlen(new_name);

                if (name_len > 0 && name_len < MAX_DEVICE_NAME_LEN) {
                    // Save to NVS
                    esp_err_t err = save_device_name_to_nvs(new_name);
                    if (err == ESP_OK) {
                        ESP_LOGI(TAG_BT_COM, "OK+SET%s", new_name);
                        // Update the in-memory copy
                        strncpy(target_device_name, new_name, MAX_DEVICE_NAME_LEN - 1);
                        target_device_name[MAX_DEVICE_NAME_LEN - 1] = '\0';
                    } else {
                        ESP_LOGE(TAG, "Failed to save device name to NVS");
                    }
                } else {
                    ESP_LOGE(TAG, "Invalid device name length: %d", name_len);
                }
                // Don't forward AT commands to BLE
                continue;
            }

            // Check if BLE is connected and we have a valid characteristic handle
            if (connect && gl_profile_tab[PROFILE_A_APP_ID].char_handle != INVALID_HANDLE) {
                // Log sent data with bt_com TAG
                // ESP_LOGI(TAG_BT_COM, "%s", uart_data);

                // Send data to BLE characteristic
                esp_ble_gattc_write_char(
                    gl_profile_tab[PROFILE_A_APP_ID].gattc_if,
                    gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                    gl_profile_tab[PROFILE_A_APP_ID].char_handle,
                    len,
                    (uint8_t*)mes_buff,
                    ESP_GATT_WRITE_TYPE_RSP,
                    ESP_GATT_AUTH_REQ_NONE
                );
            }
        }
        vTaskDelay(25 / portTICK_PERIOD_MS);
    }
}

// Initialize UART for user input/output
static void uart_init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
    // Install UART driver with larger buffers and no event queue
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE * 2, UART_BUF_SIZE * 2, 0, NULL, 0));

    // Create UART receive task
    xTaskCreate(uart_rx_task, "uart_rx_task", UART_RX_TASK_STACK_SIZE, NULL, UART_RX_TASK_PRIORITY, NULL);
}

static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *)param;

    switch (event) {
    case ESP_GATTC_REG_EVT:
        ESP_LOGI(TAG, "REG_EVT");
        // Start scanning for the target device
        esp_ble_gap_set_scan_params(&ble_scan_params);
        break;

    case ESP_GATTC_CONNECT_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_CONNECT_EVT conn_id %d, if %d", p_data->connect.conn_id, gattc_if);
        gl_profile_tab[PROFILE_A_APP_ID].conn_id = p_data->connect.conn_id;
        memcpy(gl_profile_tab[PROFILE_A_APP_ID].remote_bda, p_data->connect.remote_bda, sizeof(esp_bd_addr_t));
        ESP_LOGI(TAG, "REMOTE BDA:");
        ESP_LOG_BUFFER_HEX(TAG, gl_profile_tab[PROFILE_A_APP_ID].remote_bda, sizeof(esp_bd_addr_t));

        // Configure MTU
        esp_err_t mtu_ret = esp_ble_gattc_send_mtu_req(gattc_if, p_data->connect.conn_id);
        if (mtu_ret) {
            ESP_LOGE(TAG, "config MTU error, error code = %x", mtu_ret);
        }
        break;

    case ESP_GATTC_OPEN_EVT:
        if (param->open.status != ESP_GATT_OK) {
            ESP_LOGE(TAG, "open failed, status %d", p_data->open.status);
            break;
        }
        ESP_LOGI(TAG, "open success");
        break;

    case ESP_GATTC_DIS_SRVC_CMPL_EVT:
        if (param->dis_srvc_cmpl.status != ESP_GATT_OK) {
            ESP_LOGE(TAG, "discover service failed, status %d", param->dis_srvc_cmpl.status);
            break;
        }
        ESP_LOGI(TAG, "discover service complete conn_id %d", param->dis_srvc_cmpl.conn_id);
        // Search for the service
        esp_ble_gattc_search_service(gattc_if, param->cfg_mtu.conn_id, NULL);
        break;

    case ESP_GATTC_CFG_MTU_EVT:
        if (param->cfg_mtu.status != ESP_GATT_OK) {
            ESP_LOGE(TAG, "config mtu failed, error status = %x", param->cfg_mtu.status);
        }
        ESP_LOGI(TAG, "ESP_GATTC_CFG_MTU_EVT, Status %d, MTU %d, conn_id %d", param->cfg_mtu.status, param->cfg_mtu.mtu, param->cfg_mtu.conn_id);
        break;

    case ESP_GATTC_SEARCH_RES_EVT: {
        ESP_LOGI(TAG, "SEARCH RES: conn_id = %x is primary service %d", p_data->search_res.conn_id, p_data->search_res.is_primary);
        ESP_LOGI(TAG, "start handle %d end handle %d current handle value %d", p_data->search_res.start_handle, p_data->search_res.end_handle, p_data->search_res.srvc_id.inst_id);
        
        if (p_data->search_res.srvc_id.uuid.len == ESP_UUID_LEN_16 && p_data->search_res.srvc_id.uuid.uuid.uuid16 == REMOTE_SERVICE_UUID) {
            ESP_LOGI(TAG, "Found target service!");
            get_service = true;
            gl_profile_tab[PROFILE_A_APP_ID].service_start_handle = p_data->search_res.start_handle;
            gl_profile_tab[PROFILE_A_APP_ID].service_end_handle = p_data->search_res.end_handle;
        }
        break;
    }

    case ESP_GATTC_SEARCH_CMPL_EVT:
        if (p_data->search_cmpl.status != ESP_GATT_OK) {
            ESP_LOGE(TAG, "search service failed, error status = %x", p_data->search_cmpl.status);
            break;
        }
        ESP_LOGI(TAG, "ESP_GATTC_SEARCH_CMPL_EVT");
        
        if (get_service) {
            // Get all characteristics
            uint16_t count = 0;
            esp_gatt_status_t status = esp_ble_gattc_get_attr_count(gattc_if,
                                                                     p_data->search_cmpl.conn_id,
                                                                     ESP_GATT_DB_CHARACTERISTIC,
                                                                     gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
                                                                     gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
                                                                     INVALID_HANDLE,
                                                                     &count);
            if (status != ESP_GATT_OK) {
                ESP_LOGE(TAG, "esp_ble_gattc_get_attr_count error");
            }

            if (count > 0) {
                esp_gattc_char_elem_t *char_elem_result = (esp_gattc_char_elem_t *)malloc(sizeof(esp_gattc_char_elem_t) * count);
                if (!char_elem_result) {
                    ESP_LOGE(TAG, "gattc no mem");
                } else {
                    status = esp_ble_gattc_get_all_char(gattc_if,
                                                        p_data->search_cmpl.conn_id,
                                                        gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
                                                        gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
                                                        char_elem_result,
                                                        &count,
                                                        0);
                    if (status != ESP_GATT_OK) {
                        ESP_LOGE(TAG, "esp_ble_gattc_get_all_char error");
                    }

                    // Find the target characteristic
                    for (int i = 0; i < count; i++) {
                        if (char_elem_result[i].uuid.len == ESP_UUID_LEN_16 && 
                            char_elem_result[i].uuid.uuid.uuid16 == REMOTE_NOTIFY_CHAR_UUID) {
                            ESP_LOGI(TAG, "Found target characteristic!");
                            gl_profile_tab[PROFILE_A_APP_ID].char_handle = char_elem_result[i].char_handle;
                            
                            // Register for notifications
                            esp_ble_gattc_register_for_notify(gattc_if, gl_profile_tab[PROFILE_A_APP_ID].remote_bda, char_elem_result[i].char_handle);
                            break;
                        }
                    }
                    free(char_elem_result);
                }
            }
        }
        break;

    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
        ESP_LOGI(TAG, "ESP_GATTC_REG_FOR_NOTIFY_EVT");
        if (p_data->reg_for_notify.status != ESP_GATT_OK) {
            ESP_LOGE(TAG, "REG FOR NOTIFY failed: error status = %d", p_data->reg_for_notify.status);
        } else {
            // Write to CCCD to enable notifications
            uint16_t notify_en = 1;
            
            // Get the CCCD descriptor
            uint16_t count = 0;
            esp_gatt_status_t ret_status = esp_ble_gattc_get_attr_count(gattc_if,
                                                                         gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                                                         ESP_GATT_DB_DESCRIPTOR,
                                                                         gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
                                                                         gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
                                                                         gl_profile_tab[PROFILE_A_APP_ID].char_handle,
                                                                         &count);
            if (ret_status != ESP_GATT_OK) {
                ESP_LOGE(TAG, "esp_ble_gattc_get_attr_count error");
            }
            
            if (count > 0) {
                esp_gattc_descr_elem_t *descr_elem_result = (esp_gattc_descr_elem_t *)malloc(sizeof(esp_gattc_descr_elem_t) * count);
                if (!descr_elem_result) {
                    ESP_LOGE(TAG, "malloc error, gattc no mem");
                } else {
                    ret_status = esp_ble_gattc_get_all_descr(gattc_if,
                                                             gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                                             gl_profile_tab[PROFILE_A_APP_ID].char_handle,
                                                             descr_elem_result,
                                                             &count,
                                                             0);
                    if (ret_status != ESP_GATT_OK) {
                        ESP_LOGE(TAG, "esp_ble_gattc_get_all_descr error");
                    }

                    // Write to CCCD
                    for (int i = 0; i < count; ++i) {
                        if (descr_elem_result[i].uuid.len == ESP_UUID_LEN_16 && 
                            descr_elem_result[i].uuid.uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG) {
                            esp_ble_gattc_write_char_descr(gattc_if,
                                                           gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                                           descr_elem_result[i].handle,
                                                           sizeof(notify_en),
                                                           (uint8_t *)&notify_en,
                                                           ESP_GATT_WRITE_TYPE_RSP,
                                                           ESP_GATT_AUTH_REQ_NONE);
                            break;
                        }
                    }
                    free(descr_elem_result);
                }
            }
            
            // Perform an initial read
            ESP_LOGI(TAG, "Reading characteristic value...");
            esp_ble_gattc_read_char(gattc_if, gl_profile_tab[PROFILE_A_APP_ID].conn_id, 
                                   gl_profile_tab[PROFILE_A_APP_ID].char_handle, 
                                   ESP_GATT_AUTH_REQ_NONE);
        }
        break;
    }

    case ESP_GATTC_NOTIFY_EVT:
        // Output received BLE data with bt_com TAG
        ESP_LOGI(TAG_BT_COM, "%.*s", p_data->notify.value_len, p_data->notify.value);
        break;

    case ESP_GATTC_READ_CHAR_EVT:
        if (p_data->read.status != ESP_GATT_OK) {
            ESP_LOGE(TAG, "READ char failed, error status = %x", p_data->read.status);
        } else {
            ESP_LOGI(TAG, "READ char success:");
            ESP_LOG_BUFFER_HEX(TAG, p_data->read.value, p_data->read.value_len);
            ESP_LOGI(TAG, "Read data (string): %.*s", p_data->read.value_len, p_data->read.value);
            
            // Write example data to the characteristic
            // vTaskDelay(1000 / portTICK_PERIOD_MS);
            // char write_data[] = "Hello from ESP32!";
            // ESP_LOGI(TAG, "Writing to characteristic...");
            // esp_ble_gattc_write_char(gattc_if,
            //                         gl_profile_tab[PROFILE_A_APP_ID].conn_id,
            //                         gl_profile_tab[PROFILE_A_APP_ID].char_handle,
            //                         sizeof(write_data),
            //                         (uint8_t *)write_data,
            //                         ESP_GATT_WRITE_TYPE_RSP,
            //                         ESP_GATT_AUTH_REQ_NONE);
        }
        break;

    case ESP_GATTC_WRITE_CHAR_EVT:
        if (p_data->write.status != ESP_GATT_OK) {
            ESP_LOGE(TAG, "WRITE char failed, error status = %x", p_data->write.status);
        } else {
            // ESP_LOGI(TAG, "WRITE char success");
            // ESP_LOGI(TAG, "Connection active - waiting for notifications...");
            // ESP_LOGI(TAG, "Send data to the HM-10 to see notifications here!");
        }
        break;

    case ESP_GATTC_WRITE_DESCR_EVT:
        if (p_data->write.status != ESP_GATT_OK) {
            ESP_LOGE(TAG, "WRITE descr failed, error status = %x", p_data->write.status);
        } else {
            ESP_LOGI(TAG, "WRITE descr success - Notifications enabled");
        }
        break;

    case ESP_GATTC_DISCONNECT_EVT:
        connect = false;
        get_service = false;
        ESP_LOGI(TAG, "ESP_GATTC_DISCONNECT_EVT, reason = %d", p_data->disconnect.reason);
        ESP_LOGI(TAG, "Disconnected. Restarting scan to reconnect...");

        // Restart scanning after a short delay
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        esp_ble_gap_start_scanning(30);
        break;

    default:
        break;
    }
}

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    uint8_t *adv_name = NULL;
    uint8_t adv_name_len = 0;
    
    switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
        ESP_LOGI(TAG, "Scan parameters set, starting scan...");
        esp_ble_gap_start_scanning(30);
        break;

    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "scan start failed, error status = %x", param->scan_start_cmpl.status);
            break;
        }
        ESP_LOGI(TAG, "Scan started successfully");
        break;

    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
        switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            // Parse advertisement data to find device name
            adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv,
                                                ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);
            
            if (adv_name != NULL) {
                if (strlen(target_device_name) == adv_name_len && strncmp((char *)adv_name, target_device_name, adv_name_len) == 0) {
                    ESP_LOGI(TAG, "Found target device: %s", target_device_name);
                    ESP_LOG_BUFFER_HEX(TAG, scan_result->scan_rst.bda, 6);

                    if (!connect) {
                        connect = true;
                        ESP_LOGI(TAG, "Stopping scan and connecting...");
                        esp_ble_gap_stop_scanning();
                        memcpy(target_device_addr, scan_result->scan_rst.bda, sizeof(esp_bd_addr_t));
                        esp_ble_gattc_open(gl_profile_tab[PROFILE_A_APP_ID].gattc_if,
                                          target_device_addr,
                                          scan_result->scan_rst.ble_addr_type,
                                          true);
                    }
                }
            }
            break;
        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
            ESP_LOGI(TAG, "Scan complete");
            break;
        default:
            break;
        }
        break;
    }

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Scan stop failed, error status = %x", param->scan_stop_cmpl.status);
            break;
        }
        ESP_LOGI(TAG, "Scan stopped successfully");
        break;

    default:
        break;
    }
}

static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    // Route to profile
    if (event == ESP_GATTC_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            gl_profile_tab[param->reg.app_id].gattc_if = gattc_if;
        } else {
            ESP_LOGI(TAG, "reg app failed, app_id %04x, status %d",
                    param->reg.app_id,
                    param->reg.status);
            return;
        }
    }

    // Call profile callback
    do {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++) {
            if (gattc_if == ESP_GATT_IF_NONE || gattc_if == gl_profile_tab[idx].gattc_if) {
                if (gl_profile_tab[idx].gattc_cb) {
                    gl_profile_tab[idx].gattc_cb(event, gattc_if, param);
                }
            }
        }
    } while (0);
}

void app_main(void)
{
    // // Set default log level to WARNING
    // esp_log_level_set("*", ESP_LOG_WARN);
    
    // // Set bt_com log level to INFO to see BLE communication
    // esp_log_level_set(TAG_BT_COM, ESP_LOG_INFO);
    
    ESP_LOGI(TAG, "Starting BLE GATT Client for HM-10");

    // Initialize UART first for user feedback
    uart_init();

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Load target device name from NVS
    load_device_name_from_nvs(target_device_name, MAX_DEVICE_NAME_LEN);
    ESP_LOGI(TAG, "Target device name: %s", target_device_name);

    // Release classic BT memory
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    // Initialize BT controller
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "%s initialize controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    // Initialize Bluedroid
    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    // Register callbacks
    ret = esp_ble_gap_register_callback(esp_gap_cb);
    if (ret) {
        ESP_LOGE(TAG, "%s gap register failed, error code = %x", __func__, ret);
        return;
    }

    ret = esp_ble_gattc_register_callback(esp_gattc_cb);
    if (ret) {
        ESP_LOGE(TAG, "%s gattc register failed, error code = %x", __func__, ret);
        return;
    }

    ret = esp_ble_gattc_app_register(PROFILE_A_APP_ID);
    if (ret) {
        ESP_LOGE(TAG, "%s gattc app register failed, error code = %x", __func__, ret);
    }

    // Set MTU
    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
    if (local_mtu_ret) {
        ESP_LOGE(TAG, "set local  MTU failed, error code = %x", local_mtu_ret);
    }

    ESP_LOGI(TAG, "BLE GATT Client initialized, searching for device: %s", target_device_name);
    ESP_LOGI(TAG, "To change target device, send: AT+NAME<device_name>");
}
