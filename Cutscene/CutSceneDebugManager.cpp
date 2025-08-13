/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CutSceneDebugManagerNew.h
// PURPOSE : A class for dealing with all the debug and tool aspects of the cut scene manager.
// AUTHOR  : Thomas French
// STARTED :
//
/////////////////////////////////////////////////////////////////////////////////

#if __BANK
//Rage Headers
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "bank/combo.h"
#include "bank/data.h"
#include "bank/slider.h"
#include "cutfile/cutfobject.h"
#include "cutfile/cutfeventargs.h"
#include "cutfile/cutfevent.h"
#include "cutscene/cutschannel.h"
#include "cutscene/cutsevent.h"
#include "fwdebug/picker.h"
#include "system/nelem.h"
#include "system/controlMgr.h"

//Game Headers
#include "AnimatedModelEntity.h"
#include "camera/helpers/Frame.h"
#include "camera/CamInterface.h"
#include "camera/base/Basecamera.h"
#include "cutscene/CutSceneAnimManager.h"
#include "Cutscene/CutSceneOverlayEntity.h"
#include "cutscene/CutSceneCameraEntity.h"
#include "CutsceneCustomEvents.h"
#include "CutSceneDebugManager.h"
#include "CutSceneDefine.h"
#include "Cutscene/cutscene_channel.h"
#include "Cutscene/CutSceneCustomEvents.h"
#include "Cutscene/CutSceneParticleEffectObject.h"
#include "cutscene/CutSceneParticleEffectEntity.h"
#include "frontend/Scaleform/ScaleformStore.h"
#include "input/keyboard.h"
#include "input/keys.h"
#include "modelinfo/MloModelInfo.h"
#include "modelinfo/PedModelInfo.h"
#include "peds/rendering/PedVariationPack.h"
#include "peds/rendering/PedVariationDS.h"
#include "peds/rendering/PedVariationStream.h"
#include "frontend/scaleform/ScaleFormMgr.h"
#include "renderer/Renderer.h"
#include "renderer/Water.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "scene/entity.h"
#include "Streaming/streaming.h"

#include "../shader_source/Lighting/Shadows/cascadeshadows_common.fxh"

///////////////////////////////////////////////////////////////////////////////////////////////////
//							DEBUG MANAGER
///////////////////////////////////////////////////////////////////////////////////////////////////

ANIM_OPTIMISATIONS()
//OPTIMISATIONS_OFF()

#if __BANK
#define MAX_EXTRA_VEHICLE_NAMES (10) 
#define MAX_NUMBER_PARAM_TYPES (4)
#define MAX_NUMBER_SHADOW_TYPES (3)
#define MAX_NUMBER_DOF_STATES (3)

const char* VehicleExtraNames[MAX_EXTRA_VEHICLE_NAMES] = {"extra_1","extra_2","extra_3","extra_4","extra_5","extra_6","extra_7","extra_8","extra_9","extra_10"};
const char* OverlayParamNames[MAX_NUMBER_OVERLAY_PARAMS] = {"param_1","param_2","param_3","param_4","param_5","param_6","param_7"};
const char* OverlayParamTypes[MAX_NUMBER_PARAM_TYPES] = {"int", "float", "string", "bool"};
const char* ShadowSampleTypes[MAX_NUMBER_SHADOW_TYPES] = {"CSM_ST_BOX4x4", "CSM_ST_TWOTAP" , "CSM_ST_BOX3x3"};
const char* DofStates[MAX_NUMBER_DOF_STATES] = {"DOF_DATA_DRIVEN", "DOF_FORCED_ON (Preview Only)" , "DOF_FORCED_OFF (Preview Only)"};
bool CCutSceneDebugManager::m_cutfileValidationProcesses[CVP_MAX] = { 0 };

enum ParamTypes
{
	PT_INT,
	PT_FLOAT, 
	PT_STRING, 
	PT_BOOL
};

#endif

CCutSceneDebugManager::CCutSceneDebugManager()
{
	//Display
	m_bDisplayLoadedModels = false; 
	m_bPrintModelLoadingToDebug = false; 

	//Vehicles
	m_pVehicleVarFrameSlider = NULL; 
	m_iVehicleModelIndex = 0; 
	m_pVehicle = NULL; 
	m_iMainBodyColour = 0; 
	m_iSecondBodyColour = 0;
	m_iSpecularColour = 0; 
	m_iWheelTrimColour = 0;
	m_iBodyColour5 = 0;
	m_iBodyColour6 = 0;
	m_iLivery = 0;
	m_iLivery2 = 0;
	m_fDirtLevel = 0.0f; 
	m_iVehicleExtraIndex = 0; 
	m_bDisplayVehicleVariation = false; 
	m_VehicleVarEventId = 0; 
	m_VehicleExtraEventId = 0; 
	m_pLiverySlider = NULL; 
	m_pLivery2Slider = NULL; 
	m_pVehicleExtraCombo = NULL; 
	m_pVehicleSelectCombo = NULL; 
	m_pVehicleVariationEventCombo = NULL; 
	m_pVehicleExtraEventCombo = NULL;
	m_bVehicleDamageSelector = false; 
	m_fDerformation = 0.0f; 
	m_fDamageValue = 0.0f; 
	m_iVehicleDamageIndex = 0;
	m_pVehicleDamageEventCombo = NULL; 
	m_vLocalDamage = VEC3_ZERO; 

	//Peds
	m_pActorEntity = NULL;
	m_iPedModelIndex = 0; 
	m_varDrawableId = 0; 
	m_varTextureId =0 ; 
	m_varComponentId = 0; 
	m_varPalette = 0; 
	m_maxDrawables = 0; 
	m_maxTextures = 0; 
	m_maxPalettes = 0; 
	m_PropSlotId = 0;
	m_maxPropDrawableId= -1; 
	m_PropDrawableId = -1; 
	m_maxPropTextureId = -1; 
	m_PropTextureId = -1; 
	strncpy(m_varDrawablName, "Invalid", 24);
	strncpy(m_varTexName, "Invalid", 24);
	m_PedVarEventId = 0; 
	m_pEventCombo = NULL;
	m_pPedSelectCombo = NULL; 
	m_pVarDrawablIdSlider = NULL; 
	m_CboCompVariationIndex = NULL;
	m_CboPropVariationIndex = NULL;
	m_pVarTexIdSlider = NULL; 
	m_pVarPaletteIdSlider = NULL;
	m_pVarPedFrameSlider = NULL;	
	m_pPropDrawablIdSlider = NULL; 
	m_pPropTexIdSlider = NULL; 
	m_vWorldZOffset = VEC3_ZERO;
	m_bDisplayAllSkeletons = false; 

	//ptfx
	m_pPTFXSelectCombo = NULL; 
	m_PtfxEnt = NULL; 
	m_Ptfx = NULL;
	m_iPtfxIndex = 0; 
	m_vPTFXPos = VEC3_ZERO; 
	m_vPTFXRot = VEC3_ZERO; 

	//prop
	m_pPropSelectCombo = NULL; 
	m_iPropModelIndex = 0;
	m_pPropEntity = NULL; 

	//Frame sliders
	m_pVarDebugFrameSlider = NULL; 
	m_pVarSkipFrameSlider = NULL; 
	m_pPedVarPlayBackSlider = NULL;
	m_pVehicleVarPlayBackSlider = NULL; 
	m_pVarFrameSlider = NULL; 
	m_pOverlayPlayBackSlider = NULL; 
	m_pWaterIndexFrameSlider = NULL; 

	//Timecycle
	m_bOverrideTimeCycle = false; 
	m_fTimeCycleOverrideTime = 0; 
	m_cGameTime[0] = '\0'; 
	m_CameraCutName[0] = '\0';
	//Overlay
	m_pOverlaySelectCombo = NULL; 
	m_pOverlayMethodsCombo = NULL;
	m_iOverlayIndex = 0; 
	m_pOverlay = NULL; 
	m_pOverlayEventCombo = NULL; 
	m_vOverlayPos.x = 0.0f; 
	m_vOverlayPos.y = 0.0f; 
	m_vOverlaySize.x = 0.0f; 
	m_vOverlaySize.y = 0.0f; 
	m_iOverlayMethodIndex = 0; 
	m_OverlayEventId = 0; 
	m_iOverlayParamNameIndex = 0; 
	m_iOverlayParamTypeIndex = 0; 
	m_bUseScreenRenderTarget = true; 

	//camera
	m_bOverrideCamBlendOutEvents = false; 
	m_vCameraPos = VEC3_ZERO; 
	m_vCameraRot = VEC3_ZERO; 
	m_bOverrideCam = false; 
	m_cameraMtx = M34_IDENTITY;
	m_bOverrideCamUsingMatrix = false; 
	m_vDofPlanes = Vector4(Vector4::ZeroType); 
	m_RenderDofPlanes = false; 
	m_bOverrideCamUsingBinary = false;
	m_binaryCamMatrix.Identity();
	m_NearOutPlaneAlpha = 255; 
	m_NearInPlaneAlpha = 255; 
	m_FarInPlaneAlpha = 255; 
	m_FarOutPlaneAlpha = 255; 
	m_FocalPointAlpha = 255; 
	m_PlaneScale = 1.0f; 
	m_FocalPointScale = 0.5f; 
	m_fFov = 45.0f; 
	m_SetPlaneSolid = false; 
	m_scaleAsScreenRatio = false; 
	m_fFadeInDuration = 1.0f; 
	m_fFadeOutDuration = 1.0f; 
	m_vFadeInColor = VEC3_ZERO; 
	m_vFadeOutColor = VEC3_ZERO; 
	m_bShouldOverrideUseLightDof = false;
	m_bShouldOverrideUseSimpleDof = false;
	m_fMotionBlurStrength = 0.0f; 

	m_fWaterRefractionIndex = DEFAULT_REFRACTIONINDEX; 
	m_ReflectionLodRangeStart = -1.0f; 
	m_ReflectionLodRangeEnd = -1.0f; 
	m_ReflectionSLodRangeStart = -1.0f;
	m_ReflectionSLodRangeEnd = -1.0f;
	m_LodMultHD = -1.0f;
	m_LodMultOrphanedHD = -1.0f; 
	m_LodMultLod = -1.0f; 
	m_LodMultSLod1 = -1.0f;
	m_LodMultSLod2 = -1.0f;
	m_LodMultSLod3 = -1.0f;
	m_LodMultSLod4 = -1.0f;
	m_WaterReflectionFarClip = -1.0f;
	m_SSAOLightInten = -1.0f;
	m_ExposurePush = 0.0f;
	m_LodMult = -1.0f;
	m_DirectionalLightMultiplier = -1.0f;
	m_LensArtefactMultiplier = -1.0f;
	m_MaxBloom = -1.0f; 
	m_bDisableHighQualityDof = false;
	m_FreezeReflectionMap = false; 
	m_DisableDirectionalLight = false; 
	m_LightFadeDistanceMult = 1.0f; 
	m_LightShadowFadeDistanceMult = 1.0f;  
	m_LightSpecularFadeDistMult = 1.0f; 
	m_LightVolumetricFadeDistanceMult = 1.0f; 
	m_AbsoluteIntensityEnabled = false; 

	m_FirstPersonBlendOutSlider = NULL; 
	m_firstPersonCameraBlendDuration = 0.0f; 
	m_firstPersonBlendOutComboIndex = 0; 
	m_firstPersonBlendOutComboSlider = NULL; 
	m_firstPersonEventStatus = NULL;
	m_firstPersonStatusText[0] = 0; ; 

	m_CharacterLight.Reset();
	
	m_iWaterRefractionIndexId = 0; 
	m_pWaterRefractionIndexCombo = NULL; 

	m_bProfilingDisableCsLights = false;
	m_bProfilingOverrideCharacterLight = false;
	m_bProfilingDisableDof = false;
	m_bProfilingUseDefaultFov = false;
	
	m_pLoadInteriorIndexCombo = NULL;
	m_FinalFrameCameraMat.Identity(); 

	m_CameraWidgetData = NULL;
	m_CutsceneStateWidgetData = NULL;

	m_InteriorPos = VEC3_ZERO; 
	m_InteriorName[0] = '\0'; 

	m_LoadInteriorIndexId = 0; 
	m_iTimecycleEventsIndexId = 0; 
	m_pTimeCycleCameraCutCombo = NULL; 

	m_vVehicleDamagerPos.Zero();

	//day dof 
	m_DayCoCHours = 0; 
	m_bShouldSetCoCForAllDay = false; 
	m_bShouldSetCoCForAllNight = false; 
	m_bShouldUseRangeForCoC = false; 
	m_pHoursGrp = NULL; 
	
	//dof 
	m_bOverrideCoCModifier = false; 
	m_CoCModifierHours = 0; 
	m_UseDayCoCHoursForModifier = false; 
	m_UseNightCoCHoursForModifier = false; 

	m_CoCModifier = 1; 
	m_TimeDayDofOverrideFlags = 0; 
	m_CoCRadius = 1; 
	m_FocusPoint = 0.0f; 
	m_DofState = 0; 

	m_pCoCModifiersCameraCutCombo = NULL; 
	m_pPerCutCoCModifiersCombo = NULL; 
	m_AllCoCModifiersCombo = NULL;  
	m_pEndHourSlider = NULL; 
	m_pSaveStatusText = NULL; 
	m_iCoCModifiersCamerCutComboIndexId = 0; 
	m_iCoCModifierIndex = 0; 
	m_iAllCoCModifierIndex = 0; 
	m_saveStatus[0] = '\0'; 
	m_ShouldSyncTimeOfDayDofOverridedWidgetIndexidtoCurrentCameraCut = true; 

}

CCutSceneDebugManager::~CCutSceneDebugManager()
{

}

void CCutSceneDebugManager::Update()
{
	HighLightSelectedVehicle(); 
	DisplaySelectedVehicleInfo(); 
	DisplayPedVariation();
	UpdateInteriorPositionSelector(); 
	UpdateCurrentCutName(); 
	UpdateFirstPersonBlendOutDuration(); 

	if (m_bVehicleDamageSelector)
	{
		Vector3 vZOffset; vZOffset.Zero();
		UpdateMouseCursorPosition(m_vVehicleDamagerPos, m_vVehicleDamagerPos, vZOffset, false);
	}
}



void CCutSceneDebugManager::UpdateInteriorPositionSelector()
{
	const camBaseCamera* pCam = camInterface::GetDominantRenderedCamera(); 

	if(pCam)
	{
		m_InteriorPos =  pCam->GetFrame().GetPosition(); 

		CInteriorInst* pIntInst = NULL;
		s32 roomIdx = -1;
		pIntInst = CPortalVisTracker::GetPrimaryInteriorInst();
		roomIdx = CPortalVisTracker::GetPrimaryRoomIdx();
		const char* interiorName = "none";
		if (pIntInst && roomIdx>0 && roomIdx!=INTLOC_INVALID_INDEX)
		{
			// get the interior and room names
			CMloModelInfo *pModelInfo = reinterpret_cast<CMloModelInfo*>(pIntInst->GetBaseModelInfo());
			naAssertf(pModelInfo, "Failed to get model info from interior instance model index %d", pIntInst->GetModelIndex());

			
			if (pModelInfo)
			{
				interiorName = pModelInfo->GetModelName();
			}

			
		}
		strcpy(m_InteriorName, interiorName ); 
	}


}

void CCutSceneDebugManager::Clear()
{
	//Vehicle
	m_pVehicle = NULL; 
	m_iMainBodyColour = 0; 
	m_iSecondBodyColour = 0;
	m_iSpecularColour = 0; 
	m_iWheelTrimColour = 0;
	m_iBodyColour5 = 0;
	m_iBodyColour6 = 0;
	m_iLivery = 0;
	m_iLivery2 = 0;
	m_fDirtLevel = 0.0f; 
	m_bDisplayVehicleVariation = false; 
	for(int i=0; i <m_VehicleModelObjectList.GetCount(); i++ )
	{
		m_VehicleModelObjectList[i] = NULL; 
	}
	m_VehicleModelObjectList.Reset();
	
	for(int i = 0; i<m_VehicleDamageEvents.GetCount(); i++)
	{
		m_VehicleDamageEvents[i] = NULL;
	}
	m_VehicleDamageEvents.Reset(); 

	m_iVehicleDamageIndex = 0;

	//Ped
	m_pActorEntity = NULL; 
	m_iPedModelIndex = 0; 
	m_varDrawableId = 0; 
	m_varTextureId =0 ; 
	m_varComponentId = 0; 
	m_varPalette = 0; 
	m_maxDrawables = 0; 
	m_maxTextures = 0; 
	m_maxPalettes = 0; 
	strncpy(m_varDrawablName, "Invalid", 24);
	strncpy(m_varTexName, "Invalid", 24);

	//camera
	m_vCameraPos = VEC3_ZERO; 
	m_vCameraRot = VEC3_ZERO; 
	m_bOverrideCam = false; 
	m_cameraMtx = M34_IDENTITY;
	m_bOverrideCamUsingMatrix = false; 
	m_fFov = 45.0f; 
	m_vDofPlanes = Vector4(Vector4::ZeroType); 
	m_fFadeInDuration = 1.0f; 
	m_fFadeOutDuration = 1.0f; 
	m_vFadeInColor = VEC3_ZERO; 
	m_vFadeOutColor = VEC3_ZERO;
	m_bShouldOverrideUseLightDof = false;
	m_fMotionBlurStrength = 0.0f; 

	//ptfx
	m_iPtfxIndex =0; 

	//props
	m_iPropModelIndex = 0;
	m_pPropEntity = NULL; 

	for (int i=0; i < m_CutSceneVariations.GetCount(); i++)
	{
		m_CutSceneVariations[0]= NULL;  
	}
	m_CutSceneVariations.Reset(); 
	
	//Overlay 
	for(int i=0; i <m_OverlayObjectList.GetCount(); i++ )
	{
		m_OverlayObjectList[i] = NULL; 
	}
	m_OverlayObjectList.Reset(); 

	for(int i=0; i < m_OverlayEvents.GetCount(); i++)
	{
		m_OverlayEvents[i] = NULL; 
	}
	m_OverlayEvents.Reset(); 

	//Props
	for(int i=0; i < m_PropList.GetCount(); i++)
	{
		m_PropList[i] = NULL; 
	}
	m_PropList.Reset(); 

	for(int i=0; i < m_pAssetManagerList.GetCount(); i++)
	{
		m_pAssetManagerList[i] = NULL; 
	}
	m_pAssetManagerList.Reset(); 

	//ptfx
	m_PtfxEnt = NULL; 
	m_Ptfx = NULL;

	for(int i=0; i < m_PtfxList.GetCount(); i++)
	{
		m_PtfxList[i] = NULL; 
	}
	m_PtfxList.Reset(); 
	
	for(int i = 0; i <m_CameraCutsIndexEvents.GetCount(); i++)
	{
		m_CameraCutsIndexEvents[i] = NULL; 
	}
	m_CameraCutsIndexEvents.Reset(); 


	ClearMapObject();

	PopulateSelectorList(m_pPedSelectCombo, "Ped Selector", m_iPedModelIndex, CutSceneManager::GetInstance()->m_pedModelObjectList,datCallback(MFA(CCutSceneDebugManager::GetSelectedPedEntityCallBack), (datBase*)this) );
	PopulateSelectorList(m_pVehicleSelectCombo, "Vehicle Selector", m_iVehicleModelIndex, m_VehicleModelObjectList,datCallback(MFA(CCutSceneDebugManager::GetSelectedVehicleEntityCallBack), (datBase*)this) );
	PopulateSelectorList(m_pOverlaySelectCombo, "Overlay Selector", m_iOverlayIndex, m_OverlayObjectList, datCallback(MFA(CCutSceneDebugManager::GetSelectedOverlayEntityCallBack), (datBase*)this) );
	PopulateSelectorList(m_pPropSelectCombo, "Prop Selector", m_iPropModelIndex, m_PropList, datCallback(MFA(CCutSceneDebugManager::GetSelectedPropEntityCallBack), (datBase*)this));
	
	PopulateCameraCutEventList(NULL, m_pTimeCycleCameraCutCombo, m_iTimecycleEventsIndexId, datCallback(MFA(CCutSceneDebugManager::UpdateTimeCycleParamsCB), (datBase*)this) ); 
	PopulateCameraCutEventList(NULL, m_pCoCModifiersCameraCutCombo, m_iCoCModifiersCamerCutComboIndexId, datCallback(MFA(CCutSceneDebugManager::UpdateCoCModifierEventOverridesCB), (datBase*)this)); 

	UpdateFirstPersonEventList(); 

	m_CameraObjectList.Reset(); 

	//Timecycle
	m_bOverrideTimeCycle = false; 
	m_fTimeCycleOverrideTime = 0; 
	
	m_CascadeBounds.Reset();
}

void CCutSceneDebugManager::PopulateSelectorList(bkCombo* pCombo, const char* ComboName, int &index, atArray<const cutfObject*> &list,   datCallback callback  )
{
	if (pCombo)
	{	
		if ( list.GetCount() == 0 )
		{
#if __BANK
			if ( pCombo != NULL )
			{
				index = 0; 

				pCombo->UpdateCombo( ComboName, &index, 1, NULL, callback);

				pCombo->SetString( 0, "(none)" );
			}
#endif
			return;
		}
		if(list.GetCount() > 0)
		{
			pCombo->UpdateCombo( ComboName, &index, list.GetCount() + 1, NULL,  callback);

			pCombo->SetString( 0, "(none)" );

			for ( int i = 0; i < list.GetCount(); ++i )
			{
				pCombo->SetString( i + 1, list[i]->GetDisplayName().c_str() );
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
//Get the entity that managers vehicle and populate all the relevant lists. 
void CCutSceneDebugManager::GetSelectedVehicleEntityCallBack()
{
	if (m_iVehicleModelIndex > 0)
	{
		cutsEntity* pCutEntity = CutSceneManager::GetInstance()->GetEntityByObjectId(m_VehicleModelObjectList[m_iVehicleModelIndex - 1]->GetObjectId()); 

		CCutsceneAnimatedVehicleEntity* pEnt = static_cast<CCutsceneAnimatedVehicleEntity*> (pCutEntity); 

		if(pEnt)
		{
			m_pVehicle = pEnt; 
			GetVehicleSettings(); 
			PopulateVehicleExtraList(); 
			PopulateVehicleVariationEventList(m_VehicleModelObjectList[m_iVehicleModelIndex - 1]); 
			PopulateVehicleExtraEventList(m_VehicleModelObjectList[m_iVehicleModelIndex - 1]); 
			PopulateVehicleDamageEventList(m_VehicleModelObjectList[m_iVehicleModelIndex - 1]); 
		}
	}
	else
	{
		//selected none from the vehicle selector.
		m_pVehicle = NULL;
		ResetVehicleSettings();
		PopulateVehicleVariationEventList(NULL); 
		PopulateVehicleExtraEventList(NULL); 
		PopulateVehicleDamageEventList(NULL); 
	}
}

/////////////////////////////////////////////////////////////////////////////////////
//Get the current vehicle settings

void CCutSceneDebugManager::InitWidgets()
{
XPARAM(enablecutscenelightauthoring);

#if SETUP_CUTSCENE_WIDGET_FOR_CONTENT_CONTROLLED_BUILD 	
	bkGroup* CurrentSceneGrp = static_cast<bkGroup*>(BANKMGR.FindWidget("Cut Scene Debug/Current Scene/"));
	bkBank* Ragebank = BANKMGR.FindBank("Cut Scene Debug");
	if (CurrentSceneGrp)
	{
		Ragebank->SetCurrentGroup(*CurrentSceneGrp);

			Ragebank->PushGroup("Time Cycle"); 
				CutSceneManager::GetInstance()->CreateVCRWidget(Ragebank);
				Ragebank->AddToggle("Override Time Cycle", &m_bOverrideTimeCycle, NullCB, "Override Time Cycle"); 
				Ragebank->AddSlider("Current Time (minutes)", &m_fTimeCycleOverrideTime, 0, (24*60)-1, 1);
				Ragebank->AddText("Game Time", &m_cGameTime[0], sizeof(m_cGameTime)); 
			Ragebank->PopGroup(); 
		Ragebank->UnSetCurrentGroup(*CurrentSceneGrp);
	}
#else	
	bkBank* Ragebank = BANKMGR.FindBank("Cut Scene Debug");
	bkGroup* CurrentSceneGrp = static_cast<bkGroup*>(BANKMGR.FindWidget("Cut Scene Debug/Current Scene/"));
	bkGroup* DebugLinesGrp = static_cast<bkGroup*>(BANKMGR.FindWidget("Cut Scene Debug/Current Scene/Debug Display/Debug Display"));
	bkGroup* CameraGrp = static_cast<bkGroup*>(BANKMGR.FindWidget("Cut Scene Debug/Current Scene/Camera Editing"));
	bkGroup* PtfxGrp = static_cast<bkGroup*>(BANKMGR.FindWidget("Cut Scene Debug/Current Scene/Particle Effects Editing"));


	if(Ragebank)
	{
		if(DebugLinesGrp)
		{	
			Ragebank->SetCurrentGroup(*DebugLinesGrp);
				Ragebank->AddToggle( "Display Skeletons", &m_bDisplayAllSkeletons,
					datCallback(MFA(CCutSceneDebugManager::DisplaySceneSkeletons),this) );
				Ragebank->AddToggle("Display Loaded Models", &m_bDisplayLoadedModels); 
				Ragebank->AddToggle("Print Asset Loading To Debug", &m_bPrintModelLoadingToDebug); 
			Ragebank->UnSetCurrentGroup(*DebugLinesGrp);
		}
		
		if(CameraGrp)
		{
			Ragebank->SetCurrentGroup(*CameraGrp);
				Ragebank->PushGroup("Camera Overrides", false); //ped variation widget	
					Ragebank->AddToggle("Override Cam", &m_bOverrideCam, NullCB, "Override Cam"); 
					Ragebank->AddVector("Cam Position", &m_vCameraPos, -100000.0f, 100000.0f, 0.25f);
					Ragebank->AddVector("Cam Rotation", &m_vCameraRot, -360.0f, 360.0f, 1.0f);
					Ragebank->AddSeparator();
					Ragebank->AddToggle("Override Cam Using Matrix", &m_bOverrideCamUsingMatrix, NullCB, "Override Cam Using Matrix"); 
					Ragebank->AddMatrix("Cam Matrix", &m_cameraMtx, WORLDLIMITS_XMIN, WORLDLIMITS_XMAX, 0.01f);
					Ragebank->AddSeparator();
					Ragebank->AddToggle("Override Cam Using Binary", &m_bOverrideCamUsingBinary, NullCB, "Override the camera matrix direct from the binary data widget stream"); 
					Ragebank->AddDataWidget("BinaryCamData", (u8*)&m_binaryCamMatrix, sizeof(Matrix34));
						Ragebank->AddSeparator();
					Ragebank->AddSlider("Cam FOV", &m_fFov, g_MinFov, g_MaxFov, 0.1f);
						Ragebank->AddSeparator();
						Ragebank->AddCombo("Dof State", &m_DofState, 3, DofStates);
						Ragebank->AddSeparator();
						Ragebank->AddSlider("Near Out Of Focus Dof Plane", &m_vDofPlanes.x, 0.0, 5000.0f, 0.1f);
						Ragebank->AddSlider("Near In Focus Dof Plane", &m_vDofPlanes.y, 0.0f, 5000.0f, 0.1f);
						Ragebank->AddSlider("Far In Focus Dof Plane", &m_vDofPlanes.z, 0.0f,  5000.0f, 0.1f);
						Ragebank->AddSlider("Far Out Of Focus Dof Plane", &m_vDofPlanes.w, 0.0f,  5000.0f, 0.1f);
						//Ragebank->AddToggle("Use Light Dof", &m_bShouldOverrideUseLightDof, NullCB, "Use Shallow Dof"); 
						//Ragebank->AddToggle("Use Simple Dof", &m_bShouldOverrideUseSimpleDof, NullCB, "Use Simple Dof"); 
						Ragebank->AddSlider("Circle of Confusion (Dof Plane Strength)", &m_CoCRadius, 1, 15, 1); 
						Ragebank->PushGroup("Scene Day CoC Hours", false);
							Ragebank->AddButton("Save Day CoC Hours", datCallback(MFA(CCutSceneDebugManager::SaveDayCoCValues), (datBase*)this));
							Ragebank->AddToggle("Set All Hours To Use Day CoC Track", &m_bShouldSetCoCForAllDay, datCallback(MFA(CCutSceneDebugManager::ToggleNightCoCOverrides), (datBase*)this), "Use Shallow Dof");
							Ragebank->AddToggle("Set All Hours To Use Night CoC Track", &m_bShouldSetCoCForAllNight, datCallback(MFA(CCutSceneDebugManager::ToggleDayCoCOverrides), (datBase*)this), "Use Shallow Dof");
							Ragebank->AddToggle("Hours For Day CoC Values", &m_bShouldUseRangeForCoC, datCallback(MFA(CCutSceneDebugManager::ToggleDayRangeCoCOverrides), (datBase*)this), "Use Shallow Dof");
						Ragebank->PopGroup();
						Ragebank->PushGroup("Shot CoC Modifier", false);
							Ragebank->AddToggle("Override CoC Modifier", &m_bOverrideCoCModifier);
							Ragebank->AddToggle("Use Scene Day CoC Hours", &m_UseDayCoCHoursForModifier, datCallback(MFA(CCutSceneDebugManager::UpdateHourSliderToDayHours), (datBase*)this), "Override Cam Using Matrix"); 
							Ragebank->AddToggle("Use Scene Night CoC Hours", &m_UseNightCoCHoursForModifier, datCallback(MFA(CCutSceneDebugManager::UpdateHourSliderToNightHours), (datBase*)this), "Override Cam Using Matrix"); 
							Ragebank->AddSlider("Time Of Day Dof Modifier", &m_CoCModifier, -15, 15, 1); 
							Ragebank->PushGroup("Modifier Hours", false);
								for(int i = 0; i < 24; i++)
								{
									char name[24];
									if(i == 0)
									{
										formatf(name, sizeof(name)-1, "Hour %d (Midnight)", i); 
									}
									else if(i == 12)
									{
										formatf(name, sizeof(name)-1, "Hour %d (Midday)", i); 
									}
									else
									{
										formatf(name, sizeof(name)-1, "Hour %d", i); 
									}
									u32 bitsPerBlock = sizeof(u32) * 8;
									int block = i / bitsPerBlock;
									int bitInBlock = i - (block * bitsPerBlock);
									Ragebank->AddToggle(name, &m_CoCModifierHours, (u32)(1 << bitInBlock));

								}
							Ragebank->PopGroup(); 
							const char *CoCModifierOverrides[] = { "(none)" };	
							Ragebank->AddSeparator();
							Ragebank->AddText("CurrentCut:",&m_CameraCutName[0], 128, true);
							const char *CameraCutList[] = { "(none)" };	
							m_pCoCModifiersCameraCutCombo = dynamic_cast<bkCombo *>( Ragebank->AddCombo( "Camera Cut Events", &m_iCoCModifiersCamerCutComboIndexId, 1, CameraCutList, datCallback(MFA(CCutSceneDebugManager::UpdateCoCModifierEventOverridesCB), (datBase*)this) ) );
							Ragebank->AddToggle("Sync the Camera Cut Events widget to the current cut", &m_ShouldSyncTimeOfDayDofOverridedWidgetIndexidtoCurrentCameraCut, NullCB, "Sync camera cut events to current cut"); 
							m_pPerCutCoCModifiersCombo = dynamic_cast<bkCombo *>( Ragebank->AddCombo( "Per Camera Cut CoC Modifiers", &m_iCoCModifierIndex, 1, CoCModifierOverrides ) );
							Ragebank->AddButton("Save CoC Modifier", datCallback(MFA(CCutSceneDebugManager::BankSaveCoCModifierCallBack), (datBase*)this)); 
							Ragebank->AddButton("Delete CoC Modifier", datCallback(MFA(CCutSceneDebugManager::DeleteCoCModifierCallBack), (datBase*)this));
							Ragebank->AddButton("Save CoC Modifier to ALL camera cuts", datCallback(MFA(CCutSceneDebugManager::BankSaveAllCoCModifiersCallBack), (datBase*)this)); 
							Ragebank->AddButton("Delete all CoC Modifiers", datCallback(MFA(CCutSceneDebugManager::DeleteAllCoCModifierOverrideCallBack), (datBase*)this)); 
							m_pSaveStatusText = Ragebank->AddText("Save Status:",&m_saveStatus[0], 64, true);
							m_AllCoCModifiersCombo = dynamic_cast<bkCombo *>( Ragebank->AddCombo( "All CoC Modifiers (READ ONLY)", &m_iAllCoCModifierIndex, 1, CoCModifierOverrides ) );
						Ragebank->PopGroup();
						Ragebank->PushGroup("First Person");
								m_firstPersonEventStatus = Ragebank->AddText("Event Save Status:",&m_firstPersonStatusText[0], 256, true);		
								const char *BlendOutEvents[] = { "(none)" };	
								m_firstPersonBlendOutComboSlider = Ragebank->AddCombo("First person Camera Events", &m_firstPersonBlendOutComboIndex, 1, BlendOutEvents); 
							Ragebank->PushGroup("First Person Blend Out");
								m_FirstPersonBlendOutSlider = Ragebank->AddSlider("Blend Out Duration", &m_firstPersonCameraBlendDuration, 0.0f, 1000.0f, 0.1f); 
								Ragebank->AddButton("Add First Person BlendOut", datCallback(MFA(CCutSceneDebugManager::AddFirstPersonBlendOutEvent), (datBase*)this)); 
							Ragebank->PopGroup();
							Ragebank->PushGroup("First Person Catch Up");
								Ragebank->AddButton("Add First Person catchup", datCallback(MFA(CCutSceneDebugManager::AddFirstPersonCatchUpEvent), (datBase*)this)); 
							Ragebank->PopGroup();
							Ragebank->AddButton("Delete First Person camera Event", datCallback(MFA(CCutSceneDebugManager::DeleteFirstPersonCameraEvent), (datBase*)this));
							Ragebank->AddSeparator(); 
							Ragebank->AddButton("Save Data Cut file", datCallback(MFA(CCutSceneDebugManager::SaveFirstPersonCameraEvents), (datBase*)this)); 
						Ragebank->PopGroup();
					Ragebank->AddSeparator();
						Ragebank->AddToggle("Render Dof Planes", &m_RenderDofPlanes, NullCB, "Use Render Dof Planes"); 
						Ragebank->AddSlider("Near Strength Plane Alpha (Pink)", &m_NearOutPlaneAlpha, 0, 255, 1); 
						Ragebank->AddSlider("Near Plane Alpha (Orange)", &m_NearInPlaneAlpha, 0, 255, 1); 
						Ragebank->AddSlider("Far Plane Alpha (Blue)", &m_FarInPlaneAlpha, 0, 255, 1); 
						Ragebank->AddSlider("Far Strength Plane Alpha (Green)", &m_FarOutPlaneAlpha, 0, 255, 1); 
						Ragebank->AddSlider("Focal Point Alpha (Red)", &m_FocalPointAlpha, 0, 255, 1); 
						Ragebank->AddSlider("Set Absolute Plane Scale", &m_PlaneScale, 0.0f,  50.0f, 0.1f); 
						Ragebank->AddSlider("Set Focal Point Scale", &m_FocalPointScale, 0.0f,  50.0f, 0.1f);
						Ragebank->AddToggle("Scale Planes as a screen ratio", &m_scaleAsScreenRatio, NullCB, "Set the planes to render as a ratio of the screen"); 
						Ragebank->AddToggle("Set Planes Solid", &m_SetPlaneSolid, NullCB, "Set the planes to render solidly"); 
					Ragebank->AddSeparator();

					Ragebank->AddSlider("Cam Motion Blur", &m_fMotionBlurStrength, 0.0f,  1.0f, 0.01f);
					Ragebank->AddSeparator();
					Ragebank->AddButton("Fade Out", datCallback(MFA(CCutSceneDebugManager::BankFadeScreenOutCallBack), (datBase*)this));
					Ragebank->AddSlider("Fade Out Duration", &m_fFadeOutDuration, 0.0f,  1000.0f, 1.0f);
					Ragebank->AddColor( "Fade Out Colour", &m_vFadeOutColor);
					Ragebank->AddButton("Fade In", datCallback(MFA(CCutSceneDebugManager::BankFadeScreenInCallBack), (datBase*)this));
					Ragebank->AddSlider("Fade In Duration", &m_fFadeInDuration, 0.0f,  1000.0f, 1.0f);
					Ragebank->AddColor( "Fade In Colour", &m_vFadeInColor);
					Ragebank->AddSeparator();
					if(PARAM_enablecutscenelightauthoring.Get())
					{
						m_bOverrideCamBlendOutEvents = true;
					}
					Ragebank->AddToggle("Override Camera Blendout Events", &m_bOverrideCamBlendOutEvents, NullCB, ""); 

				Ragebank->PopGroup();
				Ragebank->PushGroup("Camera Final Frame", false); //ped variation widget	
					Ragebank->AddMatrix("Cam Final Frame", &m_FinalFrameCameraMat,-100000.0f, 100000.0f, 0.1f); 
				Ragebank->PopGroup();
				Ragebank->PushGroup("Timecycle Modifiers", false); // timecycle modifier values
					Ragebank->AddText("CurrentCut:",&m_CameraCutName[0], 128, true);
					//const char *CameraCutList[] = { "(none)" };	
					m_pTimeCycleCameraCutCombo = dynamic_cast<bkCombo *>( Ragebank->AddCombo( "Camera Cut Events", &m_iTimecycleEventsIndexId, 1, CameraCutList, datCallback(MFA(CCutSceneDebugManager::UpdateTimeCycleParamsCB), (datBase*)this) ) );
					Ragebank->AddSlider("ReflectionLodRangeStart", &m_ReflectionLodRangeStart, -1.0, 10000.0f, 0.1f);
					Ragebank->AddSlider("ReflectionLodRangeEnd", &m_ReflectionLodRangeEnd, -1.0, 10000.0f, 0.1f);
					Ragebank->AddSlider("ReflectionSLodRangeStart", &m_ReflectionSLodRangeStart, -1.0, 10000.0f, 0.1f);
					Ragebank->AddSlider("ReflectionSLodRangeEnd", &m_ReflectionSLodRangeEnd, -1.0, 10000.0f, 0.1f);
					Ragebank->AddSlider("LodMult", &m_LodMult, -1.0, 10000.0f, 0.1f); 
					Ragebank->AddSlider("LodMultHD", &m_LodMultHD, -1.0, 10000.0f, 0.1f);
					Ragebank->AddSlider("LodMultOrphanedHD", &m_LodMultOrphanedHD, -1.0, 10000.0f, 0.1f);
					Ragebank->AddSlider("LodMultLod", &m_LodMultLod, -1.0, 10000.0f, 0.1f);
					Ragebank->AddSlider("LodMultSLod1", &m_LodMultSLod1, -1.0, 10000.0f, 0.1f);
					Ragebank->AddSlider("LodMultSLod2", &m_LodMultSLod2, -1.0, 10000.0f, 0.1f);
					Ragebank->AddSlider("LodMultSLod3", &m_LodMultSLod3, -1.0, 10000.0f, 0.1f);
					Ragebank->AddSlider("LodMultSLod4", &m_LodMultSLod4, -1.0, 10000.0f, 0.1f);
					Ragebank->AddSlider("WaterReflectionClip", &m_WaterReflectionFarClip, -1.0, 10000.0f, 0.1f);
					Ragebank->AddSlider("SSAOInten", &m_SSAOLightInten, -1.0, 10000.0f, 0.1f);
					Ragebank->AddSlider("ExposurePush", &m_ExposurePush, -1.0, 1.0f, 0.1f);
					Ragebank->AddSlider("Directional light mult", &m_DirectionalLightMultiplier, -1.0, 64.0f, 1.0f);
					Ragebank->AddSlider("Lend Artefact mult", &m_LensArtefactMultiplier, -1.0, 1.0f, 0.01f);
					Ragebank->AddSlider("Max Bloom", &m_MaxBloom, -1.0, 16.0f, 0.1f);
					Ragebank->AddToggle("Freeze Reflection Map", &m_FreezeReflectionMap, NullCB, "Freezes the reflection map for the duration of this camera cut");
					Ragebank->AddToggle("Disable Directional Light", &m_DisableDirectionalLight, NullCB, "Disables the worlds directional light for the duration cut");
					Ragebank->AddToggle("Enable Absolute Intensity", &m_AbsoluteIntensityEnabled, NullCB, "Enables the absolute intensity");
					Ragebank->AddSlider("LightFadeDistanceMult", &m_LightFadeDistanceMult, 0.0, 1.0f, 0.1f);
					Ragebank->AddSlider("LightShadowFadeDistanceMult", &m_LightShadowFadeDistanceMult, 0.0, 1.0f, 0.1f);
					Ragebank->AddSlider("LightSpecularFadeDistMult", &m_LightSpecularFadeDistMult, 0.0, 1.0f, 0.1f);
					Ragebank->AddSlider("LightVolumetricFadeDistanceMult", &m_LightVolumetricFadeDistanceMult, 0.0, 1.0f, 0.1f);
					/*	Ragebank->PushGroup("DOF Override", false);
					{
					Ragebank->AddToggle("Disable High Quality DOF (override)", &m_bDisableHighQualityDof, NullCB, "If true it disables the High Quality DOF, overriding cutscene DOF events");
					}
					Ragebank->PopGroup(); // DOF Override*/		
					Ragebank->PushGroup("Character Light", false);
					{
						Ragebank->AddToggle("Use timecycle values", &m_CharacterLight.m_bUseTimeCycleValues, NullCB, "Override Cam Using Matrix"); 
						Ragebank->AddToggle("Use as timecycle modifier", &m_CharacterLight.m_bUseAsIntensityModifier , NullCB, "Use the intensity as a time cycle modifier"); 
						Ragebank->AddVector("Direction", &m_CharacterLight.m_vDirection, -1.0f, 1.0f, 0.01f);
						Ragebank->AddColor("Colour", &m_CharacterLight.m_vColour);
						Ragebank->AddSlider("Intensity", &m_CharacterLight.m_fIntensity, 0.0f, 64.0f, 0.1f);
					}
					Ragebank->PopGroup(); //ped variation widget	
					Ragebank->AddButton("Save Time Cycle Params", datCallback(MFA(CCutSceneDebugManager::BankSaveTimeCycleParamsCallBack), (datBase*)this));
				Ragebank->PopGroup();		
				Ragebank->PushGroup("Interior", false); //ped variation widget
					const char *InteriorLoadList[] = { "(none)" };
					Ragebank->AddText("Interior name:",&m_InteriorName[0], 24, true);
					Ragebank->AddVector("Cam Position for Interior: ", &m_InteriorPos, -100000.0f, 100000.0f, 0.25f);
					m_pLoadInteriorIndexCombo = dynamic_cast<bkCombo *>( Ragebank->AddCombo( "Load Interior Events", &m_LoadInteriorIndexId, 1, InteriorLoadList ) );
					Ragebank->AddButton("Save Load Interior Event", datCallback(MFA(CCutSceneDebugManager::BankSaveLoadInteriorCallBack), (datBase*)this));
					Ragebank->AddButton("Delete Load Interior Event", datCallback(MFA(CCutSceneDebugManager::BankDeleteLoadInteriorsCallBack), (datBase*)this));
				Ragebank->PopGroup();
				m_CameraWidgetData = Ragebank->AddDataWidget("Camera Override Data", NULL,1024, datCallback(MFA(CCutSceneDebugManager::BankCameraWidgetReceiveDataCallback), (datBase*)this), 0, false);
			Ragebank->UnSetCurrentGroup(*CameraGrp);
		}

		if(PtfxGrp)
		{
			Ragebank->SetCurrentGroup(*PtfxGrp);
				Ragebank->PushGroup("Particle Effects Overrides", false); //ped variation widget	
					const char *PtfxList[] = { "(none)" };

					m_pPTFXSelectCombo = dynamic_cast<bkCombo *>( Ragebank->AddCombo( "Ptfx Selector", &m_iPtfxIndex, 1, PtfxList ) );
					Ragebank->AddToggle("Override Ptfx", &m_OverridePtfx, datCallback(MFA(CCutSceneDebugManager::SetCanOverridePtfx), this)); 
					Ragebank->AddVector("PTFX Position", &m_vPTFXPos, -100000.0f, 100000.0f, 0.25f, datCallback(MFA(CCutSceneDebugManager::UpdatePtfxPos), (datBase*)this));
					Ragebank->AddVector("PTFX Rotation", &m_vPTFXRot, -360.0f, 360.0f, 1.0f, datCallback(MFA(CCutSceneDebugManager::UpdatePtfxRot), (datBase*)this));
				Ragebank->PopGroup();
			Ragebank->UnSetCurrentGroup(*PtfxGrp);
		}

		if (CurrentSceneGrp)
		{
			Ragebank->SetCurrentGroup(*CurrentSceneGrp);
			Ragebank->PushGroup("Ped Variations", false); //ped variation widget	
					CutSceneManager::GetInstance()->CreateVCRWidget(Ragebank);
				
					m_pVarPedFrameSlider = Ragebank->AddSlider( "Current Frame", &CutSceneManager::GetInstance()->m_iCurrentFrameWithFrameRanges, 0, MAX_FRAMES, 1, 
						datCallback(MFA(CutSceneManager::BankFrameScrubbingCallback),CutSceneManager::GetInstance()) );

					//need a non const version of the phase.... boo
					//Ragebank->AddSlider("phase", &CutSceneManager::GetInstance()->GetCutScenePhase(), 0.0f 1.0f, 0.0f); 

					Ragebank->AddSeparator();
					
					const char *PedNameList[] = { "(none)" };

					m_pPedSelectCombo = dynamic_cast<bkCombo *>( Ragebank->AddCombo( "Ped Selector", &m_iPedModelIndex, 1, PedNameList ) );
					const char *EventNameList[] = { "(none)" }; //events
					m_pEventCombo = dynamic_cast<bkCombo *>( Ragebank->AddCombo( "Ped var events", &m_PedVarEventId, 1, EventNameList ) );

				Ragebank->PushGroup("Ped Variations", false); //ped variation widget

					const char* varSlotList[128];
					for (u32 i=0; i<PV_MAX_COMP; i++)
					{
						varSlotList[i] = varSlotNames[i];
					}
					Ragebank->AddText("Drawable name:",&m_varDrawablName[0], 24, true);
					Ragebank->AddText("Texture name:",&m_varTexName[0], 24, true);
					Ragebank->AddCombo("Component", &m_varComponentId, 12, &varSlotList[0], datCallback(MFA(CCutSceneDebugManager::UpdateCutSceneVarComponentCB), (datBase*)this));
					
					Ragebank->AddSlider("Max Drawable:", &m_maxDrawables, 0, 32, 0);
					m_pVarDrawablIdSlider = Ragebank->AddSlider("Drawable Id", &m_varDrawableId, 0, 32, 1, datCallback(MFA(CCutSceneDebugManager::UpdateCutSceneVarDrawblCB), (datBase*)this));
					
					Ragebank->AddSlider("Max Texture:", &m_maxTextures, 0, 32, 0);
					m_pVarTexIdSlider = Ragebank->AddSlider("Texture Id", &m_varTextureId, 0, 32, 1, datCallback(MFA(CCutSceneDebugManager::UpdateCutSceneVarTexCB), (datBase*)this));
					//bank.AddSlider("Max Palette:", &m_maxPalettes, 0, 15, 0);
					//m_pVarPaletteIdSlider =  bank.AddSlider("Palette Id", &m_varPaletteId, 0, 15, 1, datCallback(MFA(CutSceneManager::UpdateCutscneVarPaletteCB), (datBase*)this)));

				
					Ragebank->AddButton("Save Ped Var Event", datCallback(MFA(CCutSceneDebugManager::BankSavePedVariationEventCallBack), (datBase*)this));
					Ragebank->AddButton("Delete Ped Var Event", datCallback(MFA(CCutSceneDebugManager::BankDeletePedVariationCallBack), (datBase*)this));
				Ragebank->PopGroup();

				Ragebank->PushGroup("Prop Variations", false); //ped variation widget	
					Ragebank->AddSeparator();
				
					const char* propSlotList[128];
					
					for (u32 i=0; i<NUM_ANCHORS; i++)
					{
						propSlotList[i] = propSlotNames[i];
					}
					Ragebank->AddCombo("Prop Slot", &m_PropSlotId, NUM_ANCHORS, &propSlotList[0], datCallback(MFA(CCutSceneDebugManager::UpdateCutScenePropComponentCB), (datBase*)this));
					
					m_pPropDrawablIdSlider = Ragebank->AddSlider("Prop Id", &m_PropDrawableId, -1, 32, 1, datCallback(MFA(CCutSceneDebugManager::UpdateCutScenePropDrawblCB), (datBase*)this));
					
					m_pPropTexIdSlider = Ragebank->AddSlider("Tex Id", &m_PropTextureId, -1, 32, 1, datCallback(MFA(CCutSceneDebugManager::UpdateCutScenePropTexCB), (datBase*)this));
					Ragebank->AddButton("Save Prop Var Event", datCallback(MFA(CCutSceneDebugManager::BankSavePedPropVariationEventCallBack), (datBase*)this));
					Ragebank->AddButton("Delete Ped Var Event", datCallback(MFA(CCutSceneDebugManager::BankDeletePedVariationCallBack), (datBase*)this));
						
					
				Ragebank->PopGroup();
			Ragebank->PopGroup();
			
			//** VEHICLE VARITAION **
			Ragebank->PushGroup("Vehicle Variation"); 
				CutSceneManager::GetInstance()->CreateVCRWidget(Ragebank);
				m_pVarFrameSlider = Ragebank->AddSlider( "Current Frame", &CutSceneManager::GetInstance()->m_iCurrentFrameWithFrameRanges, 0, MAX_FRAMES, 1, 
					datCallback(MFA(CutSceneManager::BankFrameScrubbingCallback),CutSceneManager::GetInstance()) );

				Ragebank->AddToggle("Display Vehicle Var Config", &m_bDisplayVehicleVariation, NullCB, "Display Vehicle Variation"); 

				const char *VehicleNameList[] = { "(none)" };
				m_pVehicleSelectCombo = dynamic_cast<bkCombo *>( Ragebank->AddCombo( "Vehicle Selector", &m_iVehicleModelIndex, 1, VehicleNameList ) );				

				Ragebank->AddSlider("Main Body Colour", &m_iMainBodyColour, 0, 255, 1, datCallback(MFA(CCutSceneDebugManager::UpdateVehicleBody1ColourCallBack), (datBase*)this));
				Ragebank->AddSlider("Secondary Body Colour", &m_iSecondBodyColour, 0, 255, 1, datCallback(MFA(CCutSceneDebugManager::UpdateVehicleBody2ColourCallBack), (datBase*)this));
				Ragebank->AddSlider("Body Specular", &m_iSpecularColour, 0, 255, 1, datCallback(MFA(CCutSceneDebugManager::UpdateVehicleBody3ColourCallBack), (datBase*)this));
				Ragebank->AddSlider("Wheel Trim", &m_iWheelTrimColour, 0, 255, 1, datCallback(MFA(CCutSceneDebugManager::UpdateVehicleBody4ColourCallBack), (datBase*)this));
				Ragebank->AddSlider("Body Colour 5", &m_iBodyColour5, 0, 255, 1, datCallback(MFA(CCutSceneDebugManager::UpdateVehicleBody5ColourCallBack), (datBase*)this));
				Ragebank->AddSlider("Body Colour 6", &m_iBodyColour6, 0, 255, 1, datCallback(MFA(CCutSceneDebugManager::UpdateVehicleBody6ColourCallBack), (datBase*)this));

				Ragebank->AddSlider("Dirt", &m_fDirtLevel, 0.0f, 15.0f, 0.5f, datCallback(MFA(CCutSceneDebugManager::UpdateVehicleDirtCallBack), (datBase*)this));
				m_pLiverySlider = Ragebank->AddSlider("Livery", &m_iLivery, 0, MAX_NUM_LIVERIES, 1, datCallback(MFA(CCutSceneDebugManager::UpdateVehicleLiveriesCallBack), (datBase*)this));
				m_pLivery2Slider= Ragebank->AddSlider("Livery2", &m_iLivery2, 0, MAX_NUM_LIVERIES, 1, datCallback(MFA(CCutSceneDebugManager::UpdateVehicleLiveriesCallBack), (datBase*)this));

				//Vehicle Variation
				const char *VehicleEventNameList[] = { "(none)" }; //events
				m_pVehicleVariationEventCombo = dynamic_cast<bkCombo *>( Ragebank->AddCombo( "Vehicle var events", &m_VehicleVarEventId, 1, VehicleEventNameList ) );
				
				Ragebank->AddButton("Save Vehicle Var Event", datCallback(MFA(CCutSceneDebugManager::BankSaveVehicleVariationEventCallBack), (datBase*)this));
				Ragebank->AddButton("Delete Vehicle Var Event", datCallback(MFA(CCutSceneDebugManager::BankDeleteVehicleVariationCallBack), (datBase*)this));

				Ragebank->AddSeparator();

				//Vehicle extras
				const char *VehicleExtraList[] = { "(none)" };
				m_pVehicleExtraCombo = dynamic_cast<bkCombo *>( Ragebank->AddCombo( "Extra Selector", &m_iVehicleExtraIndex, 1, VehicleExtraList ) );
				Ragebank->AddButton("Hide Vehicle Extra", datCallback(MFA(CCutSceneDebugManager::HideVehicleExtraCallBack), (datBase*)this));
				Ragebank->AddButton("Show Vehicle Extra", datCallback(MFA(CCutSceneDebugManager::ShowVehicleExtraCallBack), (datBase*)this));
				const char *VehicleExtraEventList[] = { "none" }; 
				m_pVehicleExtraEventCombo = dynamic_cast<bkCombo *>( Ragebank->AddCombo( "Vehicle Extra events", &m_VehicleExtraEventId, 1, VehicleExtraEventList ) );
				Ragebank->AddButton("Save Vehicle Extra Event", datCallback(MFA(CCutSceneDebugManager::BankSaveVehicleExtraCallBack), (datBase*)this));
				Ragebank->AddButton("Delete Vehicle Extra Event", datCallback(MFA(CCutSceneDebugManager::BankDeleteVehicleExtrasCallBack), (datBase*)this));

				Ragebank->PushGroup("Vehicle Damage"); 
				const char *VehicleDamageEventList[] = { "(none)" };
				m_pVehicleDamageEventCombo = dynamic_cast<bkCombo *>( Ragebank->AddCombo( "Damage Event Selector", &m_iVehicleDamageIndex, 1, VehicleDamageEventList ) );
					Ragebank->AddToggle("Activate Vehicle Damage Pos", &m_bVehicleDamageSelector, NullCB, "Activate Vehicle Damage Pos"); 
					Ragebank->AddSlider("Deformation", &m_fDerformation, 0.0f, 1000.0f, 1.0f);
					Ragebank->AddSlider("Damage", &m_fDamageValue, 0.0f, 1000.0f, 1.0f);
					Ragebank->AddButton("ApplyDamage", datCallback(MFA(CCutSceneDebugManager::BankApplyVehicleDamageEventCallBack), (datBase*)this));
					Ragebank->AddButton("Save Vehicle Damage Event", datCallback(MFA(CCutSceneDebugManager::BankSaveVehicleDamageEvent), (datBase*)this));
					Ragebank->AddButton("Delete Vehicle Damage Event", datCallback(MFA(CCutSceneDebugManager::BankDeleteVehicleDamageEvent), (datBase*)this));
				Ragebank->PopGroup(); 
			Ragebank->PopGroup(); 
			
			//Time Cycle debugging
			Ragebank->PushGroup("Time Cycle"); 
				CutSceneManager::GetInstance()->CreateVCRWidget(Ragebank);
				Ragebank->AddToggle("Override Time Cycle", &m_bOverrideTimeCycle, NullCB, "Override Time Cycle"); 
				Ragebank->AddSlider("Current Time (minutes)", &m_fTimeCycleOverrideTime, 0, (24*60)-1, 1);
				Ragebank->AddText("Game Time", &m_cGameTime[0], sizeof(m_cGameTime)); 
			Ragebank->PopGroup(); 
			
			//Time Cycle debugging
			Ragebank->PushGroup("Overlay"); 
				CutSceneManager::GetInstance()->CreateVCRWidget(Ragebank);
				m_pOverlayPlayBackSlider = Ragebank->AddSlider( "Current Frame", &CutSceneManager::GetInstance()->m_iCurrentFrameWithFrameRanges, 0, MAX_FRAMES, 1, 
					datCallback(MFA(CutSceneManager::BankFrameScrubbingCallback),CutSceneManager::GetInstance()) );
				const char *OverlayNameList[] = { "(none)" };
				m_pOverlaySelectCombo = dynamic_cast<bkCombo *>( Ragebank->AddCombo( "Overlay Selector", &m_iOverlayIndex, 1, OverlayNameList ) );				 
				const char *OverlayEventsList[] = { "(none)" };
				m_pOverlayEventCombo = dynamic_cast<bkCombo *>( Ragebank->AddCombo( "Overlay Events", &m_OverlayEventId, 1, OverlayEventsList ) );
				
				Ragebank->PushGroup("Overlay Events"); 
					Ragebank->PushGroup("Overlay Property Events"); 
						Ragebank->AddSlider("Pos.x", &m_vOverlayPos.x, 0.0f, 1.0f, 0.1f, datCallback(MFA(CCutSceneDebugManager::SetOverlayPositionCB), (datBase*)this)); 
						Ragebank->AddSlider("Pos.y", &m_vOverlayPos.y, 0.0f, 1.0f, 0.1f,datCallback(MFA(CCutSceneDebugManager::SetOverlayPositionCB), (datBase*)this)); 
						Ragebank->AddSlider("Size.x", &m_vOverlaySize.x, 0.0f, 1.0f, 0.1f, datCallback(MFA(CCutSceneDebugManager::SetOverlaySizeCB), (datBase*)this)); 
						Ragebank->AddSlider("Size.y", &m_vOverlaySize.y, 0.0f, 1.0f, 0.1f, datCallback(MFA(CCutSceneDebugManager::SetOverlaySizeCB), (datBase*)this)); 
						Ragebank->AddToggle("Use Screen Render Target", &m_bUseScreenRenderTarget, NullCB, "Use Screen Render Target");
						Ragebank->AddButton("Save Overlay Property Event", datCallback(MFA(CCutSceneDebugManager::BankSaveMovieProperties), (datBase*)this));
					Ragebank->PopGroup(); 
					
					Ragebank->PushGroup("Overlay Method Events"); 
						const char *OverlayMethodNameList[] = { "(none)" };
						m_pOverlayMethodsCombo = dynamic_cast<bkCombo *>( Ragebank->AddCombo( "Overlay Method Selector", &m_iOverlayMethodIndex, 1, OverlayMethodNameList ) );	
							
						Ragebank->PushGroup("Params"); 
							m_pOverlayParamNames = dynamic_cast<bkCombo *>( Ragebank->AddCombo( "Param Name", &m_iOverlayParamNameIndex, MAX_NUMBER_OVERLAY_PARAMS, OverlayParamNames ) );	
							m_pOverlayParamTypes = dynamic_cast<bkCombo *>( Ragebank->AddCombo( "Param Type", &m_iOverlayParamTypeIndex, MAX_NUMBER_PARAM_TYPES, OverlayParamTypes) );	
							Ragebank->AddButton("Add Overlay Param", datCallback(MFA(CCutSceneDebugManager::CreateParamWidgets), (datBase*)this));
							Ragebank->AddButton("Delete Overlay Param", datCallback(MFA1(CCutSceneDebugManager::DeleteParamWidget), (datBase*)this,  (CallbackData)(size_t)m_iOverlayParamNameIndex));
						Ragebank->PopGroup(); 
							Ragebank->AddButton("Add Overlay Method Event", datCallback(MFA(CCutSceneDebugManager::BankSaveOverlayEventsCallBack), (datBase*)this)); 
					Ragebank->PopGroup(); 	
					
					Ragebank->AddButton("Delete Overlay Event", datCallback(MFA(CCutSceneDebugManager::BankDeleteOverlayEventCallBack), (datBase*)this));
				
				Ragebank->PopGroup(); 
			Ragebank->PopGroup(); 
			
			Ragebank->PushGroup("Props"); 
				const char *PropNameList[] = { "(none)" };
				m_pPropSelectCombo = dynamic_cast<bkCombo *>( Ragebank->AddCombo( "Prop Selector", &m_iPropModelIndex, 1, PropNameList ) );				 
			Ragebank->PopGroup(); 


			Ragebank->PushGroup("Set Water Reflection Index"); 
				CutSceneManager::GetInstance()->CreateVCRWidget(Ragebank);
				m_pWaterIndexFrameSlider = Ragebank->AddSlider( "Current Frame", &CutSceneManager::GetInstance()->m_iCurrentFrameWithFrameRanges, 0, MAX_FRAMES, 1, 
					datCallback(MFA(CutSceneManager::BankFrameScrubbingCallback),CutSceneManager::GetInstance()) );
				const char *WaterRelectList[] = { "(none)" };
				m_pWaterRefractionIndexCombo = dynamic_cast<bkCombo *>( Ragebank->AddCombo( "Water Reflection Event", &m_iWaterRefractionIndexId, 1, WaterRelectList ) );				 
				Ragebank->AddSlider("Water Index", &m_fWaterRefractionIndex, 0.0f, 5.0f, 0.1f, datCallback(MFA(CCutSceneDebugManager::SetOverlayPositionCB), (datBase*)this)); 
				
				Ragebank->AddButton("Save Water Reflection Event", datCallback(MFA(CCutSceneDebugManager::BankSaveWaterRefractionIndexEventCallBack), (datBase*)this));
				Ragebank->AddButton("Delete Water Reflection Event", datCallback(MFA(CCutSceneDebugManager::BankDeleteWaterRefractionIndexEventCallBack), (datBase*)this));
				
			Ragebank->PopGroup();

			Ragebank->PushGroup("Profiling");
			{
				Ragebank->AddToggle("Turn off cutscene lights", &m_bProfilingDisableCsLights);
				Ragebank->AddToggle("Override character light", &m_bProfilingOverrideCharacterLight);
				Ragebank->AddToggle("Show all peds", &g_DrawPeds);
				Ragebank->AddToggle("Hide all props", &CEntity::ms_cullProps);
				Ragebank->AddToggle("Turn off DOF", &m_bProfilingDisableDof);
				Ragebank->AddToggle("Use default FOV", &m_bProfilingUseDefaultFov);
			}
			Ragebank->PopGroup();

			Ragebank->PushGroup("Validation");
			{
				Ragebank->AddToggle("Validate shadow events", &m_cutfileValidationProcesses[CVP_SHADOW_EVENTS]);
				Ragebank->AddToggle("fix shadow events", &m_cutfileValidationProcesses[CVP_FIX_SHADOW_EVENTS]);
				Ragebank->AddToggle("Validate light events", &m_cutfileValidationProcesses[CVP_LIGHT_EVENTS]);
				Ragebank->AddButton("Validate this Scene", datCallback(MFA(CCutSceneDebugManager::ValidateEventInThisScene), (datBase*)this));
				Ragebank->AddButton("Validate All Scenes", datCallback(MFA(CCutSceneDebugManager::BatchProcessCutfilesInMemory), (datBase*)this));
			
			}
			Ragebank->PopGroup();

			m_CutsceneStateWidgetData = Ragebank->AddDataWidget("Cutscene Data", &m_CutsceneStateWidgetDataBuffer[0], CUTSCENE_STATE_DATA_WIDGET_SIZE, datCallback(MFA(CCutSceneDebugManager::BankCutsceneWidgetReceiveDataCallback), (datBase*)this), 0, false);
			Ragebank->UnSetCurrentGroup(*CurrentSceneGrp);
		}
	}	

	m_CascadeBounds.InitWidgets(); 

	if (CutSceneManager::GetInstance()->IsCutscenePlayingBack())
	{
		if(m_pPedSelectCombo)
		{
			PopulateSelectorList(m_pPedSelectCombo, "Ped Selector", m_iPedModelIndex, CutSceneManager::GetInstance()->m_pedModelObjectList,datCallback(MFA(CCutSceneDebugManager::GetSelectedPedEntityCallBack), (datBase*)this) );
		}

		if(m_pVehicleSelectCombo)
		{
			PopulateSelectorList(m_pVehicleSelectCombo, "Vehicle Selector", m_iVehicleModelIndex, m_VehicleModelObjectList,datCallback(MFA(CCutSceneDebugManager::GetSelectedVehicleEntityCallBack), (datBase*)this) );
		}
		
		if(m_pOverlaySelectCombo)
		{
			PopulateSelectorList(m_pOverlaySelectCombo, "Overlay Selector", m_iOverlayIndex, m_OverlayObjectList, datCallback(MFA(CCutSceneDebugManager::GetSelectedOverlayEntityCallBack), (datBase*)this) );
		}
		
		if(m_pPropSelectCombo)
		{
			PopulateSelectorList(m_pPropSelectCombo, "Prop Selector", m_iPropModelIndex, m_PropList, datCallback(MFA(CCutSceneDebugManager::GetSelectedPropEntityCallBack), (datBase*)this)  );
		}

		if(m_pPTFXSelectCombo)
		{
			PopulateSelectorList(m_pPTFXSelectCombo, "Prop Selector", m_iPtfxIndex, m_PtfxList, datCallback(MFA(CCutSceneDebugManager::GetSelectedPtfxEntityCallBack), (datBase*)this));
		}
		

		if(m_CameraObjectList.GetCount()>0 )
		{
			PopulateWaterRefractionIndexEventList(m_CameraObjectList[0]); 
			PopulateCameraCutEventList(m_CameraObjectList[0], m_pTimeCycleCameraCutCombo, m_iTimecycleEventsIndexId, datCallback(MFA(CCutSceneDebugManager::UpdateTimeCycleParamsCB), (datBase*)this) ); 
			PopulateCameraCutEventList(m_CameraObjectList[0], m_pCoCModifiersCameraCutCombo, m_iCoCModifiersCamerCutComboIndexId, datCallback(MFA(CCutSceneDebugManager::UpdateCoCModifierEventOverridesCB), (datBase*)this)); 
			m_CascadeBounds.PopulateSetShadowBoundsEventList(m_CameraObjectList[0]);
			UpdateFirstPersonEventList(); 
		}
	}
#endif
}

void CCutSceneDebugManager::DestroyWidgets()
{
	m_FirstPersonBlendOutSlider = NULL; 

	m_CascadeBounds.DestroyWidgets();

	m_pVarDebugFrameSlider = NULL; 
	m_pVarSkipFrameSlider = NULL; 
	m_pPedVarPlayBackSlider = NULL;
	m_pVehicleVarPlayBackSlider = NULL; 
	m_pPedSelectCombo = NULL;
	m_pCoCModifiersCameraCutCombo = NULL; 
							
	m_pPerCutCoCModifiersCombo = NULL; 
	m_pSaveStatusText = NULL; 
	m_AllCoCModifiersCombo = NULL; 
	m_firstPersonEventStatus = NULL; 
	m_firstPersonBlendOutComboSlider = NULL; 
	
	m_FirstPersonBlendOutSlider = NULL; 
	m_pTimeCycleCameraCutCombo = NULL; 
	
	m_pLoadInteriorIndexCombo = NULL; 
	m_CameraWidgetData = NULL; 


	m_pPTFXSelectCombo = NULL; 

			
	m_pVarPedFrameSlider = NULL; 
	m_pEventCombo = NULL; 
	m_pVarDrawablIdSlider = NULL; 
	m_pVarTexIdSlider = NULL; 
			
	m_pPropDrawablIdSlider = NULL; 
					
	m_pPropTexIdSlider = NULL; 
	m_pVarFrameSlider = NULL; 
	m_pVehicleSelectCombo = NULL; 			
	m_pLiverySlider = NULL; 
	m_pLivery2Slider = NULL; 
	m_pVehicleVariationEventCombo = NULL; 
	m_pVehicleExtraCombo = NULL; 
	m_pVehicleExtraEventCombo = NULL; 
	m_pVehicleDamageEventCombo = NULL; 
		
	m_pOverlayPlayBackSlider = NULL; 
	m_pOverlaySelectCombo = NULL; 			 
	m_pOverlayEventCombo = NULL; 
				
	m_pOverlayMethodsCombo = NULL; 
							
	m_pOverlayParamNames = NULL;	
	m_pOverlayParamTypes = NULL;	
			
	m_pPropSelectCombo = NULL; 
	m_pWaterIndexFrameSlider = NULL;
	m_pWaterRefractionIndexCombo = NULL;			 
	m_CutsceneStateWidgetData = NULL;

}


void CCutSceneDebugManager::BankCameraWidgetReceiveDataCallback( )
{
    u8* data = m_CameraWidgetData->GetData();
    u16 length = m_CameraWidgetData->GetLength();

	if(!Verifyf(data != NULL, "Camera widget bank received NULL data."))
		return;

	if(!Verifyf(length > 0, "Camera widget bank received data of zero length."))
		return;
	
	int offset = 0;

	m_bOverrideCamUsingMatrix = *((bool*) &data[offset]);
	offset += sizeof(bool);

	m_bOverrideCam = *((bool*) &data[offset]);
	offset += sizeof(bool);

	m_bShouldOverrideUseLightDof = *((bool*) &data[offset]);
	offset += sizeof(bool);

	m_bShouldOverrideUseSimpleDof = *((bool*) &data[offset]);
	offset += sizeof(bool);

	float* matrix = (float*) &data[offset];
	m_cameraMtx.Set(matrix[0], matrix[1], matrix[2], 
					matrix[3], matrix[4], matrix[5],
					matrix[6], matrix[7], matrix[8], 
					matrix[9], matrix[10], matrix[11]);
	offset += (sizeof(float) * 12);

	m_fFov = *((float*) &data[offset]);
	offset += sizeof(float);

	float* dofPlanes = (float*) &data[offset];
	m_vDofPlanes.Set(dofPlanes[0], dofPlanes[1], dofPlanes[2], dofPlanes[3]);
	offset += sizeof(float) * 4;

	m_fMotionBlurStrength = *((float*) &data[offset]);
	offset += sizeof(float);

	m_CoCRadius = (int)(*((float*) &data[offset]));
	offset += sizeof(float);

	TUNE_GROUP_BOOL(CUTSCENE, UseFocusPoint ,true);

	if(UseFocusPoint)
	{
		m_FocusPoint = *((float*) &data[offset]);
		m_FocusPoint /= 100.0f; //convert to meters from centimeters
		offset += sizeof(float);
	}
}


void CCutSceneDebugManager::BankCutsceneWidgetReceiveDataCallback( )
{
    u8* data = m_CutsceneStateWidgetData->GetData();
    u16 length = m_CutsceneStateWidgetData->GetLength();

	if(!Verifyf(data != NULL, "Camera widget bank received NULL data."))
		return;

	if(!Verifyf(length > 0, "Camera widget bank received data of zero length."))
		return;
	
	memset(data, 0, CUTSCENE_STATE_DATA_WIDGET_SIZE);

	int offset = 0;

	//Cutscene state.
	int stateInt = (int) CutSceneManager::GetInstance()->GetState();
	memcpy(&data[offset], &stateInt, sizeof(int));
	offset += sizeof(int);

	//Save the current frame.
	int iCurrentFrame = CutSceneManager::GetInstance()->m_iCurrentFrameWithFrameRanges;
	memcpy(&data[offset], &iCurrentFrame, sizeof(int));
	offset += sizeof(int);
	m_CutsceneStateWidgetData->RemoteUpdate();
}

void CCutSceneDebugManager::BankFadeScreenInCallBack()
{
	Color32 color(m_vFadeInColor); 
	//cutfScreenFadeEventArgs* pEventArgs = rage_new  cutfScreenFadeEventArgs( m_fFadeInDuration, color );

	//CutSceneManager::GetInstance()->DispatchEventToObjectType( CUTSCENE_FADE_GAME_ENITITY, CUTSCENE_FADE_IN_EVENT, pEventArgs);

	//this fixes a problem in the fade system where is does not deal with colours that have alphas less than 200
	s32 iFadeTime = (s32) (m_fFadeInDuration * 1000.0f);
	color.SetAlpha(255); 
	camInterface::FadeIn(iFadeTime, color);

}

void CCutSceneDebugManager::BankFadeScreenOutCallBack()
{
	Color32 color(m_vFadeOutColor); 
	//cutfScreenFadeEventArgs* pEventArgs = rage_new  cutfScreenFadeEventArgs( m_fFadeOutDuration, color );

	//CutSceneManager::GetInstance()->DispatchEventToObjectType( CUTSCENE_FADE_GAME_ENITITY, CUTSCENE_FADE_OUT_EVENT, pEventArgs);
	s32 iFadeTime = (s32) (m_fFadeOutDuration * 1000.0f);
	color.SetAlpha(255); 
	camInterface::FadeOut(iFadeTime, true, color);

}


void CCutSceneDebugManager::DisplaySceneSkeletons()
{
	CutSceneManager::GetInstance()->DispatchEventToObjectType( CUTSCENE_MODEL_OBJECT_TYPE, 
		(m_bDisplayAllSkeletons) ? CUTSCENE_SHOW_DEBUG_LINES_EVENT : CUTSCENE_HIDE_DEBUG_LINES_EVENT );
}



/////////////////////////////////////////////////////////////////////////////////////
//Set the vehicle settings to some default values
void CCutSceneDebugManager::ResetVehicleSettings()
{

	m_iMainBodyColour = 0; 
	m_iSecondBodyColour = 0; 
	m_iSpecularColour = 0;
	m_iWheelTrimColour = 0;
	m_iBodyColour5 = 0;
	m_iBodyColour6 = 0;
	m_fDirtLevel = 0.0f; 
	m_iLivery = -1; 
	m_iLivery2= -1; 

	for(int i=0; i<m_pBoneData.GetCount(); i++)
	{
		m_pBoneData[i]=NULL;
	}
	m_pBoneData.Reset(); 

	if(m_pVehicleExtraCombo)
	{	
		m_pVehicleExtraCombo->UpdateCombo( "Vehicle Extras", &m_iVehicleExtraIndex, m_pBoneData.GetCount()+1, NULL );
		m_pVehicleExtraCombo->SetString(0, "none");
	}
}

/////////////////////////////////////////////////////////////////////////////////////
//Get the current vehicle settings

void CCutSceneDebugManager::GetVehicleSettings()
{
	if((m_pLiverySlider == NULL) || (m_pLivery2Slider == NULL))
	{
		return; 
	}

	if(m_pVehicle)
	{
		if(m_pVehicle->GetGameEntity())
		{
			CVehicleModelInfo* pVehicleInfo = NULL; 
			pVehicleInfo = m_pVehicle->GetGameEntity()->GetVehicleModelInfo(); 
			if(pVehicleInfo)
			{
				m_pLiverySlider->SetRange(0, pVehicleInfo->GetLiveriesCount()); //set the range of the liveries widget 
				m_pLivery2Slider->SetRange(0, pVehicleInfo->GetLiveries2Count()); //set the range of the liveries widget 
			}

			m_iMainBodyColour = m_pVehicle->GetGameEntity()->GetBodyColour1(); 
			m_iSecondBodyColour = m_pVehicle->GetGameEntity()->GetBodyColour2(); 
			m_iSpecularColour = m_pVehicle->GetGameEntity()->GetBodyColour3(); 
			m_iWheelTrimColour = m_pVehicle->GetGameEntity()->GetBodyColour4(); 
			m_iBodyColour5 = m_pVehicle->GetGameEntity()->GetBodyColour5(); 
			m_iBodyColour6 = m_pVehicle->GetGameEntity()->GetBodyColour6(); 
			m_fDirtLevel = m_pVehicle->GetGameEntity()->GetBodyDirtLevel(); 
			m_iLivery = m_pVehicle->GetGameEntity()->GetLiveryId(); 
			m_iLivery2 = m_pVehicle->GetGameEntity()->GetLivery2Id(); 
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////

void CCutSceneDebugManager::UpdateVehicleBody1ColourCallBack()
{
	if(m_pVehicle)	
	{
		if(m_pVehicle->GetGameEntity())
		{
			m_pVehicle->GetGameEntity()->SetBodyColour1(m_iMainBodyColour); 
			m_pVehicle->GetGameEntity()->UpdateBodyColourRemapping(); 
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////

void CCutSceneDebugManager::UpdateVehicleBody2ColourCallBack()
{
	if(m_pVehicle)	
	{
		if(m_pVehicle->GetGameEntity())
		{
			m_pVehicle->GetGameEntity()->SetBodyColour2(m_iSecondBodyColour); 
			m_pVehicle->GetGameEntity()->UpdateBodyColourRemapping(); 
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////

void CCutSceneDebugManager::UpdateVehicleBody3ColourCallBack()
{
	if(m_pVehicle)	
	{
		if(m_pVehicle->GetGameEntity())
		{
			m_pVehicle->GetGameEntity()->SetBodyColour3(m_iSpecularColour); 
			m_pVehicle->GetGameEntity()->UpdateBodyColourRemapping(); 
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////

void CCutSceneDebugManager::UpdateVehicleBody4ColourCallBack()
{
	if(m_pVehicle)	
	{
		if(m_pVehicle->GetGameEntity())
		{
			m_pVehicle->GetGameEntity()->SetBodyColour4(m_iWheelTrimColour); 
			m_pVehicle->GetGameEntity()->UpdateBodyColourRemapping(); 
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////

void CCutSceneDebugManager::UpdateVehicleBody5ColourCallBack()
{
	if(m_pVehicle)	
	{
		if(m_pVehicle->GetGameEntity())
		{
			m_pVehicle->GetGameEntity()->SetBodyColour5(m_iBodyColour5); 
			m_pVehicle->GetGameEntity()->UpdateBodyColourRemapping(); 
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////

void CCutSceneDebugManager::UpdateVehicleBody6ColourCallBack()
{
	if(m_pVehicle)	
	{
		if(m_pVehicle->GetGameEntity())
		{
			m_pVehicle->GetGameEntity()->SetBodyColour6(m_iBodyColour6); 
			m_pVehicle->GetGameEntity()->UpdateBodyColourRemapping(); 
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////

void CCutSceneDebugManager::UpdateVehicleLiveriesCallBack()
{
	if(m_pVehicle)	
	{
		if(m_pVehicle->GetGameEntity())
		{
			m_pVehicle->GetGameEntity()->SetLiveryId(m_iLivery); 
			m_pVehicle->GetGameEntity()->SetLivery2Id(m_iLivery2); 
			m_pVehicle->GetGameEntity()->UpdateBodyColourRemapping(); 
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////

void CCutSceneDebugManager::UpdateVehicleDirtCallBack()
{
	if(m_pVehicle)	
	{
		if(m_pVehicle->GetGameEntity())
		{
			m_pVehicle->GetGameEntity()->SetBodyDirtLevel(m_fDirtLevel); 
			m_pVehicle->GetGameEntity()->UpdateBodyColourRemapping(); 
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////

void CCutSceneDebugManager::PopulateVehicleExtraList()
{
	if(m_pVehicleExtraCombo == NULL)
	{
		return; 
	}

	//reset the list of vehicle extras we have selected a new vehicle or there is no vehicle
	for(int i=0; i<m_pBoneData.GetCount(); i++)
	{
		m_pBoneData[i]=NULL;
	}
	m_pBoneData.Reset(); 

	m_pVehicleExtraCombo->UpdateCombo( "Vehicle Extras", &m_iVehicleExtraIndex, m_pBoneData.GetCount()+1, NULL );

	m_pVehicleExtraCombo->SetString(0, "none");

	if(m_pVehicle)	
	{	
		const crSkeletonData* pSkel = NULL; 

		if(m_pVehicle->GetGameEntity())
		{
			const crSkeletonData& Skel =  m_pVehicle->GetGameEntity()->GetSkeletonData();
			pSkel = &Skel; 
		}
		else
		{
			return;
		}

		if (pSkel)
		{
			for(int i = 0; i < MAX_EXTRA_VEHICLE_NAMES-1; i++)
			{
				const crBoneData* pBoneData = pSkel->FindBoneData(VehicleExtraNames[i]); 

				if(pBoneData)
				{
					m_pBoneData.Grow() = pBoneData; 
				}
			}

			m_pVehicleExtraCombo->UpdateCombo( "Vehicle Extras", &m_iVehicleExtraIndex, m_pBoneData.GetCount(), NULL );

			if (m_pBoneData.GetCount()>0)
			{
				for(int i=0; i<m_pBoneData.GetCount(); i++)
				{
					m_pVehicleExtraCombo->SetString(i,m_pBoneData[i]->GetName());
				}
			}
		}
		else
		{

		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CCutSceneDebugManager::HideVehicleExtraCallBack()
{
	if(m_pVehicle)	
	{
		if(m_pVehicle->GetGameEntity())
		{
			m_pVehicle->SetBoneToHide(m_pBoneData[m_iVehicleExtraIndex]->GetBoneId(),true); 

			m_pVehicle->SetGameVehicleExtra(m_pVehicle->GetHiddenBoneList()); 
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CCutSceneDebugManager::ShowVehicleExtraCallBack()
{
	if(m_pVehicle)	
	{
		if(m_pVehicle->GetGameEntity())
		{
			m_pVehicle->SetBoneToHide(m_pBoneData[m_iVehicleExtraIndex]->GetBoneId(),false); 

			m_pVehicle->SetGameVehicleExtra(m_pVehicle->GetHiddenBoneList()); 
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CCutSceneDebugManager::HighLightSelectedVehicle()
{
	if (m_iVehicleModelIndex > 0)
	{
		cutsEntity* pCutEntity = CutSceneManager::GetInstance()->GetEntityByObjectId(m_VehicleModelObjectList[m_iVehicleModelIndex - 1]->GetObjectId()); 

		CCutsceneAnimatedVehicleEntity* pVehicleEnt = static_cast<CCutsceneAnimatedVehicleEntity*> (pCutEntity);

		if(pVehicleEnt)
		{
			if(pVehicleEnt->GetGameEntity())
			{
				char VehicleInfo[32]; 	

				if(m_pVehicleSelectCombo)
				{
					formatf(VehicleInfo, sizeof(VehicleInfo)-1,"Edit: %s ", m_pVehicleSelectCombo->GetString(m_iVehicleModelIndex));
				}

				Color32 Colour (Color_red); 		
				const Vector3& VehicleMax =	m_pVehicle->GetGameEntity()->GetBoundingBoxMax(); 

				const Vector3 vVehiclePosition = VEC3V_TO_VECTOR3(pVehicleEnt->GetGameEntity()->GetTransform().GetPosition()); 
				Vector3 vPos = vVehiclePosition; 
				vPos.z += VehicleMax.z + 0.5f;

				grcDebugDraw::Line(vVehiclePosition,vPos,Colour); 
				grcDebugDraw::Text(vPos, Colour, VehicleInfo );
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CCutSceneDebugManager::DisplaySelectedVehicleInfo()
{
	if( m_bDisplayVehicleVariation)
	{
		if (m_iVehicleModelIndex > 0)
		{
			cutsEntity* pCutEntity = CutSceneManager::GetInstance()->GetEntityByObjectId(m_VehicleModelObjectList[m_iVehicleModelIndex - 1]->GetObjectId()); 

			CCutsceneAnimatedVehicleEntity* pVehicleEnt = static_cast<CCutsceneAnimatedVehicleEntity*> (pCutEntity);

			if (pVehicleEnt == NULL)
			{
				return; 
			}

			Color32 Colour (Color_red); 
			atArray<s32> RemovedComponenets; 

			if(pVehicleEnt->GetGameEntity())
			{
				RemovedComponenets = pVehicleEnt->GetHiddenBoneList(); 
			}
			else
			{
				return; 
			}

			grcDebugDraw::AddDebugOutput(Colour , m_VehicleModelObjectList[m_iVehicleModelIndex -1]->GetDisplayName().c_str());	
			grcDebugDraw::AddDebugOutput("Main Body Colour: %d", m_iMainBodyColour);
			grcDebugDraw::AddDebugOutput("Secondary Body Colour: %d", m_iSecondBodyColour);
			grcDebugDraw::AddDebugOutput("Specular Colour: %d", m_iSpecularColour);
			grcDebugDraw::AddDebugOutput("Wheel Trim Colour: %d", m_iWheelTrimColour);
			grcDebugDraw::AddDebugOutput("Body Colour 5: %d", m_iBodyColour5);
			grcDebugDraw::AddDebugOutput("Body Colour 6: %d", m_iBodyColour6);
			grcDebugDraw::AddDebugOutput("Dirt: %f", m_fDirtLevel);

			if(m_iLivery != -1)
			{
				grcDebugDraw::AddDebugOutput(Colour, "Livery: %d", m_iLivery);
			}
			else
			{
				grcDebugDraw::AddDebugOutput("Livery: %d", m_iLivery);
			}

			if(m_iLivery2 != -1)
			{
				grcDebugDraw::AddDebugOutput(Colour, "Livery2: %d", m_iLivery2);
			}
			else
			{
				grcDebugDraw::AddDebugOutput("Livery2: %d", m_iLivery2);
			}

			for(int i = 0; i < m_pBoneData.GetCount(); i++)
			{
				bool bComponentRemoved = false; 
				for(int j=0; j<RemovedComponenets.GetCount(); j++)
				{
					if(m_pBoneData[i]->GetBoneId() == RemovedComponenets[j])
					{
						grcDebugDraw::AddDebugOutput(Colour, "%s (%d)", m_pBoneData[i]->GetName(),m_pBoneData[i]->GetBoneId());
						bComponentRemoved = true; 
					}
				}

				if (bComponentRemoved == false)
				{
					grcDebugDraw::AddDebugOutput("%s (%d)", m_pBoneData[i]->GetName(),m_pBoneData[i]->GetBoneId());
				}

			}

		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////

void CCutSceneDebugManager::PopulateVehicleVariationEventList(const cutfObject* pObject)
{
	if (m_pVehicleVariationEventCombo == NULL)
	{
		return; 
	}


	//Nothing selected
	if(!pObject)
	{
		m_VehicleVarEventId = 0; 
		m_pVehicleVariationEventCombo->UpdateCombo( "Vehicle var events", &m_VehicleVarEventId, 1, NULL );
		m_pVehicleVariationEventCombo->SetString( 0 , "none");
		return; 
	}

	atArray<cutfEvent *> AllEventList;
	CutSceneManager::GetInstance()->GetCutfFile()->FindEventsForObjectIdOnly( pObject->GetObjectId(), CutSceneManager::GetInstance()->GetCutfFile()->GetEventList(), AllEventList );

	atArray<cutfEvent *>EventList; 

	for(int i =0; i < AllEventList.GetCount(); i++)
	{
		cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( AllEventList[i]->GetEventArgs() );
		if(pEventArgs->GetType() == CUTSCENE_VEHICLE_VARIATION_EVENT_ARGS_TYPE)
		{
			EventList.PushAndGrow(AllEventList[i]); 
		}
	}

	//look to edit a current event 
	if (EventList.GetCount() == 0)
	{
		m_pVehicleVariationEventCombo->UpdateCombo( "Vehicle var events", &m_VehicleVarEventId, 1, NULL );
		m_pVehicleVariationEventCombo->SetString( 0 , "none");
	}
	else
	{
		if (EventList.GetCount() > 0)
		{
			m_pVehicleVariationEventCombo->UpdateCombo( "Ped var events", &m_VehicleVarEventId, EventList.GetCount(), NULL );

			//Clear the list of variations, need to repopulate. 
			for (int i=0; i < m_VehicleCutSceneVariations.GetCount(); i++)
			{
				m_VehicleCutSceneVariations[i] = NULL;  
			}
			m_VehicleCutSceneVariations.Reset(); 

			for ( int i = 0; i < EventList.GetCount(); ++i )
			{
				if (EventList[i]->GetEventId() == CUTSCENE_SET_VARIATION_EVENT)
				{
					cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( EventList[i]->GetEventArgs() );

					if(pEventArgs && pEventArgs->GetType() == CUTSCENE_VEHICLE_VARIATION_EVENT_ARGS_TYPE)
					{						
						char EventInfo[64]; 	

						cutfVehicleVariationEventArgs* pObjectVar = static_cast<cutfVehicleVariationEventArgs*>(pEventArgs); 

						s32 FrameNumber = static_cast<s32>(EventList[i]->GetTime() * CUTSCENE_FPS); 

						formatf(EventInfo, sizeof(EventInfo)-1,"Event %d: Frame: %d , time: %f , BodyCol: %d, 2ndCol: %d, SpecCol: %d, WheelCol: %d, BodyCol5: %d, BodyCol6: %d, LiveryId: %d, Dirt: %f ", i , FrameNumber, EventList[i]->GetTime(),  
							pObjectVar->GetBodyColour(), pObjectVar->GetSecondaryBodyColour(),  pObjectVar->GetSpecularBodyColour(), pObjectVar->GetWheelTrimColour(), pObjectVar->GetBodyColour5(), pObjectVar->GetBodyColour6(), pObjectVar->GetLiveryId(), pObjectVar->GetDirtLevel());

						m_pVehicleVariationEventCombo->SetString( i, EventInfo );

						//Store a list of pointers to our objects
						m_VehicleCutSceneVariations.PushAndGrow(EventList[i]); 
					}
				}
			}
		}
	}
};

/////////////////////////////////////////////////////////////////////////////////////

void CCutSceneDebugManager::SaveVehicleVariationEvent(const cutfObject* pObject)
{
	if(!Verifyf(CutSceneManager::GetInstance()->GetCutfFile(), "Save Ped Variation: No cutf file object loaded cannot save"))
	{
		return;
	}

	if ( pObject == NULL )
	{
		return;
	}

	RAGE_TRACK( CutSceneManager_SaveVehicleVariation );

	atArray<cutfEvent *> EventList;
	CutSceneManager::GetInstance()->GetCutfFile()->FindEventsForObjectIdOnly( pObject->GetObjectId(), CutSceneManager::GetInstance()->GetCutfFile()->GetEventList(), EventList );

	bool bAddNewEvent = true; 

	//look to edit a current event 
	for ( int i = 0; i < EventList.GetCount(); ++i )
	{
		if (EventList[i]->GetEventId() == CUTSCENE_SET_VARIATION_EVENT)
		{
			if(EventList[i]->GetTime() == CutSceneManager::GetInstance()->GetCutSceneTimeWithRangeOffset() )
			{
				cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( EventList[i]->GetEventArgs() );

				if(pEventArgs && pEventArgs->GetType() == CUTSCENE_VEHICLE_VARIATION_EVENT_ARGS_TYPE)
				{
					//Don't add new event for a component that has the same id and time. 			
					cutfVehicleVariationEventArgs* pObjectVar = static_cast<cutfVehicleVariationEventArgs*>(pEventArgs); 

					pObjectVar->SetBodyColour(m_iMainBodyColour); 
					pObjectVar->SetSecondaryBodyColour(m_iSecondBodyColour); 
					pObjectVar->SetSpecularColour(m_iSpecularColour);
					pObjectVar->SetWheelTrimColour(m_iWheelTrimColour);
					pObjectVar->SetBodyColour5(m_iBodyColour5);
					pObjectVar->SetBodyColour6(m_iBodyColour6);
					pObjectVar->SetLiveryId(m_iLivery); 
					pObjectVar->SetLivery2Id(m_iLivery2); 
					pObjectVar->SetDirtLevel(m_fDirtLevel); 
					bAddNewEvent = false; 	

				}
			}
		}
	}

	if(bAddNewEvent)
	{
		cutfEventArgs *pEventArgs = rage_new cutfVehicleVariationEventArgs( pObject->GetObjectId(), m_iMainBodyColour, m_iSecondBodyColour, m_iSpecularColour, m_iWheelTrimColour, m_iBodyColour5, m_iBodyColour6, m_iLivery, m_iLivery2, m_fDirtLevel );

#if	CUTSCENE_EXPORT_EVENTS_WITH_ZERO_BASE_TIME
		float ftime = CutSceneManager::GetInstance()->GetCutSceneTimeWithRangeOffset();  
#else
		float ftime =CutSceneManager::GetInstance()-> GetCutSceneCurrentTime(); 
#endif


		CutSceneManager::GetInstance()->GetCutfFile()->AddEvent( rage_new cutfObjectIdEvent( pObject->GetObjectId(), ftime, CUTSCENE_SET_VARIATION_EVENT, pEventArgs ) );
		CutSceneManager::GetInstance()->GetCutfFile()->SortEvents();
	}
}

/////////////////////////////////////////////////////////////////////////////////////

void CCutSceneDebugManager::SaveVehicleExtraEvent(const cutfObject* pObject)
{
	if(!Verifyf(CutSceneManager::GetInstance()->GetCutfFile(), "Save Ped Variation: No cutf file object loaded cannot save"))
	{
		return;
	}

	if ( pObject == NULL )
	{
		return;
	}

	RAGE_TRACK( CutSceneManager_SaveVehicleVariation );

	atArray<cutfEvent *> EventList;
	CutSceneManager::GetInstance()->GetCutfFile()->FindEventsForObjectIdOnly( pObject->GetObjectId(), CutSceneManager::GetInstance()->GetCutfFile()->GetEventList(), EventList );

	bool bAddNewEvent = true; 

	atArray<s32> BoneToHideList; 

	//populate the current bone list
	if(m_pVehicle->GetGameEntity())
	{
		BoneToHideList = m_pVehicle->GetHiddenBoneList(); 
	}

	//look to edit a current event 
	for ( int i = 0; i < EventList.GetCount(); ++i )
	{
		if (EventList[i]->GetEventId() == CUTSCENE_SET_VARIATION_EVENT)
		{
			if(EventList[i]->GetTime() == CutSceneManager::GetInstance()->GetCutSceneTimeWithRangeOffset() )
			{
				cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( EventList[i]->GetEventArgs() );

				if(pEventArgs && pEventArgs->GetType() == CUTSCENE_VEHICLE_EXTRA_EVENT_ARGS_TYPE)
				{
					//Overwrite the existing event with the new info			
					cutfVehicleExtraEventArgs* pObjectVar = static_cast<cutfVehicleExtraEventArgs*>(pEventArgs); 

					//if there is no extra to be removed create a single entry of zero
					if(BoneToHideList.GetCount() == 0)	
					{
						BoneToHideList.PushAndGrow(0);
					}
					pObjectVar->SetBoneIdList(BoneToHideList); 

					bAddNewEvent = false; 	
				}
			}
		}
	}

	if(bAddNewEvent)
	{
		//if there is no extra to be removed create a single entry of zero
		if(BoneToHideList.GetCount() == 0)	
		{
			BoneToHideList.PushAndGrow(0);
		}

		cutfEventArgs *pEventArgs = rage_new cutfVehicleExtraEventArgs( pObject->GetObjectId(), BoneToHideList);

#if	CUTSCENE_EXPORT_EVENTS_WITH_ZERO_BASE_TIME
		float ftime = CutSceneManager::GetInstance()->GetCutSceneTimeWithRangeOffset();  
#else
		float ftime = CutSceneManager::GetInstance()->GetCutSceneCurrentTime(); 
#endif

		CutSceneManager::GetInstance()->GetCutfFile()->AddEvent( rage_new cutfObjectIdEvent( pObject->GetObjectId(), ftime, CUTSCENE_SET_VARIATION_EVENT, pEventArgs ) );
		CutSceneManager::GetInstance()->GetCutfFile()->SortEvents();
	}
}

/////////////////////////////////////////////////////////////////////////////////////

void CCutSceneDebugManager::BankSaveVehicleVariationEventCallBack()
{
	if (m_VehicleModelObjectList.GetCount() > 0 && m_iVehicleModelIndex <= m_VehicleModelObjectList.GetCount() && m_iVehicleModelIndex > 0  )
	{
		const cutfObject* pObject = m_VehicleModelObjectList[m_iVehicleModelIndex - 1]; 

		if ( (CutSceneManager::GetInstance()->IsPaused() || CutSceneManager::GetInstance()->IsPlaying()) && pObject)
		{
			//Save the ped variation into the event list
			SaveVehicleVariationEvent(pObject); 

			//update the combo box of ped variation events
			PopulateVehicleVariationEventList(pObject); 

			//Save the changes out to the cuttune file.
			CutSceneManager::GetInstance()->SaveCutfile(); 
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////

void CCutSceneDebugManager::BankSaveVehicleExtraCallBack()
{
	if (m_VehicleModelObjectList.GetCount() > 0 && m_iVehicleModelIndex <= m_VehicleModelObjectList.GetCount() && m_iVehicleModelIndex > 0  )
	{
		const cutfObject* pObject = m_VehicleModelObjectList[m_iVehicleModelIndex - 1]; 

		if ( (CutSceneManager::GetInstance()->IsPaused() || CutSceneManager::GetInstance()->IsPlaying()) && pObject)
		{
			//Save the ped variation into the event list
			SaveVehicleExtraEvent(pObject); 

			//update the combo box of ped variation events
			PopulateVehicleExtraEventList(pObject); 

			//Save the changes out to the cuttune file.
			CutSceneManager::GetInstance()->SaveCutfile(); 
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
//Populate the list of current vehicle extras

void CCutSceneDebugManager::PopulateVehicleExtraEventList(const cutfObject* pObject)
{
	if (m_pVehicleExtraEventCombo == NULL)
	{
		return; 
	}

	//Nothing selected
	if(!pObject)
	{
		m_pVehicleExtraEventCombo->UpdateCombo( "Vehicle Extra events", &m_VehicleExtraEventId, 1, NULL );
		m_pVehicleExtraEventCombo->SetString( 0 , "none");
		return; 
	}

	//Repopulate the list only with events that have extra argument types
	atArray<cutfEvent *> AllEventList;
	CutSceneManager::GetInstance()->GetCutfFile()->FindEventsForObjectIdOnly( pObject->GetObjectId(), CutSceneManager::GetInstance()->GetCutfFile()->GetEventList(), AllEventList );

	atArray<cutfEvent *>EventList; 

	for(int i =0; i < AllEventList.GetCount(); i++)
	{
		cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( AllEventList[i]->GetEventArgs() );
		if(pEventArgs->GetType() == CUTSCENE_VEHICLE_EXTRA_EVENT_ARGS_TYPE)
		{
			EventList.PushAndGrow(AllEventList[i]); 
		}
	}

	//there are no events of the extra type
	if (EventList.GetCount() == 0)
	{
		m_pVehicleExtraEventCombo->UpdateCombo( "Vehicle Extra events", &m_VehicleExtraEventId, 1, NULL );
		m_pVehicleExtraEventCombo->SetString( 0 , "none");
	}
	else
	{
		if (EventList.GetCount() > 0)
		{
			m_pVehicleExtraEventCombo->UpdateCombo( "Vehicle Extra events", &m_VehicleExtraEventId, EventList.GetCount(), NULL );

			//Clear the list of variations, need to repopulate. 
			for (int i=0; i < m_VehicleExtras.GetCount(); i++)
			{
				m_VehicleExtras[i] = NULL;  
			}
			m_VehicleExtras.Reset(); 

			for ( int i = 0; i < EventList.GetCount(); ++i )
			{
				if (EventList[i]->GetEventId() == CUTSCENE_SET_VARIATION_EVENT)
				{
					cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( EventList[i]->GetEventArgs() );

					if(pEventArgs && pEventArgs->GetType() == CUTSCENE_VEHICLE_EXTRA_EVENT_ARGS_TYPE)
					{						
						char EventInfo[128]; 	
						char BoneName[16]; 

						cutfVehicleExtraEventArgs* pObjectVar = static_cast<cutfVehicleExtraEventArgs*>(pEventArgs); 

						const atArray<s32> &ibones = pObjectVar->GetBoneIdList(); 

						s32 FrameNumber =  static_cast<s32>(EventList[i]->GetTime() * CUTSCENE_FPS); 

						formatf(EventInfo, sizeof(EventInfo)-1, "Event: %d , Frame: %d , Extras Hidden: ", i, FrameNumber); 

						for(int j =0; j < ibones.GetCount(); j++)
						{
							//Special case where we are reactivating vehicle extras
							if(ibones[j] == 0)
							{
								formatf(EventInfo, sizeof(EventInfo)-1, "Event: %d , Frame: %d , No Extras Hidden", i, FrameNumber); 
							}
							else
							{
								formatf(BoneName, sizeof(BoneName)-1, " %s, " , GetExtraBoneNumberFromName(ibones[j]) ); 

								strcat(EventInfo, BoneName); 
							}
						}

						m_pVehicleExtraEventCombo->SetString( i, EventInfo );

						//Store a list of pointers to our objects
						m_VehicleExtras.PushAndGrow(EventList[i]); 
					}
				}
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//grab  the number at the end of the extra name to be able to display the extra just as a number

const char* CCutSceneDebugManager::GetExtraBoneNumberFromName(s32 iBone)
{
	//Get a pointer to the dynamic only interested in the skeleton
	CDynamicEntity* pEntity = NULL; 

	if(m_pVehicle->GetGameEntity())
	{
		pEntity = static_cast<CDynamicEntity*>(m_pVehicle->GetGameEntity());
	}
	
	if(pEntity)
	{
		const crSkeletonData& Skel = pEntity->GetSkeletonData(); 

		int iBoneIndex; 

		if (Skel.ConvertBoneIdToIndex(static_cast<u16>(iBone), iBoneIndex))
		{
			const crBoneData* pBone = Skel.GetBoneData(iBoneIndex); 

			const char* pName = pBone->GetName(); 

			const char* number = strrchr(pName, '_'); 

			return number+1; 
		}
	}

	return ""; 
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CCutSceneDebugManager::BankDeleteVehicleVariationCallBack()
{
	if (m_VehicleCutSceneVariations.GetCount() > 0)
	{
		if(m_VehicleCutSceneVariations[m_VehicleVarEventId])
		{
			CutSceneManager::GetInstance()->GetCutfFile()->RemoveEvent(m_VehicleCutSceneVariations[m_VehicleVarEventId]);
			PopulateVehicleVariationEventList(m_VehicleModelObjectList[m_iVehicleModelIndex - 1]); 
			CutSceneManager::GetInstance()->SaveCutfile(); 
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CCutSceneDebugManager::BankDeleteVehicleExtrasCallBack()
{
	if (m_VehicleExtras.GetCount() > 0)
	{
		if(m_VehicleExtras[m_VehicleExtraEventId])
		{
			CutSceneManager::GetInstance()->GetCutfFile()->RemoveEvent(m_VehicleExtras[m_VehicleExtraEventId]);
			PopulateVehicleExtraEventList(m_VehicleModelObjectList[m_iVehicleModelIndex - 1]); 
			CutSceneManager::GetInstance()->SaveCutfile(); 
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CCutSceneDebugManager::BankApplyVehicleDamageEventCallBack()
{ 
	if (m_pVehicle)
	{
		Vector3 vLocal = m_vVehicleDamagerPos - VEC3V_TO_VECTOR3(m_pVehicle->GetGameEntity()->GetTransform().GetPosition()); 
		
		Matrix34 vehiclemat = MAT34V_TO_MATRIX34(m_pVehicle->GetGameEntity()->GetMatrix()); 
		vehiclemat.UnTransform3x3(vLocal); //convert into vehicle space
		
		m_vLocalDamage = vLocal; 

		m_pVehicle->SetVehicleDamage(vLocal, m_fDamageValue, m_fDerformation, true ); 
	}
}

void CCutSceneDebugManager::BankSaveVehicleDamageEvent()
{
	if (m_VehicleModelObjectList.GetCount() > 0 && m_iVehicleModelIndex <= m_VehicleModelObjectList.GetCount() && m_iVehicleModelIndex > 0  )
	{
		const cutfObject* pObject = m_VehicleModelObjectList[m_iVehicleModelIndex - 1]; 

		if ( (CutSceneManager::GetInstance()->IsPaused() || CutSceneManager::GetInstance()->IsPlaying()) && pObject)
		{
			//Save the ped variation into the event list
			parAttributeList DamageEventsAttributes; 
			DamageEventsAttributes.AddAttribute("vPos.x", m_vLocalDamage.x); 
			DamageEventsAttributes.AddAttribute("vPos.y", m_vLocalDamage.y); 
			DamageEventsAttributes.AddAttribute("vPos.z", m_vLocalDamage.z); 
			DamageEventsAttributes.AddAttribute("fDamage", m_fDamageValue); 
			DamageEventsAttributes.AddAttribute("fDeform", m_fDerformation); 

			SaveVehicleDamageEvent(pObject, DamageEventsAttributes); 

			//update the combo box of ped variation events
			PopulateVehicleDamageEventList(pObject); 

			//Save the changes out to the cuttune file.
			CutSceneManager::GetInstance()->SaveCutfile(); 
		}
	}
}

void CCutSceneDebugManager::SaveVehicleDamageEvent(const cutfObject* pObject, parAttributeList &Attributes)
{
	if(!Verifyf(CutSceneManager::GetInstance()->GetCutfFile(), "Save Ped Variation: No cutf file object loaded cannot save"))
	{
		return;
	}

	if ( pObject == NULL )
	{
		return;
	}

	RAGE_TRACK( CutSceneManager_SaveVehicleDamageEventn );

	atArray<cutfEvent *> EventList;
	CutSceneManager::GetInstance()->GetCutfFile()->FindEventsForObjectIdOnly( pObject->GetObjectId(), CutSceneManager::GetInstance()->GetCutfFile()->GetEventList(), EventList );

	bool bAddNewEvent = true; 

	for ( int i = 0; i < EventList.GetCount(); ++i )
	{
		if (EventList[i]->GetEventId() == CUTSCENE_SET_VARIATION_EVENT)
		{
			if(EventList[i]->GetTime() == CutSceneManager::GetInstance()->GetCutSceneTimeWithRangeOffset() )
			{
				cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( EventList[i]->GetEventArgs() );

				if(pEventArgs && pEventArgs->GetType() == CUTSCENE_NAME_EVENT_ARGS_TYPE)
				{
					
				}
			}
		}
	}

	if(bAddNewEvent)
	{
		cutfEventArgs *pEventArgs = rage_new cutfNameEventArgs("Damage Event", Attributes);

#if	CUTSCENE_EXPORT_EVENTS_WITH_ZERO_BASE_TIME
		float ftime = CutSceneManager::GetInstance()->GetCutSceneTimeWithRangeOffset();  
#else
		float ftime =CutSceneManager::GetInstance()-> GetCutSceneCurrentTime(); 
#endif


		CutSceneManager::GetInstance()->GetCutfFile()->AddEvent( rage_new cutfObjectIdEvent( pObject->GetObjectId(), ftime, CutSceneCustomEvents::CUTSCENE_SET_VEHICLE_DAMAGE_EVENT, pEventArgs ) );
		CutSceneManager::GetInstance()->GetCutfFile()->SortEvents();
	}
}


void  CCutSceneDebugManager::BankDeleteVehicleDamageEvent()
{
	if (m_VehicleDamageEvents.GetCount() > 0)
	{
		if(m_VehicleDamageEvents[m_iVehicleDamageIndex])
		{
			CutSceneManager::GetInstance()->GetCutfFile()->RemoveEvent(m_VehicleDamageEvents[m_iVehicleDamageIndex]);
			PopulateVehicleDamageEventList(m_VehicleModelObjectList[m_iVehicleModelIndex - 1]); 
			CutSceneManager::GetInstance()->SaveCutfile(); 
		}
	}
}

void CCutSceneDebugManager::PopulateVehicleDamageEventList(const cutfObject* pObject)
{
	if (m_pVehicleDamageEventCombo == NULL)
	{
		return; 
	}

	//Nothing selected
	if(!pObject)
	{
		m_pVehicleDamageEventCombo->UpdateCombo( "Vehicle Damage events", &m_iVehicleDamageIndex, 1, NULL );
		m_pVehicleDamageEventCombo->SetString( 0 , "none");
		return; 
	}

	//Repopulate the list only with events that have extra argument types
	atArray<cutfEvent *> AllEventList;
	CutSceneManager::GetInstance()->GetCutfFile()->FindEventsForObjectIdOnly( pObject->GetObjectId(), CutSceneManager::GetInstance()->GetCutfFile()->GetEventList(), AllEventList );

	atArray<cutfEvent *>EventList; 

	for(int i =0; i < AllEventList.GetCount(); i++)
	{
		cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( AllEventList[i]->GetEventArgs() );
		if(pEventArgs->GetType() == CUTSCENE_NAME_EVENT_ARGS_TYPE && AllEventList[i]->GetEventId() == CutSceneCustomEvents::CUTSCENE_SET_VEHICLE_DAMAGE_EVENT)
		{
			EventList.PushAndGrow(AllEventList[i]); 
		}
	}

	//there are no events of the extra type
	if (EventList.GetCount() == 0)
	{
		m_pVehicleDamageEventCombo->UpdateCombo( "Vehicle Damage events", &m_iVehicleDamageIndex, 1, NULL );
		m_pVehicleDamageEventCombo->SetString( 0 , "none");
	}
	else
	{
		if (EventList.GetCount() > 0)
		{
			m_pVehicleDamageEventCombo->UpdateCombo( "Vehicle Damage events", &m_iVehicleDamageIndex, EventList.GetCount(), NULL );

			//Clear the list of variations, need to repopulate. 
			for (int i=0; i < m_VehicleDamageEvents.GetCount(); i++)
			{
				m_VehicleDamageEvents[i] = NULL;  
			}
			m_VehicleDamageEvents.Reset(); 

			for ( int i = 0; i < EventList.GetCount(); ++i )
			{
				if (EventList[i]->GetEventId() == CutSceneCustomEvents::CUTSCENE_SET_VEHICLE_DAMAGE_EVENT)
				{
					cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( EventList[i]->GetEventArgs() );

					if(pEventArgs->GetType() == CUTSCENE_NAME_EVENT_ARGS_TYPE)
					{						
						char EventInfo[128]; 	

						cutfNameEventArgs* pObjectVar = static_cast<cutfNameEventArgs*>(pEventArgs); 
					
						s32 FrameNumber =  static_cast<s32>(EventList[i]->GetTime() * CUTSCENE_FPS); 

						Vector3 pos(VEC3_ZERO); 
						float Damage = 0.0f; 
						float Deform = 0.0f;
						if(pObjectVar->GetAttributeList().FindAttributeAnyCase("vPos.x"))
						{
							pos.x =  pObjectVar->GetAttributeList().FindAttributeAnyCase("vPos.x")->FindFloatValue(); 
						}
						
						if(pObjectVar->GetAttributeList().FindAttributeAnyCase("vPos.y"))
						{
							pos.y = pObjectVar->GetAttributeList().FindAttributeAnyCase("vPos.y")->FindFloatValue();
						}
						
						if(pObjectVar->GetAttributeList().FindAttributeAnyCase("vPos.z"))
						{
							pos.z = pObjectVar->GetAttributeList().FindAttributeAnyCase("vPos.z")->FindFloatValue(); 
						}
						
						if(pObjectVar->GetAttributeList().FindAttributeAnyCase("fDamage"))
						{
							Damage = pObjectVar->GetAttributeList().FindAttributeAnyCase("fDamage")->FindFloatValue(); 
						}
						
						if (pObjectVar->GetAttributeList().FindAttributeAnyCase("fDeform"))
						{
							Deform = pObjectVar->GetAttributeList().FindAttributeAnyCase("fDeform")->FindFloatValue(); 
						}

						formatf(EventInfo, sizeof(EventInfo)-1, "(%d) %s( <<%f,%f,%f,>>,%f,,%f)",FrameNumber, 
							pObjectVar->GetName().GetCStr(),pos.x,pos.y,pos.z, Damage, Deform); 

						m_pVehicleDamageEventCombo->SetString( i, EventInfo );

						//Store a list of pointers to our objects
						m_VehicleDamageEvents.PushAndGrow(EventList[i]); 
					}
				}
			}
		}
	}
}
///////////////////////////////////////////////////////////////////////////////////////////////////

void CCutSceneDebugManager::GetSelectedPedEntityCallBack()
{
	if (m_iPedModelIndex > 0)
	{
		cutsEntity* pCutEntity = CutSceneManager::GetInstance()->GetEntityByObjectId(CutSceneManager::GetInstance()->m_pedModelObjectList[m_iPedModelIndex - 1]->GetObjectId()); 

		CCutsceneAnimatedActorEntity* pActorEnt = static_cast<CCutsceneAnimatedActorEntity*> (pCutEntity); 

		if(pActorEnt)
		{
			m_pActorEntity = pActorEnt; 
			UpdateCutSceneVarComponentCB(); 
			UpdateCutScenePropComponentCB();
			PopulatePedVariationEventList(CutSceneManager::GetInstance()->m_pedModelObjectList[m_iPedModelIndex - 1]); 
		}
	}
	else
	{
		PopulatePedVariationEventList(NULL);
		m_pActorEntity = NULL; 
	}
}


void CCutSceneDebugManager::GetSelectedPropEntityCallBack()
{
	if (m_iPropModelIndex > 0)
	{
		cutsEntity* pCutEntity = CutSceneManager::GetInstance()->GetEntityByObjectId(m_PropList[m_iPropModelIndex - 1]->GetObjectId()); 

		CCutSceneAnimatedPropEntity* pPropEnt = static_cast<CCutSceneAnimatedPropEntity*> (pCutEntity); 

		if(pPropEnt)
		{
			m_pPropEntity = pPropEnt; 
	
		}
	}
	else
	{
		m_pPropEntity = NULL; 
	}
}


void CCutSceneDebugManager::GetSelectedPtfxEntityCallBack()
{
	if (m_iPtfxIndex > 0)
	{
		cutsEntity* pCutEntity = CutSceneManager::GetInstance()->GetEntityByObjectId(m_PtfxList[m_iPtfxIndex - 1]->GetObjectId()); 

		CCutSceneParticleEffectsEntity* pPtfxEnt = static_cast<CCutSceneParticleEffectsEntity*> (pCutEntity); 

		if(pPtfxEnt)
		{
			m_PtfxEnt = pPtfxEnt; 

			CCutSceneParticleEffect* pEffect = pPtfxEnt->GetParticleEffect(); 

			if(pEffect)
			{
				m_Ptfx = pEffect; 

				m_vPTFXPos = m_Ptfx->GetEffectPosition();
				m_Ptfx->SetEffectOverridePosition(m_vPTFXPos);
				
				m_vPTFXRot = m_Ptfx->GetEffectRotation() * RtoD;
				m_Ptfx->SetEffectOverrideRotation(m_vPTFXRot* DtoR); 

				m_OverridePtfx = m_Ptfx->CanOverrideEffect();  
			}
			else
			{
				m_OverridePtfx = false; 
			}
		}
	}
	else
	{
		m_PtfxEnt = NULL; 
		m_Ptfx = NULL; 
	}


}

void CCutSceneDebugManager::SetCanOverridePtfx()
{
	if(m_Ptfx)
	{
		m_Ptfx->SetCanOverrideEffect(m_OverridePtfx); 
	}
	else
	{
		m_OverridePtfx = false; 
	}
}


void CCutSceneDebugManager::UpdatePtfxPos()
{
	if(m_Ptfx && m_Ptfx->CanOverrideEffect())
	{
		m_Ptfx->SetEffectOverridePosition(m_vPTFXPos); 
	}
}

void CCutSceneDebugManager::UpdatePtfxRot()
{
	if(m_Ptfx && m_Ptfx->CanOverrideEffect())
	{
		m_Ptfx->SetEffectOverrideRotation(m_vPTFXRot * DtoR); 
	}
}

/////////////////////////////////////////////////////////////////////////////////////
//Update the peds current variation 

void CCutSceneDebugManager::UpdateCutSceneCompVarInfoIdxCB()
{
	UpdateCutSceneVarComponentCB();
}

void CCutSceneDebugManager::UpdateCutSceneVarComponentCB()
{	
	if (m_pActorEntity)
	{
		CPedModelInfo* pedModelInfo = NULL;
		if (m_pActorEntity->GetGameEntity())
		{
			m_varDrawableId = CPedVariationPack::GetCompVar(m_pActorEntity->GetGameEntity(), static_cast<ePedVarComp>(m_varComponentId));
			pedModelInfo =  m_pActorEntity->GetGameEntity()->GetPedModelInfo();
		}
		else
		{
			m_varDrawableId = 0; 
		}

		if (pedModelInfo)
		{
			m_maxDrawables = pedModelInfo->GetVarInfo()->GetMaxNumDrawbls(static_cast<ePedVarComp>(m_varComponentId))-1;

			UpdateCutSceneVarDrawblCB();
		}
	}
}

void CCutSceneDebugManager::UpdateCutScenePropVarInfoIdxCB()
{
	UpdateCutScenePropComponentCB();
}

void CCutSceneDebugManager::UpdateCutScenePropComponentCB()
{
	if (m_pActorEntity && m_pActorEntity->GetGameEntity())
	{
		ASSERT_ONLY(CPedModelInfo* pModelInfo = static_cast<CPedModelInfo*>(m_pActorEntity->GetGameEntity()->GetBaseModelInfo());)
		Assert(pModelInfo);

		if (m_pActorEntity->GetGameEntity())
		{
			m_PropDrawableId = CPedPropsMgr::GetPedPropIdx(m_pActorEntity->GetGameEntity(), (eAnchorPoints)m_PropSlotId);
		}

		m_maxPropDrawableId = CPedPropsMgr::GetMaxNumProps(m_pActorEntity->GetGameEntity(), (eAnchorPoints)m_PropSlotId)-1;
		
		if (m_pPropDrawablIdSlider != NULL)
		{
			m_pPropDrawablIdSlider->SetRange(-1.0f, (float)(m_maxPropDrawableId));
		}
	}
}

void CCutSceneDebugManager::UpdateCutScenePropDrawblCB()
{
	if (m_pActorEntity && m_pActorEntity->GetGameEntity())
	{
		if (m_PropDrawableId > m_maxPropDrawableId) {
			m_PropDrawableId = m_maxPropDrawableId;
		}

		m_PropTextureId = 0;			// reset texture to default when changing prop
		UpdateCutScenePropTexCB();

		//update the name of the prop
		if (m_PropDrawableId == PED_PROP_NONE) {
			//sprintf(propName, "No available prop");
			CPedPropsMgr::ClearPedProp(m_pActorEntity->GetGameEntity(), (eAnchorPoints)m_PropSlotId);
		} else {
			//sprintf(propName,"%s_%03d",propSlotNames[m_varPropSlot],m_maxPropDrawableId);
		}

		m_maxPropTextureId = CPedPropsMgr::GetMaxNumPropsTex(m_pActorEntity->GetGameEntity(), (eAnchorPoints)m_PropSlotId, m_PropTextureId)-1;
		if (m_pPropTexIdSlider != NULL)
		{
			m_pPropTexIdSlider->SetRange(0.0f, (float)(m_maxPropTextureId));
		}
	}
}

void CCutSceneDebugManager::UpdateCutScenePropTexCB()
{
	if (m_pActorEntity && m_pActorEntity->GetGameEntity())
	{
		if (m_pActorEntity->GetGameEntity() && m_PropTextureId>=0 && m_PropDrawableId>=0)
		{
			// set the prop and texture to the current settings
			CPedPropsMgr::SetPedProp(m_pActorEntity->GetGameEntity(), (eAnchorPoints)m_PropSlotId, m_PropDrawableId, m_PropTextureId, ANCHOR_NONE, NULL, NULL);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
//updates the peds current drawable 

void CCutSceneDebugManager::UpdateCutSceneVarDrawblCB(void)
{
	if (m_varDrawableId > m_maxDrawables)
	{
		m_varDrawableId = m_maxDrawables;
	}

	if (m_pActorEntity)
	{
		CPedModelInfo* pedModelInfo = NULL;
		if (m_pActorEntity->GetGameEntity())
		{
			m_varTextureId = CPedVariationPack::GetTexVar(m_pActorEntity->GetGameEntity(), static_cast<ePedVarComp>(m_varComponentId));
			// want to pick up current palette ID from the current debug ped (for this drawable)
			m_varPalette = CPedVariationPack::GetPaletteVar(m_pActorEntity->GetGameEntity(), static_cast<ePedVarComp>(m_varComponentId));
			pedModelInfo =  m_pActorEntity->GetGameEntity()->GetPedModelInfo();
		}
		else 
		{
			m_varTextureId = 0;
			m_varPalette = 0; 
		}	

		if(pedModelInfo)
		{
			m_maxTextures = pedModelInfo->GetVarInfo()->GetMaxNumTex(static_cast<ePedVarComp>(m_varComponentId), m_varDrawableId)-1;
			UpdateCutSceneVarTexCB();
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
//Update the texture for the current debug ped.

void CCutSceneDebugManager::UpdateCutSceneVarTexCB(void)
{
	if (m_pVarDrawablIdSlider == NULL || m_pVarTexIdSlider == NULL)
	{
		return; 
	}

	u32	maxVariations = 0;
	char	texVariation = 'a' + static_cast<char>(m_varTextureId);
	char	drawblRaceType = 'U';
	u32	raceId = 0;

	if (m_varTextureId > m_maxTextures){
		m_varTextureId = m_maxTextures;
	}

	//get the modelInfo associated with this ped ID
	if (m_pActorEntity)
	{
		CPedModelInfo* pedModelInfo = NULL; 
		if (m_pActorEntity->GetGameEntity())
		{
			pedModelInfo =  m_pActorEntity->GetGameEntity()->GetPedModelInfo();
		}

		if(pedModelInfo)
		{
			if (pedModelInfo->GetVarInfo()->IsValidDrawbl(m_varComponentId,m_varDrawableId))
			{
				//pedInfoIndex = CModelInfo::GetModelIdFromName(pedModelInfo.GetModelName()).GetModelIndex();
				raceId = pedModelInfo->GetVarInfo()->GetRaceType(m_varComponentId, m_varDrawableId, m_varTextureId, pedModelInfo);
				if (pedModelInfo->GetVarInfo()->HasRaceTextures(m_varComponentId, m_varDrawableId)){
					drawblRaceType = 'R';
				}

				if (pedModelInfo->GetVarInfo()->IsMatchComponent(m_varComponentId, m_varDrawableId, pedModelInfo)){
					drawblRaceType = 'M';
				}

				// update the texture and drawable names
				sprintf(m_varTexName,"%s_diff_%03d_%c_%s", varSlotNames[m_varComponentId], m_varDrawableId, texVariation, raceTypeNames[raceId]);
				sprintf(m_varDrawablName,"%s_%03d_%c",varSlotNames[m_varComponentId],m_varDrawableId, drawblRaceType);

				// update the max values in the sliders
				maxVariations = pedModelInfo->GetVarInfo()->GetMaxNumDrawbls(static_cast<ePedVarComp>(m_varComponentId));
				m_pVarDrawablIdSlider->SetRange(0.0f, float(maxVariations-1));
				m_pVarDrawablIdSlider->SetStep(1); 
				maxVariations = pedModelInfo->GetVarInfo()->GetMaxNumTex(static_cast<ePedVarComp>(m_varComponentId), m_varDrawableId);
				m_pVarTexIdSlider->SetRange(0.0f, float(maxVariations-1));
				m_pVarTexIdSlider->SetStep(1); 
				//set the updated values for this ped (if it is the same type as being edited in the widget!)
				if (m_pActorEntity->GetGameEntity())
				{
					if(!CPedVariationPack::HasVariation(m_pActorEntity->GetGameEntity(), static_cast<ePedVarComp>(m_varComponentId), m_varDrawableId, 0, m_varTextureId))
					{
#if !__NO_OUTPUT
						bool ReactivateLogging = false; 
						if(m_pActorEntity->GetGameEntity()->m_LogVariationCalls)
						{
							m_pActorEntity->GetGameEntity()->m_LogVariationCalls = false; 
							ReactivateLogging = true; 
						}
#endif //!__NO_OUTPUT
						m_pActorEntity->GetGameEntity()->SetVariation(static_cast<ePedVarComp>(m_varComponentId), m_varDrawableId, 0, m_varTextureId, 0, 0, false);
#if !__NO_OUTPUT
						if(ReactivateLogging)
						{
							m_pActorEntity->GetGameEntity()->m_LogVariationCalls = true;
						}
#endif //!__NO_OUTPUT
					}
				}
			}
			else
			{
				sprintf(m_varTexName,"Invalid Texture");
				sprintf(m_varDrawablName,"Invalid Drawable");

				//pVarDrawablIdSlider->SetRange(0.0f, 0.0f);
				//pVarTexIdSlider->SetRange(0.0f, 0.0f);
			}
		}
	}
	//UpdateVarPaletteCB();
}

/////////////////////////////////////////////////////////////////////////////////////
//Render the peds variation to the screen 
void CCutSceneDebugManager::DisplayPedVariation()
{
		if (m_pActorEntity)
		{
			Color32 Colour (Color_red); 

			if (m_pActorEntity->GetCutfObject())
			{
				grcDebugDraw::AddDebugOutput(Colour , m_pActorEntity->GetCutfObject()->GetDisplayName().c_str());	
			}

			const cutfObject* pObject = m_pActorEntity->GetCutfObject(); 
			const cutfModelObject* pModel = NULL; 


			if(pObject->GetType() == CUTSCENE_MODEL_OBJECT_TYPE)
			{
				pModel = static_cast<const cutfModelObject*>(pObject); 
			}
			
			if(m_pActorEntity->GetGameEntity())
			{
				const Vector3 vGameEntityPosition = VEC3V_TO_VECTOR3(m_pActorEntity->GetGameEntity()->GetTransform().GetPosition());
				Vector3 vPos = vGameEntityPosition; 
				vPos.z += 1.0f;
				grcDebugDraw::Line(vGameEntityPosition,vPos,Colour);
				char  text [64]; 
				if(pModel)
				{
					grcDebugDraw::Text(vPos, Colour, formatf(text,  "Edit: %s (Scene Handle:%s)", m_pActorEntity->GetCutfObject()->GetDisplayName().c_str(), pModel->GetHandle().GetCStr()));
				}
				else
				{
					grcDebugDraw::Text(vPos, Colour, formatf(text, "Edit: %s", m_pActorEntity->GetCutfObject()->GetDisplayName().c_str()));
				}
				
			}

			for(int index=0; index < PV_MAX_COMP ; index++)
			{
				int DrawableId = 0; 
				int TextureId = 0; 
				char  text [24]; 
				if(m_pActorEntity->GetGameEntity())
				{
					DrawableId = CPedVariationPack::GetCompVar(m_pActorEntity->GetGameEntity(), static_cast<ePedVarComp>(varSlotEnums[index]));
					TextureId = CPedVariationPack::GetTexVar(m_pActorEntity->GetGameEntity(), static_cast<ePedVarComp>(varSlotEnums[index]));
				}

				strncpy(text, varSlotNames[index],  24 ); 
				strncat(text, " D: %i, T: %i", 24); 
				if(DrawableId > 0 ||  TextureId > 0 )
				{
					grcDebugDraw::AddDebugOutput(Colour, text, DrawableId, TextureId );
				}
				else
				{
					grcDebugDraw::AddDebugOutput(text, DrawableId, TextureId );
				}
			}	
		}
}

/////////////////////////////////////////////////////////////////////////////////////

void CCutSceneDebugManager::BankSavePedVariationEventCallBack()
{
	if (CutSceneManager::GetInstance()->m_pedModelObjectList.GetCount() > 0 && m_iPedModelIndex <= CutSceneManager::GetInstance()->m_pedModelObjectList.GetCount() && m_iPedModelIndex > 0  )
	{
		if ( m_varDrawableId > -1  && m_varTextureId >-1)
		{
			const cutfObject* pObject = CutSceneManager::GetInstance()->m_pedModelObjectList[m_iPedModelIndex - 1]; 
			if ( (CutSceneManager::GetInstance()->IsPaused() || CutSceneManager::GetInstance()->IsPlaying()) && pObject)
			{
				//Save the ped variation into the event list
				SavePedVariationEvent(pObject); 

				//update the combo box of ped variation events
				PopulatePedVariationEventList(pObject); 

				//Save the changes out to the cuttune file.
				CutSceneManager::GetInstance()->SaveCutfile(); 
			}
		}
		else
		{
			cutsceneWarningf("Save failed. Invalid drawable and/or texture in cutscene variation event.");
		}

	}
}

void CCutSceneDebugManager::BankSavePedPropVariationEventCallBack()
{
	if (CutSceneManager::GetInstance()->m_pedModelObjectList.GetCount() > 0 && m_iPedModelIndex <= CutSceneManager::GetInstance()->m_pedModelObjectList.GetCount() && m_iPedModelIndex > 0  )
	{
		const cutfObject* pObject = CutSceneManager::GetInstance()->m_pedModelObjectList[m_iPedModelIndex - 1]; 

		if ( (CutSceneManager::GetInstance()->IsPaused() || CutSceneManager::GetInstance()->IsPlaying()) && pObject)
		{
			//Save the ped variation into the event list
			SavePedPropVariationEvent(pObject); 

			//update the combo box of ped variation events
			PopulatePedVariationEventList(pObject); 

			//Save the changes out to the cuttune file.
			CutSceneManager::GetInstance()->SaveCutfile(); 
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
//Add the ped variation event to the event list. 
void CCutSceneDebugManager::SavePedVariationEvent(const cutfObject* pObject)
{
	if(!Verifyf(CutSceneManager::GetInstance()->GetCutfFile(), "Save Ped Variation: No cutf file object loaded cannot save"))
	{
		return;
	}

	if ( pObject == NULL )
	{
		return;
	}

	RAGE_TRACK( CutSceneManager_SavePedVariation );

	atArray<cutfEvent *> PedEventList;
	CutSceneManager::GetInstance()->GetCutfFile()->FindEventsForObjectIdOnly( pObject->GetObjectId(), CutSceneManager::GetInstance()->GetCutfFile()->GetEventList(), PedEventList );

	bool bAddNewEvent = true; 

	//look to edit a current event 
	for ( int i = 0; i < PedEventList.GetCount(); ++i )
	{
		if (PedEventList[i]->GetEventId() == CUTSCENE_SET_VARIATION_EVENT)
		{
			if(PedEventList[i]->GetTime() == CutSceneManager::GetInstance()->GetCutSceneTimeWithRangeOffset())
			{
				cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( PedEventList[i]->GetEventArgs() );

				if(pEventArgs && pEventArgs->GetType() == CUTSCENE_ACTOR_VARIATION_EVENT_ARGS_TYPE)
				{
					//Don't add new event for a component that has the same id and time. 			
					cutfObjectVariationEventArgs* pObjectVar = static_cast<cutfObjectVariationEventArgs*>(pEventArgs); 

					if (pObjectVar->GetComponent() == m_varComponentId )
					{	
						pObjectVar->SetDrawable(m_varDrawableId); 
						pObjectVar->SetTexture(m_varTextureId); 
						bAddNewEvent = false; 	
					}
				}
			}
		}
	}


	if(bAddNewEvent)
	{
		cutfEventArgs *pEventArgs = rage_new cutfObjectVariationEventArgs( pObject->GetObjectId(), m_varComponentId, m_varDrawableId, m_varTextureId );

#if	CUTSCENE_EXPORT_EVENTS_WITH_ZERO_BASE_TIME
		float ftime = CutSceneManager::GetInstance()->GetCutSceneTimeWithRangeOffset();  
#else
		float ftime = CutSceneManager::GetInstance()->GetCutSceneCurrentTime(); 
#endif

		CutSceneManager::GetInstance()->GetCutfFile()->AddEvent( rage_new cutfObjectIdEvent( pObject->GetObjectId(), ftime, CUTSCENE_SET_VARIATION_EVENT, pEventArgs ) );
		CutSceneManager::GetInstance()->GetCutfFile()->SortEvents();
	}
}

/////////////////////////////////////////////////////////////////////////////////////
//Add the ped variation event to the event list. 
void CCutSceneDebugManager::SavePedPropVariationEvent(const cutfObject* pObject)
{
	if(!Verifyf(CutSceneManager::GetInstance()->GetCutfFile(), "Save Ped Variation: No cutf file object loaded cannot save"))
	{
		return;
	}

	if ( pObject == NULL )
	{
		return;
	}

	RAGE_TRACK( CutSceneManager_SavePedVariation );

	atArray<cutfEvent *> PedEventList;
	CutSceneManager::GetInstance()->GetCutfFile()->FindEventsForObjectIdOnly( pObject->GetObjectId(), CutSceneManager::GetInstance()->GetCutfFile()->GetEventList(), PedEventList );

	bool bAddNewEvent = true; 

	//look to edit a current event 
	for ( int i = 0; i < PedEventList.GetCount(); ++i )
	{
		if (PedEventList[i]->GetEventId() == CUTSCENE_SET_VARIATION_EVENT)
		{
			if(PedEventList[i]->GetTime() == CutSceneManager::GetInstance()->GetCutSceneTimeWithRangeOffset())
			{
				cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( PedEventList[i]->GetEventArgs() );

				if(pEventArgs && pEventArgs->GetType() == CUTSCENE_ACTOR_VARIATION_EVENT_ARGS_TYPE)
				{
					//Don't add new event for a component that has the same id and time. 			
					cutfObjectVariationEventArgs* pObjectVar = static_cast<cutfObjectVariationEventArgs*>(pEventArgs); 

					if (pObjectVar->GetComponent() == m_PropSlotId + PV_MAX_COMP)
					{	
						pObjectVar->SetDrawable(m_PropDrawableId); 
						pObjectVar->SetTexture(m_PropTextureId); 
						bAddNewEvent = false; 	
					}
				}
			}
		}
	}


	if(bAddNewEvent)
	{
		cutfEventArgs *pEventArgs = rage_new cutfObjectVariationEventArgs( pObject->GetObjectId(), m_PropSlotId + PV_MAX_COMP, m_PropDrawableId, m_PropTextureId );

#if	CUTSCENE_EXPORT_EVENTS_WITH_ZERO_BASE_TIME
		float ftime = CutSceneManager::GetInstance()->GetCutSceneTimeWithRangeOffset();  
#else
		float ftime = CutSceneManager::GetInstance()->GetCutSceneCurrentTime(); 
#endif

		CutSceneManager::GetInstance()->GetCutfFile()->AddEvent( rage_new cutfObjectIdEvent( pObject->GetObjectId(), ftime, CUTSCENE_SET_VARIATION_EVENT, pEventArgs ) );
		CutSceneManager::GetInstance()->GetCutfFile()->SortEvents();
	}
}


/////////////////////////////////////////////////////////////////////////////////////
//Update the ped variation event list for a new ped or new events. 

void CCutSceneDebugManager::PopulatePedVariationEventList(const cutfObject* pObject)
{
	if (m_pEventCombo == NULL)
	{
		return; 
	}
	//Nothing selected
	if(!pObject)
	{
		m_PedVarEventId = 0; 
		m_pEventCombo->UpdateCombo( "Ped var events", &m_PedVarEventId, 1, NULL );
		m_pEventCombo->SetString( 0 , "none");

		return; 
	}

	atArray<cutfEvent *> PedEventList;
	CutSceneManager::GetInstance()->GetCutfFile()->FindEventsForObjectIdOnly( pObject->GetObjectId(), CutSceneManager::GetInstance()->GetCutfFile()->GetEventList(), PedEventList );
	
	atArray<cutfEvent*> PedVariationList; 

	for ( int i = 0; i < PedEventList.GetCount(); i++ )
	{
		if (PedEventList[i]->GetEventId() == CUTSCENE_SET_VARIATION_EVENT)
		{
			if (PedEventList[i]->GetEventArgs())
			{
				if(PedEventList[i]->GetEventArgs()->GetType() == CUTSCENE_ACTOR_VARIATION_EVENT_ARGS_TYPE)
				{
					PedVariationList.Grow() = PedEventList[i]; 
				}
			}
			else
			{
				if ( PedEventList[i]->GetType() == CUTSCENE_OBJECT_ID_EVENT_TYPE )
				{
					const cutfObjectIdEvent* pObjectIdEvent = static_cast<const cutfObjectIdEvent*>( PedEventList[i] );
					if ( pObjectIdEvent )
					{
						const cutfObject* pObject = CutSceneManager::GetInstance()->GetObjectById(pObjectIdEvent->GetObjectId());
						if ( pObject )
						{
							if ( pObject->GetType() == CUTSCENE_MODEL_OBJECT_TYPE )
							{
								const cutfModelObject* pModelObject = static_cast< const cutfModelObject* >(pObject);
								if ( pModelObject->GetModelType() == CUTSCENE_PED_MODEL_TYPE )
								{
									PedVariationList.Grow() = PedEventList[i];
								}
							}
						}
					}
				}
			}
		}
	}
	
	m_PedVarEventId = PedVariationList.GetCount() -1; 

	//look to edit a current event 
	if (PedVariationList.GetCount() == 0)
	{
		m_PedVarEventId = 0;
		m_pEventCombo->UpdateCombo( "Ped var events", &m_PedVarEventId, 1, NULL );
		m_pEventCombo->SetString( 0 , "none");
	}
	else
	{
		if (PedVariationList.GetCount() > 0)
		{
			m_pEventCombo->UpdateCombo( "Ped var events", &m_PedVarEventId, PedVariationList.GetCount(), NULL );

			//Clear the list of variations, need to repopulate. 
			m_CutSceneVariations.Reset(); 

			for ( int i = 0; i < PedVariationList.GetCount(); ++i )
			{
				if (PedVariationList[i]->GetEventId() == CUTSCENE_SET_VARIATION_EVENT)
				{
					cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( PedVariationList[i]->GetEventArgs() );

					if(pEventArgs && pEventArgs->GetType() == CUTSCENE_ACTOR_VARIATION_EVENT_ARGS_TYPE)
					{						
						char EventInfo[64]; 	

						cutfObjectVariationEventArgs* pObjectVar = static_cast<cutfObjectVariationEventArgs*>(pEventArgs); 

						s32 FrameNumber = static_cast<s32>(PedVariationList[i]->GetTime() * CUTSCENE_FPS); 
						
						if(pObjectVar->GetComponent() < PV_MAX_COMP)
						{
							formatf(EventInfo, sizeof(EventInfo)-1,"Event %d: Frame: %d , time: %f , %s , D: %d, T: %d ", i , FrameNumber, PedVariationList[i]->GetTime(),  
								varSlotNames[pObjectVar->GetComponent()], pObjectVar->GetDrawable(), pObjectVar->GetTexture());
						}
						else
						{
							formatf(EventInfo, sizeof(EventInfo)-1,"Event %d: Frame: %d , time: %f , %s , D: %d, T: %d ", i , FrameNumber, PedVariationList[i]->GetTime(),  
								propSlotNames[pObjectVar->GetComponent()-PV_MAX_COMP], pObjectVar->GetDrawable(), pObjectVar->GetTexture());
						}

						m_pEventCombo->SetString( i, EventInfo );

						//Store a list of pointers to our objects
						m_CutSceneVariations.PushAndGrow(PedVariationList[i]); 
					}
					else
					{
						char EventInfo[64]; 	

						s32 FrameNumber = static_cast<s32>(PedVariationList[i]->GetTime() * CUTSCENE_FPS); 

						formatf(EventInfo, sizeof(EventInfo)-1,"Event %d: Frame: %d , time: %f , id: %i , CORRUPT EVENT!", i, FrameNumber, PedVariationList[i]->GetTime(), PedVariationList[i]->GetEventId());

						m_pEventCombo->SetString( i, EventInfo );

						//Store a list of pointers to our objects
						m_CutSceneVariations.PushAndGrow(PedVariationList[i]); 
					}
				}
			}
			//m_PedVarEventId = 0; 
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////

void CCutSceneDebugManager::BankDeletePedVariationCallBack()
{
	if (m_CutSceneVariations.GetCount() > 0 && m_PedVarEventId < m_CutSceneVariations.GetCount())
	{
		if(m_CutSceneVariations[m_PedVarEventId])
		{
			CutSceneManager::GetInstance()->GetCutfFile()->RemoveEvent(m_CutSceneVariations[m_PedVarEventId]);
			PopulatePedVariationEventList(CutSceneManager::GetInstance()->m_pedModelObjectList[m_iPedModelIndex - 1]); 
			CutSceneManager::GetInstance()->SaveCutfile(); 
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

SEditCutfObjectLocationInfo* CCutSceneDebugManager::GetFixupObjectInfo()
{
	if (m_pGameObject)
	{	
		//Lets check the model index is valid before proceeding, otherwise it will crash when trying to get the name.
		//Cant make parts of the map disappear only objects. 
		u32 index = m_pGameObject->GetModelIndex();
		if(CModelInfo::IsValidModelInfo(index))
		{
			SEditCutfObjectLocationInfo* pEditInfo = rage_new SEditCutfObjectLocationInfo; 
			pEditInfo->vPosition = VEC3V_TO_VECTOR3(m_pGameObject->GetTransform().GetPosition());  
			strncpy(pEditInfo->cName,CModelInfo::GetBaseModelInfoName(fwModelId(strLocalIndex(index))), CUTSCENE_OBJNAMELEN);
			pEditInfo->fRadius = m_pGameObject->GetBoundRadius();

			return pEditInfo;
		}
	}
	return NULL;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

SEditCutfObjectLocationInfo* CCutSceneDebugManager::GetHiddenObjectInfo()
{
	if (m_pGameObject)
	{	
		//Lets check the model index is valid before proceeding, otherwise it will crash when trying to get the name.
		//Cant make parts of the map disappear only objects. 
		u32 index = m_pGameObject->GetModelIndex();
		if(CModelInfo::IsValidModelInfo(index))
		{
			SEditCutfObjectLocationInfo* pEditInfo = rage_new SEditCutfObjectLocationInfo; 
			pEditInfo->vPosition = VEC3V_TO_VECTOR3(m_pGameObject->GetTransform().GetPosition());  
			strncpy(pEditInfo->cName,CModelInfo::GetBaseModelInfoName(fwModelId(strLocalIndex(index))), CUTSCENE_OBJNAMELEN);
			pEditInfo->fRadius = m_pGameObject->GetBoundRadius();

			return pEditInfo;
		}
	}
	return NULL;
}


///////////////////////////////////////////////////////////////////////////////////////////////////

sEditCutfBlockingBoundsInfo* CCutSceneDebugManager::GetBlockingBoundObjectInfo()
{
	sEditCutfBlockingBoundsInfo* pEditInfo = rage_new sEditCutfBlockingBoundsInfo; 
	pEditInfo->vCorners[0] = m_vCorners[0];
	pEditInfo->vCorners[1] = m_vCorners[1];
	pEditInfo->vCorners[2] = m_vCorners[2];
	pEditInfo->vCorners[3] = m_vCorners[3];
	pEditInfo->fHeight = m_fHeight ;

	char BoundName[CUTSCENE_OBJNAMELEN]; 
	formatf(BoundName, sizeof(BoundName)-1, "%s_BB_%d",  CutSceneManager::GetInstance()->GetCutsceneName(), CutSceneManager::GetInstance()->GetNumBlockingBoundObjects()+1); 
	strncpy(pEditInfo->cName, BoundName, CUTSCENE_OBJNAMELEN);

	return pEditInfo;
}



///////////////////////////////////////////////////////////////////////////////////////////////////

void CCutSceneDebugManager::RightArrowFastForward()
{
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_RIGHT, KEYBOARD_MODE_DEBUG))
	{
		CutSceneManager::GetInstance()->BankFastForwardCallback(); 
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CCutSceneDebugManager::LeftArrowRewind()
{
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_LEFT, KEYBOARD_MODE_DEBUG))
	{
		CutSceneManager::GetInstance()->BankRewindCallback(); 
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CCutSceneDebugManager::UpArrowPause()
{
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_UP, KEYBOARD_MODE_DEBUG))
	{
		CutSceneManager::GetInstance()->BankPauseCallback(); 
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CCutSceneDebugManager::DownArrowPlay()
{
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_DOWN, KEYBOARD_MODE_DEBUG))
	{
		CutSceneManager::GetInstance()->BankPlayForwardsCallback(); 
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CCutSceneDebugManager::CtrlRightArrowStep()
{
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_RIGHT, KEYBOARD_MODE_DEBUG_CTRL))
	{
		CutSceneManager::GetInstance()->BankStepForwardCallback(); 
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void CCutSceneDebugManager::CtrlLeftArrowStep()
{
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_LEFT, KEYBOARD_MODE_DEBUG_CTRL))
	{
		CutSceneManager::GetInstance()->BankStepBackwardCallback(); 
	}
}

/////////////////////////////////////////////////////////////////////////////////////
//Gets an object under the mouse when clicked on

void CCutSceneDebugManager::GetMapObject()
{
	//Activation is set on a widget toggle so that entity can't just be selected by default
	if(CutSceneManager::GetInstance()->m_ActivateMapObjectEditing)
	{
		if(!g_PickerManager.IsEnabled())
		{
			g_PickerManager.SetEnabled(true);
			g_PickerManager.SetUiEnabled(false);
		}

		fwEntity* pEntity = g_PickerManager.GetSelectedEntity(); //CDebugScene::GetEntityUnderMouse();
		//float radius; 

		if (pEntity)
		{	
			if(CDebugScene::GetMouseLeftPressed()) 
			{
				m_pGameObject = (CEntity* )pEntity; 
			}
			
			if(m_pGameObject)
			{
				CDebugScene::DrawEntityBoundingBox(m_pGameObject, Color32(255, 0, 0, 255));
			}
		}	
		else
		{
			if(CDebugScene::GetMouseLeftPressed()) 
			{
				m_pGameObject = NULL; 
			}
		}

		if(m_pGameObject)
		{
			if (m_pGameObject->IsArchetypeSet())
			{
				Vector3 vBoundMax = m_pGameObject->GetBoundingBoxMax(); 
				CDebugScene::DrawEntityBoundingBox(m_pGameObject, Color32(0, 0, 255, 255));
				grcDebugDraw::Text(VEC3V_TO_VECTOR3(m_pGameObject->GetTransform().GetPosition()), CRGBA(0,0,255,255),0, grcDebugDraw::GetScreenSpaceTextHeight(), "Selected for Edit");
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
//Null the pointer to our the game object.

void CCutSceneDebugManager::ClearMapObject()
{
	m_pGameObject = NULL;
}			

/////////////////////////////////////////////////////////////////////////////////////

//Select if the object is visible or not.
void CCutSceneDebugManager::DisplayHiddenObject()
{
	if (CutSceneManager::GetInstance()->m_ActivateMapObjectEditing && CutSceneManager::GetInstance()->m_editHiddenObjectList.GetCount() > 0)
	{
		Vec3V textZOffset(0.0f, 0.0f, 0.8f);

		for (int i =0 ; i < CutSceneManager::GetInstance()->m_editHiddenObjectList.GetCount(); i++)
		{
			u32 iModelIndex = CModelInfo::GetModelIdFromName( CutSceneManager::GetInstance()->m_editHiddenObjectList[i]->cName ).GetModelIndex();

			if (CModelInfo::IsValidModelInfo(iModelIndex)) 
			{
				CEntity* pEntity = CutSceneManager::GetEntityToAtPosition(CutSceneManager::GetInstance()->m_editHiddenObjectList[i]->vPosition, CutSceneManager::GetInstance()->m_editHiddenObjectList[i]->fRadius, iModelIndex  );

				if(CutSceneManager::GetInstance()->m_bRenderHiddenObjects && CutSceneManager::GetInstance()->m_ActivateMapObjectEditing)
				{
					//add special text if the object from the selector list is being edited
					char Message[50]; 
					sprintf(Message, "Model %s set to be hidden",CutSceneManager::GetInstance()->m_editHiddenObjectList[i]->cName);
					if(CutSceneManager::GetInstance()->m_iSelectedHiddenObjectIndex-1 == i)
					{
						Vec3V vPos = RCC_VEC3V(CutSceneManager::GetInstance()->m_editHiddenObjectList[i]->vPosition); 

						if(pEntity)
						{
							vPos += And(RCC_VEC3V(pEntity->GetBoundingBoxMax()), Vec3V(V_MASKZ));
							CDebugScene::DrawEntityBoundingBox(pEntity, Color32(0, 255, 0, 255));
						}

						grcDebugDraw::Line(RCC_VEC3V(CutSceneManager::GetInstance()->m_editHiddenObjectList[i]->vPosition), vPos + Vec3V(V_Z_AXIS_WZERO), Color32(0, 255, 0, 255));
						grcDebugDraw::Text(vPos + textZOffset, CRGBA(0,255,0,255),0, grcDebugDraw::GetScreenSpaceTextHeight(), "Selected for EDIT" );

						grcDebugDraw::Cross(vPos, CutSceneManager::GetInstance()->m_editHiddenObjectList[i]->fRadius, Color32(0, 255, 0, 255));
						grcDebugDraw::Cross(RCC_VEC3V(CutSceneManager::GetInstance()->m_editHiddenObjectList[i]->vPosition), CutSceneManager::GetInstance()->m_editHiddenObjectList[i]->fRadius, Color32(0, 255, 0, 255));
						grcDebugDraw::Text(CutSceneManager::GetInstance()->m_editHiddenObjectList[i]->vPosition, CRGBA(0,0,255,255),0, grcDebugDraw::GetScreenSpaceTextHeight(), Message );
					}
					else
					{
						if(pEntity)
						{
							CDebugScene::DrawEntityBoundingBox(pEntity, Color32(0, 0, 255, 255));
						}

						grcDebugDraw::Cross(RCC_VEC3V(CutSceneManager::GetInstance()->m_editHiddenObjectList[i]->vPosition), CutSceneManager::GetInstance()->m_editHiddenObjectList[i]->fRadius, Color32(0, 0, 255, 255));
						grcDebugDraw::Text(CutSceneManager::GetInstance()->m_editHiddenObjectList[i]->vPosition, CRGBA(0,0,255,255),0, grcDebugDraw::GetScreenSpaceTextHeight(), Message );
					}

					if (pEntity && !pEntity->IsVisible())
					{
						pEntity->SetIsVisibleForModule(SETISVISIBLE_MODULE_CUTSCENE, true);
						//cutsceneModelEntityDebugf("CCutSceneDebugManager::DisplayHiddenObject: Showing entity (%s)", pEntity->GetModelName());
					}
				}
				else
				{
					if(!CutSceneManager::GetInstance()->m_bRenderHiddenObjects)
					{
						if (pEntity && pEntity->IsVisible())	
						{
							pEntity->SetIsVisibleForModule(SETISVISIBLE_MODULE_CUTSCENE, false); 
							//cutsceneModelEntityDebugf("CCutSceneDebugManager::DisplayHiddenObject: Hiding entity (%s)", pEntity->GetModelName());
						}
					}
				}
			}

		}
	}
}

//Select if the object is visible or not.
void CCutSceneDebugManager::FixupSelectedObject()
{
	if (CutSceneManager::GetInstance()->m_editFixupObjectList.GetCount() > 0)
	{
		for (int i =0 ; i < CutSceneManager::GetInstance()->m_editFixupObjectList.GetCount(); i++)
		{
			u32 iModelIndex = CModelInfo::GetModelIdFromName( CutSceneManager::GetInstance()->m_editFixupObjectList[i]->cName ).GetModelIndex();

			if (CModelInfo::IsValidModelInfo(iModelIndex)) 
			{
				CEntity* pEntity = CutSceneManager::GetEntityToAtPosition(CutSceneManager::GetInstance()->m_editFixupObjectList[i]->vPosition, CutSceneManager::GetInstance()->m_editFixupObjectList[i]->fRadius, iModelIndex  );

				if (pEntity)
				{
					if(CutSceneManager::GetInstance()->m_ActivateMapObjectEditing)
					{
						char Message[50]; 
						sprintf(Message, "Model %s set to be fixed",CutSceneManager::GetInstance()->m_editFixupObjectList[i]->cName);

						grcDebugDraw::Cross(RCC_VEC3V(CutSceneManager::GetInstance()->m_editFixupObjectList[i]->vPosition), CutSceneManager::GetInstance()->m_editFixupObjectList[i]->fRadius, Color32(0, 0, 255, 255));
						CDebugScene::DrawEntityBoundingBox(pEntity, Color32(0, 0, 255, 255));
						grcDebugDraw::Text(CutSceneManager::GetInstance()->m_editFixupObjectList[i]->vPosition, CRGBA(0,0,0,255),0, grcDebugDraw::GetScreenSpaceTextHeight(), Message );

					}
					else
					{
						if(CutSceneManager::GetInstance()->m_FixupMapObject)
						{
							CutSceneManager::GetInstance()->FixupRequestedObjects(CutSceneManager::GetInstance()->m_editFixupObjectList[i]->vPosition, CutSceneManager::GetInstance()->m_editFixupObjectList[i]->fRadius, iModelIndex);
							CutSceneManager::GetInstance()->m_FixupMapObject = false; 
						}
					}
				}
			}

		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
//Get the mouse position of mouse in world coords, this interescts with collision so need to specify a z offset

void CCutSceneDebugManager::UpdateMouseCursorPosition(Vector3 &WorldPos1, Vector3 &WorldPos2, Vector3 &WorldPosZOffset, bool bUseMouseWheel)
{
	Vector3 vPos;
	Vector3 vNormal;
	void *entity;

	bool bHasPosition = CDebugScene::GetWorldPositionUnderMouse(vPos, ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE , &vNormal, &entity);

	if( bHasPosition )
	{
		if( ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT || ioMouse::GetButtons() & ioMouse::MOUSE_LEFT )
		{
			WorldPos1 = vPos + WorldPosZOffset;
		}

		if ( ioMouse::GetPressedButtons() & ioMouse::MOUSE_RIGHT || ioMouse::GetButtons() & ioMouse::MOUSE_RIGHT )
		{
			WorldPos2 = vPos + WorldPosZOffset;
		}
		if(bUseMouseWheel)
		{
			if((ioMouse::HasWheel() && ioMouse::GetDZ()>0)||ioMapper::DebugKeyDown(KEY_COMMA))
			{
				WorldPosZOffset.z += 0.1f;
				WorldPos1.z += 0.1f;
				WorldPos2.z += 0.1f;
			}

			if((ioMouse::HasWheel() && ioMouse::GetDZ()<0)||ioMapper::DebugKeyDown(KEY_COMMA))
			{
				WorldPosZOffset.z -= 0.1f;
				WorldPos1.z -= 0.1f;
				WorldPos2.z -= 0.1f;
			}
		}
	}
	grcDebugDraw::Sphere(WorldPos2, 0.05f, Color32(1.0f, 0.0f, 1.0f) );
	grcDebugDraw::Sphere(WorldPos1, 0.05f, Color32(1.0f, 0.0f, 0.0f) );
}


/////////////////////////////////////////////////////////////////////////////////////
// Create the word space representation of of the box

void CCutSceneDebugManager::CreateDebugAngledAreaMatrix(const Vector3 &vPos, float width, float length, float height, const Vector3 &vRot, Matrix34 &mMat, Vector3 &vMax)
{
	mMat.Identity(); 
	Vector3 SizeVector (width* 0.5f, length* 0.5f, height* 0.5f); 
	Vector3 vRot_temp = vRot * DtoR; 
	mMat.FromEulersXYZ(vRot_temp);
	mMat.d = vPos;
	vMax = SizeVector; 
}

/////////////////////////////////////////////////////////////////////////////////////
//Convert the corner matrix into scene space

void CCutSceneDebugManager::ConvertMatrixAndVectorLocateToFourCornersAndHeight(const Matrix34 &mLocateMat, const Vector3 &vMax, Vector3 &vCorner1, Vector3 &vCorner2, Vector3 &vCorner3, Vector3 &vCorner4, float &fHeight )  
{
	//Get scene space 
	Matrix34 SceneMat(M34_IDENTITY);
	CutSceneManager::GetInstance()->GetSceneOrientationMatrix(SceneMat);

	//locate space 
	Matrix34 LocateMat(M34_IDENTITY);
	LocateMat = mLocateMat; 

	Vector3 vMaxLocal(vMax);

	Vector3 vMinLocal = (vMaxLocal * -1.0f); 
	fHeight = vMaxLocal.z - vMinLocal.z; 

	//set up the corners in local space
	vCorner1 = Vector3(vMinLocal.x, vMinLocal.y, vMinLocal.z);  
	vCorner2 = Vector3(vMaxLocal.x, vMinLocal.y, vMinLocal.z);
	vCorner3 = Vector3(vMaxLocal.x, vMaxLocal.y, vMinLocal.z); 
	vCorner4 = Vector3(vMinLocal.x, vMaxLocal.y, vMinLocal.z); 

	//to render the corner text in the correct world pos. 
	Vector3 vCorner1RenderPos = vCorner1; 
	Vector3 vCorner2RenderPos = vCorner2; 
	Vector3 vCorner3RenderPos = vCorner3; 
	Vector3	vCorner4RenderPos = vCorner4; 

	//transform our local vector into world space for rendering
	mLocateMat.Transform(vCorner1RenderPos); 
	mLocateMat.Transform(vCorner2RenderPos);
	mLocateMat.Transform(vCorner3RenderPos);
	mLocateMat.Transform(vCorner4RenderPos);

	//need to put our result into scene space they are both in world space. The locate is effectively a child of the scene mat.
	LocateMat.DotTranspose(SceneMat); 

	//transform our local vector into locate space ie world space
	LocateMat.Transform(vCorner1); 
	LocateMat.Transform(vCorner2);
	LocateMat.Transform(vCorner3);
	LocateMat.Transform(vCorner4);

	//create the render text to display the scene space position in world space

	char position[256]; 

	formatf(position, sizeof(position)-1, "C1: (%f, %f, %f)",vCorner1.x, vCorner1.y, vCorner1.z ); 

	grcDebugDraw::Text(vCorner1RenderPos, CRGBA(0,0,0,255),0, grcDebugDraw::GetScreenSpaceTextHeight(), position );

	formatf(position, sizeof(position)-1, "C2: (%f, %f, %f)",vCorner2.x, vCorner2.y, vCorner2.z ); 

	grcDebugDraw::Text(vCorner2RenderPos, CRGBA(0,0,0,255),0, grcDebugDraw::GetScreenSpaceTextHeight(), position );

	formatf(position, sizeof(position)-1, "C3: (%f, %f, %f)",vCorner3.x, vCorner3.y, vCorner3.z ); 

	grcDebugDraw::Text(vCorner3RenderPos, CRGBA(0,0,0,255),0, grcDebugDraw::GetScreenSpaceTextHeight(), position );

	formatf(position, sizeof(position)-1, "C4: (%f, %f, %f)",vCorner4.x, vCorner4.y, vCorner4.z ); 

	grcDebugDraw::Text(vCorner4RenderPos, CRGBA(0,0,0,255),0, grcDebugDraw::GetScreenSpaceTextHeight(), position );

}

///////////////////////////////////////////////////////////////////////////
// Applies post scene update
void CCutSceneDebugManager::DisplayAndEditBlockingBounds()
{
	CutSceneManager* pManager = CutSceneManager::GetInstance(); 
	if(pManager->m_bActivateBlockingObjectEditing)
	{
		CreateDebugAngledAreaMatrix(pManager->m_vBlockingBoundPos, pManager->m_fBoundingBoxWidth, pManager->m_fBoundingBoxLength, pManager->m_fBoundingBoxHeight, pManager->m_vBlockingBoundRot, m_Mat, m_vMax );
		ConvertMatrixAndVectorLocateToFourCornersAndHeight(m_Mat, m_vMax, m_vCorners[0], m_vCorners[1], m_vCorners[2], m_vCorners[3], m_fHeight )  ;

		grcDebugDraw::Axis( m_Mat, 0.25, true); 
		grcDebugDraw::BoxOriented(RCC_VEC3V(m_vMax) * ScalarV(V_NEGONE), RCC_VEC3V(m_vMax), RCC_MAT34V(m_Mat),CRGBA(0,0,255,255), false);
	}

	if (pManager->m_bRenderBlockingBoundObjects)
	{
		for (int i =0; i<pManager->m_editBlockingBoundObjectList.GetCount(); i++)
		{
			//Render the bound name for ease of editing 
			const char* BoundName = pManager->m_editBlockingBoundObjectList[i]->cName;

			////have selected a locate from the bounding box list update it and then we can render it
			if(pManager->m_iSelectedBlockingBoundObjectIndex-1 == i )
			{
				Vector3 vConvertedToRads (0.0f, 0.0f, pManager->m_LocateMatRot.z * DtoR); 
				pManager->m_editBlockingBoundObjectList[i]->LocateMat.FromEulersXYZ(vConvertedToRads); 
				ConvertMatrixAndVectorLocateToFourCornersAndHeight(pManager->m_editBlockingBoundObjectList[i]->LocateMat, pManager->m_editBlockingBoundObjectList[i]->vMax, pManager->m_editBlockingBoundObjectList[i]->vCorners[0], pManager->m_editBlockingBoundObjectList[i]->vCorners[1], 
					pManager->m_editBlockingBoundObjectList[i]->vCorners[2],  pManager->m_editBlockingBoundObjectList[i]->vCorners[3], pManager->m_editBlockingBoundObjectList[i]->fHeight);	

				grcDebugDraw::Axis(pManager->m_editBlockingBoundObjectList[i]->LocateMat, 0.25, true);
				grcDebugDraw::BoxOriented(RCC_VEC3V(pManager->m_editBlockingBoundObjectList[i]->vMax) * ScalarV(V_NEGONE),RCC_VEC3V(pManager->m_editBlockingBoundObjectList[i]->vMax), MATRIX34_TO_MAT34V(pManager->m_editBlockingBoundObjectList[i]->LocateMat),CRGBA(0,255,0,255), false);
				grcDebugDraw::Text(pManager->m_editBlockingBoundObjectList[i]->LocateMat.d, CRGBA(0,255,0,255),0,grcDebugDraw::GetScreenSpaceTextHeight(), BoundName);

			}
			else
			{
				float creationTime = 0.0f;
				float removalTime = pManager->GetCutfFile()->GetTotalDuration();

				atArray<cutfEvent*>ObjectEvents; 
				pManager->GetCutfFile()->FindEventsForObjectIdOnly( pManager->m_editBlockingBoundObjectList[i]->pObject->GetObjectId(), pManager->GetCutfFile()->GetEventList(), ObjectEvents );

				for (s32 j=0; j<ObjectEvents.GetCount(); j++)
				{
					if (ObjectEvents[j]->GetEventId()==CUTSCENE_ADD_BLOCKING_BOUNDS_EVENT)
					{
						creationTime = ObjectEvents[j]->GetTime();
					}
					else if (ObjectEvents[j]->GetEventId()==CUTSCENE_REMOVE_BLOCKING_BOUNDS_EVENT)
					{
						removalTime = ObjectEvents[j]->GetTime();
					}
				}

				Color32 color;

				if (pManager->GetCutSceneCurrentTime() < removalTime && pManager->GetCutSceneCurrentTime()>= creationTime)
				{
					color = CRGBA(255,0,0,255);
				}
				else
				{
					color = CRGBA(0,0,255,255);
				}

				pManager->ConvertAngledAreaToMatrixAndVectors(pManager->m_editBlockingBoundObjectList[i]->vCorners, pManager->m_editBlockingBoundObjectList[i]->fHeight, pManager->m_editBlockingBoundObjectList[i]->vMin, pManager->m_editBlockingBoundObjectList[i]->vMax, pManager->m_editBlockingBoundObjectList[i]->LocateMat, creationTime); 				
				grcDebugDraw::Axis( pManager->m_editBlockingBoundObjectList[i]->LocateMat, 0.25, true); 
				grcDebugDraw::BoxOriented(RCC_VEC3V(pManager->m_editBlockingBoundObjectList[i]->vMin), VECTOR3_TO_VEC3V(pManager->m_editBlockingBoundObjectList[i]->vMax), MATRIX34_TO_MAT34V(pManager->m_editBlockingBoundObjectList[i]->LocateMat),color, false);
				grcDebugDraw::Text(pManager->m_editBlockingBoundObjectList[i]->LocateMat.d, CRGBA(0,255,0,255),0,grcDebugDraw::GetScreenSpaceTextHeight(), BoundName);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// OVERLAYS
///////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////
//Get the entity that managers vehicle and populate all the relevant lists. 
void CCutSceneDebugManager::GetSelectedOverlayEntityCallBack()
{
	if (m_iOverlayIndex > 0)
	{
		cutsEntity* pCutEntity = CutSceneManager::GetInstance()->GetEntityByObjectId(m_OverlayObjectList[m_iOverlayIndex - 1]->GetObjectId()); 

		CCutSceneScaleformOverlayEntity* pEnt = static_cast<CCutSceneScaleformOverlayEntity*> (pCutEntity); 

		if(pEnt)
		{
			m_pOverlay = pEnt; 
			GetOverlaySettings(); 
			PopulateOverlayEventList(m_OverlayObjectList[m_iOverlayIndex - 1]); 
		}
	}
	else
	{
		//selected none from the vehicle selector.
		m_pOverlay = NULL;
		PopulateOverlayEventList(NULL); 

	}
	GetOverlayMovieMethods(); 
}

void CCutSceneDebugManager::GetOverlaySettings()
{
	if(m_pOverlay)
	{
		m_vOverlayPos = m_pOverlay->GetOverlayPos(); 
		m_vOverlaySize = m_pOverlay->GetOverlaySize(); 
	}
}

void CCutSceneDebugManager::SetOverlayPositionCB()
{
	if(m_pOverlay)
	{
		m_pOverlay->SetOverlayPos(m_vOverlayPos); 
	}
}

void CCutSceneDebugManager::SetOverlaySizeCB()
{
	if(m_pOverlay)
	{
		m_pOverlay->SetOverlaySize(m_vOverlaySize); 
	}
}

void CCutSceneDebugManager::BankSaveOverlayEventsCallBack()
{
	if (m_OverlayObjectList.GetCount() > 0 && m_iOverlayIndex <= m_OverlayObjectList.GetCount() && m_iOverlayIndex > 0  )
	{
		const cutfObject* pObject = m_OverlayObjectList[m_iOverlayIndex - 1]; 

		if ( (CutSceneManager::GetInstance()->IsPaused() || CutSceneManager::GetInstance()->IsPlaying()) && pObject)
		{
			//Save the ped variation into the event list
			m_EventAttributes.Reset(); 
			AddAttributesFromParams(); 
			SaveOverlayEvent(pObject, m_OverlayMethodNames[m_iOverlayMethodIndex], CutSceneCustomEvents::CUTSCENE_SET_OVERLAY_METHOD_EVENT); 
			m_EventAttributes.Reset(); 

			//update the combo box of ped variation events
			PopulateOverlayEventList(pObject); 

			//Save the changes out to the cuttune file.
			CutSceneManager::GetInstance()->SaveCutfile(); 
		}
	}
}

void CCutSceneDebugManager::PopulateOverlayEventList(const cutfObject* pObject)
{
	if (m_pOverlayEventCombo == NULL)
	{
		return; 
	}

	//Nothing selected
	if(!pObject)
	{
		m_OverlayEventId = 0; 
		m_pOverlayEventCombo->UpdateCombo( "events", &m_OverlayEventId, 1, NULL );
		m_pOverlayEventCombo->SetString( 0 , "none");
		return; 
	}

	atArray<cutfEvent *> AllEventList;
	CutSceneManager::GetInstance()->GetCutfFile()->FindEventsForObjectIdOnly( pObject->GetObjectId(), CutSceneManager::GetInstance()->GetCutfFile()->GetEventList(), AllEventList );

	atArray<cutfEvent *>EventList; 

	for(int i =0; i < AllEventList.GetCount(); i++)
	{
		cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( AllEventList[i]->GetEventArgs() );
		if(pEventArgs->GetType() == CUTSCENE_NAME_EVENT_ARGS_TYPE)
		{
			EventList.PushAndGrow(AllEventList[i]); 
		}
	}

	//look to edit a current event 
	if (EventList.GetCount() == 0)
	{
		m_pOverlayEventCombo->UpdateCombo( "Overlay Events", &m_OverlayEventId, 1, NULL );
		m_pOverlayEventCombo->SetString( 0 , "none");
	}
	else
	{
		if (EventList.GetCount() > 0)
		{
			m_pOverlayEventCombo->UpdateCombo( "Overlay Events", &m_OverlayEventId, EventList.GetCount(), NULL );

			//Clear the list of variations, need to repopulate. 
			for (int i=0; i < m_OverlayEvents.GetCount(); i++)
			{
				m_OverlayEvents[i] = NULL;  
			}
			m_OverlayEvents.Reset(); 

			for ( int i = 0; i < EventList.GetCount(); ++i )
			{
				if (EventList[i]->GetEventId() == CutSceneCustomEvents::CUTSCENE_SET_OVERLAY_METHOD_EVENT || EventList[i]->GetEventId() == CutSceneCustomEvents::CUTSCENE_SET_OVERLAY_PROPERTIES_EVENT)
				{
					cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( EventList[i]->GetEventArgs() );

					if(pEventArgs->GetType() == CUTSCENE_NAME_EVENT_ARGS_TYPE )
					{						
						char EventInfo[64]; 	
						EventInfo[0] = '\0'; 

						cutfNameEventArgs* pObjectVar = static_cast<cutfNameEventArgs*>(pEventArgs); 

						s32 FrameNumber = static_cast<s32>(EventList[i]->GetTime() * CUTSCENE_FPS); 
						
						char ParamInfo[64]; 
						ParamInfo[0] = '\0'; 

						for(int j =0; j<MAX_NUMBER_OVERLAY_PARAMS; j++)
						{
							char ParamName [32] ; 
							formatf(ParamName, 32, "param_%d", j+1); 
							const parAttribute* pAttribute = pEventArgs->GetAttributeList().FindAttributeAnyCase(ParamName); 
							
							if(pAttribute)
							{
								char ParamData [32] ; 
								ParamData[0] = '\0'; 

								if (j > 0)
								{
									safecat(ParamInfo, " , " );
								}

								switch(pAttribute->GetType())
								{
								case parAttribute::DOUBLE:
									{
										formatf(ParamData, 32, "%f", pAttribute->GetFloatValue()); 
										safecat(ParamInfo, ParamData );
									}
									break; 
								case parAttribute::BOOL:
									{
										if(pAttribute->GetBoolValue())
										{
											safecat(ParamInfo, "true" );
										}
										else
										{
											safecat(ParamInfo, "false" );
										}
										
									}
									break;
								case parAttribute::STRING:
									{
										safecat(ParamInfo, pAttribute->GetStringValue() );
									}
									break; 
								case parAttribute::INT64:
									{
										formatf(ParamData, 32, "%d", pAttribute->GetIntValue()); 
										safecat(ParamInfo, ParamData );
									}
									break; 

								}
								
							}
						}			
						
						formatf(EventInfo, sizeof(EventInfo)-1,"(%d) %s(%s)",FrameNumber, pObjectVar->GetName().GetCStr(), ParamInfo ); 

						m_pOverlayEventCombo->SetString( i, EventInfo );

						//Store a list of pointers to our objects
						m_OverlayEvents.PushAndGrow(EventList[i]); 
					}
				}
			}
		}
	}
};

void CCutSceneDebugManager::SaveOverlayEvent(const cutfObject* pObject, const char* ArgsName, s32 iEventId)
{
	if(!Verifyf(CutSceneManager::GetInstance()->GetCutfFile(), "Overlay Event: No cutf file object loaded cannot save"))
	{
		return;
	}

	if ( pObject == NULL )
	{
		return;
	}

	atArray<cutfEvent *> EventList;
	CutSceneManager::GetInstance()->GetCutfFile()->FindEventsForObjectIdOnly( pObject->GetObjectId(), CutSceneManager::GetInstance()->GetCutfFile()->GetEventList(), EventList );

	bool bAddNewEvent = true; 

	//look to edit a current event 
	//for ( int i = 0; i < EventList.GetCount(); ++i )
	//{
	//	if (EventList[i]->GetEventId() == CUTSCENE_SET_VARIATION_EVENT)
	//	{
	//		if(EventList[i]->GetTime() == CutSceneManager::GetInstance()->GetCutSceneTimeWithRangeOffset() )
	//		{
	//			cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( EventList[i]->GetEventArgs() );

	//			if(pEventArgs && pEventArgs->GetType() == CUTSCENE_NAME_EVENT_ARGS_TYPE)
	//			{
	//				//Don't add new event for a component that has the same id and time. 			
	//			
	//				bAddNewEvent = false; 	

	//			}
	//		}
	//	}
	//}

	if(bAddNewEvent)
	{
		cutfEventArgs *pEventArgs = NULL; 
		
		pEventArgs = rage_new cutfNameEventArgs(ArgsName, m_EventAttributes);
		

#if	CUTSCENE_EXPORT_EVENTS_WITH_ZERO_BASE_TIME
		float ftime = CutSceneManager::GetInstance()->GetCutSceneTimeWithRangeOffset();  
#else
		float ftime =CutSceneManager::GetInstance()-> GetCutSceneCurrentTime(); 
#endif
		CutSceneManager::GetInstance()->GetCutfFile()->AddEvent( rage_new cutfObjectIdEvent( pObject->GetObjectId(), ftime, iEventId, pEventArgs ) );
		CutSceneManager::GetInstance()->GetCutfFile()->SortEvents();

		for(int i=0; m_OverlayParams.GetCount() > 0; )
		{
			DeleteParamWidget(i); 
		}

	}
}

void CCutSceneDebugManager::GetOverlayMovieMethods()
{
	m_OverlayMethodNames.Reset(); 
	if(m_pOverlay)
	{
		if (m_pOverlayMethodsCombo)
		{
			m_pOverlayMethodsCombo->UpdateCombo( "Overlay Methods", &m_iOverlayMethodIndex, m_OverlayMethodNames.GetCount(), &m_OverlayMethodNames[0],  NullCB);
		}
	}
	else
	{
		m_OverlayMethodNames.PushAndGrow("none"); //this is to match the combo box in index
		if (m_pOverlayMethodsCombo)
		{
			m_pOverlayMethodsCombo->UpdateCombo( "Overlay Methods", &m_iOverlayMethodIndex, 1, NULL );
			m_pOverlayMethodsCombo->SetString( 0 , "none");
		}
	}
}


void CCutSceneDebugManager::CreateParamWidgets()
{
	//look for params that already exist and don't add twice
	for(int i = 0; i <m_OverlayParams.GetCount(); i++)
	{
		if(m_OverlayParams[i].ParamNumber == m_iOverlayParamNameIndex)
		{
			return; 
		}
	}
	
	bkBank* Ragebank = BANKMGR.FindBank("Cut Scene Debug");
	bkGroup* CurrentSceneGrp = static_cast<bkGroup*>(BANKMGR.FindWidget("Cut Scene Debug/Current Scene/Overlay/Overlay Events/Overlay Method Events/Params"));
	if(Ragebank)
	{
		if (CurrentSceneGrp)
		{	
			Ragebank->SetCurrentGroup(*CurrentSceneGrp);
				
				switch(m_iOverlayParamTypeIndex)
				{
				case PT_INT:
					{
						Params NewParam; 
						NewParam.pParam = rage_new CParamTypeInt(); 
						NewParam.ParamNumber = m_iOverlayParamNameIndex;
						NewParam.pParam->AddWidgets(Ragebank, OverlayParamNames[m_iOverlayParamNameIndex]);
						m_OverlayParams.PushAndGrow(NewParam); 

					}
					break;

				case PT_FLOAT:
					{
						Params NewParam; 
						NewParam.pParam = rage_new CParamTypeFloat(); 
						NewParam.ParamNumber = m_iOverlayParamNameIndex;
						NewParam.pParam->AddWidgets(Ragebank, OverlayParamNames[m_iOverlayParamNameIndex]);
						m_OverlayParams.PushAndGrow(NewParam); 
					}
					break; 
				case PT_STRING:
					{
						Params NewParam; 
						NewParam.pParam = rage_new CParamTypeString(); 
						NewParam.ParamNumber = m_iOverlayParamNameIndex;
						NewParam.pParam->AddWidgets(Ragebank, OverlayParamNames[m_iOverlayParamNameIndex]);
						m_OverlayParams.PushAndGrow(NewParam); 
					}
					break; 
				case PT_BOOL:
					{
						Params NewParam; 
						NewParam.pParam = rage_new CParamTypeBool(); 
						NewParam.ParamNumber = m_iOverlayParamNameIndex;
						NewParam.pParam->AddWidgets(Ragebank, OverlayParamNames[m_iOverlayParamNameIndex]);
						m_OverlayParams.PushAndGrow(NewParam); 
					}
					break; 

				}
			Ragebank->UnSetCurrentGroup(*CurrentSceneGrp);
		}
	}
}

void CCutSceneDebugManager::DeleteParamWidget(s32 index)
{
	if(index >=0 && index < m_OverlayParams.GetCount() )
	{
		m_OverlayParams[index].pParam->DeleteWidgets(); 

		delete m_OverlayParams[index].pParam; 
		m_OverlayParams[index].pParam = NULL; 
		m_OverlayParams.Delete(index); 
	}
}



bool CCutSceneDebugManager::AddAttributesFromParams()
{
	if(m_OverlayParams.GetCount() > 0)
	{
		for(int i = 0; i <m_OverlayParams.GetCount(); i++)
		{
			switch(m_OverlayParams[i].pParam->GetType())
			{
			case CParamType::kParameterFloat: 
				{
					CParamTypeFloat* pTypeParam = static_cast<CParamTypeFloat*>(m_OverlayParams[i].pParam); 		
					m_EventAttributes.AddAttribute(OverlayParamNames[m_OverlayParams[i].ParamNumber],pTypeParam->m_value);
				}
				break;
			case CParamType::kParameterInt: 
				{
					CParamTypeInt* pTypeParam = static_cast<CParamTypeInt*>(m_OverlayParams[i].pParam); 
					m_EventAttributes.AddAttribute(OverlayParamNames[m_OverlayParams[i].ParamNumber],pTypeParam->m_value);
				}
				break;
			case CParamType::kParameterBool: 
				{
					CParamTypeBool* pTypeParam = static_cast<CParamTypeBool*>(m_OverlayParams[i].pParam); 
					m_EventAttributes.AddAttribute(OverlayParamNames[m_OverlayParams[i].ParamNumber],pTypeParam->m_value);
				}
				break;
			case CParamType::kParameterString: 
				{
					CParamTypeString* pTypeParam = static_cast<CParamTypeString*>(m_OverlayParams[i].pParam); 
					m_EventAttributes.AddAttribute(OverlayParamNames[m_OverlayParams[i].ParamNumber],pTypeParam->m_value);
				}
				break;

			case CParamType::kParameterWidgetNum:
			default:
			{

			}
			break; 
			}
		}
		return true; 
	}
	else
	{
		return false; 
	}
}

void CCutSceneDebugManager::BankDeleteOverlayEventCallBack()
{
	if (m_OverlayEventId < m_OverlayEvents.GetCount() )
	{
		if(m_OverlayEvents[m_OverlayEventId])
		{
			CutSceneManager::GetInstance()->GetCutfFile()->RemoveEvent(m_OverlayEvents[m_OverlayEventId]);
			PopulateOverlayEventList(m_OverlayObjectList[m_iOverlayIndex - 1]); 
			CutSceneManager::GetInstance()->SaveCutfile(); 
		}
	}
}

void CCutSceneDebugManager::BankSaveMovieProperties()
{
	if (m_OverlayObjectList.GetCount() > 0 && m_iOverlayIndex <= m_OverlayObjectList.GetCount() && m_iOverlayIndex > 0  )
	{
		const cutfObject* pObject = m_OverlayObjectList[m_iOverlayIndex - 1]; 

		if ( (CutSceneManager::GetInstance()->IsPaused() || CutSceneManager::GetInstance()->IsPlaying()) && pObject)
		{
			//Save the ped variation into the event list
			m_EventAttributes.Reset(); 
			m_EventAttributes.AddAttribute("param_1", m_vOverlaySize.x); 
			m_EventAttributes.AddAttribute("param_2", m_vOverlaySize.y); 
			m_EventAttributes.AddAttribute("param_3", m_vOverlayPos.x); 
			m_EventAttributes.AddAttribute("param_4", m_vOverlayPos.y); 
			m_EventAttributes.AddAttribute("param_5", m_bUseScreenRenderTarget); 
			SaveOverlayEvent(pObject, "OVERLAY_PROP", CutSceneCustomEvents::CUTSCENE_SET_OVERLAY_PROPERTIES_EVENT); 
			m_EventAttributes.Reset(); 

			//update the combo box of ped variation events
			PopulateOverlayEventList(pObject); 

			//Save the changes out to the cuttune file.
			CutSceneManager::GetInstance()->SaveCutfile(); 
		}
	}
}

//Water Refract index

void CCutSceneDebugManager::BankSaveWaterRefractionIndexEventCallBack()
{
	if (m_CameraObjectList.GetCount() > 0)
	{
		const cutfObject* pObject = m_CameraObjectList[0]; 

		if ( (CutSceneManager::GetInstance()->IsPaused() || CutSceneManager::GetInstance()->IsPlaying()) && pObject)
		{
			//Save the ped variation into the event list
			SaveWaterRefractionIndexEvent(pObject, CutSceneCustomEvents::CUTSCENE_SET_WATER_REFRACT_INDEX_EVENT); 

			//update the combo box of ped variation events
			PopulateWaterRefractionIndexEventList(pObject); 

			//Save the changes out to the cuttune file.
			CutSceneManager::GetInstance()->SaveCutfile(); 
		}
	}
}

void CCutSceneDebugManager::SaveWaterRefractionIndexEvent(const cutfObject* pObject, s32 iEventId)
{
	if(!Verifyf(CutSceneManager::GetInstance()->GetCutfFile(), "Overlay Event: No cutf file object loaded cannot save"))
	{
		return;
	}

	if ( pObject == NULL )
	{
		return;
	}

	atArray<cutfEvent *> EventList;
	CutSceneManager::GetInstance()->GetCutfFile()->FindEventsForObjectIdOnly( pObject->GetObjectId(), CutSceneManager::GetInstance()->GetCutfFile()->GetEventList(), EventList );

	bool bAddNewEvent = true; 

	if(bAddNewEvent)
	{
		cutfEventArgs *pEventArgs = NULL; 

		pEventArgs = rage_new cutfFloatValueEventArgs(m_fWaterRefractionIndex);

#if	CUTSCENE_EXPORT_EVENTS_WITH_ZERO_BASE_TIME
		float ftime = CutSceneManager::GetInstance()->GetCutSceneTimeWithRangeOffset();  
#else
		float ftime =CutSceneManager::GetInstance()-> GetCutSceneCurrentTime(); 
#endif
		CutSceneManager::GetInstance()->GetCutfFile()->AddEvent( rage_new cutfObjectIdEvent( pObject->GetObjectId(), ftime, iEventId, pEventArgs ) );
		CutSceneManager::GetInstance()->GetCutfFile()->SortEvents();
	}
}

void CCutSceneDebugManager::BankDeleteWaterRefractionIndexEventCallBack()
{
	if (m_iWaterRefractionIndexId < m_WaterRefractionIndexEvents.GetCount() )
	{
		const cutfObject* pObject = m_CameraObjectList[0]; 

		if(m_WaterRefractionIndexEvents[m_iWaterRefractionIndexId] && pObject)
		{
			CutSceneManager::GetInstance()->GetCutfFile()->RemoveEvent(m_WaterRefractionIndexEvents[m_iWaterRefractionIndexId]);
			PopulateWaterRefractionIndexEventList(pObject); 
			CutSceneManager::GetInstance()->SaveCutfile(); 
		}
	}
}

void CCutSceneDebugManager::PopulateWaterRefractionIndexEventList(const cutfObject* pObject)
{
	if (m_pWaterRefractionIndexCombo == NULL)
	{
		return; 
	}

	//Nothing selected
	if(!pObject)
	{
		m_pWaterRefractionIndexCombo->UpdateCombo( "events", &m_iWaterRefractionIndexId, 1, NULL );
		m_pWaterRefractionIndexCombo->SetString( 0 , "none");
		return; 
	}

	atArray<cutfEvent *> AllEventList;
	CutSceneManager::GetInstance()->GetCutfFile()->FindEventsForObjectIdOnly( pObject->GetObjectId(), CutSceneManager::GetInstance()->GetCutfFile()->GetEventList(), AllEventList );

	atArray<cutfEvent *>EventList; 

	for(int i =0; i < AllEventList.GetCount(); i++)
	{
		//cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( AllEventList[i]->GetEventArgs() );
		
		if(AllEventList[i]->GetEventId() == CutSceneCustomEvents::CUTSCENE_SET_WATER_REFRACT_INDEX_EVENT)
		{
			EventList.PushAndGrow(AllEventList[i]); 
		}
	}

	//look to edit a current event 
	if (EventList.GetCount() == 0)
	{
		m_pWaterRefractionIndexCombo->UpdateCombo( "Water Refraction Index Events", &m_iWaterRefractionIndexId, 1, NULL );
		m_pWaterRefractionIndexCombo->SetString( 0 , "none");
	}
	else
	{
		if (EventList.GetCount() > 0)
		{
			m_pWaterRefractionIndexCombo->UpdateCombo( "Water Refraction Index Events", &m_iWaterRefractionIndexId, EventList.GetCount(), NULL );

			//Clear the list of variations, need to repopulate. 
			for (int i=0; i < m_WaterRefractionIndexEvents.GetCount(); i++)
			{
				m_WaterRefractionIndexEvents[i] = NULL;  
			}
			
			m_WaterRefractionIndexEvents.Reset(); 

			for ( int i = 0; i < EventList.GetCount(); ++i )
			{
				if (EventList[i]->GetEventId() == CutSceneCustomEvents::CUTSCENE_SET_WATER_REFRACT_INDEX_EVENT)
				{
					cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( EventList[i]->GetEventArgs() );

					if(pEventArgs->GetType() == CUTSCENE_FLOAT_VALUE_EVENT_ARGS_TYPE )
					{						
						char EventInfo[64]; 	
						EventInfo[0] = '\0'; 

						cutfFloatValueEventArgs* pObjectVar = static_cast<cutfFloatValueEventArgs*>(pEventArgs); 

						s32 FrameNumber = static_cast<s32>(EventList[i]->GetTime() * CUTSCENE_FPS); 
						
						formatf(EventInfo, sizeof(EventInfo)-1,"(%d) Refraction index: %f", FrameNumber, pObjectVar->GetFloat1()); 

						m_pWaterRefractionIndexCombo->SetString( i, EventInfo );

						//Store a list of pointers to our objects
						m_WaterRefractionIndexEvents.PushAndGrow(EventList[i]); 
					}
				}
			}
		}
	}
};

void CCutSceneDebugManager::BankSaveTimeCycleParamsCallBack()
{
	if (m_CameraObjectList.GetCount() > 0)
	{
		const cutfObject* pObject = m_CameraObjectList[0]; 

		if ( (CutSceneManager::GetInstance()->IsPaused() || CutSceneManager::GetInstance()->IsPlaying()) && pObject)
		{
			//Save the ped variation into the event list
			SaveTimeCycleParamsEvent(pObject); 

			//Save the changes out to the cuttune file.
			CutSceneManager::GetInstance()->SaveGameDataCutfile(); 
		}
	}
}


void CCutSceneDebugManager::PopulateCameraCutEventList(const cutfObject* pObject, bkCombo* Combo, s32 &ComboIndex, datCallback CallBack)
{
	if (Combo == NULL)
	{
		return; 
	}

	//Nothing selected
	if(!pObject)
	{
		ComboIndex = 0; 
		Combo->UpdateCombo( "camera cut events", &ComboIndex, 1, NULL );
		Combo->SetString( 0 , "none");
		return; 
	}

	atArray<cutfEvent *> AllEventList;
	CutSceneManager::GetInstance()->GetCutfFile()->FindEventsForObjectIdOnly( pObject->GetObjectId(), CutSceneManager::GetInstance()->GetCutfFile()->GetEventList(), AllEventList );

	atArray<cutfEvent *>EventList; 

	for(int i =0; i < AllEventList.GetCount(); i++)
	{
		//cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( AllEventList[i]->GetEventArgs() );

		if(AllEventList[i]->GetEventId() == CUTSCENE_CAMERA_CUT_EVENT)
		{
			EventList.PushAndGrow(AllEventList[i]); 
		}
	}

	//look to edit a current event 
	if (EventList.GetCount() == 0)
	{
		Combo->UpdateCombo( "Camera Cut Events", &ComboIndex, 1, NULL );
		Combo->SetString( 0 , "none");
	}
	else
	{
		if (EventList.GetCount() > 0)
		{
			Combo->UpdateCombo( "Camera Cut Events", &ComboIndex, EventList.GetCount(), 0, CallBack );

			//Clear the list of variations, need to repopulate. 
			for (int i=0; i < m_CameraCutsIndexEvents.GetCount(); i++)
			{
				m_CameraCutsIndexEvents[i] = NULL;  
			}

			m_CameraCutsIndexEvents.Reset(); 

			for ( int i = 0; i < EventList.GetCount(); ++i )
			{
				if (EventList[i]->GetEventId() == CUTSCENE_CAMERA_CUT_EVENT)
				{
					cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( EventList[i]->GetEventArgs() );

					if(pEventArgs->GetType() == CUTSCENE_CAMERA_CUT_EVENT_ARGS_TYPE )
					{						
						char EventInfo[64]; 	
						EventInfo[0] = '\0'; 

						cutfCameraCutEventArgs* pObjectVar = static_cast<cutfCameraCutEventArgs*>(pEventArgs); 

						s32 FrameNumber = static_cast<s32>(EventList[i]->GetTime() * CUTSCENE_FPS); 

						formatf(EventInfo, sizeof(EventInfo)-1,"(%d) %s", FrameNumber, pObjectVar->GetName().GetCStr()); 

						Combo->SetString( i, EventInfo );

						//Store a list of pointers to our objects
						m_CameraCutsIndexEvents.PushAndGrow(EventList[i]); 
					}
				}
			}
		}
	}
};

void CCutSceneDebugManager::UpdateTimeCycleParamsCB()
{
	if (m_pTimeCycleCameraCutCombo == NULL)
	{
		return; 
	}

	if(m_CameraCutsIndexEvents[m_iTimecycleEventsIndexId])
	{
		cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( m_CameraCutsIndexEvents[m_iTimecycleEventsIndexId]->GetEventArgs() );
		if(pEventArgs->GetType() == CUTSCENE_CAMERA_CUT_EVENT_ARGS_TYPE )
		{	
			cutfCameraCutEventArgs* pObjectVar = static_cast<cutfCameraCutEventArgs*>(pEventArgs); 
			
			if(pObjectVar)
			{
				 m_ReflectionLodRangeStart = pObjectVar->GetReflectionLodRangeStart();
				 m_ReflectionLodRangeEnd = pObjectVar->GetReflectionLodRangeEnd();
				 m_ReflectionSLodRangeStart = pObjectVar->GetReflectionSLodRangeStart();
				 m_ReflectionSLodRangeEnd = pObjectVar->GetReflectionSLodRangeEnd(); 
				 m_LodMultHD = pObjectVar->GetLodMultHD();
				 m_LodMultOrphanedHD = pObjectVar->GetLodMultOrphanHD(); 
				 m_LodMultLod = pObjectVar->GetLodMultLod(); 
				 m_LodMultSLod1 = pObjectVar->GetLodMultSlod1(); 
				 m_LodMultSLod2 = pObjectVar->GetLodMultSlod2();
				 m_LodMultSLod3 = pObjectVar->GetLodMultSlod3();
				 m_LodMultSLod4 = pObjectVar->GetLodMultSlod4();
				 m_WaterReflectionFarClip = pObjectVar->GetWaterReflectionFarClip(); 
				 m_SSAOLightInten = pObjectVar->GetLightSSAOInten(); 
				 m_LodMult = pObjectVar->GetMapLodScale();
				 m_ExposurePush = pObjectVar->GetExposurePush();
				 m_bDisableHighQualityDof = pObjectVar->GetDisableHighQualityDof();
				 m_CharacterLight = pObjectVar->GetCharacterLightParams();
				 m_FreezeReflectionMap = pObjectVar->IsReflectionMapFrozen(); 
				 m_DisableDirectionalLight = pObjectVar->IsDirectionalLightDisabled(); 
				 m_LightFadeDistanceMult = pObjectVar->GetLightFadeDistanceMult(); 
				 m_LightShadowFadeDistanceMult = pObjectVar->GetLightShadowFadeDistanceMult();  
				 m_LightSpecularFadeDistMult = pObjectVar->GetLightSpecularFadeDistMult(); 
				 m_LightVolumetricFadeDistanceMult = pObjectVar->GetLightVolumetricFadeDistanceMult(); 
				 m_AbsoluteIntensityEnabled = pObjectVar->IsAbsoluteIntensityEnabled(); 
				 m_DirectionalLightMultiplier = pObjectVar->GetDirectionalLightMultiplier();
				 m_LensArtefactMultiplier = pObjectVar->GetLensArtefactMultiplier();
				 m_MaxBloom = pObjectVar->GetBloomMax(); 
			}
		}
	}
}

float CCutSceneDebugManager::ValidateTimeCycleParams(float &param)
{
	if(param < 0.0f)
	{
		param = -1.0f; 
	}

	return param; 
};

void CCutSceneDebugManager::UpdateCurrentCutName()
{
	s32 EventIndex; 
	atHashString CameraCut(FindCameraCutNameForTime(m_CameraCutsIndexEvents, CutSceneManager::GetInstance()->GetCutSceneCurrentTime(), EventIndex)); 
	
	if(EventIndex > -1 && m_ShouldSyncTimeOfDayDofOverridedWidgetIndexidtoCurrentCameraCut)
	{
		if(m_iCoCModifiersCamerCutComboIndexId != EventIndex)
		{
			m_iCoCModifiersCamerCutComboIndexId = EventIndex; 
			UpdateCoCModifierEventOverridesCB(); 
		}
	}


	char CutInfo[128]; 	
	CutInfo[0] = '\0'; 
	
	float cutTime =  FindCameraCutTimeForTime(m_CameraCutsIndexEvents, CutSceneManager::GetInstance()->GetCutSceneCurrentTime()); 

	s32 FrameNumber = static_cast<s32>(cutTime * CUTSCENE_FPS); 

	formatf(CutInfo, sizeof(CutInfo)-1,"(%.2f %d) %s", cutTime, FrameNumber, CameraCut.GetCStr()); 

	if(CameraCut.GetHash() == 0)
	{
		strcpy(m_CameraCutName, "none"); 
	}
	else
	{
#if CUTSCENE_LIGHT_EVENT_LOGGING
		if(strcmp(m_CameraCutName, CutInfo) != 0)
		{
			cutsDisplayf("\t%u\t%.2f\t%s\tCAMERA CUT", fwTimer::GetFrameCount(), CutSceneManager::GetInstance()->GetTime(), CutInfo);
		}
#endif
		strcpy(m_CameraCutName, CutInfo); 
	}

	m_CascadeBounds.SelectShadowEvent(CameraCut);
}

void CCutSceneDebugManager::UpdateFirstPersonBlendOutDuration()
{
	if(m_FirstPersonBlendOutSlider)
	{
		float TimeRemaining = CutSceneManager::GetInstance()->GetEndTime() - CutSceneManager::GetInstance()->GetTime(); 

		m_FirstPersonBlendOutSlider->SetRange(0.0f, TimeRemaining); 
	}
}

atHashString CCutSceneDebugManager::FindCameraCutNameForTime(const atArray <cutfEvent*>& events, float time, s32 &EventIndex)
{
	atHashString cameraCutName; 

	if(events.GetCount() > 0)
	{
		for ( int i = events.GetCount() - 1; i >= 0; --i )
		{
			if ( time >= events[i]->GetTime())
			{
				if (events[i]->GetEventId() == CUTSCENE_CAMERA_CUT_EVENT)
				{
					cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( events[i]->GetEventArgs() );

					if(pEventArgs->GetType() == CUTSCENE_CAMERA_CUT_EVENT_ARGS_TYPE )
					{
						cutfCameraCutEventArgs* pObjectVar = static_cast<cutfCameraCutEventArgs*>(pEventArgs); 

						if(pObjectVar)
						{
							cameraCutName = pObjectVar->GetName(); 
							EventIndex = i; 
							return cameraCutName; 
						}
					}
				}
			
			}
		}
	}
	EventIndex = -1; 
	return cameraCutName; 
}

atHashString CCutSceneDebugManager::FindNearestCameraCutNameForTime(const atArray <cutfEvent*>& events, float time, s32 &EventIndex)
{
	atHashString cameraCutName; 

	atArray <cutfEvent*> cutEvents;

	for(int i=0; i < events.GetCount(); i++)
	{
		if ( events[i]->GetEventId() == CUTSCENE_CAMERA_CUT_EVENT)
		{
			cutEvents.PushAndGrow(events[i]); 
		}
	}

	if(cutEvents.GetCount() > 0)
	{
		for ( int i = cutEvents.GetCount() - 1; i >= 0; --i )
		{
			if ( time >= cutEvents[i]->GetTime())
			{
				int index = i; 

				if(i < cutEvents.GetCount() - 1)
				{
					float SecondEventTimeDifference  = ABS(time - cutEvents[i+1]->GetTime()); 
					float  firstEventTimeDifference = ABS(time - cutEvents[i]->GetTime()); 

					if(SecondEventTimeDifference < firstEventTimeDifference)
					{
						index = i+1; 
					}
				}

				cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( cutEvents[index]->GetEventArgs() );

				if(pEventArgs->GetType() == CUTSCENE_CAMERA_CUT_EVENT_ARGS_TYPE )
				{
					cutfCameraCutEventArgs* pObjectVar = static_cast<cutfCameraCutEventArgs*>(pEventArgs); 

					if(pObjectVar)
					{
						cameraCutName = pObjectVar->GetName(); 
						EventIndex = index; 
						return cameraCutName; 
					}
				}
			}
		}
	}
	EventIndex = -1; 
	return cameraCutName; 
}

float CCutSceneDebugManager::FindCameraCutTimeForTime(const atArray <cutfEvent*>& events, float time)
{
	float cameraCutTime = -1.0f; 

	if(events.GetCount() > 0)
	{
		for ( int i = events.GetCount() - 1; i >= 0; --i )
		{
			if ( time >= events[i]->GetTime())
			{
				if (events[i]->GetEventId() == CUTSCENE_CAMERA_CUT_EVENT)
				{
					cameraCutTime = events[i]->GetTime(); 
					return cameraCutTime; 
				}
			}
		}
	}
	return cameraCutTime; 
}

const cutfEvent* CCutSceneDebugManager::FindCameraCutEventForTime(const atArray <cutfEvent*>& events, float time)
{
	if(events.GetCount() > 0)
	{
		for ( int i = events.GetCount() - 1; i >= 0; --i )
		{
			if ( time >= events[i]->GetTime())
			{
				if (events[i]->GetEventId() == CUTSCENE_CAMERA_CUT_EVENT)
				{
					return events[i]; 
				}
			}
		}
	}
	return NULL; 
}


void CCutSceneDebugManager::SaveTimeCycleParamsEvent(const cutfObject* pObject)
{
	if(!Verifyf(CutSceneManager::GetInstance()->GetCutfFile(), "Overlay Event: No cutf file object loaded cannot save"))
	{
		return;
	}

	if ( pObject == NULL )
	{
		return;
	}

	if(m_CameraCutsIndexEvents[m_iTimecycleEventsIndexId])
	{
		cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( m_CameraCutsIndexEvents[m_iTimecycleEventsIndexId]->GetEventArgs() );
		if(pEventArgs->GetType() == CUTSCENE_CAMERA_CUT_EVENT_ARGS_TYPE )
		{	
			cutfCameraCutEventArgs* pObjectVar = static_cast<cutfCameraCutEventArgs*>(pEventArgs); 

			if(pObjectVar)
			{				
				pObjectVar->SetReflectionLodRangeStart(ValidateTimeCycleParams(m_ReflectionLodRangeStart)); 
				pObjectVar->SetReflectionLodRangeEnd(ValidateTimeCycleParams(m_ReflectionLodRangeEnd));
				pObjectVar->SetReflectionSLodRangeStart(ValidateTimeCycleParams(m_ReflectionSLodRangeStart));
				pObjectVar->SetReflectionSLodRangeEnd(ValidateTimeCycleParams(m_ReflectionSLodRangeEnd)); 
				pObjectVar->SetLodMultHD(ValidateTimeCycleParams(m_LodMultHD));
				pObjectVar->SetLodMultOrphanHD(ValidateTimeCycleParams(m_LodMultOrphanedHD)); 
				pObjectVar->SetLodMultLod(ValidateTimeCycleParams(m_LodMultLod)); 
				pObjectVar->SetLodMultSlod1(ValidateTimeCycleParams(m_LodMultSLod1)); 
				pObjectVar->SetLodMultSlod2(ValidateTimeCycleParams(m_LodMultSLod2)); 
				pObjectVar->SetLodMultSlod3(ValidateTimeCycleParams(m_LodMultSLod3)); 
				pObjectVar->SetLodMultSlod4(ValidateTimeCycleParams(m_LodMultSLod4)); 
				pObjectVar->SetWaterReflectionFarClip(ValidateTimeCycleParams(m_WaterReflectionFarClip)); 
				pObjectVar->SetLightSSAOInten(ValidateTimeCycleParams(m_SSAOLightInten));
				pObjectVar->SetMapLodScale(ValidateTimeCycleParams(m_LodMult));
				pObjectVar->SetExposurePush(m_ExposurePush);
				pObjectVar->SetDisableHighQualityDof(m_bDisableHighQualityDof);
				pObjectVar->SetCharacterLightParams(m_CharacterLight);
				pObjectVar->SetReflectionMapFrozen(m_FreezeReflectionMap); 
				pObjectVar->SetDirectionalLightDisabled(m_DisableDirectionalLight); 
				pObjectVar->SetLightFadeDistanceMult(m_LightFadeDistanceMult); 
				pObjectVar->SetLightShadowFadeDistanceMult(m_LightShadowFadeDistanceMult); 
				pObjectVar->SetLightSpecularFadeDistMult(m_LightSpecularFadeDistMult); 
				pObjectVar->SetLightVolumetricFadeDistanceMult(m_LightVolumetricFadeDistanceMult); 
				pObjectVar->SetAbsoluteIntensityEnabled(m_AbsoluteIntensityEnabled);
				pObjectVar->SetDirectionalLightMultiplier(ValidateTimeCycleParams(m_DirectionalLightMultiplier));
				pObjectVar->SetLensArtefactMultiplier(ValidateTimeCycleParams(m_LensArtefactMultiplier));
				pObjectVar->SetBloomMax(ValidateTimeCycleParams(m_MaxBloom)); 
			}
		}
	}
}

void CCascadeShadowBoundsDebug::Reset()
{
	//rag control for commands
	 m_bSetCasCadeBoundsDepthBias = false;
	 m_bSetCasCadeBoundsSlopeBias = false;
	 m_bSetCasCadeBoundsShadowSample = false;

	//Set cascade bounds
	m_CascadeIndex = 0;
	m_bCascadeBoundsEnabled = false; 
	m_CascadeBoundsPosition = VEC3_ZERO;
	m_fCascadeBoundsScaleRadius = 0.0f; 
	m_EnableEntityTracker = false; 
	m_bEnableAirCraftMode = false; 
	m_bEnableDynamicDepthMode = false; 
	m_bEnableFlyCamMode = false; 

	//hFov 
	m_fBoundsHFov = 0.0f; 
	
	//vFov 
	m_fBoundsVFov = 0.0f; 
	m_fBoundScale = 0.0f; 
	m_fTrackerScale = 0.0f; 
	m_fSplitZExpWeight = 0.0f; 

	//World Height Update
	m_bEnableWorldHeightUpdate = false; 

	//World Height
	m_fWorldHeightMin = -20.0f; 
	m_fWorldHeightMax = 800.0f; 

	//Receiver Height Update
	m_bEnableReceiverHeightUpdate = false; 

	//Receiver Height
	 m_fReceiveHeightMin = -20.0f; 
	 m_fReceiveHeightMax = 800.0f; 

	//Dither Scale
	 m_fDitherScaleRadius = 1.0f; 
	
	//Depth Bias
	m_bEnableDepthBias = false; 
	m_fDepthBias = 0.0f; 

	//Slope Bias
	m_bEnableSlopeBias = false; 
	m_fSlopeBias = 0.0f; 

	//Shadow Sample
	//m_ShadowSample;

	m_ShadowSampleIndex = 0; 

	m_CascadeShadowsBoundEventsIndexId = 0; 
	m_pCascadeShadowsBoundEventsCombo = NULL;
	m_pCascadeShadowsBoundPropertyEventsCombo = NULL; 
	m_CascadeShadowsBoundPropertyEventsIndexId = 0; 
	m_pCascadeShadowSampleEventsCombo = NULL; 
	m_pShadowFrameSlider = NULL; 
	m_UseCameraCutAsParent = true; 

	m_CascadeShadowsBoundIndexEvents.Reset(); 
}

void CCutSceneDebugManager::BankSaveLoadInteriorCallBack()
{
	
	if (m_pAssetManagerList.GetCount() > 0)
	{
		const cutfObject* pObject = m_pAssetManagerList[0]; 

		if ( (CutSceneManager::GetInstance()->IsPaused() || CutSceneManager::GetInstance()->IsPlaying()) && pObject)
		{
			//Save the ped variation into the event list
			SaveInteriorLoadEvent(pObject, CutSceneCustomEvents::CUTSCENE_LOAD_INTERIOR_EVENT); 

			//update the combo box of ped variation events
			PopulateInteriorLoadEventList(pObject); 

			//Save the changes out to the cuttune file.
			CutSceneManager::GetInstance()->SaveCutfile(); 
		}
	}
}

void CCutSceneDebugManager::SaveInteriorLoadEvent(const cutfObject* pObject,  s32 UNUSED_PARAM(EventId))
{
	if(!Verifyf(CutSceneManager::GetInstance()->GetCutfFile(), "SaveInteriorLoadEvent: No cutf file object loaded cannot save"))
	{
		return;
	}

	if ( pObject == NULL )
	{
		return;
	}

	atArray<cutfEvent *> EventList;
	CutSceneManager::GetInstance()->GetCutfFile()->FindEventsForObjectIdOnly( pObject->GetObjectId(), CutSceneManager::GetInstance()->GetCutfFile()->GetLoadEventList(), EventList );

	bool bAddNewEvent = true; 

	if(bAddNewEvent)
	{
		parAttributeList* pList = rage_new parAttributeList(); 

		pList->AddAttribute("InteriorName", m_InteriorName); 
		pList->AddAttribute("InteriorPosX", m_InteriorPos.x);
		pList->AddAttribute("InteriorPosY", m_InteriorPos.y); 
		pList->AddAttribute("InteriorPosZ", m_InteriorPos.z); 

		cutfEventArgs* pEventArgs = rage_new cutfEventArgs(*pList); 

#if	CUTSCENE_EXPORT_EVENTS_WITH_ZERO_BASE_TIME
		float ftime = CutSceneManager::GetInstance()->GetCutSceneTimeWithRangeOffset();  
#else
		float ftime =CutSceneManager::GetInstance()-> GetCutSceneCurrentTime(); 
#endif
		CutSceneManager::GetInstance()->GetCutfFile()->AddLoadEvent( rage_new cutfObjectIdEvent( pObject->GetObjectId(), ftime, CutSceneCustomEvents::CUTSCENE_LOAD_INTERIOR_EVENT, pEventArgs ) );
		CutSceneManager::GetInstance()->GetCutfFile()->SortLoadEvents();
	}
}

void CCutSceneDebugManager::PopulateInteriorLoadEventList(const cutfObject* pObject)
{
	if (m_pLoadInteriorIndexCombo == NULL)
	{
		return; 
	}

	//Nothing selected
	if(!pObject)
	{
		m_pLoadInteriorIndexCombo->UpdateCombo( "events", &m_LoadInteriorIndexId, 1, NULL );
		m_pLoadInteriorIndexCombo->SetString( 0 , "none");
		return; 
	}

	atArray<cutfEvent *> AllEventList;
	CutSceneManager::GetInstance()->GetCutfFile()->FindEventsForObjectIdOnly( pObject->GetObjectId(), CutSceneManager::GetInstance()->GetCutfFile()->GetLoadEventList(), AllEventList );

	atArray<cutfEvent *>EventList; 

	for(int i =0; i < AllEventList.GetCount(); i++)
	{
		//cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( AllEventList[i]->GetEventArgs() );

		if(AllEventList[i]->GetEventId() == CutSceneCustomEvents::CUTSCENE_LOAD_INTERIOR_EVENT)
		{
			EventList.PushAndGrow(AllEventList[i]); 
		}
	}

	//look to edit a current event 
	if (EventList.GetCount() == 0)
	{
		m_pLoadInteriorIndexCombo->UpdateCombo( "Load Interior Index Events", &m_LoadInteriorIndexId, 1, NULL );
		m_pLoadInteriorIndexCombo->SetString( 0 , "none");
	}
	else
	{
		if (EventList.GetCount() > 0)
		{
			m_pLoadInteriorIndexCombo->UpdateCombo( "Load Interior Index Events", &m_LoadInteriorIndexId, EventList.GetCount(), NULL );

			//Clear the list of variations, need to repopulate. 
			for (int i=0; i < m_LoadInteriorIndexEvents.GetCount(); i++)
			{
				m_LoadInteriorIndexEvents[i] = NULL;  
			}

			m_LoadInteriorIndexEvents.Reset(); 

			for ( int i = 0; i < EventList.GetCount(); ++i )
			{
				if (EventList[i]->GetEventId() == CutSceneCustomEvents::CUTSCENE_LOAD_INTERIOR_EVENT)
				{
					cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( EventList[i]->GetEventArgs() );
					
					char InteriorName[64]; 				
					Vector3 InteriorPos = VEC3_ZERO; 

					InteriorPos.x =	pEventArgs->GetAttributeList().FindAttributeFloatValue("InteriorPosX", 0.0f, false);
					InteriorPos.y =	pEventArgs->GetAttributeList().FindAttributeFloatValue("InteriorPosY", 0.0f, false);
					InteriorPos.z =	pEventArgs->GetAttributeList().FindAttributeFloatValue("InteriorPosZ", 0.0f, false);
					
					char EventInfo[128]; 	
					EventInfo[0] = '\0'; 

					s32 FrameNumber = static_cast<s32>(EventList[i]->GetTime() * CUTSCENE_FPS); 
				
					formatf(EventInfo, sizeof(EventInfo)-1,"(%d) Load Interior: %s (Pos: %f,%f,%f)", 
						FrameNumber,
						pEventArgs->GetAttributeList().FindAttributeStringValue("InteriorName", "",InteriorName, 64), 
						InteriorPos.x, 
						InteriorPos.y, 
						InteriorPos.z); 

					m_pLoadInteriorIndexCombo->SetString( i, EventInfo );

					//Store a list of pointers to our objects
					m_LoadInteriorIndexEvents.PushAndGrow(EventList[i]); 
			}
			}
		}
	}
}

void CCutSceneDebugManager::BankDeleteLoadInteriorsCallBack()
{
	if (m_LoadInteriorIndexEvents.GetCount() > 0)
	{
		if(m_LoadInteriorIndexEvents[m_LoadInteriorIndexId])
		{
			CutSceneManager::GetInstance()->GetCutfFile()->RemoveLoadEvent(m_LoadInteriorIndexEvents[m_LoadInteriorIndexId]);
			PopulateInteriorLoadEventList(m_pAssetManagerList[0]); 
			CutSceneManager::GetInstance()->SaveCutfile(); 
		}
	}
}



void CCascadeShadowBoundsDebug::InitWidgets()
{
#if !SETUP_CUTSCENE_WIDGET_FOR_CONTENT_CONTROLLED_BUILD 

	bkBank* Ragebank = BANKMGR.FindBank("Cut Scene Debug");
	bkGroup* CurrentSceneGrp = static_cast<bkGroup*>(BANKMGR.FindWidget("Cut Scene Debug/Current Scene/Camera Editing"));
	if(Ragebank)
	{
		Ragebank->SetCurrentGroup(*CurrentSceneGrp);
			Ragebank->PushGroup("Cascade shadow"); 
				CutSceneManager::GetInstance()->CreateVCRWidget(Ragebank);
				m_pShadowFrameSlider = Ragebank->AddSlider( "Current Frame", &CutSceneManager::GetInstance()->m_iCurrentFrameWithFrameRanges, 0, MAX_FRAMES, 1, 
				datCallback(MFA(CutSceneManager::BankFrameScrubbingCallback),CutSceneManager::GetInstance()) );
				const char *List[] = { "(none)" };
				m_pCascadeShadowsBoundEventsCombo = dynamic_cast<bkCombo *>( Ragebank->AddCombo( "Shadow Events", &m_CascadeShadowsBoundEventsIndexId, 1, List, datCallback(MFA(CCascadeShadowBoundsDebug::UpdateShadowBoxCombo), (datBase*)this)));				 
				Ragebank->AddText("Current Cut:",&CutSceneManager::GetInstance()->GetDebugManager().m_CameraCutName[0], 32, true);

				Ragebank->AddButton("Refresh Events",datCallback(MFA1(CCascadeShadowBoundsDebug::PopulateSetupShadowBoundsEventList), (datBase*)this));

				Ragebank->PushGroup("SET_CASCADE_BOUNDS (Recommended Syncing to Cut Time)"); 
					Ragebank->AddSlider("Cascade Index", &m_CascadeIndex, 0, 3, 1); 
					Ragebank->AddToggle("Enable", &m_bCascadeBoundsEnabled); 
					Ragebank->AddSlider("Position.x", &m_CascadeBoundsPosition.x, -10000.0f, 10000.0f, 1.0f); 
					Ragebank->AddSlider("Position.y", &m_CascadeBoundsPosition.y, -10000.0f, 10000.0f, 1.0f); 
					Ragebank->AddSlider("Position.z", &m_CascadeBoundsPosition.z, -10000.0f, 10000.0f, 1.0f); 
					Ragebank->AddSlider("Scale Radius", &m_fCascadeBoundsScaleRadius, 0.0f, 9999.0f, 1.0f/32.0f);
					Ragebank->AddButton("Get From Script Cascade Manual Control", datCallback(MFA(CCascadeShadowBoundsDebug::BankGetFromScriptCascadeManualControl), (datBase*)this));
					Ragebank->AddButton("Send To Script Cascade Manual Control", datCallback(MFA(CCascadeShadowBoundsDebug::BankSendToScriptCascadeManualControl), (datBase*)this));
					Ragebank->AddButton("Save Set Shadow Bounds Event", datCallback(MFA1(CCascadeShadowBoundsDebug::BankSaveSetShadowBoundsEventCallBack), (datBase*)this, (CallbackData)CUTSCENE_ENABLE_CASCADE_SHADOW_BOUNDS_EVENT));
					Ragebank->AddButton("Delete Set Shadow Bounds Event", datCallback(MFA(CCascadeShadowBoundsDebug::BankDeleteSetShadowBoundsEventCallBack), (datBase*)this));
				Ragebank->PopGroup(); 
				
				Ragebank->PushGroup("CUTSCENE_CASCADE_SHADOWS_RESET_CASCADE_SHADOWS");
					Ragebank->AddButton("Save RESET_CASCADE_SHADOWS Event", datCallback(MFA1(CCascadeShadowBoundsDebug::BankSaveSetShadowBoundsEventCallBack), (datBase*)this, (CallbackData)CUTSCENE_CASCADE_SHADOWS_RESET_CASCADE_SHADOWS));
					Ragebank->AddButton("Delete RESET_CASCADE_SHADOWS Event", datCallback(MFA(CCascadeShadowBoundsDebug::BankDeleteSetShadowBoundsEventCallBack), (datBase*)this));
				Ragebank->PopGroup(); 

				Ragebank->PushGroup("CASCADE_SHADOWS_ENABLE_ENTITY_TRACKER"); 
					Ragebank->AddToggle("Set ENABLE_ENTITY_TRACKER Active", &m_EnableEntityTracker); 
					Ragebank->AddButton("Save ENABLE_ENTITY_TRACKER Event", datCallback(MFA1(CCascadeShadowBoundsDebug::BankSaveSetShadowBoundsEventCallBack), (datBase*)this, (CallbackData)CUTSCENE_CASCADE_SHADOWS_ENABLE_ENTITY_TRACKER));
					Ragebank->AddButton("Delete ENABLE_ENTITY_TRACKER Event", datCallback(MFA(CCascadeShadowBoundsDebug::BankDeleteSetShadowBoundsEventCallBack), (datBase*)this));
				Ragebank->PopGroup(); 

				Ragebank->PushGroup("CASCADE_SHADOWS_SET_WORLD_HEIGHT_UPDATE"); 
					Ragebank->AddToggle("set SET_WORLD_HEIGHT_UPDATE Active", &m_bEnableWorldHeightUpdate); 
					Ragebank->AddButton("Save SET_WORLD_HEIGHT_UPDATE Event", datCallback(MFA1(CCascadeShadowBoundsDebug::BankSaveSetShadowBoundsEventCallBack), (datBase*)this, (CallbackData)CUTSCENE_CASCADE_SHADOWS_SET_WORLD_HEIGHT_UPDATE));
					Ragebank->AddButton("Delete SET_WORLD_HEIGHT_UPDATE Event", datCallback(MFA(CCascadeShadowBoundsDebug::BankDeleteSetShadowBoundsEventCallBack), (datBase*)this));
				Ragebank->PopGroup(); 

				Ragebank->PushGroup("CASCADE_SHADOWS_SET_RECEIVER_HEIGHT_UPDATE"); 
					Ragebank->AddToggle("Set SET_RECEIVER_HEIGHT_UPDATE Active", &m_bEnableReceiverHeightUpdate); 
					Ragebank->AddButton("Save SET_RECEIVER_HEIGHT_UPDATE Event", datCallback(MFA1(CCascadeShadowBoundsDebug::BankSaveSetShadowBoundsEventCallBack), (datBase*)this, (CallbackData)CUTSCENE_CASCADE_SHADOWS_SET_RECEIVER_HEIGHT_UPDATE));
					Ragebank->AddButton("Delete SET_RECEIVER_HEIGHT_UPDATE Event", datCallback(MFA(CCascadeShadowBoundsDebug::BankDeleteSetShadowBoundsEventCallBack), (datBase*)this));
				Ragebank->PopGroup(); 
				
				Ragebank->PushGroup("CASCADE_SHADOWS_SET_AIRCRAFT_MODE"); 
					Ragebank->AddToggle("set SET_AIRCRAFT_MODE Active", &m_bEnableAirCraftMode); 
					Ragebank->AddButton("Save SET_AIRCRAFT_MODE Event", datCallback(MFA1(CCascadeShadowBoundsDebug::BankSaveSetShadowBoundsEventCallBack), (datBase*)this, (CallbackData)CUTSCENE_CASCADE_SHADOWS_SET_AIRCRAFT_MODE));
					Ragebank->AddButton("Delete SET_AIRCRAFT_MODE Event", datCallback(MFA(CCascadeShadowBoundsDebug::BankDeleteSetShadowBoundsEventCallBack), (datBase*)this));
				Ragebank->PopGroup();
				
				Ragebank->PushGroup("CASCADE_SHADOWS_SET_DYNAMIC_DEPTH_MODE"); 
					Ragebank->AddToggle("Set SET_DYNAMIC_DEPTH_MODE Active", &m_bEnableDynamicDepthMode); 
					Ragebank->AddButton("Save SET_DYNAMIC_DEPTH_MODE Event", datCallback(MFA1(CCascadeShadowBoundsDebug::BankSaveSetShadowBoundsEventCallBack), (datBase*)this, (CallbackData)CUTSCENE_CASCADE_SHADOWS_SET_DYNAMIC_DEPTH_MODE));
					Ragebank->AddButton("Delete SET_DYNAMIC_DEPTH_MODE Event", datCallback(MFA(CCascadeShadowBoundsDebug::BankDeleteSetShadowBoundsEventCallBack), (datBase*)this));
				Ragebank->PopGroup();
				
				Ragebank->PushGroup("CASCADE_SHADOWS_SET_FLY_CAMERA_MODE"); 
					Ragebank->AddToggle("Set SET_FLY_CAMERA_MODE Active", &m_bEnableFlyCamMode); 
					Ragebank->AddButton("Save SET_FLY_CAMERA_MODE Event", datCallback(MFA1(CCascadeShadowBoundsDebug::BankSaveSetShadowBoundsEventCallBack), (datBase*)this, (CallbackData)CUTSCENE_CASCADE_SHADOWS_SET_FLY_CAMERA_MODE));
					Ragebank->AddButton("Delete SET_FLY_CAMERA_MODE Event", datCallback(MFA(CCascadeShadowBoundsDebug::BankDeleteSetShadowBoundsEventCallBack), (datBase*)this));
				Ragebank->PopGroup();
				
				Ragebank->PushGroup("CUTSCENE_CASCADE_SHADOWS_SET_CASCADE_BOUNDS_HFOV"); 
					Ragebank->AddSlider("SET_CASCADE_BOUNDS_HFOV", &m_fBoundsHFov, -10000.0f, 10000.0f, 1.0f); 
					Ragebank->AddButton("Save SET_CASCADE_BOUNDS_HFOV Event", datCallback(MFA1(CCascadeShadowBoundsDebug::BankSaveSetShadowBoundsEventCallBack), (datBase*)this, (CallbackData)CUTSCENE_CASCADE_SHADOWS_SET_CASCADE_BOUNDS_HFOV));
					Ragebank->AddButton("Delete SET_CASCADE_BOUNDS_HFOV Event", datCallback(MFA(CCascadeShadowBoundsDebug::BankDeleteSetShadowBoundsEventCallBack), (datBase*)this));
				Ragebank->PopGroup();

				Ragebank->PushGroup("CUTSCENE_CASCADE_SHADOWS_SET_CASCADE_BOUNDS_VFOV"); 
					Ragebank->AddSlider("SET_CASCADE_BOUNDS_VFOV", &m_fBoundsVFov, -10000.0f, 10000.0f, 1.0f); 
					Ragebank->AddButton("Save SET_CASCADE_BOUNDS_VFOV Event", datCallback(MFA1(CCascadeShadowBoundsDebug::BankSaveSetShadowBoundsEventCallBack), (datBase*)this, (CallbackData)CUTSCENE_CASCADE_SHADOWS_SET_CASCADE_BOUNDS_VFOV));
					Ragebank->AddButton("Delete SET_CASCADE_BOUNDS_VFOV Event", datCallback(MFA(CCascadeShadowBoundsDebug::BankDeleteSetShadowBoundsEventCallBack), (datBase*)this));
				Ragebank->PopGroup();
	
				Ragebank->PushGroup("CUTSCENE_CASCADE_SHADOWS_SET_CASCADE_BOUNDS_SCALE"); 
					Ragebank->AddSlider("SET_CASCADE_BOUNDS_SCALE", &m_fBoundScale, -10000.0f, 10000.0f, 1.0f); 
					Ragebank->AddButton("Save SET_CASCADE_BOUNDS_SCALE Event", datCallback(MFA1(CCascadeShadowBoundsDebug::BankSaveSetShadowBoundsEventCallBack), (datBase*)this, (CallbackData)CUTSCENE_CASCADE_SHADOWS_SET_CASCADE_BOUNDS_SCALE));
					Ragebank->AddButton("Delete SET_CASCADE_BOUNDS_SCALE Event", datCallback(MFA(CCascadeShadowBoundsDebug::BankDeleteSetShadowBoundsEventCallBack), (datBase*)this));
				Ragebank->PopGroup();
				
				Ragebank->PushGroup("CUTSCENE_CASCADE_SHADOWS_SET_ENTITY_TRACKER_SCALE"); 
					Ragebank->AddSlider("SET_ENTITY_TRACKER_SCALE", &m_fTrackerScale, -10000.0f, 10000.0f, 1.0f); 
					Ragebank->AddButton("Save SET_ENTITY_TRACKER_SCALE Event", datCallback(MFA1(CCascadeShadowBoundsDebug::BankSaveSetShadowBoundsEventCallBack), (datBase*)this, (CallbackData)CUTSCENE_CASCADE_SHADOWS_SET_ENTITY_TRACKER_SCALE));
					Ragebank->AddButton("Delete SET_ENTITY_TRACKER_SCALE Event", datCallback(MFA(CCascadeShadowBoundsDebug::BankDeleteSetShadowBoundsEventCallBack), (datBase*)this));
				Ragebank->PopGroup();
				
				Ragebank->PushGroup("CUTSCENE_CASCADE_SHADOWS_SET_SPLIT_Z_EXP_WEIGHT"); 
					Ragebank->AddSlider("SET_SPLIT_Z_EXP_WEIGHT", &m_fSplitZExpWeight, -10000.0f, 10000.0f, 1.0f); 
					Ragebank->AddButton("Save SET_SPLIT_Z_EXP_WEIGHT Event", datCallback(MFA1(CCascadeShadowBoundsDebug::BankSaveSetShadowBoundsEventCallBack), (datBase*)this, (CallbackData)CUTSCENE_CASCADE_SHADOWS_SET_SPLIT_Z_EXP_WEIGHT));
					Ragebank->AddButton("Delete SET_SPLIT_Z_EXP_WEIGHT Event", datCallback(MFA(CCascadeShadowBoundsDebug::BankDeleteSetShadowBoundsEventCallBack), (datBase*)this));
				Ragebank->PopGroup();

				Ragebank->PushGroup("CUTSCENE_CASCADE_SHADOWS_SET_DITHER_RADIUS_SCALE"); 
					Ragebank->AddSlider("SET_DITHER_RADIUS_SCALE", &m_fDitherScaleRadius, -10000.0f, 10000.0f, 1.0f); 
					Ragebank->AddButton("Save SET_DITHER_RADIUS_SCALE Event", datCallback(MFA1(CCascadeShadowBoundsDebug::BankSaveSetShadowBoundsEventCallBack), (datBase*)this, (CallbackData)CUTSCENE_CASCADE_SHADOWS_SET_DITHER_RADIUS_SCALE));
					Ragebank->AddButton("Delete SET_DITHER_RADIUS_SCALE Event", datCallback(MFA(CCascadeShadowBoundsDebug::BankDeleteSetShadowBoundsEventCallBack), (datBase*)this));
				Ragebank->PopGroup();

				Ragebank->PushGroup("CUTSCENE_CASCADE_SHADOWS_SET_WORLD_HEIGHT_MINMAX"); 
					Ragebank->AddSlider("SET_WORLD_HEIGHT_MIN", &m_fWorldHeightMin, -10000.0f, 10000.0f, 1.0f); 
					Ragebank->AddSlider("SET_WORLD_HEIGHT_MAX", &m_fWorldHeightMax, -10000.0f, 10000.0f, 1.0f); 
					Ragebank->AddButton("Save SET_WORLD_HEIGHT_MINMAX Event", datCallback(MFA1(CCascadeShadowBoundsDebug::BankSaveSetShadowBoundsEventCallBack), (datBase*)this, (CallbackData)CUTSCENE_CASCADE_SHADOWS_SET_WORLD_HEIGHT_MINMAX));
					Ragebank->AddButton("Delete SET_WORLD_HEIGHT_MINMAX Event", datCallback(MFA(CCascadeShadowBoundsDebug::BankDeleteSetShadowBoundsEventCallBack), (datBase*)this));
				Ragebank->PopGroup();

				Ragebank->PushGroup("CUTSCENE_CASCADE_SHADOWS_SET_RECEIVER_HEIGHT_MINMAX"); 
					Ragebank->AddSlider("SET_RECEIVER_HEIGHT_MIN", &m_fReceiveHeightMin, -10000.0f, 10000.0f, 1.0f); 
					Ragebank->AddSlider("SET_RECEIVER_HEIGHT_MAX", &m_fReceiveHeightMax, -10000.0f, 10000.0f, 1.0f);
						Ragebank->AddButton("Save SET_RECEIVER_HEIGHT_MINMAX Event ", datCallback(MFA1(CCascadeShadowBoundsDebug::BankSaveSetShadowBoundsEventCallBack), (datBase*)this, (CallbackData)CUTSCENE_CASCADE_SHADOWS_SET_RECEIVER_HEIGHT_MINMAX));
					Ragebank->AddButton("Delete SET_RECEIVER_HEIGHT_MINMAX Event", datCallback(MFA(CCascadeShadowBoundsDebug::BankDeleteSetShadowBoundsEventCallBack), (datBase*)this));
				Ragebank->PopGroup();
				
				Ragebank->PushGroup("CUTSCENE_CASCADE_SHADOWS_SET_DEPTH_BIAS"); 
					Ragebank->AddToggle("OVERRIDE_DEPTH_BIAS", &m_bEnableDepthBias); 
					Ragebank->AddSlider("SET_DEPTH_BIAS", &m_fDepthBias, -10000.0f, 10000.0f, 1.0f);
					Ragebank->AddButton("Save SET_DEPTH_BIAS Event", datCallback(MFA1(CCascadeShadowBoundsDebug::BankSaveSetShadowBoundsEventCallBack), (datBase*)this, (CallbackData)CUTSCENE_CASCADE_SHADOWS_SET_DEPTH_BIAS));
					Ragebank->AddButton("Delete SET_DEPTH_BIAS Event", datCallback(MFA(CCascadeShadowBoundsDebug::BankDeleteSetShadowBoundsEventCallBack), (datBase*)this));
				Ragebank->PopGroup();
				
				Ragebank->PushGroup("CUTSCENE_CASCADE_SHADOWS_SET_SLOPE_BIAS"); 
					Ragebank->AddToggle("OVERRIDE_SLOPE_BIAS", &m_bEnableSlopeBias); 
					Ragebank->AddSlider("SET_SLOPE_BIAS", &m_fSlopeBias, -10000.0f, 10000.0f, 1.0f);
					Ragebank->AddButton("Save SET_SLOPE_BIAS Event", datCallback(MFA1(CCascadeShadowBoundsDebug::BankSaveSetShadowBoundsEventCallBack), (datBase*)this, (CallbackData)CUTSCENE_CASCADE_SHADOWS_SET_SLOPE_BIAS));
					Ragebank->AddButton("Delete SET_SLOPE_BIAS Event", datCallback(MFA(CCascadeShadowBoundsDebug::BankDeleteSetShadowBoundsEventCallBack), (datBase*)this));
				Ragebank->PopGroup();

				Ragebank->PushGroup("CUTSCENE_CASCADE_SHADOWS_SET_SHADOW_SAMPLE_TYPE"); 
					m_pCascadeShadowSampleEventsCombo = dynamic_cast<bkCombo *>( Ragebank->AddCombo( "SHADOW_SAMPLE_TYPE", &m_ShadowSampleIndex, MAX_NUMBER_SHADOW_TYPES, ShadowSampleTypes ) );
					Ragebank->AddButton("Save SET_SHADOW_SAMPLE_TYPE Event", datCallback(MFA1(CCascadeShadowBoundsDebug::BankSaveSetShadowBoundsEventCallBack), (datBase*)this, (CallbackData)CUTSCENE_CASCADE_SHADOWS_SET_SHADOW_SAMPLE_TYPE));
					Ragebank->AddButton("Delete SET_SHADOW_SAMPLE_TYPE Event", datCallback(MFA(CCascadeShadowBoundsDebug::BankDeleteSetShadowBoundsEventCallBack), (datBase*)this));
				Ragebank->PopGroup();

				Ragebank->PushGroup("Event Parenting"); 
					Ragebank->AddToggle("Set Event Time To Current Camera Cut Event Time", &m_UseCameraCutAsParent); 
				Ragebank->PopGroup();
			Ragebank->PopGroup(); 
		Ragebank->UnSetCurrentGroup(*CurrentSceneGrp); 

	}
#endif
}

void CCascadeShadowBoundsDebug::DestroyWidgets()
{
	m_pShadowFrameSlider = NULL;
	m_pCascadeShadowsBoundEventsCombo = NULL;		 
	m_pCascadeShadowSampleEventsCombo = NULL;
}

void CCascadeShadowBoundsDebug::BankGetFromScriptCascadeManualControl()
{
	if (CutSceneManager::GetInstance()->IsPaused())
	{
		const cutfObject* pObject = CutSceneManager::GetInstance()->GetDebugManager().m_CameraObjectList[0]; 
		if (pObject)
		{
#if	CUTSCENE_EXPORT_EVENTS_WITH_ZERO_BASE_TIME
			float fTime = CutSceneManager::GetInstance()->GetCutSceneTimeWithRangeOffset();  
#else
			float fTime = CutSceneManager::GetInstance()->GetCutSceneCurrentTime(); 
#endif

			float fEventTime = fTime; 

			atArray<cutfEvent *> EventList;
			CutSceneManager::GetInstance()->GetCutfFile()->FindEventsForObjectIdOnly(pObject->GetObjectId(), CutSceneManager::GetInstance()->GetCutfFile()->GetEventList(), EventList);

			s32 EventIndex; 
			atHashString cameraCutHash = CutSceneManager::GetInstance()->GetDebugManager().FindCameraCutNameForTime(EventList, CutSceneManager::GetInstance()-> GetCutSceneCurrentTime(), EventIndex); 

			if(m_UseCameraCutAsParent)
			{
				fEventTime = CutSceneManager::GetInstance()->GetDebugManager().FindCameraCutTimeForTime(EventList, fTime); 

				if(CutSceneManager::GetInstance()->GetCutSceneCurrentTime() == CutSceneManager::GetInstance()->GetEndTime())
				{
					fEventTime = CutSceneManager::GetInstance()->GetEndTime(); 
					cameraCutHash.Clear(); 
					cameraCutHash.SetFromString("endofscenecut"); 
				}
				else
				{
					fEventTime = CutSceneManager::GetInstance()->GetDebugManager().FindCameraCutTimeForTime(EventList, fTime); 
				}
			}

			/* Delete existing events */

			for (int i = 0; i < EventList.GetCount(); i ++)
			{
				cutfEvent *pEvent = EventList[i];
				if (pEvent && pEvent->GetEventId() == CUTSCENE_ENABLE_CASCADE_SHADOW_BOUNDS_EVENT)
				{
					const cutfEventArgs *pEventArgs = pEvent->GetEventArgs();
					if (pEventArgs && pEventArgs->GetType() == CUTSCENE_CASCADE_SHADOW_EVENT_ARGS)
					{
						const cutfCascadeShadowEventArgs* pShadowEventArgs = static_cast< const cutfCascadeShadowEventArgs * >(pEventArgs); 

						if (pShadowEventArgs->GetCameraCutHash() == cameraCutHash)
						{
							CutSceneManager::GetInstance()->GetCutfFile()->RemoveEvent(pEvent);
						}
					}
				}
			}

			/* Create new events */

			for (int iCascadeIndex = 0; iCascadeIndex < CASCADE_SHADOWS_COUNT; iCascadeIndex ++)
			{
				bool bIsEnabled = false;
				float fCascadeBoundsPositionX = 0.0f, fCascadeBoundsPositionY = 0.0f, fCascadeBoundsPositionZ = 0.0f;
				float fCascadeBoundsScaleRadius = 0.0f;
				bool bLerpToDisabled = false;
				float fLerpTime = 0.0f;

				CRenderPhaseCascadeShadowsInterface::Script_GetCascadeBounds(iCascadeIndex, bIsEnabled, fCascadeBoundsPositionX, fCascadeBoundsPositionY, fCascadeBoundsPositionZ, fCascadeBoundsScaleRadius, bLerpToDisabled, fLerpTime);

				Vector3 vCascadeBoundsPosition(fCascadeBoundsPositionX, fCascadeBoundsPositionY, fCascadeBoundsPositionZ);

				if (bIsEnabled)
				{
					cutfEventArgs* pEventArgs = rage_new cutfCascadeShadowEventArgs(cameraCutHash, vCascadeBoundsPosition, fCascadeBoundsScaleRadius, fLerpTime, iCascadeIndex, bIsEnabled, bLerpToDisabled); 

					if (pEventArgs)
					{
						if (m_UseCameraCutAsParent)
						{
							CutSceneManager::GetInstance()->GetCutfFile()->AddEvent(rage_new cutfObjectIdEvent(pObject->GetObjectId(), fEventTime, CUTSCENE_ENABLE_CASCADE_SHADOW_BOUNDS_EVENT, true, cameraCutHash, pEventArgs));
						}
						else
						{
							CutSceneManager::GetInstance()->GetCutfFile()->AddEvent(rage_new cutfObjectIdEvent(pObject->GetObjectId(), fEventTime, CUTSCENE_ENABLE_CASCADE_SHADOW_BOUNDS_EVENT, false, 0, pEventArgs));
						}
					}
				}
			}

			CutSceneManager::GetInstance()->GetCutfFile()->SortEvents();

			//update the combo box of ped variation events
			PopulateSetShadowBoundsEventList(pObject); 

			UpdateShadowBoxCombo();

			//Save the changes out to the cut tune file.
			CutSceneManager::GetInstance()->SaveGameDataCutfile(); 
		}
	}
}

void CCascadeShadowBoundsDebug::BankSendToScriptCascadeManualControl()
{
	if (CutSceneManager::GetInstance()->IsPaused())
	{
		s32 iCascadeShadowIndex = m_CascadeIndex;
		bool bIsEnabled = m_bCascadeBoundsEnabled;
		Vector3 vPosition = m_CascadeBoundsPosition;
		float fRadius = m_fCascadeBoundsScaleRadius;

		CRenderPhaseCascadeShadowsInterface::Script_SetCascadeBounds(iCascadeShadowIndex, bIsEnabled, vPosition.x, vPosition.y, vPosition.z, fRadius, true);
	}
}

void CCascadeShadowBoundsDebug::BankSaveSetShadowBoundsEventCallBack(s32 Event)
{
	if (CutSceneManager::GetInstance()->GetDebugManager().m_CameraObjectList.GetCount() > 0)
	{
		const cutfObject* pObject = CutSceneManager::GetInstance()->GetDebugManager().m_CameraObjectList[0]; 

		if ((CutSceneManager::GetInstance()->IsPaused() || CutSceneManager::GetInstance()->IsPlaying()) && pObject)
		{
			if (Event==CUTSCENE_ENABLE_CASCADE_SHADOW_BOUNDS_EVENT)
			{
				// check if there's a matching shadow bounds event for this cut that we need to delete.

				// first, grab the current camera cut name.
				atArray<cutfEvent *> EventList;
				CutSceneManager::GetInstance()->GetCutfFile()->FindEventsForObjectIdOnly( pObject->GetObjectId(), CutSceneManager::GetInstance()->GetCutfFile()->GetEventList(), EventList );
				s32 EventIndex; 
				atHashString cutHash = CutSceneManager::GetInstance()->GetDebugManager().FindCameraCutNameForTime(EventList, CutSceneManager::GetInstance()-> GetCutSceneCurrentTime(), EventIndex); 

				/* Try to find a matching event for the cascade index and camera cut name */
				for(int i = 0; i < m_CascadeShadowsBoundIndexEvents.GetCount(); i ++)
				{
					cutfEvent *pTempEvent = m_CascadeShadowsBoundIndexEvents[i];
					if(pTempEvent && pTempEvent->GetEventId() == CUTSCENE_ENABLE_CASCADE_SHADOW_BOUNDS_EVENT)
					{
						const cutfEventArgs *pTempEventArgs = pTempEvent->GetEventArgs();
						if(pTempEventArgs && pTempEventArgs->GetType() == CUTSCENE_CASCADE_SHADOW_EVENT_ARGS)
						{
							const cutfCascadeShadowEventArgs *pTempShadowEventArgs = static_cast< const cutfCascadeShadowEventArgs * >(pTempEventArgs);

							if(pTempShadowEventArgs->GetCameraCutHash() == cutHash && pTempShadowEventArgs->GetCascadeShadowIndex() == m_CascadeIndex)
							{
								// delete the matching event (since we need to replace it)
								m_CascadeShadowsBoundEventsIndexId = i;
								BankDeleteSetShadowBoundsEventCallBack();
								break;
							}
						}
					}
				}
			}

			//Save the shadow bounds event into the event list
			SaveSetShadowBoundsEvent(pObject, Event); 

			//update the combo box of shadow bound events
			PopulateSetShadowBoundsEventList(pObject); 

			//Save the changes out to the cut tune file.
			CutSceneManager::GetInstance()->SaveGameDataCutfile(); 
		}
	}
}

void CCascadeShadowBoundsDebug::BankDeleteSetShadowBoundsEventCallBack()
{
	if (m_CascadeShadowsBoundEventsIndexId < m_CascadeShadowsBoundIndexEvents.GetCount() )
	{
		if(CutSceneManager::GetInstance()->GetDebugManager().m_CameraObjectList.GetCount() > 0)
		{
			const cutfObject* pObject = CutSceneManager::GetInstance()->GetDebugManager().m_CameraObjectList[0]; 

			if(m_CascadeShadowsBoundIndexEvents[m_CascadeShadowsBoundEventsIndexId] && pObject)
			{
				cutfEvent* pEvent = m_CascadeShadowsBoundIndexEvents[m_CascadeShadowsBoundEventsIndexId]; 
				if(pEvent)
				{
					CutSceneManager::GetInstance()->GetCutfFile()->RemoveEvent(m_CascadeShadowsBoundIndexEvents[m_CascadeShadowsBoundEventsIndexId]);

					PopulateSetShadowBoundsEventList(pObject); 
					CutSceneManager::GetInstance()->SaveGameDataCutfile(); 
				}
			}
		}
	}
}

void CCascadeShadowBoundsDebug::UpdateCascadeShadowDebugRender(const atHashString &cameraCutHash)
{
	ClearCascadeShadowDebugRender();

	const cutfObject *pObject = CutSceneManager::GetInstance()->GetDebugManager().m_CameraObjectList[0]; 
	if (pObject)
	{
		atArray< cutfEvent * > eventList;
		CutSceneManager::GetInstance()->GetCutfFile()->FindEventsForObjectIdOnly(pObject->GetObjectId(), CutSceneManager::GetInstance()->GetCutfFile()->GetEventList(), eventList);

		for (int i = 0; i < eventList.GetCount(); i ++)
		{
			cutfEvent *pEvent = eventList[i];
			if(pEvent && pEvent->GetEventId() == CUTSCENE_ENABLE_CASCADE_SHADOW_BOUNDS_EVENT)
			{
				const cutfEventArgs *pEventArgs = pEvent->GetEventArgs();
				if(pEventArgs && pEventArgs->GetType() == CUTSCENE_CASCADE_SHADOW_EVENT_ARGS)
				{
					const cutfCascadeShadowEventArgs *pShadowEventArgs = static_cast< const cutfCascadeShadowEventArgs * >(pEventArgs);
					if(pShadowEventArgs && pShadowEventArgs->GetCameraCutHash() == cameraCutHash)
					{
						s32 iCascadeShadowIndex = pShadowEventArgs->GetCascadeShadowIndex();
						bool bIsEnabled = pShadowEventArgs->GetIsEnabled();
						Vector3 vPosition = pShadowEventArgs->GetPosition();
						float fRadius = pShadowEventArgs->GetRadius();

						CRenderPhaseCascadeShadowsInterface::Script_SetCascadeBounds(iCascadeShadowIndex, bIsEnabled, vPosition.x, vPosition.y, vPosition.z, fRadius, true);
					}
				}
			}
		}
	}
}

void CCascadeShadowBoundsDebug::ClearCascadeShadowDebugRender()
{
	for(int iCascadeShadowIndex = 0; iCascadeShadowIndex < CASCADE_SHADOWS_COUNT; iCascadeShadowIndex ++)
	{
		CRenderPhaseCascadeShadowsInterface::Script_SetCascadeBounds(iCascadeShadowIndex, false, 0.0f, 0.0f, 0.0f, 0.0f, true);
	}
}

void CCascadeShadowBoundsDebug::SelectShadowEvent(const atHashString &cameraCutHash)
{
	if(CutSceneManager::GetInstance()->IsPlaying())
	{
		for(int i = 0; i < m_CascadeShadowsBoundIndexEvents.GetCount(); i ++)
		{
			cutfEvent *pEvent = m_CascadeShadowsBoundIndexEvents[i];
			if(pEvent && pEvent->GetEventId() == CUTSCENE_ENABLE_CASCADE_SHADOW_BOUNDS_EVENT)
			{
				const cutfEventArgs *pEventArgs = pEvent->GetEventArgs();
				if(pEventArgs && pEventArgs->GetType() == CUTSCENE_CASCADE_SHADOW_EVENT_ARGS)
				{
					const cutfCascadeShadowEventArgs *pCascadeShadowEventArgs = static_cast< const cutfCascadeShadowEventArgs * >(pEventArgs);

					if(pCascadeShadowEventArgs->GetCameraCutHash() == cameraCutHash)
					{
						m_CascadeShadowsBoundEventsIndexId = i;

						UpdateShadowBoxCombo();

						break;
					}
				}
			}
		}
	}
}

void CCascadeShadowBoundsDebug::UpdateShadowBoxCombo()
{
	if(m_CascadeShadowsBoundEventsIndexId > -1 && m_CascadeShadowsBoundEventsIndexId < m_CascadeShadowsBoundIndexEvents.GetCount())
	{
		cutfEvent* pEvent = m_CascadeShadowsBoundIndexEvents[m_CascadeShadowsBoundEventsIndexId]; 
		if(pEvent)
		{
			s32 iEventId = pEvent->GetEventId(); 
				
			switch(iEventId)
			{
			case CUTSCENE_ENABLE_CASCADE_SHADOW_BOUNDS_EVENT:
				{
					const cutfEventArgs* pEventArgs = pEvent->GetEventArgs(); 

					if(pEventArgs && pEventArgs->GetType() == CUTSCENE_CASCADE_SHADOW_EVENT_ARGS)
					{
						const cutfCascadeShadowEventArgs* pShadowEventArgs = static_cast<const cutfCascadeShadowEventArgs*>(pEventArgs); 
							
						m_CascadeIndex = pShadowEventArgs->GetCascadeShadowIndex(); 

						m_bCascadeBoundsEnabled = pShadowEventArgs->GetIsEnabled();
							
						m_CascadeBoundsPosition = pShadowEventArgs->GetPosition(); 
							
						m_fCascadeBoundsScaleRadius = pShadowEventArgs->GetRadius(); 

						UpdateCascadeShadowDebugRender(pShadowEventArgs->GetCameraCutHash());
					}
				}
				break;

			case CUTSCENE_CASCADE_SHADOWS_ENABLE_ENTITY_TRACKER:
				{
					const cutfEventArgs* pEventArgs = pEvent->GetEventArgs(); 

					if(pEventArgs && pEventArgs->GetType() == CUTSCENE_BOOL_VALUE_EVENT_ARGS_TYPE)
					{
						const cutfBoolValueEventArgs* pShadowEventArgs = static_cast<const cutfBoolValueEventArgs*>(pEventArgs); 

						m_EnableEntityTracker = pShadowEventArgs->GetValue();
					}
				}
				break; 

			case CUTSCENE_CASCADE_SHADOWS_SET_WORLD_HEIGHT_UPDATE:
				{
					const cutfEventArgs* pEventArgs = pEvent->GetEventArgs(); 

					if(pEventArgs && pEventArgs->GetType() == CUTSCENE_BOOL_VALUE_EVENT_ARGS_TYPE)
					{
						const cutfBoolValueEventArgs* pShadowEventArgs = static_cast<const cutfBoolValueEventArgs*>(pEventArgs); 

						m_bEnableWorldHeightUpdate = pShadowEventArgs->GetValue();
					}
						
				}
				break; 

			case CUTSCENE_CASCADE_SHADOWS_SET_RECEIVER_HEIGHT_UPDATE:
				{
					const cutfEventArgs* pEventArgs = pEvent->GetEventArgs(); 

					if(pEventArgs && pEventArgs->GetType() == CUTSCENE_BOOL_VALUE_EVENT_ARGS_TYPE)
					{
						const cutfBoolValueEventArgs* pShadowEventArgs = static_cast<const cutfBoolValueEventArgs*>(pEventArgs); 

						m_bEnableReceiverHeightUpdate = pShadowEventArgs->GetValue();
					}
		
				}
				break; 
			case CUTSCENE_CASCADE_SHADOWS_SET_AIRCRAFT_MODE:
				{
					const cutfEventArgs* pEventArgs = pEvent->GetEventArgs(); 

					if(pEventArgs && pEventArgs->GetType() == CUTSCENE_BOOL_VALUE_EVENT_ARGS_TYPE)
					{
						const cutfBoolValueEventArgs* pShadowEventArgs = static_cast<const cutfBoolValueEventArgs*>(pEventArgs); 

						m_bEnableAirCraftMode = pShadowEventArgs->GetValue();
					}

				}
				break; 

			case CUTSCENE_CASCADE_SHADOWS_SET_DYNAMIC_DEPTH_MODE:
				{
					const cutfEventArgs* pEventArgs = pEvent->GetEventArgs(); 

					if(pEventArgs && pEventArgs->GetType() == CUTSCENE_BOOL_VALUE_EVENT_ARGS_TYPE)
					{
						const cutfBoolValueEventArgs* pShadowEventArgs = static_cast<const cutfBoolValueEventArgs*>(pEventArgs); 

						m_bEnableDynamicDepthMode = pShadowEventArgs->GetValue();
					}

				}
				break; 

			case CUTSCENE_CASCADE_SHADOWS_SET_FLY_CAMERA_MODE:
				{
					const cutfEventArgs* pEventArgs = pEvent->GetEventArgs(); 

					if(pEventArgs && pEventArgs->GetType() == CUTSCENE_BOOL_VALUE_EVENT_ARGS_TYPE)
					{
						const cutfBoolValueEventArgs* pShadowEventArgs = static_cast<const cutfBoolValueEventArgs*>(pEventArgs); 

						m_bEnableFlyCamMode = pShadowEventArgs->GetValue();
					}
						
				}
				break; 

			case CUTSCENE_CASCADE_SHADOWS_SET_CASCADE_BOUNDS_HFOV:
				{
					const cutfEventArgs* pEventArgs = pEvent->GetEventArgs(); 

					if(pEventArgs && pEventArgs->GetType() == CUTSCENE_FLOAT_VALUE_EVENT_ARGS_TYPE)
					{
						const cutfFloatValueEventArgs* pShadowEventArgs = static_cast<const cutfFloatValueEventArgs*>(pEventArgs); 

						m_fBoundsHFov = pShadowEventArgs->GetFloat1();
					}
				}
				break; 

			case CUTSCENE_CASCADE_SHADOWS_SET_CASCADE_BOUNDS_VFOV:
				{
					const cutfEventArgs* pEventArgs = pEvent->GetEventArgs(); 

					if(pEventArgs && pEventArgs->GetType() == CUTSCENE_FLOAT_VALUE_EVENT_ARGS_TYPE)
					{
						const cutfFloatValueEventArgs* pShadowEventArgs = static_cast<const cutfFloatValueEventArgs*>(pEventArgs); 

						m_fBoundsVFov = pShadowEventArgs->GetFloat1();
					}
				}
				break; 

			case CUTSCENE_CASCADE_SHADOWS_SET_CASCADE_BOUNDS_SCALE:
				{
					const cutfEventArgs* pEventArgs = pEvent->GetEventArgs(); 

					if(pEventArgs && pEventArgs->GetType() == CUTSCENE_FLOAT_VALUE_EVENT_ARGS_TYPE)
					{
						const cutfFloatValueEventArgs* pShadowEventArgs = static_cast<const cutfFloatValueEventArgs*>(pEventArgs); 

						m_fBoundScale = pShadowEventArgs->GetFloat1();
					}

				}
				break;

			case CUTSCENE_CASCADE_SHADOWS_SET_ENTITY_TRACKER_SCALE:
				{
					const cutfEventArgs* pEventArgs = pEvent->GetEventArgs(); 

					if(pEventArgs && pEventArgs->GetType() == CUTSCENE_FLOAT_VALUE_EVENT_ARGS_TYPE)
					{
						const cutfFloatValueEventArgs* pShadowEventArgs = static_cast<const cutfFloatValueEventArgs*>(pEventArgs); 

						m_fTrackerScale = pShadowEventArgs->GetFloat1();
					}
				}
				break;

			case CUTSCENE_CASCADE_SHADOWS_SET_SPLIT_Z_EXP_WEIGHT:
				{
					const cutfEventArgs* pEventArgs = pEvent->GetEventArgs(); 

					if(pEventArgs && pEventArgs->GetType() == CUTSCENE_FLOAT_VALUE_EVENT_ARGS_TYPE)
					{
						const cutfFloatValueEventArgs* pShadowEventArgs = static_cast<const cutfFloatValueEventArgs*>(pEventArgs); 

						m_fSplitZExpWeight = pShadowEventArgs->GetFloat1();
					}

				}
				break;

			case CUTSCENE_CASCADE_SHADOWS_SET_DITHER_RADIUS_SCALE:
				{
					const cutfEventArgs* pEventArgs = pEvent->GetEventArgs(); 

					if(pEventArgs && pEventArgs->GetType() == CUTSCENE_FLOAT_VALUE_EVENT_ARGS_TYPE)
					{
						const cutfFloatValueEventArgs* pShadowEventArgs = static_cast<const cutfFloatValueEventArgs*>(pEventArgs); 

						m_fDitherScaleRadius = pShadowEventArgs->GetFloat1();
					}
				}
				break;

			case CUTSCENE_CASCADE_SHADOWS_SET_WORLD_HEIGHT_MINMAX:
				{
					const cutfEventArgs* pEventArgs = pEvent->GetEventArgs(); 

					if(pEventArgs && pEventArgs->GetType() == CUTSCENE_TWO_FLOAT_VALUES_EVENT_ARGS_TYPE)
					{
						const cutfTwoFloatValuesEventArgs* pShadowEventArgs = static_cast<const cutfTwoFloatValuesEventArgs*>(pEventArgs); 

						m_fWorldHeightMin = pShadowEventArgs->GetFloat1();
						m_fWorldHeightMax = pShadowEventArgs->GetFloat2(); 
					}
				}
				break;

			case CUTSCENE_CASCADE_SHADOWS_SET_RECEIVER_HEIGHT_MINMAX:
				{
					const cutfEventArgs* pEventArgs = pEvent->GetEventArgs(); 

					if(pEventArgs && pEventArgs->GetType() == CUTSCENE_TWO_FLOAT_VALUES_EVENT_ARGS_TYPE)
					{
						const cutfTwoFloatValuesEventArgs* pShadowEventArgs = static_cast<const cutfTwoFloatValuesEventArgs*>(pEventArgs); 

						m_fReceiveHeightMin = pShadowEventArgs->GetFloat1();
						m_fReceiveHeightMax = pShadowEventArgs->GetFloat2(); 
					}
				}
				break;

			case CUTSCENE_CASCADE_SHADOWS_SET_SLOPE_BIAS:
				{
					const cutfEventArgs* pEventArgs = pEvent->GetEventArgs(); 

					if(pEventArgs && pEventArgs->GetType() == CUTSCENE_FLOAT_BOOL_EVENT_ARGS_TYPE)
					{
						const cutfFloatBoolValueEventArgs* pShadowEventArgs = static_cast<const cutfFloatBoolValueEventArgs*>(pEventArgs); 

						m_fSlopeBias = pShadowEventArgs->GetFloat1();
						m_bEnableSlopeBias = pShadowEventArgs->GetValue(); 
					}
				}
				break; 

			case CUTSCENE_CASCADE_SHADOWS_SET_DEPTH_BIAS:
				{
					const cutfEventArgs* pEventArgs = pEvent->GetEventArgs(); 

					if(pEventArgs && pEventArgs->GetType() == CUTSCENE_FLOAT_BOOL_EVENT_ARGS_TYPE)
					{
						const cutfFloatBoolValueEventArgs* pShadowEventArgs = static_cast<const cutfFloatBoolValueEventArgs*>(pEventArgs); 

						m_fDepthBias = pShadowEventArgs->GetFloat1();
						m_bEnableDepthBias = pShadowEventArgs->GetValue(); 
					}
				}
				break; 

			case CUTSCENE_CASCADE_SHADOWS_SET_SHADOW_SAMPLE_TYPE:
				{
					const cutfEventArgs* pEventArgs = pEvent->GetEventArgs(); 

					if(pEventArgs && pEventArgs->GetType() == CUTSCENE_NAME_EVENT_ARGS_TYPE)
					{
						const cutfNameEventArgs* pShadowEventArgs = static_cast<const cutfNameEventArgs*>(pEventArgs); 

						u32 NameHash = pShadowEventArgs->GetName().GetHash();
					

						for(int i=0; i < MAX_NUMBER_SHADOW_TYPES; i++)
						{
							u32 selectedShadowHash = atStringHash(ShadowSampleTypes[i]); 
								
							if(NameHash == selectedShadowHash)
							{
								m_ShadowSampleIndex = i; 
								break; 
							}
						}
					}
				}	
				break;
			}

			if(iEventId != CUTSCENE_ENABLE_CASCADE_SHADOW_BOUNDS_EVENT)
			{
				ClearCascadeShadowDebugRender();
			}
		}
	}
}

void CCascadeShadowBoundsDebug::PopulateSetupShadowBoundsEventList()
{
	if(CutSceneManager::GetInstance()->GetDebugManager().m_CameraObjectList.GetCount() > 0)
	{
		const cutfObject* pObject = CutSceneManager::GetInstance()->GetDebugManager().m_CameraObjectList[0]; 
		if (pObject)
		PopulateSetShadowBoundsEventList(pObject); 	
	}
}

void CCascadeShadowBoundsDebug::SaveSetShadowBoundsEvent(const cutfObject* pObject, s32 iEventId)
{
	if(!Verifyf(CutSceneManager::GetInstance()->GetCutfFile(), "Overlay Event: No cutf file object loaded cannot save"))
	{
		return;
	}

	if ( pObject == NULL )
	{
		return;
	}

	atArray<cutfEvent *> EventList;
	CutSceneManager::GetInstance()->GetCutfFile()->FindEventsForObjectIdOnly( pObject->GetObjectId(), CutSceneManager::GetInstance()->GetCutfFile()->GetEventList(), EventList );

	s32 EventIndex; 
	atHashString cutHash = CutSceneManager::GetInstance()->GetDebugManager().FindCameraCutNameForTime(EventList, CutSceneManager::GetInstance()-> GetCutSceneCurrentTime(), EventIndex); 

#if	CUTSCENE_EXPORT_EVENTS_WITH_ZERO_BASE_TIME
	float ftime = CutSceneManager::GetInstance()->GetCutSceneTimeWithRangeOffset();  
#else
	float ftime =CutSceneManager::GetInstance()->GetCutSceneCurrentTime(); 
#endif
	
	float fEventTime = ftime; 

	if(m_UseCameraCutAsParent)
	{
		fEventTime = CutSceneManager::GetInstance()->GetDebugManager().FindCameraCutTimeForTime(EventList, ftime); 

		if(CutSceneManager::GetInstance()->GetCutSceneCurrentTime() == CutSceneManager::GetInstance()->GetEndTime())
		{
			fEventTime = CutSceneManager::GetInstance()->GetEndTime(); 
			cutHash.Clear(); 
			cutHash.SetFromString("endofscenecut"); 
		}
		else
		{
			fEventTime = CutSceneManager::GetInstance()->GetDebugManager().FindCameraCutTimeForTime(EventList, ftime); 
		}
	}

	cutfEventArgs* pEventArgs = NULL; 

	switch(iEventId)
	{
	case CUTSCENE_ENABLE_CASCADE_SHADOW_BOUNDS_EVENT:
		{
			pEventArgs = rage_new cutfCascadeShadowEventArgs( cutHash, m_CascadeBoundsPosition, m_fCascadeBoundsScaleRadius, 0.0f, m_CascadeIndex, m_bCascadeBoundsEnabled, true); 
		}
		break;

	case CUTSCENE_CASCADE_SHADOWS_ENABLE_ENTITY_TRACKER:
		{
			pEventArgs = rage_new cutfBoolValueEventArgs(m_EnableEntityTracker);
		}
		break; 

	case CUTSCENE_CASCADE_SHADOWS_SET_WORLD_HEIGHT_UPDATE:
		{
			pEventArgs = rage_new cutfBoolValueEventArgs(m_bEnableWorldHeightUpdate);
		}
		break; 

	case CUTSCENE_CASCADE_SHADOWS_SET_RECEIVER_HEIGHT_UPDATE:
		{
			pEventArgs = rage_new cutfBoolValueEventArgs(m_bEnableReceiverHeightUpdate);
		}
		break; 
	case CUTSCENE_CASCADE_SHADOWS_SET_AIRCRAFT_MODE:
		{
			pEventArgs = rage_new cutfBoolValueEventArgs(m_bEnableAirCraftMode);
		}
		break; 

	case CUTSCENE_CASCADE_SHADOWS_SET_DYNAMIC_DEPTH_MODE:
		{
			pEventArgs = rage_new cutfBoolValueEventArgs(m_bEnableDynamicDepthMode);
		}
		break; 

	case CUTSCENE_CASCADE_SHADOWS_SET_FLY_CAMERA_MODE:
		{
			pEventArgs = rage_new cutfBoolValueEventArgs(m_bEnableFlyCamMode);
		}
		break; 

	case CUTSCENE_CASCADE_SHADOWS_SET_CASCADE_BOUNDS_HFOV:
		{
			pEventArgs = rage_new cutfFloatValueEventArgs(m_fBoundsHFov);
		}
		break; 

	case CUTSCENE_CASCADE_SHADOWS_SET_CASCADE_BOUNDS_VFOV:
		{
			pEventArgs = rage_new cutfFloatValueEventArgs(m_fBoundsVFov);
		}
		break; 

	case CUTSCENE_CASCADE_SHADOWS_SET_CASCADE_BOUNDS_SCALE:
		{
			pEventArgs = rage_new cutfFloatValueEventArgs(m_fBoundScale);
		}
		break;

	case CUTSCENE_CASCADE_SHADOWS_SET_ENTITY_TRACKER_SCALE:
		{
			pEventArgs = rage_new cutfFloatValueEventArgs(m_fTrackerScale);
		}
		break;

	case CUTSCENE_CASCADE_SHADOWS_SET_SPLIT_Z_EXP_WEIGHT:
		{
			pEventArgs = rage_new cutfFloatValueEventArgs(m_fSplitZExpWeight);
		}
		break;

	case CUTSCENE_CASCADE_SHADOWS_SET_DITHER_RADIUS_SCALE:
		{
			pEventArgs = rage_new cutfFloatValueEventArgs(m_fDitherScaleRadius);
		}
		break;

	case CUTSCENE_CASCADE_SHADOWS_SET_WORLD_HEIGHT_MINMAX:
		{
			pEventArgs = rage_new cutfTwoFloatValuesEventArgs(m_fWorldHeightMin, m_fWorldHeightMax);
		}
		break;

	case CUTSCENE_CASCADE_SHADOWS_SET_RECEIVER_HEIGHT_MINMAX:
		{
			pEventArgs = rage_new cutfTwoFloatValuesEventArgs(m_fReceiveHeightMin, m_fReceiveHeightMax);
		}
		break;
	
	case CUTSCENE_CASCADE_SHADOWS_SET_SLOPE_BIAS:
		{
			pEventArgs = rage_new cutfFloatBoolValueEventArgs(m_fSlopeBias, m_bEnableSlopeBias);
		}
		break; 

	case CUTSCENE_CASCADE_SHADOWS_SET_DEPTH_BIAS:
		{
			pEventArgs = rage_new cutfFloatBoolValueEventArgs(m_fDepthBias, m_bEnableDepthBias);
		}
		break; 

	case CUTSCENE_CASCADE_SHADOWS_SET_SHADOW_SAMPLE_TYPE:
		{
			if(m_ShadowSampleIndex >= 0 && m_ShadowSampleIndex < MAX_NUMBER_SHADOW_TYPES )
			{
				pEventArgs = rage_new cutfNameEventArgs(ShadowSampleTypes[m_ShadowSampleIndex]);
			}
			else
			{
				cutsceneAssertf(0, "trying to write an invalid shadow sample type, index out of bounds of sample type array"); 
			}
		}	
		break;

	case CUTSCENE_CASCADE_SHADOWS_RESET_CASCADE_SHADOWS:
		{
			pEventArgs = rage_new cutfBoolValueEventArgs(true);
		}
		break;
	}
	
	Assertf(pEventArgs, "Must add any new event to SaveSetShadowBoundsEvent, must specify an event args or specify none"); 

	if(pEventArgs)
	{
		if(m_UseCameraCutAsParent)
		{
			CutSceneManager::GetInstance()->GetCutfFile()->AddEvent( rage_new cutfObjectIdEvent( pObject->GetObjectId(), fEventTime, iEventId, true, cutHash, pEventArgs ) );
		}	
		else
		{
			CutSceneManager::GetInstance()->GetCutfFile()->AddEvent( rage_new cutfObjectIdEvent( pObject->GetObjectId(), fEventTime, iEventId, false, 0, pEventArgs ) );
		}
		CutSceneManager::GetInstance()->GetCutfFile()->SortEvents();
	}
}

void CCascadeShadowBoundsDebug::PopulateSetShadowBoundsEventList(const cutfObject* pObject)
{
	if (m_pCascadeShadowsBoundEventsCombo == NULL)
	{
		return; 
	}

	//Nothing selected
	if(!pObject)
	{
		m_CascadeShadowsBoundEventsIndexId = 0; 
		m_pCascadeShadowsBoundEventsCombo->UpdateCombo( "events", &m_CascadeShadowsBoundEventsIndexId, 1, 0, datCallback(MFA(CCascadeShadowBoundsDebug::UpdateShadowBoxCombo), (datBase*)this) );
		m_pCascadeShadowsBoundEventsCombo->SetString( 0 , "none");
		return; 
	}

	atArray<cutfEvent *> AllEventList;
	CutSceneManager::GetInstance()->GetCutfFile()->FindEventsForObjectIdOnly( pObject->GetObjectId(), CutSceneManager::GetInstance()->GetCutfFile()->GetEventList(), AllEventList );

	atArray<cutfEvent *>EventList; 

	for(int i =0; i < AllEventList.GetCount(); i++)
	{
		//cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( AllEventList[i]->GetEventArgs() );
		if(AllEventList[i]->GetEventId() == CUTSCENE_ENABLE_CASCADE_SHADOW_BOUNDS_EVENT || 
			AllEventList[i]->GetEventId() == CUTSCENE_CASCADE_SHADOWS_ENABLE_ENTITY_TRACKER || 
			AllEventList[i]->GetEventId() == CUTSCENE_CASCADE_SHADOWS_SET_WORLD_HEIGHT_UPDATE || 
			AllEventList[i]->GetEventId() == CUTSCENE_CASCADE_SHADOWS_SET_RECEIVER_HEIGHT_UPDATE ||
			AllEventList[i]->GetEventId() == CUTSCENE_CASCADE_SHADOWS_SET_AIRCRAFT_MODE || 
			AllEventList[i]->GetEventId() == CUTSCENE_CASCADE_SHADOWS_SET_DYNAMIC_DEPTH_MODE ||
			AllEventList[i]->GetEventId() == CUTSCENE_CASCADE_SHADOWS_SET_FLY_CAMERA_MODE ||
			AllEventList[i]->GetEventId() == CUTSCENE_CASCADE_SHADOWS_SET_CASCADE_BOUNDS_HFOV ||
			AllEventList[i]->GetEventId() == CUTSCENE_CASCADE_SHADOWS_SET_CASCADE_BOUNDS_VFOV ||
			AllEventList[i]->GetEventId() == CUTSCENE_CASCADE_SHADOWS_SET_CASCADE_BOUNDS_SCALE ||
			AllEventList[i]->GetEventId() == CUTSCENE_CASCADE_SHADOWS_SET_ENTITY_TRACKER_SCALE ||
			AllEventList[i]->GetEventId() == CUTSCENE_CASCADE_SHADOWS_SET_SPLIT_Z_EXP_WEIGHT ||
			AllEventList[i]->GetEventId() == CUTSCENE_CASCADE_SHADOWS_SET_DITHER_RADIUS_SCALE ||
			AllEventList[i]->GetEventId() == CUTSCENE_CASCADE_SHADOWS_SET_WORLD_HEIGHT_MINMAX ||
			AllEventList[i]->GetEventId() == CUTSCENE_CASCADE_SHADOWS_SET_RECEIVER_HEIGHT_MINMAX ||
			AllEventList[i]->GetEventId() == CUTSCENE_CASCADE_SHADOWS_SET_SLOPE_BIAS||
			AllEventList[i]->GetEventId() == CUTSCENE_CASCADE_SHADOWS_SET_DEPTH_BIAS||
			AllEventList[i]->GetEventId() == CUTSCENE_CASCADE_SHADOWS_SET_SHADOW_SAMPLE_TYPE ||
			AllEventList[i]->GetEventId() == CUTSCENE_CASCADE_SHADOWS_RESET_CASCADE_SHADOWS)
		{
			EventList.PushAndGrow(AllEventList[i]); 
		}
	}

	//look to edit a current event 
	if (EventList.GetCount() == 0)
	{
		m_pCascadeShadowsBoundEventsCombo->UpdateCombo( "Cascade shadow bounds events", &m_CascadeShadowsBoundEventsIndexId, 1, 0, datCallback(MFA(CCascadeShadowBoundsDebug::UpdateShadowBoxCombo), (datBase*)this) );
		m_pCascadeShadowsBoundEventsCombo->SetString( 0 , "none");
	}
	else
	{
		if (EventList.GetCount() > 0)
		{
			//limit so we dont go over the bounds
			if(m_CascadeShadowsBoundEventsIndexId == EventList.GetCount())
			{
				m_CascadeShadowsBoundEventsIndexId = EventList.GetCount() -1; 
			}
			m_pCascadeShadowsBoundEventsCombo->UpdateCombo( "Cascade shadow bounds events", &m_CascadeShadowsBoundEventsIndexId, EventList.GetCount(), 0, datCallback(MFA(CCascadeShadowBoundsDebug::UpdateShadowBoxCombo), (datBase*)this) );

			//Clear the list of variations, need to repopulate. 
			for (int i=0; i < m_CascadeShadowsBoundIndexEvents.GetCount(); i++)
			{
				m_CascadeShadowsBoundIndexEvents[i] = NULL;  
			}

			m_CascadeShadowsBoundIndexEvents.Reset(); 

			for ( int i = 0; i < EventList.GetCount(); ++i )
			{
				if(EventList[i])
				{
					const cutfEventArgs* pArgs = EventList[i]->GetEventArgs(); 
					char EventInfo[128]; 	
					EventInfo[0] = '\0'; 
					float FrameTime = EventList[i]->GetTime(); 
					s32 FrameNumber = static_cast<s32>(FrameTime * CUTSCENE_FPS); 

					switch(EventList[i]->GetEventId())
					{
						case CUTSCENE_ENABLE_CASCADE_SHADOW_BOUNDS_EVENT:
						{
							if(pArgs && pArgs->GetType() == CUTSCENE_CASCADE_SHADOW_EVENT_ARGS)
							{			
								const cutfCascadeShadowEventArgs* pEventArgs = static_cast< const cutfCascadeShadowEventArgs*>(pArgs);

								formatf(EventInfo, sizeof(EventInfo)-1,"(%.2f %d) SET_CASCADE_BOUNDS: index:%d, Active:%d, Pos:%.2f,%.2f,%.2f (%s)", 
									FrameTime, FrameNumber,
									pEventArgs->GetCascadeShadowIndex(),  
									pEventArgs->GetIsEnabled(), 
									pEventArgs->GetPosition().x,
									pEventArgs->GetPosition().y, 
									pEventArgs->GetPosition().z,
									pEventArgs->GetCameraCutHash().GetCStr()); 
							}
						}
						break;

						case CUTSCENE_CASCADE_SHADOWS_RESET_CASCADE_SHADOWS:
							{
								formatf(EventInfo, sizeof(EventInfo)-1,"(%.2f %d) RESET_CASCADE_SHADOWS", FrameTime, FrameNumber); 
							}
							break;

						case CUTSCENE_CASCADE_SHADOWS_ENABLE_ENTITY_TRACKER:
							{
								if(pArgs && pArgs->GetType() == CUTSCENE_BOOL_VALUE_EVENT_ARGS_TYPE)
								{	
									const cutfBoolValueEventArgs* pEventArgs = static_cast< const cutfBoolValueEventArgs*>(pArgs);

									formatf(EventInfo, sizeof(EventInfo)-1,"(%.2f %d) ENABLE_ENTITY_TRACKER: %d ", FrameTime, FrameNumber, pEventArgs->GetValue());
								}
							}
							break; 

						case CUTSCENE_CASCADE_SHADOWS_SET_WORLD_HEIGHT_UPDATE:
							{
								if(pArgs && pArgs->GetType() == CUTSCENE_BOOL_VALUE_EVENT_ARGS_TYPE)
								{	
									const cutfBoolValueEventArgs* pEventArgs = static_cast< const cutfBoolValueEventArgs*>(pArgs);

									formatf(EventInfo, sizeof(EventInfo)-1,"(%.2f %d) SET_WORLD_HEIGHT_UPDATE: %d",FrameTime, FrameNumber, pEventArgs->GetValue());
								}
							}
							break; 

						case CUTSCENE_CASCADE_SHADOWS_SET_RECEIVER_HEIGHT_UPDATE:
							{
								if(pArgs && pArgs->GetType() == CUTSCENE_BOOL_VALUE_EVENT_ARGS_TYPE)
								{	
									const cutfBoolValueEventArgs* pEventArgs = static_cast< const cutfBoolValueEventArgs*>(pArgs);

									formatf(EventInfo, sizeof(EventInfo)-1,"(%.2f %d) SET_RECEIVER_HEIGHT_UPDATE: %d", FrameTime, FrameNumber, pEventArgs->GetValue());
								}
							}
							break; 
						case CUTSCENE_CASCADE_SHADOWS_SET_AIRCRAFT_MODE:
							{
								if(pArgs && pArgs->GetType() == CUTSCENE_BOOL_VALUE_EVENT_ARGS_TYPE)
								{	
									const cutfBoolValueEventArgs* pEventArgs = static_cast< const cutfBoolValueEventArgs*>(pArgs);

									formatf(EventInfo, sizeof(EventInfo)-1,"(%.2f %d) SET_AIRCRAFT_MODE: %d", FrameTime, FrameNumber, pEventArgs->GetValue());
								}
							}
							break; 

						case CUTSCENE_CASCADE_SHADOWS_SET_DYNAMIC_DEPTH_MODE:
							{
								if(pArgs && pArgs->GetType() == CUTSCENE_BOOL_VALUE_EVENT_ARGS_TYPE)
								{	
									const cutfBoolValueEventArgs* pEventArgs = static_cast< const cutfBoolValueEventArgs*>(pArgs);

									formatf(EventInfo, sizeof(EventInfo)-1,"(%.2f %d) SET_DYNAMIC_DEPTH_MODE: %d", FrameTime, FrameNumber, pEventArgs->GetValue());
								}
							}
							break; 

						case CUTSCENE_CASCADE_SHADOWS_SET_FLY_CAMERA_MODE:
							{
								if(pArgs && pArgs->GetType() == CUTSCENE_BOOL_VALUE_EVENT_ARGS_TYPE)
								{	
									const cutfBoolValueEventArgs* pEventArgs = static_cast< const cutfBoolValueEventArgs*>(pArgs);

									formatf(EventInfo, sizeof(EventInfo)-1,"(%.2f %d) SET_FLY_CAMERA_MODE: %d", FrameTime, FrameNumber, pEventArgs->GetValue());
								}
							}
							break; 
							
						case CUTSCENE_CASCADE_SHADOWS_SET_CASCADE_BOUNDS_HFOV:
							{
								if(pArgs && pArgs->GetType() == CUTSCENE_FLOAT_VALUE_EVENT_ARGS_TYPE)
								{	
									const cutfFloatValueEventArgs* pEventArgs = static_cast< const cutfFloatValueEventArgs*>(pArgs);

									formatf(EventInfo, sizeof(EventInfo)-1,"(%.2f %d) CASCADE_BOUNDS_HFOV: %f", FrameTime, FrameNumber, pEventArgs->GetFloat1());
								}
							}
							break; 

						case CUTSCENE_CASCADE_SHADOWS_SET_CASCADE_BOUNDS_VFOV:
							{
								if(pArgs && pArgs->GetType() == CUTSCENE_FLOAT_VALUE_EVENT_ARGS_TYPE)
								{	
									const cutfFloatValueEventArgs* pEventArgs = static_cast< const cutfFloatValueEventArgs*>(pArgs);

									formatf(EventInfo, sizeof(EventInfo)-1,"(%.2f %d) CASCADE_BOUNDS_VFOV: %f", FrameTime, FrameNumber, pEventArgs->GetFloat1());
								}
							}
							break; 
							
						case CUTSCENE_CASCADE_SHADOWS_SET_CASCADE_BOUNDS_SCALE:
							{
								if(pArgs && pArgs->GetType() == CUTSCENE_FLOAT_VALUE_EVENT_ARGS_TYPE)
								{	
									const cutfFloatValueEventArgs* pEventArgs = static_cast< const cutfFloatValueEventArgs*>(pArgs);

									formatf(EventInfo, sizeof(EventInfo)-1,"(%.2f %d) CASCADE_BOUNDS_SCALE: %f", FrameTime, FrameNumber, pEventArgs->GetFloat1());
								}
							}
							break;

						case CUTSCENE_CASCADE_SHADOWS_SET_ENTITY_TRACKER_SCALE:
							{
								if(pArgs && pArgs->GetType() == CUTSCENE_FLOAT_VALUE_EVENT_ARGS_TYPE)
								{	
									const cutfFloatValueEventArgs* pEventArgs = static_cast< const cutfFloatValueEventArgs*>(pArgs);

									formatf(EventInfo, sizeof(EventInfo)-1,"(%.2f %d) ENTITY_TRACKER_SCALE: %f", FrameTime, FrameNumber, pEventArgs->GetFloat1());
								}
							}
							break;
							
						case CUTSCENE_CASCADE_SHADOWS_SET_SPLIT_Z_EXP_WEIGHT:
							{
								if(pArgs && pArgs->GetType() == CUTSCENE_FLOAT_VALUE_EVENT_ARGS_TYPE)
								{	
									const cutfFloatValueEventArgs* pEventArgs = static_cast< const cutfFloatValueEventArgs*>(pArgs);

									formatf(EventInfo, sizeof(EventInfo)-1,"(%.2f %d) SPLIT_Z_EXP_WEIGHT: %f", FrameTime, FrameNumber, pEventArgs->GetFloat1());
								}
							}
							break;
								
						case CUTSCENE_CASCADE_SHADOWS_SET_DITHER_RADIUS_SCALE:
							{
								if(pArgs && pArgs->GetType() == CUTSCENE_FLOAT_VALUE_EVENT_ARGS_TYPE)
								{	
									const cutfFloatValueEventArgs* pEventArgs = static_cast< const cutfFloatValueEventArgs*>(pArgs);

									formatf(EventInfo, sizeof(EventInfo)-1,"(%.2f %d) DITHER_RADIUS_SCALE: %f", FrameTime, FrameNumber, pEventArgs->GetFloat1());
								}
							}
							break;
				
						case CUTSCENE_CASCADE_SHADOWS_SET_WORLD_HEIGHT_MINMAX:
							{
								if(pArgs && pArgs->GetType() == CUTSCENE_TWO_FLOAT_VALUES_EVENT_ARGS_TYPE)
								{	
									const cutfTwoFloatValuesEventArgs* pEventArgs = static_cast< const cutfTwoFloatValuesEventArgs*>(pArgs);

									formatf(EventInfo, sizeof(EventInfo)-1,"(%.2f %d) SET_WORLD_HEIGHT_MINMAX: %f, %f", FrameTime, FrameNumber, pEventArgs->GetFloat1(), pEventArgs->GetFloat2());
								}
							}
							break;

						case CUTSCENE_CASCADE_SHADOWS_SET_RECEIVER_HEIGHT_MINMAX:
							{
								if(pArgs && pArgs->GetType() == CUTSCENE_TWO_FLOAT_VALUES_EVENT_ARGS_TYPE)
								{	
									const cutfTwoFloatValuesEventArgs* pEventArgs = static_cast< const cutfTwoFloatValuesEventArgs*>(pArgs);

									formatf(EventInfo, sizeof(EventInfo)-1,"(%.2f %d) SET_RECEIVER_HEIGHT_MINMAX: %f, %f", FrameTime, FrameNumber, pEventArgs->GetFloat1(), pEventArgs->GetFloat2());
								}
							}
							break;

						case CUTSCENE_CASCADE_SHADOWS_SET_SLOPE_BIAS:
							{
								if(pArgs && pArgs->GetType() == CUTSCENE_FLOAT_BOOL_EVENT_ARGS_TYPE)
								{	
									const cutfFloatBoolValueEventArgs* pEventArgs = static_cast< const cutfFloatBoolValueEventArgs*>(pArgs);

									formatf(EventInfo, sizeof(EventInfo)-1,"(%.2f %d) SET_SLOPE_BIAS: %d, %f", FrameTime, FrameNumber, pEventArgs->GetFloat1(), pEventArgs->GetValue());
								}
							}
							break;

						case CUTSCENE_CASCADE_SHADOWS_SET_DEPTH_BIAS:
							{
								if(pArgs && pArgs->GetType() == CUTSCENE_FLOAT_BOOL_EVENT_ARGS_TYPE)
								{	
									const cutfFloatBoolValueEventArgs* pEventArgs = static_cast< const cutfFloatBoolValueEventArgs*>(pArgs);

									formatf(EventInfo, sizeof(EventInfo)-1,"(%.2f %d) SET_DEPTH_BIAS: %d, %f", FrameTime, FrameNumber, pEventArgs->GetFloat1(), pEventArgs->GetValue());
								}
							}
							break;

						case CUTSCENE_CASCADE_SHADOWS_SET_SHADOW_SAMPLE_TYPE:
							{
								if(pArgs && pArgs->GetType() == CUTSCENE_NAME_EVENT_ARGS_TYPE)
								{	
									const cutfNameEventArgs* pEventArgs = static_cast< const cutfNameEventArgs*>(pArgs);

									formatf(EventInfo, sizeof(EventInfo)-1,"(%.2f %d) SET_SHADOW_SAMPLE_TYPE: %s", FrameTime, FrameNumber, pEventArgs->GetName().GetCStr());
								}
							}
							break; 
							
					}

					m_pCascadeShadowsBoundEventsCombo->SetString( i, EventInfo );

					//Store a list of pointers to our objects
					m_CascadeShadowsBoundIndexEvents.PushAndGrow(EventList[i]); 
				}
			}
		}
	}
};


float FindTimeForCameraCutName(const atArray <cutfEvent*>& events, const char* cutname)
{
	atHashString cameraCutName; 

	if(events.GetCount() > 0)
	{
		for ( int i = events.GetCount() - 1; i >= 0; --i )
		{
			if (events[i]->GetEventId() == CUTSCENE_CAMERA_CUT_EVENT)
			{
				cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( events[i]->GetEventArgs() );

				if(pEventArgs->GetType() == CUTSCENE_CAMERA_CUT_EVENT_ARGS_TYPE )
				{
					cutfCameraCutEventArgs* pObjectVar = static_cast<cutfCameraCutEventArgs*>(pEventArgs); 

					if(pObjectVar)
					{
						atHashString cameraCutName = pObjectVar->GetName(); 
						if ( cameraCutName == cutname)
						{
							return events[i]->GetTime();
						}
					}
				}
			}
		}
	}
	return -1.f; 
}


void CCutSceneDebugManager::ToggleNightCoCOverrides()
{
	if(m_bShouldSetCoCForAllDay)
	{
		m_bShouldSetCoCForAllNight = false; 
		m_bShouldUseRangeForCoC = false; 
		DeleteCoCDayRangeSelectorWidgets(); 
	}
}

void CCutSceneDebugManager::ToggleDayCoCOverrides()
{
	if(m_bShouldSetCoCForAllNight)
	{
		m_bShouldSetCoCForAllDay = false; 
		m_bShouldUseRangeForCoC = false; 
		DeleteCoCDayRangeSelectorWidgets(); 
	}
}

void CCutSceneDebugManager::ToggleDayRangeCoCOverrides()
{
	if(m_bShouldUseRangeForCoC)
	{
		CreateCoCHourSelectorWidgets(); 
		m_bShouldSetCoCForAllDay = false; 
		m_bShouldSetCoCForAllNight = false; 
	}
	else
	{
		DeleteCoCDayRangeSelectorWidgets(); 
	}
}


void CCutSceneDebugManager::DeleteCoCDayRangeSelectorWidgets()
{
	if(m_pHoursGrp)
	{
		m_pHoursGrp->Destroy(); 
		m_pHoursGrp = NULL; 
	}
}

void CCutSceneDebugManager::CreateCoCHourSelectorWidgets()
{
	bkBank* Ragebank = BANKMGR.FindBank("Cut Scene Debug");
	bkGroup* CameraGrp = static_cast<bkGroup*>(BANKMGR.FindWidget("Cut Scene Debug/Current Scene/Camera Editing/Camera Overrides/Scene Day CoC Hours"));
	
	if(Ragebank && CameraGrp)
	{
		if(m_pHoursGrp == NULL)
		{
			if(CutSceneManager::GetInstance())
			{
				cutfCutsceneFile2* pFile =  CutSceneManager::GetInstance()->GetCutfFile(); 

				if(pFile)
				{
					m_DayCoCHours = pFile->GetDayCoCHours();
				}
			}
		
			Ragebank->SetCurrentGroup(*CameraGrp); 
			m_pHoursGrp = Ragebank->PushGroup("Hours"); 
				for(int i = 0; i < 24; i++)
				{
					char name[24];
					if(i == 0)
					{
						formatf(name, sizeof(name)-1, "Hour %d (Midnight)", i); 
					}
					else if(i == 12)
					{
						formatf(name, sizeof(name)-1, "Hour %d (Midday)", i); 
					}
					else
					{
						formatf(name, sizeof(name)-1, "Hour %d", i); 
					}
						
					u32 bitsPerBlock = sizeof(u32) * 8;
					int block = i / bitsPerBlock;
					int bitInBlock = i - (block * bitsPerBlock);
					Ragebank->AddToggle(name, &m_DayCoCHours, (u32)(1 << bitInBlock));

				}
			Ragebank->PopGroup(); 
			Ragebank->UnSetCurrentGroup(*CameraGrp); 
		}
	}
}

void CCutSceneDebugManager::SaveDayCoCValues()
{
	if(CutSceneManager::GetInstance())
	{
		cutfCutsceneFile2* pFile =  CutSceneManager::GetInstance()->GetCutfFile(); 

		if(pFile)
		{
			//set all the values to be day 0 - 23
			if(m_bShouldSetCoCForAllDay)
			{
				pFile->SetDayCoCHours(16777215);
				CutSceneManager::GetInstance()->SaveGameDataCutfile(); 
			}
			//set
			else if(m_bShouldSetCoCForAllNight)
			{
				pFile->SetDayCoCHours(0); 
				CutSceneManager::GetInstance()->SaveGameDataCutfile(); 
			}
			else if(m_bShouldUseRangeForCoC)
			{
				pFile->SetDayCoCHours(m_DayCoCHours); 
				CutSceneManager::GetInstance()->SaveGameDataCutfile(); 
			}
		}
	}
}

void CCutSceneDebugManager::UpdateHourSliderToDayHours()
{
	if(m_UseDayCoCHoursForModifier)
	{
		m_UseNightCoCHoursForModifier = false; 
		if(CutSceneManager::GetInstance())
		{
			cutfCutsceneFile2* pFile =  CutSceneManager::GetInstance()->GetCutfFile(); 

			if(pFile)
			{
				m_CoCModifierHours = pFile->GetDayCoCHours(); 
			}
		}
	}
}

void CCutSceneDebugManager::UpdateHourSliderToNightHours()
{
	if(m_UseNightCoCHoursForModifier)
	{
		m_UseDayCoCHoursForModifier = false; 
		if(CutSceneManager::GetInstance())
		{
			cutfCutsceneFile2* pFile =  CutSceneManager::GetInstance()->GetCutfFile(); 

			if(pFile)
			{
				u32 HourFlags = pFile->GetDayCoCHours(); ; 
				u32 NewHours = 0; 
				for(int i =0 ; i < 24; i++)
				{
					if (!(HourFlags & (1 << i)))
					{
						NewHours = NewHours | 1<<i; 
					}
				}
				m_CoCModifierHours = NewHours; 
			}
		}
	}
}

void CCutSceneDebugManager::BankSaveCoCModifierCallBack()
{
	if (CutSceneManager::GetInstance()->GetDebugManager().m_CameraObjectList.GetCount() > 0)
	{
		const cutfObject* pObject = CutSceneManager::GetInstance()->GetDebugManager().m_CameraObjectList[0]; 

		if ( (CutSceneManager::GetInstance()->IsPaused() || CutSceneManager::GetInstance()->IsPlaying()) && pObject)
		{
			//Save the ped variation into the event list
			SaveCoCModifier(); 

			//update the combo box of ped variation events
			UpdateCoCModifierEventOverridesCB(); 

			//Save the changes out to the cut tune file.
			CutSceneManager::GetInstance()->SaveGameDataCutfile(); 
		}
	}
}

void CCutSceneDebugManager::BankSaveAllCoCModifiersCallBack()
{
	if (CutSceneManager::GetInstance()->GetDebugManager().m_CameraObjectList.GetCount() > 0)
	{
		const cutfObject* pObject = CutSceneManager::GetInstance()->GetDebugManager().m_CameraObjectList[0]; 

		if ( (CutSceneManager::GetInstance()->IsPaused() || CutSceneManager::GetInstance()->IsPlaying()) && pObject)
		{
			//Save the ped variation into the event list
			SaveAllCoCModifiers(); 

			//update the combo box of ped variation events
			UpdateCoCModifierEventOverridesCB(); 

			//Save the changes out to the cut tune file.
			CutSceneManager::GetInstance()->SaveGameDataCutfile(); 
		}
	}
}

void CCutSceneDebugManager::DeleteCoCModifierCallBack()
{
	if(m_CameraCutsIndexEvents[m_iCoCModifiersCamerCutComboIndexId])
	{
		cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( m_CameraCutsIndexEvents[m_iCoCModifiersCamerCutComboIndexId]->GetEventArgs() );
		if(pEventArgs->GetType() == CUTSCENE_CAMERA_CUT_EVENT_ARGS_TYPE )
		{	
			cutfCameraCutEventArgs* pObjectVar = static_cast<cutfCameraCutEventArgs*>(pEventArgs); 
			if(pObjectVar->GetCoCModifierList().GetCount() > 0)
			{
				pObjectVar->RemoveCoCModifier(m_iCoCModifierIndex); 

				UpdateCoCModifierEventOverridesCB(); 
			}
		}
	}
}

void CCutSceneDebugManager::DeleteAllCoCModifierOverrideCallBack()
{
	for(int i = 0; i < m_CameraCutsIndexEvents.GetCount(); i++)
	{
		if(m_CameraCutsIndexEvents[i])
		{
			cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( m_CameraCutsIndexEvents[i]->GetEventArgs() );
			if(pEventArgs->GetType() == CUTSCENE_CAMERA_CUT_EVENT_ARGS_TYPE )
			{	
				cutfCameraCutEventArgs* pObjectVar = static_cast<cutfCameraCutEventArgs*>(pEventArgs); 
				if(pObjectVar->GetCoCModifierList().GetCount() > 0)
				{
					for(int j =0; j < pObjectVar->GetCoCModifierList().GetCount(); j++)
					{
						pObjectVar->RemoveCoCModifier(j); 
					}
					
					m_iCoCModifiersCamerCutComboIndexId = i; 

					UpdateCoCModifierEventOverridesCB(); 
				}
			}
		}
	}
}



void CCutSceneDebugManager::SaveCoCModifier()
{
	if(m_iCoCModifiersCamerCutComboIndexId < m_CameraCutsIndexEvents.GetCount())
	{
		if(m_CameraCutsIndexEvents[m_iCoCModifiersCamerCutComboIndexId])
		{
			cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( m_CameraCutsIndexEvents[m_iCoCModifiersCamerCutComboIndexId]->GetEventArgs() );
			if(pEventArgs && pEventArgs->GetType() == CUTSCENE_CAMERA_CUT_EVENT_ARGS_TYPE)
			{	
				if(m_CoCModifier != 0)
				{
					if(m_CoCModifierHours !=0)
					{
						cutfCameraCutEventArgs* pCameraCutEventArgs = static_cast<cutfCameraCutEventArgs*>(pEventArgs); 

						if(pCameraCutEventArgs->AddCoCModifier(m_CoCModifierHours, m_CoCModifier))
						{
							if(m_pSaveStatusText)
							{
								m_pSaveStatusText->SetString("Save Success"); 
							}

						}
						else
						{
							if(m_pSaveStatusText)
							{
								m_pSaveStatusText->SetString("Save Fail - Cannot add CoC Modifier with the same hours"); 
							}
						}
					}	
					else
					{
						if(m_pSaveStatusText)
						{
							m_pSaveStatusText->SetString("Save Fail - Cannot add CoC Modifier with no hour values"); 
						}
					}
					
				}
				else
				{
					if(m_pSaveStatusText)
					{
						m_pSaveStatusText->SetString("Save Fail - Cannot add CoC Modifier with 0"); 
					}
				}
			}
		}
	}
}

void CCutSceneDebugManager::SaveAllCoCModifiers()
{
	for(int i =0; i < m_CameraCutsIndexEvents.GetCount(); i++)
	{
		m_iCoCModifiersCamerCutComboIndexId = i; 

		SaveCoCModifier(); 
	}
}

void CCutSceneDebugManager::UpdateAllCoCModifierEventOverridesCB()
{
	if (m_AllCoCModifiersCombo == NULL)
	{
		return; 
	}

	m_iAllCoCModifierIndex = 0; 
	u32 eventCountIndex = 0; 
	u32 TotalEventCount = 0; 
	bool NoEvents = true; 
	for(int k=0; k < m_CameraCutsIndexEvents.GetCount(); k++)
	{
		if(m_CameraCutsIndexEvents[k])
		{
			cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( m_CameraCutsIndexEvents[k]->GetEventArgs() );
			if(pEventArgs->GetType() == CUTSCENE_CAMERA_CUT_EVENT_ARGS_TYPE )
			{	
				cutfCameraCutEventArgs* pObjectVar = static_cast<cutfCameraCutEventArgs*>(pEventArgs); 
				if(pObjectVar->GetCoCModifierList().GetCount() > 0)
				{
					TotalEventCount += pObjectVar->GetCoCModifierList().GetCount(); 
					m_AllCoCModifiersCombo->UpdateCombo("All CoC Modifiers (READ ONLY)", &m_iAllCoCModifierIndex, TotalEventCount, NULL );
					NoEvents = false; 
					for(int i=0; i < pObjectVar->GetCoCModifierList().GetCount(); i++)
					{
						char EventInfo[128]; 	
						EventInfo[0] = '\0'; 

						//u32 m_TimeOfDayFlags; 
						//s32 m_DofStrengthModifier; 

						u32 HourFlags = pObjectVar->GetCoCModifierList()[i].m_TimeOfDayFlags; 


						char Hours[128]; 
						atString atHours; 

						for(int j =0 ; j < 24; j++)
						{
							if (HourFlags & (1 << j))
							{
								formatf(Hours, sizeof(Hours)-1,"%d,",j);
								atHours += Hours; 
							}
						}

						formatf(EventInfo, sizeof(EventInfo), "%s: CoC Modifier %d: Hours: %s",pObjectVar->GetName().GetCStr(), pObjectVar->GetCoCModifierList()[i].m_DofStrengthModifier, atHours.c_str());

						m_AllCoCModifiersCombo->SetString(eventCountIndex, EventInfo); 
						eventCountIndex++; 
					}
				}
			}
		}
	}

	if(NoEvents)
	{
		m_iAllCoCModifierIndex = 0; 
		m_AllCoCModifiersCombo->UpdateCombo( "All CoC Modifiers (READ ONLY)", &m_iAllCoCModifierIndex, 1, NULL );
		m_AllCoCModifiersCombo->SetString( 0 , "none");
	}
}
void CCutSceneDebugManager::UpdateCoCModifierEventOverridesCB()
{
	if (m_pCoCModifiersCameraCutCombo == NULL)
	{
		return; 
	}

	if(m_CameraCutsIndexEvents[m_iCoCModifiersCamerCutComboIndexId])
	{
		cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( m_CameraCutsIndexEvents[m_iCoCModifiersCamerCutComboIndexId]->GetEventArgs() );
		if(pEventArgs->GetType() == CUTSCENE_CAMERA_CUT_EVENT_ARGS_TYPE )
		{	
			cutfCameraCutEventArgs* pObjectVar = static_cast<cutfCameraCutEventArgs*>(pEventArgs); 
			if(pObjectVar->GetCoCModifierList().GetCount() > 0)
			{
				m_iCoCModifierIndex = 0; 
				m_pPerCutCoCModifiersCombo->UpdateCombo( "Per Camera Cut CoC Modifiers", &m_iCoCModifierIndex, pObjectVar->GetCoCModifierList().GetCount(), NULL );

				for(int i=0; i < pObjectVar->GetCoCModifierList().GetCount(); i++)
				{
					char EventInfo[128]; 	
					EventInfo[0] = '\0'; 
					
					//u32 m_TimeOfDayFlags; 
					//s32 m_DofStrengthModifier; 
		
					u32 HourFlags = pObjectVar->GetCoCModifierList()[i].m_TimeOfDayFlags; 
	

					char Hours[128]; 
					atString atHours; 

					for(int j =0 ; j < 24; j++)
					{
						if (HourFlags & (1 << j))
						{
							formatf(Hours, sizeof(Hours)-1,"%d,",j);
							atHours += Hours; 
						}
					}

					formatf(EventInfo, sizeof(EventInfo), "%s: CoC Modifier %d: Hours: %s",pObjectVar->GetName().GetCStr(), pObjectVar->GetCoCModifierList()[i].m_DofStrengthModifier, atHours.c_str());

					m_pPerCutCoCModifiersCombo->SetString(i, EventInfo); 
				}
			}
			else
			{
				m_iCoCModifierIndex = 0; 
				m_pPerCutCoCModifiersCombo->UpdateCombo( "Per Camera Cut CoC Modifiers", &m_iCoCModifierIndex, 1, NULL );
				m_pPerCutCoCModifiersCombo->SetString( 0 , "none");
			}
		}
	}

	UpdateAllCoCModifierEventOverridesCB(); 
}

//void CCutSceneDebugManager::PopulateTimeOfDofModifiers()
//{
//	if (m_pTimeOfDayDofOverrideCombo == NULL)
//	{
//		return; 
//	}
//
//	//Nothing selected
//	if(!pObject)
//	{
//		m_pTimeOfDayDofOverrideCombo->UpdateCombo( "TimeOfDayDofOverrides", &m_iTimeOfDayDofOverrideIndex, 1, NULL );
//		m_pTimeOfDayDofOverrideCombo->SetString( 0 , "none");
//		return; 
//	}
//
//	atArray<cutfEvent *> AllEventList;
//	CutSceneManager::GetInstance()->GetCutfFile()->FindEventsForObjectIdOnly( pObject->GetObjectId(), CutSceneManager::GetInstance()->GetCutfFile()->GetEventList(), AllEventList );
//
//	atArray<cutfEvent *>EventList; 
//
//	for(int i =0; i < AllEventList.GetCount(); i++)
//	{
//		//cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( AllEventList[i]->GetEventArgs() );
//		if(AllEventList[i]->GetEventId() == CUTSCENE_CAMERA_CUT_EVENT)
//		{
//			EventList.PushAndGrow(AllEventList[i]); 
//		}
//	}
//
//	//look to edit a current event 
//	if (EventList.GetCount() == 0)
//	{
//		m_pTimeOfDayDofOverrideCombo->UpdateCombo( "TimeOfDayDofOverrides", &m_iTimeOfDayDofOverrideIndex, 1, NULL );
//		m_pTimeOfDayDofOverrideCombo->SetString( 0 , "none");
//	}
//	else
//	{
//		if (EventList.GetCount() > 0)
//		{
//			m_iTimeOfDayDofOverrideIndex = 0; 
//			s32 NumEvents = 0; 
//
//			for(int i = 0; i < EventList.GetCount(); i++)
//			{
//				if(EventList[i] && EventList[i]->GetEventArgs() && EventList[i]->GetEventArgs().GetType() == CUTSCENE_CAMERA_CUT_EVENT_ARGS_TYPE )
//				{
//					const cutfCameraCutEventArgs* pCamCutEventArgs =  static_cast<const cutfCameraCutEventArgs*>(EventList[i]->GetEventArgs()); 
//
//					for( int j=0; j < pCamCutEventArgs->GetCoCModifierList().GetCount(); i++)
//					{
//						NumEvents++; 
//					}
//				}
//			}
//			
//			if(m_iTimeOfDayDofOverrideIndex == NumEvents)
//			{
//				m_iTimeOfDayDofOverrideIndex = NumEvents-1; 
//			}
//
//			m_pTimeOfDayDofOverrideCombo->UpdateCombo( "Cascade shadow bounds events", &m_iTimeOfDayDofOverrideIndex, NumEvents, NULL );
//			
//			//Clear the list of variations, need to repopulate. 
//			for(int i = 0; i < EventList.GetCount(); i++)
//			{
//				m_CascadeShadowsBoundIndexEvents[i] = NULL;  
//			}
//
//			m_CascadeShadowsBoundIndexEvents.Reset(); 
//		}
//}

void CCutSceneDebugManager::WriteOutAllDynamicDepthEvents(const atArray<CutNearShadowDepth>& depths )
{
	if (CutSceneManager::GetInstance()->GetDebugManager().m_CameraObjectList.GetCount() > 0)
	{
		const cutfObject* pObject = CutSceneManager::GetInstance()->GetDebugManager().m_CameraObjectList[0]; 

		if ( (CutSceneManager::GetInstance()->IsPaused() || CutSceneManager::GetInstance()->IsPlaying()) && pObject)
		{
			SaveDynamicDepthEventsToCutscene( depths, pObject);
			CutSceneManager::GetInstance()->GetCutfFile()->SortEvents();
			CutSceneManager::GetInstance()->SaveGameDataCutfile(); 
		}
	}
}

void  CCutSceneDebugManager::ClearAllDynamicDepthEvents()
{
	if (CutSceneManager::GetInstance()->GetDebugManager().m_CameraObjectList.GetCount() > 0)
	{
		const cutfObject* pObject = CutSceneManager::GetInstance()->GetDebugManager().m_CameraObjectList[0]; 

		if ( (CutSceneManager::GetInstance()->IsPaused() || CutSceneManager::GetInstance()->IsPlaying()) && pObject)
		{

			atArray<cutfEvent *> events;
			CutSceneManager::GetInstance()->GetCutfFile()->FindEventsForObjectIdOnly( pObject->GetObjectId(), CutSceneManager::GetInstance()->GetCutfFile()->GetEventList(), events );

			for ( int i = events.GetCount() - 1; i >= 0; --i )
			{
				if (events[i]->GetEventId() == CUTSCENE_CASCADE_SHADOWS_SET_DYNAMIC_DEPTH_VALUE)
				{
					CutSceneManager::GetInstance()->GetCutfFile()->RemoveEvent( events[i]);
				}
			}
			CutSceneManager::GetInstance()->GetCutfFile()->SortEvents();
			CutSceneManager::GetInstance()->SaveGameDataCutfile(); 
		}
	}
}

//Save the changes out to the cut tune file.
void CCutSceneDebugManager::SaveDynamicDepthEventsToCutscene( const atArray<CutNearShadowDepth>& depths, const cutfObject* pObject)
{
	if(!Verifyf(CutSceneManager::GetInstance()->GetCutfFile(), "Overlay Event: No cutf file object loaded cannot save"))
	{
		return;
	}

	atArray<cutfEvent *> EventList;
	CutSceneManager::GetInstance()->GetCutfFile()->FindEventsForObjectIdOnly( pObject->GetObjectId(), CutSceneManager::GetInstance()->GetCutfFile()->GetEventList(), EventList );

	for (int i=0; i<depths.GetCount(); i++){
		float eventTime = FindTimeForCameraCutName( EventList, depths[i].m_name);

		if ( eventTime >= 0)
		{
			Displayf(" Adding %s at time %f", depths[i].m_name, eventTime );
			cutfEventArgs* pEventArgs =  rage_new cutfFloatValueEventArgs(depths[i].m_minNearDepth);
			CutSceneManager::GetInstance()->GetCutfFile()->AddEvent( rage_new cutfObjectIdEvent( pObject->GetObjectId(), eventTime, CUTSCENE_CASCADE_SHADOWS_SET_DYNAMIC_DEPTH_VALUE, pEventArgs ) );
		}
	}
}

// PROFILING

bool CCutSceneDebugManager::IsLightDisabled(const CLightSource &light)
{
	u32	flags = light.GetFlags();

	// two main groups (cutscene and non-cutscene lights)
	if (m_bProfilingDisableCsLights && (flags & LIGHTFLAG_CUTSCENE))
	{
		return true;
	}

	return false;
}


void CCutSceneDebugManager::ValidateCutscene()
{
	const CutSceneManager* pConstCutsceneManager = const_cast<const CutSceneManager*>(CutSceneManager::GetInstance());
	const cutfCutsceneFile2* pCutfile = pConstCutsceneManager->GetCutfFile(); 
	if (pCutfile && CutSceneManager::GetInstance()->GetCamEntity() && CutSceneManager::GetInstance()->GetAnimManager())
	{
		static const crTag::Key TagKeyBlock = crTag::CalcKey("Block", 0xE433D77D);

		//float fAccumulatedSectionStartTime = 0.0f;
		//float fAccumulatedDuration = 0.0f;

		float fStartTimeOfInterest = 0.0f;
		float fEndTimeOfInterest = 0.0f;
		//create enough requests for the number of sections
		for(int i = 0; i < pConstCutsceneManager->GetSectionStartTimeList().GetCount(); i++ )
		{
			char dictName[CUTSCENE_LONG_OBJNAMELEN];	
			if(CutSceneManager::GetInstance()->GetCutsceneName())
			{
				if ( CutSceneManager::GetInstance()->GetAnimManager()->GetSectionedName( 
					dictName, CUTSCENE_LONG_OBJNAMELEN, CutSceneManager::GetInstance()->GetCutsceneName(), i, CutSceneManager::GetInstance()->GetSectionStartTimeList().GetCount() ) )
				{
					strLocalIndex iAnimDictIndex = strLocalIndex(fwAnimManager::FindSlot(dictName));
					if (cutsceneVerifyf(iAnimDictIndex != -1 , "Clip dict %s has returned an invalid slot, will not attempt to load this dict" , dictName)) 
					{
						if (cutsceneVerifyf(CStreaming::IsObjectInImage(iAnimDictIndex, fwAnimManager::GetStreamingModuleId()), "Clip dict %s is not in the image" , dictName)) 
						{
							fwAnimManager::StreamingRequest(iAnimDictIndex, STRFLAG_FORCE_LOAD | STRFLAG_PRIORITY_LOAD | STRFLAG_DONTDELETE);

							CStreaming::LoadAllRequestedObjects();

							if ( CStreaming::HasObjectLoaded(iAnimDictIndex, fwAnimManager::GetStreamingModuleId()) )
							{
								cutsceneDisplayf("Loaded Clip dict %s", dictName);

								crClipDictionary* pClipDict = fwAnimManager::Get(iAnimDictIndex);
								if (pClipDict)
								{
									u32 AnimBase = CutSceneManager::GetInstance()->GetCamEntity()->GetCameraObject()->GetAnimStreamingBase(); 
									u32 FinalHashString = 0; 
									if ( CutSceneManager::GetInstance()->GetAnimManager()->GetSectionedName( AnimBase, FinalHashString, i, CutSceneManager::GetInstance()->GetSectionStartTimeList().GetCount()) ) 
									{
										float fSectionStartTime = pConstCutsceneManager->GetSectionStartTimeList()[i];
										float fTotalDuration = pConstCutsceneManager->GetCutfFile()->GetTotalDuration();

										fStartTimeOfInterest = fSectionStartTime;

										const crClip* pClip = pClipDict->GetClip(FinalHashString);
										if(pClip)
										{
											crTagIterator it(*pClip->GetTags(), TagKeyBlock);
											while(*it)
											{
												const crTag* tagBlock = static_cast<const crTag*>(*it);

												float fStartTime = pClip->ConvertPhaseToTime(tagBlock->GetStart()) + fSectionStartTime;
												float fEndTime = pClip->ConvertPhaseToTime(tagBlock->GetEnd()) + fSectionStartTime;

												//
												// Search from last time to upto the new blocking tag
												//
												fEndTimeOfInterest = fStartTime;

												const atArray<cutfEvent *>& eventList = pCutfile->GetEventList();
												for ( int i = 0; i < eventList.GetCount(); ++i )
												{
													if (eventList[i]->GetEventId() == CUTSCENE_CAMERA_CUT_EVENT)
													{
														cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( eventList[i]->GetEventArgs());
														if(pEventArgs && pEventArgs->GetType() == CUTSCENE_CAMERA_CUT_EVENT_ARGS_TYPE)
														{		
															float fEventTime = eventList[i]->GetTime();
															if (fEventTime > fStartTimeOfInterest && fEventTime < fEndTimeOfInterest)
															{
																cutsceneDisplayf("Camera cut event: T(%.6f), F(%.6f), P(%.6f)\n", 
																	fEventTime,
																	fEventTime * 30.0f,
																	fEventTime/fTotalDuration);
															}
														}
													}
												}

												//
												// Search from the start of the new blocking tag to the end of the new blocking tag
												//
												fStartTimeOfInterest = fStartTime;
												fEndTimeOfInterest = fEndTime;
												
												cutsceneDisplayf("Start Camera Blocking Tag: T(%.6f), F(%.6f), P(%.6f)\n", 
													fStartTime,
													fStartTime * 30.0f,
													fStartTime/fTotalDuration);

												for ( int i = 0; i < eventList.GetCount(); ++i )
												{
													if (eventList[i]->GetEventId() == CUTSCENE_CAMERA_CUT_EVENT)
													{
														cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( eventList[i]->GetEventArgs());
														if(pEventArgs && pEventArgs->GetType() == CUTSCENE_CAMERA_CUT_EVENT_ARGS_TYPE)
														{		
															float fEventTime = eventList[i]->GetTime();
															if (fEventTime > fStartTimeOfInterest && fEventTime < fEndTimeOfInterest)
															{
																cutsceneDisplayf("Camera cut event: T(%.6f), F(%.6f), P(%.6f)\n", 
																	fEventTime,
																	fEventTime * 30.0f,
																	fEventTime/fTotalDuration);
															}
														}
													}
												}

												cutsceneDisplayf("End Camera Blocking Tag: T(%.6f), F(%.6f), P(%.6f)\n", 
													fEndTime, 
													fEndTime * 30.0f,
													fEndTime/fTotalDuration);

												// Remember the end time for the next search
												fStartTimeOfInterest = fEndTime;
												++it;
											}
										}

									}

								}
							}
						}
					}
				}
			}
		}

		//create enough requests for the number of sections
		for(int i = 0; i < pConstCutsceneManager->GetSectionStartTimeList().GetCount(); i++ )
		{
			char dictName[CUTSCENE_LONG_OBJNAMELEN];	
			if(CutSceneManager::GetInstance()->GetCutsceneName())
			{
				if ( CutSceneManager::GetInstance()->GetAnimManager()->GetSectionedName( 
					dictName, CUTSCENE_LONG_OBJNAMELEN, CutSceneManager::GetInstance()->GetCutsceneName(), i, CutSceneManager::GetInstance()->GetSectionStartTimeList().GetCount() ) )
				{
					strLocalIndex iAnimDictIndex = fwAnimManager::FindSlot(dictName);
					if (cutsceneVerifyf(iAnimDictIndex != -1 , "Clip dict %s has returned an invalid slot, will not attempt to load this dict" , dictName)) 
					{
						if (cutsceneVerifyf(CStreaming::IsObjectInImage(iAnimDictIndex, fwAnimManager::GetStreamingModuleId()), "Clip dict %s is not in the image" , dictName)) 
						{
							g_ClipDictionaryStore.ClearRequiredFlag(iAnimDictIndex.Get(), STRFLAG_DONTDELETE);
						}
					}
				}
			}
		}


	}
}

void CCutSceneDebugManager::BatchProcessCutfilesInMemory()
{
	cutsceneDisplayf("Begin in memory cutfile event validation.");
	CCutSceneDebugManager::IterateCutfile(MakeFunctor(*this, &CCutSceneDebugManager::ProcessCutfilesInMemory));
	cutsceneDisplayf("In memory cutfile event validation complete.");
}

void CCutSceneDebugManager::IterateCutfile(CCutfileIteratorCallback callback)
{
	m_NumberOfFailedScene = 0; 
	for (s32 c = 0; c < g_CutSceneStore.GetSize(); c++)
	{
		cutfCutsceneFile2Def* pNameDef = g_CutSceneStore.GetSlot(strLocalIndex(c));
		if(pNameDef)
		{
			atHashString cutfilename(pNameDef->m_name.GetCStr());

			strLocalIndex iIndex = g_CutSceneStore.FindSlotFromHashKey(cutfilename.GetHash());

			if (Verifyf((iIndex != -1), "Cut scene doesn't exist, and will not be loaded"))
			{
				//if (g_CutSceneStore.LoadFile(iIndex, pNameDef->m_name.GetCStr()) )

				if(g_CutSceneStore.StreamingBlockingLoad(iIndex, STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD))
				{	
					g_CutSceneStore.AddRef(iIndex, REF_OTHER);

					cutfCutsceneFile2Def* pDef = g_CutSceneStore.GetSlot(iIndex);

					if(pDef)
					{
						cutfCutsceneFile2* pCutfile = pDef->m_pObject;  

						if(pCutfile)
						{
							callback(pCutfile, cutfilename); 
						}

							
						//g_CutSceneStore.Remove(iIndex); 
					}

					g_CutSceneStore.RemoveRef(iIndex, REF_OTHER);
					g_CutSceneStore.ClearRequiredFlag(iIndex.Get(),STRFLAG_CUTSCENE_REQUIRED);
					g_CutSceneStore.StreamingRemove(iIndex);
				}
			}
		}
	}

	Displayf("Number of scenes failing validation: %d", m_NumberOfFailedScene); 
}

void CCutSceneDebugManager::ValidateEventInThisScene()
{
	if(CutSceneManager::GetInstance()->GetCutfFile())
	{
		cutsceneDisplayf("Validating: %s", CutSceneManager::GetInstance()->GetSceneHashString()->GetCStr()); 
		atHashString sceneName( CutSceneManager::GetInstance()->GetSceneHashString()->GetCStr()); 
		ProcessCutfilesInMemory(CutSceneManager::GetInstance()->GetCutfFile(),  sceneName); 
	}
}

void CCutSceneDebugManager::AddCutAttributesToShadowEvents(cutfCutsceneFile2* pCutfFile, atHashString sceneName)
{
	bool SaveGameData = false; 
	const atArray< cutfEvent * > &eventList = pCutfFile->GetEventList();
	
	atArray<const cutfObject*> cameraObjectList;	
	s32 CameraObjectId =-1; 
	pCutfFile->FindObjectsOfType(CUTSCENE_CAMERA_OBJECT_TYPE, cameraObjectList );
	

	if(cameraObjectList.GetCount() > 0)
	{
		CameraObjectId = cameraObjectList[0]->GetObjectId(); 
	}

	if(eventList.GetCount() > 0 && CameraObjectId > -1)
	{
		for(int i=0; i<eventList.GetCount(); i++)
		{
			cutfEvent *pShadowEvent = eventList[i];

			if(pShadowEvent)
			{
				if(pShadowEvent->GetEventId() == CUTSCENE_ENABLE_CASCADE_SHADOW_BOUNDS_EVENT || 
					pShadowEvent->GetEventId() == CUTSCENE_CASCADE_SHADOWS_ENABLE_ENTITY_TRACKER || 
					pShadowEvent->GetEventId() == CUTSCENE_CASCADE_SHADOWS_SET_WORLD_HEIGHT_UPDATE || 
					pShadowEvent->GetEventId() == CUTSCENE_CASCADE_SHADOWS_SET_RECEIVER_HEIGHT_UPDATE || 
					pShadowEvent->GetEventId() == CUTSCENE_CASCADE_SHADOWS_SET_AIRCRAFT_MODE || 
					pShadowEvent->GetEventId() == CUTSCENE_CASCADE_SHADOWS_SET_DYNAMIC_DEPTH_MODE || 
					pShadowEvent->GetEventId() == CUTSCENE_CASCADE_SHADOWS_SET_FLY_CAMERA_MODE || 
					pShadowEvent->GetEventId() == CUTSCENE_CASCADE_SHADOWS_SET_CASCADE_BOUNDS_HFOV || 
					pShadowEvent->GetEventId() == CUTSCENE_CASCADE_SHADOWS_SET_CASCADE_BOUNDS_VFOV || 
					pShadowEvent->GetEventId() == CUTSCENE_CASCADE_SHADOWS_SET_CASCADE_BOUNDS_SCALE || 
					pShadowEvent->GetEventId() ==  CUTSCENE_CASCADE_SHADOWS_SET_ENTITY_TRACKER_SCALE ||
					pShadowEvent->GetEventId() ==  CUTSCENE_CASCADE_SHADOWS_SET_SPLIT_Z_EXP_WEIGHT|| 
					pShadowEvent->GetEventId() ==  CUTSCENE_CASCADE_SHADOWS_SET_DITHER_RADIUS_SCALE|| 
					pShadowEvent->GetEventId() ==  CUTSCENE_CASCADE_SHADOWS_SET_WORLD_HEIGHT_MINMAX|| 
					pShadowEvent->GetEventId() ==  CUTSCENE_CASCADE_SHADOWS_SET_RECEIVER_HEIGHT_MINMAX|| 
					pShadowEvent->GetEventId() ==  CUTSCENE_CASCADE_SHADOWS_SET_DEPTH_BIAS|| 
					pShadowEvent->GetEventId() ==  CUTSCENE_CASCADE_SHADOWS_SET_SLOPE_BIAS|| 
					pShadowEvent->GetEventId() ==  CUTSCENE_CASCADE_SHADOWS_SET_SHADOW_SAMPLE_TYPE || 
					pShadowEvent->GetEventId() ==  CUTSCENE_CASCADE_SHADOWS_SET_DYNAMIC_DEPTH_VALUE ||
					pShadowEvent->GetEventId() == CUTSCENE_CASCADE_SHADOWS_RESET_CASCADE_SHADOWS)
				{
					s32 eventIndex; 

					atHashString cameraCutName = FindNearestCameraCutNameForTime(eventList, pShadowEvent->GetTime() , eventIndex);

					if(eventIndex > -1)
					{
						const cutfEventArgs* pEventArgs = pShadowEvent->GetEventArgs();

						if(pShadowEvent->GetEventId() == CUTSCENE_CASCADE_SHADOWS_RESET_CASCADE_SHADOWS)
						{
							parAttributeList CutAttribute; 
							CutAttribute.AddAttribute("CamCut", cameraCutName.GetCStr()); 
				
							cutfEventArgs*  pNewEventArgs = rage_new cutfBoolValueEventArgs(true, CutAttribute);
							
							CutSceneManager::GetInstance()->GetCutfFile()->AddEvent( rage_new cutfObjectIdEvent( CameraObjectId, pShadowEvent->GetTime(), CUTSCENE_CASCADE_SHADOWS_RESET_CASCADE_SHADOWS, pNewEventArgs ) );
						
							CutSceneManager::GetInstance()->GetCutfFile()->RemoveEvent(pShadowEvent);

							CutSceneManager::GetInstance()->GetCutfFile()->SortEvents();

							pEventArgs = pNewEventArgs; 

						}
						if(pEventArgs)
						{
							parAttributeList& pAttributeList = const_cast<parAttributeList&>(pEventArgs->GetAttributeList());

							if(!pAttributeList.FindAttribute("CamCut"))
							{
								pAttributeList.AddAttribute("CamCut", cameraCutName.GetCStr()); 
								SaveGameData = true; 
							}
						}
					}
				}
			}
		}
	}

	if(SaveGameData)
	{
		if(CutSceneManager::GetInstance()->IsCutscenePlayingBack())
		{

			CutSceneManager::GetInstance()->SaveGameDataCutfile(); 
		}
		else
		{
			CutSceneManager::GetInstance()->cOrignalSceneName = sceneName.GetCStr(); 
			CutSceneManager::GetInstance()->SetCutfFile(pCutfFile); 

			CutSceneManager::GetInstance()->SaveGameDataCutfile(); 

			CutSceneManager::GetInstance()->SetCutfFile(NULL); 
			CutSceneManager::GetInstance()->cOrignalSceneName.Clear(); 
		}
	}

}

void CCutSceneDebugManager::PopulateFirstPersonEventCameraComboBox()
{
	if(m_firstPersonBlendOutComboSlider)
	{
		m_firstPersonBlendOutComboIndex = 0; 
		if(m_BlendOutEvents.GetCount() == 0)
		{
			if(m_firstPersonBlendOutComboSlider)
			{
				m_firstPersonBlendOutComboSlider->UpdateCombo( "First person Camera Event", &m_firstPersonBlendOutComboIndex, 1, NULL);
				m_firstPersonBlendOutComboSlider->SetString( 0, "");
				return; 
			}
		}
		else
		{
			m_firstPersonBlendOutComboIndex =0; 
		}

		m_firstPersonBlendOutComboSlider->UpdateCombo( "First person Camera Event", &m_firstPersonBlendOutComboIndex, m_BlendOutEvents.GetCount(), NULL);

		for ( int i = 0; i < m_BlendOutEvents.GetCount(); ++i )
		{
			if(m_BlendOutEvents[i]->GetEventId() == CUTSCENE_FIRST_PERSON_BLENDOUT_CAMERA_EVENT)
			{
				const cutfFloatValueEventArgs* pEventArgs = static_cast<const cutfFloatValueEventArgs*> (m_BlendOutEvents[i]->GetEventArgs()); 

				atVarString Event("CUTSCENE_FIRST_PERSON_BLENDOUT_CAMERA_EVENT: Time: %f, Duration: %f Concat Section: %d", m_BlendOutEvents[i]->GetTime(), 
					pEventArgs->GetFloat1(),  CutSceneManager::GetInstance()->GetConcatSectionForTime(m_BlendOutEvents[i]->GetTime())); 

				m_firstPersonBlendOutComboSlider->SetString( i, Event.c_str());
			}

			if(m_BlendOutEvents[i]->GetEventId() == CUTSCENE_FIRST_PERSON_CATCHUP_CAMERA_EVENT)
			{
				atVarString Event("CUTSCENE_FIRST_PERSON_CATCHUP_CAMERA_EVENT: Time: %f, Concat Section: %d", m_BlendOutEvents[i]->GetTime(), 
					CutSceneManager::GetInstance()->GetConcatSectionForTime(m_BlendOutEvents[i]->GetTime())); 

				m_firstPersonBlendOutComboSlider->SetString( i, Event.c_str());
			}
		}
	}
}


bool CCutSceneDebugManager::CanAddEventFirstPersonCameraEvent()
{
	bool canAddEventForConcatSection = true; 

	if(CutSceneManager::GetInstance()->IsConcatted())
	{
		for(int i=0; i < m_BlendOutEvents.GetCount(); i++)
		{
			int ConcatSectionForTime = CutSceneManager::GetInstance()->GetConcatSectionForTime(CutSceneManager::GetInstance()->GetTime());

			if(ConcatSectionForTime != -1)
			{
				int concatsecionForEvent = CutSceneManager::GetInstance()->GetConcatSectionForTime(m_BlendOutEvents[i]->GetTime());

				if(concatsecionForEvent == ConcatSectionForTime)
				{
					canAddEventForConcatSection = false; 
					if(m_firstPersonEventStatus)
					{
						m_firstPersonEventStatus->SetString("ADD EVENT FAIL: CAN ONLY HAVE ONE FIRST PERSON CAMERA EVENT PER CONCAT SECTION, PLEASE DELETE CURRENT EVENT");
					}
				}
			}
			else
			{
				if(m_firstPersonEventStatus)
				{
					m_firstPersonEventStatus->SetString("ADD EVENT FAIL: TRYING TO ADD AN EVENT TO AN INVALID SECTION");
				}
				cutsceneAssertf(0, "AddFirstPersonBlendOutEvent:: Trying to add event to an invalid section"); 
				canAddEventForConcatSection = false; 
			}
		}
	}
	else
	{
		if(m_BlendOutEvents.GetCount() > 0)
		{
			canAddEventForConcatSection = false; 

			if(m_firstPersonEventStatus)
			{
				m_firstPersonEventStatus->SetString("ADD EVENT FAIL: CAN ONLY HAVE ONE FIRST PERSON CAMERA EVENT PER CONCAT SECTION, PLEASE DELETE CURRENT EVENT");
			}
		}
	}
	return canAddEventForConcatSection; 
}


void CCutSceneDebugManager::AddFirstPersonBlendOutEvent()
{
	if(m_CameraObjectList.GetCount() > 0 && m_CameraObjectList[0])
	{
		if(CanAddEventFirstPersonCameraEvent())
		{
			cutfFloatValueEventArgs* pEventArgs = rage_new cutfFloatValueEventArgs(m_firstPersonCameraBlendDuration);

			CutSceneManager::GetInstance()->GetCutfFile()->AddEvent( rage_new cutfObjectIdEvent( m_CameraObjectList[0]->GetObjectId(), CutSceneManager::GetInstance()->GetTime(), CUTSCENE_FIRST_PERSON_BLENDOUT_CAMERA_EVENT, pEventArgs ) );		
			CutSceneManager::GetInstance()->GetCutfFile()->SortEvents();

			CCutSceneCameraEntity* pCamera = static_cast<CCutSceneCameraEntity*>(CutSceneManager::GetInstance()->GetEntityByObjectId( m_CameraObjectList[0]->GetObjectId())); 
			pCamera->SetCameraHasFirstPersonBlendOutEvent(true); 
			UpdateFirstPersonEventList();

			if(m_firstPersonEventStatus)
			{
				m_firstPersonEventStatus->SetString("SUCCESSFULLY ADDED EVENT - NOT SAVED");
			}
		}
	}
}

void CCutSceneDebugManager::AddFirstPersonCatchUpEvent()
{
	if(m_CameraObjectList.GetCount() > 0 && m_CameraObjectList[0])
	{
		if(CanAddEventFirstPersonCameraEvent())
		{
			CutSceneManager::GetInstance()->GetCutfFile()->AddEvent( rage_new cutfObjectIdEvent( m_CameraObjectList[0]->GetObjectId(), CutSceneManager::GetInstance()->GetTime(), CUTSCENE_FIRST_PERSON_CATCHUP_CAMERA_EVENT, NULL ) );		
			CutSceneManager::GetInstance()->GetCutfFile()->SortEvents();
			CCutSceneCameraEntity* pCamera = static_cast<CCutSceneCameraEntity*>(CutSceneManager::GetInstance()->GetEntityByObjectId( m_CameraObjectList[0]->GetObjectId())); 
			
			if(pCamera)
			{
				pCamera->SetCameraHasFirstPersonBlendOutEvent(true); 
			}


			UpdateFirstPersonEventList();

			if(m_firstPersonEventStatus)
			{
				m_firstPersonEventStatus->SetString("SUCCESSFULLY ADDED EVENT - NOT SAVED");
			}
		}
	}
}

void CCutSceneDebugManager::DeleteFirstPersonCameraEvent()
{
	if(m_firstPersonBlendOutComboIndex < m_BlendOutEvents.GetCount())
	{
		if(m_BlendOutEvents[m_firstPersonBlendOutComboIndex])
		{
			CutSceneManager::GetInstance()->GetCutfFile()->RemoveEvent(m_BlendOutEvents[m_firstPersonBlendOutComboIndex]); 
			CutSceneManager::GetInstance()->GetCutfFile()->SortEvents();
		
			CCutSceneCameraEntity* pCamera = static_cast<CCutSceneCameraEntity*>(CutSceneManager::GetInstance()->GetEntityByObjectId( m_CameraObjectList[0]->GetObjectId())); 
			
			if(pCamera)
			{
				pCamera->SetCameraHasFirstPersonBlendOutEvent(false); 
			}
			
			if(m_firstPersonEventStatus)
			{
				m_firstPersonEventStatus->SetString("SUCCESSFULLY DELETED EVENT - NOT SAVED");
			}

			UpdateFirstPersonEventList(); 
		}
	}
}

void CCutSceneDebugManager::SaveFirstPersonCameraEvents()
{
	CutSceneManager::GetInstance()->SaveGameDataCutfile(); 

	if(m_firstPersonEventStatus)
	{
		m_firstPersonEventStatus->SetString("SAVED");
	}
}

void CCutSceneDebugManager::UpdateFirstPersonEventList()
{
	if(m_CameraObjectList.GetCount() > 0 && m_CameraObjectList[0])
	{
		for(int i=0; i<m_BlendOutEvents.GetCount(); i++)
		{
			m_BlendOutEvents[i] = NULL; 
		}

		m_BlendOutEvents.Reset(); 
		
		if(CutSceneManager::GetInstance()->GetCutfFile())
		{
			atArray<cutfEvent *> AllEventList;
			CutSceneManager::GetInstance()->GetCutfFile()->FindEventsForObjectIdOnly( m_CameraObjectList[0]->GetObjectId(), CutSceneManager::GetInstance()->GetCutfFile()->GetEventList(), AllEventList );

			for(int i =0; i < AllEventList.GetCount(); i++)
			{
				if(AllEventList[i]->GetEventId() == CUTSCENE_FIRST_PERSON_BLENDOUT_CAMERA_EVENT || AllEventList[i]->GetEventId() == CUTSCENE_FIRST_PERSON_CATCHUP_CAMERA_EVENT )
				{
					m_BlendOutEvents.PushAndGrow(AllEventList[i]); 
				}
			}
		}
	}
	else
	{
		m_BlendOutEvents.Reset(); 
	}

	PopulateFirstPersonEventCameraComboBox(); 
}



void CCutSceneDebugManager::ProcessCutfilesInMemory(cutfCutsceneFile2* pCutfile, atHashString sceneName)
{
	if(pCutfile)
	{
		bool success = true; 
		atVector<CameraCutBlockingTagInfo> TagInfo; 
		
		if(m_cutfileValidationProcesses[CVP_SHADOW_EVENTS] || m_cutfileValidationProcesses[CVP_LIGHT_EVENTS])
		{
			ValidateCameraCutData(pCutfile, sceneName, TagInfo);
		}

		if (m_cutfileValidationProcesses[CVP_SHADOW_EVENTS] || m_cutfileValidationProcesses[CVP_FIX_SHADOW_EVENTS])
		{	
			success = ValidateShadowData(pCutfile, sceneName, TagInfo, m_cutfileValidationProcesses[CVP_FIX_SHADOW_EVENTS]);
		}

		if (m_cutfileValidationProcesses[CVP_LIGHT_EVENTS])
		{	
			success = ValidateLightData(pCutfile, sceneName, TagInfo);
		}

		if(!success)
		{
			m_NumberOfFailedScene++; 
		}
	}
}

void CCutSceneDebugManager::ValidateCameraCutData(cutfCutsceneFile2* pCutfile, atHashString sceneName, atVector<CameraCutBlockingTagInfo> &BlockingTagInfo )
{
	//const CutSceneManager* pConstCutsceneManager = const_cast<const CutSceneManager*>(CutSceneManager::GetInstance());
	//const cutfCutsceneFile2* pCutfile = pConstCutsceneManager->GetCutfFile(); 
	
	const atArray< cutfObject * > &objectList = pCutfile->GetObjectList();
	cutfObject *pObject = NULL;
	CCutSceneAnimMgrEntity* pAnimManager = NULL; 
	CCutSceneCameraEntity* pCamEntity = NULL; 
	atArray<float> fSectionStartTimeList;
	pCutfile->GetCurrentSectionTimeList( fSectionStartTimeList );

	for(int i = 0; i < objectList.GetCount(); i ++)
	{
		pObject = objectList[i];
		if(Verifyf(pObject, "pObject is NULL!") && pObject->GetType() == CUTSCENE_ANIMATION_MANAGER_OBJECT_TYPE)
		{
			pAnimManager = rage_new CCutSceneAnimMgrEntity ( pObject); 
		}

		if(Verifyf(pObject, "pObject is NULL!") && pObject->GetType() == CUTSCENE_CAMERA_OBJECT_TYPE)
		{
			pCamEntity = rage_new CCutSceneCameraEntity ( pObject); 
		}
	}

	if (pCutfile && pCamEntity && pAnimManager)
	{
		static const crTag::Key TagKeyBlock = crTag::CalcKey("Block", 0xE433D77D);

		//float fAccumulatedSectionStartTime = 0.0f;
		//float fAccumulatedDuration = 0.0f;

		float fStartTimeOfInterest = 0.0f;
		float fEndTimeOfInterest = 0.0f;
		//create enough requests for the number of sections
		for(int i = 0; i < fSectionStartTimeList.GetCount(); i++ )
		{
			char dictName[CUTSCENE_LONG_OBJNAMELEN];	
			if(sceneName.GetCStr())
			{
				if ( pAnimManager->GetSectionedName( 
					dictName, CUTSCENE_LONG_OBJNAMELEN, sceneName.GetCStr(), i, fSectionStartTimeList.GetCount() ) )
				{
					strLocalIndex iAnimDictIndex = strLocalIndex(fwAnimManager::FindSlot(dictName));
					if (cutsceneVerifyf(iAnimDictIndex != -1 , "Clip dict %s has returned an invalid slot, will not attempt to load this dict" , dictName)) 
					{
						if (cutsceneVerifyf(CStreaming::IsObjectInImage(iAnimDictIndex, fwAnimManager::GetStreamingModuleId()), "Clip dict %s is not in the image" , dictName)) 
						{
							fwAnimManager::StreamingRequest(iAnimDictIndex, STRFLAG_FORCE_LOAD | STRFLAG_PRIORITY_LOAD | STRFLAG_DONTDELETE);

							CStreaming::LoadAllRequestedObjects();

							if ( CStreaming::HasObjectLoaded(iAnimDictIndex, fwAnimManager::GetStreamingModuleId()) )
							{
							//	cutsceneDisplayf("Loaded Clip dict %s", dictName);

								crClipDictionary* pClipDict = fwAnimManager::Get(iAnimDictIndex);
								if (pClipDict)
								{
									u32 AnimBase = pCamEntity->GetCameraObject()->GetAnimStreamingBase(); 
									u32 FinalHashString = 0; 
									if ( pAnimManager->GetSectionedName( AnimBase, FinalHashString, i, fSectionStartTimeList.GetCount()) ) 
									{
										float fSectionStartTime = fSectionStartTimeList[i];
										//float fTotalDuration = pCutfile->GetTotalDuration();

										fStartTimeOfInterest = fSectionStartTime;

										const crClip* pClip = pClipDict->GetClip(FinalHashString);
										if(pClip)
										{
											crTagIterator it(*pClip->GetTags(), TagKeyBlock);
											while(*it)
											{
												CameraCutBlockingTagInfo& blockingTag = BlockingTagInfo.Grow(); 

												const crTag* tagBlock = static_cast<const crTag*>(*it);

												float fStartTime = pClip->ConvertPhaseToTime(tagBlock->GetStart()) + fSectionStartTime;
												float fEndTime = pClip->ConvertPhaseToTime(tagBlock->GetEnd()) + fSectionStartTime;

												//
												// Search from last time to upto the new blocking tag
												//
												fEndTimeOfInterest = fStartTime;

												const atArray<cutfEvent *>& eventList = pCutfile->GetEventList();

												for ( int i = 0; i < eventList.GetCount(); ++i )
												{
													if (eventList[i]->GetEventId() == CUTSCENE_CAMERA_CUT_EVENT)
													{
														cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( eventList[i]->GetEventArgs());
														if(pEventArgs && pEventArgs->GetType() == CUTSCENE_CAMERA_CUT_EVENT_ARGS_TYPE)
														{		
															float fEventTime = eventList[i]->GetTime();
															if (fEventTime > fStartTimeOfInterest && fEventTime < fEndTimeOfInterest)
															{
																blockingTag.index = i; 
																blockingTag.eventTime = fEventTime; 
															}
														}
													}
												}

												//
												// Search from the start of the new blocking tag to the end of the new blocking tag
												//
												fStartTimeOfInterest = fStartTime;
												fEndTimeOfInterest = fEndTime;

												blockingTag.startBlockingTagTime = fStartTime; 

												for ( int i = 0; i < eventList.GetCount(); ++i )
												{
													if (eventList[i]->GetEventId() == CUTSCENE_CAMERA_CUT_EVENT)
													{
														cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( eventList[i]->GetEventArgs());
														if(pEventArgs && pEventArgs->GetType() == CUTSCENE_CAMERA_CUT_EVENT_ARGS_TYPE)
														{		
															float fEventTime = eventList[i]->GetTime();
															if (fEventTime > fStartTimeOfInterest && fEventTime < fEndTimeOfInterest)
															{
																/*			cutsceneDisplayf("Camera cut event: T(%.6f), F(%.6f), P(%.6f)\n", 
																fEventTime,
																fEventTime * 30.0f,
																fEventTime/fTotalDuration);*/

																blockingTag.index = i; 
																blockingTag.eventTime = fEventTime; 
															}
														}
													}
												}

												/*		cutsceneDisplayf("End Camera Blocking Tag: T(%.6f), F(%.6f), P(%.6f)\n", 
												fEndTime, 
												fEndTime * 30.0f,
												fEndTime/fTotalDuration);*/

												blockingTag.endBlockingTagTime = fEndTime; 

												// Remember the end time for the next search
												fStartTimeOfInterest = fEndTime;
												++it;
											}
										}

									}

								}
							}
						}
					}
				}
			}
		}

		//create enough requests for the number of sections
		for(int i = 0; i < fSectionStartTimeList.GetCount(); i++ )
		{
			char dictName[CUTSCENE_LONG_OBJNAMELEN];	
			if(sceneName.GetCStr())
			{
				if ( pAnimManager->GetSectionedName( 
					dictName, CUTSCENE_LONG_OBJNAMELEN, sceneName.GetCStr(), i, fSectionStartTimeList.GetCount() ) )
				{
					strLocalIndex iAnimDictIndex = fwAnimManager::FindSlot(dictName);
					if (cutsceneVerifyf(iAnimDictIndex != -1 , "Clip dict %s has returned an invalid slot, will not attempt to load this dict" , dictName)) 
					{
						if (cutsceneVerifyf(CStreaming::IsObjectInImage(iAnimDictIndex, fwAnimManager::GetStreamingModuleId()), "Clip dict %s is not in the image" , dictName)) 
						{
							g_ClipDictionaryStore.ClearRequiredFlag(iAnimDictIndex.Get(), STRFLAG_DONTDELETE);
						}
					}
				}
			}
		}
	}

	//widen any blocking tags which overlap
	for(int i=0; i<BlockingTagInfo.GetCount(); i++)
	{
		if(BlockingTagInfo[i].index == -1)
		{
			if(i>0)
			{
				if(AreNearlyEqual(BlockingTagInfo[i-1].endBlockingTagTime, BlockingTagInfo[i].startBlockingTagTime, SMALL_FLOAT))
				{
					BlockingTagInfo[i-1].endBlockingTagTime = BlockingTagInfo[i].endBlockingTagTime; 
				}
			}
		}
	}

	//delete blocking tags without a camera cut index
	for(int i= BlockingTagInfo.GetCount()-1; i>=0; i--)
	{
		if(BlockingTagInfo[i].index == -1)
		{
			BlockingTagInfo.Delete(i); 
		}
	}

	if(pAnimManager)
	{
		delete pAnimManager; 
	}

	if(pCamEntity)
	{
		delete pCamEntity; 
	}
};


bool CCutSceneDebugManager::ValidateShadowData(cutfCutsceneFile2* pCutfFile, atHashString sceneName, const atVector<CameraCutBlockingTagInfo> &BlockingTagInfo, bool fixupEvents)
{
	bool ValidaitionSuccess = true; 

	const atArray< cutfEvent * > &eventList = pCutfFile->GetEventList();
	if(eventList.GetCount() > 0)
	{
		const atArray< cutfObject * > &objectList = pCutfFile->GetObjectList();
		cutfObject *pObject = NULL;

		for(int i = 0; i < objectList.GetCount(); i ++)
		{
			pObject = objectList[i];
			if(Verifyf(pObject, "pObject is NULL!") && pObject->GetType() == CUTSCENE_CAMERA_OBJECT_TYPE)
			{
				break; 
			}
		}

		s32 cameraCutIndex = -2; 
		s32 SecondCameraCutIndex = -2; 

		if(pObject && pObject->GetType() == CUTSCENE_CAMERA_OBJECT_TYPE)
		{
			//cutfCameraObject *pCameraObject = static_cast< cutfCameraObject * >(pObject);
			bool bSearchshadowEvents = false; 
			float firstEventTime = 0.0f; 
			float secondEventTime = pCutfFile->GetTotalDuration(); 
			bool hasValidCameraCutEvents = false; 

			for(int j = 0; j < eventList.GetCount(); j ++)
			{
				cutfEvent *pCutEvent = eventList[j];

				if(Verifyf(pCutEvent, "pCutEvent is NULL!") && pCutEvent->GetEventId() == CUTSCENE_CAMERA_CUT_EVENT)
				{
					if(SecondCameraCutIndex < 0 && cameraCutIndex < 0)
					{
						cameraCutIndex = j; 
						hasValidCameraCutEvents = true; 
					}
					else
					{
						SecondCameraCutIndex = j; 
						if(SecondCameraCutIndex >= 0 && cameraCutIndex >= 0)
						{
							secondEventTime = eventList[SecondCameraCutIndex]->GetTime(); 
							firstEventTime = eventList[cameraCutIndex]->GetTime(); 
							bSearchshadowEvents = true; 
						}

					}
				}

				if(j == eventList.GetCount()-1)
				{
					if(pCutEvent->GetEventId() != CUTSCENE_CAMERA_CUT_EVENT)
					{
						secondEventTime = pCutfFile->GetTotalDuration(); 
						SecondCameraCutIndex = eventList.GetCount(); 
						if(cameraCutIndex >= 0 )
						{
							firstEventTime = eventList[cameraCutIndex]->GetTime(); 
						}
						else
						{
							cameraCutIndex = 0; 
							cutsceneDisplayf("WARNING: %s: HAS NO CAMERA CUT EVENTS", sceneName.GetCStr()); 
							return false; 
						}
						bSearchshadowEvents = true; 
					}
				}

				if(bSearchshadowEvents)
				{
					for(int k = cameraCutIndex; k < SecondCameraCutIndex; k ++)
					{
						cutfEvent *pShadowEvent = eventList[k];
						if(pShadowEvent)
						{
							if(pShadowEvent->GetEventId() == CUTSCENE_ENABLE_CASCADE_SHADOW_BOUNDS_EVENT || 
								pShadowEvent->GetEventId() == CUTSCENE_CASCADE_SHADOWS_ENABLE_ENTITY_TRACKER || 
								pShadowEvent->GetEventId() == CUTSCENE_CASCADE_SHADOWS_SET_WORLD_HEIGHT_UPDATE || 
								pShadowEvent->GetEventId() == CUTSCENE_CASCADE_SHADOWS_SET_RECEIVER_HEIGHT_UPDATE || 
								pShadowEvent->GetEventId() == CUTSCENE_CASCADE_SHADOWS_SET_AIRCRAFT_MODE || 
								pShadowEvent->GetEventId() == CUTSCENE_CASCADE_SHADOWS_SET_DYNAMIC_DEPTH_MODE || 
								pShadowEvent->GetEventId() == CUTSCENE_CASCADE_SHADOWS_SET_FLY_CAMERA_MODE || 
								pShadowEvent->GetEventId() == CUTSCENE_CASCADE_SHADOWS_SET_CASCADE_BOUNDS_HFOV || 
								pShadowEvent->GetEventId() == CUTSCENE_CASCADE_SHADOWS_SET_CASCADE_BOUNDS_VFOV || 
								pShadowEvent->GetEventId() == CUTSCENE_CASCADE_SHADOWS_SET_CASCADE_BOUNDS_SCALE || 
								pShadowEvent->GetEventId() ==  CUTSCENE_CASCADE_SHADOWS_SET_ENTITY_TRACKER_SCALE ||
								pShadowEvent->GetEventId() ==  CUTSCENE_CASCADE_SHADOWS_SET_SPLIT_Z_EXP_WEIGHT|| 
								pShadowEvent->GetEventId() ==  CUTSCENE_CASCADE_SHADOWS_SET_DITHER_RADIUS_SCALE|| 
								pShadowEvent->GetEventId() ==  CUTSCENE_CASCADE_SHADOWS_SET_WORLD_HEIGHT_MINMAX|| 
								pShadowEvent->GetEventId() ==  CUTSCENE_CASCADE_SHADOWS_SET_RECEIVER_HEIGHT_MINMAX|| 
								pShadowEvent->GetEventId() ==  CUTSCENE_CASCADE_SHADOWS_SET_DEPTH_BIAS|| 
								pShadowEvent->GetEventId() ==  CUTSCENE_CASCADE_SHADOWS_SET_SLOPE_BIAS|| 
								pShadowEvent->GetEventId() ==  CUTSCENE_CASCADE_SHADOWS_SET_SHADOW_SAMPLE_TYPE || 
								pShadowEvent->GetEventId() ==  CUTSCENE_CASCADE_SHADOWS_SET_DYNAMIC_DEPTH_VALUE ||
								pShadowEvent->GetEventId() == CUTSCENE_CASCADE_SHADOWS_RESET_CASCADE_SHADOWS)
							{								

								int cameraCuttoCompare = cameraCutIndex; 
								float nearestCutTime = firstEventTime; 

								float firstEventTimeDifference = ABS(pShadowEvent->GetTime() - firstEventTime); 
								float SecondEventTimeDifference = ABS(pShadowEvent->GetTime() - secondEventTime); 

								if(SecondEventTimeDifference < firstEventTimeDifference)
								{
									cameraCuttoCompare = SecondCameraCutIndex; 
									nearestCutTime = secondEventTime; 
								}

								bool isEventValid = false; 
								bool useBlockingTag = false; 

								for(int blockingTagIndex = 0; blockingTagIndex < BlockingTagInfo.GetCount(); blockingTagIndex++)
								{
									if(BlockingTagInfo[blockingTagIndex].index == cameraCuttoCompare)
									{							
										if(pShadowEvent->GetTime() >= BlockingTagInfo[blockingTagIndex].startBlockingTagTime && pShadowEvent->GetTime() <= BlockingTagInfo[blockingTagIndex].endBlockingTagTime)
										{
											isEventValid = true; 

										}

										useBlockingTag = true; 

										break;
									}
								}

								if(!useBlockingTag)
								{
									if(AreNearlyEqual(pShadowEvent->GetTime(), nearestCutTime, SMALL_FLOAT))
									{
										isEventValid = true;
									}
								}


								if(!isEventValid)
								{
									if(ValidaitionSuccess)
									{
										cutsceneDisplayf("%s: FAILED SHADOW EVENT VALIDATION", sceneName.GetCStr()); 
									}

									cutsceneDisplayf("Event '%s' (%f) IS NOT LINKED TO A CAMERA CUT - nearest camera cut is %f", pShadowEvent->GetDisplayName(), pShadowEvent->GetTime(), nearestCutTime); 
									ValidaitionSuccess = false; 
								}
								if(fixupEvents)
								{
									if(cameraCuttoCompare >= 0 && cameraCuttoCompare < eventList.GetCount())
									{
										if(eventList[cameraCuttoCompare]->GetEventId() == CUTSCENE_CAMERA_CUT_EVENT)
										{
											cutfEventArgs *pEventArgs = const_cast<cutfEventArgs *>( eventList[cameraCuttoCompare]->GetEventArgs() );

											if(pEventArgs->GetType() == CUTSCENE_CAMERA_CUT_EVENT_ARGS_TYPE )
											{
												cutfCameraCutEventArgs* pObjectVar = static_cast<cutfCameraCutEventArgs*>(pEventArgs); 

												if(pObjectVar)
												{
													atHashString cameraCutName = pObjectVar->GetName(); 
													pShadowEvent->SetIsChild(true); 
													pShadowEvent->SetStickyEventId(cameraCutName.GetHash()); 
													pShadowEvent->SetTime(0.0f);
												}
											}
										}
									}
									else if(nearestCutTime == pCutfFile->GetTotalDuration())
									{
										atHashString cameraCutName("endofscenecut"); 

										pShadowEvent->SetIsChild(true); 
										pShadowEvent->SetTime(0.0f);
										pShadowEvent->SetStickyEventId(cameraCutName.GetHash()); 
									}

								}

							}
						}

					}

					cameraCutIndex = SecondCameraCutIndex; 
					SecondCameraCutIndex = -1; 
					bSearchshadowEvents = false; 
				}
			}
		}
	}


	if(!ValidaitionSuccess && fixupEvents)
	{
		pCutfFile->SortEvents(); 
		if(CutSceneManager::GetInstance()->IsCutscenePlayingBack())
		{

			CutSceneManager::GetInstance()->SaveGameDataCutfile(); 
		}
		else
		{
			CutSceneManager::GetInstance()->cOrignalSceneName = sceneName.GetCStr(); 
			CutSceneManager::GetInstance()->SetCutfFile(pCutfFile); 

			CutSceneManager::GetInstance()->SaveGameDataCutfile(); 

			CutSceneManager::GetInstance()->SetCutfFile(NULL); 
			CutSceneManager::GetInstance()->cOrignalSceneName.Clear(); 
		}

	}

	return ValidaitionSuccess; 
}

bool CCutSceneDebugManager::ValidateLightData(cutfCutsceneFile2* pCutfFile, atHashString sceneName, const atVector<CameraCutBlockingTagInfo> &BlockingTagInfo)
{
	bool ValidaitionSuccess = true; 

	const atArray< cutfEvent * > &eventList = pCutfFile->GetEventList();
	if(eventList.GetCount() > 0)
	{
		const atArray< cutfObject * > &objectList = pCutfFile->GetObjectList();
		cutfObject *pObject = NULL;

		for(int i = 0; i < objectList.GetCount(); i ++)
		{
			pObject = objectList[i];
			if(Verifyf(pObject, "pObject is NULL!") && pObject->GetType() == CUTSCENE_CAMERA_OBJECT_TYPE)
			{
				break; 
			}
		}

		s32 cameraCutIndex = -1; 
		s32 SecondCameraCutIndex = -1; 

		if(pObject && pObject->GetType() == CUTSCENE_CAMERA_OBJECT_TYPE)
		{
			bool bSearchshadowEvents = false; 
			bool bHaveCameraCutBookendEvents = false; 

			for(int j = 0; j < eventList.GetCount(); j ++)
			{
				cutfEvent *pCutEvent = eventList[j];

				if(Verifyf(pCutEvent, "pCutEvent is NULL!") && pCutEvent->GetEventId() == CUTSCENE_CAMERA_CUT_EVENT)
				{
					if(SecondCameraCutIndex == -1 && cameraCutIndex == -1)
					{
						cameraCutIndex = j; 
					}
					else
					{
						SecondCameraCutIndex = j; 
						bSearchshadowEvents = true; 
						bHaveCameraCutBookendEvents = true; 
					}
				}

				if(j == eventList.GetCount()-1)
				{
					if(pCutEvent->GetEventId() != CUTSCENE_CAMERA_CUT_EVENT)
					{
						bHaveCameraCutBookendEvents = false; 
						SecondCameraCutIndex = eventList.GetCount(); 
						bSearchshadowEvents = true; 

						if(cameraCutIndex != -1)
						{
							bSearchshadowEvents = true; 
						}
					}
				}

				if(bSearchshadowEvents)
				{
					for(int k = cameraCutIndex; k < SecondCameraCutIndex; k ++)
					{
						cutfEvent *pTargetEvent = eventList[k];
						if(pTargetEvent)
						{
							if(pTargetEvent->GetEventId() == CUTSCENE_SET_LIGHT_EVENT || pTargetEvent->GetEventId() == CUTSCENE_CLEAR_LIGHT_EVENT)
							{								
								int cameraCuttoCompare = cameraCutIndex; 

								if(bHaveCameraCutBookendEvents)
								{
									if(cameraCutIndex != -1  && SecondCameraCutIndex !=-1)
									{
										float firstEventTimeDifference = ABS(pTargetEvent->GetTime() - eventList[cameraCutIndex]->GetTime()); 
										float SecondEventTimeDifference = ABS(pTargetEvent->GetTime() - eventList[SecondCameraCutIndex]->GetTime()); 

										if(SecondEventTimeDifference < firstEventTimeDifference)
										{
											cameraCuttoCompare = SecondCameraCutIndex; 
										}
									}
								}

								bool isEventValid = false; 
								bool useBlockingTag = false; 

								for(int blockingTagIndex = 0; blockingTagIndex < BlockingTagInfo.GetCount(); blockingTagIndex++)
								{
									if(BlockingTagInfo[blockingTagIndex].index == cameraCuttoCompare)
									{							
										if(pTargetEvent->GetTime() >= BlockingTagInfo[blockingTagIndex].startBlockingTagTime && pTargetEvent->GetTime() <= BlockingTagInfo[blockingTagIndex].endBlockingTagTime)
										{
											isEventValid = true; 

										}

										useBlockingTag = true; 

										break;
									}
								}

								if(!useBlockingTag)
								{
									if(AreNearlyEqual(pTargetEvent->GetTime(), eventList[cameraCuttoCompare]->GetTime(), SMALL_FLOAT))
									{
										isEventValid = true;
									}
								}

								if(!isEventValid && !bHaveCameraCutBookendEvents && pTargetEvent->GetEventId() == CUTSCENE_CLEAR_LIGHT_EVENT)
								{
									//check the clear event 
									if(AreNearlyEqual(pTargetEvent->GetTime(),  pCutfFile->GetTotalDuration(), SMALL_FLOAT))
									{
										isEventValid = true;
									}
									else
									{
										cutsceneDisplayf("%s: WARNING: Event %s(%f) is not parented but is past the final cut but not at scene end", sceneName.GetCStr(), pTargetEvent->GetDisplayName(), pTargetEvent->GetTime()); 
									}
								}
								
								if(!isEventValid)
								{
									if(ValidaitionSuccess)
									{
										cutsceneDisplayf("%s: FAILED LIGHT EVENT VALIDATION", sceneName.GetCStr()); 
									}

									cutsceneDisplayf("Event '%s' (%f) IS NOT LINKED TO A CAMERA CUT - nearest camera cut is %f", pTargetEvent->GetDisplayName(), pTargetEvent->GetTime(), eventList[cameraCuttoCompare]->GetTime()); 
									ValidaitionSuccess = false; 
								}


							}
						}

					}

					cameraCutIndex = SecondCameraCutIndex; 
					SecondCameraCutIndex = -1; 
					bHaveCameraCutBookendEvents = false; 
					bSearchshadowEvents = false; 
				}
				
			}
		}
	}
	return ValidaitionSuccess; 
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//								END OF DEBUG MANAGER
///////////////////////////////////////////////////////////////////////////////////////////////////
#endif

