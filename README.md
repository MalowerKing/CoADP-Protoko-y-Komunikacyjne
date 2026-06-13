# MiniCoAP – Protokoły Komunikacyjne

Lekka implementacja serwera CoAP (RFC 7252) na ESP32, napisana w C++.

## Wymagania

- [PlatformIO](https://platformio.org/) (CLI lub rozszerzenie VS Code)
- Płytka ESP32-C3-mini (inne ESP32 są kompatybilne, ale mogą wymagać zmian w `platformio.ini`)

## Konfiguracja Wi-Fi

Utwórz plik `secrets.ini` w katalogu głównym projektu (jest w `.gitignore` – nie commituj go):

```ini
[secrets]
build_flags =
    -DWIFI_SSID=\"NazwaSieci\"
    -DWIFI_PASS=\"Haslo\"
```

## Testowanie

```bash
pio test
```

## Wgrywanie i monitorowanie

```bash
pio run -t upload -t monitor
```

Wgrywa firmware na płytkę i otwiera monitor UART do podglądu logów.

## Użycie

Po uruchomieniu ESP32 łączy się z Wi-Fi i startuje serwer CoAP na porcie `5683`.  
Zarejestrowane zasoby:

| Ścieżka   | Metoda | Opis              |
|-----------|--------|-------------------|
| `/status` | GET    | Zwraca `OK`       |
| `/echo`   | GET    | Odsyła payload z powrotem |

Przykładowe zapytanie (wymaga `libcoap`):

```bash
coap-client -m get coap:///status
```

IP urządzenia jest widoczne w logu po uruchomieniu monitora.
