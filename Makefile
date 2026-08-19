.PHONY: all run clean

all: run

run:
	./scripts/configure.sh
	cmake --build build -j

clean:
	cmake --build build --target clean
