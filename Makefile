.PHONY: configure build run br clean

configure:
	cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug

build:
	cmake --build build

run:
	./build/order_book_app

br: build run

clean:
	rm -rf build