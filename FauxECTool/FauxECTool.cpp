// FauxECTool.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <windows.h>
#include <ioapiset.h>
#include <wil/resource.h>
#include <wil/result_macros.h>
#include <iostream>
#include "../CrosEC/Public.h"

static wil::unique_hfile device;

static void hexdump(const uint8_t* data, int len) {
	int i, j;

	if(!data || !len)
		return;

	for(i = 0; i < len; i += 16) {
		/* Left column (Hex) */
		for(j = i; j < i + 16; j++) {
			if(j < len)
				fprintf(stderr, " %02x", data[j]);
			else
				fprintf(stderr, "   ");
		}
		/* Right column (ASCII) */
		fprintf(stderr, " |");
		for(j = i; j < i + 16; j++) {
			uint8_t c = j < len ? data[j] : ' ';
			fprintf(stderr, "%c", isprint(c) ? c : '.');
		}
		fprintf(stderr, "|\n");
	}
}

static int cmd_raw(int argc, char** argv) {
	char* e;
	char* wrbuf = NULL;
	int wrsz = 0, rdsz = 0;
	uint16_t command = 0, version = 0;
	int rv = 1;
	DWORD ioctl_ret = 0;

	size_t cmdsz = sizeof(CROSEC_COMMAND) + CROSEC_CMD_MAX_REQUEST;
	PCROSEC_COMMAND cmd = (PCROSEC_COMMAND)calloc(1, cmdsz);

	if(argc < 2) {
		fprintf(stderr, "Usage: %s [command] [writes]\r\n", argv[0]);
		goto out;
	}

	command = (uint16_t)strtoul(argv[1], &e, 0);
	if(e && *e && *e == '/') {
		version = (uint16_t)strtoul(e + 1, &e, 0);
	}
	if(e && *e) {
		fprintf(stderr, "Invalid command \"%s\"\r\n", argv[1]);
		goto out;
	}

	if(argc > 2) {
		char t;
		unsigned long long v;
		char* bptr;
		wrbuf = CROSEC_COMMAND_DATA(cmd);
		bptr = wrbuf;
		e = argv[2];
		while(*e) {
			t = *e++;
			v = strtoull(e, &e, 16);
			switch(t) {
				case 'b': {
					uint8_t nv = (uint8_t)v;
					memcpy(bptr, &nv, sizeof(nv));
					bptr += sizeof(nv);
					break;
				}
				case 'w': {
					uint16_t nv = (uint16_t)v;
					memcpy(bptr, &nv, sizeof(nv));
					bptr += sizeof(nv);
					break;
				}
				case 'd': {
					uint32_t nv = (uint32_t)v;
					memcpy(bptr, &nv, sizeof(nv));
					bptr += sizeof(nv);
					break;
				}
				case 'q': {
					uint64_t nv = (uint64_t)v;
					memcpy(bptr, &nv, sizeof(nv));
					bptr += sizeof(nv);
					break;
				}
				default:
					fprintf(stderr, "Invalid typecode '%c' at position %ld.\r\n", t,
					        (long)(e - argv[2]));
					goto out;
			}
			if(*e == ',')
				++e;
		}
		wrsz = (int)(bptr - wrbuf);
	}

	rdsz = 256;

	fprintf(stderr, "%4.04x(...%u bytes...)\r\n", command, wrsz);
	hexdump((uint8_t*)wrbuf, wrsz);

	cmd->command = command;
	cmd->insize = rdsz;
	cmd->outsize = wrsz;
	cmd->result = 0xff;
	cmd->version = version;
	THROW_LAST_ERROR_IF(!DeviceIoControl(device.get(), IOCTL_CROSEC_XCMD, cmd, (DWORD)cmdsz, cmd, (DWORD)cmdsz,
	                                     &ioctl_ret, nullptr));
	if(cmd->result != 0) {
		fprintf(stderr, "EC Error\r\n");
		goto out;
	}
	rdsz = ioctl_ret - sizeof(CROSEC_COMMAND);

	fprintf(stderr, "Read %u bytes\r\n", rdsz);
	hexdump((uint8_t*)CROSEC_COMMAND_DATA(cmd), rdsz);
	rv = 0;

out:
	free(cmd);
	return !!rv;
}

int main(int argc, char** argv) {
	try {
		device.reset(CreateFileW(LR"(\\.\GLOBALROOT\Device\CrosEC)", GENERIC_READ | GENERIC_WRITE,
		                         FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr));
		THROW_LAST_ERROR_IF(!device);

		if(argc > 1) {
			return cmd_raw(argc, argv);
		}

		// Otherwise, run test mode
		size_t size = sizeof(CROSEC_COMMAND) + CROSEC_CMD_MAX_REQUEST;
		PCROSEC_COMMAND cmd = (PCROSEC_COMMAND)calloc(1, size);
		cmd->command = 0x4;
		cmd->insize = 255;
		cmd->outsize = 0;
		cmd->result = 0xff;
		cmd->version = 0;

		std::cout << "OUT: " << cmd->command << "|" << cmd->result << std::endl;
		DWORD retb{};
		THROW_LAST_ERROR_IF(!DeviceIoControl(device.get(), IOCTL_CROSEC_XCMD, cmd, (DWORD)size, cmd,
		                                     (DWORD)size, &retb, nullptr));
		std::cout << "IN_: " << cmd->command << "|" << cmd->result << std::endl;
		std::cout << "IN_: " << (char*)(CROSEC_COMMAND_DATA(cmd)) << std::endl;

		CROSEC_READMEM rm{};
		rm.bytes = 0xfe;
		rm.offset = 0;
		// DWORD retb{};
		THROW_LAST_ERROR_IF(!DeviceIoControl(device.get(), IOCTL_CROSEC_RDMEM, &rm, sizeof(rm), &rm, sizeof(rm),
		                                     &retb, nullptr));
		hexdump(rm.buffer, rm.bytes);

		rm.bytes = 0x0;
		rm.offset = 0x6f;
		// DWORD retb{};
		THROW_LAST_ERROR_IF(!DeviceIoControl(device.get(), IOCTL_CROSEC_RDMEM, &rm, sizeof(rm), &rm, sizeof(rm),
		                                     &retb, nullptr));
		hexdump(rm.buffer, rm.bytes);
	} catch(std::runtime_error e) {
		std::cerr << e.what() << std::endl;
	} catch(...) {
		LOG_CAUGHT_EXCEPTION();
	}
}
