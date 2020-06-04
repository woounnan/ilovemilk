#include "irp.h"
#include "getCode.h"
PDEVICE_OBJECT p_irpDev = NULL;
int irpCount = 0;
USHORT scanCode = NULL;

#define TIME_SECOND(x) x*10000000*-1

//Ÿ�� ����̹� ������Ʈ ���ϱ�
NTSTATUS getDriverObject(PCWSTR deviceName, PDRIVER_OBJECT *pdrv_obj)
{

	UNICODE_STRING deviceKeyUnicodeString;
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	PFILE_OBJECT file_obj;
	PDEVICE_OBJECT dev_obj;

	RtlInitUnicodeString(&deviceKeyUnicodeString, deviceName);
	//Keyboard ����̹��� ���� ����̽� ������Ʈ �����͸� ����.
	ntStatus = IoGetDeviceObjectPointer(
		&deviceKeyUnicodeString,
		FILE_READ_DATA,
		&file_obj,
		&dev_obj);

	*pdrv_obj = dev_obj->DriverObject;
	if (!NT_SUCCESS(ntStatus))
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "IoGetDeviceObjectPointer() error []");
	else
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "Successful. Getting Driver Object");

	return ntStatus;
}

//����̽� ������Ʈ �߰�
NTSTATUS myAddDevice(PDRIVER_OBJECT DriverObject, PCWCHAR TargetDeviceName)
{
	//1.��� �ʱ�ȭ
	PMYDEVICE_EXTENSION pMyDev_Ext = NULL;
	NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	UNICODE_STRING u_TargetDev;
	DbgPrintEx(DPFLTR_ACPI_ID, 0, "Call myAddDeivce()\n");
	RtlInitUnicodeString(&u_TargetDev, TargetDeviceName);

	//2.����̽� ������Ʈ ����
	ntStatus = IoCreateDevice(
		DriverObject,
		sizeof(MYDEVICE_EXTENSION),  //���⿡ ������ ũ�⸸ŭ �޸𸮿� ���������� �Ҵ�ȴ�.
		NULL, //����̽� ������Ʈ �̸�
		FILE_DEVICE_KEYBOARD,
		0,
		FALSE,
		&p_irpDev //������ ����̽� ������Ʈ�� ����� ������
	);

	if (!NT_SUCCESS(ntStatus))
	{
		DbgPrintEx(DPFLTR_ACPI_ID, 0, "IoCreateDevice to failed\n");
		//goto exit;
	}
	
	pMyDev_Ext = (PMYDEVICE_EXTENSION)p_irpDev->DeviceExtension;
	RtlZeroMemory(pMyDev_Ext, sizeof(MYDEVICE_EXTENSION));
	DbgPrintEx(DPFLTR_ACPI_ID, 0, "init::list_head:%x\n", pMyDev_Ext->listHead);
	DbgPrintEx(DPFLTR_ACPI_ID, 0, "IoCreateDevice to succeed\n");
	//3.low device object ����
	//flag ����
	p_irpDev->Flags |= DO_BUFFERED_IO;
	p_irpDev->Flags |= DO_POWER_PAGABLE;


	ntStatus = IoAttachDevice(  //��� ����̽� ���ÿ� ���� ���� ����̽�������Ʈ�� attach
		p_irpDev,
		&u_TargetDev,
		&pMyDev_Ext->pKeyboardDevice //NEXT DEVICE OBJECT
	);
	DbgPrintEx(DPFLTR_ACPI_ID, 0, "addDeivce::irpDev->ext->nextDev:%x\n", ((PMYDEVICE_EXTENSION)p_irpDev->DeviceExtension)->pKeyboardDevice);
	DbgPrintEx(DPFLTR_ACPI_ID, 0, "addDevice:: IRP::irpDev:%x\n", p_irpDev);
	if (!NT_SUCCESS(ntStatus))
	{
		DbgPrintEx(DPFLTR_ACPI_ID, 0, "IoAttachDevice to failed\n");
		goto exit;
	}
	DbgPrintEx(DPFLTR_ACPI_ID, 0, "IoAttachDevice to succeed\n");
exit:
	/*
	����̽� ������Ʈ �ʱ�ȭ �÷��׸� ����� ����
	�÷��׸� ����ٴ� ���� �ʱ�ȭ�� �����ٰ� �˸��� ����
	�ʱ�ȭ�� �����ٰ� �˸��� ������(�÷��װ��� ������ ������)
	����̽� ���ÿ� �߰����� ����̽� ������Ʈ�� ������ �� ����
	������ ���� ���� ����̽� ������Ʈ�� �����ϸ� �ǹǷ� �÷��װ��� ������ �ʰ���
	*/
	//pMyDev->Flags &= ~DO_DEVICE_INITIALIZING;  
	
	DbgPrintEx(DPFLTR_ACPI_ID, 0, "End::AddDevice()\n");
	return ntStatus;
}	

//Low ����̹��� IRP ����
NTSTATUS passDown(
	IN PDEVICE_OBJECT 	MyDevObj,
	IN PIRP	Irp
)
{
	//NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	PDEVICE_OBJECT pNextDevObj;

	PMYDEVICE_EXTENSION pMyDev = (PMYDEVICE_EXTENSION)MyDevObj->DeviceExtension;
	pNextDevObj = pMyDev->pKeyboardDevice;
	IoSkipCurrentIrpStackLocation(Irp);
	
	return IoCallDriver(pNextDevObj, Irp);
}


NTSTATUS readCompletion(
	PDEVICE_OBJECT DeviceObject,
	PIRP irp,
	PVOID Context)
{
	//NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Context);


	PMYDEVICE_EXTENSION pMyDevExt = (PMYDEVICE_EXTENSION)DeviceObject->DeviceExtension;
	DbgPrintEx(DPFLTR_ACPI_ID, 0, "Call readCompletion()\n");
	if (irp->IoStatus.Status == STATUS_SUCCESS)
	{
		PKEYBOARD_INPUT_DATA keys = irp->AssociatedIrp.SystemBuffer;
		int num_keys = irp->IoStatus.Information / sizeof(KEYBOARD_INPUT_DATA);

		for (int i = 0; i < num_keys; i++)
		{

			DbgPrintEx(DPFLTR_ACPI_ID, 0, "ScanCode: %x\n", keys[i].MakeCode);

			//Ű���� ������ ���� kData �Ҵ�
			KEY_DATA* kData = (KEY_DATA*)ExAllocatePool(NonPagedPool, sizeof(KEY_DATA));
			RtlZeroMemory(kData, sizeof(KEY_DATA));
			//Ű���� ������ ����.
			kData->keyScan = (char)keys[i].MakeCode;
			kData->keyFlags = (char)keys[i].Flags;
			
			//��ĵ�ڵ� -> Ű�ڵ�(�ƽ�Ű)
			convertScanToKey(pMyDevExt, kData, kData->keyCode);
			DbgPrintEx(DPFLTR_ACPI_ID, 0, "key:%s\n", kData->keyCode);
			/*
			DbgPrintEx(DPFLTR_ACPI_ID, 0, "####\nchar:%c\n", ((PKEY_DATA)&kData->ListEntry)->keyCode);
			if (!((strlen(kData->keyCode) != 0) && (kData->keyFlags == KEY_BREAK)))
				goto exit;
			KTIMER kTimer;
			LARGE_INTEGER timeout;
			timeout.QuadPart = (LONGLONG)TIME_SECOND(0.1);

			while (flag_one == TRUE)
			{
				KeInitializeTimer(&kTimer);
				
				KeSetTimer(&kTimer, timeout, NULL);
				KeWaitForSingleObject(&kTimer, Executive, KernelMode, FALSE, NULL);
				
				DbgPrintEx(DPFLTR_ACPI_ID, 0, "Waiting until data processing is completed.\n");
			}
			//Ű ������ ����Ʈ�� ����.
			if (pMyDevExt->listHead == NULL)
			{
				DbgPrintEx(DPFLTR_ACPI_ID, 0, "@@@@@\nchar:%c\n", ((PKEY_DATA)&kData->ListEntry)->keyCode);
				pMyDevExt->listHead = &kData->ListEntry;
				//DbgPrintEx(DPFLTR_ACPI_ID, 0, "listHead:%x\n", pMyDevExt->listHead);
			}
			else
			{
				DbgPrintEx(DPFLTR_ACPI_ID, 0, "@@@@@\nchar:%c\n", ((PKEY_DATA)&kData->ListEntry)->keyCode);

				PLIST_ENTRY pListCur = pMyDevExt->listHead;

				while (pListCur->Blink != NULL)
				{
					//DbgPrintEx(DPFLTR_ACPI_ID, 0, "pListCur:%x\n", pListCur);
					pListCur = pListCur->Blink;
				}
				//������ ����Ʈ Blink�� ���� kData ����.

				pListCur->Blink = &kData->ListEntry;
				pListCur->Blink->Flink = pListCur;
				//DbgPrintEx(DPFLTR_ACPI_ID, 0, "end list:%x\n", pListCur->Blink);
			}
			*/
		}
	}
	/*PLIST_ENTRY pListCur = pMyDevExt->listHead;
	
	while (pListCur->Blink != NULL)
	{
		DbgPrintEx(DPFLTR_ACPI_ID, 0, "char:%c\n", ((PKEY_DATA)pListCur)->keyCode);
		pListCur = pListCur->Blink;
	}*/

//exit:
	if (irp->PendingReturned)
	{
		IoMarkIrpPending(irp);
	}

	irpCount--;
	return irp->IoStatus.Status;
}

// READ IRP �� ���� ó���� ����.
NTSTATUS myRead(
	IN PDEVICE_OBJECT 	myDevObj,
	IN PIRP				irp
)
{
	//NTSTATUS ntStatus = STATUS_NOT_SUPPORTED;
	PMYDEVICE_EXTENSION pDeviceExtension;
	PDEVICE_OBJECT pNextDev;
	PIO_STACK_LOCATION pStack = IoGetCurrentIrpStackLocation(irp);

	irpCount++;
	DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "start::myRead()\n");
	DbgPrintEx(DPFLTR_ACPI_ID, 0, "irpCount:%x\n", irpCount);
	pDeviceExtension = (PMYDEVICE_EXTENSION)myDevObj->DeviceExtension;
	pNextDev = pDeviceExtension->pKeyboardDevice;

	switch (pStack->MajorFunction) {
	case IRP_MJ_READ:
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, 0, "The keyboard has been pressed\n");
		IoCopyCurrentIrpStackLocationToNext(irp);
		
		//IoSetCompletionRoutine(irp, readCompletion, myDevObj, TRUE, TRUE, TRUE);
		IoSetCompletionRoutineEx(myDevObj, irp, readCompletion, NULL, TRUE, TRUE, TRUE);
		break;
	default:
		break;
	}

	return IoCallDriver(pNextDev, irp);
}

void myIrpDelete(PDRIVER_OBJECT DriverObject)
{
	UNREFERENCED_PARAMETER(DriverObject);
	
	IoDeleteDevice(p_irpDev);
}