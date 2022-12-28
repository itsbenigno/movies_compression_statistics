all: compile
	
compile: 
	g++ -o stats compute_statistics.cpp

run: 
	./stats