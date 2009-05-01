SOURCES=main.cc input.cc
OBJS=$(patsubst .cc,.o, $(SOURCES))
CXXFLAGS=`pkg-config --cflags glibmm-2.4`
LDFLAGS=`pkg-config --libs glibmm-2.4`

ci: $(OBJS)
	g++ -o ci $(OBJS) $(LDFLAGS)

.o.cc:
	g++ -c -o $< $@ ${CXXFLAGS}
