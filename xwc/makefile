hashtable_dir = ../hashtable/
holdall_dir = ../holdall/
wordcounter_dir = ../wordcounter/
CC = gcc
CFLAGS = -std=c2x \
  -Wall -Wconversion -Werror -Wextra -Wpedantic -Wwrite-strings \
  -O0 -g \
  -I$(hashtable_dir) -I$(holdall_dir) -I$(wordcounter_dir) \
  -DHASHTABLE_STATS=0 \
  -DHOLDALL_PUT_TAIL=1
vpath %.c $(hashtable_dir) $(holdall_dir) $(wordcounter_dir)
vpath %.h $(hashtable_dir) $(holdall_dir) $(wordcounter_dir)
objects = main.o hashtable.o holdall.o wordcounter.o
executable = xwc
makefile_indicator = .\#makefile\#

.PHONY: all clean

all: $(executable)

clean:
	$(RM) $(objects) $(executable)
	@$(RM) $(makefile_indicator)

$(executable): $(objects)
	$(CC) $(objects) -o $(executable)

main.o: main.c hashtable.h holdall.h wordcounter.h
hashtable.o: hashtable.c hashtable.h
holdall.o: holdall.c holdall.h
wordcounter.o: hashtable.c hashtable.h holdall.c holdall.h

include $(makefile_indicator)

$(makefile_indicator): makefile
	@touch $@
	@$(RM) $(objects) $(executable)
