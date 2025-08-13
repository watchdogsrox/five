//=========================================================
// File:	SoundPacket.cpp
// Desc:	Sound replay packet.
//=========================================================

#include "SoundPacket.h"

#include "Control/Replay/Replaysettings.h"

#if GTA_REPLAY
#include "audio/environment/environmentgroup.h"
#include "audio/gtaaudioentity.h"
#include "audio/speechmanager.h"
#include "audioengine/engine.h"
#include "audio/scriptaudioentity.h"
#include "audio/ambience/ambientaudioentity.h"
#include "audio/frontendaudioentity.h"
#include "audio/heliaudioentity.h"
#include "audiosoundtypes/envelopesound.h"
#include "control/garages.h"
#include "control/replay/ReplayInternal.h"
#include "debug/Debug.h"
#include "peds/Ped.h"
#include "vehicles/Planes.h"
#include "vehicles/Heli.h"
#include "vehicles/train.h"
#include "vfx/misc/Fire.h"
#include "vfx/vehicleglass/VehicleGlassManager.h"

//=========================================================
// Global Variables
//=========================================================
extern audCollisionAudioEntity	g_CollisionAudioEntity;
extern audScriptAudioEntity		g_ScriptAudioEntity;
extern audSpeechManager			g_SpeechManager;
extern audCategory*				g_SpeechCategories[13];
extern const char *				g_SpeechWaveSlotName;
const u32				g_MaxDoorInstances =0;	// TODO4FIVE Doesn't exist?
//extern DoorInstanceData*		g_DoorInstanceData;
extern CFireManager				g_fireMan;

atArray<u32> CPacketSoundCreatePos::sm_ValidMetadataRefs;


static const u32 g_audSlomoSmashableTable[4][2] = 
{
	{audStringHash("VEH_WINDSCREEN_FRONT_SMASH"),audStringHash("SLO_MO_FRONT_WINDSCREEN_SMASH")},
	{audStringHash("VEH_WINDSCREEN_REAR_SMASH"),audStringHash("SLO_MO_REAR_WINDSCREEN_SMASH")},
	{audStringHash("FRONT_HEADLIGHT_SMASH"),audStringHash("SLO_MO_FRONT_HEADLIGHT_SMASH")},
	{audStringHash("REAR_HEADLIGHT_SMASH"),audStringHash("SLO_MO_REAR_HEADLIGHT_SMASH")}
};

//////////////////////////////////////////////////////////////////////////
//---------------------------------------------------------
// Func: CPacketSoundCreate::CPacketSoundCreate
// Desc: Constructor
//---------------------------------------------------------
// CPacketSoundCreate::CPacketSoundCreate() :
// 	m_PacketID(PACKETID_NONE),	m_GameTime(0),		m_ReplayID(-1),
// 	m_IsPersistent(false),		m_IsOffset(false),	m_SoundHashOrOffset(0)
// {
// 	m_OwnerEntityType = static_cast<u8> (ENTITY_TYPE_NOTHING);
// }


//---------------------------------------------------------
// Func: CPacketSoundCreate::GetPersistentSoundPtrAddress
// Desc: Returns the address of the member variable used
//        to store the persistent sound. The audio
//        controller will keep track of this member
//        variable and set it to NULL when the sound is
//        finished playing.
//---------------------------------------------------------
audSound** CPacketSoundCreate::GetPersistentSoundPtrAddress()
{
	//if( !m_IsPersistent )
	{
		replayAssertf( false, "CPacketSoundCreate::GetPersistentSoundPtrAddress: Sound is not persistent" );
		return NULL;
	}

	/*CEntity*				pEntity				= NULL;
	CPed*					pPed				= NULL;
	CVehicle*				pVehicle			= NULL;
	audSound**				ppSound				= NULL;
	audSpeechAudioEntity*	pSpeechAudioEntity	= NULL;

	// Obtain the necessary owner entity pointers if applicable.
	//  We use these to access the persistent sound member variables 
	//  in the owner's audio entity.
	if( m_CreateParams.GetOwnerReplayId() >= 0 )
	{
		pEntity = reinterpret_cast<CEntity*> (CReplayMgr::GetEntity( REPLAYIDTYPE_PLAYBACK, m_CreateParams.GetOwnerReplayId() ));

		if( !pEntity )
		{
			return NULL;		// Owner of the Sound was Deleted
		}

		if( pEntity->GetIsTypePed() )
		{
			pPed				= reinterpret_cast<CPed*> (pEntity);
			pSpeechAudioEntity	= pPed->GetSpeechAudioEntity();
		}
		else if( pEntity->GetIsTypeVehicle() )
		{
			pVehicle			= reinterpret_cast<CVehicle*> (pEntity);
		}
	}
	else if( m_CreateParams.GetOwnerReplayId() == CReplayMgr::NO_OWNER_FOR_SOUND )
	{

	}
	else
	{
		replayAssertf( false, "CPacketSoundCreate::GetPersistentSoundPtrAddress: Invalid Replay ID %d", m_CreateParams.GetOwnerReplayId() );
		return NULL;
	}

	switch( m_CreateParams.GetContext() )
	{
	case ReplaySound::CONTEXT_COLLISION_SCRAPE:
		{
			for( u32 i = 0; i < g_MaxCollisionEventHistory; ++i )
			{
				if( g_CollisionAudioEntity.m_CollisionEventHistory[i].scrapeSounds[0] == NULL )
				{
					ppSound = &( g_CollisionAudioEntity.m_CollisionEventHistory[i].scrapeSounds[0] );
					break;
				}
				else if( g_CollisionAudioEntity.m_CollisionEventHistory[i].scrapeSounds[1] == NULL )
				{
					ppSound = &( g_CollisionAudioEntity.m_CollisionEventHistory[i].scrapeSounds[1] );
					break;
				}
			}

			replayAssertf( ppSound != NULL, "CPacketSoundCreate::GetPersistentSoundPtrAddress: No free scrape sounds available" );
		}
		break;

	case ReplaySound::CONTEXT_COLLISION_ROLL:
		{
			for( u32 i = 0; i < g_MaxCollisionEventHistory; ++i )
			{
				if( g_CollisionAudioEntity.m_CollisionEventHistory[i].rollSounds[0] == NULL )
				{
					ppSound = &( g_CollisionAudioEntity.m_CollisionEventHistory[i].rollSounds[0] );
					break;
				}
				else if( g_CollisionAudioEntity.m_CollisionEventHistory[i].rollSounds[1] == NULL )
				{
					ppSound = &( g_CollisionAudioEntity.m_CollisionEventHistory[i].rollSounds[1] );
					break;
				}
			}

			replayAssertf( ppSound != NULL, "CPacketSoundCreate::GetPersistentSoundPtrAddress: No free roll sounds available" );
		}
		break;

	case ReplaySound::CONTEXT_COLLISION_IMPACT:
	case ReplaySound::CONTEXT_COLLISION_IMPACT_RESONANCE_PERSISTENT:
		{
			for( u32 i = 0; i < g_MaxCollisionEventHistory; ++i )
			{
				if( g_CollisionAudioEntity.m_CollisionEventHistory[i].impactSounds[0] == NULL )
				{
					ppSound = &( g_CollisionAudioEntity.m_CollisionEventHistory[i].impactSounds[0] );
					break;
				}
				else if( g_CollisionAudioEntity.m_CollisionEventHistory[i].impactSounds[1] == NULL )
				{
					ppSound = &( g_CollisionAudioEntity.m_CollisionEventHistory[i].impactSounds[1] );
					break;
				}
			}

			static const audStringHash mainMotorbikeCollision("MOTORBIKE_MAIN_COLLISIONS_COLLISIONS");
			if(m_SoundHashOrOffset == mainMotorbikeCollision)
			{
				//todo4five this needs changing now the slow mo system has changed
				//switch(CReplayMgr::GetSlowMotionSetting())
				//{
				//case REPLAY_SLOWMOTION_X3:
				//case REPLAY_SLOWMOTION_X4:
				//case REPLAY_SLOWMOTION_BULLET:
				//	{
				//		audSoundInitParams initParams;
				//		Vector3 pos(m_StoredInitParams.Position[0],m_StoredInitParams.Position[1],m_StoredInitParams.Position[2]);
				//		initParams.Position = pos;
				//		initParams.Volume = m_StoredInitParams.Volume;
				//		initParams.StartOffset = m_StoredInitParams.StartOffset;
				//		initParams.IsStartOffsetPercentage = true;
				//		replayDebugf1("slow mo car impact: %f, %d%",initParams.Volume,initParams.StartOffset);
				//		g_AmbientAudioEntity.CreateAndPlaySound("SLO_MO_MOTORBIKE_MAIN_COLLISION",&initParams);
				//	}
				//default:
				//	break;
				//}
			}

			static const audStringHash mainMetalCollision("MAIN_METAL_COLLISIONS_COLLISIONS");
			if(m_SoundHashOrOffset == mainMetalCollision)
			{
				//todo4five this needs changing now the slow mo system has changed
				switch(CReplayMgr::GetSlowMotionSetting())
				{
				case REPLAY_SLOWMOTION_X3:
				case REPLAY_SLOWMOTION_X4:
				case REPLAY_SLOWMOTION_BULLET:
					{
						audSoundInitParams initParams;
						Vector3 pos(m_StoredInitParams.Position[0],m_StoredInitParams.Position[1],m_StoredInitParams.Position[2]);
						initParams.Position = pos;
						initParams.Volume = m_StoredInitParams.Volume;
						initParams.StartOffset = m_StoredInitParams.StartOffset;
						initParams.IsStartOffsetPercentage = true;
						replayDebugf1("slow mo car impact: %f, %d%",initParams.Volume,initParams.StartOffset);
						g_AmbientAudioEntity.CreateAndPlaySound("SLO_MO_MAIN_METAL_COLLISION",&initParams);
					}
				default:
					break;
				}
			}
			replayAssertf( ppSound != NULL, "CPacketSoundCreate::GetPersistentSoundPtrAddress: No free impact sounds available" );
		}
		break;

	case ReplaySound::CONTEXT_DOOR_PAY_AND_SPRAY_COMPRESSOR:
		{
			for( u32 garageIdx = 0; garageIdx < MAX_NUM_GARAGES; ++garageIdx )
			{
				TODO4FIVE if( CGarages::aGarages[garageIdx].m_DoorAudioEntity.m_ResprayLoop == NULL )
				{
					ppSound = &(CGarages::aGarages[garageIdx].m_DoorAudioEntity.m_ResprayLoop);
					break;
				}
			}
			replayAssertf( ppSound != NULL, "CPacketSoundCreate::GetPersistentSoundPtrAddress: No respray sounds available" );
		}
		break;

	case ReplaySound::CONTEXT_OBJECT_GARAGE_DOOR:
	case ReplaySound::CONTEXT_OBJECT_HOSPITAL_DOOR:
		{
			for( u32 doorIdx = 0; doorIdx < g_MaxDoorInstances; ++doorIdx )
			{
				TODO4FIVE if( g_DoorInstanceData[doorIdx].sound == NULL )
				{
					ppSound = &( g_DoorInstanceData[doorIdx].sound );
					break;
				}
			}
			replayAssertf( ppSound != NULL, "CPacketSoundCreate::GetPersistentSoundPtrAddress: No door sounds available" );
		}
		break;

	case ReplaySound::CONTEXT_PED_AMBIENT_ANIM:
		{
			if( !pPed )
			{
				replayAssertf( false, "CPacketSoundCreate::GetPersistentSoundPtrAddress: Non-ped entity attempting ambient anim" );
				return NULL;
			}

			ppSound = &(pPed->GetPedAudioEntity()->m_AmbientAnimLoop);
		}
		break;

	case ReplaySound::CONTEXT_PED_MELEE_SWIPE:
		{
			if( !pPed )
			{
				replayAssertf( false, "CPacketSoundCreate::GetPersistentSoundPtrAddress: Non-ped entity attempting melee swipe" );
				return NULL;
			}

			ppSound = &(pPed->GetPedAudioEntity()->m_MeleeSwipeSound);
		}
		break;

	case ReplaySound::CONTEXT_PED_MOBILE_GARBLED:
		{
			if( !pPed )
			{
				replayAssertf( false, "CPacketSoundCreate::GetPersistentSoundPtrAddress: Non-ped entity attempting mobile garbled" );
				return NULL;
			}

			ppSound = &(pPed->GetPedAudioEntity()->m_GarbledTwoWaySound);
		}
		break;

	case ReplaySound::CONTEXT_PED_MOBILE_RING:
		{
			if( !pPed )
			{
				replayAssertf( false, "CPacketSoundCreate::GetPersistentSoundPtrAddress: Non-ped entity attempting mobile ring" );
				return NULL;
			}

			ppSound = &(pPed->GetPedAudioEntity()->m_MobileRingSound);
		}
		break;

	case ReplaySound::CONTEXT_PED_RINGTONE:
		{
			if( !pPed )
			{
				replayAssertf( false, "CPacketSoundCreate::GetPersistentSoundPtrAddress: Non-ped entity attempting ped ringtone" );
				return NULL;
			}

			ppSound = &(pPed->GetPedAudioEntity()->m_RingToneSound);
		}
		break;

	case ReplaySound::CONTEXT_PED_TROUSER_SWISH:
		{
			if( !pPed )
			{
				replayAssertf( false, "CPacketSoundCreate::GetPersistentSoundPtrAddress: Non-ped entity attempting trouser swish" );
				return NULL;
			}

			ppSound = &(pPed->GetPedAudioEntity()->m_TrouserSwishSound);
		}
		break;

	case ReplaySound::CONTEXT_PED_MOLOTOV_FUSE:
		{
			if( !pPed )
			{
				replayAssertf( false, "CPacketSoundCreate::GetPersistentSoundPtrAddress: Non-ped entity attempting molotov fuse" );
				return NULL;
			}

			ppSound = &(pPed->GetPedAudioEntity()->m_MolotovFuseSound);
		}
		break;

	case ReplaySound::CONTEXT_PED_WATER_SPLASH:
		{
			if( !pPed )
			{
				replayAssertf( false, "CPacketSoundCreate::GetPersistentSoundPtrAddress: Non-ped entity attempting water splash" );
				return NULL;
			}

			ppSound = &(pPed->GetPedAudioEntity()->m_WaterSplashLoop);
		}
		break;

	case ReplaySound::CONTEXT_PED_CLOTHING_WIND:
		{
			if( !pPed )
			{
				replayAssertf( false, "CPacketSoundCreate::GetPersistentSoundPtrAddress: Non-ped entity attempting clothing wind" );
				return NULL;
			}

			ppSound = &(pPed->GetPedAudioEntity()->m_ClothingWindSound);
		}
		break;

	case ReplaySound::CONTEXT_PED_LADDER_SLIDE:
		{
			if( !pPed )
			{
				replayAssertf( false, "CPacketSoundCreate::GetPersistentSoundPtrAddress: Non-ped entity attempting ladder slide" );
				return NULL;
			}

			ppSound = &(pPed->GetPedAudioEntity()->m_LadderSlideSound);
		}
		break;

	case ReplaySound::CONTEXT_PLANES_AMBIENT_JET:
	case ReplaySound::CONTEXT_PLANES_AMBIENT_JET_TAXI:
		{
			TODO4FIVE for( u32 soundIdx = 0; soundIdx < TTYPE_NUMBEROFTHEM_ONLY_PLANES; ++soundIdx )
			{
				if( CPlane::pMainPlaneSound[soundIdx] == NULL )
				{
					ppSound = &( CPlane::pMainPlaneSound[soundIdx] );
					break;
				}
			}
			replayAssertf( ppSound != NULL, "CPacketSoundCreate::GetPersistentSoundPtrAddress: No main plane sounds available" );
		}
		break;

	case ReplaySound::CONTEXT_PLANES_AMBIENT_JET_REAR:
		{
			TODO4FIVE for( u32 soundIdx = 0; soundIdx < TTYPE_NUMBEROFTHEM_ONLY_PLANES; ++soundIdx )
			{
				if( CPlane::pSecondaryPlaneSound[soundIdx] == NULL )
				{
					ppSound = &( CPlane::pSecondaryPlaneSound[soundIdx] );
					break;
				}
			}
			replayAssertf( ppSound != NULL, "CPacketSoundCreate::GetPersistentSoundPtrAddress: No secondary plane sounds available" );
		}
		break;

	case ReplaySound::CONTEXT_SCRIPT_CELLPHONE_RING:
	case ReplaySound::CONTEXT_SCRIPT_CELLPHONE_RING_GENERIC:
		{
			ppSound = &(g_ScriptAudioEntity.m_CellphoneRingSound);
		}
		break;

	case ReplaySound::CONTEXT_SMASHABLE_FALL:
		{
			audSound** ppFallLoop = g_vehicleGlassMan.GetAnyFallLoopSound();

			if( ppFallLoop )
			{
				ppSound = ppFallLoop;
			}

			replayAssertf( ppSound != NULL, "CPacketSoundCreate::GetPersistentSoundPtrAddress: No smash fall sounds available" );
		}
		break;

	case ReplaySound::CONTEXT_VEHICLE_HELI_DOWNWASH:
		{
			if( !pVehicle )
			{
				replayAssertf( false, "CPacketSoundCreate::GetPersistentSoundPtrAddress: Non-vehicle entity attempting heli downwash" );
				return NULL;	
			}

			//TODO4FIVE ppSound = &(pVehicle->GetVehicleAudioEntity()->m_HeliDownwash);
		}
		break;

	case ReplaySound::CONTEXT_VEHICLE_JUMP_LAND:
		{
			if( !pVehicle )
			{
				replayAssertf( false, "CPacketSoundCreate::GetPersistentSoundPtrAddress: Non-vehicle entity attempting jump land" );
				return NULL;	
			}

			//TODO4FIVE ppSound = &(pVehicle->GetVehicleAudioEntity()->m_JumpLandSound);
		}
		break;

	case ReplaySound::CONTEXT_VEHICLE_ROLL:
		{
			if( !pVehicle )
			{
				replayAssertf( false, "CPacketSoundCreate::GetPersistentSoundPtrAddress: Non-vehicle entity attempting car roll" );
				return NULL;	
			}

			//TODO4FIVE ppSound = &(pVehicle->GetVehicleAudioEntity()->m_CarRollSound);
		}
		break;

	case ReplaySound::CONTEXT_WEAPON_PROJECTILE_SOUND:
	case ReplaySound::CONTEXT_WEAPON_PROJECTILE_WHISTLE:
		{
			TODO4FIVE for( u32 projIdx = 0; projIdx < PROJ_MAX_PROJECTILES; ++projIdx )
			{
				if( CProjectileInfo::aProjectileInfo[projIdx].m_Sound == NULL )
				{
					ppSound = &( CProjectileInfo::aProjectileInfo[projIdx].m_Sound );
					break;
				}
				else if( CProjectileInfo::aProjectileInfo[projIdx].m_WhistleSound == NULL )
				{
					ppSound = &( CProjectileInfo::aProjectileInfo[projIdx].m_WhistleSound );
					break;
				}
			}
			replayAssertf( ppSound, "CPacketSoundCreate::GetPersistentSoundPtrAddress: Could not find a projectile sound" );
		}
		break;
			
	default:
		replayAssertf( false, "CPacketSoundCreate::GetPersistentSoundPtrAddress: Invalid context" );
		return NULL;
		break;
	}
	
	if( *ppSound )
	{
		// An entity is likely trying to play another sound of the same type
		//  before the first sound completes. Could occur at the start of a 
		//  replay when all the sounds in the history buffer before the time 
		//  of the first frame are processed.

		(*ppSound)->StopAndForget();
		*ppSound = NULL;
	}

	return ppSound;*/
}


//////////////////////////////////////////////////////////////////////////
CPacketSoundCreate::CPacketSoundCreate(audMetadataRef soundHash, const audSoundInitParams* initParams, u8 soundEntity)
	: CPacketEvent(PACKETID_SOUND_CREATE_AND_PLAY, sizeof(CPacketSoundCreate))
	, CPacketEntityInfo()
{
	m_SoundHashOrMeta		= soundHash;

	m_SoundEntity			= soundEntity;
	if(initParams)
	{
		m_StoredInitParams.Populate( *initParams );
		m_hasParams = true;
	}
	else
	{
		m_hasParams = false;
	}
}


//////////////////////////////////////////////////////////////////////////
bool CPacketSoundCreate::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return false;
	}

	CEntity* pEntity = eventInfo.GetEntity();
	audSoundInitParams	initParams;
	Vector3				position;
	audSoundInitParams* pInitParams = NULL;
	bool				createdEnvironmentGroup = false;
	if(m_hasParams)
	{
		bool isSuccessful = ReplaySound::ExtractSoundInitParams( initParams, m_StoredInitParams, pEntity, &m_CreateParams, &position, 0u, 0u, &createdEnvironmentGroup);

		if( !isSuccessful )
		{
			if (createdEnvironmentGroup)
			{
				const_cast<audEnvironmentGroupInterface*>(initParams.EnvironmentGroup)->RemoveSoundReference();
			}

			return false;
		}
		pInitParams = &initParams;
	}

	// TODO Potentially change the sound to play if the playback is slowed down
	audMetadataRef soundRef = m_SoundHashOrMeta;
// 	if(CReplayMgr::GetSlowMotionSetting() == REPLAY_SLOWMOTION_BULLET)
// 	{
// 		// Find a slowmo version
// 		ReplaySound::GetSound(ReplaySound::eHalfSpeedSound, soundRef);
// 	}

	audEntity* pAudEntity = NULL;
	if(pEntity)
	{
		pAudEntity = pEntity->GetAudioEntity();
		replayAssertf(pAudEntity, "CPacketSoundCreate attempting to play sound on entity %s with no audio entity", pEntity->GetModelName());
	}
	else
	{
		switch(m_SoundEntity)
		{
		case eWeaponSoundEntity:
			{
				pAudEntity = &g_WeaponAudioEntity;
				break;
			}
		default:
			replayAssert(false && "No entity to spawn sound");
			break;
		}
	}

	bool success = false;

	if(pAudEntity)
	{
		success = pAudEntity->CreateAndPlaySound(soundRef, pInitParams);
	}

	if (createdEnvironmentGroup)
	{
		const_cast<audEnvironmentGroupInterface*>(initParams.EnvironmentGroup)->RemoveSoundReference();
	}
	
	return success;
}

//////////////////////////////////////////////////////////////////////////

audEntity*	GetAudioSoundEntity(eAudioSoundEntityType type)
{
	audEntity* pAudEntity = NULL;
	switch(type)
	{
	case eWeaponSoundEntity:
		{
			pAudEntity = &g_WeaponAudioEntity;
			break;
		}
	case eAmbientSoundEntity:
		{
			pAudEntity = &g_AmbientAudioEntity;
			break;
		}
	case eCollisionAudioEntity:
		{
			pAudEntity = &g_CollisionAudioEntity;
			break;
		}
	case eFrontendSoundEntity:
		{
			pAudEntity = &g_FrontendAudioEntity;
			break;
		}
	default:
		replayAssertf(false,"Invalid eAudioSoundEntityType in GetAudioSoundEntity()");
		break;
	}

	return pAudEntity;
}

//////////////////////////////////////////////////////////////////////////

CPacketSoundCreatePos::CPacketSoundCreatePos(eAudioSoundEntityType type, u32 UNUSED_PARAM(soundHash), const Vector3& position)
: CPacketEvent(PACKETID_SOUND_CREATE_POSITION, sizeof(CPacketSoundCreatePos))
, CPacketEntityInfo()
{
	// CPacketSoundCreatePos has been deprecated due to invalid metadata ref usage - please use a different packet type. We
	// are only keeping it around to maintain compatibility with existing replay files
	FastAssert(false);
	m_SoundHash = ~0u;

	m_Position.Store(position);
	m_SoundEntityType = (u8)type;
}

void CPacketSoundCreatePos::Extract(const CEventInfo<void>&) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	// HL - This packet is deprecated due to invalid metadata ref usage. Since it was only used in a couple of places, we are attempting to maintain compatibility with 
	// existing replay files by checking to see if the 'hash' (actually a metadata ref) corresponds to the metadata ref of any of the sounds that were known to be using
	// this packet type - if so, it is safe to trigger, if not then ignore it (as its just a random offset value and could end up triggering any old sound)
	if(sm_ValidMetadataRefs.Find(m_SoundHash) != -1)
	{
		audSoundInitParams initParams;
		m_Position.Load(initParams.Position);
		GetAudioSoundEntity((eAudioSoundEntityType)m_SoundEntityType)->CreateAndPlaySound(audMetadataRef::Create(m_SoundHash), &initParams);
	}
}

void CPacketSoundCreatePos::CalculateValidMetadataOffsets()
{
	// See comments in CPacketSoundCreatePos::Extract for why we are doing this!
	sm_ValidMetadataRefs.Reset();

	audSoundSet codedWeaponSoundset;

	if(codedWeaponSoundset.Init(ATSTRINGHASH("CODED_WEAPON_SOUNDS", 0xB148A7E7)))
	{
		sm_ValidMetadataRefs.PushAndGrow(codedWeaponSoundset.Find(ATSTRINGHASH("rifleFlashlight", 0x1EEE94D5)).Get());
		sm_ValidMetadataRefs.PushAndGrow(codedWeaponSoundset.Find(ATSTRINGHASH("flashlightOn", 0x5EF7274F)).Get());
		sm_ValidMetadataRefs.PushAndGrow(codedWeaponSoundset.Find(ATSTRINGHASH("flashlightOff", 0x66ECAB94)).Get());
		sm_ValidMetadataRefs.PushAndGrow(codedWeaponSoundset.Find(ATSTRINGHASH("EMPTY_FIRE", 0x90b0f84c)).Get());
	}

	audSoundSet emptyWeaponSoundset;

	if(emptyWeaponSoundset.Init(ATSTRINGHASH("EMPTY_WEAPON_SOUNDS", 0x13AC8CD3)))
	{
		for(u32 i = 0; i < emptyWeaponSoundset.GetMetadata()->numSounds; i++)
		{
			const u32 metadataRef = emptyWeaponSoundset.GetMetadata()->Sounds[i].Sound.Get();

			if(sm_ValidMetadataRefs.Find(metadataRef) == -1)
			{
				sm_ValidMetadataRefs.PushAndGrow(metadataRef);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPacketSoundCreateMisc_Old::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	audSoundInitParams initParams;
	m_Position.Load(initParams.Position);
	initParams.IsStartOffsetPercentage = m_IsStartOffsetPercentage == 1 ? true : false;
	initParams.Volume = m_Volume;
	initParams.Predelay = m_Predelay;
	initParams.Pitch = m_Pitch;
	initParams.StartOffset = m_StartOffset;
	initParams.TrackEntityPosition = m_TrackEntityPosition == 1 ? true : false;

	CEntity* pEntityTracker = eventInfo.GetEntity(0);
	if(pEntityTracker)
	{
		initParams.Tracker = pEntityTracker->GetPlaceableTracker();
	}

	CEntity* pEntityAudio = eventInfo.GetEntity(1);
	audEntity* pAudEntity = NULL;
	if(pEntityAudio)
	{
		pAudEntity = pEntityAudio->GetAudioEntity();

		if(m_HasEnvironmentGroup == 1)
		{	
			initParams.EnvironmentGroup	= pEntityAudio->GetAudioEnvironmentGroup();
		}

		if(pEntityAudio->GetIsTypePed())
		{
			CPed* pPed = (CPed*)pEntityAudio;
			((audPedAudioEntity*)pPed->GetAudioEntity())->SetForceUpdateVariablesForReplay();
		}
	}
	else
	{
		pAudEntity = GetAudioSoundEntity((eAudioSoundEntityType)m_SoundEntityType);
	}

	if(pAudEntity)
	{
		if(m_useSoundHash)
		{
			if(audNorthAudioEngine::IsInSlowMoVideoEditor() && m_SlowMoSoundHash != g_NullSoundHash)
			{
				pAudEntity->CreateAndPlaySound(m_SlowMoSoundHash, &initParams);
			}
			else
			{
				pAudEntity->CreateAndPlaySound(m_SoundHash, &initParams);
			}
		}
		else
		{

			audMetadataRef soundMetaRef;
			soundMetaRef.SetInvalid();

			audSoundSet soundset;
			soundset.Init(m_SoundSetHash);
			if(soundset.IsInitialised())
			{
				if(audNorthAudioEngine::IsInSlowMoVideoEditor() && m_SlowMoSoundHash != g_NullSoundHash)
				{
					soundMetaRef = soundset.Find(m_SlowMoSoundHash);
				}
				else
				{
					soundMetaRef = soundset.Find(m_SoundHash);
				}
			}
			replayAssertf(soundMetaRef.IsValid(), "CPacketSoundCreateMisc invalid soundMetaRef (soundset %u, sound %u)", m_SoundSetHash, m_SoundHash);

			pAudEntity->CreateAndPlaySound(soundMetaRef, &initParams);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CPacketSoundCreateMisc::CPacketSoundCreateMisc(const u32 soundSetHash, const u32 soundHash, const audSoundInitParams* initParams, eAudioSoundEntityType soundEntity, u32 slowMoSoundHash)
	: CPacketEvent(PACKETID_SOUND_CREATE_MISC, sizeof(CPacketSoundCreateMisc))
	, CPacketEntityInfoStaticAware()
{	
	Init(soundSetHash, soundHash, initParams, soundEntity, slowMoSoundHash);
}


//////////////////////////////////////////////////////////////////////////
CPacketSoundCreateMisc::CPacketSoundCreateMisc(const u32 soundHash, const audSoundInitParams* initParams, eAudioSoundEntityType soundEntity, u32 slowMoSoundHash)
	: CPacketEvent(PACKETID_SOUND_CREATE_MISC, sizeof(CPacketSoundCreateMisc))
	, CPacketEntityInfoStaticAware()
{
	Init(0, soundHash, initParams, soundEntity, slowMoSoundHash);	
}

//////////////////////////////////////////////////////////////////////////
void CPacketSoundCreateMisc::Init(const u32 soundSetHash, const u32 soundHash, const audSoundInitParams* initParams, eAudioSoundEntityType soundEntity, u32 slowMoSoundHash)
{
	m_SoundSetHash			= soundSetHash;
	m_SoundHash				= soundHash;
	m_SlowMoSoundHash		= slowMoSoundHash;
	m_useSoundHash			= (soundSetHash == 0 || soundSetHash == g_NullSoundHash);

	audSoundInitParams defaultInitParams;
	if(!initParams)
	{
		initParams = &defaultInitParams;
	}

	m_Position.Store(initParams->Position);
	m_IsStartOffsetPercentage = initParams->IsStartOffsetPercentage ? 1 : 0;
	m_Volume = initParams->Volume;
	m_SoundEntityType = (u8)soundEntity;
	m_Predelay = initParams->Predelay;
	m_Pitch = initParams->Pitch;
	m_StartOffset = initParams->StartOffset;
	m_TrackEntityPosition = initParams->TrackEntityPosition ? 1 : 0;
	m_HasEnvironmentGroup = initParams->EnvironmentGroup ? 1 : 0;
}

void CPacketSoundCreateMisc::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	audSoundInitParams initParams;
	m_Position.Load(initParams.Position);
	initParams.IsStartOffsetPercentage = m_IsStartOffsetPercentage == 1 ? true : false;
	initParams.Volume = m_Volume;
	initParams.Predelay = m_Predelay;
	initParams.Pitch = m_Pitch;
	initParams.StartOffset = m_StartOffset;
	initParams.TrackEntityPosition = m_TrackEntityPosition == 1 ? true : false;

	CEntity* pEntityTracker = eventInfo.GetEntity(0);
	if(pEntityTracker)
	{
		initParams.Tracker = pEntityTracker->GetPlaceableTracker();
	}

	CEntity* pEntityAudio = eventInfo.GetEntity(1);
	audEntity* pAudEntity = NULL;
	if(pEntityAudio)
	{
		pAudEntity = pEntityAudio->GetAudioEntity();

		if(m_HasEnvironmentGroup == 1)
		{	
			initParams.EnvironmentGroup	= pEntityAudio->GetAudioEnvironmentGroup();
		}

		if(pEntityAudio->GetIsTypePed())
		{
			CPed* pPed = (CPed*)pEntityAudio;
			((audPedAudioEntity*)pPed->GetAudioEntity())->SetForceUpdateVariablesForReplay();
		}
	}
	else
	{
		pAudEntity = GetAudioSoundEntity((eAudioSoundEntityType)m_SoundEntityType);
	}
	
	if(pAudEntity)
	{
		if(m_useSoundHash)
		{
			if(audNorthAudioEngine::IsInSlowMoVideoEditor() && m_SlowMoSoundHash != g_NullSoundHash)
			{
				pAudEntity->CreateAndPlaySound(m_SlowMoSoundHash, &initParams);
			}
			else
			{
				pAudEntity->CreateAndPlaySound(m_SoundHash, &initParams);
			}
		}
		else
		{

			audMetadataRef soundMetaRef;
			soundMetaRef.SetInvalid();

			audSoundSet soundset;
			soundset.Init(m_SoundSetHash);
			if(soundset.IsInitialised())
			{
				if(audNorthAudioEngine::IsInSlowMoVideoEditor() && m_SlowMoSoundHash != g_NullSoundHash)
				{
					soundMetaRef = soundset.Find(m_SlowMoSoundHash);
				}
				else
				{
					soundMetaRef = soundset.Find(m_SoundHash);
				}
			}
			replayAssertf(soundMetaRef.IsValid(), "CPacketSoundCreateMisc invalid soundMetaRef (soundset %u, sound %u)", m_SoundSetHash, m_SoundHash);

			pAudEntity->CreateAndPlaySound(soundMetaRef, &initParams);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CPacketSoundCreateMiscWithVars::CPacketSoundCreateMiscWithVars(const u32 soundSetHash, const u32 soundHash, const audSoundInitParams* initParams, eAudioSoundEntityType soundEntity, u32 slowMoSoundHash)
	: CPacketEvent(PACKETID_SOUND_CREATE_MISC_W_VAR, sizeof(CPacketSoundCreateMiscWithVars))
	, CPacketEntityInfoStaticAware()
{	
	Init(soundSetHash, soundHash, initParams, soundEntity, slowMoSoundHash);
}


//////////////////////////////////////////////////////////////////////////
CPacketSoundCreateMiscWithVars::CPacketSoundCreateMiscWithVars(const u32 soundHash, const audSoundInitParams* initParams, eAudioSoundEntityType soundEntity, u32 slowMoSoundHash)
	: CPacketEvent(PACKETID_SOUND_CREATE_MISC_W_VAR, sizeof(CPacketSoundCreateMiscWithVars))
	, CPacketEntityInfoStaticAware()
{
	Init(0, soundHash, initParams, soundEntity, slowMoSoundHash);	
}

//////////////////////////////////////////////////////////////////////////
void CPacketSoundCreateMiscWithVars::Init(const u32 soundSetHash, const u32 soundHash, const audSoundInitParams* initParams, eAudioSoundEntityType soundEntity, u32 slowMoSoundHash)
{
	m_SoundSetHash			= soundSetHash;
	m_SoundHash				= soundHash;
	m_SlowMoSoundHash		= slowMoSoundHash;
	m_useSoundHash			= (soundSetHash == 0 || soundSetHash == g_NullSoundHash);

	audSoundInitParams defaultInitParams;
	if(!initParams)
	{
		initParams = &defaultInitParams;
	}

	m_Position.Store(initParams->Position);
	m_IsStartOffsetPercentage = initParams->IsStartOffsetPercentage ? 1 : 0;
	m_Volume = initParams->Volume;
	m_SoundEntityType = (u8)soundEntity;
	m_Predelay = initParams->Predelay;
	m_Pitch = initParams->Pitch;
	m_StartOffset = initParams->StartOffset;
	m_TrackEntityPosition = initParams->TrackEntityPosition ? 1 : 0;
	m_HasEnvironmentGroup = initParams->EnvironmentGroup ? 1 : 0;

	for(int i=0; i<kMaxInitParamVariables; i++)
	{
		m_Variables[i].nameHash = initParams->Variables[i].nameHash;
		m_Variables[i].value = initParams->Variables[i].value;
	}
}

void CPacketSoundCreateMiscWithVars::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	audSoundInitParams initParams;
	m_Position.Load(initParams.Position);
	initParams.IsStartOffsetPercentage = m_IsStartOffsetPercentage == 1 ? true : false;
	initParams.Volume = m_Volume;
	initParams.Predelay = m_Predelay;
	initParams.Pitch = m_Pitch;
	initParams.StartOffset = m_StartOffset;
	initParams.TrackEntityPosition = m_TrackEntityPosition == 1 ? true : false;

	for(int i=0; i<kMaxInitParamVariables; i++)
	{
		if(m_Variables[i].nameHash != 0)
		{
			initParams.SetVariableValue(m_Variables[i].nameHash, m_Variables[i].value);
		}
	}

	CEntity* pEntityTracker = eventInfo.GetEntity(0);
	if(pEntityTracker)
	{
		initParams.Tracker = pEntityTracker->GetPlaceableTracker();
	}

	CEntity* pEntityAudio = eventInfo.GetEntity(1);
	audEntity* pAudEntity = NULL;
	if(pEntityAudio)
	{
		pAudEntity = pEntityAudio->GetAudioEntity();

		if(m_HasEnvironmentGroup == 1)
		{	
			initParams.EnvironmentGroup	= pEntityAudio->GetAudioEnvironmentGroup();
		}

		if(pEntityAudio->GetIsTypePed())
		{
			CPed* pPed = (CPed*)pEntityAudio;
			((audPedAudioEntity*)pPed->GetAudioEntity())->SetForceUpdateVariablesForReplay();
		}
	}
	else
	{
		pAudEntity = GetAudioSoundEntity((eAudioSoundEntityType)m_SoundEntityType);
	}

	if(pAudEntity)
	{
		if(m_useSoundHash)
		{
			if(audNorthAudioEngine::IsInSlowMoVideoEditor() && m_SlowMoSoundHash != g_NullSoundHash)
			{
				pAudEntity->CreateAndPlaySound(m_SlowMoSoundHash, &initParams);
			}
			else
			{
				pAudEntity->CreateAndPlaySound(m_SoundHash, &initParams);
			}
		}
		else
		{

			audMetadataRef soundMetaRef;
			soundMetaRef.SetInvalid();

			audSoundSet soundset;
			soundset.Init(m_SoundSetHash);
			if(soundset.IsInitialised())
			{
				if(audNorthAudioEngine::IsInSlowMoVideoEditor() && m_SlowMoSoundHash != g_NullSoundHash)
				{
					soundMetaRef = soundset.Find(m_SlowMoSoundHash);
				}
				else
				{
					soundMetaRef = soundset.Find(m_SoundHash);
				}
			}
			replayAssertf(soundMetaRef.IsValid(), "CPacketSoundCreateMiscWithVars invalid soundMetaRef (soundset %u, sound %u)", m_SoundSetHash, m_SoundHash);

			pAudEntity->CreateAndPlaySound(soundMetaRef, &initParams);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CPacketSoundCreatePersistant::CPacketSoundCreatePersistant(u32 soundSetHash, u32 soundHash, const audSoundInitParams* initParams, bool play,  eAudioSoundEntityType soundEntity, ReplaySound::eContext context)
: CPacketEventTracked(PACKETID_SOUND_CREATE_AND_PLAY_PERSIST, sizeof(CPacketSoundCreatePersistant))
, CPacketEntityInfo()
{
	Init(soundSetHash, soundHash, initParams, play, soundEntity, context);	
}


//////////////////////////////////////////////////////////////////////////
CPacketSoundCreatePersistant::CPacketSoundCreatePersistant(u32 soundHash, const audSoundInitParams* initParams, bool play,  eAudioSoundEntityType soundEntity, ReplaySound::eContext context)
: CPacketEventTracked(PACKETID_SOUND_CREATE_AND_PLAY_PERSIST, sizeof(CPacketSoundCreatePersistant))
, CPacketEntityInfo()
{
	Init(0, soundHash, initParams, play, soundEntity, context);	
}

//////////////////////////////////////////////////////////////////////////
void CPacketSoundCreatePersistant::Init(u32 soundSetHash, u32 soundHash, const audSoundInitParams* initParams, bool play, eAudioSoundEntityType soundEntity, ReplaySound::eContext context)
{
	m_SoundSetHash = soundSetHash;
	m_SoundHash	= soundHash;
	m_useSoundHash = (soundSetHash == 0 || soundSetHash == g_NullSoundHash);
	m_SoundEntity = (u8)soundEntity;
	if(initParams)
	{
		m_StoredInitParams.Populate( *initParams, context );
		m_hasParams		= true;
	}
	else
	{
		m_hasParams		= false;
	}

	m_play				= play;
}

//////////////////////////////////////////////////////////////////////////
void CPacketSoundCreatePersistant::Extract(CTrackedEventInfo<tTrackedSoundType>& eventInfo) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	CEntity* pEntity = eventInfo.pEntity[0];

	audSoundInitParams	initParams;
	Vector3				position;
	audSoundInitParams* pInitParams = NULL;
	bool				createdEnvironmentGroup = false;
	if(m_hasParams)
	{
		bool isSuccessful = ReplaySound::ExtractSoundInitParams( initParams, m_StoredInitParams, pEntity, &m_CreateParams, &position, 0, m_SoundEntity, &createdEnvironmentGroup);

		if( !isSuccessful )
		{
			if (createdEnvironmentGroup)
			{
				const_cast<audEnvironmentGroupInterface*>(initParams.EnvironmentGroup)->RemoveSoundReference();
			}

			return;
		}
		pInitParams = &initParams;
	}

	audSound*& soundRef = eventInfo.m_pEffect[0].m_pSound = NULL;

	// First figure out what function we want to call....
	// If we're playing the sound immediately then do CreateAndPlaySound_Persistent
	// Else if we're holding onto the sound to play later then do CreateSound_PersistentReference
	typedef void(audEntity::*tFuncHash)(const u32, audSound**, const audSoundInitParams*);
	typedef void(audEntity::*tFuncMeta)(const audMetadataRef, audSound**, const audSoundInitParams*);
	tFuncHash hashFunc = NULL;
	tFuncMeta metaFunc = NULL;
	if(m_play)
	{
		hashFunc = &audEntity::CreateAndPlaySound_Persistent;
		metaFunc = &audEntity::CreateAndPlaySound_Persistent;
	}
	else
	{
		hashFunc = &audEntity::CreateSound_PersistentReference;
		metaFunc = &audEntity::CreateSound_PersistentReference;
	}

	audMetadataRef soundMetaRef;
	soundMetaRef.SetInvalid();
	if(!m_useSoundHash)
	{
		audSoundSet soundset;
		soundset.Init(m_SoundSetHash);
		if(soundset.IsInitialised())
		{
			soundMetaRef = soundset.Find(m_SoundHash);
		}
		replayAssertf(soundMetaRef.IsValid(), "CPacketSoundCreatePersistant invalid  (soundset: %u, hash: %u)", m_SoundSetHash, m_SoundHash);
	}

	// Now figure out what object to call the above function pointer on...
	if(pEntity && m_SoundEntity == eNoGlobalSoundEntity)
	{	// A proper entity has been passed in so spawn the sound from that
		naAudioEntity* pAudioEntity = pEntity->GetAudioEntity();
		
		if(replayVerifyf(pAudioEntity != NULL, "CPacketSoundCreatePersistant attempting to play sound %u on entity %s with no audio entity", m_SoundHash, pEntity->GetModelName()))
		{
			if(m_useSoundHash)
				(pAudioEntity->*(hashFunc))(m_SoundHash, &soundRef, pInitParams);
			else
				(pAudioEntity->*(metaFunc))(soundMetaRef, &soundRef, pInitParams);

			if(pEntity->GetIsTypePed())
			{
				CPed* pPed = (CPed*)pEntity;
				((audPedAudioEntity*)pPed->GetAudioEntity())->SetForceUpdateVariablesForReplay();
			}
		}
	}
	else
	{
		// No recorded entity so attempt to use one of the global entities
		switch(m_SoundEntity)
		{
		case eWeaponSoundEntity:
			{
				if(m_useSoundHash)
					(g_WeaponAudioEntity.*(hashFunc))(m_SoundHash, &soundRef, pInitParams);
				else
					(g_WeaponAudioEntity.*(metaFunc))(soundMetaRef, &soundRef, pInitParams);
				break;
			}

		case eScriptSoundEntity:
			{
				// we will only use the update packet(CPacketSoundUpdatePersistant) to track these
				break;
			}
		case eAmbientSoundEntity:
			{
				if(m_useSoundHash)
					(g_AmbientAudioEntity.*(hashFunc))(m_SoundHash, &soundRef, pInitParams);
				else
					(g_AmbientAudioEntity.*(metaFunc))(soundMetaRef, &soundRef, pInitParams);
				break;
			}
		case eFrontendSoundEntity:
			{
				if(m_useSoundHash)
					(g_FrontendAudioEntity.*(hashFunc))(m_SoundHash, &soundRef, pInitParams);
				else
					(g_FrontendAudioEntity.*(metaFunc))(soundMetaRef, &soundRef, pInitParams);
				break;
			}
		case eNoGlobalSoundEntity:
		default:
			replayAssert(false && "No entity to spawn sound");
			break;
		}
	}

	if (createdEnvironmentGroup)
	{
		const_cast<audEnvironmentGroupInterface*>(initParams.EnvironmentGroup)->RemoveSoundReference();
	}

	replayDebugf2("Start sound %p", soundRef);
}


//////////////////////////////////////////////////////////////////////////
CPacketSoundUpdatePersistant::CPacketSoundUpdatePersistant(u32 soundSetHash, u32 soundHash, const audSoundInitParams* initParams, bool play,  eAudioSoundEntityType soundEntity, ReplaySound::eContext context, const u32 playTime)
	: CPacketEventTracked(PACKETID_SOUND_UPDATE_PERSIST, sizeof(CPacketSoundUpdatePersistant))
	, CPacketEntityInfo()
{
	Init(soundSetHash, soundHash, initParams, play, soundEntity, context, playTime);	
}


//////////////////////////////////////////////////////////////////////////
CPacketSoundUpdatePersistant::CPacketSoundUpdatePersistant(u32 soundHash, const audSoundInitParams* initParams, bool play,  eAudioSoundEntityType soundEntity, ReplaySound::eContext context, const u32 playTime)
	: CPacketEventTracked(PACKETID_SOUND_UPDATE_PERSIST, sizeof(CPacketSoundUpdatePersistant))
	, CPacketEntityInfo()
{
	Init(0, soundHash, initParams, play, soundEntity, context, playTime);	
}

//////////////////////////////////////////////////////////////////////////
void CPacketSoundUpdatePersistant::Init(u32 soundSetHash, u32 soundHash, const audSoundInitParams* initParams, bool play, eAudioSoundEntityType soundEntity, ReplaySound::eContext context, u32 playTime)
{
	m_SoundSetHash = soundSetHash;
	m_SoundHash	= soundHash;
	m_useSoundHash = (soundSetHash == 0 || soundSetHash == g_NullSoundHash);
	m_SoundEntity = (u8)soundEntity;
	if(initParams)
	{
		m_StoredInitParams.Populate( *initParams, context );
		m_hasParams		= true;
	}
	else
	{
		m_hasParams		= false;
	}

	m_play				= play;
	m_playTime = playTime;
}

//////////////////////////////////////////////////////////////////////////
void CPacketSoundUpdatePersistant::Extract(CTrackedEventInfo<tTrackedSoundType>& eventInfo) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	CEntity* pEntity = eventInfo.pEntity[0];
	audSound*& soundRef = eventInfo.m_pEffect[0].m_pSound;

	if(soundRef || eventInfo.m_pEffect[2].m_pSound || (m_useSoundHash && m_SoundHash == g_NullSoundHash))
	{
		return;
	}

	audSoundInitParams	initParams;
	Vector3				position;
	audSoundInitParams* pInitParams = &initParams;
	bool				allocatedEnvironmentGroup = false;
	if(m_hasParams)
	{
		bool isSuccessful = ReplaySound::ExtractSoundInitParams( initParams, m_StoredInitParams, pEntity, &m_CreateParams, &position, 0, m_SoundEntity, &allocatedEnvironmentGroup);

		if( !isSuccessful )
		{
			if (allocatedEnvironmentGroup)
			{
				const_cast<audEnvironmentGroupInterface*>(initParams.EnvironmentGroup)->RemoveSoundReference();
			}

			return;
		}
	}

	// add the playtime to the start offset
	pInitParams->StartOffset += m_playTime;
	if(pInitParams->StartOffset < 0)
	{
		pInitParams->StartOffset = 0;
	}

	// First figure out what function we want to call....
	// If we're playing the sound immediately then do CreateAndPlaySound_Persistent
	// Else if we're holding onto the sound to play later then do CreateSound_PersistentReference
	typedef void(audEntity::*tFuncHash)(const u32, audSound**, const audSoundInitParams*);
	typedef void(audEntity::*tFuncMeta)(const audMetadataRef, audSound**, const audSoundInitParams*);
	tFuncHash hashFunc = NULL;
	tFuncMeta metaFunc = NULL;
	if(m_play)
	{
		hashFunc = &audEntity::CreateAndPlaySound_Persistent;
		metaFunc = &audEntity::CreateAndPlaySound_Persistent;
	}
	else
	{
		hashFunc = &audEntity::CreateSound_PersistentReference;
		metaFunc = &audEntity::CreateSound_PersistentReference;
	}

	audMetadataRef soundMetaRef;
	soundMetaRef.SetInvalid();
	if(!m_useSoundHash)
	{
		audSoundSet soundset;
		soundset.Init(m_SoundSetHash);
		if(soundset.IsInitialised())
		{
			soundMetaRef = soundset.Find(m_SoundHash);
		}
		replayAssertf(soundMetaRef.IsValid(), "CPacketSoundUpdatePersistant invalid  (soundset: %u, hash: %u)", m_SoundSetHash, m_SoundHash);
	}

	// Now figure out what object to call the above function pointer on...
	if(pEntity && m_SoundEntity == eNoGlobalSoundEntity)
	{	// A proper entity has been passed in so spawn the sound from that
		naAudioEntity* pAudioEntity = pEntity->GetAudioEntity();

		if(replayVerifyf(pAudioEntity != NULL, "CPacketSoundUpdatePersistant attempting to play sound %d on entity %s with no audio entity", m_SoundHash, pEntity->GetModelName()))
		{
			if(m_useSoundHash)
				(pAudioEntity->*(hashFunc))(m_SoundHash, &soundRef, pInitParams);
			else
				(pAudioEntity->*(metaFunc))(soundMetaRef, &soundRef, pInitParams);

			if(pEntity->GetIsTypePed())
			{
				CPed* pPed = (CPed*)pEntity;
				((audPedAudioEntity*)pPed->GetAudioEntity())->SetForceUpdateVariablesForReplay();
			}
		}
	}
	else
	{
		// No recorded entity so attempt to use one of the global entities
		switch(m_SoundEntity)
		{
		case eWeaponSoundEntity:
			{
				if(m_useSoundHash)
					(g_WeaponAudioEntity.*(hashFunc))(m_SoundHash, &soundRef, pInitParams);
				else
					(g_WeaponAudioEntity.*(metaFunc))(soundMetaRef, &soundRef, pInitParams);
				break;
			}

		case eScriptSoundEntity:
			{
				// This may seem weird, but we are going to create a sound ref and never play it. This will keep the eventInfo valid even if the 
				// real sound stops. This prevents extra update packets from starting the sound up again. The StopSound packet or end of the clip will stop this sound.
				// we are using m_pEffect[2] because 0 and 1 affect the staleness
				if(m_useSoundHash)
				{
					g_ScriptAudioEntity.CreateSound_PersistentReference(ATSTRINGHASH("AMP_FIX_SYNTH", 0xBD51EB95), &eventInfo.m_pEffect[2].m_pSound);
					(g_ScriptAudioEntity.*(hashFunc))(m_SoundHash, &soundRef, pInitParams);
				}
				else
				{
					g_ScriptAudioEntity.CreateSound_PersistentReference(ATSTRINGHASH("AMP_FIX_SYNTH", 0xBD51EB95), &eventInfo.m_pEffect[2].m_pSound);
					(g_ScriptAudioEntity.*(metaFunc))(soundMetaRef, &soundRef, pInitParams);
				}
				break;
			}
		case eAmbientSoundEntity:
			{
				if(m_useSoundHash)
					(g_AmbientAudioEntity.*(hashFunc))(m_SoundHash, &soundRef, pInitParams);
				else
					(g_AmbientAudioEntity.*(metaFunc))(soundMetaRef, &soundRef, pInitParams);
				break;
			}
		case eFrontendSoundEntity:
			{
				if(m_useSoundHash)
					(g_FrontendAudioEntity.*(hashFunc))(m_SoundHash, &soundRef, pInitParams);
				else
					(g_FrontendAudioEntity.*(metaFunc))(soundMetaRef, &soundRef, pInitParams);
				break;
			}
		case eNoGlobalSoundEntity:
		default:
			replayAssert(false && "No entity to spawn sound");
			break;
		}
	}

	if (allocatedEnvironmentGroup)
	{
		const_cast<audEnvironmentGroupInterface*>(initParams.EnvironmentGroup)->RemoveSoundReference();
	}

	replayDebugf2("Start sound %p", soundRef);
}


//---------------------------------------------------------
// Func: CPacketSoundCreate::ExtractAndPlay
// Desc: Recreates and plays a sound from stored data
//---------------------------------------------------------
bool CPacketSoundCreate::ExtractAndPlay( audSound **ppSound )
{
	(void)ppSound;
	//audWaveSlot*	pWaveSlot				= NULL;
	//bool			isExtractionSuccessful	= Extract( ppSound );

// 	if( !isExtractionSuccessful )
// 	{
// 		return false;
// 	}

	/*if( m_StoredInitParams.HasWaveSlot )
	{
		pWaveSlot = ReplaySound::ExtractWaveSlotPointer( m_CreateParams.GetContext() );
	}

	// Context Specific Settings
	switch( m_CreateParams.GetContext() )
	{
	case ReplaySound::CONTEXT_PED_MOBILE_RING:
		{
			(*ppSound)->SetRequestedDopplerFactor( 0.0f );
		}
		break;
	case ReplaySound::CONTEXT_SMASHABLE_SMASH:
		{
			//todo4five this needs changing now the slow mo system has changed
			//if( CReplayMgr::GetSlowMotionSetting() == REPLAY_SLOWMOTION_X4 || CReplayMgr::GetSlowMotionSetting() == REPLAY_SLOWMOTION_BULLET )
			//{
			//	for( int i=0; i<2; i++ )
			//	{
			//		if(g_audSlomoSmashableTable[i][0] == m_SoundHashOrOffset)
			//		{
			//			audSoundInitParams initParams;
			//			Vector3 pos(m_StoredInitParams.Position[0],m_StoredInitParams.Position[1],m_StoredInitParams.Position[2]);
			//			initParams.Position = pos;
			//			g_AmbientAudioEntity.CreateAndPlaySound(g_audSlomoSmashableTable[i][1],&initParams);
			//		}
			//	}
			//}
		}
		break;

	default:
		break;
	}

	(*ppSound)->PrepareAndPlay( pWaveSlot, m_StoredInitParams.AllowLoad, m_StoredInitParams.PrepareTimeLimit );
*/
	return true;
}

//////////////////////////////////////////////////////////////////////////
CPacketSoundPhoneRing::CPacketSoundPhoneRing(u32 soundSetHash, u32 soundHash, bool triggerRingtoneAsHUDSound, s32 ringToneTimeLimit, u32 startTime )
	: CPacketEventTracked(PACKETID_SOUND_PHONE_RING, sizeof(CPacketSoundPhoneRing))
	, CPacketEntityInfo()
{
	m_SoundSetHash = soundSetHash;
	m_SoundHash = soundHash;
	m_TriggerRingtoneAsHUDSound = triggerRingtoneAsHUDSound;
	m_RingToneTimeLimit = ringToneTimeLimit;
	m_StartTime = startTime;
}

/////////////////////////////////////////////////////////////////////////
void CPacketSoundPhoneRing::Extract(CTrackedEventInfo<tTrackedSoundType>& eventInfo) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	CPed* pPed = static_cast<CPed*>(eventInfo.pEntity[0]);
	
	//Make sure the sound is stopped
	if(eventInfo.m_pEffect[0].m_pSound)
	{
		return;
	}

	if(eventInfo.m_pEffect[0].m_slot == -1)
	{
		bool hadToStop = false;

		// Extra high priority for player peds as script will be requesting this
		const f32 priority = pPed->IsLocalPlayer()? 100.0f : 3.0f;
		eventInfo.m_pEffect[0].m_slot = g_SpeechManager.GetAmbientSpeechSlot(NULL, &hadToStop, priority);
		if(eventInfo.m_pEffect[0].m_slot == -1 || hadToStop)
		{
			return;
		}
		g_SpeechManager.PopulateAmbientSpeechSlotWithPlayingSpeechInfo(eventInfo.m_pEffect[0].m_slot, 0, 
#if __BANK
			"RINGTONE",
#endif
			0);
	}
	else if( !g_SpeechManager.IsSlotVacant(eventInfo.m_pEffect[0].m_slot))
	{
		g_SpeechManager.FreeAmbientSpeechSlot(eventInfo.m_pEffect[0].m_slot, true);
		eventInfo.m_pEffect[0].m_slot = -1;
		return;
	}

	audWaveSlot* waveSlot = g_SpeechManager.GetAmbientWaveSlotFromId(eventInfo.m_pEffect[0].m_slot);
	if (!waveSlot)
	{
		replayAssertf(0, "Got null waveslot trying to start ringtone sound with ID %i.", eventInfo.m_pEffect[0].m_slot);
		g_SpeechManager.FreeAmbientSpeechSlot(eventInfo.m_pEffect[0].m_slot, true);
		eventInfo.m_pEffect[0].m_slot = -1;
		return;
	}

	audSoundInitParams initParams;
	initParams.TrackEntityPosition = true;
	initParams.EnvironmentGroup = pPed->GetPedAudioEntity()->GetEnvironmentGroup(true);
	initParams.WaveSlot = waveSlot;
	initParams.StartOffset = m_StartTime; 

	if(m_TriggerRingtoneAsHUDSound)
	{
		initParams.Category = g_AudioEngine.GetCategoryManager().GetCategoryPtr(ATSTRINGHASH("HUD", 0x944FA9D9));
	}

	audMetadataRef soundMetaRef;
	soundMetaRef.SetInvalid();

	if(m_SoundSetHash != g_NullSoundHash)
	{
		audSoundSet soundset;
		soundset.Init(m_SoundSetHash);
		if(soundset.IsInitialised())
		{
			soundMetaRef = soundset.Find(m_SoundHash);
		}
	}
	else
	{
		soundMetaRef = g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectMetadataRefFromHash(m_SoundSetHash);
	}
	replayAssertf(soundMetaRef.IsValid(), "CPacketSoundCreatePersistant invalid soundMetaRef (soundset %u, sound %u)", m_SoundSetHash, m_SoundHash);

	pPed->GetPedAudioEntity()->CreateSound_PersistentReference(soundMetaRef, &(eventInfo.m_pEffect[0].m_pSound), &initParams);

	if (!eventInfo.m_pEffect[0].m_pSound)
	{
		g_SpeechManager.FreeAmbientSpeechSlot(eventInfo.m_pEffect[0].m_slot, true);
		eventInfo.m_pEffect[0].m_slot = -1;
		return;
	}

	eventInfo.m_pEffect[0].m_pSound->PrepareAndPlay(waveSlot, true, m_RingToneTimeLimit);
}

//////////////////////////////////////////////////////////////////////////
CPacketSoundStopPhoneRing::CPacketSoundStopPhoneRing()
	: CPacketEventTracked(PACKETID_SOUND_STOP_PHONE_RING, sizeof(CPacketSoundStopPhoneRing))
	, CPacketEntityInfo()
{
}

/////////////////////////////////////////////////////////////////////////
void CPacketSoundStopPhoneRing::Extract(CTrackedEventInfo<tTrackedSoundType>& eventInfo) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	if(eventInfo.m_pEffect[0].m_slot >= 0)
	{
		if(eventInfo.m_pEffect[0].m_pSound)
		{
			eventInfo.m_pEffect[0].m_pSound->StopAndForget();
		}

		g_SpeechManager.FreeAmbientSpeechSlot(eventInfo.m_pEffect[0].m_slot, true);
		eventInfo.m_pEffect[0].m_slot = -1;
	}
}

//---------------------------------------------------------
// Func: CPacketSoundUpdateAndPlay::Store
// Desc: Stores necessary data to replay a sound
//---------------------------------------------------------
CPacketSoundUpdateAndPlay::CPacketSoundUpdateAndPlay(s32 /*soundId*/, ReplaySound::CUpdateParams &updateParams)
	: CPacketEventTracked(PACKETID_SOUND_UPDATE_AND_PLAY, sizeof(CPacketSoundUpdateAndPlay))
{
	m_UpdateParams		= updateParams;

	// Store the Entity Type of the Sound Owner if There is One
	ReplaySound::StoreSoundOwnerEntityType( updateParams.GetOwnerReplayId(), m_OwnerEntityType );
}

//---------------------------------------------------------
// Func: CPacketSoundUpdateAndPlay::Extract
// Desc: Replays a sound using the stored data
//---------------------------------------------------------
void CPacketSoundUpdateAndPlay::Extract(const CEventInfo<void>&) const
{
	replayFatalAssertf(false, "CPacketSoundUpdateAndPlay::Extract Not implemented");
#if 0
 	audSound* pSound = GetSoundFromSoundId( GetReplayID() );
 
 	if( !pSound )
 	{
 		return;
 	}
 
 	// Set General Sound Settings
 	ReplaySound::ExtractSoundSettings( pSound, &m_UpdateParams );
 
 	// Set Context Specific Sound Settings
 	switch( m_UpdateParams.GetContext() )
 	{
 	case ReplaySound::CONTEXT_PED_RINGTONE:
 		{
 			pSound->SetVariableValueDownHierarchyFromName( "variation", m_UpdateParams.GetClientVariableF32() );
 			pSound->SetRequestedDopplerFactor( 0.0f );
 
 			// We used the client variable to pass in the m_MobilePhoneRingtoneId and
 			//  it was set as the sound's client variable in ExtractSoundSettings.
 			//  Undo the change since we are using the client variable for another
 			//  purpose here.
 			pSound->SetClientVariable( 0.0f );
 		}
 		break;
 
 	case ReplaySound::CONTEXT_VEHICLE_SMASH_SIREN:
 		{
 			CEntity *pEntity = GetEntity<CEntity>( REPLAYIDTYPE_PLAYBACK, m_UpdateParams.GetOwnerReplayId() );
 
 			if( !(pEntity->GetIsTypeVehicle()) )
 			{
 				replayAssertf( false, "CPacketSoundUpdateAndPlay::Extract: Non-vehicle attempting smash siren" );
 				return;
 			}
 
 			CVehicle *pVehicle = verify_cast<CVehicle*> (pEntity);
 			pVehicle->GetVehicleAudioEntity()->m_SirenState = AUD_SIREN_OFF;
 
 			pSound->FindAndSetVariableValue( ATSTRINGHASH("isFucked", 0x82DA7FEC), 1.f ); 
 		}
 		break;
 
 	default:
 		break;
 	}
 
 	// TODO: Pass in settings for prepare and play
 
 	pSound->PrepareAndPlay();
#endif
}



//////////////////////////////////////////////////////////////////////////
CPacketSoundUpdateSettings::CPacketSoundUpdateSettings(s32 soundId, ReplaySound::CUpdateParams &updateParams)
: CPacketEvent(PACKETID_SOUND_UPDATE_SETTINGS, sizeof(CPacketSoundUpdateSettings))
{
	m_HasPostSubmixVolumeAttenuationChanged	= false;
	m_HasVolumeChanged						= false;
	m_HasPositionChanged					= false;
	m_HasPitchChanged						= false;
	m_HasLowPassFilterCutoffChanged			= false;

	m_SoundID			= soundId;
	m_Context			= static_cast<u16> (updateParams.GetContext());
	
	if( updateParams.GetHasPostSubmixVolumeAttenuationChanged() )
	{
		m_PostSubmixVolumeAttenuation			= updateParams.GetPostSubmixVolumeAttenuation();
		m_HasPostSubmixVolumeAttenuationChanged	= true;
	}

	if( updateParams.GetHasVolumeChanged() )
	{
		m_Volume			= updateParams.GetVolume();
		m_HasVolumeChanged	= true;
	}

	if( updateParams.GetHasPositionChanged() )
	{
		m_Position[0]			= updateParams.GetPosition().x;
		m_Position[1]			= updateParams.GetPosition().y;
		m_Position[2]			= updateParams.GetPosition().z;
		m_HasPositionChanged	= true;
	}

	if( updateParams.GetHasPitchChanged() )
	{
		m_Pitch				= updateParams.GetPitch();
		m_HasPitchChanged	= true;
	}

	if( updateParams.GetHasLowPassFilterCutoffChanged() )
	{
		m_LowPassFilterCutoff			= updateParams.GetLowPassFilterCutoff();
		m_HasLowPassFilterCutoffChanged	= true;
	}
}

//---------------------------------------------------------
// Func: CPacketSoundUpdateSettings::Extract
// Desc: Replays a sound using the stored data
//---------------------------------------------------------
void CPacketSoundUpdateSettings::Extract(const CEventInfo<void>&) const
{
	/*TODO4FIVE audSound* pSound = CReplayMgr::GetSoundFromSoundId( GetSoundID() );

	if( !pSound || (pSound->GetReplayId() != GetSoundID()))
	{
		return;
	}

	// Set Requested Settings
	if( m_HasPostSubmixVolumeAttenuationChanged )
	{
		pSound->SetRequestedPostSubmixVolumeAttenuation( m_PostSubmixVolumeAttenuation );
	}

	if( m_HasVolumeChanged )
	{
		pSound->SetRequestedVolume(	m_Volume );
	}

	if( m_HasPositionChanged )
	{
		pSound->SetRequestedPosition( GetPosition() );
	}

	if( m_HasPitchChanged )
	{
		pSound->SetRequestedPitch( m_Pitch );
	}

	if( m_HasLowPassFilterCutoffChanged )
	{
		pSound->SetRequestedLPFCutoff( m_LowPassFilterCutoff );
	}*/
}

//---------------------------------------------------------
// Func: CPacketSoundUpdateLPFCutoff::Store
// Desc: Stores a low pass filter cutoff sound update
//---------------------------------------------------------
CPacketSoundUpdateLPFCutoff::CPacketSoundUpdateLPFCutoff(s32 /*soundId*/, ReplaySound::CUpdateParams &updateParams)
	: CPacketEventTracked(PACKETID_SOUND_LPF_CUTOFF, sizeof(CPacketSoundUpdateLPFCutoff))
	, CPacketEntityInfo()
{
	if( updateParams.GetHasLowPassFilterCutoffChanged() )
	{
		m_LowPassFilterCutoff = updateParams.GetLowPassFilterCutoff();
	}
}

//---------------------------------------------------------
// Func: CPacketSoundUpdateLPFCutoff::Extract
// Desc: Updates the low pass filter cutoff for a replay sound
//---------------------------------------------------------
void CPacketSoundUpdateLPFCutoff::Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

 	audSound*& pSound = pData.m_pEffect[0].m_pSound;
 
 	if( !pSound )
 	{
 		return;
 	}
 
 	pSound->SetRequestedLPFCutoff(	m_LowPassFilterCutoff );
}


//////////////////////////////////////////////////////////////////////////
CPacketSoundUpdatePitch::CPacketSoundUpdatePitch(const s32 pitch)
	: CPacketEventTracked(PACKETID_SOUND_UPDATE_PITCH, sizeof(CPacketSoundUpdatePitch))
	, CPacketEntityInfo()
{
	// In audRequestedSettings::SetPitch the pitch is casted to an s16
	m_pitch = (s16)pitch;
}


//////////////////////////////////////////////////////////////////////////
void CPacketSoundUpdatePitch::Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	audSound*& pSound = pData.m_pEffect[0].m_pSound;

	if(pSound != NULL)
	{
		pSound->SetRequestedPitch(m_pitch);
		//replayDebugf1("Set pitch %p", pSound);
		return;
	}
}

//---------------------------------------------------------
// Func: CPacketSoundUpdatePosition::Store
// Desc: Stores a sound position update
//---------------------------------------------------------
CPacketSoundUpdatePosition::CPacketSoundUpdatePosition(Vec3V_In position)
	: CPacketEventTracked(PACKETID_SOUND_POS, sizeof(CPacketSoundUpdatePosition))
	, CPacketEntityInfo()
	, m_Position(position)
{}

//---------------------------------------------------------
// Func: CPacketSoundUpdatePosition::Extract
// Desc: Updates the position for a replay sound
//---------------------------------------------------------
void CPacketSoundUpdatePosition::Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	audSound*& pSound = pData.m_pEffect[0].m_pSound;

	if( !pSound )
	{
		return;
	}

	Vector3 position = m_Position;
	pSound->SetRequestedPosition( position );
}

//---------------------------------------------------------
// Func: CPacketSoundUpdatePostSubmix::Store
// Desc: Stores a post submix volume attenuation update
//---------------------------------------------------------
CPacketSoundUpdatePostSubmix::CPacketSoundUpdatePostSubmix(s32 /*soundId*/, ReplaySound::CUpdateParams &updateParams)
	: CPacketEventTracked(PACKETID_SOUND_POST_SUBMIX, sizeof(CPacketSoundUpdatePostSubmix))
	, CPacketEntityInfo()
{
	if( updateParams.GetHasPostSubmixVolumeAttenuationChanged() )
	{
		m_PostSubmixVolumeAttenuation = updateParams.GetPostSubmixVolumeAttenuation();
	}
}

//---------------------------------------------------------
// Func: CPacketSoundUpdatePostSubmix::Extract
// Desc: Updates the post submix volume attenuation  for a
//        replay sound.
//---------------------------------------------------------
void CPacketSoundUpdatePostSubmix::Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	audSound*& pSound = pData.m_pEffect[0].m_pSound;

	if( !pSound )
	{
		return;
	}

	pSound->SetRequestedPostSubmixVolumeAttenuation( m_PostSubmixVolumeAttenuation );
}


//////////////////////////////////////////////////////////////////////////
CPacketSoundUpdateVolume::CPacketSoundUpdateVolume(const float volume) 
	: CPacketEventTracked(PACKETID_SOUND_UPDATE_VOLUME, sizeof(CPacketSoundUpdateVolume))
	, CPacketEntityInfo()
	, m_Volume(volume)
{}

//---------------------------------------------------------
// Func: CPacketSoundUpdateVolume::Extract
// Desc: Updates the volume for a replay sound
//---------------------------------------------------------
void CPacketSoundUpdateVolume::Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	audSound*& pSound = pData.m_pEffect[0].m_pSound;

	if(pSound != NULL)
	{
		pSound->SetRequestedVolume(	m_Volume );
		//replayDebugf1("Set volume %p", pSound);
		return;
	}
}

//---------------------------------------------------------
// Func: CPacketSoundUpdatePitchPos::Store
// Desc: Stores a sound pitch & position update
//---------------------------------------------------------
CPacketSoundUpdatePitchPos::CPacketSoundUpdatePitchPos(s32 /*soundId*/, ReplaySound::CUpdateParams &updateParams)
	: CPacketEventTracked(PACKETID_SOUND_PITCH_POS, sizeof(CPacketSoundUpdatePitchPos))
	, CPacketEntityInfo()
{
	if( updateParams.GetHasPitchChanged() )
	{
		m_Pitch	= updateParams.GetPitch();
	}

	if( updateParams.GetHasPositionChanged() )
	{
		m_Position[0] = updateParams.GetPosition().x;
		m_Position[1] = updateParams.GetPosition().y;
		m_Position[2] = updateParams.GetPosition().z;
	}
}

//---------------------------------------------------------
// Func: CPacketSoundUpdatePitchPos::Extract
// Desc: Updates the pitch & position for a replay sound
//---------------------------------------------------------
void CPacketSoundUpdatePitchPos::Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	audSound*& pSound = pData.m_pEffect[0].m_pSound;

	if( !pSound )
	{
		return;
	}

	Vector3 position;
	position.Set( m_Position[0], m_Position[1], m_Position[2] );
	
	pSound->SetRequestedPosition( position );
	pSound->SetRequestedPitch(	m_Pitch );
}

//---------------------------------------------------------
// Func: CPacketSoundUpdatePitchVol::Store
// Desc: Stores a sound pitch & volume update
//---------------------------------------------------------
CPacketSoundUpdatePitchVol::CPacketSoundUpdatePitchVol(s32 /*soundId*/, ReplaySound::CUpdateParams &updateParams)
	: CPacketEventTracked(PACKETID_SOUND_PITCH_VOL, sizeof(CPacketSoundUpdatePitchVol))
	, CPacketEntityInfo()
{
	if( updateParams.GetHasPitchChanged() )
	{
		m_Pitch = updateParams.GetPitch();
	}

	if( updateParams.GetHasVolumeChanged() )
	{
		m_Volume = updateParams.GetVolume();
	}
}

//---------------------------------------------------------
// Func: CPacketSoundUpdatePitchVol::Extract
// Desc: Updates the pitch & volume for a replay sound
//---------------------------------------------------------
void CPacketSoundUpdatePitchVol::Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	audSound*& pSound = pData.m_pEffect[0].m_pSound;
 
 	if( !pSound )
 	{
 		return;
 	}
 
 	pSound->SetRequestedPitch(	m_Pitch );
 	pSound->SetRequestedVolume( m_Volume );
}

//---------------------------------------------------------
// Func: CPacketSoundUpdatePosVol::Store
// Desc: Stores a sound position & volume update
//---------------------------------------------------------
CPacketSoundUpdatePosVol::CPacketSoundUpdatePosVol(s32 /*soundId*/, ReplaySound::CUpdateParams &updateParams)
	: CPacketEventTracked(PACKETID_SOUND_POS_VOL, sizeof(CPacketSoundUpdatePosVol))
	, CPacketEntityInfo()
{
	if( updateParams.GetHasPositionChanged() )
	{
		m_Position[0] = updateParams.GetPosition().x;
		m_Position[1] = updateParams.GetPosition().y;
		m_Position[2] = updateParams.GetPosition().z;
	}

	if( updateParams.GetHasVolumeChanged() )
	{
		m_Volume = updateParams.GetVolume();
	}
}

//---------------------------------------------------------
// Func: CPacketSoundUpdatePosVol::Extract
// Desc: Updates the position & volume for a replay sound
//---------------------------------------------------------
void CPacketSoundUpdatePosVol::Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	audSound*& pSound = pData.m_pEffect[0].m_pSound;

	if( !pSound )
	{
		return;
	}

	Vector3 position;
	position.Set( m_Position[0], m_Position[1], m_Position[2] );

	pSound->SetRequestedPosition( position );
	pSound->SetRequestedVolume( m_Volume );
}

//---------------------------------------------------------
// Func: CPacketSoundUpdatePitchPosVol::Store
// Desc: Stores a sound pitch, position & volume update
//---------------------------------------------------------
CPacketSoundUpdatePitchPosVol::CPacketSoundUpdatePitchPosVol(s32 /*soundId*/, ReplaySound::CUpdateParams &updateParams)
	: CPacketEventTracked(PACKETID_SOUND_PITCH_POS_VOL, sizeof(CPacketSoundUpdatePitchPosVol))
	, CPacketEntityInfo()
{
	if( updateParams.GetHasPitchChanged() )
	{
		m_Pitch = updateParams.GetPitch();
	}

	if( updateParams.GetHasPositionChanged() )
	{
		m_Position[0] = updateParams.GetPosition().x;
		m_Position[1] = updateParams.GetPosition().y;
		m_Position[2] = updateParams.GetPosition().z;
	}

	if( updateParams.GetHasVolumeChanged() )
	{
		m_Volume = updateParams.GetVolume();
	}
}

//---------------------------------------------------------
// Func: CPacketSoundUpdatePitchPosVol::Extract
// Desc: Updates the position & volume for a replay sound
//---------------------------------------------------------
void CPacketSoundUpdatePitchPosVol::Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	audSound*& pSound = pData.m_pEffect[0].m_pSound;

	if( !pSound )
	{
		return;
	}

	Vector3 position;
	position.Set( m_Position[0], m_Position[1], m_Position[2] );

	pSound->SetRequestedPitch(	m_Pitch );
	pSound->SetRequestedPosition( position );
	pSound->SetRequestedVolume( m_Volume );
}


//////////////////////////////////////////////////////////////////////////
// Set Doppler Packet
CPacketSoundUpdateDoppler::CPacketSoundUpdateDoppler(const float doppler)
	: CPacketEventTracked(PACKETID_SOUND_UPDATE_DOPPLER, sizeof(CPacketSoundUpdateDoppler))
	, CPacketEntityInfo()
	, m_doppler(doppler)
{
	replayDebugf1("CPacketSoundUpdateDoppler");
}

//////////////////////////////////////////////////////////////////////////
void CPacketSoundUpdateDoppler::Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	audSound*& pSound = pData.m_pEffect[0].m_pSound;

	if(pSound != NULL && FPIsFinite(m_doppler))
	{
		pSound->SetRequestedDopplerFactor(m_doppler);
		replayDebugf1("Set doppler %p", pSound);
		return;
	}
}


//////////////////////////////////////////////////////////////////////////
CPacketSoundSetValueDH::CPacketSoundSetValueDH(const u32 nameHash, const float value)
	: CPacketEventTracked(PACKETID_SOUND_SET_VALUE_DH, sizeof(CPacketSoundSetValueDH))
	, CPacketEntityInfo()
	, m_nameHash(nameHash)
	, m_value(value)
{
	replayDebugf1("CPacketSoundSetValueDH");
}

//////////////////////////////////////////////////////////////////////////
void CPacketSoundSetValueDH::Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	audSound*& pSound = pData.m_pEffect[0].m_pSound;

	if(pSound != NULL)
	{
		pSound->FindAndSetVariableValue(m_nameHash, m_value);
		replayDebugf1("Set value %p", pSound);
		return;
	}
}


//////////////////////////////////////////////////////////////////////////
CPacketSoundSetClientVar::CPacketSoundSetClientVar(u32 val)
	: CPacketEventTracked(PACKETID_SOUND_SET_CLIENT_VAR, sizeof(CPacketSoundSetClientVar))
	, CPacketEntityInfo()
	, m_valueU32(val)
{
	SetPadBool(IsFloatBit, false);	// Use u32
}

//////////////////////////////////////////////////////////////////////////
CPacketSoundSetClientVar::CPacketSoundSetClientVar(f32 val)
	: CPacketEventTracked(PACKETID_SOUND_SET_CLIENT_VAR, sizeof(CPacketSoundSetClientVar))
	, CPacketEntityInfo()
	, m_valueF32(val)
{
	SetPadBool(IsFloatBit, true);	// Use float
}

//////////////////////////////////////////////////////////////////////////
void CPacketSoundSetClientVar::Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	audSound*& pSound = pData.m_pEffect[0].m_pSound;

	if(pSound == NULL)
		return;

	if(GetPadBool(IsFloatBit))
		pSound->SetClientVariable(m_valueF32);
	else
		pSound->SetClientVariable(m_valueU32);
}

//////////////////////////////////////////////////////////////////////////
// Stop Sound Packet
CPacketSoundStop::CPacketSoundStop(bool forget)
	: CPacketEventTracked(PACKETID_SOUND_STOP, sizeof(CPacketSoundStop))
	, CPacketEntityInfo()
{
	SetPadBool(ForgetBit, forget);
}

//////////////////////////////////////////////////////////////////////////
void CPacketSoundStop::Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	for(u8 i = 0; i < 2; ++i)
	{
		audSound*& pSound = pData.m_pEffect[i].m_pSound;
		if(pSound)
		{
			replayDebugf2("Stop sound %p", pSound);
			if(GetPadBool(ForgetBit))
			{
				pSound->StopAndForget();
				pSound = NULL;
			}
			else
				pSound->Stop();
		}
	}
}


//////////////////////////////////////////////////////////////////////////
// Start Sound Packet
CPacketSoundStart::CPacketSoundStart()
	: CPacketEventTracked(PACKETID_SOUND_START, sizeof(CPacketSoundStart))
	, CPacketEntityInfo()
{}

//////////////////////////////////////////////////////////////////////////
void CPacketSoundStart::Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	audSound*& pSound = pData.m_pEffect[0].m_pSound;
	if(pSound != NULL)
	{
		pSound->PrepareAndPlay();
		replayDebugf1("Start sound %p", pSound);
		return;
	}

	replayAssertf(false, "No sound to start");
}



//////////////////////////////////////////////////////////////////////////
CPacketSoundWeaponEcho::CPacketSoundWeaponEcho(const u32 echoHash, const Vector3& position)
	: CPacketEvent(PACKETID_SOUND_WEAPON_ECHO, sizeof(CPacketSoundWeaponEcho))
	, CPacketEntityInfo()
	, m_echoSoundHash(echoHash)
{
	REPLAY_UNUSEAD_VAR(m_echoSoundHash);
	m_position.Store(position);
}

//////////////////////////////////////////////////////////////////////////
bool CPacketSoundWeaponEcho::Extract(const CEventInfo<void>&) const
{
	Vector3 position;
	m_position.Load(position);

	//g_WeaponAudioEntity.PlayTwoEchos(m_echoSoundHash, 0, position, 0.0f, 0);

	return true;
}


//---------------------------------------------------------
// Func: CPacketBreathSound::Store
// Desc: Stores necessary data to recreate breathing sounds
//---------------------------------------------------------
CPacketBreathSound::CPacketBreathSound(f32 breathRate)
	: CPacketEventTracked(PACKETID_SOUND_BREATH, sizeof(CPacketBreathSound))
	, CPacketEntityInfo()
{
	m_BreathRate	= breathRate;
}

//---------------------------------------------------------
// Func: CPacketBreathSound::Extract
// Desc: Recreate breathing sounds using the stored data
//---------------------------------------------------------
void CPacketBreathSound::Extract(const CEventInfo<CPed>& eventInfo) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	CPed*		pPed	= eventInfo.GetEntity();
	audSound*	pSound	= NULL;

	if(!pPed)
	{
		replayAssertf(false, "CPacketBreathSound::Extract: Non-ped entity attempting to play breath sound.");
		return;
	}

	audPedAudioEntity*	pAudioEntity = pPed->GetPedAudioEntity();
	const char*			soundName	 = pPed->IsMale() ? "PLAYER_BREATH" : "FEMALE_PLAYER_BREATH";
	Vector3				pos;
	audSoundInitParams	initParams;

	//TODO4FIVE initParams.OcclusionGroup = pPed->GetAudioOcclusionGroup();

	/*TODO4FIVE pAudioEntity->GetPositionForPedSound( AUD_MOUTH, pos );*/
	pAudioEntity->CreateSound_LocalReference(soundName, &pSound, &initParams);

	if(pSound)
	{
		pSound->FindAndSetVariableValue(ATSTRINGHASH("breathRate", 0xF03798D8), m_BreathRate);
		/*TODO4FIVE pSound->SetClientVariable( (u32)AUD_MOUTH );*/
		pSound->SetRequestedPosition(pos);
		pSound->SetUpdateEntity(true);
		pSound->PrepareAndPlay();
	}
}

//---------------------------------------------------------
// Func: CPacketAutomaticSound::Store
// Desc: Stores necessary data to recreate sounds
//---------------------------------------------------------
CPacketAutomaticSound::CPacketAutomaticSound(u32 context, s32 /*ownerReplayId*/)
	: CPacketEventTracked(PACKETID_SOUND_AUTOMATIC, sizeof(CPacketAutomaticSound))
	, CPacketEntityInfo()
{
	m_Context		= context;
}

//---------------------------------------------------------
// Func: CPacketAutomaticSound::Extract
// Desc: Recreates sounds by using the stored data
//---------------------------------------------------------
void CPacketAutomaticSound::Extract(const CEventInfo<void>&) const
{
	replayFatalAssertf(false, "Todo");
// 	CPed*					pPed				= NULL;
// 	CVehicle*				pVehicle			= NULL;
// 	audPedAudioEntity*		pPedAudioEntity		= NULL;
// 	audVehicleAudioEntity*	pVehicleAudioEntity	= NULL;
// 
// 	// Obtain the Required Audio Entity
// 	if( GetReplayID() >= 0 )
// 	{
// 		CEntity* pEntity = GetEntity<CEntity>(GetReplayID());
// 
// 		if( pEntity )
// 		{
// 			if( pEntity->GetIsTypePed() )
// 			{
// 				pPed				= reinterpret_cast<CPed*> (pEntity);
// 				pPedAudioEntity		= pPed->GetPedAudioEntity();
// 			}
// 			else if( pEntity->GetIsTypeVehicle() )
// 			{
// 				pVehicle			= reinterpret_cast<CVehicle*> (pEntity);
// 				pVehicleAudioEntity	= pVehicle->GetVehicleAudioEntity();
// 			}
// 		}
// 	}
// 
// 	if( !pPedAudioEntity && !pVehicleAudioEntity )
// 	{
// 		replayAssertf( false, "CPacketAutomaticSound::Extract: No Ped or Vehicle Owner Audio Entity for this Sound." );
// 		return;
// 	}
// 
// 	// Call the Appropriate Function that Plays the Sound
// 	switch( m_Context )
// 	{
// 	case ReplaySound::CONTEXT_VEHICLE_BLIP_HORN:
// 		{
// 			/*TODO4FIVE pVehicleAudioEntity->BlipHorn();*/
// 		}
// 		break;
// 
// 	default:
// 		break;
// 	}
}

//---------------------------------------------------------
// Func: CPacketFireInitSound::Store
// Desc: Stores necessary data to start a fire sound
//---------------------------------------------------------
CPacketFireInitSound::CPacketFireInitSound(s32 mainFireReplayId, s32 fireStartReplayId, s32 entityOnFireReplayId, audSoundInitParams &initParams)
	: CPacketEventTracked(PACKETID_SOUND_FIRE_INIT, sizeof(CPacketFireInitSound))
	, CPacketEntityInfo()
{
	m_MainFireReplayId		= mainFireReplayId;
	m_FireStartReplayId		= fireStartReplayId;
	m_EntityOnFireReplayId	= entityOnFireReplayId;
	SetPadU8(StartOffsetU8, static_cast<u8> (initParams.StartOffset));

	m_Position[0]	= initParams.Position.x;
	m_Position[1]	= initParams.Position.y;
	m_Position[2]	= initParams.Position.z;
}

//---------------------------------------------------------
// Func: CPacketFireInitSound::Extract
// Desc: Recreates sounds by using the stored data
//---------------------------------------------------------
void CPacketFireInitSound::Extract( bool /*shouldSkipStart*/ ) const
{
	replayFatalAssertf(false, "CPacketFireInitSound::Extract Not Implemented");
#if 0
 	CEntity*			pEntity = NULL;
 	audSoundInitParams	initParams;
 
 	// Set the Occlusion Group
 	if( m_EntityOnFireReplayId >= 0 )
 	{
 		pEntity = GetEntiyAsEntity( REPLAYIDTYPE_PLAYBACK, m_EntityOnFireReplayId );
 
 		if( pEntity )
 		{
 			//TODO4FIVE initParams.OcclusionGroup = pEntity->GetAudioOcclusionGroup();
 		}
 	}
 
 	initParams.StartOffset				= (s32)GetPadU8(StartOffsetU8);
 	initParams.IsStartOffsetPercentage	= true;
 
 	Vector3 position( m_Position[0], m_Position[1], m_Position[2] );
 	initParams.Position	= position;
 
 	// Main Fire Sound
 	ReplaySound::CreatePlayAndBindFireSound( m_MainFireReplayId, ATSTRINGHASH("GENERAL_GROUND_FIRE", 0x8C16A1EB), initParams );
 
 	// Fire Start Sound
 	if( !shouldSkipStart && m_FireStartReplayId >= 0 )
 	{
 		audSound*	pFireStartSound = NULL;
 		bool*		pIsSoundIdTaken = GetPointerToIsSoundIdTaken( m_FireStartReplayId );
 		void*		pSoundHandle	= GetPointerToSoundHandle( m_FireStartReplayId );
 
 		if( pIsSoundIdTaken == NULL || pSoundHandle == NULL )
 		{
 			replayAssertf( false, "CPacketFireInitSound::Extract: Could not obtain pointer to isSoundIdTaken or sound handle." );
 			return;
 		}
 
 
 		GetAudioEntity().CreateSound_LocalReference( "FIRE_START", &pFireStartSound, &initParams );
 
 		if( pFireStartSound )
 		{
 			pFireStartSound->SetReplayId( m_FireStartReplayId, pIsSoundIdTaken, pSoundHandle );
 		}
 
 		if( pFireStartSound )
 		{
 			pFireStartSound->PrepareAndPlay();
 			BindSoundToSoundId( m_FireStartReplayId, pFireStartSound );
 		}
 		else
 		{
 			replayAssertf( false, "CPacketFireInitSound::Extract: Could not create fire start sound" );
 		}
 	}
#endif
}

//---------------------------------------------------------
// Func: CPacketFireVehicleUpdateSound::Store
// Desc: Stores necessary data to update a vehicle fire sound
//---------------------------------------------------------
void CPacketFireVehicleUpdateSound::Store( s32 mainFireReplayId,	s32 windFireReplayId,	s32 closeFireReplayId,
										   s32 vehicleReplayId,		s32 startOffset									)
{
	CPacketBase::Store(PACKETID_SOUND_FIRE_VEHICLE_UPDATE, sizeof(CPacketFireVehicleUpdateSound));

	 m_GameTime				= fwTimer::GetReplayTimeInMilliseconds();
	m_MainFireReplayId		= mainFireReplayId;
	m_WindFireReplayId		= windFireReplayId;
	m_CloseFireReplayId		= closeFireReplayId;
	m_VehicleReplayId		= vehicleReplayId;
	SetPadU8(StartOffsetU8, static_cast<u8> (startOffset));
}

//---------------------------------------------------------
// Func: CPacketFireVehicleUpdateSound::Extract
// Desc: Updates the vehicle fire sounds
//---------------------------------------------------------
void CPacketFireVehicleUpdateSound::Extract( bool /*shouldSkipStart*/ ) const
{
	replayFatalAssertf(false, "CPacketFireVehicleUpdateSound::Extract Not Implemented");
#if 0
 	CVehicle* pVehicle = NULL;
 
 	if( m_VehicleReplayId >= 0 )
 	{
 		CEntity* pEntity = GetEntity<CEntity>( REPLAYIDTYPE_PLAYBACK, m_VehicleReplayId );
 
 		if( pEntity && pEntity->GetIsTypeVehicle() )
 		{
 			pVehicle = reinterpret_cast<CVehicle*> (pEntity);
 		}
 		else
 		{
 			replayAssertf( false, "CPacketFireVehicleUpdateSound::Extract: Could not obtain vehicle pointer" );
 			return;
 		}
 	}
 	else
 	{
 		replayAssertf( false, "CPacketFireVehicleUpdateSound::Extract: Invalid vehicle ID" );
 		return;
 	}
 
 	audSoundInitParams	initParams;
 	initParams.Position	= VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
 		
 	//TODO4FIVE initParams.OcclusionGroup			= pVehicle->GetAudioOcclusionGroup();
 	initParams.StartOffset				= (s32)GetPadU8(StartOffsetU8);
 	initParams.IsStartOffsetPercentage	= true;
 
 	ReplaySound::CreatePlayAndBindFireSound( m_MainFireReplayId,  ATSTRINGHASH("MAIN_FIRE_LOOP", 0x8EA83EA6),  initParams );
 	ReplaySound::CreatePlayAndBindFireSound( m_WindFireReplayId,  ATSTRINGHASH("WIND_FIRE_LOOP", 0x7CB02B6D),  initParams );
 	ReplaySound::CreatePlayAndBindFireSound( m_CloseFireReplayId, ATSTRINGHASH("CLOSE_FIRE_LOOP", 0xD73B126B), initParams );
 
 	if( !shouldSkipStart )
 	{
 		GetAudioEntity().CreateAndPlaySound( "FIRE_START", &initParams );
 	}
#endif
}

//---------------------------------------------------------
// Func: CPacketFireUpdateSound::Store
// Desc: Stores necessary data to update a fire sound
//---------------------------------------------------------
void CPacketFireUpdateSound::Store( s32 mainFireReplayId,	s32 windFireReplayId,	s32 closeFireReplayId,
								   f32 strengthVolume,		f32 windSpeedVolume,	Vector3 &position		)
{
	CPacketBase::Store(PACKETID_SOUND_FIRE_UPDATE, sizeof(CPacketFireUpdateSound));

	m_GameTime				= fwTimer::GetReplayTimeInMilliseconds();
	m_MainFireReplayId		= mainFireReplayId;
	m_WindFireReplayId		= windFireReplayId;
	m_CloseFireReplayId		= closeFireReplayId;
	m_StrengthVolume		= strengthVolume;
	m_WindSpeedVolume		= windSpeedVolume;

	m_Position[0] = position.x;
	m_Position[1] = position.y;
	m_Position[2] = position.z;
}

//---------------------------------------------------------
// Func: CPacketFireUpdateSound::Extract
// Desc: Updates a fire sound
//---------------------------------------------------------
void CPacketFireUpdateSound::Extract(const CEventInfo<void>&) const
{
	replayFatalAssertf(false, "CPacketFireUpdateSound::Extract Not Implemented");
#if 0
 	if( GetMainFireReplayId() >= 0 || GetWindFireReplayId() >= 0 || GetCloseFireReplayId() >= 0 )
 	{
 		audSound* pMainFireSound	= NULL;
 		audSound* pWindFireSound	= NULL;
 		audSound* pCloseFireSound	= NULL;
 
 		if( m_MainFireReplayId	>= 0 )	pMainFireSound	= GetSoundFromSoundId( m_MainFireReplayId );
 		if( m_WindFireReplayId	>= 0 )	pWindFireSound	= GetSoundFromSoundId( m_WindFireReplayId );
 		if( m_CloseFireReplayId	>= 0 )	pCloseFireSound	= GetSoundFromSoundId( m_CloseFireReplayId );
 
 		Vector3 position( m_Position[0], m_Position[1], m_Position[2] );
 
 		if( pMainFireSound )
 		{
 			pMainFireSound->SetRequestedPosition( position );
 			pMainFireSound->SetRequestedVolume( m_StrengthVolume );
 		}
 
 		if( pWindFireSound )
 		{
 			pWindFireSound->SetRequestedPosition( position );
 			pWindFireSound->SetRequestedVolume( m_StrengthVolume + m_WindSpeedVolume );
 		}
 
 		if( pCloseFireSound )
 		{
 			pCloseFireSound->SetRequestedPosition( position );
 			pCloseFireSound->SetRequestedVolume( m_StrengthVolume );
 		}
 	}
#endif
}


//////////////////////////////////////////////////////////////////////////
// Vehicle horn packet
tPacketVersion gPacketVehicleHornPacketVersion_v1 = 1;
CPacketVehicleHorn::CPacketVehicleHorn(u32 hornType, s16 hornIndex, f32 holdTime)
	: CPacketEvent(PACKETID_SOUND_VEHICLE_HORN, sizeof(CPacketVehicleHorn),gPacketVehicleHornPacketVersion_v1)
	, CPacketEntityInfo()
	, m_HornType(hornType)
	, m_HornIndex(hornIndex)
	, m_HoldTime(holdTime)
{}


//////////////////////////////////////////////////////////////////////////
void CPacketVehicleHorn::Extract(const CEventInfo<CVehicle>& eventInfo) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	CVehicle* pVehicle = eventInfo.GetEntity();
	if(!pVehicle)
		return;

	pVehicle->GetVehicleAudioEntity()->SetHornType(m_HornType);
	if( GetPacketVersion() >= gPacketVehicleHornPacketVersion_v1 )
	{
		pVehicle->GetVehicleAudioEntity()->SetHornSoundIdx(m_HornIndex);
	}
	pVehicle->GetVehicleAudioEntity()->PlayVehicleHorn(m_HoldTime);
}


//////////////////////////////////////////////////////////////////////////
// Vehicle horn stop packet
CPacketVehicleHornStop::CPacketVehicleHornStop()
	: CPacketEvent(PACKETID_SOUND_VEHICLE_HORN_STOP, sizeof(CPacketVehicleHornStop))
	, CPacketEntityInfo()
{}


//////////////////////////////////////////////////////////////////////////
void CPacketVehicleHornStop::Extract(const CEventInfo<CVehicle>& eventInfo) const
{
	CVehicle* pVehicle = eventInfo.GetEntity();
	if(!pVehicle)
		return;

	pVehicle->GetVehicleAudioEntity()->StopHorn();
}

//////////////////////////////////////////////////////////////////////////
CPacketPresuckSound::CPacketPresuckSound(const u32 presuckSoundHash, const u32 presuckPreemptTime, const u32 dynamicSceneHash)
	: CPacketEvent(PACKETID_SOUND_PRESUCK, sizeof(CPacketPresuckSound))
	, CPacketEntityInfoStaticAware()
{
	m_PresuckSoundHash = presuckSoundHash;
	m_PresuckPreemptTime = presuckPreemptTime;
	replayAssertf(m_PresuckPreemptTime < REPLAY_PRELOAD_TIME, "m_PresuckPreemptTime has to be less than %i seconds", REPLAY_PRELOAD_TIME);	
	m_DynamicSceneHash = dynamicSceneHash;
	m_AudioScene = NULL;
}


//////////////////////////////////////////////////////////////////////////
void CPacketPresuckSound::Extract(const CEventInfo<CEntity>&) const
{
	if(m_AudioScene)
	{
		m_AudioScene->Stop();
		m_AudioScene = NULL;
	}
}


//////////////////////////////////////////////////////////////////////////
ePreplayResult CPacketPresuckSound::Preplay(const CEventInfo<CEntity>& eventInfo) const
{
	if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_FWD && eventInfo.GetPlaybackFlags().GetPlaybackSpeed() < 1.0f)
	{
		// Now check we have an entity...
		CEntity* pEntity = eventInfo.GetEntity();
		if(!pEntity)
		{
			return PREPLAY_MISSING_ENTITY;	// Missing entity...wait
		}

		audSoundInitParams initParams;
		initParams.Tracker = pEntity->GetPlaceableTracker();
		naAudioEntity* pAudioEntity = pEntity->GetAudioEntity();

		if(replayVerifyf(pAudioEntity != NULL, "CPacketPresuckSound attempting to play sound %d on entity %s with no audio entity", m_PresuckSoundHash, pEntity->GetModelName()))
		{
			replayDebugf1("PreSuck playing at %u, packet at %u", eventInfo.GetReplayTime(), GetGameTime());
			pAudioEntity->CreateAndPlaySound(m_PresuckSoundHash, &initParams);
			 
			MixerScene * sceneSettings  = DYNAMICMIXMGR.GetObject<MixerScene>(m_DynamicSceneHash); 
			if(sceneSettings)
			{

				DYNAMICMIXER.StartScene(sceneSettings, &m_AudioScene);
			}
		}
		else
		{
			return PREPLAY_FAIL;		// Failed for some other reason
		}
	}
	else
	{
		return PREPLAY_WAIT_FOR_TIME;	// Time was right but playback state wasn't...
	}

	return PREPLAY_SUCCESSFUL; 
}


//////////////////////////////////////////////////////////////////////////
void CPacketPresuckSound::PrintXML(FileHandle handle) const
{
	CPacketEvent::PrintXML(handle);	
	CPacketEntityInfoStaticAware::PrintXML(handle);

	char str[1024];
	snprintf(str, 1024, "\t\t<m_PresuckPreemptTime>%u</m_PresuckPreemptTime>\n", m_PresuckPreemptTime);
	CFileMgr::Write(handle, str, istrlen(str));

	snprintf(str, 1024, "\t\t<m_PresuckSoundHash>%u</m_PresuckSoundHash>\n", m_PresuckSoundHash);
	CFileMgr::Write(handle, str, istrlen(str));

	snprintf(str, 1024, "\t\t<m_DynamicSceneHash>%u</m_DynamicSceneHash>\n", m_DynamicSceneHash);
	CFileMgr::Write(handle, str, istrlen(str));
}


//////////////////////////////////////////////////////////////////////////
CPacketPlayStreamFromEntity::CPacketPlayStreamFromEntity(const char* currentStreamName, const u32 currentStartOffset, const char* currentSetName)
	: CPacketEventTracked(PACKETID_SND_PLAYSTREAMFROMENTITY, sizeof(CPacketPlayStreamFromEntity))
	, CPacketEntityInfo()
	, m_StartOffset(currentStartOffset)

{
	safecpy(m_CurrentStreamName, currentStreamName);
	safecpy(m_SetName, currentSetName);
}

void CPacketPlayStreamFromEntity::Extract(const CTrackedEventInfo<tTrackedSoundType>& eventInfo) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	CEntity* pEntity = eventInfo.pEntity[0];
	if(pEntity && g_ScriptAudioEntity.IsScriptStreamPrepared())
	{
		g_ScriptAudioEntity.PlayStreamFromEntityInternal(pEntity);
	}
}

ePreloadResult CPacketPlayStreamFromEntity::Preload(const CTrackedEventInfo<tTrackedSoundType>&) const
{	
	//Check we've got a setname set, this can be NULL
	const char* setName = NULL;
	if( strlen(m_SetName) != 0 )
		setName = m_SetName;

	if(CReplayMgr::IsCursorFastForwarding() || CReplayMgr::IsCursorRewinding() || CReplayMgr::IsScrubbing())
		return PRELOAD_SUCCESSFUL; 

	if( g_ScriptAudioEntity.IsStreamSoundValid() && (g_ScriptAudioEntity.IsScriptStreamPrepared() || g_ScriptAudioEntity.IsScriptStreamPlaying()) )
		return PRELOAD_SUCCESSFUL;

	if( g_ScriptAudioEntity.PreloadStreamInternal(m_CurrentStreamName, m_StartOffset, setName) )
		return PRELOAD_SUCCESSFUL;

	return PRELOAD_FAIL; 
}

//////////////////////////////////////////////////////////////////////////
CPacketPlayStreamFromPosition::CPacketPlayStreamFromPosition(const Vector3 &pos, const char* currentStreamName, const u32 currentStartOffset, const char* currentSetName)
	: CPacketEventTracked(PACKETID_SND_PLAYSTREAMFROMPOSITION, sizeof(CPacketPlayStreamFromPosition))
	, CPacketEntityInfo()
	, m_StartOffset(currentStartOffset)

{
	safecpy(m_CurrentStreamName, currentStreamName);
	safecpy(m_SetName, currentSetName);
	m_Pos = pos;
}

void CPacketPlayStreamFromPosition::Extract(const CTrackedEventInfo<tTrackedSoundType>&) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	if( g_ScriptAudioEntity.IsScriptStreamPrepared())
		g_ScriptAudioEntity.PlayStreamFromPositionInternal(m_Pos);
}

ePreloadResult CPacketPlayStreamFromPosition::Preload(const CTrackedEventInfo<tTrackedSoundType>&) const
{	
	//Check we've got a setname set, this can be NULL
	const char* setName = NULL;
	if( strlen(m_SetName) != 0 )
		setName = m_SetName;

	if(CReplayMgr::IsCursorFastForwarding() || CReplayMgr::IsCursorRewinding() || CReplayMgr::IsScrubbing())
		return PRELOAD_SUCCESSFUL; 

	if( g_ScriptAudioEntity.IsStreamSoundValid() && (g_ScriptAudioEntity.IsScriptStreamPrepared() || g_ScriptAudioEntity.IsScriptStreamPlaying()) )
		return PRELOAD_SUCCESSFUL;

	if( g_ScriptAudioEntity.PreloadStreamInternal(m_CurrentStreamName, m_StartOffset, setName) )
		return PRELOAD_SUCCESSFUL;

	return PRELOAD_FAIL; 
}

//////////////////////////////////////////////////////////////////////////
CPacketPlayStreamFrontend::CPacketPlayStreamFrontend(const char* currentStreamName, const u32 currentStartOffset, const char* currentSetName)
	: CPacketEventTracked(PACKETID_SND_PLAYSTREAMFRONTED, sizeof(CPacketPlayStreamFrontend))
	, CPacketEntityInfo()
	, m_StartOffset(currentStartOffset)

{
	safecpy(m_CurrentStreamName, currentStreamName);
	safecpy(m_SetName, currentSetName);
}

void CPacketPlayStreamFrontend::Extract(const CTrackedEventInfo<tTrackedSoundType>&) const
{
	if( g_ScriptAudioEntity.IsScriptStreamPrepared())
		g_ScriptAudioEntity.PlayStreamFrontendInternal();
}

// bool CPacketPlayStreamFrontend::HasExpired() const
// {
// 	return !g_ScriptAudioEntity.IsPlayingStream();
// }

ePreloadResult CPacketPlayStreamFrontend::Preload(const CTrackedEventInfo<tTrackedSoundType>&) const
{	
	//Check we've got a setname set, this can be NULL
	const char* setName = NULL;
	if( strlen(m_SetName) != 0 )
		setName = m_SetName;

	if(CReplayMgr::IsCursorFastForwarding() || CReplayMgr::IsCursorRewinding() || CReplayMgr::IsScrubbing())
		return PRELOAD_SUCCESSFUL; 

	//Displayf("Preload %s %s %u", m_CurrentStreamName, m_SetName, m_StartOffset);

	if( g_ScriptAudioEntity.IsStreamSoundValid() && (g_ScriptAudioEntity.IsScriptStreamPrepared() || g_ScriptAudioEntity.IsScriptStreamPlaying()) )
		return PRELOAD_SUCCESSFUL;

	if( g_ScriptAudioEntity.PreloadStreamInternal(m_CurrentStreamName, m_StartOffset, setName) )
		return PRELOAD_SUCCESSFUL;

	return PRELOAD_FAIL; 
}

//////////////////////////////////////////////////////////////////////////
CPacketStopStream::CPacketStopStream()
	: CPacketEvent(PACKETID_SND_STOPSTREAM, sizeof(CPacketStopStream))
	, CPacketEntityInfo()

{
}

void CPacketStopStream::Extract(const CEventInfo<void>&) const
{
	g_ScriptAudioEntity.StopStream();
}


const u32 ReplaySound::MAX_CONTEXT_NAME_LENGTH	= 42;
atMap<int, ReplaySound::CSoundRegister>	ReplaySound::SoundsRegister;

//---------------------------------------------------------
// Func: ReplaySound::CInitParams::Populate
// Desc: Stores the necessary info from the passed in init params.
//---------------------------------------------------------
void ReplaySound::CInitParams::Populate( const audSoundInitParams &originalInitParams, eContext originalContext )
{
	Position.Store(originalInitParams.Position);
	HasPosition				= true;

	Volume					= originalInitParams.Volume;
	PostSubmixAttenuation	= originalInitParams.PostSubmixAttenuation;
	Pitch					= originalInitParams.Pitch;
	Pan						= originalInitParams.Pan;
	VolumeCurveScale		= originalInitParams.VolumeCurveScale;
	PrepareTimeLimit		= originalInitParams.PrepareTimeLimit;

	StartOffset				= originalInitParams.StartOffset;
	Predelay				= originalInitParams.Predelay;
	AttackTime				= originalInitParams.AttackTime;

	u32ClientVar			= originalInitParams.u32ClientVar;
	
	TimerId					= originalInitParams.TimerId;
	LPFCutoff				= originalInitParams.LPFCutoff;
	VirtualisationScoreOffset	= originalInitParams.VirtualisationScoreOffset;
	ShadowPcmSourceId		= originalInitParams.ShadowPcmSourceId;
	PcmSourceChannelId		= originalInitParams.PcmSourceChannelId;
	SourceEffectSubmixId	= originalInitParams.SourceEffectSubmixId;	
	BucketId				= originalInitParams.BucketId;
	EffectRoute				= originalInitParams.EffectRoute;
	SpeakerMask				= originalInitParams.SpeakerMask;

	PrimaryShadowSound		= originalInitParams.PrimaryShadowSound;
	IsStartOffsetPercentage	= originalInitParams.IsStartOffsetPercentage;
	UpdateEntity			= originalInitParams.UpdateEntity;
	AllowOrphaned			= originalInitParams.AllowOrphaned;
	AllowLoad				= originalInitParams.AllowLoad;
	RemoveHierarchy			= originalInitParams.RemoveHierarchy;
	ShouldPlayPhysically	= originalInitParams.ShouldPlayPhysically;
	IsAuditioning			= originalInitParams.IsAuditioning;

	HasTracker				= originalInitParams.Tracker		? true : false;
	//HasEffectChain			= originalInitParams.EffectChain	? true : false;
	
	Context					= originalContext;
	HasWaveSlot				= originalInitParams.WaveSlot		? true : false;
		
	HasCategory				= originalInitParams.Category		? true : false;
	HasEnvironmentGroup		= originalInitParams.EnvironmentGroup	? true : false;
	//HasShadowSound			= originalInitParams.ShadowSound	? true : false;
}

//---------------------------------------------------------
// Func: ReplaySound::StoreSoundOwnerEntityType
// Desc: Stores the entity type of the sound owner in the
//       u8 reference (ownerEntityType) that is passed in.
//---------------------------------------------------------
void ReplaySound::StoreSoundOwnerEntityType( s32 ownerReplayID, u8 &ownerEntityType )
{
	// Find Out and Store Sound Owner Entity Type
	ownerEntityType = static_cast<u8> (ENTITY_TYPE_NOTHING);

	if( ownerReplayID >= 0 )
	{
		CEntity *pEntity = CReplayMgrInternal::GetEntityAsEntity(ownerReplayID);

		if( pEntity )
		{
			if( pEntity->GetIsTypeVehicle() )
			{
				ownerEntityType = static_cast<u8> (ENTITY_TYPE_VEHICLE);
			}
			else if( pEntity->GetIsTypePed() )
			{
				ownerEntityType = static_cast<u8> (ENTITY_TYPE_PED);
			}
			else if( pEntity->GetIsTypeObject() )
			{
				ownerEntityType = static_cast<u8> (ENTITY_TYPE_OBJECT);
			}
		}
	}
}

//---------------------------------------------------------
// Func: ReplaySound::CreateOcclusionGroupForExplosion
// Desc: Updates the settings for a sound
//---------------------------------------------------------
/*TODO4FIVE audOcclusionGroupInterface* ReplaySound::CreateOcclusionGroupForExplosion( CEntity *pEntity, const Vector3 *pPosition )
{
	audOcclusionGroupInterface* pOcclusionGroup = audGtaOcclusionGroup::AllocateGtaOcclusionGroup();

	if( pOcclusionGroup )
	{
		audGtaOcclusionGroup *pGtaOcclusionGroup = dynamic_cast<audGtaOcclusionGroup*> (pOcclusionGroup);

		// Init(Null...) sets no audEntity on the occlusionGroup - as we're on the same thread as the audController,
		// and we're about to kick off a sound, this is fine, and it won't be instantly deleted. 
		pGtaOcclusionGroup->Init( NULL, 20, 0 );

		replayAssertf( pPosition != NULL, "ReplaySound::CreateOcclusionGroupForExplosion: Invalid Position" );

		Matrix34 source;
		source.a = Vector3( 1.0f, 0.0f, 0.0f );
		source.b = Vector3( 0.0f, 1.0f, 0.0f );
		source.c = Vector3( 0.0f, 0.0f, 1.0f );
		source.d = *pPosition;

		pGtaOcclusionGroup->SetSource( source );
		pOcclusionGroup->FullyUpdate( CTimer::GetReplayTimeInMilliseconds() );
		pGtaOcclusionGroup->ForceSourceEnvironmentUpdate( pEntity );
	}

	return pOcclusionGroup;
}*/

//---------------------------------------------------------
// Func: ReplaySound::ExtractSoundInitParams
// Desc: Extracts the storedInitParams for recreating the
//       sound and sets up the passed in initParams.
//---------------------------------------------------------
bool ReplaySound::ExtractSoundInitParams( audSoundInitParams &initParams, const CInitParams &storedInitParams, CEntity *pEntity, const CCreateParams *createParams, Vector3 *pPosition, u32 UNUSED_PARAM(painVoiceType), u8 soundEntity, bool* createdEnvironmentGroup)
{
	(void)createParams;
	// TODO: Might need to capture audGtaAudioEngine changes (i.e. RegisterLoudSound)

	/*TODO4FIVE CEntity *pEntity = NULL;*/

	// Update Pointers for the Replay Entities
	//  We only do this if they pointed to objects for the original entities.
	//  We check this by seeing if the pointers are in their default NULL state.

	if( storedInitParams.HasPosition )
	{
		Vector3 pos;
		storedInitParams.Position.Load(pos);
		pPosition->Set(pos);
		initParams.Position = pos;
	}

	/*TODO4FIVE if( createParams->GetOwnerReplayId() == CReplayMgr::NO_OWNER_FOR_SOUND )
	{
		if( storedInitParams.HasTracker )
		{
			replayAssertf( false, "ReplaySound::ExtractSoundInitParams: Tracker but no Sound Owner" );
			return false; b
		}

		if( storedInitParams.HasOcclusionGroup )
		{
			if( createParams->GetContext() == ReplaySound::CONTEXT_EXPLOSION_ENTITY_OTHER			||
				createParams->GetContext() == ReplaySound::CONTEXT_EXPLOSION_VEHICLE_ENTITY_OTHER	||
				createParams->GetContext() == ReplaySound::CONTEXT_EXPLOSION_WATER_ENTITY_OTHER		)
			{
				initParams.OcclusionGroup = ReplaySound::CreateOcclusionGroupForExplosion( pEntity, initParams.Position );
			}
			else
			{
				replayAssertf( false, "ReplaySound::ExtractSoundInitParams: Occlusion Group but no Sound Owner" );
				return false;
			}
		}
	}
	else // Sound Has Owner
	{
		if( storedInitParams.HasTracker || storedInitParams.HasOcclusionGroup )
		{
			pEntity = reinterpret_cast<CEntity*> (CReplayMgr::GetEntity( REPLAYIDTYPE_PLAYBACK, createParams->GetOwnerReplayId() ));

			if( !pEntity )
			{
				// Owner of the Sound was Deleted
				return false;
			}

			// Obtain Tracker if it Exists
			if( storedInitParams.HasTracker )
			{
				if( pEntity->GetIsTypeObject() )
				{
					initParams.Tracker = ((CObject*)pEntity)->GetPlaceableTracker();
				}
				else
				{
					initParams.Tracker = pEntity->GetPlaceableTracker();
				}
			}

			// Obtain Occlusion Group if it Exists
			if( storedInitParams.HasOcclusionGroup )
			{
				// Note: Only Peds and Vehicles Have Audio Occlusion Groups (as of June 26/2008)

				if( pEntity->GetIsTypePed() )
				{
					CPed *pPed = reinterpret_cast<CPed*> (pEntity);

					if( createParams->GetContext() == ReplaySound::CONTEXT_WEAPON_FIRE )
					{
						initParams.OcclusionGroup = pPed->GetWeaponOcclusionGroup();
					}
					else
					{
						initParams.OcclusionGroup = pPed->GetPedAudioEntity()->GetOcclusionGroup();
					}
				}
				else if( pEntity->GetIsTypeVehicle() )
				{
					initParams.OcclusionGroup = pEntity->GetAudioOcclusionGroup();
				}
				else // Other Entities
				{
					if( createParams->GetContext() == ReplaySound::CONTEXT_EXPLOSION_ENTITY_OTHER			||
						createParams->GetContext() == ReplaySound::CONTEXT_EXPLOSION_VEHICLE_ENTITY_OTHER	||
						createParams->GetContext() == ReplaySound::CONTEXT_EXPLOSION_WATER_ENTITY_OTHER		)
					{
						initParams.OcclusionGroup = ReplaySound::CreateOcclusionGroupForExplosion( pEntity, initParams.Position );
					}
					else if( createParams->GetContext() == ReplaySound::CONTEXT_PLANES_AMBIENT_JET		||
							 createParams->GetContext() == ReplaySound::CONTEXT_PLANES_AMBIENT_JET_REAR	||
							 createParams->GetContext() == ReplaySound::CONTEXT_PLANES_AMBIENT_JET_TAXI		)
					{
						// TODO: See how this sounds first, if it's bad, create an occlusion group
						initParams.OcclusionGroup = NULL;
					}
					else
					{
						replayAssertf( false, "ReplaySound::ExtractSoundInitParams: Occlusion Group from Unhandled Sound Owner Type" );
						return false;
					}
				}
			}
		}
	}	*/

	if(storedInitParams.HasTracker)
	{
		if(pEntity)
			initParams.Tracker = pEntity->GetPlaceableTracker();
	}

	if( storedInitParams.HasWaveSlot )
	{
		initParams.WaveSlot = ReplaySound::ExtractWaveSlotPointer( (eContext)storedInitParams.Context, pEntity );
	}

	if( storedInitParams.HasCategory )
	{
		initParams.Category	= g_AudioEngine.GetCategoryManager().GetCategoryPtr( storedInitParams.CategoryHashValue );
	}

	if(storedInitParams.HasEnvironmentGroup)
	{
		if(pEntity)
			initParams.EnvironmentGroup	= pEntity->GetAudioEnvironmentGroup();
		else if (soundEntity == eScriptSoundEntity && pPosition)
		{
			initParams.EnvironmentGroup = g_ScriptAudioEntity.CreateReplayScriptedEnvironmentGroup(NULL, VECTOR3_TO_VEC3V(*pPosition));		
		}
		else
		{
			initParams.EnvironmentGroup = ExtractEnvironmentGroupPointer(storedInitParams, pEntity, createdEnvironmentGroup);
	
		}
	}

	// Extract Everything Else
	initParams.Volume					= storedInitParams.Volume;
	initParams.PostSubmixAttenuation	= storedInitParams.PostSubmixAttenuation;
	initParams.Pitch					= storedInitParams.Pitch;
	initParams.Pan						= storedInitParams.Pan;
	initParams.VolumeCurveScale			= storedInitParams.VolumeCurveScale;
	initParams.PrepareTimeLimit			= storedInitParams.PrepareTimeLimit;
	
	initParams.StartOffset				= storedInitParams.StartOffset;
	initParams.Predelay					= storedInitParams.Predelay;
	initParams.AttackTime				= storedInitParams.AttackTime;
	
	initParams.u32ClientVar				= storedInitParams.u32ClientVar;
	
	initParams.TimerId					= storedInitParams.TimerId;
	initParams.LPFCutoff				= storedInitParams.LPFCutoff;
	initParams.VirtualisationScoreOffset= storedInitParams.VirtualisationScoreOffset;
	initParams.ShadowPcmSourceId		= storedInitParams.ShadowPcmSourceId;
	initParams.PcmSourceChannelId		= storedInitParams.PcmSourceChannelId;
	initParams.SourceEffectSubmixId		= storedInitParams.SourceEffectSubmixId;
	initParams.BucketId					= storedInitParams.BucketId;
	initParams.EffectRoute				= storedInitParams.EffectRoute;
	initParams.SpeakerMask				= storedInitParams.SpeakerMask;

	initParams.PrimaryShadowSound		= storedInitParams.PrimaryShadowSound;
	initParams.IsStartOffsetPercentage	= storedInitParams.IsStartOffsetPercentage;
	initParams.UpdateEntity				= storedInitParams.UpdateEntity;
	initParams.AllowOrphaned			= storedInitParams.AllowOrphaned;
	initParams.AllowLoad				= storedInitParams.AllowLoad;
	initParams.RemoveHierarchy			= storedInitParams.RemoveHierarchy;
	initParams.ShouldPlayPhysically		= storedInitParams.ShouldPlayPhysically;
	initParams.IsAuditioning			= storedInitParams.IsAuditioning;

	if(initParams.StartOffset < 0)
	{
		initParams.StartOffset = 0;
	}
	return true;
}

//---------------------------------------------------------
// Func: ReplaySound::ExtractSoundSettings
// Desc: Extracts and updates the settings for a sound
//---------------------------------------------------------
#if 0
void ReplaySound::ExtractSoundSettings( audSound *pSound, const CUpdateParams *updateParams )
{
	if( !pSound )
	{
		replayAssertf( false, "ReplaySound::ExtractSoundSettings: Could not obtain sound from sound id" );
		return;
	}

	// Set Requested Settings
	// TODO: Add more Set Calls as Needed
	if( updateParams->GetHasPostSubmixVolumeAttenuationChanged() )
	{
		pSound->SetRequestedPostSubmixVolumeAttenuation( updateParams->GetPostSubmixVolumeAttenuation() );
	}

	if( updateParams->GetHasVolumeChanged() )
	{
		pSound->SetRequestedVolume(	updateParams->GetVolume() );
	}

	if( updateParams->GetHasPositionChanged() )
	{
		pSound->SetRequestedPosition( updateParams->GetPosition() );
	}

	if( updateParams->GetHasPitchChanged() )
	{
		pSound->SetRequestedPitch( updateParams->GetPitch() );
	}

	if( updateParams->GetHasLowPassFilterCutoffChanged() )
	{
		pSound->SetRequestedLPFCutoff( updateParams->GetLowPassFilterCutoff() );
	}

	if( updateParams->GetHasShouldUpdateEntityChanged() )
	{
		pSound->SetUpdateEntity( updateParams->GetShouldUpdateEntity() );
	}

	if( updateParams->GetHasAllowOrphanedChanged() )
	{
		pSound->SetAllowOrphaned( updateParams->GetAllowOrphaned() );
	}

	switch( updateParams->GetClientVariableType() )
	{
	case ReplaySound::CLIENT_VAR_U32:
		{
			pSound->SetClientVariable( updateParams->GetClientVariableU32() );
		}
		break;

	case ReplaySound::CLIENT_VAR_F32:
		{
			pSound->SetClientVariable( updateParams->GetClientVariableF32() );
		}
		break;

	case ReplaySound::CLIENT_VAR_PTR:
		{
			pSound->SetClientVariable( updateParams->GetClientVariablePtr() );
		}
		break;

	case ReplaySound::CLIENT_VAR_NONE:
	default:
		break;
	}

	if( updateParams->GetShouldInvalidateEntity() )
	{
		pSound->InvalidateEntity();
	}
}
#endif
//---------------------------------------------------------
// Func: ReplaySound::ExtractWaveSlotPointer
// Desc: Returns a pointer to the wave slot that a sound
//       should use.
//---------------------------------------------------------
audWaveSlot* ReplaySound::ExtractWaveSlotPointer( eContext context, CEntity* pEntity )
{	
	switch( context )
	{
	case ReplaySound::CONTEXT_HELI_GOING_DOWN:
		{
			if(audVerifyf(pEntity && pEntity->GetIsTypeVehicle(), "Expecting to trigger heli going down sound on a vehicle!"))
			{
				CVehicle* pVehicle = (CVehicle*)pEntity;
				if(audVerifyf(pVehicle->InheritsFromHeli(), "Expecting to trigger heli going down sound on a heli!"))
				{
					s32 speechSlotId = -1; 
					audHeliAudioEntity* heliAudioEntity = (audHeliAudioEntity*)pVehicle->GetVehicleAudioEntity();

					if(heliAudioEntity)
					{
						if(heliAudioEntity->GetSpeechSlotID() == -1)
						{
							// Let the helicopter audio entity know the speech slot so that we can free it once we're done with it
							bool hadToStop = false;
#if AUD_DEFERRED_SPEECH_DEBUG_PRINT
							speechSlotId = g_SpeechManager.GetAmbientSpeechSlot(NULL, &hadToStop, 6.6f, THREAD_INVALID, "HELI_GOING_DOWN_REPLAY", true);							
#else
							speechSlotId = g_SpeechManager.GetAmbientSpeechSlot(NULL, &hadToStop, 6.6f, THREAD_INVALID, true);							
#endif
							if(speechSlotId == -1)
							{
								return NULL;
							}
							heliAudioEntity->SetSpeechSlotID(speechSlotId);

							g_SpeechManager.PopulateAmbientSpeechSlotWithPlayingSpeechInfo(speechSlotId, 0, 
#if __BANK
								"HELI_GOING_DOWN_REPLAY",
#endif
								0);
						}
						else
						{
							speechSlotId = heliAudioEntity->GetSpeechSlotID();
						}
					}
						
					if(speechSlotId >= 0)
					{
						return g_SpeechManager.GetAmbientWaveSlotFromId(speechSlotId);
					}
				}
			}
			
			return NULL;
		}
		break;

	default:
		{
			replayAssertf( false, "ReplaySound::ExtractWaveSlotPointer: Init Param Waveslot Pointer not Updated" );
			return NULL;
		}
		break;
	}
}

//---------------------------------------------------------
// Func: ReplaySound::ExtractEnvironmentGroupPointer
// Desc: Returns a pointer to the enviorment group created for the sound.
//---------------------------------------------------------
naEnvironmentGroup* ReplaySound::ExtractEnvironmentGroupPointer(const CInitParams &storedInitParams, CEntity* pEntity, bool* createdEnvironmentGroup)
{
	switch(storedInitParams.Context)
	{
	case ReplaySound::CONTEXT_AMBIENT_REGISTERED_EFFECTS:
		{
			naEnvironmentGroup* occlusionGroup = naEnvironmentGroup::Allocate("AmbientEffect");
			if(occlusionGroup)
			{
				occlusionGroup->Init(NULL, 40.f);		
				Vector3 pos;
				storedInitParams.Position.Load(pos);
				occlusionGroup->SetSource(VECTOR3_TO_VEC3V(pos));
				occlusionGroup->ForceSourceEnvironmentUpdate(pEntity);
				occlusionGroup->AddSoundReference();

				if (createdEnvironmentGroup)
				{
					*createdEnvironmentGroup = true;
				}
			}
			return occlusionGroup;
		}
		break;
	default:
		replayAssert(false && "Has environment group but no entity to get it from");
		break;
	}
	return NULL;
}



//---------------------------------------------------------
// Func: ReplaySound::CUpdateParams::CUpdateParams
// Desc: Constructor
//---------------------------------------------------------
ReplaySound::CUpdateParams::CUpdateParams( eContext context ) :	
	m_ClientVariableType( CLIENT_VAR_NONE ),
	m_Context( context ),
	m_OwnerReplayId( -1 ),
	m_WasWaveSlotProvided( false ),
	m_AllowLoad( false ),
	m_TimeLimit( 0 ),
	m_ShouldInvalidateEntity( false ),
	m_HasPostSubmixVolumeAttenuationChanged( false ),
	m_HasVolumeChanged( false ),
	m_HasPositionChanged( false ),
	m_HasPitchChanged( false ),
	m_HasLowPassFilterCutoffChanged( false ),
	m_HasShouldUpdateEntityChanged( false ),
	m_HasAllowOrphanedChanged( false )
{
}

//---------------------------------------------------------
// Func: ReplaySound::GetPersistentFireSoundPtr
// Desc: Obtain a free persistent audSound* for fire sounds
//---------------------------------------------------------
audSound**	ReplaySound::GetPersistentFireSoundPtr( u32 searchStartIdx )
{
	audSound**			ppSound				= NULL;
	/*TODO4FIVE audFireAudioEntity*	pFireAudioEntity	= NULL;*/

	for( ; searchStartIdx < MAX_FIRES; ++searchStartIdx )
	{
		/*TODO4FIVE pFireAudioEntity = &(g_fireMan.m_fires[searchStartIdx].m_AudioEntity);

		if( pFireAudioEntity->m_MainFireLoop == NULL )
		{
			ppSound = &(pFireAudioEntity->m_MainFireLoop);
			break;
		}
		else if( pFireAudioEntity->m_WindFireLoop == NULL )
		{
			ppSound = &(pFireAudioEntity->m_WindFireLoop);
			break;
		}
		else if( pFireAudioEntity->m_CloseFireLoop == NULL )
		{
			ppSound = &(pFireAudioEntity->m_CloseFireLoop);
			break;
		}*/
	}

	if( !ppSound )
	{
		replayAssertf( false, "ReplaySound::GetPersistentFireSoundPtr: No persistent fire sounds available" );
		return NULL;
	}
	else
	{
		return ppSound;
	}
}

//---------------------------------------------------------
// Func: ReplaySound::CreatePlayAndBindFireSound
// Desc: Creates & plays a fire sound. The sound ID is
//        then binded to the sound.
//---------------------------------------------------------
bool ReplaySound::CreatePlayAndBindFireSound( s32 /*soundId*/, u32 /*fireSoundHash*/, audSoundInitParams &/*initParams*/ )
{
	replayFatalAssertf(false, "Todo");
// 	if( soundId >= 0 )
// 	{
// 		audSound** ppFireSound = NULL;
// 
// 		ppFireSound = ReplaySound::GetPersistentFireSoundPtr();
// 
// 		if( !ppFireSound )
// 		{
// 			return false;
// 		}
// 
// 		bool* pIsSoundIdTaken	= CReplayMgrInternal::GetPointerToIsSoundIdTaken( soundId );
// 		void* pSoundHandle		= CReplayMgrInternal::GetPointerToSoundHandle( soundId );
// 
// 		if( pIsSoundIdTaken == NULL || pSoundHandle == NULL )
// 		{
// 			replayAssertf( false, "CreatePlayAndBindFireSound::Extract: Could not obtain pointer to isSoundIdTaken or sound handle." );
// 			return false;
// 		}
// 
// 		CReplayMgrInternal::s_AudioEntity.CreateSound_PersistentReference( fireSoundHash, ppFireSound, &initParams );
// 
// 		if(*ppFireSound)
// 		{
// 			(*ppFireSound)->SetReplayId(soundId, pIsSoundIdTaken, pSoundHandle );
// 		}
// 
// 		if( *ppFireSound )
// 		{
// 			if( fireSoundHash == ATSTRINGHASH("WIND_FIRE_LOOP", 0x7CB02B6D) )
// 			{
// 				(*ppFireSound)->SetRequestedVolume( -100.f );
// 			}
// 
// 			(*ppFireSound)->PrepareAndPlay();
// 			CReplayMgrInternal::BindPersistentSoundToSoundId( soundId, ppFireSound );
// 			return true;
// 		}
// 		else
// 		{
// 			replayAssertf( false, "ReplaySound::CreatePlayAndBindFireSound: Could not create fire sound" );
// 			return false;
// 		}
// 	}
	return false;
}

//---------------------------------------------------------
// Func: ReplaySound::GetPedPtrFromReplayId
// Desc: Safely obtains a pointer to the ped with the
//        passed in replay ID.
//---------------------------------------------------------
CPed* ReplaySound::GetPedPtrFromReplayId( s32 replayId )
{
	CReplayID id( replayId );

	s16	baseId = id.GetEntityID();

	if( (baseId >= 0) && (baseId < CReplayMgrInternal::MAX_REPLAY_SOUNDS) )
	{
		CEntity *pEntity = CReplayMgrInternal::GetEntityAsEntity(replayId);

		if( !(pEntity && pEntity->GetIsTypePed()) )
		{
			return NULL;	// Speech Owner Not a Ped
		}

		return ( verify_cast<CPed*> (pEntity) );
	}
	else
	{
		return NULL;		// No Owner for Sound
	}
}

void ReplaySound::RegisterSounds(int normalSound, audMetadataRef halfSound, audMetadataRef quarterSound, audMetadataRef doubleSound, audMetadataRef tripleSound)
{
	CSoundRegister* reg = SoundsRegister.Access(normalSound);
	replayAssert(reg == NULL);
	if(reg != NULL)
		return;

	CSoundRegister newReg;
	newReg.HalfSpeedSound = halfSound;
	newReg.QuarterSpeedSound = quarterSound;
	newReg.DoubleSpeedSound = doubleSound;
	newReg.TripleSpeedSound = tripleSound;
	SoundsRegister[normalSound] = newReg;
}


void ReplaySound::GetSound(int speed, audMetadataRef& normalSound)
{
	int i;
	CSoundRegister* reg = SoundsRegister.Access(/*normalSound*/i);
	if(!reg)
		return;

	switch(speed)
	{
	case eHalfSpeedSound:
		if(reg->HalfSpeedSound.IsValid())
			normalSound = reg->HalfSpeedSound;
		break;
	case eQuarterSpeedSound:
		if(reg->QuarterSpeedSound.IsValid())
			normalSound = reg->QuarterSpeedSound;
		break;
	case eDoubleSpeedSound:
		if(reg->DoubleSpeedSound.IsValid())
			normalSound = reg->DoubleSpeedSound;
		break;
	case eTripleSpeedSound:
		if(reg->TripleSpeedSound.IsValid())
			normalSound = reg->TripleSpeedSound;
		break;
	default:
		break;
	}
}



#endif // GTA_REPLAY
