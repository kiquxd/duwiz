.PHONY: run-linux run-mac

run-linux:
	mkdir -p build
	g++ $$(find src -name "*.cpp") -std=c++20 -o build/app -lftxui-component -lftxui-dom -lftxui-screen



run-mac:
	mkdir -p build
	g++ $$(find src -name "*.cpp") -std=c++20 \
			-I$$(brew --prefix)/include -L$$(brew --prefix)/lib \
			-o build/app -lftxui-component -lftxui-dom -lftxui-screen
