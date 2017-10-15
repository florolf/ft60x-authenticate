all: authenticate.c
	gcc -o authenticate -Wall -Wextra authenticate.c `pkg-config --cflags --libs libusb-1.0`

clean:
	rm -f authenticate
