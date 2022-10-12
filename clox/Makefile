CFLAGS = -g # Optional. Not required for proper compilation
ALL_CFLAGS = -I. $(CFLAGS) # -I. is required for proper compilation
LDFLAGS = -rdynamic
CC = gcc

#
# TODO: investigate how to remove all redundancy in this file
#
main: main.o vm.o scanner.o compiler.o bytecode.o value.o memory.o debug.o
	$(CC) -o main $(CFLAGS) $(LDFLAGS) main.o vm.o scanner.o compiler.o value.o bytecode.o memory.o debug.o

test: main
	./main

main.o: main.c bytecode.h common.h
	$(CC) -c $(ALL_CFLAGS) main.c

vm.o: vm.c vm.h
	$(CC) -c $(ALL_CFLAGS) vm.c

compiler.o: compiler.c common.h scanner.h
	$(CC) -c $(ALL_CFLAGS) compiler.c

scanner.o: scanner.c scanner.h common.h
	$(CC) -c $(ALL_CFLAGS) scanner.c

bytecode.o: bytecode.c bytecode.h memory.h value.h
	$(CC) -c $(ALL_CFLAGS) bytecode.c

value.o: value.c value.h memory.h
	$(CC) -c $(ALL_CFLAGS) value.c

debug.o: debug.c debug.h
	$(CC) -c $(ALL_CFLAGS) debug.c

memory.o: memory.c memory.h
	$(CC) -c $(ALL_CFLAGS) memory.c

clean:
	rm main *.o
