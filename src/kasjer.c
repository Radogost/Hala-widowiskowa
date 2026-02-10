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
        //usleep(100000); // Taktowanie kasy

        sleep(1); // Kasjer sprawdza co sekundę, czy jest potrzebny (to jest OK, bo nie trzyma semafora podczas sleep)
        
        semop(semid, &lock, 1);
        
        // ZASADA 2 i 3: Liczba czynnych kas
        // Min 2 kasy. 
        // 1 kasa na każde K/10 kibiców w kolejce.
        int threshold = hala->K / 10;
        if (threshold == 0) threshold = 1;
        
        int needed_cashiers = 2 + (hala->queue_to_cashiers / threshold);
        if (needed_cashiers > MAX_CASHIERS) needed_cashiers = MAX_CASHIERS;
        
        hala->active_cashiers_count = needed_cashiers;

        // Jeśli moje ID jest wyższe niż liczba potrzebnych kas, zamykam się (czekam)
        if (id >= needed_cashiers) {
            semop(semid, &unlock, 1);
            sleep(1); // Kasjer odpoczywa
            continue;
        }

        // Praca kasjera - obsługa kibiców jest w procesie Kibica (który sprawdza active_cashiers),
        // ale tutaj możemy symulować logowanie lub specjalne akcje.
        // W modelu procesowym "Kibic" jest aktywny i to on "kupuje" bilet.
        // Kasjer tylko udostępnia swoją "dostępność" przez active_cashiers_count.
        
        semop(semid, &unlock, 1);
    }
    return 0;
}