// FauxECTool.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <windows.h>
#include <ioapiset.h>
#include <wil/resource.h>
#include <wil/result_macros.h>
#include <iostream>
#include "../CrosEC/Public.h"

void hexdump(const uint8_t* data, int len) {
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
			int c = j < len ? data[j] : ' ';
			fprintf(stderr, "%c", isprint(c) ? c : '.');
		}
		fprintf(stderr, "|\n");
	}
}

int main() {
	try {
		wil::unique_hfile device;
		device.reset(CreateFileW(LR"(\\.\GLOBALROOT\Device\CrosEC)", GENERIC_READ | GENERIC_WRITE,
		                         FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr));
		THROW_LAST_ERROR_IF(!device);

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
	} catch(std::runtime_error e) {
		std::cerr << e.what() << std::endl;
	} catch(...) {
		LOG_CAUGHT_EXCEPTION();
	}
}
