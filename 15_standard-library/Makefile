program = lispy
objs = lispy.c ../mpc/mpc.c

lispy: $(objs)
	cc -std=c99 -Wall -I../mpc -ledit -lm -o $(program) $^
	etags *.[ch]

clean:
	$(RM) $(program) TAGS
