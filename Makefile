CC := $(Q)$(CXX)
RM := $(Q)$(RM)

.PHONY: all clean install

TARGETS := hit-proc

all: $(TARGETS)

clean:
	$(RM) $(TARGETS) *.o

hit-proc: hit-proc.o
	$(CXX) -o $@ $^

