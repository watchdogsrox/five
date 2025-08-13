// Rage headers
#include "script/wrapper.h"

// Game headers
#include "Game/Wanted.h"
#include "peds/Ped.h"
#include "peds/PedIntelligence.h"
#include "Peds/PedPhoneComponent.h"
#include "physics/physics.h"
#include "renderer/RenderTargetMgr.h"
#include "scene/world/GameWorld.h"
#include "script/commands_graphics.h"
#include "script/Script.h"
#include "script/script_hud.h"
#include "script/script_helper.h"
#include "vehicles/vehicle.h"

// NY headers
#include "Frontend/MobilePhone.h"
#include "vehicles/vehiclepopulation.h"


namespace gta_commands
{


bool CommandCreateEmergencyServicesCar(int VehicleModelHashKey, float DestinationX, float DestinationY, float DestinationZ)
{
	Vector3 TempVec;
	bool 	LatestCmpFlagResult = false;

	fwModelId VehicleModelId;
	CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, &VehicleModelId);		//	ignores return value
	if(SCRIPT_VERIFY( VehicleModelId.IsValid(), "CREATE_EMERGENCY_SERVICES_CAR - this is not a valid model index"))
	{
		TempVec = Vector3(DestinationX, DestinationY, DestinationZ);

		if (CVehiclePopulation::ScriptGenerateOneEmergencyServicesCar(VehicleModelId.GetModelIndex(), TempVec))
		{
			LatestCmpFlagResult = true;
		}
	}
		return LatestCmpFlagResult;
}

bool CommandCreateEmergencyServicesCarThenWalk(int VehicleModelHashKey, float DestinationX, float DestinationY, float DestinationZ)
{
	Vector3 TempVec;
	bool LatestCmpFlagResult = false;

	fwModelId VehicleModelId;
	CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, &VehicleModelId);		//	ignores return value
	if(SCRIPT_VERIFY( VehicleModelId.IsValid(), "CREATE_EMERGENCY_SERVICES_CAR - this is not a valid model index"))
	{
		TempVec = Vector3(DestinationX, DestinationY, DestinationZ);
		
		if (CVehiclePopulation::ScriptGenerateOneEmergencyServicesCar(VehicleModelId.GetModelIndex(), TempVec, true))
		{
			LatestCmpFlagResult = true;
		}
	}
	return LatestCmpFlagResult;
}

bool CommandCreateEmergencyServicesCarReturnDriver(int VehicleModelHashKey, float DestinationX, float DestinationY, float DestinationZ, int &VehIndex, int &Ped1Index, int &Ped2Index)
{
	Vector3 TempVec;

	fwModelId VehicleModelId;
	CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, &VehicleModelId);		//	ignores return value
	if(SCRIPT_VERIFY( VehicleModelId.IsValid(), "CREATE_EMERGENCY_SERVICES_CAR - this is not a valid model index"))
	{
		TempVec = Vector3(DestinationX, DestinationY, DestinationZ);
		CVehicle *pVeh;
		if ( (pVeh = CVehiclePopulation::ScriptGenerateOneEmergencyServicesCar(VehicleModelId.GetModelIndex(), TempVec)) != NULL)
		{
			VehIndex = CTheScripts::GetGUIDFromEntity(*pVeh);

			Ped1Index = 0;
			Ped2Index = 0;
			if (pVeh->GetDriver())
			{
				Ped1Index = CTheScripts::GetGUIDFromEntity(*pVeh->GetDriver());
			}
			
			for( s32 iSeat = 0; iSeat < pVeh->GetSeatManager()->GetMaxSeats(); iSeat++ )
			{
				if (pVeh->GetSeatManager()->GetPedInSeat(iSeat) && iSeat != pVeh->GetDriverSeat())
				{
					Ped2Index = CTheScripts::GetGUIDFromEntity(*pVeh->GetSeatManager()->GetPedInSeat(iSeat));
					break;
				}
			}

			return true;
		}
	}
	return false;
}

int CommandGetVehicleTypeOfModel(int VehicleModelHashKey)
{
	VehicleType ReturnVehicleType = VEHICLE_TYPE_NONE;

	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfoFromHashKey((u32) VehicleModelHashKey, NULL);

	if(pModelInfo && pModelInfo->GetModelType()==MI_TYPE_VEHICLE)
	{
		ReturnVehicleType = ((CVehicleModelInfo*)pModelInfo)->GetVehicleType();
	}

	return ReturnVehicleType;
}


void CommandCreateMobilePhone(int MobilePhoneType)
{
	if (FindPlayerPed() && FindPlayerPed()->GetPhoneComponent())
	{
		CPhoneMgr::Create((CPhoneMgr::ePhoneType)MobilePhoneType);
		FindPlayerPed()->GetPlayerInfo()->SetPlayerDataDisplayMobilePhone( true );

		CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(CPhoneMgr::GetPhoneModelIndex());
		if (pModelInfo)
		{
			s32 nPhoneModelIndex = CPhoneMgr::GetPhoneModelIndex().GetModelIndex();
			u32 nPhoneModelHash = pModelInfo->GetHashKey();
			FindPlayerPed()->GetPhoneComponent()->SetPhoneModelIndex(nPhoneModelIndex, nPhoneModelHash);
		}
	}

}

void CommandDestroyMobilePhone()
{
	CPhoneMgr::Delete();
	FindPlayerPed()->GetPlayerInfo()->SetPlayerDataDisplayMobilePhone( false );
}

void CommandSetMobilePhoneScale(float NewPhoneScale)
{
	CPhoneMgr::SetScale(NewPhoneScale);
}

float CommandGetMobilePhoneScale()
{
	return CPhoneMgr::GetScale();
}

void CommandSetMobilePhoneRotation(const scrVector & scrNewRotationVector, s32 RotOrder)
{
	Vector3 NewRotationVector = Vector3(scrNewRotationVector);
	NewRotationVector *= DtoR;
	CPhoneMgr::SetRotation(NewRotationVector, static_cast<EulerAngleOrder>(RotOrder));
}

void CommandGetMobilePhoneRotation(Vector3& ReturnRotationVector, s32 RotOrder)
{
	EulerAngleOrder StoredRotationOrder;
	CPhoneMgr::GetRotation(ReturnRotationVector, StoredRotationOrder);

	Quaternion quat;
	CScriptEulers::QuaternionFromEulers(quat, ReturnRotationVector, StoredRotationOrder);
	ReturnRotationVector = CScriptEulers::QuaternionToEulers(quat, static_cast<EulerAngleOrder>(RotOrder));

	ReturnRotationVector *= RtoD;
}

void CommandSetMobilePhonePosition(const scrVector & scrNewPositionVector)
{
	const float SIXTEEN_BY_NINE = 16.0f/9.0f;
	float fAspect = CHudTools::GetAspectRatio(true);
	
	Vector3 NewPositionVector = Vector3(scrNewPositionVector);
	
#if SUPPORT_MULTI_MONITOR
	if(GRCDEVICE.GetMonitorConfig().isMultihead())
	{
		NewPositionVector.x *= (1.0f/3.0f);
	}
#endif // SUPPORT_MULTI_MONITOR

	if(CHudTools::IsSuperWideScreen())
	{
		float fDifference = (SIXTEEN_BY_NINE / fAspect);
		float fFinalX = 0.5f - ((0.5f - scrNewPositionVector.x) * fDifference);
		NewPositionVector.x = fFinalX;
	}

	CPhoneMgr::SetPosition(NewPositionVector.x, NewPositionVector.y, NewPositionVector.z);
}

void CommandGetMobilePhonePosition(Vector3& ReturnPositionVector)
{
	Vector3 *pVec = CPhoneMgr::GetPosition();
	ReturnPositionVector = *pVec;
}

void CommandSetMobilePhoneBrightness(int MobilePhoneType, float Brightness)
{
	CPhoneMgr::SetBrightness((CPhoneMgr::ePhoneType)MobilePhoneType, Brightness);
}

void CommandSetMobilePhoneScreenBrightness(int MobilePhoneType, float Brightness)
{
	CPhoneMgr::SetScreenBrightness((CPhoneMgr::ePhoneType)MobilePhoneType, Brightness);
}

float CommandGetMobilePhoneBrightness(int MobilePhoneType)
{
	return CPhoneMgr::GetBrightness((CPhoneMgr::ePhoneType)MobilePhoneType);
}

float CommandGetMobilePhoneScreenBrightness(int MobilePhoneType)
{
	return CPhoneMgr::GetScreenBrightness((CPhoneMgr::ePhoneType)MobilePhoneType);
}

bool CommandIsMobilePhoneInstantiated(void)
{
	return CPhoneMgr::IsPhoneInstantiated();
}

void CommandScriptIsMovingThePhoneOffscreen(bool bVal)
{
	CPhoneMgr::SetGoingOffScreen(bVal);
}

bool CommandCanPhoneBeSeenOnScreen()
{
	return true;
}

void CommandSetMobilePhoneDOF(bool value)
{
	CPhoneMgr::SetDOFState(value);
}

bool CommandGetMobilePhoneDOF()
{
	return CPhoneMgr::GetDOFState();
}

bool CommandCodeWantsMobilePhoneRemoved()
{
	return CPhoneMgr::GetRemoveFlags() != 0;
}

bool CommandCodeWantsMobilePhoneRemovedForWeaponSwitching()
{
	return (CPhoneMgr::GetRemoveFlags() & (CPhoneMgr::WANTS_PHONE_REMOVED_WEAPON_SWITCHING_S1 | CPhoneMgr::WANTS_PHONE_REMOVED_WEAPON_SWITCHING_S2)) != 0;
}

void CommandGetMobilePhoneRenderId(int& renderId)
{
	renderId = CRenderTargetMgr::RTI_PhoneScreen;
}

void CommandAllowEmergencyServices(bool bAllow)
{
	CVehiclePopulation::ms_bAllowEmergencyServicesToBeCreated = bAllow;
}

//========================CELL INPUT============================================= 

void CommandCELL_SET_INPUT(int phoneInput)
{
	CPhoneMgr::SetLatestPhoneInput(phoneInput);
}

//========================CELL HORIZONTAL============================================= 

void CommandCELL_HORIZONTAL_MODE_TOGGLE(bool bToggle)
{
	CPhoneMgr::HorizontalActivate(bToggle);
}

//========================CELL CAM============================================= 

void CommandCELL_CAM_ACTIVATE(bool bEnablePhotoUpdate, bool bGoFirstPerson)
{
	CPhoneMgr::CamActivate(bEnablePhotoUpdate,bGoFirstPerson);
}

void CommandCELL_CAM_ACTIVATE_SELFIE_MODE(bool bShouldActivate)
{
	CPhoneMgr::CamActivateSelfieMode(bShouldActivate);
}

void CommandCELL_CAM_ACTIVATE_SHALLOW_DOF_MODE(bool bShouldActivate)
{
	CPhoneMgr::CamActivateShallowDofMode(bShouldActivate);
}

void CommandCELL_CAM_SET_SELFIE_MODE_SIDE_OFFSET_SCALING(float scaling)
{
	CPhoneMgr::CamSetSelfieModeSideOffsetScaling(scaling);
}

void CommandCELL_CAM_SET_SELFIE_MODE_HORZ_PAN_OFFSET(float scaling)
{
	CPhoneMgr::CamSetSelfieModeHorzPanOffset(scaling);
}

void CommandCELL_CAM_SET_SELFIE_MODE_VERT_PAN_OFFSET(float scaling)
{
	CPhoneMgr::CamSetSelfieModeVertPanOffset(scaling);
}

void CommandCELL_CAM_SET_SELFIE_MODE_ROLL_OFFSET(float offset)
{
	CPhoneMgr::CamSetSelfieRoll(offset);
}

void CommandCELL_CAM_SET_SELFIE_MODE_DISTANCE_SCALING(float scaling)
{
	CPhoneMgr::CamSetSelfieDistance(scaling);
}

void CommandCELL_CAM_SET_SELFIE_MODE_HEAD_YAW_OFFSET(float offset)
{
	CPhoneMgr::CamSetSelfieHeadYaw(offset);
}

void CommandCELL_CAM_SET_SELFIE_MODE_HEAD_ROLL_OFFSET(float offset)
{
	CPhoneMgr::CamSetSelfieHeadRoll(offset);
}

void CommandCELL_CAM_SET_SELFIE_MODE_HEAD_PITCH_OFFSET(float offset)
{
	CPhoneMgr::CamSetSelfieHeadPitch(offset);
}

bool CommandCELL_CAM_IS_CHAR_VISIBLE(int ped)
{
	return CPhoneMgr::CamIsCharVisible(ped);
}

bool CommandCELL_CAM_IS_CHAR_VISIBLE_NO_FACE_CHECK(int ped)
{
	return CPhoneMgr::CamIsCharVisible(ped,false);
}

void CommandCELL_CAM_SET_ZOOM(float zoom)
{
	CPhoneMgr::CamSetZoom(zoom);
}

void CommandCELL_CAM_SET_CENTRE_POS(float x, float y)
{
	Vector2 v(x,y);
	CPhoneMgr::CamSetCentre(v);
}

void CommandCELL_CAM_SET_COLOUR_BRIGHTNESS(float r, float g, float b, float bright) 
{
	CPhoneMgr::CamSetColourAndBrightness(r,g,b,bright);
}

//----------------------------------------------------------------------------------------------------------------------------
void CommandDrawPhoto(float CentreX, float CentreY, float Width, float Height, float Rotation, int R, int G, int B, int A)
{
#if RSG_GEN9
	grcTexture* pTextureToDraw = CPhoneMgr::CamGetRT()->GetTexture();
#else
	grcTexture* pTextureToDraw = CPhoneMgr::CamGetRT();
#endif

	graphics_commands::DrawSpriteTex(pTextureToDraw, CentreX, CentreY, Width, Height, Rotation, R, G, B, A, false, SCRIPT_GFX_SPRITE_NOALPHA, Vector2(0.0f,0.0f), Vector2(1.0f, 1.0f));
}

//----------------------------------------------------------------------------------------------------------------------------
void CommandDrawFrontBuff(float CentreX, float CentreY, float Width, float Height, float Rotation, int R, int G, int B, int A)
{
#if RSG_GEN9
	grcTexture* pTextureToDraw = reinterpret_cast<grcTexture *>(sga::Factory::GetFrontBufferTexture()); ///// PostFX::GetBackBuffer();
#else
	grcTexture* pTextureToDraw = grcTextureFactory::GetInstance().GetFrontBuffer();///// PostFX::GetBackBuffer();
#endif
#if __XENON || __PPU
	//		const rage::grcTexture* pTexture = grcTextureFactory::GetInstance().GetBackBuffer();	
	grcTexture* pTextureToDraw = grcTextureFactory::GetInstance().GetFrontBuffer();
#endif
	graphics_commands::DrawSpriteTex(pTextureToDraw, CentreX, CentreY, Width, Height, Rotation, R, G, B, A, false, SCRIPT_GFX_SPRITE, Vector2(0.0f,0.0f), Vector2(1.0f, 1.0f));
}

void CommandFlashWeaponIcon(bool UNUSED_PARAM(bOnOff))
{

}

bool CommandWantedStarsAreFlashing()
{
	bool bAreFlashing = false;

	CWanted *pWanted = CGameWorld::FindLocalPlayerWanted();
	if (pWanted)
	{
		bAreFlashing = (pWanted->m_WantedLevelBeforeParole != WANTED_CLEAN || pWanted->m_TimeOutsideCircle);
	}

	return bAreFlashing;
}

	void SetupScriptCommands()
	{
		SCR_REGISTER_UNUSED(CREATE_EMERGENCY_SERVICES_CAR, 0x72625256,									CommandCreateEmergencyServicesCar		);
		SCR_REGISTER_UNUSED(CREATE_EMERGENCY_SERVICES_CAR_THEN_WALK, 0x53e5166c,						CommandCreateEmergencyServicesCarThenWalk		);
		SCR_REGISTER_UNUSED(CREATE_EMERGENCY_SERVICES_CAR_RETURN_DRIVER, 0x42825c00,					CommandCreateEmergencyServicesCarReturnDriver	);
		SCR_REGISTER_UNUSED(GET_VEHICLE_TYPE_OF_MODEL, 0x6e53485d,										CommandGetVehicleTypeOfModel);

		SCR_REGISTER_SECURE(CREATE_MOBILE_PHONE,0xc7a697c4ad1d0175,									CommandCreateMobilePhone);
		SCR_REGISTER_SECURE(DESTROY_MOBILE_PHONE,0xa7518cfb10a7defb,									CommandDestroyMobilePhone);
		SCR_REGISTER_UNUSED(IS_MOBILE_PHONE_INSTANTIATED,0x37cd68a41953aae1,							CommandIsMobilePhoneInstantiated);

		SCR_REGISTER_SECURE(SET_MOBILE_PHONE_SCALE,0x3f9b1fdeb8a770ef,									CommandSetMobilePhoneScale);
		SCR_REGISTER_UNUSED(GET_MOBILE_PHONE_SCALE,0xeb8bda097c569fb0,									CommandGetMobilePhoneScale);
		SCR_REGISTER_SECURE(SET_MOBILE_PHONE_ROTATION,0x6447da2f47a963e9,								CommandSetMobilePhoneRotation);
		SCR_REGISTER_SECURE(GET_MOBILE_PHONE_ROTATION,0xde866220fc1119fa,								CommandGetMobilePhoneRotation);
		SCR_REGISTER_SECURE(SET_MOBILE_PHONE_POSITION,0x31636f0193379566,								CommandSetMobilePhonePosition);
		SCR_REGISTER_SECURE(GET_MOBILE_PHONE_POSITION,0xc81489026785778a,								CommandGetMobilePhonePosition);
		
		SCR_REGISTER_UNUSED(SET_MOBILE_PHONE_BRIGHTNESS,0xa785cc728dc3b248,						CommandSetMobilePhoneBrightness);
		SCR_REGISTER_UNUSED(SET_MOBILE_PHONE_SCREEN_BRIGHTNESS,0xdc8f8df60c5ccf1d,						CommandSetMobilePhoneScreenBrightness);
		SCR_REGISTER_UNUSED(GET_MOBILE_PHONE_BRIGHTNESS,0xb507b6e943c74908,						CommandGetMobilePhoneBrightness);
		SCR_REGISTER_UNUSED(GET_MOBILE_PHONE_SCREEN_BRIGHTNESS,0xe025d76cceaa75ff,						CommandGetMobilePhoneScreenBrightness);

		SCR_REGISTER_UNUSED(CODE_WANTS_MOBILE_PHONE_REMOVED,0x849d0fae1843d877,						CommandCodeWantsMobilePhoneRemoved);
		SCR_REGISTER_UNUSED(CODE_WANTS_MOBILE_PHONE_REMOVED_FOR_WEAPON_SWITCHING,0x82a71e2d5a08b2c1,	CommandCodeWantsMobilePhoneRemovedForWeaponSwitching);
		SCR_REGISTER_SECURE(SCRIPT_IS_MOVING_MOBILE_PHONE_OFFSCREEN,0x692d1abae39f0b00,				CommandScriptIsMovingThePhoneOffscreen);

		SCR_REGISTER_SECURE(CAN_PHONE_BE_SEEN_ON_SCREEN,0x53758b69d0956dc7,							CommandCanPhoneBeSeenOnScreen);
		
		SCR_REGISTER_SECURE(SET_MOBILE_PHONE_DOF_STATE,0x78d4941e4dbedf3c,								CommandSetMobilePhoneDOF);
		SCR_REGISTER_UNUSED(GET_MOBILE_PHONE_DOF_STATE,0x2f870a6202f4a148,								CommandGetMobilePhoneDOF);

		SCR_REGISTER_SECURE(CELL_SET_INPUT,0xbca3fa8d0984a7d3,											CommandCELL_SET_INPUT);
		SCR_REGISTER_SECURE(CELL_HORIZONTAL_MODE_TOGGLE,0x2374aa601c2c4709,							CommandCELL_HORIZONTAL_MODE_TOGGLE);

		SCR_REGISTER_SECURE(CELL_CAM_ACTIVATE,0x13ad5fb4a6e050f1,										CommandCELL_CAM_ACTIVATE);
		SCR_REGISTER_SECURE(CELL_CAM_ACTIVATE_SELFIE_MODE,0x041acbe09a018963,							CommandCELL_CAM_ACTIVATE_SELFIE_MODE);
		SCR_REGISTER_SECURE(CELL_CAM_ACTIVATE_SHALLOW_DOF_MODE,0x3cddeb389e6f7e65,						CommandCELL_CAM_ACTIVATE_SHALLOW_DOF_MODE);
		SCR_REGISTER_SECURE(CELL_CAM_SET_SELFIE_MODE_SIDE_OFFSET_SCALING,0x3df0d49ddcc957a5,			CommandCELL_CAM_SET_SELFIE_MODE_SIDE_OFFSET_SCALING);
		SCR_REGISTER_SECURE(CELL_CAM_SET_SELFIE_MODE_HORZ_PAN_OFFSET,0x8ecf6c4a4045833d,				CommandCELL_CAM_SET_SELFIE_MODE_HORZ_PAN_OFFSET);
		SCR_REGISTER_SECURE(CELL_CAM_SET_SELFIE_MODE_VERT_PAN_OFFSET,0x5c34e45ca45f7a49,				CommandCELL_CAM_SET_SELFIE_MODE_VERT_PAN_OFFSET);
		SCR_REGISTER_SECURE(CELL_CAM_SET_SELFIE_MODE_ROLL_OFFSET,0x45209e14b5251bd9,					CommandCELL_CAM_SET_SELFIE_MODE_ROLL_OFFSET);
		SCR_REGISTER_SECURE(CELL_CAM_SET_SELFIE_MODE_DISTANCE_SCALING,0xbfc33ddacaec2532,				CommandCELL_CAM_SET_SELFIE_MODE_DISTANCE_SCALING);
		SCR_REGISTER_SECURE(CELL_CAM_SET_SELFIE_MODE_HEAD_YAW_OFFSET,0x707bc059b4839126,				CommandCELL_CAM_SET_SELFIE_MODE_HEAD_YAW_OFFSET);
		SCR_REGISTER_SECURE(CELL_CAM_SET_SELFIE_MODE_HEAD_ROLL_OFFSET,0xaa8b9d1d78a7db5a,				CommandCELL_CAM_SET_SELFIE_MODE_HEAD_ROLL_OFFSET);
		SCR_REGISTER_SECURE(CELL_CAM_SET_SELFIE_MODE_HEAD_PITCH_OFFSET,0xb385b6bd5599fee4,				CommandCELL_CAM_SET_SELFIE_MODE_HEAD_PITCH_OFFSET);
		SCR_REGISTER_UNUSED(CELL_CAM_IS_CHAR_VISIBLE,0x312a52d622d216f6,								CommandCELL_CAM_IS_CHAR_VISIBLE);
		SCR_REGISTER_SECURE(CELL_CAM_IS_CHAR_VISIBLE_NO_FACE_CHECK,0x3f6a1a0363ec062d,					CommandCELL_CAM_IS_CHAR_VISIBLE_NO_FACE_CHECK);

		SCR_REGISTER_UNUSED(CELL_CAM_SET_ZOOM,0xceb3ab395c8fd061,										CommandCELL_CAM_SET_ZOOM);
		SCR_REGISTER_UNUSED(CELL_CAM_SET_CENTRE_POS,0xd643afee9dc874e3,								CommandCELL_CAM_SET_CENTRE_POS);

		SCR_REGISTER_UNUSED(CELL_CAM_SET_COLOUR_BRIGHTNESS,0xd94204e6313a9f61,							CommandCELL_CAM_SET_COLOUR_BRIGHTNESS);

		SCR_REGISTER_UNUSED(DRAW_SPRITE_PHOTO,0x83fede900389d430,										CommandDrawPhoto);
		SCR_REGISTER_UNUSED(DRAW_SPRITE_FRONT_BUFF, 0xf4963043,											CommandDrawFrontBuff);
	
		SCR_REGISTER_SECURE(GET_MOBILE_PHONE_RENDER_ID,0x411781aa7d295558,								CommandGetMobilePhoneRenderId);

		SCR_REGISTER_UNUSED(ALLOW_EMERGENCY_SERVICES, 0x2dc66ee2,										CommandAllowEmergencyServices);

		SCR_REGISTER_UNUSED(FLASH_WEAPON_ICON, 0x868fd46b,												CommandFlashWeaponIcon);
		SCR_REGISTER_UNUSED(WANTED_STARS_ARE_FLASHING, 0xb7893903,										CommandWantedStarsAreFlashing);

	}
}
