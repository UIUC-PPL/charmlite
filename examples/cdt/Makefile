include ../../common.mk

all: pgm

pgm: pgm.o ../../libs/core.o
	$(CXX) $(OPTS) pgm.o ../../libs/core.o -o pgm

pgm.o: pgm.cpp
	$(CXX) $(OPTS) -c -o pgm.o pgm.cpp

test: pgm
	./charmrun +p$(CMK_NUM_PES) ./pgm $(TESTOPTS)
