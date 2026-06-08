#ifndef MINI_COAP_H
#define MINI_COAP_H

#include <stdint.h>
#include <stddef.h>
#include <lwip/sockets.h>

// --- KODY ZAPYTAŃ I ODPOWIEDZI ---
typedef enum {
    COAP_TYPE_CON = 0,
    COAP_TYPE_NON = 1,
    COAP_TYPE_ACK = 2,
    COAP_TYPE_RST = 3
} coap_msg_type_t;

typedef enum {
    COAP_METHOD_GET    = 1,
    COAP_METHOD_POST   = 2,
    COAP_METHOD_PUT    = 3,
    COAP_METHOD_DELETE = 4,
    COAP_METHOD_FETCH  = 5,
    COAP_METHOD_PATCH  = 6,
    COAP_METHOD_IPATCH = 7
} coap_method_t;

// --- GŁÓWNA STRUKTURA PAKIETU ---
typedef struct {
    uint8_t version;
    coap_msg_type_t type;
    uint8_t token_len;
    uint8_t code;
    uint16_t message_id;
    
    uint8_t token[8];
    
    const uint8_t *options;
    size_t options_len;
    
    const uint8_t *payload;
    size_t payload_len;
} coap_packet_t;

// --- DEFINICJA HANDLERA (Funkcji obsługującej dany URL) ---
// Taki handler zostanie wywołany, gdy ktoś zapyta o konkretny zasób
typedef void (*coap_resource_handler_t)(struct sockaddr_in *client_addr, coap_packet_t *request);

// --- FUNKCJE API (MVP) ---

// 1. Uruchamia serwer w osobnym zadaniu FreeRTOS
void mini_coap_server_start(void);

// 2. Rejestruje ścieżkę (np. "led") i podpina pod nią funkcję
void mini_coap_register_resource(const char *uri_path, coap_method_t method, coap_resource_handler_t handler);

// 3. Parsowanie i Serializacja
int mini_coap_parse_pdu(const uint8_t *buffer, size_t buffer_len, coap_packet_t *packet);
int mini_coap_serialize_pdu(const coap_packet_t *packet, uint8_t *buffer, size_t *buffer_len);

// 4. Wysyłanie odpowiedzi do klienta (z wnętrza handlera)
void mini_coap_send_response(struct sockaddr_in *client_addr, coap_packet_t *request, uint8_t response_code, const uint8_t *payload, size_t payload_len);

void mini_coap_route_request(coap_packet_t *request, struct sockaddr_in *client_addr);

#endif // MINI_COAP_H
