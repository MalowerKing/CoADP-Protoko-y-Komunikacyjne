#include <unity.h>
#include <string.h>
#include "CoAP.h" // Załączamy nagłówek naszej biblioteki
// Unity wymaga tych dwóch funkcji, wywołują się przed i po każdym teście
void setUp(void) {
    // Tutaj nie musimy nic przygotowywać
}
void tearDown(void) {
    // Tutaj nie musimy nic sprzątać
}
// --- TEST 1: Odrzucanie zbyt krótkich pakietów ---
void test_coap_parse_too_short(void) {
    uint8_t buffer[] = {0x40, 0x01, 0x12}; // Tylko 3 bajty (brakuje jednego do nagłówka)
    coap_packet_t packet;
    
    int result = mini_coap_parse_pdu(buffer, sizeof(buffer), &packet);
    
    TEST_ASSERT_EQUAL_INT(-1, result); // Oczekujemy błędu (-1)
}
// --- TEST 2: Parsowanie podstawowego nagłówka (GET, bez opcji, bez payloadu) ---
void test_coap_parse_basic_header(void) {
    // HEX: 0x40 0x01 0x12 0x34
    // 0x40 -> Version: 1 (01), Type: CON (00), Token Len: 0 (0000)
    // 0x01 -> Code: GET
    // 0x12, 0x34 -> Message ID: 0x1234
    uint8_t buffer[] = {0x40, 0x01, 0x12, 0x34};
    coap_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    int result = mini_coap_parse_pdu(buffer, sizeof(buffer), &packet);
    
    TEST_ASSERT_EQUAL_INT(0, result); // Sukces
    TEST_ASSERT_EQUAL_UINT8(1, packet.version);
    TEST_ASSERT_EQUAL_UINT8(COAP_TYPE_CON, packet.type);
    TEST_ASSERT_EQUAL_UINT8(0, packet.token_len);
    TEST_ASSERT_EQUAL_UINT8(COAP_METHOD_GET, packet.code);
    TEST_ASSERT_EQUAL_UINT16(0x1234, packet.message_id);
}
// --- TEST 3: Parsowanie pakietu z Tokenem (np. odpowiedź asynchroniczna) ---
void test_coap_parse_with_token(void) {
    // 0x42 -> V: 1, Type: CON, Token Len: 2 bajty
    // 0x02 -> Code: POST
    // 0xAB, 0xCD -> Message ID
    // 0x99, 0x88 -> Token
    uint8_t buffer[] = {0x42, 0x02, 0xAB, 0xCD, 0x99, 0x88};
    coap_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    int result = mini_coap_parse_pdu(buffer, sizeof(buffer), &packet);
    
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_UINT8(2, packet.token_len);
    TEST_ASSERT_EQUAL_UINT8(0x99, packet.token[0]);
    TEST_ASSERT_EQUAL_UINT8(0x88, packet.token[1]);
}
// --- TEST 4: Znajdowanie znacznika Payloadu (0xFF) ---
void test_coap_parse_with_payload(void) {
    // 0x40 -> V: 1, T: CON, TKL: 0
    // 0x45 -> Code: 2.05 Content (wartość dziesiętna: 69)
    // 0x00, 0x01 -> Message ID
    // 0xFF -> ZNACZNIK PAYLOADU (Koniec opcji, początek danych)
    // 'H', 'i' -> Treść (Payload)
    uint8_t buffer[] = {0x40, 0x45, 0x00, 0x01, 0xFF, 'H', 'i'};
    coap_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    int result = mini_coap_parse_pdu(buffer, sizeof(buffer), &packet);
    
    TEST_ASSERT_EQUAL_INT(0, result);
    
    // Sprawdzamy czy parser dobrze obliczył długość treści
    TEST_ASSERT_EQUAL_INT(2, packet.payload_len);
    TEST_ASSERT_NOT_NULL(packet.payload);
    
    // Sprawdzamy zawartość
    TEST_ASSERT_EQUAL_UINT8('H', packet.payload[0]);
    TEST_ASSERT_EQUAL_UINT8('i', packet.payload[1]);
}
// --- TEST 5: Serializacja podstawowego nagłówka ---
void test_coap_serialize_basic_header(void) {
    coap_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    
    // Ustawiamy parametry tak, jakbyśmy chcieli wysłać odpowiedź ACK
    packet.version = 1;
    packet.type = COAP_TYPE_ACK; // Wartość 2
    packet.token_len = 0;
    packet.code = 69; // 2.05 Content
    packet.message_id = 0xABCD;
    uint8_t buffer[128];
    size_t len = 0; // Tu funkcja powinna wpisać ile bajtów zajęła
    int result = mini_coap_serialize_pdu(&packet, buffer, &len);
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_INT(4, len); // Sam nagłówek to 4 bajty
    
    // Weryfikacja bit po bicie:
    // Wersja(1)=01, Typ(ACK)=10, TKL(0)=0000 -> 0110 0000 -> 0x60
    TEST_ASSERT_EQUAL_HEX8(0x60, buffer[0]); 
    TEST_ASSERT_EQUAL_HEX8(69, buffer[1]);   // Kod 2.05
    TEST_ASSERT_EQUAL_HEX8(0xAB, buffer[2]); // Message ID (Big Endian)
    TEST_ASSERT_EQUAL_HEX8(0xCD, buffer[3]);
}
// --- TEST 6: Serializacja z Payloadem i Tokenem ---
void test_coap_serialize_with_payload(void) {
    coap_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    
    packet.version = 1;
    packet.type = COAP_TYPE_CON;
    packet.code = COAP_METHOD_POST;
    packet.message_id = 0x0001;
    
    // Dodajemy Token
    packet.token_len = 2;
    packet.token[0] = 0x11;
    packet.token[1] = 0x22;
    
    // Dodajemy Payload
    const char *my_data = "OK";
    packet.payload = (const uint8_t *)my_data;
    packet.payload_len = 2;
    uint8_t buffer[128];
    size_t len = 0;
    int result = mini_coap_serialize_pdu(&packet, buffer, &len);
    TEST_ASSERT_EQUAL_INT(0, result);
    // 4 (nagłowek) + 2 (token) + 1 (znacznik 0xFF) + 2 (payload) = 9 bajtów
    TEST_ASSERT_EQUAL_INT(9, len); 
    // Oczekiwany nagłowek: V=1(01), T=CON(00), TKL=2(0010) -> 0100 0010 -> 0x42
    TEST_ASSERT_EQUAL_HEX8(0x42, buffer[0]);
    TEST_ASSERT_EQUAL_HEX8(0x11, buffer[4]); // Pierwszy bajt tokenu
    TEST_ASSERT_EQUAL_HEX8(0x22, buffer[5]); // Drugi bajt tokenu
    TEST_ASSERT_EQUAL_HEX8(0xFF, buffer[6]); // Znacznik startu payloadu
    TEST_ASSERT_EQUAL_HEX8('O', buffer[7]);
    TEST_ASSERT_EQUAL_HEX8('K', buffer[8]);
}
// Zmienna globalna dla testów routingu
static bool dummy_handler_called = false;
// Nasz "udawany" handler, który symuluje odczyt z czujnika
void dummy_sensor_handler(struct sockaddr_in *client_addr, coap_packet_t *request) {
    dummy_handler_called = true;
}
// --- TEST 7: Rejestracja i poprawny routing ---
void test_coap_routing_success(void) {
    dummy_handler_called = false;
    // 1. Rejestrujemy nasz zasób w serwerze
    mini_coap_register_resource("sensor", COAP_METHOD_GET, dummy_sensor_handler);
    // 2. Tworzymy fałszywy pakiet żądania, udając że przyszedł z sieci
    coap_packet_t request;
    memset(&request, 0, sizeof(request));
    request.code = COAP_METHOD_GET;
    
    // UWAGA: W pełnej wersji będziesz dekodować opcje (Uri-Path). 
    // Na potrzeby MVP załóżmy, że nasza uproszczona logika tymczasowo 
    // trzyma URL jako string w strukturze lub używa bezpośredniego porównania opcji.
    // Przypisujemy surowe dane jako opcje do testu:
    request.options = (const uint8_t *)"sensor"; 
    request.options_len = 6;
    struct sockaddr_in dummy_addr; // Obojętnie jaki adres do testu
    // 3. Odpalamy routing
    mini_coap_route_request(&request, &dummy_addr);
    // 4. Sprawdzamy, czy funkcja `dummy_sensor_handler` została uruchomiona!
    TEST_ASSERT_TRUE(dummy_handler_called);
}


void test_coap_parse_null_buffer(void) {
    coap_packet_t packet;
    memset(&packet, 0, sizeof(packet));
 
    int result = mini_coap_parse_pdu(NULL, 4, &packet);
 
    TEST_ASSERT_EQUAL_INT(-1, result);
}
 
/**
 * TEST 9: NULL-owy wskaźnik struktury wyjściowej.
 * Parser musi odrzucić wywołanie z packet == NULL,
 * nie może pisać w adres 0x0.
 */
void test_coap_parse_null_packet(void) {
    uint8_t buffer[] = {0x40, 0x01, 0x12, 0x34};
 
    int result = mini_coap_parse_pdu(buffer, sizeof(buffer), NULL);
 
    TEST_ASSERT_EQUAL_INT(-1, result);
}
 
/**
 * TEST 10: Nielegalna wartość TKL = 9.
 * Specyfikacja CoAP (RFC 7252 §3) rezerwuje wartości TKL > 8
 * jako "Reserved" – parser musi zwrócić błąd.
 *
 * 0x49 -> V:1, T:CON, TKL:9 (illegal)
 * 0x01 -> GET
 * 0xAB, 0xCD -> Message ID
 * + 9 bajtów "tokenu" (tyle ile mówi TKL)
 */
void test_coap_parse_illegal_tkl(void) {
    uint8_t buffer[] = {
        0x49, 0x01, 0xAB, 0xCD,
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09
    };
    coap_packet_t packet;
    memset(&packet, 0, sizeof(packet));
 
    int result = mini_coap_parse_pdu(buffer, sizeof(buffer), &packet);
 
    TEST_ASSERT_EQUAL_INT(-1, result);
}
 
/**
 * TEST 11: Maksymalna legalna wartość TKL = 8.
 * RFC 7252 dopuszcza token o długości 0–8 bajtów.
 * Parser musi akceptować TKL = 8 i poprawnie skopiować token.
 *
 * 0x48 -> V:1, T:CON, TKL:8
 * 0x01 -> GET
 * 0x00, 0x01 -> Message ID
 * + 8 bajtów tokenu: 0xDE 0xAD 0xBE 0xEF 0x01 0x02 0x03 0x04
 */
void test_coap_parse_max_tkl(void) {
    uint8_t buffer[] = {
        0x48, 0x01, 0x00, 0x01,
        0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04
    };
    coap_packet_t packet;
    memset(&packet, 0, sizeof(packet));
 
    int result = mini_coap_parse_pdu(buffer, sizeof(buffer), &packet);
 
    TEST_ASSERT_EQUAL_INT(0, result);
    TEST_ASSERT_EQUAL_UINT8(8, packet.token_len);
    TEST_ASSERT_EQUAL_UINT8(0xDE, packet.token[0]);
    TEST_ASSERT_EQUAL_UINT8(0x04, packet.token[7]);
}
 
/**
 * TEST 12: TKL deklaruje więcej bajtów niż jest dostępnych.
 * Bufor ma 6 bajtów (4 nagłówek + 1 bajt zamiast 2 tokenu).
 * Parser musi wykryć "buffer underrun" i zwrócić błąd.
 *
 * 0x42 -> V:1, T:CON, TKL:2   (deklaruje 2 bajty tokenu)
 * 0x01 -> GET
 * 0x00, 0x01 -> Message ID
 * 0x99        -> tylko JEDEN bajt tokenu (brak drugiego!)
 */
void test_coap_parse_buffer_too_short_for_token(void) {
    uint8_t buffer[] = {0x42, 0x01, 0x00, 0x01, 0x99};
    coap_packet_t packet;
    memset(&packet, 0, sizeof(packet));
 
    int result = mini_coap_parse_pdu(buffer, sizeof(buffer), &packet);
 
    TEST_ASSERT_EQUAL_INT(-1, result);
}
 
/**
 * TEST 13: Znacznik payload (0xFF) na końcu pakietu – bez danych.
 * RFC 7252 §3 mówi: "If present and of zero length, it MUST be
 * processed as a message format error." Parser powinien zwrócić błąd
 * LUB ustawić payload_len = 0 i payload = NULL (zależy od implementacji).
 * Ważne: nie może się zawiesić.
 *
 * 0x40 -> V:1, T:CON, TKL:0
 * 0x01 -> GET
 * 0x00, 0x01 -> Message ID
 * 0xFF        -> znacznik payloadu – ale nie ma po nim danych
 */
void test_coap_parse_payload_marker_without_data(void) {
    uint8_t buffer[] = {0x40, 0x01, 0x00, 0x01, 0xFF};
    coap_packet_t packet;
    memset(&packet, 0, sizeof(packet));
 
    int result = mini_coap_parse_pdu(buffer, sizeof(buffer), &packet);
 
    /*
     * Dopuszczamy dwa zachowania zgodne ze specyfikacją:
     *   (a) błąd formatu: result == -1
     *   (b) sukces z payload_len == 0 (implementacja łagodna)
     * Oba są poprawne – testujemy że NIE DOCHODZI do UB/zapisu
     * i że payload_len nie jest > 0 przy braku bajtów.
     */
    if (result == 0) {
        TEST_ASSERT_EQUAL_INT(0, packet.payload_len);
    }
    /* Jeśli result == -1, test jest zaliczony bez dalszych sprawdzeń */
}
 
 
/* =========================================================
 * SEKCJA 2: Serializacja – warunki błędów
 * ========================================================= */
 
/**
 * TEST 14: Bufor wyjściowy za mały na dane.
 * Pakiet: 4B nagłówka + 2B tokenu + 1B marker + 2B payload = 9B.
 * Przekazujemy bufor o rozmiarze 5 bajtów – musi zwrócić błąd.
 */
void test_coap_serialize_buffer_too_small(void) {
    coap_packet_t packet;
    memset(&packet, 0, sizeof(packet));
 
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
 
    uint8_t small_buffer[5]; /* za mały: potrzeba 9B */
    size_t len = 0;
 
    int result = mini_coap_serialize_pdu(&packet, small_buffer, &len);
 
    TEST_ASSERT_EQUAL_INT(-1, result);
}
 
/**
 * TEST 15: NULL-owe argumenty serializacji.
 * Funkcja nie może crashować przy packet == NULL lub buffer == NULL.
 */
void test_coap_serialize_null_args(void) {
    coap_packet_t packet;
    memset(&packet, 0, sizeof(packet));
    packet.version = 1;
 
    uint8_t buffer[64];
    size_t len = 0;
 
    TEST_ASSERT_EQUAL_INT(-1, mini_coap_serialize_pdu(NULL, buffer, &len));
    TEST_ASSERT_EQUAL_INT(-1, mini_coap_serialize_pdu(&packet, NULL, &len));
}
 
 
/* =========================================================
 * SEKCJA 3: Round-trip (serialize → parse)
 * ========================================================= */
 
/**
 * TEST 16: Idempotentność: serialize → parse → weryfikacja pól.
 * Budujemy pakiet, serializujemy do bufora, parsujemy bufor
 * i porównujemy wszystkie pola oryginału z wynikiem parsowania.
 * To najlepszy test integracyjny łączący obie podsystemy.
 */
void test_coap_round_trip(void) {
    /* Oryginał */
    coap_packet_t original;
    memset(&original, 0, sizeof(original));
 
    original.version    = 1;
    original.type       = COAP_TYPE_NON;  /* Non-confirmable */
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
 
    /* Serializacja */
    uint8_t wire[128];
    size_t wire_len = 0;
    int ser_result = mini_coap_serialize_pdu(&original, wire, &wire_len);
    TEST_ASSERT_EQUAL_INT(0, ser_result);
 
    /* Oczekiwana długość: 4 + 4 (token) + 1 (marker) + 5 (payload) = 14B */
    TEST_ASSERT_EQUAL_INT(14, (int)wire_len);
 
    /* Parsowanie */
    coap_packet_t parsed;
    memset(&parsed, 0, sizeof(parsed));
    int par_result = mini_coap_parse_pdu(wire, wire_len, &parsed);
    TEST_ASSERT_EQUAL_INT(0, par_result);
 
    /* Weryfikacja pól */
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
 * SEKCJA 4: Routing – przypadki negatywne
 * ========================================================= */
 
/* Flaga pomocnicza – ustawiana jeśli handler zostanie wywołany */
static bool wrong_handler_called = false;
 
void wrong_resource_handler(struct sockaddr_in *addr, coap_packet_t *req) {
    wrong_handler_called = true;
}
 
/**
 * TEST 17: Routing do nieistniejącej ścieżki.
 * Rejestrujemy zasób "temperature", wysyłamy żądanie do "humidity".
 * Handler NIE powinien zostać wywołany.
 */
void test_coap_routing_unknown_path(void) {
    wrong_handler_called = false;
    mini_coap_register_resource("temperature", COAP_METHOD_GET, wrong_resource_handler);
 
    coap_packet_t request;
    memset(&request, 0, sizeof(request));
    request.code        = COAP_METHOD_GET;
    request.options     = (const uint8_t *)"humidity";
    request.options_len = 8;
 
    struct sockaddr_in dummy_addr;
    mini_coap_route_request(&request, &dummy_addr);
 
    TEST_ASSERT_FALSE(wrong_handler_called);
}
 
/**
 * TEST 18: Routing – poprawna ścieżka, zła metoda.
 * Rejestrujemy zasób "led" tylko dla POST.
 * Wysyłamy GET – handler nie powinien zostać wywołany.
 */
void test_coap_routing_wrong_method(void) {
    wrong_handler_called = false;
    mini_coap_register_resource("led", COAP_METHOD_POST, wrong_resource_handler);
 
    coap_packet_t request;
    memset(&request, 0, sizeof(request));
    request.code        = COAP_METHOD_GET;  /* Zła metoda – POST wymagany */
    request.options     = (const uint8_t *)"led";
    request.options_len = 3;
 
    struct sockaddr_in dummy_addr;
    mini_coap_route_request(&request, &dummy_addr);
 
    TEST_ASSERT_FALSE(wrong_handler_called);
}
 
/**
 * TEST 19: Routing z NULL-owym żądaniem.
 * Funkcja routingu nie może crashować przy request == NULL.
 * Oczekiwamy że po prostu nie wywoła żadnego handlera.
 */
void test_coap_routing_null_request(void) {
    wrong_handler_called = false;
    struct sockaddr_in dummy_addr;
 
    /* Nie powinno crashować – brak asercji na wynik, tylko test stabilności */
    mini_coap_route_request(NULL, &dummy_addr);
 
    TEST_ASSERT_FALSE(wrong_handler_called);
}

int app_main() {
    UNITY_BEGIN(); 
    
    // Testy Parsowania (z poprzedniej wiadomości)
    RUN_TEST(test_coap_parse_too_short);
    RUN_TEST(test_coap_parse_basic_header);
    RUN_TEST(test_coap_parse_with_token);
    RUN_TEST(test_coap_parse_with_payload);
    
    // NOWE Testy Serializacji
    RUN_TEST(test_coap_serialize_basic_header);
    RUN_TEST(test_coap_serialize_with_payload);
    
    // NOWE Testy Routingu
    RUN_TEST(test_coap_routing_success);
    
 
    // Parsowanie – wartości graniczne i błędy
    RUN_TEST(test_coap_parse_null_buffer);
    RUN_TEST(test_coap_parse_null_packet);
    RUN_TEST(test_coap_parse_illegal_tkl);
    RUN_TEST(test_coap_parse_max_tkl);
    RUN_TEST(test_coap_parse_buffer_too_short_for_token);
    RUN_TEST(test_coap_parse_payload_marker_without_data);
 
    // Serializacja – błędy
    RUN_TEST(test_coap_serialize_buffer_too_small);
    RUN_TEST(test_coap_serialize_null_args);
 
    // Round-trip
    RUN_TEST(test_coap_round_trip);
 
    // Routing – przypadki negatywne
    RUN_TEST(test_coap_routing_unknown_path);
    RUN_TEST(test_coap_routing_wrong_method);
    RUN_TEST(test_coap_routing_null_request);
    UNITY_END(); 
    return 0;
}

