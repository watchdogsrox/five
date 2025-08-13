#include "debugrecorder.h"

#if DR_ENABLED

//RAGE
#include "ai/task/task.h"
#include "ai/task/taskspew.h"
#include "entity/entity.h"
#include "fwanimation/move.h"
#include "input/keyboard.h"
#include "input/keys.h"
#include "move/move_state.h"
#include "physics/debugEvents.h"
#include "system/param.h"
#include "vectormath/vec3v.h"

//GAME
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/viewports/ViewportManager.h"
#include "debug/DebugWanted.h"
#include "debug/DebugAnimation.h"
#include "network/debug/NetworkDebug.h"
#include "peds/ped.h"
#include "peds/pedfactory.h"
#include "peds/pedIntelligence.h"
#include "peds/horse/horseComponent.h"
#include "physics/gtaInst.h"
#include "scene/world/GameWorld.h"
#include "streaming/streaming.h"
#include "task/motion/taskMotionBase.h"
#include "ai/task/taskspew.h"
#include "vehicles/vehicle.h"
#include "vehicles/vehiclefactory.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "vehicleAi/task/TaskVehicleGoToAutomobile.h"

PARAM(DR_AllInsts, "Default to recording all instances rather than the focus inst");
PARAM(DR_LoadFilter, "Load a file describing the filters to switch on initially");
PARAM(DR_Autostart, "Automatically start recording");
PARAM(DR_TrackMode, "Set mode to start tracking at");

#if __WIN32
#pragma warning(push)
#pragma warning(disable:4201)	// nonstandard extension used : nameless struct/union
#endif

void DisableAllEventTypes();

namespace rage
{
	EXT_PFD_DECLARE_ITEM_SLIDER(BoundDrawDistance);
}
extern void FixupObject(CEntity *pObject, const Matrix34 &testMat);
atFixedArray<debugPlayback::phInstId, GameRecorder::kMaxInstsPerFrame> GameRecorder::m_FrameInsts;
atFixedArray<debugPlayback::phInstId, GameRecorder::kMaxInstsPerFrame> GameRecorder::m_LastFrameInsts;
atArray<DebugReplayObjectInfo*> DebugReplayObjectInfo::sm_ChainStarts;
atArray<debugPlayback::phInstId> DebugReplayObjectInfo::sm_PendingChainListFrom;
atArray<debugPlayback::phInstId> DebugReplayObjectInfo::sm_PendingChainListTo;

static bool sbTrackPlayer=false;
static float sfTrackRange = 16.0f;

DebugReplayObjectInfo::DebugReplayObjectInfo() 
	: m_vBBMax(V_ZERO)
	, m_vBBMin(V_ZERO)
	, mp_NextInChain(0)
	, mp_UserData(0)
	, m_ClassType(0)
	, m_ChainIndex(-1)
	, m_bIsPlayer(false)
	, m_bPlayerIsDriving(false) 
{
}

DebugReplayObjectInfo::DebugReplayObjectInfo(phInst *pInst)
	: debugPlayback::NewObjectData(pInst)
	, mp_NextInChain(0)
	, mp_UserData(pInst->GetUserData())
	, m_ClassType(0)
	, m_ChainIndex(-1)
{
	if(pInst)
	{
		m_ClassType = pInst->GetClassType();
		switch (m_ClassType)
		{
		case PH_INST_FRAG_VEH:
		case PH_INST_FRAG_PED:
		case PH_INST_FRAG_OBJECT:
		case PH_INST_VEH:
		case PH_INST_PED:
		case PH_INST_OBJECT: {
				CEntity *pObject = (CEntity*)pInst->GetUserData();
				if (pObject)
				{
					m_ModelName = pObject->GetModelName();

					//Store information about the player being involved
					if (pObject->GetIsTypePed())
					{
						CPed *pPed = (CPed*)pObject;
						m_bIsPlayer = pPed->IsLocalPlayer();
#if ENABLE_HORSE
						if (!m_bIsPlayer)
						{
							const CHorseComponent* pHrs = pPed->GetHorseComponent();
							if (pHrs)
							{
								if (pHrs->GetFreshnessCtrl().IsMountedByPlayer())
								{
									m_bPlayerIsDriving = true;
								}
							}
						}
#endif
					}
					else if (pObject->GetIsTypeVehicle())
					{
						CVehicle *pVeh = (CVehicle*)pObject;
						CPed *pDriver = pVeh->GetDriver();
						m_bPlayerIsDriving = pDriver && pDriver->IsPlayer();
					}

					if (pObject->GetArchetype())
					{
						m_vBBMax = RCC_VEC3V(pObject->GetArchetype()->GetBoundingBoxMax());
						m_vBBMin = RCC_VEC3V(pObject->GetArchetype()->GetBoundingBoxMin());
					}
					else
					{
						m_vBBMax = RCC_VEC3V(pObject->GetBoundingBoxMax());
						m_vBBMin = RCC_VEC3V(pObject->GetBoundingBoxMin());
					}
				}
			}
			break;
		case PH_INST_FRAG_GTA:							// marker for start of GTA fragInst's
		case PH_INST_GTA:
		case PH_INST_PROJECTILE:
			break;
		}
	}
}

void DebugReplayObjectInfo::ClearReplayObject()
{
	if (m_rReplayObject)
	{
		CGameWorld::Remove(m_rReplayObject);

		if(m_rReplayObject->GetIsTypeVehicle())
			CVehicleFactory::GetFactory()->Destroy((CVehicle*)m_rReplayObject.Get());
		else if(m_rReplayObject->GetIsTypePed())
			CPedFactory::GetFactory()->DestroyPed((CPed*)m_rReplayObject.Get());
		else if(m_rReplayObject->GetIsTypeObject())
			CObjectPopulation::DestroyObject((CObject*)m_rReplayObject.Get());
		else
			delete m_rReplayObject;
	}
}

void DebugReplayObjectInfo::RebuildChainsFromScratchCB(debugPlayback::DataBase& rData)
{
	DebugReplayObjectInfo *pNOD = dynamic_cast<DebugReplayObjectInfo *>(&rData);
	if (pNOD && (pNOD->m_ChainIndex>-1))
	{
		Assert(!pNOD->mp_NextInChain);
		pNOD->mp_NextInChain = sm_ChainStarts[ pNOD->m_ChainIndex ];
		sm_ChainStarts[ pNOD->m_ChainIndex ] = pNOD;
	}			
}

static int iHighestIndex=-1;
void DebugReplayObjectInfo::GetHighestChainIndexCB(debugPlayback::DataBase& rData)
{
	DebugReplayObjectInfo *pNOD = dynamic_cast<DebugReplayObjectInfo *>(&rData);
	if (pNOD && pNOD->m_ChainIndex)
	{
		iHighestIndex = Max(pNOD->m_ChainIndex, iHighestIndex);
	}			
}

void DebugReplayObjectInfo::RebuildChainsFromScratch()
{
	sm_ChainStarts.Reset();
	debugPlayback::DebugRecorder *pInstance = debugPlayback::DebugRecorder::GetInstance();
	if(pInstance)
	{
		iHighestIndex = -1;
		pInstance->CallOnAllStoredData(GetHighestChainIndexCB);
		if (iHighestIndex >= 0)
		{
			sm_ChainStarts.Resize(iHighestIndex + 1);
			pInstance->CallOnAllStoredData(RebuildChainsFromScratchCB);
		}
	}
}

void DebugReplayObjectInfo::UpdateChains()
{
	debugPlayback::DebugRecorder *pInstance = debugPlayback::DebugRecorder::GetInstance();
	if(pInstance)
	{
		const int kCount = sm_PendingChainListFrom.GetCount();
		for (int i=0 ; i<kCount ; i++)
		{
			DebugReplayObjectInfo *pFromInfo = dynamic_cast<DebugReplayObjectInfo*>(pInstance->GetSharedData(sm_PendingChainListFrom[i].ToU32()));
			DebugReplayObjectInfo *pToInfo = dynamic_cast<DebugReplayObjectInfo*>(pInstance->GetSharedData(sm_PendingChainListTo[i].ToU32()));
			
			if (pFromInfo && pToInfo)
			{
				Assert(pFromInfo->mp_UserData == pToInfo->mp_UserData);
				if ((pFromInfo->m_ChainIndex > -1) && (pToInfo->m_ChainIndex > -1))
				{
					Assertf(pFromInfo->m_ChainIndex == pToInfo->m_ChainIndex, "Mismatched chains? [%d != %d]", pToInfo->m_ChainIndex, pFromInfo->m_ChainIndex);
				}
				else
				{
					if (pFromInfo->m_ChainIndex > -1)
					{
						//Add pTo to the same chain at the head
						pToInfo->mp_NextInChain = sm_ChainStarts[ pFromInfo->m_ChainIndex ];
						sm_ChainStarts[ pFromInfo->m_ChainIndex ] = pToInfo;
						pToInfo->m_ChainIndex = pFromInfo->m_ChainIndex;
					}
					else if (pToInfo->m_ChainIndex > -1)
					{
						//Add pFrom to the same chain at the head
						pFromInfo->mp_NextInChain = sm_ChainStarts[ pToInfo->m_ChainIndex ];
						sm_ChainStarts[ pToInfo->m_ChainIndex ] = pFromInfo;
						pFromInfo->m_ChainIndex = pToInfo->m_ChainIndex;
					}
					else
					{
						//New chain, put both in
						int iNewIndex = sm_ChainStarts.GetCount();
						sm_ChainStarts.Grow(16) = pToInfo;
						pToInfo->mp_NextInChain = pFromInfo;
						pToInfo->m_ChainIndex = iNewIndex;
						pFromInfo->m_ChainIndex = iNewIndex;
					}
				}
			}
		}
	}

	sm_PendingChainListFrom.Reset();
	sm_PendingChainListTo.Reset();
}

void DebugReplayObjectInfo::SetNextInst(const phInst *pNew)
{
	sm_PendingChainListFrom.Grow(16) = m_rInst;
	sm_PendingChainListTo.Grow(16) = debugPlayback::phInstId(pNew);
}

void DebugReplayObjectInfo::DrawBox(Mat34V &rMat, Color32 col) const
{
	Vec3V vMin, vMax;
	GetBoundingMinMax(vMin, vMax);

	const Mat34V prevWorldMtx(grcWorldMtx(rMat));
	grcColor(col);
	Vec3V a(vMin.GetXf(),vMin.GetYf(),vMin.GetZf());
	Vec3V b(vMax.GetXf(),vMin.GetYf(),vMin.GetZf());
	Vec3V c(vMax.GetXf(),vMax.GetYf(),vMin.GetZf());
	Vec3V d(vMin.GetXf(),vMax.GetYf(),vMin.GetZf());
	Vec3V e(vMin.GetXf(),vMin.GetYf(),vMax.GetZf());
	Vec3V f(vMax.GetXf(),vMin.GetYf(),vMax.GetZf());
	Vec3V g(vMax.GetXf(),vMax.GetYf(),vMax.GetZf());
	Vec3V h(vMin.GetXf(),vMax.GetYf(),vMax.GetZf());
	grcBegin(drawLines,24);
	grcVertex3f(a);	grcVertex3f(b);
	grcVertex3f(b);	grcVertex3f(c);
	grcVertex3f(c);	grcVertex3f(d);
	grcVertex3f(d);	grcVertex3f(a);

	grcVertex3f(a);	grcVertex3f(e);
	grcVertex3f(b);	grcVertex3f(f);
	grcVertex3f(c);	grcVertex3f(g);
	grcVertex3f(d);	grcVertex3f(h);

	grcVertex3f(e);	grcVertex3f(f);
	grcVertex3f(f);	grcVertex3f(g);
	grcVertex3f(g);	grcVertex3f(h);
	grcVertex3f(h);	grcVertex3f(e);
	grcEnd();
	grcWorldMtx(prevWorldMtx);
}

phInst *DebugReplayObjectInfo::CreateObject(debugPlayback::TextOutputVisitor &rText, Mat34V &initialMatrix, bool bInvolvePlayer)
{
	//Create an object that matches the original recorded as closely as possible

	fwModelId modelId;
	/*CBaseModelInfo* pModelInfo =*/ CModelInfo::GetBaseModelInfoFromName(m_ModelName.c_str(), &modelId);

	rText.AddLine("Creating %s", m_ModelName.c_str());
	rText.PushIndent();
	switch(m_ClassType)
	{
	case PH_INST_FRAG_PED:
	case PH_INST_PED:
		{
			if (m_bIsPlayer && bInvolvePlayer)
			{
				//Clear the current player ped
				CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
				if (pPlayerPed)
				{
					rText.AddLine("Destroying old player");
					CPedFactory::GetFactory()->DestroyPed(pPlayerPed);
				}

				//Create a new one.
				const CControlledByInfo localPlayerControl(false, true);
				m_rReplayObject = pPlayerPed = CPedFactory::GetFactory()->CreatePlayerPed(localPlayerControl, modelId.GetModelIndex(), &RCC_MATRIX34(initialMatrix));
				if (pPlayerPed)
				{
					rText.AddLine("Setting up new player");
					pPlayerPed->PopTypeSet(POPTYPE_PERMANENT);
					pPlayerPed->SetDefaultDecisionMaker();
					pPlayerPed->SetCharParamsBasedOnManagerType();
					pPlayerPed->GetPedIntelligence()->SetDefaultRelationshipGroup();

					//CGameWorld::AddPlayerToWorld(pPlayerPed, pos);
					// CStreaming::SetMissionDoesntRequireObject(index, CModelInfo::GetStreamingModuleId());

					CModelInfo::SetAssetsAreDeletable(modelId);

					pPlayerPed->GetPedIntelligence()->AddTaskDefault(pPlayerPed->GetCurrentMotionTask()->CreatePlayerControlTask());

					pPlayerPed->InitSpecialAbility();
				}

			}
			else
			{
				//Create an AI ped
				rText.AddLine("Creating AI actor");
				const CControlledByInfo localAiControl(false, false);
				CPed *pNewPed = CPedFactory::GetFactory()->CreatePed(localAiControl, modelId, &RCC_MATRIX34(initialMatrix), true, true, false);
				m_rReplayObject = pNewPed;

				if (m_bPlayerIsDriving && bInvolvePlayer)
				{
					CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
					if (pPlayerPed)
					{
						rText.AddLine("Involving player");
						rText.PushIndent();

						//Remove from their current vehicle!
						if (pPlayerPed->GetIsAttachedInCar())
						{
							rText.AddLine("Remove player from old vehicle");
							pPlayerPed->SetPedOutOfVehicle();
						}
						else if (pPlayerPed->GetMyMount())
						{
							rText.AddLine("Remove player from old mount");
							pPlayerPed->SetPedOffMount();
						}

						//Update interior state from the vehicle being warped into
						if (pPlayerPed->m_nFlags.bInMloRoom || pPlayerPed->GetIsRetainedByInteriorProxy() )
						{
							CGameWorld::Remove(pPlayerPed);
							CGameWorld::Add(pPlayerPed, m_rReplayObject->GetInteriorLocation());
						}

						rText.AddLine("Set on new mount");
						pPlayerPed->GetPedIntelligence()->FlushImmediately();
						pPlayerPed->SetPedOnMount(pNewPed);

						rText.PopIndent();
					}	
				}
			}

		}
		break;
	case PH_INST_FRAG_OBJECT:
	case PH_INST_OBJECT:
		{
			m_rReplayObject = CObjectPopulation::CreateObject(modelId, ENTITY_OWNEDBY_RANDOM, true);
		}
		break;
	case PH_INST_FRAG_VEH:
	case PH_INST_VEH:
		{
			rText.AddLine("Creating new vehicle");
			CVehicle *pNewVehicle = CVehicleFactory::GetFactory()->Create(modelId, ENTITY_OWNEDBY_DEBUG, POPTYPE_TOOL, &RCC_MATRIX34(initialMatrix));
			m_rReplayObject = pNewVehicle;
			if (pNewVehicle && m_bPlayerIsDriving && bInvolvePlayer)
			{
				rText.AddLine("Putting player in vehicle!");
				//Grab player and put them in the vehicle
				CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
				if (pPlayerPed)
				{
					rText.PushIndent();
					//Remove from their current vehicle!
					//Remove from their current vehicle!
					if (pPlayerPed->GetIsAttachedInCar())
					{
						rText.AddLine("Remove from old vehicle");
						pPlayerPed->SetPedOutOfVehicle();
					}
					else if (pPlayerPed->GetIsAttachedOnMount())
					{
						rText.AddLine("Remove from old mount");
						pPlayerPed->SetPedOffMount();
					}

					//Update interior state from the vehicle being warped into
					if (pPlayerPed->m_nFlags.bInMloRoom || pPlayerPed->GetIsRetainedByInteriorProxy() )
					{
						CGameWorld::Remove(pPlayerPed);
						CGameWorld::Add(pPlayerPed, pNewVehicle->GetInteriorLocation());
					}

					rText.AddLine("Set in new vehicle");
					pPlayerPed->GetPedIntelligence()->FlushImmediately();
					pPlayerPed->SetPedInVehicle(pNewVehicle,0,0);	//Drivers seat is zero?

					rText.PopIndent();
				}
			}
		}
		break;
	case PH_INST_PROJECTILE:
		Errorf("Replay with new object not yet supported for PH_INST_PROJECTILE");
		break;
	case PH_INST_FRAG_GTA:
		Errorf("Replay with new object not yet supported for PH_INST_FRAG_GTA");
		break;
	case PH_INST_GTA:
		Errorf("Replay with new object not yet supported for PH_INST_GTA");
		break;
	default:
		Errorf("Replay with new object not yet supported for '%d'", m_ClassType);
		break;
	}

	if (m_rReplayObject)
	{
		rText.AddLine("Fixup object");
		FixupObject(m_rReplayObject, RCC_MATRIX34(initialMatrix));
	}
	rText.PopIndent();
	return m_rReplayObject ? m_rReplayObject->GetCurrentPhysicsInst() : 0;
}


const char *DebugReplayObjectInfo::GetObjectDescription(char *pBuffer, int iBuffersize) const
{
	if(m_rInst.IsValid())
	{
		if (m_ModelName.c_str())
		{
			formatf(pBuffer, iBuffersize, "Class: %d [%s]", m_ClassType, m_ModelName.c_str());
		}
		else
		{
			formatf(pBuffer, iBuffersize, "Class: %d [0x%x]", m_ClassType, m_rInst.GetObject());
		}
	}
	else
	{
		strncpy(pBuffer, "<<Invalid>>", iBuffersize);
	}
	return pBuffer;
}

debugPlayback::NewObjectData *DebugReplayObjectInfo::CreateNewObjectData(phInst *pInst)
{
	DR_MEMORY_HEAP();
	return rage_new DebugReplayObjectInfo(pInst);
}

RegdEnt DebugReplayObjectInfo::m_rReplayObject;

GameRecorder::GameRecorder()
	:m_vTeleportLocation(V_ZERO)
	,mp_ReplayFrame(0)
	,m_iLastModeSelected(-1)
	,m_iDrawnFrameNetTime(0)
	,m_bSyncNetDisplays(false)
	,m_bSelectMode(false)
	,m_bPauseOnEventReplay(false)
	,m_bDestroyReplayObject(false)
	,m_bCreateObjectForReplay(true)
	,m_bInvolvePlayer(false)
	,m_bStoreDataThisFrame(false)
	,m_CachedCreatureLookupInst(0)
	,m_CachedCreatureLookupCreature(0)
	,m_bHasTeleportLocation(false)
	,m_bHighlightSelected(true)
	,m_bShowHelpText(false)
	,m_bUseScriptCallstack(false)
	,m_bReplayCamera(false)
	,m_bPosePedsIfPossible(true)
	,m_bRecordSkeletonsEveryFrame(false)
{
	//Setup the base system
	debugPlayback::Init();

	debugPlayback::NewObjectData::smp_CreateNewObjectDescription = DebugReplayObjectInfo::CreateNewObjectData;

	debugPlayback::PhysicsEvent::sm_AppRecordInst = NewInstFunc;

	CPhysical::sm_RecordAttachmentsFunc = RecordEntityAttachment;
}

void GameRecorder::Init()
{
	if (PARAM_DR_AllInsts.Get())
	{
		debugPlayback::SetTrackAllEntities(true);
	}

	const char *pFilterFileNames=0;
	if (PARAM_DR_LoadFilter.Get(pFilterFileNames))
	{
		if (pFilterFileNames && pFilterFileNames[0])
		{
			ClearFilter();

			//Allow multiple filters to be loaded
			char buffer[512];
			safecpy(buffer, pFilterFileNames);
			const char* seps = " ,;";
			char* pFilterName = strtok(buffer, seps);
			while(pFilterName)
			{
				//Load each filter
				LoadFilter(pFilterName);

				// Try to read the token as a float.
				pFilterName = strtok(NULL, seps);
			}
		}
	}

	if (PARAM_DR_Autostart.Get())
	{
		StartRecording();
	}

	const char *modeString = 0;
	if (PARAM_DR_TrackMode.Get(modeString))
	{
		if (!stricmp(modeString, "Selected"))
		{
			debugPlayback::SetTrackMode(debugPlayback::eTrackOne);
		}
		else if (!stricmp(modeString, "Player"))
		{
			debugPlayback::SetTrackMode(debugPlayback::eTrackOne);
			sbTrackPlayer = true;
		}
		else if (!stricmp(modeString, "All"))
		{
			debugPlayback::SetTrackMode(debugPlayback::eTrackAll);
		}
		else if (!stricmp(modeString, "Frustrum"))
		{
			debugPlayback::SetTrackMode(debugPlayback::eTrackInFrustrum);
		}
		else if (!stricmp(modeString, "InRange"))
		{
			debugPlayback::SetTrackMode(debugPlayback::eTrackInRange);
		}
	}
}

GameRecorder::~GameRecorder()
{
}

void GameRecorder::OnSave(DebugRecorder::DebugRecorderData & rData)
{
	AppLevelSaveData *pAppData = (AppLevelSaveData *)&rData;
	pAppData->m_StoredSkeletonData.Resize( m_StoredSkeletonData.GetNumUsed());
	pAppData->m_StoredSkeletonDataIDs.Resize( m_StoredSkeletonData.GetNumUsed());
	atMap<size_t, StoredSkelData*>::Iterator itr = m_StoredSkeletonData.CreateIterator();
	int iCounter = 0;
	while (!itr.AtEnd())
	{
		pAppData->m_StoredSkeletonData[iCounter] = itr.GetData();
		pAppData->m_StoredSkeletonDataIDs[iCounter] = itr.GetKey();
		++iCounter;
		++itr;
	}
}

void GameRecorder::OnLoad(DebugRecorder::DebugRecorderData & rData)
{
	AppLevelSaveData *pAppData = (AppLevelSaveData *)&rData;
	const int iMax = Min(pAppData->m_StoredSkeletonDataIDs.GetCount(), pAppData->m_StoredSkeletonData.GetCount());
	Assertf(pAppData->m_StoredSkeletonDataIDs.GetCount() == pAppData->m_StoredSkeletonData.GetCount(), "Error loading save data: why are these values different (%d != %d)", pAppData->m_StoredSkeletonDataIDs.GetCount(), pAppData->m_StoredSkeletonData.GetCount());
	for (int i=0 ; i<iMax ; i++)
	{
		m_StoredSkeletonData.Insert( pAppData->m_StoredSkeletonDataIDs[i], pAppData->m_StoredSkeletonData[i] );
	}

	m_bPosePedsIfPossible = false; 
}

void GameRecorder::OnClearRecording()
{
	atMap<size_t, StoredSkelData*>::Iterator itr = m_StoredSkeletonData.CreateIterator();
	while (!itr.AtEnd())
	{
		delete itr.GetData();
		++itr;
	}
	m_StoredSkeletonData.Kill();
}

static float sfBoundDrawDist=0.0f;
void GameRecorder::OnStartDebugDraw()
{
#if __PFDRAW
	sfBoundDrawDist = PFD_BoundDrawDistance.GetValue();
	PFD_BoundDrawDistance.SetValue(10000.0f);

	//Draw the selected object if we have one
	if (m_bHighlightSelected)
	{
		debugPlayback::phInstId rSelected(debugPlayback::GetCurrentSelectedEntity());
		switch (debugPlayback::GetTrackMode())
		{
		case debugPlayback::eTrackOne:
			if (rSelected.IsValid())
			{
				grcColor(Color32(0.0f, 0.5f, 0.5f, 0.5f));
				rSelected->GetArchetype()->GetBound()->Draw(rSelected->GetMatrix());
			}
			break;
		case debugPlayback::eTrackMany: {
				int iCount = debugPlayback::GetNumSelected();
				bool bDrewSelected = false;
				for (int i=0 ; i<iCount ; i++)
				{
					debugPlayback::phInstId rThisSelected = debugPlayback::GetSelected(i);
					if (rThisSelected.IsValid())
					{
						if (rThisSelected == rSelected)
						{
							grcColor(Color32(0.0f, 0.5f, 0.5f, 0.5f));
							bDrewSelected = true;
						}
						else
						{
							grcColor(Color32(0.0f, 0.25f, 0.25f, 0.25f));
						}

						rThisSelected->GetArchetype()->GetBound()->Draw(rThisSelected->GetMatrix());
					}
				}
				if (!bDrewSelected && rSelected.IsValid())
				{
					//Flash until we've actually selected it
					if ((TIME.GetFrameCount() & 0x7) > 3)
					{
						grcColor(Color32(0.0f, 0.5f, 0.5f, 0.5f));
						rSelected->GetArchetype()->GetBound()->Draw(rSelected->GetMatrix());
					}
				}
			break;
		}
		default:
			if (rSelected.IsValid())
			{
				grcColor(Color32(0.25f, 0.25f, 0.25f, 0.5f));
				rSelected->GetArchetype()->GetBound()->Draw(rSelected->GetMatrix());
			}
			break;
		}
	}
#endif // __PFDRAW
}

u32 m_LastMouseTrackFrame=0;
float m_fLastMouseY=0.0f;
float m_fMouseYOffset=0.0f;
bool m_bDragging=false;
void GameRecorder::OnEndDebugDraw(bool bMenuItemSelected)
{
#if __PFDRAW
	PFD_BoundDrawDistance.SetValue(sfBoundDrawDist);
#endif

	debugPlayback::UpdatePerhapsSelected(!bMenuItemSelected);

	if (AnimMotionTreeTracker::sm_iHoveredFrame == TIME.GetFrameCount())
	{
		if (ioMouse::GetButtons() & ioMouse::MOUSE_LEFT)
		{
			m_bDragging = true;
		}
	}
	else if (!(ioMouse::GetButtons() & ioMouse::MOUSE_LEFT))
	{
		m_bDragging = false;
	}

	if (m_bDragging)
	{
		float fMouseY = float(ioMouse::GetY());
		if (m_LastMouseTrackFrame == (TIME.GetFrameCount() - 1))
		{
			float fDeltaY = fMouseY - m_fLastMouseY;
			m_fMouseYOffset += fDeltaY;
		}

		m_LastMouseTrackFrame = TIME.GetFrameCount();
		m_fLastMouseY = fMouseY;

		if (m_fMouseYOffset > 10.0f)
		{
			AnimMotionTreeTracker::sm_LineOffset = Max(AnimMotionTreeTracker::sm_LineOffset-1, 0);
			m_fMouseYOffset -= 10.0f;
		}
		if (m_fMouseYOffset < -10.0f)
		{
			AnimMotionTreeTracker::sm_LineOffset++;
			m_fMouseYOffset += 10.0f;
		}
	}
	else
	{
		m_fMouseYOffset = 0.0f;
	}
}

void GameRecorder::OnStartRecording()
{
	//Data should have been cleared
	Assert(!m_StoredSkeletonData.GetNumUsed());

	//Fill out some meta data

	//Teleport location
	m_bHasTeleportLocation = false;
	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
	if (pPlayerPed)
	{
		m_vTeleportLocation = pPlayerPed->GetMatrixPrePhysicsUpdate().GetCol3ConstRef();
		m_bHasTeleportLocation = true;
	}

	//Reset data as needed
	m_LastFrameInsts.Reset();
	m_FrameInsts.Reset();
}

void GameRecorder::OnDebugDraw3d(const debugPlayback::EventBase *pHovered)
{
	debugWanted::OnDebugDraw3d(GetSelectedFrame(), GetSelectedEvent(), pHovered);
}

void GameRecorder::OnDrawFrame(const debugPlayback::Frame *pFrame)
{
	if (pFrame)
	{
		m_iDrawnFrameNetTime = pFrame->m_NetworkTime;
	}
}

void GameRecorder::ClearFilter()
{
	DisableAllEventTypes();
	m_bRecordSkeletonsEveryFrame = false;
	m_bPosePedsIfPossible = false;
	StoreSkeleton::ms_bUseLowLodSkeletons = false;
}

bool GameRecorder::LoadFilter(const char *pFilerFileName)
{
	FilterInfo *pFilterInfo = 0;
	if (Verifyf(PARSER.LoadObjectPtr(pFilerFileName, "meta", pFilterInfo), "Failed to load debug filter %s", pFilerFileName))
	{
		for (int i=0 ; i<pFilterInfo->m_Filters.GetCount() ; i++)
		{
			debugPlayback::DR_EventType::Enable( pFilterInfo->m_Filters[i] );
		}
		m_bRecordSkeletonsEveryFrame |= pFilterInfo->m_bRecordSkeletonsEveryFrame;
		m_bPosePedsIfPossible |= pFilterInfo->m_bPosePedsIfPossible;
		StoreSkeleton::ms_bUseLowLodSkeletons |= pFilterInfo->m_bUseLowLodSkeletons;
		delete pFilterInfo;
		return true;
	}
	Errorf("Failed to load debug filter %s", pFilerFileName);
	return false;
}

bool GameRecorder::SaveFilter(const char *pFilerFileName)
{
	FilterInfo tmpInfo;
	debugPlayback::DR_EventType::AddEnabled(tmpInfo.m_Filters);
	tmpInfo.m_bRecordSkeletonsEveryFrame = m_bRecordSkeletonsEveryFrame;
	tmpInfo.m_bPosePedsIfPossible = m_bPosePedsIfPossible;
	tmpInfo.m_bUseLowLodSkeletons = StoreSkeleton::ms_bUseLowLodSkeletons;
	return PARSER.SaveObject(pFilerFileName, "meta", &tmpInfo);
}

bool AreInstsFromSameEntity(debugPlayback::phInstId rCurrentInst, debugPlayback::phInstId rOtherInst)
{
	GameRecorder *pInst = GameRecorder::GetAppLevelInstance();
	return pInst->DoInstsShareAnEntity(rCurrentInst, rOtherInst);
}

void FilterToSelectedEntity(debugPlayback::DataBase& rData)
{
	debugPlayback::phInstId rSelected(debugPlayback::GetCurrentSelectedEntity());
	if (rSelected.IsValid())
	{
		if (rData.IsNewObjectData())
		{
			debugPlayback::NewObjectData *pNOD = static_cast<debugPlayback::NewObjectData *>(&rData);
			if (AreInstsFromSameEntity(rSelected, pNOD->m_rInst))
			{
				if (!pNOD->IsFilterActive())
				{
					pNOD->OnFilterToggle();
				}
			}
			else
			{
				if (pNOD->IsFilterActive())
				{
					pNOD->OnFilterToggle();
				}
			}
		}			
	}
}

static debugPlayback::phInstId toEnable;
void AddSelectedEntityToFilter(debugPlayback::DataBase& rData)
{
	if (toEnable.IsAssigned())
	{
		if (rData.IsNewObjectData())
		{
			debugPlayback::NewObjectData *pNOD = static_cast<debugPlayback::NewObjectData *>(&rData);
			if (AreInstsFromSameEntity(toEnable, pNOD->m_rInst))
			{
				if (!pNOD->IsFilterActive())
				{
					pNOD->OnFilterToggle();
				}
			}
		}			
	}
}

void FilterToAllEntity(debugPlayback::DataBase& rData)
{
	if (rData.IsNewObjectData())
	{
		debugPlayback::NewObjectData *pNOD = static_cast<debugPlayback::NewObjectData *>(&rData);
		if (!pNOD->IsFilterActive())
		{
			pNOD->OnFilterToggle();
		}
	}			
}

void ClearDataFilter(debugPlayback::DataBase& rData)
{
	if (rData.IsNewObjectData())
	{
		debugPlayback::NewObjectData *pNOD = static_cast<debugPlayback::NewObjectData *>(&rData);
		if (pNOD->IsFilterActive())
		{
			pNOD->OnFilterToggle();
		}
	}			
}


bool GameRecorder::AppOptions(debugPlayback::TextOutputVisitor &rMenuText, debugPlayback::TextOutputVisitor &rInfoText, bool bMouseDown, bool bPaused)
{
	bool bMenuItemSelected = false;
	if (rMenuText.AddLine("ShowHelp[%s]", m_bShowHelpText ? "X" : " ") && bMouseDown)
	{
		m_bShowHelpText = !m_bShowHelpText;
		bMenuItemSelected = true;
	}

	if (m_bShowHelpText)
	{
		rInfoText.AddLine("CTRL + C on your debug keyboard will toggle continuous mode");
		rInfoText.AddLine("CTRL + M on your debug keyboard will toggle the mode menu and cycle modes");
		rInfoText.AddLine("CTRL + S on your debug keyboard will start/stop a new recording");
		rInfoText.AddLine("CTRL + A auto selects the player / player's vehicle");
	}

	debugPlayback::phInstId rSelected(debugPlayback::GetCurrentSelectedEntity());
	if (debugPlayback::GetTrackMode() == debugPlayback::eTrackOne)
	{
		if (rSelected.IsValid())
		{
			switch (rSelected->GetClassType())
			{
			case PH_INST_FRAG_VEH:
			case PH_INST_FRAG_PED:
			case PH_INST_FRAG_OBJECT:
			case PH_INST_VEH:
			case PH_INST_PED:
			case PH_INST_OBJECT: {
				CEntity *pObject = (CEntity*)rSelected->GetUserData();
				if (pObject)
				{
					rInfoText.AddLine("Selected: %s", pObject->GetModelName());
				}
				break;
			}
			default:
				rInfoText.AddLine("Selected: %x [%s]", rSelected.GetObject(), rSelected->GetArchetype()->GetFilename());
				break;
			}

			//if (rMenuText.AddLine("Highlight[%s]", m_bHighlightSelected ? "X" : " ") && bMouseDown)
			//{
			//	m_bHighlightSelected = !m_bHighlightSelected;
			//	bMenuItemSelected = true;
			//}
		}
	}

	//if we're not already recording.
	//Shortcut to modes list
	if (ioMapper::DebugKeyPressed(KEY_M) && ioMapper::DebugKeyDown(KEY_CONTROL))
	{
		if (GetFilterIndex() == 4)
		{
			++m_iLastModeSelected;
			m_bSelectMode = true;
		}
		else
		{
			SetFilterIndex(4);			
		}
	}

	if (ioMapper::DebugKeyPressed(KEY_S) && ioMapper::DebugKeyDown(KEY_CONTROL))
	{
		if (IsRecording())
		{
			StopRecording();
		}
		else
		{
			StartRecording();
		}
	}

	//And some other nice menu options to get rec
	if (ioMapper::DebugKeyPressed(KEY_A) && ioMapper::DebugKeyDown(KEY_CONTROL))
	{	
		CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
		if (pPlayerPed)
		{
			if (pPlayerPed->GetVehiclePedInside())
			{
				debugPlayback::TrackEntity(pPlayerPed->GetVehiclePedInside(), true);
			}
			else if (pPlayerPed->GetMyMount())
			{
				debugPlayback::TrackEntity(pPlayerPed->GetMyMount(), true);
			}
			else
			{
				debugPlayback::TrackEntity(pPlayerPed, true);
			}
			debugPlayback::SetTrackMode( debugPlayback::eTrackOne );
			bMenuItemSelected = true;
		}
	}


	//Allow us to record ALL entities
	const char *pTracking="Unknown";
	char bufferIfNeeded[256];
	debugPlayback::eTrackMode nextMode = debugPlayback::eTrackAll;
	switch (debugPlayback::GetTrackMode())
	{
		case debugPlayback::eTrackAll:
			pTracking="All";
			nextMode = debugPlayback::eTrackInFrustrum;
			break;
		case debugPlayback::eTrackInFrustrum:
			pTracking="Frustrum";
			nextMode = debugPlayback::eTrackMany;
			break;
		case debugPlayback::eTrackMany:
			pTracking="InstList";
			nextMode = debugPlayback::eTrackOne;
			break;
		case debugPlayback::eTrackOne:
			pTracking="Selected";
			nextMode = debugPlayback::eTrackInRange;
			break;
		case debugPlayback::eTrackInRange:
			formatf(bufferIfNeeded, "InRange[%f]", sfTrackRange);
			pTracking=bufferIfNeeded;
			nextMode = debugPlayback::eTrackAll;
			break;
		default:
			break;
	}

	if (rMenuText.AddLine("[TrackInsts:%s]", pTracking))
	{
		if (bMouseDown)
		{
			debugPlayback::SetTrackMode(nextMode);
			bMenuItemSelected = true;
			bMouseDown = false;
		}
		else
		{
			if (debugPlayback::GetTrackMode() == debugPlayback::eTrackInRange)
			{
				//Hovering, allow mouse wheel to change range
				if (ioMouse::GetDZ() > 0)
				{
					sfTrackRange *= 2.0f;
				}
				else if(ioMouse::GetDZ() < 0)
				{
					sfTrackRange *= 0.5f;
					if (sfTrackRange < 0.5f)
					{
						sfTrackRange = 0.5f;
					}
				}
			}
		}
	}


	if (bPaused || !IsRecording())
	{
		if (NetworkInterface::IsGameInProgress())
		{
			if (rMenuText.AddLine("SyncDisplay[%s]", m_bSyncNetDisplays ? "X" : " ") && bMouseDown)
			{
				m_bSyncNetDisplays = !m_bSyncNetDisplays;
			}
		}

		rMenuText.AddLine("Display Filter");
		rMenuText.PushIndent();
		if (rMenuText.AddLine("Clear") && bMouseDown)
		{
			CallOnAllStoredData(ClearDataFilter);
			SetDataFilterOn(false);
			RefreshFilteredState();
			bMenuItemSelected = true;
			bMouseDown = false;
		}


		const debugPlayback::EventBase *pSelectedEvent = GetSelectedEvent();
		if (pSelectedEvent)
		{
			if (pSelectedEvent->IsPhysicsEvent())
			{
				const debugPlayback::PhysicsEvent *pPhysEvent = static_cast<const debugPlayback::PhysicsEvent *>(pSelectedEvent);
				if (rMenuText.AddLine("+Selected Event") && bMouseDown)
				{
					EnableDataFilterForEvent(*pSelectedEvent);
					SetDataFilterOn(true);

					//Also any data for related insts
					toEnable = pPhysEvent->m_rInst;
					CallOnAllStoredData(AddSelectedEntityToFilter);
					bMenuItemSelected = true;
					bMouseDown = false;
				}
			}
		}

		if (rSelected.IsValid())
		{
			if (rMenuText.AddLine("+Focus Entity") && bMouseDown)
			{
				toEnable = debugPlayback::GetCurrentSelectedEntity();
				CallOnAllStoredData(AddSelectedEntityToFilter);
				SetDataFilterOn(true);
				RefreshFilteredState();
				bMenuItemSelected = true;
				bMouseDown = false;
			}

			if (rMenuText.AddLine("=Focus Entity") && bMouseDown)
			{
				CallOnAllStoredData(ClearDataFilter);
				toEnable = debugPlayback::GetCurrentSelectedEntity();
				CallOnAllStoredData(AddSelectedEntityToFilter);
				SetDataFilterOn(true);
				RefreshFilteredState();
				bMenuItemSelected = true;
				bMouseDown = false;
			}
		}
		rMenuText.PopIndent();
	}

	rMenuText.AddLine("Record Filter");
	rMenuText.PushIndent();
	if (rMenuText.AddLine("Clear") && bMouseDown)
	{
		debugPlayback::ClearSelectedEntities();
		bMenuItemSelected = true;
		bMouseDown = false;
	}

	if (rSelected.IsValid())
	{
		if (rMenuText.AddLine("+Focus Entity") && bMouseDown)
		{
			debugPlayback::AddSelectedEntity( *rSelected.GetObject() );
			bMenuItemSelected = true;
			bMouseDown = false;
		}

		if (rMenuText.AddLine("=Focus Entity") && bMouseDown)
		{
			debugPlayback::SetTrackMode(debugPlayback::eTrackOne);
			debugPlayback::SetCurrentSelectedEntity( rSelected.GetObject(), true );
			bMenuItemSelected = true;
			bMouseDown = false;
		}
	}
	rMenuText.PopIndent();

	if (bPaused || !IsRecording())
	{
		if (GetSelectedFrame())
		{
			const debugPlayback::Frame &rFrame = *GetSelectedFrame();
			static bool bShowReplayOptions = false;
			if (rMenuText.AddLine("Replay") && bMouseDown)
			{
				bShowReplayOptions = !bShowReplayOptions;
				bMenuItemSelected = true;
				bMouseDown = false;
			}
			if(bShowReplayOptions)
			{
				rMenuText.PushIndent();
			
				if (rMenuText.AddLine("Trigger frame") && bMouseDown)
				{
					bMenuItemSelected = true;
					bMouseDown = false;
					mp_ReplayFrame = &rFrame;

					m_bPauseOnEventReplay = false;
					if (fwTimer::IsUserPaused())		
					{
						fwTimer::EndUserPause();
						m_bPauseOnEventReplay = true;
					}
				}

				//These two lines toggle the check box
				bool bSelected = rMenuText.AddLine("New object[%s]", m_bCreateObjectForReplay ? "X" : " ");
				bSelected |= rMenuText.AddLine("Use selected[%s]", m_bCreateObjectForReplay ? " " : "X");
				if (bSelected && bMouseDown)
				{
					m_bCreateObjectForReplay = !m_bCreateObjectForReplay;
					bMenuItemSelected = true;
					bMouseDown = false;
				}

				if (rMenuText.AddLine("Involve player[%s]", m_bInvolvePlayer ? "X" : " ") && bMouseDown)
				{
					m_bInvolvePlayer = !m_bInvolvePlayer;
					bMenuItemSelected = true;
					bMouseDown = false;
				}

				if (CEntity *pEnt = DebugReplayObjectInfo::m_rReplayObject)
				{
					//Don't offer destroy option if the replay object is the player
					bool bIsPlayer = false;
					if (pEnt->GetIsTypePed())
					{
						CPed *pPed = (CPed*)pEnt;
						if (pPed->IsLocalPlayer())
						{
							bIsPlayer = false;
						}
					}

					if ((!bIsPlayer) && rMenuText.AddLine("Destroy prop") && bMouseDown)
					{
						m_bDestroyReplayObject = true;
						bMenuItemSelected = true;
						bMouseDown = false;
					}
				}

				rMenuText.PopIndent();
			}
		}

		if (rMenuText.AddLine("TrackCamera[%s]", m_bReplayCamera ? "X" : " ") && bMouseDown)
		{
			bMenuItemSelected = true;
			bMouseDown = false;
			m_bReplayCamera = !m_bReplayCamera;
		}
	}

	//if (DR_EVENT_ENABLED(AnimMotionTreeTracker))
	{
		if (rMenuText.AddLine("PoseEntites[%s]", m_bPosePedsIfPossible ? "X" : " ") && bMouseDown)
		{
			bMenuItemSelected = true;
			bMouseDown = false;
			m_bPosePedsIfPossible = !m_bPosePedsIfPossible;
		}
	}

	if (DR_EVENT_ENABLED(StoreSkeletonPostUpdate) || DR_EVENT_ENABLED(StoreSkeletonPreUpdate))
	{
		if (rMenuText.AddLine("Skeletons Every Frame[%s]", m_bRecordSkeletonsEveryFrame ? "X" : " ") && bMouseDown)
		{
			bMenuItemSelected = true;
			bMouseDown = false;
			m_bRecordSkeletonsEveryFrame = !m_bRecordSkeletonsEveryFrame;
		}

		if (rMenuText.AddLine("Low LOD Skeletons[%s]", StoreSkeleton::ms_bUseLowLodSkeletons ? "X" : " ") && bMouseDown)
		{
			bMenuItemSelected = true;
			bMouseDown = false;
			StoreSkeleton::ms_bUseLowLodSkeletons = !StoreSkeleton::ms_bUseLowLodSkeletons;
		}
	}

	return bMenuItemSelected;
}

void GameRecorder::RecordEntityAttachment(const phInst *pAttached, const phInst *pAttachedTo, const Matrix34 &matNew)
{
	debugPlayback::RecordEntityAttachment(pAttached, pAttachedTo, matNew);
}

const phInst *GameRecorder::GetInstFromCreature(const crCreature *pCreature)
{
	if (pCreature == m_CachedCreatureLookupCreature)
	{
		return m_CachedCreatureLookupInst;
	}
	const phInst * const *ppInst = m_CreatureToInstMap.Access(pCreature);
	m_CachedCreatureLookupCreature = pCreature;
	m_CachedCreatureLookupInst = ppInst ? *ppInst : 0;
	return m_CachedCreatureLookupInst;
}

void GameRecorder::NewInstFunc(debugPlayback::phInstId id)
{
	///Store some information on the object we'll need to save for the future
	DebugRecorder &rRecorder = *DebugRecorder::GetInstance();
	if (!rRecorder.GetSharedData(id.ToU32()))
	{
		DR_MEMORY_HEAP();
		debugPlayback::NewObjectData *pOb = debugPlayback::NewObjectData::CreateNewObjectData(id);
		rRecorder.AddSharedData(*pOb, id.ToU32());
	}

	//Mark for storing any end of frame data (currently skeletons and aiTask trees)
	if (m_FrameInsts.GetCount() < m_FrameInsts.GetMaxCount())
	{
		for (int i=0 ; i<m_FrameInsts.GetCount() ; i++)
		{
			if (m_FrameInsts[i] == id)
			{
				return;
			}
		}

		m_FrameInsts.Append() = id;
	}
}

const DebugReplayObjectInfo* GameRecorder::GetObjectInfo(debugPlayback::phInstId rInst) const
{
	if (rInst.IsAssigned())
	{
		debugPlayback::DebugRecorder *pInstance = debugPlayback::DebugRecorder::GetInstance();
		if(pInstance)
		{
			debugPlayback::DataBase *pData = pInstance->GetSharedData(rInst.ToU32());
			if (pData)
			{
				return dynamic_cast<DebugReplayObjectInfo*>(pData);
			}
		}
	}
	return 0;
}

bool GameRecorder::DoInstsShareAnEntity(debugPlayback::phInstId rCurrentInst, debugPlayback::phInstId rOtherInst) const
{
	if (rCurrentInst.IsAssigned())
	{
		if (rCurrentInst == rOtherInst)
		{
			return true;
		}
		const debugPlayback::DataBase *pData0Base = GetSharedData(rCurrentInst.ToU32());
		if (pData0Base)
		{
			const DebugReplayObjectInfo *pData0 = dynamic_cast<const DebugReplayObjectInfo *>(pData0Base);
			if (pData0)
			{
				const debugPlayback::DataBase *pData1Base = GetSharedData(rOtherInst.ToU32());
				if (pData1Base)
				{
					const DebugReplayObjectInfo *pData1 = dynamic_cast<const DebugReplayObjectInfo *>(pData1Base);
					if (pData1)
					{
						return (pData1->GetFirstInChain() == pData0->GetFirstInChain());
					}
				}
			}
		}
	}
	return false;
}

bool GameRecorder::HasSkelData(u32 iSig)
{
	return !!m_StoredSkeletonData.Access( iSig );
}

StoredSkelData *GameRecorder::GetSkelData(u32 iSig)
{
	return m_StoredSkeletonData[iSig];
}

void GameRecorder::AddSkelData(StoredSkelData *pData, u32 iSig)
{
	Assertf(!HasSkelData(iSig), "Trying to add skeleton data multiple times - will leak");
	m_StoredSkeletonData[iSig] = pData;
}

void EnableCorePhyics()
{
	//Enable the physics specific types
	debugPlayback::DR_EventType::Enable("RecordedContact");
	debugPlayback::DR_EventType::Enable("RecordedContactWithMatrices");
	debugPlayback::DR_EventType::Enable("ApplyForceEvent");
	debugPlayback::DR_EventType::Enable("ApplyForceCGEvent");
	debugPlayback::DR_EventType::Enable("ApplyImpulseCGEvent");
	debugPlayback::DR_EventType::Enable("ApplyTorqueEvent");
	debugPlayback::DR_EventType::Enable("ApplyImpulseEvent");
	debugPlayback::DR_EventType::Enable("SetVelocityEvent");
	debugPlayback::DR_EventType::Enable("SetAngularMomentumEvent");
	debugPlayback::DR_EventType::Enable("SetAngularVelocityEvent");
	debugPlayback::DR_EventType::Enable("UpdateLocation");
	debugPlayback::DR_EventType::Enable("SetMatrixEvent");
	debugPlayback::DR_EventType::Enable("EntityAttachmentUpdate");
	debugPlayback::DR_EventType::Enable("PedGroundInstanceEvent");
	debugPlayback::DR_EventType::Enable("PedCapsuleInfoEvent");
}

void EnableVehiclePhyics()
{
	//Enable the physics specific types
	debugPlayback::DR_EventType::Enable("RecordedContact");
	debugPlayback::DR_EventType::Enable("RecordedContactWithMatrices");
	debugPlayback::DR_EventType::Enable("ApplyForceEvent");
	debugPlayback::DR_EventType::Enable("ApplyForceCGEvent");
	debugPlayback::DR_EventType::Enable("ApplyImpulseCGEvent");
	debugPlayback::DR_EventType::Enable("ApplyTorqueEvent");
	debugPlayback::DR_EventType::Enable("ApplyImpulseEvent");
	debugPlayback::DR_EventType::Enable("SetVelocityEvent");
	debugPlayback::DR_EventType::Enable("SetAngularMomentumEvent");
	debugPlayback::DR_EventType::Enable("SetAngularVelocityEvent");
	debugPlayback::DR_EventType::Enable("UpdateLocation");
	debugPlayback::DR_EventType::Enable("SetMatrixEvent");
	debugPlayback::DR_EventType::Enable("EntityAttachmentUpdate");
	debugPlayback::DR_EventType::Enable("PedGroundInstanceEvent");
	debugPlayback::DR_EventType::Enable("PedCapsuleInfoEvent");
	debugPlayback::DR_EventType::Enable("SetTaggedVectorEvent");
}

void DisableAllEventTypes()
{
	debugWanted::StopWantedRecording();
	debugPlayback::DR_EventType::DisableAll();
}

void EnableWantedRecording()
{
	debugPlayback::DR_EventType::Enable("LawEntityEvent");
	debugPlayback::DR_EventType::Enable("LawUpdateEvent");
	debugPlayback::DR_EventType::Enable("StoreSkeletonPostUpdate");
	debugWanted::StartWantedRecording(10);	//TODO: Make this variable
}

void EnableAnimationRecording(bool 
#if DR_ANIMATIONS_ENABLED
							  bEnableLowLevel
#endif
							  )
{
	//Enable tasks useful for animation debugging
	debugPlayback::DR_EventType::Enable("CTaskFlowDebugEvent");
	debugPlayback::DR_EventType::Enable("MoveTransitionStringCollection");
	if (bEnableLowLevel)
	{
		debugPlayback::DR_EventType::Enable("MoveConditionStringCollection");
		debugPlayback::DR_EventType::Enable("MoveSetValueStringCollection");
	}
	debugPlayback::DR_EventType::Enable("MoveGetValue");
	debugPlayback::DR_EventType::Enable("MoveSetValue");
	debugPlayback::DR_EventType::Enable("StoreSkeletonPostUpdate");
	debugPlayback::DR_EventType::Enable("EntityAttachmentUpdate");

	//?
	debugPlayback::DR_EventType::Enable("PedDesiredVelocityEvent");			

	debugPlayback::DR_EventType::Enable("PedAnimatedVelocityEvent");			

}

bool GameRecorder::AddAppFilters(debugPlayback::OnScreenOutput &rOutput, debugPlayback::OnScreenOutput &
#if !PDR_ENABLED
								 rInfoText
#endif
								 , int &iFilterIndex, bool bMouseDown)
{
	int iMode=0;
	bool bSelected = false;	//Set to true to consume mouse input... bad design choice that will need revisiting 
							//with better input handling.... one day....maybe

	switch(iFilterIndex)
	{
	case 4:
		if (rOutput.AddLine("<-MODES->") && bMouseDown)
		{
			++iFilterIndex;
			return true;
		}

		rOutput.m_bDrawBackground = iMode == m_iLastModeSelected;
		if((rOutput.AddLine("[Physics]") && bMouseDown) || ((iMode == m_iLastModeSelected) && m_bSelectMode))
		{
			bMouseDown = false;
			bSelected = true;
			m_iLastModeSelected = iMode;
			m_bSelectMode = false;
			
			//Disable all event types
			DisableAllEventTypes();

			EnableCorePhyics();
		}

		++iMode;

		rOutput.m_bDrawBackground = iMode == m_iLastModeSelected;
		if((rOutput.AddLine("[Vehicle Physics]") && bMouseDown) || ((iMode == m_iLastModeSelected) && m_bSelectMode))
		{
			bMouseDown = false;
			bSelected = true;
			m_iLastModeSelected = iMode;
			m_bSelectMode = false;

			//Disable all event types
			DisableAllEventTypes();

			EnableVehiclePhyics();
		}


#if !PDR_ENABLED
		if (rOutput.m_bDrawBackground)	//I.e. is the selected mode
		{
			//Show info text letting the user know physics needs to be enabled
			rInfoText.m_bDrawBackground = (TIME.GetFrameCount() & 0xf) > 7;
			rInfoText.AddLine("Physics recording requires PDR_ENABLED on in code!");
			rInfoText.m_bDrawBackground = false;
		}
#endif
		++iMode;

		rOutput.m_bDrawBackground = iMode == m_iLastModeSelected;
		if((rOutput.AddLine("[Ped Move]") && bMouseDown) || ((iMode == m_iLastModeSelected) && m_bSelectMode))
		{
			bMouseDown = false;
			bSelected = true;
			m_bSelectMode = false;
			m_iLastModeSelected = iMode;

			//Disable all event types
			DisableAllEventTypes();

			debugPlayback::DR_EventType::Enable("QPedSpringEvent");
			debugPlayback::DR_EventType::Enable("PedDesiredVelocityEvent");			
			debugPlayback::DR_EventType::Enable("PedAnimatedVelocityEvent");			
			debugPlayback::DR_EventType::Enable("RecordedContact");

			debugPlayback::DR_EventType::Enable("InstLabelEvent");
			debugPlayback::DR_EventType::Enable("SetTaggedFloatEvent");
			debugPlayback::DR_EventType::Enable("SetTaggedVectorEvent");
			debugPlayback::DR_EventType::Enable("SetTaggedMatrixEvent");
		}
		++iMode;

		rOutput.m_bDrawBackground = iMode == m_iLastModeSelected;
		if((rOutput.AddLine("[AI]") && bMouseDown) || ((iMode == m_iLastModeSelected) && m_bSelectMode))
		{
			bMouseDown = false;
			bSelected = true;
			m_bSelectMode = false;
			m_iLastModeSelected = iMode;

			//Disable all event types
			DisableAllEventTypes();

			//Enable the AI specific event types
			debugPlayback::DR_EventType::Enable("CTaskFlowDebugEvent");
			debugPlayback::DR_EventType::Enable("CStoreTasks");
		}
		++iMode;

#ifdef MOVE_DR_ENABLED
		rOutput.m_bDrawBackground = iMode == m_iLastModeSelected;
		if ((rOutput.AddLine("[Anim-high]") && bMouseDown) || ((iMode == m_iLastModeSelected) && m_bSelectMode))
		{
			bMouseDown = false;
			bSelected = true;
			m_bSelectMode = false;
			m_iLastModeSelected = iMode;

			DisableAllEventTypes();

			EnableAnimationRecording(false);
		}
		++iMode;

		rOutput.m_bDrawBackground = iMode == m_iLastModeSelected;
		if ((rOutput.AddLine("[Anim-low]") && bMouseDown) || ((iMode == m_iLastModeSelected) && m_bSelectMode))
		{
			bMouseDown = false;
			bSelected = true;
			m_bSelectMode = false;
			m_iLastModeSelected = iMode;

			DisableAllEventTypes();

			EnableAnimationRecording(true);
		}
		++iMode;
#endif //#ifdef MOVE_DR_ENABLED
		rOutput.m_bDrawBackground = iMode == m_iLastModeSelected;
		if ((rOutput.AddLine("[Wanted]") && bMouseDown) || ((iMode == m_iLastModeSelected) && m_bSelectMode))
		{
			bMouseDown = false;
			bSelected = true;
			m_bSelectMode = false;
			m_iLastModeSelected = iMode;

			DisableAllEventTypes();

			EnableWantedRecording();
		}
		++iMode;

		if (m_iLastModeSelected > iMode)
		{
			if (m_bSelectMode)
			{
				DisableAllEventTypes();
			}
			m_iLastModeSelected = 0;
		}
		break;

	default:
		iFilterIndex = 0;
		return false;
	}

	return bSelected;
}



namespace rage
{
	namespace debugPlayback
	{
		struct VehicleAvoidenceDataEvent : public debugPlayback::PhysicsEvent
		{
			struct StoredObstructionItem
			{
				//<Float16 name="m_Dist"/>
				//	<s8 name="m_Direction"/>
				//	<u8 name="m_Type"/>
				union {
					struct {
						u16 m_Dist;
						s16 m_Direction : 12;
						s16 m_Type : 4;
					};
					u32 m_AsU32;
				};

				void Extract(float fMinRange, float fMaxRange, float fCentreAngle, float /*fMaxAngle*/, float &rRange, float &rAngle) const
				{
					rAngle = fCentreAngle + (float(m_Direction) * (PI / 2047.0f));
					if (fMaxRange > fMinRange)
					{
						rRange = float(m_Dist) * 256.0f / 65535.0f;
					}
					else
					{
						rRange = fMaxRange;
					}
				}

				PAR_SIMPLE_PARSABLE;
			};

			Vec3V m_vRight;
			Vec3V m_vForwards;
			atArray<StoredObstructionItem> m_Obstructions;
			float m_fMinRange;
			float m_fMaxRange;
			float m_fCentreAngle;
			float m_fMaxAngle;
			float m_fVehDriveOrientation;

			virtual void DebugDraw3d(eDrawFlags /*drawFlags*/) const
			{
				for (int i=0 ; i<m_Obstructions.GetCount() ; i++)
				{
					Color32 col(255,0,0);
					switch(m_Obstructions[i].m_Type)
					{
					case 0:
						col.Set(255,0,0);
						break;
					case 1:
						col.Set(0,255,0);
						break;
					case 2:
						col.Set(0,0,255);
						break;
					case 3:
						col.Set(255,255,255);
						break;
					}
					col.SetAlpha(DebugRecorder::GetGlobalAlphaScale());
					float fAngle = 0.0f, fRange = 0.0f;
					m_Obstructions[i].Extract(m_fMinRange, m_fMaxRange, m_fCentreAngle, m_fMaxAngle, fRange, fAngle);

					Vec3V right = m_vRight * ScalarV(fRange * Sinf(m_fVehDriveOrientation - fAngle));
					Vec3V forwards = m_vForwards * ScalarV(fRange * Cosf(m_fVehDriveOrientation - fAngle));

					grcDrawLine(m_Pos, m_Pos+right+forwards, col);
				}
			}

			virtual bool DebugEventText(TextOutputVisitor &rOutput) const
			{
				PrintInstName(rOutput);
				rOutput.PushIndent();
				for (int i=0 ; i<m_Obstructions.GetCount() ; i++)
				{
					float fAngle = 0.0f, fRange = 0.0f;
					m_Obstructions[i].Extract(m_fMinRange, m_fMaxRange, 0.0f, m_fMaxAngle, fRange, fAngle);	//TMS: printing angles relative to car
					const char *pType="unknown";
					switch(m_Obstructions[i].m_Type)
					{
					case 0:
						pType="Veh";
						break;
					case 1:
						pType="Ped";
						break;
					case 2:
						pType="Obj";
						break;
					case 3:
						pType="Edge";
						break;
					}
					rOutput.AddLine("%s: Dir %f, Range %f", pType, fAngle, fRange);
				}
				rOutput.PopIndent();
				return true;
			}

			VehicleAvoidenceDataEvent(const CallstackHelper rCallstack, const phInst *pInst)
				:PhysicsEvent(rCallstack, pInst)				
				,m_fMinRange(0.0f)
				,m_fMaxRange(0.0f)
				,m_fCentreAngle(0.0f)
				,m_fMaxAngle(0.0f)
				,m_fVehDriveOrientation(0.0f)
			{
			}

			VehicleAvoidenceDataEvent()
				:m_fMinRange(0.0f)
				,m_fMaxRange(0.0f)
				,m_fCentreAngle(0.0f)
				,m_fMaxAngle(0.0f)
				,m_fVehDriveOrientation(0.0f)
			{
			}

			void SetData(float fMinDist, float fMaxDist, float fCentreAngle, float fMaxAngle, float fDriveOrientation, Vec3V_In vRight, Vec3V_In vVehForwards)
			{
				m_fMinRange = fMinDist;
				m_fMaxRange = fMaxDist;
				m_fCentreAngle = fCentreAngle;
				m_fMaxAngle = fMaxAngle;
				m_fVehDriveOrientation = fDriveOrientation;
				m_vRight = vRight;
				m_vForwards = vVehForwards;
			}

			PAR_PARSABLE;
			DR_EVENT_DECL(VehicleAvoidenceDataEvent)
		};

		struct VehicleTargetPointEvent : public debugPlayback::PhysicsEvent
		{
			virtual void DebugDraw3d(eDrawFlags drawFlags) const
			{
				Color32 col(255,0,0,DebugRecorder::GetGlobalAlphaScale());
				u32 utask = m_Task.GetHash();
				col.SetBytes(u8(utask >> 24), u8(utask >> 16), u8(utask >> 8), DebugRecorder::GetGlobalAlphaScale());
				if (drawFlags & eDrawSelected)
				{
					if ((TIME.GetFrameCount() & 0x15) > 0x7)
					{
						col.SetRed(0);
						col.SetGreen(0);
						col.SetBlue(0);
					}
				}
				if (drawFlags & eDrawHovered)
				{
					if ((TIME.GetFrameCount() & 0x15) > 0x7)
					{
						col.SetRed(255);
						col.SetGreen(255);
						col.SetBlue(255);
					}
				}

				grcDrawLine(m_TargetPos, m_Pos, col);
				grcDrawLine(m_TargetPos, m_TargetPos+Vec3V(0.0f,0.0f,2.5f), col);
				grcDrawLine(m_Pos, m_TargetPos+Vec3V(0.0f,0.0f,2.5f), col);
				grcColor(col);
			}

			//Click on far end to select line
			virtual bool GetEventPos(Vec3V &pos) const
			{
				pos = m_TargetPos;
				return true;
			}

			virtual const char *GetEventSubType() const 
			{
				return m_Task.GetCStr(); 
			}

			void SetData(const char *pTaskName, Vec3V_In targetPos)
			{
				m_TargetPos = targetPos;
				m_Task = pTaskName;
			}
			
			VehicleTargetPointEvent(const CallstackHelper rCallstack, const phInst *pInst)
				:PhysicsEvent(rCallstack, pInst)
			{
			}

			VehicleTargetPointEvent()
				:m_TargetPos(V_ZERO)
			{
			}

			atHashString m_Task;
			Vec3V m_TargetPos;

			PAR_PARSABLE;
			DR_EVENT_DECL(VehicleTargetPointEvent)
		};

		struct PedAnimatedVelocityEvent : public debugPlayback::PhysicsEvent
		{
			virtual void DebugDraw3d(eDrawFlags drawFlags) const
			{
				Color32 col(255,0,0,DebugRecorder::GetGlobalAlphaScale());
				if (drawFlags & eDrawSelected)
				{
					col.SetGreen(128);
				}
				if (drawFlags & eDrawHovered)
				{
					col.SetGreen(255);
				}

				DrawVelocity(m_Linear, m_Pos, ScalarV(V_ONE), col);
			}

			void SetData(Vec3V_In linear)
			{
				m_Linear = linear;
			}

			PedAnimatedVelocityEvent(const CallstackHelper rCallstack, const phInst *pInst)
				:PhysicsEvent(rCallstack, pInst)
			{
			}

			PedAnimatedVelocityEvent()
				:m_Linear(V_ZERO)
			{	}

			Vec3V m_Linear;

			PAR_PARSABLE;
			DR_EVENT_DECL(PedAnimatedVelocityEvent)		
		};

		struct DRNetBlenderStateEvent  : public debugPlayback::PhysicsEvent
		{
			virtual void DebugDraw3d(debugPlayback::eDrawFlags drawFlags) const
			{
				Color32 col(255,0,128,DebugRecorder::GetGlobalAlphaScale());
				if (drawFlags & eDrawSelected)
				{
					col.SetGreen(128);
				}
				if (drawFlags & eDrawHovered)
				{
					col.SetGreen(255);
				}

				DrawAxis(0.25f, m_Latest);

				grcWorldIdentity();
				grcDrawLine(m_Latest.GetCol0(), m_Pos, col);
			}

			void SetData(Mat34V_In lastRecieved, bool positionUpdated, bool orientationUpdated, bool velocityUpdated, bool angVelocityUpdated)
			{
				m_Latest = lastRecieved;
				m_PositionUpdated = positionUpdated;
				m_OrientationUpdated = orientationUpdated;
				m_VelocityUpdated = velocityUpdated;
				m_AngVelocityUpdated = angVelocityUpdated;
			}

			DRNetBlenderStateEvent(const debugPlayback::CallstackHelper rCallstack, const phInst *pInst)
				:PhysicsEvent(rCallstack, pInst)
			{
			}

			DRNetBlenderStateEvent() 
				: m_Latest(V_ZERO)
				, m_PositionUpdated(false)
				, m_OrientationUpdated(false)
				, m_VelocityUpdated(false)
				, m_AngVelocityUpdated(false) {}

			Mat34V m_Latest;
			bool m_PositionUpdated;
			bool m_OrientationUpdated;
			bool m_VelocityUpdated;
			bool m_AngVelocityUpdated;

			PAR_PARSABLE;
			DR_EVENT_DECL(DRNetBlenderStateEvent)
		};

		struct PedDesiredVelocityEvent : public debugPlayback::PhysicsEvent
		{
			virtual void DebugDraw3d(eDrawFlags drawFlags) const
			{
				Color32 col(255,0,128,DebugRecorder::GetGlobalAlphaScale());
				if (drawFlags & eDrawSelected)
				{
					col.SetGreen(128);
				}
				if (drawFlags & eDrawHovered)
				{
					col.SetGreen(255);
				}

				DrawVelocity(m_Linear, m_Pos, ScalarV(V_ONE), col);
				col.SetBlue(128);
				DrawRotation(m_Angular, m_Pos, ScalarV(V_ONE), col);
			}

			void SetData(Vec3V_In linear, Vec3V_In angular)
			{
				m_Linear = linear;
				m_Angular = angular;
			}

			PedDesiredVelocityEvent(const CallstackHelper rCallstack, const phInst *pInst)
				:PhysicsEvent(rCallstack, pInst)
			{
			}

			PedDesiredVelocityEvent():m_Linear(V_ZERO), m_Angular(V_ZERO){}

			Vec3V m_Linear;
			Vec3V m_Angular;

			PAR_PARSABLE;
			DR_EVENT_DECL(PedDesiredVelocityEvent)
		};
		
		struct VehicleDataEvent : public debugPlayback::PhysicsEvent
		{
			virtual void DebugDraw3d(eDrawFlags drawFlags) const
			{
				Color32 col(255,0,128,DebugRecorder::GetGlobalAlphaScale());
				if (drawFlags & eDrawSelected)
				{
					col.SetGreen(128);
					col.SetBlue(0);
				}
				if (drawFlags & eDrawHovered)
				{
					col.SetGreen(255);
				}

				QuatV vRot = m_Quat.Get();
				Mat34V mat;
				Mat34VFromQuatV(mat, vRot, m_Pos);		
				DrawAxis(1.0f, mat);

				const DebugReplayObjectInfo *pObject = GameRecorder::GetAppLevelInstance()->GetObjectInfo( m_rInst );
				if (pObject)
				{
					pObject->DrawBox(mat, col);
				}

				DrawVelocity(m_Linear, m_Pos, ScalarV(V_ONE), col);
				col.SetRed(128);
				DrawVelocity(m_vVehicleVelocity, m_Pos, ScalarV(V_ONE), col);
				col.SetBlue(128);
				DrawRotation(m_Angular, m_Pos, ScalarV(V_ONE), col);

				if(m_InterpolationTargetAndTime.GetWi())
				{
					Mat34V matLocal;
					Mat34VFromQuatV(matLocal, m_InterpolationTargetOrientation.Get(), m_InterpolationTargetAndTime.GetXYZ());
					DrawAxis(1.0f, matLocal);
				}
			}

			void SetData(Vec3V_In linear, Vec3V_In angular)
			{
				m_Linear = linear;
				m_Angular = angular;
			}

			VehicleDataEvent(const CallstackHelper rCallstack, const phInst *pInst)
				:PhysicsEvent(rCallstack, pInst)
				,m_Linear(V_ZERO)
				,m_Angular(V_ZERO)
				,m_InterpolationTargetAndTime(V_ZERO)
				,m_fSteerAngle(0.0f)
				,m_fSecondSteerAngle(0.0f)
				,m_fCheatPowerIncrease(0.0f)
				,m_fThrottle(0.0f)
				,m_fBrake(0.0f)
				,m_fForwardSpeed(0.0f)
				,m_fDesiredSpeed(0.0f)
				,m_fSmoothedSpeed(0.0f)
				,m_bHandBrake(false)
				,m_bCheatFlagGrip2(false)
				,m_bLodEventScanning(false)
				,m_bLodDummy(false)
				,m_bLodSuperDummy(false)
				,m_bLodTimeSlicing(false)
			{
				if (pInst && (pInst->GetClassType() == PH_INST_FRAG_VEH))
				{
					const phCollider *pCollider=PHSIM->GetCollider(pInst);
					if (pCollider)
					{
						m_Linear = pCollider->GetVelocity();
						m_Angular = pCollider->GetAngVelocity();
					}

					CVehicle *pVehicle = (CVehicle*)pInst->GetUserData();
					if (pVehicle)
					{
						//Store vehicle specific data here!
						m_vVehicleVelocity = RCC_VEC3V( pVehicle->GetVelocity() );
						m_fSteerAngle = pVehicle->GetSteerAngle();
						m_fSecondSteerAngle = pVehicle->GetSecondSteerAngle();
						m_fCheatPowerIncrease = pVehicle->GetCheatPowerIncrease(); 
						m_fThrottle = pVehicle->GetThrottle();
						m_fBrake = pVehicle->GetBrake();
						m_bHandBrake = pVehicle->GetHandBrake();
						m_bCheatFlagGrip2 = pVehicle->GetCheatFlag(VEH_CHEAT_GRIP2);
						m_InterpolationTargetAndTime = pVehicle->GetInterpolationTargetAndTime();
						if(m_InterpolationTargetAndTime.GetWi())
						{
							m_InterpolationTargetOrientation.Set(pVehicle->GetInterpolationTargetOrientation());
						}
						else
						{
							m_InterpolationTargetOrientation.Set(QuatV(V_IDENTITY));
						}

						m_fForwardSpeed = DotProduct(pVehicle->GetVelocity(), VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetB()));
						m_fDesiredSpeed = 0.0f;
						if( pVehicle->GetIntelligence()->GetActiveTaskSimplest() )
						{
							m_fDesiredSpeed = pVehicle->GetIntelligence()->GetActiveTaskSimplest()->GetCruiseSpeed();
						}
						m_fSmoothedSpeed = pVehicle->GetIntelligence()->GetSmoothedSpeedSq();
						m_bLodEventScanning = pVehicle->GetVehicleAiLod().IsLodFlagSet(CVehicleAILod::AL_LodEventScanning);
						m_bLodDummy = pVehicle->GetVehicleAiLod().IsLodFlagSet(CVehicleAILod::AL_LodDummy);
						m_bLodSuperDummy = pVehicle->GetVehicleAiLod().IsLodFlagSet(CVehicleAILod::AL_LodSuperDummy);
						m_bLodTimeSlicing = pVehicle->GetVehicleAiLod().IsLodFlagSet(CVehicleAILod::AL_LodTimeslicing);
					}

					QuatV vQ = QuatVFromMat34V(pInst->GetMatrix());
					vQ = Normalize( vQ );
					m_Quat.Set(vQ);
				}
			}

			VehicleDataEvent()
				:m_Linear(V_ZERO)
				,m_vVehicleVelocity(V_ZERO)
				,m_Angular(V_ZERO)
				,m_InterpolationTargetAndTime(V_ZERO)
				,m_fSteerAngle(0.0f)
				,m_fSecondSteerAngle(0.0f)
				,m_fCheatPowerIncrease(0.0f)
				,m_fThrottle(0.0f)
				,m_fBrake(0.0f)
				,m_fForwardSpeed(0.0f)
				,m_fDesiredSpeed(0.0f)
				,m_fSmoothedSpeed(0.0f)
				,m_bHandBrake(false)
				,m_bCheatFlagGrip2(false)
				,m_bLodEventScanning(false)
				,m_bLodDummy(false)
				,m_bLodSuperDummy(false)
				,m_bLodTimeSlicing(false)
			{}

			Vec3V m_Linear;
			Vec3V m_vVehicleVelocity;
			Vec3V m_Angular;
			Vec4V m_InterpolationTargetAndTime;
			StoreSkeleton::u32Quaternion m_InterpolationTargetOrientation;
			float m_fSteerAngle;
			float m_fSecondSteerAngle;
			float m_fCheatPowerIncrease;
			float m_fThrottle;
			float m_fBrake;
			float m_fForwardSpeed;
			float m_fDesiredSpeed;
			float m_fSmoothedSpeed;
			StoreSkeleton::u32Quaternion m_Quat;
			bool m_bHandBrake;
			bool m_bCheatFlagGrip2;
			bool m_bLodEventScanning;
			bool m_bLodDummy;
			bool m_bLodSuperDummy;
			bool m_bLodTimeSlicing;

			PAR_PARSABLE;
			DR_EVENT_DECL(VehicleDataEvent)
		};

		struct PedDataEvent : public debugPlayback::PhysicsEvent
		{
			virtual void DebugDraw3d(eDrawFlags drawFlags) const
			{
				Color32 col(0,255,128,DebugRecorder::GetGlobalAlphaScale());
				if (drawFlags & eDrawSelected)
				{
					col.SetRed(128);
				}
				if (drawFlags & eDrawHovered)
				{
					col.SetBlue(255);
				}

				DrawVelocity(m_Linear, m_Pos, ScalarV(V_ONE), col);
				col.SetBlue(128);
				DrawRotation(m_Angular, m_Pos, ScalarV(V_ONE), col);

				QuatV vRot = m_Quat.Get();
				Mat34V matLocal;
				Mat34VFromQuatV(matLocal, vRot, m_Pos);		
				DrawAxis(1.0f, matLocal);

				const DebugReplayObjectInfo *pObject = GameRecorder::GetAppLevelInstance()->GetObjectInfo( m_rInst );
				if (pObject)
				{
					pObject->DrawBox(matLocal, col);
				}
			}

			void SetData(Vec3V_In linear, Vec3V_In angular)
			{
				m_Linear = linear;
				m_Angular = angular;
			}

			PedDataEvent(const CallstackHelper rCallstack, const phInst *pInst)
				:PhysicsEvent(rCallstack, pInst)
				,m_Linear(V_ZERO)
				,m_Angular(V_ZERO)
			{
				if (pInst && (pInst->GetClassType() == PH_INST_FRAG_PED || pInst->GetClassType() == PH_INST_PED))
				{
					const phCollider *pCollider=PHSIM->GetCollider(pInst);
					if (pCollider)
					{
						m_Linear = pCollider->GetVelocity();
						m_Angular = pCollider->GetAngVelocity();
					}

					QuatV vQ = QuatVFromMat34V(pInst->GetMatrix());
					vQ = Normalize( vQ );
					m_Quat.Set(vQ);

					//CPed *pPed = (CPed*)pInst->GetUserData();
					//if (pPed)
					//{
					//	//Store any ped specific data here!
					//}
				}
			}

			PedDataEvent()
				:m_Linear(V_ZERO)
				,m_Angular(V_ZERO)
			{}

			Vec3V m_Linear;
			Vec3V m_Angular;
			StoreSkeleton::u32Quaternion m_Quat;

			PAR_PARSABLE;
			DR_EVENT_DECL(PedDataEvent)
		};

		void PedCapsuleInfoEvent::DebugDraw3d(eDrawFlags drawFlags) const
		{
			Color32 col(128,0,128,DebugRecorder::GetGlobalAlphaScale());
			if (drawFlags & eDrawSelected)
			{
				col.SetGreen(128);
			}
			if (drawFlags & eDrawHovered)
			{
				col.SetGreen(255);
			}

			grcDebugDraw::Capsule(m_Start, m_End, m_Radius, col, false);
			Vector3 vMin(RCC_VECTOR3(m_Ground));
			Vector3 vMax(vMin);
			vMin.x -= 0.05f;
			vMin.y -= 0.05f;
			vMin.z -= 0.05f;
			vMax.x += 0.05f;
			vMax.y += 0.05f;
			vMax.z += 0.05f;
			grcDrawSolidBox(vMin, vMax, col);
		}

		struct PedGroundInstanceEvent : public debugPlayback::PhysicsEvent
		{
			Mat34V m_GroundMatrix;
			Vec3V m_vExtent;
			float m_fMinSize;
			float m_fSpeedDiff;
			float m_fSpeedAllowance;
			debugPlayback::phInstId m_GroundInstance;
			int m_iPassedAxisCount;
			int m_iComponent;

			virtual bool IsInstInvolved(phInstId rInst) const
			{
				return (m_GroundInstance == rInst) ||  debugPlayback::PhysicsEvent::IsInstInvolved(rInst);
			}

			virtual void DebugDraw3d(eDrawFlags drawFlags) const
			{
				Color32 col(255,0,0,DebugRecorder::GetGlobalAlphaScale());
				if (drawFlags & eDrawSelected)
				{
					col.SetRed(0);
				}
				if (drawFlags & eDrawHovered)
				{
					col.SetGreen(255);
				}

				//Draw the instance
#if __STATS && __PFDRAW
				if (m_GroundInstance.IsValid())
				{
					grcColor(col);
					m_GroundInstance->GetArchetype()->GetBound()->Draw(m_GroundMatrix);
				}
				else
#endif
				{
					grcDrawAxis(0.2f, m_GroundMatrix, true);
				}
			}

			PedGroundInstanceEvent() 
				:m_GroundMatrix(V_IDENTITY)
				,m_vExtent(V_ZERO)
				,m_fMinSize(0.0f)
				,m_fSpeedDiff(0.0f)
				,m_fSpeedAllowance(0.0f)
				,m_iPassedAxisCount(0) 

			{
		
			}

			void SetData(const phInst *pGroundInst, int iComponent, Vec3V_In extent, float fMinSize, int iPassedAxisCount, float fSpeedDiff, float fSpeedAllowance)
			{
				m_GroundInstance.Set(pGroundInst);
				if (pGroundInst)
				{
					RecordInst(m_GroundInstance);
					m_GroundMatrix = pGroundInst->GetMatrix();
				}
				m_vExtent = extent;
				m_fMinSize = fMinSize;
				m_fSpeedDiff = fSpeedDiff;
				m_fSpeedAllowance = fSpeedAllowance;
				Assign(m_iComponent, iComponent);
				Assign(m_iPassedAxisCount, iPassedAxisCount);
			}

			PAR_PARSABLE;
			DR_EVENT_DECL(PedGroundInstanceEvent)
		};

		struct PedSpringEvent : public debugPlayback::PhysicsEvent
		{
			static float sm_fAxisScale;
			Vec3V m_vForce;
			Vec3V m_vGroundNormal;
			Vec3V m_vGroundVelocity;
			float m_fSpringCompression;
			float m_fSpringDamping;
			float m_fMass;

			PedSpringEvent()
				:m_vForce(V_ZERO)
				,m_vGroundNormal(V_ZERO)
				,m_vGroundVelocity(V_ZERO)
				,m_fSpringCompression(0.0f)
				,m_fSpringDamping(0.0f)
				,m_fMass(1.0f)
			{
			}
			void AddEventOptions(const Frame &frame, TextOutputVisitor &rText, bool &bMouseDown) const
			{
				PhysicsEvent::AddEventOptions(frame, rText, bMouseDown);
				if (rText.AddLine("Scale [%f]", sm_fAxisScale) && bMouseDown)
				{
					sm_fAxisScale *= .5f;
					if(sm_fAxisScale<0.005f)
					{
						sm_fAxisScale = 0.1f;
					}
					bMouseDown = false;
				}
			}

			virtual void DebugDraw3d(eDrawFlags drawFlags) const
			{
				Color32 col(255,0,0,DebugRecorder::GetGlobalAlphaScale());
				if (drawFlags & eDrawSelected)
				{
					col.SetRed(0);
				}
				if (drawFlags & eDrawHovered)
				{
					col.SetGreen(255);
				}

				DrawVelocity(m_vForce, m_Pos, ScalarV(sm_fAxisScale / m_fMass), col);

				col.SetBlue(255);
				Vec3V damping(0.0f, 0.0f, m_fSpringDamping);
				DrawVelocity(damping, m_Pos, ScalarV(sm_fAxisScale / m_fMass), col);
			}

			void SetData(Vec3V_In force, Vec3V_In groundNormal, Vec3V_In groundVelocity, float fSpringCompression, float fSpringDamping)
			{
				m_vForce = force;
				m_vGroundNormal = groundNormal;
				m_vGroundVelocity = groundVelocity;
				m_fSpringCompression = fSpringCompression;
				m_fSpringDamping = fSpringDamping;
				if (m_rInst.IsValid() && m_rInst->GetArchetype())
				{
					m_fMass = m_rInst->GetArchetype()->GetMass();
				}
				else
				{
					m_fMass = 1.0f;
				}
			}

			PAR_PARSABLE;
			DR_EVENT_DECL(PedSpringEvent)
		};

		struct QPedSpringEvent : public debugPlayback::PhysicsEvent
		{
			static float sm_fAxisScale;
			Vec3V m_Force;
			Vec3V m_Damping;
			float m_fMaxForce;
			float m_fCompression;
			float m_fMass;

			void AddEventOptions(const Frame &frame, TextOutputVisitor &rText, bool &bMouseDown) const
			{
				PhysicsEvent::AddEventOptions(frame, rText, bMouseDown);
				if (rText.AddLine("Scale [%f]", sm_fAxisScale) && ioMouse::GetDZ())
				{
					const float kScaleProp = 2.0f;
					const float kScaleProp2 = kScaleProp*kScaleProp;
					const float kScaleProp8 = kScaleProp2*kScaleProp2*kScaleProp2*kScaleProp2;
					const float kScaleProp32 = kScaleProp8*kScaleProp8*kScaleProp8*kScaleProp8;
					const float kScalePropMax = kScaleProp32;
					const float kScalePropMin = 1.0f / kScalePropMax;
					sm_fAxisScale *= ioMouse::GetDZ() > 0 ? kScaleProp : 1.0f /kScaleProp;
					sm_fAxisScale = Clamp(sm_fAxisScale, kScalePropMin, kScalePropMax);
					bMouseDown = false;
				}
			}

			virtual void DebugDraw3d(eDrawFlags drawFlags) const
			{
				Color32 col(255,0,0,DebugRecorder::GetGlobalAlphaScale());
				if (drawFlags & eDrawSelected)
				{
					col.SetRed(0);
				}
				if (drawFlags & eDrawHovered)
				{
					col.SetGreen(255);
				}

				DrawVelocity(m_Force, m_Pos, ScalarV(sm_fAxisScale / m_fMass), col);

				col.SetBlue(255);
				DrawVelocity(m_Damping, m_Pos, ScalarV(sm_fAxisScale / m_fMass), col);
			}

			void SetData(Vec3V_In force, Vec3V_In damping, Vec3V_In forcePos, float fMaxForce, float fCompression)
			{
				m_Force = force;
				m_Damping = damping;
				m_Pos = forcePos;
				m_fMaxForce = fMaxForce;
				m_fCompression = fCompression;
				if (m_rInst.IsValid() && m_rInst->GetArchetype())
				{
					m_Pos += m_rInst->GetMatrix().GetCol3();
					m_fMass = m_rInst->GetArchetype()->GetMass();
				}
				else
				{
					m_fMass = 1.0f;
				}
			}

			QPedSpringEvent();

			PAR_PARSABLE;
			DR_EVENT_DECL(QPedSpringEvent)
		};

		QPedSpringEvent::QPedSpringEvent() : m_Force(V_ZERO), m_Damping(V_ZERO), m_fMaxForce(0.0f), m_fCompression(0.0f) {}

		struct EntityAttachmentUpdate : public debugPlayback::PhysicsEvent
		{
			Mat34V m_NewMat;
			debugPlayback::phInstId m_AttachedTo;
			virtual bool IsInstInvolved(phInstId rInst) const
			{
				return (m_AttachedTo == rInst) ||  debugPlayback::PhysicsEvent::IsInstInvolved(rInst);
			}

			virtual void DebugDraw3d(eDrawFlags drawFlags) const
			{
				Color32 col(255,0,0,DebugRecorder::GetGlobalAlphaScale());
				if (drawFlags & eDrawSelected)
				{
					col.SetRed(0);
				}
				if (drawFlags & eDrawHovered)
				{
					col.SetGreen(255);
				}

				//Draw the location
				grcDrawAxis(0.2f, m_NewMat, true);
			}

			EntityAttachmentUpdate()  : m_NewMat(V_IDENTITY) {}

			void SetData(const phInst &rAttachedTo, Mat34V_In matNew)
			{
				m_AttachedTo.Set(&rAttachedTo);
				if (rAttachedTo.IsInLevel())
				{
					if(m_AttachedTo.IsValid())
					{
						RecordInst(m_AttachedTo);
					}
				}
				m_NewMat = matNew;
			}

			PAR_PARSABLE;
			DR_EVENT_DECL(EntityAttachmentUpdate)
		};

		struct ScriptEntityEvent : public debugPlayback::PhysicsEvent
		{
			atHashString m_FuncName;
			int m_iEntityAccessCount;
			ScriptEntityEvent():m_iEntityAccessCount(-1){}
			void SetData(const char *pFuncName, int iFuncEntityCount)
			{
				m_FuncName = pFuncName;
				m_iEntityAccessCount = iFuncEntityCount;
			}
			virtual const char* GetEventSubType() const 
			{
				return m_FuncName.GetCStr(); 
			}
			PAR_PARSABLE;
			DR_EVENT_DECL(ScriptEntityEvent)
		};

		class CStoreTasks : public debugPlayback::PhysicsEvent
		{
		public:
			struct TaskInfo
			{	atHashString m_TaskState;
				u8 m_TaskPriority;
				PAR_SIMPLE_PARSABLE;
			};
		private:
			atArray<TaskInfo> m_TaskList;
			atHashString m_Title;
		public:
			CStoreTasks(const CallstackHelper rCallstack, const phInst *pInst, const fwEntity *pEntity, const char *pString)
				:debugPlayback::PhysicsEvent(rCallstack, pInst)
				,m_Title(pString)
			{
				//Need to roll through any tasks applied to this entity
				const CTaskManager* pTaskManager = NULL;

				switch(pEntity->GetType())
				{
				case ENTITY_TYPE_PED:
					pTaskManager = static_cast<const CPed*>(pEntity)->GetPedIntelligence() ? static_cast<const CPed*>(pEntity)->GetPedIntelligence()->GetTaskManager() : NULL;
					break;
				case ENTITY_TYPE_VEHICLE:
					pTaskManager = static_cast<const CVehicle*>(pEntity)->GetIntelligence() ? static_cast<const CVehicle*>(pEntity)->GetIntelligence()->GetTaskManager() : NULL;
					break;
				case ENTITY_TYPE_OBJECT:
					pTaskManager = static_cast<const CObject*>(pEntity)->GetObjectIntelligence() ? static_cast<const CObject*>(pEntity)->GetObjectIntelligence()->GetTaskManager() : NULL;
					break;
				default:
					break;
				}

				if(pTaskManager)
				{
					int iTotalTaskCount = 0;
					for(s32 i = 0; i < pTaskManager->GetTreeCount() ; i++)
					{
						aiTaskTree* pTree = pTaskManager->GetTree(i);
						if(pTree)
						{
							for(s32 j = 0; j < pTree->GetPriorityCount() ; j++)
							{
								aiTask* pSubTask = pTree->GetTask(j);
								while(pSubTask)
								{
									++iTotalTaskCount;
									pSubTask = pSubTask->GetSubTask();
								}
							}
						}
					}

					m_TaskList.Reset();
					m_TaskList.Resize(iTotalTaskCount);

					int iTaskCount = 0;
					for(s32 i = 0; i < pTaskManager->GetTreeCount() ; i++)
					{
						aiTaskTree* pTree = pTaskManager->GetTree(i);
						if(pTree)
						{
							for(s32 j = 0; j < pTree->GetPriorityCount() ; j++)
							{
								aiTask* pSubTask = pTree->GetTask(j);
								while(pSubTask)
								{
									char buff[256];
									formatf(buff, "%s : %s", pSubTask->GetTaskName(), pSubTask->GetStateName(pSubTask->GetState()));
									m_TaskList[ iTaskCount ].m_TaskState = buff;
									Assign(m_TaskList[ iTaskCount ].m_TaskPriority, j);
									++iTaskCount;
									pSubTask = pSubTask->GetSubTask();
								}
							}
						}
					}
				}
			}

			bool DebugEventText(TextOutputVisitor &rOutput) const
			{
				PrintInstName(rOutput);
				rOutput.AddLine("%s", m_Title.GetCStr());
				rOutput.PushIndent();
				for (int i=m_TaskList.GetCount()-1 ; i>=0 ; i--)
				{
					rOutput.AddLine("%d - %s", m_TaskList[i].m_TaskPriority, m_TaskList[i].m_TaskState.GetCStr());
				}
				rOutput.PopIndent();
				return true;
			}

			const char *GetEventSubType() const 
			{
				return m_Title.GetCStr(); 
			}

			CStoreTasks(){}
			~CStoreTasks(){}
			PAR_PARSABLE;
			DR_EVENT_DECL(CStoreTasks);
		};

		CameraEvent::CameraEvent()
			:m_vCamPos(V_ZERO)
			,m_vCamUp(V_ZERO)
			,m_vCamFront(V_ZERO)
			,m_fFarClip(0.0f)
			,m_fTanFOV(0.0f)
			,m_fTanVFOV(0.0f)
		{
		}

		CameraEvent::CameraEvent(const debugPlayback::CallstackHelper rCallstack)
			:debugPlayback::EventBase(rCallstack)
		{
			//Get current camera information
			const grcViewport &vp = g_SceneToGBufferPhaseNew->GetGrcViewport();
			m_vCamPos = vp.GetCameraPosition();
			m_vCamUp = vp.GetCameraMtx().GetCol1();
			m_vCamFront = vp.GetCameraMtx().GetCol2();
			m_fFarClip = vp.GetFarClip();
			m_fTanFOV = vp.GetTanHFOV();
			m_fTanVFOV = vp.GetTanVFOV();
		}

		void CameraEvent::DebugDraw3d(debugPlayback::eDrawFlags /*drawFlags*/) const
		{
			//Draw frustrum?
			static const float kFrustrumSize = 100.0f;
			static const float fOffsets[4][2] = {
				{-kFrustrumSize,  kFrustrumSize},
				{ kFrustrumSize,  kFrustrumSize},
				{ kFrustrumSize, -kFrustrumSize},
				{-kFrustrumSize, -kFrustrumSize}
			};

			Vec3V vCamLeft = Cross(m_vCamUp, m_vCamFront);
			for (int i=0 ; i<4 ; i++)
			{
				Vec3V vEnd = m_vCamPos;
				vEnd += m_vCamFront * ScalarV(-kFrustrumSize);	//Cam front points backwards?
				vEnd += m_vCamUp * ScalarV(fOffsets[i][0]);
				vEnd += vCamLeft * ScalarV(fOffsets[i][1]);
				grcDrawLine(m_vCamPos, vEnd, Color32(255,255,255));
			}
		}

		bool CameraEvent::Replay(phInst *) const
		{
			camDebugDirector& dbgDirector = camInterface::GetDebugDirector();
			CControlMgr::SetDebugPadOn(true);
			dbgDirector.ActivateFreeCam();
			camFrame& freeCamFrame = dbgDirector.GetFreeCamFrameNonConst();
			freeCamFrame.SetPosition(RCC_VECTOR3(m_vCamPos));
			Vector3 vfront = VEC3V_TO_VECTOR3(-m_vCamFront);
			freeCamFrame.SetWorldMatrixFromFrontAndUp(vfront, RCC_VECTOR3(m_vCamUp));
			freeCamFrame.SetFarClip(m_fFarClip);
			return false;
		}

		float QPedSpringEvent::sm_fAxisScale = 0.1f;
		float PedSpringEvent::sm_fAxisScale = 0.1f;

		DR_EVENT_IMP(QPedSpringEvent);
		DR_EVENT_IMP(CStoreTasks);
		DR_EVENT_IMP(CameraEvent);
		DR_EVENT_IMP(PedSpringEvent);
		DR_EVENT_IMP(PedGroundInstanceEvent);
		DR_EVENT_IMP(PedDesiredVelocityEvent);
		DR_EVENT_IMP(DRNetBlenderStateEvent);
		DR_EVENT_IMP(VehicleDataEvent);
		DR_EVENT_IMP(PedDataEvent);
		DR_EVENT_IMP(PedCapsuleInfoEvent);
		DR_EVENT_IMP(PedAnimatedVelocityEvent);
		DR_EVENT_IMP(VehicleTargetPointEvent);
		DR_EVENT_IMP(VehicleAvoidenceDataEvent);
		DR_EVENT_IMP(EntityAttachmentUpdate);
		DR_EVENT_IMP(ScriptEntityEvent);

		void CurrentInstChange(phInstId rOld, const phInst *pNew)
		{
			if (rOld.IsAssigned())
			{
				//Keep link from the old NewObjectData to the new one that will be formed.
				debugPlayback::DebugRecorder *pInstance = debugPlayback::DebugRecorder::GetInstance();
				if(pInstance)
				{
					debugPlayback::DataBase *pData = pInstance->GetSharedData(rOld.ToU32());
					if (pData)
					{
						DebugReplayObjectInfo *pNewObjectData = dynamic_cast<DebugReplayObjectInfo*>(pData);
						if (pNewObjectData)
						{
							pNewObjectData->SetNextInst(pNew);
						}
					}
				}				
			}
			debugPlayback::InstSwappedOut(rOld, pNew);
		}

		const phInst *GetInstFromCreature(const crCreature *pCreature)
		{
			GameRecorder *pInstance = GameRecorder::GetAppLevelInstance();
			if(pInstance)
			{
				return pInstance->GetInstFromCreature(pCreature);
			}
			return 0;
		}


		void SyncToNet(const msgSyncDRDisplay &rMsg)
		{
			GameRecorder *pInst = GameRecorder::GetAppLevelInstance();
			if (pInst)
			{
				if (rMsg.m_bDisplayOn != pInst->IsDisplayOn())
				{
					pInst->ToggleDisplay();
				}

				//Sync event time line display
				pInst->SetZoomDataZoom(rMsg.m_fZoom);
				pInst->SetZoomDataOffset(rMsg.m_fOffset);

				//TODO (probably worth scrubbing from current location)
				const Frame *pFrame = pInst->GetFirstFramePtr();
				if (!pFrame || (pFrame->m_NetworkTime > rMsg.m_iNetTime))
				{
					return;
				}

				const Frame *pLastFrame = pFrame;
				while (pFrame)
				{
					if (pFrame->m_NetworkTime > rMsg.m_iNetTime)
					{
						if (pLastFrame != pInst->GetSelectedFrame())
						{
							pInst->Select(pLastFrame);
						}
						return;
					}
					pLastFrame = pFrame;
					pFrame = pFrame->mp_Next;
				}
			}
		}

#if __DEV
		void TrackScriptGetsEntityForMod(fwEntity *pEntity)
		{
			if (pEntity && DR_EVENT_ENABLED(ScriptEntityEvent))
			{
				scrThread *pThread = scrThread::GetActiveThread();
				if (pThread && pEntity->GetCurrentPhysicsInst())
				{
					const char *pFuncName = scrThread::GetCurrentCmdName();
					if (pFuncName)
					{
						//TMS:	We can spam this multiple times when calling a single script function so try and filter down to
						//		a single event per entity involved. What would be cool would be to have a single event with
						//		all the entities involved attached but that is a bit complicated to do at the moment.
						static u32 s_CmdCounter=0xffffffff;	
						static u32 s_CmdEntityListCount;
						const int kMaxEntitiesToTrack = 8;
						static fwEntity *s_CmdEntityList[kMaxEntitiesToTrack];
						if (scrThread::sm_DevCurrentCmdNameCounter != s_CmdCounter)
						{
							s_CmdEntityListCount = 1;
							s_CmdCounter = scrThread::sm_DevCurrentCmdNameCounter;	
							s_CmdEntityList[0] = pEntity;

						}
						else
						{
							for (int i=0 ; i<s_CmdEntityListCount ; i++)
							{
								//Did we already track this entity for this call?
								if (s_CmdEntityList[i] == pEntity)
									return;
							}

							if (s_CmdEntityListCount < kMaxEntitiesToTrack)
							{
								s_CmdEntityList[s_CmdEntityListCount] = pEntity;
								++s_CmdEntityListCount;
							}
						}

						debugPlayback::SetUseScriptCallstack(true);

						ScriptEntityEvent *pEvent = AddPhysicsEvent<ScriptEntityEvent>(pEntity->GetCurrentPhysicsInst());
						if (pEvent)
						{
							pEvent->SetData(pFuncName, s_CmdEntityListCount);
						}

						debugPlayback::SetUseScriptCallstack(false);
					}
				}
			}
		}
#endif

		void RecordQuadrupedSpringForce(const phInst *pInst, Vec3V_In pos, Vec3V_In force, Vec3V_In damping, float fMaxForce, float fCompression)
		{
			QPedSpringEvent *pEvent = AddPhysicsEvent<QPedSpringEvent>(pInst);
			if (pEvent)
			{
				pEvent->SetData(force, damping, pos, fMaxForce, fCompression);
			}
		}

		void RecordPedSpringForce(const phInst *pInst, Vec3V_In force, Vec3V_In groundNormal, Vec3V_In groundVelocity, float fSpringCompression, float fSpringDamping)
		{
			PedSpringEvent *pEvent = AddPhysicsEvent<PedSpringEvent>(pInst);
			if (pEvent)
			{
				pEvent->SetData(force, groundNormal, groundVelocity, fSpringCompression, fSpringDamping);
			}
		}

		void RecordPedFlag(const phInst *pInst, const char *pFlagName, bool bFlag)
		{
			SetTaggedFloatEvent *pEvent = AddPhysicsEvent<SetTaggedFloatEvent>(pInst);
			if (pEvent)
			{
				DR_MEMORY_HEAP();
				pEvent->SetData(bFlag ? 1.0f : 0.0f, pFlagName);
			}
		}

		void RecordPedDesiredVelocity(const phInst *pInst, Vec3V_In linear, Vec3V_In angular)
		{
			PedDesiredVelocityEvent *pEvent = AddPhysicsEvent<PedDesiredVelocityEvent>(pInst);
			if (pEvent)
			{
				pEvent->SetData(linear, angular);
			}
		}

		void RecordPedCapsuleInfo(const phInst *pInst, Vec3V_In start, Vec3V_In end, Vec3V_In ground, float radius, Vec3V_In boundOffset)
		{
			PedCapsuleInfoEvent *pEvent = AddPhysicsEvent<PedCapsuleInfoEvent>(pInst);
			if (pEvent)
			{
				pEvent->SetData(start, end, ground, radius, boundOffset);
			}
		}

		void RecordBlenderState(const phInst *pInst, Mat34V_In lastReceivedMatrix, bool positionUpdated, bool orientationUpdated, bool velocityUpdated, bool angVelocityUpdated)
		{
			DRNetBlenderStateEvent *pEvent = AddPhysicsEvent<DRNetBlenderStateEvent>(pInst);
			if (pEvent)
			{
				pEvent->SetData(lastReceivedMatrix, positionUpdated, orientationUpdated, velocityUpdated, angVelocityUpdated);
			}
		}

		void RecordPedAnimatedVelocity(const phInst *pInst, Vec3V_In linear)
		{
			PedAnimatedVelocityEvent *pEvent = AddPhysicsEvent<PedAnimatedVelocityEvent>(pInst);
			if (pEvent)
			{
				pEvent->SetData(linear);
			}
		}

		void RecordPedGroundInstance(const phInst *pInst, const phInst *pGroundInst, int iComponent, Vec3V_In extent, float fMinSize, int iPassedAxisCount, float fSpeedDiff, float fSpeedAllowance)
		{
			//if (pGroundInst)
			{
				PedGroundInstanceEvent *pEvent = AddPhysicsEvent<PedGroundInstanceEvent>(pInst);
				if (pEvent)
				{
					pEvent->SetData(pGroundInst, iComponent, extent, fMinSize, iPassedAxisCount, fSpeedDiff, fSpeedAllowance);
				}
			}
		}

		void RecordEntityAttachment(const phInst *attached, const phInst *attachedTo, const Matrix34 &rNew)
		{
			if (attached && attachedTo)
			{
				EntityAttachmentUpdate *pEvent = AddPhysicsEvent<EntityAttachmentUpdate>(attached);
				if (pEvent)
				{
					pEvent->SetData(*attachedTo, RCC_MAT34V(rNew));
				}
			}
		}

		void RecordVehicleObstructionArray(const phInst *pVehicleInst, const void* pVoid, int iArraySize, float fVehicleDriveOrientation, Vec3V_In vRight, Vec3V_In vVehForwards)
		{
			if (!DR_EVENT_ENABLED(VehicleAvoidenceDataEvent))
			{
				return;
			}

			int iObsCount=0;
			float fMinObstructionRange=FLT_MAX;
			float fMaxObstructionRange=0;
			float fAngleSpread = 0.0f;

			const CTaskVehicleGoToPointWithAvoidanceAutomobile::AvoidanceInfo *AvoidanceInfoArray = static_cast<const CTaskVehicleGoToPointWithAvoidanceAutomobile::AvoidanceInfo*>(pVoid);
			for (int i = 0; i < iArraySize; i++)
			{
				if (AvoidanceInfoArray[i].ObstructionType != CTaskVehicleGoToPointWithAvoidanceAutomobile::AvoidanceInfo::Invalid_Obstruction)
				{
					float fDist = AvoidanceInfoArray[i].fDistToObstruction;
					fMinObstructionRange = Min(fMinObstructionRange, fDist);
					fMaxObstructionRange = Max(fMaxObstructionRange, fDist);
					fAngleSpread = Max(fAngleSpread, Abs(AvoidanceInfoArray[i].fDirection));
					++iObsCount;
				}
			}

			if (iObsCount == 0)
			{
				return;
			}

			VehicleAvoidenceDataEvent *pNewEvent = AddPhysicsEvent<VehicleAvoidenceDataEvent>(pVehicleInst);
			if (pNewEvent)
			{
				DR_MEMORY_HEAP();
				float fCentralDirection = AvoidanceInfoArray[0].fDirection;

				pNewEvent->SetData(fMinObstructionRange, fMaxObstructionRange, fCentralDirection, fAngleSpread, fVehicleDriveOrientation, vRight, vVehForwards);
				pNewEvent->m_Obstructions.Resize(iObsCount);
				iObsCount = 0;
				for (int i = 0; i < iArraySize; i++)
				{
					CTaskVehicleGoToPointWithAvoidanceAutomobile::AvoidanceInfo::eObstructionType eObType = AvoidanceInfoArray[i].ObstructionType;
					if (eObType != CTaskVehicleGoToPointWithAvoidanceAutomobile::AvoidanceInfo::Invalid_Obstruction)
					{					
						float fDirDelta = AvoidanceInfoArray[i].fDirection - fCentralDirection;
						if (fDirDelta > PI)
						{
							fDirDelta -= 2.0f*PI;
						}
						else if (fDirDelta < -PI)
						{
							fDirDelta += 2.0f*PI;
						}
						float fQuantizedDirection = (Clamp(fDirDelta/PI, -1.0f, 1.0f)) * 2047.0f;
						pNewEvent->m_Obstructions[iObsCount].m_Direction = u16(fQuantizedDirection);
						pNewEvent->m_Obstructions[iObsCount].m_Dist = u16((65535.0f * Min(256.0f, AvoidanceInfoArray[i].fDistToObstruction)) / 256.0f);
						pNewEvent->m_Obstructions[iObsCount].m_Type =  eObType;
						Assert(pNewEvent->m_Obstructions[iObsCount].m_Type == eObType);
						++iObsCount;		
					}
				}
			}	
		}

		void SetUseScriptCallstack(bool bUseIt)
		{
			GameRecorder *pGameRecorder = GameRecorder::GetAppLevelInstance();
			if (pGameRecorder)
			{
				pGameRecorder->SetUseScriptCallstack(bUseIt);
			}
		}

		void SetVisualSkeletonIfPossible(fwEntity *pEntity)
		{
			GameRecorder *pInstance = GameRecorder::GetAppLevelInstance();
			if (pInstance)
			{
				pInstance->ReposePedIfPossible(pEntity);
			}
		}

	}
}

debugPlayback::Callstack *spCallStack = 0;
void ScriptCallstackPrintStackTrace(const char *fmt, ...)
{
	if (spCallStack && spCallStack->m_SymbolArray.GetCount() < spCallStack->m_SymbolArray.GetCapacity())
	{
		va_list args;
		va_start(args, fmt);

		char buff[256];
		vformatf(buff, sizeof(buff), fmt, args);
		va_end(args);
		spCallStack->m_SymbolArray.Append() = buff;
	}
}

size_t GameRecorder::RegisterCallstack(int iIgnorelevels)
{
	if (m_bUseScriptCallstack && scrThread::GetActiveThread())
	{
		scrThread *pActiveThread = scrThread::GetActiveThread();

		//Cache strings etc right now

		const int kCallstackDepth = 12;
		int temp[kCallstackDepth];
		for (int i=0 ; i<kCallstackDepth ; i++)
		{
			temp[i] = 0;
		}

		pActiveThread->CaptureStackTrace(temp,kCallstackDepth);
		size_t hash = 0;
		for (int i=0; i<kCallstackDepth; i++)
			hash ^= temp[i];

		//New callstack?
		sysCriticalSection section(m_CallstackCSToken);
		if (!m_Callstacks.Access(hash))
		{
			DR_MEMORY_HEAP();
			debugPlayback::Callstack *pCallstack = rage_new debugPlayback::Callstack;
			m_Callstacks[hash] = pCallstack;

			//Common array's use size_t so copy data across
			size_t temp2[kCallstackDepth];
			for (int i=0 ; i<kCallstackDepth ; i++)
			{
				temp2[i] = temp[i];
			}
			pCallstack->Init(temp2, kCallstackDepth);
			pCallstack->m_iSignature = hash;
			
			//Cache the strings right now while we've got the script program loaded (will only do once per new callstack)
			pCallstack->m_SymbolArray.Reserve(pCallstack->m_AddressArray.GetCount());
			spCallStack = pCallstack;
			pActiveThread->PrintStackTrace(ScriptCallstackPrintStackTrace);
			spCallStack = 0;
			return hash;
		}

		return hash;	
	}

	return DebugRecorder::RegisterCallstack(iIgnorelevels + 1);
}

//Make sure we highlight related events when selecting one
void GameRecorder::OnEventSelection(const debugPlayback::EventBase *pEvent)
{
	if (pEvent && pEvent->IsPhysicsEvent())
	{
		const debugPlayback::PhysicsEvent *pPhEvent = static_cast<const debugPlayback::PhysicsEvent*>(pEvent);
		const DebugReplayObjectInfo *pObject = GetObjectInfo(pPhEvent->m_rInst);
		if (pObject)
		{
			SetDataToHighlight(pObject);
			return;
		}
	}

	//Make sure its clear if we selected an event and it has no data to display
	//HOWEVER don't clear it if we are clearing the selected event as this "loses the trail" when debugging
	if (pEvent)
	{
		SetDataToHighlight(0);
	}
}

void GameRecorder::OnNewDataSelection()
{
	const DebugReplayObjectInfo *pObject = GetObjectInfo(debugPlayback::GetCurrentSelectedEntity());
	if (pObject)
	{
		SetDataToHighlight(pObject);
	}
}

//Moved down here to understand classes declared above
void GameRecorder::PreUpdate()
{
	static int iLastFrameTrapped = 0;
	int iGetCurrentFrame = fwTimer::GetFrameCount();
	m_bStoreDataThisFrame = (!fwTimer::IsGamePaused()) || (iGetCurrentFrame > iLastFrameTrapped);
	iLastFrameTrapped = iGetCurrentFrame;

#if DR_ANIMATIONS_ENABLED
	//Map for "quick" lookups between creature ptrs and insts
	m_CreatureToInstMap.Kill();
	m_CachedCreatureLookupCreature = 0;
	m_CachedCreatureLookupInst = 0;
	if (mvDRTrackInterface::sm_bEnabled)
	{
		for (s32 pedIndex = 0; pedIndex < CPed::GetPool()->GetSize(); ++pedIndex)
		{
			CPed* pPed = CPed::GetPool()->GetSlot(pedIndex);
			if(pPed && pPed->GetCreature() && pPed->GetCurrentPhysicsInst() && debugPlayback::PhysicsEvent::IsSelected(*pPed->GetCurrentPhysicsInst()))
			{
				m_CreatureToInstMap.Insert(pPed->GetCreature() , pPed->GetCurrentPhysicsInst());
			}
		}
	}

	if (DR_EVENT_ENABLED( StoreSkeletonPreUpdate ))
	{
		if (m_bStoreDataThisFrame)
		{
			debugPlayback::DebugRecorder *pInstance = debugPlayback::DebugRecorder::GetInstance();
			if (pInstance)
			{
				debugPlayback::CallstackHelper ch;
				if (pInstance->IsRecording())
				{
					Color32 startFrameColor(255,255,0);

					if (m_bRecordSkeletonsEveryFrame)
					{
						DR_MEMORY_HEAP();
						for (s32 pedIndex = 0; pedIndex < CPed::GetPool()->GetSize(); ++pedIndex)
						{
							CPed* pPed = CPed::GetPool()->GetSlot(pedIndex);
							if(pPed && pPed->GetCurrentPhysicsInst() && debugPlayback::PhysicsEvent::IsSelected(*pPed->GetCurrentPhysicsInst()))
							{
								pInstance->AddEvent(*rage_new StoreSkeletonPreUpdate(ch, pPed->GetCurrentPhysicsInst(), pPed, "Skel:StartOfFrame", startFrameColor));
							}
						}
					}
					else
					{
						DR_MEMORY_HEAP();
						//Record skeletons for all insts with other data recorded
						for (int i=0 ; i<m_LastFrameInsts.GetCount() ; i++)
						{
							if(m_LastFrameInsts[i].IsValid())
							{
								CEntity *pEntity = (CEntity *)m_LastFrameInsts[i]->GetUserData();
								if(pEntity)
								{
									if (DR_EVENT_ENABLED( StoreSkeletonPreUpdate ))
									{
										pInstance->AddEvent(*rage_new StoreSkeletonPreUpdate(ch, m_LastFrameInsts[i], pEntity, "Skel:StartOfFrame", startFrameColor));
									}
								}
							}
						}
					}
				}
			}
		}
	}
#endif
}

void GameRecorder::ReposePedIfPossible(fwEntity *pEntity)
{
	if (m_bPosePedsIfPossible && (!IsRecording() || fwTimer::IsUserPaused()))
	{
		sysCriticalSection critSection(m_DataCSToken);	//Stop data being modified while we're in here, probably won't be anyway
		const debugPlayback::Frame *pCurrentFrame = GetHoveredFrame();
		if (!pCurrentFrame)
		{
			pCurrentFrame = GetSelectedFrame();
		}
		if (pCurrentFrame)
		{
			//Quick roll through entities
			const debugPlayback::EventBase *pEvent = pCurrentFrame->mp_LastEvent;
			while (pEvent)
			{
				if (StoreSkeleton::IsSkeletonEvent(pEvent))
				{
					phInst *pInst = ((StoreSkeleton*)pEvent)->m_rInst;
					if (pInst && pInst->GetUserData() == (void*)pEntity)
					{
						pEvent->Replay(pEntity->GetCurrentPhysicsInst());
						return;
					}
				}
				pEvent = pEvent->mp_Prev;
			}

			//Slower, check history to allow for inst changes (mover -> ragdoll -> mover etc)
			pEvent = pCurrentFrame->mp_LastEvent;
			while (pEvent)
			{
				if (StoreSkeleton::IsSkeletonEvent(pEvent))
				{
					debugPlayback::phInstId rCurrentInst(pEntity->GetCurrentPhysicsInst());
					if (DoInstsShareAnEntity(rCurrentInst, ((StoreSkeleton*)pEvent)->m_rInst))
					{
						pEvent->Replay(pEntity->GetCurrentPhysicsInst());
						return;
					}
				}
				pEvent = pEvent->mp_Prev;
			}
			
		}	
	}
}

void GameRecorder::Update()
{
	if (m_bSyncNetDisplays)
	{
		msgSyncDRDisplay tmpMsg(m_iDrawnFrameNetTime, IsDisplayOn(), GetZoomDataZoom(), GetZoomDataOffset());
		if (tmpMsg != m_LastSyncMsg)
		{
			NetworkDebug::SyncDRFrame(tmpMsg);
			m_LastSyncMsg = tmpMsg;
		}
	}

	//High level gates to minimize performance impact
	UpdateAnimEventRecordingState();
	CPhysical::smb_RecordAttachments = DR_EVENT_ENABLED(debugPlayback::EntityAttachmentUpdate);
	debugPlayback::UpdateTaskTransitionHighLevelGate();

	if (sbTrackPlayer)
	{
		//Make sure the selected inst is the players current inst (note we might
		//lose a few events on the switch, would need to hook into the switch code itself
		//to fix that).
		CPed* pPlayer = CGameWorld::FindLocalPlayer();
		if (pPlayer)
		{
			phInst *pCurrentInst = pPlayer->GetCurrentPhysicsInst();
			debugPlayback::SetCurrentSelectedEntity(pCurrentInst, true);
		}
	}

	//Blast position and range through
	if (debugPlayback::GetTrackMode() == debugPlayback::eTrackInRange)
	{
		CPed* pPlayer = CGameWorld::FindLocalPlayer();
		if (pPlayer)
		{
			ScalarV vRange(sfTrackRange);
			Vec3V vPos(pPlayer->GetTransform().GetPosition());
			debugPlayback::SetTrackDistToRecord(vPos, vRange);
		}
	}

	if (m_bStoreDataThisFrame)
	{
#if DR_ANIMATIONS_ENABLED
		if (DR_EVENT_ENABLED( StoreSkeletonPostUpdate ) || DR_EVENT_ENABLED( debugPlayback::CStoreTasks ))
		{
			debugPlayback::DebugRecorder *pInstance = debugPlayback::DebugRecorder::GetInstance();
			if (pInstance)
			{
				debugPlayback::CallstackHelper ch;
				if (pInstance->IsRecording())
				{
					DR_MEMORY_HEAP();
					Color32 startFrameColor(0,255,255);

					//Record skeletons for all insts recorded
					for (int i=0 ; i<m_FrameInsts.GetCount() ; i++)
					{
						if(m_FrameInsts[i].IsValid())
						{
							CEntity *pEntity = (CEntity *)m_FrameInsts[i]->GetUserData();
							if(pEntity)
							{
								if (DR_EVENT_ENABLED( StoreSkeletonPostUpdate ) && !m_bRecordSkeletonsEveryFrame)
								{
									pInstance->AddEvent(*rage_new StoreSkeletonPostUpdate(ch, m_FrameInsts[i], pEntity, "Skel:EndOfFrame", startFrameColor));
								}
								if (DR_EVENT_ENABLED( debugPlayback::CStoreTasks ))
								{
									pInstance->AddEvent(*rage_new debugPlayback::CStoreTasks(ch, m_FrameInsts[i], pEntity, "Tasks:EndOfFrame"));
								}
							}
						}
					}

					//Record every frame ped information
					if (DR_EVENT_ENABLED(StoreSkeletonPostUpdate) && m_bRecordSkeletonsEveryFrame)
					{
						debugPlayback::DebugRecorder *pInstance = debugPlayback::DebugRecorder::GetInstance();
						if (pInstance)
						{
							for (s32 pedIndex = 0; pedIndex < CPed::GetPool()->GetSize(); ++pedIndex)
							{
								CPed* pPed = CPed::GetPool()->GetSlot(pedIndex);
								if(pPed && pPed->GetCurrentPhysicsInst() && debugPlayback::PhysicsEvent::IsSelected(*pPed->GetCurrentPhysicsInst()))
								{
									pInstance->AddEvent(*rage_new StoreSkeletonPostUpdate(ch, pPed->GetCurrentPhysicsInst(), pPed, "Skel:EndOfFrame", startFrameColor));
								}
							}
						}
					}
				}
			}
		}
#endif //DR_ANIMATIONS_ENABLED

		//Record every frame vehicle information
		if (DR_EVENT_ENABLED(debugPlayback::VehicleDataEvent) || DR_EVENT_ENABLED(debugPlayback::VehicleTargetPointEvent))
		{
			debugPlayback::DebugRecorder *pInstance = debugPlayback::DebugRecorder::GetInstance();
			if (pInstance)
			{
				debugPlayback::CallstackHelper ch;
				if (pInstance->IsRecording())
				{
					DR_MEMORY_HEAP();
					for (s32 vehIndex = 0; vehIndex < CVehicle::GetPool()->GetSize(); ++vehIndex)
					{
						CVehicle* pVehicle = CVehicle::GetPool()->GetSlot(vehIndex);
						if(pVehicle && pVehicle->GetCurrentPhysicsInst() && debugPlayback::PhysicsEvent::IsSelected(*pVehicle->GetCurrentPhysicsInst()))
						{
							if (DR_EVENT_ENABLED(debugPlayback::VehicleDataEvent))
							{
								pInstance->AddEvent(*rage_new debugPlayback::VehicleDataEvent(ch, pVehicle->GetCurrentPhysicsInst()));
							}

							if (DR_EVENT_ENABLED(debugPlayback::VehicleTargetPointEvent))
							{
								CTask* pActiveTask = pVehicle->GetIntelligence()->GetActiveTask();
								while(pActiveTask)
								{
									if(pActiveTask->IsVehicleMissionTask())
									{
										Vector3 vCoors;
										static_cast<CTaskVehicleMissionBase *>(pActiveTask)->FindTargetCoors(pVehicle, vCoors);
										if (vCoors.Mag2() > SMALL_FLOAT)
										{
											debugPlayback::VehicleTargetPointEvent *pTPE = rage_new debugPlayback::VehicleTargetPointEvent(ch, pVehicle->GetCurrentPhysicsInst());
											char eventName[128];
											formatf(eventName, "%s : TARGET", pActiveTask->GetName().c_str());
											pTPE->SetData(eventName, RCC_VEC3V(vCoors));
											pInstance->AddEvent(*pTPE);									
										}
									}
									pActiveTask = pActiveTask->GetSubTask();
								}
							}
						}
					}
				}
			}
		}

		//Record every frame ped information
		if (DR_EVENT_ENABLED(debugPlayback::PedDataEvent))
		{
			debugPlayback::DebugRecorder *pInstance = debugPlayback::DebugRecorder::GetInstance();
			if (pInstance)
			{
				debugPlayback::CallstackHelper ch;
				if (pInstance->IsRecording())
				{
					DR_MEMORY_HEAP();
					for (s32 pedIndex = 0; pedIndex < CPed::GetPool()->GetSize(); ++pedIndex)
					{
						CPed* pPed = CPed::GetPool()->GetSlot(pedIndex);
						if(pPed && pPed->GetCurrentPhysicsInst() && debugPlayback::PhysicsEvent::IsSelected(*pPed->GetCurrentPhysicsInst()))
						{
							pInstance->AddEvent(*rage_new debugPlayback::PedDataEvent(ch, pPed->GetCurrentPhysicsInst()));
						}
					}
				}
			}
		}


		if (DR_EVENT_ENABLED( AnimMotionTreeTracker ))
		{
			debugPlayback::DebugRecorder *pInstance = debugPlayback::DebugRecorder::GetInstance();
			if (pInstance)
			{
				debugPlayback::CallstackHelper ch;
				if (pInstance->IsRecording())
				{
					DR_MEMORY_HEAP();
					for (s32 pedIndex = 0; pedIndex < CPed::GetPool()->GetSize(); ++pedIndex)
					{
						CPed* pPed = CPed::GetPool()->GetSlot(pedIndex);
						if(pPed && pPed->GetCurrentPhysicsInst() && debugPlayback::PhysicsEvent::IsSelected(*pPed->GetCurrentPhysicsInst()))
						{
							pInstance->AddEvent(*rage_new AnimMotionTreeTracker(ch, pPed->GetCurrentPhysicsInst(), "MotionTree:EndOfFrame"));
						}
					}
				}
			}
		}

		if (DR_EVENT_ENABLED(debugPlayback::CameraEvent))
		{
			debugPlayback::DebugRecorder *pInstance = debugPlayback::DebugRecorder::GetInstance();
			if (pInstance)
			{
				debugPlayback::CallstackHelper ch;
				if (pInstance->IsRecording())
				{
					DR_MEMORY_HEAP();
					pInstance->AddEvent(*rage_new debugPlayback::CameraEvent(ch));
				}
			}
		}
	}
	else if (GetFirstFramePtr())
	{
		//Not recording data try and set the camera up?
		if (m_bReplayCamera)
		{
			const debugPlayback::Frame *pCurrentFrame = GetHoveredFrame();
			if (!pCurrentFrame)
			{
				pCurrentFrame = GetSelectedFrame();
			}
			if (pCurrentFrame)
			{
				const debugPlayback::EventBase *pEvent = pCurrentFrame->mp_LastEvent;
				while (pEvent)
				{
					if (pEvent->GetEventType() == debugPlayback::CameraEvent::GetStaticEventType())
					{
						pEvent->Replay(0);
						break;
					}
					pEvent = pEvent->mp_Prev;
				}
			}
		}
	}

	m_LastFrameInsts.SetCount(m_FrameInsts.GetCount());
	for (int i=0 ; i<m_FrameInsts.GetCount() ; i++)
	{
		m_LastFrameInsts[i] = m_FrameInsts[i];
	}
	m_FrameInsts.Reset();

	//Make sure our inst change histories are connected up if needed (needed when we add backwards links to objects, )
	DebugReplayObjectInfo::UpdateChains();

	if (!mp_ReplayFrame)
	{
		if (m_bPauseOnEventReplay)
		{
			//Pause the game update as we've now updated physics since a reset
			fwTimer::StartUserPause();
			m_bPauseOnEventReplay = false;
		}

		if (m_bDestroyReplayObject)
		{
			DebugReplayObjectInfo::ClearReplayObject();
			m_bDestroyReplayObject = false;
		}
		return;
	}

	//Build a list of the previous unique physics events
	debugPlayback::phInstId rPlaybackId;
	Mat34V initialMatrix;	//TODO: Recover the initial matrix
	bool bHaveMatrix = false;
	debugPlayback::TTYTextOutputVisitor tty;
	debugPlayback::EventBase *pEvent = mp_ReplayFrame->mp_LastEvent;
	while (pEvent)
	{
		//Is the event a physics event?
		if (pEvent->IsPhysicsEvent())
		{
			const debugPlayback::PhysicsEvent *pPhEvent = static_cast<const debugPlayback::PhysicsEvent*>(pEvent);
			if (!rPlaybackId.IsAssigned())
			{
				const DebugReplayObjectInfo *pObject = GetObjectInfo(pPhEvent->m_rInst);
				if (pObject)
				{
					rPlaybackId = pPhEvent->m_rInst;
					tty.AddLine("------------------------");
					tty.AddLine("Object for replay from event %d = %s", pPhEvent->m_iEventIndex, pObject->GetModelName());
				}
			}

			//Create the object at the right location if possible!
			const debugPlayback::SetMatrixEvent *pMatEvent = dynamic_cast<const debugPlayback::SetMatrixEvent*>(pPhEvent);
			if (pMatEvent)
			{
				initialMatrix = pMatEvent->m_Matrix;
				bHaveMatrix = true;
			}
		}
		pEvent = pEvent->mp_Prev;
	}

	if (!bHaveMatrix)
	{
		Errorf("Failed to find matrix to hit replay at");
		mp_ReplayFrame = 0;
		return;
	}

	//Teleport to the initial matrix!
	if (m_bHasTeleportLocation)
	{
		CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
		if (pPlayerPed)
		{
			pPlayerPed->Teleport( RCC_VECTOR3(m_vTeleportLocation), pPlayerPed->GetCurrentHeading() );

			// Load all objects
			CStreaming::LoadAllRequestedObjects( );	
		}
	}

	//Only allow one replay object at a time!
	DebugReplayObjectInfo::ClearReplayObject();

	//Create the replay object if needed.
	phInst *pReplayOnThis = m_bCreateObjectForReplay ? 0 : debugPlayback::PhysicsEvent::GetSelectedInst();
	if (m_bCreateObjectForReplay && rPlaybackId.IsAssigned())
	{
		debugPlayback::DebugRecorder *pInstance = debugPlayback::DebugRecorder::GetInstance();
		if(pInstance)
		{
			debugPlayback::DataBase *pData = pInstance->GetSharedData(rPlaybackId.ToU32());
			if (pData)
			{
				DebugReplayObjectInfo *pNewObjectData = dynamic_cast<DebugReplayObjectInfo*>(pData);
				if (pNewObjectData)
				{
					tty.AddLine("------------------------");
					tty.PushIndent();
					pNewObjectData->DebugText(tty);
					tty.PopIndent();
					tty.AddLine("------------------------");
					pReplayOnThis = pNewObjectData->CreateObject(tty, initialMatrix, m_bInvolvePlayer);
					tty.AddLine("------------------------");
					tty.AddLine("Created replay object 0x%x", pReplayOnThis);

					if(!pReplayOnThis)
					{
						tty.AddLine("------------------------");
					}
				}
			}
		}
	}

	//Play them back as commands, print useful info as we go.
	//Play the events of the replay frame in order
	if(pReplayOnThis)
	{
		tty.AddLine("------------------------");
		tty.AddLine("		Event replay");
		tty.AddLine("------------------------");
		tty.PushIndent();
		debugPlayback::EventBase *pEvent = mp_ReplayFrame->mp_LastEvent;
		while (pEvent)
		{
			if (pEvent->Replay(pReplayOnThis))
			{
				tty.AddLine("Replay from %s", pEvent->GetEventType());
				pEvent->DebugText(tty);
			}
			pEvent = pEvent->mp_Next;
		}
		tty.PopIndent();
		tty.AddLine("------------------------");
	}
	else
	{
		Errorf("Failed to get an object to run a replay with!");
	}

	//Thoughts for trapping a specific event on a frame... might work, also BreakOnEvent might just work
	//- Tag the event type we want to trap, and the count into the frame
	//- Trap the event when it happens...
	//- Clear the replay settings
	mp_ReplayFrame = 0;
}

#if __WIN32
#pragma warning(pop)	// nonstandard extension used : nameless struct/union
#endif

//PARSER
using namespace rage;
using namespace debugPlayback;
#include "debugRecorder_parser.h"

#endif
