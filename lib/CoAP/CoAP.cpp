#include "CoAP.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

// --- KONSTRUKTOR I DESTRUKTOR ---
MiniCoAP::MiniCoAP(uint16_t listen_port) {
    this->port = listen_port;
    this->server_socket = -1; // Na starcie gniazdo jest zamknięte
}

MiniCoAP::~MiniCoAP() {
    if (server_socket != -1) {
        close(server_socket);
    }
}

// --- PARSOWANIE PAKIETU ---
int MiniCoAP::parse_pdu(const uint8_t *buffer, size_t buffer_len, coap_packet_t *packet) {
    if (buffer_len < 4) {
        ESP_LOGE(TAG, "Pakiet za krotki na naglowek CoAP");
        return -1;
    }

    // TODO: Wyłuskanie (bit-shifting) Version, Type, Token Length z buffer[0]
    // TODO: Zapisanie Code z buffer[1]
    // TODO: Odczytanie Message ID (Big Endian) z buffer[2] i buffer[3]
    // TODO: Skopiowanie Tokenu
    // TODO: Odszukanie poczatku Payloadu (znacznik 0xFF)

    return 0; // Sukces
}

// --- SERIALIZACJA PAKIETU ---
int MiniCoAP::serialize_pdu(const coap_packet_t *packet, uint8_t *buffer, size_t *buffer_len) {
    // TODO: Złożenie pierwszych 4 bajtów z pomocą przesunięć bitowych (<< i |)
    // TODO: Doklejenie Tokenu
    // TODO: Doklejenie ew. Opcji
    // TODO: Doklejenie znacznika 0xFF i Payloadu
    // TODO: Zaktualizowanie zmiennej buffer_len
    
    return 0;
}

// --- REJESTROWANIE ZASOBÓW I ROUTING ---
void MiniCoAP::register_resource(const char *uri_path, coap_method_t method, coap_resource_handler_t handler) {
    // TODO: Dodanie ścieżki i wskaźnika na funkcję do lokalnej tablicy/listy
    ESP_LOGI(TAG, "Zarejestrowano zasob: %s", uri_path);
}

void MiniCoAP::route_request(coap_packet_t *request, struct sockaddr_in *client_addr) {
    // TODO: Odczytanie opcji Uri-Path z pakietu
    // TODO: Porównanie ścieżki z zarejestrowanymi zasobami
    // TODO: Wywołanie odpowiedniego handlera lub odesłanie "4.04 Not Found"
}

// --- WYSYŁANIE ODPOWIEDZI ---
void MiniCoAP::send_response(struct sockaddr_in *client_addr, coap_packet_t *request, uint8_t response_code, const uint8_t *payload, size_t payload_len) {
    // TODO: Stworzenie nowej struktury coap_packet_t (Odpowiedz)
    // TODO: Skopiowanie Message ID oraz Tokenu z requestu
    // TODO: Zmiana typu na ACK i ustawienie kodu np. na 2.05 (Content)
    // TODO: Serializacja
    // TODO: sendto() z uzyciem zmiennej this->server_socket
}

// --- PĘTLA SERWERA UDP (Metoda klasy) ---
void MiniCoAP::server_task() {
    uint8_t rx_buffer[1024];
    struct sockaddr_in dest_addr;

    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(this->port);

    this->server_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (this->server_socket < 0) {
        ESP_LOGE(TAG, "Blad socket()");
        vTaskDelete(NULL);
        return;
    }

    bind(this->server_socket, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    ESP_LOGI(TAG, "Serwer Mini-CoAP dziala na porcie %d", this->port);

    while (1) {
        struct sockaddr_storage source_addr;
        socklen_t socklen = sizeof(source_addr);

        int len = recvfrom(this->server_socket, rx_buffer, sizeof(rx_buffer), 0, 
                           (struct sockaddr *)&source_addr, &socklen);

        if (len > 0) {
            coap_packet_t request;
            if (this->parse_pdu(rx_buffer, len, &request) == 0) {
                this->route_request(&request, (struct sockaddr_in *)&source_addr);
            }
        }
    }
}

// --- TRAMPOLINA FREERTOS ---
// FreeRTOS musi dostać funkcję statyczną. Przekazujemy wskaźnik 'this' w parametrze arg
void MiniCoAP::task_trampoline(void* _this) {
    MiniCoAP* app = static_cast<MiniCoAP*>(_this);
    app->server_task(); // Odpalamy właściwą metodę obiektu
}

// --- START SERWERA ---
void MiniCoAP::start() {
    // Jako parametr pvParameters przekazujemy 'this'
    xTaskCreate(MiniCoAP::task_trampoline, "coap_task", 4096, this, 5, NULL);
}
