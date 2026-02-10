#include "common.h"
#include "ipc_utils.h"
#include <sys/wait.h>

int shm_id = -1;
int sem_id = -1;
SharedState *stan = NULL;

// Sprzątanie procesów zombie (aby system nie "zatykał się" zakończonymi procesami)
void handle_zombie(int sig) {
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// Sprzątanie zasobów przy wyjściu (Ctrl+C lub błąd)
void clean_exit(int sig) {
    printf("\n[KIEROWNIK] Otrzymano sygnał %d. Zamykanie systemu...\n", sig);
    
    if (sig == SIGSEGV) printf("!!! BŁĄD KRYTYCZNY (Segmentation Fault) !!!\n");

    // 1. Zabijamy wszystkie procesy potomne (kasjerów, kibiców)
    kill(0, SIGTERM);
    
    // 2. Usuwamy pamięć dzieloną
    if (shm_id != -1) {
        shmctl(shm_id, IPC_RMID, NULL);
        printf("-> Pamięć usunięta.\n");
    }

    // 3. Usuwamy semafory
    if (sem_id != -1) {
        semctl(sem_id, 0, IPC_RMID);
        printf("-> Semafory usunięte.\n");
    }

    exit(0);
}

int main() {
    // Rejestracja sygnałów
    signal(SIGINT, clean_exit);  
    signal(SIGTERM, clean_exit); 
    signal(SIGSEGV, clean_exit); 

    // Obsługa zombie (ważne przy setkach kibiców!)
    struct sigaction sa;
    sa.sa_handler = handle_zombie;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);

    printf("[KIEROWNIK] Start systemu hali sportowej (Pojemność: %d)...\n", POJEMNOSC_HALI);

    // --- 1. TWORZENIE ZASOBÓW ---
    shm_id = shmget(SHM_KEY, sizeof(SharedState), IPC_CREAT | 0666);
    check_error(shm_id, "shmget");
    
    stan = (SharedState *)shmat(shm_id, NULL, 0);
    if (stan == (void *)-1) check_error(-1, "shmat");

    // Inicjalizacja zerowa
    stan->aktywne_kasy = 0;
    stan->liczba_kibicow_w_kolejce = 0;
    stan->liczba_vip_w_kolejce = 0;
    stan->ewakuacja = 0;
    
    // Ustawienie biletów na start
    for(int i=0; i<LICZBA_SEKTOROW; i++) {
        stan->bilety_dostepne[i] = POJEMNOSC_SEKTORA;
    }

    // --- 2. TWORZENIE SEMAFORÓW ---
    sem_id = semget(SEM_KEY, 20, IPC_CREAT | 0666);
    check_error(sem_id, "semget");
    sem_init(sem_id, SEM_MUTEX_MAIN, 1); // Mutex otwarty

    // --- 3. START POCZĄTKOWYCH KASJERÓW ---
    // Zgodnie z zasadą: "Zawsze działają min. 2 stanowiska"
    printf("[KIEROWNIK] Uruchamiam 2 kasy startowe...\n");
    for(int i=0; i<2; i++) {
        if(fork() == 0) { execl("./kasjer", "kasjer", NULL); exit(1); }
    }

    printf("[KIEROWNIK] System gotowy. Rozpoczynam symulację.\n");

    // --- 4. PĘTLA GŁÓWNA ---
    while(1) {
        sleep(1); // Cykl sterowania co 1 sekundę

        // 0. SPRAWDZENIE STANU BILETÓW
        int suma_biletow = 0;
        for(int i=0; i<LICZBA_SEKTOROW; i++) {
            suma_biletow += stan->bilety_dostepne[i];
        }

        // A. GENERATOR KIBICÓW (FALA UDERZENIOWA)
        // Generujemy nowych TYLKO jeśli są jeszcze bilety!
        if (suma_biletow > 0) {
            static int licznik_cykli = 0;
            licznik_cykli++;

            if (licznik_cykli % 3 == 0) {
                int wielkosc_fali = (rand() % 40) + 20; 
                printf("\n[KIEROWNIK] >>> NADCHODZI FALA KIBICÓW (%d osób)! <<<\n", wielkosc_fali);
                
                for (int i = 0; i < wielkosc_fali; i++) {
                    if (fork() == 0) {
                        execl("./kibic", "kibic", NULL);
                        exit(1);
                    }
                }
            }
        } 
        else {
            printf("\n[KIEROWNIK] Brak biletów - wstrzymano wpuszczanie nowych kibiców.\n");
        }

        // B. DYNAMICZNE ZARZĄDZANIE KASAMI
        int kolejka = stan->liczba_kibicow_w_kolejce + stan->liczba_vip_w_kolejce;
        int prog_otwarcia = POJEMNOSC_HALI / 10; 
        
        int potrzebne_kasy = (kolejka / prog_otwarcia) + 1;
        if (potrzebne_kasy < 2) potrzebne_kasy = 2; // Minimum zawsze 2 (chyba że koniec biletów)
        if (potrzebne_kasy > MAX_KAS) potrzebne_kasy = MAX_KAS;

        // Jeśli bilety się skończyły, nie otwieramy nowych kas (kasjerzy sami się zamkną)
        if (suma_biletow > 0 && stan->aktywne_kasy < potrzebne_kasy) {
            int do_otwarcia = potrzebne_kasy - stan->aktywne_kasy;
            printf("\n[KIEROWNIK] DUŻA KOLEJKA (%d os.)! Otwieram %d nowe kasy...\n", kolejka, do_otwarcia);
            
            for(int k=0; k<do_otwarcia; k++) {
                if(fork() == 0) { execl("./kasjer", "kasjer", NULL); exit(1); }
                usleep(100000); 
            }
        }

        // C. RAPORT STANU
        printf("\n========= RAPORT KIEROWNIKA =========\n");
        printf(" Kolejka (Zwykli): %d\n", stan->liczba_kibicow_w_kolejce);
        printf(" Kolejka (VIP):    %d\n", stan->liczba_vip_w_kolejce);
        printf(" Aktywne Kasy:     %d\n", stan->aktywne_kasy);
        printf(" RAZEM WOLNYCH MIEJSC: %d\n", suma_biletow);
        printf("=====================================\n");

        // D. WARUNEK ZAKOŃCZENIA SYMULACJI (ETAP SPRZEDAŻY)
        // Jeśli nie ma biletów, kasy zamknięte i kolejka pusta -> KONIEC
        if (suma_biletow == 0 && stan->aktywne_kasy == 0 && kolejka == 0) {
            printf("\n[KIEROWNIK] Wszystkie bilety sprzedane, kasy zamknięte.\n");
            printf("[KIEROWNIK] Koniec etapu sprzedaży.\n");
            break; // Wyjście z pętli while(1) -> przejście do clean_exit
        }
    }

    // Wywołanie funkcji sprzątającej na końcu main
    clean_exit(0);
    return 0;
}