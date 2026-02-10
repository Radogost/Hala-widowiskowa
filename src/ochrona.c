#include "common/shared.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    int sector_id = atoi(argv[1]); // ID sektora 0-7
    
    key_t key_shm = get_shm_key(FTOK_PATH, SHM_ID);
    int shmid = shmget(key_shm, sizeof(ArenaState), 0600);
    ArenaState *hala = (ArenaState*)shmat(shmid, NULL, 0);
    
    // Pracownik techniczny czuwa
    while(1) {
        sleep(1);
        if (hala->sector_signal_status[sector_id] == 1) {
            // Sektor wstrzymany - pracownik pilnuje (logika blokady jest w kibic.c)
        }
        if (hala->evacuation_mode) {
            // Otwórz wszystko przy ewakuacji
            hala->sector_signal_status[sector_id] = 0; 
        }
    }
    return 0;
}