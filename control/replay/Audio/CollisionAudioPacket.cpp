
#include "CollisionAudioPacket.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "scene/Entity.h"
#include "objects/object.h"
#include "audiosoundtypes/sound.h"
#include "peds/ped.h"

const u8 CollisionTypeIsRollBit = 0;
//////////////////////////////////////////////////////////////////////////
CPacketCollisionPlayPacket::CPacketCollisionPlayPacket(u32 soundHash, const float volume, const Vector3 position[], u8 index)
: CPacketEventTracked(PACKETID_SOUND_COLLISIONPLAY, sizeof(CPacketCollisionPlayPacket))
, CPacketEntityInfoStaticAware()
, m_soundHash(soundHash)
, m_clientVar(0.0f)
, m_volume(volume)
, m_index(index)
{
	SetPadBool(CollisionTypeIsRollBit, true);	// Roll
	m_position[0].Store(position[0]);
	m_position[1].Store(position[1]);
	m_MaterialId[0] = 0;
	m_MaterialId[1] = 0;
}


//////////////////////////////////////////////////////////////////////////
CPacketCollisionPlayPacket::CPacketCollisionPlayPacket(u32 soundHash, const s32 pitch, const float volume, const Vector3 position[], u8 index)
: CPacketEventTracked(PACKETID_SOUND_COLLISIONPLAY, sizeof(CPacketCollisionPlayPacket))
, CPacketEntityInfoStaticAware()
, m_soundHash(soundHash)
, m_pitch(pitch)
, m_volume(volume)
, m_index(index)
{
	SetPadBool(CollisionTypeIsRollBit, false);	// Scrape
	m_position[0].Store(position[0]);
	m_position[1].Store(position[1]);
	m_MaterialId[0] = 0;
	m_MaterialId[1] = 0;
}


//////////////////////////////////////////////////////////////////////////
CPacketCollisionPlayPacket::CPacketCollisionPlayPacket(u32 soundHash, const float volume, const Vector3 position[], phMaterialMgr::Id materialId[], u8 index)
	: CPacketEventTracked(PACKETID_SOUND_COLLISIONPLAY, sizeof(CPacketCollisionPlayPacket))
	, CPacketEntityInfoStaticAware()
	, m_soundHash(soundHash)
	, m_clientVar(0.0f)
	, m_volume(volume)
	, m_index(index)
	
{
	SetPadBool(CollisionTypeIsRollBit, true);	// Roll
	m_position[0].Store(position[0]);
	m_position[1].Store(position[1]);
	m_MaterialId[0] = materialId[0];
	m_MaterialId[1] = materialId[1];
}


//////////////////////////////////////////////////////////////////////////
CPacketCollisionPlayPacket::CPacketCollisionPlayPacket(u32 soundHash, const s32 pitch, const float volume, const Vector3 position[], phMaterialMgr::Id materialId[], u8 index)
	: CPacketEventTracked(PACKETID_SOUND_COLLISIONPLAY, sizeof(CPacketCollisionPlayPacket))
	, CPacketEntityInfoStaticAware()
	, m_soundHash(soundHash)
	, m_pitch(pitch)
	, m_volume(volume)
	, m_index(index)
{
	SetPadBool(CollisionTypeIsRollBit, false);	// Scrape
	m_position[0].Store(position[0]);
	m_position[1].Store(position[1]);
	m_MaterialId[0] = materialId[0];
	m_MaterialId[1] = materialId[1];
}


//////////////////////////////////////////////////////////////////////////
void CPacketCollisionPlayPacket::Extract(CTrackedEventInfo<tTrackedSoundType>& data) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	if(data.m_pEffect[0] != NULL)
	{
		return;
	}
	
	audSoundInitParams initParams;

	if(data.pEntity[0] || data.pEntity[1])
	{
		if(data.pEntity[0])
		{
			initParams.EnvironmentGroup = data.pEntity[0]->GetAudioEnvironmentGroup();
		}
		else if (data.pEntity[1])
		{
			initParams.EnvironmentGroup = data.pEntity[1]->GetAudioEnvironmentGroup();
		}

		if(initParams.EnvironmentGroup == NULL)
		{
			Vector3 position;
			m_position[0].Load(position);
			initParams.EnvironmentGroup = g_CollisionAudioEntity.GetOcclusionGroup(VECTOR3_TO_VEC3V(position), data.pEntity[0], m_MaterialId[0]);
			if(initParams.EnvironmentGroup == NULL)
			{
				m_position[1].Load(position);
				initParams.EnvironmentGroup = g_CollisionAudioEntity.GetOcclusionGroup(VECTOR3_TO_VEC3V(position), data.pEntity[1], m_MaterialId[1]);
			}
		}
	}
	
	g_CollisionAudioEntity.CreateSound_PersistentReference(m_soundHash, &(data.m_pEffect[0].m_pSound), &initParams);

	if(data.m_pEffect[0] != NULL)
	{
		Vector3 position;
		m_position[m_index].Load(position);

		if(GetPadBool(CollisionTypeIsRollBit))	// Is Roll?
		{
			data.m_pEffect[0].m_pSound->SetClientVariable(m_clientVar);
		}
		else // Scrape
		{
			data.m_pEffect[0].m_pSound->SetRequestedPitch(m_pitch);
		}

		data.m_pEffect[0].m_pSound->SetRequestedVolume(m_volume);
		data.m_pEffect[0].m_pSound->SetRequestedPosition(position);

		data.m_pEffect[0].m_pSound->PrepareAndPlay();
	}
}


//////////////////////////////////////////////////////////////////////////
CPacketCollisionUpdatePacket::CPacketCollisionUpdatePacket(const float volume, const Vector3& position)
: CPacketEventTracked(PACKETID_SOUND_COLLISIONUPDATE, sizeof(CPacketCollisionUpdatePacket))
, CPacketEntityInfo()
, m_clientVar(0.0f)
, m_volume(volume)
{
	SetPadBool(CollisionTypeIsRollBit, true);	// Roll
	m_position.Store(position);
}


//////////////////////////////////////////////////////////////////////////
CPacketCollisionUpdatePacket::CPacketCollisionUpdatePacket(const s32 pitch, const float volume, const Vector3& position)
: CPacketEventTracked(PACKETID_SOUND_COLLISIONUPDATE, sizeof(CPacketCollisionUpdatePacket))
, CPacketEntityInfo()
, m_pitch(pitch)
, m_volume(volume)
{
	SetPadBool(CollisionTypeIsRollBit, false);	// Scrape
	m_position.Store(position);
}


//////////////////////////////////////////////////////////////////////////
void CPacketCollisionUpdatePacket::Extract(CTrackedEventInfo<tTrackedSoundType>& data) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	replayAssert(data.m_pEffect[0] != NULL);
	if(data.m_pEffect[0] != NULL)
	{
		Vector3 position;
		m_position.Load(position);

		if(GetPadBool(CollisionTypeIsRollBit))	// Is Roll?
		{
			data.m_pEffect[0].m_pSound->SetClientVariable(m_clientVar);
		}
		else // Scrape
		{
			data.m_pEffect[0].m_pSound->SetRequestedPitch(m_pitch);
		}

		data.m_pEffect[0].m_pSound->SetRequestedVolume(m_volume);
		data.m_pEffect[0].m_pSound->SetRequestedPosition(position);
	}
}


//////////////////////////////////////////////////////////////////////////
CPacketVehiclePedCollisionPacket::CPacketVehiclePedCollisionPacket(const Vector3 pos, const audVehicleCollisionContext* impactContext)
	: CPacketEvent(PACKETID_SOUND_VEHICLE_PED_COLLSION, sizeof(CPacketVehiclePedCollisionPacket))
	, CPacketEntityInfo()
{
	m_pos = pos;
	m_ImpactOtherComponent = impactContext->collisionEvent.otherComponent;
	m_ImpactImpactMag = impactContext->impactMag;
	m_ImpactType = impactContext->type;
}

void CPacketVehiclePedCollisionPacket::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	audPedAudioEntity* pedAudio = ((CPed*)eventInfo.GetEntity(0))->GetPedAudioEntity();
	CVehicle* pVehicle = (CVehicle*)eventInfo.GetEntity(1);
	audVehicleAudioEntity* pVehicleAudio = (audVehicleAudioEntity*)pVehicle->GetAudioEntity();
	audVehicleCollisionAudio* pCollisionAudio = &pVehicleAudio->GetCollisionAudio();

	audVehicleCollisionContext impactContext;
	impactContext.collisionEvent.otherComponent = m_ImpactOtherComponent;
	impactContext.impactMag = m_ImpactImpactMag;
	impactContext.type = m_ImpactType;

	pedAudio->PlayVehicleImpact(m_pos, pCollisionAudio->GetVehicle(), &impactContext);
	pedAudio->PlayUnderOverVehicleSounds(m_pos, pCollisionAudio->GetVehicle(), &impactContext);
}


//////////////////////////////////////////////////////////////////////////
CPacketPedImpactPacket::CPacketPedImpactPacket(const Vector3 pos, f32 impactVel, u32 collisionEventOtherComponent, bool isMeleeImpact)
	: CPacketEvent(PACKETID_SOUND_PED_IMPACT_COLLSION, sizeof(CPacketPedImpactPacket))
	, CPacketEntityInfoStaticAware()
{
	m_pos = pos;
	m_ImpactVel = impactVel;
	m_CollisionEventOtherComponent = collisionEventOtherComponent;
	m_IsMeleeImpact = isMeleeImpact;
}

void CPacketPedImpactPacket::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	audPedAudioEntity* pedAudio = ((CPed*)eventInfo.GetEntity(0))->GetPedAudioEntity();
	CEntity* pEntity = eventInfo.GetEntity(1);

	pedAudio->TriggerImpactSounds(m_pos, NULL/*not used*/, pEntity, m_ImpactVel, m_CollisionEventOtherComponent, 0 /*not used*/, m_IsMeleeImpact);
}


//////////////////////////////////////////////////////////////////////////
CPacketVehicleSplashPacket::CPacketVehicleSplashPacket(const f32 downSpeed,bool outOfWater)
	: CPacketEvent(PACKETID_SOUND_VEHICLE_SPLASH, sizeof(CPacketVehicleSplashPacket))
	, CPacketEntityInfo()
{
	m_DownSpeed = downSpeed;
	m_OutOfWater = outOfWater;
}

void CPacketVehicleSplashPacket::Extract(const CEventInfo<CVehicle>& eventInfo) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	audVehicleAudioEntity* vehicleAudio = ((CVehicle*)eventInfo.GetEntity(0))->GetVehicleAudioEntity();
	
	if(vehicleAudio)
	{
		vehicleAudio->TriggerSplash(m_DownSpeed, m_OutOfWater);
	}
}

#endif // GTA_REPLAY
