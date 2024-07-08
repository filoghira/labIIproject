all:
	gcc  -o pagerank *.c -lpthread -Wall -Wextra -pedantic -std=c99 -O3 -lm

clean:
	rm -f $(OBJS) $(OUT)
