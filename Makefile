.PHONY = default all clean

ROSE_PATH = ../rose

TARGET = vmls
CC = gcc
IFLAGS = -I. -I$(ROSE_PATH)
CFLAGS = -g -Wall
LFLAGS = -L. -L$(ROSE_PATH) -Wl,-rpath,. -Wl,-rpath,$(ROSE_PATH)
LIBS = -lc -lrose -lsqlite3

HEADERS = conf.h db.h rtpp.h h264.h cam.h mtsp.h st.h https.h ws.h
CFILES = conf.c main.c db.c rtpp.c h264.c cam.c mtsp.c st.c https.c ws.c wsl.c
OBJECTS = $(CFILES:.c=.o)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $(IFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS)
	$(CC) $(LFLAGS) $^ -o $@ $(LIBS)

default: $(TARGET)

all: $(TARGET)

clean:
	rm -f *.o *~ $(TARGET)
