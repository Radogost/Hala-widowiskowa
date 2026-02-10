#include "common/shared.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <time.h>

struct sembuf lock = {0, -1, 0};
struct sembuf unlock = {0, 1, 0};

int main() {
    srand(time(NULL) ^ getpid());
    
    key_t key_shm = get_shm_key(FTOK_PATH, SHM_ID);
    int shmid = shmget(key_shm, sizeof(ArenaState), 0600);
    ArenaState *hala = (ArenaState*)shmat(shmid, NULL, 0);
    key_t key_sem = get_sem_key(FTOK_PATH, SEM_ID);
    int semid = semget(key_sem, 1, 0600);

    // Losowanie czy VIP
    int is_vip = (rand() % 1000) < 3; // 0.3% szans
    int my_ticket_sector = -1;
    int my_team = (rand() % 2) + 1; // 1 lub 2

// 1. ZAKUP BILETU (tylko non-VIP)
    if (!is_vip) {
        semop(semid, &lock, 1);
        hala->queue_to_cashiers++;
        semop(semid, &unlock, 1);

        // Symulacja stania w kolejce
        int has_ticket = 0;
        while(!has_ticket) {
            //usleep(100000); // Czekaj (poza sekcją krytyczną - to jest OK)
            usleep(1000000 + (rand() % 2000000));
            
            semop(semid, &lock, 1); // <--- ZAMYKASZ SEMAFOR
            
            // Sprawdź czy są kasy (active_cashiers > 0)
            if (hala->active_cashiers_count > 0) {
                // ... (Twoja logika szukania miejsca - jest poprawna) ...
                
                int start_sector = rand() % MAX_SECTORS;
                int found_sector = -1;

                for (int i = 0; i < MAX_SECTORS; i++) {
                    int current_idx = (start_sector + i) % MAX_SECTORS;
                    if (hala->tickets_sold[current_idx] < hala->sector_capacity) {
                        found_sector = current_idx;
                        break;
                    }
                }

                if (found_sector != -1) {
                    my_ticket_sector = found_sector;
                    hala->tickets_sold[my_ticket_sector]++;
                    hala->queue_to_cashiers--;
                    has_ticket = 1;
                } else {
                    // Brak miejsc - nic nie robimy, pętla leci dalej
                }
            }
            
            // !!! TU BYŁ BŁĄD - BRAKOWAŁO TEJ LINIJKI !!!
            semop(semid, &unlock, 1); // <--- MUSISZ OTWORZYĆ SEMAFOR!
            
        } // Koniec pętli while
    } else {
        // Logika VIP (jest OK)
        semop(semid, &lock, 1);
        hala->vip_count++;
        semop(semid, &unlock, 1);
        return 0; 
    }

    // 2. KONTROLA BEZPIECZEŃSTWA
    // Idź do bramki swojego sektora
    int gate_passed = 0;
    while(!gate_passed && !hala->evacuation_mode) {
        semop(semid, &lock, 1);
        
        // Spr. sygnał 1 (wstrzymanie)
        if (hala->sector_signal_status[my_ticket_sector] == 1) {
            semop(semid, &unlock, 1);
            sleep(1); continue;
        }

        // Szukaj wolnego stanowiska (są 2 na sektor)
        for(int i=0; i<SECURITY_PER_GATE; i++) {
            SecurityPost *post = &hala->security[my_ticket_sector][i];
            
            // Zasady: max 3 osoby, zgodność drużyny
            int can_enter = 0;
            if (post->occupied_count == 0) can_enter = 1;
            else if (post->occupied_count < MAX_AT_SECURITY && post->team_id == my_team) can_enter = 1;

            if (can_enter) {
                post->occupied_count++;
                post->team_id = my_team;
                
                semop(semid, &unlock, 1);
                
                // Symulacja kontroli
                usleep(200000);
                
                semop(semid, &lock, 1);
                post->occupied_count--;
                if(post->occupied_count == 0) post->team_id = 0; // Reset flagi drużyny
                
                // Wejście na sektor
                if (hala->people_in_sector[my_ticket_sector] < hala->sector_capacity) {
                    hala->people_in_sector[my_ticket_sector]++;
                    gate_passed = 1;
                }
                break; // Wychodzimy z pętli for szukania stanowiska
            }
        }
        semop(semid, &unlock, 1);
        if(!gate_passed) usleep(50000); // Czekaj jeśli tłok
    }

    // 3. MECZ
    while(!hala->evacuation_mode) {
        sleep(1);
    }

    // 4. EWAKUACJA
    // Każdy kibic czeka losowo od 0.5s do 2.5s zanim dojdzie do wyjścia.
    // Dzięki temu na dashboardzie liczby będą spadać płynnie, a nie skokowo.
    usleep(500000 + (rand() % 2000000));
    semop(semid, &lock, 1);
    if(my_ticket_sector != -1 && hala->people_in_sector[my_ticket_sector] > 0)
        hala->people_in_sector[my_ticket_sector]--;
    semop(semid, &unlock, 1);

    return 0;
}