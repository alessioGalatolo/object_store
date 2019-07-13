#########################################
#	Progetto SOL AA 2018/2019	# 
#	Alessio Galatolo		#
#########################################



CC          =  gcc
AR          =  ar
CFLAGS      += -std=c99 -Wall -Werror -g
ARFLAGS     =  rvs
INCLUDES	= -I.
LDFLAGS		= -L.
LIBS        = -pthread -lm
OPTFLAGS	= -O3 


TARGETS		= object_store \
		client_test
		  
.PHONY: all clean test
.SUFFIXES: .c .h

%: %.c
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $< $(LDFLAGS) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -c -o $@ $< $(LIBS)

%.a: %.o
	$(AR) $(ARFLAGS) lib$@ $<

object_store: object_store.c utils.h threadpool.a
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $< $(LDFLAGS) -lthreadpool $(LIBS)

client_test: client_test.c objstr.a
	$(CC) $(CFLAGS) $(INCLUDES) $(OPTFLAGS) -o $@ $< $(LDFLAGS) -lobjstr $(LIBS)

all		: $(TARGETS)

clean		: 
	rm -f $(TARGETS)
	rm -f -r data
	rm -f *.o *.a
	rm -f testout.log
	rm -f objstore.sock

test:	all
	./object_store &
	./test.sh
	./testsum.sh

