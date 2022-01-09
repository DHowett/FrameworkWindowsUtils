#pragma once

DEFINE_GUID(GUID_DEVINTERFACE_CrosEC,
	0xd66bb4f8, 0x0a7a, 0x4f89, 0x90, 0x33, 0x79, 0x8a, 0xff, 0xa4, 0xf5, 0x38);
// {d66bb4f8-0a7a-4f89-9033-798affa4f538}

#define FILE_DEVICE_CROS_EMBEDDED_CONTROLLER 0x80EC

#define IOCTL_CROSEC_XCMD  CTL_CODE(FILE_DEVICE_CROS_EMBEDDED_CONTROLLER, 0x801, METHOD_BUFFERED, FILE_READ_DATA | FILE_WRITE_DATA)
#define IOCTL_CROSEC_RDMEM CTL_CODE(FILE_DEVICE_CROS_EMBEDDED_CONTROLLER, 0x802, METHOD_BUFFERED, FILE_READ_DATA)

#define CROSEC_CMD_MAX_REQUEST  0x100
#define CROSEC_CMD_MAX_RESPONSE 0x100
#define CROSEC_MEMMAP_SIZE      0xFF

#define CROSEC_STATUS_IN_PROGRESS ((NTSTATUS)0xE0EC0001) // EC Command in progress
#define CROSEC_STATUS_UNAVAILABLE ((NTSTATUS)0xE0EC0002) // EC not available

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
