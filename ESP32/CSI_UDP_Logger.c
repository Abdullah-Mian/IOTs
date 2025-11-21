/**
 * ESPectre-Lite: A Stable Wi-Fi CSI Data Logger for ESP32 with UDP Streaming
 *
 * This firmware provides a robust and stable foundation for capturing Channel State
 * Information (CSI) using an ESP32 and streaming it over UDP.
 *
 * This version replaces the serial printf with a UDP socket to send raw CSI data
 * to a listening server on a computer. All stability patterns from the previous
 * version (RTOS queue, separate processing task) are maintained.
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

// --- USER CONFIGURATION ---
#define WIFI_SSID         "YOUR_WIFI_SSID"      // <-- Set your Wi-Fi SSID
#define WIFI_PASSWORD     "YOUR_WIFI_PASSWORD"  // <-- Set your Wi-Fi Password
#define SERVER_IP_ADDR    "192.168.1.101"       // <-- VERY IMPORTANT: Set your Laptop's IP Address
#define SERVER_PORT       12345                 // The port to send UDP data to

// --- SYSTEM GLOBALS ---
static const char *TAG = "CSI_UDP_APP";
static QueueHandle_t s_csi_queue;

// Global variables for the UDP socket
static int g_udp_socket = -1;
static struct sockaddr_in g_dest_addr;


/**
 * @brief Initializes the UDP client socket and destination address.
 *
 * This function is called once the ESP32 has connected to Wi-Fi and obtained an IP address.
 */
static void udp_client_init(void)
{
    g_udp_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (g_udp_socket < 0) {
        ESP_LOGE(TAG, "Failed to create UDP socket: errno %d", errno);
        return;
    }

    // Configure the destination address structure
    g_dest_addr.sin_family = AF_INET;
    g_dest_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP_ADDR, &g_dest_addr.sin_addr.s_addr);

    ESP_LOGI(TAG, "UDP socket created. Streaming CSI to %s:%d", SERVER_IP_ADDR, SERVER_PORT);
}


/**
 * @brief The "Consumer" Task: Sends CSI data via UDP.
 *
 * This task waits for CSI data from the queue and sends it over the network.
 */
static void csi_processing_task(void *pvParameters)
{
    wifi_csi_info_t *csi_info;

    while (1) {
        if (xQueueReceive(s_csi_queue, &csi_info, portMAX_DELAY) == pdPASS) {
            
            // If the UDP socket is ready, send the data.
            if (g_udp_socket != -1) {
                // Send the raw CSI buffer. This is very efficient.
                sendto(g_udp_socket, csi_info->buf, csi_info->len, 0, (struct sockaddr *)&g_dest_addr, sizeof(g_dest_addr));
            }

            // Always free the memory that was allocated in the callback.
            free(csi_info->buf);
            free(csi_info);
        }
    }
}


/**
 * @brief The "Producer": The Wi-Fi CSI callback function.
 * This remains unchanged from the stable serial version. It's fast and non-blocking.
 */
static void wifi_csi_cb(void *ctx, wifi_csi_info_t *data)
{
    if (!data || !data->buf) return;

    wifi_csi_info_t *csi_info_copy = malloc(sizeof(wifi_csi_info_t));
    if (!csi_info_copy) return;
    memcpy(csi_info_copy, data, sizeof(wifi_csi_info_t));

    int8_t *csi_buf_copy = malloc(data->len);
    if (!csi_buf_copy) {
        free(csi_info_copy);
        return;
    }
    memcpy(csi_buf_copy, data->buf, data->len);
    csi_info_copy->buf = csi_buf_copy;

    if (xQueueSend(s_csi_queue, &csi_info_copy, 0) != pdTRUE) {
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

        // Now that the network is ready, initialize the UDP client.
        udp_client_init();
        
        // And now it's safe to enable CSI.
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
    ESP_ERROR_CHECK(nvs_flash_init());

    s_csi_queue = xQueueCreate(150, sizeof(wifi_csi_info_t *));
    if (!s_csi_queue) { ESP_LOGE(TAG, "Failed to create CSI queue."); return; }
    ESP_LOGI(TAG, "CSI queue created.");

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t* sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = { .ssid = WIFI_SSID, .password = WIFI_PASSWORD },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Wi-Fi started. Waiting for connection...");
    
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_LOGI(TAG, "Wi-Fi power saving disabled for real-time CSI.");

    xTaskCreate(csi_processing_task, "csi_processing_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "CSI processing task started and waiting for data.");
}