CC = gcc
CFLAGS = -Wall -fPIC -g
LDFLAGS = -L.

build: libso_stdio.so

main: libso_stdio.so libutil.so main.o
	$(CC) main.o -o main -lso_stdio  -lutil $(LDFLAGS)

main.o: main.c
	$(CC) $(CFLAGS) -c $^

libutil.so: test_util.o
	$(CC) -shared test_util.o -o libutil.so

libso_stdio.so: so_stdio.o
	$(CC) -shared so_stdio.o -o libso_stdio.so

so_stdio.o: so_stdio.c
	$(CC) $(CFLAGS) -c $^

test_util.o: test_util.c
	$(CC) $(CFLAGS) -c $^

.PHONY:

run:
	./main

clean:
	rm -f *.o main libso_stdio.so libutil.so here _log
