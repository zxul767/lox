main: main.o vm.o bytecode.o value.o memory.o debug.o
	$(CC) -o main main.o vm.o value.o bytecode.o memory.o debug.o

test: main
	./main

main.o: main.c bytecode.h common.h
vm.o: vm.c vm.h
bytecode.o: bytecode.c bytecode.h memory.h value.h
value.o: value.c value.h memory.h
debug.o: debug.c debug.h
memory.o: memory.c memory.h

clean:
	rm main *.o
