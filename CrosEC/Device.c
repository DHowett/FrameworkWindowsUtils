#include "driver.h"
#include "device.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, CrosECCreateDevice)
#pragma alloc_text(PAGE, CrosECEvtDeviceContextCleanup)
#endif

NTSTATUS CrosECCreateDevice(_Inout_ PWDFDEVICE_INIT DeviceInit) {
	WDF_OBJECT_ATTRIBUTES deviceAttributes;
	PDEVICE_CONTEXT deviceContext;
	WDFDEVICE device;

	PAGED_CODE();

	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);
	deviceAttributes.EvtCleanupCallback = CrosECEvtDeviceContextCleanup;

	WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_CONTROLLER);
	DECLARE_CONST_UNICODE_STRING(Name, L"\\Device\\CrosEC");
	WdfDeviceInitAssignName(DeviceInit, &Name);

	NT_RETURN_IF_NTSTATUS_FAILED(WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device));
	deviceContext = DeviceGetContext(device);

	deviceContext->inflightCommand = ExAllocatePool2(POOL_FLAG_NON_PAGED, CROSEC_CMD_MAX, CROS_EC_POOL_TAG);
	KeInitializeTimer(&deviceContext->waitTimer);
	KeInitializeGuardedMutex(&deviceContext->mutex);

	NT_RETURN_IF_NTSTATUS_FAILED(ECProbe(device));

	NT_RETURN_IF_NTSTATUS_FAILED(
		WdfDeviceCreateDeviceInterface(device, &GUID_DEVINTERFACE_CrosEC, NULL /* ReferenceString */));

	NT_RETURN_IF_NTSTATUS_FAILED(CrosECQueueInitialize(device));

	return STATUS_SUCCESS;
}

NTSTATUS CrosECEvtDeviceContextCleanup(_In_ WDFOBJECT object) {
	PAGED_CODE();

	PDEVICE_CONTEXT deviceContext = DeviceGetContext(object);
	ExFreePoolWithTag(deviceContext->inflightCommand, CROS_EC_POOL_TAG);
	deviceContext->inflightCommand = NULL;
	return STATUS_SUCCESS;
}
