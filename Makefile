CC := cc
CFLAGS := -O2 -Wall -Wextra -std=c11
SRC := src/tinyssg.c src/md4c.c src/md4c-html.c src/entity.c
BIN := tinyssg
INPUT := input
OUTPUT := output

.PHONY: all clean build run

all: build

build: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f $(BIN)

SERVE_SCRIPT := ./serve.sh
PRE_SCRIPT := ../scripts/pre.sh
POST_SCRIPT := ../scripts/post.sh

run: build
	@if [ -x "$(PRE_SCRIPT)" ]; then \
		$(PRE_SCRIPT); \
	fi
	./$(BIN)
	@if [ -x "$(POST_SCRIPT)" ]; then \
		$(POST_SCRIPT); \
	fi
serve: run
	@if [ -x "$(SERVE_SCRIPT)" ]; then \
		$(SERVE_SCRIPT); \
	else \
		echo "Serve script not found or not executable."; \
	fi
