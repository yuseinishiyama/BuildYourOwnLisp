program = functions
objs = functions.c ../mpc/mpc.c

functions: $(objs)
	cc -std=c99 -Wall -I../mpc -ledit -lm -o $(program) $^
	etags *.[ch]

clean:
	$(RM) $(program) TAGS
