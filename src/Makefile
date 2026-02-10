CC = gcc
CFLAGS = -Wall -g -I./common/
HEADER = common/shared.h

all: kierownik kasjer ochrona kibic

kierownik: kierownik.c $(HEADER)
	$(CC) $(CFLAGS) kierownik.c -o kierownik

kasjer: kasjer.c $(HEADER)
	$(CC) $(CFLAGS) kasjer.c -o kasjer

ochrona: ochrona.c $(HEADER)
	$(CC) $(CFLAGS) ochrona.c -o ochrona

kibic: kibic.c $(HEADER)
	$(CC) $(CFLAGS) kibic.c -o kibic

clean:
	rm -f kierownik kasjer ochrona kibic magazyn.dat raport_symulacji.txt
	rm -rf *.dSYM