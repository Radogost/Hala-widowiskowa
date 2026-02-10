#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <poll.h>
#include "common/shared.h"

ArenaState *hala;
int shmid, semid;
pid_t pids[50]; 
int pid_count = 0;

struct sembuf lock = {0, -1, 0};
struct sembuf unlock = {0, 1, 0};

void cleanup() {
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);
    kill(0, SIGTERM); 
    printf("\n[Kierownik] Zasoby wyczyszczone. Koniec.\n");
}

void handle_sigint(int sig) {
    cleanup();
    exit(0);
}

// Generator kibiców (Tryb Falowy)
void run_fan_generator(int K) {
    if (fork() == 0) {
        srand(getpid());
        int total_fans = K + (int)(K * 0.05); 
        
        int rush_mode = 0;
        int counter = 0;

        for (int i = 0; i < total_fans; i++) {
            if (fork() == 0) {
                execl("./kibic", "./kibic", NULL);
                exit(1);
            }

            counter++;
            if (counter > 30 && (rand() % 100 < 15)) { 
                rush_mode = !rush_mode;
                counter = 0;
            }

            if (rush_mode) usleep(2000 + (rand() % 4000)); 
            else usleep(200000 + (rand() % 300000)); 
        }
        exit(0);
    }
}

// Funkcja wyświetlająca dashboard (bez wbudowanego sem_lock, aby kontrolować to w main)
void draw_ui() {
    printf("\033[H\033[J"); // Czyści ekran
    printf(CLR_BOLD "=== SYSTEM ZARZĄDZANIA HALĄ SPORTOWĄ ===\n" CLR_RESET);
    printf("Pojemność K: %d | Kolejka do kas: %d | Czynne kasy: %d\n", 
           hala->K, hala->queue_to_cashiers, hala->active_cashiers_count);
    
    printf("----------------------------------------------------\n");
    printf("SEKTORY (Pojemność sek: %d):\n", hala->sector_capacity);
    for(int i=0; i<MAX_SECTORS; i++) {
        char *status = (hala->sector_signal_status[i] == 0) ? CLR_GREEN "OTWARTY" CLR_RESET : CLR_RED "STOP   " CLR_RESET;
        printf("Sektor %d: [%s] Ludzi: %3d | Kontrola: [%d/%d A:%d] [%d/%d A:%d]\n", 
               i+1, status, hala->people_in_sector[i], 
               hala->security[i][0].occupied_count, MAX_AT_SECURITY, hala->security[i][0].team_id,
               hala->security[i][1].occupied_count, MAX_AT_SECURITY, hala->security[i][1].team_id);
    }
    printf("VIP Sektor: %d\n", hala->vip_count);
    printf("Status meczu: %s\n", hala->is_match_started ? CLR_CYAN "TRWA" CLR_RESET : "OCZEKIWANIE");
    printf("Ewakuacja: %s\n", hala->evacuation_mode ? CLR_RED "TAK" CLR_RESET : "NIE");
    printf("----------------------------------------------------\n");
    printf("1: Wstrzymaj sektor | 2: Wznów sektor | 3: Ewakuacja | 4: Start Meczu (Tp) | 5: Wyjście\n");
    printf("Wybór: ");
}

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_sigint);

    // Pobranie danych startowych (bez czyszczenia ekranu, zwykły input)
    int K = 0;
    printf("Podaj pojemność hali K: ");
    if (scanf("%d", &K) != 1) K = 100;
    if (K < 8) K = 80;

    // 1. Inicjalizacja IPC
    key_t key_shm = get_shm_key(FTOK_PATH, SHM_ID);
    shmid = shmget(key_shm, sizeof(ArenaState), IPC_CREAT | 0600);
    check_error(shmid, "shmget");
    hala = (ArenaState*)shmat(shmid, NULL, 0);
    
    // Reset stanu
    hala->K = K;
    hala->sector_capacity = K / 8;
    hala->queue_to_cashiers = 0;
    hala->active_cashiers_count = 2;
    hala->is_match_started = 0;
    hala->evacuation_mode = 0;
    hala->vip_count = 0;
    for(int i=0; i<MAX_SECTORS; i++) {
        hala->sector_signal_status[i] = 0;
        hala->people_in_sector[i] = 0;
        hala->tickets_sold[i] = 0;
        for(int j=0; j<SECURITY_PER_GATE; j++) {
            hala->security[i][j].occupied_count = 0;
            hala->security[i][j].team_id = 0;
        }
    }

    key_t key_sem = get_sem_key(FTOK_PATH, SEM_ID);
    semid = semget(key_sem, 1, IPC_CREAT | 0600);
    check_error(semid, "semget");
    semctl(semid, 0, SETVAL, 1);

    // 2. Uruchomienie procesów
    for(int i=0; i<MAX_CASHIERS; i++) {
        if(fork() == 0) {
            char id[5]; sprintf(id, "%d", i);
            execl("./kasjer", "./kasjer", id, NULL);
            exit(1);
        }
    }

    for(int i=0; i<MAX_SECTORS; i++) {
        if(fork() == 0) {
            char id[5]; sprintf(id, "%d", i);
            execl("./ochrona", "./ochrona", id, NULL);
            exit(1);
        }
    }

    run_fan_generator(K);

    // 3. Pętla sterująca (Poprawiona logika UI)
    struct pollfd fds = { .fd = STDIN_FILENO, .events = POLLIN };
    int cmd = 0;

    while(1) {
        // LOCK -> Rysuj -> UNLOCK -> Czekaj na input
        semop(semid, &lock, 1);
        draw_ui();
        fflush(stdout); // Ważne: wypchnij tekst na ekran przed odblokowaniem
        semop(semid, &unlock, 1);
        
        // Czekaj na input przez 2000ms (2 sekundy)
        // Jeśli wpiszesz coś w tym czasie, poll zwróci > 0
        int ret = poll(&fds, 1, 2000);

        if (ret > 0) {
            // Użytkownik coś wpisał! Odczytaj to.
            if (scanf("%d", &cmd) == 1) {
                
                semop(semid, &lock, 1); // Blokada na czas zmiany danych
                if (cmd == 1) { 
                    int s; 
                    printf("\nKtóry sektor (1-8) wstrzymać? "); 
                    scanf("%d", &s);
                    if(s>=1 && s<=8) {
                        hala->sector_signal_status[s-1] = 1;
                        log_event("KIEROWNIK", "Wysłano sygnał 1 (Stop)");
                    }
                }
                else if (cmd == 2) { 
                    int s; 
                    printf("\nKtóry sektor (1-8) wznowić? "); 
                    scanf("%d", &s);
                    if(s>=1 && s<=8) {
                        hala->sector_signal_status[s-1] = 0;
                        log_event("KIEROWNIK", "Wysłano sygnał 2 (Start)");
                    }
                }
                else if (cmd == 3) { 
                    hala->evacuation_mode = 1;
                    log_event("KIEROWNIK", "EWAKUACJA! Sygnał 3");
                }
                else if (cmd == 4) {
                    hala->is_match_started = 1;
                    log_event("KIEROWNIK", "Start meczu (Tp)");
                }
                else if (cmd == 5) {
                    semop(semid, &unlock, 1);
                    handle_sigint(0);
                }
                semop(semid, &unlock, 1);
            }
            // Po wykonaniu komendy pętla wraca na początek i od razu odświeża ekran
        }
        // Jeśli ret == 0 (timeout), pętla wraca na początek i odświeża ekran
    }

    return 0;
}