#ifndef _ENTITYDRAWHANDLER_H_INCLUDED_
#define _ENTITYDRAWHANDLER_H_INCLUDED_

#include "entity/drawdata.h"
#include "fwdrawlist/drawlist.h"
#include "renderer/DrawLists/drawlist.h"
#include "system/timemgr.h"
#include "system/threadtype.h"

class CBaseModelInfo;
class CEntity;

namespace rage
{
	class bkBank;
}

extern bool g_prototype_batch;

class CDrawListPrototypeManager
{
public:
	static inline IDrawListPrototype* GetPrototype();
	static inline void SetPrototype(IDrawListPrototype * prototype);
	static void Flush(bool forceAllocateNewBuffer = false);

	static inline void AddData(CEntity * entity, CEntityDrawHandler * drawHandler);
	static inline void AllocNewBuffer();


private:
	static IDrawListPrototype *	ms_Prototype;
	static u32				ms_TimeStamp;
	
	// This is the beginning of the current page.
	static u8*				ms_BasePageBuffer;
	
	// This is the "cursor" within our current batch. Our next instance will be written here.
	static u8*				ms_PageBuffer;

	// This is the beginning of the current batch within the page.
	static u8*				ms_PageBufferCurrentStart;

	static u32              ms_PageBufferSize;

#if __ASSERT
	// Timestamp at which the last page was created.
	static u32				ms_PageCreationTimestamp;
#endif // __ASSERT
	
	// This is the beginning of the current batch within the page.
	static DrawListAddress  ms_Address;

	// This is the beginning of the current page.
	static DrawListAddress  ms_BaseAddress;
};

inline IDrawListPrototype* CDrawListPrototypeManager::GetPrototype()
{
	return ms_Prototype;
}
 
inline void CDrawListPrototypeManager::SetPrototype(IDrawListPrototype * prototype)
{
	Assert(sysThreadType::IsUpdateThread());

	if(ms_Prototype)
	{
		if(ms_Prototype != prototype)
		{
			Flush();
			ms_Prototype = prototype;

			if (ms_Prototype)
			{
				// If we are on a new frame alloc a new buffer
				u32 currentTimeStamp = gDCBuffer->GetTimeStamp();
				if (ms_TimeStamp != currentTimeStamp)
				{
					Assertf(ms_PageBuffer == ms_PageBufferCurrentStart, "we should have flushed this out" );
					ms_TimeStamp = currentTimeStamp;
					AllocNewBuffer();
				}
				else if ((ms_PageBuffer + ms_Prototype->SizeOfElement()) > (ms_BasePageBuffer + ms_PageBufferSize))
				{
					AllocNewBuffer();
				}
			}
		}
		else
		{
			Assertf(ms_TimeStamp == gDCBuffer->GetTimeStamp(), "Prototype buffer wasn't flushed out, timestamps %d vs %d, prototype %p", ms_TimeStamp, gDCBuffer->GetTimeStamp(), ms_Prototype);
		}
	}
	else
	{
		ms_Prototype = prototype;

		if (ms_Prototype)
		{
			// If we are on a new frame alloc a new buffer
			u32 currentTimeStamp = gDCBuffer->GetTimeStamp();
			if (ms_TimeStamp != currentTimeStamp)
			{
				Assertf(ms_PageBuffer == ms_PageBufferCurrentStart, "we should have flushed this out" );
				ms_TimeStamp = currentTimeStamp;
				AllocNewBuffer();
			}
			else if ((ms_PageBuffer + ms_Prototype->SizeOfElement()) > (ms_BasePageBuffer + ms_PageBufferSize))
			{
				AllocNewBuffer();
			}

		}
	}

}

inline void CDrawListPrototypeManager::AddData(CEntity * entity, CEntityDrawHandler * drawHandler)
{
	Assert(sysThreadType::IsUpdateThread());

	// Add a data block to our page.
	Assertf(ms_TimeStamp == gDCBuffer->GetTimeStamp(), "Adding data to a stale page, timestamp %d, created at %d (buffer at %p vs %p)", ms_TimeStamp, gDCBuffer->GetTimeStamp(), ms_PageBuffer, ms_PageBufferCurrentStart);

#if __ASSERT
	u8 *expectedNext = ms_PageBuffer + ms_Prototype->SizeOfElement();
#endif // __ASSERT

	register u8* pageBuffer = (u8*) ms_Prototype->AddDataForEntity(entity, drawHandler, ms_PageBuffer);
	ms_PageBuffer = pageBuffer;

	Assert(expectedNext == pageBuffer);

	// if we can't fit another one flush
	if (Unlikely((pageBuffer + ms_Prototype->SizeOfElement()) > (ms_BasePageBuffer + ms_PageBufferSize)))
	{
		Flush(true);
	}
}

inline void CDrawListPrototypeManager::AllocNewBuffer()
{
	Assert(sysThreadType::IsUpdateThread());

	// Let's grab a new block of data from the ring buffer.
	ms_BasePageBuffer = (u8*)gDCBuffer->AddDataBlock(NULL, ms_PageBufferSize, ms_BaseAddress);

	// Reset our cursor to point to the beginning of the page.
	ms_PageBuffer     = ms_BasePageBuffer;
	ms_PageBufferCurrentStart = ms_PageBuffer;

	// Create a draw list address from this too.
	u32 offsetPastAlloc = ms_BaseAddress.GetOffset();
	ms_BaseAddress.SetOffset(offsetPastAlloc + sizeof(dlCmdDataBlock));

	Assertf(ms_BasePageBuffer == (u8*)gDCBuffer->ResolveDrawListAddress(ms_BaseAddress), "Addresses don't match up, got %p, expected %p", ms_BasePageBuffer, (u8*)gDCBuffer->ResolveDrawListAddress(ms_BaseAddress));

	ms_Address = ms_BaseAddress;
#if __ASSERT
	ms_PageCreationTimestamp = gDCBuffer->GetTimeStamp();
#endif // __ASSERT
}


class CDrawDataAddParams : public fwDrawDataAddParams {
public:
	CDrawDataAddParams(bool setupLights = false)
		: m_SetupLights(setupLights)
#if __DEV
		, m_Magic(MAGIC)
#endif // _DEV
	{
	}

	// If true, this is a forward-lighting pass, and we need to set up the closest lights.
	bool m_SetupLights;

#if __DEV
	// A magic value to ensure that we're using a subclass.
	int m_Magic;

	enum {
		MAGIC = 0x500bc1a5
	};
#endif // __DEV
};

class CEntityDrawHandler : public fwDrawData
{
public:
	CEntityDrawHandler(CEntity* pEntity, rmcDrawable* pDrawable);
	virtual ~CEntityDrawHandler();

	fwCustomShaderEffect* ShaderEffectCreateInstance(CBaseModelInfo *pMI, CEntity *pEntity);

#if __BANK
	static bool ShouldSkipEntity(const CEntity* pEntity);
#endif

	dlCmdBase*	AddToDrawList(fwEntity* pEntity, fwDrawDataAddParams* pParams) = 0;

	void	BeforeAddToDrawList(fwEntity *pEntity, u32 modelIndex, u32 renderMode, u32 bucket, u32 bucketMask, fwDrawDataAddParams* pBaseParams, bool bAlwaysSetCSE = false);			// called just before DLC(AddToDrawList, (...))
	void	AfterAddToDrawList(fwEntity *pEntity, u32 renderMode, fwDrawDataAddParams* pBaseParams);											// counterpart for BeforeAddToDrawList()

	bool	IsInActiveList() const			{ return m_pActiveListLink != NULL; }

	static void SetupLightsAndGlobalInInteriorFlag(const CEntity* pEntity, u32 renderMode, u32 bucketMask, bool bSetupLights);
	static void ResetLightOverride(const CEntity* pEntity, u32 renderMode, bool bSetupLights);

protected:
	static dlCmdBase* AddBasicToDrawList(CEntityDrawHandler* pDrawHandler, fwEntity* pEntity, fwDrawDataAddParams* pParams ASSERT_ONLY(, bool bDoInstancedDataCheck = true));
	static dlCmdBase* AddFragmentToDrawList(CEntityDrawHandler* pDrawHandler, fwEntity* pEntity, fwDrawDataAddParams* pParams ASSERT_ONLY(, bool bDoInstancedDataCheck = true));
	static dlCmdBase* AddBendableToDrawList(CEntityDrawHandler* pDrawHandler, fwEntity* pEntity, fwDrawDataAddParams* pParams ASSERT_ONLY(, bool bDoInstancedDataCheck = true));

private:
	void AddEntityToRenderedList(CEntity* pEntity);
	void RemoveEntityFromRenderedList();
};

class CNoDrawingDrawHandler : public CEntityDrawHandler
{
public:
	CNoDrawingDrawHandler(CEntity* pEntity, rmcDrawable* pDrawable) : CEntityDrawHandler(pEntity, pDrawable) {};
	dlCmdBase*		AddToDrawList(fwEntity*, fwDrawDataAddParams*) { return NULL; }
};

class CEntityBasicDrawHandler : public CEntityDrawHandler
{
public:
	CEntityBasicDrawHandler(CEntity* pEntity, rmcDrawable* pDrawable) : CEntityDrawHandler(pEntity, pDrawable) {};
	dlCmdBase*		AddToDrawList(fwEntity* pEntity, fwDrawDataAddParams* pParams) { return CEntityDrawHandler::AddBasicToDrawList(this, pEntity, pParams);}
};

class CEntityFragDrawHandler : public CEntityDrawHandler
{
public:
	CEntityFragDrawHandler(CEntity* pEntity, rmcDrawable* pDrawable) : CEntityDrawHandler(pEntity, pDrawable) {};
	dlCmdBase*		AddToDrawList(fwEntity* pEntity, fwDrawDataAddParams* pParams) { CDrawListPrototypeManager::Flush(); return CEntityDrawHandler::AddFragmentToDrawList(this, pEntity, pParams);}
};

class CEntityBendableDrawHandler : public CEntityDrawHandler
{
public:
	CEntityBendableDrawHandler(CEntity* pEntity, rmcDrawable* pDrawable) : CEntityDrawHandler(pEntity, pDrawable) {};
	dlCmdBase*		AddToDrawList(fwEntity* pEntity, fwDrawDataAddParams* pParams) { CDrawListPrototypeManager::Flush(); return CEntityDrawHandler::AddBendableToDrawList(this, pEntity, pParams); }
};

class CEntityInstancedBasicDrawHandler : public CEntityDrawHandler
{
public:
	CEntityInstancedBasicDrawHandler(CEntity *pEntity, rmcDrawable *pDrawable) : CEntityDrawHandler(pEntity, pDrawable) {}
	dlCmdBase*		AddToDrawList(fwEntity* pEntity, fwDrawDataAddParams* pParams);
};

class CEntityInstancedFragDrawHandler : public CEntityDrawHandler
{
public:
	CEntityInstancedFragDrawHandler(CEntity *pEntity, rmcDrawable *pDrawable) : CEntityDrawHandler(pEntity, pDrawable) {}
	dlCmdBase*		AddToDrawList(fwEntity* pEntity, fwDrawDataAddParams* pParams);
};

class CEntityInstancedBendableDrawHandler : public CEntityDrawHandler
{
public:
	CEntityInstancedBendableDrawHandler(CEntity *pEntity, rmcDrawable *pDrawable) : CEntityDrawHandler(pEntity, pDrawable) {}
	dlCmdBase*		AddToDrawList(fwEntity* pEntity, fwDrawDataAddParams* pParams);
};



#endif