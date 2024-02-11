CXX=g++
CXXFLAGS=-std=c++11 -fno-stack-protector
LDLIBS=-pthread

RM=rm

# Final binary -> root/bin/parallel_server
BIN=../../bin/parallel_server

.PHONY: all
all: $(BIN)

Tcp_Server.o: Tcp_Server.cpp Tcp_Server.hpp
	$(CXX) $(CXXFLAGS) $(LDLIBS) -c Tcp_Server.cpp -o Tcp_Server.o

main.o: main.cpp Tcp_Server.hpp
	$(CXX) $(CXXFLAGS) $(LDLIBS) -c main.cpp -o main.o

$(BIN): main.o Tcp_Server.o
	$(CXX) $(CXXFLAGS) $(LDLIBS) main.o Tcp_Server.o -o $(BIN)


.PHONY: clean
clean:
# '-' at the beginning to ignore return value
# i.e. no error if no matched file found
	-$(RM) *.o *.out
