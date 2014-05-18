s_expressions: s_expressions.c ../mpc/mpc.c
	cc -std=c99 -Wall -I../mpc -ledit -lm -o s_expressions s_expressions.c ../mpc/mpc.c
	etags *.[ch]

clean:
	rm s_expressions
	rm TAGS
