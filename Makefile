all: compile run
	
compile: 
	g++ -o stats statistics.cpp -lgsl

run: 
	./stats
	cat movies_compression_statistics.csv