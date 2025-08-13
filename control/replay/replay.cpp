#include "replay.h"

#if GTA_REPLAY

#include "audio/speechmanager.h"
#include "audio/scriptaudioentity.h"
#include "peds/ped.h"

#include "ReplayInterfaceObj.h"
#include "ReplayInterfaceVeh.h"
#include "ReplayAttachmentManager.h"
#include "Game/ReplayInterfaceGame.h"
#include "audio/SoundPacket.h"

#include "Effects/ParticleMiscFxPacket.h"

extern audSpeechManager	g_SpeechManager;
extern audScriptAudioEntity g_ScriptAudioEntity;
extern const u8			g_MaxAmbientSpeechSlots;

//////////////////////////////////////////////////////////////////////////
void CReplayMgr::InitSession(unsigned initMode)
{	
	CReplayControl::InitSession(initMode);

	CReplayMgrInternal::InitSession(initMode);

	ReplayBufferMarkerMgr::InitSession(initMode);
}

//////////////////////////////////////////////////////////////////////////
void CReplayMgr::ShutdownSession(unsigned shutdownMode)
{	
	CReplayMgrInternal::ShutdownSession(shutdownMode);

	ReplayBufferMarkerMgr::ShutdownSession(shutdownMode);

	CReplayControl::ShutdownSession(shutdownMode);
}

void CReplayMgr::InitWidgets()
{	
	CReplayMgrInternal::InitWidgets();

	CReplayControl::InitWidgets();

	ReplayBufferMarkerMgr::InitWidgets();
}


//////////////////////////////////////////////////////////////////////////
void CReplayMgr::ProcessRecord(u32 replayTimeInMilliseconds)		
{	
	CReplayInterfaceGame *pGameInterface = GetGameInterface();

	if(pGameInterface)
		pGameInterface->SetReplayTimeInMillisecondsForNextFrameToRecord(replayTimeInMilliseconds);

	CReplayMgrInternal::ProcessRecord();					
}


//////////////////////////////////////////////////////////////////////////
void CReplayMgr::ClearSpeechSlots()
{
	for( int i = 0; i < g_MaxAmbientSpeechSlots; ++i )
	{
		if( g_SpeechManager.m_AmbientSpeechSlots[i].SpeechAudioEntity != NULL )
		{
			g_SpeechManager.m_AmbientSpeechSlots[i].SpeechAudioEntity->StopAmbientSpeech();
		}

		g_ScriptAudioEntity.AbortScriptedConversation(false BANK_ONLY(,"replay cleaning speech slots"));
	}
}


//////////////////////////////////////////////////////////////////////////

void CReplayMgr::ReplayRecordSound(const u32 soundHash, const audSoundInitParams *initParams, const CEntity* pAudEntity, const CEntity* pTracker, eAudioSoundEntityType entityType, const u32 slowMoSoundHash)
{
	if(CReplayMgr::ShouldRecord())
	{
		REPLAY_CHECK(entityType != eNoGlobalSoundEntity || pAudEntity != NULL, NO_RETURN_VALUE, "ReplayRecordSound should have either an entity or entityType to replay the sound");

		if(initParams->Variables[0].nameHash != 0)
			CReplayMgr::RecordFx<CPacketSoundCreateMiscWithVars>(CPacketSoundCreateMiscWithVars(soundHash, initParams, entityType, slowMoSoundHash), pTracker, pAudEntity);
		else
			CReplayMgr::RecordFx<CPacketSoundCreateMisc>(CPacketSoundCreateMisc(soundHash, initParams, entityType, slowMoSoundHash), pTracker, pAudEntity);
	}
}


//////////////////////////////////////////////////////////////////////////

void CReplayMgr::ReplayRecordSound(const u32 soundSetHash, const u32 soundHash, const audSoundInitParams *initParams, const CEntity* pAudEntity, const CEntity* pTracker, eAudioSoundEntityType entityType, const u32 slowMoSoundHash)
{
	if(CReplayMgr::ShouldRecord())
	{
		REPLAY_CHECK(entityType != eNoGlobalSoundEntity || pAudEntity != NULL, NO_RETURN_VALUE, "ReplayRecordSound should have either an entity or entityType to replay the sound");

		if(initParams && initParams->Variables[0].nameHash != 0)
			CReplayMgr::RecordFx<CPacketSoundCreateMiscWithVars>(CPacketSoundCreateMiscWithVars(soundSetHash, soundHash, initParams, entityType, slowMoSoundHash), pTracker, pAudEntity);
		else
			CReplayMgr::RecordFx<CPacketSoundCreateMisc>(CPacketSoundCreateMisc(soundSetHash, soundHash, initParams, entityType, slowMoSoundHash), pTracker, pAudEntity);
	}
}

//////////////////////////////////////////////////////////////////////////
void CReplayMgr::ReplayRecordSoundPersistant(const u32 soundHash, const audSoundInitParams *initParams, audSound *trackingSound, const CEntity* pAudEntity, eAudioSoundEntityType entityType, ReplaySound::eContext context, bool isUpdate)
{
	if(CReplayMgr::ShouldRecord() && trackingSound)
	{
		REPLAY_CHECK(entityType != eNoGlobalSoundEntity || pAudEntity != NULL, NO_RETURN_VALUE, "ReplayRecordSoundPersistant should have either an entity or entityType to replay the sound");
		REPLAY_CHECK(!initParams || initParams->EnvironmentGroup == NULL || pAudEntity != NULL || entityType == eScriptSoundEntity || context !=  ReplaySound::CONTEXT_INVALID, NO_RETURN_VALUE, "ReplayRecordSoundPersistant needs an pAudEntity or eScriptSoundEntity if it has an environement group");
		REPLAY_CHECK(!initParams || initParams->WaveSlot == NULL || context != ReplaySound::CONTEXT_INVALID, NO_RETURN_VALUE, "ReplayRecordSoundPersistant has a WaveSlot but doesn't have a context");

		u32 playTime = trackingSound->GetCurrentPlayTime(g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0));
		if(!isUpdate)
		{
			playTime = 0;
		}
		CReplayMgr::RecordPersistantFx<CPacketSoundUpdatePersistant>(
			CPacketSoundUpdatePersistant(soundHash, initParams, true, entityType, context, playTime),
			CTrackedEventInfo<tTrackedSoundType>(trackingSound), 
			pAudEntity, true);
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayMgr::ReplayRecordSoundPersistant(const u32 soundSetHash, const u32 soundHash, const audSoundInitParams *initParams, audSound *trackingSound, const CEntity* pAudEntity, eAudioSoundEntityType entityType, ReplaySound::eContext context, bool isUpdate)
{
	if(CReplayMgr::ShouldRecord() && trackingSound)
	{
		REPLAY_CHECK(entityType != eNoGlobalSoundEntity || pAudEntity != NULL, NO_RETURN_VALUE, "ReplayRecordSoundPersistant should have either an entity or entityType to replay the sound");
		REPLAY_CHECK(!initParams || initParams->EnvironmentGroup == NULL || pAudEntity != NULL || entityType == eScriptSoundEntity || context != ReplaySound::CONTEXT_INVALID, NO_RETURN_VALUE, "ReplayRecordSoundPersistant needs an pAudEntity or eScriptSoundEntity if it has an environement group");
		REPLAY_CHECK(!initParams || initParams->WaveSlot == NULL || context != ReplaySound::CONTEXT_INVALID, NO_RETURN_VALUE, "ReplayRecordSoundPersistant has a WaveSlot but doesn't have a context");

		u32 playTime = trackingSound->GetCurrentPlayTime(g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0));
		if(!isUpdate)
		{
			playTime = 0;
		}
		CReplayMgr::RecordPersistantFx<CPacketSoundUpdatePersistant>(
			CPacketSoundUpdatePersistant(soundSetHash, soundHash, initParams, true, entityType, context, playTime),
			CTrackedEventInfo<tTrackedSoundType>(trackingSound), 
			pAudEntity, true);
	}
}

void CReplayMgr::ReplayPresuckSound(const u32 presuckSoundHash, const u32 presuckPreemptTime, const u32 dynamicSceneHash, const CEntity* pAudEntity)
{
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordFx<CPacketPresuckSound>(
			CPacketPresuckSound(presuckSoundHash, presuckPreemptTime, dynamicSceneHash),pAudEntity);
	}
}

void CReplayMgr::ReplayRecordPlayStreamFromEntity(CEntity* pEntity, audSound *trackingSound, const char* currentStreamName, const u32 currentStartOffset, const char* currentSetName)
{
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordPersistantFx<CPacketPlayStreamFromEntity>(
			CPacketPlayStreamFromEntity(currentStreamName, currentStartOffset, currentSetName),
			CTrackedEventInfo<tTrackedSoundType>(trackingSound), 
			pEntity, true);
	}
}

void CReplayMgr::ReplayRecordPlayStreamFromPosition(const Vector3 &pos, audSound *trackingSound, const char* currentStreamName, const u32 currentStartOffset, const char* currentSetName)
{
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordPersistantFx<CPacketPlayStreamFromPosition>(
			CPacketPlayStreamFromPosition(pos, currentStreamName, currentStartOffset, currentSetName),
			CTrackedEventInfo<tTrackedSoundType>(trackingSound), 
			NULL, true);
	}
}

void CReplayMgr::ReplayRecordPlayStreamFrontend(audSound *trackingSound, const char* currentStreamName, const u32 currentStartOffset, const char* currentSetName)
{
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordPersistantFx<CPacketPlayStreamFrontend>(
			CPacketPlayStreamFrontend(currentStreamName, currentStartOffset, currentSetName),
			CTrackedEventInfo<tTrackedSoundType>(trackingSound), 
			NULL, true);
	}
}

void CReplayMgr::RecordStopStream()
{
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordFx<CPacketStopStream>(CPacketStopStream());
	}
}


void CReplayMgr::RecordEntityAttachment(const CReplayID& child, const CReplayID& parent)
{
	if(CReplayMgr::IsRecording())
		ReplayAttachmentManager::Get().AddAttachment(child, parent);
}


void CReplayMgr::RecordEntityDetachment(const CReplayID& child, const CReplayID& parent)
{
	if(CReplayMgr::IsRecording())
		ReplayAttachmentManager::Get().AddDetachment(child, parent);
}


//////////////////////////////////////////////////////////////////////////
void CReplayMgr::Process()
{	
	ReplayBufferMarkerMgr::Process();

	CReplayControl::Process();

	CReplayMgrInternal::Process();
}



void CReplayMgr::JumpToFloat(float timeToJumpToMS, u32 jumpOptions)
{	
	//Stop any existing precaching
	CReplayMgrInternal::StopPreCaching();
	
	//Start a new precaching after jump has been processed
	CReplayMgrInternal::KickPrecaching(true);	

	CReplayMgrInternal::JumpTo(timeToJumpToMS, jumpOptions);
}

//////////////////////////////////////////////////////////////////////////
void CReplayMgr::SetTrafficLightCommands(CEntity* pEntity, const char* commands)
{
	if(pEntity->GetIsTypeDummyObject())
		return; // Bail on dummy's atm as they won't have a replay id

	RecordMapObject((CObject*)pEntity);

	if(((CObject*)pEntity)->GetReplayID() == ReplayIDInvalid)
		return;

	bool newlyAdded = false;
	if(ReplayTrafficLightExtension::HasExtension(pEntity) == false)
	{
		ReplayTrafficLightExtension::Add(pEntity);
		newlyAdded = true;
	}

	ReplayTrafficLightExtension* pExt = ReplayTrafficLightExtension::GetExtension(pEntity);

	bool commandsChanged = pExt->HaveTrafficLightCommandsChanged(commands);
	if(newlyAdded == false || commandsChanged == true)
	{
		// Record event here
		CReplayMgr::RecordFx<CPacketTrafficLight>(
			CPacketTrafficLight(pExt->GetTrafficLightCommands(), commands), 
			pEntity);
	}

	pExt->SetTrafficLightCommands(commands);
}

//////////////////////////////////////////////////////////////////////////
bool CReplayMgr::ShouldIncreaseFidelity()
{
#if RSG_PC
	if(IsRecordingEnabled())
	{
		return true;
	}
#endif

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CReplayMgr::FakeSnipingMode()
{
	CPed *pPlayerPed = CGameWorld::FindLocalPlayer();
	return pPlayerPed && ReplayReticuleExtension::GetIsFirstPerson(pPlayerPed);
}


//////////////////////////////////////////////////////////////////////////
bool CReplayMgr::ShouldPreventMapChangeExecution(const CEntity* pEntity)
{
	return pEntity->GetOwnedBy() == ENTITY_OWNEDBY_REPLAY;
}


//////////////////////////////////////////////////////////////////////////
const char* CReplayMgr::GetTrafficLightCommands(CEntity* pEntity)
{
	if(ReplayTrafficLightExtension::HasExtension(pEntity) == true)
	{
		ReplayTrafficLightExtension* pExt = ReplayTrafficLightExtension::GetExtension(pEntity);
		return pExt->GetTrafficLightCommands();
	}

	return NULL;
}


//////////////////////////////////////////////////////////////////////////
void CReplayMgr::SetVehicleVariationData(CVehicle* pVehicle)
{
	if(!IsReplayInControlOfWorld() && GetVehicleInterface())
		GetVehicleInterface()->SetVehicleVariationData(pVehicle);
}


//////////////////////////////////////////////////////////////////////////
void CReplayMgr::RecordMapObject(CObject* pObject)
{
	if(!IsReplayInControlOfWorld() && GetObjectInterface())
		GetObjectInterface()->RecordMapObject(pObject);
}


//////////////////////////////////////////////////////////////////////////
void CReplayMgr::StopRecordingMapObject(CObject* pObject)
{
	if(!IsReplayInControlOfWorld() && GetObjectInterface())
		GetObjectInterface()->StopRecordingMapObject(pObject);
}


//////////////////////////////////////////////////////////////////////////
bool CReplayMgr::IsRecordingMapObject(CObject* pObject)
{
	if(!IsReplayInControlOfWorld() && GetObjectInterface())
		return GetObjectInterface()->IsRecordingMapObject(pObject);
	else
		return false;
}


//////////////////////////////////////////////////////////////////////////
void CReplayMgr::RecordObject(CObject* pObject)
{
	if(!IsReplayInControlOfWorld() && GetObjectInterface())
		GetObjectInterface()->RecordObject(pObject);
}


//////////////////////////////////////////////////////////////////////////
void CReplayMgr::StopRecordingObject(CObject* pObject)
{
	if(!IsReplayInControlOfWorld() && GetObjectInterface())
		GetObjectInterface()->StopRecordingObject(pObject);
}


//////////////////////////////////////////////////////////////////////////
bool CReplayMgr::ShouldHideMapObject(CObject* pObject)
{
	if(IsEditModeActive() && GetObjectInterface())
		return GetObjectInterface()->IsRecordingMapObject(pObject);
	else
		return false;
}


//////////////////////////////////////////////////////////////////////////
bool CReplayMgr::DisallowObjectPhysics(CObject* pObject)
{
	return IsEditModeActive() && ShouldHideMapObject(pObject) && !GetObjectInterface()->AllowObjectPhysicsOverride();
}

bool CReplayMgr::ShouldProcessControl(CEntity* pEntity)
{
	if(!CReplayMgr::IsEditModeActive() || !pEntity->GetUsedInReplay() )
	{
		return true;
	}

	if(pEntity->GetIsTypePed() || pEntity->GetIsTypeVehicle() || pEntity->GetIsTypeAnimatedBuilding())
	{
		return true;
	}

	if(pEntity->GetIsTypeObject() && pEntity->GetAnimDirector() && pEntity->GetBaseModelInfo()->GetAutoStartAnim())
	{
		return true;
	}

	return false;
}

bool CReplayMgr::ShouldRecordWarpFlag(CEntity* pEntity)
{
	if(pEntity && pEntity->GetIsTypeObject())
	{
		if(((CObject*)pEntity)->IsADoor())
		{
			return false;
		}
		
		if(pEntity->GetModelNameHash() == 0x5774cdad/*"gr_prop_inttruck_door_01"*/)
		{
			// Don't warp the sliding door on replay playback (url:bugstar:3596911)
			// Unfortunately this isn't an actual door!!?
			return false;
		}
	}

	return true;
}

#if __BANK
//////////////////////////////////////////////////////////////////////////
bool CReplayMgr::IsMapObjectExpectedToBeHidden(u32 hash, CReplayID& replayID, FrameRef& frameRef)
{
	return GetObjectInterface()->IsMapObjectExpectedToBeHidden(hash, replayID, frameRef);
}
#endif // __BANK


//////////////////////////////////////////////////////////////////////////
void CReplayMgr::RecordRoomRequest(u32 interiorProxyNameHash, u32 posHash, u32 roomIndex)
{
	if(CReplayMgr::IsRecording())
	{
		GetGameInterface()->GetInteriorManager()->RecordRoomRequest(interiorProxyNameHash, posHash, roomIndex);
	}
}


bool CReplayMgr::NotifyInteriorToExteriorEviction(CObject *pObject, fwInteriorLocation interiorLocation)
{
	if(IsEditModeActive())
	{
		if(pObject->GetIsTypeObject() && (pObject->IsPickup() == false))
		{
			GetObjectInterface()->AddMoveToInteriorLocation(pObject, interiorLocation);
			return true;
		}
		else
			// TODO:- Pick-ups!
			replayWarningf("CReplayMgr::NotifyInteriorToExteriorEviction()...Unsupported interior to exterior eviction!");
	}
	return false;
}



//////////////////////////////////////////////////////////////////////////
void CReplayMgr::RecordDestroyedMapObject(CObject *pObject)
{
	CReplayInterfaceObject *pObjectInterface = GetObjectInterface();

	if(pObjectInterface)
		pObjectInterface->GetDestroyedMapObjectsManager()->MakeRecordOfDestruction(pObject);
}

void CReplayMgr::RecordRemadeMapObject(CObject *pObject)
{
	CReplayInterfaceObject *pObjectInterface = GetObjectInterface();

	if(pObjectInterface)
		pObjectInterface->GetDestroyedMapObjectsManager()->DeleteRecordOfDestruction(pObject);
}



//////////////////////////////////////////////////////////////////////////
void CReplayMgr::ExtractCachedWaterFrame(float *pDest, float *pSecondaryDest)
{ 
	GetGameInterface()->ExtractWaterFrame(pDest, pSecondaryDest); 
}


//////////////////////////////////////////////////////////////////////////
bool CReplayMgr::ShouldForceHideEntity(CEntity* pEntity)
{
	if(!CReplayMgr::IsReplayInControlOfWorld())
		return false;
	
	if(	pEntity->GetModelNameHash() == 0x3fcbb5b2/*"apa_prop_ap_starb_text"*/ ||
		pEntity->GetModelNameHash() == 0x4e4ccf6e/*"apa_prop_ap_port_text"*/ ||
		pEntity->GetModelNameHash() == 0x68a1d07a/*"apa_prop_ap_stern_text"*/ ||
		pEntity->GetModelNameHash() == 0x83e4a1b1/*"ex_prop_ex_office_text"*/ ||
		//pEntity->GetModelNameHash() == 0x39541D5D/*"bkr_prop_memorial_wall_01a" from the bikers gang house*/ ||
		pEntity->GetModelNameHash() == 0x3A5702EA/*"bkr_prop_rt_memorial_president"*/ ||
		pEntity->GetModelNameHash() == 0x9E2EB034/*"bkr_prop_rt_memorial_vice_pres"*/ ||
		pEntity->GetModelNameHash() == 0x999EED3E/*"bkr_prop_rt_memorial_active_01"*/ ||
		pEntity->GetModelNameHash() == 0x5C1BF26D/*"bkr_prop_rt_memorial_active_02"*/ ||
		pEntity->GetModelNameHash() == 0x4E4556C0/*"bkr_prop_rt_memorial_active_03"*/ ||
		pEntity->GetModelNameHash() == 0xf709586b/*"bkr_prop_rt_clubhouse_table"*/ ||
		pEntity->GetModelNameHash() == 0xbeda7c53/*"bkr_prop_rt_clubhouse_plan_01a"*/ ||
		pEntity->GetModelNameHash() == 0xE8DABD71/*"tr_prop_tr_flag_01a"*/)
		return true;

	return false;
}

//////////////////////////////////////////////////////////////////////////
// Copied from the LODLightManager for now.
u32	CReplayMgr::GenerateObjectHash(const Vector3 &position, s32 modelNameHash)
{
	s32	hashArray[4];
	const Vector3 pos = (position * 10.0f);
	hashArray[0] = static_cast<s32>(pos.x);
	hashArray[1] = static_cast<s32>(pos.y);
	hashArray[2] = static_cast<s32>(pos.z);
	hashArray[3] = static_cast<s32>(modelNameHash);
	return hash((u32*)hashArray, 4, 0);
}


u32 CReplayMgr::GetInvalidHash()
{
	return REPLAY_INVALID_OBJECT_HASH;
}

// ----------------------------------------------------------------------------------------------- //

// Macros internal to hash()
#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))
#define mix(a,b,c)						\
{										\
	a -= c;  a ^= rot(c, 4);  c += b;	\
	b -= a;  b ^= rot(a, 6);  a += c;	\
	c -= b;  c ^= rot(b, 8);  b += a;	\
	a -= c;  a ^= rot(c,16);  c += b;	\
	b -= a;  b ^= rot(a,19);  a += c;	\
	c -= b;  c ^= rot(b, 4);  b += a;	\
}

#define final(a,b,c)		\
{							\
	c ^= b; c -= rot(b,14);	\
	a ^= c; a -= rot(c,11);	\
	b ^= a; b -= rot(a,25);	\
	c ^= b; c -= rot(b,16);	\
	a ^= c; a -= rot(c,4);	\
	b ^= a; b -= rot(a,14);	\
	c ^= b; c -= rot(b,24);	\
}

// ----------------------------------------------------------------------------------------------- //
// k - the key, an array of uint32_t values
// length - the length of the key, as u32
// initval - the previous hash, or an arbitrary value
// ----------------------------------------------------------------------------------------------- //

u32	CReplayMgr::hash(const u32 *k, u32 length, u32 initval)
{
	u32 a,b,c;

	/* Set up the internal state */
	a = b = c = 0xdeadbeef + (length<<2) + initval;

	/* handle most of the key */
	while (length > 3)
	{
		a += k[0];
		b += k[1];
		c += k[2];
		mix(a,b,c);
		length -= 3;
		k += 3;
	}

	/* handle the last 3 uint32_t's, all the case statements fall through */
	switch(length)
	{ 
	case 3 : c+=k[2];
	case 2 : b+=k[1];
	case 1 : a+=k[0];
		final(a,b,c);
	case 0:     /* case 0: nothing left to add */
		break;
	}
	/* report the result */
	return c;
}


//////////////////////////////////////////////////////////////////////////
void CReplayMgr::RecordCloth(CPhysical *pEntity, rage::clothController *pClothController, u32 idx)
{
	if(GetGameInterface())
		GetGameInterface()->GetClothPacketManager().RecordCloth(pEntity, pClothController, idx);
}


//////////////////////////////////////////////////////////////////////////
void CReplayMgr::SetClothAsReplayControlled(CPhysical *pEntity, fragInst *pFragInst, bool onOff)
{
	if(onOff)
	{
		if(GetGameInterface())
			GetGameInterface()->GetClothPacketManager().SetClothAsReplayControlled(pEntity, pFragInst);
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayMgr::PreClothSimUpdate(CPed *pLocalPlayer)
{
	if(GetGameInterface())
		GetGameInterface()->GetClothPacketManager().PreClothSimUpdate(pLocalPlayer);
}


//////////////////////////////////////////////////////////////////////////
CReplayInterfaceVeh* CReplayMgr::GetVehicleInterface()
{
	iReplayInterface* pI = CReplayMgrInternal::GetReplayInterface("CVehicle");
	if(pI == NULL)
		return NULL;
	CReplayInterfaceVeh* pVehicleInterface = verify_cast<CReplayInterfaceVeh*>(pI);
	replayFatalAssertf(pVehicleInterface, "Incorrect interface");

	return pVehicleInterface;
}


//////////////////////////////////////////////////////////////////////////
CReplayInterfaceObject* CReplayMgr::GetObjectInterface()
{
	iReplayInterface* pI = CReplayMgrInternal::GetReplayInterface("CObject");
	if(pI == NULL)
		return NULL;
	CReplayInterfaceObject* pObjectInterface = verify_cast<CReplayInterfaceObject*>(pI);
	replayFatalAssertf(pObjectInterface, "Incorrect interface");

	return pObjectInterface;
}


//////////////////////////////////////////////////////////////////////////
CReplayInterfaceGame* CReplayMgr::GetGameInterface()
{
	iReplayInterface* pI = CReplayMgrInternal::GetReplayInterface("Game");
	if(pI == NULL)
		return NULL;
	CReplayInterfaceGame * pObjectInterface = verify_cast<CReplayInterfaceGame*>(pI);
	replayFatalAssertf(pObjectInterface, "Incorrect interface");

	return pObjectInterface;
}

void CReplayMgr::RecordWarpedEntity(CDynamicEntity* pEntity)
{
	if(pEntity && ShouldRecord() && ShouldRecordWarpFlag(pEntity))
	{
		// If a vehicle has warped this frame then tell all of it's child that they have warped as well.
		if(pEntity->GetIsTypeVehicle())
		{
			CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);
			fwAttachmentEntityExtension *vehicleAttachExt = pVehicle->GetAttachmentExtension();
			if(vehicleAttachExt)
			{
				CPhysical *childAttachment = static_cast<CPhysical*>(vehicleAttachExt->GetChildAttachment());
				while(childAttachment)
				{
					childAttachment->m_nDEflags.bReplayWarpedThisFrame = true;
					
					childAttachment = static_cast<CPhysical*>(childAttachment->GetSiblingAttachment());
				}
			}
		}

		pEntity->m_nDEflags.bReplayWarpedThisFrame = true;
	}
}

void CReplayMgr::CancelRecordingWarpedOnEntity(CPhysical* pEntity)
{
	if(ShouldRecord() && ShouldRecordWarpFlag(pEntity))
	{	
		if(pEntity->GetIsTypeVehicle())
		{
			CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);
			fwAttachmentEntityExtension *vehicleAttachExt = pVehicle->GetAttachmentExtension();
			if(vehicleAttachExt)
			{
				CPhysical *childAttachment = static_cast<CPhysical*>(vehicleAttachExt->GetChildAttachment());
				while(childAttachment)
				{
					childAttachment->m_nDEflags.bReplayWarpedThisFrame = false;

					childAttachment = static_cast<CPhysical*>(childAttachment->GetSiblingAttachment());
				}
			}
		}

		pEntity->m_nDEflags.bReplayWarpedThisFrame = false;
	}
}

#endif // GTA_REPLAY
