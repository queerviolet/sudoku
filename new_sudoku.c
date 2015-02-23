#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define SUDOKU_SZ 9
#define SUDOKU_BLK_SZ 3

typedef uint16_t bitset;

typedef struct {
  uint8_t *value;
  bitset *row;
  bitset *col;
  bitset *block;
  uint8_t num_possible;
  uint8_t possible[SUDOKU_SZ];
} cell_t;

#define cell_at(board, row, col) \
  ((cell_t) { \
    .value = &board->cells[INDEX(row, col)], \
    .row = &board->rows[row], \
    .col = &board->cols[col], \
    .block = &board->blocks[BLOCK(row, col)] \
  })

#define cell_taken(cell) (*cell->row | *cell->col | *cell->block)

typedef struct {
  cell_t cells[SUDOKU_SZ * SUDOKU_SZ];
  size_t next;
  size_t size;
} cell_queue;

typedef struct {
  uint8_t cells[SUDOKU_SZ * SUDOKU_SZ];
  bitset rows[SUDOKU_SZ];
  bitset cols[SUDOKU_SZ];
  bitset blocks[SUDOKU_SZ];
  cell_queue queue;
} sudoku;

#define INDEX(row, col) (col + SUDOKU_SZ * row)

#define BLOCK(row, col) \
  (col / SUDOKU_BLK_SZ + \
  SUDOKU_BLK_SZ * (row / SUDOKU_BLK_SZ))  // integer division

/****** Bitsets ******/

#define SET_HAS(bitset, sym) (1 & (bitset >> (sym - 1)))

#define VALID_MOVE(board, row, col, sym) \
  !SET_HAS(board->rows[row] & \
    board->cols[col] & \
    board->blocks[BLOCK(row, col)], sym)

void print_mask(uint16_t mask) {
  for (uint8_t sym = 1; sym <= SUDOKU_SZ; ++sym) {
    if (SET_HAS(mask, sym)) {
      printf("%u, ", sym);
    }
  }
}

void print_masks(uint16_t type[]) {
  for (size_t i = 0; i != SUDOKU_SZ; ++i) {
    uint16_t mask = type[i];
    print_mask(mask);
    printf("\n");
  }
  printf("\n");
}

void sudoku_print(sudoku *board) {
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
  /*printf("rows: "); print_masks(board->rows);
  printf("cols: "); print_masks(board->cols);
  printf("blocks: "); print_masks(board->blocks);*/
}

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
  uint16_t mask = ~(1 << (symbol - 1));
  *cell->row &= mask;
  *cell->col &= mask;
  *cell->block &= mask;
}

// unsorted
void enqueue(cell_queue *queue, cell_t cell) {
  queue->cells[queue->size++] = cell;
}

void enqueue_all(cell_queue *dst, cell_queue *src) {
  memcpy(&dst->cells[dst->size],
         &src->cells,
         src->size * sizeof(cell_t));
  dst->size += src->size;
}

cell_t * dequeue(cell_queue *queue) {
  return &queue->cells[queue->next++];
}

void requeue(cell_queue *queue) {
  queue->next--;
}

/*void print_queue(cell_queue *queue) {
  printf("{");
  for (size_t i = 0; i != queue->size; ++i) {
    printf("row:%zu, col:%zu; ",
      queue->cells[i].row, queue->cells[i].col);
  }
  printf("} next:%zu, size:%zu\n", queue->next, queue->size);
}*/

// quicksort
cell_queue sorted_queue(cell_queue *queue, int asc) {
  if (queue->size <= 1) {
    return *queue;
  }
  uint8_t pivot = queue->cells[0].num_possible;
  cell_queue low, high;
  memset(&low, 0, sizeof(cell_queue));
  memset(&high, 0, sizeof(cell_queue));
  for (size_t i = 1; i != queue->size; ++i) {
    if (asc) {
      if (queue->cells[i].num_possible < pivot) {
        enqueue(&low, queue->cells[i]);
      } else {
        enqueue(&high, queue->cells[i]);
      }
    } else {
      if (queue->cells[i].num_possible > pivot) {
        enqueue(&low, queue->cells[i]);
      } else {
        enqueue(&high, queue->cells[i]);
      }     
    }
  }
  low = sorted_queue(&low, asc);
  high = sorted_queue(&high, asc);
  enqueue(&low, queue->cells[0]);
  enqueue_all(&low, &high);
  return low;
}

typedef enum {
  FREEDOM_ASCENDING = 0,
  FREEDOM_DESCENDING = 1,
  POSITION = 2
} queue_strategy;

void cell_init(cell_t *cell) {
  uint16_t taken = *cell->row | *cell->col | *cell->block;
  cell->num_possible = 0;
  for (uint8_t try = 1; try <= SUDOKU_SZ; ++try) {
    if (!SET_HAS(taken, try)) {
      cell->possible[cell->num_possible++] = try;
    }
  }
}

void sudoku_init(sudoku *board, queue_strategy strategy) {
  for (size_t row = 0; row != SUDOKU_SZ; ++row) {
    for (size_t col = 0; col != SUDOKU_SZ; ++col) {
      uint8_t sym = board->cells[INDEX(row, col)];
      cell_t cell = cell_at(board, row, col);
      if (sym != 0) {
        if (!VALID_MOVE(board, row, col, sym)) {
          printf("failed at: row:%zu, col:%zu, sym:%u\n",
            row, col, sym);
          sudoku_print(board);
        }
        assert(VALID_MOVE(board, row, col, sym));
        cell_set(&cell, sym);
      } else {
        // empty space
        cell_init(&cell);
        enqueue(&(board->queue), cell);
      }
    }
  }
  cell_queue sorted = board->queue;
  switch(strategy) {
  case FREEDOM_ASCENDING:
    sorted = sorted_queue(&sorted, 1);
    break;

  case FREEDOM_DESCENDING:
    sorted = sorted_queue(&sorted, 0);
    break;

  case POSITION:
    // already sorted by position.
    break;
  }

  // check my quicksort
  assert(sorted.size == board->queue.size);
  board->queue = sorted;
}

typedef struct {
  size_t solutions;
  size_t depth;
} solver_stats;

solver_stats sudoku_solve_with_queue(sudoku *board, size_t max_solutions, size_t depth) {
  if (board->queue.next == board->queue.size) {
    sudoku_print(board);
    printf("\n\n");
    return (solver_stats) {.solutions = 1, .depth = depth};
  }
  cell_t *cell = dequeue(&board->queue);
  solver_stats stats = {.solutions = 0, .depth = 0};
  bitset taken = cell_taken(cell);
  for (uint8_t try = 1; try <= SUDOKU_SZ; ++try) {
    if (!SET_HAS(taken, try)) {
      cell_set(cell, try);
      solver_stats descent = sudoku_solve_with_queue(board, max_solutions, depth + 1);
      stats.solutions += descent.solutions;
      stats.depth = descent.depth > stats.depth?
      descent.depth : stats.depth;
      if (stats.solutions >= max_solutions) {
        return stats;
      }
      cell_clear(cell);
    }
  }
  requeue(&board->queue);
  return stats;
}

// returns an uninitialized sudoku solver
int sudoku_read(sudoku *board) {
  memset(board, 0, sizeof(sudoku));
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
  sudoku board;
  while (sudoku_read(&board)) {
    sudoku_init(&board, POSITION);
    printf("===== Puzzle %d: =====\n", puzzle++);
    sudoku_print(&board);
    printf("*** Solutions:\n");
    solver_stats stats = sudoku_solve_with_queue(&board, 1, 1);
    printf("%zu solutions, max_stack_depth:%zu\n",
      stats.solutions, stats.depth);
  }
  return 0;
}