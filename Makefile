CXX ?= g++
CXXFLAGS := -Wall -Wextra -O3 -std=c++20 $(CXXFLAGS)

prefix ?= /usr

all: test

test: test.cpp safmat.hpp
	$(CXX) -o $@ $< $(CXXFLAGS)

install: test
	#install -vDm644 safmat.hpp $(DESTDIR)$(prefix)/include/safmat.hpp

run: test
	./test

clean:
	rm -f test

.PHONY: all clean install run
