CC=g++
 
.PHONY: all

all: myldapsearch clean

myldapsearch: myldapsearch.o  
	$(CC) myldapsearch.o -o myldapsearch

myldapsearch.o: myldapsearch.cpp 
	$(CC) -c myldapsearch.cpp

clean:
	rm *.o




