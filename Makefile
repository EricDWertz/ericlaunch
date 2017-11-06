CC = gcc
CFLAGS = -Wunused-result -g -Wall -lm -lX11 -lXext `pkg-config --cflags gtk+-3.0`
LDFLAGS = `pkg-config --libs gtk+-3.0`
DEPS = eric_window.h
OBJ = eric_window.o main.o

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

ericlaunch: $(OBJ)
	gcc $(LDFLAGS) $(CFLAGS) -o $@ $^
