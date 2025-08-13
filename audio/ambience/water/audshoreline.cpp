//  
// audio/audshoreline.cpp
//  
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#include "audioengine/curve.h"
#include "audioengine/engine.h"
#include "audioengine/widgets.h"
// Game headers
#include "audshoreline.h"
#include "audio/northaudioengine.h" 
#include "audio/frontendaudioentity.h"
#include "audio/gameobjects.h"
#include "debug/vectormap.h"
#include "renderer/water.h"
#include "game/weather.h"
#include "vfx/VfxHelper.h"
#include "audio/weatheraudioentity.h"

AUDIO_AMBIENCE_OPTIMISATIONS()

extern audAmbientAudioEntity g_AmbientAudioEntity;

audSound* audShoreLine::sm_RainOnWaterSound;
audCurve audShoreLine::sm_DistToNodeSeparation;
f32 audShoreLine::sm_DistanceThresholdToStopSounds = 215.f;
BANK_ONLY(bool audShoreLine::sm_DrawWaterBehaviour = false;);
BANK_ONLY(u32 audShoreLine::sm_MaxNumPointsPerShore = 32;);
// ----------------------------------------------------------------
audShoreLine::audShoreLine()
{
	m_ActivationBox.Init();
	m_IsActive = false;
	m_JustActivated = false;
	m_Settings = NULL;
	sm_RainOnWaterSound = NULL;
	m_DistanceToShore = LARGE_FLOAT;
	m_CentrePoint = Vector3(0.f,0.f,0.f);
#if __BANK
	m_WaterHeights.Reset();
	m_RotAngle = 0.f;
	m_Width = 0.f;
	m_Height = 0.f;
#endif
}
// ----------------------------------------------------------------
void audShoreLine::Load(BANK_ONLY(const ShoreLineAudioSettings *settings))
{
#if __BANK
	if(settings && !m_Settings)
	{
		m_Settings = static_cast<const ShoreLineAudioSettings*>(settings);
	}
	m_RotAngle = 0.f;
	m_Width = 0.f;
	m_Height = 0.f;
	m_WaterHeights.Reset();
#endif
	naAssertf(m_Settings,"Failed loading shoreline audio settings");
	m_ActivationBox.Init();
	m_IsActive = false;
	m_JustActivated = false;
	sm_RainOnWaterSound = NULL;
	m_DistanceToShore = LARGE_FLOAT;
	m_CentrePoint = Vector3(0.f,0.f,0.f);
}	
// ----------------------------------------------------------------
void audShoreLine::InitClass()
{
	sm_DistToNodeSeparation.Init(ATSTRINGHASH("DISTANCE_TO_NODE_SEPARATION", 0x464E8670));
	sm_RainOnWaterSound = NULL;
}
//------------------------------------------------------------------------------------------------------------------------------
void audShoreLine::PlayRainOnWaterSound(const audMetadataRef &soundRef)
{
	if(!sm_RainOnWaterSound)
	{
		audSoundInitParams initParams; 
		initParams.AttackTime = 300;
		initParams.Position = m_CentrePoint;
		initParams.Volume = g_WeatherAudioEntity.GetRainVerticallAtt();
		g_AmbientAudioEntity.CreateAndPlaySound_Persistent(soundRef, &sm_RainOnWaterSound,&initParams);
	}
	else
	{
		sm_RainOnWaterSound->SetRequestedPosition(m_CentrePoint);
		sm_RainOnWaterSound->SetRequestedVolume(g_WeatherAudioEntity.GetRainVerticallAtt());
	}
}
#if __BANK
// ----------------------------------------------------------------
const f32 audShoreLine::GetWaterHeightOnPoint(u32 idx) const 
{
	if( m_WaterHeights.GetCount() && idx < m_WaterHeights.GetCount())
	{
		return m_WaterHeights[idx];
	}
	else 
	{
		//naErrorf("No avaliable water height for current idx : %u", idx);
		return -9999.f;
	}
}
#endif
// ----------------------------------------------------------------
naEnvironmentGroup* audShoreLine::CreateShoreLineEnvironmentGroup(const Vector3 &position)
{
	naEnvironmentGroup* environmentGroup = naEnvironmentGroup::Allocate("Shoreline");
	if(environmentGroup)
	{
		// Init with NULL entity so the naEnvironmentGroup is cleaned up when the sound is done
		environmentGroup->Init(NULL, 20.0f);	
		environmentGroup->SetSource(VECTOR3_TO_VEC3V(position));
		environmentGroup->SetInteriorInfoWithInteriorInstance(NULL, INTLOC_ROOMINDEX_LIMBO);
		environmentGroup->SetUsePortalOcclusion(true);
		environmentGroup->SetMaxPathDepth(2);
		environmentGroup->SetUseInteriorDistanceMetrics(true);
		environmentGroup->SetUseCloseProximityGroupsForUpdates(true);
	}
	return environmentGroup;
}
// ----------------------------------------------------------------
bool audShoreLine::UpdateActivation()
{
	// Early return if the player is in an interior
	bool interiorShore = AUD_GET_TRISTATE_VALUE(m_Settings->Flags, FLAG_ID_SHORELINEAUDIOSETTINGS_ISININTERIOR) == AUD_TRISTATE_TRUE;
	bool playerInInterior = audNorthAudioEngine::GetGtaEnvironment()->AreWeInAnInterior();
	if((playerInInterior && !interiorShore) || (!playerInInterior && interiorShore) || g_FrontendAudioEntity.GetSpecialAbilityMode() == audFrontendAudioEntity::kSpecialAbilityModeTrevor)
	{
		return false;
	}
	if(m_Settings && m_Settings->ActivationBox.RotationAngle != 0.f)
	{
		// create rotation matrix, centered at box centre
		Vector3 centre = Vector3(m_Settings->ActivationBox.Center.X,m_Settings->ActivationBox.Center.Y,0.f);
		// create rotation matrix, centered at box centre
		Matrix34 mat; // don't need to init the matrix, we're going to init it with MakeRotateZ
		mat.MakeRotateZ(m_Settings->ActivationBox.RotationAngle * DtoR);
		mat.d = centre;
		// convert listener position to matrix space
		Vector3 relativePos = VEC3V_TO_VECTOR3(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition());
		relativePos.z = 0.f;
		mat.UnTransform(relativePos);
		Vec3V activationBoxSize = Vec3V(m_Settings->ActivationBox.Size.Width,m_Settings->ActivationBox.Size.Height,0.f);
		spdAABB activationBox = spdAABB(activationBoxSize*ScalarV(V_NEGHALF),activationBoxSize*ScalarV(V_HALF));
		return activationBox.ContainsPointFlat(Vec2V(relativePos.x,relativePos.y));
	}
	else
	{
		return m_ActivationBox.IsInside(Vector2(g_AudioEngine.GetEnvironment().GetVolumeListenerPosition().GetXf(),g_AudioEngine.GetEnvironment().GetVolumeListenerPosition().GetYf()));
	}
}
// Computes the closest point to the player in the shore
void audShoreLine::CalculateCentrePoint(const Vector3& closestPoint,const Vector3& nextPoint,const Vector3& prevPoint
										, const Vector3& lakeDirectionToNextNode, const Vector3 &lakeDirectionFromPrevNode
										,const Vector3& playerPos,const Vector3& dirToPlayer, Vector3& centrePoint)
{
	// Compute the proyection of the player position in both directions ( vPNext,vPPrev ) 

	f32 vPNextMag = dirToPlayer.Dot(lakeDirectionToNextNode) ; 
	f32 vPPrevMag = dirToPlayer.Dot(lakeDirectionFromPrevNode) ; 
	Vector3 vPNext = (vPNextMag * lakeDirectionToNextNode);
	Vector3 vPPrev = (vPPrevMag * lakeDirectionFromPrevNode);

	vPNext = playerPos - (dirToPlayer - vPNext);
	vPPrev = playerPos - (dirToPlayer - vPPrev);

	// Make sure the points don't go out of the shore.
	vPNext.x = Clamp(vPNext.x,Min(nextPoint.x,closestPoint.x),Max(nextPoint.x,closestPoint.x));
	vPNext.y = Clamp(vPNext.y,Min(nextPoint.y,closestPoint.y),Max(nextPoint.y,closestPoint.y));

	vPPrev.x = Clamp(vPPrev.x,Min(prevPoint.x,closestPoint.x),Max(prevPoint.x,closestPoint.x));
	vPPrev.y = Clamp(vPPrev.y,Min(prevPoint.y,closestPoint.y),Max(prevPoint.y,closestPoint.y));

	// Now interpolate vPNext and vPPrev to get the closest point to the player on the shore.
	// The deltaCorrections are used to control undesired jumps on concave shapes.
	f32 deltaCorrectionVPNext= 1.f;
	f32 deltaCorrectionVPrev = 1.f;

	Vector3 vPNextToNextPoint =  nextPoint - vPNext;
	f32 vPNextToNextPointMag = vPNextToNextPoint.Mag2();

	Vector3 vPNextToClosestPoint =  vPNext - closestPoint;
	f32 vPNextToClosestPointMag = vPNextToClosestPoint.Mag2();

	deltaCorrectionVPNext = fabs(vPNextToNextPointMag - vPNextToClosestPointMag)/ Max(vPNextToNextPointMag,vPNextToClosestPointMag);

	Vector3 closestPointToVPPrev =  closestPoint - vPPrev;
	f32 closestPointToVPPrevMag = closestPointToVPPrev.Mag2();

	Vector3 vPPrevToPrevPoint =  vPPrev - prevPoint;
	f32 vPPrevToPrevPointMag = vPPrevToPrevPoint.Mag2();
	deltaCorrectionVPrev = fabs(closestPointToVPPrevMag - vPPrevToPrevPointMag)/ Max(closestPointToVPPrevMag,vPPrevToPrevPointMag);

	if( FPIsFinite(deltaCorrectionVPNext * deltaCorrectionVPrev) )
	{
		f32 playerToVPNextMag = (playerPos - vPNext).Mag2();
		f32 playerToVPPrevMag = (playerPos - vPPrev).Mag2();

		f32 playerToVPNextMagFixed = playerToVPNextMag * deltaCorrectionVPNext;
		f32 playerToVPPrevMagFixed = playerToVPPrevMag * deltaCorrectionVPrev;

		// Now that the error is corrected, interpolate
		centrePoint = (vPNext *(playerToVPPrevMagFixed/(playerToVPNextMagFixed + playerToVPPrevMagFixed))) + (vPPrev *(playerToVPNextMagFixed/(playerToVPNextMagFixed + playerToVPPrevMagFixed)));
		naAssertf(FPIsFinite(centrePoint.Mag2()),"invalid river centre point");
	}
	else
	{
		// default to the last successfully calculated.
		centrePoint = m_CentrePoint;
		naWarningf("Got non-finite results when calculating the shore closest point, defaulting to the last correctly calculated one.");
	}
}

void audShoreLine::CalculateLeftPoint(const Vector3& /*centrePoint*/,const Vector3 &/*lakeDirectionToNextNode*/, const Vector3 &/*lakeDirectionFromPrevNode*/
									,Vector3 &/*leftPoint*/,const f32 /*distance*/, const u8 /*closestRiverIdx*/)
{

}
void audShoreLine::CalculateRightPoint(const Vector3& /*centrePoint*/,const Vector3 &/*lakeDirectionToNextNode*/, const Vector3 &/*lakeDirectionFromPrevNode*/
										,Vector3 &/*rightPoint*/,const f32 /*distance*/, const u8 /*closestRiverIdx*/)
{

}
void audShoreLine::StopRainOnWaterSound()
{
	if(sm_RainOnWaterSound)
	{
		sm_RainOnWaterSound->SetReleaseTime(300);
		sm_RainOnWaterSound->StopAndForget();
	}
}
#if __BANK
//------------------------------------------------------------------------------------------------------------------------------
void audShoreLine::EditShorelinePointWidth(const u32 /*pointIdx*/)
{
}
//------------------------------------------------------------------------------------------------------------------------------
void audShoreLine::SetActivationBoxSize(f32 width,f32 height)
{
	m_Width = width;
	m_Height = height;
}
//------------------------------------------------------------------------------------------------------------------------------
void audShoreLine::MoveActivationBox(const Vector2 &vec)
{
	m_ActivationBox = fwRect(vec, m_ActivationBox.GetWidth()/2.0f, m_ActivationBox.GetHeight()/2.0f);
}
//------------------------------------------------------------------------------------------------------------------------------
void audShoreLine::RotateActivationBox(f32 rotAngle)
{
	m_RotAngle += rotAngle;
	if(m_RotAngle > 360)
	{
		m_RotAngle -= 360;
	}
	else if(m_RotAngle < 0)
	{
		m_RotAngle += 360;
	}
	Vector2 centre; 
	m_ActivationBox.GetCentre(centre.x,centre.y);
	Vector2 topLeft = Vector2(-m_Width/2.f,m_Height/2.f);
	Vector2 topRight = Vector2(m_Width/2.f,m_Height/2.f);
	Vector2 bottomLeft = Vector2(-m_Width/2.f,-m_Height/2.f);
	Vector2 bottomRight = Vector2(m_Width/2.f,-m_Height/2.f);

	// Rotate all the points by the required amount
	topLeft.Rotate(m_RotAngle* DtoR);
	topRight.Rotate(m_RotAngle* DtoR);
	bottomLeft.Rotate(m_RotAngle* DtoR);
	bottomRight.Rotate(m_RotAngle* DtoR);

	// Now work out the smallest box that contains all the points
	float minXSize = Min(Min(topLeft.x, topRight.x), Min(bottomLeft.x, bottomRight.x));
	float minYSize = Min(Min(topLeft.y, topRight.y), Min(bottomLeft.y, bottomRight.y));
	float maxXSize = Max(Max(topLeft.x, topRight.x), Max(bottomLeft.x, bottomRight.x));
	float maxYSize = Max(Max(topLeft.y, topRight.y), Max(bottomLeft.y, bottomRight.y));

	m_ActivationBox = fwRect(centre, (maxXSize - minXSize)/2.0f, (maxYSize - minYSize)/2.0f);
}
//------------------------------------------------------------------------------------------------------------------------------
void audShoreLine::DrawActivationBox(bool useSizeFromData)const 
{
	if(m_WaterHeights.GetCount() && !m_ActivationBox.IsInvalidOrZeroArea())
	{
		Mat34V test(V_IDENTITY);
		Mat34VRotateLocalZ(test, ScalarVFromF32(m_RotAngle * DtoR));
		Vector2 centre; 
		m_ActivationBox.GetCentre(centre.x,centre.y);
		test.SetCol3(Vec3V(centre.x,centre.y,m_WaterHeights[0]));
		Vector3 sizeMin = Vector3(m_Width,m_Height,m_WaterHeights[0] + 10.f);
		Vector3 sizeMax = Vector3(m_Width,m_Height,m_WaterHeights[0]);
		if(useSizeFromData)
		{
			sizeMin = Vector3(m_ActivationBox.GetWidth(),m_ActivationBox.GetHeight(),m_WaterHeights[0] + 10.f);
			sizeMax = Vector3(m_ActivationBox.GetWidth(),m_ActivationBox.GetHeight(),m_WaterHeights[0]);
		}
		const Vec3V boxMax = VECTOR3_TO_VEC3V(sizeMin * VEC3_HALF);
		const Vec3V boxMin = VECTOR3_TO_VEC3V(sizeMax * -VEC3_HALF);
		Color32 color = Color32(0,255,0,128);
		if(!m_IsActive)
		{
			color = Color32(255,0,0,128);
		}
		grcDebugDraw::BoxOriented(boxMin, boxMax, test, color);
	}
}
//------------------------------------------------------------------------------------------------------------------------------
void audShoreLine::RenderDebug()const 
{
	if(m_WaterHeights.GetCount() && !m_ActivationBox.IsInvalidOrZeroArea())
	{
		Vector3 minPoint = Vector3(m_ActivationBox.GetLeft(),m_ActivationBox.GetBottom(),m_WaterHeights[0]);
		Vector3 maxPoint = Vector3(m_ActivationBox.GetRight(),m_ActivationBox.GetTop(),m_WaterHeights[0] + 10.f);
		CVectorMap::DrawRectAxisAligned(minPoint, maxPoint, Color32(0,255,0,255),true);
	}
}
//------------------------------------------------------------------------------------------------------------------------------
void audShoreLine::SerialiseActivationBox(char* xmlMsg, char* tmpBuf)
{
	Vector2 centre; 
	m_ActivationBox.GetCentre(centre.x,centre.y);
	// NOTE - this code is missing newlines in the generated XML, so I'm not sure what purpose the indenting has.
	// Ideally the caller would pass in the buffer and a size, and do this in a single formatf call so overruns are caught.
	sprintf(tmpBuf, "			<ActivationBox>");
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "				<Center>");
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "					<X>%f</X>",centre.x);
	strcat(xmlMsg, tmpBuf);				   
	sprintf(tmpBuf, "					<Y>%f</Y>",centre.y);
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "				</Center>");
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "				<Size>");
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "					<Width>%f</Width>",m_Width);
	strcat(xmlMsg, tmpBuf);				   
	sprintf(tmpBuf, "					<Height>%f</Height>",m_Height);
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "				</Size>");
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "				<RotationAngle>%f</RotationAngle>",m_RotAngle);
	strcat(xmlMsg, tmpBuf);
	sprintf(tmpBuf, "			</ActivationBox>");
	strcat(xmlMsg, tmpBuf);
}
//------------------------------------------------------------------------------------------------------------------------------
void audShoreLine::Cancel()
{
}
//------------------------------------------------------------------------------------------------------------------------------
Vector2 audShoreLine::GetPoint(const u32 pointIdx)
{
	Vector2 shorePoint = Vector2(0.f,0.f);
	if(naVerifyf(pointIdx < m_ShoreLinePoints.GetCount(),"Wrong point index"))
	{
		shorePoint.x = m_ShoreLinePoints[pointIdx].x;
		shorePoint.y = m_ShoreLinePoints[pointIdx].y;
	}
	return shorePoint;
}
//------------------------------------------------------------------------------------------------------------------------------
bool audShoreLine::GetPointZ(const u32 pointIdx,Vector3 &point)
{
	if(pointIdx < m_WaterHeights.GetCount())
	{
		if(m_WaterHeights[pointIdx])
		{
			point.x = m_ShoreLinePoints[pointIdx].x;
			point.y = m_ShoreLinePoints[pointIdx].y;
			point.z = 100.f;//m_WaterHeights[pointIdx];
			return true;
		}
	}
	return false;
}
// ----------------------------------------------------------------
void audShoreLine::AddWidgets(bkBank &bank)
{
	bank.AddSlider("sm_DistanceThresholdToStopSounds", &sm_DistanceThresholdToStopSounds , 0.f, 10000.f, 0.1f);
	bank.AddToggle("sm_DrawWaterBehaviour",&sm_DrawWaterBehaviour);
}
// ----------------------------------------------------------------
void audShoreLine::AddEditorWidgets(bkBank &bank)
{
	bank.PushGroup("Common parameters");
	bank.PopGroup();
}
#endif
