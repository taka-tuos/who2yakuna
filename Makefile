TARGET		= who2yakuna
OBJS_TARGET	= main.o

CFLAGS = -g
LDFLAGS = 
LIBS = -lc -lm -lcurl -ljson-c -lncursesw -lpthread

include Makefile.in
