NAME = proj2
EXEC = $(NAME)
sources = $(NAME).c

CXX = gcc
RM = rm -f


CFLAGS = -std=gnu99 -Wall -Wextra -pedantic -Werror
LDFLAGS = -lrt -pthread

OBJFILES = $(sources:.c=.o)

.PHONY : all

all : $(EXEC) 

%.o : %.c 
	$(CXX) $(CFLAGS) -c $< -o $@

$(EXEC) : $(OBJFILES)
	$(CXX) $(CFLAGS) -o $@ $(OBJFILES) $(LDFLAGS)

clean:
	$(RM) *.o core *.out

cleanall: clean
	$(RM) $(EXEC)
