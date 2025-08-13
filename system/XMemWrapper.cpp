#include "XMemWrapper.h"
#if __XENON

// rage headers
#include "system/memory.h"
#include "system/memmanager.h"
#include "system/memvisualize.h"
#include "audiohardware/driver.h"
#include "system/tinyheap.h"
#include "system/systemallocator_xenon.h"
#include "streaming/streamingengine.h"

// game headers
#include "network/Network.h"

//////////////////////////////////////////////////////////////////////////
//
__THREAD void*	XGraphicsExtMemBufferHelper::ms_pBuffer[XGRAPHICS_MB_COUNT] = { NULL, NULL };
u32				XGraphicsExtMemBufferHelper::ms_bufferSize[XGRAPHICS_MB_COUNT] = { 0U, 0U };

const unsigned PrivatePageSize = 65536;
sysCriticalSectionToken PrivatePhysicalToken;
sysTinyHeap *s_FirstHeap;

void* PrivatePhysicalAlloc(unsigned long dwSize,unsigned long dwAllocAttributes)
{
	SYS_CS_SYNC(PrivatePhysicalToken);
	sysTinyHeap *i = s_FirstHeap;
	void *result;
	if (dwSize)
		dwSize = (dwSize + 31) & ~31;
	else
		dwSize = 32;
	while (i) 
	{
		if (i->GetUser1() == dwAllocAttributes)
		{
			result = i->Allocate(dwSize + 16);	// This guarantees all nodes are at multiple of 32
			if (result)
			{
				Assert(((size_t)result & 31) == 16);
				return ((char*)result + 16);
			}
		}
		i = (sysTinyHeap*)i->GetUser0();
	}
	// Otherwise allocate a new page, making sure it's large enough including heap overhead
	unsigned thisPageSize = PrivatePageSize;
	while (thisPageSize < dwSize + sizeof(sysTinyHeap) + 32)
		thisPageSize += PrivatePageSize;
	// MEMORYSTATUS before, after;
	// GlobalMemoryStatus(&before);
	sysTinyHeap *newHeap = (sysTinyHeap*)XMemAllocDefault(thisPageSize, dwAllocAttributes);
	// GlobalMemoryStatus(&after);
	if (!newHeap)
		Quitf("PrivatePhysicalAlloc - Physical memory exhausted");
	newHeap->Init(newHeap + 1,thisPageSize - sizeof(sysTinyHeap));
	newHeap->SetUser0((size_t)s_FirstHeap);
	// Remember the exact allocation attributes so we don't mix heap types up
	newHeap->SetUser1(dwAllocAttributes);
	// printf("PrivatePhysicalAlloc - New private heap sized %x type %x created (%u delta).\n",thisPageSize,dwAllocAttributes,before.dwAvailPhys - after.dwAvailPhys);
	s_FirstHeap = newHeap;
	result = s_FirstHeap->Allocate(dwSize + 16);
	Assert(((size_t)result & 31) == 16);
	return ((char*)result + 16);
}

bool PrivatePhysicalFree(void *ptr)
{
	SYS_CS_SYNC(PrivatePhysicalToken);

	// NOTE: We never return memory to the system; this could be done if necessary
	// but shader reloading will recycle the memory anyway.
	sysTinyHeap *i = s_FirstHeap;
	while (i) 
	{
		if (ptr >= i->GetHeapStart() && ptr < i->GetHeapEnd()) 
		{
			// tinyheap guarantees 16 byte alignment so we fudge the result
			i->Free((char*)ptr - 16);
			// TODO - if the heap was now empty, we could return the whole thing to the system now
			return true;
		}
		i = (sysTinyHeap*)i->GetUser0();
	}
	return false;
} 

size_t PrivatePhysicalSize(void *ptr)
{
	SYS_CS_SYNC(PrivatePhysicalToken);
	sysTinyHeap *i = s_FirstHeap;
	while (i) 
	{
		if (ptr >= i->GetHeapStart() && ptr < i->GetHeapEnd()) 
			return i->GetSize((char*)ptr - 16) - 16;
		i = (sysTinyHeap*)i->GetUser0();
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
//
void* WINAPI XMemAllocCustom(unsigned long dwSize, unsigned long  dwAllocAttributes)
{
	XALLOC_ATTRIBUTES	*pAttributes = (XALLOC_ATTRIBUTES *)&dwAllocAttributes;

	int			iAllocId = pAttributes->dwAllocatorId;
	unsigned	iAlignment = pAttributes->dwAlignment;
	const bool	bIsPhysical = XALLOC_IS_PHYSICAL(dwAllocAttributes);
	void		*pBuffer = NULL;

	if	(bIsPhysical)
	{
		if	(iAlignment == XALLOC_PHYSICAL_ALIGNMENT_DEFAULT)
			iAlignment = 4096;
		else
			iAlignment = 1 << iAlignment;
	}
	else
	{
		switch	(iAlignment)
		{
		case XALLOC_ALIGNMENT_DEFAULT:
		case XALLOC_ALIGNMENT_16:
			iAlignment = 16;
			break;
		case XALLOC_ALIGNMENT_8:
			iAlignment = 8;
			break;
		case XALLOC_ALIGNMENT_4:
			iAlignment = 4;
			break;
		default:
			Assert(0);
		}
	}

	if (iAllocId == eXALLOCAllocatorId_XHV)
	{
		USE_MEMBUCKET(MEMBUCKET_NETWORK);

		CNetwork::UseNetworkHeap(NET_MEM_VOICE);
		sysMemAllocator* pNetworkHeap = CNetwork::GetNetworkHeap();
		Assert(pNetworkHeap);
		if (pNetworkHeap){
			pBuffer = pNetworkHeap->Allocate(dwSize, iAlignment);

			//check the zero initialize flag
			if	(pAttributes->dwZeroInitialize)
			{
				memset(pBuffer, 0, dwSize);
			}
		}
		CNetwork::StopUsingNetworkHeap();
	}
	else if	(iAllocId == eXALLOCAllocatorId_XAUDIO2)
	{
		if	(bIsPhysical)
		{
			pBuffer = audDriver::AllocatePhysical(dwSize, iAlignment);
		}
		else
		{
			pBuffer = audDriver::AllocateVirtual(dwSize, iAlignment);
		}

		//check the zero initialize flag
		if	(pAttributes->dwZeroInitialize)
		{
			memset(pBuffer, 0, dwSize);
		}
	}
	else if (iAllocId == eXALLOCAllocatorId_XGRAPHICS)
	{
		if (bIsPhysical)
		{
			pBuffer = XMemAllocDefault(dwSize, dwAllocAttributes);

#if __XENON && __ASSERT
			MEMORYSTATUS ms;
			GlobalMemoryStatus(&ms);
			size_t remain = ms.dwAvailPhys >> 10;
			Assertf(remain > 100, "Free title memory below safe value (100k) : %dk", remain);		// e.g. MP voice chat requires 80k of title free mem...
#endif //__XENON && __ASSERT

#if RAGE_TRACKING
			if (pBuffer && ::rage::diagTracker::GetCurrent() && sysMemVisualize::GetInstance().HasXTL())
				::rage::diagTracker::GetCurrent()->Tally(pBuffer, dwSize, 0);
#endif
		}
		else
		{

			if(XGraphicsExtMemBufferHelper::ms_pBuffer[XGRAPHICS_MB_USER_PEDHEADSHOTMGR] && Verifyf(XGraphicsExtMemBufferHelper::ms_bufferSize[XGRAPHICS_MB_USER_PEDHEADSHOTMGR] >= dwSize, "PedHeadshotTextureBuilder: Preallocated XGraphics memory is not enough!"))
			{
				pBuffer = XGraphicsExtMemBufferHelper::ms_pBuffer[XGRAPHICS_MB_USER_PEDHEADSHOTMGR];
			}
			else
			{
				streamDebugf1("Virtual XGraphics allocation, size=%d, alignment=%d", dwSize, iAlignment);
				MEM_USE_USERDATA(MEMUSERDATA_XGRAPHICS);
				pBuffer = strStreamingEngine::GetAllocator().Allocate(dwSize, iAlignment, MEMTYPE_RESOURCE_VIRTUAL);
				streamDebugf1("Virtual XG pointer: %p", pBuffer);
			}

			//check the zero initialize flag
			if	(pAttributes->dwZeroInitialize)
			{
				memset(pBuffer, 0, dwSize);
			}
		}
	}
	else
	{
		if (bIsPhysical && iAlignment == 32)
		{
			pBuffer = PrivatePhysicalAlloc(dwSize, dwAllocAttributes);
			if (pBuffer && pAttributes->dwZeroInitialize)
				memset(pBuffer, 0, dwSize);
		}
		else
		{
			pBuffer = XMemAllocDefault(dwSize, dwAllocAttributes);

#if __XENON && __ASSERT
			MEMORYSTATUS ms;
			GlobalMemoryStatus(&ms);
			size_t remain = ms.dwAvailPhys >> 10;
			Assertf(remain > 100, "Free title memory below safe value (100k) : %dk", remain);		// e.g. MP voice chat requires 80k of title free mem...
#endif //__XENON && __ASSERT
		}

#if RAGE_TRACKING
		if (pBuffer && ::rage::diagTracker::GetCurrent() && sysMemVisualize::GetInstance().HasXTL())
			::rage::diagTracker::GetCurrent()->Tally(pBuffer, dwSize, 0);
#endif
	}

#if MEMORY_TRACKER
	if (pBuffer)
	{
		sysMemManager::GetInstance().GetSystemTracker()->Allocate(XMemSize(pBuffer, dwAllocAttributes), iAllocId);
	}
#endif

	return(pBuffer);
}

//////////////////////////////////////////////////////////////////////////
//
void WINAPI	 XMemFreeCustom(void *pAddress, unsigned long dwAllocAttributes)
{
	XALLOC_ATTRIBUTES *pAttributes = (XALLOC_ATTRIBUTES *)&dwAllocAttributes;
	const bool	bIsPhysical = XALLOC_IS_PHYSICAL(dwAllocAttributes);

	if	(pAddress)
	{
		int	iAllocId = pAttributes->dwAllocatorId;

#if MEMORY_TRACKER
		const size_t size = XMemSize(pAddress, dwAllocAttributes);
		sysMemManager::GetInstance().GetSystemTracker()->Free(size, iAllocId);
#endif
		if (iAllocId == eXALLOCAllocatorId_XHV)
		{
			sysMemAllocator* pNetworkHeap = CNetwork::GetNetworkHeap();
			Assert(pNetworkHeap);
			if (pNetworkHeap){
				pNetworkHeap->Free(pAddress);
			}
		}
		else if	(iAllocId == eXALLOCAllocatorId_XAUDIO2)
		{
			if	(bIsPhysical)
			{
				audDriver::FreePhysical(pAddress);
			}
			else
			{
				audDriver::FreeVirtual(pAddress);
			}
		}
		else if (iAllocId == eXALLOCAllocatorId_XGRAPHICS)
		{
			streamDebugf1("XGraphics free: %p", pAddress);
			if (bIsPhysical)
			{
#if RAGE_TRACKING
				if (::rage::diagTracker::GetCurrent() && sysMemVisualize::GetInstance().HasXTL())
					::rage::diagTracker::GetCurrent()->UnTally(pAddress, size);
#endif
				XMemFreeDefault(pAddress, dwAllocAttributes);
			}
			else if (pAddress != XGraphicsExtMemBufferHelper::ms_pBuffer[XGRAPHICS_MB_USER_PEDHEADSHOTMGR])
			{
				MEM_USE_USERDATA(MEMUSERDATA_XGRAPHICS);
				streamDebugf1("XG free is from the streaming allocator");
				strStreamingEngine::GetAllocator().Free(pAddress);
			}
			else
			{
				// This is a temporary block to debug what's going on.
				if (pAddress == XGraphicsExtMemBufferHelper::ms_pBuffer[XGRAPHICS_MB_USER_MESHBLENDMGR])
				{
					streamDebugf1("Blocked because of the mesh blender");
				}
				else
				{
					streamDebugf1("Blocked because of the ped head shot texture builder");
				}
			}
		}
		else
		{
#if RAGE_TRACKING
			if (::rage::diagTracker::GetCurrent() && sysMemVisualize::GetInstance().HasXTL())
				::rage::diagTracker::GetCurrent()->UnTally(pAddress, size);
#endif
			if (!bIsPhysical || !PrivatePhysicalFree(pAddress))
				XMemFreeDefault(pAddress, dwAllocAttributes);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
SIZE_T WINAPI XMemSizeCustom(void *pAddress, unsigned long dwAllocAttributes)
{
	XALLOC_ATTRIBUTES *pAttributes = (XALLOC_ATTRIBUTES *)&dwAllocAttributes;

	int			iAllocId = pAttributes->dwAllocatorId;
	const bool	bIsPhysical = XALLOC_IS_PHYSICAL(dwAllocAttributes);
	size_t		iSize = 0;

	if (iAllocId == eXALLOCAllocatorId_XHV)
	{
		sysMemAllocator* pAllocator = CNetwork::GetNetworkHeap();
		iSize = pAllocator->GetSizeWithOverhead(pAddress);
	}
	else if	(iAllocId == eXALLOCAllocatorId_XAUDIO2)
	{
		if	(bIsPhysical)
		{
			iSize = audDriver::GetPhysicalSize(pAddress);
		}
		else
		{
			iSize = audDriver::GetVirtualSize(pAddress);
		}
	}
	else if (iAllocId == eXALLOCAllocatorId_XGRAPHICS)
	{
		if (bIsPhysical)
		{
			iSize = PrivatePhysicalSize(pAddress);
		}
		else if (pAddress != XGraphicsExtMemBufferHelper::ms_pBuffer[XGRAPHICS_MB_USER_PEDHEADSHOTMGR])
		{
			iSize = strStreamingEngine::GetAllocator().GetSizeWithOverhead(pAddress);
		}
	}
	else
	{
		if (bIsPhysical)
			iSize = PrivatePhysicalSize(pAddress);
		if (!iSize)
			iSize = XMemSizeDefault(pAddress, dwAllocAttributes);
	}

	return(iSize);
}


#endif // __XENON