#include "esp_wifi.h"
#include "mini_coap.h" // Twój moduł z folderu lib!

// Funkcja wywoływana, gdy ktoś zrobi zapytanie do /status
void handle_status_get(struct sockaddr_in *client_addr, coap_packet_t *request) {
    const char *reply = "Wszystko dziala!";
    mini_coap_send_response(client_addr, request, 69 /* 2.05 Content */, (const uint8_t*)reply, strlen(reply));
}

// W kodzie inicjującym Wi-Fi:
if (mamy_polaczenie_wifi) {
    mini_coap_register_resource("status", COAP_METHOD_GET, handle_status_get);
    mini_coap_server_start();
}
