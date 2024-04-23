CC = gcc
CFLAGS = -Wall -pthread -lrt
LFLAGS = -Wall -Wextra -pthread

.PHONY: all clean

hashmap.o: hashmap.c hashmap.h
	gcc -Wall -pthread -lrt -c -o hashmap.o hashmap.c

util.o: util.c util.h
	gcc -Wall -pthread -lrt -c -o util.o util.c

#####################################################################

all: th-lookup

th-lookup: th-lookup.o hashmap.o util.o
	gcc -Wall -Wextra -pthread th-lookup.o hashmap.o util.o -o $@

th-lookup.o: th-lookup.c th-lookup.h
	gcc -Wall -pthread -lrt -c -o th-lookup.o th-lookup.c

#####################################################################

clean:
	rm -f th-lookup 
	rm -f *.o
	rm -f *~
	rm -f /dev/shm/*
	rm -f results.txt