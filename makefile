all:
	gcc  -o pagerank *.c -lpthread

clean:
	rm -f $(OBJS) $(OUT)
