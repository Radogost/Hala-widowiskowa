#include "common/shared.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

struct sembuf lock = {0, -1, 0};
struct sembuf unlock = {0, 1, 0};

int main() {
    srand(time(NULL) ^ getpid());
    
    // Inicjalizacja IPC
    key_t key_shm = get_shm_key(FTOK_PATH, SHM_ID);
    int shmid = shmget(key_shm, sizeof(ArenaState), 0600);
    ArenaState *hala = (ArenaState*)shmat(shmid, NULL, 0);
    key_t key_sem = get_sem_key(FTOK_PATH, SEM_ID);
    int semid = semget(key_sem, 1, 0600);

    // --- DEKLARACJE ZMIENNYCH (NA GÓRZE - ROZWIĄZUJE PROBLEM GOTO) ---
    int is_vip = (rand() % 1000) < 3; 
    int my_ticket_sector = -1;
    int my_team = (rand() % 2) + 1; 
    char log_buf[150];

    int tickets_owned = 0; 
    int gate_passed = 0;
    int escaped_from_queue = 0;
    int has_ticket = 0;
    int patience_lost_count = 0;

    // 1. ZAKUP BILETU
    if (!is_vip) {
        semop(semid, &lock, 1);
        hala->queue_to_cashiers++;
        semop(semid, &unlock, 1);

        sprintf(log_buf, "PID %d: Staje w kolejce do kasy", getpid());
        log_event("KIBIC", log_buf);

        while(!has_ticket) {
            // Sprawdzenie ewakuacji przed usleep
            if (hala->evacuation_mode) {
                semop(semid, &lock, 1);
                hala->queue_to_cashiers--; 
                semop(semid, &unlock, 1);
                escaped_from_queue = 1;
                break; 
            }

            usleep(500000); 
            
            semop(semid, &lock, 1);
            
            // Sprawdzenie ewakuacji w sekcji krytycznej
            if (hala->evacuation_mode) {
                hala->queue_to_cashiers--;
                semop(semid, &unlock, 1);
                escaped_from_queue = 1;
                break;
            }

            if (hala->active_cashiers_count > 0) {
                int start_sector = rand() % MAX_SECTORS;
                int found_sector = -1;
                int wanted = (rand() % 2) + 1; 

                for (int i = 0; i < MAX_SECTORS; i++) {
                    int idx = (start_sector + i) % MAX_SECTORS;
                    if (hala->tickets_sold[idx] + wanted <= hala->sector_capacity) {
                        found_sector = idx;
                        tickets_owned = wanted;
                        break;
                    }
                }

                if (found_sector != -1) {
                    my_ticket_sector = found_sector;
                    hala->tickets_sold[my_ticket_sector] += tickets_owned;
                    hala->queue_to_cashiers--;
                    has_ticket = 1;

                    sprintf(log_buf, "PID %d: Kupil [%d BILET(Y)] na Sektor %d (Druzyna %d)", 
                            getpid(), tickets_owned, my_ticket_sector+1, my_team);
                    log_event("KIBIC", log_buf);
                }
            }
            semop(semid, &unlock, 1);
        }

        if (escaped_from_queue) goto evacuation_exit;
        
    } else {
        semop(semid, &lock, 1);
        hala->vip_count++;
        semop(semid, &unlock, 1);
        log_event("VIP", "VIP wszedl na stadion");
        return 0; 
    }

    // 2. KONTROLA BEZPIECZEŃSTWA
    while(!gate_passed && !hala->evacuation_mode) {
        semop(semid, &lock, 1);
        
        if (hala->sector_signal_status[my_ticket_sector] == 1) {
            semop(semid, &unlock, 1);
            sleep(1); continue;
        }

        int entered_gate = 0; // 1. Flaga pomocnicza: czy udało się wejść w tej rundzie?

        for(int i=0; i<SECURITY_PER_GATE; i++) {
            SecurityPost *post = &hala->security[my_ticket_sector][i];
            
            int can_enter = 0;
            if (post->occupied_count == 0) can_enter = 1;
            else if (post->occupied_count < MAX_AT_SECURITY && post->team_id == my_team) can_enter = 1;

            if (can_enter) {
                post->occupied_count++;
                post->team_id = my_team;
                entered_gate = 1; // 2. Oznaczamy sukces
                
                semop(semid, &unlock, 1);
                
                usleep(200000); 
                
                semop(semid, &lock, 1);
                post->occupied_count--;
                if(post->occupied_count == 0) post->team_id = 0; 
                
                if (hala->people_in_sector[my_ticket_sector] + tickets_owned <= hala->sector_capacity) {
                    hala->people_in_sector[my_ticket_sector] += tickets_owned;
                    gate_passed = 1;
                    sprintf(log_buf, "PID %d: Wszedł na Sektor %d (Grupa: %d os.)", getpid(), my_ticket_sector+1, tickets_owned);
                    log_event("KIBIC", log_buf);
                }
                break; 
            }
        }

        // 3. LOGIKA AGRESJI
        // Sprawdzamy to PO pętli for, ale PRZED odblokowaniem semafora (unlock)
        if (!entered_gate) {
            patience_lost_count++;
            if (patience_lost_count > 5) {
                sprintf(log_buf, "PID %d: AGRESYWNE ZACHOWANIE! Czeka zbyt dlugo (%d raz)", getpid(), patience_lost_count);
                log_event("AGRESJA", log_buf);
                
                // Opcjonalnie: krótki sleep, żeby agresywny kibic "pychał się" szybciej
                semop(semid, &unlock, 1);
                usleep(10000); 
                continue; 
            }
        }
        // -------------------------

        semop(semid, &unlock, 1);
        if(!gate_passed) usleep(50000); 
    }

    // 3. MECZ
    while(!hala->evacuation_mode) {
        sleep(1);
    }

evacuation_exit:
    // 4. EWAKUACJA
    usleep(500000 + (rand() % 2000000));
    
    semop(semid, &lock, 1);
    if(gate_passed && my_ticket_sector != -1) {
        if (hala->people_in_sector[my_ticket_sector] >= tickets_owned) {
            hala->people_in_sector[my_ticket_sector] -= tickets_owned;
        } else {
            hala->people_in_sector[my_ticket_sector] = 0;
        }
    }
    semop(semid, &unlock, 1);

    sprintf(log_buf, "PID %d: Ewakuowany (Z sektora: %s)", getpid(), gate_passed ? "TAK" : "NIE");
    log_event("KIBIC", log_buf);

    shmdt(hala);
    return 0;
}