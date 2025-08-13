#ifndef __XMEMWRAPPER_H__
#define __XMEMWRAPPER_H__

#if __XENON

#include "system/xtl.h"
#include "system/tls.h"

void* WINAPI	XMemAllocCustom		(unsigned long dwSize, unsigned long  dwAllocAttributes);
void WINAPI		XMemFreeCustom		(void *pAddress, unsigned long dwAllocAttributes);
SIZE_T WINAPI	XMemSizeCustom		(void *pAddress, unsigned long dwAllocAttributes);


// Some xgraphics functions allocate memory internally,
// this wrapper allows users of those function to provide their own memory buffer.
// It's hooked up in XMemAlloc and XMemFree in main.cpp
enum eXGraphicsExtMemBufferId 
{
	XGRAPHICS_MB_USER_MESHBLENDMGR = 0,
	XGRAPHICS_MB_USER_PEDHEADSHOTMGR = 1,
	XGRAPHICS_MB_COUNT,
};

class XGraphicsExtMemBufferHelper
{
public :


	static __THREAD void*	ms_pBuffer[XGRAPHICS_MB_COUNT];
	static u32				ms_bufferSize[XGRAPHICS_MB_COUNT];

};

#endif // __XENON

#endif // __XMEMWRAPPER_H__