CC=gcc 
TARGET=picturedsk 
SOURCES=main.c apple_gcr.c bitmap.c bmp_bitmap.c buffered_reader.c woz_image.c
CFLAGS=-O3
LFLAGS=-lm

OBJS=$(SOURCES:.c=.o)

# the target is obtained linking all .o files
all: $(SOURCES) $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LFLAGS) $(OBJS) -o $(TARGET)

purge: clean
	rm -f $(TARGET)

clean:
	rm -f *.o