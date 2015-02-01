LDFLAGS += -lcurses -lrt

all: dumbsnake

clean:
	$(RM) dumbsnake

.PHONY: all clean
