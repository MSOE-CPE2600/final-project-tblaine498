# Modified by: Tyler Blaine
# Assignments: 11/12
# Section: CPE2600 111

CC=gcc
CFLAGS=-c -Wall -g
LDFLAGS=-lm
SOURCES= main.c
OBJECTS=$(SOURCES:.c=.o)
EXECUTABLE=image

all: $(SOURCES) $(EXECUTABLE) 

# pull in dependency info for *existing* .o files
-include $(OBJECTS:.o=.d)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

.c.o: 
	$(CC) $(CFLAGS) $< -o $@
	$(CC) -MM $< > $*.d

clean:
	rm -rf $(OBJECTS) $(EXECUTABLE) *.d