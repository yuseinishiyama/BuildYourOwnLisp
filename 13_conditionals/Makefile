program = conditionals
objs = conditionals.c ../mpc/mpc.c

conditionals: $(objs)
	cc -std=c99 -Wall -I../mpc -ledit -lm -o $(program) $^
	etags *.[ch]

clean:
	$(RM) $(program) TAGS
