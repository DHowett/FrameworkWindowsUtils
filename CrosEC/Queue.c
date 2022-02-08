#include "driver.h"
#include "queue.tmh"

#include "EC.h"

NTSTATUS CrosECIoctlXCmd(_In_ WDFDEVICE Device, _In_ PDEVICE_CONTEXT DeviceContext, _In_ WDFREQUEST Request);
NTSTATUS CrosECIoctlReadMem(_In_ WDFDEVICE Device, _In_ PDEVICE_CONTEXT DeviceContext, _In_ WDFREQUEST Request);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, CrosECQueueInitialize)
#endif

#define EC_CMD_USB_PD_FW_UPDATE 0x0110
#define EC_CMD_FLASH_WRITE      0x0012
#define EC_CMD_FLASH_ERASE      0x0013
#define EC_CMD_FLASH_PROTECT    0x0015

static NTSTATUS sCrosECErrorCodeMapping[] = {
	[EC_RES_SUCCESS] = STATUS_SUCCESS,
	[EC_RES_INVALID_COMMAND] = STATUS_INVALID_PARAMETER,
	[EC_RES_ERROR] = STATUS_UNSUCCESSFUL,
	[EC_RES_INVALID_PARAM] = STATUS_INVALID_PARAMETER,
	[EC_RES_ACCESS_DENIED] = STATUS_ACCESS_DENIED,
	[EC_RES_INVALID_RESPONSE] = STATUS_DATA_ERROR,
	[EC_RES_INVALID_VERSION] = STATUS_DATA_ERROR,
	[EC_RES_INVALID_CHECKSUM] = STATUS_CRC_ERROR,
	[EC_RES_IN_PROGRESS] = CROSEC_STATUS_IN_PROGRESS,
	[EC_RES_UNAVAILABLE] = CROSEC_STATUS_UNAVAILABLE,
	[EC_RES_TIMEOUT] = STATUS_IO_TIMEOUT,
	[EC_RES_OVERFLOW] = STATUS_BUFFER_OVERFLOW,
	[EC_RES_INVALID_HEADER] = STATUS_DATA_ERROR,
	[EC_RES_REQUEST_TRUNCATED] = STATUS_BUFFER_TOO_SMALL,
	[EC_RES_RESPONSE_TOO_BIG] = STATUS_BUFFER_OVERFLOW,
	[EC_RES_BUS_ERROR] = STATUS_UNSUCCESSFUL,
	[EC_RES_BUSY] = STATUS_DEVICE_BUSY,
	[EC_RES_INVALID_HEADER_VERSION] = STATUS_DATA_ERROR,
	[EC_RES_INVALID_HEADER_CRC] = STATUS_CRC_ERROR,
	[EC_RES_INVALID_DATA_CRC] = STATUS_CRC_ERROR,
	[EC_RES_DUP_UNAVAILABLE] = STATUS_UNSUCCESSFUL,
};

NTSTATUS CrosECQueueInitialize(_In_ WDFDEVICE Device) {
	WDFQUEUE queue;
	NTSTATUS status;
	WDF_IO_QUEUE_CONFIG queueConfig;

	PAGED_CODE();

	//
	// Configure a default queue so that requests that are not
	// configure-fowarded using WdfDeviceConfigureRequestDispatching to goto
	// other queues get dispatched here.
	//
	WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchParallel);

	queueConfig.EvtIoDeviceControl = CrosECEvtIoDeviceControl;
	queueConfig.EvtIoStop = CrosECEvtIoStop;

	status = WdfIoQueueCreate(Device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &queue);

	if(!NT_SUCCESS(status)) {
		TraceEvents(TRACE_LEVEL_ERROR, TRACE_QUEUE, "WdfIoQueueCreate failed %!STATUS!", status);
		return status;
	}

	return status;
}

NTSTATUS CrosECIoctlXCmd(_In_ WDFDEVICE Device, _In_ PDEVICE_CONTEXT DeviceContext, _In_ WDFREQUEST Request) {
	PCROSEC_COMMAND cmd;
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
	NT_RETURN_IF(STATUS_BUFFER_TOO_SMALL, cmdLen < (sizeof(CROSEC_COMMAND) + cmd->outsize));
	NT_RETURN_IF(STATUS_BUFFER_TOO_SMALL, outLen < (sizeof(CROSEC_COMMAND) + cmd->insize));

	// I know this seems overprotective, and that I am wielding too much power over you,
	// but I don't think that the Windows driver should let you erase your EC flash.
	// Since the device grants access to all administrators, that would put you one
	// bad apple away from bricking your machine. Sorry.
	NT_RETURN_IF(STATUS_ACCESS_DENIED, cmd->command == EC_CMD_FLASH_ERASE || cmd->command == EC_CMD_FLASH_PROTECT ||
	                                           cmd->command == EC_CMD_FLASH_WRITE ||
	                                           cmd->command == EC_CMD_USB_PD_FW_UPDATE);

	memset(DeviceContext->inflightCommand, 0, CROSEC_CMD_MAX);
	memcpy(DeviceContext->inflightCommand, cmd, cmdLen);

	KeAcquireGuardedMutex(&DeviceContext->mutex);

	int res = ECSendCommandLPCv3(Device, cmd->command, cmd->version, CROSEC_COMMAND_DATA(cmd), cmd->outsize,
	                             CROSEC_COMMAND_DATA(DeviceContext->inflightCommand), cmd->insize);

	KeReleaseGuardedMutex(&DeviceContext->mutex);

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE,
	            "%!FUNC! Request 0x%p Command %u Version %u OutSize %u Result %d", Request, cmd->command,
	            cmd->version, cmd->outsize, res);

	if(res < -EECRESULT) {
		// Propagate a response code from the EC as res (EC result codes are positive)
		DeviceContext->inflightCommand->result = (-res) - EECRESULT;
		res = 0;  // tell the client we received nothing
	} else if(res < 0) {
		// Transform the protocol failure into an NTSTATUS and return early.
		NT_RETURN_IF(STATUS_FAIL_CHECK, -res > EC_RES_DUP_UNAVAILABLE);
		return sCrosECErrorCodeMapping[-res];
	} else {
		DeviceContext->inflightCommand->result = 0;  // 0 = SUCCESS
	}

	int requiredReplySize = sizeof(CROSEC_COMMAND) + res;
	if(requiredReplySize > outLen) {
		return STATUS_BUFFER_TOO_SMALL;
	}

	memcpy(outbuf, DeviceContext->inflightCommand, requiredReplySize);
	WdfRequestSetInformation(Request, requiredReplySize);
	return STATUS_SUCCESS;
}

NTSTATUS CrosECIoctlReadMem(_In_ WDFDEVICE Device, _In_ PDEVICE_CONTEXT DeviceContext, _In_ WDFREQUEST Request) {
	PCROSEC_READMEM rq, rs;
	NT_RETURN_IF_NTSTATUS_FAILED(WdfRequestRetrieveInputBuffer(Request, sizeof(*rq), (PVOID*)&rq, NULL));
	NT_RETURN_IF_NTSTATUS_FAILED(WdfRequestRetrieveOutputBuffer(Request, sizeof(*rs), (PVOID*)&rs, NULL));

	NT_RETURN_IF(STATUS_INVALID_ADDRESS, (rq->offset + rq->bytes) > CROSEC_MEMMAP_SIZE);

	KeAcquireGuardedMutex(&DeviceContext->mutex);
	int res = ECReadMemoryLPC(Device, rq->offset, rs->buffer, rq->bytes);
	KeReleaseGuardedMutex(&DeviceContext->mutex);

	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "%!FUNC! Request 0x%p Offset 0x%x Buffer %d Result %d",
	            Request, rq->offset, rq->bytes, res);

	NT_RETURN_IF(STATUS_UNSUCCESSFUL, res < 0);

	rs->offset = rq->offset;
	rs->bytes = res;

	WdfRequestSetInformation(Request, sizeof(*rs));
	return STATUS_SUCCESS;
}

VOID CrosECEvtIoDeviceControl(_In_ WDFQUEUE Queue,
                              _In_ WDFREQUEST Request,
                              _In_ size_t OutputBufferLength,
                              _In_ size_t InputBufferLength,
                              _In_ ULONG IoControlCode) {
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE,
	            "%!FUNC! Queue 0x%p, Request 0x%p OutputBufferLength %d InputBufferLength %d IoControlCode %d",
	            Queue, Request, (int)OutputBufferLength, (int)InputBufferLength, IoControlCode);

	WDFDEVICE device = WdfIoQueueGetDevice(Queue);
	PDEVICE_CONTEXT deviceContext = DeviceGetContext(device);
	NTSTATUS Status = STATUS_INVALID_PARAMETER;

	switch(IoControlCode) {
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

VOID CrosECEvtIoStop(_In_ WDFQUEUE Queue, _In_ WDFREQUEST Request, _In_ ULONG ActionFlags) {
	TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "%!FUNC! Queue 0x%p, Request 0x%p ActionFlags %d", Queue,
	            Request, ActionFlags);

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
