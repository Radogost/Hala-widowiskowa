#include "common.h"
#include "ipc_utils.h"

int main() {
    // Inicjalizacja losowości
    srand(getpid() + time(NULL)); 

    int shm_id = shmget(SHM_KEY, sizeof(SharedState), 0666);
    int sem_id = semget(SEM_KEY, 0, 0666);
    
    // Zabezpieczenie przed brakiem zasobów
    if (shm_id == -1 || sem_id == -1) exit(1);
    
    SharedState *stan = (SharedState *)shmat(shm_id, NULL, 0);

    // --- LOSOWANIE ZGODNE Z WYMOGAMI ZADANIA ---
    int sektor = rand() % LICZBA_SEKTOROW;
    
    // WYMÓG: "Jeden kibic może kupić maksymalnie 2 bilety"
    int ilosc_chce_kupic = (rand() % 2) + 1; // Wylosuje 1 lub 2
    
    // WYMÓG: "Liczba kibiców VIP mniejsza niż 0,3% * K"
    // Dla K=1000, 0.3% to 3 osoby.
    // Losujemy z 1000. Jeśli wypadnie 0, 1 lub 2 -> VIP. (Prawdopodobieństwo 0.3%)
    int is_vip = ((rand() % 1000) < 3); 

    // --- 1. WEJŚCIE DO KOLEJKI ---
    sem_lock(sem_id, SEM_MUTEX_MAIN);
    
    if (is_vip) {
        stan->liczba_vip_w_kolejce++;
        // VIP oznaczony kolorem żółtym
        printf("\033[1;33m[VIP %d]\033[0m Omijam kolejkę! Chcę %d bilet(y) do sek. %d\n", 
               getpid(), ilosc_chce_kupic, sektor);
    } else {
        stan->liczba_kibicow_w_kolejce++;
        printf("[KIBIC %d] Staję w kolejce po %d bilet(y) do sek. %d\n", 
               getpid(), ilosc_chce_kupic, sektor);
    }
    
    sem_unlock(sem_id, SEM_MUTEX_MAIN);

    // --- SYMULACJA CZEKANIA ---
    if (is_vip) {
        // VIP wchodzi "bez kolejki" (minimalne opóźnienie techniczne)
        usleep(100000); 
    } else {
        // Zwykły kibic czeka w kolejce
        sleep(1 + (rand() % 2));
    }

    // --- 2. ZAKUP ---
    sem_lock(sem_id, SEM_MUTEX_MAIN);
    
    // SYMULACJA WYCIĄGANIA PIENIĘDZY (To spowolni kolejkę!)
    usleep(100000); // 0.1 sekundy opóźnienia na każdego klienta
    
    // Wyjście z kolejki
    if (is_vip) stan->liczba_vip_w_kolejce--;
    else        stan->liczba_kibicow_w_kolejce--;

    // Weryfikacja dostępności
    if (stan->bilety_dostepne[sektor] >= ilosc_chce_kupic) {
        stan->bilety_dostepne[sektor] -= ilosc_chce_kupic;
        
        // Wybieramy słowo: "bilet" dla 1, "bilety" dla reszty (czyli 2)
        char *forma = (ilosc_chce_kupic == 1 ? "bilet" : "bilety");

        if (is_vip) {
            printf("\033[1;33m[VIP %d] KUPIŁEM %d %s! (Sektor %d, Zostało: %d)\033[0m\n", 
                   getpid(), ilosc_chce_kupic, forma, sektor, stan->bilety_dostepne[sektor]);
        } else {
            printf("[KIBIC %d] Kupiłem %d %s. (Sektor %d, Zostało: %d)\n", 
                   getpid(), ilosc_chce_kupic, forma, sektor, stan->bilety_dostepne[sektor]);
        }
    } else {
        // Porażka
        if (is_vip) {
            printf("\033[1;33m[VIP %d] Brak biletów w sek. %d (Zostało: %d)! Wychodzę.\033[0m\n", 
                   getpid(), sektor, stan->bilety_dostepne[sektor]);
        } else {
            printf("[KIBIC %d] Brak biletów w sek. %d (Zostało: %d). Trudno.\n", 
                   getpid(), sektor, stan->bilety_dostepne[sektor]);
        }
        sem_unlock(sem_id, SEM_MUTEX_MAIN);
        shmdt(stan);
        exit(0);
    }

    sem_unlock(sem_id, SEM_MUTEX_MAIN);

    // Tu w przyszłości: przejście do bramek...
    shmdt(stan);
    return 0;
}