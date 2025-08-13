#include "LightningFxPacket.h"

#if GTA_REPLAY


//////////////////////////////////////////////////////////////////////////
CPacketDirectionalLightningFxPacket::CPacketDirectionalLightningFxPacket(const Vector2& rootOrbitPos, u8 blastCount, LightningBlast* pBlasts, const Vector3& direction, float flashAmount)
	: CPacketEvent(PACKETID_DIRECTIONALLIGHTNING, sizeof(CPacketDirectionalLightningFxPacket))
{
	m_rootOrbitPos.Store(rootOrbitPos);
	m_blastCount = blastCount;

	for(u8 i = 0; i < blastCount; ++i)
	{
		m_lightningBlasts[i].m_OrbitPos.Store(pBlasts[i].m_OrbitPos);
		m_lightningBlasts[i].m_Intensity = pBlasts[i].m_Intensity;
		m_lightningBlasts[i].m_Duration = pBlasts[i].m_Duration;
	}

	m_flashDirection.Store(direction);
	m_flashAmount = flashAmount;
}


//////////////////////////////////////////////////////////////////////////
void CPacketDirectionalLightningFxPacket::Extract(const CEventInfo<void>&) const
{
	Vector2 rootOrbitPos;
	m_rootOrbitPos.Load(rootOrbitPos);
	Vector3 flashDirection;
	m_flashDirection.Load(flashDirection);

	g_vfxLightningManager.CreateDirectionalBurstSequence(rootOrbitPos, m_blastCount, m_lightningBlasts, flashDirection, m_flashAmount);
}



//////////////////////////////////////////////////////////////////////////
CPacketCloudLightningFxPacket::CPacketCloudLightningFxPacket(const Vector2& rootOrbitPos, u8 dominantIndex, int lightCount, LightingBurstSequence* pSequence)
	: CPacketEvent(PACKETID_CLOUDLIGHTNING, sizeof(CPacketCloudLightningFxPacket))
{
	m_rootOrbitPos.Store(rootOrbitPos);
	m_dominantIndex	= dominantIndex;

	for(int i = 0; i < lightCount; ++i)
	{
		m_cloudBursts[i].m_rootOrbitPos.Store(pSequence[i].m_OrbitRoot);

		m_cloudBursts[i].m_delay		= pSequence[i].m_Delay;
		m_cloudBursts[i].m_blastCount	= (u8)pSequence[i].m_BlastCount;
		for(u8 blast = 0; blast < m_cloudBursts[i].m_blastCount; ++blast)
		{
			m_cloudBursts[i].m_lightningBlasts[blast].m_OrbitPos.Store(pSequence[i].m_LightningBlasts[blast].m_OrbitPos);
			m_cloudBursts[i].m_lightningBlasts[blast].m_size		= pSequence[i].m_LightningBlasts[blast].m_Size;
			m_cloudBursts[i].m_lightningBlasts[blast].m_Intensity	= pSequence[i].m_LightningBlasts[blast].m_Intensity;
			m_cloudBursts[i].m_lightningBlasts[blast].m_Duration	= pSequence[i].m_LightningBlasts[blast].m_Duration;
		}
	}
	m_burstCount = (u8)lightCount;
}


//////////////////////////////////////////////////////////////////////////
void CPacketCloudLightningFxPacket::Extract(const CEventInfo<void>&) const
{
	Vector2 rootOrbitPos;
	m_rootOrbitPos.Load(rootOrbitPos);

	g_vfxLightningManager.CreateCloudLightningSequence(rootOrbitPos, m_dominantIndex, m_burstCount, m_cloudBursts);
}



//////////////////////////////////////////////////////////////////////////
CPacketLightningStrikeFxPacket::CPacketLightningStrikeFxPacket(const Vec3V& flashDir, const CVfxLightningStrike& strike)
	: CPacketEvent(PACKETID_LIGHTNINGSTRIKE, sizeof(CPacketLightningStrikeFxPacket))
{
	m_flashDirection.Store(VEC3V_TO_VECTOR3(flashDir));

	const SLightningSegments& segments = strike.GetSegments();
	for(int i = 0; i < segments.GetCount(); ++i)
	{
		m_segments[i].distanceFromSource	= segments[i].distanceFromSource;
		m_segments[i].endPos				= segments[i].endPos;
		m_segments[i].Intensity				= segments[i].Intensity;
		m_segments[i].IsMainBoltSegment		= segments[i].IsMainBoltSegment;
		m_segments[i].startPos				= segments[i].startPos;
		m_segments[i].width					= segments[i].width;
	}
	m_numSegments					= (u8)segments.GetCount();

	m_duration						= strike.GetDuration();

	m_keyframeLerpMainIntensity		= strike.GetMainIntensityKeyFrameLerp();
	m_keyframeLerpBranchIntensity	= strike.GetBranchIntensityKeyFrameLerp();
	m_keyframeLerpBurst				= strike.GetBurstIntensityKeyFrameLerp();
	m_intensityFlickerMult			= strike.GetMainFlickerIntensity();
	m_colour						= strike.GetColor();
	m_horizDir						= strike.GetHorizDir();
	m_height						= strike.GetHeight();
	m_distance						= strike.GetDistance();
	m_animationTime					= strike.GetAnimationTime();
	m_isBurstLightning				= strike.IsBurstLightning();
}


//////////////////////////////////////////////////////////////////////////
void CPacketLightningStrikeFxPacket::Extract(const CEventInfo<void>&) const
{
	CVfxLightningStrike& strike = g_vfxLightningManager.GetLightningStrike();

	SLightningSegments& segments = strike.GetSegments();
	segments.clear();
	for(int i = 0; i < m_numSegments; ++i)
	{
		SLightningSegment& segment = segments.Append();
		segment.distanceFromSource	= m_segments[i].distanceFromSource;
		segment.endPos				= m_segments[i].endPos;
		segment.Intensity			= m_segments[i].Intensity;
		segment.IsMainBoltSegment	= m_segments[i].IsMainBoltSegment;
		segment.startPos			= m_segments[i].startPos;
		segment.width				= m_segments[i].width;
	}
	strike.SetDuration(m_duration);
	strike.SetMainIntensityKeyFrameLerp(m_keyframeLerpMainIntensity);
	strike.SetBranchIntensityKeyFrameLerp(m_keyframeLerpBranchIntensity);
	strike.SetBurstIntensityKeyFrameLerp(m_keyframeLerpBurst);
	strike.SetMainFlickerIntensity(m_intensityFlickerMult);
	strike.SetColor(m_colour);
	strike.SetHeight(m_height);
	strike.SetDistance(m_distance);
	strike.SetAnimationTime(m_animationTime);
	strike.SetIsBurstLightning(m_isBurstLightning);

	Vector2 horizDir;
	m_horizDir.Load(horizDir);
	strike.SetHorizDir(horizDir);

	g_vfxLightningManager.AddLightningStrike();
}

//////////////////////////////////////////////////////////////////////////
CPacketRequestCloudHat::CPacketRequestCloudHat(bool scripted, int index, float transitionTime, bool assumedMaxLength, int prevIndex, float prevTransitionTime, bool prevAssumedMaxLength)
	: CPacketEvent(PACKETID_REQUESTCLOUDHAT, sizeof(CPacketRequestCloudHat))
	, m_scripted(scripted)
	, m_index(index)
	, m_transitionTime(transitionTime)
	, m_assumedMaxLength(assumedMaxLength)
	, m_prevIndex(prevIndex)
	, m_prevTransitionTime(prevTransitionTime)
	, m_prevAssumedMaxLength(prevAssumedMaxLength)
{

}

void CPacketRequestCloudHat::Extract(const CEventInfo<void>& eventInfo) const
{
	if(eventInfo.GetPlaybackFlags().GetState() & REPLAY_DIRECTION_BACK)
	{
		if( m_prevIndex == -1 )	
			return;

		if(m_scripted)				
			CLOUDHATMGR->RequestScriptCloudHat(m_prevIndex, m_prevTransitionTime);
		else
			CLOUDHATMGR->RequestWeatherCloudHat(m_prevIndex, m_prevTransitionTime, m_prevAssumedMaxLength);
	}
	else
	{
		if(m_scripted)
			CLOUDHATMGR->RequestScriptCloudHat(m_index, m_transitionTime);
		else
			CLOUDHATMGR->RequestWeatherCloudHat(m_index, m_transitionTime, m_assumedMaxLength);
	}

}

bool CPacketRequestCloudHat::HasExpired(const CEventInfo<void>&) const
{
	const CloudHatFragContainer &currentCloudHat = CLOUDHATMGR->GetCloudHat(m_index);
	if( currentCloudHat.IsActive() || currentCloudHat.IsTransitioning() )
		return false;
	else 
		return true;
}

//////////////////////////////////////////////////////////////////////////
CPacketUnloadAllCloudHats::CPacketUnloadAllCloudHats()
	: CPacketEvent(PACKETID_UNLOADALLCLOUDHATS, sizeof(CPacketUnloadAllCloudHats))
{

}

void CPacketUnloadAllCloudHats::Extract(const CEventInfo<void>&) const
{				
	CLOUDHATMGR->UnloadAllCloudHats();
}

#endif // GTA_REPLAY
