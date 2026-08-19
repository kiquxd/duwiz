.PHONY: all run clean

all: run

run:
	./scripts/configure.sh
	cmake --build build -j
	ctest --test-dir build --output-on-failure

clean:
	cmake --build build --target clean
