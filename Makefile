CC=gcc
CFLAGS=-Wall -Wextra -Wpedantic -Wvla -DDEBUG -g -std=c11
SOURCES=$(wildcard src/**.c)
OBJECTS=$(patsubst src/%.c, build/%.o, $(SOURCES))
EXE=fungus

all: $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(EXE)

clean:
	rm $(wildcard build/**.o)

# make is actually kind of dope, what?
build/%.o : src/%.c
	$(CC) $(CFLAGS) -c $< -o $@
