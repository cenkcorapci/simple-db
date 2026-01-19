.PHONY: all build clean run test

all: build

build:
	mkdir -p build
	cd build && cmake .. && make

clean:
	rm -rf build

run: build
	./build/simpledb

test: build
	./build/simpledb --test
