require 'set'

class Cell
  # Board is an instance of Sudoku
  # row is 
  def initialize board, row, col, row_set, col_set, block_set
    @board = board
    @row = row
    @col = col

    @row_set = row_set
    @col_set = col_set
    @block_set = block_set
  end

  def set value
    @board.set(@row, @col, value)
  end

  def clear
    @board.clear(@row, @col)
  end

  def possibilities
    # 1. Find out what values are taken in my row, column, block

    taken = @row_set | @col_set | @block_set
    return Sudoku9x9::ALL_VALUES - taken
  end

  def to_s    
    "cell at #{@row},#{@col},   row:#{@row_set.inspect} col:#{@col_set.inspect} block:#{@block_set.inspect}"
  end
end

# Sudoku9x9 is a solver for boards of the obvious size
class Sudoku9x9
  SIZE = 9
  ALL_VALUES = Set[*1..SIZE]

  # Takes a string like 0283-39483223583etc. No newlines.
  def initialize(str)
    # cells is an array of ints
    @cells = str.split('').map(&:to_i)

    @rows = Array.new(SIZE) { Set.new }
    @cols = Array.new(SIZE) { Set.new }
    @blocks = Array.new(SIZE) { Set.new }

    @solved_cells = []
    @empty_cells = []

    SIZE.times do |row|
      SIZE.times do |col|
        cell = Cell.new(self, row, col, @rows[row], @cols[col],
                        @blocks[block_of(row, col)])
        value = @cells[index(row, col)]
        if value == 0
          @empty_cells.push(cell)
        else
          @solved_cells.push(cell)
          cell.set(value)
        end
      end
    end

    @empty_cells.sort_by! { |cell| cell.possibilities.length }

    @solve_calls = 0
  end

  # Does not check whether move is valid
  def set(row, col, value)
    @cells[index(row, col)] = value
    @rows[row].add(value)
    @cols[col].add(value)
    @blocks[block_of(row, col)].add(value)
  end

  def clear(row, col)
    value = @cells[index(row, col)]
    @cells[index(row, col)] = 0
    @rows[row].delete(value)
    @cols[col].delete(value)
    @blocks[block_of(row, col)].delete(value)
  end

  # takes an int from 0 to SIZE-1 and returns that row as an array
  def row(row)
    first_index = index(row, 0)
    last_index = index(row, SIZE - 1)
    return @cells[first_index..last_index]
  end

  # takes an int from 0 to SIZE-1 and returns that row as an array
  def col(col)
    @cells.each_slice(SIZE).map { |row| row[col] }
  end

  def block_of(row, col)
    index_to_block = [
       0, 0, 0, 1, 1, 1, 2, 2, 2,
       0, 0, 0, 1, 1, 1, 2, 2, 2,
       0, 0, 0, 1, 1, 1, 2, 2, 2,
       3, 3, 3, 4, 4, 4, 5, 5, 5,
       3, 3, 3, 4, 4, 4, 5, 5, 5,
       3, 3, 3, 4, 4, 4, 5, 5, 5,
       6, 6, 6, 7, 7, 7, 8, 8, 8,
       6, 6, 6, 7, 7, 7, 8, 8, 8,
       6, 6, 6, 7, 7, 7, 8, 8, 8
     ]
     return index_to_block[index(row, col)]
  end

  # takes a int row and col (0 to SIZE-1) returns the associated block
  # as an array
  def block(row, col)    
     block = block_of(row, col)
     @cells.each_with_index.map do |value, index|
       if index_to_block[index] == block
         value
       end
     end.compact
  end

  # Takes int row, col and returns an index into @cells
  def index(row, col)
    row * SIZE + col
  end

  def full?
    @cells.index(0).nil?
  end

  # Returns self if a solution was found, false otherwise
  # solve modifies the board
  def solve
    # notes:
    # 1. Loop through cells, find free cells, see what's available for each cell (deduction)

    # puts self
    # puts ''

    @solve_calls += 1
    if @solve_calls % 10000 == 0
      puts self
      puts ''
    end

    cell = @empty_cells.shift
    cell.possibilities.each do |possibility|
      cell.set(possibility)
      # success!
      return self if @empty_cells.empty?
      solution = solve
      # recursive success!
      return solution if solution
      cell.clear()
    end
    @empty_cells.unshift(cell)

    # failure!
    return nil
  end

  def to_s
    @cells.each_slice(SIZE).map do |row|
      row.join(' ')
    end.join("\n")
  end
end

def quick_dirty_test
  # test driver code
  # TODO: write specs
  sudoku = Sudoku9x9.new(gets.chomp)
  puts sudoku
  p sudoku.row 0
  p sudoku.row 8
  p sudoku.block 4, 4
  p sudoku.col 0
  p sudoku.col 8

  cell = Cell.new(sudoku, 0, 1)
  puts '----'
  puts 'possibilities for 0, 1:'
  p cell.possibilities
  cell.set 5
  puts 'new board:'
  puts sudoku
  cell.clear
  puts 'after clearing:'
  puts sudoku
  puts 'new possibilities for 0, 1:'
  p cell.possibilities
  puts '---'
end

puts Sudoku9x9.new(gets.chomp).solve
