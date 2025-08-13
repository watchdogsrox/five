// Title	:	compEntity.h
// Author	:	John Whyte
// Started	:	7/06/2010

#ifndef _COMP_ENTITY_H_
#define _COMP_ENTITY_H_

#include "crclip/clip.h"
#include "fwanimation/animdirector.h"
#include "fwtl/pool.h"
#include "scene/Entity.h"
#include "scene/RegdRefTypes.h"
#include "streaming/streamingrequest.h"

#include "control/replay/replay.h"


// process all composites in the space of this number of frames
#define COMPENTITY_UPDATE_NUM_FRAMES_TO_PROCESS	(4)

enum eCompEntityState {
	CE_STATE_INVALID		=	-1,
	CE_STATE_INIT			=	0,
	CE_STATE_SYNC_STARTING  =   1,
	CE_STATE_STARTING		=	2,
	CE_STATE_START			=	3,
	CE_STATE_PRIMING		=	4,
	CE_STATE_PRIMED			=	5,
	CE_STATE_START_ANIM		=	6,
	CE_STATE_ANIMATING		=	7,
	CE_STATE_SYNC_ENDING	=	8,
	CE_STATE_ENDING			=	9,
	CE_STATE_END			=	10,
	CE_STATE_RESET			=	11,
	CE_STATE_PAUSED			=	12,
	CE_STATE_RESUME			=	13,
	CE_STATE_PRIMING_PRELUDE =	14,
	CE_STATE_ABANDON		=	15,

#if GTA_REPLAY
	CE_STATE_AWAIT_PRELOAD	=	16,			// A state to await completion of pre-loading
	CE_STATE_PRELOADED		=	17,			// A holding state, we know it's finished pre-loading if we get here
#endif	//GTA_REPLAY

};

enum eCompEntityAssetSet {
	CE_START_ASSETS			= 0,
	CE_ANIM_ASSETS			= 1,
	CE_PTFX_ASSETS			= 2,
	CE_END_ASSETS			= 3,
	CE_NONE					= 4,
};

// forward declarations
class CCompEntityEffectsData;

//
// name:		CCompEntity
// description:	Describes a composite entity
class CCompEntity : public CEntity
{
public:
	CCompEntity(const eEntityOwnedBy ownedBy);
	~CCompEntity();

	static void Shutdown(unsigned shutdownMode);

	PPUVIRTUAL bool CreateDrawable();
	PPUVIRTUAL void DeleteDrawable();

	PPUVIRTUAL void SetModelId(fwModelId modelId);

	static void Update();

	static void UpdateCompEntitiesUsingGroup(s32 groupIdx, eCompEntityState targetState);

	void TriggerAnim(void);
	
	eCompEntityState GetState(void) { return(m_currentState); }

	void SetToStarting(void);					// request beginning model and display it once loaded
	void SetToEnding(void);						// request the end model and display it once loaded
	void SetToPriming(void);					// request the anim model and dictionary
	void SetToInit(void);						// move to initial state
	void SetToAbandon(void);					// abandon current requests
	void Reset(void);							// flush any requests or loaded assets, set back to starting
	void PauseAnimAt(float targetPhase);		// pause a rayfire whilst playing anim when it hits the target phase
	void SetAnimTo(float targetPhase);			// set the rayfire anim to the target phase and pause it
	void ResumeAnim(void);						// resume playing of anim from a paused state

	void ImmediateUpdate(void);				// available as a public member for the NavMeshGenerator tool.

	static bool	IsAllowedToDelete() { return(ms_bAllowedToDelete); }
	bool GetIsActive() { return(m_bIsActive); }
	void SetIsActive(bool bSet) { m_bIsActive = bSet; }

	bool GetIsCutsceneDriven() { return(m_bCutsceneDriven); }
	void SetIsCutsceneDriven(bool bSet) { m_bCutsceneDriven = bSet; }

	CObject* GetPrimaryAnimatedObject() { return(GetChild(0)); }

	inline CCompEntityModelInfo* GetCompEntityModelInfo() const {return (CCompEntityModelInfo*)GetBaseModelInfo();}	

#if __BANK
	static void AddWidgets(bkBank* pBank);
	static void ForceAllCB(void);

	static CCompEntity* SearchForOwner(fwEntity* pEntity);

	static void DebugRender(void);

	void GenerateDebugInfo(char* pString);

	void SetModifiedBy(const char* pString);
	atString& GetModifiedBy()		{ return(m_modifiedBy); }

#else //__BANK
	void	SetModifiedBy(const char*) {}
#endif //__BANK

	CObject*	GetChild(u32 idx) { if (idx < m_ChildObjects.GetCount()) { return(m_ChildObjects[idx]); } else return(NULL);}
	const atFixedArray<RegdObj, 10> & GetChildObjects() const { return m_ChildObjects; }

	FW_REGISTER_CLASS_POOL(CCompEntity);	

private:
	void	IssueAssetRequests(eCompEntityAssetSet desiredAssetSet);
	void	SwitchToAssetSet(eCompEntityAssetSet desiredAssets);
	bool	IsAssetSetReady(eCompEntityAssetSet assetSet);
	void	RemoveCurrentAssetSetExceptFor(eCompEntityAssetSet exceptForAssets);

	void	ReleaseStreamRequests();

	void	CreateNewChildObject(u32 modelHash);
	void	RestoreChildren();

	void	TriggerObjectAnim();
	void	GetObjAnimPhaseInfo(float& currPhase, float& lastPhase);

	void ProcessAnimsVisibility(float currPhase);
	void ProcessEffects(float currPhase, float lastPhase);
	void RegisterEffect(const CCompEntityEffectsData& effectData, CObject* pCurrChild, int effectId);
	void TriggerEffect(const CCompEntityEffectsData& effectData, CObject* pCurrChild, int effectId);

	bool	IsThisAChild(fwEntity* pEntity) { for(u32 i=0;i<m_ChildObjects.GetCount(); i++) {if ((fwEntity*)(m_ChildObjects[i].Get()) == pEntity) return(true);} return(false); }

	void AddExtraVisibiltyDofs(fwAnimDirector& director, const crClip& clip); 

	void ValidateImapGroupSafeToDelete(u32 imapSwapIndex, s32 imapToCheckFor);

	void ComputeEndPhase(); 

#if GTA_REPLAY

public:
	bool	GetDidPreLoadComplete()
	{
		return m_bPreLoadComplete;
	}

	static bool	ShouldForceAnimUpdateWhenPaused()
	{
		return ms_bForceAnimUpdateWhenPaused;
	}

	static void SetForceAnimUpdateWhenPaused()
	{
		ms_bForceAnimUpdateWhenPaused = true;
	}

	static void ResetForceAnimUpdateWhenPaused()
	{
		ms_bForceAnimUpdateWhenPaused = false;
	}

private:

	void SwitchToReplayMode();
	void SwitchToGameplayMode();

	void RemoveUnwanted();
	void AddWanted();

	void RemoveChildObjects();
	void ImapObjectsRemove(s32 index);

	void PreLoadAllAssets();

	u32	 GetUniqueID();

	bool HasPreLoadCompleted();
	void ReleasePreLoadedAssets();

	void CacheCurrentMapState();

#endif

	// new code
	strRequest				m_startRequest;
	strRequest				m_endRequest;
	strRequestArray<20>		m_animRequests;
	strRequest				m_ptfxRequest;

	atFixedArray<RegdObj, 10>	m_ChildObjects;		// all child objects - max possible : 8 animating

	// imap support
	s32 m_startImapIndex;
	s32 m_endImapIndex;

	// imap group swapper
	u32 m_imapSwapIndex;

#if GTA_REPLAY
	int		m_ReplayLoadIndex;
	bool	m_bPreLoadPacketSent;
	bool	m_bPreLoadComplete;
	void	WritePreLoadPacketIfRequired(bool loading);
#endif

	eCompEntityState		m_currentState;

	bool		m_bIsActive : 1;
	bool		m_bCutsceneDriven : 1;

	float		m_pausePhase;
	float		m_targetPausePhase;
	float		m_targetEndPhase; 
#if __BANK
	atString	m_modifiedBy;
#endif //__BANK

#if GTA_REPLAY
	// Stuff explicitly for replay
	bool	m_bLoadAll;						// Flag to determine if we're loading all the assets for the entire replay.
	u32		m_CachedStartMapState;			// A cache of the current map state on entry
	static bool	ms_bForceAnimUpdateWhenPaused;
#endif

	// statics
	static bool ms_bAllowedToDelete;
	static fwPtrListSingleLink	ms_activeEntries;
};

#if __DEV && !__SPU
// forward declare so we don't get multiple definitions
template<> void fwPool<CCompEntity>::PoolFullCallback();
#endif

#endif //_COMP_ENTITY_H_
