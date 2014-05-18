program = q_expressions
objs = q_expressions.c ../mpc/mpc.c

evaluation: $(objs)
	cc -std=c99 -Wall -I../mpc -ledit -lm -o $(program) $^
	etags *.[ch]

clean:
	$(RM) $(program) TAGS
