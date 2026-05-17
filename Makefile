CC     = gcc
CFLAGS = -Wall -Wextra -g -I/usr/include/libnl3
LIBS   = -lcap -ljson-c -lnl-3 -lnl-route-3 -lutil

SRC = src/main.c \
      src/cli.c \
      src/container.c \
      src/namespaces.c \
      src/rootfs.c \
      src/cgroups.c \
      src/caps.c \
      src/network.c \
      src/state.c \
      src/utils.c \
      src/config.c \
      src/pty.c

OUT = koza

all: $(OUT)

$(OUT): $(SRC)
	$(CC) $(CFLAGS) -o $(OUT) $(SRC) $(LIBS)

clean:
	rm -f $(OUT)
