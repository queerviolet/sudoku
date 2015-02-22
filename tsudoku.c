#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <dispatch/dispatch.h>
#include <sys/time.h>

#define SUDOKU_SZ 9
#define SUDOKU_BLK_SZ 3

typedef uint16_t bitmap;

typedef struct {
	size_t row;
	size_t col;
} cell_pos;

typedef struct {
	cell_pos indices[SUDOKU_SZ * SUDOKU_SZ];
	size_t next;
	size_t size;
} cell_queue;

typedef struct {
	uint8_t cells[SUDOKU_SZ * SUDOKU_SZ];
	bitmap rows[SUDOKU_SZ];
	bitmap cols[SUDOKU_SZ];
	bitmap blocks[SUDOKU_SZ];
	cell_queue queue;
} sudoku;

sudoku BOARD_1 = {
	.cells = {
    	0, 0, 0,  2, 6, 0,  7, 0, 1,  // 0
    	6, 8, 0,  0, 7, 0,  0, 9, 0,  // 0
    	1, 9, 0,  0, 0, 4,  5, 0, 0,  // 0
    
    	8, 2, 0,  1, 0, 0,  0, 4, 0,  // 3
    	0, 0, 4,  6, 0, 2,  9, 0, 0,  // 3
    	0, 5, 0,  0, 0, 3,  0, 2, 8,  // 3
    
    	0, 0, 9,  3, 0, 0,  0, 7, 4,  // 6
    	0, 4, 0,  0, 5, 0,  0, 3, 6,  // 6
    	7, 0, 3,  0, 1, 8,  0, 0, 0,  // 6
    },
    .rows = { 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    .cols = { 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    .blocks = { 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

#define INDEX(row, col) (col + SUDOKU_SZ * row)

#if SUDOKU_SZ == 9
    const uint8_t BLOCKS_9x9[SUDOKU_SZ * SUDOKU_SZ] = {
        	0, 0, 0,  1, 1, 1,  2, 2, 2,  // 0
        	0, 0, 0,  1, 1, 1,  2, 2, 2,  // 0
        	0, 0, 0,  1, 1, 1,  2, 2, 2,  // 0

        	3, 3, 3,  4, 4, 4,  5, 5, 5,  // 3
        	3, 3, 3,  4, 4, 4,  5, 5, 5,  // 3
        	3, 3, 3,  4, 4, 4,  5, 5, 5,  // 3

        	6, 6, 6,  7, 7, 7,  8, 8, 8,  // 6
        	6, 6, 6,  7, 7, 7,  8, 8, 8,  // 6
        	6, 6, 6,  7, 7, 7,  8, 8, 8,  // 6
    };
    #define BLOCK(row, col) (BLOCKS_9x9[INDEX(row, col)])
#else
	#define BLOCK(row, col) \
        (col / SUDOKU_BLK_SZ + \
		    SUDOKU_BLK_SZ * (row / SUDOKU_BLK_SZ))  // integer division
#endif

/****** Bitmaps ******/

#define MAP_HAS(bitmap, sym) (1 & (bitmap >> (sym - 1)))

#define VALID_MOVE(board, row, col, sym) \
	!MAP_HAS(board->rows[row] & \
		board->cols[col] & \
		board->blocks[BLOCK(row, col)], sym)

void print_mask(uint16_t mask) {
	for (uint8_t sym = 1; sym <= SUDOKU_SZ; ++sym) {
		if (MAP_HAS(mask, sym)) {
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
void sudoku_set(sudoku *board, size_t row, size_t col, uint8_t symbol) {
	board->cells[INDEX(row, col)] = symbol;
	uint16_t mask = 1 << (symbol - 1);
	//printf("row:%zu col:%zu sym:%u mask:%u\n", row, col, symbol, mask);
	board->rows[row] |= mask;
	board->cols[col] |= mask;
	board->blocks[BLOCK(row, col)] |= mask;
}

void sudoku_clear(sudoku *board, size_t row, size_t col) {
	uint8_t symbol = board->cells[INDEX(row, col)];
	board->cells[INDEX(row, col)] = 0;
	uint16_t mask = ~(1 << (symbol - 1));
	board->rows[row] &= mask;
	board->cols[col] &= mask;
	board->blocks[BLOCK(row, col)] &= mask;
}

uint8_t sudoku_available(sudoku *board, cell_pos pos) {
	uint16_t taken = board->rows[pos.row] |
	  board->cols[pos.col] |
	  board->blocks[BLOCK(pos.row, pos.col)];
	uint8_t available = 0;
	for (uint8_t try = 1; try <= SUDOKU_SZ; ++try) {
		if (!MAP_HAS(taken, try)) {
			++available;
		}
	}
	return available;
}

// unsorted
void enqueue(cell_queue *queue, cell_pos pos) {
	queue->indices[queue->size++] = pos;
}

void enqueue_all(cell_queue *dst, cell_queue *src) {
	memcpy(&dst->indices[dst->size],
		&src->indices,
		src->size * sizeof(cell_pos));
	dst->size += src->size;
}

cell_pos dequeue(cell_queue *queue) {
	return queue->indices[queue->next++];
}

void requeue(cell_queue *queue) {
	queue->next--;
}

void print_queue(cell_queue *queue) {
	printf("{");
	for (size_t i = 0; i != queue->size; ++i) {
		printf("row:%zu, col:%zu; ",
			queue->indices[i].row, queue->indices[i].col);
	}
	printf("} next:%zu, size:%zu\n", queue->next, queue->size);
}

// quicksort
cell_queue sorted_queue(sudoku *board, cell_queue *queue, int asc) {
	if (queue->size <= 1) {
		return *queue;
	}
	uint8_t pivot = sudoku_available(board, queue->indices[0]);
	cell_queue low, high;
	memset(&low, 0, sizeof(cell_queue));
	memset(&high, 0, sizeof(cell_queue));
	for (size_t i = 1; i != queue->size; ++i) {
		if (asc) {
			if (sudoku_available(board, queue->indices[i]) < pivot) {
				enqueue(&low, queue->indices[i]);
			} else {
				enqueue(&high, queue->indices[i]);
			}
		} else {
			if (sudoku_available(board, queue->indices[i]) > pivot) {
				enqueue(&low, queue->indices[i]);
			} else {
				enqueue(&high, queue->indices[i]);
			}			
		}
	}
	low = sorted_queue(board, &low, asc);
	high = sorted_queue(board, &high, asc);
	enqueue(&low, queue->indices[0]);
	enqueue_all(&low, &high);
	return low;
}

typedef enum {
	FREEDOM_ASCENDING = 0,
	FREEDOM_DESCENDING = 1,
	POSITION = 2
} queue_strategy;

void sudoku_init(sudoku *board, queue_strategy strategy) {
	for (size_t row = 0; row != SUDOKU_SZ; ++row) {
		for (size_t col = 0; col != SUDOKU_SZ; ++col) {
			uint8_t sym = board->cells[INDEX(row, col)];
			if (sym != 0) {
				if (!VALID_MOVE(board, row, col, sym)) {
					printf("failed at: row:%zu, col:%zu, sym:%u\n",
						row, col, sym);
					sudoku_print(board);
				}
				assert(VALID_MOVE(board, row, col, sym));
				sudoku_set(board, row, col, sym);
			} else {
				// empty space
				enqueue(&(board->queue),
					(cell_pos) { .row = row, .col = col });
			}
		}
	}
	cell_queue sorted = board->queue;
	switch(strategy) {
	case FREEDOM_ASCENDING:
		sorted = sorted_queue(board, &sorted, 1);
		break;

	case FREEDOM_DESCENDING:
	    sorted = sorted_queue(board, &sorted, 0);
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
	int finished; // were we terminated before we could return?
	size_t solutions;
	size_t depth;
} solver_stats;

solver_stats sudoku_solve(sudoku *board, size_t depth, int *halt) {
	if (*halt) {
		return (solver_stats) {.finished = 0, .solutions = 0, .depth = depth };
	}

	if (board->queue.next == board->queue.size) {
		sudoku_print(board);
		printf("\n\n");
		return (solver_stats) {.finished = 1, .solutions = 1, .depth = depth};
	}
	cell_pos pos = dequeue(&board->queue);
	uint8_t sym = board->cells[INDEX(pos.row, pos.col)];
	solver_stats stats = {.finished = 1, .solutions = 0, .depth = 0};
	uint16_t taken = board->rows[pos.row] |
	    board->cols[pos.col] |
	    board->blocks[BLOCK(pos.row, pos.col)];
	for (uint8_t try = 1; try <= SUDOKU_SZ; ++try) {
		if (!MAP_HAS(taken, try)) {
			sudoku_set(board, pos.row, pos.col, try);
			solver_stats descent = sudoku_solve(board, depth + 1, halt);
			stats.finished = descent.finished;
			stats.solutions += descent.solutions;
			stats.depth = descent.depth > stats.depth?
			descent.depth : stats.depth;
			sudoku_clear(board, pos.row, pos.col);
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

void print_report(const char *solver, solver_stats stats,
	struct timeval start, struct timeval stop) {
	struct timeval elapsed;
	timersub(&stop, &start, &elapsed);
	printf("[%s] %s solver: %zu sec, %d usec  %zu solutions, depth:%zu\n",
		stats.finished? "DONE" : "HALTED",
		solver,
		elapsed.tv_sec, elapsed.tv_usec,		
		stats.solutions, stats.depth);
}

int main(int argc,  char **argv) {
	dispatch_queue_t solvers = dispatch_queue_create("works.ashi.sudoku", DISPATCH_QUEUE_CONCURRENT);

	int puzzle = 1;
	sudoku board;
	while (sudoku_read(&board)) {
		sudoku pos = board, asc = board, desc = board;

		sudoku_init(&pos, POSITION);
		sudoku_init(&asc, FREEDOM_ASCENDING);
		sudoku_init(&desc, FREEDOM_DESCENDING);

		printf("===== Puzzle %d: =====\n", puzzle++);
		sudoku_print(&board);
		printf("*** Solutions:\n");
		__block int halt = 0;

		dispatch_semaphore_t pos_done = dispatch_semaphore_create(0);
		dispatch_semaphore_t asc_done = dispatch_semaphore_create(0);
		dispatch_semaphore_t desc_done = dispatch_semaphore_create(0);

		dispatch_async(solvers, ^{
			struct timeval start, stop;
			gettimeofday(&start, NULL);
			solver_stats stats = sudoku_solve((sudoku *) &pos, 1, &halt);
			halt = 1;
			gettimeofday(&stop, NULL);
			print_report("position", stats, start, stop);
			dispatch_semaphore_signal(pos_done);
		});
		dispatch_async(solvers, ^{
			struct timeval start, stop;
			gettimeofday(&start, NULL);
			solver_stats stats = sudoku_solve((sudoku *) &asc, 1, &halt);
			halt = 1;
			gettimeofday(&stop, NULL);
			print_report("freedom_ascending", stats, start, stop);
			dispatch_semaphore_signal(asc_done);
		});
		dispatch_async(solvers, ^{
			struct timeval start, stop;
			gettimeofday(&start, NULL);						
			solver_stats stats = sudoku_solve((sudoku *) &desc, 1, &halt);
			halt = 1;
			gettimeofday(&stop, NULL);
			print_report("freedom_descending", stats, start, stop);
			dispatch_semaphore_signal(desc_done);
		});
		dispatch_semaphore_wait(pos_done, DISPATCH_TIME_FOREVER);
		dispatch_semaphore_wait(asc_done, DISPATCH_TIME_FOREVER);
		dispatch_semaphore_wait(desc_done, DISPATCH_TIME_FOREVER);
	}
	return 0;
}