/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CutsceneObject.cpp
// PURPOSE : Class to contain vehicle, peds and objects.
// AUTHOR  : Thomas French /Derek Payne
// STARTED : 09/02/2006
//
/////////////////////////////////////////////////////////////////////////////////

// rage includes:
#include "creature/creature.h"
#include "fwanimation/animdirector.h"

// game includes:
#include "animation/debug/AnimViewer.h"
#include "cutscene/CutSceneManagerNew.h"		//look to remove
#include "CutsceneObject.h"
#include "Cutscene/CutSceneAnimation.h"
#include "cutscene/AnimatedModelEntity.h"
#include "game/clock.h"
#include "game/weather.h"
#include "modelinfo/PedModelInfo.h"
#include "modelinfo/VehicleModelInfo.h"
#include "Peds/Ped.h"
#include "peds/PedHelmetComponent.h"
#include "peds/rendering/PedVariationStream.h"
#include "physics/gtaInst.h"
#include "shaders/CustomShaderEffectVehicle.h"
#include "renderer/Entities/CutSceneDrawHandler.h"
#include "renderer/lights/lights.h"
#include "scene/DynamicEntity.h"
#include "scene/world/gameWorld.h"
#include "shaders/CustomShaderEffectPed.h"
#include "TimeCycle/TimeCycleConfig.h"
#include "debug/DebugScene.h"

//channel
#include "Cutscene/cutscene_channel.h"

/////////////////////////////////////////////////////////////////////////////////

ANIM_OPTIMISATIONS()
SCRIPT_OPTIMISATIONS()

/////////////////////////////////////////////////////////////////////////////////

extern int g_headLightsTexKey;
extern int g_carBottomTexKey;
extern int g_bikeBottomTexKey;
extern ConfigVehicleEmissiveSettings g_LightSwitchSettings;
extern float g_CarAngleSettings;

/////////////////////////////////////////////////////////////////////////////////
#if 0
static void AddVehicleLight(const Matrix34 &matVehicle, 
							const Vector3& worldPosn, const Vector3& dir, const Vector3& colour,
							float intensity, float falloffMax, 
							float innerConeAngle, float outerConeAngle,
							fwInteriorLocation intLoc )
{

	Vector3 worldDir;
	Vector3 worldTan;
	matVehicle.Transform3x3(dir, worldDir);
	matVehicle.Transform3x3(Vector3(0,0,1), worldTan);

	CLightSource light(LIGHT_TYPE_SPOT, LIGHTFLAG_VEHICLE, worldPosn, colour, intensity, LIGHT_ALWAYS_ON);
	light.SetDirTangent(worldDir, worldTan);
	light.SetRadius(falloffMax);
	light.SetSpotlight(innerConeAngle, outerConeAngle);
	light.SetInterior(intLoc);
	CLights::AddSceneLight(light);
}

/////////////////////////////////////////////////////////////////////////////////

static void AddVehicleAmbientOccluder(const Matrix34 &matVehicle, const Vector3& worldPosn, const Vector3& worldDir, const Vector3& worldTan, float scalex, float scaley, float scalez, float distfade, int type)
{
	(void)matVehicle;

	int texKey = ((type==0)?g_carBottomTexKey:g_bikeBottomTexKey);

	//	static float debugLumThres0=0.5f;
	//	static float debugLumThres1=1.0f;
	static Vector3 debugDownDir=Vector3(0,0,-1);

	float downfade=rage::Max(worldDir.Dot(debugDownDir),0.0f);

	float fade=downfade*distfade;//*CMaths::Clamp((TimeCycle::GetAmbientMultiplier0()-debugLumThres0)/(debugLumThres1-debugLumThres0), 0.0f, 1.0f);

	if (fade>0.01f)
	{
		CLightSource light(LIGHT_TYPE_AO_VOLUME, 0, worldPosn, Vector3(0,0,0), fade, LIGHT_ALWAYS_ON);
		light.SetDirTangent(worldDir, worldTan);
		light.SetAOVolume(scalex, scaley, scalez);
		light.SetTexture(texKey, CLights::m_defaultTxdID);
		CLights::AddSceneLight(light);
	}
}

/////////////////////////////////////////////////////////////////////////////////

static void AddVehicleTwinLight(const Matrix34 &matVehicle, 
								const Vector3& worldPosn, const Vector3& dir, const Vector3& colour,
								float intensity, float falloffMax, 
								float innerConeAngle, float outerConeAngle,
								fwInteriorLocation intLoc)
{
	Vector3 worldDir;
	Vector3 worldTan;
	matVehicle.Transform3x3(dir, worldDir);
	matVehicle.Transform3x3(Vector3(0,0,1), worldTan);


	CLightSource light(LIGHT_TYPE_SPOT, LIGHTFLAG_VEHICLE, worldPosn, colour, intensity, LIGHT_ALWAYS_ON);
	light.SetDirTangent(worldDir, worldTan);
	light.SetRadius(falloffMax);
	light.SetSpotlight(innerConeAngle, outerConeAngle);
	light.SetInterior(intLoc);
	light.SetTexture(g_headLightsTexKey, CLights::m_defaultTxdID);
	CLights::AddSceneLight(light);
}

#endif

/////////////////////////////////////////////////////////////////////////////////
// CCutSceneObject:	Virtual base class for cutscen objects

CCutSceneObject::CCutSceneObject(const eEntityOwnedBy ownedBy)
	: CObject( ownedBy )
{
	m_pShaderEffect = NULL;
	m_pShaderEffectType = NULL;
	m_bAnimated = false;
	SetIsInUse(true);

	// Always update anims
	m_nDEflags.bForcePrePhysicsAnimUpdate = true;
}

/////////////////////////////////////////////////////////////////////////////////

CCutSceneObject::~CCutSceneObject()
{
	DEV_BREAK_IF_FOCUS( CDebugScene::ShouldDebugBreakOnDestroyOfFocusEntity(), this );
	DEV_BREAK_ON_PROXIMITY( CDebugScene::ShouldDebugBreakOnProximityOfDestroyCallingEntity(), VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()) );
	
	if (m_pShaderEffect) {
		delete m_pShaderEffect;
		m_pShaderEffect = NULL;
	}
	if (m_pShaderEffectType){
		m_pShaderEffectType->RemoveRef();
		m_pShaderEffectType = NULL;
	}
	
	SetIsInUse(false);
}

/////////////////////////////////////////////////////////////////////////////////

fwDrawData* CCutSceneObject::AllocateDrawHandler(rmcDrawable* pDrawable)
{
	return rage_new CDynamicEntityDrawHandler(this, pDrawable); // NOTE! Not using CObjectDrawHandler!
}

/////////////////////////////////////////////////////////////////////////////////

////////////////////
// CCutSceneProp
////////////////////

/////////////////////////////////////////////////////////////////////////////////

CCutSceneProp::CCutSceneProp(const eEntityOwnedBy ownedBy, u32 ModelIndex, const Vector3& vPos)
	: CCutSceneObject( ownedBy )
{
	m_type = CUTSCENE_OBJECT_TYPE_OBJECT;
	Initialise (ModelIndex, vPos); 
}

/////////////////////////////////////////////////////////////////////////////////

CCutSceneProp::~CCutSceneProp()
{

}

/////////////////////////////////////////////////////////////////////////////////

void CCutSceneProp::Initialise(u32 iModelIndex, const Vector3& vPos)
{
	SetTypeObject();

	SetModelId(fwModelId(iModelIndex));

	SetPosition(vPos);
	SetHeading(0.0f);
	ClearBaseFlag(fwEntity::USE_COLLISION);
	SetIsVisible(true);
	CreateDrawable();

	SetAsCutsceneObject();
	//SetNewBounds();
	CGameWorld::Add(this, CGameWorld::OUTSIDE );	
}

/////////////////////////////////////////////////////////////////////////////////

void CCutSceneProp::SetAsCutsceneObject()
{
	//SetTypeCutsceneObject();
	CreateDrawable();
	if(!GetSkeleton())
		CreateSkeleton();

	if (GetSkeleton())
	{
		CreateAnimDirector(); 
	}
	else
	{
		m_bAnimated = false;
	}

	// a few default settings for rendering of cut scene objects:

#define CUTSCENE_OBJECT_LOD_LEVEL	(1000)  // should be enough for cut scene objects

	SetLodDistance(CUTSCENE_OBJECT_LOD_LEVEL);
}

//////////////////////
// CCutSceneVehicle
//////////////////////

/////////////////////////////////////////////////////////////////////////////////

CCutSceneVehicle::CCutSceneVehicle(const eEntityOwnedBy ownedBy, u32 ModelIndex, const Vector3& vPos)
	: CCutSceneObject( ownedBy )
{
	 m_colour1 = 0;
	 m_colour2 = 0;
	 m_colour3 = 0;
	 m_colour4 = 0;
	nBodyDirtLevel = 0.0f; 

	m_type = CUTSCENE_OBJECT_TYPE_VEHICLE;
	bVehicleHasBeenColoured = false;
	
	Initialise(ModelIndex, vPos); 
}

/////////////////////////////////////////////////////////////////////////////////

void CCutSceneVehicle::Initialise(u32 iModelIndex, const Vector3& vPos)
{
	SetTypeObject();

	SetModelId(fwModelId(iModelIndex));

	SetPosition(vPos);
	SetHeading(0.0f);
	ClearBaseFlag(fwEntity::USE_COLLISION);
	SetIsVisible(true);
	CreateDrawable();
	
	CBaseModelInfo* pModelInfo = GetBaseModelInfo();
	if(pModelInfo->GetFragType())
	{
		m_nFlags.bIsFrag = true;
		const Matrix34 mat = MAT34V_TO_MATRIX34(GetMatrix());

		fragInstGta* pFragInst = rage_new fragInstGta(PH_INST_FRAG_VEH, pModelInfo->GetFragType(), mat);
		Assertf(pFragInst, "Failed to allocate new fragInstGta.");
		if(!pFragInst)
			return;

		SetPhysicsInst(pFragInst, true);

		pFragInst->Insert(false);
		pFragInst->ReportMovedBySim();
	}

	SetAsCutsceneVehicle();
	//SetNewBounds();
	CGameWorld::Add(this, CGameWorld::OUTSIDE );	
}

/////////////////////////////////////////////////////////////////////////////////
// PURPOSE:	sets up this object to be a vehicle-style object

void CCutSceneVehicle::SetAsCutsceneVehicle()
{
	//SetTypeCutsceneVehicle();
	CreateDrawable();
	if(!GetSkeleton())
		CreateSkeleton();
	CreateAnimDirector();

	Assertf(GetSkeleton(), "CUTSCENE: this cutscene obj doesn't have a skeleton");

	SetBodyDirtLevel(0);

	// try and copy over data from an existing car of the same model:
	for (s32 iMissionVehicle = 0; iMissionVehicle < CVehicle::GetPool()->GetSize(); iMissionVehicle++)
	{
		CVehicle *pVehicle = CVehicle::GetPool()->GetSlot(iMissionVehicle);

		if (pVehicle && pVehicle->PopTypeIsMission())
		{
			if (pVehicle->GetModelIndex() == GetModelIndex())
			{
				// yes the model is the same, so copy it:
				SetBodyColour1(pVehicle->GetBodyColour1());
				SetBodyColour2(pVehicle->GetBodyColour2());
				SetBodyColour3(pVehicle->GetBodyColour3());
				SetBodyColour4(pVehicle->GetBodyColour4());
				SetBodyDirtLevel(pVehicle->GetBodyDirtLevel());
				SetBodyDirtColor(pVehicle->GetBodyDirtColor());
				SetLiveryIdx((s8)(pVehicle->GetLiveryId()&0xFF));
				UpdateBodyColourRemapping();
				bVehicleHasBeenColoured = true;
				break;
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////

CVehicleModelInfo* CCutSceneVehicle::GetVehicleModelInfo()
{
	CBaseModelInfo* pBaseModel = GetBaseModelInfo();
	Assert(pBaseModel);
	Assert(pBaseModel->GetModelType() == MI_TYPE_VEHICLE);
	
	CVehicleModelInfo* pVehModelInfo = static_cast<CVehicleModelInfo*>(pBaseModel);
	
	return pVehModelInfo; 
}

/////////////////////////////////////////////////////////////////////////////////

ePrerenderStatus CCutSceneVehicle::PreRender()
{
	
		// update dirt:
		Assert(GetDrawHandler().GetShaderEffect());
		GetDrawHandler().GetShaderEffect()->Update(this);

		for (s32 i = 0; i < m_iBoneOfComponentToRemove.GetCount(); i++)
		{
			HideFragmentComponent(m_iBoneOfComponentToRemove[i]);
		}

		// if this vehicle has instanced wheels, need to flip the matrices on the RHS
		CVehicleModelInfo* pVehModelInfo = (CVehicleModelInfo*)GetBaseModelInfo();
		Assert(pVehModelInfo);

		if(pVehModelInfo->GetStructure()->m_nWheelInstances[0][0])
		{
			FixupVehicleWheelMatrix(VEH_WHEEL_RF, pVehModelInfo);
			FixupVehicleWheelMatrix(VEH_WHEEL_RM1, pVehModelInfo);
			FixupVehicleWheelMatrix(VEH_WHEEL_RM2, pVehModelInfo);
			FixupVehicleWheelMatrix(VEH_WHEEL_RM3, pVehModelInfo);
			FixupVehicleWheelMatrix(VEH_WHEEL_RR, pVehModelInfo);
		}

	return CCutSceneObject::PreRender();
}

/////////////////////////////////////////////////////////////////////////////////

void CCutSceneVehicle::PreRender2()
{
	CCutSceneObject::PreRender2();

#if 0
	const Matrix34 matVehicle = MAT34V_TO_MATRIX34( GetMatrix());

	CVehicleModelInfo *pModelInfo =  GetVehicleModelInfo();
	CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(GetDrawHandler().GetShaderEffect());
	Assert(pShader);

	//TODO: ask cutscene manager if vehicle lights should be on?
	bool lightsOn = false;

	// put lights on between certain times (with slight randomness)
	// and when its foggy or raining
	if (((CClock::GetHour() > 20 || (CClock::GetHour() > 19 && CClock::GetMinute() > (GetRandomSeed() & 63))) ||
		(CClock::GetHour() < 6 || (CClock::GetHour() < 7 && CClock::GetMinute() < (GetRandomSeed() & 63)))) ||
		(g_weather.GetFog() > (GetRandomSeed()/50000.0f)) ||
		(g_weather.GetRain() > (GetRandomSeed()/50000.0f)))
	{
		//TF **REPLACE WITH NEW CUTSCENE**
		/*if (!(CCutsceneManager::GetCutsceneFlags() & CUTSCENE_FLAGS_NO_VEHICLE_LIGHTS))
		{
			lightsOn = true;
		}*/
	}

	if (lightsOn)
	{
		static dev_bool debugEnableVehicleLights=true;
		if (debugEnableVehicleLights==true)
		{
			DoHeadLightsEffect(VEH_HEADLIGHT_L, VEH_HEADLIGHT_R, true, true, 1.0f, pModelInfo, matVehicle, false);
			DoTailLightsEffect(VEH_TAILLIGHT_L, -1, VEH_TAILLIGHT_R, true, false, true, 1.0f, pModelInfo, matVehicle, false);

			static dev_bool debugEnableOtherVehicleEmissives=true;
			if( debugEnableOtherVehicleEmissives )
			{
				pShader->SetLightValue(CVehicleLightSwitch::LW_DEFAULT,g_LightSwitchSettings.lights_On[CVehicleLightSwitch::LW_DEFAULT]);
			}
		}
	}

	static dev_bool debugEnableVehicleAmbOcc=true;
	if (debugEnableVehicleAmbOcc==true)
	{
		if (pModelInfo->GetVehicleType()==VEHICLE_TYPE_BIKE || pModelInfo->GetVehicleType()==VEHICLE_TYPE_BICYCLE)
		{	
			int nBoneIndex0 = pModelInfo->GetBoneIndex(BIKE_WHEEL_F);
			int nBoneIndex1 = pModelInfo->GetBoneIndex(BIKE_WHEEL_R);

			if (nBoneIndex0>-1 && nBoneIndex1>-1) //this checks if its a 2 wheeled vehicle
			{
				Matrix34 mWheelMat0;
				GetGlobalMtx(nBoneIndex0, mWheelMat0);

				Matrix34 mWheelMat1;
				GetGlobalMtx(nBoneIndex0, mWheelMat1);

				Vector3 wheelPos0 = mWheelMat0.d;
				Vector3 wheelPos1 = mWheelMat1.d;

				Vector3 down=Vector3(0.0f, 0.0f, -1.0f);
				Vector3 forward=Vector3(0.0f, 1.0f, 0.0f);

				static float debugMidBalance=0.5f;

				Vector3 wheelMid=(wheelPos0*debugMidBalance)+(wheelPos1*(1.0f-debugMidBalance));

				static float debugBikeRadiusScale=2.0f;
				float wheelRadius = pModelInfo->GetTyreRadius(true)*debugBikeRadiusScale;

				static Vector3 debugMidOffset=Vector3(0,0,0);

				Vector3 occPos=wheelMid+debugMidOffset;

				static float debugBikeWidthScale=1.0f;
				static float debugBikeLengthScale=0.5f;
				static float debugBikeHeightScale=1.0f;

				float width=wheelRadius*debugBikeWidthScale;
				//				float length=((wheelPos0.y-wheelPos1.y)+(wheelRadius*2.0f))*debugBikeLengthScale;
				float length=((wheelPos0-wheelPos1).Mag()+(wheelRadius*2.0f))*debugBikeLengthScale;
				float height=wheelRadius*debugBikeHeightScale;

				Vector3 worldPos;
				Vector3 worldDown;
				Vector3 worldForward;

				worldPos=occPos;
				matVehicle.Transform3x3(down, worldDown);
				matVehicle.Transform3x3(forward, worldForward);

				static int debugAmbTex0=1;

				AddVehicleAmbientOccluder(matVehicle, worldPos, worldDown, worldForward, width*0.5f, length*0.5f, height*0.5f, 1.0f, debugAmbTex0);
			}
		}
		else
		{
			int nBoneIndex0 = pModelInfo->GetBoneIndex(VEH_WHEEL_LF);
			int nBoneIndex1 = pModelInfo->GetBoneIndex(VEH_WHEEL_RF);
			int nBoneIndex2 = pModelInfo->GetBoneIndex(VEH_WHEEL_LR);
			int nBoneIndex3 = pModelInfo->GetBoneIndex(VEH_WHEEL_RR);

			if (nBoneIndex0>-1 && nBoneIndex1>-1 && nBoneIndex2>-1 && nBoneIndex3>-1) //this checks if its a 4 wheeled vehicle
			{
				Matrix34 mWheelMat0;
				Matrix34 mWheelMat1;
				Matrix34 mWheelMat2;
				Matrix34 mWheelMat3;

				GetGlobalMtx(nBoneIndex0, mWheelMat0);
				GetGlobalMtx(nBoneIndex1, mWheelMat1);
				GetGlobalMtx(nBoneIndex2, mWheelMat2);
				GetGlobalMtx(nBoneIndex3, mWheelMat3);

				Vector3 wheelPos0 = mWheelMat0.d;
				Vector3 wheelPos1 = mWheelMat1.d;
				Vector3 wheelPos2 = mWheelMat2.d;
				Vector3 wheelPos3 = mWheelMat3.d;

				Vector3 down=Vector3(0.0f, 0.0f, -1.0f);
				Vector3 forward=Vector3(0.0f, 1.0f, 0.0f);

				Vector3 wheelMid=(wheelPos0+wheelPos1+wheelPos2+wheelPos3)*0.25f;

				float wheelRadius = pModelInfo->GetTyreRadius(true);

				Vector3 occPos=wheelMid;

				static float debugCarWidthScale=1.0f;
				static float debugCarLengthScale=1.0f;
				static float debugCarHeightScale=1.0f;

				float width=((wheelPos1-wheelPos0).Mag()+(wheelPos2-wheelPos3).Mag())*0.5f*debugCarWidthScale;
				float length=((wheelPos0-wheelPos2).Mag()+(wheelPos1-wheelPos3).Mag())*0.5f*debugCarLengthScale;
				float height=wheelRadius*2.0f*debugCarHeightScale;

				Vector3 worldPos;
				Vector3 worldDown;
				Vector3 worldForward;

				worldPos=occPos;
				matVehicle.Transform3x3(down, worldDown);
				matVehicle.Transform3x3(forward, worldForward);

				static int debugAmbTex1=0;

				AddVehicleAmbientOccluder(matVehicle, worldPos, worldDown, worldForward, width*0.5f, length*0.5f, height*0.5f, 1.0f, debugAmbTex1);
			}
		}
	}
#endif

}

/////////////////////////////////////////////////////////////////////////////////

void CCutSceneVehicle::UpdateBodyColourRemapping()
{
	if(m_pDrawHandler->GetShaderEffect())
	{
		CCustomShaderEffectVehicle *pShaderEffectVehicle = static_cast<CCustomShaderEffectVehicle*>(m_pDrawHandler->GetShaderEffect());
		pShaderEffectVehicle->UpdateVehicleColors(this);
	}
	bVehicleHasBeenColoured = true;
}

/////////////////////////////////////////////////////////////////////////////////

void CCutSceneVehicle::GetRemovedFragments(atArray<s32> &RemovedFragments)
{
	for(int i=0;i < m_iBoneOfComponentToRemove.GetCount(); i++)
	{
		RemovedFragments.PushAndGrow(m_iBoneOfComponentToRemove[i]); 
	}
}

/////////////////////////////////////////////////////////////////////////////////

void CCutSceneVehicle::HideFragmentComponent(s32 iBoneId)
{
	const crSkeletonData* pSkeletonData = m_pDrawHandler->GetDrawable()->GetSkeletonData();
	int nBoneIndex = 0;

	// Zero this bones matrix this frame:
	if (pSkeletonData->ConvertBoneIdToIndex((u16)iBoneId, nBoneIndex))
	{
		GetLocalMtx(nBoneIndex).Zero();
	}
}

/////////////////////////////////////////////////////////////////////////////////

void CCutSceneVehicle::SetComponentToHide(const atArray<s32> &RemovedFragments)
{
	if(m_iBoneOfComponentToRemove.GetCount() < MAX_BONES_TO_REMOVE && RemovedFragments.GetCount()< MAX_BONES_TO_REMOVE )
	{
		m_iBoneOfComponentToRemove.Reset(); 
		
		//special case where we are reactivating previously switched off extras, the array will have the first element 0 
		if(RemovedFragments[0] != 0)
		{
			for(int index=0; index < RemovedFragments.GetCount(); index++ )
			{
				m_iBoneOfComponentToRemove.PushAndGrow(RemovedFragments[index]); 
			}
		}
	}
}

/////////////////////////////////////////////////////////
// for debugging  need to be able to remove individual objects from the hide list

#if __DEV
void CCutSceneVehicle::RemoveFragmentFromHideList(s32 iBoneId)
{
	for(int i=0; i < m_iBoneOfComponentToRemove.GetCount(); i++ )
	{
		if(m_iBoneOfComponentToRemove[i] == iBoneId)
		{
			m_iBoneOfComponentToRemove.Delete(i); 
		}
	}
}
#endif

/////////////////////////////////////////////////////////

void CCutSceneVehicle::SetComponentToHide(s32 iBoneId, bool bHide)
{
	if(m_iBoneOfComponentToRemove.GetCount() < MAX_BONES_TO_REMOVE)
	{
		for (int i=0; i < m_iBoneOfComponentToRemove.GetCount(); i++)
		{
			if (iBoneId == m_iBoneOfComponentToRemove[i])
			{
				//dont need to hide a component we already have.
				if(bHide)
				{
					return; 
				}
				else
				{
					//lets "unhide" this component
					m_iBoneOfComponentToRemove.Delete(i); 
					return; 
				}
			
			}
		}

		m_iBoneOfComponentToRemove.PushAndGrow(iBoneId);
	}
}


void CCutSceneVehicle::FixupVehicleWheelMatrix(eHierarchyId nWheelId, CVehicleModelInfo* pModelInfo)
{
	int nBoneIndex = pModelInfo->GetBoneIndex(nWheelId);
	if(nBoneIndex > -1)
	{
		GetLocalMtx(nBoneIndex).a.Scale(-1.0f);
		GetLocalMtx(nBoneIndex).c.Scale(-1.0f);
	}
}

CCutSceneVehicle::~CCutSceneVehicle()
{
	m_nFlags.bIsFrag = true;		//possible remove
}
#if 0
void CCutSceneVehicle::DoHeadLightsEffect(s32 BoneLeft, s32 BoneRight, bool leftOn, bool rightOn, float fade, CVehicleModelInfo* pModelInfo, const Matrix34 &matVehicle, bool bExtraBright)
{
	Matrix34	mBoneMtxLeft, mBoneMtxRight;
	s32		BoneIdLeft = pModelInfo->GetBoneIndex(BoneLeft);
	s32		BoneIdRight = pModelInfo->GetBoneIndex(BoneRight);

	const float I = 6.0f;
	//	const float Size = 0.75f;
	//	const float Str = 0.5f;

	CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(GetDrawHandler().GetShaderEffect());
	Assert(pShader);

	//static dev_float distLeft = 0.25f;
	//static dev_float distRight = 0.25f;
	// Determine if full beam is on or off.
	bool fullBeam = false;

	fullBeam |= bExtraBright;

	/*	float coronaIntensity  = Str * fade * Hdrr * TimeCycle::GetSpriteBrightness();
	float coronaSize = Sizee * Size * TimeCycle::GetSpriteSize();
	if( fullBeam )
	{
	coronaIntensity *= CHeadlightTuningData::ms_fullBeamHeadlightCoronaIntensityMult;
	coronaSize *= CHeadlightTuningData::ms_fullBeamHeadlightCoronaSizeMult;
	}
	*/
	if (BoneIdLeft>-1)
	{
		GetGlobalMtx(BoneIdLeft, mBoneMtxLeft);
		if (leftOn)
		{
			//			Coronas::RegisterCorona(fwIdKeyGenerator::Get(this, BoneLeft+100), 255, 255, 255, coronaIntensity, pBoneMtxLeft->d, coronaSize, 80.0f, distLeft, true, gs_EngineLightFadeRate, false, true, &matVehicle.b, false);
			pShader->SetLightValue(				BONEID_TO_LIGHTID(BoneLeft),
				g_LightSwitchSettings.lights_On[BONEID_TO_LIGHTID(BoneLeft)]);
		}
		else
		{
			pShader->SetLightValue(				BONEID_TO_LIGHTID(BoneLeft),
				g_LightSwitchSettings.lights_Off[BONEID_TO_LIGHTID(BoneLeft)]);
		}
	}

	if (BoneIdRight>-1)
	{
		GetGlobalMtx(BoneIdRight, mBoneMtxLeft);

		if (rightOn)
		{
			//			Coronas::RegisterCorona(fwIdKeyGenerator::Get(this, BoneRight+100), 255, 255, 255, coronaIntensity, pBoneMtxRight->d, coronaSize, 80.0f, distRight, true, gs_EngineLightFadeRate, false, true, &matVehicle.b, false);
			pShader->SetLightValue(				BONEID_TO_LIGHTID(BoneRight),
				g_LightSwitchSettings.lights_On[BONEID_TO_LIGHTID(BoneRight)]);

		}
		else
		{
			pShader->SetLightValue(				BONEID_TO_LIGHTID(BoneRight),
				g_LightSwitchSettings.lights_Off[BONEID_TO_LIGHTID(BoneRight)]);
		}
	}

	// Note that making just the player's car have bigger headlights shouldn't really
	// affect rendering at all since those lights are already almost full screen.
	// I.e. it won't change the number of pixel visited when calculating the players
	// headlights affect on the scene.

	// Determine the angle of the light.
	float ang = g_CarAngleSettings;
	/*	if(CHeadlightTuningData::ms_useFullBeamOrDippedBeamMods)
	{
	if(fullBeam)
	{
	ang += CHeadlightTuningData::ms_aimFullBeamMod;
	}
	else
	{
	ang += CHeadlightTuningData::ms_aimDippedBeamMod;
	}
	}*/
	Vector3 direction = Vector3( 0.0f, 1.0f, ang);
	direction.Normalize();

	// Determine the intesity and distance multiplier.
	float headlightIntensityMult	= 1.0f * CHeadlightTuningData::ms_globalHeadlightIntensityMult;
	float headlightDistanceMult		= 1.0f * CHeadlightTuningData::ms_globalHeadlightDistMult;
	/*	{
	if(CHeadlightTuningData::ms_makeHeadlightsCastFurtherForPlayer && pDriver && pDriver->IsLocalPlayer())
	{
	headlightIntensityMult	*= CHeadlightTuningData::ms_playerHeadlightIntensityMult;
	headlightDistanceMult	*= CHeadlightTuningData::ms_playerHeadlightDistMult;
	}
	if(CHeadlightTuningData::ms_useFullBeamOrDippedBeamMods)
	{
	if(fullBeam)
	{
	headlightIntensityMult	*= CHeadlightTuningData::ms_fullBeamHeadlightIntensityMult;
	headlightDistanceMult	*= CHeadlightTuningData::ms_fullBeamHeadlightDistMult;
	}
	}
	}*/

	const float intensity	= (1.0f * I * fade* g_HeadLightSettings.intensity) * headlightIntensityMult;
	const float falloffMax	= (10.0f + 0.0f * g_HeadLightSettings.falloffMax) * headlightDistanceMult;
	const float coneInnerAngleForBothLightsOn	= g_HeadLightSettings.innerConeAngle * CHeadlightTuningData::ms_globalConeInnerAngleMod;
	const float coneOuterAngleForBothLightsOn	= g_HeadLightSettings.outerConeAngle * CHeadlightTuningData::ms_globalConeOuterAngleMod;
	const float coneInnerAngleJustOneLightOn	= g_HeadLightSettings.innerConeAngle * CHeadlightTuningData::ms_globalConeInnerAngleMod * CHeadlightTuningData::ms_globalOnlyOneLightMod;
	const float coneOuterAngleJustOneLightOn	= g_HeadLightSettings.outerConeAngle * CHeadlightTuningData::ms_globalConeOuterAngleMod * CHeadlightTuningData::ms_globalOnlyOneLightMod;

	fwInteriorLocation interiorLocation = this->GetInteriorLocation();

	// Now deal with actual light casting
	if (BoneIdLeft>-1 && BoneIdRight>-1)
	{
		if (true == leftOn && false == rightOn)
		{			// Only left light is on

			AddVehicleLight(matVehicle, 
				mBoneMtxLeft.d, direction,
				RCC_VECTOR3(g_HeadLightSettings.colour),
				intensity, falloffMax, 
				coneInnerAngleJustOneLightOn,  coneOuterAngleJustOneLightOn,
				interiorLocation );

		}
		else if (false == leftOn && true == rightOn)
		{			// Only right light is on

			AddVehicleLight(matVehicle, 
				mBoneMtxRight.d, direction,
				RCC_VECTOR3(g_HeadLightSettings.colour),
				intensity, falloffMax, 
				coneInnerAngleJustOneLightOn,  coneOuterAngleJustOneLightOn,
				interiorLocation );

		}
		else if (true == leftOn && true == rightOn)
		{			// Both lights are on
			const Vector3 vMax = GetBoundingBoxMax();
			Vector3 leftBonePos; 
			Vector3 rightBonePos; 
			GetDefaultBonePositionSimple(BoneIdLeft,leftBonePos);
			GetDefaultBonePositionSimple(BoneIdRight,rightBonePos);

			const Vector3 localLightPos = (leftBonePos + rightBonePos) *0.5f;

			Vector3 lightPos(localLightPos.x,vMax.y,localLightPos.z);
			lightPos = TransformIntoWorldSpace(lightPos);

			AddVehicleTwinLight(matVehicle, 
				lightPos, direction,
				RCC_VECTOR3(g_HeadLightSettings.colour),
				intensity, falloffMax, 
				coneInnerAngleForBothLightsOn,  coneOuterAngleForBothLightsOn,
				interiorLocation);
		}
	}
	else if ( BoneIdLeft != -1 && (true == leftOn ))
	{ // probably a bike
		AddVehicleLight(matVehicle, 
			mBoneMtxLeft.d, direction,
			RCC_VECTOR3(g_HeadLightSettings.colour),
			intensity, falloffMax, 
			coneInnerAngleJustOneLightOn,  coneOuterAngleJustOneLightOn,
			interiorLocation );
	}
}
#endif

fwDrawData* CCutSceneVehicle::AllocateDrawHandler(rmcDrawable* pDrawable)
{
	return rage_new CCutSceneVehicleDrawHandler(this, pDrawable);
}
#if 0
void CCutSceneVehicle::DoTailLightsEffect(s32 BoneLeft, s32 BoneMiddle, s32 BoneRight, bool leftOn, bool middleOn, bool rightOn, float fade, CVehicleModelInfo* pModelInfo, const Matrix34 &matVehicle, bool isBrakes)
{
	(void)isBrakes;

	Matrix34	mBoneMtxLeft, mBoneMtxRight;
	s32		BoneIdLeft = pModelInfo->GetBoneIndex(BoneLeft);
	s32		BoneIdMiddle = -1;
	s32		BoneIdRight = pModelInfo->GetBoneIndex(BoneRight);

	if (BoneMiddle > -1)
	{
		BoneIdMiddle = pModelInfo->GetBoneIndex(BoneMiddle);
	}

	static float I = 2.5f;

	CCustomShaderEffectVehicle* pShader = static_cast<CCustomShaderEffectVehicle*>(GetDrawHandler().GetShaderEffect());
	Assert(pShader);


	if (BoneIdLeft>-1)
	{
		GetGlobalMtx(BoneIdLeft, mBoneMtxLeft);

		const CVehicleLightSwitch::LightId lightBone = BONEID_TO_LIGHTID(BoneLeft);

		if (leftOn)
		{
			pShader->SetLightValue(				lightBone,
				g_LightSwitchSettings.lights_On[lightBone]);

		}
		else
		{
			pShader->SetLightValue(				lightBone,
				g_LightSwitchSettings.lights_Off[lightBone]);
		}
	}

	if (BoneIdMiddle>-1)
	{

		if (middleOn)
		{
			pShader->SetLightValue(				BONEID_TO_LIGHTID(BoneMiddle),
				g_LightSwitchSettings.lights_On[BONEID_TO_LIGHTID(BoneMiddle)]);

		}
		else
		{
			pShader->SetLightValue(				BONEID_TO_LIGHTID(BoneMiddle),
				g_LightSwitchSettings.lights_Off[BONEID_TO_LIGHTID(BoneMiddle)]);
		}
	}

	if (BoneIdRight>-1)
	{
		GetGlobalMtx(BoneIdRight, mBoneMtxRight);

		const CVehicleLightSwitch::LightId lightBone = BONEID_TO_LIGHTID(BoneRight);
		if (rightOn)
		{
			pShader->SetLightValue(				lightBone,
				g_LightSwitchSettings.lights_On[lightBone]);
		}
		else
		{
			pShader->SetLightValue(				lightBone,
				g_LightSwitchSettings.lights_Off[lightBone]);
		}
	}

	fwInteriorLocation interiorLocation = this->GetInteriorLocation();

	// Now deal with actual light casting
	if (BoneIdLeft>-1 && BoneIdRight>-1)
	{
		const float lightStrength = fade;//* ((true == isBrakes) ? BrakeLightScaler : TailLightScaler);

		if (true == leftOn && false == rightOn)
		{			// Only left light is on
			AddVehicleLight(matVehicle, 
				mBoneMtxLeft.d, Vector3(0.0f, -1.0f, 0.0f), RCC_VECTOR3(g_TailLightSettings.colour),
				I * lightStrength * g_TailLightSettings.intensity, g_TailLightSettings.falloffMax, 
				g_TailLightSettings.innerConeAngle * 0.6f, g_TailLightSettings.outerConeAngle * 0.6f, interiorLocation);
		}
		else if (true == rightOn && false == leftOn)
		{			// Only right light is on
			AddVehicleLight(matVehicle, 
				mBoneMtxRight.d, Vector3(0.0f, -1.0f, 0.0f), RCC_VECTOR3(g_TailLightSettings.colour),
				I * lightStrength * g_TailLightSettings.intensity, g_TailLightSettings.falloffMax, 
				g_TailLightSettings.innerConeAngle * 0.6f, g_TailLightSettings.outerConeAngle * 0.6f, interiorLocation);
		}
		else if (true == rightOn && true == leftOn)
		{			// Both lights are on
			const Vector3 vMin = GetBoundingBoxMin();

			Vector3 leftBonePos; 
			Vector3 rightBonePos; 
			GetDefaultBonePositionSimple(BoneIdLeft,leftBonePos);
			GetDefaultBonePositionSimple(BoneIdRight,rightBonePos);

			const Vector3 localLightPos = (leftBonePos + rightBonePos) *0.5f;

			Vector3 lightPos(localLightPos.x,vMin.y,localLightPos.z);
			lightPos = TransformIntoWorldSpace(lightPos);

			AddVehicleLight(matVehicle, 
				lightPos, Vector3(0.0f, -1.0f, 0.0f), RCC_VECTOR3(g_TailLightSettings.colour),
				I * lightStrength * g_TailLightSettings.intensity, g_TailLightSettings.falloffMax, 
				g_TailLightSettings.innerConeAngle, g_TailLightSettings.outerConeAngle, interiorLocation);
		}
	}
	else if ( BoneIdLeft != -1 && (true == leftOn ))
	{ // probably a bike
		const float lightStrength = fade;// * ((true == isBrakes) ? BrakeLightScaler : 1.0f);

		AddVehicleLight(matVehicle, 
			mBoneMtxLeft.d, Vector3(0.0f, -1.0f, 0.0f), RCC_VECTOR3(g_TailLightSettings.colour),
			I * lightStrength * g_TailLightSettings.intensity, g_TailLightSettings.falloffMax, 
			g_TailLightSettings.innerConeAngle * 0.6f, g_TailLightSettings.outerConeAngle * 0.6f, interiorLocation);
	}
}

void CCutSceneVehicle::GetDefaultBonePositionSimple(s32 boneID, Vector3& vecResult) const
{
	Assert(boneID != -1);
	vecResult.Zero();

	const crBoneData* boneData = GetSkeletonData().GetBoneData(boneID);
	vecResult = RCC_VECTOR3(boneData->GetDefaultTranslation());

	const crBoneData* pParentBoneData = boneData->GetParent();
	while(pParentBoneData)
	{
		vecResult += RCC_VECTOR3(pParentBoneData->GetDefaultTranslation());

		pParentBoneData = pParentBoneData->GetParent();
	}
}
#endif

// eof


