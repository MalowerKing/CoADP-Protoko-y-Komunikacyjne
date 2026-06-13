#include <unity.h>
#include <string.h>
#include "CoAP.h" // Załączamy nagłówek naszej biblioteki z klasą MiniCoAP

// Unity wymaga tych dwóch funkcji, wywołują się przed i po każdym teście
void setUp(void) {
    // Tutaj nie musimy nic przygotowywać
}

void tearDown(void) {
    // Tutaj nie musimy nic sprzątać
}

/* =========================================================
 * SEKCJA 1: Parsowanie – przypadki podstawowe i błędy
 * ========================================================= */

// --- TEST 1: Odrzucanie zbyt krótkich pakietów ---
void test_coap_parse_too_short(void) {
    uint8_t buffer[] = {0x40, 0x01, 0x12}; // Tylko 3 bajty (brakuje jednego do nagłówka)
    coap_packet_t packet;
    MiniCoAP coap;
    
    int result = coap.parse_pdu(buffer, sizeof(buffer), &packet);
    
    TEST_ASSERT_EQUAL_INT(-1, result); // Oczekujemy błędu (-1)
}

// --- TEST 2: Parsowanie podstawowego nagłówka (GET, bez opcji, bez payloadu) ---
void test_coap_parse_basic_header(void) {
    uint8_t buffer[] = {0x40, 0x01, 0x12, 0x34};
    coap_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    MiniCoAP coap;

    int result = coap.parse_pdu(buffer, sizeof(buffer), &packet);
    
    TEST_ASSERT_EQUAL_INT(0, result); // Sukces
    TEST_ASSERT_EQUAL_UINT8(1, packet.version);
    TEST_ASSERT_EQUAL_UINT8(COAP_TYPE_CON, packet.type);
    TEST_ASSERT_EQUAL_UINT8(0, packet.token_len);
    TEST_ASSERT_EQUAL_UINT8(COAP_METHOD_GET, packet.code);
    TEST_ASSERT_EQUAL_UINT16(0x1234, packet.message_id);
}

// --- TEST 3: Parsowanie pakietu z Tokenem ---
void test_coap_parse_with_token(void) {
    uint8_t buffer[] = {0x42, 0x02, 0xAB, 0xCD, 0x99, 0x88};
    coap_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    MiniCoAP coap;

    int result = coap.parse_pdu(buffer, sizeof(buffer), &packet);
    
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_UINT8(2, packet.token_len);
    TEST_ASSERT_EQUAL_UINT8(0x99, packet.token[0]);
    TEST_ASSERT_EQUAL_UINT8(0x88, packet.token[1]);
}

// --- TEST 4: Znajdowanie znacznika Payloadu (0xFF) ---
void test_coap_parse_with_payload(void) {
    uint8_t buffer[] = {0x40, 0x45, 0x00, 0x01, 0xFF, 'H', 'i'};
    coap_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    MiniCoAP coap;

    int result = coap.parse_pdu(buffer, sizeof(buffer), &packet);
    
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_INT(2, packet.payload_len);
    TEST_ASSERT_NOT_NULL(packet.payload);
    TEST_ASSERT_EQUAL_UINT8('H', packet.payload[0]);
    TEST_ASSERT_EQUAL_UINT8('i', packet.payload[1]);
}

// --- TEST 8: NULL-owy bufor wejściowy ---
void test_coap_parse_null_buffer(void) {
    coap_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    MiniCoAP coap;
 
    int result = coap.parse_pdu(NULL, 4, &packet);
 
    TEST_ASSERT_EQUAL_INT(-1, result);
}
 
// --- TEST 9: NULL-owy wskaźnik struktury wyjściowej ---
void test_coap_parse_null_packet(void) {
    uint8_t buffer[] = {0x40, 0x01, 0x12, 0x34};
    MiniCoAP coap;
 
    int result = coap.parse_pdu(buffer, sizeof(buffer), NULL);
 
    TEST_ASSERT_EQUAL_INT(-1, result);
}
 
// --- TEST 10: Nielegalna wartość TKL = 9 ---
void test_coap_parse_illegal_tkl(void) {
    uint8_t buffer[] = {
        0x49, 0x01, 0xAB, 0xCD,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09
    };
    coap_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    MiniCoAP coap;
 
    int result = coap.parse_pdu(buffer, sizeof(buffer), &packet);
 
    TEST_ASSERT_EQUAL_INT(-1, result);
}
 
// --- TEST 11: Maksymalna legalna wartość TKL = 8 ---
void test_coap_parse_max_tkl(void) {
    uint8_t buffer[] = {
        0x48, 0x01, 0x00, 0x01,
        0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04
    };
    coap_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    MiniCoAP coap;
 
    int result = coap.parse_pdu(buffer, sizeof(buffer), &packet);
 
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_UINT8(8, packet.token_len);
    TEST_ASSERT_EQUAL_UINT8(0xDE, packet.token[0]);
    TEST_ASSERT_EQUAL_UINT8(0x04, packet.token[7]);
}
 
// --- TEST 12: TKL deklaruje więcej bajtów niż jest dostępnych ---
void test_coap_parse_buffer_too_short_for_token(void) {
    uint8_t buffer[] = {0x42, 0x01, 0x00, 0x01, 0x99};
    coap_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    MiniCoAP coap;
 
    int result = coap.parse_pdu(buffer, sizeof(buffer), &packet);
 
    TEST_ASSERT_EQUAL_INT(-1, result);
}
 
// --- TEST 13: Znacznik payload (0xFF) na końcu pakietu – bez danych ---
void test_coap_parse_payload_marker_without_data(void) {
    uint8_t buffer[] = {0x40, 0x01, 0x00, 0x01, 0xFF};
    coap_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    MiniCoAP coap;
 
    int result = coap.parse_pdu(buffer, sizeof(buffer), &packet);
 
    if (result == 0) {
        TEST_ASSERT_EQUAL_INT(0, packet.payload_len);
    }
}

/* =========================================================
 * SEKCJA 2: Serializacja – przypadki podstawowe i błędy
 * ========================================================= */

// --- TEST 5: Serializacja podstawowego nagłówka ---
void test_coap_serialize_basic_header(void) {
    coap_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    MiniCoAP coap;
    
    packet.version = 1;
    packet.type = COAP_TYPE_ACK; 
    packet.token_len = 0;
    packet.code = 69; // 2.05 Content
    packet.message_id = 0xABCD;

    uint8_t buffer[128];
    size_t len = 0; 
    int result = coap.serialize_pdu(&packet, buffer, &len);

    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_INT(4, len); 
    
    TEST_ASSERT_EQUAL_HEX8(0x60, buffer[0]); 
    TEST_ASSERT_EQUAL_HEX8(69, buffer[1]);   
    TEST_ASSERT_EQUAL_HEX8(0xAB, buffer[2]); 
    TEST_ASSERT_EQUAL_HEX8(0xCD, buffer[3]);
}

// --- TEST 6: Serializacja z Payloadem i Tokenem ---
void test_coap_serialize_with_payload(void) {
    coap_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    MiniCoAP coap;
    
    packet.version = 1;
    packet.type = COAP_TYPE_CON;
    packet.code = COAP_METHOD_POST;
    packet.message_id = 0x0001;
    
    packet.token_len = 2;
    packet.token[0] = 0x11;
    packet.token[1] = 0x22;
    
    const char *my_data = "OK";
    packet.payload = (const uint8_t *)my_data;
    packet.payload_len = 2;

    uint8_t buffer[128];
    size_t len = 0;
    int result = coap.serialize_pdu(&packet, buffer, &len);

    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_INT(9, len); 
    TEST_ASSERT_EQUAL_HEX8(0x42, buffer[0]);
    TEST_ASSERT_EQUAL_HEX8(0x11, buffer[4]); 
    TEST_ASSERT_EQUAL_HEX8(0x22, buffer[5]); 
    TEST_ASSERT_EQUAL_HEX8(0xFF, buffer[6]); 
    TEST_ASSERT_EQUAL_HEX8('O', buffer[7]);
    TEST_ASSERT_EQUAL_HEX8('K', buffer[8]);
}

void test_coap_serialize_buffer_too_small(void) {
    coap_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    MiniCoAP coap;

    packet.version    = 1;
    packet.type       = COAP_TYPE_CON;
    packet.code       = COAP_METHOD_POST;
    packet.message_id = 0x0001;
    packet.token_len  = 2;
    packet.token[0]   = 0x11;
    packet.token[1]   = 0x22;

    const char *data  = "OK";
    packet.payload     = (const uint8_t *)data;
    packet.payload_len = 2;

    uint8_t small_buffer[5];
    size_t len = sizeof(small_buffer);  // ← FIX: was 0, must be 5

    int result = coap.serialize_pdu(&packet, small_buffer, &len);

    TEST_ASSERT_EQUAL_INT(-1, result);  // required=9 > capacity=5 → -1 ✓
}
 
// --- TEST 15: NULL-owe argumenty serializacji ---
void test_coap_serialize_null_args(void) {
    coap_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    packet.version = 1;
    MiniCoAP coap;
 
    uint8_t buffer[64];
    size_t len = 0;
 
    TEST_ASSERT_EQUAL_INT(-1, coap.serialize_pdu(NULL, buffer, &len));
    TEST_ASSERT_EQUAL_INT(-1, coap.serialize_pdu(&packet, NULL, &len));
}

/* =========================================================
 * SEKCJA 3: Round-trip (serialize → parse)
 * ========================================================= */
 
// --- TEST 16: Integracja: serialize → parse → weryfikacja pól ---
void test_coap_round_trip(void) {
    coap_packet_t original;
    memset(&original, 0, sizeof(original));
    MiniCoAP coap;
 
    original.version    = 1;
    original.type       = COAP_TYPE_NON;  
    original.code       = COAP_METHOD_PUT;
    original.message_id = 0xBEEF;
    original.token_len  = 4;
    original.token[0]   = 0xCA;
    original.token[1]   = 0xFE;
    original.token[2]   = 0xBA;
    original.token[3]   = 0xBE;
 
    const char *data   = "hello";
    original.payload     = (const uint8_t *)data;
    original.payload_len = 5;
 
    uint8_t wire[128];
    size_t wire_len = 0;
    int ser_result = coap.serialize_pdu(&original, wire, &wire_len);
    TEST_ASSERT_EQUAL_INT(0, ser_result);
    TEST_ASSERT_EQUAL_INT(14, (int)wire_len);
 
    coap_packet_t parsed;
    memset(&parsed, 0, sizeof(parsed));
    int par_result = coap.parse_pdu(wire, wire_len, &parsed);
    TEST_ASSERT_EQUAL_INT(0, par_result);
 
    TEST_ASSERT_EQUAL_UINT8(original.version,    parsed.version);
    TEST_ASSERT_EQUAL_UINT8(original.type,       parsed.type);
    TEST_ASSERT_EQUAL_UINT8(original.code,       parsed.code);
    TEST_ASSERT_EQUAL_UINT16(original.message_id, parsed.message_id);
    TEST_ASSERT_EQUAL_UINT8(original.token_len,  parsed.token_len);
    TEST_ASSERT_EQUAL_UINT8(0xCA, parsed.token[0]);
    TEST_ASSERT_EQUAL_UINT8(0xBE, parsed.token[3]);
    TEST_ASSERT_EQUAL_INT(5, parsed.payload_len);
    TEST_ASSERT_EQUAL_UINT8('h', parsed.payload[0]);
    TEST_ASSERT_EQUAL_UINT8('o', parsed.payload[4]);
}

/* =========================================================
 * SEKCJA 4: Routing – przypadki pozytywowe i negatywne
 * ========================================================= */

static bool dummy_handler_called = false;
static bool wrong_handler_called = false;

void dummy_sensor_handler(struct sockaddr_in *client_addr, coap_packet_t *request) {
    dummy_handler_called = true;
}
 
void wrong_resource_handler(struct sockaddr_in *addr, coap_packet_t *req) {
    wrong_handler_called = true;
}
 
// --- TEST 7: Rejestracja i poprawny routing ---
void test_coap_routing_success(void) {
    dummy_handler_called = false;
    MiniCoAP coap;

    coap.register_resource("sensor", COAP_METHOD_GET, dummy_sensor_handler);

    coap_packet_t request;
    memset(&request, 0, sizeof(request));
    request.code = COAP_METHOD_GET;
    request.options = (const uint8_t *)"sensor"; 
    request.options_len = 6;
    struct sockaddr_in dummy_addr; 

    coap.route_request(&request, &dummy_addr);
    
    TEST_ASSERT_TRUE(dummy_handler_called);
}

// --- TEST 17: Routing do nieistniejącej ścieżki ---
void test_coap_routing_unknown_path(void) {
    wrong_handler_called = false;
    MiniCoAP coap;
    coap.register_resource("temperature", COAP_METHOD_GET, wrong_resource_handler);
 
    coap_packet_t request;
    memset(&request, 0, sizeof(request));
    request.code        = COAP_METHOD_GET;
    request.options     = (const uint8_t *)"humidity";
    request.options_len = 8;
 
    struct sockaddr_in dummy_addr;
    coap.route_request(&request, &dummy_addr);
 
    TEST_ASSERT_FALSE(wrong_handler_called);
}
 
// --- TEST 18: Routing – poprawna ścieżka, zła metoda ---
void test_coap_routing_wrong_method(void) {
    wrong_handler_called = false;
    MiniCoAP coap;
    coap.register_resource("led", COAP_METHOD_POST, wrong_resource_handler);
 
    coap_packet_t request;
    memset(&request, 0, sizeof(request));
    request.code        = COAP_METHOD_GET;  
    request.options     = (const uint8_t *)"led";
    request.options_len = 3;
 
    struct sockaddr_in dummy_addr;
    coap.route_request(&request, &dummy_addr);
 
    TEST_ASSERT_FALSE(wrong_handler_called);
}
 
// --- TEST 19: Routing z NULL-owym żądaniem ---
void test_coap_routing_null_request(void) {
    wrong_handler_called = false;
    MiniCoAP coap;
    struct sockaddr_in dummy_addr;
 
    coap.route_request(NULL, &dummy_addr);
 
    TEST_ASSERT_FALSE(wrong_handler_called);
}

// --- GŁÓWNA FUNKCJA TESTOWA FREERTOS/ESP-IDF ---
extern "C" void app_main() {
    UNITY_BEGIN(); 
    
    // Parsowanie podstawowe
    RUN_TEST(test_coap_parse_too_short);
    RUN_TEST(test_coap_parse_basic_header);
    RUN_TEST(test_coap_parse_with_token);
    RUN_TEST(test_coap_parse_with_payload);
    
    // Parsowanie – błędy i krawędzie
    RUN_TEST(test_coap_parse_null_buffer);
    RUN_TEST(test_coap_parse_null_packet);
    RUN_TEST(test_coap_parse_illegal_tkl);
    RUN_TEST(test_coap_parse_max_tkl);
    RUN_TEST(test_coap_parse_buffer_too_short_for_token);
    RUN_TEST(test_coap_parse_payload_marker_without_data);
 
    // Serializacja
    RUN_TEST(test_coap_serialize_basic_header);
    RUN_TEST(test_coap_serialize_with_payload);
    RUN_TEST(test_coap_serialize_buffer_too_small);
    RUN_TEST(test_coap_serialize_null_args);
 
    // Integracja (Round-trip)
    RUN_TEST(test_coap_round_trip);
 
    // Routing
    RUN_TEST(test_coap_routing_success);
    RUN_TEST(test_coap_routing_unknown_path);
    RUN_TEST(test_coap_routing_wrong_method);
    RUN_TEST(test_coap_routing_null_request);
    
    UNITY_END(); 
}
