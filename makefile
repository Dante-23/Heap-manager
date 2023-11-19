TARGET:exe
exe:main.o mm.o glthread.o
	g++ -std=c++17 -o bin/exe -g bin/main.o bin/mm.o bin/glthread.o
	rm bin/main.o bin/mm.o bin/glthread.o
main.o:main.cpp
	g++ -std=c++17 -c main.cpp -o bin/main.o
mm.o:src/memory_manager.cpp
	g++ -std=c++17 -c src/memory_manager.cpp -o bin/mm.o
glthread.o:src/glthread.cpp
	g++ -std=c++17 -c src/glthread.cpp -o bin/glthread.o
clean:
	rm bin/exe