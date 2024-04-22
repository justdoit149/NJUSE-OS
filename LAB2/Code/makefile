all: main

main: main.cpp my_print.asm	
	nasm -f elf32 my_print.asm -o my_print.o
	g++ -m32 main.cpp my_print.o -o main
	./main

clean:
	rm -f main *.o main.exe
