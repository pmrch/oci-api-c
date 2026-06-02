# Compiler choice
CC = gcc

# Strict Flag Collection
# -Werror: Turn warnings into errors
# -Wpedantic: Reject everything that isn't ISO C
# -Wextra: The "extra" warnings often missed by -Wall
STRICT_FLAGS = -Wall -Wextra -Wpedantic -Werror -Wformat=2 \
               -Wnull-dereference -Wduplicated-cond -Wduplicated-branches \
               -Wshadow -Wundef -Wcast-qual -Wconversion -Wlogical-op \
               -Wdouble-promotion -Wfloat-equal -Wstrict-prototypes

# Combine with standard flags and optimization
CFLAGS = -std=c23 $(STRICT_FLAGS) -O2 -Iinclude

# Linker flags (for libraries)
LDFLAGS = -L/usr/local/lib -ldotenv -linih -lyyjson

# Target definition
SRC := $(shell find src -name '*.c')
OBJ := $(patsubst src/%.c,build/%.o,$(SRC))
DEP := $(OBJ:.o=.d)

TARGET = oci_sdk
$(TARGET): $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) -o $@

print-version:
	@$(CC) --version

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build

fclean: clean
	rm -f $(TARGET)

re: fclean $(TARGET)

.PHONY: clean fclean re

.PHONY: compile_commands
compile_commands:
	bear -- make

-include $(DEP)