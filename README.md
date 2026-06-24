# E46 OLED Digital Dash

## Opis

Wielofunkcyjny, cyfrowy wyświetlacz dla BMW E46, zbudowany na bazie mikrokontrolera ESP32-S3. Projekt wykorzystuje wyświetlacz OLED SSD1306 do prezentowania w czasie rzeczywistym danych z magistrali CAN pojazdu. Urządzenie oferuje również interfejs webowy do konfiguracji i podglądu danych na żywo.

## Funkcje

- **Wyświetlanie danych w czasie rzeczywistym:**
  - Obroty silnika (RPM)
  - Prędkość pojazdu (KM/H)
  - Temperatura płynu chłodzącego
  - Temperatura oleju
  - Napięcie akumulatora
  - Chwilowe zużycie paliwa (L/H lub L/100km)
  - Pozycja pedału gazu
- **Wiele ekranów:**
  - **Siatka:** Dowolna konfiguracja 4 metryk.
  - **Sport:** Pasek obrotów, pasek wciśnięcia pedału gazu, prędkość i temperatura.
  - **Timer:** Pomiar przyspieszenia 0-100 km/h.
  - **Peaki:** Wyświetlanie maksymalnych wartości z trasy (RPM, prędkość, temperatura).
- **Alerty i wskaźniki:**
  - Konfigurowalny Shift Light.
  - Alert o przegrzaniu silnika.
- **Interfejs Webowy:**
  - Podgląd danych na żywo (mirroring ekranu OLED).
  - Konfiguracja wyświetlanych metryk.
  - Regulacja jasności ekranu i pozycji obrazu.
  - Interaktywna kalibracja (adaptacja) zakresu pedału gazu.
  - Możliwość uruchomienia animacji startowej i resetowania peaków.
- **Aktualizacje OTA:** Możliwość wgrywania nowego oprogramowania przez Wi-Fi.
- **Personalizacja:**
  - Wybór animacji startowej.
  - Ustawienia zapisywane w pamięci flash, odporne na aktualizacje.

## Sprzęt

- Mikrokontroler: **ESP32-S3** (np. ESP32-S3-DevKitC-1)
- Wyświetlacz: **OLED 128x64 SSD1306** (interfejs I2C)
- Transceiver CAN: Układ do komunikacji z magistralą CAN (np. TJA1050, MCP2551)
- Przycisk do zmiany ekranów.

## Oprogramowanie i Biblioteki

Projekt został stworzony w środowisku **PlatformIO** z wykorzystaniem frameworka **Arduino**.

- **Adafruit GFX & SSD1306:** Sterowanie wyświetlaczem OLED.
- **ESPAsyncWebServer & AsyncTCP:** Obsługa serwera WWW i WebSocket.
- **ArduinoJson:** Parsowanie i generowanie danych w formacie JSON.
- **LittleFS:** System plików dla interfejsu webowego.
- **Preferences:** Zapisywanie ustawień w pamięci NVS.

## Instalacja

Instalacja oprogramowania odbywa się poprzez wgranie gotowego pliku `firmware.bin`, który można znaleźć w zakładce **Releases** na stronie projektu.

Do wgrania oprogramowania można użyć jednego z poniższych narzędzi:
- **Oficjalny ESP-IDF:** [https://docs.espressif.com/projects/esptool/en/latest/esp32s3/esptool/flashing-firmware.html](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/esptool/flashing-firmware.html)
- **Tasmota Web Installer:** [https://tasmota.github.io/install/](https://tasmota.github.io/install/) (pozwala na wgranie własnego pliku .bin)

## Konfiguracja

1.  Po uruchomieniu, urządzenie tworzy sieć Wi-Fi o nazwie **"BMW_E46_OLED"**.
2.  Połącz się z tą siecią. Hasło nie jest wymagane.
3.  Otwórz przeglądarkę internetową i przejdź pod adres IP urządzenia (domyślnie 192.168.4.1).
4.  Użyj interfejsu webowego do personalizacji ustawień, które zostaną automatycznie zapisane w pamięci urządzenia.
