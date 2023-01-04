all: compile
	
compile: 
	g++ -o stats compute_statistics.cpp -lgsl

run: 
	./stats