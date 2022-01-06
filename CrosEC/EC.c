#include <ntddk.h>
#include <wdf.h>
#include <initguid.h>

#include "EC.h"

static __inline void outb(unsigned char __val, unsigned int __port)
{
	WRITE_PORT_UCHAR((PUCHAR)__port, __val);
}

static __inline void outw(unsigned short __val, unsigned int __port)
{
	WRITE_PORT_USHORT((PUSHORT)__port, __val);
}

static __inline unsigned char inb(unsigned int __port)
{
	return READ_PORT_UCHAR((PUCHAR)__port);
}

static __inline unsigned short inw(unsigned int __port)
{
	return READ_PORT_USHORT((PUSHORT)__port);
}

typedef enum _ec_transaction_direction { EC_TX_WRITE, EC_TX_READ } ec_transaction_direction;

// As defined in MEC172x section 16.8.3
// https://ww1.microchip.com/downloads/en/DeviceDoc/MEC172x-Data-Sheet-DS00003583C.pdf
#define FW_EC_BYTE_ACCESS               0x00
#define FW_EC_LONG_ACCESS_AUTOINCREMENT 0x03

#define FW_EC_EC_ADDRESS_REGISTER0      0x0802
#define FW_EC_EC_ADDRESS_REGISTER1      0x0803
#define FW_EC_EC_DATA_REGISTER0         0x0804
#define FW_EC_EC_DATA_REGISTER1         0x0805
#define FW_EC_EC_DATA_REGISTER2         0x0806
#define FW_EC_EC_DATA_REGISTER3         0x0807

typedef USHORT uint16_t;
typedef ULONG uint32_t;
typedef UCHAR uint8_t;

static int ec_transact(ec_transaction_direction direction, uint16_t address,
	char* data, uint16_t size)
{
	int pos = 0;
	uint16_t temp[2];
	if (address % 4 > 0) {
		outw((address & 0xFFFC) | FW_EC_BYTE_ACCESS, FW_EC_EC_ADDRESS_REGISTER0);
		/* Unaligned start address */
		for (int i = address % 4; i < 4; ++i) {
			char* storage = &data[pos++];
			if (direction == EC_TX_WRITE)
				outb(*storage, FW_EC_EC_DATA_REGISTER0 + i);
			else if (direction == EC_TX_READ)
				*storage = inb(FW_EC_EC_DATA_REGISTER0 + i);
		}
		address = (address + 4) & 0xFFFC; // Up to next multiple of 4
	}

	if (size - pos >= 4) {
		outw((address & 0xFFFC) | FW_EC_LONG_ACCESS_AUTOINCREMENT, FW_EC_EC_ADDRESS_REGISTER0);
		// Chunk writing for anything large, 4 bytes at a time
		// Writing to 804, 806 automatically increments dest address
		while (size - pos >= 4) {
			if (direction == EC_TX_WRITE) {
				memcpy(temp, &data[pos], sizeof(temp));
				outw(temp[0], FW_EC_EC_DATA_REGISTER0);
				outw(temp[1], FW_EC_EC_DATA_REGISTER2);
			}
			else if (direction == EC_TX_READ) {
				temp[0] = inw(FW_EC_EC_DATA_REGISTER0);
				temp[1] = inw(FW_EC_EC_DATA_REGISTER2);
				memcpy(&data[pos], temp, sizeof(temp));
			}

			pos += 4;
			address += 4;
		}
	}

	if (size - pos > 0) {
		// Unaligned remaining data - R/W it by byte
		outw((address & 0xFFFC) | FW_EC_BYTE_ACCESS, FW_EC_EC_ADDRESS_REGISTER0);
		for (int i = 0; i < (size - pos); ++i) {
			char* storage = &data[pos + i];
			if (direction == EC_TX_WRITE)
				outb(*storage, FW_EC_EC_DATA_REGISTER0 + i);
			else if (direction == EC_TX_READ)
				*storage = inb(FW_EC_EC_DATA_REGISTER0 + i);
		}
	}
	return 0;
}

/*
 * Wait for the EC to be unbusy.  Returns 0 if unbusy, non-zero if
 * timeout.
 */
static int wait_for_ec(int status_addr, int timeout_usec)
{
	int i;
	int delay = 5;

	for (i = 0; i < timeout_usec; i += delay) {
		/*
		 * Delay first, in case we just sent out a command but the EC
		 * hasn't raised the busy flag.  However, I think this doesn't
		 * happen since the LPC commands are executed in order and the
		 * busy flag is set by hardware.  Minor issue in any case,
		 * since the initial delay is very short.
		 */
		//KTIMER kDelayTimer;
		//KeInitializeTimer(&kDelayTimer);
		//KeSetTimerEx(&kDelayTiemr, )
		//MicroSecondDelay(MIN(delay, timeout_usec - i));

		if (!(inb(status_addr) & EC_LPC_STATUS_BUSY_MASK))
			return 0;

		/* Increase the delay interval after a few rapid checks */
		if (i > 20)
			delay = min(delay * 2, 10000);
	}
	return -1; /* Timeout */
}

static uint8_t ec_checksum_buffer(char* data, int size)
{
	uint8_t sum = 0;
	for (int i = 0; i < size; ++i) {
		sum += data[i];
	}
	return sum;
};

int ec_command(int command, int version, const void* outdata,
	int outsize, void* indata, int insize)
{
	uint8_t csum = 0;
	int i;

	union {
		struct ec_host_request rq;
		char data[EC_LPC_HOST_PACKET_SIZE];
	} u;

	union {
		struct ec_host_response rs;
		char data[EC_LPC_HOST_PACKET_SIZE];
	} r;

	/* Fail if output size is too big */
	if (outsize + sizeof(u.rq) > EC_LPC_HOST_PACKET_SIZE)
		return -EC_RES_REQUEST_TRUNCATED;

	/* Fill in request packet */
	/* TODO(crosbug.com/p/23825): This should be common to all protocols */
	u.rq.struct_version = EC_HOST_REQUEST_VERSION;
	u.rq.checksum = 0;
	u.rq.command = (uint16_t)command;
	u.rq.command_version = (uint8_t)version;
	u.rq.reserved = 0;
	u.rq.data_len = (uint16_t)outsize;

	memcpy(&u.data[sizeof(u.rq)], outdata, outsize);
	csum = ec_checksum_buffer(u.data, outsize + sizeof(u.rq));
	u.rq.checksum = (uint8_t)(-csum);

	if (wait_for_ec(EC_LPC_ADDR_HOST_CMD, 1000000)) {
		return -EC_RES_ERROR;
	}

	ec_transact(EC_TX_WRITE, 0, u.data, (uint16_t)(outsize + sizeof(u.rq)));

	/* Start the command */
	outb(EC_COMMAND_PROTOCOL_3, EC_LPC_ADDR_HOST_CMD);

	if (wait_for_ec(EC_LPC_ADDR_HOST_CMD, 1000000)) {
		return -EC_RES_ERROR;
	}

	/* Check result */
	i = inb(EC_LPC_ADDR_HOST_DATA);
	if (i) {
		return -EECRESULT - i;
	}

	csum = 0;
	ec_transact(EC_TX_READ, 0, r.data, sizeof(r.rs));

	if (r.rs.struct_version != EC_HOST_RESPONSE_VERSION) {
		return -EC_RES_INVALID_RESPONSE;
	}

	if (r.rs.reserved) {
		return -EC_RES_INVALID_RESPONSE;
	}

	if (r.rs.data_len > insize) {
		return -EC_RES_RESPONSE_TOO_BIG;
	}

	if (r.rs.data_len > 0) {
		ec_transact(EC_TX_READ, 8, r.data + sizeof(r.rs), r.rs.data_len);
		if (ec_checksum_buffer(r.data, sizeof(r.rs) + r.rs.data_len)) {
			return -EC_RES_INVALID_CHECKSUM;
		}

		memcpy(indata, r.data + sizeof(r.rs), r.rs.data_len);
	}
	return r.rs.data_len;
}