#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
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

        // --- DECYZJA KASJERA O PRACY ---
        // Każdy kasjer ma swoje ID (np. 0, 1, 2...).
        // Jeśli jest potrzebnych 3 kasjerów, to pracują ID: 0, 1, 2.
        // Kasjerzy o ID 3, 4, 5... idą spać.
        
        if (id >= needed_cashiers) {
            // Jestem niepotrzebny -> zamykam swoje okienko
            semop(semid, &unlock, 1);
            
            // Śpię dłużej, żeby nie męczyć semafora (jestem w rezerwie)
            sleep(1); 
            continue;
        }

        // --- PRACA KASJERA ---
        // Jestem potrzebny! 
        // W tym modelu (procesowym) kasjer głównie "udostępnia" slot.
        // Faktyczna "sprzedaż" dzieje się w procesie Kibica, który zmniejsza kolejkę.
        // Ale dla realizmu możemy tu "pomrugać" w logach albo po prostu zwolnić semafor.
        
        semop(semid, &unlock, 1);
    }
    return 0;
}