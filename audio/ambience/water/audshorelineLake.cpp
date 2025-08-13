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
#include "audshorelineLake.h"
#include "audio/northaudioengine.h"
#include "audio/gameobjects.h"
#include "debug/vectormap.h"
#include "renderer/water.h"
#include "game/weather.h"
#include "vfx/VfxHelper.h"
#include "peds/ped.h"
#include "audio/weatheraudioentity.h"

AUDIO_AMBIENCE_OPTIMISATIONS()

atRangeArray<audSound*,NumWaterSoundPos - 1> audShoreLineLake::sm_LakeSound;
atRangeArray<f32,NumWaterSoundPos> audShoreLineLake::sm_WaterHeights;

audSoundSet audShoreLineLake::sm_LakeSounds;
f32 audShoreLineLake::sm_InnerDistance = 1.f;
f32 audShoreLineLake::sm_OutterDistance = 25.f;
f32 audShoreLineLake::sm_MaxDistanceToShore = 10.f;

audShoreLineLake* audShoreLineLake::sm_ClosestLakeShore = NULL;

f32 audShoreLineLake::sm_ClosestDistToShore = LARGE_FLOAT;
u8 audShoreLineLake::sm_LakeClosestNode = 0;
u8 audShoreLineLake::sm_LastLakeClosestNode = 0;

BANK_ONLY(bool audShoreLineLake::sm_UseLeftRightPoints = false;)
// ----------------------------------------------------------------
// Initialise static member variables
// ----------------------------------------------------------------
audShoreLineLake::audShoreLineLake(const ShoreLineLakeAudioSettings* settings) : audShoreLine()
{
	m_Settings = static_cast<const ShoreLineAudioSettings*>(settings);

	m_PrevShoreline = NULL;
	m_NextShoreline = NULL;

}
// ----------------------------------------------------------------
audShoreLineLake::~audShoreLineLake()
{
	m_PrevShoreline = NULL;
	m_NextShoreline = NULL;
}
// ----------------------------------------------------------------
void audShoreLineLake::InitClass()
{
	sm_LakeSounds.Init(ATSTRINGHASH("LAKE_WATER_SOUNDSET", 0xBA0ED7EF));
	for (u8 i = 0; i < NumWaterSoundPos - 1;i ++)
	{
		sm_LakeSound[i] = NULL;
	}
	for (u8 i = 0; i < NumWaterHeights;i ++)
	{
		sm_WaterHeights[i] = -9999.f;
	}

}
// ----------------------------------------------------------------
void audShoreLineLake::ShutDownClass()
{
	for (u32 i = 0; i < NumWaterSoundPos - 1;i ++)
	{
		if(sm_LakeSound[i])
		{
			sm_LakeSound[i]->StopAndForget();
		}
	}
	sm_ClosestLakeShore = NULL;
}
// ----------------------------------------------------------------
// Load shoreline data from a file
// ----------------------------------------------------------------
void audShoreLineLake::Load(BANK_ONLY(const ShoreLineAudioSettings *settings))
{
	audShoreLine::Load(BANK_ONLY(settings));

	Vector2 center = Vector2(GetLakeSettings()->ActivationBox.Center.X,GetLakeSettings()->ActivationBox.Center.Y);
	m_ActivationBox = fwRect(center,GetLakeSettings()->ActivationBox.Size.Width/2.f,GetLakeSettings()->ActivationBox.Size.Height/2.f);
#if __BANK
	m_RotAngle = GetLakeSettings()->ActivationBox.RotationAngle;
	m_Width = GetLakeSettings()->ActivationBox.Size.Width;
	m_Height = GetLakeSettings()->ActivationBox.Size.Height;
#endif
}	
// ----------------------------------------------------------------
// Update the shoreline activation
// ----------------------------------------------------------------
bool audShoreLineLake::UpdateActivation()
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
		for (u32 i = 0; i < NumWaterSoundPos - 1; i ++)
		{
			if(sm_LakeSound[i])
			{
				sm_LakeSound[i]->StopAndForget();
			}
		}
	}
	return false;
}
// ----------------------------------------------------------------
void audShoreLineLake::ComputeClosestShore()
{
	// Look for the closest point to the player.
	Vector3 pos2D = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition());
	pos2D.z = 0.0f; 
	// Look for the closest point
	for(u8 i = 0; i < GetLakeSettings()->numShoreLinePoints; i++)
	{
		const f32 dist2 = (Vector3(GetLakeSettings()->ShoreLinePoints[i].X,GetLakeSettings()->ShoreLinePoints[i].Y,0.f)-pos2D).Mag2();
		if(dist2 < sm_ClosestDistToShore)
		{
			sm_ClosestDistToShore = dist2;
			sm_LakeClosestNode = i;
			sm_ClosestLakeShore = this;
		}
	}
}
// ----------------------------------------------------------------
bool audShoreLineLake::IsClosestShore()
{
	return sm_ClosestLakeShore == this;
}
// ----------------------------------------------------------------
// Update the shoreline 
// ----------------------------------------------------------------
void audShoreLineLake::CalculatePoints(Vector3 &centrePoint,Vector3 &/*leftPoint*/,Vector3 &/*rightPoint*/, const u8 /*closestRiverIdx*/)
{
	// First of all get the direction to the next and prev node.
	Vector3 lakeDirectionToNextNode,lakeDirectionFromPrevNode;
	const ShoreLineLakeAudioSettings* settings = sm_ClosestLakeShore->GetLakeSettings();
	Vector3 closestPoint = Vector3(settings->ShoreLinePoints[sm_LakeClosestNode].X,settings->ShoreLinePoints[sm_LakeClosestNode].Y,sm_WaterHeights[ClosestIndex]);
	Vector3 nextPoint;
	Vector3 prevPoint;

	u8 nextNodeIdx = (sm_LakeClosestNode + 1);
	u8 prevNodeIdx = (sm_LakeClosestNode - 1);

	if(sm_LakeClosestNode > 0 && sm_LakeClosestNode < settings->numShoreLinePoints -1)
	{
		nextPoint = Vector3(GetLakeSettings()->ShoreLinePoints[nextNodeIdx].X,GetLakeSettings()->ShoreLinePoints[nextNodeIdx].Y,sm_WaterHeights[NextIndex]);
		prevPoint = Vector3(GetLakeSettings()->ShoreLinePoints[prevNodeIdx].X,GetLakeSettings()->ShoreLinePoints[prevNodeIdx].Y,sm_WaterHeights[PrevIndex]);
	}
	else if ( sm_LakeClosestNode == 0)
	{
		nextPoint = Vector3(GetLakeSettings()->ShoreLinePoints[nextNodeIdx].X,GetLakeSettings()->ShoreLinePoints[nextNodeIdx].Y,sm_WaterHeights[NextIndex]);
		audShoreLineLake *prevShoreLine = sm_ClosestLakeShore->GetPrevLakeReference();
		if( !prevShoreLine)
		{
			// TODO: Add support to tag the shore not to do this.
			prevShoreLine = sm_ClosestLakeShore->FindLastReference();
			naAssertf(prevShoreLine, "Got a null when looking for the last shore, something went wrong") ;
		}
		if(prevShoreLine && prevShoreLine->GetLakeSettings())
		{
			prevPoint =  Vector3(prevShoreLine->GetLakeSettings()->ShoreLinePoints[prevShoreLine->GetLakeSettings()->numShoreLinePoints -1].X
				,prevShoreLine->GetLakeSettings()->ShoreLinePoints[prevShoreLine->GetLakeSettings()->numShoreLinePoints - 1].Y
				,closestPoint.GetZ());
		}
	}
	else if (sm_LakeClosestNode < settings->numShoreLinePoints)
	{
		audShoreLineLake *nextShoreLine = sm_ClosestLakeShore->GetNextLakeReference();
		if(!nextShoreLine)
		{
			// TODO: Add support to tag the shore not to do this.
			nextShoreLine = sm_ClosestLakeShore->FindFirstReference();
			naAssertf(nextShoreLine, "Got a null when looking for the first shore, something went wrong");
		}
		if( nextShoreLine && nextShoreLine->GetLakeSettings())
		{			
			nextPoint = Vector3(nextShoreLine->GetLakeSettings()->ShoreLinePoints[0].X
				,nextShoreLine->GetLakeSettings()->ShoreLinePoints[0].Y
				,closestPoint.GetZ());
			nextNodeIdx = 0;
		}
		prevPoint = Vector3(GetLakeSettings()->ShoreLinePoints[prevNodeIdx].X,GetLakeSettings()->ShoreLinePoints[prevNodeIdx].Y,sm_WaterHeights[PrevIndex]);
	}

	lakeDirectionToNextNode = nextPoint	- closestPoint;
	lakeDirectionToNextNode.NormalizeSafe();

	lakeDirectionFromPrevNode = closestPoint - prevPoint;
	lakeDirectionFromPrevNode.NormalizeSafe();

	Vector3 pos = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition());
	Vector3 dirToListener =  pos - closestPoint;
	f32 distanceToCentre = dirToListener.Mag2();
	m_DistanceToShore = distanceToCentre;
	if(distanceToCentre > (sm_DistanceThresholdToStopSounds * sm_DistanceThresholdToStopSounds))
	{
		m_IsActive = false;
		if(IsClosestShore())
		{
			for (u32 i = 0; i < NumWaterSoundPos - 1; i ++)
			{
				if(sm_LakeSound[i])
				{
					sm_LakeSound[i]->StopAndForget();
				}
			}
		}
		return;
	}
	CalculateCentrePoint(closestPoint,nextPoint,prevPoint,lakeDirectionToNextNode,lakeDirectionFromPrevNode,pos,dirToListener,centrePoint);

#if __BANK 
	if(sm_DrawWaterBehaviour)
	{
		//Draw maths that calculates the point 
		grcDebugDraw::Line(closestPoint, closestPoint + dirToListener, Color_red );
		grcDebugDraw::Sphere(centrePoint + Vector3( 0.f,0.f,1.f),0.3f,Color_green);
	}
#endif
}
// ----------------------------------------------------------------
audShoreLineLake* audShoreLineLake::FindFirstReference()
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
audShoreLineLake* audShoreLineLake::FindLastReference()
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
void audShoreLineLake::Update(const f32 /*timeElapsedS*/)
{
	if(IsClosestShore())
	{
		if(m_IsActive && GetLakeSettings())  
		{
			UpdateLakeSound();
		}
	}
}
// ----------------------------------------------------------------
void audShoreLineLake::UpdateLakeSound()
{
	u8 nextDesireNode = (sm_LastLakeClosestNode + 1) % GetLakeSettings()->numShoreLinePoints;
	u8 prevDesireNode = sm_LastLakeClosestNode -1; 
	if(sm_LastLakeClosestNode == 0)
	{
		prevDesireNode = (u8)(GetLakeSettings()->numShoreLinePoints - 1) ;
	}
	if ((sm_LakeClosestNode != sm_LastLakeClosestNode) && ((sm_LakeClosestNode != nextDesireNode) && (sm_LakeClosestNode != prevDesireNode)))
	{
		if(sm_LakeSound[Left])
		{
			sm_LakeSound[Left]->StopAndForget();
		}
		if(sm_LakeSound[Right])
		{
			sm_LakeSound[Right]->StopAndForget();
		}
	}
	sm_LastLakeClosestNode = sm_LakeClosestNode;

	Vector3 centrePoint,leftPoint,rightPoint;
	CalculatePoints(centrePoint,leftPoint,rightPoint);
	m_CentrePoint = centrePoint;
	if(m_IsActive)
	{
		leftPoint = rightPoint;

		if(!sm_LakeSound[Left])
		{
			audMetadataRef soundRef = sm_LakeSounds.Find(ATSTRINGHASH("SMALL_LAKE_LEFT", 0x744AAAD8));
			if(GetLakeSettings()->LakeSize == AUD_LAKE_MEDIUM)
			{
				soundRef = sm_LakeSounds.Find(ATSTRINGHASH("MEDIUM_LAKE_LEFT", 0x1AE12F68));
			}
			else if(GetLakeSettings()->LakeSize == AUD_LAKE_LARGE)
			{
				soundRef = sm_LakeSounds.Find(ATSTRINGHASH("LARGE_LAKE_LEFT", 0xB84E8F7));
			}

			audSoundInitParams centreInitParams;
			centreInitParams.Position = centrePoint;
			g_AmbientAudioEntity.CreateAndPlaySound_Persistent(soundRef,&sm_LakeSound[Left],&centreInitParams);
		}
		if(!sm_LakeSound[Right])
		{
			audMetadataRef soundRef = sm_LakeSounds.Find(ATSTRINGHASH("SMALL_LAKE_RIGHT", 0x97233858));
			if(GetLakeSettings()->LakeSize == AUD_LAKE_MEDIUM)
			{
				soundRef = sm_LakeSounds.Find(ATSTRINGHASH("MEDIUM_LAKE_RIGHT", 0xFA0B1927));
			}
			else if(GetLakeSettings()->LakeSize == AUD_LAKE_LARGE)
			{
				soundRef = sm_LakeSounds.Find(ATSTRINGHASH("LARGE_LAKE_RIGHT", 0xA2C82DF0));
			}

			audSoundInitParams centreInitParams;
			centreInitParams.Position = centrePoint;
			g_AmbientAudioEntity.CreateAndPlaySound_Persistent(soundRef,&sm_LakeSound[Right],&centreInitParams);
		}
		if(sm_LakeSound[Left] )    
		{
			sm_LakeSound[Left]->SetRequestedPosition(centrePoint);
		}
		if(sm_LakeSound[Right] )    
		{
			sm_LakeSound[Right]->SetRequestedPosition(centrePoint);
		}
#if __BANK
		if(sm_DrawWaterBehaviour)
		{
			char name[64];
			formatf(name,"%s",audNorthAudioEngine::GetMetadataManager().GetObjectName(sm_ClosestLakeShore->GetLakeSettings()->NameTableOffset,0));
			grcDebugDraw::AddDebugOutput(Color_green,name);
		}
#endif
	}
}
// ----------------------------------------------------------------
// Check if a point is over the water
// ----------------------------------------------------------------
bool audShoreLineLake::IsPointOverWater(const Vector3& /*pos*/) const
{
	//u32 winding = 0;

	//for(u32 index = 0; index < GetLakeSettings()->numShoreLinePoints; index++)
	//{
	//	u32 nextIndex = index + 1;

	//	if(nextIndex >= GetLakeSettings()->numShoreLinePoints)
	//	{
	//		nextIndex = 0;
	//	}

	//	Vector2 thisShoreLinePos = Vector2(GetLakeSettings()->ShoreLinePoints[index].X, GetLakeSettings()->ShoreLinePoints[index].Y);
	//	Vector2 nextShoreLinePos = Vector2(GetLakeSettings()->ShoreLinePoints[nextIndex].X, GetLakeSettings()->ShoreLinePoints[nextIndex].Y);

	//	if(thisShoreLinePos.y <= pos.y)
	//	{
	//		if(nextShoreLinePos.y > pos.y)
	//		{
	//			f32 isLeft = ((nextShoreLinePos.x - thisShoreLinePos.x) * (pos.y - thisShoreLinePos.y) - (pos.x - thisShoreLinePos.x) * (nextShoreLinePos.y - thisShoreLinePos.y));

	//			if(isLeft > 0)
	//			{
	//				winding++;
	//			}
	//		}
	//	}
	//	else
	//	{
	//		if(nextShoreLinePos.y <= pos.y)
	//		{
	//			f32 isLeft = ((nextShoreLinePos.x - thisShoreLinePos.x) * (pos.y - thisShoreLinePos.y) - (pos.x - thisShoreLinePos.x) * (nextShoreLinePos.y - thisShoreLinePos.y));

	//			if(isLeft < 0)
	//			{
	//				winding--;
	//			}
	//		}
	//	}
	//}

	////return winding != 0;
	return false;
}
// ----------------------------------------------------------------
void audShoreLineLake::UpdateWaterHeight(bool BANK_ONLY(updateAllHeights))
{
	if(m_Settings)
	{
		if(IsClosestShore())
		{
			// Closest index : 
			f32 waterz;
			CVfxHelper::GetWaterZ(Vec3V(GetLakeSettings()->ShoreLinePoints[sm_LakeClosestNode].X, GetLakeSettings()->ShoreLinePoints[sm_LakeClosestNode].Y, 0.f), waterz);
			sm_WaterHeights[ClosestIndex] = waterz;

			s8 nextNodeIdx = (sm_LakeClosestNode + 1);
			s8 prevNodeIdx = (sm_LakeClosestNode - 1);

			if(sm_LakeClosestNode > 0 && sm_LakeClosestNode < GetLakeSettings()->numShoreLinePoints -1)
			{
				CVfxHelper::GetWaterZ(Vec3V(GetLakeSettings()->ShoreLinePoints[nextNodeIdx].X, GetLakeSettings()->ShoreLinePoints[nextNodeIdx].Y, 0.f), waterz);
				sm_WaterHeights[NextIndex] = waterz;
				CVfxHelper::GetWaterZ(Vec3V(GetLakeSettings()->ShoreLinePoints[prevNodeIdx].X, GetLakeSettings()->ShoreLinePoints[prevNodeIdx].Y, 0.f), waterz);
				sm_WaterHeights[PrevIndex] = waterz;
			}
			else if ( sm_LakeClosestNode == 0)
			{
				CVfxHelper::GetWaterZ(Vec3V(GetLakeSettings()->ShoreLinePoints[nextNodeIdx].X, GetLakeSettings()->ShoreLinePoints[nextNodeIdx].Y, 0.f), waterz);
				sm_WaterHeights[NextIndex] = waterz;
				audShoreLineLake *prevShoreLine = GetPrevLakeReference();
				if( !prevShoreLine)
				{
					prevShoreLine = FindLastReference();
					naAssertf(prevShoreLine, "Got a null when looking for the last shore, something went wrong") ;
				}
				if(prevShoreLine && prevShoreLine->GetLakeSettings())
				{
					prevNodeIdx = prevShoreLine->GetLakeSettings()->numShoreLinePoints - 1;
					CVfxHelper::GetWaterZ(Vec3V(prevShoreLine->GetLakeSettings()->ShoreLinePoints[prevNodeIdx].X, prevShoreLine->GetLakeSettings()->ShoreLinePoints[prevNodeIdx].Y, 0.f), waterz);
					sm_WaterHeights[PrevIndex] = waterz;
				}
			}
			else if (sm_LakeClosestNode < GetLakeSettings()->numShoreLinePoints)
			{
				audShoreLineLake *nextShoreLine = GetNextLakeReference();
				if(!nextShoreLine)
				{
					nextShoreLine = FindFirstReference();
					naAssertf(nextShoreLine, "Got a null when looking for the first shore, something went wrong");
				}
				if( nextShoreLine && nextShoreLine->GetLakeSettings())
				{	
					nextNodeIdx = 0;
					CVfxHelper::GetWaterZ(Vec3V(nextShoreLine->GetLakeSettings()->ShoreLinePoints[nextNodeIdx].X, nextShoreLine->GetLakeSettings()->ShoreLinePoints[nextNodeIdx].Y, 0.f), waterz);
					sm_WaterHeights[NextIndex] = waterz;
				}
				CVfxHelper::GetWaterZ(Vec3V(GetLakeSettings()->ShoreLinePoints[prevNodeIdx].X, GetLakeSettings()->ShoreLinePoints[prevNodeIdx].Y, 0.f), waterz);
				sm_WaterHeights[PrevIndex] = waterz;
			}
		}
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
		for(u32 idx = 0 ; idx <(u32)GetLakeSettings()->numShoreLinePoints; idx++)
		{
			f32 waterz;
			CVfxHelper::GetWaterZ(Vec3V(GetLakeSettings()->ShoreLinePoints[idx].X, GetLakeSettings()->ShoreLinePoints[idx].Y, 0.f), waterz);
			m_WaterHeights.PushAndGrow(waterz);
		}

	}
#endif
}

#if __BANK
// ----------------------------------------------------------------
// Initialise the shore line
// ----------------------------------------------------------------
void audShoreLineLake::InitDebug()
{
	if(m_Settings)
	{
		m_ShoreLinePoints.Reset();
		m_ShoreLinePoints.Resize(GetLakeSettings()->numShoreLinePoints);
		for(s32 i = 0; i < GetLakeSettings()->numShoreLinePoints; i++)
		{
			m_ShoreLinePoints[i].x = GetLakeSettings()->ShoreLinePoints[i].X;
			m_ShoreLinePoints[i].y = GetLakeSettings()->ShoreLinePoints[i].Y;
		}
		Vector2 center = Vector2(GetLakeSettings()->ActivationBox.Center.X,GetLakeSettings()->ActivationBox.Center.Y);
		m_ActivationBox = fwRect(center,GetLakeSettings()->ActivationBox.Size.Width/2.f,GetLakeSettings()->ActivationBox.Size.Height/2.f);
		m_RotAngle = GetLakeSettings()->ActivationBox.RotationAngle;
		m_Width = m_ActivationBox.GetWidth();
		m_Height = m_ActivationBox.GetHeight();
	}
}
// ----------------------------------------------------------------
// Serialise the shoreline data
// ----------------------------------------------------------------
bool audShoreLineLake::Serialise(const char* shoreName,bool editing)
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
// ----------------------------------------------------------------
void audShoreLineLake::FormatShoreLineAudioSettingsXml(char * xmlMsg, const char * settingsName)
{
	sprintf(xmlMsg, "<RAVEMessage>\n");
	char tmpBuf[256] = {0};

	atString upperSettings(settingsName);
	upperSettings.Uppercase();

	sprintf(tmpBuf, "	<EditObjects metadataType=\"GAMEOBJECTS\" chunkNameHash=\"%u\">\n", atStringHash("BASE"));
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "		<ShoreLineLakeAudioSettings name=\"%s\">\n", &settingsName[0]);
	strcat(xmlMsg, tmpBuf);

	SerialiseActivationBox(xmlMsg,tmpBuf);

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
	sprintf(tmpBuf, "		</ShoreLineLakeAudioSettings>\n");
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "	</EditObjects>\n");
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "</RAVEMessage>\n");
	strcat(xmlMsg, tmpBuf);
}
//------------------------------------------------------------------------------------------------------------------------------
void audShoreLineLake::DrawShoreLines(bool drawSettingsPoints, const u32 pointIndex, bool /*checkForPlaying*/)	const
{
	// draw lines
	if(drawSettingsPoints && GetLakeSettings() && (u32)GetLakeSettings()->numShoreLinePoints > 0 && m_WaterHeights.GetCount() 
		&& GetLakeSettings()->numShoreLinePoints == m_WaterHeights.GetCount())
	{
		for(u32 i = 0; i < (u32)GetLakeSettings()->numShoreLinePoints; i++)
		{
			Vector3 currentPoint = Vector3(GetLakeSettings()->ShoreLinePoints[i].X, GetLakeSettings()->ShoreLinePoints[i].Y, m_WaterHeights[i] + 0.3f);
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
			if(i < GetLakeSettings()->numShoreLinePoints - 1 )
			{
				Vector3 nextPoint = Vector3(GetLakeSettings()->ShoreLinePoints[i+1].X, GetLakeSettings()->ShoreLinePoints[i+1].Y, m_WaterHeights[i+1] + 0.3f);
				grcDebugDraw::Line(currentPoint, nextPoint,	Color_red);
			}
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
void audShoreLineLake::RenderDebug()const 
{
	audShoreLine::RenderDebug();
	for(u32 i = 0; i < (u32)GetLakeSettings()->numShoreLinePoints - 1; i++)
	{
		CVectorMap::DrawLine(Vector3(GetLakeSettings()->ShoreLinePoints[i].X, GetLakeSettings()->ShoreLinePoints[i].Y, 0.f), 
			Vector3(GetLakeSettings()->ShoreLinePoints[i+1].X, GetLakeSettings()->ShoreLinePoints[i+1].Y, 0.f),
			Color_red,Color_blue);
	}
}
// ----------------------------------------------------------------
void audShoreLineLake::AddDebugShorePoint(const Vector2 &vec)
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
u32 audShoreLineLake::GetNumPoints()
{
	if(m_Settings)
	{
		return GetLakeSettings()->numShoreLinePoints;
	}
	return 0;
}
//------------------------------------------------------------------------------------------------------------------------------
void audShoreLineLake::EditShorelinePoint(const u32 pointIdx, const Vector3 &newPos)
{
	if (pointIdx < sm_MaxNumPointsPerShore)
	{
		if(m_Settings)
		{
			if(naVerifyf(pointIdx < GetLakeSettings()->numShoreLinePoints,"Wrong point index"))
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
void audShoreLineLake::MoveShoreAlongDirection(const Vector2 &dirOfMovement,const u8 /*side*/)
{	
	for (u32 i = 0; i < m_ShoreLinePoints.GetCount(); i++)
	{
		m_ShoreLinePoints[i] = m_ShoreLinePoints[i] + dirOfMovement;
	}
}
//------------------------------------------------------------------------------------------------------------------------------
void audShoreLineLake::Cancel()
{	
	audShoreLine::Cancel();
}
// ----------------------------------------------------------------
// Add widgets to RAG tool
// ----------------------------------------------------------------
void audShoreLineLake::AddWidgets(bkBank &bank)
{
	bank.PushGroup("Lake");
		bank.AddSlider("Inner distance", &sm_InnerDistance , 0.f, 1000.f, 0.1f);
		bank.AddSlider("Outter distance", &sm_OutterDistance, 0.f, 1000.f, 0.1f);
		bank.AddSlider("Max distance to shore", &sm_MaxDistanceToShore, 0.f, 100.f, 0.1f);
	bank.PopGroup();
}
//------------------------------------------------------------------------------------------------------------------------------
void audShoreLineLake::AddEditorWidgets(bkBank &bank)
{
	bank.PushGroup("Lake parameters");
	bank.PopGroup();
}
#endif
