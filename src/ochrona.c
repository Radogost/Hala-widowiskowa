#include "common/shared.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <stdio.h>

int main(int argc, char *argv[]) {
    int sector_id = atoi(argv[1]); // ID sektora 0-7
    char log_buf[100]; // Bufor na tekst loga
    
    key_t key_shm = get_shm_key(FTOK_PATH, SHM_ID);
    int shmid = shmget(key_shm, sizeof(ArenaState), 0600);
    ArenaState *hala = (ArenaState*)shmat(shmid, NULL, 0);
    
    // Zmienna do zapamiętania poprzedniego stanu (0=Otwarty, 1=Zamknięty)
    int last_status = 0; 

    // Pracownik techniczny czuwa
    while(1) {
        sleep(1);
        
        // Odczytujemy aktualny stan z pamięci dzielonej
        int current_status = hala->sector_signal_status[sector_id];

        // --- LOGOWANIE ZMIAN STANU ---
        
        // Sytuacja 1: Było otwarte (0), jest zamknięte (1)
        if (current_status == 1 && last_status == 0) {
            sprintf(log_buf, "Sektor %d: Otrzymano rozkaz BLOKADY (Kibice wstrzymani)", sector_id+1);
            log_event("OCHRONA", log_buf);
        }
        // Sytuacja 2: Było zamknięte (1), jest otwarte (0)
        else if (current_status == 0 && last_status == 1) {
            sprintf(log_buf, "Sektor %d: Otrzymano rozkaz OTWARCIA (Wznowienie ruchu)", sector_id+1);
            log_event("OCHRONA", log_buf);
        }
        
        // Aktualizujemy "pamięć" ochroniarza
        last_status = current_status;
        // -----------------------------

        // Obsługa ewakuacji (Priorytet najwyższy - otwiera wszystko)
        if (hala->evacuation_mode) {
            hala->sector_signal_status[sector_id] = 0; 
        }
    }
    return 0;
}