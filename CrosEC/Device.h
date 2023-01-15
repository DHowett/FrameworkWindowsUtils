#pragma once

#include "public.h"
#include <acpiioct.h>
#include <acpitabl.h>

EXTERN_C_START

typedef enum _CROSEC_DEVICE_FLAGS {
	DEVICE_FLAG_USE_ACPI_LOCKING = 1 << 0,
} CROSEC_DEVICE_FLAGS;

typedef struct _DEVICE_CONTEXT {
	PCROSEC_COMMAND inflightCommand;
	KTIMER waitTimer;
	KGUARDED_MUTEX mutex;
	CROSEC_DEVICE_FLAGS flags;
	ACPI_INTERFACE_STANDARD2 acpiInterface;
} DEVICE_CONTEXT, *PDEVICE_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(DEVICE_CONTEXT, DeviceGetContext)

NTSTATUS CrosECCreateDevice(_Inout_ PWDFDEVICE_INIT DeviceInit);
NTSTATUS CrosECEvtDeviceContextCleanup(_In_ WDFOBJECT object);

NTSTATUS CrosECDeviceAcquireLock(_In_ WDFOBJECT device);
NTSTATUS CrosECDeviceReleaseLock(_In_ WDFOBJECT device);
_IRQL_requires_max_(PASSIVE_LEVEL) NTSTATUS Acpi_EvaluateDsmWithReadmem(_In_ WDFDEVICE device,
                                                                        size_t offset,
                                                                        size_t size,
                                                                        _Outptr_opt_ PACPI_EVAL_OUTPUT_BUFFER* Output);
_IRQL_requires_max_(PASSIVE_LEVEL) NTSTATUS Acpi_EvaluateDsmWithPayload(_In_ WDFDEVICE device,
                                                                        const char* buffer,
                                                                        size_t size,
                                                                        _Outptr_opt_ PACPI_EVAL_OUTPUT_BUFFER* Output);

#define CROS_EC_POOL_TAG 'CRos'

EXTERN_C_END
