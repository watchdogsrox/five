/////////////////////////////////////////////////////////////////////////////////
// FILE :		PedFactory.cpp
// PURPOSE :	The factory class for peds and dummy peds.
// AUTHOR :		Obbe.
//				Adam Croston.
// CREATED :	
/////////////////////////////////////////////////////////////////////////////////
#include "Peds\pedfactory.h"

// rage includes
#include "debug/Debug.h"
#include "vector/matrix34.h"

// game includes
#include "animation/MovePed.h"
#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/helpers/Frame.h"
#include "camera/viewports/ViewportManager.h"
#include "frontend/MiniMap.h"
#include "game/ModelIndices.h"
#include "fwanimation\directorcomponentragdoll.h"
#include "ModelInfo\pedmodelinfo.h"
#include "network/live/livemanager.h"
#include "network/Network.h"
#include "network/NetworkInterface.h"
#include "Network/Objects/NetworkObjectPopulationMgr.h"
#include "network/players/NetGamePlayer.h"
#include "peds/PedHelmetComponent.h"
#include "Peds\PedIntelligence.h"
#include "Peds\pedtype.h"
#include "Peds/PedMoveBlend/PedMoveBlendOnFoot.h"
#include "Peds/PedMoveBlend/PedMoveBlendInWater.h"
#include "Peds/PedMoveBlend/PedMoveBlendOnSkis.h"
#include "peds/Ped.h"
#include "peds/PedCloth.h"
#include "peds/pedpopulation.h"
#include "Peds\PedWeapons\PedTargeting.h"
#include "Peds/rendering/PedProps.h"
#include "peds/rendering/PedVariationDS.h"
#include "peds/rendering/PedVariationStream.h"
#include "peds/rendering/PedVariationPack.h"
#include "Peds\QueriableInterface.h"
#include "Peds/pedpopulation.h"			// For CPedPopulation.
#include "physics/physics.h"
#include "pickups\PickupManager.h"
#include "Task\Default\TaskWander.h"
#include "Task\General\TaskBasic.h"
#include "scene/world/GameWorld.h"
#include "script/handlers/GameScriptEntity.h"
#include "script/script.h"
#include "shaders/CustomShaderEffectPed.h"
#include "streaming/populationstreaming.h"
#include "streaming/streaming.h"
#include "system/memmanager.h"
#include "vfx/misc/Fire.h"
#include "vfx/particles/PtFxManager.h"
#include "weapons/inventory/PedInventoryLoadOut.h"
#include "weapons/Weapon.h"

#include "Control/replay/ReplaySettings.h"

#if GTA_REPLAY
#include "Control/replay/replay.h"
#endif

AI_OPTIMISATIONS()

#define LARGEST_MOVEBLENDER		MAX(sizeof(CPedMoveBlendInWater), MAX(sizeof(CPedMoveBlendOnFoot), sizeof(CPedMoveBlendOnSkis)))

// The size of these pools will now be set through the InitPool() calls, when the value
// of MAXNOOFPEDS has been determined.

#if COMMERCE_CONTAINER
	FW_INSTANTIATE_CLASS_POOL_NO_FLEX_SPILLOVER(CPed, CONFIGURED_FROM_FILE, 0.26f, atHashString("Peds",0x8da12117));
	FW_INSTANTIATE_CLASS_POOL_NO_FLEX_SPILLOVER(CPedIntelligence, CONFIGURED_FROM_FILE, 0.26f, atHashString("PedIntelligence",0x394ac584));
#else
	FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CPed, CONFIGURED_FROM_FILE, 0.26f, atHashString("Peds",0x8da12117));
	FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CPedIntelligence, CONFIGURED_FROM_FILE, 0.26f, atHashString("PedIntelligence",0x394ac584));
#endif

PARAM(temppeddebug, "prints some temporary ped debug output");

dev_float COP_SHOT_RATE = 0.3f;


CPedFactory*	CPedFactory::ms_pInstance					= NULL;

#if __DEV
bool			CPedFactory::ms_bWithinPedFactory			= false;
bool			CPedFactory::ms_bInDestroyPedFunction		= false;
bool			CPedFactory::ms_bInDestroyPlayerPedFunction	= false;
#endif // __DEV

CPedFactory::sDestroyedPed CPedFactory::ms_reuseDestroyedPedArray[MAX_DESTROYED_PEDS_CACHED];
bool			CPedFactory::ms_reuseDestroyedPeds			= true;		// when destroying peds we cache them for a while and reuse them when the same type is spawned again
u32				CPedFactory::ms_reuseDestroyedPedCacheTime	= 10000;	// time in ms how long we should keep cached peds around
u32				CPedFactory::ms_reuseDestroyedPedCount		= 0;		// keep track of how many peds we have cached

#if __BANK
bool			CPedFactory::ms_reuseDestroyedPedsDebugOutput = false;
RegdPed			CPedFactory::ms_pLastCreatedPed;
bool			CPedFactory::ms_bLogCreatedPeds = false;
bool			CPedFactory::ms_bLogDestroyedPeds = false;
u32				CPedFactory::ms_iCreatedPedCount = 0;
u32				CPedFactory::ms_iDestroyedPedCount = 0;
#endif // __BANK


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CPedFactory
// PURPOSE :	Constructor.  It also initializes the various pools used by objects
//				created within the factory.
// PARAMETERS :	None
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
CPedFactory::CPedFactory() :
m_pLocalPlayer(NULL)
{
	// This used to be a CompileTimeAssert() before, when MAXNOOFPEDS was known
	// at compile time.
	Assert(MAX_NUM_NETOBJPEDS <= MAXNOOFPEDS);
	CPed::InitPool( MAXNOOFPEDS, MEMBUCKET_GAMEPLAY );
#if __DEV
	CPed::GetPool()->RegisterPoolCallback(CPed::PoolFullCallback);
#endif // __DEV
	CPedTargetting::InitPool( MEMBUCKET_GAMEPLAY );

#if (__XENON || __PS3)
	#define TASKINFO_HEAP_SIZE (40 << 10)
#else
	// see MAX_NUM_TASK_INFOS in QueriableInterface.h
	// 268 peds * 8 task infos per ped * average TaskInfo size of 90 bytes (overestimate) = 192 kB
	#define TASKINFO_HEAP_SIZE ((40 << 10) * 10)
#endif

	sysMemStartFlex();
	CTaskInfo::InitPool(TASKINFO_HEAP_SIZE, MAX_NUM_TASK_INFOS, MEMBUCKET_GAMEPLAY);
	sysMemEndFlex();

#if !__NO_OUTPUT && !__FINAL
	CTaskInfo::GetPool()->RegisterPoolCallback(CTaskInfo::PoolFullCallback);
#endif

	CChatHelper::InitPool( MEMBUCKET_GAMEPLAY );
	// CChatHelper::GetPool()->SetCanDealWithNoMemory(true); // Existing code already checks number of slots free before calling NEW

    TaskSequenceInfo::InitPool( MEMBUCKET_GAMEPLAY );
#if __BANK
	TaskSequenceInfo::GetPool()->RegisterPoolCallback(TaskSequenceInfo::PoolFullCallback);
#endif
	// CPedTargetting::GetPool()->SetCanDealWithNoMemory(true);
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	~CPedFactory
// PURPOSE :	Destructor.  It also shuts down the various pools used by objects
//				created within the factory.
// PARAMETERS :	None
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
CPedFactory::~CPedFactory()
{
	CPed::ShutdownPool();
	CPedTargetting::ShutdownPool();
	CTaskInfo::ShutdownPool();
	CChatHelper::ShutdownPool();

    TaskSequenceInfo::ShutdownPool();

	ClearDestroyedPedCache();
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CreateFactory
// PURPOSE :	Create the entire project specific factory.
// PARAMETERS :	None
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPedFactory::CreateFactory()
{
	Assert(ms_pInstance == NULL);
	ms_pInstance = rage_new CPedFactory;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	DestroyFactory
// PURPOSE :	Default destroy entire factory. Individual projects
//				can override this.
// PARAMETERS :	None
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPedFactory::DestroyFactory()
{
	Assert(ms_pInstance);

	delete ms_pInstance;
	ms_pInstance = NULL;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CreatePed
// PURPOSE :	This function allow us to create a project specific ped.
// PARAMETERS :	pedControlInfo - How is this ped controlled (is it network, player, etc.)
//				modelIndex - the model to use for this ped.
//				pMat - the placement and orientation matrix to use for the ped.
//				bShouldBeCloned - if set to true and a network game is running,
//					this ped will be registered with the network object manager
//					so that it will cloned and synchronized on all the remote
//					machines involved in the network game.
//				bCreatedByScript - 
//				bFailSilentlyIfOutOfPeds - if true, suppress asserts if we don't have space in the pool or in the cache,
//					for use as a replacement to code that previously checked GetNoOfFreeSpaces().
// RETURNS :	The new ped.
/////////////////////////////////////////////////////////////////////////////////
CPed* CPedFactory::CreatePed(const CControlledByInfo& pedControlInfo, fwModelId modelId, const Matrix34 *pMat, bool bApplyDefaultVariation, bool bShouldBeCloned, bool bCreatedByScript,
		bool bFailSilentlyIfOutOfPeds, bool bScenarioPedCreatedByConcealeadPlayer)
{
	if(pedControlInfo.IsControlledByLocalOrNetworkPlayer())
	{
		Assert(0); // must use CreatePlayer() instead
	}

	if (NetworkInterface::IsGameInProgress() && bShouldBeCloned && !bCreatedByScript)
	{
		NetworkObjectType objectType = static_cast<NetworkObjectType>(pedControlInfo.IsControlledByLocalOrNetworkPlayer() ? NET_OBJ_TYPE_PLAYER : NET_OBJ_TYPE_PED);

		if(!CNetworkObjectPopulationMgr::CanRegisterLocalObjectOfType(objectType, false))
		{
			return NULL;
		}
	}

	const CControlledByInfo localAiControl(false, false);

#if 0
	CPed* pPed = bCreatedByScript ? NULL : CreatePedFromDestroyedCache(localAiControl, modelId, pMat, bShouldBeCloned);
#else
    CPed* pPed = NULL;
#endif

	if(!Verifyf(FRAGCACHEMGR->CanGetNewCacheEntries(1),"Unable to create ped because we have no available cache entries."))
	{
		return NULL;
	}

#if !__FINAL
	if (!gbAllowPedGeneration)
	{
		return pPed;
	}
#endif //!__FINAL


#if (__ASSERT && GTA_REPLAY)
	if( !CReplayMgr::IsReplayInControlOfWorld() )
#endif //(__ASSERT && GTA_REPLAY)
	{
		Assertf(!pMat || !IsCloseAll(RCC_VEC3V(pMat->d), Vec3V(V_ZERO), Vec3V(V_FLT_SMALL_1)),
			"Ped is being created very close to the origin: (%.f, %f, %f);  bCreatedByScript = %s", 
			pMat->d.x, pMat->d.y, pMat->d.z, bCreatedByScript ? "true":"false");
	}

	CPedModelInfo* pModelInfo = ((CPedModelInfo*)CModelInfo::GetBaseModelInfo(modelId));
	Assert(pModelInfo);
	if (!pPed)
	{
		ePedType pedType = pModelInfo->GetDefaultPedType();
		switch(pedType)
		{
		case PEDTYPE_COP:
			pPed=SetUpAsCopPed(localAiControl, modelId.GetModelIndex(), pMat, bCreatedByScript, bFailSilentlyIfOutOfPeds);
			break;

		default:
			pPed=SetUpAsCivilianPed(localAiControl, modelId.GetModelIndex(), pMat, bCreatedByScript, bFailSilentlyIfOutOfPeds);
			break;
		}

		if (!pPed)
		{
			return NULL;
		}

		pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_CreatedByFactory, true );

		if (bScenarioPedCreatedByConcealeadPlayer)
		{
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_CreatedByConcealedPlayer, true);
		}

		// ******* NETWORK REGISTRATION HAS TO BE DONE BEFORE ANY FURTHER STATE CHANGES ON THE PED! *******************

		if (bShouldBeCloned && NetworkInterface::IsGameInProgress())
		{
			bool bRegistered = NetworkInterface::RegisterObject(pPed, 0, bCreatedByScript ? CNetObjGame::GLOBALFLAG_SCRIPTOBJECT : 0);

			// ped could not be registered as a network object (due to maximum number of ped objects being exceeded
			// scripted peds are queued to be created
			if (!bRegistered && !bCreatedByScript)
			{
				DestroyPedInternal(pPed);
				return NULL;
			}
		}
	}

	pPed->m_nPhysicalFlags.bNotToBeNetworked = !bShouldBeCloned;

#if __BANK
    if(pPed->m_nPhysicalFlags.bNotToBeNetworked && NetworkInterface::IsGameInProgress() && !pedControlInfo.IsControlledByNetwork())
    {
		if ((Channel_ai.FileLevel >= DIAG_SEVERITY_DEBUG2) || (Channel_ai.TtyLevel >= DIAG_SEVERITY_DEBUG2))
		{
	        aiDebugf2("Creating a none networked ped in a network game! Ped = %p", pPed);
	        sysStack::PrintStackTrace();
		}
    }
#endif // __BANK

	if (bApplyDefaultVariation)
		VariationInitialisation(pPed);

	if (bCreatedByScript)
		CPedInventoryLoadOutManager::SetDefaultLoadOut(pPed);

	pPed->SetStubbleGrowth(fwRandom::GetRandomNumberInRange(0.f, pModelInfo->GetStubble()));

	BANK_ONLY(ms_pLastCreatedPed = pPed);

	pPed->GetLodData().SetResetDisabled(true);
	pPed->SetAlpha(LODTYPES_ALPHA_VISIBLE);

#if __BANK
	if(pMat)
	{
		pPed->m_vecInitialSpawnPosition = VECTOR3_TO_VEC3V(pMat->GetVector(3));
	}
#endif // __BANK

#if __BANK
	LogCreatedPed(pPed, "CPedFactory::CreatePed()", 0, Color_green);
#endif // __BANK

	REPLAY_ONLY(CReplayMgr::OnCreateEntity(pPed));
	return pPed;
}

#if __BANK
void CPedFactory::LogCreatedPed(CPed * pPed, const char * pCallerDesc, s32 iTextOffsetY, Color32 iTextCol)
{
	if(ms_bLogCreatedPeds && pPed)
	{
		CViewport* gameViewport = gVpMan.GetGameViewport();
		if(gameViewport)
		{
			static const int sizeoftext = 128;
			char text[sizeoftext];

			const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			const Vector3 vCameraPos = VEC3V_TO_VECTOR3(gameViewport->GetGrcViewport().GetCameraPosition());
			const float fDist = (vCameraPos - vPedPos).Mag();

			formatf(text, sizeoftext, "%i : %s", ms_iCreatedPedCount, pPed->GetModelName());
			grcDebugDraw::Text(vPedPos, iTextCol, 0, -grcDebugDraw::GetScreenSpaceTextHeight() + iTextOffsetY, text, false, (s32)(10.0f / fwTimer::GetTimeStep()));
			grcDebugDraw::Sphere(vPedPos, 0.25f, Color_green4, false, (s32)(10.0f / fwTimer::GetTimeStep()));

			if (NetworkInterface::IsGameInProgress() && pPed->GetNetworkObject())
			{
				formatf(text, sizeoftext, " NETWORKNAME: %s,",pPed->GetNetworkObject()->GetLogName());
			}
			else
			{
				formatf(text, sizeoftext, " DEBUGNAME: %s,",pPed->GetDebugNameFromObjectID());
			}

			bool bOnScreen = CPedPopulation::IsCandidateInViewFrustum(vPedPos, 1.0f, CPedPopulation::SP_SpawnValidityCheck_GetCurrentParams().fAddRangeInViewMax, CPedPopulation::SP_SpawnValidityCheck_GetCurrentParams().fAddRangeInViewMin);

			Displayf("**************************************************************************************************");
			Displayf("%s : %i", pCallerDesc, ms_iCreatedPedCount);
			Displayf("FRAME: %d, NAME : %s,%s POPTYPE: %s, PEDTYPE: %s, DISTANCE : %.1f, POSITION : %.1f, %.1f, %.1f, CAM POS: %.1f, %.1f, %.1f, PED : 0x%p, bAtStartup : %s, bInstantFill : %s, bOnScreen: %s, bIsClone: %s", fwTimer::GetFrameCount(), pPed->GetModelName(), text, CTheScripts::GetPopTypeName(pPed->PopTypeGet()), CPedType::GetPedTypeNameFromId(pPed->GetPedType()), fDist, vPedPos.x, vPedPos.y, vPedPos.z, vCameraPos.x, vCameraPos.y, vCameraPos.z, pPed, CPedPopulation::ShouldUseStartupMode() ? "true":"false", CPedPopulation::GetInstantFillPopulation() ? "true":"false", bOnScreen ? "true":"false", pPed->IsNetworkClone() ? "true":"false");
			if (scrThread::GetActiveThread())
				scrThread::GetActiveThread()->PrintStackTrace();
			sysStack::PrintStackTrace();
			Displayf("**************************************************************************************************");

			ms_iCreatedPedCount++;
		}
	}
}
#endif


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CreatePedFromSource
// PURPOSE :	Creates a new ped with the variation data from the specified source ped
// PARAMETERS :	pedControlInfo - How is this ped controlled (is it network, player, etc.)
//				modelIndex - the model to use for this ped.
//				pMat - the placement and orientation matrix to use for the ped.
//				pSourcePed - ped to copy variation data from
//				bStallForAssetLoad - if true the streamed assets will be loaded synchronously and
//					ped will be ready for render when this function returns
//				bShouldBeCloned - if set to true and a network game is running,
//					this ped will be registered with the network object manager
//					so that it will cloned and synchronized on all the remote
//					machines involved in the network game.
//				bCreatedByScript - 
// RETURNS :	The new ped, which might not be ready for rendering.
/////////////////////////////////////////////////////////////////////////////////
CPed* CPedFactory::CreatePedFromSource(const CControlledByInfo& pedControlInfo, fwModelId modelId, const Matrix34 *pMat, const CPed* pSourcePed, bool bStallForAssetLoad, bool bShouldBeCloned, bool bCreatedByScript)
{
	// ensure that the model is loaded
	if (!CModelInfo::HaveAssetsLoaded(modelId))
	{
		CModelInfo::RequestAssets(modelId, STRFLAG_FORCE_LOAD | STRFLAG_PRIORITY_LOAD);
		CStreaming::LoadAllRequestedObjects(true);
	}

	if (!CModelInfo::HaveAssetsLoaded(modelId))
	{
		Assertf(false, "Ped model %d hasn't loaded successfully, can't create ped!", modelId.GetModelIndex());
		return NULL;
	}

	CPed* pPed = CreatePed(pedControlInfo, modelId, pMat, (pSourcePed == NULL), bShouldBeCloned, bCreatedByScript);
	if (!pPed)
		return NULL;

	if (pSourcePed)
		CopyVariation(pSourcePed, pPed, bStallForAssetLoad);

	return pPed;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ClonePed
// PURPOSE :	Creates a new ped by cloning the given one.
// PARAMETERS :	source - ped instance to clone
// RETURNS :	new ped instance cloned from the source
/////////////////////////////////////////////////////////////////////////////////
CPed* CPedFactory::ClonePed(const CPed* source, bool bRegisterAsNetworkObject, bool bLinkBlends, bool bCloneCompressedDamage)
{
	CPed* newPed = NULL;
	if (source)
	{

		// Prevent streamed peds being cloned if we can't store their CPedStreamRenderGfx (pool full)
		CPedModelInfo* pModelInfo = static_cast<CPedModelInfo*>(source->GetBaseModelInfo());
		if( pModelInfo && pModelInfo->GetIsStreamedGfx() )
		{
			// If the number of requests is >= the space we have for them, we can't clone
			if( CPedStreamRequestGfx::GetPool()->GetNoOfUsedSpaces() >= CPedStreamRenderGfx::GetPool()->GetNoOfFreeSpaces() )
			{
				return newPed;	// Which is currently NULL!
			}
		}

		Matrix34 tempMat;
		tempMat.Identity();
		tempMat.d = VEC3V_TO_VECTOR3(source->GetTransform().GetPosition()); 
		const CControlledByInfo dummyControllInfo(false, false);

		newPed = CPedFactory::GetFactory()->CreatePedFromSource(dummyControllInfo, source->GetModelId(), &tempMat, source, false, bRegisterAsNetworkObject, true);

		if (Verifyf(newPed, "CLONE_PED - Couldn't create a new ped"))
		{
			newPed->SetLodDistance(CPed::ms_uMissionPedLodDistance);

			newPed->ActivatePhysics();

			newPed->m_nPhysicalFlags.bNotDamagedByAnything = source->m_nPhysicalFlags.bNotDamagedByAnything;
			newPed->SetPedConfigFlag(CPED_CONFIG_FLAG_DisablePlayerLockon, source->GetPedConfigFlag(CPED_CONFIG_FLAG_DisablePlayerLockon));

			newPed->SetHealth(source->GetHealth(), true);
			newPed->SetEndurance(source->GetEndurance(), true);
			newPed->SetArmour(source->GetArmour());

			// We need to ensure the crew emblem is taken from the ped so it does not fall back to the local player
			newPed->SetSavedClanId(source->GetSavedClanId());
			newPed->GetPedDrawHandler().GetVarData().SetOverrideCrewLogoTxdHash(source->GetPedDrawHandler().GetVarData().GetOverrideCrewLogoTxdHash());
			newPed->GetPedDrawHandler().GetVarData().SetOverrideCrewLogoTexHash(source->GetPedDrawHandler().GetVarData().GetOverrideCrewLogoTexHash());

			newPed->CloneHeadBlend(source, bLinkBlends);

			CGameWorld::Add(newPed, source->GetInteriorLocation());

			newPed->GetPortalTracker()->RequestRescanNextUpdate();
			newPed->GetPortalTracker()->Update(VEC3V_TO_VECTOR3(newPed->GetTransform().GetPosition()));
			newPed->CloneDamage(source, bCloneCompressedDamage);
		}
	}

	return newPed;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ClonePedToTarget
// PURPOSE :	Clones settings from source to target ped
// PARAMETERS :	source - ped instance to clone from
//				target - ped instance to clone to
// RETURNS :	
/////////////////////////////////////////////////////////////////////////////////
void CPedFactory::ClonePedToTarget(const CPed* source, CPed* target, bool bCloneCompressedDamage)
{
	if (source && target)
	{
		// delete old head blend data so new variation doesn't trigger a reblend
		CPedHeadBlendData* oldBlendData = target->GetExtensionList().GetExtension<CPedHeadBlendData>();
		if (oldBlendData)
			target->GetExtensionList().Destroy(CPedHeadBlendData::GetAutoId());

		target->CloneHeadBlend(source, false);
		CopyVariation(source, target, false);
		target->ReleaseDamageSet();
		target->CloneDamage(source, bCloneCompressedDamage);

		// We need to ensure the crew emblem is taken from the ped so it does not fall back to the local player
		target->SetSavedClanId(source->GetSavedClanId());
		target->GetPedDrawHandler().GetVarData().SetOverrideCrewLogoTxdHash(source->GetPedDrawHandler().GetVarData().GetOverrideCrewLogoTxdHash());
		target->GetPedDrawHandler().GetVarData().SetOverrideCrewLogoTexHash(source->GetPedDrawHandler().GetVarData().GetOverrideCrewLogoTexHash());
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	VariationInitialisation
// PURPOSE :	Initialse the variation data for this ped (handle packed or streamed peds as required
// PARAMETERS :	pPed - ped with variation data to be initialised
// RETURNS :	N/A
/////////////////////////////////////////////////////////////////////////////////
void CPedFactory::VariationInitialisation(CPed* pPed)
{
	pPed->SetVarDefault();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	DestroyPedInternal
// PURPOSE :	Default factory ped destroy function. Individual projects can
//				override this.
// PARAMETERS :	pPed - the ped to destroy
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
bool CPedFactory::DestroyPedInternal(CPed* pPed, bool cached)
{
	Assert( pPed );
	if( pPed->m_ClothCollision )
	{
		pPed->m_ClothCollision->Shutdown();
		pPed->m_ClothCollision = NULL;
	}

	CPedPopulation::IncrementNumPedsDestroyedThisCycle();

	CPedPopulation::RemovePedFromPopulationCount(pPed);

	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_PedBeingDeleted, true );

	CTheScripts::UnregisterEntity(pPed, cached); // if cached clear the script extension as well

#if 0 // CS
    // drop any mission objects the ped is carrying (avoids assertions in the ped weapon manager destructor)
    pPed->GetInventory().DropAllMissionObjects();
#endif // 0

	// remove the ped from a ped group he may be in
	CPedGroup* pPedGroup = pPed->GetPedsGroup();

	if (pPedGroup && pPedGroup->IsLocallyControlled())
	{
		pPedGroup->GetGroupMembership()->RemoveMember(pPed);
	}

	Assert(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_CreatedByFactory ));
	pPed->SetPedConfigFlag( CPED_CONFIG_FLAG_CreatedByFactory, false );

	CGameWorld::Remove(pPed);

#if 0
	// if cached delete requested add the ped to the cache and don't free the memory, but ignore script peds
	if (cached && !pPed->GetExtension<CScriptEntityExtension>())
	{
		return AddDestroyedPedToCache(pPed);
	}
#endif

	// We should be removed from the world now. If something re-inserts us, that can be a potentially fatal problem.
	pedAssert(!CGameWorld::GetPedBeingDeleted());
	CGameWorld::SetPedBeingDeleted(pPed);

	// If this ped is holding a prop, we want to destroy it as well, so we don't have props hanging around for cutscenes or whathaveyou. [5/21/2013 mdawe]
	CPropManagementHelper* pPropHelper = pPed->GetPedIntelligence()->GetActivePropManagementHelper();
	if (pPropHelper)
	{
		pPropHelper->SetDestroyProp(true);
	}

	// make sure all tasks are aborted so they can clean up properly
	pPed->GetPedIntelligence()->FlushImmediately(false, true, true, false);

	pedAssert(CGameWorld::GetPedBeingDeleted() == pPed);
	CGameWorld::SetPedBeingDeleted(NULL);

	pedAssertf(!pPed->GetOwnerEntityContainer(), "Ped 0x%p (%s) should have been removed from the scene graph, did FlushImmediately() just reinsert it?", pPed, pPed->GetModelName());

#if __BANK
	LogDestroyedPed(pPed, "CPedFactory::DestroyPedInternal()", 0, Color_yellow);
#endif // __BANK

	DEV_ONLY(ms_bInDestroyPedFunction = true);
	delete pPed;
	DEV_ONLY(ms_bInDestroyPedFunction = false);
	
	return true;
}

#if __BANK
void CPedFactory::LogDestroyedPed(CPed * pPed, const char * pCallerDesc, s32 iTextOffsetY, Color32 iTextCol)
{
	if(ms_bLogDestroyedPeds && pPed)
	{
		CViewport* gameViewport = gVpMan.GetGameViewport();
		if(gameViewport)
		{
			static const int sizeoftext = 128;
			char text[sizeoftext];

			const Vector3 vPedPos = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
			const Vector3 vCameraPos = VEC3V_TO_VECTOR3(gameViewport->GetGrcViewport().GetCameraPosition());
			const float fDist = (vCameraPos - vPedPos).Mag();

			formatf(text, sizeoftext, "%i : %s", ms_iDestroyedPedCount, pPed->GetModelName());
			grcDebugDraw::Text(vPedPos, iTextCol, 0, -grcDebugDraw::GetScreenSpaceTextHeight() + iTextOffsetY, text, false, (s32)(10.0f / fwTimer::GetTimeStep()));
			grcDebugDraw::Sphere(vPedPos, 0.25f, Color_yellow4, false, (s32)(10.0f / fwTimer::GetTimeStep()));

			if (NetworkInterface::IsGameInProgress() && pPed->GetNetworkObject())
			{
				formatf(text, sizeoftext, " NETWORKNAME: %s,",pPed->GetNetworkObject()->GetLogName());
			}
			else
			{
				formatf(text, sizeoftext, " DEBUGNAME: %s,",pPed->GetDebugNameFromObjectID());
			}

			Displayf("**************************************************************************************************");
			Displayf("%s : %i", pCallerDesc, ms_iDestroyedPedCount);
			Displayf("NAME : %s,%s POPTYPE: %s, PEDTYPE: %s, DISTANCE : %.1f, POSITION : %.1f, %.1f, %.1f, PED : 0x%p", pPed->GetModelName(), text, CTheScripts::GetPopTypeName(pPed->PopTypeGet()), CPedType::GetPedTypeNameFromId(pPed->GetPedType()), fDist, vPedPos.x, vPedPos.y, vPedPos.z, pPed);
			if (scrThread::GetActiveThread())
				scrThread::GetActiveThread()->PrintStackTrace();
			sysStack::PrintStackTrace();
			Displayf("**************************************************************************************************");

			ms_iDestroyedPedCount++;
		}
	}
}
#endif

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CopyVariation
// PURPOSE :	Default factory ped destroy function. Individual projects can
//				override this.
// PARAMETERS :	source - source ped
//				dest - destination ped
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPedFactory::CopyVariation(const CPed* source, CPed* dest, bool stallForAssets)
{
	if(source && dest)
	{
		int flags = 0;
		if (stallForAssets)
			flags = STRFLAG_FORCE_LOAD  | STRFLAG_PRIORITY_LOAD;

		for(int iComponent=0; iComponent < PV_MAX_COMP; iComponent++)
		{
			u32 iDrawableId =0; 
			u8 TextureVarId =0;
			u8 Palette = 0; 

			iDrawableId = CPedVariationPack::GetCompVar(source, static_cast<ePedVarComp>(iComponent));

			TextureVarId = CPedVariationPack::GetTexVar(source, static_cast<ePedVarComp>(iComponent));
			// want to pick up current palette ID from the current debug ped (for this drawable)
			Palette = CPedVariationPack::GetPaletteVar(source, static_cast<ePedVarComp>(iComponent));

			//if (dest->IsVariationValid(static_cast<ePedVarComp>(iComponent),iDrawableId,TextureVarId, varIndex))
			{
                dest->SetVariation(static_cast<ePedVarComp>(iComponent), iDrawableId, 0, TextureVarId, Palette, flags, false);
			}
		}
	}

	CPedPropsMgr::SetPedPropsPacked(dest, CPedPropsMgr::GetPedPropsPacked(source, true));
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	CreatePlayerPed
// PURPOSE :	Create a player ped.  This function allow us to create a project
//				specific player.
// PARAMETERS :	pedControlInfo - How is this ped controlled (is it network, player, etc.)
//				modelIndex - the model to use for this ped.
//				infoIndex - addition player specific info.
//				pMat - the placement and orientation matrix to use for the ped.
//				bShouldBeCloned - if set to true and a network game is running,
//					this ped will be registered with the network object manager
//					so that it will cloned and synchronized on all the remote
//					machines involved in the network game.
// RETURNS :	The new player ped.
/////////////////////////////////////////////////////////////////////////////////
CPed* CPedFactory::CreatePlayerPed(const CControlledByInfo& pedControlInfo, u32 modelIndex, const Matrix34 *pMat, CPlayerInfo* pPlayerInfo )
{
	CPed* pPlayerPed=0;
	bool bRegisterAsNetworkObject = !pPlayerInfo; // if a player info is being passed in, a network object will already exist for this player

	Assert(!pedControlInfo.IsControlledByLocalAi());// Must use different setup as not actually requesting a player.

	if (!pPlayerInfo)
	{
		// get the player info from our network player if available, or create a new one
		if (CNetwork::IsNetworkOpen())
		{
			pPlayerInfo = NetworkInterface::GetLocalPlayer()->GetPlayerInfo();
			gnetDebug2("CPedFactory::CreatePlayerPed - Creating new player info (%p). Grabbing from local player", pPlayerInfo);
		}
		else
		{
			pPlayerInfo = rage_new CPlayerInfo(NULL, NetworkInterface::GetActiveGamerInfo());
			gnetDebug2("CPedFactory::CreatePlayerPed Creating new player info (%p). Name = %s", pPlayerInfo, NetworkInterface::GetActiveGamerInfo() ? NetworkInterface::GetActiveGamerInfo()->GetName() : "");
		}

		sysStack::PrintStackTrace();

		Assert(pPlayerInfo);
	}

#if __DEV
	ms_bWithinPedFactory = true;
#endif

	pPlayerPed=SetUpAsPlayerPed(pedControlInfo, modelIndex, pMat, *pPlayerInfo);  
	if(pPlayerPed)
	{
		pPlayerPed->SetPedConfigFlag( CPED_CONFIG_FLAG_CreatedByFactory, true );
	}

	if(pedControlInfo.IsControlledByLocalPlayer())
	{
#if __ASSERT
        // it's only OK to overwrite an existing local player if we are changing the player model
        if(m_pLocalPlayer)
        {
            Assertf(CPlayerInfo::IsChangingPlayerModel(), "Overwriting existing local player ped!");
        }
#endif // __ASSERT

		if(m_pLocalPlayer && m_pLocalPlayer != pPlayerPed)
		{
			m_pLocalPlayer->UpdateFirstPersonFlags(false);
		}

		m_pLocalPlayer = pPlayerPed;

		if (bRegisterAsNetworkObject && NetworkInterface::IsGameInProgress())
		{
			NetworkInterface::RegisterObject(pPlayerPed, 0, netObject::GLOBALFLAG_CLONEALWAYS|CNetObjGame::GLOBALFLAG_SCRIPTOBJECT);
		}
	}

#if __DEV
	ms_bWithinPedFactory = false;
#endif
	BANK_ONLY(ms_pLastCreatedPed = pPlayerPed);

	REPLAY_ONLY(CReplayMgr::OnCreateEntity(pPlayerPed));

	return pPlayerPed;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	DestroyPlayerPed
// PURPOSE :	Default factory player ped destroy function. Individual projects
//				can override this.
// PARAMETERS :	pPlayerPed - the player ped to destroy
// RETURNS :	Nothing
/////////////////////////////////////////////////////////////////////////////////
void CPedFactory::DestroyPlayerPed(CPed* pPlayerPed)
{
#if __DEV
    ms_bWithinPedFactory           = true;
	ms_bInDestroyPlayerPedFunction = true;
    ms_bInDestroyPedFunction       = true;
#endif // __DEV

    bool bDestroyPlayerInfo = true;

	pPlayerPed->SetPedConfigFlag( CPED_CONFIG_FLAG_PedBeingDeleted, true );

	if (pPlayerPed->GetSpecialAbility())
	{
		pPlayerPed->GetSpecialAbility()->Deactivate();
	}

	CPlayerInfo* pInfo = pPlayerPed->GetPlayerInfo();

	// make sure all tasks are aborted so they can clean up properly
	pPlayerPed->GetPedIntelligence()->FlushImmediately();

	// inform network object manager about this ped being deleted

	if (pPlayerPed->GetNetworkObject())
	{
		// the network player representing this player ped will delete the player info when it is removed
		if (pInfo && pPlayerPed->GetNetworkObject()->GetPlayerOwner() && pPlayerPed->GetNetworkObject()->GetPlayerOwner()->IsValid())
		{
			Assert(pPlayerPed->GetNetworkObject()->GetPlayerOwner()->GetPlayerInfo() == pInfo);
			bDestroyPlayerInfo = false;
		}

		Assert(!pPlayerPed->IsNetworkClone());
		NetworkInterface::UnregisterObject(pPlayerPed);
	}

	Assert(!pPlayerPed->GetNetworkObject());

	Assert(pPlayerPed->GetPedConfigFlag( CPED_CONFIG_FLAG_CreatedByFactory ));
	pPlayerPed->SetPedConfigFlag( CPED_CONFIG_FLAG_CreatedByFactory, false );

	if (pInfo)
    {
        pPlayerPed->SetPlayerInfo(NULL);
		pInfo->SetPlayerPed(NULL);
    }

	if (m_pLocalPlayer == pPlayerPed)
	{
		m_pLocalPlayer = NULL;
	}

	CGameWorld::Remove(pPlayerPed);
	delete pPlayerPed;

	if (pInfo && bDestroyPlayerInfo)
	{
		gnetDebug2("CPedFactory::DestroyPlayerPed - Deleting player info (%p). Name: %s", pInfo, pInfo->m_GamerInfo.GetName());

#if __BANK
		// ensure no other peds are using this player info
		CPed::Pool *pedPool = CPed::GetPool();

		for(int index = 0; index < pedPool->GetSize(); index++)
		{
			CPed *pPed = pedPool->GetSlot(index);

			if(pPed && (pPed != pPlayerPed))
			{
				if(pPed->GetPlayerInfo() == pInfo)
				{
					sysStack::PrintStackTrace();
					gnetAssertf(0, "CPedFactory::DestroyPlayerPed - Deleting a player info pointer (%p) when it is still referenced by %s", pInfo, pPed->GetDebugName());
					gnetError("CPedFactory::DestroyPlayerPed - Deleting a player info pointer (%p) when it is still referenced by %s", pInfo, pPed->GetDebugName());
				}
			}
		}
#endif

		delete pInfo;
	}

#if __DEV
    ms_bWithinPedFactory           = false;
	ms_bInDestroyPlayerPedFunction = false;
    ms_bInDestroyPedFunction       = false;
#endif // __DEV
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	SetUpAsCivilianPed
// PURPOSE :	Sets up project specific civilian peds.
// PARAMETERS :	pedControlInfo - How is this ped controlled (is it network, player, etc.)
//				modelIndex - the model to use for this ped.
//				pMat - the placement and orientation matrix to use for the ped.
//				bCreatedFromScript -
//				bFailSilentlyIfOutOfPeds - if true, suppress asserts if we don't have space in the pool or in the cache,
//					for use as a replacement to code that previously checked GetNoOfFreeSpaces().
// RETURNS :	The newly set up ped.
/////////////////////////////////////////////////////////////////////////////////
CPed* CPedFactory::SetUpAsCivilianPed(const CControlledByInfo& pedControlInfo, const u32 modelIndex, const Matrix34 *pMat, const bool bCreatedFromScript, bool bFailSilentlyIfOutOfPeds) const
{
	// Create the ped.
	CPed* pPed = ConcreteCreatePed(pedControlInfo, modelIndex, bFailSilentlyIfOutOfPeds);
	if (!pPed)
	{
		return NULL;
	}

	// Set up the on foot move blend, the model index, and the entity matrix.
	SetPedModelMatAndMovement(pPed, modelIndex, pMat);

	if(!bCreatedFromScript)
	{
		// Allow randomly created civilians to be agitated.
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_CanBeAgitated, true);
		// Prevent randomly created civilians from using seats on the running-board of vehicles
		if (!pPed->IsLawEnforcementPed())
		{
			pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_PreventUsingLowerPrioritySeats, true);
		}
	}

	// Return the new civilian ped.
	return pPed;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	SetUpAsCopPed
// PURPOSE :	Sets up project specific cop peds.
// PARAMETERS :	pedControlInfo - How is this ped controlled (is it network, player, etc.)
//				modelIndex - the model to use for this ped.
//				pMat - the placement and orientation matrix to use for the ped.
//				bCreatedFromScript -
//				bFailSilentlyIfOutOfPeds - if true, suppress asserts if we don't have space in the pool or in the cache,
//					for use as a replacement to code that previously checked GetNoOfFreeSpaces().
// RETURNS :	The newly set up ped.
/////////////////////////////////////////////////////////////////////////////////
CPed* CPedFactory::SetUpAsCopPed(const CControlledByInfo& pedControlInfo, const u32 modelIndex, const Matrix34 *pMat, const bool bCreatedFromScript, bool bFailSilentlyIfOutOfPeds) const
{
	// Create the ped.
	CPed* pPed = ConcreteCreatePed(pedControlInfo, modelIndex, bFailSilentlyIfOutOfPeds);
	if (!pPed)
	{
		return NULL;
	}

	// Set up the on foot move blend, the model index, and the entity matrix.
	SetPedModelMatAndMovement(pPed, modelIndex, pMat);

	//Allow randomly created cops to be agitated.
	if(!bCreatedFromScript)
	{
		pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_CanBeAgitated, true);
	}

	// Don't give cop weapons if created from script
	if (!bCreatedFromScript)
	{
	    static const atHashWithStringNotFinal COP_LOADOUT("LOADOUT_COP",0xB01B33EA);
        CPedInventoryLoadOutManager::SetLoadOut(pPed, COP_LOADOUT.GetHash());

		if(pPed->GetWeaponManager())
			pPed->GetPedIntelligence()->GetCombatBehaviour().SetShootRateModifier(COP_SHOT_RATE);

		pPed->m_nArmour = 0;
	}

	bool bCanBust = !bCreatedFromScript;
	pPed->GetPedIntelligence()->GetCombatBehaviour().ChangeFlag(CCombatData::BF_CanBust, bCanBust);

	pPed->GetPedIntelligence()->AddTaskDefault(pPed->ComputeWanderTask(*pPed));

	//Set up police to communicate events with friends/other cops.
	pPed->GetPedIntelligence()->SetInformRespectedFriends(CPedIntelligence::MAX_INFORM_FRIEND_DISTANCE_MAX_VALUE, CPedIntelligence::MAX_NUM_FRIENDS_TO_INFORM_MAX_VALUE);

	// Return the new cop ped.
	return pPed;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	SetUpAsPlayerPed
// PURPOSE :	Sets up project specific player peds.
// PARAMETERS :	pedControlInfo - How is this ped controlled (is it network, player, etc.)
//				modelIndex - the model to use for this ped.
//				pMat - the placement and orientation matrix to use for the ped.
// RETURNS :	The newly set up player ped.
/////////////////////////////////////////////////////////////////////////////////
CPed* CPedFactory::SetUpAsPlayerPed(const CControlledByInfo& pedControlInfo, const u32 modelIndex, const Matrix34 *pMat, CPlayerInfo& playerInfo) const
{ 
	Assert(!pedControlInfo.IsControlledByLocalAi());// Must use different setup as not actually requesting a player.

#if __DEV
	const bool wasWithinPedFactory = ms_bWithinPedFactory;
#endif

	// Create the player ped.
	CPed* pPlayerPed = ConcreteCreatePed(pedControlInfo, modelIndex, false);
	Assert(pPlayerPed);
	if(!pPlayerPed)
	{
		return NULL;
	}

#if __DEV
	ms_bWithinPedFactory = wasWithinPedFactory;
#endif

	pPlayerPed->SetPlayerInfo(&playerInfo);
	playerInfo.SetPlayerPed(pPlayerPed);

	// Create a group with the player being the leader.
	if (!pPlayerPed->IsControlledByNetworkPlayer() && 
		(!NetworkInterface::IsNetworkOpen() || pPlayerPed->GetNetworkObject())) // we can't assign a group to the player in MP until he has a network object and a physical player index
	{
		pPlayerPed->GetPlayerInfo()->SetupPlayerGroup();
	}

	// Set up the on foot move blend, the model index, and the entity matrix.
	SetPedModelMatAndMovement(pPlayerPed, modelIndex, pMat);

	pPlayerPed->GetPedIntelligence()->GetCombatBehaviour().SetFlag(CCombatData::BF_CanTauntInVehicle);
	pPlayerPed->GetPedIntelligence()->GetCombatBehaviour().SetFiringPattern(FIRING_PATTERN_FULL_AUTO);

	// Return the new player ped.
	return pPlayerPed;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	SetPedMovementModelAndMat
// PURPOSE :	Sets up the on foot move blend, the model index, and the entity
//				matrix.
// PARAMETERS :	pPed - the ped to set up.
//				modelIndex - the model to use for this ped.
//				pMat - the placement and orientation matrix to use for the ped.
// RETURNS :	Nothing.
/////////////////////////////////////////////////////////////////////////////////
void CPedFactory::SetPedModelMatAndMovement(CPed* pPed, const u32 modelIndex, const Matrix34 * pMat) const
{
	// Set the position and orientation.
	if(pMat)
	{
		pPed->SetMatrix(*pMat);

#if __TRACK_PEDS_IN_NAVMESH
		pPed->GetNavMeshTracker().Teleport(pMat->d);
#endif
	}

	// Set the model index.
	pPed->SetModelId(fwModelId((strLocalIndex(modelIndex))));

	// Set the ped's fade out distance
	pPed->SetLodDistance(CPed::ms_uDefaultPedLodDistance);
}



/////////////////////////////////////////////////////////////////////////////////
// FUNCTION :	ConcreteCreatePed
// PURPOSE :	This function allow us to create a ped using CPedFactory rather
//				than the project specific version of CPedFactory
// PARAMETERS :	pedControlInfo - How is this ped controlled (is it network, player, etc.)
//				modelIndex - the model to use for this ped.
//				bFailSilentlyIfOutOfPeds - if true, suppress asserts if we don't have space in the pool or in the cache,
//						for use as a replacement to code that previously checked GetNoOfFreeSpaces().
// RETURNS :	The new ped.
/////////////////////////////////////////////////////////////////////////////////
CPed* CPedFactory::ConcreteCreatePed(const CControlledByInfo& pedControlInfo, u32 modelIndex, bool ASSERT_ONLY(bFailSilentlyIfOutOfPeds)) const
{
#if __DEV
ms_bWithinPedFactory = true;
#endif // __DEV

	CPed *result = NULL;
	fwModelId modelId((strLocalIndex(modelIndex)));
	if (modelId.IsValid() && CModelInfo::GetBaseModelInfo(modelId))
	{
		if (CPed::GetPool()->GetNoOfFreeSpaces() == 0)
		{
#if 0
			if (!EjectOnePedFromCache())
#endif
			{
				Assertf(bFailSilentlyIfOutOfPeds, "%s Pool Full, Size == %d (you need to raise %s PoolSize in common/data/gameconfig.xml)", CPed::GetPool()->GetName(), CPed::GetPool()->GetSize(), CPed::GetPool()->GetName());
#if __DEV
				ms_bWithinPedFactory = false;
#endif // __DEV
				return NULL;
			}
		}

		result = rage_new CPed(ENTITY_OWNEDBY_POPULATION, pedControlInfo, modelId.GetModelIndex()); 

        if (PARAM_temppeddebug.Get())
		{
            if (result)
            {
                Displayf("[PED DEBUG]: Created ped %p, %s, frame: %d", result, CModelInfo::GetBaseModelInfo(modelId)->GetModelName(), fwTimer::GetFrameCount());
                sysStack::PrintStackTrace();
            }
		}
	}
	Assertf(result, "Unable to create ped with modelIndex: %d", modelIndex);

#if __DEV
ms_bWithinPedFactory = false;
#endif // __DEV

	if (result)
	{
		CPedPopulation::IncrementNumPedsCreatedThisCycle();
	}

	return result;
}


namespace CPoolHelpers
{
	int g_PedPoolSize = 0;

	int GetPedPoolSize()
	{
		Assertf(g_PedPoolSize != 0, "Trying to get the ped pool size before it's been set.");

		// We also do a trap here, to catch it in non-assert builds, but more importantly,
		// to catch it in case it's used from static initialization code when asserts may
		// not yet be working.
		TrapZ(g_PedPoolSize);

		return g_PedPoolSize;
	}

	void SetPedPoolSize(int poolSize)
	{
		Assert(g_PedPoolSize == 0);	// Not really meant to call this more than once at this time.

		g_PedPoolSize = poolSize;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DestroyPed
// PURPOSE:		some types of ped can't be destroyed immediately - wait till next frame update
//////////////////////////////////////////////////////////////////////////
bool CPedFactory::DestroyPed(CPed* pPed, bool cached)
{
	PF_AUTO_PUSH_DETAIL("Destroy Ped");
	if (pPed)
	{
#if 0
		// don't destroy ped if it's on the population cache list
		if (IsPedInDestroyedCache(pPed))
		{
			pPed->ClearAllKnownRefs();
			return true;
		}
#endif

		// inform network object manager about this ped being deleted
		if(pPed->GetNetworkObject())
		{
			if (pPed->IsNetworkClone() && pPed->GetNetworkObject()->IsScriptObject())
			{
				if (!pPed->PopTypeIsMission())
					Assertf(0, "Warning: clone %s is being destroyed (it is not a mission ped anymore)!", pPed->GetNetworkObject()->GetLogName());
				else
					Assertf(0, "Warning: clone %s is being destroyed!", pPed->GetNetworkObject()->GetLogName());
			}

			Assert(!pPed->IsNetworkClone());
			NetworkInterface::UnregisterObject(pPed);
		}

		Assert(!pPed->GetNetworkObject());

		return DestroyPedInternal(pPed, cached);
	}
	
	return false;
}


bool CPedFactory::AddDestroyedPedToCache(CPed* ped)
{
	if (!ped)
		return false;

	// if we have the cache turned off or this is a streamed ped, free it now
	if (!ms_reuseDestroyedPeds || !ped->GetPedModelInfo() || ped->GetPedModelInfo()->GetIsStreamedGfx())
	{
		DEV_ONLY(ms_bInDestroyPedFunction = true);
		delete ped;
		DEV_ONLY(ms_bInDestroyPedFunction = false);

		return true;
	}

	// make sure we don't already have this pointer in the list
	s32 firstFree = -1;
	for (s32 i = 0; i < MAX_DESTROYED_PEDS_CACHED; ++i)
	{
		// skip if this ped is already on the list
		if (ms_reuseDestroyedPedArray[i].ped == ped)
			return true;

		if (firstFree == -1 && ms_reuseDestroyedPedArray[i].ped == NULL)
			firstFree = i;
	}

	// we have a slot, cache the ped
	if (firstFree > -1)
	{
		ped->RemoveBlip(BLIP_TYPE_CHAR);

		ped->GetFrameCollisionHistory()->Reset();
		ped->GetPedIntelligence()->FlushImmediately(true, true, true, false);
		ped->GetPedIntelligence()->ClearScanners();
		ped->GetPedIntelligence()->SetOrder(NULL);

		ped->ClearDamage();
		ped->ClearDecorations();

		ped->ClearPedBrainWhenDeletingPed();

		// remove script guid so script can't grab this ped pointer while it's in the cache
		fwScriptGuid::DeleteGuid(*ped);
		
		ped->RemovePhysics();
		CPhysics::GetLevel()->SetInstanceIncludeFlags(ped->GetRagdollInst()->GetLevelIndex(), 0);
		Vec3V vPos = ped->GetRagdollInst()->GetPosition();
		vPos.SetZf(-100.0f);
		ped->GetRagdollInst()->SetPosition(vPos);

		ped->ClearAllKnownRefs();

		REPLAY_ONLY(CReplayMgr::OnDelete(ped));

		ms_reuseDestroyedPedArray[firstFree].ped = ped;
		ms_reuseDestroyedPedArray[firstFree].nukeTime = fwTimer::GetTimeInMilliseconds() + ms_reuseDestroyedPedCacheTime;
		ms_reuseDestroyedPedArray[firstFree].framesUntilReuse = 1;
		ms_reuseDestroyedPedCount++;

		ped->SetIsInPopulationCache(true);
		Assert(ms_reuseDestroyedPedArray[firstFree].ped);
		return true;
	}

	// no space left, destroy ped
	DEV_ONLY(ms_bInDestroyPedFunction = true);
	delete ped;
	DEV_ONLY(ms_bInDestroyPedFunction = false);

	return true;
}

bool CPedFactory::IsPedInDestroyedCache(const CPed* ped)
{
	if (!ped)
		return true;

	for (s32 i = 0; i < MAX_DESTROYED_PEDS_CACHED; ++i)
	{
		if (ms_reuseDestroyedPedArray[i].ped && ms_reuseDestroyedPedArray[i].ped == ped)
			return true;
	}

	return false;
}

void CPedFactory::ClearDestroyedPedCache()
{
	for (s32 i = 0; i < MAX_DESTROYED_PEDS_CACHED; ++i)
	{
		if (ms_reuseDestroyedPedArray[i].ped)
		{
			CPed* ped = ms_reuseDestroyedPedArray[i].ped;
			ms_reuseDestroyedPedArray[i].ped = NULL;

			// we can delete the ped like this because we've done all work of removing it from the world when it was added to this list
			DEV_ONLY(ms_bInDestroyPedFunction = true);
			delete ped;
			DEV_ONLY(ms_bInDestroyPedFunction = false);
		}
	}
	ms_reuseDestroyedPedCount = 0;
}

void CPedFactory::RemovePedGroupFromCache(const CLoadedModelGroup& pedGroup)
{
	const u32 numPedsInGroup = pedGroup.CountMembers();
	if (!numPedsInGroup)
		return;

	for (s32 i = 0; i < MAX_DESTROYED_PEDS_CACHED; ++i)
	{
		for (s32 f = 0; f < numPedsInGroup; ++f)
		{
			u32 pedModelIndex = pedGroup.GetMember(f);
			if (pedModelIndex == fwModelId::MI_INVALID)
				continue;

			if (ms_reuseDestroyedPedArray[i].ped && ms_reuseDestroyedPedArray[i].ped->GetModelIndex() == pedModelIndex)
			{
				CPed* ped = ms_reuseDestroyedPedArray[i].ped;
				ms_reuseDestroyedPedArray[i].ped = NULL;

				// we can delete the ped like this because we've done all work of removing it from the world when it was added to this list
				DEV_ONLY(ms_bInDestroyPedFunction = true);
				delete ped;
				DEV_ONLY(ms_bInDestroyPedFunction = false);
				ms_reuseDestroyedPedCount--;
			}
		}
	}
}

void CPedFactory::ProcessDestroyedPedsCache()
{
	u32 curTime = fwTimer::GetTimeInMilliseconds();
	for (s32 i = 0; i < MAX_DESTROYED_PEDS_CACHED; ++i)
	{
		if (ms_reuseDestroyedPedArray[i].ped)
		{
            Assertf(!ms_reuseDestroyedPedArray[i].ped->GetExtension<CScriptEntityExtension>(), "Ped '%s' in the ped cache suddenly became a script ped", ms_reuseDestroyedPedArray[i].ped->GetModelName());
			if (curTime >= ms_reuseDestroyedPedArray[i].nukeTime)
			{
				CPed* ped = ms_reuseDestroyedPedArray[i].ped;
				ms_reuseDestroyedPedArray[i].ped = NULL;

				// we can delete the ped like this because we've done all work of removing it from the world when it was added to this list
				DEV_ONLY(ms_bInDestroyPedFunction = true);
				delete ped;
				DEV_ONLY(ms_bInDestroyPedFunction = false);
				ms_reuseDestroyedPedCount--;
			}
			else
				ms_reuseDestroyedPedArray[i].framesUntilReuse = (s8)rage::Max(0, ms_reuseDestroyedPedArray[i].framesUntilReuse - 1);
		}
	}

#if __BANK
	if (ms_reuseDestroyedPedsDebugOutput)
	{
		grcDebugDraw::AddDebugOutput("Cached destroyed peds: %d", ms_reuseDestroyedPedCount);
		u32 testCount = 0;

		for (s32 i = 0; i < MAX_DESTROYED_PEDS_CACHED; ++i)
		{
			if (ms_reuseDestroyedPedArray[i].ped)
			{
				CPed* ped = ms_reuseDestroyedPedArray[i].ped;
				grcDebugDraw::AddDebugOutput("Ped %2d - 0x%8x - %s - %d", testCount + 1, ped, ped->GetModelName(), (ms_reuseDestroyedPedArray[i].nukeTime - fwTimer::GetTimeInMilliseconds()) / 1000);
				testCount++;
			}
		}
		Assertf(testCount == ms_reuseDestroyedPedCount, "Reuse destroyed ped count is out of sync with array! (%d - %d)", testCount, ms_reuseDestroyedPedCount);
	}
#endif
}

CPed* CPedFactory::CreatePedFromDestroyedCache(const CControlledByInfo& pedControlInfo, fwModelId modelId, const Matrix34 *pMat, bool bShouldBeCloned)
{
	// we don't cache player peds
	if (pedControlInfo.IsControlledByLocalOrNetworkPlayer())
		return NULL;

	// find a ped with the same model id in the cache
	CPed* ped = NULL;
	s32 cacheIndex = -1;
	for (s32 i = 0; i < MAX_DESTROYED_PEDS_CACHED; ++i)
	{
		if (ms_reuseDestroyedPedArray[i].ped && ms_reuseDestroyedPedArray[i].ped->GetModelIndex() == modelId.GetModelIndex() && ms_reuseDestroyedPedArray[i].framesUntilReuse == 0)
		{
			ped = ms_reuseDestroyedPedArray[i].ped;
			cacheIndex = i;
			break;
		}
	}

	if (!ped)
		return NULL;
	Assert(cacheIndex != -1);

	// set default values, as if ped was newly created by the factory
	ped->m_PedConfigFlags.Init(ped);
	ped->m_PedResetFlags.Init(ped);
	ped->SetControlledByInfo(pedControlInfo);
	ped->SetOwnedBy(ENTITY_OWNEDBY_POPULATION);
	ped->DelayedRemovalTimeReset();
	ped->DelayedConversionTimeReset();
	ped->SetArrestState(ArrestState_None);
	ped->SetDeathState(DeathState_Alive);
	ped->InitHealth();
	ped->GetPedAiLod().ResetAllLodFlags();

	ped->SetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollOnPedCollisionWhenDead, false);
	ped->SetPedConfigFlag(CPED_CONFIG_FLAG_DontActivateRagdollOnVehicleCollisionWhenDead, false);
	ped->SetPedConfigFlag(CPED_CONFIG_FLAG_CanBeAgitated, true);

	if(pMat)
	{
		ped->SetMatrix(*pMat);

#if __TRACK_PEDS_IN_NAVMESH
		ped->GetNavMeshTracker().Teleport(pMat->d);
#endif
	}

	CPedModelInfo* pModelInfo = ((CPedModelInfo*)CModelInfo::GetBaseModelInfo(modelId));
	Assert(pModelInfo);
	ePedType pedType = pModelInfo->GetDefaultPedType();
	switch(pedType)
	{
	case PEDTYPE_COP:
		{
			// Don't give cop weapons if created from script
			static const atHashWithStringNotFinal COP_LOADOUT("LOADOUT_COP",0xB01B33EA);
			CPedInventoryLoadOutManager::SetLoadOut(ped, COP_LOADOUT.GetHash());

			if(ped->GetWeaponManager())
				ped->GetPedIntelligence()->GetCombatBehaviour().SetShootRateModifier(0.3f); // 0.3f as COP_SHOT_RATE in PedFactory.cpp

			ped->SetArmour(0.f);

			ped->GetPedIntelligence()->AddTaskDefault(ped->ComputeWanderTask(*ped));

			//Set up police to communicate events with friends/other cops.
			ped->GetPedIntelligence()->SetInformRespectedFriends(CPedIntelligence::MAX_INFORM_FRIEND_DISTANCE_MAX_VALUE, CPedIntelligence::MAX_NUM_FRIENDS_TO_INFORM_MAX_VALUE);
		}
		break;
	default:
		break;
	}

	// ******* NETWORK REGISTRATION HAS TO BE DONE BEFORE ANY FURTHER STATE CHANGES ON THE PED! *******************
	if (bShouldBeCloned && NetworkInterface::IsGameInProgress())
	{
		bool bRegistered = NetworkInterface::RegisterObject(ped, 0, 0);

		// ped could not be registered as a network object (due to maximum number of ped objects being exceeded
		// scripted peds are queued to be created
		if (!bRegistered)
		{
			return NULL;
		}
	}

	// pretend the factory created this ped
	ped->SetPedConfigFlag(CPED_CONFIG_FLAG_CreatedByFactory, true);

	// clear cache entry
	ms_reuseDestroyedPedArray[cacheIndex].ped = NULL;
	ms_reuseDestroyedPedCount--;

	CPedPropsMgr::ClearAllPedProps(ped);
	InitCachedPed(ped);

	ped->SetIsInPopulationCache(false);
	//ped->ClearAllKnownRefs();

	return ped;
}


void CPedFactory::InitCachedPed(CPed* ped)
{
	DEV_ONLY(ms_bWithinPedFactory = true);
	// Make sure that player's animated inst is allowed to activate. It might have been
	// told not to do so as part of the dead ragdoll shenanigans.
	Assert(ped->GetAnimatedInst());
	ped->GetAnimatedInst()->SetInstFlag(phInst::FLAG_NEVER_ACTIVATE, false);

	ped->ClearClothController();

	//
	// DETACH PLAYER FROM VEHICLE, ETC:
	//
	ped->SetPedOutOfVehicle(CPed::PVF_Warp);

	if(ped->GetIsAttached())
	{
		ped->DetachFromParent(0);
	}

	Assert(ped->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle) == false);
	Assert(!ped->GetIsAttached());

	CPickupManager::DetachAllPortablePickupsFromPed(ped);

	//
	// CLEANUP ANY RENDERING / VISUAL EFFECTS:
	//
	ped->GetPedDrawHandler().GetPropData().SetSkipRender(false);
	ped->m_nPhysicalFlags.bRenderScorched = false;

	// clean up wet effect
	ped->ClearWetClothing();

	if (ped->GetPedVfx())
	{
		ped->GetPedVfx()->Init(ped);
	}

	g_ptFxManager.RemovePtFxFromEntity(ped);
	g_fireMan.ExtinguishEntityFires(ped, true);

	//
	// CLEANUP ANIMATION:
	//
	// Remove any higher priority animation
	ped->GetMovePed().SetTaskNetwork(NULL);
	ped->GetMovePed().SetAdditiveNetwork(NULL);

	// get rid of ragdoll frame
	ped->GetMovePed().SwitchToAnimated(0.f);

	// The player may have been posed in a different animation state, set posed to false so the current animation state is forced after the camera update
	ped->GetAnimDirector()->SetPosed(false);

	ped->RestoreHeadingChangeRate();

	ped->GetPortalTracker()->Teleport();
	ped->GetPortalTracker()->RequestRescanNextUpdate();

	//
	// RESET PED STATE
	//
	if (!ped->IsNetworkClone()) // a clone always has its state updated via sync messages 
	{
#if __DEV
		ped->m_nDEflags.bCheckedForDead = true;
#endif
		ped->SetIsVisibleForModule( SETISVISIBLE_MODULE_GAMEPLAY, true );
		ped->SetIsVisibleForModule( SETISVISIBLE_MODULE_FIRST_PERSON, true );

		ped->ClearDeathInfo();

		ped->ResetWeaponDamageInfo();
		if (ped->GetHelmetComponent())
			ped->GetHelmetComponent()->DisableHelmet();

		ped->SetIsCrouching(false);
		ped->StopAllMotion(true);

		ped->ClearBaseFlag(fwEntity::REMOVE_FROM_WORLD);
		ped->SetPedConfigFlag( CPED_CONFIG_FLAG_BlockWeaponSwitching, false );
		ped->SetPedConfigFlag( CPED_CONFIG_FLAG_PedBeingDeleted, false );
		ped->SetVelocity(ORIGIN);

		// Clear all gadgets
		CPedWeaponManager* pWeapMgr = ped->GetWeaponManager();
		if (pWeapMgr)
		{
			pWeapMgr->DestroyEquippedGadgets();
			pWeapMgr->DestroyEquippedWeaponObject(true OUTPUT_ONLY(,"CPedFactory::InitCachedPed"));

			// DestroyEquippedWeaponObject above just destroys the object, but doesn't change the weapon
			// hash in CPedWeaponManager::m_equippedWeapon. Without this, peds that get recycled would
			// appear to CTaskAmbientClips to spawn with props in their hand, and cause asserts to fail.
			// Should fix B* 425305: "[ai_task] Error: Assertf(!m_AmbientClipRequestHelper.RequiresProp()
			// || m_AmbientClipRequestHelper.GetRequiredProp() == m_Prop) FAILED: New clip requires new
			// prop [Prop_CS_Hotdog_01], but ped has permanent prop [Prop_CS_Binder_01]".
			pWeapMgr->EquipWeapon(ped->GetDefaultUnarmedWeaponHash());
		}
	}

	ped->ReleaseCoverPoint();

	// reset physics inst flags
	fragInstNMGta* pFragInst = ped->GetRagdollInst();		
	// Deal with frag, get original include flags from type
	if (Verifyf(pFragInst, "No ragdoll on cached ped '%s'", ped->GetModelName()))
	{
		if (Verifyf(pFragInst->GetTypePhysics(), "Ped ragdoll has no physics type info (%s)", ped->GetModelName()))
		{
			if (Verifyf(pFragInst->GetTypePhysics()->GetArchetype(), "Ped ragdoll has no physics archetype (%s)", ped->GetModelName()))
			{
				Assertf(!ped->GetIsAttached(), "Trying to reset collision on attached ped.");
				u32 uOrigIncludeFlags = pFragInst->GetTypePhysics()->GetArchetype()->GetIncludeFlags();
				CPhysics::GetLevel()->SetInstanceIncludeFlags(ped->GetRagdollInst()->GetLevelIndex(), uOrigIncludeFlags);
			}
		}

	}
	
	// CPed::Init() will clear out the group index, so if it's set to something else
	// now we'll probably get an assert from the group code since the group will still
	// have a pointer to us. This should have been avoided by leaving the group at the point
	// where we got added to reuse pool.
	Assert(ped->GetPedGroupIndex() == PEDGROUP_INDEX_NONE);

	ped->CPed::Init(true);

	// Make sure we are not set to be removed as soon as possible again!
	ped->SetRemoveAsSoonAsPossible(false);

	DEV_ONLY(ms_bWithinPedFactory = false);
}

bool CPedFactory::EjectOnePedFromCache()
{
	if (!ms_reuseDestroyedPedCount)
		return false;

	for (s32 i = 0; i < MAX_DESTROYED_PEDS_CACHED; ++i)
	{
		if (ms_reuseDestroyedPedArray[i].ped)
		{
			CPed* ped = ms_reuseDestroyedPedArray[i].ped;
			ms_reuseDestroyedPedArray[i].ped = NULL;

			DEV_ONLY(ms_bInDestroyPedFunction = true);
			delete ped;
			DEV_ONLY(ms_bInDestroyPedFunction = false);
			ms_reuseDestroyedPedCount--;
			return true;
		}
	}

	return false;
}
