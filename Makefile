.phony all:
all: mts

mts: mts.c
	gcc mts.c -pthread -o mts

.PHONY clean:
clean:
	-rm -rf *.o *.exe
