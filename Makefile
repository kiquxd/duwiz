.PHONY: all run test clean

all: run

run:
	./scripts/configure.sh
	cmake --build build -j

test: run
	ctest --test-dir build --output-on-failure

clean:
	cmake --build build --target clean
