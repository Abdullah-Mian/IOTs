/**
 * ESPectre-Lite: A Stable Wi-Fi CSI Data Logger for ESP32
 *
 * This firmware provides a robust and stable foundation for capturing Channel State
 * Information (CSI) using a classic ESP32. It is confirmed to compile and run.
 *
 * Key stability features:
 * 1.  Connects to a standard Wi-Fi network as a station.
 * 2.  Disables Wi-Fi power saving for consistent, low-latency packet reception.
 * 3.  Uses a FreeRTOS queue (Producer-Consumer pattern) to safely handle CSI data,
 *     preventing crashes from doing heavy work in the Wi-Fi driver's context.
 * 4.  Correctly uses signed pointers (int8_t*) for CSI buffers to match the ESP-IDF
 *     API, resolving all previous compiler errors.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h> // For assert()
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

// --- USER CONFIGURATION ---
#define WIFI_SSID      "YOUR_WIFI_SSID"      // <-- Set your Wi-Fi SSID
#define WIFI_PASSWORD  "YOUR_WIFI_PASSWORD"  // <-- Set your Wi-Fi Password

// --- SYSTEM CONFIGURATION ---
static const char *TAG = "CSI_APP";
static QueueHandle_t s_csi_queue;

/**
 * @brief The "Consumer" Task: Processes CSI data received from the queue.
 */
static void csi_processing_task(void *pvParameters)
{
    wifi_csi_info_t *csi_info;

    while (1) {
        if (xQueueReceive(s_csi_queue, &csi_info, portMAX_DELAY) == pdPASS) {
            
            printf("CSI_DATA,");
            printf("%d,%d,", csi_info->rx_ctrl.rssi, csi_info->len);

            int8_t *csi_buf = csi_info->buf;
            for (int i = 0; i < csi_info->len; i++) {
                printf("%d", csi_buf[i]);
                if (i < csi_info->len - 1) {
                    printf(",");
                }
            }
            printf("\n");

            // Safely free the memory that was allocated in the callback.
            free(csi_info->buf);
            free(csi_info);
        }
    }
}

/**
 * @brief The "Producer": The Wi-Fi CSI callback function.
 */
static void wifi_csi_cb(void *ctx, wifi_csi_info_t *data)
{
    if (!data || !data->buf) {
        return;
    }

    wifi_csi_info_t *csi_info_copy = malloc(sizeof(wifi_csi_info_t));
    if (!csi_info_copy) {
        ESP_LOGE(TAG, "csi_cb: Malloc fail for csi_info");
        return;
    }
    memcpy(csi_info_copy, data, sizeof(wifi_csi_info_t));

    // FIX: The buffer MUST be of type int8_t* to match the csi_info struct definition.
    int8_t *csi_buf_copy = malloc(data->len);
    if (!csi_buf_copy) {
        ESP_LOGE(TAG, "csi_cb: Malloc fail for csi_buffer");
        free(csi_info_copy);
        return;
    }
    memcpy(csi_buf_copy, data->buf, data->len);
    csi_info_copy->buf = csi_buf_copy; // This assignment is now type-correct.

    if (xQueueSend(s_csi_queue, &csi_info_copy, 0) != pdTRUE) {
        // Queue is full, free the memory we just allocated.
        free(csi_buf_copy);
        free(csi_info_copy);
    }
}

/**
 * @brief Event handler for Wi-Fi and IP events.
 */
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Connecting to Wi-Fi...");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Disconnected from Wi-Fi. Reconnecting...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Wi-Fi Connected! Got IP: " IPSTR, IP2STR(&event->ip_info.ip));

        // Now that we are connected, it's safe to enable CSI.
        // For classic ESP32, this is the correct CSI configuration structure.
        wifi_csi_config_t csi_config = {
            .lltf_en = true, .htltf_en = true, .stbc_htltf2_en = true,
            .ltf_merge_en = true, .channel_filter_en = true, .manu_scale = false,
        };
        ESP_ERROR_CHECK(esp_wifi_set_csi_config(&csi_config));
        ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(wifi_csi_cb, NULL));
        ESP_ERROR_CHECK(esp_wifi_set_csi(true));
        ESP_LOGI(TAG, "CSI capturing enabled.");
    }
}

void app_main(void)
{
    // 1. Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Create the FreeRTOS queue
    s_csi_queue = xQueueCreate(150, sizeof(wifi_csi_info_t *));
    if (s_csi_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create CSI queue. Halting.");
        return;
    }
    ESP_LOGI(TAG, "CSI queue created successfully.");

    // 3. Initialize Networking Stack and Event Loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t* sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    // 4. Initialize Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 5. Register Event Handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    // 6. Configure Wi-Fi Connection
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    // 7. Start Wi-Fi
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Wi-Fi started. Waiting for connection...");
    
    // 8. IMPORTANT: Disable Wi-Fi power saving for low-latency CSI.
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_LOGI(TAG, "Wi-Fi power saving disabled for real-time CSI.");

    // 9. Create and start the CSI processing task.
    xTaskCreate(csi_processing_task, "csi_processing_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "CSI processing task started and waiting for data.");
}