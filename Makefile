CC := gcc

SRC_DIR := src
BIN_DIR := bin
TEST_DIR := tests

APP_BIN := $(BIN_DIR)/anteater_chess
ENGINE_MATCH_BIN := $(BIN_DIR)/engine_match
SELFPLAY_BIN := $(BIN_DIR)/selfplay_data
TEST_BIN := $(BIN_DIR)/test_rules

CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -O3 -march=native -DNDEBUG -I$(SRC_DIR)
SDL_CFLAGS := $(shell pkg-config --cflags sdl2 SDL2_ttf SDL2_image)
SDL_LIBS := $(shell pkg-config --libs sdl2 SDL2_ttf SDL2_image)

CORE_SRC := \
	$(SRC_DIR)/board.c \
	$(SRC_DIR)/moves.c \
	$(SRC_DIR)/game.c \
	$(SRC_DIR)/cpu.c \
	$(SRC_DIR)/eval.c

APP_SRC := \
	$(SRC_DIR)/main.c \
	$(SRC_DIR)/gui.c \
	$(CORE_SRC)

ENGINE_MATCH_SRC := \
	$(SRC_DIR)/engine_match.c \
	$(CORE_SRC)

SELFPLAY_SRC := \
	$(SRC_DIR)/selfplay_data.c \
	$(SRC_DIR)/data_logger.c \
	$(CORE_SRC)

GENERATED_HEADERS := \
	$(SRC_DIR)/generated_eval_weights.h \
	$(SRC_DIR)/generated_piece_square_tables.h

.PHONY: all engine_match selfplay test run clean install

all: $(APP_BIN)

$(APP_BIN): $(APP_SRC) $(GENERATED_HEADERS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) $(APP_SRC) -o $@ $(SDL_LIBS)

engine_match: $(ENGINE_MATCH_BIN)

$(ENGINE_MATCH_BIN): $(ENGINE_MATCH_SRC) $(GENERATED_HEADERS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(ENGINE_MATCH_SRC) -o $@

selfplay: $(SELFPLAY_BIN)

$(SELFPLAY_BIN): $(SELFPLAY_SRC) $(GENERATED_HEADERS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(SELFPLAY_SRC) -o $@

$(TEST_BIN): $(TEST_DIR)/test_rules.c $(CORE_SRC) $(GENERATED_HEADERS)
	@mkdir -p $(BIN_DIR)
	$(CC) -std=c11 -Wall -Wextra -Wpedantic -g -O0 -I$(SRC_DIR) $^ -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

run: $(APP_BIN)
	./$(APP_BIN)

install:
	@echo "Build full app:            make"
	@echo "Run full app:              ./$(APP_BIN)"
	@echo "Build engine tester:       make engine_match"
	@echo "Run engine tester:         ./$(ENGINE_MATCH_BIN) 20 6 test/run1 10 1000 fast 1024"
	@echo "Run parallel engine test:  ./scripts/run_engine_match_parallel.sh test/run1 100 6 10 1000 fast 20 20480"
	@echo "Build self-play generator: make selfplay"
	@echo "Run self-play:             ./$(SELFPLAY_BIN) training_data.csv 3000 120 10 1000 fast 23 1 1024"
	@echo "Run parallel self-play:    ./scripts/run_selfplay_parallel.sh training_data.csv 3000 120 10 1000 fast 30 23 20480"

clean:
	rm -rf $(BIN_DIR)
