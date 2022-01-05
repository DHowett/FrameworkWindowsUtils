/*++

Module Name:

	public.h

Abstract:

	This module contains the common declarations shared by driver
	and user applications.

Environment:

	user and kernel

--*/

//
// Define an Interface Guid so that apps can find the device and talk to it.
//

DEFINE_GUID(GUID_DEVINTERFACE_CrosEC,
	0xd66bb4f8, 0x0a7a, 0x4f89, 0x90, 0x33, 0x79, 0x8a, 0xff, 0xa4, 0xf5, 0x38);
// {d66bb4f8-0a7a-4f89-9033-798affa4f538}
