.PHONY: clean

CC := x86_64-w64-mingw32-g++-posix

SOURCES := $(wildcard src/*.cpp)
HEADERS := $(wildcard include/*.h)
OBJECTS := $(SOURCES:src/%.cpp=build/%.o)

-include local.mk

sensei.exe : $(OBJECTS)
	@$(CC) -pthread -o $@ $(OBJECTS) -Lglfw/lib-mingw -lglfw -lopengl32 -lglu32 -lgdiplus  -lddraw -ldxguid -lole32 -loleaut32 -lstrmiids -luuid -lws2_32

build/%.o : src/%.cpp $(HEADERS)
	@echo "Compiling: $<"
	@mkdir -p ./build
	@$(CC) -pthread -Og -g -std=c++11 -c $< -o $@ -Iinclude -Iglfw/include/GL/ -mno-ms-bitfields

clean:
	@rm -rf build
	@rm -f sensei.exe

