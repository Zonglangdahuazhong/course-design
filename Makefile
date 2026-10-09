CC = gcc

CFLAGS = -std=c11 -Wall -Wextra -g -Iinclude

GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0)
GTK_LIBS = $(shell pkg-config --libs gtk+-3.0)

SRC = \
	src/map.c \
	src/order.c \
	src/dijkstra.c \
	src/distance.c \
	src/tsp.c \
	src/path_restore.c \
	src/rtree.c \
	gui.c

TARGET = campus_gui


all:
	$(CC) $(CFLAGS) $(GTK_CFLAGS) $(SRC) -o $(TARGET) $(GTK_LIBS) -lm


run: all
	./$(TARGET)


clean:
	rm -f $(TARGET)