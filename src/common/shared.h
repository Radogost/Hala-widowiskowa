/**
 * @file shared.h
 * @brief Definicje stałych, struktur i funkcji pomocniczych dla systemu stadionowego.
 *
 * Plik zawiera definicję struktury ArenaState mapowanej w pamięci dzielonej (SHM),
 * klucze IPC oraz funkcje do logowania zdarzeń i obsługi błędów.
 * Jest dołączany przez wszystkie procesy w systemie.
 */

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

// #ifdef __linux__
// union semun {
//     int val;
//     struct semid_ds *buf;
//     unsigned short *array;
// };
// #endif

/**
 * @struct SecurityPost
 * @brief Reprezentuje pojedyncze stanowisko kontroli bezpieczeństwa.
 */
// Struktura pojedynczego stanowiska kontroli
typedef struct {
    int occupied_count; /**< Liczba osób aktualnie kontrolowanych (max 3). */
    int team_id;        /**< ID drużyny aktualnie obsługiwanej (zapobiega mieszaniu kibiców). */
} SecurityPost;

/**
 * @struct ArenaState
 * @brief Główny stan systemu przechowywany w Pamięci Dzielonej (Shared Memory).
 *
 * Struktura dostępna dla wszystkich procesów. Zawiera liczniki, flagi stanu
 * oraz tablice reprezentujące sektory i kasy. Dostęp do pól modyfikowalnych
 * musi być synchronizowany semaforem.
 */
// Struktura stanu całej hali (Pamięć dzielona)
typedef struct {
    int K;                          /**< Całkowita pojemność hali. */
    int sector_capacity;            /**< Pojemność pojedynczego sektora (K/8). */
    int is_match_started;           /**< Flaga Tp: 1 = Mecz trwa, 0 = Oczekiwanie. */
    int evacuation_mode;            /**< Sygnał ewakuacji: 1 = Ewakuacja, 0 = Normalna praca. */

    // KASY
    int queue_to_cashiers;          /**< Liczba osób w głównej kolejce (zmienna atomowa). */
    int active_cashiers_count;      /**< Liczba aktywnych kasjerów (skalowanie dynamiczne). */
    int tickets_sold[MAX_SECTORS];  /**< Liczba biletów sprzedanych na dany sektor. */
    
    // SEKTORY I BRAMKI
    int sector_signal_status[MAX_SECTORS]; /**< Status bramki: 0=Otwarta, 1=Zablokowana przez Kierownika. */
    int people_in_sector[MAX_SECTORS];     /**< Fizyczna liczba osób na sektorze. */
    int vip_count;                         /**< Licznik wejść VIP. */

    // KONTROLA BEZPIECZEŃSTWA
    SecurityPost security[MAX_SECTORS][SECURITY_PER_GATE]; /**< Stanowiska ochrony. */

} ArenaState;

// Funkcje pomocnicze
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

/**
 * @brief Rejestruje zdarzenie w pliku raportu z dokładnym znacznikiem czasu.
 * * @param who Nazwa modułu zgłaszającego (np. "KIEROWNIK", "KASJER").
 * @param msg Treść komunikatu.
 */
// Prosty logger do pliku
static inline void log_event(const char *who, const char *msg) {
    FILE *f = fopen("raport_symulacji.txt", "a");
    if (f) {
        time_t now = time(NULL);
        char *t_str = ctime(&now);
        t_str[24] = '\0';
        fprintf(f, "[%s] [%s] %s\n", t_str, who, msg);
        fclose(f);
    }
}

#endif