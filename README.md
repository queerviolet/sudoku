# sudoku
Some people find sudoku satisfying. I find writing solvers satisfying in the same way.

The C solver here is pleasantly fast. Constraint sets are stored as bitmaps; each cell
holds a reference to three (row, col, block). Each iteration, we scan the free cells
list for the next most constrained cell. I was concerned this O(n) scan would be
too expensive, but it turns out to be tremendously worth it, since we detect propagated
constraint violations ASAP. An earlier version used a free cell queue which was sorted
once at initilization. That version was about half the speed of this one.

The other solvers are in various states of disarray, alas.

To try it:

    make && cat puzzles/* | time ./sudoku
    
My results:


    $ cat puzzles/* | time ./sudoku > /dev/null
        1.20 real         1.19 user         0.00 sys

    $ sysctl -n machdep.cpu.brand_string
    Intel(R) Core(TM) i7-4578U CPU @ 3.00GHz


The number of cores sadly doesn't matter.