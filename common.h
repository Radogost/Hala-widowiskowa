#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

// --- KONFIGURACJA ---
#define SHM_KEY 0x18001
#define SEM_KEY 0x18002

// Zmiana pojemności na 1000, aby VIP (0.3%) miał sens matematyczny
#define POJEMNOSC_HALI 1000
#define LICZBA_SEKTOROW 8
#define POJEMNOSC_SEKTORA (POJEMNOSC_HALI / LICZBA_SEKTOROW)

#define MAX_KAS 10
#define MAX_NA_BRAMCE 3

// --- STRUKTURA PAMIĘCI DZIELONEJ ---
typedef struct {
    // Kasy
    int liczba_kibicow_w_kolejce;
    int liczba_vip_w_kolejce;     // <--- TEGO BRAKOWAŁO!
    int aktywne_kasy;

    // Bilety
    int bilety_dostepne[LICZBA_SEKTOROW];

    // Bramki (8 sektorów, 2 stanowiska na sektor)
    struct {
        int zajete_miejsca;       
        int obslugiwana_druzyna;  
    } bramki[LICZBA_SEKTOROW][2];

    // Flaga końca
    int ewakuacja;

} SharedState;

// --- SEMAFORY ---
#define SEM_MUTEX_MAIN 0      
#define SEM_GATE_BASE 1       

#endif