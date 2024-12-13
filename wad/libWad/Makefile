all: Wad.o libWad.a

Wad.o: Wad.cpp Wad.h
	g++ -c Wad.cpp -o Wad.o -I.
libWad.a: Wad.o
	ar cr libWad.a Wad.o

clean:
	rm -f Wad.o libWad.a