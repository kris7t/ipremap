
CXXFLAGS += -std=c++11

.PHONY: all clean

all: ipremap ipremapd

clean:
	rm -rf *.o ipremap ipremapd

ipremap: ipremap.o

ipremapd: \
	ipremapd.o \
	remap_chain.o \
	mapper.o \
	server.o
	$(CXX) $^ $(LDFLAGS) -o $@
