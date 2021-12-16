include ../../common.mk

all: pgm

pgm: pgm.o ../../libs/core.o
	$(CXX) $(OPTS) ../../libs/core.o pgm.o -o pgm

pgm.o: pgm.cc
	$(CXX) $(OPTS) -c -o pgm.o pgm.cc

test: pgm
	./charmrun +p$(CMK_NUM_PES) ./pgm $(TESTOPTS)
