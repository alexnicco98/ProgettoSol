# Makefile
CC	=  gcc
CFLAGS	= -Wall -g
LDFLAGS = -L ./lib -Wl,-rpath=./lib
obj = ./src/supermercato.c ./lib/libbt.so
TARGET = ./bin/myprog

.PHONY: all test2 clean

./bin/myprog : $(obj)
	$(CC) -pthread $(CFLAGS) $^ -I ./include -o $@ $(LDFLAGS) -lbt

./lib/libbt.so : ./src/cliente.o  ./src/direttore.o ./src/cassiere.o
	$(CC) $(CFLAGS) -shared -o $@ $^

./src/direttore.o : ./src/direttore.c
	$(CC) $(CFLAGS) $< -I ./include -fPIC -c -o $@

./src/cliente.o : ./src/cliente.c
	$(CC) $(CFLAGS) $< -I ./include -fPIC -c -o $@

./src/cassiere.o : ./src/cassiere.c
	$(CC) $(CFLAGS) $< -I ./include -fPIC -c -o $@

all: $(TARGET)

test2:	./bin/myprog
	@echo "Eseguo il test"
	./bin/myprog
	@echo "Test2 OK"
	./analisi.sh

clean:
	@echo "Removing garbage"
	-rm -f ./bin/myprog
	-rm -f ./log.txt
	-rm -f ./src/*.o ./lib/libbt.so
