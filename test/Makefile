# cpp/test/Makefile — game.cpp のホスト側ユニットテスト。
# 使い方:
#   make test     # ビルドして実行
#   make clean

CXX      ?= clang++
CXXFLAGS := -std=c++17 -O0 -g -Wall -Wextra -DHOST_TEST
INCLUDES := -I stubs -I ..
SOURCES  := test_game.cpp ../game.cpp ../kana.cpp ../font.cpp stubs/stub_sound.cpp
HEADERS  := stubs/pico/rand.h stubs/pico/types.h

test_game: $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SOURCES) -o $@

.PHONY: test
test: test_game
	./test_game

.PHONY: clean
clean:
	rm -f test_game
