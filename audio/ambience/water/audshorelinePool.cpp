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
#include "audshorelinePool.h"
#include "audio/northaudioengine.h"
#include "audio/gameobjects.h"
#include "debug/vectormap.h"
#include "renderer/water.h"
#include "game/weather.h"
#include "vfx/VfxHelper.h"
#include "audio/weatheraudioentity.h"

AUDIO_AMBIENCE_OPTIMISATIONS()

audCurve audShoreLinePool::sm_WindStrengthToTimeFrequency;
audCurve audShoreLinePool::sm_WindStrengthToDistance;
audCurve audShoreLinePool::sm_WindStrengthToSpeed;
audSoundSet audShoreLinePool::sm_PoolWaterSounds;
u8 audShoreLinePool::sm_NumExtraPoolPointsToUpdate = 3;

BANK_ONLY(s32 audShoreLinePool::sm_PoolWaterLappingDelayMin = -500;);
BANK_ONLY(s32 audShoreLinePool::sm_PoolWaterLappingDelayMax = 500;);
BANK_ONLY(s32 audShoreLinePool::sm_PoolWaterSplashDelayMin = 5000;);
BANK_ONLY(s32 audShoreLinePool::sm_PoolWaterSplashDelayMax = 14000;);
BANK_ONLY(bool audShoreLinePool::sm_ShowIndices = false;);
// ----------------------------------------------------------------
// Initialise static member variables
// ----------------------------------------------------------------
void audShoreLinePool::InitClass()
{
	sm_WindStrengthToTimeFrequency.Init(ATSTRINGHASH("WIND_STRENGHT_TO_TIME_FREQUENCY", 0x92E828C7));
	sm_WindStrengthToDistance.Init(ATSTRINGHASH("WIND_STRENGTH_TO_DISTANCE", 0xEFBD7492));
	sm_WindStrengthToSpeed.Init(ATSTRINGHASH("WIND_STRENGTH_TO_SPEED", 0x8562B09E));
	sm_PoolWaterSounds.Init(ATSTRINGHASH("POOL_WATER_SOUNDSET", 0xE2C306A4));
}
//------------------------------------------------------------------------------------------------------------------------------
audShoreLinePool::audShoreLinePool(const ShoreLinePoolAudioSettings* settings) : audShoreLine()
{
	m_Settings = static_cast<const ShoreLineAudioSettings*>(settings);
	m_PoolWaterSound = NULL;
	m_PoolSoundDir.Zero();
	m_TimeSinceLastPoolSoundPlayed = 0;
	m_PoolSize = 0;
}
audShoreLinePool::~audShoreLinePool()
{
	if(m_PoolWaterSound)
	{
		m_PoolWaterSound->StopAndForget();
	}
	for(u32 i = 0 ; i < g_PoolEdgePoints; i++)
	{
		if(m_WaterLappingSounds[i])
		{
			m_WaterLappingSounds[i]->StopAndForget();
		}
	}
}
// ----------------------------------------------------------------
// Load shoreline data from a file
// ----------------------------------------------------------------
void audShoreLinePool::Load(BANK_ONLY(const ShoreLineAudioSettings *settings))
{
	audShoreLine::Load(BANK_ONLY(settings));
// TODO: To be deleted.
	for(u32 i = 0; i < GetPoolSettings()->numShoreLinePoints; i++)
	{
		m_ActivationBox.Add(GetPoolSettings()->ShoreLinePoints[i].X,GetPoolSettings()->ShoreLinePoints[i].Y);
	}
	const f32 activationRange = 20.f;
	m_ActivationBox.Grow(activationRange, activationRange);

	for(u32 i = 0 ; i < g_PoolEdgePoints; i++)
	{
		m_WaterLappingSounds[i] = NULL;
		m_LastSplashTime[i] = 0;
		m_RandomSplashDelay[i] = (u16)audEngineUtil::GetRandomNumberInRange(4000,12000);
	}

	m_PoolWaterSound = NULL;
	m_PoolSoundDir.Zero();
	m_TimeSinceLastPoolSoundPlayed = 0;
	m_PoolSize = GetPoolSettings()->SmallestDistanceToPoint;
}
// ----------------------------------------------------------------
// Update the shoreline activation
// ----------------------------------------------------------------
bool audShoreLinePool::UpdateActivation()
{
	if(audShoreLine::UpdateActivation())
	{
		if(!m_IsActive)
		{
			m_IsActive = true;
			m_JustActivated = true;
		}
		u8 numPointsToUpdate = (sm_NumExtraPoolPointsToUpdate * 2) + 1;
		if((u32)GetPoolSettings()->numShoreLinePoints > numPointsToUpdate)
		{
			// Look for the closest point to the listener.
			f32 closestDist2 = LARGE_FLOAT;
			s32 closestIdx = 0;
			s32 startIdx = -1;
			s32 startSIdx = -1;
			s32 endIdx = -1;
			s32 endSIdx = -1;
			Vector3 pos2D = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition());
			pos2D.z = 0.0f; 
			GetSearchIndices(pos2D,startIdx,endIdx,startSIdx,endSIdx);
			// Once we have our search indices, look for the closest point
			for(s32 i = startIdx; i < endIdx; i++)
			{
				const f32 dist2 = (Vector3(GetPoolSettings()->ShoreLinePoints[i].X,GetPoolSettings()->ShoreLinePoints[i].Y,0.f)-pos2D).Mag2();
				if(dist2 < closestDist2)
				{
					closestDist2 = dist2;
					closestIdx = i;
				}
			}
			if(startSIdx != -1)
			{
				naAssert (endSIdx != -1);
				for(s32 i = startSIdx; i < endSIdx; i++)
				{
					const f32 dist2 = (Vector3(GetPoolSettings()->ShoreLinePoints[i].X,GetPoolSettings()->ShoreLinePoints[i].Y,0.f)-pos2D).Mag2();
					if(dist2 < closestDist2)
					{
						closestDist2 = dist2;
						closestIdx = i;
					}
				}
			}
			// Now in closestIdx we have the closest point to the current shoreline. Get the n points to both sides to update.
			u32 waterIndices = 0;
			m_WaterEdgeIndices[waterIndices++] = (u8)closestIdx;
		}			
		else
		{
			for(u8 i = 0; i < GetPoolSettings()->numShoreLinePoints; i++)
			{
				m_WaterEdgeIndices[i] = i;
			}
		}
		if(m_JustActivated)
		{
			const u32 now = fwTimer::GetTimeInMilliseconds();
			for(u32 i = 0; i < numPointsToUpdate; i++)
			{
				m_LastSplashTime[i] = now;
				m_RandomSplashDelay[i] = (u16)audEngineUtil::GetRandomNumberInRange(4000,12000);
				if(m_WaterLappingSounds[i])
				{
					m_WaterLappingSounds[i]->StopAndForget();
				}
			}
			m_JustActivated = false;
		}
		return true;
	}
	m_IsActive = false;
	m_DistanceToShore = LARGE_FLOAT;
	if (m_PoolWaterSound)
	{
		m_PoolWaterSound->StopAndForget();
	}
	for(u32 i = 0; i < g_PoolEdgePoints; i++)
	{
		if(m_WaterLappingSounds[i])
		{
			m_WaterLappingSounds[i]->StopAndForget();
		}
	}
	return false;
}
bool audShoreLinePool::IsPointOverWater(const Vector3& pos) const
{
	/* TODO - Reinstate this code once RAVE activation box issues are sorted. We want to do a cheap test
	   against the bounding box containing all the shoreline points (not the activation box, which is larger)
	   and do an early return if we're not within it
	if(!m_BoundingBox.ContainsPointFlat(VECTOR3_TO_VEC3V(pos)))
	{
		return false;
	}
	*/

	u32 winding = 0;

	for(u32 index = 0; index < GetPoolSettings()->numShoreLinePoints; index++)
	{
		u32 nextIndex = index + 1;

		if(nextIndex >= GetPoolSettings()->numShoreLinePoints)
		{
			nextIndex = 0;
		}

		Vector2 thisShoreLinePos = Vector2(GetPoolSettings()->ShoreLinePoints[index].X, GetPoolSettings()->ShoreLinePoints[index].Y);
		Vector2 nextShoreLinePos = Vector2(GetPoolSettings()->ShoreLinePoints[nextIndex].X, GetPoolSettings()->ShoreLinePoints[nextIndex].Y);

		if(thisShoreLinePos.y <= pos.y)
		{
			if(nextShoreLinePos.y > pos.y)
			{
				f32 isLeft = ((nextShoreLinePos.x - thisShoreLinePos.x) * (pos.y - thisShoreLinePos.y) - (pos.x - thisShoreLinePos.x) * (nextShoreLinePos.y - thisShoreLinePos.y));

				if(isLeft > 0)
				{
					winding++;
				}
			}
		}
		else
		{
			if(nextShoreLinePos.y <= pos.y)
			{
				f32 isLeft = ((nextShoreLinePos.x - thisShoreLinePos.x) * (pos.y - thisShoreLinePos.y) - (pos.x - thisShoreLinePos.x) * (nextShoreLinePos.y - thisShoreLinePos.y));

				if(isLeft < 0)
				{
					winding--;
				}
			}
		}
	}

	return winding != 0;
}
//------------------------------------------------------------------------------------------------------------------------------
void audShoreLinePool::GetSearchIndices(const Vector3 &position2D,s32 &startIdx,s32 &endIdx,s32 &startSIdx,s32 &endSIdx) const
{
	Vector3 center; 
	m_ActivationBox.GetCentre(center.x,center.y);
	center.z = 0.f;
	Vector3 dirToListener  =  position2D - center;
	dirToListener.NormalizeSafe();
	const Vector3 north = Vector3(0.0f, 1.0f, 0.0f);
	const Vector3 east = Vector3(1.0f,0.0f,0.0f);
	f32 northDotAngle = north.Dot(dirToListener);
	f32 eastDotAngle = east.Dot(dirToListener);

	// by checking the dot angle we know what quadrants to update
	if(northDotAngle < 0)
	{
		if(eastDotAngle <= - 0.707f)
		{
			// Update first and fourth quadrants.
			startIdx = 0;
			endIdx = GetPoolSettings()->FirstQuadIndex;
			if(GetPoolSettings()->FourthQuadIndex < (s32)GetPoolSettings()->numShoreLinePoints)
			{
				startSIdx = GetPoolSettings()->FourthQuadIndex;
				endSIdx = (s32)GetPoolSettings()->numShoreLinePoints;
			}
#if __BANK
			if(sm_ShowIndices)
			{
				char txt[256];
				formatf(txt,"Update first and fourth quadrants.");
				grcDebugDraw::AddDebugOutput(Color_white,txt);
				formatf(txt,"[Start %d] [End %d] [SStart %d] [SEnd %d] ",startIdx,endIdx,startSIdx,endSIdx);
				grcDebugDraw::AddDebugOutput(Color_white,txt);
			}
#endif
		}
		else if( eastDotAngle <= 0.707f)
		{
			//Update first and second quadrants.
			startIdx = 0;
			endIdx = GetPoolSettings()->SecondQuadIndex;
			if(GetPoolSettings()->FourthQuadIndex < (s32)GetPoolSettings()->numShoreLinePoints)
			{
				startSIdx = GetPoolSettings()->FourthQuadIndex;
				endSIdx = (s32)GetPoolSettings()->numShoreLinePoints;
			}
#if __BANK
			if(sm_ShowIndices)
			{
				char txt[256];
				formatf(txt,"Update first and second quadrants.");
				grcDebugDraw::AddDebugOutput(Color_white,txt);
				formatf(txt,"[Start %d] [End %d] [SStart %d] [SEnd %d] ",startIdx,endIdx,startSIdx,endSIdx);
				grcDebugDraw::AddDebugOutput(Color_white,txt);
			}
#endif
		}
		else 
		{
			//Update second and third quadrants
			startIdx = GetPoolSettings()->FirstQuadIndex + 1;
			endIdx = GetPoolSettings()->ThirdQuadIndex;
#if __BANK
			if(sm_ShowIndices)
			{
				char txt[256];
				formatf(txt,"Update second and third quadrants");
				grcDebugDraw::AddDebugOutput(Color_white,txt);
				formatf(txt,"[Start %d] [End %d] [SStart %d] [SEnd %d] ",startIdx,endIdx,startSIdx,endSIdx);
				grcDebugDraw::AddDebugOutput(Color_white,txt);
			}
#endif
		}
	}
	else 
	{
		if(eastDotAngle > 0.707f)
		{
			//Update second and third quadrants
			startIdx = GetPoolSettings()->FirstQuadIndex + 1;
			endIdx = GetPoolSettings()->ThirdQuadIndex;
#if __BANK
			if(sm_ShowIndices)
			{
				char txt[256];
				formatf(txt,"Update second and third quadrants");
				grcDebugDraw::AddDebugOutput(Color_white,txt);
				formatf(txt,"[Start %d] [End %d] [SStart %d] [SEnd %d] ",startIdx,endIdx,startSIdx,endSIdx);
				grcDebugDraw::AddDebugOutput(Color_white,txt);
			}
#endif
		}
		else if (eastDotAngle <= 0.707f && eastDotAngle >= -0.707f)
		{
			//Update third and fourth quadrants
			startIdx = GetPoolSettings()->SecondQuadIndex + 1;
			endIdx = GetPoolSettings()->FourthQuadIndex;
#if __BANK
			if(sm_ShowIndices)
			{
				char txt[256];
				formatf(txt,"Update third and fourth quadrants");
				grcDebugDraw::AddDebugOutput(Color_white,txt);
				formatf(txt,"[Start %d] [End %d] [SStart %d] [SEnd %d] ",startIdx,endIdx,startSIdx,endSIdx);
				grcDebugDraw::AddDebugOutput(Color_white,txt);
			}
#endif
		}
		else 
		{
			// Update fourth and first quadrant
			startIdx = 0;
			endIdx = GetPoolSettings()->FirstQuadIndex;
			if(GetPoolSettings()->FourthQuadIndex < (s32)GetPoolSettings()->numShoreLinePoints)
			{
				startSIdx = GetPoolSettings()->FourthQuadIndex;
				endSIdx = (s32)GetPoolSettings()->numShoreLinePoints;
			}
#if __BANK
			if(sm_ShowIndices)
			{
				char txt[256];
				formatf(txt,"Update first and fourth quadrants.");
				grcDebugDraw::AddDebugOutput(Color_white,txt);
				formatf(txt,"[Start %d] [End %d] [SStart %d] [SEnd %d] ",startIdx,endIdx,startSIdx,endSIdx);
				grcDebugDraw::AddDebugOutput(Color_white,txt);
			}
#endif
		}
	}
}
// ----------------------------------------------------------------
// Update the shoreline 
// ----------------------------------------------------------------
void audShoreLinePool::Update(const f32 /*timeElapsedS*/)
{
		// Play the main water sound along the surface of the water.
		Vector3 poolCentre; 
		m_ActivationBox.GetCentre(poolCentre.x,poolCentre.y);
		poolCentre.z = m_PoolWaterHeight;
		m_CentrePoint = poolCentre;
		Vector3 closestPoint = Vector3(GetPoolSettings()->ShoreLinePoints[m_WaterEdgeIndices[0]].X
										 ,GetPoolSettings()->ShoreLinePoints[m_WaterEdgeIndices[0]].Y
										 ,m_PoolWaterHeight);
		m_DistanceToShore = (poolCentre - VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition())).Mag2();
		Vector3 dirToCentre = poolCentre - closestPoint;
		dirToCentre.RotateZ(PI * 0.5f);
		Vector3 windSpeed;
		WIND.GetLocalVelocity( RC_VEC3V(poolCentre), RC_VEC3V(windSpeed));
		f32 windStrength = g_WeatherAudioEntity.GetWindStrength();
		dirToCentre.NormalizeSafe();
		windSpeed.NormalizeSafe();
		f32 dot = dirToCentre.Dot(m_PoolSoundDir);
		s32 numPointsToUpdate = (sm_NumExtraPoolPointsToUpdate * 2) + 1;
		if(dot > 0.f)
		{
			s32 shoreLineIdx = m_WaterEdgeIndices[0];
			// Left from closest idx.
			for (u32 i = 1; i < numPointsToUpdate; i++)
			{
				shoreLineIdx = (m_WaterEdgeIndices[i-1] - audEngineUtil::GetRandomNumberInRange (1, 3));
				if(shoreLineIdx < 0) 
				{
					shoreLineIdx  = (s32)GetPoolSettings()->numShoreLinePoints -1;
				}
				m_WaterEdgeIndices[i] = (u8)shoreLineIdx;
			}
		}
		else 
		{
			s32 shoreLineIdx = m_WaterEdgeIndices[0];
			// Left from closest idx.
			for (u32 i = 1; i < numPointsToUpdate; i++)
			{
				shoreLineIdx = (m_WaterEdgeIndices[i-1] + audEngineUtil::GetRandomNumberInRange (1, 3)) % (s32)GetPoolSettings()->numShoreLinePoints;
				m_WaterEdgeIndices[i] = (u8)shoreLineIdx;
			}
		}

		if(!m_PoolWaterSound)
		{
			// Get the wind direction and strength around to the centre point of the shorline 
			m_TimeSinceLastPoolSoundPlayed += fwTimer::GetTimeStepInMilliseconds();
		
			if(m_TimeSinceLastPoolSoundPlayed > (sm_WindStrengthToTimeFrequency.CalculateValue(windStrength) + audEngineUtil::GetRandomNumberInRange (GetPoolSettings()->WaterLappingMinDelay,GetPoolSettings()->WaterLappingMaxDelay)))
			{
				m_TimeSinceLastPoolSoundPlayed = 0;
				m_PoolSoundDir = windSpeed;
				audSoundInitParams initParams; 
			m_PoolSize = sm_WindStrengthToDistance.CalculateValue(windStrength);
			Vector3 playPosition = poolCentre - (windSpeed * m_PoolSize * 0.5f);
				initParams.Position = playPosition;
				initParams.EnvironmentGroup = CreateShoreLineEnvironmentGroup(playPosition);
				g_AmbientAudioEntity.CreateAndPlaySound_Persistent(sm_PoolWaterSounds.Find(ATSTRINGHASH("WATER_LAPPING", 0x72C1BEA)),&m_PoolWaterSound,&initParams);
			}
		}
		else 
		{
		if(m_PoolSize > 0.f)
			{
				f32 deltaDistance = sm_WindStrengthToSpeed.CalculateValue(windStrength) * fwTimer::GetTimeStep();
				// we must update an existing event, modulate the energy and position the sound properly.
				Vector3 newPosition = VEC3V_TO_VECTOR3(m_PoolWaterSound->GetRequestedPosition())
					+  deltaDistance * m_PoolSoundDir;
				m_PoolWaterSound->SetRequestedPosition(newPosition);
			m_PoolSize -= deltaDistance;
#if __BANK
				if(sm_DrawWaterBehaviour)
				{
					grcDebugDraw::Sphere( m_PoolWaterSound->GetRequestedPosition(),0.1f,Color_red);
				}
#endif
			}
			
		}
		// Play random water slapping on the closest points. 
		for (u32 i = 0; i < numPointsToUpdate; i++)
		{
			if(fwTimer::GetTimeInMilliseconds() > m_LastSplashTime[i] + m_RandomSplashDelay[i])
			{
				u32 idx = m_WaterEdgeIndices[i];
				if(naVerifyf(idx < GetPoolSettings()->numShoreLinePoints,"Wrong node index." ))
				{
					m_RandomSplashDelay[i] = (u16)audEngineUtil::GetRandomNumberInRange(GetPoolSettings()->WaterSplashMinDelay,GetPoolSettings()->WaterSplashMaxDelay);
					audSoundInitParams initParams;
					Vector3 pos = Vector3(GetPoolSettings()->ShoreLinePoints[idx].X, GetPoolSettings()->ShoreLinePoints[idx].Y, m_PoolWaterHeight);
					initParams.Position = pos;
					initParams.EnvironmentGroup = CreateShoreLineEnvironmentGroup(pos);
					//initParams.Volume = audDriverUtil::ComputeDbVolumeFromLinear(sm_LappingWaveWindToVol.CalculateValue(windStrength));
					g_AmbientAudioEntity.CreateAndPlaySound(sm_PoolWaterSounds.Find(ATSTRINGHASH("WATER_SPLASHING", 0x2759CDA4)),&initParams);
					m_LastSplashTime[i] = fwTimer::GetTimeInMilliseconds();
#if __BANK
					if(sm_DrawWaterBehaviour)
					{
						grcDebugDraw::Sphere( pos,0.3f,Color_blue);
					}
#endif
				}
			}
		}
}
//------------------------------------------------------------------------------------------------------------------------------
void audShoreLinePool::UpdateWaterHeight(bool BANK_ONLY(updateAllHeights))
{

	if(m_Settings)
	{
		f32 waterz;
		CVfxHelper::GetWaterZ(Vec3V(GetPoolSettings()->ShoreLinePoints[0].X, GetPoolSettings()->ShoreLinePoints[0].Y, 0.f), waterz);
		m_PoolWaterHeight = waterz;
	}
#if __BANK
	m_WaterHeights.Reset();
	m_WaterHeights.Resize(0);
	if(!m_Settings)
	{
		for(u32 idx = 0 ; idx <m_ShoreLinePoints.GetCount(); idx++)
		{
			f32 waterz;
			CVfxHelper::GetWaterZ(Vec3V(m_ShoreLinePoints[idx].x,m_ShoreLinePoints[idx].y, 0.f), waterz);
			m_WaterHeights.PushAndGrow(waterz);
		}
	}
	else if (updateAllHeights || audAmbientAudioEntity::sm_DrawShoreLines || audAmbientAudioEntity::sm_DrawActivationBox || sm_DrawWaterBehaviour)
	{
		for(u32 idx = 0 ; idx <(u32)GetPoolSettings()->numShoreLinePoints; idx++)
		{
			f32 waterz;
			CVfxHelper::GetWaterZ(Vec3V(GetPoolSettings()->ShoreLinePoints[idx].X, GetPoolSettings()->ShoreLinePoints[idx].Y, 0.f), waterz);
			m_WaterHeights.PushAndGrow(waterz);
			m_PoolWaterHeight = waterz;
		}
		
	}
#endif
}
//------------------------------------------------------------------------------------------------------------------------------
#if __BANK
// ----------------------------------------------------------------
// Initialise the shore line
// ----------------------------------------------------------------
void audShoreLinePool::InitDebug()
{
	if(m_Settings)
	{
		m_ShoreLinePoints.Reset();
		m_ShoreLinePoints.Resize(GetPoolSettings()->numShoreLinePoints);
		for(s32 i = 0; i < GetPoolSettings()->numShoreLinePoints; i++)
		{
			m_ShoreLinePoints[i].x = GetPoolSettings()->ShoreLinePoints[i].X;
			m_ShoreLinePoints[i].y = GetPoolSettings()->ShoreLinePoints[i].Y;
		}
		sm_PoolWaterLappingDelayMin = GetPoolSettings()->WaterLappingMinDelay;
		sm_PoolWaterLappingDelayMax = GetPoolSettings()->WaterLappingMaxDelay;
		sm_PoolWaterSplashDelayMin = GetPoolSettings()->WaterSplashMinDelay;
		sm_PoolWaterSplashDelayMax = GetPoolSettings()->WaterSplashMaxDelay;
		Vector2 center = Vector2(GetPoolSettings()->ActivationBox.Center.X,GetPoolSettings()->ActivationBox.Center.Y);
		m_ActivationBox = fwRect(center,GetPoolSettings()->ActivationBox.Size.Width/2.f,GetPoolSettings()->ActivationBox.Size.Height/2.f);
		m_RotAngle = GetPoolSettings()->ActivationBox.RotationAngle;
		m_Width = m_ActivationBox.GetWidth();
		m_Height = m_ActivationBox.GetHeight();
	}
}
// ----------------------------------------------------------------
// Serialise the shoreline data
// ----------------------------------------------------------------
f32 audShoreLinePool::CalculateSmallestDistanceToPoint()
{
	f32 smallestDistance = LARGE_FLOAT;
	for (u32 i = 0; i < m_ShoreLinePoints.GetCount(); i++)
	{
		Vector3 center; 
		m_ActivationBox.GetCentre(center.x,center.y);
		center.z = 0.f;
		Vector3 dirToCentre = center - Vector3(m_ShoreLinePoints[i].x,m_ShoreLinePoints[i].y,0.f);
		f32 distanceToCentre = fabs(dirToCentre.Mag());
		if(distanceToCentre <= smallestDistance)
		{
			smallestDistance = distanceToCentre;
		}
	}
	return smallestDistance;
}
//------------------------------------------------------------------------------------------------------------------------------
bool audShoreLinePool::Serialise(const char* shoreName,bool editing)
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
void audShoreLinePool::FormatShoreLineAudioSettingsXml(char * xmlMsg, const char * settingsName)
{
	sprintf(xmlMsg, "<RAVEMessage>\n");
	char tmpBuf[256] = {0};

	atString upperSettings(settingsName);
	upperSettings.Uppercase();

	sprintf(tmpBuf, "	<EditObjects metadataType=\"GAMEOBJECTS\" chunkNameHash=\"%u\">\n", atStringHash("BASE"));
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "		<ShoreLinePoolAudioSettings name=\"%s\">\n", &settingsName[0]);
	strcat(xmlMsg, tmpBuf);

	SerialiseActivationBox(xmlMsg,tmpBuf);

	sprintf(tmpBuf, "			<WaterLappingMinDelay>%d</WaterLappingMinDelay>",sm_PoolWaterLappingDelayMin);
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "			<WaterLappingMaxDelay>%d</WaterLappingMaxDelay>",sm_PoolWaterLappingDelayMax);
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "			<WaterSplashMinDelay>%d</WaterSplashMinDelay>",sm_PoolWaterSplashDelayMin);
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "			<WaterSplashMaxDelay>%d</WaterSplashMaxDelay>",sm_PoolWaterSplashDelayMax);
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "			<FirstQuadIndex>-1</FirstQuadIndex>");
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "			<SecondQuadIndex>-1</SecondQuadIndex>");
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "			<ThirdQuadIndex>-1</ThirdQuadIndex>");
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "			<FourthQuadIndex>-1</FourthQuadIndex>");
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "			<SmallestDistanceToPoint>%f</SmallestDistanceToPoint>",CalculateSmallestDistanceToPoint());
	strcat(xmlMsg, tmpBuf);

	for(s32 i = 0 ; i < m_ShoreLinePoints.GetCount(); i++)
	{
		sprintf(tmpBuf, "		<ShoreLinePoints>");
		strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "			<X>%f</X>",m_ShoreLinePoints[i].x);
		strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "			<Y>%f</Y>",m_ShoreLinePoints[i].y);
		strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "		</ShoreLinePoints>");
		strcat(xmlMsg, tmpBuf);
	}
	sprintf(tmpBuf, "		</ShoreLinePoolAudioSettings>\n");
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "	</EditObjects>\n");
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "</RAVEMessage>\n");
	strcat(xmlMsg, tmpBuf);
}
//------------------------------------------------------------------------------------------------------------------------------
void audShoreLinePool::MoveShoreAlongDirection(const Vector2 &dirOfMovement,const u8 /*side*/)
{	
	for (u32 i = 0; i < m_ShoreLinePoints.GetCount(); i++)
	{
		m_ShoreLinePoints[i] = m_ShoreLinePoints[i] + dirOfMovement;
	}
}
// ----------------------------------------------------------------
// Draw all debug info related to the shoreline
// ----------------------------------------------------------------
void audShoreLinePool::DrawActivationBox(bool /*useSizeFromData*/)const 
{
	// activation box
	if(!m_ActivationBox.IsInvalidOrZeroArea() && m_WaterHeights.GetCount())
	{
		Vector2 center;
		m_ActivationBox.GetCentre(center.x,center.y);
		spdAABB firstQuad,secondQuad,thirdQuad,fourthQuad;
		//First quadrant
		firstQuad.Set(Vec3V(m_ActivationBox.GetLeft(),m_ActivationBox.GetBottom(),m_WaterHeights[0])
			,Vec3V(center.x,center.y,m_WaterHeights[0] + 10.f));
		grcDebugDraw::BoxAxisAligned(firstQuad.GetMin(), firstQuad.GetMax(), Color32(0,255,0,128));
		//Second quadrant
		Vec3V min = Vec3V(center.x,m_ActivationBox.GetBottom(),m_WaterHeights[0]);
		Vec3V max = Vec3V(m_ActivationBox.GetRight(),center.y,m_WaterHeights[0] + 10.f);
		secondQuad.Set(min,max);
		grcDebugDraw::BoxAxisAligned(secondQuad.GetMin(), secondQuad.GetMax(), Color32(255,0,0,128));
		//Third quadrant
		thirdQuad.Set(Vec3V(center.x,center.y,m_WaterHeights[0])
			,Vec3V(m_ActivationBox.GetRight(),m_ActivationBox.GetTop(),m_WaterHeights[0] + 10.f));
		grcDebugDraw::BoxAxisAligned(thirdQuad.GetMin(), thirdQuad.GetMax(), Color32(0,0,255,128));
		//Fourth quadrant
		min = Vec3V(m_ActivationBox.GetLeft(),center.y,m_WaterHeights[0]);
		max = Vec3V(center.x,m_ActivationBox.GetTop(),m_WaterHeights[0] + 10.f);
		fourthQuad.Set(min,max);
		grcDebugDraw::BoxAxisAligned(fourthQuad.GetMin(), fourthQuad.GetMax(), Color32(255,0,255,128));
	}
}
//------------------------------------------------------------------------------------------------------------------------------
void audShoreLinePool::DrawShoreLines(bool drawSettingsPoints, const u32 pointIndex, bool /*checkForPlaying*/)	const
{
	// draw lines
	if(drawSettingsPoints && GetPoolSettings() && (u32)GetPoolSettings()->numShoreLinePoints > 0 && m_WaterHeights.GetCount()
		&& GetPoolSettings()->numShoreLinePoints == m_WaterHeights.GetCount())
	{
		for(u32 i = 0; i < (u32)GetPoolSettings()->numShoreLinePoints - 1; i++)
		{
			Vector3 currentPoint = Vector3(GetPoolSettings()->ShoreLinePoints[i].X, GetPoolSettings()->ShoreLinePoints[i].Y, m_WaterHeights[i]);
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
			if(i < GetPoolSettings()->numShoreLinePoints - 1 )
			{
				Vector3 nextPoint = Vector3(GetPoolSettings()->ShoreLinePoints[i+1].X, GetPoolSettings()->ShoreLinePoints[i+1].Y, m_WaterHeights[i+1]);
				grcDebugDraw::Line(currentPoint, nextPoint,	Color_red);
			}
		}
	}
	if(m_ShoreLinePoints.GetCount() && m_WaterHeights.GetCount() && m_ShoreLinePoints.GetCount() == m_WaterHeights.GetCount())
	{
		for(u32 i = 0; i < (u32)m_ShoreLinePoints.GetCount(); i++)
		{
			Vector3 currentPoint = Vector3(m_ShoreLinePoints[i].x, m_ShoreLinePoints[i].y, m_WaterHeights[i] );
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
				Vector3 nextPoint = Vector3(m_ShoreLinePoints[i+1].x, m_ShoreLinePoints[i+1].y, m_WaterHeights[i+1]);
				grcDebugDraw::Line(currentPoint, nextPoint,	Color_red);
			}
		}
	}
}
// ----------------------------------------------------------------
void audShoreLinePool::RenderDebug()const 
{
	if(!m_ActivationBox.IsInvalidOrZeroArea() && m_WaterHeights.GetCount())
	{
		Vector2 center;
		m_ActivationBox.GetCentre(center.x,center.y);
		spdAABB firstQuad,secondQuad,thirdQuad,fourthQuad;
		//First quadrant
		firstQuad.Set(Vec3V(m_ActivationBox.GetLeft(),m_ActivationBox.GetBottom(),m_WaterHeights[0])
			,Vec3V(center.x,center.y,m_WaterHeights[0]));
		CVectorMap::DrawRectAxisAligned(VEC3V_TO_VECTOR3(firstQuad.GetMin()), VEC3V_TO_VECTOR3(firstQuad.GetMax()), Color32(0,255,0,255),true);
		//Second quadrant
		Vec3V min = Vec3V(center.x,m_ActivationBox.GetBottom(),m_WaterHeights[0]);
		Vec3V max = Vec3V(m_ActivationBox.GetRight(),center.y,m_WaterHeights[0]);
		secondQuad.Set(min,max);
		CVectorMap::DrawRectAxisAligned(VEC3V_TO_VECTOR3(secondQuad.GetMin()), VEC3V_TO_VECTOR3(secondQuad.GetMax()), Color32(255,0,0,255),true);
		//Third quadrant
		thirdQuad.Set(Vec3V(center.x,center.y,m_WaterHeights[0])
			,Vec3V(m_ActivationBox.GetRight(),m_ActivationBox.GetTop(),m_WaterHeights[0]));
		CVectorMap::DrawRectAxisAligned(VEC3V_TO_VECTOR3(thirdQuad.GetMin()), VEC3V_TO_VECTOR3(thirdQuad.GetMax()), Color32(0,0,255,255),true);
		//Fourth quadrant
		min = Vec3V(m_ActivationBox.GetLeft(),center.y,m_WaterHeights[0]);
		max = Vec3V(center.x,m_ActivationBox.GetTop(),m_WaterHeights[0]);
		fourthQuad.Set(min,max);
		CVectorMap::DrawRectAxisAligned(VEC3V_TO_VECTOR3(fourthQuad.GetMin()),VEC3V_TO_VECTOR3( fourthQuad.GetMax()), Color32(255,0,255,255),true);

		for(u32 i = 0; i < (u32)GetPoolSettings()->numShoreLinePoints - 1; i++)
		{
			CVectorMap::DrawLine(Vector3(GetPoolSettings()->ShoreLinePoints[i].X, GetPoolSettings()->ShoreLinePoints[i].Y, 0.f), 
				Vector3(GetPoolSettings()->ShoreLinePoints[i+1].X, GetPoolSettings()->ShoreLinePoints[i+1].Y, 0.f),
				Color_red,Color_blue);
		}
	}
}
//------------------------------------------------------------------------------------------------------------------------------
u32 audShoreLinePool::GetNumPoints()
{
	if(m_Settings)
	{
		return GetPoolSettings()->numShoreLinePoints;
	}
	return 0;
}
// ----------------------------------------------------------------
void audShoreLinePool::AddDebugShorePoint(const Vector2 &vec)
{
	m_ShoreLinePoints.PushAndGrow(vec);
	m_ActivationBox.Add(vec.x,vec.y);
	m_Width = m_ActivationBox.GetWidth();
	m_Height = m_ActivationBox.GetHeight();
}
//------------------------------------------------------------------------------------------------------------------------------
void audShoreLinePool::EditShorelinePoint(const u32 pointIdx, const Vector3 &newPos)
{
	if(m_Settings)
	{
		if(naVerifyf(pointIdx < GetPoolSettings()->numShoreLinePoints,"Wrong point index"))
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
////------------------------------------------------------------------------------------------------------------------------------
void audShoreLinePool::Cancel()
{	
	audShoreLine::Cancel();
	sm_PoolWaterLappingDelayMin = -500;
	sm_PoolWaterLappingDelayMax = 500;
	sm_PoolWaterSplashDelayMin = 5000;
	sm_PoolWaterSplashDelayMax = 14000;

}
// ----------------------------------------------------------------
// Add widgets to RAG tool
// ----------------------------------------------------------------
void audShoreLinePool::AddWidgets(bkBank &bank)
{
	bank.PushGroup("Pool");
		bank.AddToggle("sm_ShowIndices",&sm_ShowIndices);
	bank.PopGroup();
}
//------------------------------------------------------------------------------------------------------------------------------
void audShoreLinePool::AddEditorWidgets(bkBank &bank)
{
	bank.PushGroup("Pool parameters",false);
	bank.AddSlider("Water lapping delay min (ms)", &sm_PoolWaterLappingDelayMin, -5000, 0, 100);
	bank.AddSlider("Water lapping delay max (ms)", &sm_PoolWaterLappingDelayMax, 0,5000,100);
	bank.AddSlider("Random splash delay min (ms)", &sm_PoolWaterSplashDelayMin, 0, 30000, 100);
	bank.AddSlider("Random splash delay max (ms)", &sm_PoolWaterSplashDelayMax, 0, 30000, 100);
	bank.PopGroup();
}
#endif
