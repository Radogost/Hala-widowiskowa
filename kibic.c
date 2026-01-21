#include "common.h"
#include "ipc_utils.h"

int main() {
    srand(getpid());
    int shm_id = shmget(SHM_KEY, sizeof(SharedState), 0666);
    int sem_id = semget(SEM_KEY, 0, 0666);
    
    if (shm_id == -1 || sem_id == -1) exit(1); // Brak zasobów = koniec
    SharedState *stan = (SharedState *)shmat(shm_id, NULL, 0);

    int sektor = rand() % LICZBA_SEKTOROW;
    
    // 1. Wejście do kolejki
    sem_lock(sem_id, SEM_MUTEX_MAIN);
    stan->liczba_kibicow_w_kolejce++;
    sem_unlock(sem_id, SEM_MUTEX_MAIN);

    printf("[KIBIC %d] Stoję w kolejce po bilet do sek. %d\n", getpid(), sektor);
    sleep(1); // Czekanie w kolejce

    // 2. Kupno biletu
    sem_lock(sem_id, SEM_MUTEX_MAIN);
    stan->liczba_kibicow_w_kolejce--; // Wychodzę z kolejki
    
    if (stan->bilety_dostepne[sektor] > 0) {
        stan->bilety_dostepne[sektor]--;
        
        printf("[KIBIC %d] KUPIŁEM bilet do sektora %d! (Zostało: %d)\n", 
               getpid(), sektor, stan->bilety_dostepne[sektor]);

    } else {
        // Tu też warto dodać informację o sektorze:
        printf("[KIBIC %d] Brak biletów do sektora %d! Wychodzę.\n", getpid(), sektor);
        
        sem_unlock(sem_id, SEM_MUTEX_MAIN);
        shmdt(stan);
        exit(0);
    }
    sem_unlock(sem_id, SEM_MUTEX_MAIN);

    // 3. Koniec symulacji dla tego kibica
    shmdt(stan);
    return 0;
}