CC = gcc
CXX = g++
INCLUDE_OPENCV = `pkg-config --cflags --libs opencv`
LINK_PTHREAD = -lpthread
CFLAG = -std=c++17

CLIENT = client.cpp
SERVER = server.cpp
CLI = client
SER = server

all: server client
  
server: $(SERVER)
	$(CXX) $(SERVER) -o $(SER) $(INCLUDE_OPENCV) $(LINK_PTHREAD) $(CFLAG)
client: $(CLIENT)
	$(CXX) $(CLIENT) -o $(CLI) $(INCLUDE_OPENCV) $(LINK_PTHREAD) $(CFLAG)

.PHONY: clean

clean:
	rm $(CLI) $(SER)
