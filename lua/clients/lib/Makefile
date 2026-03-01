# makefile for lualib

INC= $(LUA)/include
LIB= $(LUA)/lib

CC= gcc
CFLAGS= -Wall -O2 -I$(INC) $(DEFS) -fPIC

OBJS= iolib.o mathlib.o strlib.o
SLIB= $(LIB)/liblualib.a
DLIB= $(LIB)/liblualib.so.1.1

libs: $(SLIB) $(DLIB)

$(SLIB): $(OBJS)
	ar rcs $@ $(OBJS)
	ranlib $(SLIB)

$(DLIB): $(OBJS)
	$(CC) -shared -o $@ $(OBJS)

clean:
	rm -f $(OBJS) $(SLIB) $(DLIB)
