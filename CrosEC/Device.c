#include "driver.h"
#include "device.tmh"

NTSTATUS Acpi_EvaluateDsm(_In_ WDFDEVICE device,
                          _In_ ULONG FunctionIndex,
                          _Outptr_opt_ PACPI_EVAL_OUTPUT_BUFFER* Output);
NTSTATUS Acpi_EvaluateDsmWithPayload(_In_ WDFDEVICE device,
                                     const char* buffer,
                                     size_t size,
                                     _Outptr_opt_ PACPI_EVAL_OUTPUT_BUFFER* Output);
#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, CrosECCreateDevice)
#pragma alloc_text(PAGE, CrosECEvtDeviceContextCleanup)
#pragma alloc_text(PAGE, CrosECDeviceAcquireLock)
#pragma alloc_text(PAGE, CrosECDeviceReleaseLock)
#pragma alloc_text(PAGE, Acpi_EvaluateDsm)
#pragma alloc_text(PAGE, Acpi_EvaluateDsmWithPayload)
#pragma alloc_text(PAGE, Acpi_EvaluateDsmWithReadmem)
#endif

DEFINE_GUID(GUID_ACPI_ECPR_DSM, 0x8829106f, 0x5320, 0x44e4, 0x8f, 0x20, 0xfa, 0x67, 0x32, 0x6a, 0x71, 0xeb);

_IRQL_requires_max_(PASSIVE_LEVEL) NTSTATUS Acpi_EvaluateDsmWithPayload(_In_ WDFDEVICE device,
                                                                        const char* buffer,
                                                                        size_t size,
                                                                        _Outptr_opt_ PACPI_EVAL_OUTPUT_BUFFER* Output) {
	NTSTATUS status;
	WDFMEMORY inputMemory;
	WDF_MEMORY_DESCRIPTOR inputMemDesc;
	PACPI_EVAL_INPUT_BUFFER_COMPLEX inputBuffer;
	size_t inputBufferSize;
	size_t inputArgumentBufferSize;
	PACPI_METHOD_ARGUMENT argument;
	WDF_MEMORY_DESCRIPTOR outputMemDesc;
	PACPI_EVAL_OUTPUT_BUFFER outputBuffer;
	size_t outputBufferSize;
	size_t outputArgumentBufferSize;
	WDF_OBJECT_ATTRIBUTES attributes;
	WDF_REQUEST_SEND_OPTIONS sendOptions;

	PAGED_CODE();

	inputMemory = WDF_NO_HANDLE;
	outputBuffer = NULL;

	inputArgumentBufferSize = ACPI_METHOD_ARGUMENT_LENGTH(sizeof(GUID)) +
	                          ACPI_METHOD_ARGUMENT_LENGTH(sizeof(ULONG)) +
	                          ACPI_METHOD_ARGUMENT_LENGTH(sizeof(ULONG)) +
	                          FIELD_OFFSET(ACPI_METHOD_ARGUMENT, Argument) + ACPI_METHOD_ARGUMENT_LENGTH(size);

	inputBufferSize = FIELD_OFFSET(ACPI_EVAL_INPUT_BUFFER_COMPLEX, Argument) + inputArgumentBufferSize;

	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	attributes.ParentObject = device;

	status = WdfMemoryCreate(&attributes, NonPagedPoolNx, 0, inputBufferSize, &inputMemory, (PVOID*)&inputBuffer);

	if(!NT_SUCCESS(status)) {
		goto Exit;
	}

	RtlZeroMemory(inputBuffer, inputBufferSize);

	inputBuffer->Signature = ACPI_EVAL_INPUT_BUFFER_COMPLEX_SIGNATURE;
	inputBuffer->Size = (ULONG)inputArgumentBufferSize;
	inputBuffer->ArgumentCount = 4;
	inputBuffer->MethodNameAsUlong = (ULONG)'MSD_';

	argument = &(inputBuffer->Argument[0]);
	ACPI_METHOD_SET_ARGUMENT_BUFFER(argument, &GUID_ACPI_ECPR_DSM, sizeof(GUID_ACPI_ECPR_DSM));

	argument = ACPI_METHOD_NEXT_ARGUMENT(argument);
	ACPI_METHOD_SET_ARGUMENT_INTEGER(argument, 0);

	argument = ACPI_METHOD_NEXT_ARGUMENT(argument);
	ACPI_METHOD_SET_ARGUMENT_INTEGER(argument, 1);

	argument = ACPI_METHOD_NEXT_ARGUMENT(argument);
	argument->DataLength = ACPI_METHOD_ARGUMENT_LENGTH((USHORT)size);
	argument->Type = ACPI_METHOD_ARGUMENT_PACKAGE_EX;
	argument = (PACPI_METHOD_ARGUMENT)argument->Data;
	ACPI_METHOD_SET_ARGUMENT_BUFFER(argument, buffer, (USHORT)size)

	outputArgumentBufferSize = ACPI_METHOD_ARGUMENT_LENGTH(256);
	outputBufferSize = FIELD_OFFSET(ACPI_EVAL_OUTPUT_BUFFER, Argument) + outputArgumentBufferSize;

	outputBuffer =
		(PACPI_EVAL_OUTPUT_BUFFER)ExAllocatePool2(POOL_FLAG_NON_PAGED, outputBufferSize, CROS_EC_POOL_TAG);

	if(outputBuffer == NULL) {
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto Exit;
	}

	WDF_MEMORY_DESCRIPTOR_INIT_HANDLE(&inputMemDesc, inputMemory, NULL);
	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputMemDesc, outputBuffer, (ULONG)outputBufferSize);

	WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions, WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);
	WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&sendOptions, WDF_REL_TIMEOUT_IN_MS(1000));

	status = WdfIoTargetSendInternalIoctlSynchronously(WdfDeviceGetIoTarget(device), NULL, IOCTL_ACPI_EVAL_METHOD,
	                                                   &inputMemDesc, &outputMemDesc, &sendOptions, NULL);

	if(!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		            "[Device: 0x%p] IOCTL_ACPI_EVAL_METHOD for _DSM failed - %!STATUS!", device, status);
		goto Exit;
	}

	if(outputBuffer->Signature != ACPI_EVAL_OUTPUT_BUFFER_SIGNATURE) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		            "[Device: 0x%p] ACPI_EVAL_OUTPUT_BUFFER signature is incorrect", device);
		status = STATUS_ACPI_INVALID_DATA;
		goto Exit;
	}

Exit:

	if(inputMemory != WDF_NO_HANDLE) {
		WdfObjectDelete(inputMemory);
	}

	if(!NT_SUCCESS(status) || (Output == NULL)) {
		if(outputBuffer) {
			ExFreePoolWithTag(outputBuffer, CROS_EC_POOL_TAG);
		}
	} else {
		*Output = outputBuffer;
	}

	return status;
}

_IRQL_requires_max_(PASSIVE_LEVEL) NTSTATUS Acpi_EvaluateDsmWithReadmem(_In_ WDFDEVICE device,
                                                                        size_t offset,
                                                                        size_t size,
                                                                        _Outptr_opt_ PACPI_EVAL_OUTPUT_BUFFER* Output) {
	NTSTATUS status;
	WDFMEMORY inputMemory;
	WDF_MEMORY_DESCRIPTOR inputMemDesc;
	PACPI_EVAL_INPUT_BUFFER_COMPLEX inputBuffer;
	size_t inputBufferSize;
	size_t inputArgumentBufferSize;
	PACPI_METHOD_ARGUMENT argument;
	WDF_MEMORY_DESCRIPTOR outputMemDesc;
	PACPI_EVAL_OUTPUT_BUFFER outputBuffer;
	size_t outputBufferSize;
	size_t outputArgumentBufferSize;
	WDF_OBJECT_ATTRIBUTES attributes;
	WDF_REQUEST_SEND_OPTIONS sendOptions;

	PAGED_CODE();

	inputMemory = WDF_NO_HANDLE;
	outputBuffer = NULL;

	inputArgumentBufferSize = ACPI_METHOD_ARGUMENT_LENGTH(sizeof(GUID)) +
	                          ACPI_METHOD_ARGUMENT_LENGTH(sizeof(ULONG)) +
	                          ACPI_METHOD_ARGUMENT_LENGTH(sizeof(ULONG)) +
	                          FIELD_OFFSET(ACPI_METHOD_ARGUMENT, Argument) + (ACPI_METHOD_ARGUMENT_LENGTH(sizeof(ULONG)) * 2);

	inputBufferSize = FIELD_OFFSET(ACPI_EVAL_INPUT_BUFFER_COMPLEX, Argument) + inputArgumentBufferSize;

	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	attributes.ParentObject = device;

	status = WdfMemoryCreate(&attributes, NonPagedPoolNx, 0, inputBufferSize, &inputMemory, (PVOID*)&inputBuffer);

	if(!NT_SUCCESS(status)) {
		goto Exit;
	}

	RtlZeroMemory(inputBuffer, inputBufferSize);

	inputBuffer->Signature = ACPI_EVAL_INPUT_BUFFER_COMPLEX_SIGNATURE;
	inputBuffer->Size = (ULONG)inputArgumentBufferSize;
	inputBuffer->ArgumentCount = 4;
	inputBuffer->MethodNameAsUlong = (ULONG)'MSD_';

	argument = &(inputBuffer->Argument[0]);
	ACPI_METHOD_SET_ARGUMENT_BUFFER(argument, &GUID_ACPI_ECPR_DSM, sizeof(GUID_ACPI_ECPR_DSM));

	argument = ACPI_METHOD_NEXT_ARGUMENT(argument);
	ACPI_METHOD_SET_ARGUMENT_INTEGER(argument, 0);

	argument = ACPI_METHOD_NEXT_ARGUMENT(argument);
	ACPI_METHOD_SET_ARGUMENT_INTEGER(argument, 2); // FunctionIndex

	argument = ACPI_METHOD_NEXT_ARGUMENT(argument);
	argument->DataLength = ACPI_METHOD_ARGUMENT_LENGTH(sizeof(ULONG)) * 2;
	argument->Type = ACPI_METHOD_ARGUMENT_PACKAGE_EX;
	argument = (PACPI_METHOD_ARGUMENT)argument->Data;
	ACPI_METHOD_SET_ARGUMENT_INTEGER(argument, (ULONG)offset);
	argument = ACPI_METHOD_NEXT_ARGUMENT(argument);
	ACPI_METHOD_SET_ARGUMENT_INTEGER(argument, (ULONG)size);

	outputArgumentBufferSize = ACPI_METHOD_ARGUMENT_LENGTH((ULONG)size);
	outputBufferSize = FIELD_OFFSET(ACPI_EVAL_OUTPUT_BUFFER, Argument) + outputArgumentBufferSize;

	outputBuffer =
		(PACPI_EVAL_OUTPUT_BUFFER)ExAllocatePool2(POOL_FLAG_NON_PAGED, outputBufferSize, CROS_EC_POOL_TAG);

	if(outputBuffer == NULL) {
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto Exit;
	}

	WDF_MEMORY_DESCRIPTOR_INIT_HANDLE(&inputMemDesc, inputMemory, NULL);
	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputMemDesc, outputBuffer, (ULONG)outputBufferSize);

	WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions, WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);
	WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&sendOptions, WDF_REL_TIMEOUT_IN_MS(1000));

	status = WdfIoTargetSendInternalIoctlSynchronously(WdfDeviceGetIoTarget(device), NULL, IOCTL_ACPI_EVAL_METHOD,
	                                                   &inputMemDesc, &outputMemDesc, &sendOptions, NULL);

	if(!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		            "[Device: 0x%p] IOCTL_ACPI_EVAL_METHOD for _DSM failed - %!STATUS!", device, status);
		goto Exit;
	}

	if(outputBuffer->Signature != ACPI_EVAL_OUTPUT_BUFFER_SIGNATURE) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		            "[Device: 0x%p] ACPI_EVAL_OUTPUT_BUFFER signature is incorrect", device);
		status = STATUS_ACPI_INVALID_DATA;
		goto Exit;
	}

Exit:

	if(inputMemory != WDF_NO_HANDLE) {
		WdfObjectDelete(inputMemory);
	}

	if(!NT_SUCCESS(status) || (Output == NULL)) {
		if(outputBuffer) {
			ExFreePoolWithTag(outputBuffer, CROS_EC_POOL_TAG);
		}
	} else {
		*Output = outputBuffer;
	}

	return status;
}

_IRQL_requires_max_(PASSIVE_LEVEL) NTSTATUS Acpi_EvaluateDsm(_In_ WDFDEVICE device,
                                                             _In_ ULONG FunctionIndex,
                                                             _Outptr_opt_ PACPI_EVAL_OUTPUT_BUFFER* Output) {
	NTSTATUS status;
	WDFMEMORY inputMemory;
	WDF_MEMORY_DESCRIPTOR inputMemDesc;
	PACPI_EVAL_INPUT_BUFFER_COMPLEX inputBuffer;
	size_t inputBufferSize;
	size_t inputArgumentBufferSize;
	PACPI_METHOD_ARGUMENT argument;
	WDF_MEMORY_DESCRIPTOR outputMemDesc;
	PACPI_EVAL_OUTPUT_BUFFER outputBuffer;
	size_t outputBufferSize;
	size_t outputArgumentBufferSize;
	WDF_OBJECT_ATTRIBUTES attributes;
	WDF_REQUEST_SEND_OPTIONS sendOptions;

	PAGED_CODE();

	inputMemory = WDF_NO_HANDLE;
	outputBuffer = NULL;

	inputArgumentBufferSize = ACPI_METHOD_ARGUMENT_LENGTH(sizeof(GUID)) +
	                          ACPI_METHOD_ARGUMENT_LENGTH(sizeof(ULONG)) +
	                          ACPI_METHOD_ARGUMENT_LENGTH(sizeof(ULONG)) + ACPI_METHOD_ARGUMENT_LENGTH(0);

	inputBufferSize = FIELD_OFFSET(ACPI_EVAL_INPUT_BUFFER_COMPLEX, Argument) + inputArgumentBufferSize;

	WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
	attributes.ParentObject = device;

	status = WdfMemoryCreate(&attributes, NonPagedPoolNx, 0, inputBufferSize, &inputMemory, (PVOID*)&inputBuffer);

	if(!NT_SUCCESS(status)) {
		goto Exit;
	}

	RtlZeroMemory(inputBuffer, inputBufferSize);

	inputBuffer->Signature = ACPI_EVAL_INPUT_BUFFER_COMPLEX_SIGNATURE;
	inputBuffer->Size = (ULONG)inputArgumentBufferSize;
	inputBuffer->ArgumentCount = 4;
	inputBuffer->MethodNameAsUlong = (ULONG)'MSD_';

	argument = &(inputBuffer->Argument[0]);
	ACPI_METHOD_SET_ARGUMENT_BUFFER(argument, &GUID_ACPI_ECPR_DSM, sizeof(GUID_ACPI_ECPR_DSM));

	argument = ACPI_METHOD_NEXT_ARGUMENT(argument);
	ACPI_METHOD_SET_ARGUMENT_INTEGER(argument, 0);

	argument = ACPI_METHOD_NEXT_ARGUMENT(argument);
	ACPI_METHOD_SET_ARGUMENT_INTEGER(argument, FunctionIndex);

	argument = ACPI_METHOD_NEXT_ARGUMENT(argument);
	argument->Type = ACPI_METHOD_ARGUMENT_PACKAGE_EX;
	argument->DataLength = 0;

	outputArgumentBufferSize = ACPI_METHOD_ARGUMENT_LENGTH(sizeof(ULONG));
	outputBufferSize = FIELD_OFFSET(ACPI_EVAL_OUTPUT_BUFFER, Argument) + outputArgumentBufferSize;

	outputBuffer =
		(PACPI_EVAL_OUTPUT_BUFFER)ExAllocatePool2(POOL_FLAG_NON_PAGED, outputBufferSize, CROS_EC_POOL_TAG);

	if(outputBuffer == NULL) {
		status = STATUS_INSUFFICIENT_RESOURCES;
		goto Exit;
	}

	WDF_MEMORY_DESCRIPTOR_INIT_HANDLE(&inputMemDesc, inputMemory, NULL);
	WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&outputMemDesc, outputBuffer, (ULONG)outputBufferSize);

	WDF_REQUEST_SEND_OPTIONS_INIT(&sendOptions, WDF_REQUEST_SEND_OPTION_SYNCHRONOUS);
	WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&sendOptions, WDF_REL_TIMEOUT_IN_MS(1000));

	status = WdfIoTargetSendInternalIoctlSynchronously(WdfDeviceGetIoTarget(device), NULL, IOCTL_ACPI_EVAL_METHOD,
	                                                   &inputMemDesc, &outputMemDesc, &sendOptions, NULL);

	if(!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		            "[Device: 0x%p] IOCTL_ACPI_EVAL_METHOD for _DSM failed - %!STATUS!", device, status);
		goto Exit;
	}

	if(outputBuffer->Signature != ACPI_EVAL_OUTPUT_BUFFER_SIGNATURE) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_DEVICE,
		            "[Device: 0x%p] ACPI_EVAL_OUTPUT_BUFFER signature is incorrect", device);
		status = STATUS_ACPI_INVALID_DATA;
		goto Exit;
	}

Exit:

	if(inputMemory != WDF_NO_HANDLE) {
		WdfObjectDelete(inputMemory);
	}

	if(!NT_SUCCESS(status) || (Output == NULL)) {
		if(outputBuffer) {
			ExFreePoolWithTag(outputBuffer, CROS_EC_POOL_TAG);
		}
	} else {
		*Output = outputBuffer;
	}

	return status;
}

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

	if(SUCCEEDED_NTSTATUS(WdfFdoQueryForInterface(device, &GUID_ACPI_INTERFACE_STANDARD2,
	                                              (PINTERFACE)&deviceContext->acpiInterface,
	                                              sizeof(ACPI_INTERFACE_STANDARD2), 1, NULL))) {
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "Got ACPI interface");
		PACPI_EVAL_OUTPUT_BUFFER output;
		if(SUCCEEDED_NTSTATUS(Acpi_EvaluateDsm(device, 0, &output))) {
			TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "Evaluated DSM; count is %d", output->Count);
			if(output->Count == 1) {
				TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "Arg0 type is %d",
				            output->Argument[0].Type);
				if(output->Argument[0].Type == ACPI_METHOD_ARGUMENT_INTEGER) {
					TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "Arg0 val is %d",
					            output->Argument[0].Data[0]);
					if(output->Argument[0].Data[0] == 1) {
						// call DSM, check return
						//TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE,
						            //"Using ACPI locking with CrosEC driver");
						// deviceContext->flags |= DEVICE_FLAG_USE_ACPI_LOCKING;
					}
				}
			}
			ExFreePoolWithTag(output, CROS_EC_POOL_TAG);
		}
	} else {
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "Did not get ACPI interface");
	}

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

NTSTATUS CrosECDeviceAcquireLock(_In_ WDFOBJECT device) {
	NTSTATUS Status = STATUS_SUCCESS;
	PDEVICE_CONTEXT deviceContext = DeviceGetContext(device);
	KeAcquireGuardedMutex(&deviceContext->mutex);
	if(deviceContext->flags & DEVICE_FLAG_USE_ACPI_LOCKING) {
		PACPI_EVAL_OUTPUT_BUFFER output;
		if(SUCCEEDED_NTSTATUS(Acpi_EvaluateDsm(device, 1, &output))) {
			if(output->Count == 1) {
				if(output->Argument[0].Type == ACPI_METHOD_ARGUMENT_INTEGER) {
					if(output->Argument[0].Data[0] != 0) {
						Status = STATUS_LOCK_NOT_GRANTED;
					} else {
						TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "ACPI Lock taken!");
					}
				}
			}
			ExFreePoolWithTag(output, CROS_EC_POOL_TAG);
		}
	}
	return Status;
}

NTSTATUS CrosECDeviceReleaseLock(_In_ WDFOBJECT device) {
	PDEVICE_CONTEXT deviceContext = DeviceGetContext(device);
	if(deviceContext->flags & DEVICE_FLAG_USE_ACPI_LOCKING) {
		Acpi_EvaluateDsm(device, 2, NULL);
		TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DEVICE, "ACPI Lock released!");
	}
	KeReleaseGuardedMutex(&deviceContext->mutex);
	return STATUS_SUCCESS;
}
