CFLAGS=-g -Wall -Werror -Wpedantic --std=gnu99

all: resolve

resolve: resolve.c -lm

clean:
	rm -rf *~ *.o resolve

.PHONY : clean all