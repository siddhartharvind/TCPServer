CXX=gcc
CXXFLAGS=

all: server
server: server.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<


.PHONY: clean
clean:
	$(RM) server
