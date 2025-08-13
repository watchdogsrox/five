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
#include "audshorelineOcean.h"
#include "audio/northaudioengine.h"
#include "audio/gameobjects.h"
#include "debug/vectormap.h"
#include "renderer/water.h"
#include "game/weather.h"
#include "vfx/VfxHelper.h"
#include "peds/ped.h"
#include "audio/weatheraudioentity.h"

AUDIO_AMBIENCE_OPTIMISATIONS()

atRangeArray<audSound*,NumWaterSoundPos> audShoreLineOcean::sm_Sounds;
atRangeArray<audSound*,NumWaterSoundPos> audShoreLineOcean::sm_OceanWaveStartSound;
atRangeArray<audSound*,NumWaterSoundPos> audShoreLineOcean::sm_OceanWaveBreakSound;
atRangeArray<audSound*,NumWaterSoundPos> audShoreLineOcean::sm_OceanWaveEndSound;
atRangeArray<audSound*,NumWaterSoundPos> audShoreLineOcean::sm_OceanWaveRecedeSound;
atRangeArray<audShoreLineOcean*,NumWaterSoundPos> audShoreLineOcean::sm_CurrentShore;
atRangeArray<f32,NumWaterSoundPos> audShoreLineOcean::sm_WaterHeights;

audSoundSet audShoreLineOcean::sm_OceanSoundSet;
							   

audSmoother audShoreLineOcean::sm_ProyectionSmoother;
audSmoother audShoreLineOcean::sm_WidthSmoother;

Vector3 audShoreLineOcean::sm_PrevNode;
Vector3 audShoreLineOcean::sm_ClosestNode;
Vector3 audShoreLineOcean::sm_NextNode;
Vector3 audShoreLineOcean::sm_CurrentOceanDirection;
Vector3 audShoreLineOcean::sm_WaveStartDetectionPoint;
Vector3 audShoreLineOcean::sm_WaveBreaksDetectionPoint;
Vector3 audShoreLineOcean::sm_WaveEndsDetectionPoint;

audShoreLineOcean* audShoreLineOcean::sm_ClosestShore = NULL;

audSound* audShoreLineOcean::sm_OutInTheOceanSound = NULL;

f32 audShoreLineOcean::sm_WaveStartDetectionPointHeight = -9999.f;
f32 audShoreLineOcean::sm_WaveBreaksDetectionPointHeight = -9999.f;
f32 audShoreLineOcean::sm_WaveEndsDetectionPointHeight = -9999.f;
f32 audShoreLineOcean::sm_LastWaveStartDetectionPointHeight = -9999.f;
f32 audShoreLineOcean::sm_LastWaveBreaksDetectionPointHeight = -9999.f;
f32 audShoreLineOcean::sm_LastRecedeEndDetectionPointHeight = -9999.f; 

f32 audShoreLineOcean::sm_InnerDistance = 25.f;
f32 audShoreLineOcean::sm_OutterDistance = 35.f;
f32 audShoreLineOcean::sm_MaxDistanceToShore = 50.f;

f32 audShoreLineOcean::sm_ClosestDistToShore = LARGE_FLOAT;
f32 audShoreLineOcean::sm_DistanceToShore = 0.f;
f32 audShoreLineOcean::sm_SqdDistanceIntoWater = 0.f;

u8 audShoreLineOcean::sm_ClosestNodeIdx = 0;
u8 audShoreLineOcean::sm_LastClosestNodeIdx = 0;
u8 audShoreLineOcean::sm_PrevNodeIdx = 0;
u8 audShoreLineOcean::sm_NextNodeIdx = 0;

bool audShoreLineOcean::sm_ListenerAboveWater = false;
// ----------------------------------------------------------------
// Initialise static member variables
// ----------------------------------------------------------------
audShoreLineOcean::audShoreLineOcean(const ShoreLineOceanAudioSettings* settings) : audShoreLine()
{
	m_Settings = static_cast<const ShoreLineAudioSettings*>(settings);

	m_PrevShoreline = NULL;
	m_NextShoreline = NULL;
}
// ----------------------------------------------------------------
audShoreLineOcean::~audShoreLineOcean()
{
	m_PrevShoreline = NULL;
	m_NextShoreline = NULL;
}
// ----------------------------------------------------------------
void audShoreLineOcean::InitClass()
{
	sm_OceanSoundSet.Init(ATSTRINGHASH("OCEAN_BEACH_SOUNDSET", 0xB12515A5));

	for (u32 i = 0; i < NumWaterSoundPos;i ++)
	{
		sm_Sounds[i] = NULL;
		sm_OceanWaveStartSound[i] = NULL;
		sm_OceanWaveBreakSound[i] = NULL;
		sm_OceanWaveEndSound[i] = NULL;
		sm_OceanWaveRecedeSound[i] = NULL;
		sm_CurrentShore[i] = NULL;
	}
	for (u8 i = 0; i < NumWaterHeights;i ++)
	{
		sm_WaterHeights[i] = -9999.f;
	}
	sm_WaveStartDetectionPointHeight = -9999.f;
	sm_WaveBreaksDetectionPointHeight = -9999.f;
	sm_WaveEndsDetectionPointHeight = -9999.f;
	sm_LastWaveStartDetectionPointHeight = -9999.f;
	sm_LastWaveBreaksDetectionPointHeight = -9999.f;
	sm_LastRecedeEndDetectionPointHeight = -9999.f; 

	sm_CurrentOceanDirection.Zero();
	sm_WaveStartDetectionPoint.Zero();
	sm_WaveBreaksDetectionPoint.Zero();
	sm_WaveEndsDetectionPoint.Zero();

	sm_ClosestShore = NULL;

	sm_PrevNode = VEC3_ZERO;
	sm_ClosestNode = VEC3_ZERO;
	sm_NextNode = VEC3_ZERO;

	sm_ClosestDistToShore = LARGE_FLOAT;
	sm_DistanceToShore = 0.f;
	sm_SqdDistanceIntoWater = 0.f;
	sm_ClosestNodeIdx = 0;
	sm_PrevNodeIdx = 0;
	sm_NextNodeIdx = 0;

	sm_ProyectionSmoother.Init(0.01f,0.01f,0.f,1000.f);
	sm_WidthSmoother.Init(0.01f,0.01f,0.f,1000.f);
	sm_WidthSmoother.CalculateValue(0.f);
}
// ----------------------------------------------------------------
void audShoreLineOcean::ShutDownClass()
{
	for (u8 i = 0; i < NumWaterSoundPos; i ++)
	{
		if(sm_Sounds[i])
		{ 
			sm_Sounds[i]->StopAndForget();
		}
		if(sm_OceanWaveStartSound[i])
		{
			sm_OceanWaveStartSound[i]->StopAndForget();
		}
		if(sm_OceanWaveBreakSound[i])
		{
			sm_OceanWaveBreakSound[i]->StopAndForget();
		}
		if(sm_OceanWaveEndSound[i])
		{
			sm_OceanWaveEndSound[i]->StopAndForget();
		}
		if(sm_OceanWaveRecedeSound[i])
		{
			sm_OceanWaveRecedeSound[i]->StopAndForget();
		}
		sm_CurrentShore[i] = NULL;
	}
	sm_ClosestShore = NULL;
}
// ----------------------------------------------------------------
// Load shoreline data from a file
// ----------------------------------------------------------------
void audShoreLineOcean::Load(BANK_ONLY(const ShoreLineAudioSettings *settings))
{
	audShoreLine::Load(BANK_ONLY(settings));

	Vector2 center = Vector2(GetOceanSettings()->ActivationBox.Center.X,GetOceanSettings()->ActivationBox.Center.Y);
	m_ActivationBox = fwRect(center,GetOceanSettings()->ActivationBox.Size.Width/2.f,GetOceanSettings()->ActivationBox.Size.Height/2.f);
	BANK_ONLY(m_RotAngle = GetOceanSettings()->ActivationBox.RotationAngle;)
#if __BANK
	m_Width = GetOceanSettings()->ActivationBox.Size.Width;
	m_Height = GetOceanSettings()->ActivationBox.Size.Height;
#endif

}	
// ----------------------------------------------------------------
// Update the shoreline activation
// ----------------------------------------------------------------
bool audShoreLineOcean::UpdateActivation()
{
	if(audShoreLine::UpdateActivation())
	{
		if(!m_IsActive)
		{
			m_IsActive = true;
			m_JustActivated = true;
		}
		ComputeClosestShore();
		return true;
	}
	m_IsActive = false;
	m_DistanceToShore = LARGE_FLOAT;
	if(IsClosestShore())
	{
		StopSounds();
	}
	return false;
}
// ----------------------------------------------------------------
void audShoreLineOcean::ComputeClosestShore(Vec3V position)
{
	// Look for the closest point to the player.
	Vector3 pos2D = VEC3V_TO_VECTOR3(position);
	pos2D.z = 0.0f; 
	// Look for the closest point
	for(u8 i = 0; i < GetOceanSettings()->numShoreLinePoints; i++)
	{
		const f32 dist2 = (Vector3(GetOceanSettings()->ShoreLinePoints[i].X,GetOceanSettings()->ShoreLinePoints[i].Y,0.f)-pos2D).Mag2();
		if(dist2 < sm_ClosestDistToShore)
		{
			sm_ClosestDistToShore = dist2;
			sm_ClosestNodeIdx = i;
			sm_ClosestShore = this;
		}
	}
}
// ----------------------------------------------------------------
void audShoreLineOcean::ComputeClosestShore()
{
	ComputeClosestShore(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition());	
}
// ----------------------------------------------------------------
bool audShoreLineOcean::IsFirstShoreLine()
{
	return sm_ClosestShore == sm_ClosestShore->FindFirstReference();
}
// ----------------------------------------------------------------
audShoreLineOcean* audShoreLineOcean::FindFirstReference()
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
audShoreLineOcean* audShoreLineOcean::FindLastReference()
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
bool audShoreLineOcean::IsClosestShore()
{
	return sm_ClosestShore == this;
}
// ----------------------------------------------------------------
// Update the shoreline 
// ----------------------------------------------------------------
void audShoreLineOcean::ComputeShorelineNodesInfo()
{
	const ShoreLineOceanAudioSettings* settings = GetOceanSettings();
	if(naVerifyf(settings, "Failed getting ocean audio settings"))
	{
		// Calculate closest point and width since we have the closest index already.
		sm_ClosestNode = Vector3(settings->ShoreLinePoints[sm_ClosestNodeIdx].X,settings->ShoreLinePoints[sm_ClosestNodeIdx].Y,sm_WaterHeights[ClosestIndex]);
		// Initialize prev and next point to be the closest one in case we fail getting them.
		sm_NextNode = sm_ClosestNode;
		sm_NextNodeIdx = sm_ClosestNodeIdx;  

		sm_PrevNode = sm_ClosestNode;
		sm_PrevNodeIdx = sm_ClosestNodeIdx;

		// Start computing prev and next nodes.
		sm_NextNodeIdx = (sm_ClosestNodeIdx + 1);
		sm_PrevNodeIdx = (sm_ClosestNodeIdx - 1);
		// Inside the current shore.
		if(sm_ClosestNodeIdx > 0 && sm_ClosestNodeIdx < settings->numShoreLinePoints -1)
		{
			// Next node info
			if(naVerifyf(sm_NextNodeIdx < settings->numShoreLinePoints,"Next node idx out of bounds" ))
			{
				sm_NextNode = Vector3(settings->ShoreLinePoints[sm_NextNodeIdx].X,settings->ShoreLinePoints[sm_NextNodeIdx].Y,sm_WaterHeights[NextIndex]);
			}
			// Prev node info
			if(naVerifyf(sm_PrevNodeIdx < settings->numShoreLinePoints,"Prev node idx out of bounds" ))
			{
				sm_PrevNode = Vector3(settings->ShoreLinePoints[sm_PrevNodeIdx].X,settings->ShoreLinePoints[sm_PrevNodeIdx].Y,sm_WaterHeights[PrevIndex]);
			}
		}
		// Right at the beginning, we need to check for the prev shore if any.
		else if ( sm_ClosestNodeIdx == 0)
		{
			// Next node info
			if(naVerifyf(sm_NextNodeIdx < settings->numShoreLinePoints,"Next node idx out of bounds" ))
			{
				sm_NextNode = Vector3(settings->ShoreLinePoints[sm_NextNodeIdx].X,settings->ShoreLinePoints[sm_NextNodeIdx].Y,sm_WaterHeights[NextIndex]);
			}
			// Prev node info
			const audShoreLineOcean *prevShoreLine = sm_ClosestShore->GetPrevOceanReference();
			if( prevShoreLine && prevShoreLine->GetOceanSettings())
			{
				// We have got a prev shoreline, set the prev node to be the last (of the prev shoreline).
				sm_PrevNode =  Vector3(prevShoreLine->GetOceanSettings()->ShoreLinePoints[prevShoreLine->GetOceanSettings()->numShoreLinePoints -1].X
					,prevShoreLine->GetOceanSettings()->ShoreLinePoints[prevShoreLine->GetOceanSettings()->numShoreLinePoints - 1].Y
					,sm_ClosestNode.GetZ());
				sm_PrevNodeIdx = prevShoreLine->GetOceanSettings()->numShoreLinePoints - 1;
			}
			else
			{
				// Since we don't have a prev shore, leave the prev node as the closest (0) and move closest and new to make room.
				// This is a bit funky, but it will allow us the stop the PrevSound position at the beginning of the shore.
				naAssertf(sm_ClosestNodeIdx == 0, "The closest node should be 0 for this to happen");
				sm_PrevNode =  sm_ClosestNode;
				sm_PrevNodeIdx = sm_ClosestNodeIdx;
				sm_ClosestNodeIdx ++;
				if(naVerifyf(sm_ClosestNodeIdx < settings->numShoreLinePoints,"closest node idx out of bounds" ))
				{
					sm_ClosestNode = Vector3(settings->ShoreLinePoints[sm_ClosestNodeIdx].X,settings->ShoreLinePoints[sm_ClosestNodeIdx].Y,sm_WaterHeights[ClosestIndex]);
					sm_NextNodeIdx = sm_ClosestNodeIdx + 1;
					if(naVerifyf(sm_NextNodeIdx < settings->numShoreLinePoints,"Next node idx out of bounds" ))
					{
						sm_NextNode = Vector3(settings->ShoreLinePoints[sm_NextNodeIdx].X,settings->ShoreLinePoints[sm_NextNodeIdx].Y,sm_WaterHeights[NextIndex]);
					}
				}
			}

		}
		// Right at the end, we need to check for the next shore if any.
		else if (sm_ClosestNodeIdx < settings->numShoreLinePoints)
		{
			// Next node info
			const audShoreLineOcean *nextShoreLine = sm_ClosestShore->GetNextOceanReference();
			if( nextShoreLine && nextShoreLine->GetOceanSettings())
			{			
				sm_NextNode = Vector3(nextShoreLine->GetOceanSettings()->ShoreLinePoints[0].X
					,nextShoreLine->GetOceanSettings()->ShoreLinePoints[0].Y
					,sm_ClosestNode.GetZ());
				sm_NextNodeIdx = 0;
			}
			else
			{
				// Since we don't have a next shore, leave the next node as the closest (settings->numShoreLinePoints - 1) and move closest and prev to make room.
				// This is a bit funky, but it will allow us the stop the NextSound position at the end of the shore.
				naAssertf(sm_ClosestNodeIdx == (settings->numShoreLinePoints - 1), "The closest node should be %u for this to happen",(settings->numShoreLinePoints  - 1));
				sm_NextNode =  sm_ClosestNode;
				sm_NextNodeIdx = sm_ClosestNodeIdx;
				sm_ClosestNodeIdx --;
				if(naVerifyf(sm_ClosestNodeIdx < settings->numShoreLinePoints,"closest node idx out of bounds" ))
				{
					sm_ClosestNode = Vector3(settings->ShoreLinePoints[sm_ClosestNodeIdx].X,settings->ShoreLinePoints[sm_ClosestNodeIdx].Y,sm_WaterHeights[ClosestIndex]);
					sm_PrevNodeIdx = sm_ClosestNodeIdx - 1;
					if(naVerifyf(sm_PrevNodeIdx < settings->numShoreLinePoints,"Prev node idx out of bounds" ))
					{
						sm_PrevNode = Vector3(settings->ShoreLinePoints[sm_PrevNodeIdx].X,settings->ShoreLinePoints[sm_PrevNodeIdx].Y,sm_WaterHeights[PrevIndex]);
					}
				}
			}
			// Prev node info
			if(naVerifyf(sm_PrevNodeIdx < settings->numShoreLinePoints,"Prev node idx out of bounds" ))
			{
				sm_PrevNode = Vector3(settings->ShoreLinePoints[sm_PrevNodeIdx].X,settings->ShoreLinePoints[sm_PrevNodeIdx].Y,sm_WaterHeights[PrevIndex]);
			}
		}
	}
}
// ----------------------------------------------------------------
void audShoreLineOcean::CalculatePoints(Vector3 &centrePoint,Vector3 &leftPoint,Vector3 &rightPoint, const u8 /*closestRiverIdx*/)
{
	if(naVerifyf(IsClosestShore(), "We want to calculate the points only for the closest Ocean, something went wrong"))
	{
		ComputeShorelineNodesInfo();
		// First of all get the direction to the next and prev node.
		Vector3 oceanDirectionToNextNode,oceanDirectionFromPrevNode;

		oceanDirectionToNextNode = sm_NextNode	- sm_ClosestNode;
		oceanDirectionToNextNode.Normalize();

		oceanDirectionFromPrevNode = sm_ClosestNode - sm_PrevNode;
		oceanDirectionFromPrevNode.Normalize();

		Vector3 pos = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition());
		pos.z = sm_ClosestNode.z;
		Vector3 dirToListener =  pos - sm_ClosestNode;

		CalculateCentrePoint(sm_ClosestNode,sm_NextNode,sm_PrevNode,oceanDirectionToNextNode,oceanDirectionFromPrevNode,pos,dirToListener,centrePoint);
		pos = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition());
		sm_SqdDistanceIntoWater = dirToListener.Mag2();
		dirToListener = pos - centrePoint;
		Vector3 closestToCentre = sm_ClosestNode - centrePoint;
		closestToCentre.Normalize();
		f32 nextDot = fabs(oceanDirectionToNextNode.Dot(closestToCentre));
		f32 prevDot = fabs(oceanDirectionFromPrevNode.Dot(closestToCentre));

		f32 ratio = 1.f;
		sm_CurrentShore[Centre] = sm_ClosestShore;
		if (nextDot > prevDot)
		{
			// Interpolate the width at the centre point
			ratio = 1.f - fabs(((sm_NextNode - centrePoint).Mag() / (sm_NextNode - sm_ClosestNode).Mag()));
		}
		else 
		{
			// Interpolate the width at the centre point
			ratio = 1.f - fabs((sm_ClosestNode - centrePoint).Mag() / (sm_ClosestNode - sm_PrevNode).Mag());
		}
		f32 distanceToCentre = fabs(dirToListener.Mag());
		sm_DistanceToShore = distanceToCentre;
		m_DistanceToShore = distanceToCentre * distanceToCentre;
		if(sm_DistanceToShore > sm_DistanceThresholdToStopSounds)
		{
			m_IsActive = false;
			if(IsClosestShore())
			{
				StopSounds();
			}
			return;
		}
		f32 distanceToMove = sm_DistToNodeSeparation.CalculateRescaledValue(sm_InnerDistance,sm_OutterDistance,0.f,sm_MaxDistanceToShore,distanceToCentre);

		CalculateLeftPoint(centrePoint,oceanDirectionToNextNode,oceanDirectionFromPrevNode,leftPoint, distanceToMove);
		CalculateRightPoint(centrePoint,oceanDirectionToNextNode,oceanDirectionFromPrevNode,rightPoint, distanceToMove);
		CalculateDetectionPoints(centrePoint);
		
#if __BANK 
		if(sm_DrawWaterBehaviour)
		{
			//Draw maths that calculates the point 
			grcDebugDraw::Line(sm_ClosestNode, sm_ClosestNode + dirToListener, Color_red );
			grcDebugDraw::Sphere(centrePoint + Vector3( 0.f,0.f,1.f),0.3f,Color_green);
			char txt[64];
			formatf(txt, "distanceToCentre %f",distanceToCentre);
			grcDebugDraw::Text(centrePoint + Vector3( 0.f,0.f,1.3f),Color_white,txt);
			grcDebugDraw::Sphere(leftPoint + Vector3( 0.f,0.f,1.f),0.3f,Color_green);
			grcDebugDraw::Sphere(rightPoint + Vector3( 0.f,0.f,1.f),0.3f,Color_green);

			grcDebugDraw::Sphere(sm_WaveEndsDetectionPoint,0.3f,(sm_OceanWaveRecedeSound[Left]) ? Color_green :((sm_OceanWaveEndSound[Left]) ? Color_orange : Color_yellow));
			grcDebugDraw::Sphere(sm_WaveStartDetectionPoint,0.3f,(sm_OceanWaveStartSound[Left]) ? Color_green :Color_yellow);
			grcDebugDraw::Sphere(sm_WaveBreaksDetectionPoint,0.3f,(sm_OceanWaveBreakSound[Left]) ? Color_green :Color_yellow);

			formatf(txt,"Waves height %f",sm_WaveStartDetectionPoint.GetZ());
			grcDebugDraw::Text(sm_WaveStartDetectionPoint,Color_white,txt);
			formatf(txt,"Waves height %f",sm_WaveBreaksDetectionPoint.GetZ());
			grcDebugDraw::Text(sm_WaveBreaksDetectionPoint,Color_white,txt);
			formatf(txt,"Recede height %f",sm_WaveEndsDetectionPoint.GetZ());
			grcDebugDraw::Text(sm_WaveEndsDetectionPoint,Color_white,txt);
			formatf(txt,"Distance to shore %f",sm_ClosestDistToShore);
			grcDebugDraw::AddDebugOutput(Color_white,txt);
		}
#endif
	}
}
// ----------------------------------------------------------------
void audShoreLineOcean::CalculateDetectionPoints(const Vector3 &centrePoint)
{
	Vector3 directionIntoOcean;
	switch(GetOceanSettings()->OceanDirection)
	{
	case AUD_OCEAN_DIR_NORTH:
		directionIntoOcean = Vector3(0.0f,1.f,0.0f);
		break;
	case AUD_OCEAN_DIR_NORTH_EAST:
		directionIntoOcean = Vector3(0.707f,0.707f,0.0f);
		break;
	case AUD_OCEAN_DIR_EAST:
		directionIntoOcean = Vector3(1.f,0.0f,0.0f);
		break;
	case AUD_OCEAN_DIR_SOUTH_EAST:
		directionIntoOcean = Vector3(0.707f,-0.707f,0.0f);
		break;
	case AUD_OCEAN_DIR_SOUTH:
		directionIntoOcean = Vector3(0.0f,-1.f,0.0f);
		break;
	case AUD_OCEAN_DIR_SOUTH_WEST:
		directionIntoOcean = Vector3(-0.707f,-0.707f,0.0f);
		break;
	case AUD_OCEAN_DIR_WEST:
		directionIntoOcean = Vector3(-1.f,0.0f,0.0f);
		break;
	case AUD_OCEAN_DIR_NORTH_WEST:
		directionIntoOcean = Vector3(-0.707f,0.707f,0.0f);
		break;
	default: 
		naAssertf(false, "wrong ocean direction");
	}
	//directionIntoOcean.RotateZ(g_Rot * DtoR);
	sm_WaveStartDetectionPoint = Vector3(centrePoint.GetX(),centrePoint.GetY(),sm_WaveStartDetectionPointHeight) + directionIntoOcean * GetOceanSettings()->WaveStartDPDistance;
	sm_WaveBreaksDetectionPoint = Vector3(centrePoint.GetX(),centrePoint.GetY(),sm_WaveBreaksDetectionPointHeight) + directionIntoOcean * GetOceanSettings()->WaveBreaksDPDistance;
	sm_WaveEndsDetectionPoint = Vector3(centrePoint.GetX(),centrePoint.GetY(),sm_WaveEndsDetectionPointHeight) + directionIntoOcean * GetOceanSettings()->WaveEndDPDistance;

}
// ----------------------------------------------------------------
void audShoreLineOcean::CalculateLeftPoint(const Vector3& centrePoint,const Vector3 &lakeDirectionToNextNode, const Vector3 &lakeDirectionFromPrevNode
										   ,Vector3 &leftPoint,const f32 distance, const u8 /*closestRiverIdx*/)
{
	//Start from the centre point
	leftPoint = centrePoint;

	// Square the distance for performance purposes.
	f32 distanceToMove = distance * distance;

	Vector3 currentDirection; 
	f32 currentDistance = 0.f;

	// Compute in which (prev or next) direction the centre point is.
	u8 pointIdx = sm_ClosestNodeIdx;
	Vector3 closestToCentre = sm_ClosestNode - centrePoint;
	closestToCentre.Normalize();
	f32 nextDot = fabs(lakeDirectionToNextNode.Dot(closestToCentre));
	f32 prevDot = fabs(lakeDirectionFromPrevNode.Dot(closestToCentre));

	audShoreLineOcean *currentShoreLine = sm_ClosestShore;
	if (nextDot > prevDot)
	{
		currentDirection = lakeDirectionToNextNode;
		currentDistance = (sm_NextNode - centrePoint).Mag2();
		pointIdx = sm_NextNodeIdx;
		// if sm_NextNodeIdx is 0 and we have a next reference, our current shore should be the next.
		if (pointIdx == 0 && sm_ClosestShore->GetNextOceanReference())
		{
			currentShoreLine = sm_ClosestShore->GetNextOceanReference();
		}
	}
	else 
	{
		currentDirection = lakeDirectionFromPrevNode;
		currentDistance = (sm_ClosestNode - centrePoint).Mag2();
		pointIdx = sm_ClosestNodeIdx;
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
			if( currentShoreLine && currentShoreLine->GetOceanSettings())
			{	
				leftEndPoint = Vector3(currentShoreLine->GetOceanSettings()->ShoreLinePoints[0].X
					,currentShoreLine->GetOceanSettings()->ShoreLinePoints[0].Y
					,sm_WaterHeights[NextIndex]);
				audShoreLineOcean* prevShoreline = currentShoreLine->GetPrevOceanReference();
				if( prevShoreline && prevShoreline->GetOceanSettings())
				{			
					const u8 index = prevShoreline->GetOceanSettings()->numShoreLinePoints - 1;
					leftStartPoint = Vector3(prevShoreline->GetOceanSettings()->ShoreLinePoints[index].X
						,prevShoreline->GetOceanSettings()->ShoreLinePoints[index].Y
						,sm_WaterHeights[NextIndex]);
				}
			}
		}
		else
		{
			const ShoreLineOceanAudioSettings *settings = currentShoreLine->GetOceanSettings();
			if(settings && naVerifyf(pointIdx < settings->numShoreLinePoints,"Index out of bounds when computing left point, current index :%d",pointIdx))
			{
				leftPoint =  Vector3(settings->ShoreLinePoints[pointIdx].X,settings->ShoreLinePoints[pointIdx].Y,sm_WaterHeights[NextIndex]);
				distanceToMove -= currentDistance;
				u8 followingNodeIdx = (pointIdx + 1);
				Vector3 followingPoint;
				if(followingNodeIdx < settings->numShoreLinePoints)
				{
					followingPoint = Vector3(settings->ShoreLinePoints[followingNodeIdx].X,settings->ShoreLinePoints[followingNodeIdx].Y,sm_WaterHeights[NextIndex]);
				}
				else
				{
					audShoreLineOcean* nextShoreline = currentShoreLine->GetNextOceanReference();
					if( nextShoreline && nextShoreline->GetOceanSettings())
					{			
						followingPoint = Vector3(nextShoreline->GetOceanSettings()->ShoreLinePoints[0].X
							,nextShoreline->GetOceanSettings()->ShoreLinePoints[0].Y
							,sm_WaterHeights[NextIndex]);
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
	// CurrentShoreLine has the next reference to the closest one,
	sm_CurrentShore[Left] = currentShoreLine;
}
// ----------------------------------------------------------------
void audShoreLineOcean::CalculateRightPoint(const Vector3& centrePoint,const Vector3 &lakeDirectionToNextNode, const Vector3 &lakeDirectionFromPrevNode
											,Vector3 &rightPoint,const f32 distance, const u8 /*closestRiverIdx*/)
{
	//Start from the centre point
	rightPoint = centrePoint;

	// Square the distance for performance purposes.
	f32 distanceToMove = distance * distance;

	Vector3 currentDirection; 
	f32 currentDistance = 0.f;

	// Compute in which (prev or next) direction the centre point is.
	u8 pointIdx = sm_ClosestNodeIdx;
	Vector3 closestToCentre = sm_ClosestNode - centrePoint;
	closestToCentre.Normalize();
	f32 nextDot = fabs(lakeDirectionToNextNode.Dot(closestToCentre));
	f32 prevDot = fabs(lakeDirectionFromPrevNode.Dot(closestToCentre));

	audShoreLineOcean *currentShoreLine = sm_ClosestShore;
	if (nextDot > prevDot)
	{
		currentDirection = lakeDirectionToNextNode;
		currentDistance = (sm_ClosestNode - centrePoint).Mag2();
		pointIdx = sm_ClosestNodeIdx;
	}
	else 
	{
		currentDirection = lakeDirectionFromPrevNode;
		currentDistance = (sm_PrevNode - centrePoint).Mag2();
		pointIdx = sm_PrevNodeIdx;
		// if sm_PrevNodeIdx is the last node of the prev reference our current shoreline should be the prev
		if(sm_ClosestNodeIdx == 0 && sm_ClosestShore->GetPrevOceanReference() && sm_ClosestShore->GetPrevOceanReference()->GetOceanSettings()
			&& (pointIdx == (sm_ClosestShore->GetPrevOceanReference()->GetOceanSettings()->numShoreLinePoints - 1) ))
		{
			currentShoreLine = sm_ClosestShore->GetPrevOceanReference();
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
			// To compute the ratio for the left point.
			if( currentShoreLine && currentShoreLine->GetOceanSettings())
			{	
				const u8 index = currentShoreLine->GetOceanSettings()->numShoreLinePoints - 1;
				rightStartPoint = Vector3(currentShoreLine->GetOceanSettings()->ShoreLinePoints[index].X
					,currentShoreLine->GetOceanSettings()->ShoreLinePoints[index].Y
					,sm_WaterHeights[PrevIndex]);
				audShoreLineOcean* nextShoreLine = currentShoreLine->GetNextOceanReference();
				if( nextShoreLine  && nextShoreLine->GetOceanSettings())
				{			
					rightEndPoint = Vector3(nextShoreLine->GetOceanSettings()->ShoreLinePoints[0].X
						,nextShoreLine->GetOceanSettings()->ShoreLinePoints[0].Y
						,sm_WaterHeights[PrevIndex]);
				}
			}
		}
		else
		{
			const ShoreLineOceanAudioSettings *settings = currentShoreLine->GetOceanSettings();
			if(settings && naVerifyf(pointIdx < settings->numShoreLinePoints,"Index out of bounds when computing right point, current index :%d",pointIdx))
			{
				rightPoint =  Vector3(settings->ShoreLinePoints[pointIdx].X,settings->ShoreLinePoints[pointIdx].Y,sm_WaterHeights[PrevIndex]);
				distanceToMove -= currentDistance;
				s8 prevNodeIdx = (pointIdx - 1);
				Vector3 previousPoint; 
				// if prevNodeIdx still inside the bounds, use the current shoreline.
				if(prevNodeIdx >= 0)
				{
					previousPoint = Vector3(settings->ShoreLinePoints[prevNodeIdx].X
						,settings->ShoreLinePoints[prevNodeIdx].Y,sm_WaterHeights[PrevIndex]);
				}
				// we are right at the beginning, check if we have a prev reference.
				else
				{
					audShoreLineOcean* prevShoreLine = currentShoreLine->GetPrevOceanReference();
					if( prevShoreLine && prevShoreLine->GetOceanSettings())
					{
						prevNodeIdx = prevShoreLine->GetOceanSettings()->numShoreLinePoints - 1;

						previousPoint =  Vector3(prevShoreLine->GetOceanSettings()->ShoreLinePoints[prevNodeIdx].X
							,prevShoreLine->GetOceanSettings()->ShoreLinePoints[prevNodeIdx].Y
							,sm_WaterHeights[PrevIndex]);
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
	sm_CurrentShore[Right] = currentShoreLine;
}

// ----------------------------------------------------------------
void audShoreLineOcean::Update(const f32 /*timeElapsedS*/)
{
	if(IsClosestShore())
	{
		if(m_IsActive)  
		{
			u8 nextDesireNode = (sm_LastClosestNodeIdx + 1) % GetOceanSettings()->numShoreLinePoints;
			u8 prevDesireNode = sm_LastClosestNodeIdx -1; 
			if(sm_LastClosestNodeIdx == 0)
			{
				prevDesireNode = (u8)(GetOceanSettings()->numShoreLinePoints - 1) ;
			}
			if ((sm_ClosestNodeIdx != sm_LastClosestNodeIdx) && ((sm_ClosestNodeIdx != nextDesireNode) && (sm_ClosestNodeIdx != prevDesireNode)))
			{
				StopSounds();
			}
			sm_LastClosestNodeIdx = sm_ClosestNodeIdx;
				
			Vector3 centrePoint,leftPoint,rightPoint;
			CalculatePoints(centrePoint,leftPoint,rightPoint);
			m_CentrePoint = centrePoint;
			if(m_IsActive)
			{
				UpdateOceanBeachSound(centrePoint,leftPoint,rightPoint);
				UpdateWaveStarts(leftPoint,rightPoint);
				UpdateWaveBreaks(leftPoint,rightPoint);
				UpdateWaveEndsAndRecede(leftPoint,rightPoint);
			}
		}
		
		sm_LastWaveStartDetectionPointHeight = sm_WaveStartDetectionPoint.GetZ();
		sm_LastWaveBreaksDetectionPointHeight = sm_WaveBreaksDetectionPoint.GetZ();
		sm_LastRecedeEndDetectionPointHeight = sm_WaveEndsDetectionPoint.GetZ();
	}
}
// ----------------------------------------------------------------
void audShoreLineOcean::UpdateWaveStarts(const Vector3 &leftPoint,const Vector3 &rightPoint)
{
	// Check if the big waves detects a wave : 
	if(sm_LastWaveStartDetectionPointHeight < sm_WaveStartDetectionPoint.GetZ())
	{
		if(sm_WaveStartDetectionPoint.GetZ() > GetOceanSettings()->WaveStartHeight)
		{
			audMetadataRef leftSound,rightSound = g_NullSoundRef; 
			leftSound = sm_OceanSoundSet.Find(ATSTRINGHASH("WAVE_START_LEFT", 0xF8DD7F));
			rightSound = sm_OceanSoundSet.Find(ATSTRINGHASH("WAVE_START_RIGHT", 0xC46DE00D));
			// Play a big wave
			if(!sm_OceanWaveStartSound[Left]  && !sm_OceanWaveStartSound[Right])
			{
				audSoundInitParams leftInitParams;
				leftInitParams.Position = leftPoint;
				g_AmbientAudioEntity.CreateAndPlaySound_Persistent(leftSound,&sm_OceanWaveStartSound[Left],&leftInitParams);
				//audSoundInitParams centreInitParams;
				//centreInitParams.Position = centrePoint;
				//g_AmbientAudioEntity.CreateAndPlaySound_Persistent(sm_OceanSounds.Find(ATSTRINGHASH("WAVES_02_CENTRE", 0xE032D90B)),&m_OceanWavesBigCentreSound,&centreInitParams);
				audSoundInitParams rightInitParams;
				rightInitParams.Position = rightPoint;
				g_AmbientAudioEntity.CreateAndPlaySound_Persistent(rightSound,&sm_OceanWaveStartSound[Right],&rightInitParams);
			}
		}
	}
	if(sm_OceanWaveStartSound[Left] )    
	{
		sm_OceanWaveStartSound[Left]->FindAndSetVariableValue(ATSTRINGHASH("distanceToShore", 0x2B21E5E5),sm_DistanceToShore);
		sm_OceanWaveStartSound[Left]->SetRequestedPosition(leftPoint);
	}
	if(sm_OceanWaveStartSound[Right] )    
	{
		sm_OceanWaveStartSound[Right]->FindAndSetVariableValue(ATSTRINGHASH("distanceToShore", 0x2B21E5E5),sm_DistanceToShore);
		sm_OceanWaveStartSound[Right]->SetRequestedPosition(rightPoint);
	}
}
// ----------------------------------------------------------------
void audShoreLineOcean::UpdateWaveBreaks(const Vector3 &leftPoint,const Vector3 &rightPoint)
{
 
 	if(sm_LastWaveBreaksDetectionPointHeight < sm_WaveBreaksDetectionPoint.GetZ())
 	{
 		if(sm_WaveBreaksDetectionPoint.GetZ() > GetOceanSettings()->WaveBreaksHeight)
 		{
 			audMetadataRef leftSound,rightSound = g_NullSoundRef; 
 			leftSound = sm_OceanSoundSet.Find(ATSTRINGHASH("WAVE_BREAK_LEFT", 0x4EAA94CF));
 			rightSound = sm_OceanSoundSet.Find(ATSTRINGHASH("WAVE_BREAK_RIGHT", 0x4D4E33A4));
 			if(!sm_OceanWaveBreakSound[Left] && !sm_OceanWaveBreakSound[Right])
 			{
 				audSoundInitParams leftInitParams;
 				leftInitParams.Position = leftPoint;
 				g_AmbientAudioEntity.CreateAndPlaySound_Persistent(leftSound,&sm_OceanWaveBreakSound[Left],&leftInitParams);
 				audSoundInitParams rightInitParams;
 				rightInitParams.Position = rightPoint;
 				g_AmbientAudioEntity.CreateAndPlaySound_Persistent(rightSound,&sm_OceanWaveBreakSound[Right],&rightInitParams);
 			}
 		}
 	}
 	if(sm_OceanWaveBreakSound[Left] )    
 	{
		sm_OceanWaveBreakSound[Left]->FindAndSetVariableValue(ATSTRINGHASH("distanceToShore", 0x2B21E5E5),sm_DistanceToShore);
		sm_OceanWaveBreakSound[Left]->SetRequestedPosition(leftPoint);
 	}
 	if(sm_OceanWaveBreakSound[Right] )    
 	{
		sm_OceanWaveBreakSound[Right]->FindAndSetVariableValue(ATSTRINGHASH("distanceToShore", 0x2B21E5E5),sm_DistanceToShore);
		sm_OceanWaveBreakSound[Right]->SetRequestedPosition(rightPoint);
 	}

}
// ----------------------------------------------------------------
void audShoreLineOcean::UpdateWaveEndsAndRecede(const Vector3 &leftPoint,const Vector3 &rightPoint)
{
 	// Check if the big waves detects a wave : 
 	if(sm_LastRecedeEndDetectionPointHeight < sm_WaveEndsDetectionPoint.GetZ())
 	{
 		if(sm_WaveEndsDetectionPoint.GetZ() > GetOceanSettings()->WaveEndHeight)
 		{
 			for (u32 i = 0; i < NumWaterSoundPos;i++)
 			{
 				if(sm_OceanWaveRecedeSound[i])
 				{
 					sm_OceanWaveRecedeSound[i]->StopAndForget();
 				}
 			}
 			audMetadataRef leftSound,rightSound = g_NullSoundRef; 
 			leftSound = sm_OceanSoundSet.Find(ATSTRINGHASH("WAVE_END_LEFT", 0xD6608543));
 			rightSound = sm_OceanSoundSet.Find(ATSTRINGHASH("WAVE_END_RIGHT", 0xCD83919A));
 			if(!sm_OceanWaveEndSound[Left] && !sm_OceanWaveEndSound[Right])
 			{
 				audSoundInitParams leftInitParams;
 				leftInitParams.Position = leftPoint;
 				g_AmbientAudioEntity.CreateAndPlaySound_Persistent(leftSound,&sm_OceanWaveEndSound[Left],&leftInitParams);
 				audSoundInitParams rightInitParams;
 				rightInitParams.Position = rightPoint;
 				g_AmbientAudioEntity.CreateAndPlaySound_Persistent(rightSound,&sm_OceanWaveEndSound[Right],&rightInitParams);
 			}
 		}
 	}
 	else if (GetOceanSettings()->OceanType == AUD_OCEAN_TYPE_BEACH)
 	{
 		if(sm_WaveEndsDetectionPoint.GetZ() < GetOceanSettings()->RecedeHeight)
 		{ 
 			// Play the recede
 			if(!sm_OceanWaveRecedeSound[Left] && !sm_OceanWaveRecedeSound[Right])
 			{
 				audSoundInitParams leftInitParams;
 				leftInitParams.Position = leftPoint;
 				g_AmbientAudioEntity.CreateAndPlaySound_Persistent(sm_OceanSoundSet.Find(ATSTRINGHASH("WAVES_RECEDE_LEFT", 0xACBCB7B9)),&sm_OceanWaveRecedeSound[Left],&leftInitParams);
 				audSoundInitParams rightInitParams;
 				rightInitParams.Position = rightPoint;
 				g_AmbientAudioEntity.CreateAndPlaySound_Persistent(sm_OceanSoundSet.Find(ATSTRINGHASH("WAVES_RECEDE_RIGHT", 0x2337825A)),&sm_OceanWaveRecedeSound[Right],&rightInitParams);
 			}
 		}
 	}
 
 	if(sm_OceanWaveEndSound[Left] )    
 	{
		sm_OceanWaveEndSound[Left]->FindAndSetVariableValue(ATSTRINGHASH("distanceToShore", 0x2B21E5E5),sm_DistanceToShore);
		sm_OceanWaveEndSound[Left]->SetRequestedPosition(leftPoint);
 	}
 	if(sm_OceanWaveEndSound[Right] )    
 	{
		sm_OceanWaveEndSound[Right]->FindAndSetVariableValue(ATSTRINGHASH("distanceToShore", 0x2B21E5E5),sm_DistanceToShore);
		sm_OceanWaveEndSound[Right]->SetRequestedPosition(rightPoint);
 	}
 	if(sm_OceanWaveRecedeSound[Left] )    
 	{
		sm_OceanWaveRecedeSound[Left]->FindAndSetVariableValue(ATSTRINGHASH("distanceToShore", 0x2B21E5E5),sm_DistanceToShore);
		sm_OceanWaveRecedeSound[Left]->SetRequestedPosition(leftPoint);
 	}
 	if(sm_OceanWaveRecedeSound[Right] )    
 	{
		sm_OceanWaveRecedeSound[Right]->FindAndSetVariableValue(ATSTRINGHASH("distanceToShore", 0x2B21E5E5),sm_DistanceToShore);
		sm_OceanWaveRecedeSound[Right]->SetRequestedPosition(rightPoint);
 	}
}
// ----------------------------------------------------------------
void audShoreLineOcean::PlayOutInTheOceanSound()
{
	if(!sm_OutInTheOceanSound)
	{
		g_AmbientAudioEntity.CreateAndPlaySound_Persistent(sm_OceanSoundSet.Find(ATSTRINGHASH("OUT_IN_OCEAN", 0xBAE52A34)),&sm_OutInTheOceanSound);
	}
}
// ----------------------------------------------------------------
void audShoreLineOcean::StopOutInTheOceanSound()
{
	if(sm_OutInTheOceanSound)
	{
		sm_OutInTheOceanSound->StopAndForget();
	}
}
// ----------------------------------------------------------------
void audShoreLineOcean::ResetDistanceToShore(bool resetClosestShoreline)
{
	sm_ClosestDistToShore = LARGE_FLOAT; 
	
	if(sm_ClosestShore && resetClosestShoreline) 
	{		
		sm_ClosestShore->StopSounds();
		sm_ClosestShore = NULL; 
	} 
}
// ----------------------------------------------------------------
void audShoreLineOcean::ComputeListenerOverOcean()
{
	sm_ListenerAboveWater =IsPointOverOceanWater(VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition()));
}
// ----------------------------------------------------------------
void audShoreLineOcean::UpdateOceanBeachSound(const Vector3 &centrePoint,const Vector3 &leftPoint,const Vector3 &rightPoint)
{
 	// Left sound.
 	audMetadataRef soundRef = g_NullSoundRef;
 	if(sm_CurrentShore[Left] && sm_CurrentShore[Left]->GetOceanSettings())
 	{
 		GetSoundsRefs(soundRef,Left);
 		UpdateOceanSound(soundRef,Left,leftPoint);
 	}
 	// Centre
 	GetSoundsRefs(soundRef,Centre);
 	UpdateOceanSound(soundRef,Centre,centrePoint);
 	// Right sound
 	if(sm_CurrentShore[Right] && sm_CurrentShore[Right]->GetOceanSettings())
 	{
 		GetSoundsRefs(soundRef,Right);
 		UpdateOceanSound(soundRef,Right,rightPoint);
 	}
#if __BANK
	
	if(sm_DrawWaterBehaviour)
	{
		char name[64];
		formatf(name,"%s",audNorthAudioEngine::GetMetadataManager().GetObjectName(sm_CurrentShore[Centre]->GetOceanSettings()->NameTableOffset,0));
		grcDebugDraw::AddDebugOutput(Color_green,name);

		char txt[256];
		formatf(txt, "Closest node %u ",sm_ClosestNodeIdx);
		grcDebugDraw::AddDebugOutput(Color_green,txt);
		if(sm_CurrentShore[Left] && sm_CurrentShore[Left]->GetOceanSettings())
		{
			formatf(name,"%s",audNorthAudioEngine::GetMetadataManager().GetObjectName(sm_CurrentShore[Left]->GetOceanSettings()->NameTableOffset,0));
		}
		else
		{
			formatf(name,"none");
		}
		formatf(txt, "LEFT: [Shore %s]",name);
		grcDebugDraw::AddDebugOutput(Color_green,txt);
		if(sm_Sounds[Left])
		{
			formatf(txt, "Sound %s  vol %f",g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectNameFromNameTableOffset(sm_Sounds[Left]->GetBaseMetadata()->NameTableOffset),sm_Sounds[Left]->GetRequestedVolume());
			grcDebugDraw::AddDebugOutput(Color_white,txt);
		}
		if(GetOceanSettings())
		{
			formatf(name,"%s",audNorthAudioEngine::GetMetadataManager().GetObjectName(GetOceanSettings()->NameTableOffset,0));
		}
		else
		{
			formatf(name,"none");
		}
		formatf(txt, "CENTRE: [Shore %s]", name);
		grcDebugDraw::AddDebugOutput(Color_green,txt);
		if(sm_Sounds[Centre])
		{
			formatf(txt, "Sound %s  vol %f",g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectNameFromNameTableOffset(sm_Sounds[Centre]->GetBaseMetadata()->NameTableOffset),sm_Sounds[Centre]->GetRequestedVolume());
			grcDebugDraw::AddDebugOutput(Color_white,txt);
		}
		if(sm_CurrentShore[Right] && sm_CurrentShore[Right]->GetOceanSettings())
		{
			formatf(name,"%s",audNorthAudioEngine::GetMetadataManager().GetObjectName(sm_CurrentShore[Right]->GetOceanSettings()->NameTableOffset,0));
		}
		else
		{
			formatf(name,"none");
		}
		formatf(txt, "RIGHT: [Shore %s] ", name);
		grcDebugDraw::AddDebugOutput(Color_green,txt);
		if(sm_Sounds[Right])
		{
			formatf(txt, "Sound %s  vol %f",g_AudioEngine.GetSoundManager().GetFactory().GetMetadataManager().GetObjectNameFromNameTableOffset(sm_Sounds[Right]->GetBaseMetadata()->NameTableOffset),sm_Sounds[Right]->GetRequestedVolume());
			grcDebugDraw::AddDebugOutput(Color_white,txt);
		}
	}
#endif
	//if(!sm_Sounds[Left])
	//{
	//	audSoundInitParams leftInitParams;
	//	leftInitParams.Position = leftPoint;
	//	g_AmbientAudioEntity.CreateAndPlaySound_Persistent(sm_OceanSounds.Find(ATSTRINGHASH("OCEAN_BEACH_LEFT", 0xE5C3A68F)),&sm_Sounds[Left],&leftInitParams);
	//}
	//if(!sm_Sounds[Centre])
	//{
	//	audSoundInitParams centreInitParams;
	//	centreInitParams.Position = centrePoint;
	//	g_AmbientAudioEntity.CreateAndPlaySound_Persistent(sm_OceanSounds.Find(ATSTRINGHASH("OCEAN_BEACH_CENTRE", 0x3F9E5B25)),&sm_Sounds[Centre],&centreInitParams);
	//}
	//if(!sm_Sounds[Right])
	//{
	//	audSoundInitParams rightInitParams;
	//	rightInitParams.Position = rightPoint;
	//	g_AmbientAudioEntity.CreateAndPlaySound_Persistent(sm_OceanSounds.Find(ATSTRINGHASH("OCEAN_BEACH_RIGHT", 0xD2B4B7FA)),&sm_Sounds[Right],&rightInitParams);
	//}
	//if(sm_Sounds[Left] )    
	//{
	//	sm_Sounds[Left]->SetRequestedPosition(leftPoint);
	//}
	//if(sm_Sounds[Centre] )    
	//{
	//	sm_Sounds[Centre]->SetRequestedPosition(centrePoint);
	//}
	//if(sm_Sounds[Right] )    
	//{
	//	sm_Sounds[Right]->SetRequestedPosition(rightPoint);
	//}
}
// ----------------------------------------------------------------
void audShoreLineOcean::UpdateOceanSound(const audMetadataRef soundRef,const u8 soundIdx,const Vector3 &position)
{
	// Play the sounds if we don't have them and update their position.
	if (!sm_Sounds[soundIdx])
	{
		audSoundInitParams initParams;
		initParams.Position = position;
		g_AmbientAudioEntity.CreateAndPlaySound_Persistent(soundRef,&sm_Sounds[soundIdx],&initParams);
	}
	if(sm_Sounds[soundIdx])    
	{
		sm_Sounds[soundIdx]->FindAndSetVariableValue(ATSTRINGHASH("distanceToShore", 0x2B21E5E5),sm_DistanceToShore);
		sm_Sounds[soundIdx]->SetRequestedVolume(0.f);
		sm_Sounds[soundIdx]->SetRequestedPosition(position);
	}
}
// ----------------------------------------------------------------
void audShoreLineOcean::GetSoundsRefs(audMetadataRef &soundRef,const u8 soundIdx)
{
	soundRef = sm_OceanSoundSet.Find(ATSTRINGHASH("OCEAN_LEFT", 0x603D93E9));
	if(soundIdx == Centre)
	{
		soundRef = sm_OceanSoundSet.Find(ATSTRINGHASH("OCEAN_CENTRE", 0x249C4527));
	}
	else if (soundIdx == Right)
	{
		soundRef = sm_OceanSoundSet.Find(ATSTRINGHASH("OCEAN_RIGHT", 0xD90448C9));
	}
}
// ----------------------------------------------------------------
bool audShoreLineOcean::IsPointOverOceanWater(const Vector3& pos) 
{
	if(sm_ClosestShore)
	{
		return sm_ClosestShore->IsPointOverWater(pos);
	}
	return false;
}
// Check if a point is over the water
// ----------------------------------------------------------------
bool audShoreLineOcean::IsPointOverWater(const Vector3& pos) const
{
	if(!GetOceanSettings())
	{
		return false;
	}

	Vector3 oceanDirectionToNextNode,oceanDirectionFromPrevNode;
	f32 closestDist2 = LARGE_FLOAT;

	Vector3 pos2D = pos;
	pos2D.z = 0.0f; 
	u32 oceanClosestNode = 0;

	// Once we have our search indices, look for the closest point
	for(s32 i = 1; i < GetOceanSettings()->numShoreLinePoints - 1; i++)
	{
		const f32 dist2 = (Vector3(GetOceanSettings()->ShoreLinePoints[i].X,GetOceanSettings()->ShoreLinePoints[i].Y,0.f)-pos2D).Mag2();
		if(dist2 < closestDist2)
		{
			closestDist2 = dist2;
			oceanClosestNode = i;
		}
	}

	// Now in closestIdx we have the closest point to the current shoreline. Get the n points to both sides to update.
	if(oceanClosestNode > 0 && oceanClosestNode < GetOceanSettings()->numShoreLinePoints - 1)
	{
		oceanDirectionToNextNode = Vector3(GetOceanSettings()->ShoreLinePoints[oceanClosestNode + 1].X,GetOceanSettings()->ShoreLinePoints[oceanClosestNode + 1].Y,0.0f)
			- Vector3(GetOceanSettings()->ShoreLinePoints[oceanClosestNode].X,GetOceanSettings()->ShoreLinePoints[oceanClosestNode].Y,0.0f);
		oceanDirectionToNextNode.Normalize();
		oceanDirectionFromPrevNode = Vector3(GetOceanSettings()->ShoreLinePoints[oceanClosestNode].X,GetOceanSettings()->ShoreLinePoints[oceanClosestNode].Y,0.0f)
			- Vector3(GetOceanSettings()->ShoreLinePoints[oceanClosestNode -1].X,GetOceanSettings()->ShoreLinePoints[oceanClosestNode - 1].Y,0.0f);
		oceanDirectionFromPrevNode.Normalize();
	}
	else if (oceanClosestNode == 0)
	{
		oceanDirectionToNextNode = Vector3(GetOceanSettings()->ShoreLinePoints[oceanClosestNode + 1].X,GetOceanSettings()->ShoreLinePoints[oceanClosestNode + 1].Y,0.0f)
			- Vector3(GetOceanSettings()->ShoreLinePoints[oceanClosestNode].X,GetOceanSettings()->ShoreLinePoints[oceanClosestNode].Y,0.0f);
		oceanDirectionToNextNode.Normalize();
		oceanDirectionFromPrevNode.Zero();
	}
	else if (oceanClosestNode == (u32)(GetOceanSettings()->numShoreLinePoints) )
	{
		oceanDirectionToNextNode.Zero();
		oceanDirectionFromPrevNode = Vector3(GetOceanSettings()->ShoreLinePoints[oceanClosestNode].X,GetOceanSettings()->ShoreLinePoints[oceanClosestNode].Y,0.0f)
			- Vector3(GetOceanSettings()->ShoreLinePoints[oceanClosestNode -1].X,GetOceanSettings()->ShoreLinePoints[oceanClosestNode - 1].Y,0.0f);
		oceanDirectionFromPrevNode.Normalize();
	}

	// Now that we now the prev and next direction calculate which one we should use
	Vector3 closestPoint = Vector3(GetOceanSettings()->ShoreLinePoints[oceanClosestNode].X,GetOceanSettings()->ShoreLinePoints[oceanClosestNode].Y,0.0f);
	Vector3 dirToPoint =  pos2D - closestPoint;
	f32 dotProduct = oceanDirectionToNextNode.Dot(dirToPoint);
	Vector3	currentOceanDirection;
	if(dotProduct >= 0.f)
	{
		currentOceanDirection = oceanDirectionToNextNode;
	}
	else
	{
		currentOceanDirection = oceanDirectionFromPrevNode;
	}

	Vector3 dirIntoSea = currentOceanDirection;
	dirIntoSea.Normalize();
	dirIntoSea.RotateZ(PI * 0.5f);

	f32 proyectionMag = dirToPoint.Dot(currentOceanDirection) ;
	Vector3 uProyection = (proyectionMag * currentOceanDirection);
	Vector3 pointInShore = pos - (dirToPoint - uProyection);

	// Test which side of the line we are
	return (pointInShore + dirIntoSea).Dist2(pos2D) > (pointInShore - dirIntoSea).Dist2(pos2D);
}
// ----------------------------------------------------------------
void audShoreLineOcean::UpdateWaterHeight(bool BANK_ONLY(updateAllHeights))
{
	if(m_Settings)
	{
		if(IsClosestShore())
		{
			// Closest index : 
			f32 waterz;
			CVfxHelper::GetWaterZ(Vec3V(GetOceanSettings()->ShoreLinePoints[sm_ClosestNodeIdx].X, GetOceanSettings()->ShoreLinePoints[sm_ClosestNodeIdx].Y, 0.f), waterz);
			sm_WaterHeights[ClosestIndex] = waterz;

			s8 nextNodeIdx = (sm_ClosestNodeIdx + 1);
			s8 prevNodeIdx = (sm_ClosestNodeIdx - 1);

			if(sm_ClosestNodeIdx > 0 && sm_ClosestNodeIdx < GetOceanSettings()->numShoreLinePoints -1)
			{
				CVfxHelper::GetWaterZ(Vec3V(GetOceanSettings()->ShoreLinePoints[nextNodeIdx].X, GetOceanSettings()->ShoreLinePoints[nextNodeIdx].Y, 0.f), waterz);
				sm_WaterHeights[NextIndex] = waterz;
				CVfxHelper::GetWaterZ(Vec3V(GetOceanSettings()->ShoreLinePoints[prevNodeIdx].X, GetOceanSettings()->ShoreLinePoints[prevNodeIdx].Y, 0.f), waterz);
				sm_WaterHeights[PrevIndex] = waterz;
			}
			else if ( sm_ClosestNodeIdx == 0)
			{
				CVfxHelper::GetWaterZ(Vec3V(GetOceanSettings()->ShoreLinePoints[nextNodeIdx].X, GetOceanSettings()->ShoreLinePoints[nextNodeIdx].Y, 0.f), waterz);
				sm_WaterHeights[NextIndex] = waterz;
				audShoreLineOcean *prevShoreLine = GetPrevOceanReference();
				if( !prevShoreLine)
				{
					prevShoreLine = FindLastReference();
					naAssertf(prevShoreLine, "Got a null when looking for the last shore, something went wrong") ;
				}
				if(prevShoreLine && prevShoreLine->GetOceanSettings())
				{
					prevNodeIdx = prevShoreLine->GetOceanSettings()->numShoreLinePoints - 1;
					CVfxHelper::GetWaterZ(Vec3V(prevShoreLine->GetOceanSettings()->ShoreLinePoints[prevNodeIdx].X, prevShoreLine->GetOceanSettings()->ShoreLinePoints[prevNodeIdx].Y, 0.f), waterz);
					sm_WaterHeights[PrevIndex] = waterz;
				}
			}
			else if (sm_ClosestNodeIdx < GetOceanSettings()->numShoreLinePoints)
			{
				audShoreLineOcean *nextShoreLine = GetNextOceanReference();
				if(!nextShoreLine)
				{
					nextShoreLine = FindFirstReference();
					naAssertf(nextShoreLine, "Got a null when looking for the first shore, something went wrong");
				}
				if( nextShoreLine && nextShoreLine->GetOceanSettings())
				{	
					nextNodeIdx = 0;
					CVfxHelper::GetWaterZ(Vec3V(nextShoreLine->GetOceanSettings()->ShoreLinePoints[nextNodeIdx].X, nextShoreLine->GetOceanSettings()->ShoreLinePoints[nextNodeIdx].Y, 0.f), waterz);
					sm_WaterHeights[NextIndex] = waterz;
				}
				CVfxHelper::GetWaterZ(Vec3V(GetOceanSettings()->ShoreLinePoints[prevNodeIdx].X, GetOceanSettings()->ShoreLinePoints[prevNodeIdx].Y, 0.f), waterz);
				sm_WaterHeights[PrevIndex] = waterz;
			}
		}
	}
	CVfxHelper::GetWaterZ(Vec3V(sm_WaveStartDetectionPoint.GetX(),sm_WaveStartDetectionPoint.GetY(), 0.f), sm_WaveStartDetectionPointHeight);
	CVfxHelper::GetWaterZ(Vec3V(sm_WaveBreaksDetectionPoint.GetX(), sm_WaveBreaksDetectionPoint.GetY(), 0.f), sm_WaveBreaksDetectionPointHeight);
	CVfxHelper::GetWaterZ(Vec3V(sm_WaveEndsDetectionPoint.GetX(), sm_WaveEndsDetectionPoint.GetY(), 0.f), sm_WaveEndsDetectionPointHeight);

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
		for(u32 idx = 0 ; idx <(u32)GetOceanSettings()->numShoreLinePoints; idx++)
		{
			f32 waterz;
			CVfxHelper::GetWaterZ(Vec3V(GetOceanSettings()->ShoreLinePoints[idx].X, GetOceanSettings()->ShoreLinePoints[idx].Y, 0.f), waterz);
			m_WaterHeights.PushAndGrow(waterz);
		}
	}
#endif
}

// ----------------------------------------------------------------
void audShoreLineOcean::StopSounds()
{
	for (u32 i = 0; i < NumWaterSoundPos;i ++)
	{
		if(sm_Sounds[i])
		{
			sm_Sounds[i]->StopAndForget();
		}
		if(sm_OceanWaveStartSound[i])
		{
			sm_OceanWaveStartSound[i]->StopAndForget();
		}
		if(sm_OceanWaveBreakSound[i])
		{
			sm_OceanWaveBreakSound[i]->StopAndForget();
		}
		if(sm_OceanWaveEndSound[i])
		{
			sm_OceanWaveEndSound[i]->StopAndForget();
		}
		if(sm_OceanWaveRecedeSound[i])
		{
			sm_OceanWaveRecedeSound[i]->StopAndForget();
		}
		if(sm_OceanWaveStartSound[i])
		{
			sm_OceanWaveStartSound[i]->StopAndForget();
		}
	}
}
#if __BANK
// ----------------------------------------------------------------
// Initialise the shore line
// ----------------------------------------------------------------
void audShoreLineOcean::InitDebug()
{
	if(m_Settings)
	{
		m_ShoreLinePoints.Reset();
		m_ShoreLinePoints.Resize(GetOceanSettings()->numShoreLinePoints);
		for(s32 i = 0; i < GetOceanSettings()->numShoreLinePoints; i++)
		{
			m_ShoreLinePoints[i].x = GetOceanSettings()->ShoreLinePoints[i].X;
			m_ShoreLinePoints[i].y = GetOceanSettings()->ShoreLinePoints[i].Y;
		}
		Vector2 center = Vector2(GetOceanSettings()->ActivationBox.Center.X,GetOceanSettings()->ActivationBox.Center.Y);
		m_ActivationBox = fwRect(center,GetOceanSettings()->ActivationBox.Size.Width/2.f,GetOceanSettings()->ActivationBox.Size.Height/2.f);
		m_RotAngle = GetOceanSettings()->ActivationBox.RotationAngle;
		m_Width = m_ActivationBox.GetWidth();
		m_Height = m_ActivationBox.GetHeight();
	}
}
// ----------------------------------------------------------------
// Serialise the shoreline data
// ----------------------------------------------------------------
bool audShoreLineOcean::Serialise(const char* shoreName,bool editing)
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
void audShoreLineOcean::FormatShoreLineAudioSettingsXml(char * xmlMsg, const char * settingsName)
{
	sprintf(xmlMsg, "<RAVEMessage>\n");
	char tmpBuf[256] = {0};

	atString upperSettings(settingsName);
	upperSettings.Uppercase();

	sprintf(tmpBuf, "	<EditObjects metadataType=\"GAMEOBJECTS\" chunkNameHash=\"%u\">\n", atStringHash("BASE"));
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "		<ShoreLineOceanAudioSettings name=\"%s\">\n", &settingsName[0]);
	strcat(xmlMsg, tmpBuf);

	SerialiseActivationBox(xmlMsg,tmpBuf);
	if(GetNextOceanReference() && GetNextOceanReference()->GetOceanSettings())
	{
		sprintf(tmpBuf, "	<NextShoreline>%s</NextShoreline>",
			audNorthAudioEngine::GetMetadataManager().GetObjectName(GetNextOceanReference()->GetOceanSettings()->NameTableOffset,0));
		strcat(xmlMsg, tmpBuf);
	}
	if(GetOceanSettings())
	{
		sprintf(tmpBuf, "	<OceanType>%s</OceanType>",GetOceanTypeName((OceanWaterType)GetOceanSettings()->OceanType));
		strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "	<OceanDirection>%s</OceanDirection>",GetOceanDirectionName((OceanWaterDirection)GetOceanSettings()->OceanDirection));
		strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "	<WaveStartDPDistance>%f</WaveStartDPDistance>",GetOceanSettings()->WaveStartDPDistance);
		strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "	<WaveStartHeight>%f</WaveStartHeight>",GetOceanSettings()->WaveStartHeight);
		strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "	<WaveBreaksDPDistance>%f</WaveBreaksDPDistance>",GetOceanSettings()->WaveBreaksDPDistance);
		strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "	<WaveBreaksHeight>%f</WaveBreaksHeight>",GetOceanSettings()->WaveBreaksHeight);
		strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "	<WaveEndDPDistance>%f</WaveEndDPDistance>",GetOceanSettings()->WaveEndDPDistance);
		strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "	<WaveEndHeight>%f</WaveEndHeight>",GetOceanSettings()->WaveEndHeight);
		strcat(xmlMsg, tmpBuf);
		sprintf(tmpBuf, "	<RecedeHeight>%f</RecedeHeight>",GetOceanSettings()->RecedeHeight);
		strcat(xmlMsg, tmpBuf);
	}

	for(u32 i = 0 ; i < m_ShoreLinePoints.GetCount(); i++)
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
	sprintf(tmpBuf, "		</ShoreLineOceanAudioSettings>\n");
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "	</EditObjects>\n");
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "</RAVEMessage>\n");
	strcat(xmlMsg, tmpBuf);
}
//------------------------------------------------------------------------------------------------------------------------------
const char * audShoreLineOcean::GetOceanTypeName(OceanWaterType oceanType)
{
	switch(oceanType)
	{
	case AUD_OCEAN_TYPE_BEACH:
		return "AUD_OCEAN_TYPE_BEACH";
	case AUD_OCEAN_TYPE_ROCKS:
		return "AUD_OCEAN_TYPE_ROCKS";
	case AUD_OCEAN_TYPE_PORT:
		return "AUD_OCEAN_TYPE_PORT";
	case AUD_OCEAN_TYPE_PIER:
		return "AUD_OCEAN_TYPE_PIER";
	default:
		return "";
	}
}
//------------------------------------------------------------------------------------------------------------------------------
const char * audShoreLineOcean::GetOceanDirectionName(OceanWaterDirection oceanDirection)
{
	switch(oceanDirection)
	{
	case AUD_OCEAN_DIR_NORTH:
		return "AUD_OCEAN_DIR_NORTH";
	case AUD_OCEAN_DIR_NORTH_EAST:
		return "AUD_OCEAN_DIR_NORTH_EAST";
	case AUD_OCEAN_DIR_EAST:
		return "AUD_OCEAN_DIR_EAST";
	case AUD_OCEAN_DIR_SOUTH_EAST:
		return "AUD_OCEAN_DIR_SOUTH_EAST";
	case AUD_OCEAN_DIR_SOUTH:
		return "AUD_OCEAN_DIR_SOUTH";
	case AUD_OCEAN_DIR_SOUTH_WEST:
		return "AUD_OCEAN_DIR_SOUTH_WEST";
	case AUD_OCEAN_DIR_WEST:
		return "AUD_OCEAN_DIR_WEST";
	case AUD_OCEAN_DIR_NORTH_WEST:
		return "AUD_OCEAN_DIR_NORTH_WEST";
	default:
		return "";
	}
}
//------------------------------------------------------------------------------------------------------------------------------
void audShoreLineOcean::DrawShoreLines(bool drawSettingsPoints, const u32 pointIndex, bool /*checkForPlaying*/)	const
{
	// draw lines
	if(drawSettingsPoints && GetOceanSettings() && (u32)GetOceanSettings()->numShoreLinePoints > 0 && m_WaterHeights.GetCount() 
		&& GetOceanSettings()->numShoreLinePoints == m_WaterHeights.GetCount())
	{
		for(u32 i = 0; i < (u32)GetOceanSettings()->numShoreLinePoints; i++)
		{
			Vector3 currentPoint = Vector3(GetOceanSettings()->ShoreLinePoints[i].X, GetOceanSettings()->ShoreLinePoints[i].Y, m_WaterHeights[i] + 0.3f);
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
			if(i < GetOceanSettings()->numShoreLinePoints - 1 )
			{
				Vector3 nextPoint = Vector3(GetOceanSettings()->ShoreLinePoints[i+1].X, GetOceanSettings()->ShoreLinePoints[i+1].Y, m_WaterHeights[i+1] + 0.3f);
				grcDebugDraw::Line(currentPoint, nextPoint,	Color_red);
			}
		}
		// Draw link
		if(m_NextShoreline && m_NextShoreline->GetWaterHeightsCount() > 0 && (u32)m_NextShoreline->GetOceanSettings()->numShoreLinePoints == m_NextShoreline->GetWaterHeightsCount())
		{
			Vector3 lastPoint = Vector3(GetOceanSettings()->ShoreLinePoints[GetOceanSettings()->numShoreLinePoints-1].X, GetOceanSettings()->ShoreLinePoints[GetOceanSettings()->numShoreLinePoints-1].Y, m_WaterHeights[GetOceanSettings()->numShoreLinePoints-1] + 0.3f);
			Vector3 firstPoint = Vector3(m_NextShoreline->GetOceanSettings()->ShoreLinePoints[0].X, m_NextShoreline->GetOceanSettings()->ShoreLinePoints[0].Y, m_NextShoreline->GetWaterHeightOnPoint(0) + 0.3f);
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
				grcDebugDraw::Line(currentPoint, nextPoint,	Color_red);
			}
		}
	}
}
// ----------------------------------------------------------------
void audShoreLineOcean::RenderDebug()const 
{
	audShoreLine::RenderDebug();
	for(u32 i = 0; i < (u32)GetOceanSettings()->numShoreLinePoints - 1; i++)
	{
		CVectorMap::DrawLine(Vector3(GetOceanSettings()->ShoreLinePoints[i].X, GetOceanSettings()->ShoreLinePoints[i].Y, 0.f), 
			Vector3(GetOceanSettings()->ShoreLinePoints[i+1].X, GetOceanSettings()->ShoreLinePoints[i+1].Y, 0.f),
			Color_red,Color_blue);
	}
}
// ----------------------------------------------------------------
void audShoreLineOcean::AddDebugShorePoint(const Vector2 &vec)
{
	if(m_ShoreLinePoints.GetCount() < sm_MaxNumPointsPerShore)
	{
		m_ShoreLinePoints.PushAndGrow(vec);
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
u32 audShoreLineOcean::GetNumPoints()
{
	if(m_Settings)
	{
		return GetOceanSettings()->numShoreLinePoints;
	}
	return 0;
}
//------------------------------------------------------------------------------------------------------------------------------
void audShoreLineOcean::EditShorelinePoint(const u32 pointIdx, const Vector3 &newPos)
{
	if (pointIdx < sm_MaxNumPointsPerShore)
	{
		if(m_Settings)
		{
			if(naVerifyf(pointIdx < GetOceanSettings()->numShoreLinePoints,"Wrong point index"))
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
void audShoreLineOcean::MoveShoreAlongDirection(const Vector2 &dirOfMovement,const u8 /*side*/)
{	
	for (u32 i = 0; i < m_ShoreLinePoints.GetCount(); i++)
	{
		m_ShoreLinePoints[i] = m_ShoreLinePoints[i] + dirOfMovement;
	}
}
//------------------------------------------------------------------------------------------------------------------------------
void audShoreLineOcean::Cancel()
{	
	audShoreLine::Cancel();
}
// ----------------------------------------------------------------
// Add widgets to RAG tool
// ----------------------------------------------------------------
void audShoreLineOcean::AddWidgets(bkBank &bank)
{
	bank.PushGroup("Ocean");
		bank.AddSlider("Inner distance", &sm_InnerDistance , 0.f, 1000.f, 0.1f);
		bank.AddSlider("Outter distance", &sm_OutterDistance, 0.f, 1000.f, 0.1f);
		bank.AddSlider("Max distance to shore", &sm_MaxDistanceToShore, 0.f, 100.f, 0.1f);

	bank.PopGroup();
}
//------------------------------------------------------------------------------------------------------------------------------
void audShoreLineOcean::AddEditorWidgets(bkBank &bank)
{
	bank.PushGroup("Ocean parameters");
	bank.PopGroup();
}
#endif
