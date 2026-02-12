CC = gcc
CFLAGS = -Wall -g -I./common/
HEADER = common/shared.h

# Dodaj kierowniktest do listy all
all: kierownik kasjer ochrona kibic kierowniktest

kierownik: kierownik.c $(HEADER)
	$(CC) $(CFLAGS) kierownik.c -o kierownik

kasjer: kasjer.c $(HEADER)
	$(CC) $(CFLAGS) kasjer.c -o kasjer

ochrona: ochrona.c $(HEADER)
	$(CC) $(CFLAGS) ochrona.c -o ochrona

kibic: kibic.c $(HEADER)
	$(CC) $(CFLAGS) kibic.c -o kibic

# --- TESTY KIEROWNIKA ---
kierowniktest: kierowniktest.c $(HEADER)
	$(CC) $(CFLAGS) kierowniktest.c -o kierowniktest

clean:
	rm -f kierownik kasjer ochrona kibic kierowniktest raport_symulacji.txt
	rm -rf *.dSYM