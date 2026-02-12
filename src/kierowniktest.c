#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <string.h>
#include <poll.h>
#include <errno.h>

// Wskazujemy poprawną ścieżkę do shared.h
#include "common/shared.h"

// --- ZMIENNE GLOBALNE ---
int shmid, semid;
pid_t pids_cashiers[MAX_CASHIERS];
pid_t pid_generator;

// Operacje na semaforach
struct sembuf lock = {0, -1, 0};
struct sembuf unlock = {0, 1, 0};

ArenaState *hala; 

// --- PROTOTYPY ---
void cleanup();
void handle_sigint(int sig);
void run_test_generator(int K, int test_mode);
void print_test_dashboard(int test_mode, double elapsed);

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_sigint);

    // Parsowanie argumentów
    int K = 500;
    int testnum = 1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--n") == 0) K = atoi(argv[++i]);
        if (strcmp(argv[i], "--testnum") == 0) testnum = atoi(argv[++i]);
    }

    srand(time(NULL));

    // --- CZYSZCZENIE RAPORTU ---
    FILE *f = fopen("raport_symulacji.txt", "w");
    if (f) {
        fprintf(f, "--- START NOWEJ SYMULACJI (TRYB TEST: %d) ---\n", testnum);
        fclose(f);
    }

    // --- INICJALIZACJA IPC ---
    key_t key_shm = get_shm_key(FTOK_PATH, SHM_ID);
    key_t key_sem = get_sem_key(FTOK_PATH, SEM_ID);

    shmid = shmget(key_shm, sizeof(ArenaState), IPC_CREAT | 0600);
    check_error(shmid, "shmget");

    semid = semget(key_sem, 1, IPC_CREAT | 0600);
    check_error(semid, "semget");
    
    semctl(semid, 0, SETVAL, 1);

    hala = (ArenaState*)shmat(shmid, NULL, 0);
    if (hala == (void*)-1) { perror("shmat"); exit(1); }

    // --- RESETOWANIE STANU HALI ---
    semop(semid, &lock, 1);
    hala->K = K;
    hala->sector_capacity = K / 8; 
    hala->queue_to_cashiers = 0;
    hala->active_cashiers_count = 2; 
    hala->evacuation_mode = 0;
    hala->is_match_started = 0;
    hala->vip_count = 0;
    
    for(int i=0; i<MAX_SECTORS; i++) {
        hala->people_in_sector[i] = 0;
        hala->tickets_sold[i] = 0;
        hala->sector_signal_status[i] = 0;
        for(int j=0; j<SECURITY_PER_GATE; j++) {
            hala->security[i][j].occupied_count = 0;
            hala->security[i][j].team_id = 0;
        }
    }
    semop(semid, &unlock, 1);

    // 1. Uruchomienie Ochrony
    for(int i=0; i<MAX_SECTORS; i++) {
        if (fork() == 0) {
            char id[5]; sprintf(id, "%d", i);
            execl("./ochrona", "./ochrona", id, NULL);
            exit(1);
        }
    }

    // 2. Uruchomienie Kasjerów
    for (int i = 0; i < MAX_CASHIERS; i++) {
        if (fork() == 0) {
            char id_str[10]; sprintf(id_str, "%d", i);
            execl("./kasjer", "./kasjer", id_str, NULL);
            exit(1);
        }
    }

    // 3. Uruchomienie Generatora
    if ((pid_generator = fork()) == 0) {
        run_test_generator(K, testnum);
        exit(0);
    }

    // --- PĘTLA UI Z AUTOMATYKĄ ---
    struct pollfd fds = { .fd = STDIN_FILENO, .events = POLLIN };
    int cmd = 0;
    
    time_t start_time = time(NULL);
    int match_triggered = 0;
    int evac_triggered = 0;

    while (cmd != 9) {
        time_t current_time = time(NULL);
        double elapsed = difftime(current_time, start_time);

        print_test_dashboard(testnum, elapsed);
        
        // SCENARIUSZ AUTOMATYCZNY DLA TESTU 4
        if (testnum == 4) {
            // Start meczu po 15 sekundach
            if (elapsed >= 15 && !match_triggered) {
                semop(semid, &lock, 1);
                hala->is_match_started = 1;
                semop(semid, &unlock, 1);
                log_event("SYSTEM", "Automatyczne rozpoczecie meczu (Tp)");
                match_triggered = 1;
            }

            // Ewakuacja po 30 sekundach
            if (elapsed >= 30 && !evac_triggered) {
                semop(semid, &lock, 1);
                hala->evacuation_mode = 1;
                semop(semid, &unlock, 1);
                log_event("SYSTEM", "Automatyczne URUCHOMIENIE EWAKUACJI");
                evac_triggered = 1;
            }
        }

        // Obsługa wejścia użytkownika
        if (poll(&fds, 1, 500) > 0) {
            if (scanf("%d", &cmd) == 1) {
                if (cmd == 9) break;
                
                semop(semid, &lock, 1);
                if (cmd == 3) { 
                    hala->evacuation_mode = 1; 
                    log_event("KIEROWNIK", "Reczna ewakuacja"); 
                }
                if (cmd == 4) { 
                    hala->is_match_started = 1; 
                    log_event("KIEROWNIK", "Reczny start meczu"); 
                }
                semop(semid, &unlock, 1);
            }
        }
    }

    cleanup();
    return 0;
}

void run_test_generator(int K, int test_mode) {
    srand(getpid());
    int total_fans = (test_mode == 3) ? K + 300 : K + (int)(K * 0.2);

    for (int i = 0; i < total_fans; i++) {
        // TEST 2: AUTOBUS
        if (test_mode == 2 && (rand() % 100 < 5)) {
            int bus_size = (K / 8) + (rand() % 20);
            for (int b = 0; b < bus_size; b++) {
                if (fork() == 0) { execl("./kibic", "./kibic", NULL); exit(1); }
                i++;
            }
            usleep(800000);
            continue;
        }

        if (fork() == 0) {
            execl("./kibic", "./kibic", NULL);
            exit(1);
        }

        if (test_mode == 3) usleep(5000); 
        else usleep(100000 + (rand() % 150000));
    }
}

void print_test_dashboard(int test_mode, double elapsed) {
    semop(semid, &lock, 1); 
    
    int total_spectators = 0;
    for(int i=0; i<MAX_SECTORS; i++) total_spectators += hala->people_in_sector[i];

    printf("\033[H\033[J"); 
    printf("====================================================\n");
    printf(CLR_MAGENTA " TEST SYSTEMU STADIONOWEGO | Czas: %.0fs | Tryb: %d\n" CLR_RESET, elapsed, test_mode);
    printf("====================================================\n");
    
    printf(" POJEMNOŚĆ (K): %d | NA TRYBUNACH: %d\n", hala->K, total_spectators);
    printf(" KOLEJKA DO KAS: " CLR_BOLD "%d" CLR_RESET " osób\n", hala->queue_to_cashiers);
    printf(" STATUS: %s | EWAKUACJA: %s\n", 
           hala->is_match_started ? CLR_CYAN "MECZ TRWA" CLR_RESET : "OCZEKIWANIE",
           hala->evacuation_mode ? CLR_RED "AKTYWNA" CLR_RESET : "BRAK");
    
    int threshold = hala->K / 10; 
    if(threshold == 0) threshold = 1;
    
    printf("----------------------------------------------------\n");
    printf(" WYMÓG ZADANIA: 1 kasa na %d osób w kolejce\n", threshold);
    printf(" KASY: ");
    for (int i = 0; i < MAX_CASHIERS; i++) {
        if (i < hala->active_cashiers_count) printf("[" CLR_GREEN "K%d" CLR_RESET "] ", i+1);
        else printf("[" CLR_RED " Z " CLR_RESET "] ");
    }
    
    if (test_mode == 4) {
        printf("\n\n" CLR_YELLOW "SCENARIUSZ AUTOMATYCZNY:" CLR_RESET);
        if (elapsed < 15) printf("\n -> Start meczu za: %.0fs", 15 - elapsed);
        else if (elapsed < 30) printf("\n -> Ewakuacja za: %.0fs", 30 - elapsed);
        else printf("\n -> Trwa ewakuacja...");
    }

    printf("\n\n [3] Ewakuacja | [4] Start meczu | [9] Zakoncz\n");
    
    semop(semid, &unlock, 1);
}

void cleanup() {
    if (pid_generator > 0) kill(pid_generator, SIGKILL);
    kill(0, SIGTERM); 
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);
    printf("\n[KierownikTest] Zasoby IPC usuniete. Koniec.\n");
}

void handle_sigint(int sig) {
    cleanup();
    exit(0);
}