# Changelog

Wszystkie istotne zmiany w tym projekcie będą dokumentowane w tym pliku.

Format opiera się na [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [Unreleased] - 2024-07-25

### Dodano
- **Komputer Pokładowy:** Dodano nowy ekran wyświetlający dystans, średnie spalanie i średnią prędkość.
- **Status Pojazdu na WWW:** Wprowadzono nową zakładkę w interfejsie webowym, która na żywo pokazuje status drzwi (osobno dla kierowcy i pasażera), bagażnika, przebieg oraz temperaturę zewnętrzną.
- **Reset Komputera Pokładowego:** Dodano możliwość resetowania danych komputera pokładowego przez dłuższe przytrzymanie przycisku na odpowiednim ekranie lub za pomocą dedykowanego przycisku na stronie WWW.
- **Wskaźnik Biegu:** Dodano wskaźnik aktualnego biegu (1-5, R, N), obliczany na podstawie obrotów i prędkości.
- **Adaptacja Biegów:** Wprowadzono prosty mechanizm adaptacji przełożeń dla każdego biegu (w tym wstecznego) z poziomu interfejsu webowego.
- **Backup i Przywracanie Ustawień:** Dodano możliwość eksportu i importu pełnej konfiguracji urządzenia do/z pliku JSON przez stronę WWW.
- **Ulepszona Aktualizacja OTA:** Stworzono dedykowaną stronę do aktualizacji oprogramowania z wizualnym paskiem postępu.
- **Nowe Animacje Startowe:** Dodano dwie nowe animacje powitalne: "Klasyczne BMW" i "Dynamiczne Linie".
- **Pasek Pozycji Przepustnicy:** Na ekranie "Sport" dodano drugi pasek postępu, wizualizujący wciśnięcie pedału gazu.
- **Plik `CHANGELOG.md`:** Dodano plik śledzący historię zmian w projekcie.
- **Szczegółowe `README.md`:** Utworzono i rozbudowano plik `README.md` o opis projektu, listę funkcji, schemat połączeń i instrukcje.

### Zmieniono
- **Interfejs Webowy:** Przebudowano interfejs, dodając nowe zakładki i przyciski funkcyjne. Zastąpiono suwaki pedału gazu przyciskami do interaktywnej kalibracji.
- **Partycjonowanie Pamięci:** Poprawiono schemat partycji w `partitions.csv`, aby zapewnić, że ustawienia i pliki webowe nie są kasowane podczas aktualizacji oprogramowania.
- **Wygląd Ekranu Timera:** Zwiększono rozmiar czcionki dla wartości pomiaru czasu na ekranie timera, aby poprawić czytelność.

### Naprawiono
- **Pomiar Spalania:** Naprawiono błąd, przez który pomiar chwilowego zużycia paliwa zawsze pokazywał 0.0.
- **Animacja Startowa:** Poprawiono pozycjonowanie animacji startowej.
