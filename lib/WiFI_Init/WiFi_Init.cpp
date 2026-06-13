#include "WiFi_Init.h"

#include "esp_netif.h"
#include "esp_log.h"
#include "lwip/ip4_addr.h"
#include "nvs_flash.h"
#include <cstring>

// ════════════════════════════════════════════════════════════════════════════
// Konstruktor / Destruktor
// ════════════════════════════════════════════════════════════════════════════

WiFi_Init::WiFi_Init(const char*   ssid,
                     const char*   password,
                     int           max_retries,
                     StateCallback on_change)
    : _ssid(ssid),
      _password(password),
      _max_retries(max_retries),
      _on_change(on_change)
{}

WiFi_Init::~WiFi_Init() {
    stop();
    if (_event_group) {
        vEventGroupDelete(_event_group);
        _event_group = nullptr;
    }
}

// ════════════════════════════════════════════════════════════════════════════
// begin() — inicjalizacja stosu i start taska
// ════════════════════════════════════════════════════════════════════════════

void WiFi_Init::begin() {
    // Inicjalizacja lwIP i domyślnego interfejsu STA (tylko raz)
    static bool netif_inited = false;
    if (!netif_inited) {
        // NVS musi być zainicjalizowane przed WiFi (przechowuje kalibrację RF i dane WiFi)
        esp_err_t nvs_ret = nvs_flash_init();
        if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES ||
            nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            // Partycja NVS uszkodzona lub niekompatybilna — wymaż i zainicjuj ponownie
            ESP_LOGW(TAG, "NVS wymaga formatowania (err=0x%x)", nvs_ret);
            ESP_ERROR_CHECK(nvs_flash_erase());
            nvs_ret = nvs_flash_init();
        }
        ESP_ERROR_CHECK(nvs_ret);

        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_create_default_wifi_sta();
        netif_inited = true;
    }

    // Inicjalizacja drivera WiFi z domyślną konfiguracją
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Rejestracja zdarzeń WiFi i IP
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &_event_handler, this, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &_event_handler, this, nullptr));

    _event_group = xEventGroupCreate();

    // Task FreeRTOS zarządzający połączeniem i reconnect
    xTaskCreate(_reconnect_task,
                "wifi_reconnect",
                TASK_STACK_SIZE,
                this,
                TASK_PRIORITY,
                &_task_handle);

    ESP_LOGI(TAG, "begin() — task reconnect uruchomiony");
}

// ════════════════════════════════════════════════════════════════════════════
// wait_for_connection() — blokuje do czasu połączenia lub błędu
// ════════════════════════════════════════════════════════════════════════════

bool WiFi_Init::wait_for_connection(uint32_t timeout_ms) {
    if (!_event_group) return false;

    TickType_t ticks = (timeout_ms == 0)
                       ? portMAX_DELAY
                       : pdMS_TO_TICKS(timeout_ms);

    EventBits_t bits = xEventGroupWaitBits(
        _event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,   // nie kasuj bitów
        pdFALSE,   // czekaj na KTÓRYKOLWIEK
        ticks);

    return (bits & WIFI_CONNECTED_BIT) != 0;
}

// ════════════════════════════════════════════════════════════════════════════
// stop()
// ════════════════════════════════════════════════════════════════════════════

void WiFi_Init::stop() {
    _stop_flag = true;
    if (_task_handle) {
        // Daj taskowi szansę na zakończenie pętli
        vTaskDelay(pdMS_TO_TICKS(100));
        vTaskDelete(_task_handle);
        _task_handle = nullptr;
    }
    esp_wifi_disconnect();
    esp_wifi_stop();
    _set_state(State::DISCONNECTED);
}

// ════════════════════════════════════════════════════════════════════════════
// Handler zdarzeń ESP-IDF (statyczny)
// ════════════════════════════════════════════════════════════════════════════

void WiFi_Init::_event_handler(void*            arg,
                                esp_event_base_t event_base,
                                int32_t          event_id,
                                void*            event_data)
{
    auto* self = static_cast<WiFi_Init*>(arg);

    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi start → łączę z '%s'", self->_ssid);
                self->_do_connect();
                break;

            case WIFI_EVENT_STA_DISCONNECTED: {
                auto* disc = static_cast<wifi_event_sta_disconnected_t*>(event_data);
                ESP_LOGW(TAG, "Rozłączono (reason=%d)", disc->reason);

                // Powiadamiamy task reconnect przez EventGroup (kasujemy CONNECTED)
                xEventGroupClearBits(self->_event_group, WIFI_CONNECTED_BIT);
                self->_set_state(State::DISCONNECTED);
                break;
            }

            default:
                break;
        }

    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        auto* evt = static_cast<ip_event_got_ip_t*>(event_data);
        char buf[16];
        esp_ip4addr_ntoa(&evt->ip_info.ip, buf, sizeof(buf));
        self->_ip_str = buf;
        self->_retry_count = 0;

        ESP_LOGI(TAG, "Połączono! IP: %s", buf);
        self->_set_state(State::CONNECTED);

        xEventGroupSetBits(self->_event_group, WIFI_CONNECTED_BIT);
        xEventGroupClearBits(self->_event_group, WIFI_FAIL_BIT);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Task FreeRTOS — pętla reconnect
// ════════════════════════════════════════════════════════════════════════════

void WiFi_Init::_reconnect_task(void* pvParam) {
    static_cast<WiFi_Init*>(pvParam)->_reconnect_loop();
    vTaskDelete(nullptr);
}

void WiFi_Init::_reconnect_loop() {
    // Konfiguracja trybu i danych logowania
    wifi_config_t wifi_cfg = {};
    strncpy(reinterpret_cast<char*>(wifi_cfg.sta.ssid),
            _ssid, sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy(reinterpret_cast<char*>(wifi_cfg.sta.password),
            _password, sizeof(wifi_cfg.sta.password) - 1);
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());   // → wywoła WIFI_EVENT_STA_START

    while (!_stop_flag) {
        // Czekaj na rozłączenie (brak CONNECTED_BIT)
        EventBits_t bits = xEventGroupWaitBits(
            _event_group,
            WIFI_CONNECTED_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

        if (bits & WIFI_CONNECTED_BIT) {
            // Jesteśmy połączeni — czekaj na utratę połączenia
            while (!_stop_flag && (_state == State::CONNECTED)) {
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }

        if (_stop_flag) break;

        // Sprawdź limit prób
        if (_max_retries > 0 && _retry_count >= _max_retries) {
            ESP_LOGE(TAG, "Przekroczono limit prób (%d). CoAP zaczeka.", _max_retries);
            _set_state(State::FAILED);
            xEventGroupSetBits(_event_group, WIFI_FAIL_BIT);
            // Nie przerywamy taska — CoAP czeka na CONNECTED_BIT
            // Reset retry po długiej pauzie, żeby spróbować ponownie
            vTaskDelay(pdMS_TO_TICKS(30000));
            _retry_count = 0;
            continue;
        }

        // Próba ponownego połączenia
        _retry_count++;
        ESP_LOGI(TAG, "Reconnect próba %d/%s ...",
                 _retry_count,
                 (_max_retries < 0) ? "∞" : std::to_string(_max_retries).c_str());

        _set_state(State::CONNECTING);
        vTaskDelay(pdMS_TO_TICKS(RECONNECT_DELAY_MS));
        _do_connect();
    }

    ESP_LOGI(TAG, "Task reconnect zakończony.");
}

// ════════════════════════════════════════════════════════════════════════════
// Helpers
// ════════════════════════════════════════════════════════════════════════════

void WiFi_Init::_do_connect() {
    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_connect() błąd: %s", esp_err_to_name(err));
    }
    _set_state(State::CONNECTING);
}

void WiFi_Init::_set_state(State s) {
    if (_state == s) return;
    _state = s;
    if (_on_change) _on_change(s);
}
