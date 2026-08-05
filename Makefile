.PHONY: run

run:
	mkdir -p build
	g++ $$(find src -name "*.cpp") -std=c++20 -o build/app -lftxui-component -lftxui-dom -lftxui-screen
