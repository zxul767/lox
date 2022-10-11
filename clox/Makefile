main: main.o bytecode.o memory.o debug.o value.o
	$(CC) -o main main.o bytecode.o memory.o debug.o value.o

test: main
	./main

main.o: main.c bytecode.h common.h
bytecode.o: bytecode.h memory.h value.h
value.o: value.h memory.h
debug.o: debug.c debug.h
memory.o: memory.h

clean:
	rm main main.o bytecode.o memory.o debug.o value.o
