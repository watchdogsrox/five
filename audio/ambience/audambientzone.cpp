//  
// audio/audshoreline.cpp
//  
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#include "audambientzone.h"
#include "water/audshoreline.h"
#include "ambientaudioentity.h"
#include "audioengine/curve.h"
#include "audioengine/widgets.h" 
#include "audio/northaudioengine.h"
#include "audioengine/engine.h"
#include "audio/vehiclereflectionsaudioentity.h"
#include "game/Clock.h"
#include "game/weather.h"
#include "physics/WorldProbe/worldprobe.h"
#include "scene/world/GameWorldHeightMap.h"
#include "water/audshorelineOcean.h"
#include "replaycoordinator/ReplayCoordinator.h"

AUDIO_AMBIENCE_OPTIMISATIONS()

BANK_ONLY(extern char g_DrawZoneFilter[128];)
BANK_ONLY(extern bool g_DrawInteriorZones;)
BANK_ONLY(extern s32 g_DrawAmbientZones;)
BANK_ONLY(extern s32 g_AmbientZoneRenderType;)
BANK_ONLY(extern s32 g_ActivationZoneRenderType;)
BANK_ONLY(extern bool g_DrawActivationZoneAABB;)
BANK_ONLY(extern audAmbientZone* g_CurrentEditZone;)
BANK_ONLY(extern s32 g_EditZoneAttribute;)
BANK_ONLY(extern s32 g_EditZoneAnchor;)
BANK_ONLY(extern bool g_TestZoneOverWaterLogic;)
BANK_ONLY(extern bool g_StopAllRules;)
BANK_ONLY(extern bool g_DrawZoneAnchorPoint;)

extern audCurve g_BlockedToVolume;

s32 audAmbientZone::sm_NumStreamingRulesPlaying = 0;
audWaveSlotManager audAmbientZone::sm_WaveSlotManager;
const AmbientBankMap* audAmbientZone::sm_BankMap;
atArray<audDLCBankMap> audAmbientZone::sm_DLCBankMaps;
AmbientSlotMap* audAmbientZone::sm_SlotMap;
bool audAmbientZone::sm_HasBeenInitialised = false;
audCurve audAmbientZone::sm_DefaultPedDensityTODCurve;
atRangeArray<AmbientRule*, 30> audAmbientZone::sm_GlobalPlayingRules;
atRangeArray<audAmbientZone::AmbientZoneMixerScene, audAmbientZone::kMaxAmbientZoneMixerScenes> audAmbientZone::sm_AmbientZoneMixerScenes;

// ----------------------------------------------------------------
// Initialise the zone
// ----------------------------------------------------------------
void audAmbientZone::Init(AmbientZone *zone, u32 zoneNameHash, bool isInteriorLinkedZone, f32 masterRotationAngle)
{
	naAssertf(zone, "Null AmbientZone passed as parameter to audAmbientZone::Init");
	m_ZoneData = zone;
	m_DefaultEnabledState = AUD_GET_TRISTATE_VALUE(m_ZoneData->Flags, FLAG_ID_AMBIENTZONE_ZONEENABLEDSTATEPERSISTENT);
	m_ZoneNameHash = zoneNameHash;
	m_MasterRotationAngle = masterRotationAngle;
	m_IsInteriorLinkedZone = isInteriorLinkedZone;

	m_InitialActivationZone = zone->ActivationZone;
	m_InitialPositioningZone = zone->PositioningZone;

	m_PedDensityTOD.Init(zone->PedDensityTOD);

	InitBounds();
	m_ForceStateUpdate = false;
	m_Active = false;
	m_IsReflectionZone = false;

	BANK_ONLY(m_Name = audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(zone->NameTableOffset));

#if __BANK
	if(m_PedDensityTOD.IsValid())
	{
		audDisplayf("Ambient zone %s has a custom TOD curve (%s)", GetName(), m_PedDensityTOD.GetName());
	}
#endif

	m_NumRules = zone->numRules;
	m_BuiltUpFactor = zone->BuiltUpFactor;
	m_IsSphere = (zone->Shape == kAmbientZoneSphere || zone->Shape == kAmbientZoneSphereLineEmitter);
	bool shorelinesRequired = false;
	
	for(u32 i = 0; i < m_NumRules; i++)
	{
		AmbientRule* rule = audNorthAudioEngine::GetObject<AmbientRule>(zone->Rule[i].Ref);

		if(!m_IsInteriorLinkedZone && !shorelinesRequired && AUD_GET_TRISTATE_VALUE(rule->Flags, FLAG_ID_AMBIENTRULE_CANTRIGGEROVERWATER) == AUD_TRISTATE_FALSE)
		{
			shorelinesRequired = true;
		}

		if(!rule)
		{
			NOTFINAL_ONLY(naErrorf("Ambient zone with invalid rule: %s", audNorthAudioEngine::GetMetadataManager().GetNameFromNTO_Debug(zone->NameTableOffset));)
		}
		else
		{
			rule->LastPlayTime = UINT_MAX - rule->MinRepeatTime;
		}
	}

	// Need to do a bit of funky pointer manipulation here due to having two variable length lists one after another
	u8* numDirectionalAmbiences = NULL;
	AmbientZone::tDirAmbience* directionalAmbiences = NULL;

	if(zone->numRules == 0)
	{
		// If there is no rules list, the directional ambience count value is packed immediately after the rules count u8
		numDirectionalAmbiences = (u8*)(&zone->numRules + sizeof(u8));
		directionalAmbiences = (AmbientZone::tDirAmbience*)(&zone->numRules + sizeof(u32));
	}
	else
	{
		numDirectionalAmbiences = (u8*)(&zone->Rule[zone->numRules]);
		directionalAmbiences = (AmbientZone::tDirAmbience*)(numDirectionalAmbiences + sizeof(u32));
	}

	audAssert(*numDirectionalAmbiences <= AmbientZone::MAX_DIRAMBIENCES);

	for(u8 i=0; i < *numDirectionalAmbiences; i++)
	{
		g_AmbientAudioEntity.AddDirectionalAmbience(audNorthAudioEngine::GetObject<DirectionalAmbience>(directionalAmbiences[i].Ref), this, directionalAmbiences[i].Volume);
	}

	if(shorelinesRequired)
	{
		ComputeShoreLines();
		Assign(m_NumShoreLines, m_ShoreLines.GetCount());
		m_ShoreLines.Reset();
	}
	else
	{
		m_NumShoreLines = 0;
	}
}

// ----------------------------------------------------------------
// Compute any shorelines that this zone contains
// ----------------------------------------------------------------
void audAmbientZone::ComputeShoreLines()
{
	g_AmbientAudioEntity.GetAllShorelinesIntersecting(GetPositioningZoneAABB(), m_ShoreLines);
}

// ----------------------------------------------------------------
// Initialise any static class parameters
// ----------------------------------------------------------------
void audAmbientZone::InitClass()
{
	sm_DefaultPedDensityTODCurve.Init("PED_WALLA_DEFAULT_TOD_CURVE");
	sm_SlotMap = audNorthAudioEngine::GetObject<AmbientSlotMap>(ATSTRINGHASH("AMBIENCE_SLOT_MAP", 0x091feda5a));

	audMetadataObjectInfo info;
	if(audVerifyf(audNorthAudioEngine::GetMetadataManager().GetObjectInfoFromSpecificChunk(ATSTRINGHASH("AMBIENCE_BANK_MAP_AUTOGENERATED", 0x08e3df280), 0, info), "Could not find ambient bank map"))
	{
		sm_BankMap = info.GetObject<AmbientBankMap>();
	}

	if(sm_SlotMap)
	{
		for(u32 loop = 0; loop < sm_SlotMap->numSlotMaps; loop++)
		{
			sm_WaveSlotManager.AddWaveSlot(sm_SlotMap->SlotMap[loop].WaveSlotName, sm_SlotMap->SlotMap[loop].SlotType, sm_SlotMap->SlotMap[loop].Priority);
		}
	}

	for(u32 loop = 0; loop < sm_GlobalPlayingRules.GetMaxCount(); loop++)
	{
		sm_GlobalPlayingRules[loop] = NULL;
	}

	sm_HasBeenInitialised = true;
}

// ----------------------------------------------------------------
// Initialise the bounds of the zone
// ----------------------------------------------------------------
void audAmbientZone::InitBounds()
{
	ComputeActivationZoneCenter();
	ComputePositioningZoneCenter();

	if(m_ZoneData->Shape == kAmbientZoneCuboidLineEmitter ||
		m_ZoneData->Shape == kAmbientZoneSphereLineEmitter)
	{
		CalculatePositioningLine();
	}
}

// ----------------------------------------------------------------
// Work out the max number of rules to play at once
// ----------------------------------------------------------------
u32 audAmbientZone::GetMaxSimultaneousRules() const 
{ 
	if(m_ZoneData)
	{
		return Min(g_MaxPlayingRules, (u32)m_ZoneData->NumRulesToPlay);
	}
	else
	{
		return 0;
	}
}

// ----------------------------------------------------------------
// Give a 0-1 ratio of how high a point is compared to the top/bottom of the positioning zone
// ----------------------------------------------------------------
f32 audAmbientZone::GetHeightFactorInPositioningZone(Vec3V_In pos) const
{
	return ComputeZoneInfluenceRatioAtPoint(pos);
}
// ----------------------------------------------------------------
// Check if the current ambient zone is flagged to occlude rain
// ----------------------------------------------------------------
bool audAmbientZone::HasToOccludeRain()
{
	if( m_ZoneData )
	{
		return AUD_GET_TRISTATE_VALUE(m_ZoneData->Flags, FLAG_ID_AMBIENTZONE_OCCLUDERAIN) == AUD_TRISTATE_TRUE;
	}
	return false;
}
// ----------------------------------------------------------------
// Check if the given point is within the activation range of the zone
// ----------------------------------------------------------------
bool audAmbientZone::IsPointInActivationRange(Vec3V_In position) const
{
	if(!IsSphere())
	{
		// cuboid test
		// create rotation matrix, centered at box centre
		Mat34V mat;
		Mat34VFromZAxisAngle(mat, ScalarV((m_ZoneData->ActivationZone.RotationAngle + m_MasterRotationAngle) * DtoR));
		mat.Setd(m_ActivationZoneCenter);
		// convert listener position to matrix space
		const Vec3V boxRelativePos = UnTransformOrtho(mat, position);
		spdAABB activationBox = spdAABB(m_ActivationZoneSize*ScalarV(V_NEGHALF),m_ActivationZoneSize*ScalarV(V_HALF));
		return activationBox.ContainsPoint(boxRelativePos);
	}
	else
	{
		const ScalarV maxDist = m_ActivationZoneSize.GetX();
		return IsTrue(IsLessThanOrEqual(MagSquared(m_ActivationZoneCenter - position), (maxDist*maxDist)));
	}
}

// ----------------------------------------------------------------
// Check if the given point is within the positioning range of the zone
// ----------------------------------------------------------------
bool audAmbientZone::IsPointInPositioningZone(Vec3V_In position) const
{
	if(!IsSphere())
	{
		// cuboid test

		// create rotation matrix, centered at box centre
		Mat34V mat;
		Mat34VFromZAxisAngle(mat, ScalarV((m_ZoneData->PositioningZone.RotationAngle + m_MasterRotationAngle) * DtoR));
		mat.Setd(m_PositioningZoneCenter);
		// convert listener position to matrix space
		const Vec3V boxRelativePos = UnTransformOrtho(mat, position);
		spdAABB positioningBox = spdAABB(m_PositioningZoneSize*ScalarV(V_NEGHALF),m_PositioningZoneSize*ScalarV(V_HALF));
		return positioningBox.ContainsPoint(boxRelativePos);
	}
	else
	{
		const ScalarV maxDist = m_PositioningZoneSize.GetX();
		return IsTrue(IsLessThanOrEqual(MagSquared(m_PositioningZoneCenter - position), (maxDist*maxDist)));
	}
}

// ----------------------------------------------------------------
// Check if the given point is within the positioning range of the zone
// ----------------------------------------------------------------
bool audAmbientZone::IsPointInPositioningZoneFlat(Vec3V_In position) const
{
	// create rotation matrix, centered at box centre
	Mat34V mat;
	Mat34VFromZAxisAngle(mat, ScalarV((m_ZoneData->PositioningZone.RotationAngle + m_MasterRotationAngle) * DtoR));
	mat.Setd(m_PositioningZoneCenter);
	// convert listener position to matrix space
	const Vec3V boxRelativePos = UnTransformOrtho(mat, position);
	spdAABB positioningBox = spdAABB(m_PositioningZoneSize*ScalarV(V_NEGHALF),m_PositioningZoneSize*ScalarV(V_HALF));
	return positioningBox.ContainsPointFlat(boxRelativePos);
}

// ----------------------------------------------------------------
// Check which rules could possibly play, and update their banks as required
// ----------------------------------------------------------------
void audAmbientZone::UpdateDynamicWaveSlots(const u16 gameTimeMinutes, bool isRaining, Vec3V_In listenerPos)
{
	// Update the wave slots in a random fashion (start at a random index, then randomly loop around forward
	// or backward). This just ensures that we don't always get the first N rules hogging all the wave slots
	if(m_NumRules == 0 || !IsActive())
	{
		return;
	}

	s32 numRulesUpdated = 0;
	s32 currentRuleIndex = audEngineUtil::GetRandomNumberInRange(0, m_NumRules - 1);
	bool walkDirectionForwards = audEngineUtil::GetRandomNumberInRange(0, 100) % 2 == 0;

	Vec3V closestPointToListener = ComputeClosestPointToPosition(listenerPos);
	Vec3V closestPointToPlayer = Vec3V(V_ZERO);
	bool closestPointToPlayerCalculated = false;

	while(numRulesUpdated < m_NumRules)
	{
		if(m_Rules[currentRuleIndex])
		{
			u32 bankId = m_Rules[currentRuleIndex]->DynamicBankID;
			u32 slotType = m_Rules[currentRuleIndex]->DynamicSlotType;

			if(m_WaveSlots[currentRuleIndex])
			{
				// The slot manager invalidated the wave slot - we need to re-request it
				if(m_WaveSlots[currentRuleIndex]->loadedBankId != bankId)
				{
					audAmbientZone::sm_WaveSlotManager.FreeWaveSlot(m_WaveSlots[currentRuleIndex]);
					continue;
				}
			}

			if(bankId < AUD_INVALID_BANK_ID)
			{
				bool ruleIsValid = IsValidTime(m_Rules[currentRuleIndex]->MinTimeMinutes, m_Rules[currentRuleIndex]->MaxTimeMinutes, gameTimeMinutes);

				if(AUD_GET_TRISTATE_VALUE(m_Rules[currentRuleIndex]->Flags, FLAG_ID_AMBIENTRULE_STOPWHENRAINING)==AUD_TRISTATE_TRUE && isRaining)
				{
					ruleIsValid = false;
				}

				for(u32 conditionLoop = 0; conditionLoop < m_Rules[currentRuleIndex]->numConditions && conditionLoop < AmbientRule::MAX_CONDITIONS; conditionLoop++)
				{
					if(m_Rules[currentRuleIndex]->Condition[conditionLoop].BankLoading == INFLUENCE_BANK_LOAD)
					{
						if(!IsConditionValid(m_Rules[currentRuleIndex], conditionLoop))
						{
							ruleIsValid = false;
							break;
						}
					}
				}

				if(ruleIsValid)
				{
					Vec3V ruleListenerPos = listenerPos;
					Vec3V closestPoint = closestPointToListener;

					if(AUD_GET_TRISTATE_VALUE(m_Rules[currentRuleIndex]->Flags, FLAG_ID_AMBIENTRULE_USEPLAYERPOSITION) == AUD_TRISTATE_TRUE)
					{
						ruleListenerPos = CGameWorld::FindLocalPlayer()->GetTransform().GetPosition();

						// This is reasonably rare, so don't bother calculating/cacheing in advance, only when a rule actually requires it
						if(!closestPointToPlayerCalculated)
						{
							closestPointToPlayer = ComputeClosestPointToPosition(CGameWorld::FindLocalPlayer()->GetTransform().GetPosition());
							closestPointToPlayerCalculated = true;
						}

						closestPoint = closestPointToPlayer;
					}

					const ScalarV dist2 = MagSquared(ruleListenerPos - closestPoint);
					const ScalarV ruleMaxDist = ScalarV(m_Rules[currentRuleIndex]->MaxDist);

					if(IsTrue(IsGreaterThan(dist2, (ruleMaxDist * ruleMaxDist))))
					{
						ruleIsValid = false;
					}
				}

				// If the rule is valid, try to load the appropriate bank
				if(ruleIsValid)
				{
					if(m_WaveSlots[currentRuleIndex] == NULL && g_AmbientAudioEntity.IsRuleStreamingAllowed() && audAmbientZone::sm_WaveSlotManager.GetNumLoadsInProgress() == 0)
					{
						m_WaveSlots[currentRuleIndex] = audAmbientZone::sm_WaveSlotManager.LoadBank(bankId, slotType, true);
					}
				}
				// If the rule isn't valid, mark the wave slot as not being used
				else
				{
					if(m_WaveSlots[currentRuleIndex])
					{
						audAmbientZone::sm_WaveSlotManager.FreeWaveSlot(m_WaveSlots[currentRuleIndex]);
					}
				}
			}
		}

		numRulesUpdated++;

		if(walkDirectionForwards)
		{
			currentRuleIndex++;

			if(currentRuleIndex >= m_NumRules)
			{
				currentRuleIndex = 0;
			}
		}
		else
		{
			currentRuleIndex--;

			if(currentRuleIndex < 0)
			{
				currentRuleIndex = m_NumRules - 1;
			}
		}
	}
}

// ----------------------------------------------------------------
// Free any streaming slots associated with this rule
// ----------------------------------------------------------------
void audAmbientZone::FreeRuleStreamingSlot(u32 ruleIndex)
{
	if(m_PlayingRules[ruleIndex].speechSlotID >= 0)
	{
		g_SpeechManager.FreeAmbientSpeechSlot(m_PlayingRules[ruleIndex].speechSlotID, true);
		m_PlayingRules[ruleIndex].speechSlotID = -1;
		m_PlayingRules[ruleIndex].speechSlot = NULL;
		sm_NumStreamingRulesPlaying--;
		audAssert(sm_NumStreamingRulesPlaying >= 0);
	}
}

// ----------------------------------------------------------------
// Start an ambient zone mixer scene
// ----------------------------------------------------------------
bool audAmbientZone::StartAmbientZoneMixerScene(u32 sceneName)
{
	if(sceneName != 0U)
	{
		s32 firstFreeIndex = -1;

		for(u32 loop = 0; loop < kMaxAmbientZoneMixerScenes; loop++)
		{
			if(sm_AmbientZoneMixerScenes[loop].mixerScene)
			{
				if(sm_AmbientZoneMixerScenes[loop].mixerSceneName == sceneName)
				{
					sm_AmbientZoneMixerScenes[loop].refCount++;
					return true;
				}
			}
			else if(firstFreeIndex == -1)
			{
				firstFreeIndex = loop;
			}
		}

		if(audVerifyf(firstFreeIndex >= 0, "Insufficient ambient zone mixer scene slots"))
		{
			DYNAMICMIXER.StartScene(sceneName, &sm_AmbientZoneMixerScenes[firstFreeIndex].mixerScene);

			if(sm_AmbientZoneMixerScenes[firstFreeIndex].mixerScene)
			{
				sm_AmbientZoneMixerScenes[firstFreeIndex].mixerSceneName = sceneName;
				sm_AmbientZoneMixerScenes[firstFreeIndex].refCount++;
				return true;
			}
		}
	}

	return false;
}

// ----------------------------------------------------------------
// Stop an ambient zone mixer scene
// ----------------------------------------------------------------
bool audAmbientZone::StopAmbientZoneMixerScene(u32 sceneName)
{
	if(sceneName != 0U)
	{
		for(u32 loop = 0; loop < kMaxAmbientZoneMixerScenes; loop++)
		{
			if(sm_AmbientZoneMixerScenes[loop].mixerScene &&
			   sm_AmbientZoneMixerScenes[loop].mixerSceneName == sceneName)
			{
				audAssert(sm_AmbientZoneMixerScenes[loop].refCount > 0);

				sm_AmbientZoneMixerScenes[loop].refCount--;
				
				if(sm_AmbientZoneMixerScenes[loop].refCount == 0)
				{
					sm_AmbientZoneMixerScenes[loop].mixerScene->Stop();
					sm_AmbientZoneMixerScenes[loop].mixerScene = NULL;
					sm_AmbientZoneMixerScenes[loop].mixerSceneName = 0u;
					sm_AmbientZoneMixerScenes[loop].applyValue = 0.0f;
					return true;
				}
			}
		}
	}

	return false;
}

// ----------------------------------------------------------------
// Set a mixer scene apply value
// ----------------------------------------------------------------
void audAmbientZone::SetMixerSceneApplyValue(u32 sceneName, f32 applyValue)
{
	if(sceneName != 0U)
	{
		for(u32 loop = 0; loop < kMaxAmbientZoneMixerScenes; loop++)
		{
			if(sm_AmbientZoneMixerScenes[loop].mixerScene &&
			   sm_AmbientZoneMixerScenes[loop].mixerSceneName == sceneName)
			{
				sm_AmbientZoneMixerScenes[loop].applyValue = Max(sm_AmbientZoneMixerScenes[loop].applyValue, applyValue);
				break;
			}
		}
	}
}

// ----------------------------------------------------------------
// Reset the mixer scene apply values
// ----------------------------------------------------------------
void audAmbientZone::ResetMixerSceneApplyValues()
{
	for(u32 loop = 0; loop < kMaxAmbientZoneMixerScenes; loop++)
	{
		sm_AmbientZoneMixerScenes[loop].applyValue = 0.0f;
	}
}

// ----------------------------------------------------------------
// Update the mixer scene apply values
// ----------------------------------------------------------------
void audAmbientZone::UpdateMixerSceneApplyValues()
{
	for(u32 loop = 0; loop < kMaxAmbientZoneMixerScenes; loop++)
	{
		if(sm_AmbientZoneMixerScenes[loop].mixerScene)
		{
			sm_AmbientZoneMixerScenes[loop].mixerScene->SetVariableValue(ATSTRINGHASH("apply", 0xE865CDE8), sm_AmbientZoneMixerScenes[loop].applyValue);
		}
	}
}

#if __BANK
// ----------------------------------------------------------------
// Debug draw mixer scenes
// ----------------------------------------------------------------
void audAmbientZone::DebugDrawMixerScenes()
{
	char tempString[256];
	f32 yCoord = 0.1f;
	f32 xCoord = 0.1f;
	
	for(u32 loop = 0; loop < kMaxAmbientZoneMixerScenes; loop++)
	{
		if(sm_AmbientZoneMixerScenes[loop].mixerScene)
		{
			formatf(tempString, "Scene:%s RefCount:%d Apply:%.02f", DYNAMICMIXMGR.GetMetadataManager().GetObjectNameFromNameTableOffset(sm_AmbientZoneMixerScenes[loop].mixerScene->GetSceneSettings()->NameTableOffset), sm_AmbientZoneMixerScenes[loop].refCount, sm_AmbientZoneMixerScenes[loop].applyValue);
		}
		else
		{
			formatf(tempString, "Empty");
		}

		grcDebugDraw::Text(Vector2(xCoord, yCoord), Color32(255,255,255), tempString);
		yCoord += 0.02f;
	}
}
#endif

// ----------------------------------------------------------------
// Register a global playing rule
// ----------------------------------------------------------------
bool audAmbientZone::RegisterGlobalPlayingRule(AmbientRule* rule)
{
	if(rule->MaxGlobalInstances > -1)
	{
		for(u32 loop = 0; loop < sm_GlobalPlayingRules.GetMaxCount(); loop++)
		{
			if(sm_GlobalPlayingRules[loop] == NULL)
			{
				sm_GlobalPlayingRules[loop] = rule;
				return true;
			}
		}
	}
	else
	{
		return true;
	}

	return false;
}

// ----------------------------------------------------------------
// Deregister a global playing rule
// ----------------------------------------------------------------
bool audAmbientZone::DeregisterGlobalPlayingRule(AmbientRule* rule)
{
	if(rule->MaxGlobalInstances > -1)
	{
		for(u32 loop = 0; loop < sm_GlobalPlayingRules.GetMaxCount(); loop++)
		{
			if(sm_GlobalPlayingRules[loop] == rule)
			{
				sm_GlobalPlayingRules[loop] = NULL;
				return true;
			}
		}
	}
	else
	{
		return true;
	}
	
	audAssert(0);
	return false;
}

// ----------------------------------------------------------------
// Check if the given rule is valid to play
// ----------------------------------------------------------------
bool audAmbientZone::IsGlobalRuleAllowedToPlay(AmbientRule* rule)
{
	u32 count = 0;
	audAssertf(rule->MaxGlobalInstances > -1, "Not a global rule");
	
	if(rule->MaxGlobalInstances == 0)
	{
		return false;
	}
	else
	{
		for(u32 loop = 0; loop < sm_GlobalPlayingRules.GetMaxCount(); loop++)
		{
			if(sm_GlobalPlayingRules[loop] == rule)
			{
				count++;

				if(count >= rule->MaxGlobalInstances)
				{
					return false;
				}
			}
		}
	}

	return true;
}

// ----------------------------------------------------------------
// Update the zone
// ----------------------------------------------------------------
void audAmbientZone::Update(const u32 timeInMs, Vec3V_In listenerPos, const bool isRaining, const f32 loudSoundExclusionRadius, const u16 gameTimeMinutes)
{
#if __BANK
	if(audNorthAudioEngine::GetMetadataManager().IsRAVEConnected() && this != g_CurrentEditZone && m_ZoneData)
	{
		bool ruleDataChanged = IsActive() && (m_ZoneData->numRules != m_Rules.GetCount());

		if(!ruleDataChanged)
		{
			for(u32 loop = 0; loop < m_ZoneData->numRules && loop < m_Rules.GetCount(); loop++)
			{
				AmbientRule* rule = audNorthAudioEngine::GetObject<AmbientRule>(m_ZoneData->Rule[loop].Ref);

				if(m_Rules[loop] != rule ||
				   (rule && m_Rules[loop] && (m_Rules[loop]->Sound != rule->Sound)))
				{
					ruleDataChanged = true;
					break;
				}
			}
		}

		if(ruleDataChanged)
		{
			StopAllSounds();

			for(u32 i = 0; i < m_WaveSlots.GetCount(); i++)
			{
				sm_WaveSlotManager.FreeWaveSlot(m_WaveSlots[i]);
			}

			m_NumRules = m_ZoneData->numRules;

			m_Rules.ResizeGrow(m_NumRules);
			m_WaveSlots.ResizeGrow(m_NumRules);

			for(u32 i = 0; i < m_NumRules; i++)
			{
				m_Rules[i] = NULL;
				m_WaveSlots[i] = NULL;
			}

			m_PlayingRules.ResizeGrow(GetMaxSimultaneousRules());

			for(u32 i = 0; i < m_NumRules; i++)
			{
				m_Rules[i] = audNorthAudioEngine::GetObject<AmbientRule>(m_ZoneData->Rule[i].Ref);

				if(!m_Rules[i])
				{
					naErrorf("Ambient zone with invalid rule: %s", audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(m_ZoneData->NameTableOffset));
				}
				else
				{
					m_Rules[i]->LastPlayTime = 0;
				}
			}

			CalculateDynamicBankUsage();
			InitBounds();
		}
		else if(g_DrawAmbientZones != audAmbientAudioEntity::AMBIENT_ZONE_RENDER_NONE || 
			    g_DrawInteriorZones)
		{
			InitBounds();
		}

		if(m_ZoneData->Shape == kAmbientZoneCuboidLineEmitter ||
			m_ZoneData->Shape == kAmbientZoneSphereLineEmitter)
		{
			CalculatePositioningLine();
		}

		if(IsActive())
		{
			for(u32 i = 0; i < m_NumRules; i++)
			{
				if(m_Rules[i])
				{
					if(m_Rules[i]->DynamicBankID == -1)
					{
						CalculateDynamicBankUsage();
						break;
					}
				}
			}
		}
	}
#endif // __BANK

	bool zoneEnabled = true;

#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		zoneEnabled = g_AmbientAudioEntity.IsZoneEnabledInReplay(m_ZoneNameHash, m_DefaultEnabledState);

		// Allow zones to switch off immediately once we load into the next replay clip
		if(IsActive() && !zoneEnabled && CReplayCoordinator::ShouldShowLoadingScreen())
		{
			ForceStateUpdate();
		}
	}
	else
#endif
	{
		bool hasTemporaryState = AUD_GET_TRISTATE_VALUE(m_ZoneData->Flags, FLAG_ID_AMBIENTZONE_ZONEENABLEDSTATENONPERSISTENT)!=AUD_TRISTATE_UNSPECIFIED;
		zoneEnabled = AUD_GET_TRISTATE_VALUE(m_ZoneData->Flags, FLAG_ID_AMBIENTZONE_ZONEENABLEDSTATEPERSISTENT)==AUD_TRISTATE_TRUE;

		// If this zone has a temporary modifier assigned, then defer to this, otherwise use the persistent state
		if(hasTemporaryState)
		{
			zoneEnabled = AUD_GET_TRISTATE_VALUE(m_ZoneData->Flags, FLAG_ID_AMBIENTZONE_ZONEENABLEDSTATENONPERSISTENT) == AUD_TRISTATE_TRUE;
		}
	}

	if(AUD_GET_TRISTATE_VALUE(m_ZoneData->Flags, FLAG_ID_AMBIENTZONE_DISABLEINMULTIPLAYER) == AUD_TRISTATE_TRUE)
	{
		if(NetworkInterface::IsGameInProgress()  REPLAY_ONLY(|| (CReplayMgr::IsEditModeActive() && CReplayMgr::IsMultiplayerClip())))
		{
			zoneEnabled = false;
		}
	}

	bool zoneActive = IsPointInActivationRange(listenerPos);

	// The zone enable flag only gets queried if the zone is currently inactive. Once the zone gets set to disabled, you have to leave 
	// its activation zone before it actually gets turned off.
	if(!m_Active || m_ForceStateUpdate)
	{
		zoneActive &= zoneEnabled;
	}
	
	if(zoneActive)
	{
		if(!m_Active)
		{
			SetActive(true);

			// force random delay for all rules
			for(u32 j = 0; j < m_NumRules; j++)
			{
				if(m_Rules[j])
				{
					bool ruleAlreadyActive = false;

					for(u32 zoneLoop = 0; zoneLoop < g_AmbientAudioEntity.GetNumActiveZones() && !ruleAlreadyActive; zoneLoop++)
					{
						const audAmbientZone* activeZone = g_AmbientAudioEntity.GetActiveZone(zoneLoop);

						if(activeZone != this)
						{
							for(u32 ruleLoop = 0; ruleLoop < activeZone->GetNumRules() && !ruleAlreadyActive; ruleLoop++)
							{
								if(activeZone->GetRule(ruleLoop) == m_Rules[j])
								{
									ruleAlreadyActive = true;
								}
							}
						}
					}

					if(!ruleAlreadyActive)
					{
						if(AUD_GET_TRISTATE_VALUE(m_Rules[j]->Flags, FLAG_ID_AMBIENTRULE_IGNOREINITIALMINREPEATTIME)==AUD_TRISTATE_FALSE)
						{
							// pick a delay time between no delay and the entire min repeat time
							const u32 delay = (u32)audEngineUtil::GetRandomNumberInRange(0, m_Rules[j]->MinRepeatTime *1000);
							// Min() here prevents underflow
							m_Rules[j]->LastPlayTime = Min(timeInMs, timeInMs - delay);
						}
					}
				}
			}
		}

		SetMixerSceneApplyValue(m_ZoneData->AudioScene, ComputeZoneInfluenceRatioAtPoint(listenerPos));

		u32 numPlaying = 0;
		for(u32 i = 0; i < m_PlayingRules.GetCount(); i++)
		{
			if(m_PlayingRules[i].sound)
			{
				Vec3V ruleListenerPos = listenerPos;

				if(AUD_GET_TRISTATE_VALUE(m_PlayingRules[i].rule->Flags, FLAG_ID_AMBIENTRULE_USEPLAYERPOSITION) == AUD_TRISTATE_TRUE)
				{
					ruleListenerPos = CGameWorld::FindLocalPlayer()->GetTransform().GetPosition();
				}

				// Reposition the sound if required
				if(m_PlayingRules[i].followListener)
				{
					if(m_PlayingRules[i].spawnHeight == AMBIENCE_WORLD_BLANKET_HEIGHT)
					{
						m_PlayingRules[i].position = ComputeClosestPointToPosition(ComputeClosestPointOnGround(ruleListenerPos));
					}
					else
					{
						m_PlayingRules[i].position = ComputeClosestPointToPosition(ruleListenerPos);
					}

					m_PlayingRules[i].sound->SetRequestedPosition(m_PlayingRules[i].position);

					if(m_PlayingRules[i].environmentGroup)
					{
						m_PlayingRules[i].environmentGroup->SetSource(m_PlayingRules[i].position);
					}
				}

				// check that this rule shouldn't be stopped due to distance or time
				const f32 dist2 = MagSquared(ruleListenerPos - m_PlayingRules[i].position).Getf();

				// Once a rule is actually playing, allow a little leeway so that we can rotate the camera etc. without it instantly snapping off
				const f32 hysteresisDist2 = 10.0f * 10.0f;

				bool validTimeToPlay = IsValidTime(m_PlayingRules[i].minTimeMinutes, m_PlayingRules[i].maxTimeMinutes, gameTimeMinutes);
				bool validDistanceToPlay = IsValidDistance(dist2, m_PlayingRules[i].minDistance2 - hysteresisDist2, m_PlayingRules[i].maxDistance2 + hysteresisDist2);
				bool validConditions = true;

				if(AUD_GET_TRISTATE_VALUE(m_PlayingRules[i].rule->Flags, FLAG_ID_AMBIENTRULE_STOPONZONEDEACTIVATION) == AUD_TRISTATE_FALSE)
				{
					validDistanceToPlay = true;
					validTimeToPlay = true;
				}

				if(m_PlayingRules[i].stopOnLoudSound && dist2 < (loudSoundExclusionRadius * loudSoundExclusionRadius))
				{
					validDistanceToPlay = false;
				}

				if(AUD_GET_TRISTATE_VALUE(m_PlayingRules[i].rule->Flags, FLAG_ID_AMBIENTRULE_CHECKCONDITIONSEACHFRAME) == AUD_TRISTATE_TRUE)
				{
					validConditions = AreAllConditionsValid(m_PlayingRules[i].rule, m_PlayingRules[i].sound);
				}

#if __BANK
				if(g_StopAllRules)
				{
					validTimeToPlay = false;
				}
#endif

				if( !validDistanceToPlay ||
					!validConditions ||
					!validTimeToPlay ||
					(m_PlayingRules[i].stopWhenRaining && isRaining))
				{
					m_PlayingRules[i].sound->StopAndForget();
					FreeRuleStreamingSlot(i);
				}
				else
				{
					numPlaying++;	
				}			
			}
			else
			{
				if(m_PlayingRules[i].rule)
				{
					DeregisterGlobalPlayingRule(m_PlayingRules[i].rule);
					m_PlayingRules[i].rule = NULL;
				}
				
				FreeRuleStreamingSlot(i);
			}
		}

		bool* validRuleToPlay = Alloca(bool, m_NumRules);

		for(u32 j = 0; j < m_NumRules; j++)
		{
			validRuleToPlay[j] = IsValidRuleToPlay(j, timeInMs, gameTimeMinutes, isRaining);
		}

		// We're now pre-computing the rule validity checks, but this means that we can only trigger one rule per frame
		// as triggering a rule can potentially invalidate furthur instances from playing (if eg. we exceed the local/global instance count)
		u32 numRulesPlayedThisFrame = 0;

		// check for new sounds to start
		for(u32 count = numPlaying; count < m_PlayingRules.GetCount() && numRulesPlayedThisFrame == 0; count++)
		{
			f32 weightSum = 0.f;

			// pick a new rule to play
			for(u32 j = 0; j < m_NumRules; j++)
			{
				if(validRuleToPlay[j])
				{
					weightSum += m_Rules[j]->Weight;	
				}
			}

			if(weightSum > 0.f)
			{
				f32 r = audEngineUtil::GetRandomNumberInRange(0.f, weightSum);
				for(u32 j = 0; j < m_NumRules; j++)
				{
					if(validRuleToPlay[j])
					{
						r -= m_Rules[j]->Weight;
						if(r <= 0.f)
						{
							s32 index = -1;
							for(u32 loop = 0; loop < m_PlayingRules.GetCount(); loop++)
							{
								if(!m_PlayingRules[loop].sound)
								{
									index = loop;
									break;
								}
							}

							if(index >= 0)
							{
								Vec3V ruleListenerPos = listenerPos;

								if(AUD_GET_TRISTATE_VALUE(m_Rules[j]->Flags, FLAG_ID_AMBIENTRULE_USEPLAYERPOSITION) == AUD_TRISTATE_TRUE)
								{
									ruleListenerPos = CGameWorld::FindLocalPlayer()->GetTransform().GetPosition();
								}

								m_PlayingRules[index].minDistance2 = m_Rules[j]->MinDist*m_Rules[j]->MinDist;
								m_PlayingRules[index].maxDistance2 = m_Rules[j]->MaxDist*m_Rules[j]->MaxDist;
								m_PlayingRules[index].minTimeMinutes = m_Rules[j]->MinTimeMinutes;
								m_PlayingRules[index].maxTimeMinutes = m_Rules[j]->MaxTimeMinutes;

								// Rule should always be clear by this point, but just to be safe check and unregister if necessary
								if(m_PlayingRules[index].rule)
								{
									DeregisterGlobalPlayingRule(m_PlayingRules[index].rule);
									m_PlayingRules[index].rule = NULL;
								}

								m_PlayingRules[index].triggerOnLoudSound = AUD_GET_TRISTATE_VALUE(m_Rules[j]->Flags, FLAG_ID_AMBIENTRULE_TRIGGERONLOUDSOUND)==AUD_TRISTATE_TRUE;
								m_PlayingRules[index].stopOnLoudSound = AUD_GET_TRISTATE_VALUE(m_Rules[j]->Flags, FLAG_ID_AMBIENTRULE_STOPONLOUDSOUND)==AUD_TRISTATE_TRUE;
								m_PlayingRules[index].stopWhenRaining = AUD_GET_TRISTATE_VALUE(m_Rules[j]->Flags, FLAG_ID_AMBIENTRULE_STOPWHENRAINING)==AUD_TRISTATE_TRUE;
								m_PlayingRules[index].spawnHeight = m_Rules[j]->SpawnHeight;

								// If the sound is set to follow the listener, calculate the closest point within the zone
								if(AUD_GET_TRISTATE_VALUE(m_Rules[j]->Flags, FLAG_ID_AMBIENTRULE_FOLLOWLISTENER)==AUD_TRISTATE_TRUE)
								{
									if(m_PlayingRules[index].spawnHeight == AMBIENCE_WORLD_BLANKET_HEIGHT)
									{
										m_PlayingRules[index].position = ComputeClosestPointToPosition(ComputeClosestPointOnGround(ruleListenerPos));
									}
									else
									{
										m_PlayingRules[index].position = ComputeClosestPointToPosition(ruleListenerPos);
									}

									m_PlayingRules[index].followListener = true;
								}
								// Otherwise pick a random position within the zone			
								else
								{

									switch (m_Rules[j]->ExplicitSpawnPositionUsage)
									{
									case RULE_EXPLICIT_SPAWN_POS_INTERIOR_RELATIVE:
										{
											CInteriorInst* pIntInst = CPortalVisTracker::GetPrimaryInteriorInst();

											if(pIntInst)
											{
												m_PlayingRules[index].position = pIntInst->GetTransform().Transform(RCC_VEC3V(m_Rules[j]->ExplicitSpawnPosition));
											}
											else
											{
#if __BANK
												naDisplayf("Trying to play an interior relative sound for zone %s, but no active interior found", GetName());
#endif
												continue;
											}
										}
										break;

									case RULE_EXPLICIT_SPAWN_POS_WORLD_RELATIVE:
										m_PlayingRules[index].position = RCC_VEC3V(m_Rules[j]->ExplicitSpawnPosition);
										break;

									default:
										m_PlayingRules[index].position = ComputeRandomPositionForRule(ruleListenerPos, m_Rules[j]);
										break;
									}

									if(m_PlayingRules[index].spawnHeight == AMBIENCE_WORLD_BLANKET_HEIGHT || m_PlayingRules[index].spawnHeight == AMBIENCE_HEIGHT_WORLD_BLANKET_HEIGHT_PLUS_EXPLICIT)
									{
										const Vec3V blanketHeight = ComputeClosestPointOnGround(m_PlayingRules[index].position);
										m_PlayingRules[index].position.SetZ(blanketHeight.GetZ());
									}
									else if(m_PlayingRules[index].spawnHeight == AMBIENCE_LISTENER_HEIGHT || m_PlayingRules[index].spawnHeight == AMBIENCE_LISTENER_HEIGHT_HEIGHT_PLUS_EXPLICIT)
									{
										m_PlayingRules[index].position.SetZ(ruleListenerPos.GetZ());
									}

									m_PlayingRules[index].followListener = false;
								}

								BANK_ONLY(m_PlayingRules[index].name = audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(m_Rules[j]->NameTableOffset));

								const f32 dist2 = MagSquared(m_PlayingRules[index].position - ruleListenerPos).Getf();
								bool distanceValid = IsValidDistance(dist2, m_PlayingRules[index].minDistance2, m_PlayingRules[index].maxDistance2);

								if(m_PlayingRules[index].stopOnLoudSound && dist2 < (loudSoundExclusionRadius * loudSoundExclusionRadius))
								{
									distanceValid = false;
								}

								if(AUD_GET_TRISTATE_VALUE(m_Rules[j]->Flags, FLAG_ID_AMBIENTRULE_CANTRIGGEROVERWATER) == AUD_TRISTATE_FALSE)
								{
									if(IsPointOverWater(m_PlayingRules[index].position))
									{
										distanceValid = false;
									}
								}

								if(distanceValid)
								{
									audSoundInitParams initParams;
									initParams.Position = RCC_VECTOR3(m_PlayingRules[index].position);
									bool streamingRule = (AUD_GET_TRISTATE_VALUE(m_Rules[j]->Flags, FLAG_ID_AMBIENTRULE_STREAMINGSOUND) == AUD_TRISTATE_TRUE);

									if(streamingRule)
									{
										// Only allow one streaming rule at once
										if(sm_NumStreamingRulesPlaying > 0)
										{
											continue;
										}

										// Only allow if there are lots of slots free
										if(g_SpeechManager.GetNumVacantAmbientSlots() <= 3)
										{
											continue;
										}

										// Try to grab a free ambient speech slot at low priority
										s32 speechSlotId = g_SpeechManager.GetAmbientSpeechSlot(NULL, NULL, -1.0f);

										if(speechSlotId >= 0)
										{
											audWaveSlot* speechSlot = g_SpeechManager.GetAmbientWaveSlotFromId(speechSlotId);

											// Success! Use this slot to load and play our asset
											if(speechSlot)
											{
												m_PlayingRules[index].speechSlotID = speechSlotId;
												m_PlayingRules[index].speechSlot = speechSlot;
												initParams.WaveSlot = speechSlot;
												initParams.AllowLoad = true;
												initParams.PrepareTimeLimit = 5000;
												sm_NumStreamingRulesPlaying++;
												g_SpeechManager.PopulateAmbientSpeechSlotWithPlayingSpeechInfo(speechSlotId, 0, 
#if __BANK
													m_PlayingRules[index].name,
#endif
													0);
											}
											else
											{
												g_SpeechManager.FreeAmbientSpeechSlot(m_PlayingRules[index].speechSlotID, true);
												continue;
											}
										}
										else
										{
											continue;
										}
									}
									else if(m_WaveSlots[j])
									{
										initParams.WaveSlot = m_WaveSlots[j]->waveSlot;
									}

									if(m_Rules[j]->Category != ATSTRINGHASH("BASE", 0x44E21C90))
									{
										initParams.Category = g_AudioEngine.GetCategoryManager().GetCategoryPtr(m_Rules[j]->Category);
									}

									m_PlayingRules[index].environmentGroup = NULL;

									if(audNorthAudioEngine::GetOcclusionManager()->GetIsPortalOcclusionEnabled() && 
										AUD_GET_TRISTATE_VALUE(m_Rules[j]->Flags, FLAG_ID_AMBIENTRULE_USEOCCLUSION) != AUD_TRISTATE_FALSE)
									{
										naEnvironmentGroup* environmentGroup = naEnvironmentGroup::Allocate("AmbientRule");
										if(environmentGroup)
										{
											environmentGroup->Init(NULL, 20);
											environmentGroup->SetSource(m_PlayingRules[index].position);
											environmentGroup->SetUsePortalOcclusion(true);
											environmentGroup->SetMaxPathDepth(m_Rules[j]->MaxPathDepth);
											environmentGroup->SetUseInteriorDistanceMetrics(true);
											environmentGroup->SetUseCloseProximityGroupsForUpdates(true);

											if(AUD_GET_TRISTATE_VALUE(m_ZoneData->Flags, FLAG_ID_AMBIENTZONE_INTERIORZONE)==AUD_TRISTATE_TRUE &&
												m_InteriorProxy && m_InteriorProxy->GetInteriorInst())
											{
												environmentGroup->SetInteriorInfoWithInteriorInstance(m_InteriorProxy->GetInteriorInst(), m_RoomIdx);
												initParams.EnvironmentGroup = environmentGroup;
												m_PlayingRules[index].environmentGroup = environmentGroup;
											}
											else
											{
												environmentGroup->SetInteriorInfoWithInteriorInstance(NULL, INTLOC_ROOMINDEX_LIMBO);
												initParams.EnvironmentGroup = environmentGroup;
												m_PlayingRules[index].environmentGroup = environmentGroup;
											}
										}
									}

									g_AmbientAudioEntity.CreateSound_PersistentReference(m_Rules[j]->Sound, &m_PlayingRules[index].sound, &initParams);

									if(m_PlayingRules[index].sound)
									{
										bool played = false;

										// Check the conditions again after triggering, in case there are some sound-based conditions that we can't
										// evaluate until after triggering
										if(AreAllConditionsValid(m_Rules[j], m_PlayingRules[index].sound))
										{
											if(RegisterGlobalPlayingRule(m_Rules[j]))
											{
												// All ambience sounds are masked to the first microphone. 
												m_PlayingRules[index].sound->SetRequestedListenerMask(1);

												// Calculate new play time
												s32 repeatTimeVariance = audEngineUtil::GetRandomNumberInRange(-((s32)m_Rules[j]->MinRepeatTimeVariance), (s32)m_Rules[j]->MinRepeatTimeVariance);
												m_Rules[j]->LastPlayTime = timeInMs + (repeatTimeVariance * 1000);

												m_PlayingRules[index].rule = m_Rules[j];
												numRulesPlayedThisFrame++;

												if(streamingRule)
												{
													m_PlayingRules[index].sound->PrepareAndPlay(m_PlayingRules[index].speechSlot, true, 5000);
												}
												else
												{
													m_PlayingRules[index].sound->PrepareAndPlay();
												}
												
												played = true;
											}
										}

										if(!played)
										{
											m_PlayingRules[index].sound->StopAndForget();
											FreeRuleStreamingSlot(index);

											// Sound didn't play, offset the delay so that we don't spam the system
											if(AUD_GET_TRISTATE_VALUE(m_Rules[j]->Flags, FLAG_ID_AMBIENTRULE_TRIGGERONLOUDSOUND) != AUD_TRISTATE_TRUE)
											{										
												m_Rules[j]->LastPlayTime = timeInMs - (m_Rules[j]->MinRepeatTime*1000) + 500;
											}
										}
									}
								}
								else
								{
									// We failed to play for whatever reason. Wait half a second before re-trying so that we don't continually spam the system. Don't do this for 
									// loud-sound-triggered rules, as we want to spawn them quickly following a loud sound
									if(AUD_GET_TRISTATE_VALUE(m_Rules[j]->Flags, FLAG_ID_AMBIENTRULE_TRIGGERONLOUDSOUND) != AUD_TRISTATE_TRUE)
									{										
										m_Rules[j]->LastPlayTime = timeInMs - (m_Rules[j]->MinRepeatTime*1000) + 500;
									}
								}
							}

							break;
						}					
					}
				}
			}
		}
	}
	else
	{
		// Zone has just become inactive
		if(m_Active)
		{
			SetActive(false);
		}
	}
	
	// Update everything's volume for being blocked
	if(m_Active)
	{
		for(u32 i = 0;i < m_PlayingRules.GetCount(); i++)
		{
			if(m_PlayingRules[i].sound)
			{
				f32 sniperVol = 0.0f;
				if(!m_PlayingRules[i].followListener)
				{
					sniperVol = audNorthAudioEngine::GetMicrophones().GetSniperAmbienceVolume(m_PlayingRules[i].sound->GetRequestedPosition());
				}
				
				// If it's not an interior zone then we're using an environmentgroup, which processes our occlusion so don't do any blockage calculations
				if(audNorthAudioEngine::GetOcclusionManager()->GetIsPortalOcclusionEnabled())
				{
					m_PlayingRules[i].sound->SetRequestedPostSubmixVolumeAttenuation(sniperVol);
				}
				else
				{
					// Update it's volume to take account of local environment - so they don't play through walls
					f32 exteriorOcclusionFactor = audNorthAudioEngine::GetOcclusionManager()->GetExteriorOcclusionFor3DPosition(RCC_VECTOR3(m_PlayingRules[i].position));
					f32 blockedFactor = audNorthAudioEngine::GetOcclusionManager()->GetBlockedFactorFor3DPosition(RCC_VECTOR3(m_PlayingRules[i].position));
					f32 linearVolume = 1.0f;

					// Interior zones are occluded on the outside and audible on the inside, and ignore the blocked metric (since we're indoors, 
					// every direction is blocked!)
					if(AUD_GET_TRISTATE_VALUE(m_ZoneData->Flags, FLAG_ID_AMBIENTZONE_INTERIORZONE)==AUD_TRISTATE_TRUE)
					{
						linearVolume = exteriorOcclusionFactor;
					}
					// Interior zone flag must be explicitly true or false to cause occlusion. We use TRISTATE_UNDEFINED to mean 'do neither'
					else if(AUD_GET_TRISTATE_VALUE(m_ZoneData->Flags, FLAG_ID_AMBIENTZONE_INTERIORZONE)==AUD_TRISTATE_FALSE)
					{
						// The blockability factor determines in % how blocked this rule is allowed to get
						if(m_PlayingRules[i].rule->BlockabilityFactor < 100)
						{
							blockedFactor *= (m_PlayingRules[i].rule->BlockabilityFactor / 100.0f);
						}

						linearVolume = g_BlockedToVolume.CalculateValue(blockedFactor) * (1.0f-exteriorOcclusionFactor);
					}

					m_PlayingRules[i].sound->SetRequestedPostSubmixVolumeAttenuation(audDriverUtil::ComputeDbVolumeFromLinear(linearVolume) + sniperVol);
				}
			}
		}

		if(m_IsReflectionZone)
		{
			const Vector3 reflectionsSource = g_ReflectionsAudioEntity.GetCurrentSourcePosition();
			const Vec3V closestPointOnPositioningZone = ComputeClosestPointOnPositioningZoneBoundary(RCC_VEC3V(reflectionsSource));
			g_ReflectionsAudioEntity.AddAmbientZoneReflectionPosition(RCC_VECTOR3(closestPointOnPositioningZone), this);
		}
	}

	m_ForceStateUpdate = false;
}

// ---------------------------------------------------------------------------------------------
// Set the zone active/inactive
// ---------------------------------------------------------------------------------------------
void audAmbientZone::SetActive(bool active)
{
	if(active && !m_Active)
	{
		StartAmbientZoneMixerScene(m_ZoneData->AudioScene);
		m_PlayingRules.ResizeGrow(GetMaxSimultaneousRules());
		m_WaveSlots.ResizeGrow(m_NumRules);
		m_Rules.ResizeGrow(m_NumRules);

		for(u32 i = 0; i < m_NumRules; i++)
		{
			m_WaveSlots[i] = NULL;
			m_Rules[i] = audNorthAudioEngine::GetObject<AmbientRule>(m_ZoneData->Rule[i].Ref);
		}

		if(m_NumShoreLines > 0)
		{
			m_ShoreLines.Reserve(m_NumShoreLines);
			ComputeShoreLines();
		}

		m_Active = true;
	}
	else if(m_Active && !active)
	{
		StopAllSounds(false);
		FreeWaveSlots();

		m_PlayingRules.Reset();
		m_WaveSlots.Reset();
		m_Rules.Reset();
		m_ShoreLines.Reset();

		if(m_IsReflectionZone)
		{
			g_ReflectionsAudioEntity.RemoveAmbientZoneReflectionPosition(this);
		}

		m_Active = false;
		StopAmbientZoneMixerScene(m_ZoneData->AudioScene);
	}
}

// ---------------------------------------------------------------------------------------------
// Calculate the desired ped density values for this zone
// ---------------------------------------------------------------------------------------------
void audAmbientZone::GetPedDensityValues(f32& minDensity, f32& maxDensity) const
{
	const f32 hours = static_cast<f32>(CClock::GetHour()) + (CClock::GetMinute()/60.f);
	f32 densityScalar = 1.0f;

	if(!m_IsInteriorLinkedZone)
	{
		densityScalar = m_PedDensityTOD.IsValid()? m_PedDensityTOD.CalculateValue(hours) : sm_DefaultPedDensityTODCurve.CalculateValue(hours);
	}

	minDensity = Clamp(densityScalar * m_ZoneData->MinPedDensity, 0.0f, 1.0f);
	maxDensity = Clamp(densityScalar * m_ZoneData->MaxPedDensity, 0.0f, 1.0f);
}

// ---------------------------------------------------------------------------------------------
// Get the number of instances of a given rule that are curently playing
// ---------------------------------------------------------------------------------------------
u32 audAmbientZone::GetNumInstancesPlaying(AmbientRule* rule) const
{
	u32 numInstancesPlaying = 0;

	if(rule)
	{
		for(u32 loop = 0; loop < m_PlayingRules.GetCount(); loop++)
		{
			if(m_PlayingRules[loop].rule == rule &&
			   m_PlayingRules[loop].sound &&
			   m_PlayingRules[loop].rule)
			{
				numInstancesPlaying++;
			}
		}
	}

	return numInstancesPlaying;
}

// ---------------------------------------------------------------------------------------------
// Stop all sounds
// ---------------------------------------------------------------------------------------------
void audAmbientZone::StopAllSounds(bool forceStop)
{
	for(u32 i = 0; i < m_PlayingRules.GetCount(); i++)
	{
		if(m_PlayingRules[i].sound)
		{
			bool allowFinish = false;

			if(!forceStop)
			{
				allowFinish = m_PlayingRules[i].rule && (AUD_GET_TRISTATE_VALUE(m_PlayingRules[i].rule->Flags, FLAG_ID_AMBIENTRULE_STOPONZONEDEACTIVATION) == AUD_TRISTATE_FALSE);
			}

			// Certain one shot sounds are flagged as being allowed to finish playing when the zone deactivates, so just forget them and leave them to stop naturally
			if(allowFinish)
			{
				m_PlayingRules[i].sound->SetUpdateEntity(false);
				m_PlayingRules[i].sound->InvalidateEntitySoundReference();
				m_PlayingRules[i].sound = NULL;
			}
			else
			{
				m_PlayingRules[i].sound->StopAndForget();
			}
		}

		if(m_PlayingRules[i].rule)
		{
			DeregisterGlobalPlayingRule(m_PlayingRules[i].rule);
			m_PlayingRules[i].rule = NULL;
		}

		FreeRuleStreamingSlot(i);
	}

	FreeWaveSlots();
}

// ---------------------------------------------------------------------------------------------
// Free all the non playing wave slots
// ---------------------------------------------------------------------------------------------
void audAmbientZone::FreeWaveSlots()
{
	for(u32 i = 0; i< m_WaveSlots.GetCount(); i++)
	{
		audAmbientZone::sm_WaveSlotManager.FreeWaveSlot(m_WaveSlots[i]);
	}
}

// ---------------------------------------------------------------------------------------------
// Fix up the mapping between rules and dynamic banks
// ---------------------------------------------------------------------------------------------
void audAmbientZone::CalculateDynamicBankUsage()
{
	if(m_ZoneData)
	{
		const u32 numRules = m_Active? m_Rules.GetCount() : m_ZoneData->numRules;

		for(u32 loop = 0; loop < numRules; loop++)
		{
			AmbientRule* rule = m_Active? m_Rules[loop] : audNorthAudioEngine::GetObject<AmbientRule>(m_ZoneData->Rule[loop].Ref);

			if(rule && rule->DynamicBankID == -1 && (AUD_GET_TRISTATE_VALUE(rule->Flags, FLAG_ID_AMBIENTRULE_STREAMINGSOUND) != AUD_TRISTATE_TRUE))
			{
				DynamicBankListBuilderFn bankListBuilder;
				SOUNDFACTORY.ProcessHierarchy(rule->Sound, bankListBuilder);

				if(bankListBuilder.dynamicBankList.GetCount() == 1)
				{
					rule->DynamicBankID = bankListBuilder.dynamicBankList[0];

					// Ok - we know which bank this rule requires. Now we need to work out what slot type that bank gets loaded into
					if(sm_BankMap)
					{
						bool slotTypeFound = false;
						u32 bankHash = atStringHash(g_AudioEngine.GetSoundManager().GetFactory().GetBankNameFromIndex(rule->DynamicBankID));

						for(u32 bankMapLoop = 0; bankMapLoop < sm_BankMap->numBankMaps; bankMapLoop++)
						{
							// So we've found a bank->slot mapping for our bank
							if(sm_BankMap->BankMap[bankMapLoop].BankName == bankHash)
							{
								// Record the relevant slot in the rule. Now the rule is fully clued up - it know what bank it
								// needs to load and what slot type it needs to go into
								rule->DynamicSlotType = sm_BankMap->BankMap[bankMapLoop].SlotType;
								slotTypeFound = true;
							}
						}

						// Didn't find a slot yet - search dlc bank maps
						if(!slotTypeFound)
						{
							for(u32 dlcBankMapIndex = 0; dlcBankMapIndex < sm_DLCBankMaps.GetCount(); dlcBankMapIndex++)
							{
								const AmbientBankMap* dlcBankMap = sm_DLCBankMaps[dlcBankMapIndex].bankMap;

								for(u32 bankMapLoop = 0; bankMapLoop < dlcBankMap->numBankMaps; bankMapLoop++)
								{
									if(dlcBankMap->BankMap[bankMapLoop].BankName == bankHash)
									{
										rule->DynamicSlotType = dlcBankMap->BankMap[bankMapLoop].SlotType;
									}
								}
							}
						}
					}
				}
				else
				{
					rule->DynamicBankID = AUD_INVALID_BANK_ID;
				}

				naAssertf(bankListBuilder.dynamicBankList.GetCount() < 2, 
					"Ambient rule %s is referencing more than one dynamically loaded bank (%s + %s)", 
					audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(rule->NameTableOffset),
					g_AudioEngine.GetSoundManager().GetFactory().GetBankNameFromIndex(bankListBuilder.dynamicBankList[0]),
					g_AudioEngine.GetSoundManager().GetFactory().GetBankNameFromIndex(bankListBuilder.dynamicBankList[1]));
			}
		}
	}
}

// ----------------------------------------------------------------
// Check if the given point is over any of the water contained in this zone
// ----------------------------------------------------------------
bool audAmbientZone::IsPointOverWater(Vec3V_In pos) const
{
	// First check if the water is being forced to a particular state
	if(m_ZoneData->ZoneWaterCalculation == AMBIENT_ZONE_FORCE_OVER_LAND)
	{
		return false;
	}
	else if(m_ZoneData->ZoneWaterCalculation == AMBIENT_ZONE_FORCE_OVER_WATER)
	{
		return true;
	}
	
	Vector3 pos2D = VEC3V_TO_VECTOR3(pos);
	pos2D.z = 0.0f; 

	f32 closestOceanDist2 = LARGE_FLOAT;
	s32 closestOceanIndex = -1;

	for(s32 loop = 0; loop < m_ShoreLines.GetCount(); loop++)
	{
		audShoreLine* shoreLine = m_ShoreLines[loop];

		// We only want to check the closest ocean shoreline, so just use this loop to identify which one that is
		if(shoreLine->GetWaterType() == AUD_WATER_TYPE_OCEAN)
		{
			// We can make some assumptions about oceans - they're usually straight-ish lines in a similar direction direction, one after another, so we can make
			// do with just looking at the distance to the start and end points rather than traversing every point
			const ShoreLineOceanAudioSettings* settings = ((audShoreLineOcean*)shoreLine)->GetOceanSettings();
			const f32 dist2Start = (Vector3(settings->ShoreLinePoints[0].X, settings->ShoreLinePoints[0].Y, 0.f) - pos2D).Mag2();
			const f32 dist2End = (Vector3(settings->ShoreLinePoints[settings->numShoreLinePoints - 1].X, settings->ShoreLinePoints[settings->numShoreLinePoints - 1].Y ,0.f) - pos2D).Mag2();
			const f32 minDistance = Min(dist2Start, dist2End);

			if(minDistance < closestOceanDist2)
			{
				closestOceanDist2 = minDistance;
				closestOceanIndex = loop;
			}
		}
		else
		{
			// If the position isn't even in the shore line activation zone, no point checking further
			if(shoreLine->GetActivationBoxAABB().IsInside(Vector2(pos2D.x, pos2D.y)))
			{
				if(shoreLine->IsPointOverWater(VEC3V_TO_VECTOR3(pos)))
				{
					return true;
				}
			}
		}
	}

	if(closestOceanIndex >= 0)
	{
		if(m_ShoreLines[closestOceanIndex]->IsPointOverWater(VEC3V_TO_VECTOR3(pos)))
		{
			return true;
		}
	}

	return false;
}

// ----------------------------------------------------------------
// Encapsulates all the conditions for a new rule trying to play
// ----------------------------------------------------------------
bool audAmbientZone::IsValidRuleToPlay(const u32 ruleIndex, const u32 timeInMs, const u16 gameTimeMinutes, const bool isRaining) const
{
#if __BANK
	if(g_StopAllRules)
	{
		return false;
	}
#endif

	// evaluate rule
	if(!m_Rules[ruleIndex])
	{
		return false;
	}

	if(timeInMs <= m_Rules[ruleIndex]->LastPlayTime + m_Rules[ruleIndex]->MinRepeatTime*1000)
	{
		return false;
	}

	if(AUD_GET_TRISTATE_VALUE(m_Rules[ruleIndex]->Flags, FLAG_ID_AMBIENTRULE_TRIGGERONLOUDSOUND) == AUD_TRISTATE_TRUE)
	{
		// Use fwTimer to work out the elapsed time, as that's what GetLastLoudSoundTime uses
		if(fwTimer::GetTimeInMilliseconds() - audNorthAudioEngine::GetLastLoudSoundTime() > 500)
		{
			return false;
		}
	}

	if(AUD_GET_TRISTATE_VALUE(m_Rules[ruleIndex]->Flags, FLAG_ID_AMBIENTRULE_DISABLEINMULTIPLAYER) == AUD_TRISTATE_TRUE)
	{
		if(NetworkInterface::IsGameInProgress() REPLAY_ONLY(|| (CReplayMgr::IsEditModeActive() && CReplayMgr::IsMultiplayerClip())))
		{
			return false;
		}
	}

	if(!IsValidTime(m_Rules[ruleIndex]->MinTimeMinutes, m_Rules[ruleIndex]->MaxTimeMinutes, gameTimeMinutes))
	{
		// Countdown timer says its valid to play, but the game world time doesn't. Re-randomise the sound (as if its just played) so that multiple rules that 
		// share the same activation time won't all suddenly jump on at once
		if(AUD_GET_TRISTATE_VALUE(m_Rules[ruleIndex]->Flags, FLAG_ID_AMBIENTRULE_IGNOREINITIALMINREPEATTIME)==AUD_TRISTATE_FALSE)
		{
			s32 repeatTimeVariance = audEngineUtil::GetRandomNumberInRange(-((s32)m_Rules[ruleIndex]->MinRepeatTimeVariance), (s32)m_Rules[ruleIndex]->MinRepeatTimeVariance);
			m_Rules[ruleIndex]->LastPlayTime = timeInMs + (repeatTimeVariance * 1000);
		}

		return false;
	}

	if((AUD_GET_TRISTATE_VALUE(m_Rules[ruleIndex]->Flags, FLAG_ID_AMBIENTRULE_STOPWHENRAINING)==AUD_TRISTATE_TRUE && isRaining))
	{
		return false;
	}

	if(!AreAllConditionsValid(m_Rules[ruleIndex]))
	{
		return false;
	}

	if(AUD_GET_TRISTATE_VALUE(m_Rules[ruleIndex]->Flags, FLAG_ID_AMBIENTRULE_STREAMINGSOUND) != AUD_TRISTATE_TRUE)
	{
		if(m_Rules[ruleIndex]->DynamicBankID != AUD_INVALID_BANK_ID)
		{
			// Can only play this if the appropriate bank has been loaded
			if(m_WaveSlots[ruleIndex] == NULL ||
				!m_WaveSlots[ruleIndex]->IsLoaded())
			{
				// Countdown timer says its valid to play, but the bank isn't loaded. Re-randomise the sound (as if its just played) so that multiple rules that 
				// share the same bank won't all suddenly start as soon as it loads
				if(AUD_GET_TRISTATE_VALUE(m_Rules[ruleIndex]->Flags, FLAG_ID_AMBIENTRULE_IGNOREINITIALMINREPEATTIME)==AUD_TRISTATE_FALSE)
				{
					s32 repeatTimeVariance = audEngineUtil::GetRandomNumberInRange(-((s32)m_Rules[ruleIndex]->MinRepeatTimeVariance), (s32)m_Rules[ruleIndex]->MinRepeatTimeVariance);
					m_Rules[ruleIndex]->LastPlayTime = timeInMs + (repeatTimeVariance * 1000);
				}

				return false;
			}
		}
	}
	else
	{
		if(!g_AmbientAudioEntity.IsRuleStreamingAllowed())
		{
			// Countdown timer says its valid to play, but rule streaming isn't allowed. Re-randomise the sound (as if its just played) so that all the streaming 
			// rules won't all suddenly trigger start as soon as we slow down
			if(AUD_GET_TRISTATE_VALUE(m_Rules[ruleIndex]->Flags, FLAG_ID_AMBIENTRULE_IGNOREINITIALMINREPEATTIME)==AUD_TRISTATE_FALSE)
			{
				s32 repeatTimeVariance = audEngineUtil::GetRandomNumberInRange(-((s32)m_Rules[ruleIndex]->MinRepeatTimeVariance), (s32)m_Rules[ruleIndex]->MinRepeatTimeVariance);
				m_Rules[ruleIndex]->LastPlayTime = timeInMs + (repeatTimeVariance * 1000);
			}

			return false;
		}
	}

	// If we've marked that we want to observe the maxLocalInstances rule
	if(m_Rules[ruleIndex]->MaxLocalInstances > -1)
	{
		if(m_Rules[ruleIndex]->MaxLocalInstances == 0 ||
		   GetNumInstancesPlaying(m_Rules[ruleIndex]) >= m_Rules[ruleIndex]->MaxLocalInstances)
		{
			return false;
		}
	}

	// If we've marked that we want to observe the maxGlobalInstances rule
	if(m_Rules[ruleIndex]->MaxGlobalInstances > -1)
	{
		if(!IsGlobalRuleAllowedToPlay(m_Rules[ruleIndex]))
		{
			return false;
		}
	}

	return true;
}

// ----------------------------------------------------------------
// Are the rule's conditions currently valid
// ----------------------------------------------------------------
bool audAmbientZone::AreAllConditionsValid(AmbientRule* rule, audSound* sound) const
{
	if(rule)
	{
		for(u32 loop = 0; loop < rule->numConditions && loop < AmbientRule::MAX_CONDITIONS; loop++)
		{
			if(!IsConditionValid(rule, loop, sound))
			{
				return false;
			}
		}
	}

	return true;
}

// ----------------------------------------------------------------
// Check if a single condition is valid
// ----------------------------------------------------------------
bool audAmbientZone::IsConditionValid(AmbientRule* rule, u32 conditionIndex, audSound* sound) const
{
	if(rule &&
	   conditionIndex < rule->numConditions)
	{
		if(rule->Condition[conditionIndex].ConditionVariable != 0)
		{
			f32 leftVariableValue = 0.0f;
			bool variableValid = false;
			
			// Try to find the variable in the global variable list
			f32* variableAddress = g_AudioEngine.GetSoundManager().GetVariableAddress(rule->Condition[conditionIndex].ConditionVariable);

			if(variableAddress)
			{
				leftVariableValue = *variableAddress;
				variableValid = true;
			}

			// No luck - attempt to query from the sound if one has been provided
			if(!variableValid && sound)
			{
				audDynamicValueId valueID = sound->GetDynamicVariableHandleFromName(rule->Condition[conditionIndex].ConditionVariable);

				// Manually handle a couple of the conditions - the voice position related dynamic variables won't update until the next sound update, but we want to
				// ensure that we're testing against the most recent requested value
				switch(valueID)
				{
				case AUD_DYNAMICVALUE_LISTENER_DISTANCE:
					{
						Vec3V ruleListenerPos;

						if(AUD_GET_TRISTATE_VALUE(rule->Flags, FLAG_ID_AMBIENTRULE_USEPLAYERPOSITION) == AUD_TRISTATE_TRUE)
						{
							ruleListenerPos = CGameWorld::FindLocalPlayer()->GetTransform().GetPosition();
						}
						else
						{
							ruleListenerPos = g_AudioEngine.GetEnvironment().GetPanningListenerPosition(0);
						}

						leftVariableValue = Dist(ruleListenerPos, sound->GetRequestedPosition()).Getf();
						variableValid = true;
					}
					break;
				case AUD_DYNAMICVALUE_POSX:
					leftVariableValue = sound->GetRequestedPosition().GetXf();
					variableValid = true;
					break;
				case AUD_DYNAMICVALUE_POSY:
					leftVariableValue = sound->GetRequestedPosition().GetYf();
					variableValid = true;
					break;
				case AUD_DYNAMICVALUE_POSZ:
					leftVariableValue = sound->GetRequestedPosition().GetZf();
					variableValid = true;
					break;
				default:
					variableValid = sound->FindVariableValue(rule->Condition[conditionIndex].ConditionVariable, leftVariableValue);
					break;
				}
			}

			if(variableValid)
			{
				f32 rightVariableValue = rule->Condition[conditionIndex].ConditionValue;
				bool conditionValid = false;

				// Now see what condition we have, and do the compare.
				switch (rule->Condition[conditionIndex].ConditionType)
				{
				case RULE_IF_CONDITION_LESS_THAN:
					if (FLOAT_LT(leftVariableValue, rightVariableValue))
					{
						conditionValid = true;
					}
					break;
				case RULE_IF_CONDITION_LESS_THAN_OR_EQUAL_TO:
					if (FLOAT_LTEQ(leftVariableValue, rightVariableValue))
					{
						conditionValid = true;
					}
					break;
				case RULE_IF_CONDITION_GREATER_THAN:
					if (FLOAT_GT(leftVariableValue, rightVariableValue))
					{
						conditionValid = true;
					}
					break;
				case RULE_IF_CONDITION_GREATER_THAN_OR_EQUAL_TO:
					if (FLOAT_GTEQ(leftVariableValue, rightVariableValue))
					{
						conditionValid = true;
					}
					break;
				case RULE_IF_CONDITION_EQUAL_TO:
					if (FLOAT_EQ(leftVariableValue, rightVariableValue))
					{
						conditionValid = true;
					}
					break;
				case RULE_IF_CONDITION_NOT_EQUAL_TO:
					if(!FLOAT_EQ(leftVariableValue, rightVariableValue))
					{
						conditionValid = true;
					}
					break;
				default:
					naAssert(0);
					break;
				}

				if(!conditionValid)
				{
					return false;
				}
			}
		}
	}
	
	return true;
}

// ----------------------------------------------------------------
// Compute the zone's 'influence ratio' at a given point. The influence ration
// is at max (1.0) inside the positioning zone, and fades down to 0.0f at the edge
// of the activation zone. Can be used when you need to scale a zone-controlled variable based
// on how close the listener is to the center of the zone
// ----------------------------------------------------------------
f32 audAmbientZone::ComputeZoneInfluenceRatioAtPoint(Vec3V_In point) const
{
	if(IsPointInPositioningZone(point))
	{
		return 1.0f;
	}
	else if(!IsPointInActivationRange(point))
	{
		return 0.0f;
	}
	else
	{
		Vec3V_Out closestPointOnPositioningZone = ComputeClosestPointToPosition(point);
		Vec3V_Out closestPointOnActivationZone = ComputeClosestPointOnActivationZoneBoundary(point);

		ScalarV positioningZoneDist = Dist(point, closestPointOnPositioningZone);
		ScalarV activationZoneDist = Dist(point, closestPointOnActivationZone);

		return (activationZoneDist / (positioningZoneDist + activationZoneDist)).Getf();
	}
}

// ----------------------------------------------------------------
// Generate a 2D rectangle that can be used for the quad tree
// ----------------------------------------------------------------
fwRect audAmbientZone::GetActivationZoneAABB() const
{
	fwRect activationZoneAABB;

	if(m_ZoneData)
	{
		Vec3V rectHalfSize = m_ActivationZoneSize * ScalarV(V_HALF);

		if(m_ZoneData->Shape == kAmbientZoneSphere ||
			m_ZoneData->Shape == kAmbientZoneSphereLineEmitter)
		{
			if(m_ZoneData->Shape == kAmbientZoneSphere)
			{
				rectHalfSize = m_ActivationZoneSize;
			}

			rectHalfSize.SetY(rectHalfSize.GetX());
		}

		// Calculate the 4 corners of the rectangle and rotate all the points by the required amount
		ScalarV rotationAngle = ScalarV((m_ZoneData->ActivationZone.RotationAngle + m_MasterRotationAngle) * DtoR);
		Vec2V topLeft = Rotate(Vec2V(Negate(rectHalfSize.GetX()), rectHalfSize.GetY()), rotationAngle);
		Vec2V topRight = Rotate(Vec2V(rectHalfSize.GetX(), rectHalfSize.GetY()), rotationAngle);
		Vec2V bottomLeft = Rotate(Vec2V(Negate(rectHalfSize.GetX()), Negate(rectHalfSize.GetY())), rotationAngle);
		Vec2V bottomRight = Rotate(Vec2V(rectHalfSize.GetX(), Negate(rectHalfSize.GetY())), rotationAngle);

		// Now work out the smallest box that contains all the points
		ScalarV minXSize = Min(Min(topLeft.GetX(), topRight.GetX()), Min(bottomLeft.GetX(), bottomRight.GetX()));
		ScalarV minYSize = Min(Min(topLeft.GetY(), topRight.GetY()), Min(bottomLeft.GetY(), bottomRight.GetY()));
		ScalarV maxXSize = Max(Max(topLeft.GetX(), topRight.GetX()), Max(bottomLeft.GetX(), bottomRight.GetX()));
		ScalarV maxYSize = Max(Max(topLeft.GetY(), topRight.GetY()), Max(bottomLeft.GetY(), bottomRight.GetY()));

		Vec2V offset = Vec2V(m_ActivationZoneCenter.GetX(), m_ActivationZoneCenter.GetY());
		activationZoneAABB = fwRect(RCC_VECTOR2(offset), ((maxXSize - minXSize) * ScalarV(V_HALF)).Getf(), ((maxYSize - minYSize) * ScalarV(V_HALF)).Getf());
	}

	return activationZoneAABB;
}

// ----------------------------------------------------------------
// Generate a 2D rectangle representing the positioning zone
// ----------------------------------------------------------------
fwRect audAmbientZone::GetPositioningZoneAABB() const
{
	fwRect positioningZoneAABB;

	if(m_ZoneData)
	{
		Vec3V rectHalfSize = m_PositioningZoneSize * ScalarV(V_HALF);

		if(m_ZoneData->Shape == kAmbientZoneSphere ||
			m_ZoneData->Shape == kAmbientZoneSphereLineEmitter)
		{
			if(m_ZoneData->Shape == kAmbientZoneSphere)
			{
				rectHalfSize = m_PositioningZoneSize;
			}

			rectHalfSize.SetY(rectHalfSize.GetX());
		}

		// Calculate the 4 corners of the rectangle and rotate all the points by the required amount
		ScalarV rotationAngle = ScalarV((m_ZoneData->PositioningZone.RotationAngle + m_MasterRotationAngle) * DtoR);
		Vec2V topLeft = Rotate(Vec2V(Negate(rectHalfSize.GetX()), rectHalfSize.GetY()), rotationAngle);
		Vec2V topRight = Rotate(Vec2V(rectHalfSize.GetX(), rectHalfSize.GetY()), rotationAngle);
		Vec2V bottomLeft = Rotate(Vec2V(Negate(rectHalfSize.GetX()), Negate(rectHalfSize.GetY())), rotationAngle);
		Vec2V bottomRight = Rotate(Vec2V(rectHalfSize.GetX(), Negate(rectHalfSize.GetY())), rotationAngle);

		// Now work out the smallest box that contains all the points
		ScalarV minXSize = Min(Min(topLeft.GetX(), topRight.GetX()), Min(bottomLeft.GetX(), bottomRight.GetX()));
		ScalarV minYSize = Min(Min(topLeft.GetY(), topRight.GetY()), Min(bottomLeft.GetY(), bottomRight.GetY()));
		ScalarV maxXSize = Max(Max(topLeft.GetX(), topRight.GetX()), Max(bottomLeft.GetX(), bottomRight.GetX()));
		ScalarV maxYSize = Max(Max(topLeft.GetY(), topRight.GetY()), Max(bottomLeft.GetY(), bottomRight.GetY()));

		Vec2V offset = Vec2V(m_PositioningZoneCenter.GetX(), m_PositioningZoneCenter.GetY());
		positioningZoneAABB = fwRect(RCC_VECTOR2(offset), ((maxXSize - minXSize) * ScalarV(V_HALF)).Getf(), ((maxYSize - minYSize) * ScalarV(V_HALF)).Getf());
	}	

	return positioningZoneAABB;
}

// ----------------------------------------------------------------
// Compute the closest point, clamped to the ground height
// ----------------------------------------------------------------
Vec3V_Out audAmbientZone::ComputeClosestPointOnGround(Vec3V_In position) const
{
	Vec3V returnPosition = position;

	// This does the trick, but probably too slow (optional async version seems to be possibly by specifying a different ELosContext value.
	// Also, does not exclude buildings, which we ideally would like it to (so that you don't get street level ambiences activating at the top of a building)
	/*
	f32 height = CWorldProbe::FindGroundZFor3DCoord(TOP_SURFACE, position);
	*/

	// This is fast, but inaccurate (just finds lowest point within whichever 50m square node the position resides). Also does not exclude buildings.
	f32 height = CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(position.GetXf(), position.GetYf());
	returnPosition.SetZ( height );
	return returnPosition;
}

// ----------------------------------------------------------------
// Calculate the positioning line
// ----------------------------------------------------------------
void audAmbientZone::CalculatePositioningLine()
{
	if(m_ZoneData->Shape == kAmbientZoneCuboidLineEmitter ||
		m_ZoneData->Shape == kAmbientZoneSphereLineEmitter)
	{
		// This would need a bit of work - we'd need the initial line coords to be specified in interior relative coordinates, and the 
		// debug editor code would need a once over to ensure that the correct params get modified by the debug pad input
		naAssertf(!m_IsInteriorLinkedZone, "Line based positioning zones not currently supported for interior linked zones");

		// We hijack the center/size parameters in order to define the positioning line
		Vector3 lineStart = m_ZoneData->PositioningZone.Centre;
		Vector3 lineEnd = m_ZoneData->PositioningZone.Size;
		Vector3 lineCenter = (lineStart + lineEnd)/2.0f;

		lineStart -= lineCenter;
		lineEnd -= lineCenter;

		lineStart.RotateZ(DtoR * (m_ZoneData->PositioningZone.RotationAngle + m_MasterRotationAngle));
		lineEnd.RotateZ(DtoR * (m_ZoneData->PositioningZone.RotationAngle + m_MasterRotationAngle));

		lineStart += lineCenter + m_ZoneData->PositioningZone.PostRotationOffset;
		lineEnd += lineCenter + m_ZoneData->PositioningZone.PostRotationOffset;

		m_PositioningLine = fwGeom::fwLine(lineStart, lineEnd);
	}
}

// ----------------------------------------------------------------
// Get the closest point within the zone boundary to the given position
// ----------------------------------------------------------------
Vec3V_Out audAmbientZone::ComputeClosestPointToPosition(Vec3V_In position) const
{
	if(m_ZoneData->Shape == kAmbientZoneCuboidLineEmitter ||
	   m_ZoneData->Shape == kAmbientZoneSphereLineEmitter)
	{
		Vec3V closestPoint;
		m_PositioningLine.FindClosestPointOnLineV(position, closestPoint);
		return closestPoint;
	}
	else if(IsSphere())
	{
		const ScalarV maxDist = m_PositioningZoneSize.GetX();
		const Vec3V closestPoint = SelectFT(IsLessThanOrEqual(MagSquared(m_PositioningZoneCenter - position), (maxDist*maxDist)), m_PositioningZoneCenter - (Normalize(m_PositioningZoneCenter - position) * maxDist), position);
		return closestPoint;
	}
	else
	{
		// create rotation matrix, centered at box centre
		Mat34V mat;
		Mat34VFromZAxisAngle(mat, ScalarV((m_ZoneData->PositioningZone.RotationAngle + m_MasterRotationAngle) * DtoR));
		mat.Setd(m_PositioningZoneCenter);

		// convert listener position to matrix space
		const Vec3V zoneSize = m_PositioningZoneSize*ScalarV(V_HALF);
		const Vec3V boxRelativePos = UnTransformOrtho(mat, position);
		const Vec3V boxRelativeAbs = Abs(boxRelativePos);

		// We're inside - sound should be placed on the listener
		if (IsLessThanAll(Subtract(boxRelativeAbs, zoneSize), Vec3V(V_ZERO)))
		{
			return position;
		}
		else // We're outside, sound should be placed at the closest point on the box to the player
		{
			const VecBoolV insideTest = IsLessThan(boxRelativeAbs, zoneSize);
			const Vec3V outsidePoint = SelectFT(VecBoolV(Vec::V4VConstant(V_80000000)), zoneSize, boxRelativePos);
			const Vec3V closestPoint = Transform(mat, SelectFT(insideTest, outsidePoint, boxRelativePos));
			return closestPoint;
		}
	}
}

// ----------------------------------------------------------------
// Compute the closest point on the activation zone
// ----------------------------------------------------------------
Vec3V_Out audAmbientZone::ComputeClosestPointOnActivationZoneBoundary(Vec3V_In position) const
{
	return ComputeClosestPointOnBoundary(position, m_ActivationZoneSize, m_ActivationZoneCenter, m_ZoneData->ActivationZone.RotationAngle);
}

// ----------------------------------------------------------------
// Compute the closest point on the positioning zone
// ----------------------------------------------------------------
Vec3V_Out audAmbientZone::ComputeClosestPointOnPositioningZoneBoundary(Vec3V_In position) const
{
	if(m_ZoneData->Shape == kAmbientZoneCuboidLineEmitter ||
		m_ZoneData->Shape == kAmbientZoneSphereLineEmitter)
	{
		Vec3V closestPoint;
		m_PositioningLine.FindClosestPointOnLineV(position, closestPoint);
		return closestPoint;
	}
	else
	{
		return ComputeClosestPointOnBoundary(position, m_PositioningZoneSize, m_PositioningZoneCenter, m_ZoneData->PositioningZone.RotationAngle);
	}
}

// ----------------------------------------------------------------
// Compute the closest point on a given boundary
// ----------------------------------------------------------------
Vec3V_Out audAmbientZone::ComputeClosestPointOnBoundary(Vec3V_In position, Vec3V_In size, Vec3V_In center, const f32 rotation) const
{
	if(IsSphere())
	{
		const ScalarV maxDist = size.GetX();
		const Vec3V posToCenter = Normalize(center - position);
		return center - (posToCenter * maxDist);
	}
	else
	{
		// create rotation matrix, centered at box centre
		Mat34V mat;
		Mat34VFromZAxisAngle(mat, ScalarV((rotation + m_MasterRotationAngle) * DtoR));
		mat.Setd(center);

		// convert listener position to matrix space
		const Vec3V boxRelativePos = UnTransformOrtho(mat, position);
		spdAABB activationBox = spdAABB(size*ScalarV(V_NEGHALF),size*ScalarV(V_HALF));

		const Vec3V closestPoint = Transform(mat, FindClosestPointOnAABB(activationBox, boxRelativePos));
		return closestPoint;
	}
}

// ----------------------------------------------------------------
// Compute the center of the activation zone
// ----------------------------------------------------------------
void audAmbientZone::ComputeActivationZoneCenter()
{
	m_ActivationZoneCenter.ZeroComponents();

	if(m_ZoneData)
	{
		// Interior zones shouldn't use any data coming from RAVE, only what we initially passed in
		if(m_IsInteriorLinkedZone)
		{
			m_ActivationZoneCenter = VECTOR3_TO_VEC3V(m_InitialActivationZone.Centre);
			m_ActivationZoneSize = VECTOR3_TO_VEC3V(m_InitialActivationZone.Size * m_ZoneData->ActivationZone.SizeScale);
		}
		else
		{
			m_ActivationZoneCenter = VECTOR3_TO_VEC3V(m_ZoneData->ActivationZone.Centre);
			m_ActivationZoneSize = VECTOR3_TO_VEC3V(m_ZoneData->ActivationZone.Size * m_ZoneData->ActivationZone.SizeScale);
		}
		
		Vec2V offset = Rotate(Vec2V(m_ZoneData->ActivationZone.PostRotationOffset.x, m_ZoneData->ActivationZone.PostRotationOffset.y), ScalarV((m_ZoneData->ActivationZone.RotationAngle + m_MasterRotationAngle) * DtoR));
		m_ActivationZoneCenter.SetX(m_ActivationZoneCenter.GetX() + offset.GetX());
		m_ActivationZoneCenter.SetY(m_ActivationZoneCenter.GetY() + offset.GetY());
		m_ActivationZoneCenter.SetZ(m_ActivationZoneCenter.GetZ() + ScalarV(m_ZoneData->ActivationZone.PostRotationOffset.z));
	}
}
 
// ----------------------------------------------------------------
// Compute the center of the positioning zone
// ----------------------------------------------------------------
void audAmbientZone::ComputePositioningZoneCenter()
{
	m_PositioningZoneCenter.ZeroComponents();

	if(m_ZoneData)
	{
		// Interior zones shouldn't use any data coming from RAVE, only what we initially passed in
		if(m_IsInteriorLinkedZone)
		{
			m_PositioningZoneCenter = VECTOR3_TO_VEC3V(m_InitialPositioningZone.Centre);
			m_PositioningZoneSize = VECTOR3_TO_VEC3V(m_InitialPositioningZone.Size * m_ZoneData->PositioningZone.SizeScale);
		}
		else
		{
			m_PositioningZoneCenter = VECTOR3_TO_VEC3V(m_ZoneData->PositioningZone.Centre);
			m_PositioningZoneSize = VECTOR3_TO_VEC3V(m_ZoneData->PositioningZone.Size * m_ZoneData->PositioningZone.SizeScale);
		}

		Vec2V offset = Rotate(Vec2V(m_ZoneData->PositioningZone.PostRotationOffset.x, m_ZoneData->PositioningZone.PostRotationOffset.y), ScalarV((m_ZoneData->PositioningZone.RotationAngle + m_MasterRotationAngle) * DtoR));
		m_PositioningZoneCenter.SetX(m_PositioningZoneCenter.GetX() + offset.GetX());
		m_PositioningZoneCenter.SetY(m_PositioningZoneCenter.GetY() + offset.GetY());
		m_PositioningZoneCenter.SetZ(m_PositioningZoneCenter.GetZ() + ScalarV(m_ZoneData->PositioningZone.PostRotationOffset.z));
	}
}

// ----------------------------------------------------------------
// Get a random position based on the parameters of the provided rule
// ----------------------------------------------------------------
Vec3V_Out audAmbientZone::ComputeRandomPositionForRule(Vec3V_In listenerPosition, const AmbientRule *rule) const
{	
	if(m_ZoneData->Shape == kAmbientZoneCuboidLineEmitter ||
		m_ZoneData->Shape == kAmbientZoneSphereLineEmitter)
	{
		const Vec3V randomPos = Vec3V(audEngineUtil::GetRandomNumberInRange(Min(m_PositioningLine.m_start.x, m_PositioningLine.m_end.x), Max(m_PositioningLine.m_start.x, m_PositioningLine.m_end.x)),
									  audEngineUtil::GetRandomNumberInRange(Min(m_PositioningLine.m_start.y, m_PositioningLine.m_end.y), Max(m_PositioningLine.m_start.y, m_PositioningLine.m_end.y)),
								      audEngineUtil::GetRandomNumberInRange(Min(m_PositioningLine.m_start.z, m_PositioningLine.m_end.z), Max(m_PositioningLine.m_start.z, m_PositioningLine.m_end.z)));

		return ComputeClosestPointToPosition(randomPos);
	}
	else if(IsSphere())
	{
		const Vec3V randomDirection = Normalize(Vec3V(audEngineUtil::GetRandomVectorInRange(Vec3V(V_NEGONE), Vec3V(V_ONE))));
		ScalarV randomDist = ScalarV(audEngineUtil::GetRandomNumberInRange(rule->MinDist, rule->MaxDist));
		Vec3V position = listenerPosition + (randomDist * randomDirection);
		return ComputeClosestPointToPosition(position);
	}
	else
	{		
		// create rotation matrix, centered at box centre
		Mat34V mat;
		Mat34VFromZAxisAngle(mat, ScalarV((m_ZoneData->PositioningZone.RotationAngle + m_MasterRotationAngle) * DtoR));
		mat.Setd(m_PositioningZoneCenter);

		// work everything out in matrix space, and transform at the end
		const Vec3V exclusionSphereCentre = UnTransformOrtho(mat, listenerPosition);
		const Vec3V boxMax = m_PositioningZoneSize * ScalarV(V_HALF);
		const Vec3V boxMin = m_PositioningZoneSize * ScalarV(V_NEGHALF);

		const ScalarV distXToMaxEdge = boxMax.GetX() - exclusionSphereCentre.GetX();
		const ScalarV distXToMinEdge = exclusionSphereCentre.GetX() - boxMin.GetX();
		const ScalarV distYToMaxEdge = boxMax.GetY() - exclusionSphereCentre.GetY();
		const ScalarV distYToMinEdge = exclusionSphereCentre.GetY() - boxMin.GetY();
		const ScalarV distZToMaxEdge = boxMax.GetZ() - exclusionSphereCentre.GetZ();
		const ScalarV distZToMinEdge = exclusionSphereCentre.GetZ() - boxMin.GetZ();
		const ScalarV rangeMin = ScalarV(rule->MinDist);

		Vec3V minPos, maxPos;

		// too close to the max edge; shift min X back toward the min edge (constrained by box)
		maxPos.SetX(SelectFT(IsLessThan(distXToMaxEdge, rangeMin), boxMax.GetX(), Max(exclusionSphereCentre.GetX() - rangeMin, boxMin.GetX())));
		minPos.SetX(SelectFT(IsLessThan(distXToMinEdge, rangeMin), boxMin.GetX(), Min(exclusionSphereCentre.GetX() + rangeMin, boxMax.GetX())));
		maxPos.SetY(SelectFT(IsLessThan(distYToMaxEdge, rangeMin), boxMax.GetY(), Max(exclusionSphereCentre.GetY() - rangeMin, boxMin.GetY())));
		minPos.SetY(SelectFT(IsLessThan(distYToMinEdge, rangeMin), boxMin.GetY(), Min(exclusionSphereCentre.GetY() + rangeMin, boxMax.GetY())));
		maxPos.SetZ(SelectFT(IsLessThan(distZToMaxEdge, rangeMin), boxMax.GetZ(), Max(exclusionSphereCentre.GetZ() - rangeMin, boxMin.GetZ())));
		minPos.SetZ(SelectFT(IsLessThan(distZToMinEdge, rangeMin), boxMin.GetZ(), Min(exclusionSphereCentre.GetZ() + rangeMin, boxMax.GetZ())));

		// constrain to maxdist
		const ScalarV ruleMaxDist = ScalarV(rule->MaxDist);
		minPos.SetX(Max(minPos.GetX(), exclusionSphereCentre.GetX() - ruleMaxDist, boxMin.GetX()));
		maxPos.SetX(Min(maxPos.GetX(), exclusionSphereCentre.GetX() + ruleMaxDist, boxMax.GetX()));
		minPos.SetY(Max(minPos.GetY(), exclusionSphereCentre.GetY() - ruleMaxDist, boxMin.GetY()));
		maxPos.SetY(Min(maxPos.GetY(), exclusionSphereCentre.GetY() + ruleMaxDist, boxMax.GetY()));
		minPos.SetZ(Max(minPos.GetZ(), exclusionSphereCentre.GetZ() - ruleMaxDist, boxMin.GetZ()));
		maxPos.SetZ(Min(maxPos.GetZ(), exclusionSphereCentre.GetZ() + ruleMaxDist, boxMax.GetZ()));

		const Vec3V randomPosition = audEngineUtil::GetRandomVectorInRange(minPos, maxPos);

		// transform position into world space
		const Vec3V finalPosition = Transform(mat, randomPosition);
		return finalPosition;
	}
}

#if __BANK
// ----------------------------------------------------------------
// Render debug info on this zone - eg. bounds and activation range
// ----------------------------------------------------------------
void audAmbientZone::DebugDraw() const
{
	if(g_CurrentEditZone != this)
	{
		if(g_DrawZoneFilter[0] != 0 && !audAmbientAudioEntity::MatchName(GetName(), g_DrawZoneFilter))
		{
			return;
		}
	}

	char tempString[64];
	const Vec3V listenerPos = g_AudioEngine.GetEnvironment().GetPanningListenerPosition(0);
	sprintf(tempString, "%s %.02f", GetName(), ComputeZoneInfluenceRatioAtPoint(listenerPos));

	grcDebugDraw::Text(m_PositioningZoneCenter, Color32(0,0,255), tempString);

	if(m_Active)
	{
		grcDebugDraw::Text(m_PositioningZoneCenter, Color32(0,255,0), tempString);
	}
	else
	{
		grcDebugDraw::Text(m_PositioningZoneCenter, Color32(255,0,0), tempString);
	}

	const Color32 zoneColor = m_Active ? Color32(0,255,0) : Color32(255,0,0);
	const Color32 activationRangeColor = Color32(75,75,0);

	if(g_CurrentEditZone == this)
	{
		const Color32 editZoneColor = Color32(0,255,0);

		if(g_EditZoneAttribute == audAmbientAudioEntity::AMBIENT_ZONE_EDIT_BOTH || g_EditZoneAttribute == audAmbientAudioEntity::AMBIENT_ZONE_EDIT_POSITIONING)
		{
			if(m_ZoneData->Shape == kAmbientZoneCuboidLineEmitter ||
				m_ZoneData->Shape == kAmbientZoneSphereLineEmitter)
			{
				if(AUD_GET_TRISTATE_VALUE(m_ZoneData->Flags, FLAG_ID_AMBIENTZONE_HASTUNNELREFLECTIONS) == AUD_TRISTATE_TRUE)
				{
					grcDebugDraw::Cylinder(VECTOR3_TO_VEC3V(m_PositioningLine.m_start), VECTOR3_TO_VEC3V(m_PositioningLine.m_end), Max(m_ZoneData->BuiltUpFactor * 100.0f, 1.0f), zoneColor, zoneColor);
				}
				else
				{
					grcDebugDraw::Line(m_PositioningLine.m_start, m_PositioningLine.m_end, zoneColor);
				}
			}
			else
			{
				DebugDrawBounds(m_PositioningZoneCenter, 
					m_PositioningZoneSize, 
					m_ZoneData->PositioningZone.RotationAngle + m_MasterRotationAngle, 
					editZoneColor, 
					true);

				DebugDrawBounds(m_PositioningZoneCenter, 
					m_PositioningZoneSize, 
					m_ZoneData->PositioningZone.RotationAngle + m_MasterRotationAngle, 
					editZoneColor, 
					false);
			}
			
			if(g_DrawZoneAnchorPoint && !m_IsInteriorLinkedZone && m_ZoneData->Shape == kAmbientZoneCuboid)
			{
				DrawAnchorPoint(m_PositioningZoneCenter, m_PositioningZoneSize, m_ZoneData->PositioningZone.RotationAngle + m_MasterRotationAngle);	
			}		
		}

		if(g_EditZoneAttribute == audAmbientAudioEntity::AMBIENT_ZONE_EDIT_BOTH || g_EditZoneAttribute == audAmbientAudioEntity::AMBIENT_ZONE_EDIT_ACTIVATION)
		{
			DebugDrawBounds(m_ActivationZoneCenter, 
				m_ActivationZoneSize, 
				m_ZoneData->ActivationZone.RotationAngle + m_MasterRotationAngle, 
				activationRangeColor, 
				true);

			DebugDrawBounds(m_ActivationZoneCenter, 
				m_ActivationZoneSize, 
				m_ZoneData->ActivationZone.RotationAngle + m_MasterRotationAngle, 
				activationRangeColor, 
				false);
			
			if(g_DrawZoneAnchorPoint && !m_IsInteriorLinkedZone && m_ZoneData->Shape == kAmbientZoneCuboid)
			{
				DrawAnchorPoint(m_ActivationZoneCenter, m_ActivationZoneSize, m_ZoneData->ActivationZone.RotationAngle + m_MasterRotationAngle);
			}			
		}
	}
	else
	{
		if(g_AmbientZoneRenderType != audAmbientAudioEntity::AMBIENT_ZONE_RENDER_TYPE_OFF)
		{
			if(m_ZoneData->Shape == kAmbientZoneCuboidLineEmitter ||
				m_ZoneData->Shape == kAmbientZoneSphereLineEmitter)
			{
				if(AUD_GET_TRISTATE_VALUE(m_ZoneData->Flags, FLAG_ID_AMBIENTZONE_HASTUNNELREFLECTIONS) == AUD_TRISTATE_TRUE)
				{
					grcDebugDraw::Cylinder(VECTOR3_TO_VEC3V(m_PositioningLine.m_start), VECTOR3_TO_VEC3V(m_PositioningLine.m_end), Max(m_ZoneData->BuiltUpFactor * 100.0f, 1.0f), zoneColor, zoneColor);
				}
				else
				{
					grcDebugDraw::Line(m_PositioningLine.m_start, m_PositioningLine.m_end, zoneColor);
				}
			}
			else
			{
				if(g_AmbientZoneRenderType == audAmbientAudioEntity::AMBIENT_ZONE_RENDER_TYPE_SOLID)
				{
					DebugDrawBounds(m_PositioningZoneCenter, 
						m_PositioningZoneSize, 
						m_ZoneData->PositioningZone.RotationAngle + m_MasterRotationAngle, 
						zoneColor, 
						true);
				}

				DebugDrawBounds(m_PositioningZoneCenter, 
					m_PositioningZoneSize, 
					m_ZoneData->PositioningZone.RotationAngle + m_MasterRotationAngle, 
					zoneColor, 
					false);
			}
		}

		if(g_ActivationZoneRenderType != audAmbientAudioEntity::AMBIENT_ZONE_RENDER_TYPE_OFF)
		{
			if(g_ActivationZoneRenderType == audAmbientAudioEntity::AMBIENT_ZONE_RENDER_TYPE_SOLID)
			{
				DebugDrawBounds(m_ActivationZoneCenter, 
					m_ActivationZoneSize, 
					m_ZoneData->ActivationZone.RotationAngle + m_MasterRotationAngle, 
					activationRangeColor, 
					true);
			}

			DebugDrawBounds(m_ActivationZoneCenter, 
				m_ActivationZoneSize, 
				m_ZoneData->ActivationZone.RotationAngle + m_MasterRotationAngle, 
				activationRangeColor, 
				false);

			// Draw a line connecting the ambience zone and its activation range if both are enabled
			if(g_AmbientZoneRenderType != audAmbientAudioEntity::AMBIENT_ZONE_RENDER_TYPE_OFF)
			{
				grcDebugDraw::Line(m_ActivationZoneCenter, m_PositioningZoneCenter, zoneColor);
			}
		}
	}

	if(g_DrawActivationZoneAABB)
	{
		// Keep the height, overwrite the width/depth
		Vec3V renderSize = m_ActivationZoneSize;
		fwRect activationZoneAABB = GetActivationZoneAABB();
		renderSize.SetXf(activationZoneAABB.GetWidth());
		renderSize.SetYf(activationZoneAABB.GetHeight());

		const Color32 quadTreeColor = Color32(0,0,255);

		DebugDrawBounds(m_ActivationZoneCenter,
			renderSize, 
			0.0f, 
			quadTreeColor, 
			false);
	}

	if(m_Active && g_TestZoneOverWaterLogic)
	{
		for(u32 ruleLoop = 0; ruleLoop < m_Rules.GetCount(); ruleLoop++)
		{
			for(u32 loop = 0; loop < 10; loop++)
			{
				Vec3V randomPosition = ComputeRandomPositionForRule(g_AudioEngine.GetEnvironment().GetPanningListenerPosition(0), m_Rules[ruleLoop]);
				bool isOverWater = IsPointOverWater(randomPosition);
				randomPosition.SetZf(Max(WorldProbe::FindGroundZFor3DCoord(TOP_SURFACE, RCC_VECTOR3(randomPosition)), 0.5f));
				grcDebugDraw::Sphere(randomPosition, 1.0f, isOverWater? Color32(0.0f, 0.0f, 1.0f, 0.4f) : Color32(0.0f, 1.0f, 0.0f, 0.4f), true, 10);
			}
		}
	}
}

// ----------------------------------------------------------------
// Draw a sphere representing an anchor point
// ----------------------------------------------------------------
void audAmbientZone::DrawAnchorPoint(Vec3V_In zoneCentre, Vec3V_In zoneSize, f32 rotationDegrees) const
{
	Mat34V mat(V_IDENTITY);			
	Mat34VRotateLocalZ(mat, ScalarVFromF32(rotationDegrees * DtoR));

	Vec3V xDirection = Transform(mat, Vec3V(V_X_AXIS_WZERO));
	Vec3V yDirection = Transform(mat, Vec3V(V_Y_AXIS_WZERO));
	Vec3V zDirection = Transform(mat, Vec3V(V_Z_AXIS_WZERO));
	Vec3V anchorPoint;

	switch(g_EditZoneAnchor)
	{
	case audAmbientAudioEntity::AMBIENT_ZONE_EDIT_ANCHOR_TOP_LEFT:					
		anchorPoint = zoneCentre - (xDirection * ScalarV(V_HALF) * zoneSize.GetX()) + (yDirection * ScalarV(V_HALF) * zoneSize.GetY()) + (zDirection * ScalarV(V_HALF) * zoneSize.GetZ());						
		break;
	case audAmbientAudioEntity::AMBIENT_ZONE_EDIT_ANCHOR_TOP_RIGHT:					
		anchorPoint = zoneCentre + (xDirection * ScalarV(V_HALF) * zoneSize.GetX()) + (yDirection * ScalarV(V_HALF) * zoneSize.GetY()) + (zDirection * ScalarV(V_HALF) * zoneSize.GetZ());												
		break;
	case audAmbientAudioEntity::AMBIENT_ZONE_EDIT_ANCHOR_BOTTOM_LEFT:					
		anchorPoint = zoneCentre - (xDirection * ScalarV(V_HALF) * zoneSize.GetX()) - (yDirection * ScalarV(V_HALF) * zoneSize.GetY()) + (zDirection * ScalarV(V_HALF) * zoneSize.GetZ());						
		break;
	case audAmbientAudioEntity::AMBIENT_ZONE_EDIT_ANCHOR_BOTTOM_RIGHT:	
		anchorPoint = zoneCentre + (xDirection * ScalarV(V_HALF) * zoneSize.GetX()) - (yDirection * ScalarV(V_HALF) * zoneSize.GetY()) + (zDirection * ScalarV(V_HALF) * zoneSize.GetZ());						
		break;
	default:
		anchorPoint = zoneCentre + (zDirection * ScalarV(V_HALF) * zoneSize.GetZ());
	}

	f32 zoneArea = zoneSize.GetXf() * zoneSize.GetYf();
	f32 sphereArea = zoneArea * 0.01f;
	grcDebugDraw::Sphere(anchorPoint, Min(5.0f, Sqrtf(sphereArea/3.14f)), Color32(255, 0, 0, 120), true);
}

// ----------------------------------------------------------------
// Draw the playing sounds for this zone
// ----------------------------------------------------------------
void audAmbientZone::DebugDrawPlayingSounds() const
{
	for(u32 i = 0; i < m_PlayingRules.GetCount(); i++)
	{
		if(m_PlayingRules[i].sound)
		{			
			char buf[128];
			const f32 distFromListener = Mag(g_AudioEngine.GetEnvironment().GetPanningListenerPosition(0) - m_PlayingRules[i].position).Getf();
#if __DEV
			formatf(buf, "%s\n%s\nDist:%.02f\nVol:%.02f", m_PlayingRules[i].name, m_PlayingRules[i].sound->GetName(), distFromListener,m_PlayingRules[i].sound->GetRequestedPostSubmixVolumeAttenuation());
#else
			formatf(buf, "%s\nDist:%.02f", m_PlayingRules[i].name, distFromListener);
#endif

			Vec3V textPosition = m_PlayingRules[i].position;
			Color32 sphereColor = Color32(0xE0000000 | atStringHash(m_PlayingRules[i].name));
			sphereColor.SetAlpha(255);

			grcDebugDraw::Line(m_PlayingRules[i].position, CGameWorld::FindLocalPlayer()->GetTransform().GetPosition(), Color32(255,255,255), Color32(255,255,255));
			grcDebugDraw::Text(textPosition, Color32(255,255,255), buf);			
			grcDebugDraw::Sphere(m_PlayingRules[i].position, 1.f, sphereColor, !m_PlayingRules[i].followListener, 1, 16);
		}
	}
}

// ----------------------------------------------------------------
// Helper function to draw a box/sphere of given dimensions
// ----------------------------------------------------------------
void audAmbientZone::DebugDrawBounds(Vec3V_In center, Vec3V_In size, const f32 rotationAngle, Color32 color, bool solid) const
{
	if(solid)
	{
		color.SetAlpha( 70 );
	}

	if(m_ZoneData->Shape == kAmbientZoneCuboid ||
	   m_ZoneData->Shape == kAmbientZoneCuboidLineEmitter)
	{
		// create rotation matrix, centered at box centre
		Mat34V mat(V_IDENTITY);
		Mat34VRotateLocalZ(mat, ScalarVFromF32(rotationAngle * DtoR));
		mat.SetCol3(center);

		const Vec3V boxMax = size * ScalarV(V_HALF);
		const Vec3V boxMin = size * ScalarV(V_NEGHALF);

		grcDebugDraw::BoxOriented(boxMin, boxMax, mat, color, solid);
	}
	else if(m_ZoneData->Shape == kAmbientZoneSphere ||
			m_ZoneData->Shape == kAmbientZoneSphereLineEmitter)
	{
		grcDebugDraw::Sphere(center, size.GetXf(), color, solid, 1, 32);
	}
}

// ----------------------------------------------------------------
// Display a list of all the playing rules in this zone
// ----------------------------------------------------------------
void audAmbientZone::DebugDrawRules(f32& playingRuleYOffset, f32& activeRuleYOffset) const
{
	if(m_Active)
	{
		// Draw all the rules that are currently playing
		for(atArray<audPlayingRuleState>::const_iterator i = m_PlayingRules.begin(); i != m_PlayingRules.end(); i++)
		{
			if(i->sound)
			{
				bool streaming = (i->rule != NULL && (AUD_GET_TRISTATE_VALUE(i->rule->Flags, FLAG_ID_AMBIENTRULE_STREAMINGSOUND) == AUD_TRISTATE_TRUE));
				grcDebugDraw::Text(Vector2(0.1f, playingRuleYOffset), streaming? Color32(0,255,0) : Color32(255,255,255), i->name );

				char tempString[16];
				f32 distFromListener = Mag(g_AudioEngine.GetEnvironment().GetPanningListenerPosition(0) - i->position).Getf();
				sprintf(tempString, "%.02f", distFromListener );
				grcDebugDraw::Text(Vector2(0.33f, playingRuleYOffset), streaming? Color32(0,255,0) : Color32(255,255,255), tempString );
				playingRuleYOffset += 0.02f;
			}
		}

		// Grab some basic info needed to work out if rules are allowed to play
		const u16 gameTimeMinutes = static_cast<u16>((CClock::GetHour() * 60) + CClock::GetMinute());
		const u32 timeInMs = g_AudioEngine.GetSoundManager().GetTimeInMilliseconds(0);
		const bool isRaining = g_weather.IsRaining();

		const char* zoneName = audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(m_ZoneData->NameTableOffset);
		Color32 textColor = Color32(0,255,0);
		grcDebugDraw::Text(Vector2(0.45f, activeRuleYOffset), textColor, zoneName);
		activeRuleYOffset += 0.02f;

		// Render all the rules that are currently active
		for(u32 loop = 0; loop < m_Rules.GetCount(); loop++)
		{
			textColor = Color32(255,255,255);

			if(m_Rules[loop])
			{
				bool isValid = IsValidTime( m_Rules[loop]->MinTimeMinutes, m_Rules[loop]->MaxTimeMinutes, gameTimeMinutes );

				if((AUD_GET_TRISTATE_VALUE(m_Rules[loop]->Flags, FLAG_ID_AMBIENTRULE_STOPWHENRAINING)==AUD_TRISTATE_TRUE && isRaining))
				{
					isValid = false;
				}

				if(!AreAllConditionsValid(m_Rules[loop]))
				{
					isValid = false;
				}

				if(AUD_GET_TRISTATE_VALUE(m_Rules[loop]->Flags, FLAG_ID_AMBIENTRULE_STREAMINGSOUND) == AUD_TRISTATE_TRUE)
				{
					if(!g_AmbientAudioEntity.IsRuleStreamingAllowed())
					{
						isValid = false;
					}
				}

				// Mark invalid sounds in red
				if(!isValid)
				{
					textColor = Color32(255,0,0);
				}

				const char* name = audNorthAudioEngine::GetMetadataManager().GetObjectNameFromNameTableOffset(m_Rules[loop]->NameTableOffset);
				grcDebugDraw::Text(Vector2(0.47f, activeRuleYOffset), textColor, name );

				const f32 mintimeHours = Floorf(m_Rules[loop]->MinTimeMinutes/60.0f);
				const f32 minTimeMinutesFraction = (m_Rules[loop]->MinTimeMinutes - (60.0f * mintimeHours))/60.0f;

				const f32 maxtimeHours = Floorf(m_Rules[loop]->MaxTimeMinutes/60.0f);
				const f32 maxTimeMinutesFraction = (m_Rules[loop]->MaxTimeMinutes - (60.0f * maxtimeHours))/60.0f;

				char tempString[16];
				sprintf(tempString, "%.02f", mintimeHours + minTimeMinutesFraction);
				grcDebugDraw::Text(Vector2(0.7f, activeRuleYOffset), textColor, tempString );

				sprintf(tempString, "%.02f", maxtimeHours + maxTimeMinutesFraction);
				grcDebugDraw::Text(Vector2(0.76f, activeRuleYOffset), textColor, tempString );

				u32 nextPlayTime = (m_Rules[loop]->LastPlayTime + m_Rules[loop]->MinRepeatTime*1000);
				u32 delay = 0;

				if(timeInMs < nextPlayTime)
				{
					delay = nextPlayTime - timeInMs;
				}

				sprintf(tempString, "%.02f", ((f32)delay)/1000.0f);
				grcDebugDraw::Text(Vector2(0.83f, activeRuleYOffset), textColor, tempString );

				if(AUD_GET_TRISTATE_VALUE(m_Rules[loop]->Flags, FLAG_ID_AMBIENTRULE_STREAMINGSOUND) == AUD_TRISTATE_TRUE)
				{
					if(!g_AmbientAudioEntity.IsRuleStreamingAllowed())
					{
						sprintf(tempString, "Too Fast");
					}
					else
					{
						sprintf(tempString, "Streaming");
					}

					grcDebugDraw::Text(Vector2(0.89f, activeRuleYOffset), textColor, tempString );
				}
				else if(m_Rules[loop]->DynamicBankID == -1)
				{
					sprintf(tempString, "Uninitialized");
					grcDebugDraw::Text(Vector2(0.89f, activeRuleYOffset), textColor, tempString );
				}
				else if(m_Rules[loop]->DynamicBankID == AUD_INVALID_BANK_ID)
				{
					sprintf(tempString, "Static");
					grcDebugDraw::Text(Vector2(0.89f, activeRuleYOffset), textColor, tempString );
				}
				else
				{
					if(m_WaveSlots[loop] &&
						m_WaveSlots[loop]->IsLoaded())
					{
						sprintf(tempString, "Loaded");
						grcDebugDraw::Text(Vector2(0.89f, activeRuleYOffset), textColor, tempString );
					}
					else
					{
						const Vec3V listenerPos = g_AudioEngine.GetEnvironment().GetPanningListenerPosition(0);
						const Vec3V closestPoint = ComputeClosestPointToPosition(listenerPos);
						const f32 dist2 = MagSquared(listenerPos - closestPoint).Getf();

						bool isValid = IsValidTime( m_Rules[loop]->MinTimeMinutes, m_Rules[loop]->MaxTimeMinutes, gameTimeMinutes );

						if(AUD_GET_TRISTATE_VALUE(m_Rules[loop]->Flags, FLAG_ID_AMBIENTRULE_STOPWHENRAINING)==AUD_TRISTATE_TRUE && isRaining)
						{
							isValid = false;
						}

						// Try to be a bit more specific about why this rule may not have loaded
						if(isValid && dist2 > (m_Rules[loop]->MaxDist * m_Rules[loop]->MaxDist))
						{
							sprintf(tempString, "Too Far");
							grcDebugDraw::Text(Vector2(0.89f, activeRuleYOffset), textColor, tempString );
						}
						else if(!g_AmbientAudioEntity.IsRuleStreamingAllowed())
						{
							sprintf(tempString, "Too Fast");
							grcDebugDraw::Text(Vector2(0.89f, activeRuleYOffset), textColor, tempString );
						}
						else
						{
							sprintf(tempString, "Not Loaded");
							grcDebugDraw::Text(Vector2(0.89f, activeRuleYOffset), textColor, tempString );
						}
					}
				}
			}

			activeRuleYOffset += 0.02f;
		}
	}
}
#endif
