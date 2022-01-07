#pragma once

#include <wdf.h>

#ifndef __packed
#define __packed
#endif

#ifndef __aligned
#define __aligned(x)
#endif
#define __ec_align1 __packed
#define __ec_align2 __packed __aligned(2)
#define __ec_align4 __packed __aligned(4)
#define __ec_align_size1 __packed
#define __ec_align_offset1 __packed
#define __ec_align_offset2 __packed __aligned(2)
#define __ec_todo_packed __packed
#define __ec_todo_unpacked

#define EC_LPC_STATUS_FROM_HOST   0x02
#define EC_LPC_STATUS_PROCESSING  0x04
#define EC_LPC_STATUS_BUSY_MASK \
	(EC_LPC_STATUS_FROM_HOST | EC_LPC_STATUS_PROCESSING)
#define EC_LPC_HOST_PACKET_SIZE  0x100  /* Max size of version 3 packet */
#define EC_HOST_REQUEST_VERSION 3
#define EC_COMMAND_PROTOCOL_3 0xda

#define EC_HOST_RESPONSE_VERSION 3
#define EC_LPC_ADDR_HOST_DATA  0x200
#define EC_LPC_ADDR_HOST_CMD   0x204
#define EC_LPC_ADDR_HOST_ARGS    0x800
#define EC_LPC_ADDR_HOST_PARAM   0x804
#define EC_MEMMAP_SIZE 0xFF
#define EECRESULT 1000

#include <pshpack1.h>
/*
 * @version: Command version number (often 0)
 * @command: Command to send (EC_CMD_...)
 * @outsize: Outgoing length in bytes
 * @insize: Max number of bytes to accept from EC
 * @result: EC's response to the command (separate from communication failure)
 * @data: Where to put the incoming data from EC and outgoing data to EC
 */
struct cros_ec_command_v2 {
	ULONG version;
	ULONG command;
	ULONG outsize;
	ULONG insize;
	ULONG result;
#pragma warning(suppress: 4200)
	UCHAR data[0];
};

/*
 * @offset: within EC_LPC_ADDR_MEMMAP region
 * @bytes: number of bytes to read. zero means "read a string" (including '\0')
 *         (at most only EC_MEMMAP_SIZE bytes can be read)
 * @buffer: where to store the result
 * ioctl returns the number of bytes read, negative on error
 */
struct cros_ec_readmem_v2 {
	ULONG offset;
	ULONG bytes;
	UCHAR buffer[EC_MEMMAP_SIZE];
};

struct ec_host_request {
	UCHAR struct_version;
	UCHAR checksum;
	USHORT command;
	UCHAR command_version;
	UCHAR reserved;
	USHORT data_len;
};

struct ec_host_response {
	UCHAR struct_version;
	UCHAR checksum;
	USHORT result;
	USHORT data_len;
	USHORT reserved;
} __ec_align4;

enum ec_status {
	EC_RES_SUCCESS = 0,
	EC_RES_INVALID_COMMAND = 1,
	EC_RES_ERROR = 2,
	EC_RES_INVALID_PARAM = 3,
	EC_RES_ACCESS_DENIED = 4,
	EC_RES_INVALID_RESPONSE = 5,
	EC_RES_INVALID_VERSION = 6,
	EC_RES_INVALID_CHECKSUM = 7,
	EC_RES_IN_PROGRESS = 8,		/* Accepted, command in progress */
	EC_RES_UNAVAILABLE = 9,		/* No response available */
	EC_RES_TIMEOUT = 10,		/* We got a timeout */
	EC_RES_OVERFLOW = 11,		/* Table / data overflow */
	EC_RES_INVALID_HEADER = 12,     /* Header contains invalid data */
	EC_RES_REQUEST_TRUNCATED = 13,  /* Didn't get the entire request */
	EC_RES_RESPONSE_TOO_BIG = 14,   /* Response was too big to handle */
	EC_RES_BUS_ERROR = 15,		/* Communications bus error */
	EC_RES_BUSY = 16,		/* Up but too busy.  Should retry */
	EC_RES_INVALID_HEADER_VERSION = 17,  /* Header version invalid */
	EC_RES_INVALID_HEADER_CRC = 18,      /* Header CRC invalid */
	EC_RES_INVALID_DATA_CRC = 19,        /* Data CRC invalid */
	EC_RES_DUP_UNAVAILABLE = 20,         /* Can't resend response */
} __packed;
#include <poppack.h>

int ECReadMemoryLPC(WDFDEVICE originatingDevice, int offset, void* buffer, int length);
int ECSendCommandLPCv3(WDFDEVICE originatingDevice,  int command, int version, const void* outdata,
	int outsize, void* indata, int insize);
