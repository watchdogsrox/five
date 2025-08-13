///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxLens.cpp
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	16 Jun 2008
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "VfxLens.h"

// rage
#include "system/param.h"
#include "rmptfx/ptxeffectinst.h"
#include "string/stringhash.h"

// framework
#include "fwsys/timer.h"
#include "vfx/ptfx/ptfxdebug.h"
#include "vfx/ptfx/ptfxreg.h"
#include "vfx/ptfx/ptfxwrapper.h"
#include "vfx/vfxwidget.h"

// game
#include "Camera/CamInterface.h"
#include "Camera/Cinematic/Camera/Mounted/CinematicMountedCamera.h"
#include "Camera/Gameplay/Aim/FirstPersonShooterCamera.h"
#include "Camera/Gameplay/GameplayDirector.h"
#include "Camera/Viewports/ViewportManager.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "Core/Game.h"
#include "Peds/Ped.h"
#include "Peds/Rendering/PedVariationStream.h"
#include "Renderer/Water.h"
#include "Scene/World/GameWorld.h"
#include "network/Sessions/NetworkSession.h"
#include "security/ragesecgameinterface.h"
#include "Vehicles/Vehicle.h"
#include "Vfx/Particles/PtFxManager.h"
#include "Vfx/Systems/VfxWeapon.h"
#include "Vfx/VfxHelper.h"


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_SYSTEM_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
//  ENUMS
///////////////////////////////////////////////////////////////////////////////	


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

CVfxLens	g_vfxLens;


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxLens
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void			CVfxLens::Init							(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
		// initialise the type infos
		for (int i=0; i<VFXLENSTYPE_NUM; i++)
		{
			m_infos[i].m_distSqrToCamOverride = 0.0f + (i*0.001f);
#if __BANK
			m_infos[i].m_disableType = false;
			m_infos[i].m_debugType = false;
#endif
		}

		m_infos[VFXLENSTYPE_RAIN].m_ptFxHashName = atHashWithStringNotFinal("lens_rain",0x47E62A7F);
		m_infos[VFXLENSTYPE_SNOW].m_ptFxHashName = atHashWithStringNotFinal("lens_snow",0x6A954099);
//		m_infos[VFXLENSTYPE_UNDERWATER].m_ptFxHashName = atHashWithStringNotFinal("lens_underwater",0xEA6DDADF);
		m_infos[VFXLENSTYPE_WATER].m_ptFxHashName = atHashWithStringNotFinal("lens_water",0xE6A3CFB4);
//		m_infos[VFXLENSTYPE_SAND].m_ptFxHashName = atHashWithStringNotFinal("lens_sand",0x2FFBB5E0);
//		m_infos[VFXLENSTYPE_MUD].m_ptFxHashName = atHashWithStringNotFinal("lens_mud",0x8E0BA3B9);
//		m_infos[VFXLENSTYPE_BLOOD].m_ptFxHashName = atHashWithStringNotFinal("lens_blood",0x71842487);
//		m_infos[VFXLENSTYPE_DUST].m_ptFxHashName = atHashWithStringNotFinal("lens_dust",0x13006DB6);
//		m_infos[VFXLENSTYPE_SMOKE].m_ptFxHashName = atHashWithStringNotFinal("lens_smoke",0x4E7DFC31);
		m_infos[VFXLENSTYPE_BLAZE].m_ptFxHashName = atHashWithStringNotFinal("lens_blaze",0x4EBA14B9);
		m_infos[VFXLENSTYPE_RUNNING_WATER].m_ptFxHashName = atHashWithStringNotFinal("lens_water_run",0x7240C37B);
		m_infos[VFXLENSTYPE_BIKE_DIRT].m_ptFxHashName = atHashWithStringNotFinal("lens_bug_dirt",0x2B151ACB);
		m_infos[VFXLENSTYPE_NOIR].m_ptFxHashName = atHashWithStringNotFinal("lens_noir",0x49A0EB7A);

#if __BANK
		m_disableFirstPersonLensPtFx = false;

		m_infos[VFXLENSTYPE_RAIN].m_disableTypeWidgetName = "Disable Rain Lens Fx";
		m_infos[VFXLENSTYPE_SNOW].m_disableTypeWidgetName = "Disable Snow Lens Fx";
//		m_infos[VFXLENSTYPE_UNDERWATER].m_disableTypeWidgetName = "Disable Underwater Lens Fx";
		m_infos[VFXLENSTYPE_WATER].m_disableTypeWidgetName = "Disable Water Lens Fx";
//		m_infos[VFXLENSTYPE_SAND].m_disableTypeWidgetName = "Disable Sand Lens Fx";
//		m_infos[VFXLENSTYPE_MUD].m_disableTypeWidgetName = "Disable Mud Lens Fx";
//		m_infos[VFXLENSTYPE_BLOOD].m_disableTypeWidgetName = "Disable Blood Lens Fx";
//		m_infos[VFXLENSTYPE_DUST].m_disableTypeWidgetName = "Disable Dust Lens Fx";
//		m_infos[VFXLENSTYPE_SMOKE].m_disableTypeWidgetName = "Disable Smoke Lens Fx";
		m_infos[VFXLENSTYPE_BLAZE].m_disableTypeWidgetName = "Disable Blaze Lens Fx";
		m_infos[VFXLENSTYPE_RUNNING_WATER].m_disableTypeWidgetName = "Disable Running Water Fx";
		m_infos[VFXLENSTYPE_BIKE_DIRT].m_disableTypeWidgetName = "Disable Bike Dirt Fx";
		m_infos[VFXLENSTYPE_NOIR].m_disableTypeWidgetName = "Disable Noir Lens Fx";

		m_infos[VFXLENSTYPE_RAIN].m_debugTypeWidgetName = "Debug Rain Lens Fx";
		m_infos[VFXLENSTYPE_SNOW].m_debugTypeWidgetName = "Debug Snow Lens Fx";
//		m_infos[VFXLENSTYPE_UNDERWATER].m_debugTypeWidgetName = "Debug Underwater Lens Fx";
		m_infos[VFXLENSTYPE_WATER].m_debugTypeWidgetName = "Debug Water Lens Fx";
//		m_infos[VFXLENSTYPE_SAND].m_debugTypeWidgetName = "Debug Sand Lens Fx";
//		m_infos[VFXLENSTYPE_MUD].m_debugTypeWidgetName = "Debug Mud Lens Fx";
//		m_infos[VFXLENSTYPE_BLOOD].m_debugTypeWidgetName = "Debug Blood Lens Fx";
//		m_infos[VFXLENSTYPE_DUST].m_debugTypeWidgetName = "Debug Dust Lens Fx";
//		m_infos[VFXLENSTYPE_SMOKE].m_debugTypeWidgetName = "Debug Smoke Lens Fx";
		m_infos[VFXLENSTYPE_BLAZE].m_debugTypeWidgetName = "Debug Blaze Lens Fx";
		m_infos[VFXLENSTYPE_RUNNING_WATER].m_debugTypeWidgetName = "Debug Running Water Fx";
		m_infos[VFXLENSTYPE_BIKE_DIRT].m_debugTypeWidgetName = "Debug Bike Dirt Fx";
		m_infos[VFXLENSTYPE_NOIR].m_debugTypeWidgetName = "Debug Noir Lens Fx";
#endif
    }
    else if(initMode == INIT_AFTER_MAP_LOADED)
    {
		// initialise some variables
		for (int i=0; i<VFXLENSTYPE_NUM; i++)
		{
			m_infos[i].m_registeredThisFrame = false;
		}
    }
}

///////////////////////////////////////////////////////////////////////////////
//  ShutdownLevel
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxLens::Shutdown  						(unsigned shutdownMode)
{
    if (shutdownMode == SHUTDOWN_SESSION)
    {
	    ptfxReg::UnRegister(this);
    }
}


///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void			CVfxLens::Update						()
{
#if __BANK
	// return if the particle effects are disabled in the widgets
	if (ptfxDebug::GetDisablePtFx())
	{
		return;
	}
#endif

	// register bike dirt if needed
	CPed* pPed = CGameWorld::FindLocalPlayer();
	if (pPed && pPed->GetIsInVehicle())
	{
		CVehicle* pVehicle = pPed->GetMyVehicle();
		if (pVehicle && (pVehicle->GetVehicleType()==VEHICLE_TYPE_BIKE || pVehicle->GetVehicleType()==VEHICLE_TYPE_QUADBIKE))
		{
			float vehSpeed = pVehicle->GetVelocity().Mag();
			if (vehSpeed>25.0f)
			{
				Register(VFXLENSTYPE_BIKE_DIRT, 1.0f, 0.0f, 0.0f, 1.0f);
			}
		}
	}

	// check if the camera has switched
	bool cameraSwitched = CVfxHelper::HasCameraSwitched(false, false);

	// process the types
	for (int i=0; i<VFXLENSTYPE_NUM; i++)
	{
		// check if this is a valid camera view for this lens system 
		bool validCamera = IsValidCamera((VfxLensType)i);
		bool isValidCameraActive = validCamera && IsValidCameraActive((VfxLensType)i);

		// deal with clearing the current lens effect
		if (!validCamera || cameraSwitched)
		{
			ptfxRegInst* pRegFx = ptfxReg::Find(this, i);
			if (pRegFx && pRegFx->m_pFxInst)
			{
				ptfxReg::UnRegister(pRegFx->m_pFxInst, true);
			}
		}

		// update the lens effect if required
		if (m_infos[i].m_registeredThisFrame && validCamera && isValidCameraActive)
		{
			if (m_infos[i].m_ptFxHashName.GetHash()!=0)
			{
				UpdatePtFx(m_infos[i], i, cameraSwitched);
			}
		}

		// reset the registration flag
		m_infos[i].m_registeredThisFrame = false;
	}
	//@@: range VFXLENS_UPDATE_RAGE_SEC_POP_REACTION {
	RAGE_SEC_POP_REACTION
	//@@: } VFXLENS_UPDATE_RAGE_SEC_POP_REACTION
}


///////////////////////////////////////////////////////////////////////////////
//  Register
///////////////////////////////////////////////////////////////////////////////

void			CVfxLens::Register						(VfxLensType type, float levelEvo, float angleEvo, float camSpeedEvo, float alphaTint)
{
	if (m_infos[type].m_registeredThisFrame)
	{
		// already being added this frame - update evos only if greater than current
		m_infos[type].m_levelEvo = Max(m_infos[type].m_levelEvo, levelEvo);
		m_infos[type].m_angleEvo = Max(m_infos[type].m_angleEvo, angleEvo);
		m_infos[type].m_camSpeedEvo = Max(m_infos[type].m_camSpeedEvo, camSpeedEvo);
		m_infos[type].m_alphaTint = Max(m_infos[type].m_alphaTint, alphaTint);
	}
	else
	{
		// first add this frame - set up initial state
		m_infos[type].m_registeredThisFrame = true;
		m_infos[type].m_levelEvo = levelEvo;
		m_infos[type].m_angleEvo = angleEvo;
		m_infos[type].m_camSpeedEvo = camSpeedEvo;
		m_infos[type].m_alphaTint = alphaTint;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFx
///////////////////////////////////////////////////////////////////////////////

void			CVfxLens::UpdatePtFx					(VfxLensInfo& info, int typeIdx, bool cameraSwitched)
{
#if __BANK
	// return if the particle effects are disabled in the widgets
	if (info.m_disableType)
	{
		return;
	}
#endif

	// register the fx system
	bool justCreated = false;
	ptxEffectInst* pFxInst = NULL;
#if __BANK
	if (info.m_debugType)
	{
		pFxInst = ptfxManager::GetRegisteredInst(this, typeIdx, false, atHashWithStringNotFinal("lens_test", 0xcc9352a3), justCreated);
	}
	else
#endif
	{
		pFxInst = ptfxManager::GetRegisteredInst(this, typeIdx, false, info.m_ptFxHashName, justCreated);
	}

	if (pFxInst)
	{
		if (justCreated)
		{
			pFxInst->SetOverrideDistSqrToCamera(info.m_distSqrToCamOverride, true);
		}

		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("level", 0xF66D3B99), info.m_levelEvo);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("angle", 0xF3805B5), info.m_angleEvo);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("camspeed", 0xB988815B), info.m_camSpeedEvo);

		//Always update the position of the effect to the camera's position
		//So it gets correct lighting
		pFxInst->SetBaseMtx(MATRIX34_TO_MAT34V(camInterface::GetMat()));

		// check if the effect has just been created 
		if (justCreated || cameraSwitched)
		{
			ptfxWrapper::SetAlphaTint(pFxInst, info.m_alphaTint);

			//Set the vehicle interior flag, so it can be rendered in the vehicle interior
			//we go to into first person mode.
			pFxInst->SetFlag(PTFX_EFFECTINST_FORCE_VEHICLE_INTERIOR);

			// set a flag to stop this effect being removed by any RemovePtFxInRange call etc
			pFxInst->SetFlag(PTFX_EFFECTINST_DONT_REMOVE);

			// it has just been created - start it and set it's position
			pFxInst->Start();
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  IsValidCamera
///////////////////////////////////////////////////////////////////////////////

bool			CVfxLens::IsValidCamera			(VfxLensType type)
{
	bool inFirstPersonView = false;

#if __BANK
	if (!m_disableFirstPersonLensPtFx)
	{
		const camBaseCamera* pCamera = camInterface::GetDominantRenderedCamera();
#if FPS_MODE_SUPPORTED
		if (pCamera && pCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId())==true)
		{
			inFirstPersonView = true;
		}
#endif

		if (pCamera && pCamera->GetIsClassId(camCinematicMountedCamera::GetStaticClassId()))
		{
			inFirstPersonView = static_cast<const camCinematicMountedCamera*>(pCamera)->IsFirstPersonCamera();
		}
	}
#endif

	bool isUnderwater = CVfxHelper::GetGameCamWaterDepth()>0.0f;

	bool fpPropAllowsWeatherLensEffects = (CPedVariationStream::GetFirstPersonPropWeatherSE() > 0.0f);
	bool fpPropAllowsEnvironmentLensEffects = (CPedVariationStream::GetFirstPersonPropEnvironmentSE() > 0.0f);

	if (type==VFXLENSTYPE_BLAZE || type==VFXLENSTYPE_NOIR)
	{
		return true;
	}
	else if (type==VFXLENSTYPE_RAIN || type==VFXLENSTYPE_SNOW)
	{
		return inFirstPersonView && fpPropAllowsWeatherLensEffects && !isUnderwater;	
	}
	else if (type==VFXLENSTYPE_WATER || type==VFXLENSTYPE_RUNNING_WATER || type==VFXLENSTYPE_BIKE_DIRT)
	{
		return inFirstPersonView && fpPropAllowsEnvironmentLensEffects && !isUnderwater;	
	}

	vfxAssertf(0, "CVfxLens::IsValidCamera - type not recognised");
	return false;
}


///////////////////////////////////////////////////////////////////////////////
//  IsValidCameraActive
///////////////////////////////////////////////////////////////////////////////

bool			CVfxLens::IsValidCameraActive		(VfxLensType type)
{
	if (type==VFXLENSTYPE_RAIN || type==VFXLENSTYPE_SNOW)
	{
		CPed* pPed = const_cast<CPed*>(camInterface::GetGameplayDirector().GetFollowPed());
		if (pPed)
		{
			return !pPed->GetIsShelteredFromRain();
		}
	}
	else if (type==VFXLENSTYPE_WATER)
	{
		CPed* pPed = const_cast<CPed*>(camInterface::GetGameplayDirector().GetFollowPed());
		if (pPed)
		{
			return !pPed->GetIsShelteredInVehicle();
		}
	}
	else if (type==VFXLENSTYPE_BLAZE || type==VFXLENSTYPE_NOIR || type==VFXLENSTYPE_RUNNING_WATER || type==VFXLENSTYPE_BIKE_DIRT)
	{
		return true;
	}

	vfxAssertf(0, "CVfxLens::IsValidCamera - type not recognised");
	return false;
}


///////////////////////////////////////////////////////////////////////////////
//  InitWidgets
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CVfxLens::InitWidgets			()
{
	bkBank* pVfxBank = vfxWidget::GetBank();

	pVfxBank->PushGroup("Vfx Lens", false);
	{
		pVfxBank->AddTitle("DEBUG");

		pVfxBank->AddToggle("Disable First Person Lens PtFx", &m_disableFirstPersonLensPtFx);
		
		for (int i=0; i<VFXLENSTYPE_NUM; i++)
		{
			pVfxBank->AddToggle(m_infos[i].m_disableTypeWidgetName, &m_infos[i].m_disableType);
		}

		for (int i=0; i<VFXLENSTYPE_NUM; i++)
		{
			pVfxBank->AddToggle(m_infos[i].m_debugTypeWidgetName, &m_infos[i].m_debugType);
		}
	}
	pVfxBank->PopGroup();
}
#endif





