//=====================================================================================================
// name:		FootStepPacket.cpp
//=====================================================================================================

#include "FootStepPacket.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "control/replay/replayinternal.h"
#include "control/replay/ReplayInterfacePed.h"

#include "audio/northaudioengine.h"
#include "audio/pedaudioentity.h"
#include "peds/ped.h"
#include "peds/rendering/PedVariationPack.h"

//////////////////////////////////////////////////////////////////////////
// Ped foot step sound effect packet
CPacketFootStepSound::CPacketFootStepSound(u8 event)
	: CPacketEvent(PACKETID_SOUND_PEDFOOTSTEP, sizeof(CPacketFootStepSound))
	, CPacketEntityInfo()
	, m_event(event)
{}

//////////////////////////////////////////////////////////////////////////
void CPacketFootStepSound::Extract(const CEventInfo<CPed>& eventInfo) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	CPed* pPed = eventInfo.GetEntity();
	if(pPed)
		pPed->GetPedAudioEntity()->GetFootStepAudio().AddFootstepEvent(static_cast<audFootstepEvent>(m_event));
}


//////////////////////////////////////////////////////////////////////////
// Ped bush sound effect packet
CPacketPedBushSound::CPacketPedBushSound(u8 event)
	: CPacketEvent(PACKETID_SOUND_PEDBUSH, sizeof(CPacketPedBushSound))
	, CPacketEntityInfo()
	, m_event(event)
{}

//////////////////////////////////////////////////////////////////////////
void CPacketPedBushSound::Extract(const CEventInfo<CPed>& eventInfo) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	CPed* pPed = eventInfo.GetEntity();
	if(pPed)
		pPed->GetPedAudioEntity()->GetFootStepAudio().AddBushEvent(static_cast<audFootstepEvent>(m_event));
}


//////////////////////////////////////////////////////////////////////////
// Ped cloth sound effect packet
CPacketPedClothSound::CPacketPedClothSound(u8 event)
	: CPacketEvent(PACKETID_SOUND_PEDCLOTH, sizeof(CPacketPedClothSound))
	, CPacketEntityInfo()
	, m_event(event)
{}

//////////////////////////////////////////////////////////////////////////
void CPacketPedClothSound::Extract(const CEventInfo<CPed>& eventInfo) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	CPed* pPed = eventInfo.GetEntity();
	if(pPed)
		pPed->GetPedAudioEntity()->GetFootStepAudio().AddClothEvent(static_cast<audFootstepEvent>(m_event));
}


//////////////////////////////////////////////////////////////////////////
// Ped petrol can sound effect packet
CPacketPedPetrolCanSound::CPacketPedPetrolCanSound(u8 event)
	: CPacketEvent(PACKETID_SOUND_PEDPETROLCAN, sizeof(CPacketPedPetrolCanSound))
	, CPacketEntityInfo()
	, m_event(event)
{}

//////////////////////////////////////////////////////////////////////////
void CPacketPedPetrolCanSound::Extract(const CEventInfo<CPed>& eventInfo) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	CPed* pPed = eventInfo.GetEntity();
	if(pPed)
		pPed->GetPedAudioEntity()->GetFootStepAudio().AddPetrolCanEvent(static_cast<audFootstepEvent>(m_event),true);
}

//////////////////////////////////////////////////////////////////////////
// Ped molotov sound effect packet
CPacketPedMolotovSound::CPacketPedMolotovSound(u8 event)
	: CPacketEvent(PACKETID_SOUND_PEDMOLOTOV, sizeof(CPacketPedMolotovSound))
	, CPacketEntityInfo()
	, m_event(event)
{}

//////////////////////////////////////////////////////////////////////////
void CPacketPedMolotovSound::Extract(const CEventInfo<CPed>& eventInfo) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	CPed* pPed = eventInfo.GetEntity();
	if(pPed)
		pPed->GetPedAudioEntity()->GetFootStepAudio().AddMolotovEvent(static_cast<audFootstepEvent>(m_event),true);
}

//////////////////////////////////////////////////////////////////////////
// Ped script sweetener sound effect packet
CPacketPedScriptSweetenerSound::CPacketPedScriptSweetenerSound(u8 event)
	: CPacketEvent(PACKETID_SOUND_SCRIPTSWEETENER, sizeof(CPacketPedScriptSweetenerSound))
	, CPacketEntityInfo()
	, m_event(event)
{}

//////////////////////////////////////////////////////////////////////////
void CPacketPedScriptSweetenerSound::Extract(const CEventInfo<CPed>& eventInfo) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	CPed* pPed = eventInfo.GetEntity();
	if(pPed)
		pPed->GetPedAudioEntity()->GetFootStepAudio().AddScriptSweetenerEvent(static_cast<audFootstepEvent>(m_event));
}
//////////////////////////////////////////////////////////////////////////
// Ped water sound effect packet
CPacketPedWaterSound::CPacketPedWaterSound(u8 event)
	: CPacketEvent(PACKETID_SOUND_PEDWATER, sizeof(CPacketPedWaterSound))
	, CPacketEntityInfo()
	, m_event(event)
{}
//////////////////////////////////////////////////////////////////////////
void CPacketPedWaterSound::Extract(const CEventInfo<CPed>& eventInfo) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	CPed* pPed = eventInfo.GetEntity();
	if(pPed)
		pPed->GetPedAudioEntity()->GetFootStepAudio().AddWaterEvent(static_cast<audFootstepEvent>(m_event));
}


//////////////////////////////////////////////////////////////////////////
// Ped slope debris sound effect packet
CPacketPedSlopeDebrisSound::CPacketPedSlopeDebrisSound(u8 event, const Vec3V& slopeDir, float slopeAngle)
	: CPacketEvent(PACKETID_SOUND_PEDSLOPEDEBRIS, sizeof(CPacketPedSlopeDebrisSound))
	, CPacketEntityInfo()
	, m_slopeAngle(slopeAngle)
	, m_event(event)
{
	m_slopeDirection.Store(VEC3V_TO_VECTOR3(slopeDir));
}

//////////////////////////////////////////////////////////////////////////
void CPacketPedSlopeDebrisSound::Extract(const CEventInfo<CPed>& eventInfo) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	CPed* pPed = eventInfo.GetEntity();
	Vector3 slopeDir;
	m_slopeDirection.Load(slopeDir);

	if(pPed)
		pPed->GetPedAudioEntity()->GetFootStepAudio().AddSlopeDebrisEvent(static_cast<audFootstepEvent>(m_event), VECTOR3_TO_VEC3V(slopeDir), m_slopeAngle);
}


//////////////////////////////////////////////////////////////////////////
CPacketPedStandingMaterial::CPacketPedStandingMaterial(u32 materialNameHash)
	: CPacketEvent(PACKETID_SOUND_PEDSTANDINMATERIAL, sizeof(CPacketPedStandingMaterial))
	, CPacketEntityInfo()
	, m_materialNameHash(materialNameHash)
{}

//////////////////////////////////////////////////////////////////////////
void CPacketPedStandingMaterial::Extract(const CEventInfo<CPed>& eventInfo) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	CPed* pPed = eventInfo.GetEntity();
	replayAssertf(pPed, "Ped cannot be NULL in CPacketPedStandingMaterial::Extract");

	if(pPed)
	{
		CollisionMaterialSettings* pMaterialSettings = audNorthAudioEngine::GetObject<CollisionMaterialSettings>(m_materialNameHash);
		replayAssert(pMaterialSettings);
		if(pMaterialSettings != NULL)
		{
			pPed->GetPedAudioEntity()->GetFootStepAudio().SetCurrentMaterialSettings(pMaterialSettings);
		}
	}
}


//////////////////////////////////////////////////////////////////////////
void CPacketPedStandingMaterial::PrintXML(FileHandle handle) const
{
	CPacketEvent::PrintXML(handle);
	CPacketEntityInfo::PrintXML(handle);

	char str[1024];
	snprintf(str, 1024, "\t\t<m_materialNameHash>%u</m_materialNameHash>\n", m_materialNameHash);
	CFileMgr::Write(handle, str, istrlen(str));
}


//////////////////////////////////////////////////////////////////////////
// Ped fall to death sound
CPacketFallToDeath::CPacketFallToDeath()
: CPacketEvent(PACKETID_SOUND_PEDFALLTODEATH, sizeof(CPacketFallToDeath))
, CPacketEntityInfo()
{}

//////////////////////////////////////////////////////////////////////////
void CPacketFallToDeath::Extract(const CEventInfo<CPed>& eventInfo) const
{
	if(!CReplayMgr::IsJustPlaying())
	{
		return;
	}

	CPed* pPed = eventInfo.GetEntity();
	if(pPed)
		pPed->GetPedAudioEntity()->TriggerFallToDeath();
}


#endif // GTA_REPLAY
