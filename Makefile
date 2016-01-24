CFLAGS = -g -DCELLS_PER_COLUMN=1024

all: life life_debug

life : dist.o core.o
	cilk++ -o life ./dist.o ./core.o -lcilkutil

life_debug : dist_debug.o core.o
	cilk++ -o life_debug ./dist_debug.o ./core.o -lcilkutil

clean:
	rm life dist.o core.o dist_debug.o

core.o : core.c
	cilk++ $(CFLAGS) -O3 -c ./core.c

dist.o : dist.c
	cilk++ $(CFLAGS) -c dist.c

dist_debug.o : dist.c
	cilk++ $(CFLAGS) -DDEBUG -c dist.c -o dist_debug.o
