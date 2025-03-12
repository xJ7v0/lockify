CC = gcc
LIBS = -lX11 -lXss
CFLAGS ?= -Wall -Wextra
SRC = lockify.c
OBJ = lockify.o
EXEC = lockify
INSTALL_DIR = /usr/bin

# Default target: compile and install
all: $(EXEC)

# Rule to compile the source code
$(EXEC): $(OBJ)
	$(CC) ${LIBS} $(OBJ) -o $(EXEC)

# Rule to generate the object file from source
$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c $(SRC)

install: $(EXEC)
	install -m 755 $(EXEC) $(INSTALL_DIR}
	echo "$(EXEC) has been installed to $(INSTALL_DIR)"

clean:
	rm -f $(OBJ) $(EXEC)

uninstall:
	rm -f $(INSTALL_DIR)/$(EXEC)
	echo "$(EXEC) has been uninstalled from $(INSTALL_DIR)"

# Phony targets (not actual file names)
.PHONY: all clean install uninstall
