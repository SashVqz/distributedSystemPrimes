rm primes.txt 
gcc -o program program.c
./program 800000000 2000 4 1
sort primes.txt > countprimes.txt
