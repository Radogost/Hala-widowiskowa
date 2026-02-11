#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <stdio.h>
#include "common/shared.h"

int main(int argc, char *argv[]) {
    int id = atoi(argv[1]); // ID kasjera 0-9
    
    key_t key_shm = get_shm_key(FTOK_PATH, SHM_ID);
    int shmid = shmget(key_shm, sizeof(ArenaState), 0600);
    ArenaState *hala = (ArenaState*)shmat(shmid, NULL, 0);
    
    key_t key_sem = get_sem_key(FTOK_PATH, SEM_ID);
    int semid = semget(key_sem, 1, 0600);
    
    struct sembuf lock = {0, -1, 0};
    struct sembuf unlock = {0, 1, 0};

    // Logika kasjera: dynamiczne otwieranie/zamykanie kas w zależności od kolejki
    char log_buf[100];
    int was_active = 0;

    while(1) {
        // ZMIANA: Zamiast sleep(1), sprawdzamy częściej (co 0.2s)
        // Dzięki temu, jak przyjedzie autobus, kasy otworzą się natychmiast.
        usleep(200000); 
        
        semop(semid, &lock, 1);
        
        // --- LOGIKA SKALOWANIA (K/10) ---
        int threshold = hala->K / 10;
        
        // Zabezpieczenie: jeśli hala jest malutka (np. K=5), próg nie może być 0
        if (threshold <= 0) threshold = 1;
        
        // Obliczamy ile kas jest potrzebnych
        // Baza (2) + (Kolejka / Próg)
        int needed_cashiers = 2 + (hala->queue_to_cashiers / threshold);

        // Nie możemy przekroczyć maksymalnej liczby kasjerów (np. 10)
        if (needed_cashiers > MAX_CASHIERS) needed_cashiers = MAX_CASHIERS;
        
        // Aktualizujemy globalną informację dla Kibiców
        hala->active_cashiers_count = needed_cashiers;

        // Sprawdzamy, czy ten konkretny kasjer powinien pracować
        int am_active = (id < needed_cashiers);

        // --- SEKCJA LOGOWANIA (TYLKO PRZY ZMIANIE STANU) ---
        if (am_active && !was_active) {
            // Byłem zamknięty, teraz się otwieram
            sprintf(log_buf, "Kasa %d: OTWIERA SIĘ (Kolejka: %d os.)", id+1, hala->queue_to_cashiers);
            log_event("KASJER", log_buf);
        }
        else if (!am_active && was_active) {
            // Byłem otwarty, teraz się zamykam
            sprintf(log_buf, "Kasa %d: ZAMYKA SIĘ (Spadek kolejki)", id+1);
            log_event("KASJER", log_buf);
        }
        was_active = am_active; // Zapamiętaj stan na następną pętlę
        // ---------------------------------------------------

        // Decyzja o spaniu
        if (!am_active) {
            // Jestem niepotrzebny -> zamykam swoje okienko (zwalniam semafor)
            semop(semid, &unlock, 1);
            
            sleep(1); // Śpię dłużej
            continue;
        }

        // Jestem potrzebny (udostępniam slot)
        semop(semid, &unlock, 1);
    }
    return 0;
}