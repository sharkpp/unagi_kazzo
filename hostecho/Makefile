LIBUSB = d:/dev/LibUSB-Win32
KAZZO = ../firmware

OBJECT = opendevice.o hostecho.o usbdevice.o

CC = gcc
CFLAG = -I$(LIBUSB)/include -I$(KAZZO) -Wall -Werror
#CFLAG += -O2 
CFLAG += -O0 -g
LIB = -L$(LIBUSB)/lib/gcc -lusb

PROGRAM = ../kazzo_test.exe

all: $(PROGRAM)

.c.o:
	$(CC) $(CFLAG) -c $<

$(PROGRAM): $(OBJECT)
	$(CC) -o $(PROGRAM) $(OBJECT) $(LIB)

clean:
	rm -f *.o $(PROGRAM)
hostecho.o: $(KAZZO)/kazzo_request.h
