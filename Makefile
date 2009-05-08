SOURCES=main.cc input.cc
OBJS=$(patsubst %.cc,%.o, $(SOURCES))
CXXFLAGS=`pkg-config --cflags glibmm-2.4` -g
LDFLAGS=`pkg-config --libs glibmm-2.4` -lelf

ci: $(OBJS)
	g++ -o ci $(OBJS) $(LDFLAGS)

.cc.o: 
	g++ -c -o $@ $< ${CXXFLAGS}

clean:
	rm -f $(OBJS)
