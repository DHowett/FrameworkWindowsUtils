/*++

Module Name:

	queue.c

Abstract:

	This file contains the queue entry points and callbacks.

Environment:

	Kernel-mode Driver Framework

--*/

#include "driver.h"
#include "queue.tmh"

#include "EC.h"

NTSTATUS CrosECIoctlXCmd(_In_ WDFDEVICE Device, _In_ PDEVICE_CONTEXT DeviceContext, _In_ WDFREQUEST Request);
NTSTATUS CrosECIoctlReadMem(_In_ WDFDEVICE Device, _In_ PDEVICE_CONTEXT DeviceContext, _In_ WDFREQUEST Request);

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, CrosECQueueInitialize)
#endif

NTSTATUS
CrosECQueueInitialize(
	_In_ WDFDEVICE Device
)
/*++

Routine Description:

	 The I/O dispatch callbacks for the frameworks device object
	 are configured in this function.

	 A single default I/O Queue is configured for parallel request
	 processing, and a driver context memory allocation is created
	 to hold our structure QUEUE_CONTEXT.

Arguments:

	Device - Handle to a framework device object.

Return Value:

	VOID

--*/
{
	WDFQUEUE queue;
	NTSTATUS status;
	WDF_IO_QUEUE_CONFIG queueConfig;

	PAGED_CODE();

	//
	// Configure a default queue so that requests that are not
	// configure-fowarded using WdfDeviceConfigureRequestDispatching to goto
	// other queues get dispatched here.
	//
	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(
		&queueConfig,
		WdfIoQueueDispatchParallel
	);

	queueConfig.EvtIoDeviceControl = CrosECEvtIoDeviceControl;
	queueConfig.EvtIoStop = CrosECEvtIoStop;

	status = WdfIoQueueCreate(
		Device,
		&queueConfig,
		WDF_NO_OBJECT_ATTRIBUTES,
		&queue
	);

	if (!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "WdfIoQueueCreate failed %!STATUS!", status);
		return status;
	}

	return status;
}

#define CROSEC_CMD_MAX (sizeof(struct cros_ec_command_v2) + CROSEC_CMD_MAX_REQUEST)

NTSTATUS CrosECIoctlXCmd(_In_ WDFDEVICE Device, _In_ PDEVICE_CONTEXT DeviceContext, _In_ WDFREQUEST Request) {
	struct cros_ec_command_v2* cmd;
	size_t cmdLen;
	NT_RETURN_IF_NTSTATUS_FAILED(WdfRequestRetrieveInputBuffer(Request, sizeof(cmd), (PVOID*)&cmd, &cmdLen));

	void* outbuf;
	size_t outLen;
	NT_RETURN_IF_NTSTATUS_FAILED(WdfRequestRetrieveOutputBuffer(Request, sizeof(*cmd), &outbuf, &outLen));
	NT_ANALYSIS_ASSUME(outLen >= sizeof(*cmd));

	// User tried to send/receive too much data
	NT_RETURN_IF(STATUS_BUFFER_OVERFLOW, cmdLen > CROSEC_CMD_MAX);
	NT_RETURN_IF(STATUS_BUFFER_OVERFLOW, outLen > CROSEC_CMD_MAX);
	// User tried to send/receive more bytes than they offered in storage
	NT_RETURN_IF(STATUS_BUFFER_TOO_SMALL, cmdLen < (sizeof(struct cros_ec_command_v2) + cmd->outsize));
	NT_RETURN_IF(STATUS_BUFFER_TOO_SMALL, outLen < (sizeof(struct cros_ec_command_v2) + cmd->insize));

	RtlZeroMemory(DeviceContext->inflightCommand, CROSEC_CMD_MAX);
	memcpy(DeviceContext->inflightCommand, cmd, cmdLen);

	int res = ECSendCommandLPCv3(Device, cmd->command, cmd->version, cmd->data, cmd->outsize, DeviceContext->inflightCommand->data, cmd->insize);
	if (res > 0) {
		DeviceContext->inflightCommand->insize = res;
		res = 0; // propagate it to the client
	}
	else {
		DeviceContext->inflightCommand->insize = 0; // Tell the client that nothing was received
	}

	DeviceContext->inflightCommand->result = res;

	int requiredReplySize = sizeof(struct cros_ec_command_v2) + DeviceContext->inflightCommand->insize;

	memcpy(outbuf, DeviceContext->inflightCommand, requiredReplySize);
	WdfRequestSetInformation(Request, requiredReplySize);
	return STATUS_SUCCESS;
}

NTSTATUS CrosECIoctlReadMem(_In_ WDFDEVICE Device, _In_ PDEVICE_CONTEXT DeviceContext, _In_ WDFREQUEST Request) {
	(void)DeviceContext;
	struct cros_ec_readmem_v2* rq;
	NT_RETURN_IF_NTSTATUS_FAILED(WdfRequestRetrieveInputBuffer(Request, sizeof(*rq), (PVOID*)&rq, NULL));
	struct cros_ec_readmem_v2* rs;
	NT_RETURN_IF_NTSTATUS_FAILED(WdfRequestRetrieveOutputBuffer(Request, sizeof(*rs), (PVOID*)&rs, NULL));

	rs->offset = rq->offset;
	rs->bytes = rq->bytes;
	if (rq->bytes > 0) {
		// Read specified bytes
		ECReadMemoryLPC(Device, rq->offset, rs->buffer, rq->bytes);
	}
	else {
		int i = 0;
		UCHAR* s = &rs->buffer[0];
		while (i < CROSEC_MEMMAP_SIZE) {
			ECReadMemoryLPC(Device, rq->offset + i, s, 1);
			++i;
			if (!*s) { break; }
		}
		rs->bytes = i;
	}

	WdfRequestSetInformation(Request, sizeof(*rs));
	return STATUS_SUCCESS;
}

VOID
CrosECEvtIoDeviceControl(
	_In_ WDFQUEUE Queue,
	_In_ WDFREQUEST Request,
	_In_ size_t OutputBufferLength,
	_In_ size_t InputBufferLength,
	_In_ ULONG IoControlCode
)
/*++

Routine Description:

	This event is invoked when the framework receives IRP_MJ_DEVICE_CONTROL request.

Arguments:

	Queue -  Handle to the framework queue object that is associated with the
			 I/O request.

	Request - Handle to a framework request object.

	OutputBufferLength - Size of the output buffer in bytes

	InputBufferLength - Size of the input buffer in bytes

	IoControlCode - I/O control code.

Return Value:

	VOID

--*/
{
	TraceEvents(TRACE_LEVEL_INFORMATION,
		TRACE_QUEUE,
		"%!FUNC! Queue 0x%p, Request 0x%p OutputBufferLength %d InputBufferLength %d IoControlCode %d",
		Queue, Request, (int)OutputBufferLength, (int)InputBufferLength, IoControlCode);

	WDFDEVICE device = WdfIoQueueGetDevice(Queue);
	PDEVICE_CONTEXT deviceContext = DeviceGetContext(device);
	NTSTATUS Status = STATUS_INVALID_PARAMETER;

	switch (IoControlCode) {
	case IOCTL_CROSEC_XCMD: {
		Status = CrosECIoctlXCmd(device, deviceContext, Request);
		break;
	}
	case IOCTL_CROSEC_RDMEM: {
		Status = CrosECIoctlReadMem(device, deviceContext, Request);
		break;
	}
	}

	WdfRequestComplete(Request, Status);

	return;
}

VOID
CrosECEvtIoStop(
	_In_ WDFQUEUE Queue,
	_In_ WDFREQUEST Request,
	_In_ ULONG ActionFlags
)
/*++

Routine Description:

	This event is invoked for a power-managed queue before the device leaves the working state (D0).

Arguments:

	Queue -  Handle to the framework queue object that is associated with the
			 I/O request.

	Request - Handle to a framework request object.

	ActionFlags - A bitwise OR of one or more WDF_REQUEST_STOP_ACTION_FLAGS-typed flags
				  that identify the reason that the callback function is being called
				  and whether the request is cancelable.

Return Value:

	VOID

--*/
{
	TraceEvents(TRACE_LEVEL_INFORMATION,
		TRACE_QUEUE,
		"%!FUNC! Queue 0x%p, Request 0x%p ActionFlags %d",
		Queue, Request, ActionFlags);

	//
	// In most cases, the EvtIoStop callback function completes, cancels, or postpones
	// further processing of the I/O request.
	//
	// Typically, the driver uses the following rules:
	//
	// - If the driver owns the I/O request, it calls WdfRequestUnmarkCancelable
	//   (if the request is cancelable) and either calls WdfRequestStopAcknowledge
	//   with a Requeue value of TRUE, or it calls WdfRequestComplete with a
	//   completion status value of STATUS_SUCCESS or STATUS_CANCELLED.
	//
	//   Before it can call these methods safely, the driver must make sure that
	//   its implementation of EvtIoStop has exclusive access to the request.
	//
	//   In order to do that, the driver must synchronize access to the request
	//   to prevent other threads from manipulating the request concurrently.
	//   The synchronization method you choose will depend on your driver's design.
	//
	//   For example, if the request is held in a shared context, the EvtIoStop callback
	//   might acquire an internal driver lock, take the request from the shared context,
	//   and then release the lock. At this point, the EvtIoStop callback owns the request
	//   and can safely complete or requeue the request.
	//
	// - If the driver has forwarded the I/O request to an I/O target, it either calls
	//   WdfRequestCancelSentRequest to attempt to cancel the request, or it postpones
	//   further processing of the request and calls WdfRequestStopAcknowledge with
	//   a Requeue value of FALSE.
	//
	// A driver might choose to take no action in EvtIoStop for requests that are
	// guaranteed to complete in a small amount of time.
	//
	// In this case, the framework waits until the specified request is complete
	// before moving the device (or system) to a lower power state or removing the device.
	// Potentially, this inaction can prevent a system from entering its hibernation state
	// or another low system power state. In extreme cases, it can cause the system
	// to crash with bugcheck code 9F.
	//

	return;
}
