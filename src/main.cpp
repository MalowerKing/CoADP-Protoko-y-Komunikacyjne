#include "esp_wifi.h"
#include "MiniCoAP.h"// Twój moduł z folderu lib!

MiniCoAP coapServer;

// Funkcja wywoływana, gdy ktoś zrobi zapytanie do /status
void handle_status_get(struct sockaddr_in *client_addr, coap_packet_t *request) {
    const char *reply = "Wszystko dziala!";
   coapServer.register_resource("sensor", COAP_METHOD_GET, moj_handler);
    coapServer.start();
}

// W kodzie inicjującym Wi-Fi:
if (mamy_polaczenie_wifi) {
    mini_coap_register_resource("status", COAP_METHOD_GET, handle_status_get);
    mini_coap_server_start();
}
