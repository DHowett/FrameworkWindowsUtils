#include "driver.h"
#include "device.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, CrosECCreateDevice)
#pragma alloc_text (PAGE, CrosECEvtDeviceContextCleanup)
#endif

NTSTATUS CrosECCreateDevice(_Inout_ PWDFDEVICE_INIT DeviceInit) {
	WDF_OBJECT_ATTRIBUTES deviceAttributes;
	PDEVICE_CONTEXT deviceContext;
	WDFDEVICE device;
	NTSTATUS status;

	PAGED_CODE();

	WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&deviceAttributes, DEVICE_CONTEXT);
	deviceAttributes.EvtCleanupCallback = CrosECEvtDeviceContextCleanup;

	WdfDeviceInitSetDeviceType(DeviceInit, FILE_DEVICE_CONTROLLER);
	DECLARE_CONST_UNICODE_STRING(Name, L"\\Device\\CrosEC");
	WdfDeviceInitAssignName(DeviceInit, &Name);

	status = WdfDeviceCreate(&DeviceInit, &deviceAttributes, &device);

	if (NT_SUCCESS(status)) {
		deviceContext = DeviceGetContext(device);

		deviceContext->inflightCommand = ExAllocatePoolWithTag(NonPagedPool, CROSEC_CMD_MAX, CROS_EC_POOL_TAG);
		KeInitializeTimer(&deviceContext->waitTimer);

		status = WdfDeviceCreateDeviceInterface(
			device,
			&GUID_DEVINTERFACE_CrosEC,
			NULL // ReferenceString
		);

		if (NT_SUCCESS(status)) {
			status = CrosECQueueInitialize(device);
		}
	}

	return status;
}

NTSTATUS CrosECEvtDeviceContextCleanup(_In_ WDFOBJECT object) {
	PAGED_CODE();

	PDEVICE_CONTEXT deviceContext = DeviceGetContext(object);
	ExFreePoolWithTag(deviceContext->inflightCommand, CROS_EC_POOL_TAG);
	deviceContext->inflightCommand = NULL;
	return STATUS_SUCCESS;
}
