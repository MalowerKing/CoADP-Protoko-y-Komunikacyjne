#pragma once

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <string>

// Makra wstrzykiwane przez build system z secrets.ini
#ifndef WIFI_SSID
  #error "WIFI_SSID nie zdefiniowane! Sprawdz secrets.ini i platformio.ini"
#endif
#ifndef WIFI_PASS
  #error "WIFI_PASS nie zdefiniowane! Sprawdz secrets.ini i platformio.ini"
#endif

// Bity zdarzeń (EventGroup)
#define WIFI_CONNECTED_BIT   BIT0
#define WIFI_FAIL_BIT        BIT1

class WiFi_Init {
public:
    // Stan połączenia
    enum class State {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        FAILED
    };

    // Callback wołany po zmianie stanu (opcjonalny)
    using StateCallback = void (*)(State state);

    /**
     * @param ssid         SSID sieci (domyślnie z secrets.ini)
     * @param password     Hasło sieci (domyślnie z secrets.ini)
     * @param max_retries  Ile razy próbować przed FAILED (-1 = nieskończenie)
     * @param on_change    Opcjonalny callback na zmianę stanu
     */
    WiFi_Init(const char* ssid     = WIFI_SSID,
              const char* password = WIFI_PASS,
              int         max_retries = -1,
              StateCallback on_change = nullptr);

    ~WiFi_Init();

    /**
     * Inicjalizuje stos TCP/IP, rejestruje handlery zdarzeń
     * i uruchamia task FreeRTOS zarządzający połączeniem.
     * Nieblokująca — natychmiast zwraca sterowanie.
     */
    void begin();

    /**
     * Blokuje wywołujący task do momentu połączenia lub błędu.
     * @param timeout_ms  Maksymalny czas oczekiwania (0 = bez limitu)
     * @return true jeśli połączono, false przy timeout/błędzie
     */
    bool wait_for_connection(uint32_t timeout_ms = 0);

    State       get_state()   const { return _state; }
    std::string get_ip()      const { return _ip_str; }
    bool        is_connected() const { return _state == State::CONNECTED; }

    // Ręczny disconnect + zatrzymanie taska reconnect
    void stop();

private:
    static constexpr const char* TAG = "WiFi_Init";
    static constexpr uint32_t    RECONNECT_DELAY_MS = 5000;
    static constexpr uint32_t    TASK_STACK_SIZE     = 4096;
    static constexpr UBaseType_t TASK_PRIORITY       = 5;

    const char*   _ssid;
    const char*   _password;
    int           _max_retries;
    StateCallback _on_change;

    volatile State _state       = State::DISCONNECTED;
    std::string    _ip_str;
    int            _retry_count = 0;
    bool           _stop_flag   = false;

    EventGroupHandle_t _event_group = nullptr;
    TaskHandle_t       _task_handle = nullptr;

    // ── Statyczne handlery zdarzeń ESP-IDF ──────────────────────────────
    static void _event_handler(void* arg,
                                esp_event_base_t event_base,
                                int32_t          event_id,
                                void*            event_data);

    // ── Task FreeRTOS (reconnect loop) ──────────────────────────────────
    static void _reconnect_task(void* pvParam);
    void        _reconnect_loop();   // właściwa logika taska

    void _set_state(State s);
    void _do_connect();
};
