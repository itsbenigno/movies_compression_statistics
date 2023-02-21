all: compile run
	
compile: 
	g++ -o stats statistics.cpp -lgsl

run: 
	./stats