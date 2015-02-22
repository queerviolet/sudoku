package main

import (
	"fmt"
	"errors"
	"os"
	"bufio"
	"bytes"
)

const SUDOKU_SZ = 9
const SUDOKU_BLK_SZ = 3

type sudoku struct {
	cells [SUDOKU_SZ * SUDOKU_SZ]byte
	rows [SUDOKU_SZ]uint16
	cols [SUDOKU_SZ]uint16
	blocks [SUDOKU_SZ]uint16
}

var BOARD_1 = sudoku {
	cells: [SUDOKU_SZ * SUDOKU_SZ]byte{
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
    rows: [SUDOKU_SZ]uint16{ 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    cols: [SUDOKU_SZ]uint16{ 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    blocks: [SUDOKU_SZ]uint16{ 0, 0, 0, 0, 0, 0, 0, 0, 0 },
};

func index(row, col int) int {
	return col + SUDOKU_SZ * row
}

func block(row, col int) int {
	return col / SUDOKU_BLK_SZ +
		SUDOKU_BLK_SZ * (row / SUDOKU_BLK_SZ)  // integer division
}

func filled(bitmap uint16, sym byte) bool {
	return 1 & (bitmap >> (sym - 1)) != 0
}

func (board *sudoku) maskAt(row, col int) uint16 {
	return uint16(board.rows[row]) |
		uint16(board.cols[col]) |
		uint16(board.blocks[block(row, col)])
}

func (board *sudoku) with(row, col int, sym byte) (newBoard *sudoku) {
	created := sudoku{}
	created = *board
	created.set(row, col, sym)
	return &created
}


func (board *sudoku) isAllowed(row, col int, sym byte) bool {
	return !filled(board.maskAt(row, col), sym)
}

// doesn't check validity
func (board *sudoku) set(row, col int, sym byte) {
	board.cells[index(row, col)] = sym
	mask := uint16(1 << (sym - 1))
	board.rows[row] |= mask
	board.cols[col] |= mask
	board.blocks[block(row, col)] |= mask
}

func (board *sudoku) clear(row, col int) {
	idx := index(row, col)
	sym := board.cells[idx]
	board.cells[idx] = 0
	mask := uint16(^(1 << (sym - 1)))
	board.rows[row] &= mask
	board.cols[col] &= mask
	board.blocks[block(row, col)] &= mask
}

func (board *sudoku) get(row, col int) byte {
	return board.cells[index(row, col)]
}

func (board *sudoku) init() error {
	for row := 0; row != SUDOKU_SZ; row++ {
		for col := 0; col != SUDOKU_SZ; col++ {
			sym := board.cells[index(row, col)]
			if sym != 0 {
				if !board.isAllowed(row, col, sym) {
					return errors.New(fmt.Sprintf("failed at row:%v, col:%v",
						row, col))
				}
				board.set(row, col, sym)
			}
		}
	}
	return nil
}

func maskToStr(mask uint16) string {
	var buf bytes.Buffer
	for sym := 0; sym < SUDOKU_SZ; sym++ {
		if filled(mask, byte(sym)) {
			buf.WriteString(fmt.Sprintf("%v, ", sym))
		}
	}
	return buf.String()
}

func (board *sudoku) solve() int {
	for row := 0; row != SUDOKU_SZ; row++ {
		for col := 0; col != SUDOKU_SZ; col++ {
			sym := board.get(row, col)
			if sym == 0 {
				solutions := 0
				taken := board.maskAt(row, col)
				for t := byte(1); t <= SUDOKU_SZ; t++ {
					if !filled(taken, t) {
						solutions += board.with(row, col, t).solve()
					}
				}
				return solutions
			}
		}
	}
	fmt.Println(board)
	return 1
}

func (board *sudoku) solveParallel(solutions chan *sudoku, done chan int) {
	for row := 0; row != SUDOKU_SZ; row++ {
		for col := 0; col != SUDOKU_SZ; col++ {
			sym := board.get(row, col)
			if sym == 0 {
				taken := board.maskAt(row, col)
				children := make(chan int, SUDOKU_SZ)
				childCount := 0
				for t := byte(1); t <= SUDOKU_SZ; t++ {
					if !filled(taken, t) {
						childCount++
						go board.with(row, col, t).solveParallel(solutions, children)
					}
				}
				solutionCount := 0
				for ; childCount > 0; childCount-- {
					solutionCount += <-children
				}
				done <- solutionCount
				return
			}
		}
	}
	solutions <- board
	done <- 1
}

type Solver struct {
	queue chan *sudoku
	solutions chan *sudoku
	done chan int
	count int
	workerChannels []chan bool
}

func (solver *Solver) work(board *sudoku) {
	spawned := 0	
	defer func() {
		/*fmt.Printf("spawned:%v, len(queue):%v, count:%v, solutions:%v\n", spawned, len(solver.queue), solver.count, len(solver.solutions))*/
		if spawned == 0 {
			if len(solver.queue) == 0 {
				solver.done <- solver.count
			}
		}
	}()
	for row := 0; row != SUDOKU_SZ; row++ {
		for col := 0; col != SUDOKU_SZ; col++ {
			sym := board.get(row, col)
			if sym == 0 {
				taken := board.maskAt(row, col)
				for t := byte(1); t <= SUDOKU_SZ; t++ {
					if !filled(taken, t) {
						spawned++
						solver.queue <- board.with(row, col, t)
					}
				}
				return
			}
		}
	}
	fmt.Println("solved!")
	solver.solutions <- board
	solver.count++
}

func (solver *Solver) worker(halt chan bool) {
	for {
		var board *sudoku
		select {
		case board = <-solver.queue:
			//fmt.Printf("%v\n\n", board)
			solver.work(board)
		case <-halt:
			return			
		}
	}
}

func newSolver(workers int) (*Solver) {
	workerChans := make([]chan bool, workers)
	for i := range workerChans {
		workerChans[i] = make(chan bool)
	}
	return &Solver{
		queue: make(chan *sudoku, workers * 100000000),
		solutions: make(chan *sudoku, 128),
		done: make(chan int, 1),
		workerChannels: workerChans,
	}
}

func (solver *Solver) start() {
	for i := range solver.workerChannels {
		go solver.worker(solver.workerChannels[i])
	}
}

func (solver *Solver) stop() {
	for i := range solver.workerChannels {
		solver.workerChannels[i] <- true
	}
}

func (board *sudoku) read(reader *bufio.Reader) error {
	for i := 0; i != SUDOKU_SZ * SUDOKU_SZ; i++ {
		for {
			c, _, err := reader.ReadRune()
			if err != nil {
				return err
			}
			if c == '-' {
				board.cells[i] = 0
				break
			}
			sym := byte(c - '0')
			if sym >= 0 && sym <= SUDOKU_SZ {
				board.cells[i] = sym
				break
			}
		}
	}
	if err := board.init(); err != nil {
		return err
	}
	return nil
}

func (board *sudoku) String() string {
	var buf bytes.Buffer
	for row := 0; row != SUDOKU_SZ; row++ {
		for col := 0; col != SUDOKU_SZ; col++ {
			buf.WriteString(fmt.Sprintf("%v ", board.get(row, col)))
		}
		buf.WriteString("\n")
	}
	return buf.String()
}

func main() {
	//BOARD_1.init()
	//BOARD_1.solve()
	problem := 0
	solver := newSolver(1)
	solver.start()
	reader := bufio.NewReader(os.Stdin)
	for {
		fmt.Println("now here")
		board := &sudoku{}
		err := board.read(reader)
		problem++;
		if err != nil {
			fmt.Println(err)
			return
		}
		fmt.Printf("===== Problem %v =====\n", problem)
		fmt.Println(board)
		fmt.Println("*** Solutions: ***")
		/*solutions := make(chan *sudoku, 32)
		done := make(chan int, 1)		
		go board.solveParallel(solutions, done)*/
		solver.queue <- board
		var solution *sudoku
		var count int
		func() {
			for {
				select {
				case solution = <-solver.solutions:
					fmt.Printf("%v\n\n", solution)

				case count = <-solver.done:
					fmt.Printf("Found %v solutions.\n\n", count)
					return
				}
			}
		}()
		fmt.Println("here I am.")
	}
}
