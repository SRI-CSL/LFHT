OS   =  $(shell uname)

ARCH ?= $(shell uname -m)


ifneq ($(ARCH), x86_64)
  $(error  "Got me just one ARCH today, and that puppy is x86_64.")
endif


LIBRARYNAME=liblfht

ifeq (Darwin, $(findstring Darwin, ${OS}))
#  DARWIN
LIB = ${LIBRARYNAME}.dylib
LIBFLAGS = -dynamiclib
LDFLAGS =
else
# LINUX
LIB = ${LIBRARYNAME}.so
LIBFLAGS = -shared -Wl,-soname,${LIB}
LDFLAGS = -lpthread
endif


CFLAGS= -Wall -fPIC -I./include -I./arch/${ARCH} -g

SRC_GLOBS = $(addsuffix /*.c,src)
SRC = $(sort $(wildcard $(SRC_GLOBS)))



OBJ = $(patsubst src/%.c, obj/%.o, $(SRC))

TARGET = lib/${LIB}

.PHONY: clean

all: ${TARGET}
	@echo Done.	


$(TARGET): $(OBJ) | lib
	$(CC) $(LIBFLAGS) -o $@ $(OBJ) 


obj:
	mkdir -p obj

obj/%.o: src/%.c | obj
	${CC} ${CFLAGS} $< -c -o $@

lib:
	mkdir -p lib


TESTS=./tests

check: ${TARGET}
	make -C ${TESTS} check

vcheck: ${TARGET}
	make -C ${TESTS} vcheck

stress: ${TARGET}
	make -C ${TESTS} stress

clean:
	rm -f *~ *.o
	rm -rf obj lib
	make -C ${TESTS} clean



