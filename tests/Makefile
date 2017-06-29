LFHT_HOME=..


OS   =  $(shell uname)

LIBRARYNAME=liblfht

ifeq (Darwin, $(findstring Darwin, ${OS}))
LDFLAGS =
else
LDFLAGS = -lpthread
endif

CFLAGS= -Wall  -I${LFHT_HOME}/include -I${LFHT_HOME}/arch/${ARCH} -g

OBJDIR= ${LFHT_HOME}/obj
OBJ_GLOBS = $(addsuffix /*.o,${OBJDIR})
OBJS = $(sort $(wildcard ${OBJ_GLOBS}))



TESTS = test_one test_two test_three test_four test_five test_six test_seven test_eight futz

all: ${TESTS}


%: %.c
	${CC} ${CFLAGS} $< -o $@ ${OBJS} ${LDFLAGS}


check: ${TESTS}

	./test_one
	./test_three 4
	./test_three 8
	./test_two  32 1
	./test_two  32 4
	./test_two  32 8
	./test_two  32 16
	./test_two  64 16
	./test_four 64 16
	./test_five 16 16
	./test_five 32 16	
	./test_five 24 16	
	./test_seven 16 16
	./test_seven 32 16	
	./test_seven 24 16	
	./test_eight  32 1
	./test_eight  32 4
	./test_eight  32 8
	./test_eight  32 16
	./test_eight  64 16

vcheck: ${TESTS}
	valgrind ./test_one
	valgrind ./test_two  32 1
	valgrind ./test_three 4
	valgrind ./test_four 64 16

stress: ${TESTS}
	set -e; number=1 ; while [ $$number -le 1000000 ] ; do \
	   echo "$$number : ./test_two  32 4" ; \
	   ./test_two  32 4 ; \
	   echo "$$number : ./test_two  32 8" ; \
	   ./test_two  32 8 ; \
	   echo "$$number : /test_two  32 16" ; \
	   ./test_two  32 16 ; \
	   echo "$$number : ./test_two  64 16" ; \
	   ./test_two  64 16 ; \
	   echo "$$number :./test_four 64 16 " ; \
	   ./test_four 64 16 ; \
	   echo "$$number : ./test_five 16 16" ; \
	   ./test_five 16 16 ; \
	   echo "$$number : ./test_five 32 16" ; \
	   ./test_five 32 16 ; \
	   echo "$$number : ./test_five 24 16" ; \
	   ./test_five 24 16 ; \
	   number=`expr $$number + 1` ;  \
	   done

stress_: ${TESTS}
	set -e; number=1 ; while [ $$number -le 100 ] ; do \
	   echo $$number ; \
	   ./test_four 64 16 ; \
	   number=`expr $$number + 1` ;  \
	   done


hell:
	valgrind --tool=helgrind ./test_two  32 1


clean:
	${RM} -rf ${TESTS} *.dSYM
