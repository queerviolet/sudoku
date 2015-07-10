all: sudoku sudoku.s sudoku-debug sudoku-debug.s

sudoku: sudoku.c
	cc -Ofast -o sudoku sudoku.c

sudoku.s: sudoku.c
	cc -S -Ofast -o sudoku.s sudoku.c

sudoku-debug: sudoku.c
	cc -g -o sudoku-debug sudoku.c

sudoku-debug.s: sudoku.c
	cc -S -g -o sudoku-debug.s sudoku.c

clean:
	rm -rf sudoku sudoku.s sudoku-debug sudoku-debug.s

.PHONY: all clean