.PHONY: examples
examples: example_c example_cpp
	./example_c
	./example_cpp

example_c: example.c benc.h
	$(CC) example.c -o example_c

example_cpp: example.c benc.h
	$(CXX) -std=c++11 example.cc -o example_cpp

.PHONY: clean
clean:
	-rm example_*
