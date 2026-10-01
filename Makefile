.PHONY: configure build test run br clean

configure:
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

build:
	cmake --build build

test: build
	ctest --test-dir build --output-on-failure

run:
	./build/order_book_app

br: build run

clean:
	rm -rf build
