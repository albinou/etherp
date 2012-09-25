PROGS= etherp-send etherp-recv

all: $(PROGS)

clean:
	rm -f *~ $(PROGS)

.c:
	$(CC) -static -Wall -O2 $< -lz -o $@
