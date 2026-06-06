# Compiler choice
CC = gcc

# Strict Flag Collection
# -Werror: Turn warnings into errors
# -Wpedantic: Reject everything that isn't ISO C
# -Wextra: The "extra" warnings often missed by -Wall
STRICT_FLAGS = -Wall -Wextra -Wpedantic -Werror -Wuninitialized -Wmaybe-uninitialized \
	-Wconversion -Wsign-conversion -Wcast-align -Wcast-qual -Wstrict-aliasing=2 -Wpointer-arith \
	-Warray-bounds -Wnull-dereference -Wmissing-prototypes -Wstrict-prototypes \
	-Wold-style-definition -Wredundant-decls -Wshadow -Wundef -Wformat=2 -Wformat-security \
	-Wwrite-strings -Wvla -Wdouble-promotion -Wfloat-equal -Wswitch-enum -Wswitch-default \
	-Wunused -Wunused-function -Wunused-variable -Wunused-parameter -Wduplicated-cond \
	-Wduplicated-branches -Wlogical-op -Wno-padded -Wno-declaration-after-statement

# Combine with standard flags and optimization
CFLAGS = -std=c23 $(STRICT_FLAGS) -O3 -march=native -Iinclude -MMD -MP -D_POSIX_C_SOURCE=199309L -flto -ffast-math #-fsanitize=address

# Linker flags (for libraries)
LDFLAGS = -L/usr/local/lib -lsodium -ldotenv -linih -lyyjson -lcurl -lm -lssl -lcrypto -ffast-math -flto #-fsanitize=address 

# Automatically find all subdirectories inside deps/ and format them as -Ideps/<lib>
DEP_DIRS := $(wildcard deps/*/)
DEP_INCLUDES := $(patsubst %, -I%, $(DEP_DIRS))

# Append them to your CFLAGS
CFLAGS += $(DEP_INCLUDES)

# Target definition
SRC_SRC := $(shell find src -name '*.c')
DEP_SRC := deps/c-rs-std/io.c

OBJ := $(patsubst src/%.c,build/%.o,$(SRC_SRC))
OBJ += build/deps/c-rs-std/io.o
	
DEP := $(OBJ:.o=.d)
TARGET = oci_sdk

$(TARGET): $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) -o $@

print-version:
	@$(CC) --version

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# 4. Rule to compile dependency files
build/deps/c-rs-std/%.o: deps/c-rs-std/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET)
	rm -rf compile_commands.json
	rm -rf build

all: compile_commands 

.PHONY: clean
.PHONY: compile_commands $(TARGET)
compile_commands:
	bear -- make

-include $(DEP)