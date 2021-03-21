BUILDDIR=build

.PHONY: build run clean

build:
	mkdir -p $(BUILDDIR)
	cd build; cmake ..; cmake --build .

run: build
	build/sudokusolver test.sudoku

all: clean build
	
clean:
	rm -rf $(BUILDDIR)

