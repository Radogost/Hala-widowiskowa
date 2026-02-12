# 🏟️ Temat – System Zarządzania Halą Widowiskową

## 🎯 Cel Projektu

Implementacja symulacji procesów w obiekcie sportowym, koncentrująca się na **zarządzaniu tłumem**, współbieżnym dostępie do zasobów (miejsca na sektorach) oraz dynamicznym skalowaniu obsługi (kasy biletowe). Projekt wykorzystuje mechanizmy IPC: **Pamięć Dzieloną** oraz **Semafory**.

## 🛠️ Kompilacja i Uruchomienie

1. Aby skompilować projekt, upewnij się, że znajdujesz się w katalogu głównym projektu i wpisz w terminalu:

```bash
make
```
2. Czyszczenie (make clean)
Aby usunąć pliki wykonywalne i stary raport (zalecane przed ponowną kompilacją):

```bash
make clean
```

3. Uruchomienie Automatyczne Scenariusze (Zalecane)
Najlepszy sposób na prezentację projektu (wybierz test 1, 2 lub 3):

```bash
./testy.sh
```
4. Uruchomienie Fizyczne Tryb Ręczny
Uruchomienie głównego zarządcy z możliwością interakcji klawiaturą:

```bash
./kierownik
```

## ⚙️ Opis Procesu

System symuluje pełną ścieżkę kibica – od przybycia pod stadion, przez zakup biletu, kontrolę bezpieczeństwa, aż po udział w wydarzeniu i ewakuację.

### 🎫 Kasy Biletowe (Autoskalowanie)

* **Zasada działania:** Liczba otwartych okienek dostosowuje się dynamicznie do długości kolejki.
* **Algorytm:**
    * **Minimum:** Zawsze czynne są **2 kasy**.
    * **Skalowanie:** Otwierana jest dodatkowa kasa na każde **K/10** osób oczekujących w kolejce.
    * **Maksimum:** Liczba kas nie może przekroczyć limitu `MAX_CASHIERS`.
* **Proces:** Kasjerzy "usypiają", gdy nie są potrzebni, i budzą się natychmiast po nagłym napływie kibiców (np. przyjazd autobusu).

### 👮 Kontrola i Sektory

* **Sektory:** Hala podzielona jest na **8 sektorów**. Każdy ma ograniczoną pojemność.
* **Ochrona:** Przed każdym sektorem znajdują się bramki kontrolne.
    * Ochroniarz wpuszcza kibiców pojedynczo (sekcja krytyczna).
    * Możliwość **zdalnego zablokowania** konkretnego sektora przez Kierownika (symulacja incydentu).
* **Kibice VIP:** Posiadają osobne wejście i nie czekają w głównej kolejce do kas.

---

## 🛑 Sterowanie i Sygnały

Całym systemem zarządza proces **Kierownik** (`kierownik` lub `kierowniktest`), który umożliwia interakcję w czasie rzeczywistym:

| Polecenie / Klawisz | Akcja | Efekt |
| :---: | :--- | :--- |
| `1` + `Nr Sektora` | **Blokada Sektora** | Ochrona wstrzymuje wpuszczanie kibiców na dany sektor (np. z powodu zamieszek). |
| `2` + `Nr Sektora` | **Odblokowanie** | Wznowienie wpuszczania kibiców na dany sektor. |
| `3` | **EWAKUACJA** | Tryb panic-mode. Wszyscy kibice natychmiast opuszczają stadion. Kasy są zamykane. |
| `4` | **Start Meczu** | Wybicie godziny "zero". |
| `5` / `9` | **Koniec** | Bezpieczne zakończenie symulacji, sprzątanie procesów i pamięci IPC. |

---

## 💻 Wymagane Procesy i Implementacja

Projekt zrealizowano w architekturze wieloprocesowej:

1.  **`kierownik` / `kierowniktest`:** Główny proces zarządczy. Tworzy środowisko IPC, uruchamia podprocesy i wyświetla **Dashboard (TUI)**.
2.  **`kasjer`:** Procesy obsługujące kolejkę. Decydują o swojej aktywności na podstawie obciążenia.
3.  **`ochrona`:** Procesy pilnujące limitów wejść na sektory i reagujące na sygnały blokady.
4.  **`kibic`:** Procesy symulujące zachowanie ludzi (zakup biletu, kontrola, oglądanie, ewakuacja).

### 🧪 Scenariusze Testowe (`tests.sh`)

Przygotowano skrypt automatyzujący weryfikację wymagań:
1.  **Test Podstawowy:** Spokojny ruch, stabilność systemu.
2.  **Test "Autobus" (Skalowanie):** Nagłe wygenerowanie dużej grupy ludzi w celu sprawdzenia, czy otworzą się dodatkowe kasy (Reguła K/10).
3.  **Test "Szturm" (Przepełnienie):** Próba wejścia większej liczby osób niż wynosi pojemność hali.
4.  **Test Automatyczny:** Samoczynna symulacja blokady sektora i ewakuacji po określonym czasie.

### 📝 Raportowanie

* Wszystkie kluczowe zdarzenia (zakup biletu, otwarcie kasy, blokada sektora, ewakuacja) są logowane w czasie rzeczywistym do pliku **`raport_symulacji.txt`**.