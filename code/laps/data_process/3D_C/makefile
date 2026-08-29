CC = gcc

OPTIONS = -O3 -lm # -std=c99 -I/usr/local/include -L/usr/local/lib -lfftw3


EXE = main.exe 
MAIN = main.c
FILE01 = iofunctions.c mathlib.c statistics.c

OBJMAIN = ${MAIN:.c=.o}
OBJ01 = ${FILE01:.c=.o}
OBJ   = $(OBJMAIN) $(OBJ01)

$(EXE): $(OBJ)
	$(CC) -o $(EXE) $(OBJ) $(OPTIONS) 
$(OBJMAIN):
	$(CC) -c $(MAIN) $(OPTIONS)
$(OBJ01):
	$(CC) -c $(FILE01) $(OPTIONS)

clean:
	rm *.o