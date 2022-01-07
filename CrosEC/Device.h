/*++

Module Name:

	device.h

Abstract:

	This file contains the device definitions.

Environment:

	Kernel-mode Driver Framework

--*/

#include "public.h"
#include "EC.h"

EXTERN_C_START

//
// The device context performs the same job as
// a WDM device extension in the driver frameworks
//
typedef struct _DEVICE_CONTEXT
{
	struct cros_ec_command_v2* inflightCommand;
	KTIMER waitTimer;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

//
// This macro will generate an inline function called DeviceGetContext
// which will be used to get a pointer to the device context memory
// in a type safe manner.
//
WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceGetContext)

//
// Function to initialize the device and its callbacks
//
NTSTATUS
CrosECCreateDevice(
	_Inout_ PWDFDEVICE_INIT DeviceInit
);

NTSTATUS CrosECEvtDeviceContextCleanup(_In_ WDFOBJECT object);

#define CROS_EC_POOL_TAG 'CRos'

EXTERN_C_END
