# TODO ( switchover to cmake )

CXX=$(CHARM_HOME)/bin/charmc -language converse++

OPTS?=-g3
INCLUDES=-I../../include
OPTS:=$(OPTS) $(INCLUDES) -std=c++11
FORMAT_OPTS?=-i
CMK_NUM_PES?=2
BINARY=pgm

../../libs/core.o: ../../src/core.cc
	mkdir -p ../../libs
	$(CXX) $(OPTS) -c -o ../../libs/core.o ../../src/core.cc

format:
	clang-format $(FORMAT_OPTS) ../../src/*.cc ../../include/*.hh ./$(BINARY).cc

clean:
	rm -f ../../libs/core.o *.o charmrun $(BINARY)