CFLAGS=--std=c17 -Wall -pedantic -Isrc/ -ggdb -Wextra -Werror -DDEBUG
BUILDDIR=build
SRCDIR=src
CC=gcc

all: $(BUILDDIR)/main.o $(BUILDDIR)/mem.o $(BUILDDIR)/util.o $(BUILDDIR)/test.o $(BUILDDIR)/mem_debug.o
	$(CC) -o $(BUILDDIR)/main $^

build:
	mkdir -p $(BUILDDIR)

$(BUILDDIR)/main.o: $(SRCDIR)/main.c build
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILDDIR)/mem.o: $(SRCDIR)/mem.c build
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILDDIR)/mem_debug.o: $(SRCDIR)/mem_debug.c build
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILDDIR)/util.o: $(SRCDIR)/util.c build
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILDDIR)/test.o: $(SRCDIR)/test.c build
	$(CC) -c $(CFLAGS) $< -o $@


clean:
	rm -rf $(BUILDDIR)

