#pragma once

#include "public.h"

EXTERN_C_START

typedef struct _DEVICE_CONTEXT {
	PCROSEC_COMMAND inflightCommand;
	KTIMER waitTimer;
	KGUARDED_MUTEX mutex;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceGetContext)

NTSTATUS CrosECCreateDevice(_Inout_ PWDFDEVICE_INIT DeviceInit);
NTSTATUS CrosECEvtDeviceContextCleanup(_In_ WDFOBJECT object);

#define CROS_EC_POOL_TAG 'CRos'

EXTERN_C_END
