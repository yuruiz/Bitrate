# Some variables
CC 		= gcc
CFLAGS		= -g -Wall -DDEBUG
LDFLAGS		= -lm
TESTDEFS	= -DTESTING			# comment this out to disable debugging code
OBJS		= proxy.o conn.o io.o log.o parse.o socket.o bitrate.o queue.o response.o mydns.o
MK_CHUNK_OBJS   = make_chunks.o chunk.o sha.o
NS_OBJS     = nameserver.o log.o mydns.o
SOURCE=src
VPATH=$(SOURCE)

BINS          = proxy nameserver

# Implicit .o target
.c.o:
	$(CC) $(TESTDEFS) -c $(CFLAGS) $<

# Explit build and testing targets

all: ${BINS}  clean_obj


run_1:
	./proxy log_101.txt 0.1 15641 1.0.0.1 127.0.0.1 15441 3.0.0.1

run_2:
	./proxy log_201.txt 0.1 15741 2.0.0.1 127.0.0.1 15441

dns_server:
	./nameserver -r log_DNS.txt 5.0.0.1 15441 ./topos/topo1/topo1.servers ./topos/topo1/topo1.lsa


spiffy:
	./hupsim.pl -m topo.map -n nodes.map -p 12345 -v 0 &

test: peer_test
	./peer_test

proxy: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

nameserver: $(NS_OBJS)
	$(CC) $(CFLAGS) $(NS_OBJS) -o $@ $(LDFLAGS)

make-chunks: $(MK_CHUNK_OBJS)
	$(CC) $(CFLAGS) $(MK_CHUNK_OBJS) -o $@ $(LDFLAGS)



clean:
	rm -f *.o $(BINS)

clean_obj:
	rm -f *.o


