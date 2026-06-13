#ifndef MINICOAP_H
#define MINICOAP_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <lwip/sockets.h>
#include <esp_log.h> // Przydaje się do ESP_LOGE / ESP_LOGI w pliku .cpp

#define COAP_RESPONSE_205  0x45
#define COAP_RESPONSE_404  0x84
#define COAP_RESPONSE_400  0x80

#define MAX_RESOURCES 10 // Limit zarejestrowanych ścieżek

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


// --- 1. NAJPIERW: Deklaracja typu handlera ---
typedef void (*coap_resource_handler_t)(struct sockaddr_in *client_addr, coap_packet_t *request);

// --- 2. POTEM: Struktura zasobu (korzysta z handlera) ---
typedef struct {
    const char *uri_path;
    coap_method_t method;
    coap_resource_handler_t handler;
} coap_resource_t;// ==========================================
// KLASA MiniCoAP
// ==========================================
class MiniCoAP {
private:
    int server_socket;
    uint16_t port;
    const char* TAG = "MINI_COAP";

    // Pętla główna serwera (metoda obiektu)
    void server_task();

    // Statyczna funkcja-trampolina dla FreeRTOS
    static void task_trampoline(void* _this);
coap_resource_t resources[MAX_RESOURCES];
    uint8_t resource_count = 0;
        

public:
    // Konstruktor
    MiniCoAP(uint16_t listen_port = 5683);
    // Destruktor
    ~MiniCoAP();

    // Metody API
    void start();
    
    int parse_pdu(const uint8_t *buffer, size_t buffer_len, coap_packet_t *packet);
    int serialize_pdu(const coap_packet_t *packet, uint8_t *buffer, size_t *buffer_len);
    
void register_resource(const char *uri_path, coap_method_t method, coap_resource_handler_t handler);
void route_request(coap_packet_t *request, struct sockaddr_in *client_addr);
    void send_response(struct sockaddr_in *client_addr, coap_packet_t *request, uint8_t response_code, const uint8_t *payload, size_t payload_len);
};

#endif // MINICOAP_H
