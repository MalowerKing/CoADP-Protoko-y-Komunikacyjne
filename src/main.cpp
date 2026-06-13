#include "WiFi_Init.h"
#include "CoAP.h"

static WiFi_Init wifi;
static MiniCoAP  coapServer;

// ── Handlery CoAP ────────────────────────────────────────────────────────────

void handle_status_get(struct sockaddr_in *client_addr, coap_packet_t *request) {
    const char *msg = "OK";
    coapServer.send_response(client_addr, request,
                             COAP_RESPONSE_205,
                             reinterpret_cast<const uint8_t*>(msg), strlen(msg));
}

void handle_sensor_get(struct sockaddr_in *client_addr, coap_packet_t *request) {
    const char *msg = "42";  // TODO: wstaw tu odczyt czujnika
    coapServer.send_response(client_addr, request,
                             COAP_RESPONSE_205,
                             reinterpret_cast<const uint8_t*>(msg), strlen(msg));
}

void handle_echo_get(struct sockaddr_in *client_addr, coap_packet_t *request) {
    if (request->payload != nullptr && request->payload_len > 0) {
        // Odsyła dokładnie to co przyszło
        coapServer.send_response(client_addr, request,
                                 COAP_RESPONSE_205,
                                 request->payload, request->payload_len);
    } else {
        const char *msg = "brak payload";
        coapServer.send_response(client_addr, request,
                                 COAP_RESPONSE_205,
                                 reinterpret_cast<const uint8_t*>(msg), strlen(msg));
    }
}

// ── Task CoAP ─────────────────────────────────────────────────────────────────

void coap_setup_task(void *) {
    wifi.wait_for_connection();
    coapServer.register_resource("status", COAP_METHOD_GET, handle_status_get);
    coapServer.register_resource("sensor", COAP_METHOD_GET, handle_sensor_get);
    coapServer.register_resource("echo",   COAP_METHOD_GET, handle_echo_get);  // ← dodaj
    coapServer.start();
    vTaskDelete(nullptr);
}

// ── Punkt wejścia ─────────────────────────────────────────────────────────────

extern "C" void app_main() {
    wifi.begin();
    xTaskCreate(coap_setup_task, "coap_setup", 4096, nullptr, 5, nullptr);
}
