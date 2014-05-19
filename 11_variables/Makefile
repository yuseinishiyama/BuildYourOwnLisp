program = variables
objs = variables.c ../mpc/mpc.c

variables: $(objs)
	cc -std=c99 -Wall -I../mpc -ledit -lm -o $(program) $^
	etags *.[ch]

clean:
	$(RM) $(program) TAGS
