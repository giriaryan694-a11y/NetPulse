# ╔══════════════════════════════════════════════╗
# ║  NetPulse — Makefile                        ║
# ║  Made by Aryan Giri                         ║
# ╚══════════════════════════════════════════════╝

CC      = gcc
TARGET  = netpulse
SRC     = netpulse.c
CFLAGS  = -O2 -Wall -Wextra -Wpedantic
LDFLAGS = -lpthread

# Default: build
all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)
	@echo ""
	@echo "  ✔  Built: ./$(TARGET)"
	@echo "  ➜  Run  : ./$(TARGET)"
	@echo ""

# Termux-specific build (alias)
termux: all

# Clean binary
clean:
	rm -f $(TARGET) *.csv

# Install to Termux PATH
install: $(TARGET)
	cp $(TARGET) $(PREFIX)/bin/$(TARGET)
	@echo "  ✔  Installed to $(PREFIX)/bin/$(TARGET)"

.PHONY: all termux clean install
