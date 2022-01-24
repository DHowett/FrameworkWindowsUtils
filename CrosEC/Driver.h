#pragma once

#include <ntddk.h>
#include <wdf.h>
#include <initguid.h>

#include "device.h"
#include "queue.h"
#include "trace.h"

#define FAILED_NTSTATUS(status) (((NTSTATUS)(status)) < 0)
#define NT_RETURN_IF_NTSTATUS_FAILED(status)           \
	do {                                           \
		const NTSTATUS __statusRet = (status); \
		if(FAILED_NTSTATUS(__statusRet)) {     \
			return __statusRet;            \
		}                                      \
	} while((void)0, 0)
#define NT_RETURN_IF(status, condition)                \
	do {                                           \
		const NTSTATUS __statusRet = (status); \
		if((condition)) {                      \
			return __statusRet;            \
		}                                      \
	} while((void)0, 0)

#define CROSEC_CMD_MAX (sizeof(CROSEC_COMMAND) + CROSEC_CMD_MAX_REQUEST)

EXTERN_C_START

// Events
DRIVER_INITIALIZE DriverEntry;
EVT_WDF_DRIVER_DEVICE_ADD CrosECEvtDeviceAdd;
EVT_WDF_OBJECT_CONTEXT_CLEANUP CrosECEvtDriverContextCleanup;

EXTERN_C_END
