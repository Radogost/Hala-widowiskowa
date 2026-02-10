#include "common.h"
#include "ipc_utils.h"

int main(int argc, char *argv[]) {
    // Unikamy ostrzeżeń kompilatora o nieużywanych argumentach
    (void)argc; (void)argv;

    // 1. Pobieramy ID istniejących zasobów IPC
    int shm_id = shmget(SHM_KEY, sizeof(SharedState), 0666);
    check_error(shm_id, "shmget kasjer");

    int sem_id = semget(SEM_KEY, 0, 0666);
    check_error(sem_id, "semget kasjer");

    // 2. Podłączamy pamięć dzieloną
    SharedState *stan = (SharedState *)shmat(shm_id, NULL, 0);
    if (stan == (void *)-1) check_error(-1, "shmat kasjer");

    // 3. Rejestracja kasjera (Zwiększamy licznik aktywnych kas)
    sem_lock(sem_id, SEM_MUTEX_MAIN);
    stan->aktywne_kasy++;
    printf("[KASJER %d] Otwieram stanowisko. Aktywne kasy: %d\n", getpid(), stan->aktywne_kasy);
    sem_unlock(sem_id, SEM_MUTEX_MAIN);

    // 4. Główna pętla pracy
    while (1) {
        // Kasjer sprawdza stan co jakiś czas (symulacja pracy)
        sleep(2);

        // Wewnątrz while(1) w kasjer.c
    sem_lock(sem_id, SEM_MUTEX_MAIN);

    // 1. Czy są jeszcze bilety? (To już masz)
    int suma_biletow = 0;
    for (int i = 0; i < LICZBA_SEKTOROW; i++) suma_biletow += stan->bilety_dostepne[i];

    if (suma_biletow == 0) {
        stan->aktywne_kasy--;
        printf("[KASJER %d] Brak biletów! Zamykam się.\n", getpid());
        sem_unlock(sem_id, SEM_MUTEX_MAIN);
        break;
    }

    // 2. LOGIKA ZAMYKANIA WG WZORU Z ZADANIA
    // Wymóg: Zamykamy, jeśli kolejka < (K/10) * (N-1)
    // Ale uwaga: Zawsze muszą działać min. 2 kasy!
    
    int N = stan->aktywne_kasy; // Liczba czynnych kas
    int K = POJEMNOSC_HALI;     // Pojemność (1000)
    int kolejka = stan->liczba_kibicow_w_kolejce + stan->liczba_vip_w_kolejce;
    
    // Obliczamy próg (limit)
    // Wzór: (K/10) * (N-1)
    int limit = (K / 10) * (N - 1);

    // Sprawdzenie warunku
    if (N > 2 && kolejka < limit) {
        stan->aktywne_kasy--; // Zmniejszamy licznik kas
        printf("[KASJER %d] Zbyt mała kolejka (%d osób na %d kas). Zamykam stanowisko!\n", 
               getpid(), kolejka, N);
        
        sem_unlock(sem_id, SEM_MUTEX_MAIN);
        break; // Koniec procesu kasjera
    }

    sem_unlock(sem_id, SEM_MUTEX_MAIN);
    }

    // 5. Sprzątanie przed wyjściem
    shmdt(stan);
    return 0;
}