// FauxECTool.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <windows.h>
#include <ioapiset.h>
#include <wil/resource.h>
#include <wil/result_macros.h>
#include "../CrosEC/Public.h"
#include "../CrosEC/EC.h"

int main()
{
    try {
        wil::unique_hfile device;
        device.reset(CreateFileW(LR"(\\.\GLOBALROOT\Device\CrosEC)", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr));
        THROW_LAST_ERROR_IF(!device);

        cros_ec_command_v2 *cmd = (cros_ec_command_v2*)malloc(sizeof(cros_ec_command_v2) + 255);
        cmd->command = 0x11;
        cmd->insize = 0;
        cmd->outsize = 255;
        cmd->result = 0xff;
        cmd->version = 0;

        DWORD retb{};
        THROW_LAST_ERROR_IF(!DeviceIoControl(device.get(), IOCTL_CROSEC_XCMD, cmd, sizeof(cros_ec_command_v2) + 255, cmd, sizeof(cros_ec_command_v2) + 255, &retb, nullptr));
        std::cout << "Hello World!\n";
        std::cout << cmd->command << "|" << cmd->result << std::endl;
    }
    catch (std::runtime_error e) {
        std::cerr << e.what() << std::endl;
    }
    catch (...) {
        LOG_CAUGHT_EXCEPTION();
    }
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
