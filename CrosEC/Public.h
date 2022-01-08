/*++

Module Name:

	public.h

Abstract:

	This module contains the common declarations shared by driver
	and user applications.

Environment:

	user and kernel

--*/

#pragma once

//
// Define an Interface Guid so that apps can find the device and talk to it.
//

DEFINE_GUID(GUID_DEVINTERFACE_CrosEC,
	0xd66bb4f8, 0x0a7a, 0x4f89, 0x90, 0x33, 0x79, 0x8a, 0xff, 0xa4, 0xf5, 0x38);
// {d66bb4f8-0a7a-4f89-9033-798affa4f538}

#define FILE_DEVICE_CROS_EMBEDDED_CONTROLLER 0x80EC

#define IOCTL_CROSEC_XCMD  CTL_CODE(FILE_DEVICE_CROS_EMBEDDED_CONTROLLER, 0x801, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_CROSEC_RDMEM CTL_CODE(FILE_DEVICE_CROS_EMBEDDED_CONTROLLER, 0x802, METHOD_BUFFERED, FILE_READ_DATA)

#define CROSEC_CMD_MAX_REQUEST  0x100
#define CROSEC_CMD_MAX_RESPONSE 0x100
#define CROSEC_MEMMAP_SIZE      0xFF
#define CROSEC_EECRESULT        1000

/*
 * @version: Command version number (often 0)
 * @command: Command to send (EC_CMD_...)
 * @outsize: Outgoing length in bytes
 * @insize: Max number of bytes to accept from EC
 * @result: EC's response to the command (separate from communication failure)
 * @data: Where to put the incoming data from EC and outgoing data to EC
 */
typedef struct _CROSEC_COMMAND {
	ULONG version;
	ULONG command;
	ULONG outsize;
	ULONG insize;
	ULONG result;
#pragma warning(suppress: 4200)
} *PCROSEC_COMMAND, CROSEC_COMMAND;

#define CROSEC_COMMAND_DATA(c) (((char*)c)+sizeof(struct _CROSEC_COMMAND))

/*
 * @offset: within EC_LPC_ADDR_MEMMAP region
 * @bytes: number of bytes to read. zero means "read a string" (including '\0')
 *         (at most only EC_MEMMAP_SIZE bytes can be read)
 * @buffer: where to store the result
 * ioctl returns the number of bytes read, negative on error
 */
typedef struct _CROSEC_READMEM {
	ULONG offset;
	ULONG bytes;
	UCHAR buffer[CROSEC_MEMMAP_SIZE];
} *PCROSEC_READMEM, CROSEC_READMEM;

typedef enum _CROSEC_STATUS {
	CROSEC_RES_SUCCESS = 0,
	CROSEC_RES_INVALID_COMMAND = 1,
	CROSEC_RES_ERROR = 2,
	CROSEC_RES_INVALID_PARAM = 3,
	CROSEC_RES_ACCESS_DENIED = 4,
	CROSEC_RES_INVALID_RESPONSE = 5,
	CROSEC_RES_INVALID_VERSION = 6,
	CROSEC_RES_INVALID_CHECKSUM = 7,
	CROSEC_RES_IN_PROGRESS = 8,		/* Accepted, command in progress */
	CROSEC_RES_UNAVAILABLE = 9,		/* No response available */
	CROSEC_RES_TIMEOUT = 10,		/* We got a timeout */
	CROSEC_RES_OVERFLOW = 11,		/* Table / data overflow */
	CROSEC_RES_INVALID_HEADER = 12,     /* Header contains invalid data */
	CROSEC_RES_REQUEST_TRUNCATED = 13,  /* Didn't get the entire request */
	CROSEC_RES_RESPONSE_TOO_BIG = 14,   /* Response was too big to handle */
	CROSEC_RES_BUS_ERROR = 15,		/* Communications bus error */
	CROSEC_RES_BUSY = 16,		/* Up but too busy.  Should retry */
	CROSEC_RES_INVALID_HEADER_VERSION = 17,  /* Header version invalid */
	CROSEC_RES_INVALID_HEADER_CRC = 18,      /* Header CRC invalid */
	CROSEC_RES_INVALID_DATA_CRC = 19,        /* Data CRC invalid */
	CROSEC_RES_DUP_UNAVAILABLE = 20,         /* Can't resend response */
} CROSEC_STATUS;
