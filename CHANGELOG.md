# Changelog

## [Unreleased] - 2024-07-25

### Dodano
- **Wskaźnik Biegu:** Dodano wskaźnik aktualnego biegu, obliczany na podstawie obrotów i prędkości.
- **Adaptacja Biegów:** Wprowadzono prosty mechanizm adaptacji przełożeń dla każdego biegu z poziomu interfejsu webowego.
- **Backup i Przywracanie Ustawień:** Dodano możliwość eksportu i importu pełnej konfiguracji urządzenia do/z pliku JSON przez stronę WWW.
- **Nowe Animacje Startowe:** Dodano dwie nowe animacje powitalne: "Klasyczne BMW" i "Dynamiczne Linie".
- **Pasek Pozycji Przepustnicy:** Na ekranie "Sport" dodano drugi pasek postępu, wizualizujący wciśnięcie pedału gazu.
- **Plik `CHANGELOG.md`:** Dodano plik śledzący historię zmian w projekcie.
- **Szczegółowe `README.md`:** Utworzono i rozbudowano plik `README.md` o opis projektu, listę funkcji, schemat połączeń i instrukcje.

### Zmieniono
- **Interfejs Webowy:** Przebudowano sekcję "Adaptacje", zastępując suwaki pedału gazu przyciskami do interaktywnej kalibracji MIN/MAX. Dodano przyciski do adaptacji biegów oraz importu/eksportu ustawień.
- **Partycjonowanie Pamięci:** Poprawiono schemat partycji w `partitions.csv`, zmieniając `spiffs` na `littlefs`, aby zapewnić, że ustawienia i pliki webowe nie są kasowane podczas aktualizacji oprogramowania (OTA/UART).
- **Wygląd Ekranu Timera:** Zwiększono rozmiar czcionki dla wartości pomiaru czasu na ekranie timera, aby poprawić czytelność.

### Naprawiono
- **Pomiar Spalania:** Naprawiono błąd, przez który pomiar chwilowego zużycia paliwa zawsze pokazywał 0.0. Problem leżał w braku obsługi odpowiedniej ramki CAN (ID `0x545`).
- **Animacja Startowa:** Poprawiono pozycjonowanie animacji startowej, która była przesunięta i nie mieściła się w całości na ekranie.
