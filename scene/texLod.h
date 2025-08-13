//
// Filename:	texLod.h
// Description:	handles streaming & swapping of top mip out of texture dicts
// Written by:	JohnW
//
//	5/10/2010	-	JohnW:	- initial;
//
//
//
#ifndef __TEXLOD_H__
#define __TEXLOD_H__

// RAGE includes
#include "atl/array.h"
#include "atl/map.h"
#include "paging/ref.h"

// fw includes
#include "fwtl/LinkList.h"
#include "fwscene/stores/txdstore.h"
#include "fwscene/texLod.h"

// game includes
#include "modelinfo/modelinfo.h"

namespace rage {
template<class T> class pgDictionary;
class grcTexture;
class bkBank;
typedef pgDictionary<grcTexture> fwTxd;
};

class CMipSwitcher;
class CBaseModelInfo;
class CUiGadgetWindow;
class CUiGadgetList;
class CUiGadgetText;

#define NG_HD_SPLIT								(RSG_ORBIS || RSG_DURANGO)
#if NG_HD_SPLIT
#define NG_HD_SPLIT_ONLY(...)  			__VA_ARGS__
#else
#define NG_HD_SPLIT_ONLY(...)
#endif // NG_HD_SPLIT	


// HDAssetManager stuff ---

class CHDAssetManager{
public:
	void Init(void);
	void Shutdown(void);

	void AddCutsceneRequestForHD(atHashString nameHash);
	void RemoveCutsceneRequestForHD(atHashString nameHash);

	bool AreCutsceneRequestsLoaded(void);
	void FlushCutsceneRequestsHD(void);

	bool	m_enableDefaultHDRequests;

	const atArray<CBaseModelInfo*, 32>&		GetCutsceneRequests() { return(m_cutsceneRequests); }

#if __BANK
	void DebugUpdate(void);

	bool	m_displayHDRequests;
#endif //__BANK

private:

	atArray<CBaseModelInfo*, 32>			m_cutsceneRequests;
};

extern CHDAssetManager		g_HDAssetMgr;

// --- classes for maintaining the association between original asset and HD txd



// --- classes for doing the texture switching ----
// class to handle switching texture with one set of mips for a different texture
class CMipSwitcher
{
public:
	CMipSwitcher(){ m_pLow = NULL; m_pHigh = NULL; m_bIsSwapped = false;}
	virtual ~CMipSwitcher(void) {}

	void Set(grcTexture* pLow, grcTexture* pHigh NG_HD_SPLIT_ONLY(, strLocalIndex hdTxdIdx, fwAssetLocation targetAsset));
	void Clear(NG_HD_SPLIT_ONLY(strLocalIndex hdTxdIdx, fwAssetLocation targetAsset));
	void SetSwapState(bool bSwap NG_HD_SPLIT_ONLY(, strLocalIndex hdTxdIdx, fwAssetLocation targetAsset));

	bool IsValid() { return(m_pLow != NULL); }

	grcTexture *GetHighTex() { return m_pHigh; }
	grcTexture *GetLowTex() { return m_pLow; }
private:

	pgRef<grcTexture> m_pLow;
	pgRef<grcTexture> m_pHigh;

#if __PS3 || __XENON
	u32 m_mipMap;
	u32 m_offset;
	u32 m_mipOffset;
	u32 m_width;
	u32 m_height;
#endif

#if NG_HD_SPLIT
    void* m_pOldHdAddr;
#endif

	bool		m_bIsSwapped;
};

#define COOLDOWN_TIME		(2000)			//in milliseconds

// struct to hold data to track when switching textures in one txd with another txd
class CTxdPairBinding{

public:
	CTxdPairBinding() { m_highTxdIdx = -1; m_cooldownTimer = 0;  m_triggeringMI = fwModelId::MI_INVALID;}
	~CTxdPairBinding(void);

	void ActivateSwap(fwAssetLocation target);
	void SetHighTxdIdx(s32 highTxdIdx);

	bool IsAlreadyBound(fwAssetLocation target) { return(target == m_targetAsset); }

	void ResetCooldownTimer(void) { m_cooldownTimer = COOLDOWN_TIME; }
	void SetTriggeringModelIndex(u32 triggeringMI) { m_triggeringMI = triggeringMI; }
	void Update(void);
	bool IsTimedOut(void) { return m_cooldownTimer <= 0; }

	void SetSwapStateAll(bool bSwap);

	const strLocalIndex GetHighTxdIdx(void) { return(m_highTxdIdx); }
	const strLocalIndex GetTriggeringMI(void) { return(m_triggeringMI); }

	bool IsTexIndexInTxdBound(u32 idx) { if (m_boundTexSwitchers.GetCount() > (int)idx && m_boundTexSwitchers[idx].IsValid()) return(true); else return(false);}

	u32 GetTimeoutVal() { return m_cooldownTimer > 0 ? m_cooldownTimer / (COOLDOWN_TIME/5) : 0; }
	u32 GetTextureMemUsage();
	fwAssetLocation &GetTargetAsset() { return m_targetAsset; }
	atArray<CMipSwitcher>& GetBoundTextureSwitchers() { return m_boundTexSwitchers; } 
private:
	atArray<CMipSwitcher>	m_boundTexSwitchers;

	strLocalIndex			m_highTxdIdx;
	fwAssetLocation			m_targetAsset;
	s32						m_cooldownTimer;	// Timer to determine when to kill the remap & return to base txd state
	strLocalIndex			m_triggeringMI;
};

class CTexLodInterface : public fwTexLodInterface
{
public:
	virtual void SetSwapStateSingle(bool bSwap, fwAssetLocation targetAsset);
};

#define MAX_EXTERNAL_HD_TXD_REQUESTS	(32)

class CExternalRequest
{
public:
	CExternalRequest() {}
	CExternalRequest(fwAssetLocation loc, u32 mi) { m_assetLoc = loc; m_modelIndex = mi; }
	~CExternalRequest() {}

	inline bool operator==(const CExternalRequest& other) const { return((other.m_assetLoc == m_assetLoc) && (other.m_modelIndex == m_modelIndex)); }

	fwAssetLocation	m_assetLoc;
	u32				m_modelIndex;
};

//
//
//
//
class CTexLod : public fwTexLod
{
public:
	static void Init();
	static void InitSession(unsigned initMode);
	static void ShutdownSession(u32 shutdownMode);
	static void Shutdown();
	static void InitDebugData();

	static void Update();

 	static void UnbindAllBoundTxds(void);

	static void SetSwapStateSingle(bool bSwap, fwAssetLocation targetAsset);
	static void SetSwapStateAll(bool bSwap, bool bDefragOnly);

	static void EnableStateSwapper(bool bEnable) { ms_bEnableSwapping = bEnable; }

#if __BANK
	static void	AddWidgets(bkBank* bank);
	static u32	GetDebugTriggerMI(void) { return(ms_debugTriggerMI.Get()); }
	static bool IsTxdUpgradedToHD(const fwTxd *pfwTxd);
	static bool IsTextureUpgradedToHD(const grcTexture *pTexture);
	static bool IsModelUpgradedToHD(const u32 modelIndex, bool bBaseAssetOnly = false);
	static bool IsModelHDTxdCapable(const u32 modelIndex, bool bBaseAssetOnly = false);
	static bool IsEntityCloseEnoughForHDSwitch(const CEntity* pEntity);
#endif //__BANK

	static void StoreHDMapping(eStoreType type, u32 assetSlot, s32 txdHDSlot);
	static bool HasAssetHDMapping(eStoreType type, u32 assetSlot) { return(ms_assetToHDMappings.Access(fwAssetLocation(type, assetSlot)) != NULL); }
	static strLocalIndex GetHDTxdSlotFromMapping(CBaseModelInfo* pModelInfo)
	{
		fwAssetLocation assetLoc = pModelInfo->GetAssetLocation();
		s32* pTxdSlotHigh = ms_assetToHDMappings.Access(assetLoc);
		if (pTxdSlotHigh)
		{
			return strLocalIndex(*pTxdSlotHigh);
		}
		return strLocalIndex(-1);
	}

	// external requests
	static void	AddHDTxdRequest(fwAssetLocation targetAsset, u32 MI)			
	{ 
		CExternalRequest req(targetAsset, MI);
		if (ms_externalRequests.Find(req) == -1 && !ms_externalRequests.IsFull())
		{
			//if (ms_assetToHDMappings.Access(targetAsset) != NULL)
			{
				ms_externalRequests.Push(req); 
			}
		}
	}

	static bool TriggerHdForEntity(CEntity* pEntity, void* pData);

	static void FlushAllUpgrades();

	static void SetHdArea(float x, float y, float z, float radius) { ms_hdArea.Set(Vec3V(x, y, z), ScalarV(rage::Min(radius, 30.f))); ms_bHasHdArea = true; };
	static void ClearHdArea() { ms_bHasHdArea = false; }

#if NG_HD_SPLIT
	static void AddDeferredFreePtr(void* ptr, size_t size, strLocalIndex hdTxdIdx, fwAssetLocation targetAsset);
#endif // NG_HD_SPLIT
	static void FlushMemRemaps();
	static bool AllowAmbientRequests(void) { return ms_allowAmbientRequests; }
	//must be called each frame
	static void DisableAmbientRequests(void) {ms_bAmbientRequestsDisabled = true;}

private:
#if __BANK
	static grcTexture*	m_TexTab[4];

	static fwTxd*		m_pHighTxd;
	static fwTxd*		m_pHigh2Txd;
	static fwTxd*		m_pLowTxd;
	static u32			m_totalMemoryUsed;
	static bool			m_showHDTextureMemUsage;
	static bool			m_showHDMemUsage;
	static bool			m_onlyExternalRequsts;
#endif //__BANK

	static bool			ms_bEnabled;
	static bool			ms_bEnableSwapping;
	static bool			ms_bAmbientRequestsDisabled;

	static bool			ms_bHasHdArea;
	static spdSphere	ms_hdArea;
	static u32			ms_nextHdAreaUpdate;
	static bool			ms_allowAmbientRequests;

	static void UpdateInternal();
	static grcTexture*	LoadTexture(fwTxd* txd, char *texname);
	static void TriggerHDTxdUpgrade(fwAssetLocation targetAsset, u32 triggeringMI, bool onlyIfLoaded = false);
	static bool AreFromSameArchive(fwAssetLocation targetAsset, s32* pTxdSlotHigh);

private:
	static atMap<s32, CTxdPairBinding*>				ms_boundTxds;
	static atMap<fwAssetLocation, s32>				ms_assetToHDMappings;

	static atFixedArray<CExternalRequest, MAX_EXTERNAL_HD_TXD_REQUESTS>		ms_externalRequests;

#if NG_HD_SPLIT
	struct sVirtMemData
	{
		void* ptr;
		size_t size;
		strLocalIndex hdTxdIdx;
		fwAssetLocation targetAsset;
		s8 freeCountdown;

		void Release();
	};
	static atArray<sVirtMemData> ms_virtMemToFree;
#endif // NG_HD_SPLIT

public:
#if __BANK
	static void KillTexLod(void) { ms_bEnabled = false; }

	static void DebugUpdate(void);
	static void UnbindAllCB(void);

	static void ForceHighBindingCB(void);
	static void ForceHighUnbindingCB(void);
	static void ChopTopOffOfLowCB(void);

	static void UpdateCellForAsset(CUiGadgetText* pResult, u32 row, u32 col, void * extraCallbackData );
	static void UpdateCellForTxd(CUiGadgetText* pResult, u32 row, u32 col, void * extraCallbackData );

	// update txd view callbacks
	static void ShowTargetAssetInTexViewCB(void);
	static void ShowTargetHDTxdInTexViewCB(void);

	inline static void Debug_RegisterEntityDistance(CEntity* pEntity, float dist)	{ if (pEntity && (pEntity == ms_pSelectedEntity)) { ms_bIsActivated = false; ms_selectedEntityDist = dist; } }
	inline static void Debug_RegisterEntityActivations(CEntity* pEntity)			{ if (pEntity && (pEntity == ms_pSelectedEntity)) { ms_bIsActivated = true; } }

	static void HDActivationDistanceCB(void);
	static void DisableHDTexCB(void);

	static bool AssetHasRequest(fwAssetLocation targetAsset);

	static CUiGadgetWindow*	ms_pDebugWindow;
	static CUiGadgetList*	ms_pHDTxdList;
	static CUiGadgetList*	ms_pTexList;
	static fwAssetLocation	ms_currentAsset;

	static strLocalIndex	ms_debugTxdIdx;
	static strLocalIndex	ms_debugTriggerMI;

	static float			ms_selectedEntityDist;
	static bool				ms_bIsActivated;
	static bool				ms_bDisableHDTex;
	static float			ms_HDActivationDistance;
	static CEntity*			ms_pSelectedEntity;

#else //__BANK
	static void	Debug_RegisterEntityDistance(CEntity*, float) {}
	static void Debug_RegisterEntityActivations(CEntity*) {}
#endif //__BANK
};

#endif //__TEXLOD_H__
