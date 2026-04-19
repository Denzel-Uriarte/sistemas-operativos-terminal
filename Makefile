CFLAGS  = -Wall -Wextra -std=c11
TARGET  = terminal
SRCS    = main.c terminal.c graph.c dispatch.c
OBJS    = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)

main.o:     main.c terminal.h graph.h
terminal.o: terminal.c terminal.h
graph.o:    graph.c graph.h terminal.h
dispatch.o: dispatch.c graph.h terminal.h
