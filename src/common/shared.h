#ifndef SHARED_H
#define SHARED_H

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <time.h>

#define FTOK_PATH "."
#define SHM_ID 1234
#define SEM_ID 5678

// Stałe z zadania
#define MAX_SECTORS 8
#define MAX_CASHIERS 10
#define SECURITY_PER_GATE 2
#define MAX_AT_SECURITY 3

// Kolory do konsoli
#define CLR_RED     "\x1b[31m"
#define CLR_GREEN   "\x1b[32m"
#define CLR_YELLOW  "\x1b[33m"
#define CLR_BLUE    "\x1b[34m"
#define CLR_MAGENTA "\x1b[35m"
#define CLR_CYAN    "\x1b[36m"
#define CLR_RESET   "\x1b[0m"
#define CLR_BOLD    "\x1b[1m"

// Struktura pojedynczego stanowiska kontroli
typedef struct {
    int occupied_count; // Ile osób jest kontrolowanych (max 3)
    int team_id;        // 0=Brak, 1=DrużynaA, 2=DrużynaB (wymóg: ta sama drużyna)
} SecurityPost;

// Struktura stanu całej hali (Pamięć dzielona)
typedef struct {
    int K; // Pojemność całkowita
    int sector_capacity; // K/8
    int is_match_started; // Czy wybiła godzina Tp
    int evacuation_mode;  // Sygnał 3

    // KASY
    int queue_to_cashiers; // Liczba osób w kolejce do kas
    int active_cashiers_count; // Liczba aktualnie czynnych kas (wyliczana)
    int tickets_sold[MAX_SECTORS]; // Sprzedane bilety na sektory
    
    // SEKTORY I BRAMKI
    // sector_status: 0=Otwarte, 1=Wstrzymane (Sygnał 1), 2=Zablokowane całkowicie
    int sector_signal_status[MAX_SECTORS]; 
    int people_in_sector[MAX_SECTORS];
    int vip_count;

    // KONTROLA BEZPIECZEŃSTWA
    // 8 sektorów, każdy ma 2 stanowiska kontrolne
    SecurityPost security[MAX_SECTORS][SECURITY_PER_GATE];

} ArenaState;

// Funkcje pomocnicze (takie same jak w Twoim projekcie)
static inline key_t get_shm_key(const char *path, int id) {
    key_t key = ftok(path, id);
    if (key == -1) { perror("ftok shm"); exit(1); }
    return key;
}

static inline key_t get_sem_key(const char *path, int id) {
    key_t key = ftok(path, id);
    if (key == -1) { perror("ftok sem"); exit(1); }
    return key;
}

static inline void check_error(int result, const char *msg) {
    if (result == -1) {
        perror(msg);
        exit(EXIT_FAILURE);
    }
}

// Prosty logger do pliku (wymóg zadania)
static inline void log_event(const char *who, const char *msg) {
    FILE *f = fopen("raport_symulacji.txt", "a");
    if (f) {
        time_t now = time(NULL);
        char *t_str = ctime(&now);
        t_str[24] = '\0'; // usunięcie \n
        fprintf(f, "[%s] [%s] %s\n", t_str, who, msg);
        fclose(f);
    }
}

#endif