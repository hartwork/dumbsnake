dumbsnake: dumbsnake.c
	$(CC) -lcurses -lrt $(CFLAGS) $(LDFLAGS) dumbsnake.c -o dumbsnake
