//  
// audio/audshoreline.cpp
//  
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 


// Game headers
#include "audioengine/curve.h"
#include "audioengine/engine.h"
#include "audioengine/widgets.h"
// Game headers
#include "audshorelineRiver.h"
#include "audio/northaudioengine.h"
#include "audio/gameobjects.h"
#include "debug/vectormap.h"
#include "renderer/water.h"
#include "game/weather.h"
#include "vfx/VfxHelper.h"
#include "peds/ped.h"
#include "audio/weatheraudioentity.h"
#include "scene/world/GameWorldWaterHeight.h"

AUDIO_AMBIENCE_OPTIMISATIONS()

audSoundSet audShoreLineRiver::sm_BrookWeakSoundset;
audSoundSet audShoreLineRiver::sm_BrookStrongSoundset;
audSoundSet audShoreLineRiver::sm_LAWeakSoundset;
audSoundSet audShoreLineRiver::sm_LAStrongSoundset;
audSoundSet audShoreLineRiver::sm_WeakSoundset;
audSoundSet audShoreLineRiver::sm_MediumSoundset;
audSoundSet audShoreLineRiver::sm_StrongSoundset;
audSoundSet audShoreLineRiver::sm_RapidsWeakSoundset;
audSoundSet audShoreLineRiver::sm_RapidsStrongSoundset;

f32 audShoreLineRiver::sm_InnerDistance = 15.f;
f32 audShoreLineRiver::sm_OutterDistance = 20.f;
f32 audShoreLineRiver::sm_MaxDistanceToShore = 50.f;

atRangeArray<audShoreLineRiver::audRiverInfo,AUD_MAX_RIVERS_PLAYING> audShoreLineRiver::sm_ClosestShores;
atRangeArray<audShoreLineRiver*,AUD_MAX_RIVERS_PLAYING> audShoreLineRiver::sm_LastClosestShores;

audCurve audShoreLineRiver::sm_EqualPowerFallCrossFade;
audCurve audShoreLineRiver::sm_EqualPowerRiseCrossFade;

BANK_ONLY(f32 audShoreLineRiver::sm_RiverWidthInCurrentPoint = 1.f;);
BANK_ONLY(u32 audShoreLineRiver::sm_MaxNumPointsPerShore = 32;);

// ----------------------------------------------------------------
// Initialise static member variables
// ----------------------------------------------------------------
audShoreLineRiver::audShoreLineRiver(const ShoreLineRiverAudioSettings* settings) : audShoreLine()
{
	m_Settings = static_cast<const ShoreLineAudioSettings*>(settings);

	m_PrevShoreline = NULL;
	m_NextShoreline = NULL;

	BANK_ONLY(m_ShoreLinePointsWidth.Reset(););
}
// ----------------------------------------------------------------
audShoreLineRiver::~audShoreLineRiver()
{
	m_PrevShoreline = NULL;
	m_NextShoreline = NULL;
}
// ----------------------------------------------------------------
void audShoreLineRiver::InitClass()
{
	sm_BrookWeakSoundset.Init(ATSTRINGHASH("RIVER_BROOK_WEAK_WATER_SOUNDSET", 0x46365994));
	sm_BrookStrongSoundset.Init(ATSTRINGHASH("RIVER_BROOK_STRONG_WATER_SOUNDSET", 0x2988A175));
	sm_LAWeakSoundset.Init(ATSTRINGHASH("RIVER_LA_WEAK_WATER_SOUNDSET", 0xD44030FC));
	sm_LAStrongSoundset.Init(ATSTRINGHASH("RIVER_LA_STRONG_WATER_SOUNDSET", 0x19CE503C));
	sm_WeakSoundset.Init(ATSTRINGHASH("RIVER_WEAK_WATER_SOUNDSET", 0xB8DFD1F0));
	sm_MediumSoundset.Init(ATSTRINGHASH("RIVER_MEDIUM_WATER_SOUNDSET", 0x71686291));
	sm_StrongSoundset.Init(ATSTRINGHASH("RIVER_STRONG_WATER_SOUNDSET", 0xE89D17FF));
	sm_RapidsWeakSoundset.Init(ATSTRINGHASH("RIVER_RAPIDS_WEAK_WATER_SOUNDSET", 0x4444420B));
	sm_RapidsStrongSoundset.Init(ATSTRINGHASH("RIVER_RAPIDS_STRONG_WATER_SOUNDSET", 0xC0D452A));

	sm_EqualPowerFallCrossFade.Init(ATSTRINGHASH("EQUAL_POWER_FALL_CROSSFADE", 0xEB62241E));
	sm_EqualPowerRiseCrossFade.Init(ATSTRINGHASH("EQUAL_POWER_RISE_CROSSFADE", 0x81841DEC));

	for (u8 i = 0; i < AUD_MAX_RIVERS_PLAYING; i++)
	{
		sm_ClosestShores[i].prevNode = VEC3_ZERO;
		sm_ClosestShores[i].closestNode = VEC3_ZERO;
		sm_ClosestShores[i].nextNode = VEC3_ZERO;

		for (u32 j = 0; j < NumWaterSoundPos;j ++)
		{
			sm_ClosestShores[i].riverSounds.sounds[0][j] = NULL;
			sm_ClosestShores[i].riverSounds.sounds[1][j] = NULL;
			sm_ClosestShores[i].riverSounds.currentShore[j] = NULL;
			sm_ClosestShores[i].riverSounds.linkRatio[j] = 0.f;
			sm_ClosestShores[i].riverSounds.currentSoundIdx[j] = 0;
			sm_ClosestShores[i].riverSounds.ending[j] = false;
		}
		for (u8 j = 0; j < NumWaterHeights;j ++)
		{
			sm_ClosestShores[i].riverSounds.waterHeights[j] = -9999.f;
		}
		sm_ClosestShores[i].river = NULL;

		sm_ClosestShores[i].distanceToClosestNode = LARGE_FLOAT;
		sm_ClosestShores[i].distanceToShore = 0.f;
		sm_ClosestShores[i].currentWidth = 0.f;
		sm_ClosestShores[i].prevNodeWidth = 0.f;
		sm_ClosestShores[i].closestNodeWidth = 0.f;
		sm_ClosestShores[i].nextNodeWidth = 0.f;

		sm_ClosestShores[i].closestNodeIdx = 0;
		sm_ClosestShores[i].prevNodeIdx = 0;
		sm_ClosestShores[i].nextNodeIdx = 0;

		sm_LastClosestShores[i] = NULL;
	}
}
// ----------------------------------------------------------------
void audShoreLineRiver::ShutDownClass()
{
	for (u8 i = 0; i < AUD_MAX_RIVERS_PLAYING; i++)
	{
		for (u32 j = 0; j < NumWaterSoundPos;j ++)
		{
			if(sm_ClosestShores[i].riverSounds.sounds[0][j])
			{
				sm_ClosestShores[i].riverSounds.sounds[0][j]->StopAndForget();
			}
			if(sm_ClosestShores[i].riverSounds.sounds[1][j])
			{
				sm_ClosestShores[i].riverSounds.sounds[1][j]->StopAndForget();
			}
		}
		sm_ClosestShores[i].river = NULL;
		sm_LastClosestShores[i] = NULL;
	}
}
// ----------------------------------------------------------------
// Load shoreline data from a file
// ----------------------------------------------------------------
void audShoreLineRiver::Load(BANK_ONLY(const ShoreLineAudioSettings *settings))
{
	audShoreLine::Load(BANK_ONLY(settings));

	Vector2 center = Vector2(GetRiverSettings()->ActivationBox.Center.X,GetRiverSettings()->ActivationBox.Center.Y);
	m_ActivationBox = fwRect(center,GetRiverSettings()->ActivationBox.Size.Width/2.f,GetRiverSettings()->ActivationBox.Size.Height/2.f);
	BANK_ONLY(m_RotAngle = GetRiverSettings()->ActivationBox.RotationAngle;)
#if __BANK
	m_Width = GetRiverSettings()->ActivationBox.Size.Width;
	m_Height = GetRiverSettings()->ActivationBox.Size.Height;
#endif
}
// ----------------------------------------------------------------
// Update the shoreline activation
// ----------------------------------------------------------------
bool audShoreLineRiver::UpdateActivation()
{
	if(audShoreLine::UpdateActivation())
	{
		if(!m_IsActive)
		{
			m_IsActive = true;
			m_JustActivated = true;
		}
		ComputeClosestShores();
		return true;
	}
	m_DistanceToShore = LARGE_FLOAT;
	m_IsActive = false;
	for(u8 i = 0; i < AUD_MAX_RIVERS_PLAYING; i ++)
	{
		if(IsClosestShore(i))
		{
			for (u32 j = 0; j < NumWaterSoundPos; j ++)
			{
				if(sm_ClosestShores[i].riverSounds.sounds[0][j])
				{
					sm_ClosestShores[i].riverSounds.sounds[0][j]->StopAndForget();
				}
				if(sm_ClosestShores[i].riverSounds.sounds[1][j])
				{
					sm_ClosestShores[i].riverSounds.sounds[1][j]->StopAndForget();
				}
			}
		}
	}
	return false;
}
// ----------------------------------------------------------------
void audShoreLineRiver::ComputeClosestShores()
{
	// Look for the closest point
	f32 sqdDistance = LARGE_FLOAT;
	u8 closestNodeIdx = 0;
	CalculateClosestShoreInfo(sqdDistance,closestNodeIdx);
	s8 closestRiverIdx = -1;
	for (u8 i = 0; i < AUD_MAX_RIVERS_PLAYING; i++)
	{
		bool linkedToClosestRiver = LinkedToClosestRiver(i);
		if(linkedToClosestRiver)
		{
			continue;
		}
		else if((sqdDistance < sm_ClosestShores[i].distanceToClosestNode))
		{
			if(!sm_ClosestShores[i].river || (sm_ClosestShores[i].river  && IsLinkedRiver(sm_ClosestShores[i].river)))
			{
				closestRiverIdx = i;
				break;
			}
		}
	}
	if(closestRiverIdx != -1)
	{
		naAssert(closestRiverIdx < AUD_MAX_RIVERS_PLAYING);
		sm_ClosestShores[closestRiverIdx].distanceToClosestNode = sqdDistance;
		sm_ClosestShores[closestRiverIdx].closestNodeIdx = closestNodeIdx;
		sm_ClosestShores[closestRiverIdx].river = this;
	}
}	
// ----------------------------------------------------------------
// This function returns true if the current shoreline ( this ) is linked or = to river
bool audShoreLineRiver::IsPrevShorelineLinked(const audShoreLineRiver* river)
{
	bool isLinked = false;
	if(river)
	{
		if(river == this)
		{
			isLinked = true; 
		}
		else if(m_PrevShoreline)
		{
			isLinked = m_PrevShoreline->IsPrevShorelineLinked(river);
		}
	}
	return isLinked; 
}
// ----------------------------------------------------------------
bool audShoreLineRiver::IsNextShorelineLinked(const audShoreLineRiver* river)
{
	bool isLinked = false;
	if(river)
	{
		if(river == this)
		{
			isLinked = true; 
		}
		else if(m_NextShoreline)
		{
			isLinked = m_NextShoreline->IsPrevShorelineLinked(river);
		}
	}
	return isLinked; 
}
// ----------------------------------------------------------------
bool audShoreLineRiver::IsLinkedRiver(const audShoreLineRiver* river)
{
	bool isLinked = false;
	if(river)
	{
		if(river == this)
		{
			isLinked = true; 
		}
		else
		{
			if(m_PrevShoreline)
			{
				isLinked = m_PrevShoreline->IsPrevShorelineLinked(river);	
			}
			if(!isLinked && m_NextShoreline)
			{
				isLinked = m_NextShoreline->IsNextShorelineLinked(river);	
			}
		}
	}
	return isLinked; 
}
// ----------------------------------------------------------------
// This function returns if the current shoreline (this) belongs to any closest river != avoidChecking
bool audShoreLineRiver::LinkedToClosestRiver(const u8 avoidChecking)
{
	for (u8 i = 0; i < AUD_MAX_RIVERS_PLAYING; i++)
	{
		if ( i != avoidChecking )
		{
			if(IsLinkedRiver(sm_ClosestShores[i].river))
			{
				return true;
			}
		}
	}
	return false; 
}
// ----------------------------------------------------------------
void audShoreLineRiver::ResetDistanceToShore()
{
	for (u8 i = 0; i < AUD_MAX_RIVERS_PLAYING; i++)
	{
		sm_ClosestShores[i].distanceToClosestNode = LARGE_FLOAT;
		sm_LastClosestShores[i] = sm_ClosestShores[i].river;
		sm_ClosestShores[i].river = NULL;
	}
} 
// ----------------------------------------------------------------
void audShoreLineRiver::CalculateClosestShoreInfo(f32 &sqdDistance, u8 &closestNodeIdx)
{
	Vector3 pos2D = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition());
	pos2D.z = 0.0f; 
	for(u8 i = 0; i < GetRiverSettings()->numShoreLinePoints; i++)
	{
		const f32 sqdDist = (Vector3(GetRiverSettings()->ShoreLinePoints[i].X,GetRiverSettings()->ShoreLinePoints[i].Y,0.f)-pos2D).Mag2();
		if(sqdDist < sqdDistance)
		{
			sqdDistance = sqdDist;
			closestNodeIdx = i;
		}
	}
}
// ----------------------------------------------------------------
audShoreLineRiver* audShoreLineRiver::FindFirstReference()
{
	if(!m_PrevShoreline)
	{
		return this;
	}
	else 
	{
		return m_PrevShoreline->FindFirstReference();
	}
}
// ----------------------------------------------------------------
audShoreLineRiver* audShoreLineRiver::FindLastReference()
{
	if(!m_NextShoreline)
	{
		return this;
	}
	else 
	{
		return m_NextShoreline->FindLastReference();
	}
}
// ----------------------------------------------------------------
// Check if a point is over the water
// ----------------------------------------------------------------
bool audShoreLineRiver::IsPointOverWater(const Vector3& pos) const
{
	// TODO: Can we do a quick bounding box check here with an early return?

	Vector3 pos2D = pos;
	pos2D.z = 0.0f; 

	f32 closestDist2 = LARGE_FLOAT;
	s32 closestNode = -1;
	f32 closestNodeWidth = 0.0f;

	// Find the closest node to the given point
	// Todo - can this search be sped up at all?
	for(s32 i = 0; i < GetRiverSettings()->numShoreLinePoints; i++)
	{
		Vector3 nodePos = Vector3(GetRiverSettings()->ShoreLinePoints[i].X, GetRiverSettings()->ShoreLinePoints[i].Y, 0.f);
		const f32 dist2 = (nodePos - pos2D).Mag2();

		if(dist2 < closestDist2)
		{
			// River width is the whole width across the river - so we only need half when checking from centre->shore
			closestNodeWidth = GetRiverSettings()->ShoreLinePoints[i].RiverWidth * 0.5f;
			closestDist2 = dist2;
			closestNode = i;
		}
	}

	// If the given position is closer to the river centre that the river width, then we're above water
	if(closestNode >= 0)
	{
		if(closestDist2 < (closestNodeWidth * closestNodeWidth))
		{
			return true;
		}
	}

	return false;
}
// ----------------------------------------------------------------
// Update the shoreline 
// ----------------------------------------------------------------
bool audShoreLineRiver::IsClosestShore(u8 &closestRiverIdx)
{
	for(u8 i = 0; i < AUD_MAX_RIVERS_PLAYING; i ++)
	{
		if(sm_ClosestShores[i].river == this)
		{
			closestRiverIdx = i;
			return true;
		}
	}
	return false;
}
// ----------------------------------------------------------------
void audShoreLineRiver::ComputeShorelineNodesInfo(const u8 closestRiverIdx)
{
	const ShoreLineRiverAudioSettings* settings = GetRiverSettings();
	if(naVerifyf(settings, "Failed getting river audio settings"))
	{
		// Calculate closest point and width since we have the closest index already.
		sm_ClosestShores[closestRiverIdx].closestNode = Vector3(settings->ShoreLinePoints[sm_ClosestShores[closestRiverIdx].closestNodeIdx].X,settings->ShoreLinePoints[sm_ClosestShores[closestRiverIdx].closestNodeIdx].Y,sm_ClosestShores[closestRiverIdx].riverSounds.waterHeights[ClosestIndex]);
		sm_ClosestShores[closestRiverIdx].closestNodeWidth = settings->ShoreLinePoints[sm_ClosestShores[closestRiverIdx].closestNodeIdx].RiverWidth;
		// Initialize prev and next point to be the closest one in case we fail getting them.
		sm_ClosestShores[closestRiverIdx].nextNode = sm_ClosestShores[closestRiverIdx].closestNode;
		sm_ClosestShores[closestRiverIdx].nextNodeWidth = sm_ClosestShores[closestRiverIdx].closestNodeWidth;
		sm_ClosestShores[closestRiverIdx].nextNodeIdx = sm_ClosestShores[closestRiverIdx].closestNodeIdx;  
		sm_ClosestShores[closestRiverIdx].prevNode = sm_ClosestShores[closestRiverIdx].closestNode;
		sm_ClosestShores[closestRiverIdx].prevNodeWidth = sm_ClosestShores[closestRiverIdx].closestNodeWidth;
		sm_ClosestShores[closestRiverIdx].prevNodeIdx = sm_ClosestShores[closestRiverIdx].closestNodeIdx;
		// Start computing prev and next nodes.
		sm_ClosestShores[closestRiverIdx].nextNodeIdx = (sm_ClosestShores[closestRiverIdx].closestNodeIdx + 1);
		sm_ClosestShores[closestRiverIdx].prevNodeIdx = (sm_ClosestShores[closestRiverIdx].closestNodeIdx - 1);
		// Inside the current shore.
		if(sm_ClosestShores[closestRiverIdx].closestNodeIdx > 0 && sm_ClosestShores[closestRiverIdx].closestNodeIdx < settings->numShoreLinePoints -1)
		{
			// Next node info
			if(naVerifyf(sm_ClosestShores[closestRiverIdx].nextNodeIdx < settings->numShoreLinePoints,"Next node idx out of bounds" ))
			{
				sm_ClosestShores[closestRiverIdx].nextNode = Vector3(settings->ShoreLinePoints[sm_ClosestShores[closestRiverIdx].nextNodeIdx].X,settings->ShoreLinePoints[sm_ClosestShores[closestRiverIdx].nextNodeIdx].Y,sm_ClosestShores[closestRiverIdx].riverSounds.waterHeights[NextIndex]);
				sm_ClosestShores[closestRiverIdx].nextNodeWidth = settings->ShoreLinePoints[sm_ClosestShores[closestRiverIdx].nextNodeIdx].RiverWidth;
			}
			// Prev node info
			if(naVerifyf(sm_ClosestShores[closestRiverIdx].prevNodeIdx < settings->numShoreLinePoints,"Prev node idx out of bounds" ))
			{
				sm_ClosestShores[closestRiverIdx].prevNode = Vector3(settings->ShoreLinePoints[sm_ClosestShores[closestRiverIdx].prevNodeIdx].X,settings->ShoreLinePoints[sm_ClosestShores[closestRiverIdx].prevNodeIdx].Y,sm_ClosestShores[closestRiverIdx].riverSounds.waterHeights[PrevIndex]);
				sm_ClosestShores[closestRiverIdx].prevNodeWidth = settings->ShoreLinePoints[sm_ClosestShores[closestRiverIdx].prevNodeIdx].RiverWidth;
			}
		}
		// Right at the beginning, we need to check for the prev shore if any.
		else if ( sm_ClosestShores[closestRiverIdx].closestNodeIdx == 0)
		{
			// Next node info
			if(naVerifyf(sm_ClosestShores[closestRiverIdx].nextNodeIdx < settings->numShoreLinePoints,"Next node idx out of bounds" ))
			{
				sm_ClosestShores[closestRiverIdx].nextNode = Vector3(settings->ShoreLinePoints[sm_ClosestShores[closestRiverIdx].nextNodeIdx].X,settings->ShoreLinePoints[sm_ClosestShores[closestRiverIdx].nextNodeIdx].Y,sm_ClosestShores[closestRiverIdx].riverSounds.waterHeights[NextIndex]);
				sm_ClosestShores[closestRiverIdx].nextNodeWidth = settings->ShoreLinePoints[sm_ClosestShores[closestRiverIdx].nextNodeIdx].RiverWidth;
			}
			// Prev node info
			const audShoreLineRiver *prevShoreLine = sm_ClosestShores[closestRiverIdx].river->GetPrevRiverReference();
			if( prevShoreLine && prevShoreLine->GetRiverSettings())
			{
				// We have got a prev shoreline, set the prev node to be the last (of the prev shoreline).
				sm_ClosestShores[closestRiverIdx].prevNode =  Vector3(prevShoreLine->GetRiverSettings()->ShoreLinePoints[prevShoreLine->GetRiverSettings()->numShoreLinePoints -1].X
					,prevShoreLine->GetRiverSettings()->ShoreLinePoints[prevShoreLine->GetRiverSettings()->numShoreLinePoints - 1].Y
					,sm_ClosestShores[closestRiverIdx].closestNode.GetZ());
				sm_ClosestShores[closestRiverIdx].prevNodeIdx = prevShoreLine->GetRiverSettings()->numShoreLinePoints - 1;
				sm_ClosestShores[closestRiverIdx].prevNodeWidth = prevShoreLine->GetRiverSettings()->ShoreLinePoints[prevShoreLine->GetRiverSettings()->numShoreLinePoints -1].RiverWidth;
			}
			else
			{
				// Since we don't have a prev shore, leave the prev node as the closest (0) and move closest and new to make room.
				// This is a bit funky, but it will allow us the stop the PrevSound position at the beginning of the shore.
				naAssertf(sm_ClosestShores[closestRiverIdx].closestNodeIdx == 0, "The closest node should be 0 for this to happen");
				sm_ClosestShores[closestRiverIdx].prevNode =  sm_ClosestShores[closestRiverIdx].closestNode;
				sm_ClosestShores[closestRiverIdx].prevNodeIdx = sm_ClosestShores[closestRiverIdx].closestNodeIdx;
				sm_ClosestShores[closestRiverIdx].prevNodeWidth = sm_ClosestShores[closestRiverIdx].closestNodeWidth;
				sm_ClosestShores[closestRiverIdx].closestNodeIdx ++;
				if(naVerifyf(sm_ClosestShores[closestRiverIdx].closestNodeIdx < settings->numShoreLinePoints,"closest node idx out of bounds" ))
				{
					sm_ClosestShores[closestRiverIdx].closestNode = Vector3(settings->ShoreLinePoints[sm_ClosestShores[closestRiverIdx].closestNodeIdx].X,settings->ShoreLinePoints[sm_ClosestShores[closestRiverIdx].closestNodeIdx].Y,sm_ClosestShores[closestRiverIdx].riverSounds.waterHeights[ClosestIndex]);
					sm_ClosestShores[closestRiverIdx].nextNodeIdx = sm_ClosestShores[closestRiverIdx].closestNodeIdx + 1;
					sm_ClosestShores[closestRiverIdx].closestNodeWidth = settings->ShoreLinePoints[sm_ClosestShores[closestRiverIdx].closestNodeIdx].RiverWidth;
					if(naVerifyf(sm_ClosestShores[closestRiverIdx].nextNodeIdx < settings->numShoreLinePoints,"Next node idx out of bounds" ))
					{
						sm_ClosestShores[closestRiverIdx].nextNode = Vector3(settings->ShoreLinePoints[sm_ClosestShores[closestRiverIdx].nextNodeIdx].X,settings->ShoreLinePoints[sm_ClosestShores[closestRiverIdx].nextNodeIdx].Y,sm_ClosestShores[closestRiverIdx].riverSounds.waterHeights[NextIndex]);
						sm_ClosestShores[closestRiverIdx].nextNodeWidth = settings->ShoreLinePoints[sm_ClosestShores[closestRiverIdx].nextNodeIdx].RiverWidth;
					}
				}
			}

		}
		// Right at the end, we need to check for the next shore if any.
		else if (sm_ClosestShores[closestRiverIdx].closestNodeIdx < settings->numShoreLinePoints)
		{
			// Next node info
			const audShoreLineRiver *nextShoreLine = sm_ClosestShores[closestRiverIdx].river->GetNextRiverReference();
			if( nextShoreLine && nextShoreLine->GetRiverSettings())
			{			
				sm_ClosestShores[closestRiverIdx].nextNode = Vector3(nextShoreLine->GetRiverSettings()->ShoreLinePoints[0].X
					,nextShoreLine->GetRiverSettings()->ShoreLinePoints[0].Y
					,sm_ClosestShores[closestRiverIdx].closestNode.GetZ());
				sm_ClosestShores[closestRiverIdx].nextNodeIdx = 0;
				sm_ClosestShores[closestRiverIdx].nextNodeWidth = nextShoreLine->GetRiverSettings()->ShoreLinePoints[0].RiverWidth;
			}
			else
			{
				// Since we don't have a next shore, leave the next node as the closest (settings->numShoreLinePoints - 1) and move closest and prev to make room.
				// This is a bit funky, but it will allow us the stop the NextSound position at the end of the shore.
				naAssertf(sm_ClosestShores[closestRiverIdx].closestNodeIdx == (settings->numShoreLinePoints - 1), "The closest node should be %u for this to happen",(settings->numShoreLinePoints  - 1));
				sm_ClosestShores[closestRiverIdx].nextNode =  sm_ClosestShores[closestRiverIdx].closestNode;
				sm_ClosestShores[closestRiverIdx].nextNodeIdx = sm_ClosestShores[closestRiverIdx].closestNodeIdx;
				sm_ClosestShores[closestRiverIdx].nextNodeWidth = sm_ClosestShores[closestRiverIdx].closestNodeWidth;
				sm_ClosestShores[closestRiverIdx].closestNodeIdx --;
				if(naVerifyf(sm_ClosestShores[closestRiverIdx].closestNodeIdx < settings->numShoreLinePoints,"closest node idx out of bounds" ))
				{
					sm_ClosestShores[closestRiverIdx].closestNode = Vector3(settings->ShoreLinePoints[sm_ClosestShores[closestRiverIdx].closestNodeIdx].X,settings->ShoreLinePoints[sm_ClosestShores[closestRiverIdx].closestNodeIdx].Y,sm_ClosestShores[closestRiverIdx].riverSounds.waterHeights[ClosestIndex]);
					sm_ClosestShores[closestRiverIdx].closestNodeWidth = settings->ShoreLinePoints[sm_ClosestShores[closestRiverIdx].closestNodeIdx].RiverWidth;
					sm_ClosestShores[closestRiverIdx].prevNodeIdx = sm_ClosestShores[closestRiverIdx].closestNodeIdx - 1;
					if(naVerifyf(sm_ClosestShores[closestRiverIdx].prevNodeIdx < settings->numShoreLinePoints,"Prev node idx out of bounds" ))
					{
						sm_ClosestShores[closestRiverIdx].prevNode = Vector3(settings->ShoreLinePoints[sm_ClosestShores[closestRiverIdx].prevNodeIdx].X,settings->ShoreLinePoints[sm_ClosestShores[closestRiverIdx].prevNodeIdx].Y,sm_ClosestShores[closestRiverIdx].riverSounds.waterHeights[PrevIndex]);
						sm_ClosestShores[closestRiverIdx].prevNodeWidth = settings->ShoreLinePoints[sm_ClosestShores[closestRiverIdx].prevNodeIdx].RiverWidth;
					}
				}
			}
			// Prev node info
			if(naVerifyf(sm_ClosestShores[closestRiverIdx].prevNodeIdx < settings->numShoreLinePoints,"Prev node idx out of bounds" ))
			{
				sm_ClosestShores[closestRiverIdx].prevNode = Vector3(settings->ShoreLinePoints[sm_ClosestShores[closestRiverIdx].prevNodeIdx].X,settings->ShoreLinePoints[sm_ClosestShores[closestRiverIdx].prevNodeIdx].Y,sm_ClosestShores[closestRiverIdx].riverSounds.waterHeights[PrevIndex]);
				sm_ClosestShores[closestRiverIdx].prevNodeWidth = settings->ShoreLinePoints[sm_ClosestShores[closestRiverIdx].prevNodeIdx].RiverWidth;
			}
		}
	}
}
// ----------------------------------------------------------------
void audShoreLineRiver::CalculatePoints(Vector3 &centrePoint,Vector3 &leftPoint,Vector3 &rightPoint, const u8 closestRiverIdx)
{
	ComputeShorelineNodesInfo(closestRiverIdx);
	// Compute the direction to the next and prev node.
	Vector3 lakeDirectionToNextNode,lakeDirectionFromPrevNode;
	lakeDirectionToNextNode = sm_ClosestShores[closestRiverIdx].nextNode - sm_ClosestShores[closestRiverIdx].closestNode;
	lakeDirectionToNextNode.Normalize();

	lakeDirectionFromPrevNode = sm_ClosestShores[closestRiverIdx].closestNode - sm_ClosestShores[closestRiverIdx].prevNode;
	lakeDirectionFromPrevNode.Normalize();

	//Vector3 pos = VEC3V_TO_VECTOR3(CGameWorld::FindLocalPlayer()->GetTransform().GetPosition());
	Vector3 pos = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition());
	pos.z = sm_ClosestShores[closestRiverIdx].closestNode.z;
	Vector3 dirToListener =  pos - sm_ClosestShores[closestRiverIdx].closestNode;

	CalculateCentrePoint(sm_ClosestShores[closestRiverIdx].closestNode,sm_ClosestShores[closestRiverIdx].nextNode,sm_ClosestShores[closestRiverIdx].prevNode,lakeDirectionToNextNode,lakeDirectionFromPrevNode,pos,dirToListener,centrePoint);
	pos = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition());
	//pos.z = centrePoint.z;
	dirToListener = pos - centrePoint;
	Vector3 closestToCentre = sm_ClosestShores[closestRiverIdx].closestNode - centrePoint;
	closestToCentre.Normalize();
	f32 nextDot = fabs(lakeDirectionToNextNode.Dot(closestToCentre));
	f32 prevDot = fabs(lakeDirectionFromPrevNode.Dot(closestToCentre));

	f32 ratio = 1.f;
	sm_ClosestShores[closestRiverIdx].riverSounds.linkRatio[Centre] = 0.f;
	sm_ClosestShores[closestRiverIdx].riverSounds.ending[Centre] = (sm_ClosestShores[closestRiverIdx].closestNodeIdx == 0);
	sm_ClosestShores[closestRiverIdx].riverSounds.currentShore[Centre] = sm_ClosestShores[closestRiverIdx].river;
	if (nextDot > prevDot)
	{
		// Interpolate the width at the centre point
		ratio = 1.f - fabs(((sm_ClosestShores[closestRiverIdx].nextNode - centrePoint).Mag() / (sm_ClosestShores[closestRiverIdx].nextNode - sm_ClosestShores[closestRiverIdx].closestNode).Mag()));
		if(sm_ClosestShores[closestRiverIdx].closestNodeIdx == GetRiverSettings()->numShoreLinePoints - 1)
		{
			sm_ClosestShores[closestRiverIdx].riverSounds.linkRatio[Centre] = ratio;
		}
		sm_ClosestShores[closestRiverIdx].currentWidth = (1.f - ratio) * sm_ClosestShores[closestRiverIdx].closestNodeWidth + ratio * sm_ClosestShores[closestRiverIdx].nextNodeWidth;
	}
	else 
	{
		// Interpolate the width at the centre point
		ratio = 1.f - fabs((sm_ClosestShores[closestRiverIdx].closestNode - centrePoint).Mag() / (sm_ClosestShores[closestRiverIdx].closestNode - sm_ClosestShores[closestRiverIdx].prevNode).Mag());
		if(sm_ClosestShores[closestRiverIdx].closestNodeIdx == 0)
		{
			sm_ClosestShores[closestRiverIdx].riverSounds.linkRatio[Centre] = ratio;
		}
		sm_ClosestShores[closestRiverIdx].currentWidth = (1.f - ratio) * sm_ClosestShores[closestRiverIdx].prevNodeWidth + ratio * sm_ClosestShores[closestRiverIdx].closestNodeWidth;
	}
	sm_ClosestShores[closestRiverIdx].riverSounds.linkRatio[Centre] = Clamp(sm_ClosestShores[closestRiverIdx].riverSounds.linkRatio[Centre],0.f,1.f);
	if( sm_ClosestShores[closestRiverIdx].river->GetRiverSettings() )
	{
		RiverType type = (RiverType)sm_ClosestShores[closestRiverIdx].river->GetRiverSettings()->RiverType;
		if(type == AUD_RIVER_LA_WEAK || type == AUD_RIVER_LA_STRONG)
		{
			// NULL out the Z component for distance since we will do a custom tuning for this later.
			dirToListener.z = 0.f;
		}
	}
	f32 distanceToCentre = fabs(dirToListener.Mag());
	m_DistanceToShore = distanceToCentre * distanceToCentre;
	if(distanceToCentre > sm_DistanceThresholdToStopSounds)
	{
		m_IsActive = false;
		StopSounds(closestRiverIdx);
		return;
	}
	f32 width = sm_ClosestShores[closestRiverIdx].currentWidth * 0.5f;
	sm_ClosestShores[closestRiverIdx].distanceToShore = distanceToCentre - width;
	sm_ClosestShores[closestRiverIdx].distanceToShore = Max(sm_ClosestShores[closestRiverIdx].distanceToShore,0.f);
	f32 distanceToMove = sm_DistToNodeSeparation.CalculateRescaledValue(sm_InnerDistance,sm_OutterDistance,0.f,sm_MaxDistanceToShore,sm_ClosestShores[closestRiverIdx].distanceToShore);

	CalculateLeftPoint(centrePoint,lakeDirectionToNextNode,lakeDirectionFromPrevNode,leftPoint, distanceToMove,closestRiverIdx);
	CalculateRightPoint(centrePoint,lakeDirectionToNextNode,lakeDirectionFromPrevNode,rightPoint, distanceToMove,closestRiverIdx);

#if __BANK 
	if(sm_DrawWaterBehaviour)
	{
		//Draw maths that calculates the point 
		grcDebugDraw::Line(sm_ClosestShores[closestRiverIdx].closestNode, sm_ClosestShores[closestRiverIdx].closestNode + dirToListener, Color_red );
		grcDebugDraw::Sphere(centrePoint + Vector3( 0.f,0.f,1.f),0.3f,Color_green);
		char txt[64];
		formatf(txt, "distanceToCentre %f",distanceToCentre);
		grcDebugDraw::Text(centrePoint + Vector3( 0.f,0.f,1.3f),Color_white,txt);
		formatf(txt, "width %f",width);
		grcDebugDraw::Text(centrePoint + Vector3( 0.f,0.f,1.5f),Color_white,txt);
		formatf(txt, "distanceToShore %f",sm_ClosestShores[closestRiverIdx].distanceToShore);
		grcDebugDraw::Text(centrePoint + Vector3( 0.f,0.f,1.7f),Color_white,txt);
		grcDebugDraw::Sphere(leftPoint + Vector3( 0.f,0.f,1.f),0.3f,Color_green);
		grcDebugDraw::Sphere(rightPoint + Vector3( 0.f,0.f,1.f),0.3f,Color_green);
	}
#endif
}
// ----------------------------------------------------------------
void audShoreLineRiver::CalculateLeftPoint(const Vector3& centrePoint,const Vector3 &lakeDirectionToNextNode, const Vector3 &lakeDirectionFromPrevNode
										  ,Vector3 &leftPoint,const f32 distance, const u8 closestRiverIdx)
{
	//Start from the centre point
	leftPoint = centrePoint;

	// Square the distance for performance purposes.
	f32 distanceToMove = distance * distance;

	Vector3 currentDirection; 
	f32 currentDistance = 0.f;

	// Compute in which (prev or next) direction the centre point is.
	u8 pointIdx = sm_ClosestShores[closestRiverIdx].closestNodeIdx;
	Vector3 closestToCentre = sm_ClosestShores[closestRiverIdx].closestNode - centrePoint;
	closestToCentre.Normalize();
	f32 nextDot = fabs(lakeDirectionToNextNode.Dot(closestToCentre));
	f32 prevDot = fabs(lakeDirectionFromPrevNode.Dot(closestToCentre));

	audShoreLineRiver *currentShoreLine = sm_ClosestShores[closestRiverIdx].river;
	if (nextDot > prevDot)
	{
		currentDirection = lakeDirectionToNextNode;
		currentDistance = (sm_ClosestShores[closestRiverIdx].nextNode - centrePoint).Mag2();
		pointIdx = sm_ClosestShores[closestRiverIdx].nextNodeIdx;
		// if sm_ClosestShores[closestRiverIdx].nextNodeIdx is 0 and we have a next reference, our current shore should be the next.
		if (pointIdx == 0 && sm_ClosestShores[closestRiverIdx].river->GetNextRiverReference())
		{
			currentShoreLine = sm_ClosestShores[closestRiverIdx].river->GetNextRiverReference();
		}
	}
	else 
	{
		currentDirection = lakeDirectionFromPrevNode;
		currentDistance = (sm_ClosestShores[closestRiverIdx].closestNode - centrePoint).Mag2();
		pointIdx = sm_ClosestShores[closestRiverIdx].closestNodeIdx;
	}
	Vector3 leftStartPoint = VEC3_ZERO;
	Vector3 leftEndPoint = VEC3_ZERO;
	// Move the point along the shoreline while we still have distance to move.
	while (distanceToMove > 0.f)
	{
		if ( currentDistance >= distanceToMove)
		{
			// Means we've found it and we need to move 
			leftPoint = leftPoint + currentDirection * sqrt(distanceToMove);
			distanceToMove = 0.f;
			// To compute the ratio for the left point.
			if( currentShoreLine && currentShoreLine->GetRiverSettings())
			{	
				leftEndPoint = Vector3(currentShoreLine->GetRiverSettings()->ShoreLinePoints[0].X
					,currentShoreLine->GetRiverSettings()->ShoreLinePoints[0].Y
					,sm_ClosestShores[closestRiverIdx].riverSounds.waterHeights[NextIndex]);
				audShoreLineRiver* prevShoreline = currentShoreLine->GetPrevRiverReference();
				if( prevShoreline && prevShoreline->GetRiverSettings())
				{			
					const u8 index = prevShoreline->GetRiverSettings()->numShoreLinePoints - 1;
					leftStartPoint = Vector3(prevShoreline->GetRiverSettings()->ShoreLinePoints[index].X
						,prevShoreline->GetRiverSettings()->ShoreLinePoints[index].Y
						,sm_ClosestShores[closestRiverIdx].riverSounds.waterHeights[NextIndex]);
				}
			}
		}
		else
		{
			const ShoreLineRiverAudioSettings *settings = currentShoreLine->GetRiverSettings();
			if(settings && naVerifyf(pointIdx < settings->numShoreLinePoints,"Index out of bounds when computing left point, current index :%d",pointIdx))
			{
				leftPoint =  Vector3(settings->ShoreLinePoints[pointIdx].X,settings->ShoreLinePoints[pointIdx].Y,sm_ClosestShores[closestRiverIdx].riverSounds.waterHeights[NextIndex]);
				distanceToMove -= currentDistance;
				u8 followingNodeIdx = (pointIdx + 1);
				Vector3 followingPoint;
				if(followingNodeIdx < settings->numShoreLinePoints)
				{
					followingPoint = Vector3(settings->ShoreLinePoints[followingNodeIdx].X,settings->ShoreLinePoints[followingNodeIdx].Y,sm_ClosestShores[closestRiverIdx].riverSounds.waterHeights[NextIndex]);
				}
				else
				{
					audShoreLineRiver* nextShoreline = currentShoreLine->GetNextRiverReference();
					if( nextShoreline && nextShoreline->GetRiverSettings())
					{			
						followingPoint = Vector3(nextShoreline->GetRiverSettings()->ShoreLinePoints[0].X
							,nextShoreline->GetRiverSettings()->ShoreLinePoints[0].Y
							,sm_ClosestShores[closestRiverIdx].riverSounds.waterHeights[NextIndex]);
						followingNodeIdx = 0;
						currentShoreLine = nextShoreline;
					}
					else 
					{
						break;
					}
				}
				currentDirection = followingPoint - leftPoint;
				currentDistance = currentDirection.Mag2();
				currentDirection.Normalize();
				pointIdx = followingNodeIdx;
			}
		}
	}
	sm_ClosestShores[closestRiverIdx].riverSounds.linkRatio[Left] = 0.f;
	//sm_ClosestShores[closestRiverIdx].riverSounds.ending[Left] = true;
	if(pointIdx == 0)
	{
		sm_ClosestShores[closestRiverIdx].riverSounds.linkRatio[Left] = 1.f - fabs(((leftEndPoint - leftPoint).Mag() / (leftEndPoint - leftStartPoint).Mag()));
		sm_ClosestShores[closestRiverIdx].riverSounds.ending[Left] = (sm_ClosestShores[closestRiverIdx].riverSounds.linkRatio[Left] > 0.5f) ;
	}
	sm_ClosestShores[closestRiverIdx].riverSounds.linkRatio[Left] = Clamp(sm_ClosestShores[closestRiverIdx].riverSounds.linkRatio[Left],0.f,1.f);
	// CurrentShoreLine has the next reference to the closest one,
	sm_ClosestShores[closestRiverIdx].riverSounds.currentShore[Left] = currentShoreLine;
	if(!sm_ClosestShores[closestRiverIdx].riverSounds.ending[Left] && currentShoreLine->GetPrevRiverReference())
	{
		// Keep the prev one if we are not yet ending the crossfade
		sm_ClosestShores[closestRiverIdx].riverSounds.currentShore[Left] = currentShoreLine->GetPrevRiverReference();
	}
	naAssertf(FPIsFinite(leftPoint.Mag2()),"invalid river left point");
}
// ----------------------------------------------------------------
void audShoreLineRiver::CalculateRightPoint(const Vector3& centrePoint,const Vector3 &lakeDirectionToNextNode, const Vector3 &lakeDirectionFromPrevNode
										   ,Vector3 &rightPoint,const f32 distance, const u8 closestRiverIdx)
{
 	//Start from the centre point
 	rightPoint = centrePoint;
 
 	// Square the distance for performance purposes.
 	f32 distanceToMove = distance * distance;
 
 	Vector3 currentDirection; 
 	f32 currentDistance = 0.f;
 
 	// Compute in which (prev or next) direction the centre point is.
 	u8 pointIdx = sm_ClosestShores[closestRiverIdx].closestNodeIdx;
 	Vector3 closestToCentre = sm_ClosestShores[closestRiverIdx].closestNode - centrePoint;
 	closestToCentre.Normalize();
 	f32 nextDot = fabs(lakeDirectionToNextNode.Dot(closestToCentre));
 	f32 prevDot = fabs(lakeDirectionFromPrevNode.Dot(closestToCentre));
 
 	audShoreLineRiver *currentShoreLine = sm_ClosestShores[closestRiverIdx].river;
 	if (nextDot > prevDot)
 	{
 		currentDirection = lakeDirectionToNextNode;
 		currentDistance = (sm_ClosestShores[closestRiverIdx].closestNode - centrePoint).Mag2();
 		pointIdx = sm_ClosestShores[closestRiverIdx].closestNodeIdx;
 	}
 	else 
 	{
 		currentDirection = lakeDirectionFromPrevNode;
 		currentDistance = (sm_ClosestShores[closestRiverIdx].prevNode - centrePoint).Mag2();
 		pointIdx = sm_ClosestShores[closestRiverIdx].prevNodeIdx;
 		// if sm_ClosestShores[closestRiverIdx].prevNodeIdx is the last node of the prev reference our current shoreline should be the prev
 		if(sm_ClosestShores[closestRiverIdx].closestNodeIdx == 0 && sm_ClosestShores[closestRiverIdx].river->GetPrevRiverReference() && sm_ClosestShores[closestRiverIdx].river->GetPrevRiverReference()->GetRiverSettings()
 			&& (pointIdx == (sm_ClosestShores[closestRiverIdx].river->GetPrevRiverReference()->GetRiverSettings()->numShoreLinePoints - 1) ))
 		{
 			currentShoreLine = sm_ClosestShores[closestRiverIdx].river->GetPrevRiverReference();
 		}
 	}
 	Vector3 rightStartPoint = VEC3_ZERO;
 	Vector3 rightEndPoint = VEC3_ZERO;
 	// Move the point along the shoreline while we still have distance to move.
 	while (distanceToMove > 0.f)
 	{
 		if ( currentDistance >= distanceToMove)
 		{
 			// Means we've found it and we need to move 
 			rightPoint = rightPoint - currentDirection * sqrt(distanceToMove);
 			distanceToMove = 0.f;
 			// To compute the ratio for the right point.
 			if( currentShoreLine && currentShoreLine->GetRiverSettings())
 			{	
 				const u8 index = currentShoreLine->GetRiverSettings()->numShoreLinePoints - 1;
 				rightStartPoint = Vector3(currentShoreLine->GetRiverSettings()->ShoreLinePoints[index].X
 										 ,currentShoreLine->GetRiverSettings()->ShoreLinePoints[index].Y
 									  	 ,sm_ClosestShores[closestRiverIdx].riverSounds.waterHeights[PrevIndex]);
 				audShoreLineRiver* nextShoreLine = currentShoreLine->GetNextRiverReference();
 				if( nextShoreLine && nextShoreLine->GetRiverSettings())
 				{			
 					rightEndPoint = Vector3(nextShoreLine->GetRiverSettings()->ShoreLinePoints[0].X
 											,nextShoreLine->GetRiverSettings()->ShoreLinePoints[0].Y
											,sm_ClosestShores[closestRiverIdx].riverSounds.waterHeights[PrevIndex]);
 				}
 			}
 		}
 		else
 		{
 			const ShoreLineRiverAudioSettings *settings = currentShoreLine->GetRiverSettings();
 			if(settings && naVerifyf(pointIdx < settings->numShoreLinePoints,"Index out of bounds when computing right point, current index :%d",pointIdx))
 			{
 				rightPoint =  Vector3(settings->ShoreLinePoints[pointIdx].X,settings->ShoreLinePoints[pointIdx].Y,sm_ClosestShores[closestRiverIdx].riverSounds.waterHeights[PrevIndex]);
 				distanceToMove -= currentDistance;
 				s8 prevNodeIdx = (pointIdx - 1);
 				Vector3 previousPoint; 
 				// if prevNodeIdx still inside the bounds, use the current shoreline.
 				if(prevNodeIdx >= 0)
 				{
 					previousPoint = Vector3(settings->ShoreLinePoints[prevNodeIdx].X
 						,settings->ShoreLinePoints[prevNodeIdx].Y,sm_ClosestShores[closestRiverIdx].riverSounds.waterHeights[PrevIndex]);
 				}
 				// we are right at the beginning, check if we have a prev reference.
 				else
 				{
 					audShoreLineRiver* prevShoreLine = currentShoreLine->GetPrevRiverReference();
 					if( prevShoreLine && prevShoreLine->GetRiverSettings())
 					{
 						prevNodeIdx = prevShoreLine->GetRiverSettings()->numShoreLinePoints - 1;
 
 						previousPoint =  Vector3(prevShoreLine->GetRiverSettings()->ShoreLinePoints[prevNodeIdx].X
 												,prevShoreLine->GetRiverSettings()->ShoreLinePoints[prevNodeIdx].Y
 												,sm_ClosestShores[closestRiverIdx].riverSounds.waterHeights[PrevIndex]);
 						currentShoreLine = prevShoreLine;
 					}
 					else
 					{
 						break;
 					}
 				}
 				currentDirection = rightPoint - previousPoint;
 				currentDistance = currentDirection.Mag2();
 				currentDirection.Normalize();
 				pointIdx = prevNodeIdx;
 			}
 		}
 	}
 	sm_ClosestShores[closestRiverIdx].riverSounds.linkRatio[Right] = 0.f;
 	sm_ClosestShores[closestRiverIdx].riverSounds.currentShore[Right] = currentShoreLine;
 	if(sm_ClosestShores[closestRiverIdx].riverSounds.currentShore[Right] && sm_ClosestShores[closestRiverIdx].riverSounds.currentShore[Right]->GetRiverSettings())
 	{
 		if(pointIdx == (sm_ClosestShores[closestRiverIdx].riverSounds.currentShore[Right]->GetRiverSettings()->numShoreLinePoints - 1) )
 		{
 			sm_ClosestShores[closestRiverIdx].riverSounds.linkRatio[Right] = 1.f - fabs(((rightEndPoint - rightPoint).Mag() / (rightEndPoint - rightStartPoint).Mag()));
 			sm_ClosestShores[closestRiverIdx].riverSounds.ending[Right] = (sm_ClosestShores[closestRiverIdx].riverSounds.linkRatio[Right] > 0.5f) ;
 		}
 		sm_ClosestShores[closestRiverIdx].riverSounds.linkRatio[Right] = Clamp(sm_ClosestShores[closestRiverIdx].riverSounds.linkRatio[Right],0.f,1.f);
 		// CurrentShoreLine has the next reference to the closest one, 
 		if(sm_ClosestShores[closestRiverIdx].riverSounds.ending[Right] && currentShoreLine->GetNextRiverReference())
 		{
 			// only update it if the left point is closest to it.
 			sm_ClosestShores[closestRiverIdx].riverSounds.currentShore[Right] = currentShoreLine->GetNextRiverReference();
 		}
 	}
 	naAssertf(FPIsFinite(rightPoint.Mag2()),"invalid river right point");
}
// ----------------------------------------------------------------
void audShoreLineRiver::Update(const f32 /*timeElapsedS*/)
{
	u8 closestRiverIdx = 0;
 	if(IsClosestShore(closestRiverIdx))
 	{
 		if(m_IsActive)  
 		{
 			UpdateRiverSounds(closestRiverIdx);
 		}
 	}
}
// ----------------------------------------------------------------
void audShoreLineRiver::UpdateRiverSounds(const u8 closestRiverIdx)
{
	Vector3 centrePoint,leftPoint,rightPoint;
	CalculatePoints(centrePoint,leftPoint,rightPoint,closestRiverIdx);
	m_CentrePoint = centrePoint;
	if(m_IsActive)
	{
		// Left sound.
		audMetadataRef soundRef = g_NullSoundRef;
		if(sm_ClosestShores[closestRiverIdx].riverSounds.currentShore[Left] && sm_ClosestShores[closestRiverIdx].riverSounds.currentShore[Left]->GetRiverSettings())
		{
			GetSoundsRefs((RiverType)sm_ClosestShores[closestRiverIdx].riverSounds.currentShore[Left]->GetRiverSettings()->RiverType,soundRef,Left);
			UpdateRiverSound(soundRef,Left,sm_ClosestShores[closestRiverIdx].riverSounds.ending[Left],leftPoint,closestRiverIdx);
		}
		// Centre
		GetSoundsRefs((RiverType)GetRiverSettings()->RiverType,soundRef,Centre);
		UpdateRiverSound(soundRef,Centre,sm_ClosestShores[closestRiverIdx].riverSounds.ending[Centre],centrePoint,closestRiverIdx);
		// Right sound
		if(sm_ClosestShores[closestRiverIdx].riverSounds.currentShore[Right] && sm_ClosestShores[closestRiverIdx].riverSounds.currentShore[Right]->GetRiverSettings())
		{
			GetSoundsRefs((RiverType)sm_ClosestShores[closestRiverIdx].riverSounds.currentShore[Right]->GetRiverSettings()->RiverType,soundRef,Right);
			UpdateRiverSound(soundRef,Right,sm_ClosestShores[closestRiverIdx].riverSounds.ending[Right],rightPoint,closestRiverIdx);
		}
#if __BANK
		if(sm_DrawWaterBehaviour)
		{
			char txt[256];
			char name[64];
			formatf(txt, "Closest node %u ",sm_ClosestShores[closestRiverIdx].closestNodeIdx);
			grcDebugDraw::AddDebugOutput(Color_green,txt);
			if(sm_ClosestShores[closestRiverIdx].riverSounds.currentShore[Left] && sm_ClosestShores[closestRiverIdx].riverSounds.currentShore[Left]->GetRiverSettings())
			{
				formatf(name,"%s",audNorthAudioEngine::GetMetadataManager().GetObjectName(sm_ClosestShores[closestRiverIdx].riverSounds.currentShore[Left]->GetRiverSettings()->NameTableOffset,0));
			}
			else
			{
				formatf(name,"none");
			}
			formatf(txt, "LEFT: [Ratio %f] [Shore %s] [Index %u]", sm_ClosestShores[closestRiverIdx].riverSounds.linkRatio[Left],name,sm_ClosestShores[closestRiverIdx].riverSounds.currentSoundIdx[Left]);
			grcDebugDraw::AddDebugOutput(Color_green,txt);
			for(u8 i = 0; i< 2; i ++)
			{
				if(sm_ClosestShores[closestRiverIdx].riverSounds.sounds[i][Left])
				{
					formatf(txt, "[%u] Sound %s  vol %f",i,g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectNameFromNameTableOffset(sm_ClosestShores[closestRiverIdx].riverSounds.sounds[i][Left]->GetBaseMetadata()->NameTableOffset),sm_ClosestShores[closestRiverIdx].riverSounds.sounds[i][Left]->GetRequestedVolume());
					grcDebugDraw::AddDebugOutput(Color_white,txt);
				}
			}
			if(GetRiverSettings())
			{
				formatf(name,"%s",audNorthAudioEngine::GetMetadataManager().GetObjectName(GetRiverSettings()->NameTableOffset,0));
			}
			else
			{
				formatf(name,"none");
			}
			formatf(txt, "CENTRE: [Ratio %f]  [Shore %s] [Index %u]", sm_ClosestShores[closestRiverIdx].riverSounds.linkRatio[Centre],name,sm_ClosestShores[closestRiverIdx].riverSounds.currentSoundIdx[Centre]);
			grcDebugDraw::AddDebugOutput(Color_green,txt);
			for(u8 i = 0; i< 2; i ++)
			{
				if(sm_ClosestShores[closestRiverIdx].riverSounds.sounds[i][Centre])
				{
					formatf(txt, "[%u] Sound %s  vol %f",i,g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectNameFromNameTableOffset(sm_ClosestShores[closestRiverIdx].riverSounds.sounds[i][Centre]->GetBaseMetadata()->NameTableOffset),sm_ClosestShores[closestRiverIdx].riverSounds.sounds[i][Centre]->GetRequestedVolume());
					grcDebugDraw::AddDebugOutput(Color_white,txt);
				}
			}
			if(sm_ClosestShores[closestRiverIdx].riverSounds.currentShore[Right] && sm_ClosestShores[closestRiverIdx].riverSounds.currentShore[Right]->GetRiverSettings())
			{
				formatf(name,"%s",audNorthAudioEngine::GetMetadataManager().GetObjectName(sm_ClosestShores[closestRiverIdx].riverSounds.currentShore[Right]->GetRiverSettings()->NameTableOffset,0));
			}
			else
			{
				formatf(name,"none");
			}
			formatf(txt, "RIGHT: [Ratio %f]  [Shore %s] [Index %u]", sm_ClosestShores[closestRiverIdx].riverSounds.linkRatio[Right],name,sm_ClosestShores[closestRiverIdx].riverSounds.currentSoundIdx[Right]);
			grcDebugDraw::AddDebugOutput(Color_green,txt);
			for(u8 i = 0; i< 2; i ++)
			{
				if(sm_ClosestShores[closestRiverIdx].riverSounds.sounds[i][Right])
				{
					formatf(txt, "[%u] Sound %s  vol %f",i,g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectNameFromNameTableOffset(sm_ClosestShores[closestRiverIdx].riverSounds.sounds[i][Right]->GetBaseMetadata()->NameTableOffset),sm_ClosestShores[closestRiverIdx].riverSounds.sounds[i][Right]->GetRequestedVolume());
					grcDebugDraw::AddDebugOutput(Color_white,txt);
				}
			}
		}
#endif
	}
}
// ----------------------------------------------------------------
void audShoreLineRiver::UpdateRiverSound(const audMetadataRef soundRef,const u8 soundIdx,const bool ending,const Vector3 &position,const u8 closestRiverIdx)
{
	bool shouldUpdatePosition = true; 
	if( !FPIsFinite(position.Mag2()))
	{
		naAssertf(false, "Invalid position about to be set up on a river sound.");
		shouldUpdatePosition = false;
	}
	// Play the sounds if we don't have them and update their position.
	if (!sm_ClosestShores[closestRiverIdx].riverSounds.sounds[sm_ClosestShores[closestRiverIdx].riverSounds.currentSoundIdx[soundIdx]][soundIdx])
	{
		audSoundInitParams initParams;
		if(shouldUpdatePosition)
			initParams.Position = position;
		g_AmbientAudioEntity.CreateAndPlaySound_Persistent(soundRef,&sm_ClosestShores[closestRiverIdx].riverSounds.sounds[sm_ClosestShores[closestRiverIdx].riverSounds.currentSoundIdx[soundIdx]][soundIdx],&initParams);
	}
	if(sm_ClosestShores[closestRiverIdx].riverSounds.sounds[sm_ClosestShores[closestRiverIdx].riverSounds.currentSoundIdx[soundIdx]][soundIdx])    
	{
		sm_ClosestShores[closestRiverIdx].riverSounds.sounds[sm_ClosestShores[closestRiverIdx].riverSounds.currentSoundIdx[soundIdx]][soundIdx]->FindAndSetVariableValue(ATSTRINGHASH("upDistanceToShore", 0xC3FFCAFF),fabs(position.z - g_AudioEngine.GetEnvironment().GetVolumeListenerPosition().GetZf()));
		sm_ClosestShores[closestRiverIdx].riverSounds.sounds[sm_ClosestShores[closestRiverIdx].riverSounds.currentSoundIdx[soundIdx]][soundIdx]->FindAndSetVariableValue(ATSTRINGHASH("distanceToShore", 0x2B21E5E5),sm_ClosestShores[closestRiverIdx].distanceToShore);
		sm_ClosestShores[closestRiverIdx].riverSounds.sounds[sm_ClosestShores[closestRiverIdx].riverSounds.currentSoundIdx[soundIdx]][soundIdx]->SetRequestedVolume(0.f);
		if(shouldUpdatePosition)
			sm_ClosestShores[closestRiverIdx].riverSounds.sounds[sm_ClosestShores[closestRiverIdx].riverSounds.currentSoundIdx[soundIdx]][soundIdx]->SetRequestedPosition(position);
	}

	u8 index = (sm_ClosestShores[closestRiverIdx].riverSounds.currentSoundIdx[soundIdx] + 1) & 1;
	if(ending && sm_ClosestShores[closestRiverIdx].riverSounds.currentShore[soundIdx] && sm_ClosestShores[closestRiverIdx].riverSounds.currentShore[soundIdx]->GetRiverSettings())
	{
		audMetadataRef crossfadeSoundRef = g_NullSoundRef;
		const audShoreLineRiver *prevShoreLine = sm_ClosestShores[closestRiverIdx].riverSounds.currentShore[soundIdx]->GetPrevRiverReference();
		if( prevShoreLine && prevShoreLine->GetRiverSettings() 
			&& (prevShoreLine->GetRiverSettings()->RiverType != sm_ClosestShores[closestRiverIdx].riverSounds.currentShore[soundIdx]->GetRiverSettings()->RiverType))
		{
			GetSoundsRefs((RiverType)prevShoreLine->GetRiverSettings()->RiverType,crossfadeSoundRef,soundIdx);
			if(sm_ClosestShores[closestRiverIdx].riverSounds.linkRatio[soundIdx] == 0.f)
			{
				// if we have a sound in the slotIndex
				if(sm_ClosestShores[closestRiverIdx].riverSounds.sounds[index][soundIdx])
				{
					// stop the old one 
					if(sm_ClosestShores[closestRiverIdx].riverSounds.sounds[sm_ClosestShores[closestRiverIdx].riverSounds.currentSoundIdx[soundIdx]][soundIdx])
					{
						sm_ClosestShores[closestRiverIdx].riverSounds.sounds[sm_ClosestShores[closestRiverIdx].riverSounds.currentSoundIdx[soundIdx]][soundIdx]->StopAndForget();
					}
					sm_ClosestShores[closestRiverIdx].riverSounds.currentSoundIdx[soundIdx] = index;
					if(soundIdx == Right)
					{
						sm_ClosestShores[closestRiverIdx].riverSounds.ending[soundIdx] = false;
					}
				}
			}
			else
			{
				if(!sm_ClosestShores[closestRiverIdx].riverSounds.sounds[index][soundIdx])
				{
					sm_ClosestShores[closestRiverIdx].riverSounds.currentSoundIdx[soundIdx] = index;
					audSoundInitParams initParams;
					if(shouldUpdatePosition)
						initParams.Position = position;
					g_AmbientAudioEntity.CreateAndPlaySound_Persistent(crossfadeSoundRef,&sm_ClosestShores[closestRiverIdx].riverSounds.sounds[index][soundIdx],&initParams);
				}
				f32 volume = sm_EqualPowerFallCrossFade.CalculateRescaledValue(-15.f,0.f,0.f,1.f,sm_ClosestShores[closestRiverIdx].riverSounds.linkRatio[soundIdx]);
				if(sm_ClosestShores[closestRiverIdx].riverSounds.sounds[sm_ClosestShores[closestRiverIdx].riverSounds.currentSoundIdx[soundIdx]][soundIdx] )    
				{
					sm_ClosestShores[closestRiverIdx].riverSounds.sounds[sm_ClosestShores[closestRiverIdx].riverSounds.currentSoundIdx[soundIdx]][soundIdx]->FindAndSetVariableValue(ATSTRINGHASH("upDistanceToShore", 0xC3FFCAFF),fabs(position.z - g_AudioEngine.GetEnvironment().GetVolumeListenerPosition().GetZf()));
					sm_ClosestShores[closestRiverIdx].riverSounds.sounds[sm_ClosestShores[closestRiverIdx].riverSounds.currentSoundIdx[soundIdx]][soundIdx]->FindAndSetVariableValue(ATSTRINGHASH("distanceToShore", 0x2B21E5E5),sm_ClosestShores[closestRiverIdx].distanceToShore);
					sm_ClosestShores[closestRiverIdx].riverSounds.sounds[sm_ClosestShores[closestRiverIdx].riverSounds.currentSoundIdx[soundIdx]][soundIdx]->SetRequestedVolume(volume);
					if(shouldUpdatePosition)
						sm_ClosestShores[closestRiverIdx].riverSounds.sounds[sm_ClosestShores[closestRiverIdx].riverSounds.currentSoundIdx[soundIdx]][soundIdx]->SetRequestedPosition(position);
				}
				volume = sm_EqualPowerRiseCrossFade.CalculateRescaledValue(-15.f,0.f,0.f,1.f,sm_ClosestShores[closestRiverIdx].riverSounds.linkRatio[soundIdx]);
				if(sm_ClosestShores[closestRiverIdx].riverSounds.sounds[index][soundIdx] )    
				{
					sm_ClosestShores[closestRiverIdx].riverSounds.sounds[index][soundIdx]->FindAndSetVariableValue(ATSTRINGHASH("upDistanceToShore", 0xC3FFCAFF),fabs(position.z - g_AudioEngine.GetEnvironment().GetVolumeListenerPosition().GetZf()));
					sm_ClosestShores[closestRiverIdx].riverSounds.sounds[index][soundIdx]->FindAndSetVariableValue(ATSTRINGHASH("distanceToShore", 0x2B21E5E5),sm_ClosestShores[closestRiverIdx].distanceToShore);
					sm_ClosestShores[closestRiverIdx].riverSounds.sounds[index][soundIdx]->SetRequestedVolume(volume);
					if(shouldUpdatePosition)
						sm_ClosestShores[closestRiverIdx].riverSounds.sounds[index][soundIdx]->SetRequestedPosition(position);
				}
			}
		}
	}
	else if ( sm_ClosestShores[closestRiverIdx].riverSounds.currentShore[soundIdx] && sm_ClosestShores[closestRiverIdx].riverSounds.currentShore[soundIdx]->GetRiverSettings())
	{
		audMetadataRef crossfadeSoundRef = g_NullSoundRef;
		const audShoreLineRiver *nextShoreLine =  sm_ClosestShores[closestRiverIdx].riverSounds.currentShore[soundIdx]->GetNextRiverReference();
		if( nextShoreLine && nextShoreLine->GetRiverSettings()
			&& (nextShoreLine->GetRiverSettings()->RiverType != sm_ClosestShores[closestRiverIdx].riverSounds.currentShore[soundIdx]->GetRiverSettings()->RiverType))
		{	
			GetSoundsRefs((RiverType)nextShoreLine->GetRiverSettings()->RiverType,crossfadeSoundRef,soundIdx);
			if(sm_ClosestShores[closestRiverIdx].riverSounds.linkRatio[soundIdx] == 0.f)
			{
				// if we have a sound in the slotIndex
				if(sm_ClosestShores[closestRiverIdx].riverSounds.sounds[index][soundIdx])
				{
					sm_ClosestShores[closestRiverIdx].riverSounds.sounds[index][soundIdx]->StopAndForget();
				}
			}
			else
			{
				// Going back without reaching the end, invert the slot.
				if(!sm_ClosestShores[closestRiverIdx].riverSounds.sounds[index][soundIdx])
				{
					audSoundInitParams initParams;
					if(shouldUpdatePosition)
						initParams.Position = position;
					g_AmbientAudioEntity.CreateAndPlaySound_Persistent(crossfadeSoundRef,&sm_ClosestShores[closestRiverIdx].riverSounds.sounds[index][soundIdx],&initParams);
				}
				f32 volume = sm_EqualPowerFallCrossFade.CalculateRescaledValue(-15.f,0.f,0.f,1.f,sm_ClosestShores[closestRiverIdx].riverSounds.linkRatio[soundIdx]);
				if(sm_ClosestShores[closestRiverIdx].riverSounds.sounds[sm_ClosestShores[closestRiverIdx].riverSounds.currentSoundIdx[soundIdx]][soundIdx] )    
				{
					sm_ClosestShores[closestRiverIdx].riverSounds.sounds[sm_ClosestShores[closestRiverIdx].riverSounds.currentSoundIdx[soundIdx]][soundIdx]->FindAndSetVariableValue(ATSTRINGHASH("upDistanceToShore", 0xC3FFCAFF),fabs(position.z - g_AudioEngine.GetEnvironment().GetVolumeListenerPosition().GetZf()));
					sm_ClosestShores[closestRiverIdx].riverSounds.sounds[sm_ClosestShores[closestRiverIdx].riverSounds.currentSoundIdx[soundIdx]][soundIdx]->FindAndSetVariableValue(ATSTRINGHASH("distanceToShore", 0x2B21E5E5),sm_ClosestShores[closestRiverIdx].distanceToShore);
					sm_ClosestShores[closestRiverIdx].riverSounds.sounds[sm_ClosestShores[closestRiverIdx].riverSounds.currentSoundIdx[soundIdx]][soundIdx]->SetRequestedVolume(volume);
					if(shouldUpdatePosition)
						sm_ClosestShores[closestRiverIdx].riverSounds.sounds[sm_ClosestShores[closestRiverIdx].riverSounds.currentSoundIdx[soundIdx]][soundIdx]->SetRequestedPosition(position);
				}
				volume = sm_EqualPowerRiseCrossFade.CalculateRescaledValue(-15.f,0.f,0.f,1.f,sm_ClosestShores[closestRiverIdx].riverSounds.linkRatio[soundIdx]);
				if(sm_ClosestShores[closestRiverIdx].riverSounds.sounds[index][soundIdx] )    
				{
					sm_ClosestShores[closestRiverIdx].riverSounds.sounds[index][soundIdx]->FindAndSetVariableValue(ATSTRINGHASH("upDistanceToShore", 0xC3FFCAFF),fabs(position.z - g_AudioEngine.GetEnvironment().GetVolumeListenerPosition().GetZf()));
					sm_ClosestShores[closestRiverIdx].riverSounds.sounds[index][soundIdx]->FindAndSetVariableValue(ATSTRINGHASH("distanceToShore", 0x2B21E5E5),sm_ClosestShores[closestRiverIdx].distanceToShore);
					sm_ClosestShores[closestRiverIdx].riverSounds.sounds[index][soundIdx]->SetRequestedVolume(volume);
					if(shouldUpdatePosition)
						sm_ClosestShores[closestRiverIdx].riverSounds.sounds[index][soundIdx]->SetRequestedPosition(position);
				}
			}
		}
	}
}
// ----------------------------------------------------------------
void audShoreLineRiver::CheckShorelineChange()
{
	for (u8 i = 0; i < AUD_MAX_RIVERS_PLAYING; i ++)
	{
 		if(sm_LastClosestShores[i] != sm_ClosestShores[i].river)
 		{
 			StopSounds(i);
 		}
	}
}
// ----------------------------------------------------------------
void audShoreLineRiver::StopSounds(const u8 closestRiverIdx)
{
	for (u32 i = 0; i < NumWaterSoundPos;i ++)
	{
		if(sm_ClosestShores[closestRiverIdx].riverSounds.sounds[0][i])
		{
			sm_ClosestShores[closestRiverIdx].riverSounds.sounds[0][i]->StopAndForget();
			sm_ClosestShores[closestRiverIdx].riverSounds.sounds[0][i] = NULL;
		}
		if(sm_ClosestShores[closestRiverIdx].riverSounds.sounds[1][i])
		{
			sm_ClosestShores[closestRiverIdx].riverSounds.sounds[1][i]->StopAndForget();
			sm_ClosestShores[closestRiverIdx].riverSounds.sounds[1][i] = NULL;
		}
		sm_ClosestShores[closestRiverIdx].riverSounds.currentShore[i] = NULL;
		sm_ClosestShores[closestRiverIdx].riverSounds.linkRatio[i] = 0.f;
		sm_ClosestShores[closestRiverIdx].riverSounds.currentSoundIdx[i] = 0;
	}
	sm_ClosestShores[closestRiverIdx].riverSounds.ending[Left] = true;
}
// ----------------------------------------------------------------
void audShoreLineRiver::GetSoundsRefs(RiverType riverType, audMetadataRef &soundRef,const u8 soundIdx)
{
	switch(riverType)
	{
	case AUD_RIVER_BROOK_WEAK:
		soundRef = sm_BrookWeakSoundset.Find(ATSTRINGHASH("RIVER_LEFT", 0x9C73C216));
		if(soundIdx == Centre)
		{
			soundRef = sm_BrookWeakSoundset.Find(ATSTRINGHASH("RIVER_CENTRE", 0x4B124736));
		}
		else if (soundIdx == Right)
		{
			soundRef = sm_BrookWeakSoundset.Find(ATSTRINGHASH("RIVER_RIGHT", 0xB994AB7B));
		}
		break;
	case AUD_RIVER_BROOK_STRONG:
		soundRef = sm_BrookStrongSoundset.Find(ATSTRINGHASH("RIVER_LEFT", 0x9C73C216));
		if(soundIdx == Centre)
		{
			soundRef = sm_BrookStrongSoundset.Find(ATSTRINGHASH("RIVER_CENTRE", 0x4B124736));
		}
		else if (soundIdx == Right)
		{
			soundRef = sm_BrookStrongSoundset.Find(ATSTRINGHASH("RIVER_RIGHT", 0xB994AB7B));
		}
		break;
	case AUD_RIVER_LA_WEAK:
		soundRef = sm_LAWeakSoundset.Find(ATSTRINGHASH("RIVER_LEFT", 0x9C73C216));
		if(soundIdx == Centre)
		{
			soundRef = sm_LAWeakSoundset.Find(ATSTRINGHASH("RIVER_CENTRE", 0x4B124736));
		}
		else if (soundIdx == Right)
		{
			soundRef = sm_LAWeakSoundset.Find(ATSTRINGHASH("RIVER_RIGHT", 0xB994AB7B));
		}
		break;
	case AUD_RIVER_LA_STRONG:
		soundRef = sm_LAStrongSoundset.Find(ATSTRINGHASH("RIVER_LEFT", 0x9C73C216));
		if(soundIdx == Centre)
		{
			soundRef = sm_LAStrongSoundset.Find(ATSTRINGHASH("RIVER_CENTRE", 0x4B124736));
		}
		else if (soundIdx == Right)
		{
			soundRef = sm_LAStrongSoundset.Find(ATSTRINGHASH("RIVER_RIGHT", 0xB994AB7B));
		}
		break;
	case AUD_RIVER_WEAK:
		soundRef = sm_WeakSoundset.Find(ATSTRINGHASH("RIVER_LEFT", 0x9C73C216));
		if(soundIdx == Centre)
		{
			soundRef = sm_WeakSoundset.Find(ATSTRINGHASH("RIVER_CENTRE", 0x4B124736));
		}
		else if (soundIdx == Right)
		{
			soundRef = sm_WeakSoundset.Find(ATSTRINGHASH("RIVER_RIGHT", 0xB994AB7B));
		}
		break;
	case AUD_RIVER_MEDIUM:
		soundRef = sm_MediumSoundset.Find(ATSTRINGHASH("RIVER_LEFT", 0x9C73C216));
		if(soundIdx == Centre)
		{
			soundRef = sm_MediumSoundset.Find(ATSTRINGHASH("RIVER_CENTRE", 0x4B124736));
		}
		else if (soundIdx == Right)
		{
			soundRef = sm_MediumSoundset.Find(ATSTRINGHASH("RIVER_RIGHT", 0xB994AB7B));
		}
		break;
	case AUD_RIVER_STRONG:
		soundRef = sm_StrongSoundset.Find(ATSTRINGHASH("RIVER_LEFT", 0x9C73C216));
		if(soundIdx == Centre)
		{
			soundRef = sm_StrongSoundset.Find(ATSTRINGHASH("RIVER_CENTRE", 0x4B124736));
		}
		else if (soundIdx == Right)
		{
			soundRef = sm_StrongSoundset.Find(ATSTRINGHASH("RIVER_RIGHT", 0xB994AB7B));
		}
		break;
	case AUD_RIVER_RAPIDS_WEAK:
		soundRef = sm_RapidsWeakSoundset.Find(ATSTRINGHASH("RIVER_LEFT", 0x9C73C216));
		if(soundIdx == Centre)
		{
			soundRef = sm_RapidsWeakSoundset.Find(ATSTRINGHASH("RIVER_CENTRE", 0x4B124736));
		}
		else if (soundIdx == Right)
		{
			soundRef = sm_RapidsWeakSoundset.Find(ATSTRINGHASH("RIVER_RIGHT", 0xB994AB7B));
		}
		break;
	case AUD_RIVER_RAPIDS_STRONG:
		soundRef = sm_RapidsStrongSoundset.Find(ATSTRINGHASH("RIVER_LEFT", 0x9C73C216));
		if(soundIdx == Centre)
		{
			soundRef = sm_RapidsStrongSoundset.Find(ATSTRINGHASH("RIVER_CENTRE", 0x4B124736));
		}
		else if (soundIdx == Right)
		{
			soundRef = sm_RapidsStrongSoundset.Find(ATSTRINGHASH("RIVER_RIGHT", 0xB994AB7B));
		}
		break;
	default:
		break;
	}
}
// ----------------------------------------------------------------
f32 audShoreLineRiver::GetWaterHeightAtPos(Vec3V_In pos)
{
	f32 waterz;
	CVfxHelper::GetWaterZ(pos, waterz);
	if( waterz <= 0.f && m_Settings && GetRiverSettings()->DefaultHeight >= 0.f)
	{
		waterz = GetRiverSettings()->DefaultHeight;
	}
	return waterz;
}
// ----------------------------------------------------------------
void audShoreLineRiver::UpdateWaterHeight(bool BANK_ONLY(updateAllHeights))
{
	if(m_Settings)
	{
		u8 closestRiverIdx = 0;
		if(IsClosestShore(closestRiverIdx))
		{
			// Closest index : 
			u8 closestIndex = sm_ClosestShores[closestRiverIdx].closestNodeIdx;
			f32 waterz;
            waterz = GetWaterHeightAtPos(Vec3V(GetRiverSettings()->ShoreLinePoints[closestIndex].X, GetRiverSettings()->ShoreLinePoints[closestIndex].Y, 0.f));			sm_ClosestShores[closestRiverIdx].riverSounds.waterHeights[ClosestIndex] = waterz;

			s8 nextNodeIdx = (closestIndex + 1);
			s8 prevNodeIdx = (closestIndex - 1);

			if(closestIndex > 0 && closestIndex < GetRiverSettings()->numShoreLinePoints -1)
			{
				waterz =  GetWaterHeightAtPos(Vec3V(GetRiverSettings()->ShoreLinePoints[nextNodeIdx].X, GetRiverSettings()->ShoreLinePoints[nextNodeIdx].Y, 0.f));
				sm_ClosestShores[closestRiverIdx].riverSounds.waterHeights[NextIndex] = waterz;
				waterz =  GetWaterHeightAtPos(Vec3V(GetRiverSettings()->ShoreLinePoints[prevNodeIdx].X, GetRiverSettings()->ShoreLinePoints[prevNodeIdx].Y, 0.f));
				sm_ClosestShores[closestRiverIdx].riverSounds.waterHeights[PrevIndex] = waterz;
			}
			else if ( closestIndex == 0)
			{
				waterz =  GetWaterHeightAtPos(Vec3V(GetRiverSettings()->ShoreLinePoints[nextNodeIdx].X, GetRiverSettings()->ShoreLinePoints[nextNodeIdx].Y, 0.f));
				sm_ClosestShores[closestRiverIdx].riverSounds.waterHeights[NextIndex] = waterz;
				audShoreLineRiver *prevShoreLine = GetPrevRiverReference();
				if( !prevShoreLine)
				{
					prevShoreLine = FindLastReference();
					naAssertf(prevShoreLine, "Got a null when looking for the last shore, something went wrong") ;
				}
				if(prevShoreLine && prevShoreLine->GetRiverSettings())
				{
					prevNodeIdx = prevShoreLine->GetRiverSettings()->numShoreLinePoints - 1;
					waterz =  GetWaterHeightAtPos(Vec3V(prevShoreLine->GetRiverSettings()->ShoreLinePoints[prevNodeIdx].X, prevShoreLine->GetRiverSettings()->ShoreLinePoints[prevNodeIdx].Y, 0.f));
					sm_ClosestShores[closestRiverIdx].riverSounds.waterHeights[PrevIndex] = waterz;
				}
			}
			else if (closestRiverIdx < GetRiverSettings()->numShoreLinePoints)
			{
				audShoreLineRiver *nextShoreLine = GetNextRiverReference();
				if(!nextShoreLine)
				{
					nextShoreLine = FindFirstReference();
					naAssertf(nextShoreLine, "Got a null when looking for the first shore, something went wrong");
				}
				if( nextShoreLine && nextShoreLine->GetRiverSettings())
				{	
					nextNodeIdx = 0;
					waterz =  GetWaterHeightAtPos(Vec3V(nextShoreLine->GetRiverSettings()->ShoreLinePoints[nextNodeIdx].X, nextShoreLine->GetRiverSettings()->ShoreLinePoints[nextNodeIdx].Y, 0.f));
					sm_ClosestShores[closestRiverIdx].riverSounds.waterHeights[NextIndex] = waterz;
				}
				waterz =  GetWaterHeightAtPos(Vec3V(GetRiverSettings()->ShoreLinePoints[prevNodeIdx].X, GetRiverSettings()->ShoreLinePoints[prevNodeIdx].Y, 0.f));
				sm_ClosestShores[closestRiverIdx].riverSounds.waterHeights[PrevIndex] = waterz;
			}

		}
	}
#if __BANK
	m_WaterHeights.Reset();
	m_WaterHeights.Resize(0);
	if (!m_Settings)
	{
		for(u32 idx = 0 ; idx <m_ShoreLinePoints.GetCount(); idx++)
		{
			f32 waterz;
			waterz =  GetWaterHeightAtPos(Vec3V(m_ShoreLinePoints[idx].x,m_ShoreLinePoints[idx].y, 0.f));
			// Try and get the prev/next water height if the result is wrong (-9999.f)
			if(waterz == -9999.f)
			{
				if( idx > 0 && m_WaterHeights[idx -1] != -9999.f)
				{
					waterz = m_WaterHeights[idx -1];
				}
			}
			m_WaterHeights.PushAndGrow(waterz);
		}
	}
	else if (updateAllHeights || audAmbientAudioEntity::sm_DrawShoreLines || audAmbientAudioEntity::sm_DrawActivationBox || sm_DrawWaterBehaviour)
	{
		for(u32 idx = 0 ; idx <(u32)GetRiverSettings()->numShoreLinePoints; idx++)
		{
			f32 waterz;
			waterz =  GetWaterHeightAtPos(Vec3V(GetRiverSettings()->ShoreLinePoints[idx].X, GetRiverSettings()->ShoreLinePoints[idx].Y, 0.f));
			// Try and get the prev/next water height if the result is wrong (-9999.f)
			if(waterz == -9999.f)
			{
				if( idx > 0 && m_WaterHeights[idx -1] != -9999.f)
				{
					waterz = m_WaterHeights[idx -1];
				}
			}
			m_WaterHeights.PushAndGrow(waterz);
		}
	}
#endif
}

#if __BANK
// ----------------------------------------------------------------
// Initialise the shore line
// ----------------------------------------------------------------
void audShoreLineRiver::InitDebug()
{
	if(m_Settings)
	{
		m_ShoreLinePoints.Reset();
		m_ShoreLinePoints.Resize(GetRiverSettings()->numShoreLinePoints);
		m_ShoreLinePointsWidth.Reset();
		m_ShoreLinePointsWidth.Resize(GetRiverSettings()->numShoreLinePoints);
		for(s32 i = 0; i < GetRiverSettings()->numShoreLinePoints; i++)
		{
			m_ShoreLinePoints[i].x = GetRiverSettings()->ShoreLinePoints[i].X;
			m_ShoreLinePoints[i].y = GetRiverSettings()->ShoreLinePoints[i].Y;
			m_ShoreLinePointsWidth[i] = GetRiverSettings()->ShoreLinePoints[i].RiverWidth;
		}
		Vector2 center = Vector2(GetRiverSettings()->ActivationBox.Center.X,GetRiverSettings()->ActivationBox.Center.Y);
		m_ActivationBox = fwRect(center,GetRiverSettings()->ActivationBox.Size.Width/2.f,GetRiverSettings()->ActivationBox.Size.Height/2.f);
		m_RotAngle = GetRiverSettings()->ActivationBox.RotationAngle;
		m_Width = m_ActivationBox.GetWidth();
		m_Height = m_ActivationBox.GetHeight();
	}
}
// ----------------------------------------------------------------
// Serialise the shoreline data
// ----------------------------------------------------------------
bool audShoreLineRiver::Serialise(const char* shoreName,bool editing)
{
	if(strlen(shoreName) == 0)
	{
		naDisplayf("Please specify a valid name");
		return false;
	}
	char name[128];
	if(!editing)
	{
		formatf(name,"SHORELINE_%s",shoreName);
	}
	else
	{
		formatf(name,"%s",shoreName);
	}
	char xmlMsg[4092];
	FormatShoreLineAudioSettingsXml(xmlMsg, name);
	return g_AudioEngine.GetRemoteControl().SendXmlMessage(xmlMsg, istrlen(xmlMsg));
}
//------------------------------------------------------------------------------------------------------------------------------
void audShoreLineRiver::FormatShoreLineAudioSettingsXml(char * xmlMsg, const char * settingsName)
{
	sprintf(xmlMsg, "<RAVEMessage>\n");
	char tmpBuf[256] = {0};

	atString upperSettings(settingsName);
	upperSettings.Uppercase();

	sprintf(tmpBuf, "	<EditObjects metadataType=\"GAMEOBJECTS\" chunkNameHash=\"%u\">\n", atStringHash("BASE"));
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "		<ShoreLineRiverAudioSettings name=\"%s\">\n", &settingsName[0]);
	strcat(xmlMsg, tmpBuf);

	SerialiseActivationBox(xmlMsg,tmpBuf);
	if(GetNextRiverReference() && GetNextRiverReference()->GetRiverSettings())
	{
		sprintf(tmpBuf, "	<NextShoreline>%s</NextShoreline>",
			audNorthAudioEngine::GetMetadataManager().GetObjectName(GetNextRiverReference()->GetRiverSettings()->NameTableOffset,0));
		strcat(xmlMsg, tmpBuf);
	}
	if(GetRiverSettings())
	{
		sprintf(tmpBuf, "	<RiverType>%s</RiverType>",GetRiverTypeName((RiverType)GetRiverSettings()->RiverType));
		strcat(xmlMsg, tmpBuf);
	}

	for(s32 i = 0 ; i < m_ShoreLinePoints.GetCount(); i++)
	{
		sprintf(tmpBuf, "		<ShoreLinePoints>");
		strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "			<X>%f</X>",m_ShoreLinePoints[i].x);
		strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "			<Y>%f</Y>",m_ShoreLinePoints[i].y);
		strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "			<RiverWidth>%f</RiverWidth>",m_ShoreLinePointsWidth[i]);
		strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "		</ShoreLinePoints>");
		strcat(xmlMsg, tmpBuf);
	}
	sprintf(tmpBuf, "		</ShoreLineRiverAudioSettings>\n");
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "	</EditObjects>\n");
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "</RAVEMessage>\n");
	strcat(xmlMsg, tmpBuf);
}
//------------------------------------------------------------------------------------------------------------------------------
const char * audShoreLineRiver::GetRiverTypeName(RiverType riverType)
{
	switch(riverType)
	{
	case AUD_RIVER_BROOK_WEAK:
		return "AUD_RIVER_BROOK_WEAK";
	case AUD_RIVER_BROOK_STRONG:
		return "AUD_RIVER_BROOK_STRONG";
	case AUD_RIVER_LA_WEAK:
		return "AUD_RIVER_LA_WEAK";
	case AUD_RIVER_LA_STRONG:
		return "AUD_RIVER_LA_STRONG";
	case AUD_RIVER_WEAK:
		return "AUD_RIVER_WEAK";
	case AUD_RIVER_MEDIUM:
		return "AUD_RIVER_MEDIUM";
	case AUD_RIVER_STRONG:
		return "AUD_RIVER_STRONG";
	case AUD_RIVER_RAPIDS_WEAK:
		return "AUD_RIVER_RAPIDS_WEAK";
	case AUD_RIVER_RAPIDS_STRONG:
		return "AUD_RIVER_RAPIDS_STRONG";
	default:
		return "";
	}
}

//------------------------------------------------------------------------------------------------------------------------------
void audShoreLineRiver::DrawPlayingShorelines() 
{
	for (u8 i = 0; i < AUD_MAX_RIVERS_PLAYING; i ++)
	{
		if(sm_ClosestShores[i].river)
		{
			sm_ClosestShores[i].river->DrawShoreLines(true,0,false);
		}
	}
}
//------------------------------------------------------------------------------------------------------------------------------
void audShoreLineRiver::DrawShoreLines(bool drawSettingsPoints, const u32 pointIndex, bool checkForPlaying)	const
{
	if(checkForPlaying)
	{
		return;
	}
	// draw lines
	if(drawSettingsPoints && GetRiverSettings() && (u32)GetRiverSettings()->numShoreLinePoints > 0 && m_WaterHeights.GetCount() 
		&& GetRiverSettings()->numShoreLinePoints == m_WaterHeights.GetCount())
	{
		for(u32 i = 0; i < (u32)GetRiverSettings()->numShoreLinePoints; i++)
		{
			Vector3 currentPoint = Vector3(GetRiverSettings()->ShoreLinePoints[i].X, GetRiverSettings()->ShoreLinePoints[i].Y, m_WaterHeights[i] + 0.3f);
			char buf[128];
			formatf(buf, sizeof(buf), "%u", i);
			Color32 textColor(255,0,0);
			Color32 sphereColor(255,0,0,128);
			if(i == pointIndex)
			{
				textColor = Color_green;
				sphereColor = Color32(0,255,0,128);
			}
			grcDebugDraw::Text(currentPoint, textColor, buf);
			grcDebugDraw::Sphere(currentPoint,0.1f,sphereColor);
			if(i < GetRiverSettings()->numShoreLinePoints - 1 )
			{
				Vector3 nextPoint = Vector3(GetRiverSettings()->ShoreLinePoints[i+1].X, GetRiverSettings()->ShoreLinePoints[i+1].Y, m_WaterHeights[i+1] + 0.3f);
				Vector3 riverDir = nextPoint - currentPoint;
				riverDir.Normalize();
				riverDir.RotateZ(PI/2);
				Vector3 currentRightPoint = currentPoint + (riverDir * GetRiverSettings()->ShoreLinePoints[i].RiverWidth * 0.5f);
				Vector3 currentLeftPoint = currentPoint - (riverDir * GetRiverSettings()->ShoreLinePoints[i].RiverWidth * 0.5f);
				grcDebugDraw::Sphere(currentRightPoint,0.1f,sphereColor);
				grcDebugDraw::Sphere(currentLeftPoint,0.1f,sphereColor);
				grcDebugDraw::Line(currentPoint, nextPoint,	Color_red);
				grcDebugDraw::Line(currentPoint, currentRightPoint,	Color_red);
				grcDebugDraw::Line(currentPoint, currentLeftPoint,	Color_red);
			}
		}
		// Draw link
		if(m_NextShoreline && m_NextShoreline->GetWaterHeightsCount() > 0 && (u32)m_NextShoreline->GetRiverSettings()->numShoreLinePoints == m_NextShoreline->GetWaterHeightsCount())
		{
			Vector3 lastPoint = Vector3(GetRiverSettings()->ShoreLinePoints[GetRiverSettings()->numShoreLinePoints-1].X, GetRiverSettings()->ShoreLinePoints[GetRiverSettings()->numShoreLinePoints-1].Y, m_WaterHeights[GetRiverSettings()->numShoreLinePoints-1] + 0.3f);
			Vector3 firstPoint = Vector3(m_NextShoreline->GetRiverSettings()->ShoreLinePoints[0].X, m_NextShoreline->GetRiverSettings()->ShoreLinePoints[0].Y, m_NextShoreline->GetWaterHeightOnPoint(0) + 0.3f);
			grcDebugDraw::Line(lastPoint, firstPoint,	Color_red);
		}
	}
	if(m_ShoreLinePoints.GetCount() && m_WaterHeights.GetCount() && m_ShoreLinePoints.GetCount() == m_WaterHeights.GetCount())
	{
		for(u32 i = 0; i < (u32)m_ShoreLinePoints.GetCount(); i++)
		{
			Vector3 currentPoint = Vector3(m_ShoreLinePoints[i].x, m_ShoreLinePoints[i].y, m_WaterHeights[i] + 0.3f);
			char buf[128];
			formatf(buf, sizeof(buf), "%u", i);
			Color32 textColor(255,0,0);
			Color32 sphereColor(255,0,0,128);
			if(i == pointIndex)
			{
				textColor = Color_green;
				sphereColor = Color32(0,255,0,128);
			}
			grcDebugDraw::Text(currentPoint, textColor, buf);
			grcDebugDraw::Sphere(currentPoint,0.1f,sphereColor);
			if(i < m_ShoreLinePoints.GetCount() - 1 )
			{
				Vector3 nextPoint = Vector3(m_ShoreLinePoints[i+1].x, m_ShoreLinePoints[i+1].y, m_WaterHeights[i+1] + 0.3f);
				Vector3 riverDir = nextPoint - currentPoint;
				riverDir.Normalize();
				riverDir.RotateZ(PI/2);
				Vector3 currentRightPoint = currentPoint + (riverDir * m_ShoreLinePointsWidth[i] * 0.5f);
				Vector3 currentLeftPoint = currentPoint - (riverDir * m_ShoreLinePointsWidth[i] * 0.5f);
				grcDebugDraw::Sphere(currentRightPoint,0.1f,sphereColor);
				grcDebugDraw::Sphere(currentLeftPoint,0.1f,sphereColor);
				grcDebugDraw::Line(currentPoint, nextPoint,	Color_red);
				grcDebugDraw::Line(currentPoint, currentRightPoint,	Color_red);
				grcDebugDraw::Line(currentPoint, currentLeftPoint,	Color_red);
			}
		}
	}
}
// ----------------------------------------------------------------
void audShoreLineRiver::RenderDebug()const 
{
	audShoreLine::RenderDebug();	
	for(u32 i = 0; i < (u32)GetRiverSettings()->numShoreLinePoints - 1; i++)
	{
		CVectorMap::DrawLine(Vector3(GetRiverSettings()->ShoreLinePoints[i].X, GetRiverSettings()->ShoreLinePoints[i].Y, 0.f), 
			Vector3(GetRiverSettings()->ShoreLinePoints[i+1].X, GetRiverSettings()->ShoreLinePoints[i+1].Y, 0.f),
			Color_red,Color_blue);
	}
}
// ----------------------------------------------------------------
void audShoreLineRiver::AddDebugShorePoint(const Vector2 &vec)
{
	if(m_ShoreLinePoints.GetCount() < sm_MaxNumPointsPerShore)
	{
		m_ShoreLinePoints.PushAndGrow(vec);
		m_ShoreLinePointsWidth.PushAndGrow(sm_RiverWidthInCurrentPoint);
		m_ActivationBox.Add(vec.x,vec.y);
		m_Width = m_ActivationBox.GetWidth();
		m_Height = m_ActivationBox.GetHeight();
	}	
	else
	{
		naErrorf("Max number of points per shoreline reached, please serialise the current shoreline and start creating a new one.");
	}
}
//------------------------------------------------------------------------------------------------------------------------------
u32 audShoreLineRiver::GetNumPoints()
{
	if(m_Settings)
	{
		return GetRiverSettings()->numShoreLinePoints;
	}
	return 0;
}
//------------------------------------------------------------------------------------------------------------------------------
void audShoreLineRiver::EditShorelinePoint(const u32 pointIdx, const Vector3 &newPos)
{
	if (pointIdx < sm_MaxNumPointsPerShore)
	{
		if(m_Settings)
		{
			if(naVerifyf(pointIdx < GetRiverSettings()->numShoreLinePoints,"Wrong point index"))
			{
				m_ShoreLinePoints[pointIdx].x = newPos.GetX();
				m_ShoreLinePoints[pointIdx].y = newPos.GetY();
			}
		}
		else
		{
			if(naVerifyf(pointIdx < m_ShoreLinePoints.GetCount(),"Wrong point index"))
			{
				m_ShoreLinePoints[pointIdx].x = newPos.GetX();
				m_ShoreLinePoints[pointIdx].y = newPos.GetY();
			}
		}
	}
}
//------------------------------------------------------------------------------------------------------------------------------
void audShoreLineRiver::EditShorelinePointWidth(const u32 pointIdx)
{
	if (pointIdx < sm_MaxNumPointsPerShore)
	{
		if(m_Settings)
		{
			if(naVerifyf(pointIdx < GetRiverSettings()->numShoreLinePoints,"Wrong point index"))
			{
				m_ShoreLinePointsWidth[pointIdx] = sm_RiverWidthInCurrentPoint;
			}
		}
		else
		{
			if(naVerifyf(pointIdx < m_ShoreLinePoints.GetCount(),"Wrong point index"))
			{
				m_ShoreLinePointsWidth[pointIdx] = sm_RiverWidthInCurrentPoint;
			}
		}
	}
}
//------------------------------------------------------------------------------------------------------------------------------
void audShoreLineRiver::MoveShoreAlongDirection(const Vector2 &dirOfMovement,const u8 /*side*/)
{	
	for (u32 i = 0; i < m_ShoreLinePoints.GetCount(); i++)
	{
		m_ShoreLinePoints[i] = m_ShoreLinePoints[i] + dirOfMovement;
	}
}
//------------------------------------------------------------------------------------------------------------------------------
void audShoreLineRiver::Cancel()
{	
	audShoreLine::Cancel();
	sm_RiverWidthInCurrentPoint = 1.f;
}
//------------------------------------------------------------------------------------------------------------------------------
void SetWidthCB()
{
	 g_AmbientAudioEntity.SetRiverWidthAtPoint();
}
//------------------------------------------------------------------------------------------------------------------------------
void ResetSoundsCB()
{
	for(u8 i = 0; i < AUD_MAX_RIVERS_PLAYING; i ++)
	{
		audShoreLineRiver::StopSounds(i);
	}
}
// ----------------------------------------------------------------
// Add widgets to RAG tool
// ----------------------------------------------------------------
void audShoreLineRiver::AddWidgets(bkBank &bank)
{
	bank.PushGroup("River");
		bank.PushGroup("River movement behaviour");
			bank.AddSlider("Inner distance", &sm_InnerDistance , 0.f, 1000.f, 0.1f);
			bank.AddSlider("Outter distance", &sm_OutterDistance, 0.f, 1000.f, 0.1f);
			bank.AddSlider("Max distance to shore", &sm_MaxDistanceToShore, 0.f, 100.f, 0.1f);
			bank.AddButton("ResetSounds", CFA(ResetSoundsCB));
		bank.PopGroup();
	bank.PopGroup();
}
//------------------------------------------------------------------------------------------------------------------------------
void audShoreLineRiver::AddEditorWidgets(bkBank &bank)
{
	bank.PushGroup("River parameters");
	bank.AddSlider("River width in current point", &sm_RiverWidthInCurrentPoint, 0.f, 1000.f, 0.1f);
	bank.AddButton("Set width at point", CFA(SetWidthCB));
	bank.PopGroup();
}
#endif
