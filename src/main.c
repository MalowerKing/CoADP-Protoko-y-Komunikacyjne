#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sockets.h"

// --- ZMIEŃ NA SWOJE DANE ---
#define COAP_PORT      5683

static const char *TAG = "COAP_PIO_RTOS";

// --- TWOJE ZADANIE SERWERA ---
static void coap_server_task(void *pvParameters) {
    char rx_buffer[128];
    char addr_str[128];
    struct sockaddr_in dest_addr;

    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(COAP_PORT);

    // Tworzenie standardowego gniazda POSIX (lwIP)
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Nie udalo sie utworzyc gniazda: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Gniazdo UDP otwarte. Nasluchuje na porcie %d", COAP_PORT);

    int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Blad bind(): errno %d", errno);
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    // Nieskończona pętla serwera (nie blokuje innych zadań dzięki FreeRTOS)
    while (1) {
        struct sockaddr_storage source_addr;
        socklen_t socklen = sizeof(source_addr);

        ESP_LOGI(TAG, "Czekam na ramki CoAP...");
        
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, 
                           (struct sockaddr *)&source_addr, &socklen);

        if (len < 0) {
            ESP_LOGE(TAG, "Blad odbierania: errno %d", errno);
            break;
        } else {
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
            ESP_LOGI(TAG, "--- Otrzymano %d bajtow od IP: %s ---", len, addr_str);
            
            // Tutaj wrzucisz parser! Np.:
            // deserialize_coap_header((uint8_t*)rx_buffer, &moj_msg);
            
            ESP_LOGI(TAG, "Naglowek HEX: %02X %02X %02X %02X", 
                     rx_buffer[0], rx_buffer[1], rx_buffer[2], rx_buffer[3]);
        }
    }

    if (sock != -1) {
        close(sock);
    }
    vTaskDelete(NULL);
}

// --- STANDARDOWA OBSŁUGA WI-FI ESP-IDF ---
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Rozlaczono z Wi-Fi. Ponawiam...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Polaczono! IP: " IPSTR, IP2STR(&event->ip_info.ip));
        
        // Odpalamy serwer CoAP dopiero, gdy mamy IP
        xTaskCreate(coap_server_task, "coap_server", 4096, NULL, 5, NULL);
    }
}

void app_main(void) {
    // 1. Inicjalizacja pamięci (niezbędne dla Wi-Fi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Setup sieci i Wi-Fi
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}
