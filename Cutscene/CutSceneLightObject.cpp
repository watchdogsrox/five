/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CutsSceneLightObject.cpp
// PURPOSE : Describes an in game light.
// AUTHOR  : Thomas French
// STARTED : 1 / 10 / 2009
//
/////////////////////////////////////////////////////////////////////////////////

//Rage includes
#include "grcore/debugdraw.h"

//Game includes
#include "audio/ambience/ambientaudioentity.h"
#include "cutscene/CutSceneManagerNew.h"
#include "CutScene/CutSceneLightObject.h"
#include "cutscene/CutSceneCameraEntity.h"
#include "cutscene/animatedmodelentity.h"
#include "Cutscene/cutscene_channel.h"
#include "Game/clock.h"
#include "fwMaths/random.h"
#include "renderer/lights/lights.h"
#include "renderer/color.h"
#include "renderer/Util/Util.h"
#include "timecycle/TimeCycle.h"
#include "Vfx/Misc/Coronas.h"
#include "control/replay/ReplayLightingManager.h"

/////////////////////////////////////////////////////////////////////////////////////

ANIM_OPTIMISATIONS ()

/////////////////////////////////////////////////////////////////////////////////////


CCutSceneLight::CCutSceneLight(const cutfLightObject* pLightObject)
{
	m_pCutfLightObj = pLightObject; 

	m_bUpdateStaticLights = false;
	
	m_AttachParentId = -1; 
	m_AttachBoneHash = 0; 
#if __BANK
	m_pEditLightInfo = NULL;
	m_vRenderLightMat.Identity(); 
	m_DebugIntensity = 0.0f; 
#endif
}

/////////////////////////////////////////////////////////////////////////////////////

CCutSceneLight::~CCutSceneLight()
{
	m_pCutfLightObj = NULL; 
}


float CCutSceneLight::CalulateIntensityBasedOnHourFlags(u32 timeFlags)
{
	float intensity = 0.0f;

	u32 hour = CClock::GetHour();

	// If light is on, make sure its on
	if (timeFlags & (1 << hour))
	{
		intensity = 1.0;
	}

	return intensity; 
}

CEntity* CCutSceneLight::GetAttachParent(cutsManager* pManager, u32 AttachParentId)
{
	cutsEntity* pCutsEntity = pManager->GetEntityByObjectId( AttachParentId); 

	CEntity* pGameEntity = NULL; 

	if(pCutsEntity && (pCutsEntity->GetType() == CUTSCENE_PROP_GAME_ENITITY ||
		pCutsEntity->GetType() == CUTSCENE_WEAPON_GAME_ENTITY ||
		pCutsEntity->GetType() == CUTSCENE_ACTOR_GAME_ENITITY ||
		pCutsEntity->GetType() == CUTSCENE_VEHICLE_GAME_ENITITY))
	{
		CCutsceneAnimatedModelEntity* pModelEntity = static_cast<CCutsceneAnimatedModelEntity*>(pCutsEntity); 
		pGameEntity =  pModelEntity->GetGameEntity(); 
	}
	return pGameEntity; 
}

s32 CCutSceneLight::GetAttachParentsBoneIndex(cutsManager* pManager, u16 BoneHash, u32 AttachParentId)
{


	s32 boneIndex = -1;

	CEntity* pGameEntity = GetAttachParent( pManager, AttachParentId); 

	if(pGameEntity && pGameEntity->GetIsDynamic())
	{
		CDynamicEntity* pDynamicEntity = static_cast<CDynamicEntity*>(pGameEntity); 

		if(pDynamicEntity->GetSkeleton())
		{	
			pDynamicEntity->GetSkeletonData().ConvertBoneIdToIndex(BoneHash, boneIndex); 

		}
	}	
	return boneIndex; 
}

#if __BANK
bool CCutSceneLight::UpdateLightDebugging(cutsManager* pManager, const Matrix34& ParentMat, const char* Label)
{
	bool isUpdating = false; 

	//if ( (m_iSelectedLightIndex > 0) && (m_iSelectedLightIndex <= m_editLightList.GetCount() ) )
	if(m_pEditLightInfo)
	{
		m_Helper.SetLabel(Label);

		if(m_pEditLightInfo->bOverridePosition || m_pEditLightInfo->bOverrideDirection)
		{		
			//get the authoring helper in local space
			Matrix34 AuthoringMatLocal;
			m_pEditLightInfo->AuthoringQuat.Normalize(); 
			AuthoringMatLocal.FromQuaternion(m_pEditLightInfo->AuthoringQuat); 
			AuthoringMatLocal.d = m_pEditLightInfo->AuthoringPos; 

			Matrix34 UpdatedAuthoringMatLocal; 

			if(m_Helper.Update(AuthoringMatLocal, 1.0f, VEC3_ZERO, UpdatedAuthoringMatLocal, ParentMat))
			{
				if(!UpdatedAuthoringMatLocal.IsOrthonormal())
				{
					Assertf(UpdatedAuthoringMatLocal.IsOrthonormal(), "not normal mat, after writing out helper"); 
				}

				isUpdating = true; 
			}
			
			//set to scene space
			Matrix34 UpdatedAuthoringMatWorld(UpdatedAuthoringMatLocal); 
			
			UpdatedAuthoringMatWorld.Dot(ParentMat); 

			//Get Light direction world
			Vector3 LightDirWorld; 
			UpdatedAuthoringMatWorld.Transform3x3(Vector3(0.0f, 0.0f, -1.0f), LightDirWorld);

			//draw the light direction
			Color32 lightColor(m_pEditLightInfo->vColor); 
			grcDebugDraw::Line(UpdatedAuthoringMatWorld.d, UpdatedAuthoringMatWorld.d + LightDirWorld, lightColor, lightColor); 

			if(m_pEditLightInfo->bOverrideDirection)
			{
				Vector3 orient;
				UpdatedAuthoringMatWorld.ToEulersXYZ(orient);
				if(pManager->GetSelectedLightInfo() == m_pEditLightInfo)
				{
					pManager->SetLightOrientation(orient * RtoD);
				}

				UpdatedAuthoringMatLocal.ToQuaternion(m_pEditLightInfo->AuthoringQuat);
			
				m_pEditLightInfo->vDirection = LightDirWorld;  
			}

			if(m_pEditLightInfo->bOverridePosition)
			{
				//update the widgets
				if(pManager->GetSelectedLightInfo() == m_pEditLightInfo)
				{
					pManager->SetLightPosition(UpdatedAuthoringMatWorld.d);
				}
				
				m_pEditLightInfo->vPosition = UpdatedAuthoringMatLocal.d;
				m_pEditLightInfo->AuthoringPos = UpdatedAuthoringMatLocal.d;
			}
		}
	}

	return isUpdating; 
}
#endif// __BANK

/////////////////////////////////////////////////////////////////////////////////////
//	PURPOSE: Updates the light, based on the anim track info. Cut scene lights are 
//	added every frame only if the intensity track for anim is >0.

void CCutSceneLight::UpdateLight(cutsManager* pManager)
{
#if __BANK
	if(m_pEditLightInfo)
	{
		if(m_pEditLightInfo->markedForDelete)
		{
			return; 
		}
	}
#endif //__BANK

	if(m_pCutfLightObj )
	{
		CutSceneManager *pCutManager = static_cast< CutSceneManager * >(pManager);
		
		bool pushLightsEarly = !pCutManager->GetPostSceneUpdate();    // early light goes into previous list

		bool bCanUpdate = false; 

		float fLightIntensity = 0.0f;
		Vector3 vLightPositionFromData(VEC3_ZERO); 
		Vector3 LightDirectionFromData(VEC3_ZERO); 

		Vector3 vLightColor(VEC3_ZERO);
		float fLightFallOff = 0.0f;
		float fLightConeAngle = 0.0f;
		float fInnerConeAngle = 0.0f; 
		u32 cutsceneLightFlags = m_pCutfLightObj->GetLightFlags(); 
		u32 HourFlags = 0; 
		Vector4 vVolumeOuterColourAndIntensity(1.0f, 1.0f, 1.0f, 1.0f);  
		float fVolumeIntensity = 0.0f; 
		float fVolumeSizeScale = 0.0f; 
		float fCoronaSize = 0.0f; 
		float fCoronaIntensity = 0.0f; 
		float fCoronaZbias = 0.0f; 
		float fExponentiolFallOff = 0.0f; 
		float fShadowBlur = 0.0f; 

		//The the scene Matrix
		Matrix34 SceneMat(M34_IDENTITY);
		CutSceneManager::GetInstance()->GetSceneOrientationMatrix(SceneMat);
		//fill static info for all lights
		HourFlags = m_pCutfLightObj->GetLightHourFlags(); 
		vVolumeOuterColourAndIntensity = m_pCutfLightObj->GetVolumeOuterColourAndIntensity(); 
		fVolumeIntensity = m_pCutfLightObj->GetVolumeIntensity(); 
		fVolumeSizeScale = m_pCutfLightObj->GetVolumeSizeScale(); 
		fCoronaIntensity = m_pCutfLightObj->GetCoronaIntensity(); 
		fCoronaSize = m_pCutfLightObj->GetCoronaSize(); 
		fCoronaZbias = m_pCutfLightObj->GetCoronaZBias(); 
		fInnerConeAngle = m_pCutfLightObj->GetInnerConeAngle(); 
		fShadowBlur = m_pCutfLightObj->GetShadowBlur(); 

		u16 boneHash =  m_AttachBoneHash; 

		s32 BoneIndex = -1; 
#if __BANK 		
		if(m_pCutfLightObj->GetType() ==  CUTSCENE_LIGHT_OBJECT_TYPE)
		{
			vLightPositionFromData = m_pCutfLightObj->GetLightPosition();
		}
#endif

		if(m_pCutfLightObj->GetType() ==  CUTSCENE_LIGHT_OBJECT_TYPE && m_bUpdateStaticLights)
		{
			fLightIntensity = m_pCutfLightObj->GetLightIntensity(); 
			
			vLightPositionFromData = m_pCutfLightObj->GetLightPosition(); 
			LightDirectionFromData = m_pCutfLightObj->GetLightDirection();
			
			if(m_AttachParentId > 0 )
			{	
				u32 parentId = (u32)m_AttachParentId; 

				CEntity* pGameEntity = GetAttachParent(pManager,parentId); 

				if(pGameEntity)
				{
					BoneIndex = GetAttachParentsBoneIndex(pManager, boneHash, parentId); 
					
					cutsceneAssertf(BoneIndex !=-1, "%d is an invalid bone hash for attachparent %d for light: %s",boneHash, parentId, m_pCutfLightObj->GetDisplayName().c_str()); 

					if(BoneIndex != -1)
					{
						Matrix34 BoneGlobal; 
						pGameEntity->GetSkeleton()->GetGlobalMtx(BoneIndex, RC_MAT34V(BoneGlobal)); 

				

#if __BANK
						char Label[128];

						formatf(Label, 127, "%s (attached:: Cannot Edit)", m_pCutfLightObj->GetDisplayName().c_str()); 

						UpdateLightDebugging(pManager, BoneGlobal, Label);


						/*		if(m_pEditLightInfo->bOverridePosition)
						{
						vLightPositionFromData = m_pEditLightInfo->vPosition; 
						}

						if(m_pEditLightInfo->bOverrideDirection)
						{
						LightDirectionFromData = VEC3_ZERO; 
						m_pEditLightInfo->AuthoringQuat.ToEulers(LightDirectionFromData); */
						//}
#endif

						if (cutsceneVerifyf(BoneGlobal.a.Mag2()>0.01f, "Invalid rotation in skeleton bone matrix for light transform"))
						{
							BoneGlobal.Transform(vLightPositionFromData);
							BoneGlobal.Transform3x3(LightDirectionFromData); 
						}
						else
						{
							vLightPositionFromData += BoneGlobal.d; // just add the offset since the rotation part if trashed.
						}
					}
				}
			}
#if __BANK
			else
			{
				if(m_pEditLightInfo && pManager->GetSelectedLightInfo() == m_pEditLightInfo)
				{
					Matrix34 sceneMat; 
					pManager->GetSceneOrientationMatrix(sceneMat); 
					UpdateLightDebugging(pManager, sceneMat, m_pCutfLightObj->GetDisplayName().c_str());
				}
			}
#endif

			vLightColor = m_pCutfLightObj->GetLightColour(); 
			fLightFallOff = m_pCutfLightObj->GetLightFallOff(); 
			fLightConeAngle = m_pCutfLightObj->GetLightConeAngle(); 
			fExponentiolFallOff = m_pCutfLightObj->GetExponentialFallOff(); 

			bCanUpdate = true; 
		}
		else if (m_Animation.GetClip())
		{	
			const crClip* pClip = m_Animation.GetClip(); 

			float fPhase = 0.0f;
			fPhase = pCutManager->GetPhaseUpdateAmount( pClip, m_Animation.GetStartTime() ); 
	
			//Vector3 vTan = Vector3(0.0f, 1.0f, 0.0f);

			m_Animation.GetFloatValue(m_Animation.GetClip(),kTrackLightIntensity, fPhase, fLightIntensity);

#if __BANK			
			m_Animation.GetVector3Value(m_Animation.GetClip(),kTrackBoneTranslation, fPhase, vLightPositionFromData);
#endif
			if ((fLightIntensity != 0.0f) BANK_ONLY (|| (m_pEditLightInfo && m_pEditLightInfo->bOverrideIntensity)))   // only add the light if intensity is not 0
			{		
				m_Animation.GetVector3Value(m_Animation.GetClip(),kTrackBoneTranslation, fPhase, vLightPositionFromData);
				
				Quaternion quat; 
				if(m_Animation.GetQuaternionValue(m_Animation.GetClip(),kTrackBoneRotation, fPhase, quat))
				{

					Matrix34 lightMat(M34_IDENTITY); 
					lightMat.FromQuaternion(quat); 

					lightMat.Transform3x3(Vector3(0,0,-1), LightDirectionFromData);
				}
				else
				{
					m_Animation.GetVector3Value(m_Animation.GetClip(),kTrackLightDirection, fPhase, LightDirectionFromData);
				}
				
				if(m_AttachParentId > 0)
				{
					u32 parentId = (u32)m_AttachParentId; 

					CEntity* pGameEntity = GetAttachParent(pManager,parentId); 

					if(pGameEntity)
					{
						BoneIndex = GetAttachParentsBoneIndex(pManager, boneHash, parentId); 

						cutsceneAssertf(BoneIndex !=-1, "%d is an invalid bone hash for attach parent %d for light: %s",boneHash, parentId, m_pCutfLightObj->GetDisplayName().c_str()); 

						if(BoneIndex != -1)
						{
							Matrix34 BoneGlobal; 
							pGameEntity->GetSkeleton()->GetGlobalMtx(BoneIndex, RC_MAT34V(BoneGlobal)); 
							if (cutsceneVerifyf(BoneGlobal.a.Mag2()>0.01f, "Invalid rotation in skeleton bone matrix for light transform"))
							{
								BoneGlobal.Transform(vLightPositionFromData);
								BoneGlobal.Transform3x3(LightDirectionFromData); 
							}
							else
							{
								vLightPositionFromData += BoneGlobal.d; // just add the offset since the rotation part if trashed.
							}
						}
					}
				}
				m_Animation.GetVector3Value(m_Animation.GetClip(),kTrackColor, fPhase,vLightColor);

				m_Animation.GetFloatValue(m_Animation.GetClip(),kTrackLightFallOff, fPhase, fLightFallOff);

				m_Animation.GetFloatValue(m_Animation.GetClip(),kTrackLightConeAngle, fPhase, fLightConeAngle);		

				m_Animation.GetFloatValue(m_Animation.GetClip(), kTrackLightExpFallOff, fPhase, fExponentiolFallOff); 
				
				//replace 
				//fInnerConeAngle = fLightConeAngle * 0.5f; 
				//with
				//TO MAKE ANIMATED AND STATIC LIGHTS SIMILAR 

				//m_Animation.GetFloatValue(m_Animation.GetClip(),kTrackLightInnerConeAngle, fPhase, fInnerConeAngle);	
				//once available

				//Need the following anim tracks
				//add the flags as clip properties, non animateable

		  
				// m_Animation.GetQauternionValue(m_Animation.GetClip(),kTrackLightVolumeOuterColourAndIntensity, fPhase, vVolumeOuterColourAndIntensity);	
				// m_Animation.GetFloatValue(m_Animation.GetClip(),kTrackLightVolumeIntensity, fPhase, fVolumeIntensity);
				// m_Animation.GetFloatValue(m_Animation.GetClip(),kTrackLightVolumeSizeScale, fPhase, fVolumeSizeScale);
				// m_Animation.GetFloatValue(m_Animation.GetClip(),kTrackLightCoronaIntensity, fPhase, fCoronaIntensity);	
				// m_Animation.GetFloatValue(m_Animation.GetClip(),kTrackLightCoronaSize, fPhase, fCoronaSize);
				// m_Animation.GetFloatValue(m_Animation.GetClip(),kTrackLightCoronaZBias, fPhase, fCoronaZbias);
				// m_Animation.GetFloatValue(m_Animation.GetClip(),kTrackLightExponentialFallOff, fPhase, fExponentiolFallOff);
				// 
				bCanUpdate = true;
			}
		}


		if(bCanUpdate)
		{
#if CUTSCENE_LIGHT_EVENT_LOGGING
			bool bLightAdded = false;
#endif

#if __BANK
			if(m_pEditLightInfo)
			{
				m_pEditLightInfo->IsActive = true; 

			}
			
	#endif
			
			Vector3 vLightPosition(VEC3_ZERO); 
			Vector3 vLightDirection(0.0f, 0.0f, -1.0f); 
			
			if(BoneIndex == -1 )
			{
				Vector3 LightTempDirection = LightDirectionFromData; 
				Vector3 LightTempPosition = vLightPositionFromData; 
#if __BANK
				if(m_pEditLightInfo)
				{
					if(m_pEditLightInfo->bOverridePosition)
					{
						LightTempPosition = m_pEditLightInfo->vPosition; 
					}
					else
					{
						m_pEditLightInfo->vPosition = LightTempPosition;
					}
				}
#endif
				Matrix34 lighMat; 
				camFrame::ComputeWorldMatrixFromFront(LightTempDirection, lighMat); 

				Matrix34 OriginalSceneMat(M34_IDENTITY);
				CutSceneManager::GetInstance()->GetOriginalSceneOrientationMatrix(OriginalSceneMat);
				OriginalSceneMat.Transform(LightTempPosition, lighMat.d); 

				//local space light mat
				lighMat.DotTranspose(OriginalSceneMat); 

				lighMat.Dot(SceneMat); 

				vLightPosition = lighMat.d; 
				vLightDirection = lighMat.b;
#if __BANK
				m_lightMatWorld = lighMat; 
#endif
			}
			else
			{
				vLightPosition = vLightPositionFromData; 
				vLightDirection = LightDirectionFromData; 

			}
	#if __BANK
			OverrideLights (vLightDirection, vLightColor, fLightFallOff, fLightConeAngle, fLightIntensity, cutsceneLightFlags, HourFlags);
			OverrideVolumeSettings(fVolumeIntensity, fVolumeSizeScale, vVolumeOuterColourAndIntensity);
			OverrideCoronaSettings(fCoronaIntensity, fCoronaSize, fCoronaZbias); 
			OverrideExponetiolFallOff(fExponentiolFallOff); 
			OverrideInnerConeAngle(fInnerConeAngle); 
			m_vRenderLightMat.d = vLightPosition; 

			if(m_pEditLightInfo)
			{
				if(m_pEditLightInfo->bOverrideShadowBlur)
				{
					fShadowBlur = m_pEditLightInfo->ShadowBlur; 
				}
				else
				{
					m_pEditLightInfo->ShadowBlur = fShadowBlur;
				}
			}
	
	#endif	

			if(pManager && pManager->IsFlagSet(cutsceneLightFlags, CUTSCENE_LIGHTFLAG_IS_CHARACTER_LIGHT))
			{
				CutSceneManager *pCutManager = static_cast< CutSceneManager * >(pManager);
				
				bool UseAsIntensityModifier = false; 

				if(pManager->IsFlagSet(cutsceneLightFlags, CUTSCENE_LIGHTFLAG_CHARACTER_LIGHT_INTENSITY_AS_MULTIPLIER))
				{
					UseAsIntensityModifier = true; 
				}
				
				if(pCutManager && pCutManager->GetCamEntity())
				{
					CCutSceneCameraEntity* cam = const_cast<CCutSceneCameraEntity*>(pCutManager->GetCamEntity()); 
					
					//highjacking the corona size for night intensity value
					float NightIntensity = fCoronaSize; 

					cam->SetCharacterLightValues(pCutManager->GetTime(), fLightIntensity, NightIntensity, vLightDirection, vLightColor, false, UseAsIntensityModifier, true  ); 
#if __BANK
					m_DebugIntensity = fLightIntensity; 
#endif
				}
				
				return; 
			}


			fLightIntensity = fLightIntensity * CalulateIntensityBasedOnHourFlags(HourFlags); 
			
			//convert the cutscene light flags to game light flags

			u32 lightFlags = 0; 
			
			if(pManager->IsFlagSet(cutsceneLightFlags, CUTSCENE_LIGHTFLAG_NO_SPECULAR))
			{
				lightFlags |= LIGHTFLAG_NO_SPECULAR; 
			}
			
			if(pManager->IsFlagSet(cutsceneLightFlags, CUTSCENE_LIGHTFLAG_DONT_LIGHT_ALPHA))
			{
				lightFlags |= LIGHTFLAG_DONT_LIGHT_ALPHA; 
			}
			
			if(pManager->IsFlagSet(cutsceneLightFlags, CUTSCENE_LIGHTFLAG_BOTH_INTERIOR_AND_EXTERIOR))
			{
				lightFlags |= LIGHTFLAG_BOTH_INTERIOR_AND_EXTERIOR; 
			}

			if(pManager->IsFlagSet(cutsceneLightFlags, CUTSCENE_LIGHTFLAG_ONLY_IN_REFLECTION))
			{
				lightFlags |=  LIGHTFLAG_ONLY_IN_REFLECTION; 
			}

			if(pManager->IsFlagSet(cutsceneLightFlags, CUTSCENE_LIGHTFLAG_NOT_IN_REFLECTION))
			{
				lightFlags |=  LIGHTFLAG_NOT_IN_REFLECTION; 
			}
			
			if (m_pCutfLightObj && m_pCutfLightObj->GetLightProperty() == cutfLightObject::LP_CASTS_SHADOWS)
			{
				cutsceneLightFlags |= CUTSCENE_LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS; 
				cutsceneLightFlags |= CUTSCENE_LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS;
		
				if (m_pCutfLightObj->GetType() ==  CUTSCENE_ANIMATED_LIGHT_OBJECT_TYPE)
					lightFlags |= LIGHTFLAG_MOVING_LIGHT_SOURCE;
			}

			//calc from sun: Calculate up front to calculate intensity based on the sun, no sun means no light
			if (pManager->IsFlagSet(cutsceneLightFlags, CUTSCENE_LIGHTFLAG_CALC_FROM_SUN))
			{
				//use hacked non-modified directional lights
				Vec4V vBaseDirLight = g_timeCycle.GetStaleBaseDirLight();
				Vector4 TimeCycle_Directional = RCC_VECTOR4(vBaseDirLight);
				vLightColor *= Vector3(TimeCycle_Directional.x, TimeCycle_Directional.y, TimeCycle_Directional.z);
				float dirScale = TimeCycle_Directional.w;
				fLightIntensity *= dirScale;

				lightFlags |= LIGHTFLAG_CALC_FROM_SUN; 
			}


			if( fLightIntensity > 0.0f )
			{
				lightFlags |= LIGHTFLAG_CUTSCENE;
		
				//corona only 
				
				if(pManager->IsFlagSet(cutsceneLightFlags, CUTSCENE_LIGHTFLAG_CORONA_ONLY))
				{
					u8 coronaFlags = 0;
					coronaFlags |= (m_pCutfLightObj->GetLightType() == LIGHT_TYPE_SPOT) ? CORONA_HAS_DIRECTION : 0;
					coronaFlags |= ((cutsceneLightFlags & CUTSCENE_LIGHTFLAG_NOT_IN_REFLECTION) != 0) ? CORONA_DONT_REFLECT : 0;
					coronaFlags |= ((cutsceneLightFlags & CUTSCENE_LIGHTFLAG_ONLY_IN_REFLECTION) != 0) ? CORONA_ONLY_IN_REFLECTION : 0;

					Color32 Color(vLightColor); 
					g_coronas.Register(
						RCC_VEC3V(vLightPosition),
						fCoronaSize,
						Color,
						fCoronaIntensity* fLightIntensity,
						fCoronaZbias,
						RCC_VEC3V(vLightDirection),
						1.0f,
						fInnerConeAngle,
						(fLightConeAngle > 0.0f ? fLightConeAngle : CORONAS_DEF_DIR_LIGHT_CONE_ANGLE_OUTER),
						coronaFlags);
				}
				else
				{
					fwUniqueObjId shadowTrackingId = 0;

					//do shadows up front cause we are adding flags
					bool castShadows = false; 
					if(pManager->IsFlagSet(cutsceneLightFlags, CUTSCENE_LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS))
					{
						lightFlags |= LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS; 
						castShadows = true; 
					}
					
					if(pManager->IsFlagSet(cutsceneLightFlags, CUTSCENE_LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS))
					{
						lightFlags |= LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS; 
						castShadows = true; 
					}

					if(castShadows)
					{	
						shadowTrackingId = fwIdKeyGenerator::Get(&m_pCutfLightObj);
						lightFlags |= LIGHTFLAG_CAST_SHADOWS; 
					}
					
					if(pManager->IsFlagSet(cutsceneLightFlags, CUTSCENE_LIGHTFLAG_DRAW_VOLUME))
					{
						lightFlags |= LIGHTFLAG_DRAW_VOLUME;
					}

					vLightDirection.Normalize(); 
					Vector3 ltan;
					ltan.Cross(vLightDirection, FindMinAbsAxis(vLightDirection));
					ltan.Normalize();

					// Setup light as a diffuse/reflector - only spots have a valid direction
					// Cannaot apply to lights pushed early as the time cycle is not valid
					if(!pushLightsEarly)
					{
						if (m_pCutfLightObj->GetLightType() == LIGHT_TYPE_SPOT)
						{
							if (pManager->IsFlagSet(cutsceneLightFlags, CUTSCENE_LIGHTFLAG_REFLECTOR))
							{
								lightFlags |= LIGHTFLAG_CALC_FROM_SUN;
								fLightIntensity *= Lights::GetReflectorIntensity(vLightDirection);
							}

							if (pManager->IsFlagSet(cutsceneLightFlags, CUTSCENE_LIGHTFLAG_DIFFUSER))
							{
								lightFlags |= LIGHTFLAG_CALC_FROM_SUN;
								fLightIntensity *= Lights::GetDiffuserIntensity(vLightDirection);
							}
						}
					}
#if __BANK
					m_DebugIntensity = fLightIntensity; 
#endif
					CLightSource light(LIGHT_TYPE_NONE, lightFlags, vLightPosition, vLightColor, fLightIntensity, HourFlags);

					switch(m_pCutfLightObj->GetLightType())
					{
						case CUTSCENE_SPOT_LIGHT_TYPE:
						{
							light.SetType(LIGHT_TYPE_SPOT); 
						}
						break; 
						
						case CUTSCENE_POINT_LIGHT_TYPE:
						{
							light.SetType(LIGHT_TYPE_POINT); 
						}
						break; 
					}

					// direction
						light.SetDirTangent(vLightDirection, ltan);

					//radius
						light.SetRadius(fLightFallOff);
					
					// set the spot light parameters. this MUST be after Radius is set
						if (m_pCutfLightObj->GetLightType()==CUTSCENE_SPOT_LIGHT_TYPE)
						{
							light.SetSpotlight(fInnerConeAngle, fLightConeAngle);
						}

					//shadow tracking
						light.SetShadowTrackingId(shadowTrackingId);
						
					//Volume
						if(pManager->IsFlagSet(cutsceneLightFlags, CUTSCENE_LIGHTFLAG_DRAW_VOLUME))
						{
							light.SetLightVolumeIntensity(fVolumeIntensity, fVolumeSizeScale, VECTOR4_TO_VEC4V(vVolumeOuterColourAndIntensity)); 	
						}

						//exponential fall off
						light.SetFalloffExponent(fExponentiolFallOff); 

						light.SetShadowBlur(fShadowBlur); 

					//Audio
					if (!pushLightsEarly)
					{
						if (pManager->IsFlagSet(cutsceneLightFlags, CUTSCENE_LIGHTFLAG_ENABLE_BUZZING) || pManager->IsFlagSet(cutsceneLightFlags, CUTSCENE_LIGHTFLAG_FORCE_BUZZING))
						{
							// we know that bit 0 is set, so subtract 1 to keep in range [0,3]
							//u32 soundIndex = ((localRandomSeed|1) & 3) - 1;
							 u32 soundIndex  = fwRandom::GetRandomNumberInRange( 0, 3);
							static const u32 sounds[] = {
								ATSTRINGHASH("AMBIENCE_NEON_NEON_1", 0x0944d782a),
								ATSTRINGHASH("AMBIENCE_NEON_NEON_2", 0x0ac1da7ca),
								ATSTRINGHASH("AMBIENCE_NEON_NEON_3", 0x078203fd0) };
							fwUniqueObjId uniqueId = fwIdKeyGenerator::Get(this, m_pCutfLightObj->GetObjectId());

							g_AmbientAudioEntity.RegisterEffectAudio(sounds[soundIndex], uniqueId, vLightPosition, NULL);
						}

						if(pManager->IsFlagSet(cutsceneLightFlags, CUTSCENE_LIGHTFLAG_ADD_AMBIENT_LIGHT))
						{
							 Lights::CalculateFromAmbient(light); 
						}
					}

					// extra flags
					// if we should be using the ped only flag, set it in the extraflags for the light.
					bool bIsPedOnlyLight = pManager->IsFlagSet(cutsceneLightFlags, CUTSCENE_LIGHTFLAG_IS_PED_ONLY_LIGHT);
					
					if(bIsPedOnlyLight)
					{
						light.SetExtraFlag(EXTRA_LIGHTFLAG_USE_AS_PED_ONLY);
					}

#if CUTSCENE_LIGHT_EVENT_LOGGING
					bLightAdded = Lights::AddSceneLight(light);

#else
					if(Lights::AddSceneLight(light, NULL, pushLightsEarly))
					{

					}
#endif

#if GTA_REPLAY
					if(CReplayMgr::IsRecording())
					{
						lightPacketData cutsceneLightData;
						cutsceneLightData.PackageData(GetLightCutObject()->GetName(), lightFlags, vLightPosition, vLightColor, fLightIntensity, HourFlags, light.GetType(),
							vLightDirection, ltan, fLightFallOff, fInnerConeAngle, fLightConeAngle, pManager->IsFlagSet(cutsceneLightFlags, CUTSCENE_LIGHTFLAG_DRAW_VOLUME),
							fVolumeIntensity, fVolumeSizeScale, vVolumeOuterColourAndIntensity, fExponentiolFallOff, fShadowBlur, pushLightsEarly,
							pManager->IsFlagSet(cutsceneLightFlags, CUTSCENE_LIGHTFLAG_ADD_AMBIENT_LIGHT));

						ReplayLightingManager::RecordCutsceneLight(cutsceneLightData);
					}
#endif
	#if __BANK 					
					Vector3 C = light.GetDirection(),
					A = light.GetTangent(),
					B;
					B.Cross(C, A);
					m_vRenderLightMat.Set(A, B, C, light.GetPosition());

	#endif
				}
			}
#if __BANK 	
			else
			{
				m_DebugIntensity = 0.0f;
			}
#endif

#if CUTSCENE_LIGHT_EVENT_LOGGING
				cutsDisplayf("\t%u\t%.2f\t%s\t%.2f\t%s", fwTimer::GetFrameCount(), pManager->GetTime(), GetLightCutObject()->GetDisplayName().c_str(), fLightIntensity, bLightAdded ? "ADDED" : "CULLED");
#endif
		}
	#if __BANK 	
		else if (!pushLightsEarly) // just do this in the post scene update
		{	
			//update the position for rendering anyway 
			m_DebugIntensity = 0.0f; 
			SceneMat.Transform(vLightPositionFromData, m_vRenderLightMat.d); 
			
			m_vRenderLightMat.Identity3x3(); 

			if(m_pEditLightInfo)
			{
				m_pEditLightInfo->IsActive = false; 
			}
		}
	#endif
	}
}


/////////////////////////////////////////////////////////////////////////////////////
//	PURPOSE: display the matrix and light name 
#if __BANK
void  CCutSceneLight::DisplayDebugLightinfo(const char* pName, bool animated, float gizmoScale) const
{	
		char Txt[64];

		if(m_pEditLightInfo && m_pEditLightInfo->IsActive)
		{		
			if( (animated && CutSceneManager::m_bRenderAnimatedLights) || (!animated && CutSceneManager::m_bRenderCutsceneStaticLight) )		
			{
				if(m_DebugIntensity == 0.0f)
				{
					formatf(Txt, sizeof(Txt),"%s (%s:Not Contributing) (I:%f)", pName, animated ? "Animated" : "Static", m_DebugIntensity); 
				}
				else
				{
					formatf(Txt, sizeof(Txt),"%s (%s:Contributing) (I:%f)", pName, animated ? "Animated" : "Static", m_DebugIntensity); 
				}
				
				grcDebugDraw::AddDebugOutput(Txt);
				grcDebugDraw::Text(m_vRenderLightMat.d, CRGBA(0,0,0,255),0, grcDebugDraw::GetScreenSpaceTextHeight(), Txt );
				//grcDebugDraw::Axis(m_vRenderLightMat, gizmoScale*0.15f, true, 1);
				grcDebugDraw::GizmoPosition(MATRIX34_TO_MAT34V(m_vRenderLightMat), gizmoScale*0.15f);
			}
		
		}
		else
		{
			if(!CutSceneManager::m_displayActiveLightsOnly)
			{
				if( (animated && CutSceneManager::m_bRenderAnimatedLights) || (!animated && CutSceneManager::m_bRenderCutsceneStaticLight) )		
				{
					formatf(Txt, sizeof(Txt),"%s (%s:Not Active) (I:%f)", pName, animated ? "Animated" : "Static", m_DebugIntensity); 
					grcDebugDraw::Text(m_vRenderLightMat.d, CRGBA(0,0,0,255),0, grcDebugDraw::GetScreenSpaceTextHeight(), Txt );
					//grcDebugDraw::Axis(m_vRenderLightMat, gizmoScale*0.15f, true, 1);
					grcDebugDraw::GizmoPosition(MATRIX34_TO_MAT34V(m_vRenderLightMat), gizmoScale*0.15f);
				}
			}
		}
	
}

void CCutSceneLight::OverrideVolumeSettings(float &VolumeIntensity, float &VolumeSize, Vector4& OuterColourAndIntensity)
{
	if (m_pEditLightInfo)
	{
		if(m_pEditLightInfo->bOverrideVolumeIntensity)
		{
			VolumeIntensity = m_pEditLightInfo->VolumeIntensity; 
		}
		else
		{
			m_pEditLightInfo->VolumeIntensity = VolumeIntensity; 
		}

		if(m_pEditLightInfo->bOverrideVolumeSizeScale)
		{
			VolumeSize = m_pEditLightInfo->VolumeSizeScale; 
		}
		else
		{
			m_pEditLightInfo->VolumeSizeScale = VolumeSize;
		}

		if(m_pEditLightInfo->bOverrideOuterColourAndIntensity)
		{
			OuterColourAndIntensity = m_pEditLightInfo->OuterColourAndIntensity; 
		}
		else
		{
			m_pEditLightInfo->OuterColourAndIntensity = OuterColourAndIntensity;
		}

	}
	}

void CCutSceneLight::OverrideCoronaSettings(float &CoronaIntensity, float &CoronaSize, float &CoronaZbias)
{
	if (m_pEditLightInfo)
	{
		if(m_pEditLightInfo->bOverrideCoronaIntensity)
		{
			CoronaIntensity = m_pEditLightInfo->CoronaIntensity; 
		}
		else
		{
			m_pEditLightInfo->CoronaIntensity = CoronaIntensity; 
		}

		if(m_pEditLightInfo->bOverrideCoronaSize)
		{
			CoronaSize = m_pEditLightInfo->CoronaSize; 
		}
		else
		{
			m_pEditLightInfo->CoronaSize = CoronaSize;
}

		if(m_pEditLightInfo->bOverrideCoronaZbias)
		{
			CoronaZbias = m_pEditLightInfo->CoronaZbias; 
		}
		else
		{
			m_pEditLightInfo->CoronaZbias = CoronaZbias;
		}
	}
}

void CCutSceneLight::OverrideExponetiolFallOff(float &fExponentiolFallOff) 
{
	if(m_pEditLightInfo)
	{	
		if(m_pEditLightInfo->bOverrideExpoFallOff)
		{
			fExponentiolFallOff = m_pEditLightInfo->ExpoFallOff; 
		}
		else
		{
			m_pEditLightInfo->ExpoFallOff = fExponentiolFallOff;
		}
	}
}

void CCutSceneLight::OverrideInnerConeAngle(float &fInnerConeAngle)
{
	if(m_pEditLightInfo)
	{
		if(m_pEditLightInfo->bOverrideInnerConeAngle)
		{
			fInnerConeAngle = m_pEditLightInfo->InnerConeAngle; 
		}
		else
		{
			m_pEditLightInfo->InnerConeAngle = fInnerConeAngle;
		}
	}
}	

void CCutSceneLight::OverrideLights (Vector3 &vLightDirection, Vector3 &vLightColor, float &fLightFallOff, float &fLightConeAngle, float &fLightIntesity, u32 &LightFlags, u32 &TimeFlags)
{
	if (m_pEditLightInfo)
	{
		
		if(m_pEditLightInfo->bOverrideIntensity)
		{
			fLightIntesity = m_pEditLightInfo->fIntensity; 
		}
		else
		{
			m_pEditLightInfo->fIntensity = fLightIntesity; 
		}
		
		if(m_pEditLightInfo->bOverrideDirection)
		{
			vLightDirection = m_pEditLightInfo->vDirection; 

		}
		else
		{
			m_pEditLightInfo->vDirection = vLightDirection; 
		}

		if(m_pEditLightInfo->bOverrideColor)
		{
			vLightColor = m_pEditLightInfo->vColor; 
		}
		else
		{
			m_pEditLightInfo->vColor = vLightColor; 
		}

		if(m_pEditLightInfo->bOverrideFallOff)
		{
			fLightFallOff = m_pEditLightInfo->fFallOff; 
		}
		else
		{
			m_pEditLightInfo->fFallOff = fLightFallOff; 
		}

		if(m_pEditLightInfo->bOverrideConeAngle)
		{
			fLightConeAngle = m_pEditLightInfo->fConeAngle;
		}
		else
		{
			m_pEditLightInfo->fConeAngle = fLightConeAngle; 
		}

		if(m_pEditLightInfo->bOverrideLightFlags)
		{
			LightFlags = m_pEditLightInfo->LightFlags; 
		}
		else
		{
			 m_pEditLightInfo->LightFlags = LightFlags; 
		}

		if(m_pEditLightInfo->bOverrideTimeFlags)
		{
			TimeFlags = m_pEditLightInfo->TimeFlags; 
		}
		else
		{
			m_pEditLightInfo->TimeFlags = TimeFlags; 
		}
	}

}
#endif
/////////////////////////////////////////////////////////////////////////////////////
