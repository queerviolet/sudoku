"use strict";

var EventEmitter = require('events').EventEmitter;

var Sudoku = function(size) {
  var blockSize = Math.sqrt(size);
  
  if (blockSize !== Math.floor(blockSize))
    throw new Error('size != some x^2')

  var board = new Uint8Array(size * size);
  
  var taken = {
    rows: new Uint16Array(size),
    cols: new Uint16Array(size),
    blocks: new Uint16Array(size),
  };

  var unsolved = [];
  var solved = [];

  var read = function(istream) {
    var emitter = new EventEmitter();
    var index = 0;

    istream.once('readable', function() {
      function getch() {
        do {
          var buf = istream.read(1);
        } while(!buf);
        return buf.toString();
      }

      function getchIgnoringComments() {
        var ch = getch();
        if (ch === '#') do {
          ch = getch();
        } while(ch !== '\n');
        return ch;
      }

      while (index < size * size) {
        var ch = getchIgnoringComments();
        if (ch === '-' || ch === '0') {
          // Empty cells are '-' or 0.
          var cell = new Cell(index++);
          unsolved.push(cell);
          emitter.emit('cell', cell);
        } else {
          var sym = ch * 1;
          // symbols from 1...size, inclusive.
          if (sym > 0 && sym <= size) {
            var cell = new Cell(index++);
            if (!cell.check(sym)) {
              return emitter.emit('error',
                new Error("Invalid board at cell " + cell));
            }
            cell.set(sym);
            solved.push(cell);
            emitter.emit('cell', cell);
          }
          // Ignore invalid numbers and NaN.
        }
      }

      emitter.emit('done', solver);
    });
    return emitter;
  }

  var solve = function(emitter) {
    unsolved.sort(function(a, b) {
      b.possible - a.possible;
    });
    var emitter = emitter || new EventEmitter();
    emitter.stats = {
      depth: 0,
      checks: 0,
      calls: 0,
    };    
    setImmediate(function() {
      recursive(emitter, 0);
      emitter.emit('done', emitter.stats);
    });
    return emitter;
  };

  var recursive = function(emitter, depth) {
    emitter.stats.depth = Math.max(++depth, emitter.stats.depth);
    if (++emitter.stats.calls % 10000000 == 0) {
      emitter.emit('stats', emitter.stats, new Solution(board));
    }
    if (unsolved.length === 0) {
      emitter.emit('solution', new Solution(board));
      return;
    }
    var cell = unsolved.shift();
    for (let sym = 1; sym <= size; ++sym) {
      ++emitter.stats.checks;
      if (cell.check(sym)) {
        cell.set(sym);
        recursive(emitter, depth);
        cell.clear();
      }
    }
    unsolved.unshift(cell);
  };

  var Solution = function(yourBoard) {
    this.board = yourBoard.slice();
  };

  Solution.prototype.toString = function() {
    return aryToString(this.board);
  };

  var aryToString = function(board) {
    var boardStr = '';
    for (var i = 0; i != size * size; i += size) {
      var row = '';
      for (var j = 0; j != size - 1; ++j) {
        row += board[i + j] + ' ';
      }
      boardStr += row + board[i + j] + '\n';
    }
    return boardStr;
  }

  var bitsetToString = function(bits) {
    var set = [];
    for (let sym = 0; sym < size; ++sym) {
      if (1 & (bits >> sym)) {
        set.push(sym + 1);
      }
    }
    return '[' + set.join(', ') + ']';
  }

  class Cell {
    constructor(index) {
      this.index = index;
      this.row = Math.floor(index / size);
      this.col = index % size;
      this.block = blockSize * Math.floor(this.row / blockSize) +
        Math.floor(this.col / blockSize);      
    }

    get possible() {
      let possible = 0
      for (let sym = 1; sym <= size; ++sym) {
        this.check(sym) && ++possible
      }
      return possible
    }

    toString() {
      return `cell at index: ${this.index} ` +
        ` row: ${this.row} ` +
        ` col: ${this.col} ` +
        ` block: ${this.block}`
    }

    check(symbol) {
      var mask = taken.rows[this.row] |
          taken.cols[this.col] |
          taken.blocks[this.block];
      //console.log('mask for', this.toString(), bitsetToString(mask));
      return !(1 & (mask >> (symbol - 1)));
    }

    set(symbol) {
      var mask = 1 << (symbol - 1);
      taken.rows[this.row] |= mask;
      taken.cols[this.col] |= mask;
      taken.blocks[this.block] |= mask;
      board[this.index] = symbol;
    }

    clear() {
      var symbol = board[this.index];
      board[this.index] = 0;
      var mask = ~(1 << (symbol - 1));
      taken.rows[this.row] &= mask;
      taken.cols[this.col] &= mask;
      taken.blocks[this.block] &= mask;
    }
  }

  var solver = {
    solve: solve,
    read: read,
    toString: function() {
      return aryToString(board);
    },
  };

  return solver;
};

Sudoku(9).read(process.stdin).
  on('error', function(error) {
    console.error(error);
  }).
  on('done', function(solver) {
    console.log(solver.toString());
    solver.solve().
      on('stats', function(stats, board) {
        console.log(JSON.stringify(stats));
        console.log(board.toString());
      }).
      on('solution', function(solution) {
        console.log('solution:')
        console.log(solution.toString());
      });
  });
