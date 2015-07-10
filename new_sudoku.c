#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define SUDOKU_BLK_SZ 3
#define SUDOKU_SZ (SUDOKU_BLK_SZ * SUDOKU_BLK_SZ)
#define SUDOKU_NUM_CELLS (SUDOKU_SZ * SUDOKU_SZ)

/****** Bitsets ******/
typedef uint16_t bitset;

#define set_has(bitset, sym) (1 & (bitset >> (sym - 1)))

void set_print(bitset mask) {
  for (uint8_t sym = 1; sym <= SUDOKU_SZ; ++sym) {
    if (set_has(mask, sym)) {
      printf("%u, ", sym);
    }
  }
}

typedef struct {
  uint8_t *value;
  bitset *row;
  bitset *col;
  bitset *block;
  uint8_t possibilities;
} cell_t;

#define cell_taken(cell) (*((cell).row) | *((cell).col) | *((cell).block))
#define cell_valid(cell, sym) (!set_has(cell_taken(cell), sym))

// doesn't check validity
void cell_set(cell_t *cell, uint8_t symbol) {
  *cell->value = symbol;
  uint16_t mask = 1 << (symbol - 1);
  *cell->row |= mask;
  *cell->col |= mask;
  *cell->block |= mask;
}

void cell_clear(cell_t *cell) {
  uint8_t symbol = *cell->value;
  *cell->value = 0;
  uint16_t mask = ~(1 << (symbol - 1));
  *cell->row &= mask;
  *cell->col &= mask;
  *cell->block &= mask;
}

uint8_t cell_num_possible(cell_t *cell) {
  uint16_t taken = *cell->row | *cell->col | *cell->block;
  uint8_t possibilities = 0;
  for (uint8_t sym = 1; sym <= SUDOKU_SZ; ++sym) {
    if (!set_has(taken, sym)) ++possibilities;
  }
  return (cell->possibilities = possibilities);
}

typedef struct {
  uint8_t cells[SUDOKU_NUM_CELLS];
  bitset rows[SUDOKU_SZ];
  bitset cols[SUDOKU_SZ];
  bitset blocks[SUDOKU_SZ];
  cell_t unsolved[SUDOKU_NUM_CELLS];
  cell_t *unsolved_end;  
  size_t num_unsolved;
} sudoku_t;

#define INDEX(row, col) (col + SUDOKU_SZ * row)

#define BLOCK(row, col) \
  (col / SUDOKU_BLK_SZ + \
  SUDOKU_BLK_SZ * (row / SUDOKU_BLK_SZ))  // integer division

void print_masks(uint16_t type[]) {
  for (size_t i = 0; i != SUDOKU_SZ; ++i) {
    uint16_t mask = type[i];
    set_print(mask);
    printf("\n");
  }
  printf("\n");
}

void sudoku_print(sudoku_t *board) {
  for (size_t row = 0; row != SUDOKU_SZ; ++row) {
    for (size_t col = 0; col != SUDOKU_SZ; ++col) {
      uint8_t sym = board->cells[INDEX(row, col)];
      if (sym) {
        printf("%d ", board->cells[INDEX(row, col)]);
      } else {
        printf("- ");
      }
    }
    printf("\n");
  }

  /*  
  printf("rows: "); print_masks(board->rows);
  printf("cols: "); print_masks(board->cols);
  printf("blocks: "); print_masks(board->blocks);
  */
}

void sudoku_add_unsolved(sudoku_t *board, cell_t cell) {
  board->unsolved[board->num_unsolved++] = cell;
  ++board->unsolved_end;
}

void sudoku_init(sudoku_t *board) {
  board->unsolved_end = board->unsolved;

  for (size_t row = 0; row != SUDOKU_SZ; ++row) {
    for (size_t col = 0; col != SUDOKU_SZ; ++col) {
      uint8_t sym = board->cells[INDEX(row, col)];
      cell_t cell = (cell_t) {
        .value = &board->cells[INDEX(row, col)],
        .row = &board->rows[row],
        .col = &board->cols[col],
        .block = &board->blocks[BLOCK(row, col)]
      };
      // printf("*(cell.value)=%d\n", *(cell.value));
      if (sym != 0) {
        if (!cell_valid(cell, sym)) {
          printf("failed at: row:%zu, col:%zu, sym:%u\n",
            row, col, sym);
          sudoku_print(board);
        }
        assert(cell_valid(cell, sym));
        cell_set(&cell, sym);
      } else {
        // empty space
        sudoku_add_unsolved(board, cell);
      }
    }
  }
}

cell_t * sudoku_next_unsolved(sudoku_t *board) {
  size_t best_pos = -1;
  cell_t * best_cell = NULL;
  for (cell_t *c = board->unsolved; c != board->unsolved_end; ++c) {
    if (*c->value) continue;
    int pos = cell_num_possible(c);
    if (pos == 0) return c;
    if (pos < best_pos) {
      best_cell = c;
      best_pos = pos;
    }
  }
  return best_cell;
}

typedef struct {
  cell_t *cell;
  uint16_t taken;
  uint8_t guess;
} guess_t;

size_t sudoku_solve(sudoku_t *board, size_t max_solutions) {
  sudoku_init(board);

  size_t solutions = 0;

  guess_t stack[SUDOKU_SZ * SUDOKU_SZ];
  guess_t *top = stack;

  cell_t *first = sudoku_next_unsolved(board);
  if (!first) {
    sudoku_print(board);
    return 1;
  }
  *top = (guess_t) {
    .cell = first,
    .taken = cell_taken(*first),
    .guess = 0
  };

  while (top >= stack && solutions < max_solutions) {
    ++top->guess;

    if (top->cell->value) {
      cell_clear(top->cell);
    }

    if (set_has(top->taken, top->guess)) {
      continue;
    }

    if (top->guess <= SUDOKU_SZ) {
      cell_set(top->cell, top->guess);
      if (board->num_unsolved == 1) {
        sudoku_print(board);
        ++solutions;
      } else {
        cell_t *next = sudoku_next_unsolved(board);
        if (next->possibilities > 0) {
          --board->num_unsolved;
          *(++top) = (guess_t) {
            .cell = next,
            .taken = cell_taken(*next),
            .guess = 0
          };
        }
      }
      continue;
    }

    ++board->num_unsolved;
    cell_clear(top->cell);
    --top;
  }

  return solutions;
}

// populates a sudoku solver
// returns true on success, false on failure.
int sudoku_read(sudoku_t *board) {
  memset(board, 0, sizeof(sudoku_t));
  for (size_t i = 0; i != SUDOKU_SZ * SUDOKU_SZ; ++i) {
    int c = getchar();
    do {
      if (c == '-') {
        board->cells[i] = 0;
        break;
      } else {
        uint8_t sym = c - '0';
        if (sym > 0 && sym <= SUDOKU_SZ) {
          board->cells[i] = sym;
          break;
        }
      }
    } while ((c = getchar()) >= 0);
    if (c < 0) {
      return 0;
    }
  }
  return 1;
}

int main(int argc,  char **argv) {
  int puzzle = 1;
  sudoku_t board;
  while (sudoku_read(&board)) {
    printf("===== Puzzle %d: =====\n", puzzle++);
    sudoku_print(&board);
    printf("*** Solutions:\n");
    size_t solutions = sudoku_solve(&board, -1);
    printf("%zu solutions\n", solutions);
  }
  return 0;
}