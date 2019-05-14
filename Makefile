CXX = g++

BINARIES = sample_grader thread_lib test grader

all: sample_grader test

test: test.cpp thread_lib
	${CXX} -g test.cpp threads.o -o test

sample_grader: main.cpp thread_lib
	${CXX} main.cpp threads.o -o sample_grader

grader: final.cpp thread_lib
		${CXX} final.cpp threads.o -o grader

thread_lib: threads.cpp
	${CXX} -c threads.cpp -o threads.o

clean:
	/bin/rm -f *.o ${BINARIES}

#gcc -c -o main.o main.c
#gcc -c -o threads.o threads.c
#gcc -c -o application main.o threads.o
