CFLAGS=-std=gnu99 -Wall -g

# not sure if this works though

servers: SimpleServer.o PersistentServer.o Helpers.o
	gcc $(CFLAGS) -lm -o $@ $^

%.o : %.c HTTPServer.h
	gcc $(CFLAGS) -c $<

clean : 
	rm -f *.o SimpleServer Persistent Helpers *~