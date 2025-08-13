//  
// audio/auddirectionalambience.cpp
//  
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#include "auddirectionalambience.h"
#include "audio\waveslotmanager.h"
#include "ambientaudioentity.h"
#include "audambientzone.h"
#include "audio\northaudioengine.h"
#include "audiohardware\driver.h"
#include "audioengine\engine.h"
#include "game\Clock.h"
#include "scene\world\GameWorldHeightMap.h"
#include "game\weather.h"
#include "water\audshorelineOcean.h"

AUDIO_AMBIENCE_OPTIMISATIONS()

BANK_ONLY(extern bool g_DrawDirectionalManagers;)
BANK_ONLY(extern bool g_DrawDirectionalManagerFinalSpeakerLevels;)
BANK_ONLY(extern bool g_ShowDirectionalAmbienceCalculations;)
BANK_ONLY(extern s32 g_DirectionalManagerRenderDir;)
BANK_ONLY(extern bool g_DirectionalManagerForceVol;)
BANK_ONLY(extern f32 g_DirectionalManagerForcedVol;)
BANK_ONLY(extern bool g_DirectionalManagerIgnoreBlockage;)
BANK_ONLY(bool g_DirectionalAmbienceChannelLeakageEnabled = true;)

extern CWeather g_weather;

// ----------------------------------------------------------------
// Directional ambience manager constructor
// ----------------------------------------------------------------
audDirectionalAmbienceManager::audDirectionalAmbienceManager() :
	m_useSinglePannedSound(false)
{
	for(u32 i = 0 ; i < kNumDirections; i++)
	{
		m_directionalSound[i] = NULL;
		m_DynamicWaveSlots[i] = NULL;
		m_dynamicBankIDs[i] = AUD_INVALID_BANK_ID;
	}

	m_muted = false;
	m_wasMuted = true;

	m_CurrentUpdateDirection = 0;
	for(u32 loop = 0; loop < AUD_AMB_DIR_MAX; loop++)
	{
		m_CachedSectorVols[loop] = 0.0f;
	}

#if __BANK
	m_debugDrawThisManager = false;
	m_useForcedVolumes = false;

	for(u32 i = 0 ; i < kNumDirections; i++)
	{
		m_forcedVolumes[i] = 0.0f;
		m_finalVolumes[i] = 0.0f;
	}
#endif
}

// ----------------------------------------------------------------
// Directional ambience manager init
// ----------------------------------------------------------------
void audDirectionalAmbienceManager::Init(DirectionalAmbience* ambience)
{
	m_directionalAmbience = ambience;

	if(m_directionalAmbience)
	{
		m_timeToVolumeCurve.Init(m_directionalAmbience->TimeToVolume);
		m_occlusionToVolCurve.Init(m_directionalAmbience->OcclusionToVol);
		m_occlusionToCutoffCurve.Init(m_directionalAmbience->OcclusionToCutOff);
		m_heightToCutOffCurve.Init(m_directionalAmbience->HeightToCutOff);
		m_heightAboveBlanketToVolCurve.Init(m_directionalAmbience->HeightAboveBlanketToVol);

		InitAmbienceCurve(&m_builtUpFactorToVolCurve, m_directionalAmbience->BuiltUpFactorToVol, ATSTRINGHASH("CONSTANT_ONE",0xCDE3CE03));
		InitAmbienceCurve(&m_buildingDensityToVolCurve, m_directionalAmbience->BuildingDensityToVol, ATSTRINGHASH("CONSTANT_ONE",0xCDE3CE03));
		InitAmbienceCurve(&m_treeDensityToVolCurve, m_directionalAmbience->TreeDensityToVol, ATSTRINGHASH("CONSTANT_ONE",0xCDE3CE03));
		InitAmbienceCurve(&m_waterFactorToVolCurve, m_directionalAmbience->WaterFactorToVol, ATSTRINGHASH("CONSTANT_ONE",0xCDE3CE03));
		InitAmbienceCurve(&m_highwayFactorToVolCurve, m_directionalAmbience->HighwayFactorToVol, ATSTRINGHASH("CONSTANT_ONE",0xCDE3CE03));
		InitAmbienceCurve(&m_vehicleDensityToVolCurve, m_directionalAmbience->VehicleCountToVol, ATSTRINGHASH("CONSTANT_ONE",0xCDE3CE03));

		m_parentZoneSmoother.Init(0.005f);
		m_OutToSeaVolumeSmoother.Init(0.00005f);

		for(u32 i = 0 ; i < kNumDirections; i++)
		{
			m_directionalSoundSmoother[i].Init(m_directionalAmbience->VolumeSmoothing);
		}

		if(AUD_GET_TRISTATE_VALUE(ambience->Flags, FLAG_ID_DIRECTIONALAMBIENCE_STOPWHENRAINING)==AUD_TRISTATE_TRUE)
		{
			m_stopWhenRaining = true;
		}
		else
		{
			m_stopWhenRaining = false;
		}

		if(AUD_GET_TRISTATE_VALUE(ambience->Flags, FLAG_ID_DIRECTIONALAMBIENCE_MONOPANNEDSOURCE)==AUD_TRISTATE_TRUE)
		{
			m_useSinglePannedSound = true;
			m_directionalSoundHashes[0] = m_directionalAmbience->SoundNorth;
		}
		else
		{
			m_useSinglePannedSound = false;
			m_directionalSoundHashes[AUD_AMB_DIR_NORTH] = m_directionalAmbience->SoundNorth;
			m_directionalSoundHashes[AUD_AMB_DIR_EAST] = m_directionalAmbience->SoundEast;
			m_directionalSoundHashes[AUD_AMB_DIR_SOUTH] = m_directionalAmbience->SoundSouth;
			m_directionalSoundHashes[AUD_AMB_DIR_WEST] = m_directionalAmbience->SoundWest;
		}
	}	
}

// ----------------------------------------------------------------
// Stop playing
// ----------------------------------------------------------------
void audDirectionalAmbienceManager::InitAmbienceCurve(audCurve* curve, u32 hash, u32 fallback)
{
	if(curve)
	{
		if(!curve->Init(hash))
		{
			curve->Init(fallback);
		}
	}
}

// ----------------------------------------------------------------
// Stop playing
// ----------------------------------------------------------------
void audDirectionalAmbienceManager::Stop()
{
	for(u32 loop = 0; loop < kNumDirections; loop++)
	{
		g_AmbientAudioEntity.StopAndForgetSounds(m_directionalSound[loop]);
		audAmbientZone::sm_WaveSlotManager.FreeWaveSlot(m_DynamicWaveSlots[loop]);

#if __BANK
		m_finalVolumes[loop] = 0.0f;
#endif
	}
}

#if __BANK
// ----------------------------------------------------------------
// Directional ambience manager debug draw
// ----------------------------------------------------------------
void audDirectionalAmbienceManager::DebugDraw()
{
	static const char *meterNames[] = {"North", "East", "South", "West"};
	static const char *speakerNames[] = {"FL", "FR", "RL", "RR"};
	static audMeterList meterList;
	static f32 meterValues[4];

	meterValues[0] = m_finalVolumes[(u32)AUD_AMB_DIR_NORTH];
	meterValues[1] = m_finalVolumes[(u32)AUD_AMB_DIR_EAST];
	meterValues[2] = m_finalVolumes[(u32)AUD_AMB_DIR_SOUTH];
	meterValues[3] = m_finalVolumes[(u32)AUD_AMB_DIR_WEST];

	meterList.left = 790.f;
	meterList.bottom = 620.f;
	meterList.width = 400.f;
	meterList.height = 200.f;
	meterList.drawValues = true;
	meterList.title = g_DrawDirectionalManagerFinalSpeakerLevels? "Speaker Levels" : "Output Volumes"; 
	meterList.values = &meterValues[0];
	meterList.names = g_DrawDirectionalManagerFinalSpeakerLevels? speakerNames : meterNames;
	meterList.numValues = sizeof(meterNames)/sizeof(meterNames[0]);
	audNorthAudioEngine::DrawLevelMeters(&meterList);

	char tempString[64];
	f32 heightAboveBlanket = MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix()).d.z - CGameWorldHeightMap::GetMinHeightFromWorldHeightMap(MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix()).d.x, MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix()).d.y);
	sprintf(tempString, "Height Above Blanket: %.02f", heightAboveBlanket);
	grcDebugDraw::Text(Vector2(0.2f, 0.1f), Color32(255,255,255), tempString);

	f32 distanceOutToSea = 0.0f;

	if(audShoreLineOcean::IsListenerOverOcean())
	{
		distanceOutToSea = audShoreLineOcean::GetSqdDistanceIntoWater();
	}

	sprintf(tempString, "Distance Out To Sea: %.02f", SqrtfSafe(distanceOutToSea));
	grcDebugDraw::Text(Vector2(0.2f, 0.12f), Color32(255,255,255), tempString);
	
	sprintf(tempString, "Parent Zones");
	f32 directionalAmbienceRenderPos = 0.15f;
	grcDebugDraw::Text(Vector2(0.37f, directionalAmbienceRenderPos), Color32(255,255,255), tempString);
	directionalAmbienceRenderPos += 0.05f;

	for(u32 loop = 0; loop < m_ParentZones.GetCount(); loop++)
	{
		sprintf(tempString, "%s", m_ParentZones[loop].ambientZone->GetName());

		if(m_ParentZones[loop].ambientZone->IsActive())
		{
			grcDebugDraw::Text(Vector2(0.37f, directionalAmbienceRenderPos), Color32(0,255,0), tempString);
		}
		else
		{
			grcDebugDraw::Text(Vector2(0.37f, directionalAmbienceRenderPos), Color32(255,0,0), tempString);
		}
		
		directionalAmbienceRenderPos += 0.02f;
	}

	m_debugDrawThisManager = true;
}

// ----------------------------------------------------------------
// Directional ambience manager debug draw
// ----------------------------------------------------------------
void audDirectionalAmbienceManager::DebugDrawInputVariables(audAmbienceDirection direction)
{
	static const char *meterNames[] = {"Built up", "Buildings", "Trees", "Water", "Highway", "Vehicles"};
	static audMeterList meterList;
	static f32 meterValues[6];

	meterValues[0] = audNorthAudioEngine::GetGtaEnvironment()->GetBuiltUpFactorForDirection(direction);
	meterValues[1] = audNorthAudioEngine::GetGtaEnvironment()->GetBuildingDensityForDirection(direction);
	meterValues[2] = audNorthAudioEngine::GetGtaEnvironment()->GetTreeDensityForDirection(direction);
	meterValues[3] = audNorthAudioEngine::GetGtaEnvironment()->GetWaterFactorForDirection(direction);
	meterValues[4] = audNorthAudioEngine::GetGtaEnvironment()->GetHighwayFactorForDirection(direction);
	meterValues[5] = audNorthAudioEngine::GetGtaEnvironment()->GetVehicleDensityForDirection(direction);

	meterList.left = 740.f;
	meterList.bottom = 280.f;
	meterList.width = 450.f;
	meterList.height = 180.f;
	meterList.values = &meterValues[0];
	meterList.names = meterNames;

	if(direction == AUD_AMB_DIR_LOCAL)
	{
		meterList.title = "Directional Metrics (Local)";
	}
	else if(direction == AUD_AMB_DIR_NORTH)
	{
		meterList.title = "Directional Metrics (North)";
	}
	else if(direction == AUD_AMB_DIR_SOUTH)
	{
		meterList.title = "Directional Metrics (South)";
	}
	else if(direction == AUD_AMB_DIR_EAST)
	{
		meterList.title = "Directional Metrics (East)";
	}
	else if(direction == AUD_AMB_DIR_WEST)
	{
		meterList.title = "Directional Metrics (West)";
	}
	
	meterList.numValues = sizeof(meterNames)/sizeof(meterNames[0]);
	audNorthAudioEngine::DrawLevelMeters(&meterList);
}
#endif

// ----------------------------------------------------------------
// Register a zone as owning this ambience
// ----------------------------------------------------------------
void audDirectionalAmbienceManager::RegisterParentZone(audAmbientZone* ambientZone, f32 volumeScale)
{
	// Check if we've already registered this zone
	for(u32 loop = 0; loop < m_ParentZones.GetCount(); loop++)
	{
		if(m_ParentZones[loop].ambientZone == ambientZone)
		{
			return;
		}
	}

	DirectionalAmbienceParentZone parentZone;
	parentZone.ambientZone = ambientZone;
	parentZone.volumeScale = volumeScale;
	m_ParentZones.PushAndGrow(parentZone);
}

// ----------------------------------------------------------------
// Check if any parent zones are still active
// ----------------------------------------------------------------
bool audDirectionalAmbienceManager::AreAnyParentZonesActive()
{
	for(u32 loop = 0; loop < m_ParentZones.GetCount(); loop++)
	{
		if(m_ParentZones[loop].ambientZone->IsActive())
		{
			return true;
		}
	}

	return false;
}

// ----------------------------------------------------------------
// Directional ambience manager update
// ----------------------------------------------------------------
void audDirectionalAmbienceManager::Update(const float timeStepS, const s32 basePan, const s32* directionalPans, const f32* perDirectionChannelVolumes, const f32 heightAboveBlanket)
{
	if(m_directionalAmbience)
	{
		UpdateInternal();
		ScalarV maxParentZoneVolume = ScalarV(V_ZERO);
		const Vec3V listenerPos = g_AudioEngine.GetEnvironment().GetPanningListenerMatrix().d();

		for(u32 loop = 0; loop < m_ParentZones.GetCount(); loop++)
		{
			if(m_ParentZones[loop].ambientZone->IsActive())
			{
				ScalarV thisZoneVolumeScale = ScalarV(m_ParentZones[loop].volumeScale);

				// Zone is active but listener position isn't within the positioning zone. Listener is therefore 
				// somewhere between the activation zone and the positioning zone boundaries, so scale up the volume as we approach the positioning zone
				if(!m_ParentZones[loop].ambientZone->IsPointInPositioningZone(listenerPos))
				{
					if(m_ParentZones[loop].ambientZone->IsSphere())
					{
						const Vec3V closestPointOnPositioningZone = m_ParentZones[loop].ambientZone->ComputeClosestPointToPosition(listenerPos);
						const Vec3V closestPointOnActivationZone = m_ParentZones[loop].ambientZone->ComputeClosestPointOnActivationZoneBoundary(listenerPos);
						const ScalarV totalDist = Dist(closestPointOnPositioningZone, closestPointOnActivationZone);
						const ScalarV distanceVolume = SelectFT(IsGreaterThan(totalDist, ScalarV(V_ZERO)), ScalarV(V_ONE), ScalarV(V_ONE) - Clamp((Dist(closestPointOnPositioningZone, listenerPos)/totalDist), ScalarV(V_ZERO), ScalarV(V_ONE)));
						thisZoneVolumeScale *= distanceVolume;
					}
					else
					{
						// We only care about 2D distance from the zone - height is taken car of by the height-volume curve
						if(!m_ParentZones[loop].ambientZone->IsPointInPositioningZoneFlat(listenerPos))
						{
							Vec3V closestPointOnPositioningZone = m_ParentZones[loop].ambientZone->ComputeClosestPointToPosition(listenerPos);
							closestPointOnPositioningZone.SetZ(listenerPos.GetZ());

							const Vec3V positioningToListener = NormalizeSafe(listenerPos - closestPointOnPositioningZone, Vec3V(V_ZERO));

							Vec3V closestPointOnActivationZone = m_ParentZones[loop].ambientZone->ComputeClosestPointOnActivationZoneBoundary(closestPointOnPositioningZone + (positioningToListener * ScalarV(10000.0f)));
							closestPointOnActivationZone.SetZ(listenerPos.GetZ());

							const ScalarV totalDist = Dist(closestPointOnPositioningZone, closestPointOnActivationZone);
							const ScalarV distanceVolume = SelectFT(IsGreaterThan(totalDist, ScalarV(V_ZERO)), ScalarV(V_ONE), ScalarV(V_ONE) - Clamp(Dist(closestPointOnPositioningZone, listenerPos)/totalDist, ScalarV(V_ZERO), ScalarV(V_ONE)));

							thisZoneVolumeScale *= distanceVolume;
						}
					}
				}

				maxParentZoneVolume = Max(maxParentZoneVolume, thisZoneVolumeScale);
			}
		}

		f32 sectorVols[5];
        f32 parentZoneVolume = m_parentZoneSmoother.CalculateValue(maxParentZoneVolume.Getf());

		const f32 time = (f32)CClock::GetHour() + (CClock::GetMinute()/60.f);
		const f32 timeBasedVolume = m_timeToVolumeCurve.CalculateValue(time);
		const f32 heightBasedVolume = m_heightAboveBlanketToVolCurve.CalculateValue(heightAboveBlanket);

		f32 oceanVolume = 1.0f;

		if(m_directionalAmbience->MaxDistanceOutToSea > 0.0f)
		{
			f32 desiredVolume = 1.0f;
			
			if(audShoreLineOcean::IsListenerOverOcean())
			{
				f32 distanceOutToSea = audShoreLineOcean::GetSqdDistanceIntoWater();
				desiredVolume = 1.0f - Min(SqrtfSafe(distanceOutToSea)/m_directionalAmbience->MaxDistanceOutToSea, 1.0f);
			}

			oceanVolume = m_OutToSeaVolumeSmoother.CalculateValue(desiredVolume, fwTimer::GetTimeStep());
		}

		const f32 heightTimeParentVolumeScale = heightBasedVolume * timeBasedVolume * oceanVolume * parentZoneVolume;
		const f32 outsideWorldOcclusion = audNorthAudioEngine::GetOcclusionManager()->GetOutsideWorldOcclusionAfterPortals();
		const f32 outsideWorldOcclusionAttenuation = m_occlusionToVolCurve.CalculateValue(outsideWorldOcclusion);
		const f32 outsideWorldOcclusionCutOff = m_occlusionToCutoffCurve.CalculateValue(outsideWorldOcclusion);
		const f32 heightCutoff = m_heightToCutOffCurve.CalculateValue(MAT34V_TO_MATRIX34(g_AudioEngine.GetEnvironment().GetPanningListenerMatrix()).d.z);
		const f32 finalCutOff = Min(outsideWorldOcclusionCutOff, heightCutoff);

		if(!m_muted && heightTimeParentVolumeScale > 0.0f)
		{
			for(u32 loop = 0; loop < 5; loop++)
			{
#if __BANK
				if(g_DirectionalManagerForceVol)
				{
					sectorVols[loop] = g_DirectionalManagerForcedVol;
					continue;
				}
#endif

				sectorVols[loop] = GetVolumeForDirection((audAmbienceDirection)loop);
				sectorVols[loop] = Clamp(sectorVols[loop], 0.0f, 1.0f);

				if(loop < AUD_AMB_DIR_LOCAL)
				{
					sectorVols[loop] *= m_occlusionToVolCurve.CalculateValue(audNorthAudioEngine::GetOcclusionManager()->GetExteriorOcclusionForDirection((audAmbienceDirection)loop));
				}
				else
				{
					sectorVols[loop] *= outsideWorldOcclusionAttenuation;
				}

				sectorVols[loop] *= m_directionalAmbience->InstanceVolumeScale;
				sectorVols[loop] *= heightTimeParentVolumeScale;

#if __BANK
				if(m_debugDrawThisManager && g_ShowDirectionalAmbienceCalculations && loop == (u32)g_DirectionalManagerRenderDir)
				{
					f32 yCoord = 0.62f;
					char tempString[128];
					formatf(tempString, "*");
					grcDebugDraw::Text(Vector2(0.15f, yCoord), Color32(255,255,255), tempString);
					yCoord += 0.02f;

					formatf(tempString, "Time Based Volume (%.02f) *", timeBasedVolume);
					grcDebugDraw::Text(Vector2(0.15f, yCoord), Color32(255,255,255), tempString);
					yCoord += 0.02f;

					formatf(tempString, "Height Based Volume (%.02f) *", heightBasedVolume);
					grcDebugDraw::Text(Vector2(0.15f, yCoord), Color32(255,255,255), tempString);
					yCoord += 0.02f;

					formatf(tempString, "Ocean Volume (%.02f) *", oceanVolume);
					grcDebugDraw::Text(Vector2(0.15f, yCoord), Color32(255,255,255), tempString);
					yCoord += 0.02f;

					formatf(tempString, "Outside World Occlusion Attenuation (%.02f) *", outsideWorldOcclusionAttenuation);
					grcDebugDraw::Text(Vector2(0.15f, yCoord), Color32(255,255,255), tempString);
					yCoord += 0.02f;

					formatf(tempString, "DA Instance Volume Scale (%.02f) *", m_directionalAmbience->InstanceVolumeScale);
					grcDebugDraw::Text(Vector2(0.15f, yCoord), Color32(255,255,255), tempString);
					yCoord += 0.02f;

					formatf(tempString, "Parent Zone Volume (%.02f)", parentZoneVolume);
					grcDebugDraw::Text(Vector2(0.15f, yCoord), Color32(255,255,255), tempString);
				}
#endif
			}

			m_CurrentUpdateDirection++;

			if(m_CurrentUpdateDirection > AUD_AMB_DIR_LOCAL)
			{
				m_CurrentUpdateDirection = 0;
			}

			m_wasMuted = false;
		}
		else
		{
			m_wasMuted = true;
			m_CurrentUpdateDirection = 0;
			sysMemSet(&m_CachedSectorVols[0], 0, sizeof(m_CachedSectorVols));
			sysMemSet(&sectorVols[0], 0, sizeof(sectorVols));
		}

		// Single panned sound case - instead of playing 4 distinct sounds panned to each direction, we play a single sound and twiddle
		// the individual speaker volumes to simulate the same effect
		if(m_useSinglePannedSound)
		{
			ALIGNAS(16) f32 volumes[4] ;
			sysMemSet(&volumes[0], 0, sizeof(volumes));

			f32 maxVolume = 0.0f;

			for(u32 direction = 0; direction < kNumDirections; direction++)
			{
				f32 blockedVolScale = audNorthAudioEngine::GetOcclusionManager()->GetBlockedLinearVolumeForDirection((audAmbienceDirection)direction);

#if __BANK
				if(g_DirectionalManagerIgnoreBlockage)
				{
					blockedVolScale = 1.0f;
				}
#endif

				f32 finalSmoothedVolume = m_directionalSoundSmoother[direction].CalculateValue(Min(1.f, (sectorVols[direction] + sectorVols[AUD_AMB_DIR_LOCAL])*blockedVolScale), timeStepS);

#if __BANK
				if(m_useForcedVolumes)
				{
					finalSmoothedVolume = m_forcedVolumes[direction];
				}

				// Remember this so we can debug graph it
				m_finalVolumes[direction] = finalSmoothedVolume;
#endif

				maxVolume = Max(maxVolume, finalSmoothedVolume);

				if(finalSmoothedVolume > 0.0f)
				{
					f32 channelVolume[4];

					for(u32 speakerIndex=0; speakerIndex<4; speakerIndex++)
					{
						channelVolume[speakerIndex] = perDirectionChannelVolumes[(direction * 4) + speakerIndex];
					}

					// If we're using a single panned sound, then we want to ensure that the overall volume emitted by all speakers is equal to our 
					// desired level, and just alter what proportion is coming from each speaker. Therefore work out our channel contribution as
					// fraction of the summed channel contribution and use this as a per-speaker scalar.
					f32 totalChannelVolume = 0.0f;
					for(u32 loop = 0; loop < kNumDirections; loop++)
					{
						totalChannelVolume += channelVolume[loop];
					}

					if(totalChannelVolume > 0.0f)
					{
						f32 invChannelVolume = 1.0f/totalChannelVolume;
						volumes[RAGE_SPEAKER_ID_FRONT_LEFT] += finalSmoothedVolume * (channelVolume[RAGE_SPEAKER_ID_FRONT_LEFT] * invChannelVolume);
						volumes[RAGE_SPEAKER_ID_FRONT_RIGHT] += finalSmoothedVolume * (channelVolume[RAGE_SPEAKER_ID_FRONT_RIGHT] * invChannelVolume);
						volumes[RAGE_SPEAKER_ID_BACK_LEFT] += finalSmoothedVolume * (channelVolume[RAGE_SPEAKER_ID_BACK_LEFT] * invChannelVolume);
						volumes[RAGE_SPEAKER_ID_BACK_RIGHT] += finalSmoothedVolume * (channelVolume[RAGE_SPEAKER_ID_BACK_RIGHT] * invChannelVolume);
					}
				}
			}

			if(!m_directionalSound[0])
			{
				if(maxVolume > 0.0f)
				{
					if(m_DynamicWaveSlots[0] == NULL &&
						m_dynamicBankIDs[0] != AUD_INVALID_BANK_ID)
					{
						m_DynamicWaveSlots[0] = audAmbientZone::sm_WaveSlotManager.LoadBank(m_dynamicBankIDs[0], m_dynamicSlotTypes[0], true);
					}

					if(m_DynamicWaveSlots[0] ||
						m_dynamicBankIDs[0] == AUD_INVALID_BANK_ID)
					{
						audSoundInitParams initParams;
						initParams.RemoveHierarchy = false;
						g_AmbientAudioEntity.CreateAndPlaySound_Persistent(m_directionalSoundHashes[0], &m_directionalSound[0], &initParams);
					}
				}
			}
			else if(maxVolume <= 0.0f)
			{
				g_AmbientAudioEntity.StopAndForgetSounds(m_directionalSound[0]);
				audAmbientZone::sm_WaveSlotManager.FreeWaveSlot(m_DynamicWaveSlots[0]);
			}
			else
			{				
				m_directionalSound[0]->SetRequestedVolume(0.0f);
				m_directionalSound[0]->SetRequestedQuadSpeakerLevels(volumes);
				m_directionalSound[0]->SetRequestedLPFCutoff(finalCutOff);

#if __BANK
				if(g_DrawDirectionalManagerFinalSpeakerLevels)
				{
					for(int loop = 0; loop < kNumDirections; loop++)
					{
						m_finalVolumes[loop] = volumes[loop];
					}
				}
#endif
			}
		}
		// Standard 4-panned sound case. Just one sound for each direction, then panned as appropriate
		else
		{
			for(u32 loop = 0; loop < kNumDirections; loop++)
			{
				f32 blockedVolScale = audNorthAudioEngine::GetOcclusionManager()->GetBlockedLinearVolumeForDirection((audAmbienceDirection)loop);
				const s32 pan = (basePan + directionalPans[loop]) % 360;
				const f32 finalSmoothedVolume = m_directionalSoundSmoother[loop].CalculateValue(Min(1.f, (sectorVols[loop] + sectorVols[AUD_AMB_DIR_LOCAL])*blockedVolScale), timeStepS);
				const f32 finalVolume = audDriverUtil::ComputeDbVolumeFromLinear(finalSmoothedVolume);
				const f32 directionalOutsideWorldOcclusion = audNorthAudioEngine::GetOcclusionManager()->GetExteriorOcclusionForDirection((audAmbienceDirection)loop);
				const f32 directionalOcclusionCutOff = m_occlusionToCutoffCurve.CalculateValue(directionalOutsideWorldOcclusion);
				const f32 directionalCutOff = Min(directionalOcclusionCutOff, heightCutoff);

#if __BANK
				// Remember this so we can debug graph it
				m_finalVolumes[loop] = finalSmoothedVolume;
#endif

				if(!m_directionalSound[loop] && finalSmoothedVolume > 0.0f)
				{
					if(m_DynamicWaveSlots[loop] == NULL &&
						m_dynamicBankIDs[loop] != AUD_INVALID_BANK_ID)
					{
						m_DynamicWaveSlots[loop] = audAmbientZone::sm_WaveSlotManager.LoadBank(m_dynamicBankIDs[loop], m_dynamicSlotTypes[loop], true);
					}

					if(m_DynamicWaveSlots[loop] ||
						m_dynamicBankIDs[loop] == AUD_INVALID_BANK_ID)
					{
						audSoundInitParams initParams;
						initParams.RemoveHierarchy = false;
						g_AmbientAudioEntity.CreateAndPlaySound_Persistent(m_directionalSoundHashes[loop], &m_directionalSound[loop], &initParams);
					}
				}

				if(m_directionalSound[loop])
				{
					// Sound no longer playing? Stop it and free up the wave slot
					if(finalSmoothedVolume <= 0.0f)
					{
						g_AmbientAudioEntity.StopAndForgetSounds(m_directionalSound[loop]);
						audAmbientZone::sm_WaveSlotManager.FreeWaveSlot(m_DynamicWaveSlots[loop]);
					}
					else
					{
						//m_directionalSound[loop]->SetRequestedPosition(g_AudioEngine.GetEnvironment().GetPanningListenerPosition());
						m_directionalSound[loop]->SetRequestedVolume(finalVolume);
						m_directionalSound[loop]->SetRequestedLPFCutoff(directionalCutOff);
						m_directionalSound[loop]->SetRequestedPan(pan);
					}
				}
			}
		}
	}

#if __BANK
	m_debugDrawThisManager = false;
#endif
}

// ----------------------------------------------------------------
// Get the volume for the given direction
// ----------------------------------------------------------------
f32 audDirectionalAmbienceManager::GetVolumeForDirection(audAmbienceDirection direction)
{
	const bool isRaining = g_weather.IsRaining();
	BANK_ONLY(bool directionRenderingEnabled = (m_debugDrawThisManager && g_ShowDirectionalAmbienceCalculations && direction == g_DirectionalManagerRenderDir);)
	f32 calculatedVolume = 0.0f;

	if(isRaining && m_stopWhenRaining)
	{
		calculatedVolume = 0.0f;
	}
	else if(m_CurrentUpdateDirection == direction || m_wasMuted BANK_ONLY(|| directionRenderingEnabled))
	{
		f32 builtUpVol = m_builtUpFactorToVolCurve.CalculateValue(audNorthAudioEngine::GetGtaEnvironment()->GetBuiltUpFactorForDirection(direction));
		f32 buildingDensityVol = m_buildingDensityToVolCurve.CalculateValue(audNorthAudioEngine::GetGtaEnvironment()->GetBuildingDensityForDirection(direction));
		f32 treeDensityVol = m_treeDensityToVolCurve.CalculateValue(audNorthAudioEngine::GetGtaEnvironment()->GetTreeDensityForDirection(direction));
		f32 waterFactorVol = m_waterFactorToVolCurve.CalculateValue(audNorthAudioEngine::GetGtaEnvironment()->GetWaterFactorForDirection(direction));
		f32 highwayFactorVol = m_highwayFactorToVolCurve.CalculateValue(audNorthAudioEngine::GetGtaEnvironment()->GetHighwayFactorForDirection(direction));
		f32 vehicleDensityVol = m_vehicleDensityToVolCurve.CalculateValue(audNorthAudioEngine::GetGtaEnvironment()->GetVehicleDensityForDirection(direction));

#if __BANK
		if(directionRenderingEnabled)
		{
			char tempString[128];
			formatf(tempString, "Clamp(BuiltUp Factor Vol (%.02f) + ", builtUpVol);
			grcDebugDraw::Text(Vector2(0.15f, 0.5f), Color32(255,255,255), tempString);

			formatf(tempString, "Building Density Vol (%.02f) + ", buildingDensityVol);
			grcDebugDraw::Text(Vector2(0.15f, 0.52f), Color32(255,255,255), tempString);

			formatf(tempString, "Tree Density Vol (%.02f) + ", treeDensityVol);
			grcDebugDraw::Text(Vector2(0.15f, 0.54f), Color32(255,255,255), tempString);

			formatf(tempString, "Water Factor Vol (%.02f) + ", waterFactorVol);
			grcDebugDraw::Text(Vector2(0.15f, 0.56f), Color32(255,255,255), tempString);

			formatf(tempString, "Highway Factor Vol (%.02f) + ", highwayFactorVol);
			grcDebugDraw::Text(Vector2(0.15f, 0.58f), Color32(255,255,255), tempString);

			formatf(tempString, "Vehicle Density Vol (%.02f), 0.0, 1.0)", vehicleDensityVol);
			grcDebugDraw::Text(Vector2(0.15f, 0.60f), Color32(255,255,255), tempString);
		}
#endif

		calculatedVolume = ( builtUpVol + buildingDensityVol + treeDensityVol + waterFactorVol + highwayFactorVol + vehicleDensityVol);
	}
	else
	{
		calculatedVolume = m_CachedSectorVols[direction];
	}

	m_CachedSectorVols[direction] = calculatedVolume;
	return calculatedVolume;
}
