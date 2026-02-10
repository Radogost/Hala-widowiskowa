#!/bin/bash

# 1. Najpierw kompilacja, żeby mieć pewność, że kod jest świeży
make

echo ""
echo "=========================================="
echo "      TESTY SYSTEMU STADIONOWEGO"
echo "=========================================="
echo "1. Test Podstawowy (Spokojny ruch, N=100)"
echo "   -> Cel: Sprawdzenie czy system działa stabilnie."
echo ""
echo "2. Test Skalowania 'AUTOBUS' (N=500)"
echo "   -> Cel: Sprawdzenie wymogu K/10."
echo "   -> Co obserwować: Nagły skok kolejki i otwarcie kas nr 3, 4."
echo ""
echo "3. Test Przepełnienia 'SZTURM' (N=200)"
echo "   -> Cel: Sprawdzenie limitów i działania ochrony."
echo "   -> Generator tworzy kibiców bardzo szybko."
echo ""

read -p "Wybierz numer testu: " test_no

case $test_no in
  1)
    # Test podstawowy
    ./kierowniktest --n 100 --testnum 1
    ;;
  
  2)
    # Test kluczowy dla projektu (Autobus)
    # Pojemność 500 -> Próg otwarcia nowej kasy to 50 osób.
    # Skrypt w C (kierowniktest.c) będzie generował "autobusy" (nagłe grupy > 60 osób).
    ./kierowniktest --n 500 --testnum 2
    ;;

  3)
    # Test stresowy (dużo ludzi, mała hala)
    ./kierowniktest --n 200 --testnum 3
    ;;
    
  *)
    echo "Nieznany numer testu."
    ;;
esac