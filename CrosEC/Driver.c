#include "driver.h"
#include "driver.tmh"

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, CrosECEvtDeviceAdd)
#pragma alloc_text (PAGE, CrosECEvtDriverContextCleanup)
#pragma alloc_text (PAGE, CrosECEvtDriverUnload)
#endif

NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath) {
	WDF_DRIVER_CONFIG config;
	NTSTATUS status;
	WDF_OBJECT_ATTRIBUTES attributes;

	WPP_INIT_TRACING(DriverObject, RegistryPath);

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	attributes.EvtCleanupCallback = CrosECEvtDriverContextCleanup;

	WDF_DRIVER_CONFIG_INIT(&config,
		NULL
	);
	config.DriverInitFlags = WdfDriverInitNonPnpDriver;
	config.EvtDriverUnload = CrosECEvtDriverUnload;

	WDFDRIVER hDriver;
	status = WdfDriverCreate(DriverObject,
		RegistryPath,
		&attributes,
		&config,
		&hDriver
	);

	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfDriverCreate failed %!STATUS!", status);
		WPP_CLEANUP(DriverObject);
		return status;
	}

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

#if 1 // CONTROL DEVICE?
	PWDFDEVICE_INIT CtlDeviceInit;
	CtlDeviceInit = WdfControlDeviceInitAllocate(hDriver, &SDDL_DEVOBJ_SYS_ALL_ADM_ALL);
	if (FAILED_NTSTATUS(status = CrosECCreateDevice(CtlDeviceInit))) {
		WdfDeviceInitFree(CtlDeviceInit);
	}
#endif

	return status;
}

NTSTATUS CrosECEvtDeviceAdd(_In_ WDFDRIVER Driver, _Inout_ PWDFDEVICE_INIT DeviceInit) {
	NTSTATUS status;

	UNREFERENCED_PARAMETER(Driver);

	PAGED_CODE();

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

	status = CrosECCreateDevice(DeviceInit);

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit");

	return status;
}

VOID CrosECEvtDriverContextCleanup(_In_ WDFOBJECT DriverObject) {
	UNREFERENCED_PARAMETER(DriverObject);

	PAGED_CODE();

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

	WPP_CLEANUP(WdfDriverWdmGetDriverObject((WDFDRIVER)DriverObject));
}

VOID CrosECEvtDriverUnload(_In_ WDFDRIVER Driver) {
	UNREFERENCED_PARAMETER(Driver);
	PAGED_CODE();
}
