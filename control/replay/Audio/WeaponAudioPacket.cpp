//=====================================================================================================
// name:		WeaponAudioPacket.cpp
//=====================================================================================================

#include "WeaponAudioPacket.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/ReplayInternal.h"
#include "audio/northaudioengine.h"
#include "audio/pedaudioentity.h"
#include "peds/ped.h"

tPacketVersion g_PacketWeaponSoundFireEvent_v1 = 1;
//////////////////////////////////////////////////////////////////////////
CPacketWeaponSoundFireEvent::CPacketWeaponSoundFireEvent(const audWeaponFireInfo& info, f32 fgunFightVolumeOffset, u32 weaponHash, int seatIdx)
	: CPacketEvent(PACKETID_SOUND_WEAPON_FIRE_EVENT, sizeof(CPacketWeaponSoundFireEvent), g_PacketWeaponSoundFireEvent_v1)
	, CPacketEntityInfo()
	, m_isSuppressed(info.isSuppressed)
	, m_settingsHash(info.settingsHash)
	, m_gunFightVolumeOffset(fgunFightVolumeOffset)
	, m_ammo(info.ammo)
	, m_weaponHash(weaponHash)
	, m_seatIdx(seatIdx)
	, m_PresuckScene(NULL)
	, m_HasDonePreplay(false)
{ 
	m_explicitPosition.Store(info.explicitPosition);
}



ePreplayResult	CPacketWeaponSoundFireEvent::Preplay(const CEventInfo<CEntity>&pData) const
{
	if(m_HasDonePreplay)
	{
		return PREPLAY_SUCCESSFUL;
	}

	if(pData.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_FWD && pData.GetPlaybackFlags().GetPlaybackSpeed() < 1.0f)
	{
		// Now check we have an entity...
		CEntity* pEntity = pData.GetEntity();
		if(!pEntity)
		{
			return PREPLAY_MISSING_ENTITY;	// Missing entity...wait
		}

		audWeaponFireInfo info;
		info.parentEntity		= pData.GetEntity();
		info.settingsHash		= m_settingsHash;
		info.isSuppressed		= m_isSuppressed;
		info.audioSettingsRef   = audNorthAudioEngine::GetMetadataManager().GetObjectMetadataRefFromHash(m_settingsHash);
		info.ammo = m_ammo;

		if (GetPacketVersion() >= g_PacketWeaponSoundFireEvent_v1)
		{
			m_explicitPosition.Load(info.explicitPosition);
		}

		if(pEntity->GetAudioEntity())
		{

			const WeaponSettings * settings = info.FindWeaponSettings();

			audWeaponFireInfo::PresuckInfo presuckInfo;
			info.PopulatePresuckInfo(settings, presuckInfo);

			if(presuckInfo.presuckTime > 0)
			{
				PreplayCore(pData, presuckInfo);
			}
			else
			{
				return PREPLAY_WAIT_FOR_TIME;
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

	m_HasDonePreplay = true;
	return PREPLAY_SUCCESSFUL; 
}

void	CPacketWeaponSoundFireEvent::PreplayCore(const CEventInfo<CEntity>&pData, const audWeaponFireInfo::PresuckInfo &presuckInfo) const
{
	CEntity * pEntity = pData.GetEntity();
	audSoundInitParams initParams;
	initParams.Tracker = pEntity->GetPlaceableTracker();
	naAudioEntity* pAudioEntity = pEntity->GetAudioEntity();

	replayDebugf1("Weapon PreSuck playing at %u, packet at %u", pData.GetReplayTime(), GetGameTime());
	pAudioEntity->CreateAndPlaySound(presuckInfo.presuckSound, &initParams);

	MixerScene * sceneSettings  = DYNAMICMIXMGR.GetObject<MixerScene>(presuckInfo.presuckScene); 
	if(sceneSettings)
	{
		DYNAMICMIXER.StartScene(sceneSettings, &m_PresuckScene);
	}
}

//////////////////////////////////////////////////////////////////////////
u32	CPacketWeaponSoundFireEvent::GetPreplayTime(const CEventInfo<CEntity>&pData) const
{
	audWeaponFireInfo info;
	info.settingsHash		= m_settingsHash;
	info.isSuppressed		= m_isSuppressed;
	info.audioSettingsRef   = audNorthAudioEngine::GetMetadataManager().GetObjectMetadataRefFromHash(m_settingsHash);
	info.ammo = m_ammo;
	info.parentEntity = pData.GetEntity();

	const WeaponSettings * settings = info.FindWeaponSettings();

	audWeaponFireInfo::PresuckInfo presuckInfo;
	info.PopulatePresuckInfo(settings, presuckInfo);

	return presuckInfo.presuckTime;
}

//////////////////////////////////////////////////////////////////////////
void CPacketWeaponSoundFireEvent::Extract(CEventInfo<CEntity>& pData) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	audWeaponFireInfo info;
	info.parentEntity		= pData.GetEntity();
	info.settingsHash		= m_settingsHash;
	info.isSuppressed		= m_isSuppressed;
	info.audioSettingsRef   = audNorthAudioEngine::GetMetadataManager().GetObjectMetadataRefFromHash(m_settingsHash);
	info.ammo = m_ammo;

	if (GetPacketVersion() >= g_PacketWeaponSoundFireEvent_v1)
	{
		m_explicitPosition.Load(info.explicitPosition);
	}

	audWeaponFireEvent weaponEvent;
	weaponEvent.fireInfo = info;	

	//Handle presuck if we've not already triggered it
	if(!m_HasDonePreplay)
	{
		const WeaponSettings * settings = info.FindWeaponSettings();
		audWeaponFireInfo::PresuckInfo presuckInfo;
		info.PopulatePresuckInfo(settings, presuckInfo);
		PreplayCore(pData, presuckInfo);
	}
	m_HasDonePreplay = false;

	if(m_PresuckScene)
	{
		m_PresuckScene->Stop();
		m_PresuckScene = NULL;
	}

	if(info.parentEntity)
	{
		fwAttachmentEntityExtension *pedAttachmentExtension = NULL;
		if(info.parentEntity->GetIsTypePed())
		{
			CPed* pPed = static_cast<CPed*>(info.parentEntity.Get());

			// Use the vehicle weapon if possible for this seat position
			if(pPed->GetVehiclePedInside())
			{				
				int seatIdx = pPed->GetMyVehicle()->GetSeatManager()->GetPedsSeatIndex(pPed);
				weaponEvent.weapon = GetVehicleWeapon(pPed->GetMyVehicle(), seatIdx);
			}
			
			// No ped attachment use weapon object to get cWeapon
			CEntity* pEntity =  pData.GetEntity(1);
			if(weaponEvent.weapon == NULL && pEntity && pEntity->GetIsTypeObject())
			{
				CObject* pObject = static_cast<CObject*>(pEntity);
				if(pObject && pObject->GetWeapon())
				{
					weaponEvent.weapon = pObject->GetWeapon();
				}
			}

			// No vehicle weapon or setup weapon the use the attachment system
			if(weaponEvent.weapon == NULL)
			{
				pedAttachmentExtension = pPed->GetAttachmentExtension();
			}
		
			if(pPed->GetPedAudioEntity())
			{
				pPed->GetPedAudioEntity()->SetGunfightVolumeOffset(m_gunFightVolumeOffset); 
			}
		}
		else if(info.parentEntity->GetIsTypeVehicle())
		{
			CVehicle* pVehicle = static_cast<CVehicle*>(info.parentEntity.Get());

			// Use the vehicle weapon if possible for this seat position
			if(m_seatIdx != -1)
			{
				weaponEvent.weapon = GetVehicleWeapon(pVehicle, m_seatIdx);
			}

			// No vehicle weapon use peds weapon
			if(pVehicle->GetDriver() && weaponEvent.weapon == NULL)
			{
				pedAttachmentExtension = pVehicle->GetDriver()->GetAttachmentExtension();
			}
		}
		else if(info.parentEntity->GetIsTypeObject())
		{
			// Handle pickups shooting
			CObject* pObject = static_cast<CObject*>(info.parentEntity.Get());
			if(pObject->IsPickup())
			{
				weaponEvent.weapon = pObject->GetWeapon();
			}
		}

		if(!replayVerifyf(weaponEvent.weapon, "CPacketWeaponSoundFireEvent::Extract unable to find weapon %i", m_weaponHash))
		{
			return;
		}
	}

	g_WeaponAudioEntity.PlayWeaponFireEvent(&weaponEvent);
}

CWeapon* CPacketWeaponSoundFireEvent::GetVehicleWeapon(CVehicle* pVehicle, int seatIdx) const
{
	if(!pVehicle || !pVehicle->GetVehicleWeaponMgr())
	{
		return NULL;
	}
	
	atArray<CVehicleWeapon*> weapons;
	pVehicle->GetVehicleWeaponMgr()->GetWeaponsForSeat(seatIdx, weapons);
	for(int i = 0; i < weapons.GetCount(); ++i)
	{
		if(!weapons[i])
			continue;
			
		if(weapons[i]->GetType() == VGT_FIXED_VEHICLE_WEAPON && weapons[i]->GetHash() == m_weaponHash)
		{
			return static_cast<CFixedVehicleWeapon*>(weapons[i])->GetWeapon();
		}
		else if(weapons[i]->GetType() == VGT_VEHICLE_WEAPON_BATTERY)
		{
			CVehicleWeaponBattery* pWeaponBattery = static_cast<CVehicleWeaponBattery*>(weapons[i]);
			for(int j = 0; j < pWeaponBattery->GetNumWeaponsInBattery(); j++)
			{
				CVehicleWeapon* pWeapon = pWeaponBattery->GetVehicleWeapon(j);

				if(pWeapon && pWeapon->GetHash() == m_weaponHash)
				{
					return static_cast<CFixedVehicleWeapon*>(pWeapon)->GetWeapon();
				}
			}
		}
	}
	return NULL;
}


tPacketVersion g_PacketWeaponSoundFireEventOld_v1 = 1;
tPacketVersion g_PacketWeaponSoundFireEventOld_v2 = 2;
//////////////////////////////////////////////////////////////////////////
CPacketWeaponSoundFireEventOld::CPacketWeaponSoundFireEventOld(const audWeaponFireInfo& info, f32 fgunFightVolumeOffset, u32 weaponHash, int seatIdx)
	: CPacketEvent(PACKETID_SOUND_WEAPON_FIRE_EVENT_OLD, sizeof(CPacketWeaponSoundFireEvent), g_PacketWeaponSoundFireEventOld_v2)
	, CPacketEntityInfo()
	, m_isSuppressed(info.isSuppressed)
	, m_settingsHash(info.settingsHash)
	, m_gunFightVolumeOffset(fgunFightVolumeOffset)
	, m_ammo(info.ammo)
	, m_weaponHash(weaponHash)
	, m_seatIdx(seatIdx)
	, m_PresuckScene(NULL)
	, m_HasDonePreplay(false)
{ 
}

ePreplayResult	CPacketWeaponSoundFireEventOld::Preplay(const CEventInfo<CEntity>&pData) const
{
	if(GetPacketVersion() >= g_PacketWeaponSoundFireEventOld_v2)
	{
		if(m_HasDonePreplay)
		{
			return PREPLAY_SUCCESSFUL;
		}
	}
	else
	{
		return PREPLAY_SUCCESSFUL;
	}

	if(pData.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_FWD && pData.GetPlaybackFlags().GetPlaybackSpeed() < 1.0f)
	{
		// Now check we have an entity...
		CEntity* pEntity = pData.GetEntity();
		if(!pEntity)
		{
			return PREPLAY_MISSING_ENTITY;	// Missing entity...wait
		}

		audWeaponFireInfo info;
		info.parentEntity		= pData.GetEntity();
		info.settingsHash		= m_settingsHash;
		info.isSuppressed		= m_isSuppressed;
		info.audioSettingsRef   = audNorthAudioEngine::GetMetadataManager().GetObjectMetadataRefFromHash(m_settingsHash);
		info.ammo = m_ammo;

		if(pEntity->GetAudioEntity())
		{
			audSoundInitParams initParams;
			initParams.Tracker = pEntity->GetPlaceableTracker();
			naAudioEntity* pAudioEntity = pEntity->GetAudioEntity();

			const WeaponSettings * settings = info.FindWeaponSettings();

			audWeaponFireInfo::PresuckInfo presuckInfo;
			info.PopulatePresuckInfo(settings, presuckInfo);

			replayDebugf1("Weapon PreSuck playing at %u, packet at %u", pData.GetReplayTime(), GetGameTime());
			pAudioEntity->CreateAndPlaySound(presuckInfo.presuckSound, &initParams);

			MixerScene * sceneSettings  = DYNAMICMIXMGR.GetObject<MixerScene>(presuckInfo.presuckScene); 
			if(sceneSettings)
			{
				DYNAMICMIXER.StartScene(sceneSettings, &m_PresuckScene);
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

	m_HasDonePreplay = true;
	return PREPLAY_SUCCESSFUL; 
}

//////////////////////////////////////////////////////////////////////////
u32	CPacketWeaponSoundFireEventOld::GetPreplayTime(const CEventInfo<CEntity>&pData) const
{
	audWeaponFireInfo info;
	info.settingsHash		= m_settingsHash;
	info.isSuppressed		= m_isSuppressed;
	info.audioSettingsRef   = audNorthAudioEngine::GetMetadataManager().GetObjectMetadataRefFromHash(m_settingsHash);
	info.ammo = m_ammo;
	info.parentEntity = pData.GetEntity();

	const WeaponSettings * settings = info.FindWeaponSettings();

	audWeaponFireInfo::PresuckInfo presuckInfo;
	info.PopulatePresuckInfo(settings, presuckInfo);

	return presuckInfo.presuckTime;
}

//////////////////////////////////////////////////////////////////////////
void CPacketWeaponSoundFireEventOld::Extract(CEventInfo<CEntity>& pData) const
{
	if(GetPacketVersion() >= g_PacketWeaponSoundFireEventOld_v2)
	{
		Preplay(pData);
		m_HasDonePreplay = false;

		if(m_PresuckScene)
		{
			m_PresuckScene->Stop();
			m_PresuckScene = NULL;
		}
	}
	audWeaponFireInfo info;
	info.parentEntity		= pData.GetEntity();
	info.settingsHash		= m_settingsHash;
	info.isSuppressed		= m_isSuppressed;
	info.audioSettingsRef   = audNorthAudioEngine::GetMetadataManager().GetObjectMetadataRefFromHash(m_settingsHash);
	info.ammo = m_ammo;

	audWeaponFireEvent weaponEvent;
	weaponEvent.fireInfo = info;	

	if(info.parentEntity)
	{
		fwAttachmentEntityExtension *pedAttachmentExtension = NULL;
		if(info.parentEntity->GetIsTypePed())
		{
			CPed* pPed = static_cast<CPed*>(info.parentEntity.Get());

			// Use the vehicle weapon if possible for this seat position
			if(GetPacketVersion() >= g_PacketWeaponSoundFireEvent_v1 && pPed->GetVehiclePedInside())
			{				
				int seatIdx = pPed->GetMyVehicle()->GetSeatManager()->GetPedsSeatIndex(pPed);
				weaponEvent.weapon = GetVehicleWeapon(pPed->GetMyVehicle(), seatIdx);
			}
			
			// No vehicle weapon use peds weapon
			if(weaponEvent.weapon == NULL)
			{
				pedAttachmentExtension = pPed->GetAttachmentExtension();
			}

			if(pPed->GetPedAudioEntity())
			{
				pPed->GetPedAudioEntity()->SetGunfightVolumeOffset(m_gunFightVolumeOffset); 
			}
		}
		else if(info.parentEntity->GetIsTypeVehicle())
		{
			CVehicle* pVehicle = static_cast<CVehicle*>(info.parentEntity.Get());

			// Use the vehicle weapon if possible for this seat position
			if(GetPacketVersion() >= g_PacketWeaponSoundFireEvent_v1 && m_seatIdx != -1)
			{
				weaponEvent.weapon = GetVehicleWeapon(pVehicle, m_seatIdx);
			}

			// No vehicle weapon use peds weapon
			if(pVehicle->GetDriver() && weaponEvent.weapon == NULL)
			{
				pedAttachmentExtension = pVehicle->GetDriver()->GetAttachmentExtension();
			}
		}

		// No vehicle weapon, use the attachment system
		if(pedAttachmentExtension)
		{
			CPhysical* pNextChild = (CPhysical *) pedAttachmentExtension->GetChildAttachment();
			while(pNextChild)
			{
				if(pNextChild->GetIsTypeObject())
				{
					CObject* pObject = static_cast<CObject*>(pNextChild);
					if(pObject && pObject->GetWeapon())
					{
						weaponEvent.weapon = pObject->GetWeapon();
						break;
					}
				}
				fwAttachmentEntityExtension *nextChildAttachExt = pNextChild->GetAttachmentExtension();
				pNextChild = (CPhysical *) nextChildAttachExt->GetSiblingAttachment();
			}		
		}

		if(!replayVerifyf(weaponEvent.weapon, "CPacketWeaponSoundFireEvent::Extract unable to find weapon %i", m_weaponHash))
		{
			return;
		}
	}

	g_WeaponAudioEntity.PlayWeaponFireEvent(&weaponEvent);
}

CWeapon* CPacketWeaponSoundFireEventOld::GetVehicleWeapon(CVehicle* pVehicle, int seatIdx) const
{
	if(!pVehicle || !pVehicle->GetVehicleWeaponMgr())
	{
		return NULL;
	}
	
	atArray<CVehicleWeapon*> weapons;
	pVehicle->GetVehicleWeaponMgr()->GetWeaponsForSeat(seatIdx, weapons);
	for(int i = 0; i < weapons.GetCount(); ++i)
	{
		if(!weapons[i])
			continue;
			
		if(weapons[i]->GetType() == VGT_FIXED_VEHICLE_WEAPON && weapons[i]->GetHash() == m_weaponHash)
		{
			return static_cast<CFixedVehicleWeapon*>(weapons[i])->GetWeapon();
		}
		else if(weapons[i]->GetType() == VGT_VEHICLE_WEAPON_BATTERY)
		{
			CVehicleWeaponBattery* pWeaponBattery = static_cast<CVehicleWeaponBattery*>(weapons[i]);
			for(int j = 0; j < pWeaponBattery->GetNumWeaponsInBattery(); j++)
			{
				CVehicleWeapon* pWeapon = pWeaponBattery->GetVehicleWeapon(j);

				if(pWeapon && pWeapon->GetHash() == m_weaponHash)
				{
					return static_cast<CFixedVehicleWeapon*>(pWeapon)->GetWeapon();
				}
			}
		}
	}
	return NULL;
}


//////////////////////////////////////////////////////////////////////////
CPacketWeaponPersistant::CPacketWeaponPersistant(const audWeaponFireInfo& info, eWeaponEffectGroup weaponEffectGroup, f32 fgunFightVolumeOffset, bool forceSingleShot, u32 lastPlayerHitTime, audSimpleSmoother hitPlayerSmoother)
	: CPacketEventTracked(PACKETID_SOUND_WEAPON_PERSIST, sizeof(CPacketWeaponPersistant))
	, CPacketEntityInfo()
	, m_isSuppressed(info.isSuppressed)
	, m_weaponEffectGroup(weaponEffectGroup)
	, m_settingsHash(info.settingsHash)
	, m_gunFightVolumeOffset(fgunFightVolumeOffset)
	, m_forceSingleShot(forceSingleShot)
	, m_lastPlayerHitTime(lastPlayerHitTime)
	, m_hitPlayerSmoother(hitPlayerSmoother)
{}


//////////////////////////////////////////////////////////////////////////
void CPacketWeaponPersistant::Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	REPLAY_CHECK(pData.pEntity[0] != NULL, NO_RETURN_VALUE, "CPacketWeaponPersistant entity shouldn't be NULL");

	audWeaponFireInfo info;
	info.parentEntity		= pData.pEntity[0];
	info.audioSettingsRef   = audNorthAudioEngine::GetMetadataManager().GetObjectMetadataRefFromHash(m_settingsHash);
	info.settingsHash		= m_settingsHash;
	info.isSuppressed		= m_isSuppressed;
	info.forceSingleShot	= m_forceSingleShot;
	info.lastPlayerHitTime	= m_lastPlayerHitTime;
	info.hitPlayerSmoother	= m_hitPlayerSmoother;


	if(pData.pEntity[0] && pData.pEntity[0]->GetIsTypePed())
	{
		CPed* pPed = static_cast<CPed*>(pData.pEntity[0]);

		if(pPed->GetPedAudioEntity())
		{
			pPed->GetPedAudioEntity()->SetGunfightVolumeOffset(m_gunFightVolumeOffset); 
		}
	}

	g_WeaponAudioEntity.StartAutomaticFire(info, &pData.m_pEffect[0].m_pSound, &pData.m_pEffect[1].m_pSound, m_weaponEffectGroup, &pData.m_pEffect[2].m_pSound);
}


//////////////////////////////////////////////////////////////////////////
CPacketWeaponPersistantStop::CPacketWeaponPersistantStop(const audWeaponFireInfo& info)
	: CPacketEventTracked(PACKETID_SOUND_WEAPON_PERSIST_STOP, sizeof(CPacketWeaponPersistantStop))
	, CPacketEntityInfo()
	, m_IsSuppressed(info.isSuppressed)
	, m_settingsHash(info.settingsHash)
{}


//////////////////////////////////////////////////////////////////////////
void CPacketWeaponPersistantStop::Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const
{
	if(pData.m_pEffect[0].m_pSound == NULL)
		return;

	audWeaponFireInfo info;
	info.parentEntity		= pData.pEntity[0];
	info.audioSettingsRef	= audNorthAudioEngine::GetMetadataManager().GetObjectMetadataRefFromHash(m_settingsHash);
	info.isSuppressed		= m_IsSuppressed;
	info.settingsHash		= m_settingsHash;

	g_WeaponAudioEntity.StopAutomaticFire(info, &pData.m_pEffect[0].m_pSound, &pData.m_pEffect[1].m_pSound, NULL);

	pData.m_pEffect[0].m_pSound = pData.m_pEffect[1].m_pSound = NULL;
}

CPacketWeaponAutoFireStop::CPacketWeaponAutoFireStop(bool stop)
	:CPacketEvent(PACKETID_SOUND_WEAPON_AUTO_FIRE_STOP, sizeof(CPacketWeaponAutoFireStop))
	, m_Stop(stop)
{
}

void CPacketWeaponAutoFireStop::Extract(CEventInfo<CEntity>& pData) const
{
	if(pData.GetEntity(0) && pData.GetEntity(0)->GetIsTypePed())
	{
		CPed* ped = (CPed*)pData.GetEntity(0);
		if(ped->GetPedAudioEntity())
		{
			ped->GetPedAudioEntity()->SetTriggerAutoWeaponStopForReplay(m_Stop);
		}
	}
}


tPacketVersion g_PacketWeaponSpinUp_v1 = 1;
tPacketVersion g_PacketWeaponSpinUp_v2 = 2;
tPacketVersion g_PacketWeaponSpinUp_v3 = 3;
//////////////////////////////////////////////////////////////////////////
CPacketWeaponSpinUp::CPacketWeaponSpinUp(bool skipSpinUp, s32 boneIndex, bool forceNPCVersion)
	: CPacketEventTracked(PACKETID_SOUND_WEAPON_SPINUP, sizeof(CPacketWeaponSpinUp), g_PacketWeaponSpinUp_v3)
	, CPacketEntityInfo()
	, m_SkipSpinUp(skipSpinUp)
	, m_BoneIndex(boneIndex)
	, m_ForceNPCVersion(forceNPCVersion)
{

}


//////////////////////////////////////////////////////////////////////////
void CPacketWeaponSpinUp::Extract(CTrackedEventInfo<tTrackedSoundType>& pData) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}
	
	REPLAY_CHECK(pData.pEntity[0] != NULL, NO_RETURN_VALUE, "CPacketWeaponSpinUp entity shouldn't be NULL");

	if(GetPacketVersion() >= g_PacketWeaponSpinUp_v3)
	{
		g_WeaponAudioEntity.PlaySpinUpSound((const CEntity*)pData.pEntity[0], &pData.m_pEffect[0].m_pSound, m_ForceNPCVersion, m_SkipSpinUp, m_BoneIndex);
	}
	else if(GetPacketVersion() >= g_PacketWeaponSpinUp_v2)
	{
		g_WeaponAudioEntity.PlaySpinUpSound((const CEntity*)pData.pEntity[0], &pData.m_pEffect[0].m_pSound, false, m_SkipSpinUp, m_BoneIndex);
	}
	else if(GetPacketVersion() >= g_PacketWeaponSpinUp_v1)
	{
		g_WeaponAudioEntity.PlaySpinUpSound((const CEntity*)pData.pEntity[0], &pData.m_pEffect[0].m_pSound, false, m_SkipSpinUp, -1);
	}
	else
	{
		g_WeaponAudioEntity.PlaySpinUpSound((const CEntity*)pData.pEntity[0], &pData.m_pEffect[0].m_pSound, false, false, -1);
	}
}


//////////////////////////////////////////////////////////////////////////
CPacketBulletImpact::CPacketBulletImpact(const u32 hash, const u32 slowMoHash, const Vector3 &position, const f32 volume)
	: CPacketEvent(PACKETID_SOUND_BULLETIMPACT, sizeof(CPacketBulletImpact))
{
	m_Position.Store(position);
	m_Volume = volume;
	m_SoundHash = hash;
	m_SlowMoSoundHash = slowMoHash;
}

//////////////////////////////////////////////////////////////////////////
void CPacketBulletImpact::Extract(CEventInfo<void>&) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	Vector3 position;
	m_Position.Load(position);

	audSoundInitParams initParams;
	initParams.Position = position;
	initParams.Volume = m_Volume;

	if(audNorthAudioEngine::IsInSlowMoVideoEditor() && m_SlowMoSoundHash != g_NullSoundHash)
	{
		g_CollisionAudioEntity.CreateAndPlaySound(m_SlowMoSoundHash, &initParams);
	}
	else
	{
		g_CollisionAudioEntity.CreateAndPlaySound(m_SoundHash, &initParams);
	}
}

//////////////////////////////////////////////////////////////////////////
CPacketBulletImpactNew::CPacketBulletImpactNew(const audSoundSetFieldRef soundRef, const audSoundSetFieldRef slowMoSoundRef, const Vector3 &position, const f32 volume)
	: CPacketEvent(PACKETID_SOUND_BULLETIMPACT_NEW, sizeof(CPacketBulletImpactNew))
{
	m_Position.Store(position);
	m_Volume = volume;
	m_SoundSetFieldRef = soundRef.Get();
	m_SlowMoSoundsetFieldRef = slowMoSoundRef.Get();
}

//////////////////////////////////////////////////////////////////////////
void CPacketBulletImpactNew::Extract(CEventInfo<void>&) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	Vector3 position;
	m_Position.Load(position);

	audSoundInitParams initParams;
	initParams.Position = position;
	initParams.Volume = m_Volume;

	audSoundSetFieldRef slowMoSoundSetFieldRef = audSoundSetFieldRef::Create(m_SlowMoSoundsetFieldRef);

	if(audNorthAudioEngine::IsInSlowMoVideoEditor() && !slowMoSoundSetFieldRef.EqualsHash(g_NullSoundHash))
	{
		g_CollisionAudioEntity.CreateAndPlaySound(slowMoSoundSetFieldRef.GetMetadataRef(), &initParams);
	}
	else
	{
		g_CollisionAudioEntity.CreateAndPlaySound(audSoundSetFieldRef::Create(m_SoundSetFieldRef).GetMetadataRef(), &initParams);
	}
}

//////////////////////////////////////////////////////////////////////////
CPacketThermalScopeAudio::CPacketThermalScopeAudio(const bool activate)
	: CPacketEvent(PACKETID_SOUND_THERMAL_SCOPE, sizeof(CPacketThermalScopeAudio))
{
	m_Activate = activate;
}

//////////////////////////////////////////////////////////////////////////
void CPacketThermalScopeAudio::Extract(CEventInfo<void>&) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}
	g_WeaponAudioEntity.ToggleThermalVision(m_Activate, true);
}


//////////////////////////////////////////////////////////////////////////
CPacketSoundPlayerWeaponReport::CPacketSoundPlayerWeaponReport(audSoundInitParams& initParams, u32 soundHash, f32* fVolume, s32* sPredelay, Vector3* vPos, const Vec3V& firePos, float fDist, f32 interiorVolumeScaling)
	: CPacketEventTracked(PACKETID_SOUND_PLAYERWEAPONREPORT, sizeof(CPacketSoundPlayerWeaponReport))
{
	m_initParams = initParams;
	m_soundHash = soundHash;
	for(int i = 0; i < 4; ++i)
	{
		m_fVolume[i] = fVolume[i];
		m_sPredelay[i] = sPredelay[i];
		m_vPos[i].Store(vPos[i]);
	}
	m_firePos.Store(RCC_VECTOR3(firePos));
	m_dist = fDist;
	m_interiorVolumeScaling = interiorVolumeScaling;
}

//////////////////////////////////////////////////////////////////////////
bool CPacketSoundPlayerWeaponReport::Extract(const CEventInfo<CEntity>& eventInfo) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return false;
	}

	CEntity* pEntity = eventInfo.GetEntity();
	(void)pEntity;

	Vector3 vPos[4];
	for(int i = 0; i < 4; ++i)
	{
		m_vPos[i].Load(vPos[i]);
	}
	Vector3 firePos;
	m_firePos.Load(firePos);

	// 	g_WeaponAudioEntity.PlayPlayerWeaponReport(m_initParams, 
	// 												m_soundHash, 
	// 												m_fVolume, 
	// 												m_sPredelay, 
	// 												vPos, 
	// 												VECTOR3_TO_VEC3V(firePos), 
	// 												m_dist, 
	// 												m_interiorVolumeScaling);

	return true;
}

tPacketVersion g_PacketSndLoadWeaponDataVersion_v1 = 1;
//////////////////////////////////////////////////////////////////////////
CPacketSndLoadWeaponData::CPacketSndLoadWeaponData(u32 weaponHash, u32 audioHash, u32 previousHash)
	: CPacketEvent(PACKETID_SND_LOADWEAPONDATA, sizeof(CPacketSndLoadWeaponData), g_PacketSndLoadWeaponDataVersion_v1)
	, CPacketEntityInfo()
	, m_weaponHash(weaponHash)
	, m_audioHash(audioHash)
	, m_previousHash(previousHash)
{

}

//////////////////////////////////////////////////////////////////////////
void CPacketSndLoadWeaponData::Extract(const CEventInfo<void>& eventInfo) const
{
	if(GetPacketVersion() >= g_PacketSndLoadWeaponDataVersion_v1 && eventInfo.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_BACK))
	{
		g_WeaponAudioEntity.AddPlayerEquippedWeapon(m_weaponHash, m_previousHash);
	}
	else
	{
		g_WeaponAudioEntity.AddPlayerEquippedWeapon(m_weaponHash, m_audioHash);
	}

	if(eventInfo.GetPlaybackFlags().IsPrecaching() || eventInfo.GetPlaybackFlags().IsSet(REPLAY_CURSOR_JUMP))
	{
		g_WeaponAudioEntity.ProcessCachedWeaponFireEvents();
	}
}


CPacketSoundNitrousActive::CPacketSoundNitrousActive(bool active)
	:CPacketEvent(PACKETID_SOUND_VEHICLE_NITROUS_ACTIVE, sizeof(CPacketSoundNitrousActive))
	, m_Active(active)
{
}

void CPacketSoundNitrousActive::Extract(CEventInfo<CVehicle>& pData) const
{
	if(pData.GetEntity(0) && pData.GetEntity(0)->GetIsTypeVehicle())
	{
		CVehicle* vehicle = (CVehicle*)pData.GetEntity(0);
		if(vehicle->GetVehicleAudioEntity())
		{
			vehicle->GetVehicleAudioEntity()->SetBoostActive(m_Active);
		}
	}
}



#endif // GTA_REPLAY
