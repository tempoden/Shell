EXECUTABLE = $(notdir $(PWD))

SOURCES = $(wildcard *.c)
OBJECTS = $(SOURCES:.c=.o)

$(EXECUTABLE): $(OBJECTS)
	$(CC) -o $(EXECUTABLE) $(OBJECTS)

clean:
	rm -f $(EXECUTABLE) $(OBJECTS)
