strlcpy.o: strlcpy.c
	gcc -c strlcpy.c
server_f: server_f.c helpers.h strlcpy.o
	gcc -o server_f server_f.c strlcpy.o
server_p: server_p.c helpers.h strlcpy.o
	gcc -o server_p server_p.c strlcpy.o -lpthread
server_s: server_s.c helpers.h strlcpy.o
	gcc -o server_s server_s.c strlcpy.o
all: server_f server_p server_s
clean:
	-rm -f *.o
	-rm -f server_f
	-rm -f server_p
	-rm -f server_s
	-rm -f *~
