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

// POPRAWKA: Wskazujemy poprawną ścieżkę do shared.h
#include "common/shared.h"

// --- ZMIENNE GLOBALNE ---
int shmid, semid;
pid_t pids_cashiers[MAX_CASHIERS];
pid_t pid_security;
pid_t pid_generator;

// POPRAWKA: Definicja operacji na semaforach (lokalnie, bo shared.h tego nie ma)
struct sembuf lock = {0, -1, 0};
struct sembuf unlock = {0, 1, 0};

// POPRAWKA: Używamy ArenaState zamiast Hala
ArenaState *hala; 

// --- PROTOTYPY ---
void cleanup();
void handle_sigint(int sig);
void run_test_generator(int K, int test_mode);
void print_test_dashboard(int test_mode);

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

    // --- NOWE: CZYSZCZENIE RAPORTU ---
    FILE *f = fopen("raport_symulacji.txt", "w");
    if (f) {
        fprintf(f, "--- START NOWEJ SYMULACJI ---\n");
        fclose(f);
    }
    // ---------------------------------

    // --- INICJALIZACJA IPC (Zgodna z shared.h) ---
    // Używamy funkcji pomocniczych z Twojego shared.h
    key_t key_shm = get_shm_key(FTOK_PATH, SHM_ID);
    key_t key_sem = get_sem_key(FTOK_PATH, SEM_ID);

    shmid = shmget(key_shm, sizeof(ArenaState), IPC_CREAT | 0600);
    if (shmid == -1) { perror("shmget"); exit(1); }

    semid = semget(key_sem, 1, IPC_CREAT | 0600);
    if (semid == -1) { perror("semget"); exit(1); }
    
    // Inicjalizacja semafora na 1 (otwarty)
    semctl(semid, 0, SETVAL, 1);

    // Dołączenie pamięci
    hala = (ArenaState*)shmat(shmid, NULL, 0);
    if (hala == (void*)-1) { perror("shmat"); exit(1); }

    // --- RESETOWANIE STANU HALI ---
    semop(semid, &lock, 1);
    hala->K = K;
    hala->sector_capacity = K / 8; // Wyliczamy pojemność sektora
    hala->queue_to_cashiers = 0;
    hala->active_cashiers_count = 2; // Min. 2 kasy
    hala->evacuation_mode = 0;
    hala->is_match_started = 0;
    hala->vip_count = 0;
    
    // Zerowanie sektorów
    for(int i=0; i<MAX_SECTORS; i++) {
        hala->people_in_sector[i] = 0;
        hala->tickets_sold[i] = 0;
        hala->sector_signal_status[i] = 0;
        // Zerowanie ochrony
        for(int j=0; j<SECURITY_PER_GATE; j++) {
            hala->security[i][j].occupied_count = 0;
            hala->security[i][j].team_id = 0;
        }
    }
    semop(semid, &unlock, 1);

    printf("[Test] Start symulacji. K=%d, Test=%d\n", K, testnum);

    // 1. Uruchomienie Ochrony (Wszystkie sektory)
    for(int i=0; i<MAX_SECTORS; i++) {
        if (fork() == 0) {
            char id[5]; sprintf(id, "%d", i);
            execl("./ochrona", "./ochrona", id, NULL);
            exit(1);
        }
    }

    // 2. Uruchomienie Kasjerów
    for (int i = 0; i < MAX_CASHIERS; i++) {
        if ((pids_cashiers[i] = fork()) == 0) {
            char id_str[10];
            sprintf(id_str, "%d", i);
            execl("./kasjer", "./kasjer", id_str, NULL);
            exit(1);
        }
    }

    // 3. Uruchomienie Generatora (Testowego)
    if ((pid_generator = fork()) == 0) {
        run_test_generator(K, testnum);
        exit(0);
    }

    // --- PĘTLA UI ---
    struct pollfd fds = { .fd = STDIN_FILENO, .events = POLLIN };
    int cmd = 0;

    while (cmd != 9) {
        print_test_dashboard(testnum);
        
        // Czekamy 0.5s na odświeżenie
        if (poll(&fds, 1, 500) > 0) {
            scanf("%d", &cmd);
            if (cmd == 9) handle_sigint(SIGINT);
        }
    }

    cleanup();
    return 0;
}

// --- GENERATOR TESTOWY ---
void run_test_generator(int K, int test_mode) {
    srand(getpid());
    int total_fans = (test_mode == 3) ? K + 300 : K + (int)(K * 0.2);

    for (int i = 0; i < total_fans; i++) {
        
        // === TEST 2: AUTOBUS (SKALOWANIE KAS) ===
        if (test_mode == 2 && (rand() % 100 < 5)) { // 5% szans na autobus
            int bus_size = (K / 8) + (rand() % 20); // Grupa > 12% hali
            
            // Wpuszczamy grupę BŁYSKAWICZNIE
            for (int b = 0; b < bus_size; b++) {
                if (fork() == 0) { execl("./kibic", "./kibic", NULL); exit(1); }
                i++;
            }
            usleep(800000); // Przerwa po autobusie
            continue;
        }

        // Pojedynczy kibic
        if (fork() == 0) {
            execl("./kibic", "./kibic", NULL);
            exit(1);
        }

        if (test_mode == 3) usleep(5000); // Szturm
        else usleep(100000 + (rand() % 150000)); // Normalnie
    }
}

void print_test_dashboard(int test_mode) {
    semop(semid, &lock, 1); 
    
    // Obliczamy sumę kibiców na sektorach
    int total_spectators = 0;
    for(int i=0; i<MAX_SECTORS; i++) total_spectators += hala->people_in_sector[i];

    printf("\033[H\033[J"); 
    printf("==========================================\n");
    printf(CLR_MAGENTA " TEST SYSTEMU STADIONOWEGO (Tryb: %d)\n" CLR_RESET, test_mode);
    printf("==========================================\n");
    
    printf(" POJEMNOŚĆ: %d\n", hala->K);
    printf(" NA TRYBUNACH: %d\n", total_spectators);
    printf(" KOLEJKA:   " CLR_BOLD "%d" CLR_RESET " osób\n", hala->queue_to_cashiers);
    
    // Wyświetlanie kas zgodnie z regułą K/10
    int threshold = hala->K / 10; 
    if(threshold == 0) threshold = 1;
    
    printf("------------------------------------------\n");
    printf(" WYMÓG ZADANIA: 1 kasa na %d osób w kolejce\n", threshold);
    printf(" KASY: ");
    for (int i = 0; i < MAX_CASHIERS; i++) {
        if (i < hala->active_cashiers_count) printf("[" CLR_GREEN "K%d" CLR_RESET "] ", i+1);
        else printf("[" CLR_RED " Z " CLR_RESET "] ");
    }
    printf("\n\n [9] Zakończ test\n");
    
    semop(semid, &unlock, 1);
}

void cleanup() {
    if (pid_generator > 0) kill(pid_generator, SIGKILL);
    // Zabijamy wszystkie procesy potomne (kibice, ochrona, kasjerzy)
    kill(0, SIGTERM); 
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);
}

void handle_sigint(int sig) {
    cleanup();
    exit(0);
}