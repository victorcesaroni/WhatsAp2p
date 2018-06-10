HEADERS = networklib.h linkedlist.h shareddefs.h 
LIBS = -pthread

default: centralserv client

centralserv.o: centralserv.c $(HEADERS)
	gcc -c centralserv.c -o centralserv.o $(LIBS)

client.o: client.c $(HEADERS)
	gcc -c client.c -o client.o $(LIBS)

centralserv: centralserv.o
	gcc centralserv.o -o cs $(LIBS)
		
client: client.o
	gcc client.o -o cl $(LIBS)

clean:
	-rm -f centralserv.o
	-rm -f centralserv
	-rm -f client.o
	-rm -f client
	-rm -f cl
	-rm -f cs
	
runcl:
	./cl

runcs:
	./cs 7777

