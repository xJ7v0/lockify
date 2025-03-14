CC = gcc
LIBS = -lX11 -lXss
CFLAGS ?= -Wall -Wextra
SRC = lockify.c
OBJ = lockify.o
EXEC = lockify

-include config.mak

ifeq ($(PREFIX),)

all:
	@echo "Please set prefix in config.mak before running make."
	@exit 1

else


# Default target: compile and install
all: $(EXEC)

# Rule to compile the source code
$(EXEC): $(OBJ)
	$(CC) ${LIBS} $(OBJ) -o $(EXEC)

# Rule to generate the object file from source
$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c $(SRC)

install: $(EXEC)
	install -m 755 $(EXEC) $(PREFIX)/bin
	@echo "$(EXEC) has been installed to $(PREFIX)/bin"

clean:
	rm -f $(OBJ) $(EXEC)

uninstall:
	rm -f $(PREFIX)/bin/$(EXEC)
	@echo "$(EXEC) has been uninstalled from $(PREFIX)/bin"

endif

# Phony targets (not actual file names)
.PHONY: all clean install uninstall
