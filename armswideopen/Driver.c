#include <ntifs.h>

#define IO_UNSET_THREADHIDEFROMDEBUGGER CTL_CODE(FILE_DEVICE_UNKNOWN, 0x8B2, METHOD_BUFFERED, FILE_SPECIAL_ACCESS)
#define DebugMessage(x, ...) DbgPrintEx(0, 0, x, __VA_ARGS__)

UNICODE_STRING dev, dos;

NTSTATUS IrpCreate(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	DebugMessage("armswideopen: connected\n");

	return STATUS_SUCCESS;
}

NTSTATUS IrpClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;

	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	DebugMessage("armswideopen: disconnected!\n");

	return STATUS_SUCCESS;
}

int GetEthreadOffset() {
	RTL_OSVERSIONINFOW versionInformation;

	RtlGetVersion(&versionInformation);

	switch (versionInformation.dwBuildNumber) {
	case 17763:
		return 436;
	case 19042:
		return 324;
	}

	return -1;
}

NTSTATUS IrpDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);

	if (stack->Parameters.DeviceIoControl.IoControlCode == IO_UNSET_THREADHIDEFROMDEBUGGER)
	{
		PULONG buffer = (PULONG)Irp->AssociatedIrp.SystemBuffer;
		ULONG input = *buffer;

		int ethreadOffset = GetEthreadOffset();

		if (ethreadOffset < 0) {
			DebugMessage("armswideopen: Can't calculate ETHREAD offset for current kernel build, aborting\n");
		}

		LONG* pEthread;
		NTSTATUS result = PsLookupThreadByThreadId((HANDLE)input, (PETHREAD*)&pEthread);
		if (result < 0) {
			DebugMessage("armswideopen: Couldn't get pointer to ETHREAD struct, error: %X\n", result);
		}
		else {
			DebugMessage("armswideopen: Unsetting ThreadHideFromDebugger flag\n");
			_InterlockedAnd((volatile LONG*)(pEthread + ethreadOffset), (0xFFFFFFFF - 4));

			ObDereferenceObject(pEthread);
		}

		Irp->IoStatus.Status = STATUS_SUCCESS;
		Irp->IoStatus.Information = 0;
	}
	else
	{
		Irp->IoStatus.Status = STATUS_UNSUCCESSFUL;
		Irp->IoStatus.Information = 0;
	}

	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return Irp->IoStatus.Status;
}

NTSTATUS UnloadDriver(PDRIVER_OBJECT pDriverObject)
{
	UNREFERENCED_PARAMETER(pDriverObject);

	DebugMessage("armswideopen: shutdown\n");

	IoDeleteSymbolicLink(&dos);
	IoDeleteDevice(pDriverObject->DeviceObject);

	return STATUS_SUCCESS;
}

NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT     pDriverObject,
    _In_ PUNICODE_STRING    pRegistryPath
)
{
    UNREFERENCED_PARAMETER(pRegistryPath);
    pDriverObject->DriverUnload = (PDRIVER_UNLOAD) UnloadDriver;
    DebugMessage("armswideopen: init\n");

    RtlInitUnicodeString(&dev, L"\\Device\\armswideopen");
    RtlInitUnicodeString(&dos, L"\\DosDevices\\armswideopen");

	PDEVICE_OBJECT pDeviceObject;

    IoCreateDevice(pDriverObject, 0, &dev, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &pDeviceObject);
    IoCreateSymbolicLink(&dos, &dev);

    pDriverObject->MajorFunction[IRP_MJ_CREATE] = IrpCreate;
    pDriverObject->MajorFunction[IRP_MJ_CLOSE] = IrpClose;
    pDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = IrpDeviceControl;

    pDeviceObject->Flags |= DO_DIRECT_IO;
    pDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    return STATUS_SUCCESS;
}
