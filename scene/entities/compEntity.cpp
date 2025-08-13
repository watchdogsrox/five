// Title	:	compEntity.cpp
// Author	:	John Whyte
// Started	:	7/06/2010


// framework includes
#include "fwanimation/animmanager.h"
#include "fwanimation/directorcomponentcreature.h"
#include "fwsys/gameskeleton.h"
#include "vfx/ptfx/ptfxmanager.h"
#include "vfx/ptfx/ptfxscript.h"
#include "fwutil/PtrList.h"
#include "streaming/streamingvisualize.h"

// game includes
#include "animation/MoveObject.h"
#include "camera/CamInterface.h"
#include "debug/GtaPicker.h"
#include "diag/art_channel.h"
#include "scene/entities/compEntity.h"
#include "scene/loader/maptypes.h"
#include "scene/scene_channel.h"
#include "scene/world/GameWorld.h"
#include "script/script.h"
#include "script/script_brains.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "modelInfo/CompEntityModelInfo.h"
#include "objects/object.h"
#include "Objects/DummyObject.h"
#include "pathserver/ExportCollision.h"
#include "Vfx/Systems/VfxEntity.h"
#include "Vfx/VfxHelper.h"
#include "vfx/misc/LODLightManager.h"
#include "fwdebug/picker.h"

#include "control/replay/misc/ReplayRayFirePacket.h"
#include "control/replay/ReplayRayFireManager.h"
#include "control/replay/ReplayIPLManager.h"

FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CCompEntity, CONFIGURED_FROM_FILE, 0.69f, atHashString("CompEntity",0x72fb71da));

bool CCompEntity::ms_bAllowedToDelete = false;
fwPtrListSingleLink	CCompEntity::ms_activeEntries;

#if __BANK
CCompEntity* g_debugCompEntity = NULL;
int	g_CompEntityDebugEvent = 0;
float g_activationRadius = 10.0f;
bool g_pauseCompEntityAnim = false;
float g_compEntityAnimDislayPhase = 0.0f;
float g_compEntityAnimPausePhase = 0.0f;
s32 g_selectedAnimModel = -1;
bool g_bDisplayCompEntityStates = false;
bool g_bDisplayMemUsage = false;
#if GTA_REPLAY
bool g_bLoadAll = false;
#endif	//GTA_REPLAY
extern RegdEnt gpDisplayObject;
#endif //__BANK

#if GTA_REPLAY
bool CCompEntity::ms_bForceAnimUpdateWhenPaused = false;
#endif	//GTA_REPLAY

// Greetings wary traveller!
// Be very careful if you intend to extend the states handled by this system.
// I would recommend completely rewriting it so that each transition from state to state is handled _explicitly_ with code
// There are unpleasant interactions between compEntity, imapGroupMgr and scripts at work here. JW.

// name:		CCompEntity::CCompEntity
// description:	Constructor for a composite entity
CCompEntity::CCompEntity(const eEntityOwnedBy ownedBy)
	: CEntity( ownedBy )
{
	SetTypeComposite();
	//SetUsesCollision(true);
	SetBaseFlag(fwEntity::IS_FIXED);

	m_currentState = CE_STATE_INIT;

	m_bIsActive = false;

	m_bCutsceneDriven = false;

	m_startImapIndex = -1;
	m_endImapIndex = -1;

	m_imapSwapIndex = fwImapGroupMgr::INVALID_INDEX;

	m_targetPausePhase = 0.0f;
	m_pausePhase = 0.0f;
	m_targetEndPhase = 0.99f;

#if __BANK
	m_modifiedBy = "None";
#endif //__BANK

#if GTA_REPLAY
	m_bLoadAll = (CReplayMgr::GetMode() == REPLAYMODE_EDIT);
	m_ReplayLoadIndex = -1;				// Not requested yet
	m_bPreLoadComplete = false;			// Pre Load complete flag
	m_bPreLoadPacketSent = false;		// For WritePreLoadPacketIfRequired()
	m_CachedStartMapState = CE_STATE_INIT;
#endif

	//TellAudioAboutAudioEffectsAdd();			// Tells the audio an entity has been created that may have audio effects on it.
}

void CCompEntity::SetModelId(fwModelId modelId){
	CEntity::SetModelId(modelId);

	CCompEntityModelInfo* pCEMI = GetCompEntityModelInfo();
	Assert(pCEMI);

	if (pCEMI->GetUseImapForStartAndEnd()){
		strLocalIndex startImapIndex = strLocalIndex(fwMapDataStore::GetStore().FindSlotFromHashKey(pCEMI->GetStartImapFile().GetHash()));
		Assertf((startImapIndex != -1), "Not found : %s is not an .imap group (in rayfire %s)", pCEMI->GetStartImapFile().GetCStr(), pCEMI->GetModelName());
		if (startImapIndex != -1){
			Assertf(fwMapDataStore::GetStore().IsScriptManaged(startImapIndex), "%s is not an .imap group (in rayfire %s)", fwMapDataStore::GetStore().GetName(startImapIndex), pCEMI->GetModelName());
			m_startImapIndex = startImapIndex.Get();
		}

		strLocalIndex endImapIndex = strLocalIndex(fwMapDataStore::GetStore().FindSlotFromHashKey(pCEMI->GetEndImapFile().GetHash()));
		Assertf((endImapIndex != -1), "Not found : %s is not an .imap group (in rayfire %s)", pCEMI->GetEndImapFile().GetCStr(), pCEMI->GetModelName());
		if (endImapIndex != -1){
			Assertf(fwMapDataStore::GetStore().IsScriptManaged(endImapIndex), "%s is not an .imap group (in rayfire %s)", fwMapDataStore::GetStore().GetName(endImapIndex), pCEMI->GetModelName());
			m_endImapIndex = endImapIndex.Get();
		}
	} 
}

//
// name:		CCompEntity::~CCompEntity
// description:	Destructor for a composite entity
CCompEntity::~CCompEntity()
{
	// don't modify state of world when deleting entity
	m_startImapIndex = -1;
	m_endImapIndex = -1;

	if (GetIsActive()){
		SetIsActive(false);
		ms_activeEntries.Remove(this);
		DeleteDrawable();
	}

	m_currentState = CE_STATE_INVALID;

#if GTA_REPLAY
	WritePreLoadPacketIfRequired(false);

	if( m_bLoadAll )
	{
		ReleasePreLoadedAssets();
	}
	else
#endif
	{
		RemoveCurrentAssetSetExceptFor(CE_NONE);			// clear flags for all assets
	}

	TellAudioAboutAudioEffectsRemove();			// Tells the audio an entity has been removed that may have audio effects on it.

	// don't leave an active swap dangling, if the comp entity is removed during syncing etc
	if (m_imapSwapIndex != fwImapGroupMgr::INVALID_INDEX)
	{
		fwMapDataStore::GetStore().GetGroupSwapper().MarkSwapForCleanup(m_imapSwapIndex);
		m_imapSwapIndex = fwImapGroupMgr::INVALID_INDEX;
	}
}

//
// name:		CCompEntity::Shutdown
// description:	cleanup all the children of active CompEntities
void CCompEntity::Shutdown(unsigned shutdownMode){

	if (shutdownMode == SHUTDOWN_SESSION)
	{
		CCompEntity* pComposite = NULL;
		s32 poolSize=CCompEntity::GetPool()->GetSize();
		s32 offset = 0;

		while(offset < poolSize)
		{
			pComposite = CCompEntity::GetPool()->GetSlot(offset);

			if (pComposite){
				if (pComposite->GetIsActive()){
					pComposite->SetIsActive(false);
					ms_activeEntries.Remove(pComposite);
					pComposite->DeleteDrawable();
					pComposite->RemoveCurrentAssetSetExceptFor(CE_NONE);			// clear flags for all assets
				}

				pComposite->m_currentState = CE_STATE_INIT;
			}
			offset++;
		}
	}
}

// if the parent object is instructed to delete drawable then make sure all children are cleaned up
void CCompEntity::DeleteDrawable(){

	ms_bAllowedToDelete = true;

	ReleaseStreamRequests();

	// cleanup all currently created child objects
	for(u32 i=0; i<m_ChildObjects.GetCount(); i++){
		if (m_ChildObjects[i]){
			m_ChildObjects[i]->DeleteDrawable();
			CObjectPopulation::DestroyObject(m_ChildObjects[i]);
		}
	}

	//Assertf(m_currentState != CE_STATE_PRIMED, "Removing compEntity drawables whilst in a primed state!");

	CEntity::DeleteDrawable();

	ms_bAllowedToDelete = false;
}

// this object shouldn't have a drawable...
bool CCompEntity::CreateDrawable(){

	return(CEntity::CreateDrawable());
}

// request beginning model and display it once loaded
void CCompEntity::SetToStarting(void)
{
#if GTA_REPLAY
	if( m_bLoadAll )
	{
		m_currentState = CE_STATE_START;
		RemoveUnwanted();
		AddWanted();
	}
	else
#endif

	if (m_currentState != CE_STATE_START)
	{
		ReleaseStreamRequests();
		IssueAssetRequests(CE_START_ASSETS);
		m_currentState = CE_STATE_SYNC_STARTING;
	}
}

// request the end model and display it once loaded
void CCompEntity::SetToEnding(void)
{

#if GTA_REPLAY
	if( m_bLoadAll )
	{
		m_currentState = CE_STATE_END;
		RemoveUnwanted();
		AddWanted();
	}
	else
#endif

	if ((m_currentState == CE_STATE_ANIMATING) || (m_currentState == CE_STATE_PAUSED)){
		SetAnimTo(1.0f);
		ImmediateUpdate();
		ResumeAnim();
	} else if (m_currentState != CE_STATE_END){
		ReleaseStreamRequests();
		IssueAssetRequests(CE_END_ASSETS);
		m_currentState = CE_STATE_SYNC_ENDING;
	}
}

// request the anim model and dictionary
void CCompEntity::SetToPriming(void)
{

#if GTA_REPLAY
	if( m_bLoadAll )
	{
		m_currentState = CE_STATE_PRIMED;
		return;
	}
#endif

	if ((m_currentState != CE_STATE_PRIMED) && (m_currentState != CE_STATE_PRIMING))
	{
		if (m_currentState == CE_STATE_START || m_currentState == CE_STATE_PRIMING_PRELUDE)
		{
			ReleaseStreamRequests();
			IssueAssetRequests(CE_ANIM_ASSETS);
			IssueAssetRequests(CE_PTFX_ASSETS);
			m_currentState = CE_STATE_PRIMING;
		} else
		{
			// set to priming, after the start state is loaded
			SetToStarting();
			m_currentState = CE_STATE_PRIMING_PRELUDE;
		}
	}
}

void CCompEntity::SetToInit(void)
{
	m_currentState = CE_STATE_INIT;
}

void CCompEntity::SetToAbandon(void)
{
#if GTA_REPLAY
	if( m_bLoadAll )
	{
		ReleasePreLoadedAssets();
	}
	else
#endif
	{
		RemoveCurrentAssetSetExceptFor(CE_NONE);
		ReleaseStreamRequests();
	}

	m_currentState = CE_STATE_ABANDON;
}

// trigger animation
void CCompEntity::TriggerAnim(void)	
{ 
#if GTA_REPLAY
	if( m_bLoadAll )
	{
		m_currentState = CE_STATE_ANIMATING;
		RemoveUnwanted();
		AddWanted();
	}
	else
#endif

	if (m_currentState == CE_STATE_PRIMED) 
	{ 
		m_currentState = CE_STATE_START_ANIM; 
	} else {
#if __ASSERT
		char displayString[255];
		GenerateDebugInfo(displayString);
		sprintf(displayString,"%s : %s", GetModelName(), displayString);
		sceneAssertf(false,"Rayfire can ONLY animate from a primed state. %s",displayString);
#endif //__ASSERT
	}
}

// request the anim model and dictionary
void CCompEntity::Reset(void)
{
#if GTA_REPLAY
	if( m_bLoadAll )
	{
		SetToStarting();
	}
	else
#endif

	if (m_currentState != CE_STATE_RESET){
		m_targetPausePhase = 0.0f;
		ReleaseStreamRequests();			// release requests first
		RemoveCurrentAssetSetExceptFor(CE_NONE);			// then we can set assets as we desire

		m_currentState = CE_STATE_RESET;
		SetToStarting();
	}
}

// pause anim at the desired phase (if desired phase is 0.0f then pause anim immediately)
void CCompEntity::PauseAnimAt(float targetPhase){

	if (targetPhase == 0.0f){
		m_targetPausePhase = m_pausePhase;		// pause immediately
		m_currentState = CE_STATE_PAUSED;
	} else {
		m_targetPausePhase = targetPhase;
	}
}

// set the rayfire anim to the target phase and pause it
void CCompEntity::SetAnimTo(float targetPhase){
	m_pausePhase = targetPhase;
	m_targetPausePhase = m_pausePhase;
	m_currentState = CE_STATE_PAUSED;
}

// resume playing of anim from a paused state
void CCompEntity::ResumeAnim(){

	if (m_currentState == CE_STATE_PAUSED){
		m_currentState = CE_STATE_ANIMATING;
	}
}

// make sure that there is nothing pending for this rayfire
void	CCompEntity::ReleaseStreamRequests() 
{
	m_startRequest.ClearRequiredFlags(STRFLAG_DONTDELETE);
	m_endRequest.ClearRequiredFlags(STRFLAG_DONTDELETE);
	m_animRequests.ClearRequiredFlags(STRFLAG_DONTDELETE);
	m_ptfxRequest.ClearRequiredFlags(STRFLAG_DONTDELETE);

	m_startRequest.Release(); 
	m_endRequest.Release(); 
	m_animRequests.ReleaseAll(); 
	m_ptfxRequest.Release();

	fwImapGroupMgr& imapGroupMgr = INSTANCE_STORE.GetGroupSwapper();
#if GTA_REPLAY
	if( !m_bLoadAll )		// LoadAll assets are removed in ReleasePreLoadedAssets, called from the destructor
#endif	//GTA_REPLAY
	{
		if (fwImapGroupMgr::IsValidIndex(m_imapSwapIndex) && imapGroupMgr.GetIsActive(m_imapSwapIndex))
		{
			if (imapGroupMgr.GetIsSwapImapLoaded(m_imapSwapIndex)){
				imapGroupMgr.CompleteSwap(m_imapSwapIndex);
			} else {
				imapGroupMgr.AbandonSwap(m_imapSwapIndex);
			}
			m_imapSwapIndex = fwImapGroupMgr::INVALID_INDEX;;
		}
	}
}

// scan through the composite entity list over a number of frames & update entries
void CCompEntity::Update(){

#if NAVMESH_EXPORT
	if(CNavMeshDataExporter::IsExportingCollision())
	{
		return;
	}
#endif

#if GTA_REPLAY
	ReplayRayFireManager::ProcessDeferred();
#endif

	CCompEntity* pComposite = NULL;
	s32 poolSize=CCompEntity::GetPool()->GetSize();
	static s32 offset = 0;
	s32 base = 0;
	u32 skip = COMPENTITY_UPDATE_NUM_FRAMES_TO_PROCESS;

	// go through the pool over a number of frames, looking for entries to start immediately processing
	while((base+offset) < poolSize)
	{
		pComposite = CCompEntity::GetPool()->GetSlot(base + offset);

		if (pComposite){
			CBaseModelInfo* pMI = pComposite->GetBaseModelInfo();
			Assert(pMI);

			if (pMI){
				float fadeDist2 = (float)(pComposite->GetLodDistance() * pComposite->GetLodDistance());

				Vector3 vCameraPosition= camInterface::GetPos();
				Vector3 vCompEntityPos = VEC3V_TO_VECTOR3(pComposite->GetTransform().GetPosition());
				Vector3 vDelta(vCameraPosition - vCompEntityPos);
				if (vDelta.Mag2() < fadeDist2 
					REPLAY_ONLY( || CReplayMgr::IsEditModeActive() || pComposite->m_bLoadAll ) )		// Unsure update is called if we're in replay, but compentity isn't, and vice versa to do the swap ASAP.
				{
					if ( !pComposite->GetIsActive() )
					{
						ms_activeEntries.Add(pComposite);
						pComposite->SetIsActive(true);
						pComposite->RestoreChildren();
					}
				} 
				else 
				{
					if (pComposite->GetIsActive() && !(pComposite->GetState() == CE_STATE_ANIMATING))
					{
						pComposite->SetIsActive(false);
						ms_activeEntries.Remove(pComposite);
						pComposite->DeleteDrawable();
					}
				}
			}
		}
		base += skip;
	}

	offset = (offset + 1)%COMPENTITY_UPDATE_NUM_FRAMES_TO_PROCESS;


	// go through the immediate list and process each active compEntity every frame
	fwPtrNode* pNode = ms_activeEntries.GetHeadPtr();

	while (pNode != NULL)
	{
		CCompEntity* pCompEntity = static_cast<CCompEntity*>(pNode->GetPtr());
		pNode = pNode->GetNextPtr();
		if (pCompEntity){
			pCompEntity->ImmediateUpdate();
		}
	}

#if __BANK
	if (g_PickerManager.IsCurrentOwner("Rayfire picker")){
		g_debugCompEntity = NULL;
		fwEntity* pPickedEntity = g_PickerManager.GetSelectedEntity();
		if (pPickedEntity){
			if (pPickedEntity->GetType() == ENTITY_TYPE_COMPOSITE){
				g_debugCompEntity = static_cast<CCompEntity*>(pPickedEntity);
			}
		} 
	}
#endif //__BANK
}

void CCompEntity::UpdateCompEntitiesUsingGroup(s32 groupIdx, eCompEntityState targetState)
{
	CCompEntity* pComposite = NULL;
	s32 poolSize=CCompEntity::GetPool()->GetSize();

	// go through the pool over a number of frames, looking for entries that need handled
	for(u32 i=0; i<poolSize; i++)
	{
		pComposite = CCompEntity::GetPool()->GetSlot(i);
		if (pComposite)
		{
			if ((pComposite->m_startImapIndex == groupIdx) || (pComposite->m_endImapIndex == groupIdx))
			{
				switch(targetState)
				{
					case CE_STATE_INIT:
						pComposite->SetModifiedBy(CTheScripts::GetCurrentScriptName());
						pComposite->SetToInit();
						break;
					case CE_STATE_ABANDON:
						pComposite->SetModifiedBy(CTheScripts::GetCurrentScriptName());
						pComposite->SetToAbandon();
						break;
					default:
						Assertf(false,"State not recognised for update using group");
				}
			}
		}
	}
}

#if __BANK
void SelectAnimToDisplayCB(void){

	if (g_debugCompEntity){
		for(s32 i=0; i<8; i++){
			CObject* pObj = g_debugCompEntity->GetChild(i);
			if (pObj){
				if ((g_selectedAnimModel == -1) || (i == g_selectedAnimModel)){
					pObj->SetIsVisibleForModule(SETISVISIBLE_MODULE_WORLD, true, false);
				} else {
					pObj->SetIsVisibleForModule(SETISVISIBLE_MODULE_WORLD, false, false);
				}
			}
		} 
	}
}
#endif //__BANK

void CCompEntity::ProcessAnimsVisibility(float currPhase){

	CCompEntityModelInfo* pCEModelInfo = static_cast<CCompEntityModelInfo*>(GetBaseModelInfo());
	sceneAssert(pCEModelInfo);

		for(s32 i=0; i<8; i++){
			CObject* pObj = GetChild(i);
			if (pObj){
				if ((currPhase > pCEModelInfo->GetAnims(i).m_punchInPhase) && (currPhase < pCEModelInfo->GetAnims(i).m_punchOutPhase)){
					pObj->SetIsVisibleForModule(SETISVISIBLE_MODULE_WORLD, true, false);
				} else {
					pObj->SetIsVisibleForModule(SETISVISIBLE_MODULE_WORLD, false, false);
				}
			}
		} 

#if __BANK
		if (g_selectedAnimModel != -1){
			SelectAnimToDisplayCB();		// this widget should override visibility if it is active
		}
#endif //__BANK
}

void CCompEntity::ComputeEndPhase()
{
	CObject* pAnimatedObj = GetPrimaryAnimatedObject();
	if( pAnimatedObj && pAnimatedObj->GetAnimDirector() )
	{
		CMoveObject &move = pAnimatedObj->GetMoveObject();
		const crClip* clip = move.GetClipHelper().GetClip();

		if(clip)
		{
			m_targetEndPhase = 1.0f - clip->ConvertTimeToPhase(1/30.0f); 
		}	
	}
}

// state machine update for composite entity
void CCompEntity::ImmediateUpdate()
{

	CCompEntityModelInfo* pCEModelInfo = static_cast<CCompEntityModelInfo*>(GetBaseModelInfo());
	sceneAssert(pCEModelInfo);

#if GTA_REPLAY

	WritePreLoadPacketIfRequired(true);

	CacheCurrentMapState();

	// Because compentities aren't recreated when entering the replay, if we enter the replay stood next to a compentity that's in the replay it doesnt' know it's in replay until this point
	if( CReplayMgr::GetMode() == REPLAYMODE_EDIT && m_bLoadAll == false )
	{
		SwitchToReplayMode();
	}
	else if( m_bLoadAll == true && CReplayMgr::GetMode() != REPLAYMODE_EDIT )
	{
		SwitchToGameplayMode();
	}
	else if( m_bLoadAll == true && m_ReplayLoadIndex == -1 )
	{
		// Were never in gameplay mode, so no switch happened, link to assets now
		PreLoadAllAssets();		// Links to pre-loaded assets
		m_currentState = CE_STATE_AWAIT_PRELOAD;
	}

	eCompEntityState prevState = m_currentState;	// Store this so we can tell whether the state has changed.

#endif

	float currPhase = 0.0f;
	float lastPhase = 0.0f;
	switch(m_currentState){
		case CE_STATE_INIT:
			// if using imap groups, then state of the created compEntity has to match what is currently active
			if (pCEModelInfo->GetUseImapForStartAndEnd()){
				strLocalIndex startImapIndex = strLocalIndex(fwMapDataStore::GetStore().FindSlotFromHashKey(pCEModelInfo->GetStartImapFile().GetHash()));
				Assert(fwMapDataStore::GetStore().IsValidSlot(startImapIndex));
				if (fwMapDataStore::GetStore().GetIsStreamable(startImapIndex))
				{
					m_currentState = CE_STATE_START;
				}

				strLocalIndex endImapIndex = strLocalIndex(fwMapDataStore::GetStore().FindSlotFromHashKey(pCEModelInfo->GetEndImapFile().GetHash()));
				Assert(fwMapDataStore::GetStore().IsValidSlot(endImapIndex));
				if (fwMapDataStore::GetStore().GetIsStreamable(endImapIndex)){
					m_currentState = CE_STATE_END;
				}
			}

#if GTA_REPLAY
			if( !m_bLoadAll )
#endif // GTA_REPLAY
			{
				if (m_currentState == CE_STATE_INIT)
				{
					SetToStarting();
				}
			}
			break;
		case CE_STATE_SYNC_STARTING:
			if (IsAssetSetReady(CE_START_ASSETS))
			{
				m_currentState = CE_STATE_STARTING;
			}
			break;
		case CE_STATE_STARTING:
			RemoveCurrentAssetSetExceptFor(CE_START_ASSETS);
			SwitchToAssetSet(CE_START_ASSETS);
			ReleaseStreamRequests();
			m_currentState = CE_STATE_START;
			break;
		case CE_STATE_START:
			break;
		case CE_STATE_PRIMING_PRELUDE:
			if (IsAssetSetReady(CE_START_ASSETS))
			{
				RemoveCurrentAssetSetExceptFor(CE_START_ASSETS);
				SwitchToAssetSet(CE_START_ASSETS);
				ReleaseStreamRequests();
				SetToPriming();
			}
			break;
		case CE_STATE_PRIMING:			// waiting for assets to stream in
			if (IsAssetSetReady(CE_ANIM_ASSETS) && IsAssetSetReady(CE_PTFX_ASSETS) /*&& IsAssetSetReady(CE_END_ASSETS)*/)
			{
				m_currentState = CE_STATE_PRIMED;
			} else {
				Displayf("[Rayfire %s priming] - ANIM:%s  PTFX:%s", GetModelName(), (IsAssetSetReady(CE_ANIM_ASSETS) ? "YES" : "NO"), 
					(IsAssetSetReady(CE_PTFX_ASSETS) ? "YES" : "NO"));
			}
			break;
		case CE_STATE_PRIMED:			// activate start model
			// avoid these being cleaned up whilst waiting to start anim & make sure in ready to animated state
			Assert(IsAssetSetReady(CE_ANIM_ASSETS));
			Assert(IsAssetSetReady(CE_PTFX_ASSETS));
			break;
		case CE_STATE_START_ANIM:		// switch to anim model & begin animation
			RemoveCurrentAssetSetExceptFor(CE_ANIM_ASSETS);
			SwitchToAssetSet(CE_ANIM_ASSETS);
			TriggerObjectAnim();
			for(u32 i = 0; i<m_ChildObjects.GetCount(); i++){
				m_ChildObjects[i]->m_nDEflags.bForcePrePhysicsAnimUpdate = true;			// always update rayfire anims
			}
			m_currentState = CE_STATE_ANIMATING;
			IssueAssetRequests(CE_END_ASSETS);
			break;
		case CE_STATE_ANIMATING:		// animating model
		case CE_STATE_PAUSED:			// animating model (but animation is currently paused)
			//sceneAssertf(GetPrimaryAnimatedObject(), "primary object is null during animation");
			if (GetPrimaryAnimatedObject()){
				GetObjAnimPhaseInfo(currPhase, lastPhase);

				//ProcessAnimsVisibility(currPhase);				// not doing visibility using this any more

				if (m_targetPausePhase > lastPhase && m_targetPausePhase < currPhase){
					m_currentState = CE_STATE_PAUSED;
				}

				//if (m_currentState == CE_STATE_ANIMATING)	// need the particles to always show in replay

					ProcessEffects(currPhase, lastPhase);

				ComputeEndPhase();

				if(currPhase >=  m_targetEndPhase)
				{
#if GTA_REPLAY
					if(m_bLoadAll)
					{
						m_currentState = CE_STATE_END;
					}
					else
#endif
					{
						m_currentState = CE_STATE_SYNC_ENDING;
					}
				}
			} 
			else 
			{
#if GTA_REPLAY
				if(m_bLoadAll)
				{
					m_currentState = CE_STATE_END;
				}
				else
#endif
				m_currentState = CE_STATE_SYNC_ENDING;
			}

#if __BANK
			if (g_debugCompEntity)	{
				g_compEntityAnimDislayPhase = currPhase;
			}
#endif //__BANK

			break;
		case CE_STATE_SYNC_ENDING:
			if (IsAssetSetReady(CE_END_ASSETS))
			{
				m_currentState = CE_STATE_ENDING;
			}
			break;
		case CE_STATE_ENDING:
			RemoveCurrentAssetSetExceptFor(CE_END_ASSETS);
			SwitchToAssetSet(CE_END_ASSETS);
			ReleaseStreamRequests();
			m_currentState = CE_STATE_END;
			break;
		case CE_STATE_END:
			break;
		case CE_STATE_ABANDON:
			break;

#if GTA_REPLAY

		case CE_STATE_AWAIT_PRELOAD:
			if (HasPreLoadCompleted())
			{
				m_bPreLoadComplete = true;
				m_currentState = CE_STATE_PRELOADED;
			}
			break;

		case CE_STATE_PRELOADED:
			{
				if( m_CachedStartMapState == CE_STATE_END )
				{
					SetToEnding();
				}
				else
				{
					SetToStarting();
				}
			}
			break;

#endif	//GTA_REPLAY

		default:
			break;
	}

#if GTA_REPLAY

	if( CReplayMgr::ShouldRecord() )
	{
		Vector3 pos = VEC3V_TO_VECTOR3( GetTransform().GetPosition() );
		float phase = 0.0f;
		bool doRecordUpdating = false;
		u32		recordState = PacketRayFireInfo::STATE_START;

		// Start State
		if( /*m_currentState == CE_STATE_START ||	// Don't record at these states as lots of Rayfires idle in these states which we don't want to record
			m_currentState == CE_STATE_SYNC_STARTING || 
			m_currentState == CE_STATE_STARTING || */
			m_currentState == CE_STATE_PRIMED ||
			m_currentState == CE_STATE_PRIMING )
		{
			// Issue start packet
			//recordState = PacketRayFireInfo::STATE_START;
			doRecordUpdating = true;
		}
		// End state
		else if(m_currentState == CE_STATE_ENDING || m_currentState == CE_STATE_SYNC_ENDING || m_currentState == CE_STATE_END)
		{
			recordState = PacketRayFireInfo::STATE_END;
			if(prevState != CE_STATE_END)
				doRecordUpdating = true;
		}
		// Animating
		else if( m_currentState == CE_STATE_START_ANIM || m_currentState == CE_STATE_ANIMATING || m_currentState == CE_STATE_PAUSED )
		{
			recordState = PacketRayFireInfo::STATE_ANIMATING;
			CObject* pAnimatedObj = GetPrimaryAnimatedObject();
			if( pAnimatedObj && pAnimatedObj->GetAnimDirector() )
			{
				CMoveObject &move = pAnimatedObj->GetMoveObject();
				phase = move.GetClipHelper().GetPhase();
			}
			doRecordUpdating = true;
		}
		
		if( doRecordUpdating )
		{
			CReplayMgr::RecordPersistantFx<CPacketRayFireUpdating>( CPacketRayFireUpdating(pCEModelInfo->GetModelNameHash(), pos, recordState, phase, m_currentState == CE_STATE_END), 
															CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)pCEModelInfo->GetModelNameHash()), NULL, m_currentState != CE_STATE_END );
		}
		else
		{
			CReplayMgr::RecordFx(CPacketRayFireStatic(pCEModelInfo->GetModelNameHash(), pos, recordState, phase));
			CReplayMgr::StopTrackingFx(CTrackedEventInfo<ptxEffectRef>((ptxEffectRef)pCEModelInfo->GetModelNameHash()));
		}
	}
#endif

}

// issue the streaming requests required for this composite object (request all of the children)
void CCompEntity::IssueAssetRequests(eCompEntityAssetSet desiredAssets){

	CCompEntityModelInfo* pCEMI = static_cast<CCompEntityModelInfo*>(GetBaseModelInfo());
	Assert(pCEMI);
	fwImapGroupMgr& imapGroupMgr = INSTANCE_STORE.GetGroupSwapper();

	STRVIS_AUTO_CONTEXT(strStreamingVisualize::COMPENTITIY);

	// handle .imap swaps correctly - use the swapper rather than streaming models
	if (pCEMI->GetUseImapForStartAndEnd())
	{
		u32 targetImapHash = 0;
		bool bCreateSwap = true;

		if (desiredAssets == CE_START_ASSETS){
			targetImapHash = pCEMI->GetStartImapFile().GetHash();
		} else if (desiredAssets == CE_END_ASSETS) {
			targetImapHash = pCEMI->GetEndImapFile().GetHash();
		} else {
			bCreateSwap = false;
		}
		
		// end state already loaded - don't create a swap!
		if (targetImapHash != 0){
			strLocalIndex targetImapIdx = fwMapDataStore::GetStore().FindSlotFromHashKey(targetImapHash);
			if (fwMapDataStore::GetStore().HasObjectLoaded(targetImapIdx)){
				bCreateSwap = false;
			}
		}

		if (bCreateSwap){
			ASSERT_ONLY(bool bSuccess = )imapGroupMgr.CreateNewSwap(atHashString((u32) 0), atHashString(targetImapHash), m_imapSwapIndex, false);      // false means “not automatic”
			Assert(bSuccess);
		}
	}
	else {
		if (desiredAssets == CE_START_ASSETS){
			fwModelId startModelId;
			CModelInfo::GetBaseModelInfoFromHashKey((u32) pCEMI->GetStartModelHash(), &startModelId);
			if (Verifyf(startModelId.IsValid(), "CCompEntity::IssueAssetRequests - couldn't find start model for %s", CModelInfo::GetBaseModelInfoName(startModelId)))
			{
				strLocalIndex transientLocalIdx = strLocalIndex(CModelInfo::AssignLocalIndexToModelInfo(startModelId));
				m_startRequest.Request(transientLocalIdx, CModelInfo::GetStreamingModuleId(), STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD|STRFLAG_DONTDELETE);
			}
		}

 		if (desiredAssets == CE_END_ASSETS){
	 		fwModelId endModelId;
			CModelInfo::GetBaseModelInfoFromHashKey((u32) pCEMI->GetEndModelHash(), &endModelId);
			if (Verifyf(endModelId.IsValid(), "CCompEntity::IssueAssetRequests - couldn't find end model for %s", CModelInfo::GetBaseModelInfoName(endModelId)))
			{
				strLocalIndex transientLocalIdx = strLocalIndex(CModelInfo::AssignLocalIndexToModelInfo(endModelId));
				m_endRequest.Request(transientLocalIdx, CModelInfo::GetStreamingModuleId(), STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD|STRFLAG_DONTDELETE);
			}
		}
	}

	if (desiredAssets == CE_ANIM_ASSETS){
		// setup streaming requests for all the anim models & animation data
		artAssertf(m_animRequests.GetRequestMaxCount() >= pCEMI->GetNumOfAnims()*2, "%s uses too many anims", pCEMI->GetModelName());
		for(u32 i=0; i<pCEMI->GetNumOfAnims(); i++)
		{
			CCompEntityAnims& animData = pCEMI->GetAnims(i);

			fwModelId animModelId;
			CModelInfo::GetBaseModelInfoFromHashKey((u32) atHashString(animData.GetAnimatedModel()), &animModelId);
			if (Verifyf(animModelId.IsValid(), "CCompEntity::IssueAssetRequests - couldn't find anim model for %s", CModelInfo::GetBaseModelInfoName(animModelId)))
			{
				s32 transientLocalIdx = CModelInfo::AssignLocalIndexToModelInfo(animModelId).Get();
				m_animRequests.PushRequest(transientLocalIdx, CModelInfo::GetStreamingModuleId(), STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD|STRFLAG_DONTDELETE);
			}

			s32 animSlotIndex = fwAnimManager::FindSlotFromHashKey(atHashString(animData.GetAnimDict())).Get();
			if (Verifyf(CModelInfo::IsValidModelInfo(animSlotIndex), "CCompEntity::IssueAssetRequests - couldn't find animation for %s", CModelInfo::GetBaseModelInfoName(GetModelId())))
			{
				m_animRequests.PushRequest(animSlotIndex, fwAnimManager::GetStreamingModuleId(), STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD|STRFLAG_DONTDELETE);
			}
		}
	}

	if (desiredAssets == CE_PTFX_ASSETS){
		strLocalIndex ptfxSlotId = strLocalIndex(-1);
		const atHashString PtfxAssetNameHash = pCEMI->GetPtFxAssetName();
		if (PtfxAssetNameHash.GetHash() != 0)
		{
			ptfxSlotId = ptfxManager::GetAssetStore().FindSlotFromHashKey(PtfxAssetNameHash.GetHash());

			if (Verifyf(ptfxSlotId.Get()>-1, "cannot find particle asset with this name in any loaded rpf (%s)", PtfxAssetNameHash.GetCStr()))
			{
				m_ptfxRequest.Request(ptfxSlotId, ptfxManager::GetAssetStore().GetStreamingModuleId(), STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD|STRFLAG_DONTDELETE);
			}
		}
	}
}

// has the desired asset set fully loaded in memory (and ready for switching)
bool CCompEntity::IsAssetSetReady(eCompEntityAssetSet assetSet)
{
	CCompEntityModelInfo* pCEMI = static_cast<CCompEntityModelInfo*>(GetBaseModelInfo());
	Assert(pCEMI);
	fwImapGroupMgr& imapGroupMgr = INSTANCE_STORE.GetGroupSwapper();

#if GTA_REPLAY
	if( m_bLoadAll )
	{
		if( m_ReplayLoadIndex >= 0 && m_ReplayLoadIndex < MAX_RAYFIRES_PRELOAD_COUNT)
		{
			RayFireReplayLoadingData *pData = ReplayRayFireManager::GetLoadingData(m_ReplayLoadIndex);

			Assert(pData);

			switch(assetSet)
			{
			case CE_START_ASSETS:
				if (pCEMI->GetUseImapForStartAndEnd())
				{
					return imapGroupMgr.GetIsLoaded(pData->m_imapSwapIndex);

					/*
					strLocalIndex startImapIndex = strLocalIndex(fwMapDataStore::GetStore().FindSlotFromHashKey(pCEMI->GetStartImapFile().GetHash()));
					if (fwMapDataStore::GetStore().HasObjectLoaded(startImapIndex))
					{
						return(true);
					}
					*/

				} 
				else 
				{
					if (pData->m_startRequest.HasLoaded())
					{
						return(true);
					}
				}
				break;
			case CE_ANIM_ASSETS:
				if ( pData->m_animRequests.GetRequestCount() > 0 && pData->m_animRequests.HaveAllLoaded())
				{
					return(true);
				}
				break;
			case CE_PTFX_ASSETS:
				if ((!pData->m_ptfxRequest.IsValid()) || pData->m_ptfxRequest.HasLoaded())
				{
					return(true);
				}
				break;
			case CE_END_ASSETS:
				if (pCEMI->GetUseImapForStartAndEnd())
				{
					return imapGroupMgr.GetIsLoaded(pData->m_imapSwapEndIndex);

					/*
					strLocalIndex endImapIndex = strLocalIndex(fwMapDataStore::GetStore().FindSlotFromHashKey(pCEMI->GetEndImapFile().GetHash()));
					if (fwMapDataStore::GetStore().HasObjectLoaded(endImapIndex))
					{
						return(true);
					}
					*/
				} 
				else 
				{
					if (pData->m_endRequest.HasLoaded())
					{
						return(true);
					}
				}
				break;

			default:
				Assert(false);
			}
		}
	}
	else
#endif	//GTA_REPLAY
	{
		// if there is a swap active then use it to determine start / end asset state
		if (m_imapSwapIndex != fwImapGroupMgr::INVALID_INDEX){
			if (assetSet == CE_START_ASSETS && imapGroupMgr.GetFinalImapIndex(m_imapSwapIndex) == m_startImapIndex){
				return(imapGroupMgr.GetIsLoaded(m_imapSwapIndex));
			}
			if (assetSet == CE_END_ASSETS && imapGroupMgr.GetFinalImapIndex(m_imapSwapIndex) == m_endImapIndex){
				return(imapGroupMgr.GetIsLoaded(m_imapSwapIndex));
			}
		}

		switch(assetSet)
		{
		case CE_START_ASSETS:
			if (pCEMI->GetUseImapForStartAndEnd()){
				if (fwMapDataStore::GetStore().HasObjectLoaded(strLocalIndex(m_startImapIndex))){
					return(true);
				}
			} else {
				if (m_startRequest.HasLoaded()){
					return(true);
				}
			}
			break;
		case CE_ANIM_ASSETS:
			if ( m_animRequests.GetRequestCount() > 0 && m_animRequests.HaveAllLoaded()){
				return(true);
			}
			break;
		case CE_PTFX_ASSETS:
			if ((!m_ptfxRequest.IsValid()) || m_ptfxRequest.HasLoaded()){
				return(true);
			}
			break;
		case CE_END_ASSETS:
			if (pCEMI->GetUseImapForStartAndEnd()){
				if (fwMapDataStore::GetStore().HasObjectLoaded(strLocalIndex(m_endImapIndex))){
					return(true);
				}
			} else {
				if (m_endRequest.HasLoaded()){
					return(true);
				}
			}
			break;

		default:
			Assert(false);
		}
	}

	return(false);
}

// 
void CCompEntity::RestoreChildren()
{
#if GTA_REPLAY
	if( m_bLoadAll )
	{
		// Early out, replay will reset any state
		return;
	}
#endif

	switch(m_currentState){
	case CE_STATE_INIT:
	case CE_STATE_ABANDON:
		m_currentState = CE_STATE_INIT;
		break;
	case CE_STATE_SYNC_STARTING:
	case CE_STATE_STARTING:
	case CE_STATE_START:
		m_currentState = CE_STATE_INIT;		// force a state change
		SetToStarting();
		break;
	case CE_STATE_SYNC_ENDING:
	case CE_STATE_ENDING:
	case CE_STATE_END:
		m_currentState = CE_STATE_INIT;		// force a state change
		SetToEnding();
		break;
	case CE_STATE_PRIMED:
	case CE_STATE_PRIMING:
	case CE_STATE_PRIMING_PRELUDE:
		m_currentState = CE_STATE_INIT;		// force a state change
		SetToPriming();
		break;
	default:
		Assertf(false, "Unexpected request to restore children for %s (%d)", GetModelName(), m_currentState);
	}

}

void CCompEntity::ValidateImapGroupSafeToDelete(u32 ASSERT_ONLY(imapSwapIndex), s32 ASSERT_ONLY(imapToCheckFor))
{
#if __ASSERT
	fwImapGroupMgr& imapGroupMgr = INSTANCE_STORE.GetGroupSwapper();

	if (imapSwapIndex != fwImapGroupMgr::INVALID_INDEX)
	{
		strLocalIndex finalImap = strLocalIndex(imapGroupMgr.GetFinalImapIndex(m_imapSwapIndex));
		Assert(finalImap != -1);

		if (imapToCheckFor == finalImap.Get())
		{
			INSTANCE_DEF* pDef = INSTANCE_STORE.GetSlot(finalImap);
			Assert(pDef);
			Assertf(!pDef->GetPostponeWorldAdd(), "current rayfire (%s) state : %d : deleting (%s)", GetModelName(), m_currentState, INSTANCE_STORE.GetName(finalImap));
		}
	}
#endif //__ASSERT
}

void CCompEntity::RemoveCurrentAssetSetExceptFor(eCompEntityAssetSet ExceptFor)
{

	ms_bAllowedToDelete = true;

	// cleanup all objects
	for(u32 i=0; i<m_ChildObjects.GetCount(); i++){
		if (m_ChildObjects[i]){
			m_ChildObjects[i]->DeleteDrawable();
			CObjectPopulation::DestroyObject(m_ChildObjects[i]);
		}
	}

	CCompEntityModelInfo* pCEMI = GetCompEntityModelInfo();
	Assert(pCEMI);
	fwImapGroupMgr& imapGroupMgr = INSTANCE_STORE.GetGroupSwapper();

	if (ExceptFor != CE_START_ASSETS) {
		// only remove start state if not swapping to it
		if (m_imapSwapIndex == fwImapGroupMgr::INVALID_INDEX || imapGroupMgr.GetFinalImapIndex(m_imapSwapIndex) != m_startImapIndex)
		{
			ValidateImapGroupSafeToDelete(m_imapSwapIndex, m_startImapIndex);
			INSTANCE_STORE.RemoveGroup(strLocalIndex(m_startImapIndex), pCEMI->GetStartImapFile());
		}

		m_startRequest.ClearRequiredFlags(STRFLAG_DONTDELETE);
	}
			
	if (ExceptFor != CE_END_ASSETS) {
		// only remove end state if not swapping to it
		if (m_imapSwapIndex == fwImapGroupMgr::INVALID_INDEX || imapGroupMgr.GetFinalImapIndex(m_imapSwapIndex) != m_endImapIndex)
		{
			ValidateImapGroupSafeToDelete(m_imapSwapIndex, m_endImapIndex);
			INSTANCE_STORE.RemoveGroup(strLocalIndex(m_endImapIndex), pCEMI->GetEndImapFile());	
		}

		m_endRequest.ClearRequiredFlags(STRFLAG_DONTDELETE);
	}

	if (ExceptFor != CE_ANIM_ASSETS) {
		m_animRequests.ClearRequiredFlags(STRFLAG_DONTDELETE);
	}

	if (ExceptFor != CE_PTFX_ASSETS) {
		m_ptfxRequest.ClearRequiredFlags(STRFLAG_DONTDELETE);
	}

	m_ChildObjects.Reset();

	ms_bAllowedToDelete = false;
}

void CCompEntity::CreateNewChildObject(u32 modelHash)
{
	fwModelId targetModelId; 
	CModelInfo::GetBaseModelInfoFromHashKey((u32) modelHash, &targetModelId);

#if __ASSERT
	CCompEntityModelInfo* pCEMI = GetCompEntityModelInfo();
	artAssertf(m_ChildObjects.GetMaxCount()>m_ChildObjects.GetCount(), "%s has too many child objects", pCEMI->GetModelName());
#endif

	RegdObj& pChildObject = m_ChildObjects.Append();
	pChildObject = CObjectPopulation::CreateObject(targetModelId, ENTITY_OWNEDBY_COMPENTITY, false, true, false);
	Assert(pChildObject);

	if (pChildObject){
		CTheScripts::GetScriptsForBrains().CheckAndApplyIfNewEntityNeedsScript(pChildObject, CScriptsForBrains::OBJECT_STREAMED);	//	, NULL);
		pChildObject->GetLodData().SetResetDisabled(true);
		pChildObject->SetPosition(VEC3V_TO_VECTOR3(GetTransform().GetPosition()));
		pChildObject->SetHeading(GetTransform().GetHeading());
		pChildObject->m_nFlags.bIsCompEntityProcObject = true;

		pChildObject->SetRenderPhaseVisibilityMask(GetRenderPhaseVisibilityMask());

		fwInteriorLocation intLocation = GetInteriorLocation();
		//REMOVE POST GTAV NG - - - - 
		//checking for hash of "des_plog_door_start" this is a hack to get the light 
		//attached to the ray fire (subsequently attached to a portal) to cast shadows.
		//It needs to be attached to the room to cast shadows, but doing this in the resource will cause the 
		//object to disappear as you walk through the exploded door.
		if(modelHash == 0x14c71c28)
		{			
			intLocation.SetRoomIndex(5);
		}
		//END REMOVE POST GTAV NG - - - - 
		CGameWorld::Add(pChildObject, intLocation);		// add to same location as parent	
				
		
		pChildObject->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(pChildObject->GetTransform().GetPosition()));
		pChildObject->CreateDrawable();
	}
}

void CCompEntity::SwitchToAssetSet(eCompEntityAssetSet desiredAssets)
{
	u32 modelHash = 0;
	CCompEntityModelInfo* pCEMI = GetCompEntityModelInfo();
	Assert(pCEMI);
	fwImapGroupMgr& imapGroupMgr = INSTANCE_STORE.GetGroupSwapper();

	// create desired new asset set
	switch(desiredAssets)
	{
		case CE_START_ASSETS:
			if (pCEMI->GetUseImapForStartAndEnd()){
				imapGroupMgr.CompleteSwap(m_imapSwapIndex);
				m_imapSwapIndex = fwImapGroupMgr::INVALID_INDEX;
			} else {
				modelHash = pCEMI->GetStartModelHash();
				CreateNewChildObject(modelHash);
			}
			break;
		case CE_ANIM_ASSETS:
			if (m_imapSwapIndex != fwImapGroupMgr::INVALID_INDEX){
				imapGroupMgr.RemoveStartState(m_imapSwapIndex);
			}
			for(u32 i=0; i<pCEMI->GetNumOfAnims(); i++)
			{
				CCompEntityAnims& animData = pCEMI->GetAnims(i);
				modelHash = atHashString(animData.GetAnimatedModel());
				CreateNewChildObject(modelHash);
			}
			break;
		case CE_END_ASSETS:
			if (pCEMI->GetUseImapForStartAndEnd()){
				imapGroupMgr.CompleteSwap(m_imapSwapIndex);
				m_imapSwapIndex = fwImapGroupMgr::INVALID_INDEX;
			} else {
				modelHash = pCEMI->GetEndModelHash();
				CreateNewChildObject(modelHash);
			}
			break;

		default:
			Assert(false);
	}

	ms_bAllowedToDelete = false;
}

void CCompEntity::TriggerObjectAnim()
{
	CCompEntityModelInfo* pCEMI = GetCompEntityModelInfo();
	Assert(pCEMI);

	for(u32 i=0; i<m_ChildObjects.GetCount(); i++)
	{
		// Check the object
		u32 animHashKey = atHashString(pCEMI->GetAnims(i).GetAnim());
		u32 animDictHash = atHashString(pCEMI->GetAnims(i).GetAnimDict());

		CObject* pObj = m_ChildObjects[i];
		if (sceneVerifyf(pObj, "TriggerObjectAnim - Object doesn't exist"))
		{
			pObj->CreateDrawable();
			if (sceneVerifyf(pObj->GetDrawable(), "TriggerObjectAnim - Object does not have a drawable"))
			{
				if (sceneVerifyf(pObj->GetDrawable()->GetSkeletonData(), "TriggerObjectAnim - Object does not have a skeleton data"))
				{
					s32 animSlotIndex = fwAnimManager::FindSlotFromHashKey(animDictHash).Get();
					// Check the animation
					if (sceneVerifyf(fwAnimManager::GetClipIfExistsByDictIndex(animSlotIndex, animHashKey), "TriggerObjectAnim - Couldn't find animation. Has it been loaded?"))
					{
						// Lazily create the skeleton if it doesn't exist
						if (!pObj->GetSkeleton())
						{
							pObj->CreateSkeleton();
						}
						if (sceneVerifyf(pObj->GetSkeleton(), "TriggerObjectAnim - Failed to create a skeleton"))
						{
							// Lazily create the animBlender if it doesn't exist
							if (!pObj->GetAnimDirector())
							{ 
								pObj->CreateAnimDirector(*pObj->GetDrawable());
							}
							if (sceneVerifyf(pObj->GetAnimDirector(), "TriggerObjectAnim - Failed to create an anim director"))
							{
								// Object has to be on the process control list for the animation blender to be updated
								if (!pObj->GetIsOnSceneUpdate())
								{
									pObj->AddToSceneUpdate();
								}

								const crClip* clip = fwAnimManager::GetClipIfExistsByDictIndex(animSlotIndex, animHashKey);

								CMoveObject& move = pObj->GetMoveObject();
								move.GetClipHelper().BlendInClipByClip(pObj, clip, NORMAL_BLEND_IN_DELTA, NORMAL_BLEND_OUT_DELTA, false, true );

								if(clip)
								{
									AddExtraVisibiltyDofs(*pObj->GetAnimDirector(), *clip);
								}
							}
						}
					}
				}
			}
		}
	}
}

bool AddExtraVisibilityDofsCb(const crExpressions* expr, void* data)
{
	if(expr)
	{
		fwAnimDirector& director = *reinterpret_cast<fwAnimDirector*>(data);
		fwAnimDirectorComponentCreature& component = *director.GetComponent<fwAnimDirectorComponentCreature>();
		
		expr->InitializeCreature(component.GetCreature(), &component.GetFrameDataFactory(), &component.GetCommonPool());
	}

	return true;
}

void CCompEntity::AddExtraVisibiltyDofs(fwAnimDirector& director, const crClip& clip)
{
	clip.ForAllExpressions(AddExtraVisibilityDofsCb, &director);
}

void CCompEntity::GetObjAnimPhaseInfo(float& currPhase, float& lastPhase)
{
	CCompEntityModelInfo* pCEMI = GetCompEntityModelInfo();
	Assert(pCEMI);
	u32 animHashKey = atHashString(pCEMI->GetAnims(0).GetAnim());
	u32 animDictHash = atHashString(pCEMI->GetAnims(0).GetAnimDict());

	CObject* pObj = m_ChildObjects[0];
	if (sceneVerifyf(pObj, "GetObjectAnimCurrentPhase - Object doesn't exist"))
	{
		if (pObj->GetAnimDirector())
		{
			s32 animSlotIndex = fwAnimManager::FindSlotFromHashKey(animDictHash).Get();
			CMoveObject& move = pObj->GetMoveObject();

			if (sceneVerifyf(move.GetClipHelper().IsClipPlaying(animSlotIndex, animHashKey), "GetObjectAnimCurrentPhase - Animation is not playing on the object"))
			{
				currPhase = move.GetClipHelper().GetPhase();

				if (!GetIsCutsceneDriven())
				{
					float fRate = 1.0f;
					
					if (m_currentState == CE_STATE_PAUSED )
					{
						move.GetClipHelper().SetPhase(m_pausePhase);

#if GTA_REPLAY
						if(m_bLoadAll == true)
						{
							// We're playing back a replay clip
							move.GetClipHelper().SetLastPhase(currPhase);

							if( move.GetClipHelper().GetPhase() != m_pausePhase )
							{
								SetForceAnimUpdateWhenPaused();
							}
						}
#endif //GTA_REPLAY

						fRate = 0.0f;
						move.GetClipHelper().SetRate(fRate);
						currPhase = m_pausePhase;
					} 
					else 
					{
						fRate = 1.0f;
						move.GetClipHelper().SetRate(fRate);
						m_pausePhase = currPhase;
					}

					for(u32 i=1; i<m_ChildObjects.GetCount(); i++){
						if (m_ChildObjects[i]){
							CMoveObject& move = m_ChildObjects[i]->GetMoveObject();
							move.GetClipHelper().SetRate(fRate);
							move.GetClipHelper().SetPhase(currPhase);
						}
					}
				}
				else
				{
					if (m_currentState == CE_STATE_PAUSED)
					{
						currPhase = m_pausePhase;
					} 
					else 
					{
						m_pausePhase = currPhase;
					}
				}

				lastPhase = move.GetClipHelper().GetLastPhase();
			}
		}
	}
}

// register/trigger the effects associated with this animation
void CCompEntity::ProcessEffects(float currPhase, float lastPhase)
{
	for(u32 animIdx=0; animIdx<m_ChildObjects.GetCount(); animIdx++)
	{
		CObject* pCurrChild = m_ChildObjects[animIdx];

		if (!pCurrChild || !pCurrChild->GetSkeleton()){
			continue;
		}

		CCompEntityModelInfo* pCEMI = static_cast<CCompEntityModelInfo*>(GetBaseModelInfo());
		modelinfoAssert(pCEMI);

		if (pCEMI){
			for(s32 j=pCEMI->GetNumOfEffects(animIdx)-1; j >=0; j--)
			{
				u32 effectId = (animIdx * 256) + j;		// assuming less than 256 effect per child...

				// handle registering this effect
				if (pCEMI->GetEffect(animIdx,j).GetStartPhase() <= currPhase && pCEMI->GetEffect(animIdx,j).GetEndPhase() > currPhase)
				{
					// register this effect
					RegisterEffect(pCEMI->GetEffect(animIdx,j), pCurrChild, effectId);
				}
		
				// handle triggering this effect
				if (pCEMI->GetEffect(animIdx,j).GetStartPhase() < currPhase && pCEMI->GetEffect(animIdx,j).GetStartPhase() >= lastPhase)
				{
					// activate this effect
					TriggerEffect(pCEMI->GetEffect(animIdx,j), pCurrChild, effectId);
				}
				
				u32 boneTag = pCEMI->GetEffect(animIdx,j).GetBoneTag();
				u32 boneIndex = pCurrChild->GetBoneIndexFromBoneTag((eAnimBoneTag)boneTag);
				Matrix34 boneMat;
				pCurrChild->GetSkeleton()->GetGlobalMtx(boneIndex, RC_MAT34V(boneMat));
	// #if DEBUG_DRAW
	// 			grcDebugDraw::Cross(boneMat.d, 2.0f, Color32(0, 0, 255, 255));
	// #endif //DEBUG_DRAW
			}
		}
	}

	return;
}

void CCompEntity::RegisterEffect(const CCompEntityEffectsData& effectData, CObject* pCurrChild, int effectId)
{
		if (effectData.GetFxType()==CE_EFFECT_PTFX)
		{
			if (effectData.GetPtFxIsTriggered()==false)
			{
				g_vfxEntity.UpdatePtFxEntityRayFire(pCurrChild, &effectData, effectId);
			}
		}
}

void CCompEntity::TriggerEffect(const CCompEntityEffectsData& effectData, CObject* pCurrChild, int UNUSED_PARAM(effectId))
{
		if (effectData.GetFxType()==CE_EFFECT_PTFX)
		{
			if (effectData.GetPtFxIsTriggered()==true)
			{
				g_vfxEntity.TriggerPtFxEntityRayFire(pCurrChild, &effectData);
			}
		}
}


#if GTA_REPLAY

void CCompEntity::CacheCurrentMapState()
{
	// Try to find out what state the map is in at this time
	if( m_CachedStartMapState == CE_STATE_INIT )
	{
		m_CachedStartMapState = CE_STATE_START;
		CCompEntityModelInfo* pCEModelInfo = static_cast<CCompEntityModelInfo*>(GetBaseModelInfo());
		sceneAssert(pCEModelInfo);
		if (pCEModelInfo->GetUseImapForStartAndEnd())
		{
			strLocalIndex endImapIndex = strLocalIndex(fwMapDataStore::GetStore().FindSlotFromHashKey(pCEModelInfo->GetEndImapFile().GetHash()));
			Assert(fwMapDataStore::GetStore().IsValidSlot(endImapIndex));
			if (fwMapDataStore::GetStore().GetIsStreamable(endImapIndex))
			{
				m_CachedStartMapState = CE_STATE_END;
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SwitchToReplayMode
// PURPOSE:		
//////////////////////////////////////////////////////////////////////////
void CCompEntity::SwitchToReplayMode()
{
	Assert(ms_bAllowedToDelete == false);

	ms_bAllowedToDelete = true;
	RemoveChildObjects();
	ms_bAllowedToDelete = false;
	RemoveCurrentAssetSetExceptFor(CE_NONE);			// clear flags for all assets
	ReleaseStreamRequests();

	m_bLoadAll = true;
	PreLoadAllAssets();
	m_currentState = CE_STATE_AWAIT_PRELOAD;
}


//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SwitchToGameplayMode
// PURPOSE:		
//////////////////////////////////////////////////////////////////////////
void CCompEntity::SwitchToGameplayMode()
{
	m_bLoadAll = false;
	ReleasePreLoadedAssets();
	Reset();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	RemoveObjects
// PURPOSE:		
//////////////////////////////////////////////////////////////////////////
void CCompEntity::ImapObjectsRemove(s32 index)
{
	if(index != -1)
	{
		CObject::Pool* m_pObjectPool = CObject::GetPool();
		for(s32 i=0; i<m_pObjectPool->GetSize(); i++)
		{
			CEntity* pEntity = m_pObjectPool->GetSlot(i);
			if(pEntity && pEntity->GetIplIndex()==index)
			{
				CObject* m_pObject = (CObject*)pEntity;

				Assertf(m_pObject->GetOwnedBy() != ENTITY_OWNEDBY_SCRIPT, "CCompEntity::ImapObjectsRemove - removing a script object. It could be an object grabbed from the world by GET_CLOSEST_OBJECT_OF_TYPE");

				if(m_pObject->GetRelatedDummy())
				{
					CDummyObject* pDummyObject = m_pObject->GetRelatedDummy();
					Assert(m_pObject->GetIplIndex() == pDummyObject->GetIplIndex());
					pDummyObject->Enable();
					m_pObject->ClearRelatedDummy();
				}
				else
				{
					Assert(m_pObject->GetOwnedBy() != ENTITY_OWNEDBY_RANDOM);
				}
				Assert(m_pObject->GetOwnedBy() != ENTITY_OWNEDBY_FRAGMENT_CACHE);
				CObjectPopulation::DestroyObject(m_pObject);
			}	
		}
	}
}

void CCompEntity::RemoveUnwanted()
{
	ms_bAllowedToDelete = true;

	RayFireReplayLoadingData *pData = ReplayRayFireManager::GetLoadingData(m_ReplayLoadIndex);
	if(pData)
	{
		fwImapGroupMgr& imapGroupMgr = INSTANCE_STORE.GetGroupSwapper();
		RemoveChildObjects();

		if( m_currentState != CE_STATE_START )
		{
			// Remove the start stuff
			if (pData->m_bUseImapForStartAndEnd)
			{
				// Remove start imap
				//Displayf("Removing imap %s", pCEMI->GetStartImapFile().GetCStr());
				s32 endIDX = imapGroupMgr.GetFinalImapIndex(pData->m_imapSwapIndex);
				ImapObjectsRemove(endIDX);
				imapGroupMgr.SetStart(pData->m_imapSwapIndex);	// Set to start state (no imap)
			}
		}

		if ( m_currentState != CE_STATE_END )
		{
			// Remove End stuff
			if(pData->m_bUseImapForStartAndEnd)
			{
				// Remove end imap
				//Displayf("Removing imap %s", pCEMI->GetEndImapFile().GetCStr());
				s32 endIDX = imapGroupMgr.GetFinalImapIndex(pData->m_imapSwapEndIndex);
				ImapObjectsRemove(endIDX);
				imapGroupMgr.SetStart(pData->m_imapSwapEndIndex);	// Set to start state (no imap)
			}
		}
	}


	ms_bAllowedToDelete = false;
}

void CCompEntity::AddWanted()
{
	u32 modelHash = 0;

	RayFireReplayLoadingData *pData = ReplayRayFireManager::GetLoadingData(m_ReplayLoadIndex);
	if(pData)
	{
		CCompEntityModelInfo *pCEMI = GetCompEntityModelInfo();
		fwImapGroupMgr& imapGroupMgr = INSTANCE_STORE.GetGroupSwapper();

		if( m_currentState == CE_STATE_START )
		{
			if (pData->m_bUseImapForStartAndEnd)
			{
				// Check if the imap we're off to is *already* set, only swap if not.
				s32 endIDX = imapGroupMgr.GetFinalImapIndex(pData->m_imapSwapIndex);
				INSTANCE_DEF* pDef = INSTANCE_STORE.GetSlot(strLocalIndex(endIDX));
				if (pDef && pDef->IsLoaded())
				{
					fwStreamedSceneGraphNode* pNode = pDef->m_pObject->GetSceneGraphNode();
					if (pNode && !pNode->GetIsInWorld())
					{
						// Add the start imap
						NOTFINAL_ONLY(Displayf("Adding imap %s", pCEMI->GetStartImapFile().GetCStr());)
						imapGroupMgr.SetEnd(pData->m_imapSwapIndex);	// Set to end state (start imap)
					}
				}
			} 
			else 
			{
				modelHash = pCEMI->GetStartModelHash();
				CreateNewChildObject(modelHash);
			}
		}
		else if( m_currentState == CE_STATE_ANIMATING )
		{
			// Add the anim stuff
			for(u32 i=0; i<pCEMI->GetNumOfAnims(); i++)
			{
				CCompEntityAnims& animData = pCEMI->GetAnims(i);
				modelHash = atHashString(animData.GetAnimatedModel());
				CreateNewChildObject(modelHash);
			}

			TriggerObjectAnim();

			for(u32 i = 0; i<m_ChildObjects.GetCount(); i++)
			{
				m_ChildObjects[i]->m_nDEflags.bForcePrePhysicsAnimUpdate = true;
			}
		}
		else if ( m_currentState == CE_STATE_END )
		{
			if (pCEMI->GetUseImapForStartAndEnd())
			{
				// Check if the imap we're off to is *already* set, only swap if not.
				s32 endIDX = imapGroupMgr.GetFinalImapIndex(pData->m_imapSwapEndIndex);
				INSTANCE_DEF* pDef = INSTANCE_STORE.GetSlot(strLocalIndex(endIDX));
				if (pDef && pDef->IsLoaded())
				{
					fwStreamedSceneGraphNode* pNode = pDef->m_pObject->GetSceneGraphNode();
					if (pNode && !pNode->GetIsInWorld())
					{
						// Add the End iMap
						NOTFINAL_ONLY(Displayf("Adding imap %s", pCEMI->GetEndImapFile().GetCStr());)
						imapGroupMgr.SetEnd(pData->m_imapSwapEndIndex);	// Set to end state
					}
				}
			} 
			else 
			{
				modelHash = pCEMI->GetEndModelHash();
				CreateNewChildObject(modelHash);
			}
		}
	}
}

void CCompEntity::RemoveChildObjects()
{
	// cleanup all objects
	for(u32 i=0; i<m_ChildObjects.GetCount(); i++)
	{
		if (m_ChildObjects[i])
		{
			m_ChildObjects[i]->DeleteDrawable();
			CObjectPopulation::DestroyObject(m_ChildObjects[i]);
		}
	}
	m_ChildObjects.Reset();
}

void CCompEntity::PreLoadAllAssets()
{
	m_bPreLoadComplete = false;
	CCompEntityModelInfo *pCEMI = GetCompEntityModelInfo();
	CPacketRayFirePreLoad loadInfo( true, GetUniqueID(), pCEMI );
	m_ReplayLoadIndex = ReplayRayFireManager::Load(loadInfo.GetInfo());
	m_currentState = CE_STATE_AWAIT_PRELOAD;
}

bool CCompEntity::HasPreLoadCompleted()
{
	return ReplayRayFireManager::HasLoaded(m_ReplayLoadIndex);
}

void CCompEntity::ReleasePreLoadedAssets()
{
	Assert(ms_bAllowedToDelete == false);

	ms_bAllowedToDelete = true;
	RemoveChildObjects();
	ms_bAllowedToDelete = false;

	ReplayRayFireManager::Release(m_ReplayLoadIndex, GetUniqueID());
	m_ReplayLoadIndex = -1;
	m_bPreLoadComplete = false;
}

u32	 CCompEntity::GetUniqueID()
{
	s32	positionAsInt[3];

	CCompEntityModelInfo *pCEMI = GetCompEntityModelInfo();
	Vector3 position = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
	position *= 100.0f;
	positionAsInt[0] = (s32)(position.x);
	positionAsInt[1] = (s32)(position.y);
	positionAsInt[2] = (s32)(position.z);

	return CLODLightManager::hash( (const u32*)positionAsInt, 3, pCEMI->GetModelNameHash() );
}

// Required because script intercept the state machine so I've no idea what the state is going to be, so I liberally sprinkle
void	CCompEntity::WritePreLoadPacketIfRequired(bool loading)
{
	if( !m_bLoadAll && CReplayMgr::ShouldRecord() )
	{
		if( loading && !m_bPreLoadPacketSent )
		{
			CReplayMgr::RecordFx<CPacketRayFirePreLoad>( CPacketRayFirePreLoad( true, GetUniqueID(), GetCompEntityModelInfo() ));
			m_bPreLoadPacketSent = true;
		}
		else if( !loading )
		{
			// We don't need to worry about unloading preload packet since it comes from the destructor
			CReplayMgr::RecordFx<CPacketRayFirePreLoad>( CPacketRayFirePreLoad( false, GetUniqueID(), GetCompEntityModelInfo() ));
		}
	}
}

#endif	//GTA_REPLAY

#if __DEV
template<> void fwPool<CCompEntity>::PoolFullCallback()
{
	INSTANCE_STORE.PrintLoadedObjs();
}
#endif

#if __BANK

void CCompEntity::ForceAllCB(void){

	if (g_debugCompEntity){
		g_debugCompEntity->SetModifiedBy("Widget");
		switch(g_CompEntityDebugEvent){
			case 1:
				g_debugCompEntity->SetToStarting();
				break;
			case 2:
				g_debugCompEntity->SetToPriming();
				break;
			case 3:
				g_debugCompEntity->TriggerAnim();
				break;
			case 4:
				g_debugCompEntity->SetToEnding();
				break;
			case 5:
				g_debugCompEntity->Reset();
				break;
			case 6:
				g_debugCompEntity->PauseAnimAt(0.0f);
				break;
			case 7:
				g_debugCompEntity->ResumeAnim();
				break;
			default:
				break;
		}
	}
	g_CompEntityDebugEvent = 0;
}

void HandlePauseCB(void){
	if (g_debugCompEntity)
	{
		g_debugCompEntity->PauseAnimAt(g_compEntityAnimPausePhase);
	}
}

void ScrubAnimCB(void) {
	if (g_debugCompEntity)
	{
		g_debugCompEntity->SetAnimTo(g_compEntityAnimDislayPhase);
	}
}

void ActivatePickerCB(void){

	fwPickerManagerSettings RayfirePickerManagerSettings(INTERSECTING_ENTITY_PICKER, true, false, 0, false);		// no mask -> no picking!
	g_PickerManager.SetPickerManagerSettings(&RayfirePickerManagerSettings);

	g_PickerManager.SetEnabled(true);
	g_PickerManager.TakeControlOfPickerWidget("Rayfire picker");
	g_PickerManager.ResetList(false);

	CCompEntity* pComposite = NULL;
	s32 poolSize=CCompEntity::GetPool()->GetSize();
	s32 base = 0;

	while(base < poolSize)
	{
		pComposite = CCompEntity::GetPool()->GetSlot(base);
		if(pComposite)
		{
			g_PickerManager.AddEntityToPickerResults(pComposite, false, false);
		}	
		base++;
	}

	g_bDisplayCompEntityStates = true;
}

const char*	compEntityStates[] = { 
	"CE_STATE_INIT",	
	"CE_STATE_SYNC_STARTING", 
	"CE_STATE_STARTING",
	"CE_STATE_START",
	"CE_STATE_PRIMING",
	"CE_STATE_PRIMED",
	"CE_STATE_START_ANIM",
	"CE_STATE_ANIMATING",
	"CE_STATE_SYNC_ENDING",
	"CE_STATE_ENDING",
	"CE_STATE_END",
	"CE_STATE_RESET",
	"CE_STATE_PAUSED",
	"CE_STATE_RESUME",
	"CE_STATE_PRIMING_PRELUDE",
	"CE_STATE_ABANDON",

#if GTA_REPLAY

	"CE_STATE_AWAIT_PRELOAD",
	"CE_STATE_PRELOADED",

#endif	//GTA_REPLAY

};

char assetSetStates[5] = "----";
void CCompEntity::GenerateDebugInfo(char* pString)
{
	for(u32 i = CE_START_ASSETS; i <= CE_END_ASSETS; i++){
		if (IsAssetSetReady((eCompEntityAssetSet)i)){
			assetSetStates[i] = '+';
		} else {
			assetSetStates[i] = '-';
		}
	}

	u32 state = GetState();
	sprintf(pString,"%s : [%s] (%c)", compEntityStates[state], assetSetStates, GetIsActive() ? '!':' ');
}

void CCompEntity::SetModifiedBy(const char* pString)
{ 
	m_modifiedBy = pString;
}

void DumpModels(atArray<u32>& modelIdxArray)
{
	u32 physicalSize = 0;
	u32 virtualSize = 0;

	char displayString[255];
	for(u32 i = 0; i< modelIdxArray.GetCount(); i++)
	{
		fwModelId modelId((strLocalIndex(modelIdxArray[i])));
		if (modelId.IsValid())
		{
			CModelInfo::GetObjectAndDependenciesSizes(modelId, virtualSize, physicalSize);

			sprintf(displayString, "%s : phys: %d, virt: %d", CModelInfo::GetBaseModelInfoName(modelId), physicalSize, virtualSize);
			grcDebugDraw::AddDebugOutput(displayString);
		}
	}
}

void CCompEntity::DebugRender(void){
#if __BANK
	// Print name of ped model above head.
	if(g_bDisplayCompEntityStates)
	{
		CCompEntity* pComposite = NULL;
		s32 poolSize=CCompEntity::GetPool()->GetSize();
		s32 base = 0;

		while(base < poolSize)
		{
			pComposite = CCompEntity::GetPool()->GetSlot(base);
			if(pComposite)
			{
				//u32 state = pComposite->GetState();
				CRGBA compColour(100,255,128,255);
				if (pComposite == g_debugCompEntity){
					compColour.SetRed(255);
				}

				char displayString[255];
				pComposite->GenerateDebugInfo(displayString);
				grcDebugDraw::Text(VEC3V_TO_VECTOR3(pComposite->GetTransform().GetPosition()) + Vector3(0.f,0.f,1.3f), compColour, displayString);
			}	
			base++;
		}
	}

	if (g_bDisplayMemUsage)
	{
		CCompEntity* pComposite = NULL;
		s32 poolSize=CCompEntity::GetPool()->GetSize();
		s32 base = 0;

		grcDebugDraw::AddDebugOutput(" ");
		grcDebugDraw::AddDebugOutput("Memory Use for rayfire sequences");
		grcDebugDraw::AddDebugOutput("--------------------------------");

		while(base < poolSize)
		{
			pComposite = CCompEntity::GetPool()->GetSlot(base);
			if(pComposite)
			{
				// display name
				char displayString[255];
				char debugInfo[128];
				pComposite->GenerateDebugInfo(debugInfo);

				sprintf(displayString, "rayfire: %s  :  %s", pComposite->GetModelName(), debugInfo);
				grcDebugDraw::AddDebugOutput(displayString);

				Vec3V pos = pComposite->GetTransform().GetPosition();
				Vector3 delta = camInterface::GetPos() - RCC_VECTOR3(pos);
				sprintf(displayString, "Distance : %.1fm",delta.Mag());
				grcDebugDraw::AddDebugOutput(displayString);

				sprintf(displayString, "Last modified by : %s", pComposite->GetModifiedBy().c_str());
				grcDebugDraw::AddDebugOutput(displayString);

				atArray<u32> modelIdxArray;
				modelIdxArray.Reset();
				modelIdxArray.Reserve(16);

				// display start costs (if loaded)
				if ((pComposite->GetState() == CE_STATE_START) || (pComposite->GetState() == CE_STATE_SYNC_STARTING)){
					if (pComposite->GetCompEntityModelInfo()->GetUseImapForStartAndEnd()){
						// display name of group
						strLocalIndex slotIdx = strLocalIndex(g_MapDataStore.FindSlotFromHashKey(pComposite->GetCompEntityModelInfo()->GetStartImapFile()));
						sprintf(displayString, "start IPL group : %s", g_MapDataStore.GetName(slotIdx));
						grcDebugDraw::AddDebugOutput(displayString);

					} else {
						// dump stats for the start entity (collisions, model, textures)
						fwModelId modelId;
						CModelInfo::GetBaseModelInfoFromHashKey(pComposite->GetCompEntityModelInfo()->GetStartModelHash(), &modelId);
						modelIdxArray.Push((u32)modelId.GetModelIndex());
						DumpModels(modelIdxArray);
					}
				}

				// display anim costs (if loaded)
				// loop over anim models & display costs
				// display costs for each animation entity (collisions, model, texures)
				if ((pComposite->GetState() == CE_STATE_ANIMATING) || pComposite->GetState() == CE_STATE_PAUSED || pComposite->GetState() == CE_STATE_PRIMED)
				{
					for(u32 i=0; i<pComposite->GetChildObjects().GetCount(); i++)
					{
						if (pComposite->GetChild(i))
						{
							modelIdxArray.Push(pComposite->GetChild(i)->GetModelIndex());
						}
					}
					DumpModels(modelIdxArray);
				}
				// display costs for each animation clip

				// display end costs (if loaded)
				if ((pComposite->GetState() == CE_STATE_END) || (pComposite->GetState() == CE_STATE_SYNC_ENDING)){
					if (pComposite->GetCompEntityModelInfo()->GetUseImapForStartAndEnd()){
						// display name of group
						strLocalIndex slotIdx = strLocalIndex(g_MapDataStore.FindSlotFromHashKey(pComposite->GetCompEntityModelInfo()->GetEndImapFile()));
						sprintf(displayString, "end IPL group : %s", g_MapDataStore.GetName(slotIdx));
						grcDebugDraw::AddDebugOutput(displayString);
					} else {
						// dump stats for the end entity (collisions, model, textures)
						fwModelId modelId;
						CModelInfo::GetBaseModelInfoFromHashKey(pComposite->GetCompEntityModelInfo()->GetEndModelHash(), &modelId);
						modelIdxArray.Push((u32)modelId.GetModelIndex());
						DumpModels(modelIdxArray);
					}
				}

				grcDebugDraw::AddDebugOutput("-------------------");
			}	
			base++;
		}
	}
#endif
}

const char* pCompEntityDebugEvents[8] = {"<select event>","GOTO START", "PRIME ANIM","TRIGGER ANIM","GOTO END","RESET","PAUSE","RESUME"};

void CCompEntity::AddWidgets(bkBank* BANK_ONLY(pBank)){
	if (pBank){
		pBank->PushGroup("Composite Entities");
		pBank->AddButton("Rayfire picker", ActivatePickerCB);
		pBank->AddCombo("Send Event", &g_CompEntityDebugEvent, 8, pCompEntityDebugEvents, ForceAllCB);
		pBank->AddSlider("Pause at frame", &g_compEntityAnimPausePhase, 0.0f, 0.98f, 0.01f, HandlePauseCB);
		pBank->AddSlider("Current frame", &g_compEntityAnimDislayPhase, 0.0f,  0.98f, 0.01f,  ScrubAnimCB);
		pBank->AddSlider("Display selected anim model", &g_selectedAnimModel, -1, 8, 1, SelectAnimToDisplayCB);
		pBank->AddToggle("Display states", &g_bDisplayCompEntityStates);
		pBank->AddToggle("Display current mem usage",&g_bDisplayMemUsage);
#if GTA_REPLAY
		pBank->AddToggle("Load All Assets (Replay)", &g_bLoadAll);
#endif
		pBank->PopGroup();
	}
}
#endif //__BANK




