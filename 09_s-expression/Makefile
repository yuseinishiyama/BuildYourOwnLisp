evaluation: s_expression.c ../mpc/mpc.c
	cc -std=c99 -Wall -I../mpc -ledit -lm -o s_expression s_expression.c ../mpc/mpc.c
	etags *.[ch]

clean:
	rm s_expression
	rm TAGS
