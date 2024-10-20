output: tarea1.o
	g++ tarea1.o -o Tarea1
tarea1.o: tarea1.cpp
	g++ -c tarea1.cpp
clean:
	rm *.o Tarea1
