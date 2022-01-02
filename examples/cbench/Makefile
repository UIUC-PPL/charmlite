include ../../common.mk

OPTS:=$(OPTS) -Ofast

all: pgm

pgm: pgm.o ../../libs/core.o
	$(CXX) $(OPTS) ../../libs/core.o pgm.o -o pgm

pgm.o: pgm.cpp
	$(CXX) $(OPTS) -c -o pgm.o pgm.cpp

test: pgm
	./charmrun +p$(CMK_NUM_PES) ./pgm $(TESTOPTS)
