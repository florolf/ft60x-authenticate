#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <assert.h>

#include <libusb.h>

libusb_context *usb;
libusb_device_handle *usb_handle;

int usb_init(void)
{
	int ret = libusb_init(&usb);
	if(ret) {
		fprintf(stderr, "libusb_init failed: %s\n", libusb_error_name(ret));
		return -1;
	}

	usb_handle = libusb_open_device_with_vid_pid(usb, 0x0403, 0x601f);
	if(!usb_handle) {
		fprintf(stderr, "couldn't find USB device\n");
		return -1;
	}

	return 0;
}

#define FTDI_DFU 0xDF

#define CMD_GET_AUTH_VERSION 0x0001
#define CMD_SET_AUTH_SEED 0x0002
#define CMD_DO_AUTH 0x0003
#define CMD_EXIT_DFU_MODE 0x0004

// based on https://en.wikipedia.org/wiki/Fletcher%27s_checksum
uint32_t fletcher32(const uint8_t *buffer, size_t count)
{
	uint32_t sum1 = 0xffff, sum2 = 0xffff;
	size_t offset = 0;
	count /= 2;

	while (count) {
		size_t tlen = ((count >= 359) ? 359 : count);
		count -= tlen;
		do {
			uint16_t data = buffer[offset] | (buffer[offset + 1] << 8);
			sum2 += sum1 += data;
			offset += 2;
		} while (--tlen > 0);
		sum1 = (sum1 & 0xffff) + (sum1 >> 16);
		sum2 = (sum2 & 0xffff) + (sum2 >> 16);
	}
	/* Second reduction step to reduce sums to 16 bits */
	sum1 = (sum1 & 0xffff) + (sum1 >> 16);
	sum2 = (sum2 & 0xffff) + (sum2 >> 16);
	return (sum2 << 16) | sum1;
}

int cct(uint8_t request_type, uint8_t request, uint16_t value, uint16_t index, uint8_t *data, uint16_t len)
{
	int ret = libusb_control_transfer(usb_handle, request_type, request, value, index, data, len, 500);

	if(ret < 0) {
		fprintf(stderr, "control transfer failed: %s\n", libusb_error_name(ret));
		exit(EXIT_FAILURE);
	}

	return ret;
}

int main(void)
{
	if(usb_init() < 0) {
		fprintf(stderr, "USB initialization failed\n");
		return EXIT_FAILURE;
	}

	assert(libusb_claim_interface(usb_handle, 0) == 0);
	assert(libusb_claim_interface(usb_handle, 1) == 0);

	uint8_t buf[128];

	// verify the authentication version to be 2
	cct(LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_IN, FTDI_DFU, CMD_GET_AUTH_VERSION, 0, buf, 4);
	assert(memcmp(buf, "\x02\x00\x00\x00", 4) == 0);

	// set authentication seed (arbitrary value, needs to be 16 bytes)
	char *seed = "sixteen letters!";
	memcpy(buf, seed, 16);
	cct(LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_OUT, FTDI_DFU, CMD_SET_AUTH_SEED, 0, buf, 16);

	struct libusb_device_descriptor dd;
	assert(sizeof(dd) == 18);

	libusb_get_device_descriptor(libusb_get_device(usb_handle), &dd);
	memcpy(&buf[0], &dd, 18);
	memcpy(&buf[18], seed, 16);

	uint32_t fletcher = fletcher32(buf, 34);
	buf[0] = (fletcher >>  0) & 0xFF;
	buf[1] = (fletcher >>  8) & 0xFF;
	buf[2] = (fletcher >> 16) & 0xFF;
	buf[3] = (fletcher >> 24) & 0xFF;

	// authenticate
	cct(LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE | LIBUSB_ENDPOINT_OUT, FTDI_DFU, CMD_DO_AUTH, 0, buf, 4);

	// reattach as DFU
	cct(LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE | LIBUSB_ENDPOINT_OUT, 0, 0, 0, 0, 0);

	return EXIT_SUCCESS;
}
