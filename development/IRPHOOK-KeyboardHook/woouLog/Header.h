#ifndef __HEADER_h__
#define __HEADER_h__


#include <wdm.h>
#include "ntddk.h"
#include "ntddkbd.h"

typedef struct KEY_STATE
{
	BOOLEAN kSHIFT; //if the shift key is pressed 
	BOOLEAN kCAPSLOCK; //if the caps lock key is pressed down
	BOOLEAN kCTRL; //if the control key is pressed down
	BOOLEAN kALT; //if the alt key is pressed down
}KEY_STATE;
typedef struct KEY_DATA
{
	LIST_ENTRY ListEntry;
	char keyScan;
	char keyCode[3];
	char keyFlags;
}KEY_DATA, *PKEY_DATA;

typedef struct _MYDEVICE_EXTENSION
{
	PDEVICE_OBJECT pKeyboardDevice; //Ű���� ����̽� ������Ʈ
	KEY_STATE kState;				//Ű���� ����(cap/shift/ctrl ��)
	PLIST_ENTRY listHead;
	PLIST_ENTRY listTail;
}MYDEVICE_EXTENSION, *PMYDEVICE_EXTENSION;


extern CHAR flag_one;
#endif