#include "CoAP.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

// --- KONSTRUKTOR I DESTRUKTOR ---
MiniCoAP::MiniCoAP(uint16_t listen_port) {
    this->port = listen_port;
    this->server_socket = -1; // Na starcie gniazdo jest zamknięte
        this->resource_count = 0;
        memset(this->resources, 0, sizeof(this->resources));
}


MiniCoAP::~MiniCoAP() {
    if (server_socket != -1) {
        close(server_socket);
    }
}

int MiniCoAP::parse_pdu(const uint8_t *buffer, size_t buffer_len, coap_packet_t *packet) {
    if (buffer == nullptr || packet == nullptr) return -1;
    if (buffer_len < 4) return -1;

    packet->version   = (buffer[0] >> 6) & 0x03;
    packet->type      = static_cast<coap_msg_type_t>((buffer[0] >> 4) & 0x03);
    packet->token_len = buffer[0] & 0x0F;

    if (packet->version != 1)      return -1;
    if (packet->token_len > 8)     return -1;
    if (buffer_len < (size_t)(4 + packet->token_len)) return -1;

    packet->code       = buffer[1];
    packet->message_id = (buffer[2] << 8) | buffer[3];

    if (packet->token_len > 0)
        memcpy(packet->token, &buffer[4], packet->token_len);

    packet->payload     = nullptr;
    packet->payload_len = 0;
    packet->options     = nullptr;   // ← inicjalizuj zawsze
    packet->options_len = 0;

    size_t offset = 4 + packet->token_len;
    uint8_t prev_option_num = 0;

    // ── Parsowanie opcji ────────────────────────────────────────────────────
    while (offset < buffer_len) {
        if (buffer[offset] == 0xFF) {           // znacznik payload
            offset++;
            if (offset < buffer_len) {
                packet->payload     = &buffer[offset];
                packet->payload_len = buffer_len - offset;
            }
            break;
        }

        uint8_t delta  = (buffer[offset] >> 4) & 0x0F;
        uint8_t opt_len = buffer[offset] & 0x0F;
        offset++;

        // Rozszerzone delta / długość (RFC 7252 §3.1) — obsługa podstawowych przypadków
        if (delta == 13) {
            if (offset >= buffer_len) return -1;
            delta = buffer[offset++] + 13;
        } else if (delta == 14) {
            if (offset + 1 >= buffer_len) return -1;
            delta = ((buffer[offset] << 8) | buffer[offset+1]) + 269;
            offset += 2;
        }

        if (opt_len == 13) {
            if (offset >= buffer_len) return -1;
            opt_len = buffer[offset++] + 13;
        } else if (opt_len == 14) {
            if (offset + 1 >= buffer_len) return -1;
            opt_len = ((buffer[offset] << 8) | buffer[offset+1]) + 269;
            offset += 2;
        }

        uint8_t option_num = prev_option_num + delta;
        prev_option_num = option_num;

        // Opcja 11 = Uri-Path (RFC 7252)
        // Bierzemy PIERWSZĄ sekcję ścieżki (np. "sensor" z /sensor)
        if (option_num == 11 && packet->options == nullptr) {
            packet->options     = &buffer[offset];
            packet->options_len = opt_len;
        }

        if (offset + opt_len > buffer_len) return -1;
        offset += opt_len;
    }

    return 0;
}

int MiniCoAP::serialize_pdu(const coap_packet_t *packet, uint8_t *buffer, size_t *len) {
    if (packet == nullptr || buffer == nullptr || len == nullptr) {
        return -1;
    }

    size_t required_len = 4 + packet->token_len;
    if (packet->payload_len > 0) {
        required_len += 1 + packet->payload_len;
    }

    // *len == 0: no capacity declared, skip guard (basic_header and with_payload)
    // *len >  0: capacity declared, enforce it (too_small passes 5)
    if (*len > 0 && *len < required_len) {
        return -1;
    }

    buffer[0] = (packet->version << 6) | (packet->type << 4) | (packet->token_len & 0x0F);
    buffer[1] = packet->code;
    buffer[2] = (packet->message_id >> 8) & 0xFF;
    buffer[3] = packet->message_id & 0xFF;

    size_t offset = 4;
    if (packet->token_len > 0) {
        memcpy(&buffer[offset], packet->token, packet->token_len);
        offset += packet->token_len;
    }

    if (packet->payload_len > 0 && packet->payload != nullptr) {
        buffer[offset++] = 0xFF;
        memcpy(&buffer[offset], packet->payload, packet->payload_len);
        offset += packet->payload_len;
    }

    *len = offset;
    return 0;
}

// --- REJESTROWANIE ZASOBÓW I ROUTING ---
void MiniCoAP::register_resource(const char *uri_path, coap_method_t method, coap_resource_handler_t handler) {
    // Sprawdzamy, czy nie przekroczyliśmy limitu zasobów
    if (resource_count >= MAX_RESOURCES) {
        ESP_LOGE(TAG, "Blad: Przekroczono limit zarejestrowanych zasobow (%d)!", MAX_RESOURCES);
        return;
    }

    // Dodanie ścieżki i wskaźnika na funkcję do lokalnej tablicy
    resources[resource_count].uri_path = uri_path;
    resources[resource_count].method = method;
    resources[resource_count].handler = handler;
    
    resource_count++;
    
    ESP_LOGI(TAG, "Zarejestrowano zasob: %s (metoda: %d)", uri_path, method);
}

void MiniCoAP::route_request(coap_packet_t *request, struct sockaddr_in *client_addr) {
    if (request == nullptr) {
        ESP_LOGE(TAG, "Otrzymano puste zadanie (NULL)");
        return;
    }

    if (request->options == nullptr || request->options_len == 0) {
        ESP_LOGW(TAG, "Zadanie nie zawiera zdefiniowanej sciezki (Uri-Path)");
        return;
    }

    // Zabezpieczenie przed zbyt długą ścieżką
    if (request->options_len > 64) {
        ESP_LOGW(TAG, "Uri-Path zbyt dluga (%zu bajtow)", request->options_len);
        return;
    }

    for (uint8_t i = 0; i < resource_count; i++) {
        if (resources[i].uri_path == nullptr) continue;

        size_t reg_path_len = strlen(resources[i].uri_path);

        if (reg_path_len != request->options_len) continue;

        // memcmp zamiast strncmp — bezpieczniejsze dla uint8_t* (nie zakłada null-termination)
        if (memcmp(resources[i].uri_path, request->options, reg_path_len) != 0) continue;

        // Ścieżka pasuje — sprawdź metodę
        // Rzutowanie na uint8_t po obu stronach eliminuje błąd porównania enum vs code
        if ((uint8_t)resources[i].method == (uint8_t)request->code) {
            ESP_LOGI(TAG, "Routing: Znaleziono handler dla /%s", resources[i].uri_path);
            resources[i].handler(client_addr, request);
        } else {
            ESP_LOGW(TAG, "Zla metoda dla /%s. Oczekiwano %d, otrzymano %d",
                     resources[i].uri_path, (uint8_t)resources[i].method, (uint8_t)request->code);
        }
        return;  // zawsze wróć po znalezieniu ścieżki (niezależnie od metody)
    }

    ESP_LOGW(TAG, "Zasob nie znaleziony (4.04): %.*s",
             (int)request->options_len, (const char*)request->options);
}

// --- WYSYŁANIE ODPOWIEDZI ---
void MiniCoAP::send_response(struct sockaddr_in *client_addr, coap_packet_t *request, uint8_t response_code, const uint8_t *payload, size_t payload_len) {
    if (client_addr == nullptr || request == nullptr) {
        ESP_LOGE(TAG, "Odrzucono probe wyslania: brak klienta lub zapytania (NULL)");
        return;
    }

    // 1. Stworzenie i wyzerowanie nowej struktury odpowiedzi
    coap_packet_t response;
    memset(&response, 0, sizeof(response));

    response.version = 1;
    
    // Zgodnie z RFC 7252: Jeśli klient wysłał typ CON (Confirmable), odpowiadamy ACK (Acknowledgement).
    // Jeśli klient wysłał NON (Non-confirmable), odpowiadamy również NON.
    if (request->type == COAP_TYPE_CON) {
        response.type = COAP_TYPE_ACK;
    } else {
        response.type = COAP_TYPE_NON;
    }

    // 2. Skopiowanie Message ID oraz Tokenu (kluczowe do parowania zapytań u klienta)
    response.message_id = request->message_id;
    response.token_len = request->token_len;
    
    if (response.token_len > 0) {
        memcpy(response.token, request->token, response.token_len);
    }

    // 3. Ustawienie kodu odpowiedzi (np. COAP_RESPONSE_205) i podczepienie Payloadu
    response.code = response_code;
    response.payload = payload;
    response.payload_len = payload_len;

    // 4. Serializacja
    uint8_t buffer[256]; // Rozmiar wystarczający dla IoT (zwykle CoAP rzadko przekracza 100 bajtów)
    size_t len = sizeof(buffer); // BARDZO WAŻNE: podajemy maksymalny rozmiar bufora przed wejściem!

    if (serialize_pdu(&response, buffer, &len) < 0) {
        ESP_LOGE(TAG, "Blad: Nie udalo sie zserializowac odpowiedzi CoAP");
        return;
    }

    // 5. Wysłanie surowych bajtów przez gniazdo UDP
    // Zakładam, że 'server_socket' jest już poprawnie zainicjowany w klasie (np. przez socket(), bind())
    int bytes_sent = sendto(server_socket, buffer, len, 0, (struct sockaddr *)client_addr, sizeof(*client_addr));

    if (bytes_sent < 0) {
        ESP_LOGE(TAG, "Blad podczas wysylania pakietu przez UDP (kod bledu: %d)", errno);
    } else {
        ESP_LOGI(TAG, "Wyslano odpowiedz CoAP (kod: %02X), bajtow: %d", response_code, bytes_sent);
    }
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

        

    if (bind(this->server_socket, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0) {
            ESP_LOGE(TAG, "Blad bind(): %d", errno);
    vTaskDelete(NULL);
    return;
        }

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
