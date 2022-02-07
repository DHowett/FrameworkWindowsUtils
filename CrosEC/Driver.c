#include "driver.h"
#include "driver.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, CrosECEvtDeviceAdd)
#pragma alloc_text(PAGE, CrosECEvtDriverContextCleanup)
#endif

NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath) {
	WDF_DRIVER_CONFIG config;
	NTSTATUS status;
	WDF_OBJECT_ATTRIBUTES attributes;

	WPP_INIT_TRACING(DriverObject, RegistryPath);

	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	attributes.EvtCleanupCallback = CrosECEvtDriverContextCleanup;

	WDF_DRIVER_CONFIG_INIT(&config, CrosECEvtDeviceAdd);

	status = WdfDriverCreate(DriverObject, RegistryPath, &attributes, &config, WDF_NO_HANDLE);

	if(!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfDriverCreate failed %!STATUS!", status);
		WPP_CLEANUP(DriverObject);
		return status;
	}

	return status;
}

NTSTATUS CrosECEvtDeviceAdd(_In_ WDFDRIVER Driver, _Inout_ PWDFDEVICE_INIT DeviceInit) {
	NTSTATUS status;

	UNREFERENCED_PARAMETER(Driver);

	PAGED_CODE();

	status = CrosECCreateDevice(DeviceInit);

	return status;
}

VOID CrosECEvtDriverContextCleanup(_In_ WDFOBJECT DriverObject) {
	UNREFERENCED_PARAMETER(DriverObject);

	PAGED_CODE();

	WPP_CLEANUP(WdfDriverWdmGetDriverObject((WDFDRIVER)DriverObject));
}
