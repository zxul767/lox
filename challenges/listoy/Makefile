main: main.o list.o
	$(CC) -o main main.o list.o

test: main
	./main

main.o: main.c list.h
list.o: list.c list.h

clean:
	rm main main.o list.o
