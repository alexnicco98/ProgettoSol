# Makefile
CC	=  gcc
CFLAGS	= -Wall -g
LDFLAGS = -L ./lib -Wl,-rpath=./lib
obj = ./src/main.c ./lib/libbt.so
TARGET = ./bin/myprog

.PHONY: all test1 clean

all: $(TARGET)

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

# for leak check use:
# valgrind --leak-check=full

# for find the origins of undefined values
# valgrind --track-origins=yes

# for view structures that are duplicated
# but not get deleted
# --show-reachable=yes
test1:	./bin/myprog
	@echo "Eseguo il test"
	valgrind --leak-check=full ./bin/myprog
	@echo "Test1 OK"

# diff out ./testfile/test1-out_atteso.txt

clean:
	@echo "Removing garbage"
	-rm -f ./bin/myprog
	-rm -f ./src/*.o *~ ./lib/libbt.so
#	-rmdir -p ./bin/myprog.dSYM with mac i should remove this dir
