//
// filename:	RenderTargetMgr.h
// description:	
//

#ifndef INC_RENDERTARGETMGR_H_
#define INC_RENDERTARGETMGR_H_

// --- Include Files ------------------------------------------------------------
#include "fwtl/pool.h"

#include "renderer/RenderPhases/RenderPhase.h"
#include "renderer/RenderPhases/RenderPhaseStd.h"
#include "script/script_memory_tracking.h"

// --- Defines ------------------------------------------------------------------

#define NUM_NAMEDRENDERTARGETS			(24)

#define ENABLE_LEGACY_SCRIPTED_RT_SUPPORT 0

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------
namespace rage {
class grcRenderTarget;
class rmcDrawable;
};

class CViewport;

struct NamedRenderTargetBufferedData
{
	rage::grcRenderTarget  *rt;
	rage::grcTextureHandle  tex;
	u32                     id;
};

class CRenderTargetMgr
{
public:
	static void InitClass(unsigned initMode);
	static void ShutdownClass(unsigned shutdownMode);

	enum RenderTargetId
	{
		RTI_PhoneScreen,
		RTI_MainScreen,
		RTI_Scripted,
		RTI_MovieMesh,
		RTI_Count
	};
	
	CRenderTargetMgr()
	{
		phaseListNew[RTI_PhoneScreen] = NULL;
		phaseListNew[RTI_MainScreen] = NULL;
		phaseListNew[RTI_Scripted] = NULL;
		phaseListNew[RTI_MovieMesh] = NULL;
	}

	grcRenderTarget* CreateRenderTarget(rage::u16 poolID, const char* pName, s32 width, s32 height, s32 poolHeap = 0 WIN32PC_ONLY(, grcRenderTarget* originalTarget = NULL));

	void UseRenderTarget(RenderTargetId target);
#if GTA_REPLAY
	void UseRenderTargetForReplay(RenderTargetId target);
#endif

	void SetRenderPhase(RenderTargetId target, CRenderPhase *phase);

	void ClearRenderPhasePointersNew();

	// Named render targets
	
	struct namedRendertarget
	{
		atFinalHashString name;
		u32 owner;
		u32 uniqueId;
		
		u32 modelidx;
		grcEffectVar diffuseTexVar;
		int shaderIdx;
		
		grcTextureHandle texture;
		grcRenderTarget *rendertarget;
		
		union {
			s32 fragIdx;
			s32 drawIdx;
			s32 drawDictIdx;
		};
		u8 drawDictDrawableIdx;
		u8	drawableType;

#if RSG_PC
		int release_count;
#endif
		bool release;
		bool delayedLoad;
		bool delayedLoadReady;
		bool delayedLoadSetup;
		bool isLinked;
		bool drawableRefAdded;

		bool CreateRenderTarget(grcTexture *texture); 

#if __SCRIPT_MEM_CALC
		u32 GetMemoryUsage() const;
#endif	//	__SCRIPT_MEM_CALC
	};


#if GTA_REPLAY
	const atFixedArray<namedRendertarget,NUM_NAMEDRENDERTARGETS> &GetRenderTargetList() { return namedRendertargetList; }
#endif

	
	bool RegisterNamedRendertarget(const char *name, unsigned int ownerTag);
    bool RegisterNamedRendertarget(const atFinalHashString& RtName, u32 ownerTag, u32 uniqueId);
	bool ReleaseNamedRendertarget(const char *name, unsigned int ownerTag);
	bool ReleaseNamedRendertarget(atFinalHashString hashName, unsigned int ownerTag);
	bool ReleaseNamedRendertarget(u32 nameHashValue, unsigned int ownerTag);

	const namedRendertarget* LinkNamedRendertargets(u32 modelidx, unsigned int ownerTag);
    void LinkNamedRendertargets(const atFinalHashString& name, unsigned int ownerTag, grcTextureHandle texture, u32 modelIdx, fwArchetype::DrawableType assetType, s32 assetIdx);
	
	bool SetNamedRendertargetDelayLoad(const atFinalHashString &name, unsigned int ownerTag, bool loadType);
	bool SetupNamedRendertargetDelayLoad(const atFinalHashString &name, unsigned int ownerTag, grcTexture *texture);
	
	namedRendertarget *GetNamedRendertarget(const atFinalHashString &name, unsigned int ownerTag);
	namedRendertarget *GetNamedRendertarget(u32 nameHashValue, unsigned int ownerTag);
	const namedRendertarget *GetNamedRendertarget(const u32	nameHashValue);
	const namedRendertarget *GetNamedRenderTargetFromUniqueID(u32 uniqueID);

	unsigned int GetNamedRendertargetId(const char *name, unsigned int ownerTag) const;
	unsigned int GetNamedRendertargetId(const char *name, unsigned int ownerTag, u32 modelIdx) const;

	bool IsNamedRenderTargetLinked(u32 modelidx, unsigned int ownerTag);
	bool IsNamedRenderTargetLinkedConst(u32 modelidx, unsigned int ownerTag) const;

#if !__FINAL
	bool IsNamedRenderTargetValid(const char *name);
#endif

	int GetNamedRendertargetsCount() const { return namedRendertargetList.GetCount(); }
	void StoreNamedRendertargets(NamedRenderTargetBufferedData *out);

	void CleanupNamedRenderTargets();
	void Update();
	void ReleaseAllTargets();

#if __SCRIPT_MEM_CALC
	u32 GetMemoryUsageForNamedRenderTargets(unsigned int ownerTag) const;
#endif	//	__SCRIPT_MEM_CALC

	static u32 GetRenderTargetUniqueId(const char *name, unsigned int ownerTag);

private:

	rmcDrawable* GetDrawable(const namedRendertarget* pRt);
	void AddRefToDrawable(const namedRendertarget* pRt)	{ ModifyDrawableRefCount(pRt, true); };
	void RemoveRefFromDrawable(const namedRendertarget* pRt) { ModifyDrawableRefCount(pRt, false); };
	void ModifyDrawableRefCount(const namedRendertarget* pRt, bool bAddRef);

	atFixedArray<namedRendertarget,NUM_NAMEDRENDERTARGETS> namedRendertargetList;
	
	CRenderPhase *phaseListNew[RTI_Count];
};


__forceinline void CRenderTargetMgr::UseRenderTarget(RenderTargetId target)
{
	switch(target)
	{
		case RTI_PhoneScreen:
#if ENABLE_LEGACY_SCRIPTED_RT_SUPPORT
		case RTI_Scripted:
#endif // ENABLE_LEGACY_SCRIPTED_RT_SUPPORT		
			if (phaseListNew[target])
			{
				phaseListNew[target]->Enable();
#if RSG_PC
				phaseListNew[target]->SetMultiGPUDrawListCounter(GRCDEVICE.GetGPUCount(true));
#endif //RSG_PC
			}
			break;
		case RTI_MovieMesh:
			if (phaseListNew[RTI_MovieMesh])
			{
				phaseListNew[RTI_MovieMesh]->Enable();
			}
		case RTI_MainScreen:
			/* NoOp : This one is always enabled */
			break;
		default:
			if (phaseListNew[RTI_Scripted])
			{
				phaseListNew[RTI_Scripted]->Enable();
			}
			break;
	}
}

#if GTA_REPLAY
__forceinline void CRenderTargetMgr::UseRenderTargetForReplay(RenderTargetId target)
{
	if (target == RTI_PhoneScreen)
	{
		CRenderPhasePhoneScreen* phase = static_cast<CRenderPhasePhoneScreen*>(phaseListNew[target]);
		phase->EnableForReplay();
	}
	else
	{
		UseRenderTarget(target);
	}
}
#endif

__forceinline void CRenderTargetMgr::SetRenderPhase(RenderTargetId target, CRenderPhase *phaseNew)
{
	Assert(target < RTI_Count);
	Assert(!phaseNew || phaseListNew[target] == NULL);
	phaseListNew[target] = phaseNew;
}



extern CRenderTargetMgr gRenderTargetMgr;

// --- Globals ------------------------------------------------------------------

#endif // !INC_RENDERTARGETMGR_H_
