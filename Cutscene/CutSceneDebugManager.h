/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : CutSceneDebugManagerNew.h
// PURPOSE : A class for dealing with all the debug and tool aspects of the cut scene manager.
// AUTHOR  : Thomas French
// STARTED :
//
/////////////////////////////////////////////////////////////////////////////////
#if __BANK
//rage headers
#include "bank/slider.h"
#include "bank/data.h"

#include "cutscene/cutsmanager.h"
#include "crskeleton/bonedata.h"
#include "cutscene/cutsentity.h"
#include "cutfile/cutfevent.h"
#include "atl/vector.h"

//game headers
#include "scene/RegdRefTypes.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "renderer/Lights/LightSource.h"

#ifndef CUTSCENE_CUTSCENEDEBUGMANAGER_H 
#define CUTSCENE_CUTSCENEDEBUGMANAGER_H 

//Forward declare
class CCutsceneAnimatedActorEntity; 
class CCutsceneAnimatedVehicleEntity; 
class CCutSceneScaleformOverlayEntity; 
class CCutSceneAnimatedPropEntity; 
class CCutSceneParticleEffectsEntity; 
class CCutSceneParticleEffect; 

class CParamType : public datBase
{
public: 
	CParamType():
	m_type (kParameterWidgetNum)
	{
	}

	enum ParameterWidgetType
	{
		kParameterFloat = 0,
		kParameterBool,
		kParameterInt,
		kParameterString,
		kParameterWidgetNum
	};

	virtual void AddWidgets(bkBank* bank, const char* Name) = 0;
	
	virtual void DeleteWidgets() = 0; 

	ParameterWidgetType GetType() const {  return m_type;  }
	
	ParameterWidgetType m_type; 


};

class CParamTypeFloat : public CParamType
{
public: 
	CParamTypeFloat()
	{
		m_type = kParameterFloat; 
		m_value = 0.0f; 
	}
	
	float GetValue() const { return m_value; }

	void AddWidgets(bkBank* bank, const char* Name)
	{
		m_pWidget = bank->AddSlider(Name, &m_value, 0.0f, 100.0f , 1.0f);
	}
	
	void DeleteWidgets()
	{
		if (m_pWidget)
		{
			m_pWidget->Destroy(); 
			m_pWidget = NULL; 
		}
	}

	bkSlider* m_pWidget; 

	float m_value; 

};

class CParamTypeInt : public CParamType
{
public: 
	CParamTypeInt()
	{	
		m_type = kParameterInt; 
		m_value = 0; 
	}
	
	s32 GetValue() const { return m_value; }

	void AddWidgets(bkBank* bank, const char* Name)
	{
		m_pWidget = bank->AddSlider(Name, &m_value, 0, 100, 1);
	}

	void DeleteWidgets()
	{
		if (m_pWidget)
		{
			m_pWidget->Destroy(); 
			m_pWidget = NULL; 
		}

	}

	bkSlider* m_pWidget; 
	s32 m_value; 
};

class CParamTypeBool : public CParamType
{
public: 

	CParamTypeBool()
	{
		m_type = kParameterBool;
		m_value = false; 
	}

	void AddWidgets(bkBank* bank, const char* Name)
	{
		m_pWidget = bank->AddToggle(Name, &m_value, NullCB ); 
	}

	void DeleteWidgets()
	{
		if(m_pWidget)
		{
			m_pWidget->Destroy(); 
			m_pWidget = NULL; 
		}
	}

	bkToggle* m_pWidget; 
	bool m_value; 
};


class CParamTypeString : public CParamType
{
public: 

	CParamTypeString()
	{
		m_type = kParameterString;
		m_value[0] = '\0'; 
	}

	void AddWidgets(bkBank* bank, const char* Name)
	{
		m_pWidget = bank->AddText(Name, &m_value[0], 128);
	}
	
	void DeleteWidgets()
	{	
		if(m_pWidget)
		{
			m_pWidget->Destroy(); 
			m_pWidget = NULL; 
		}
	}

	bkText* m_pWidget; 
	char m_value[128]; 
};

struct Params
{
	s32 ParamNumber; 
	CParamType* pParam; 
};

class CCascadeShadowBoundsDebug : public datBase
{
public:
	CCascadeShadowBoundsDebug() { Reset(); } 
	//~CCascadeShadowBoundsDebug() {;} 

	void Reset(); 

	void InitWidgets(); 
	void DestroyWidgets(); 

	void BankGetFromScriptCascadeManualControl();
	void BankSendToScriptCascadeManualControl();
	void BankSaveSetShadowBoundsEventCallBack(s32 Event); 
	void BankDeleteSetShadowBoundsEventCallBack(); 

	void SaveSetShadowBoundsEvent(const cutfObject* pObject, s32 iEventId); 
	
	void PopulateSetShadowBoundsEventList(const cutfObject* pObject); 
	void PopulateSetupShadowBoundsEventList();

	void UpdateCascadeShadowDebugRender(const atHashString &cameraCutHash);
	void ClearCascadeShadowDebugRender();
	void SelectShadowEvent(const atHashString &cameraCutHash);

	void UpdateShadowBoxCombo(); 

private:

	Vector3 m_CascadeBoundsPosition;

public:	

	bkSlider* m_pShadowFrameSlider; 

private:

	int m_CascadeIndex;


	//float args
	float m_fCascadeBoundsScaleRadius; 
	float m_fBoundsVFov; 
	float m_fBoundsHFov; 
	float m_fBoundScale; 
	float m_fTrackerScale; 
	float m_fSplitZExpWeight; 
	float m_fDitherScaleRadius; 

	float m_fWorldHeightMin; 
	float m_fWorldHeightMax; 
	float m_fReceiveHeightMin; 
	float m_fReceiveHeightMax; 

	float m_fDepthBias; 
	float m_fSlopeBias; 
	
	s32 m_ShadowSampleIndex; 
	bkCombo* m_pCascadeShadowSampleEventsCombo;

	//bool args
	bool m_EnableEntityTracker; 
	bool m_bEnableWorldHeightUpdate; 
	bool m_bEnableReceiverHeightUpdate; 
	bool m_bEnableAirCraftMode; 
	bool m_bEnableDynamicDepthMode; 
	bool m_bEnableFlyCamMode; 

	bool m_bEnableDepthBias; 
	bool m_bEnableSlopeBias; 
 
	bool m_bSetCasCadeBoundsDepthBias; 
	bool m_bSetCasCadeBoundsSlopeBias; 
	bool m_bSetCasCadeBoundsShadowSample; 

	bool m_bCascadeBoundsEnabled; 
	bool m_UseCameraCutAsParent; 

	bkCombo* m_pCascadeShadowsBoundEventsCombo;
	atVector <cutfEvent*> m_CascadeShadowsBoundIndexEvents;	//Array of events
	int m_CascadeShadowsBoundEventsIndexId; 

	bkCombo* m_pCascadeShadowsBoundPropertyEventsCombo;
	atVector <cutfEvent*> m_CascadeShadowsBoundPropertyIndexEvents;	//Array of events
	int m_CascadeShadowsBoundPropertyEventsIndexId; 
	
};


struct CutNearShadowDepth
{
	char		m_name[128];
	float		m_minNearDepth;
	float		m_firstFrameNearDepth;

	CutNearShadowDepth( const char* n ){
		if ( n)
			safecpy(  m_name, n);
		m_firstFrameNearDepth = -1;			
	}
	CutNearShadowDepth() {}

	void Set( float depth ){
		if (m_firstFrameNearDepth <0 ){
			m_firstFrameNearDepth = depth;
			m_minNearDepth = depth;
		}
		m_minNearDepth= Min( m_minNearDepth, depth);			
	}
	void Dump(){
		Displayf("%s : Min Depth %f First Frame Depth %f", m_name,m_minNearDepth, m_firstFrameNearDepth );
	}
};	


struct CameraCutBlockingTagInfo
{
	int index;
	float startBlockingTagTime;
	float endBlockingTagTime;
	float eventTime;

	CameraCutBlockingTagInfo()
	{
		index = -1;
		startBlockingTagTime = 0.0f; 
		endBlockingTagTime = 0.0f; 
		eventTime = 0.0f;
	}

};

class CCutSceneDebugManager : public datBase
{
public:
	
	CCutSceneDebugManager(); 
	~CCutSceneDebugManager();

	//variation event editor
	atArray <cutfEvent*> m_CutSceneVariations;	//Array of events
	int	m_PedVarEventId;		//Index into array of events
	bkCombo* m_pEventCombo;			//Event combo box


	void Update(); //update loop called for debug functions usually rendering functions;  

	void Clear(); //reset the debug manager vars
	
	//Fill out a selector list with a list
	void PopulateSelectorList(bkCombo* pCombo, const char* ComboName, int &index, atArray<const cutfObject*> &list,  datCallback callback  ); 
	void PopulatePedVariationInfos();

	void InitWidgets(); 
	void DestroyWidgets(); 
	
	//** Vehicles
	bkCombo* m_pVehicleSelectCombo;							//Combo box of vehicles to select
	int m_iVehicleModelIndex;								//index into vehicle selector
	atArray<const cutfObject*> m_VehicleModelObjectList;	//List of vehicle models
	
	//	** VEHICLES VARIATION
	void GetSelectedVehicleEntityCallBack(); //get the vehicle manager entity
	void PopulateVehicleVariationEventList(const cutfObject* pObject); 
	void HighLightSelectedVehicle(); //Draw some debug info over the selected vehicle
	void DisplaySelectedVehicleInfo();  //Render the vehicle info to the screen
	
	// vehicle damage
	bool m_bVehicleDamageSelector; 
	Vector3 m_vVehicleDamagerPos; 
	
	//	** PEDS VARIATION 
	void DisplaySceneSkeletons(); 

	bkCombo* m_pPedSelectCombo;		//selected ped combo box
	int	m_iPedModelIndex;		//index into ped name list
	bkSlider* m_pVarPedFrameSlider;		//Slider for the frame range
	CCutsceneAnimatedActorEntity* m_pActorEntity;		//manager class that points to selected ped
	bool m_bDisplayAllSkeletons; 

	void PopulatePedVariationEventList(const cutfObject* pObject);  	//generate a list of ped var events for a ped variation
	void GetSelectedPedEntityCallBack(); //Gets the manager entity from a drop down box in ped variation debug
	void DisplayPedVariation(); //Renders the current set up for the peds variations
	CCutsceneAnimatedActorEntity* BankGetAnimatedActorEntity() const { return m_pActorEntity; }

	//ptfx 
	bkCombo* m_pPTFXSelectCombo; 
	int m_iPtfxIndex; 
	atArray<const cutfObject*> m_PtfxList;
	void GetSelectedPtfxEntityCallBack(); 
	CCutSceneParticleEffectsEntity* m_PtfxEnt; 
	CCutSceneParticleEffect* m_Ptfx; 

	bool m_OverridePtfx; 
	Vector3 m_vPTFXPos; 
	Vector3 m_vPTFXRot;

	void UpdatePtfxPos(); 
	void UpdatePtfxRot();
	void SetCanOverridePtfx(); 

	void PopulateCameraCutEventList(const cutfObject* pObject, bkCombo* Combo, s32 &ComboIndex, datCallback CallBack); 


	//Hidden, fix up and bounds editing
	SEditCutfObjectLocationInfo*	GetHiddenObjectInfo();
	SEditCutfObjectLocationInfo*	GetFixupObjectInfo();
	sEditCutfBlockingBoundsInfo*	GetBlockingBoundObjectInfo(); 

	bkCombo* m_pPropSelectCombo;
	int m_iPropModelIndex; 
	CCutSceneAnimatedPropEntity*  m_pPropEntity; 
	CCutSceneAnimatedPropEntity*  BankGetAnimatedPropEntity() const { return m_pPropEntity; }
	atArray<const cutfObject*> m_PropList;	//List of overlays
	void GetSelectedPropEntityCallBack(); 

	//Arrow key debug
	void RightArrowFastForward();
	void LeftArrowRewind();
	void UpArrowPause();
	void DownArrowPlay();
	void CtrlRightArrowStep(); 
	void CtrlLeftArrowStep();

	void GetMapObject(); //get the map object via a mouse pointer
	void ClearMapObject();	//clear selected map object.
	void DisplayHiddenObject(); //Select if map object is visible or not.
	void FixupSelectedObject(); //Fixes up broken selected object.
	void UpdateMouseCursorPosition(Vector3 &WorldPos1, Vector3 &WorldPos2, Vector3 &WorldPosZOffset, bool bUseMouseWheel = true); //Get point in world space via the mouse
	void CreateDebugAngledAreaMatrix(const Vector3 &vPos, float width, float length, float height, const Vector3 &vRot, Matrix34 &mMat, Vector3 &vMax); 
	void ConvertMatrixAndVectorLocateToFourCornersAndHeight( const Matrix34 &mMat, const Vector3 &vMax, Vector3 &vCorner1, Vector3 &vCorner2, Vector3 &vCorner3, Vector3 &vCorner4, float &fHeight);  
	void DisplayAndEditBlockingBounds();

	Vector3 m_vWorldZOffset; 
	Vector3 m_vCorners[4]; 
	float m_fHeight; 
	Matrix34 m_Mat; 
	Vector3 m_vMax; 
	Matrix34 m_TempMat; 
	Vector3 m_vTempMax; 

	//entity debugging
	RegdEnt	m_pGameObject;

	//Frame sliders
	bkSlider* m_pVarFrameSlider;
	bkSlider* m_pVarDebugFrameSlider; 
	bkSlider* m_pVarSkipFrameSlider; 
	bkSlider* m_pPedVarPlayBackSlider;
	bkSlider* m_pVehicleVarPlayBackSlider; 
	bkSlider* m_pOverlayPlayBackSlider; 
	bkSlider* m_pWaterIndexFrameSlider; 

	//time cycle debug
	bool m_bOverrideTimeCycle; 
	u32 m_fTimeCycleOverrideTime; 
	Vector3 m_vTimeClock; 
	char m_cGameTime[32]; 

	//Overlay 
	atArray<const cutfObject*> m_OverlayObjectList;	//List of overlays
	CCutSceneScaleformOverlayEntity* m_pOverlay; 
	bkCombo* m_pOverlaySelectCombo; 
	s32 m_iOverlayIndex; 
	void GetSelectedOverlayEntityCallBack(); 
	void PopulateOverlayEventList(const cutfObject* pObject); 

	//Camera
	void SetFinalFrameCameraMat(const Matrix34& mat) { m_FinalFrameCameraMat.Set(mat); }
	void GetFinalFrameCameraMat(Matrix34 &mat) const { mat.Set(m_FinalFrameCameraMat); }
	u32 GetDofState() const { return m_DofState; } 

	bool m_bOverrideCamBlendOutEvents;

	bool m_bOverrideCam; 
	Vector3 m_vCameraPos; 
	Vector3 m_vCameraRot; 

	bool m_bOverrideCamUsingMatrix;
	Matrix34 m_cameraMtx;

	bool m_bOverrideCamUsingBinary;
	Matrix34 m_binaryCamMatrix;

	bkData* m_CameraWidgetData;				// Camera widget data handler.
	bkData* m_CutsceneStateWidgetData;		// Cutscene widget data handler.

	char m_InteriorName[64]; 
	Vector3 m_InteriorPos; 

	float m_fFov; 

	float m_fFarDof; 
	float m_fDofStrength;
	Vector4 m_vDofPlanes; 
	
	bool m_RenderDofPlanes; 
	u32 m_NearOutPlaneAlpha; 
	u32 m_NearInPlaneAlpha; 
	u32 m_FarInPlaneAlpha; 
	u32 m_FarOutPlaneAlpha; 
	u32 m_FocalPointAlpha; 
	float m_PlaneScale;
	float m_FocalPointScale; 
	bool m_SetPlaneSolid; 

	bool m_scaleAsScreenRatio; 
	float m_fFadeInDuration; 
	float m_fFadeOutDuration; 
	Vector3 m_vFadeInColor; 
	Vector3 m_vFadeOutColor; 
	bool m_bShouldOverrideUseLightDof;
	bool m_bShouldOverrideUseSimpleDof; 
	bool m_bShouldUseDofStrength; 
	int m_CoCRadius;
	float m_FocusPoint; 
	s32 m_DofState; 

	bool m_ShouldSyncTimeOfDayDofOverridedWidgetIndexidtoCurrentCameraCut; 

	//Day / Night DOF values
	bool m_bShouldSetCoCForAllDay; 
	bool m_bShouldSetCoCForAllNight; 
	bool m_bShouldUseRangeForCoC; 
	bool m_bOverrideCoCModifier; 
	u32 m_DayCoCHours; 
	bkGroup* m_pHoursGrp; 

	//void UpdateDayHoursForTimeOfDayDofOverrideEndHourSliderRange(); 
	void ToggleNightCoCOverrides(); 
	void ToggleDayCoCOverrides(); 
	void ToggleDayRangeCoCOverrides(); 
	void DeleteCoCDayRangeSelectorWidgets(); 
	void CreateCoCHourSelectorWidgets(); 
	void SaveDayCoCValues(); 

	//DOF Modifier Values
	bool m_UseDayCoCHoursForModifier; 
	bool m_UseNightCoCHoursForModifier;
	bool m_UserDefinedHoursForModifier; 

	u32 m_TimeDayDofOverrideFlags; 
	u32 m_CoCModifierHours; 
	s32 m_CoCModifier; 
	bkSlider* m_pEndHourSlider; 

	void UpdateHourSliderToNightHours(); 
	void UpdateHourSliderToDayHours(); 
	
	float m_fMotionBlurStrength; 
	
	float m_fWaterRefractionIndex; 

	Matrix34 m_FinalFrameCameraMat; 

	void PopulateWaterRefractionIndexEventList(const cutfObject* pObject); 
	atArray<const cutfObject*> m_CameraObjectList;	
	
	float m_ReflectionLodRangeStart;
	float m_ReflectionLodRangeEnd;
	float m_ReflectionSLodRangeStart;
	float m_ReflectionSLodRangeEnd;
	float m_LodMult; 
	float m_LodMultHD;
	float m_LodMultOrphanedHD;
	float m_LodMultLod;
	float m_LodMultSLod1;
	float m_LodMultSLod2;
	float m_LodMultSLod3;
	float m_LodMultSLod4;
	float m_WaterReflectionFarClip;
	float m_SSAOLightInten;
	float m_ExposurePush;
	float m_LightFadeDistanceMult; 
	float m_LightShadowFadeDistanceMult; 
	float m_LightSpecularFadeDistMult;
	float m_LightVolumetricFadeDistanceMult;
	float m_DirectionalLightMultiplier;
	float m_LensArtefactMultiplier;
	float m_MaxBloom; 
	cutfCameraCutCharacterLightParams m_CharacterLight;
	bool m_bDisableHighQualityDof;
	bool m_FreezeReflectionMap; 
	bool m_DisableDirectionalLight; 
	bool m_AbsoluteIntensityEnabled; 

	//displaymodels
	bool m_bDisplayLoadedModels; 
	bool m_bPrintModelLoadingToDebug; 
	CCascadeShadowBoundsDebug m_CascadeBounds; 

	//interior
	atArray<const cutfObject*> m_pAssetManagerList; 
	
	char m_CameraCutName[128]; 

	//pIntInst = CPortalVisTracker::GetPrimaryInteriorInst();
	//roomIdx = CPortalVisTracker::GetPrimaryRoomIdx();

	atHashString FindCameraCutNameForTime(const atArray <cutfEvent*>& events, float time, s32 &EventIndex);
	float FindCameraCutTimeForTime(const atArray <cutfEvent*>& events, float time); 
	const cutfEvent* FindCameraCutEventForTime(const atArray <cutfEvent*>& events, float time); 
	atHashString FindNearestCameraCutNameForTime(const atArray <cutfEvent*>& events, float time, s32 &EventIndex); 

	//Save the changes out to the cut tune file. custom_shadow.xml file
	static void WriteOutAllDynamicDepthEvents(const atArray<CutNearShadowDepth>& depths );
	static void ClearAllDynamicDepthEvents();

	// Profiling
	bool IsLightDisabled(const CLightSource &light);
	bool ProfilingIsOverridingCharacterLight() const { return m_bProfilingOverrideCharacterLight; }
	bool ProfilingIsDofDisabled() const { return m_bProfilingDisableDof; }
	bool ProfilingUseDefaultFov() const { return m_bProfilingUseDefaultFov; }

	void ValidateCutscene(); 

	void UpdateFirstPersonEventList(); 

	//Timecycle events
	atArray <cutfEvent*> m_CameraCutsIndexEvents;	//Array of events

private:
	static void SaveDynamicDepthEventsToCutscene( const atArray<CutNearShadowDepth>& depths, const cutfObject* pObject );

	void BankCameraWidgetReceiveDataCallback();
	void BankCutsceneWidgetReceiveDataCallback();

	void BankFadeScreenInCallBack(); 
	void BankFadeScreenOutCallBack(); 
	
	//	** VEHICLES VARIATION
	void GetVehicleSettings(); //store the vehicle settings
	void ResetVehicleSettings(); //reset vehicle settings
	
	////Variations
	void SaveVehicleVariationEvent(const cutfObject* pObject); 
	void BankSaveVehicleVariationEventCallBack(); 
	void BankDeleteVehicleVariationCallBack(); 

	////*Colours, liverys and dirt
	void UpdateVehicleBody1ColourCallBack(); //set the main body colour
	void UpdateVehicleBody2ColourCallBack(); //set the secondary body colour
	void UpdateVehicleBody3ColourCallBack(); //update the specular colour	
	void UpdateVehicleBody4ColourCallBack(); //update the wheel trim colour
	void UpdateVehicleBody5ColourCallBack(); //update the body colour 5
	void UpdateVehicleBody6ColourCallBack(); //update the body colour 6
	void UpdateVehicleLiveriesCallBack(); //update the livery (a livery is a emblem on the vehicle ie flames down the side)
	void UpdateVehicleDirtCallBack(); //Update the dirt level
	//
	////*Vehicle extras e.g special bones that we collapse the verts on. bull bars, spoilers etc
	void PopulateVehicleExtraList(); //Populate the available extras for a vehicle
	void PopulateVehicleExtraEventList(const cutfObject* pObject); //Populate a list of extra events for the vehicle
	void HideVehicleExtraCallBack(); //Hide the vehicle extras	 
	void ShowVehicleExtraCallBack(); //Display the vehicle extras
	void SaveVehicleExtraEvent(const cutfObject* pObject); 
	void BankSaveVehicleExtraCallBack(); 
	void BankDeleteVehicleExtrasCallBack(); 
	
	const char* GetExtraBoneNumberFromName(s32 iBone); 

	#define CUTSCENE_STATE_DATA_WIDGET_SIZE 64
	u8 m_CutsceneStateWidgetDataBuffer[CUTSCENE_STATE_DATA_WIDGET_SIZE]; 

	CCutsceneAnimatedVehicleEntity* m_pVehicle; 
	bkCombo* m_pVehicleExtraCombo; 
	bkSlider* m_pLiverySlider; 
	bkSlider* m_pLivery2Slider; 
	u8 m_iMainBodyColour; 
	u8 m_iSecondBodyColour; 
	u8 m_iSpecularColour; 
	u8 m_iWheelTrimColour;
	u8 m_iBodyColour5;
	u8 m_iBodyColour6;
	s32 m_iLivery;
	s32 m_iLivery2;
	float m_fDirtLevel; 
	s32 m_iBoneId; 
	atArray<const crBoneData*> m_pBoneData; 
	
	s32 m_iVehicleExtraIndex; 
	bool m_bDisplayVehicleVariation; 

	////vehicle variation 
	int m_VehicleVarEventId; 
	int m_VehicleExtraEventId; 
	bkCombo* m_pVehicleVariationEventCombo; 
	bkCombo* m_pVehicleExtraEventCombo; 
	bkSlider* m_pVehicleVarFrameSlider; 
	atArray <cutfEvent*> m_VehicleCutSceneVariations;	//Array of events
	atArray <cutfEvent*> m_VehicleExtras; 

	//Vehicle Damage
	float m_fDamageValue; 
	float m_fDerformation; 
	void BankApplyVehicleDamageEventCallBack(); 
	int m_iVehicleDamageIndex; 
	bkCombo* m_pVehicleDamageEventCombo; 
	atArray <cutfEvent*> m_VehicleDamageEvents; 
	Vector3 m_vLocalDamage; 
	void BankSaveVehicleDamageEvent();
	void SaveVehicleDamageEvent(const cutfObject* pObject, parAttributeList &Attributes); 
	void BankDeleteVehicleDamageEvent(); 
	void PopulateVehicleDamageEventList(const cutfObject* pObject); //Populate the available extras for a vehicle

	//	**PED VARIATION
	//Populate a list for a combo box, element 0 assumes nothing is selected
	void UpdateCutSceneVarComponentCB(); 	//Updates the variation component 
	void UpdateCutSceneVarDrawblCB(); //Updates the drawable component
	void UpdateCutSceneVarTexCB(); //Updates the drawables texture
	void UpdateCutSceneCompVarInfoIdxCB(); 	//Updates the component variation info index
	
	void UpdateCutScenePropComponentCB(); 	//Updates the variation component 
	void UpdateCutScenePropDrawblCB(); //Updates the drawable component
	void UpdateCutScenePropTexCB(); //Updates the drawables texture
	void UpdateCutScenePropVarInfoIdxCB(); 	//Updates the prop variation info index 
	
	//*CallBacks
	void BankSavePedVariationEventCallBack(); 	//Saves the event for ped variation
	void BankDeletePedVariationCallBack(); //Delete the ped variation event.
	void SavePedVariationEvent(const cutfObject* pObject); 
	
	void BankSavePedPropVariationEventCallBack(); 
	void SavePedPropVariationEvent(const cutfObject* pObject); 

	//Ped Variation
	bkSlider* m_pVarDrawablIdSlider;	//Drawable slider
	bkSlider* m_pVarTexIdSlider;		//Texture slider
	bkSlider* m_pVarPaletteIdSlider;	//Pallet Slide (NOT USED AT THE MO)
	bkCombo* m_CboCompVariationIndex;
	bkCombo* m_CboPropVariationIndex;
	int m_varDrawableId;		//Currently selected drawable
	int	m_varTextureId;			//Currently selected texture
	int	m_varComponentId;		//Currently selected component
	int	m_varPalette;			//Currently selected palette
	int	m_maxDrawables;			//Max num of drawables for this component 
	int	m_maxTextures;			//Max num of texture for this drawable 
	int	m_maxPalettes;			//Max num of pallette
	
	char m_varDrawablName[24];	//drawable name
	char m_varTexName[24];		//variation texture name
	
	//props
	bkSlider* m_pPropDrawablIdSlider; 
	bkSlider* m_pPropTexIdSlider; 
	int m_PropSlotId; 
	int m_maxPropDrawableId; 
	int m_PropDrawableId; 
	int m_maxPropTextureId; 
	int m_PropTextureId; 

	// Profiling
	bool m_bProfilingDisableCsLights;
	bool m_bProfilingOverrideCharacterLight;
	bool m_bProfilingDisableDof;
	bool m_bProfilingUseDefaultFov;

	//Overlay Editing 
	Vector2 m_vOverlayPos; 
	Vector2 m_vOverlaySize; 
	bkCombo* m_pOverlayEventCombo;

	s32 m_OverlayEventId; 
	atArray <cutfEvent*> m_OverlayEvents;	//Array of events
	parAttributeList m_EventAttributes; 
	bkCombo* m_pOverlayMethodsCombo; 
	s32 m_iOverlayMethodIndex; 
	atArray<const char*> m_OverlayMethodNames; 
	
	bkCombo* m_pOverlayParamNames;
	bkCombo* m_pOverlayParamTypes;
	
	s32	m_iOverlayParamNameIndex;
	s32	m_iOverlayParamTypeIndex;
	bool m_bUseScreenRenderTarget; 

	void GetOverlaySettings(); 
	void SetOverlayPositionCB();
	void SetOverlaySizeCB(); 
	void BankSaveOverlayEventsCallBack(); 
	void BankDeleteOverlayEventCallBack(); 
	void DeleteParamWidget(s32 index); 
	
	void SaveOverlayEvent(const cutfObject* pObject, const char* ArgNames, s32 iEventId); 
	void GetOverlayMovieMethods(); 
	void CreateParamWidgets(); 
	void SetRenderTarget(); 
	void BankSaveMovieProperties(); 
	//void BankSaveRenderTargetEvent(); 

	atArray<Params> m_OverlayParams; 
	bool AddAttributesFromParams();

	//Water Refract index
	s32 m_iWaterRefractionIndexId;
	
	bkCombo* m_pWaterRefractionIndexCombo;
	atArray <cutfEvent*> m_WaterRefractionIndexEvents;	//Array of events


	void BankSaveWaterRefractionIndexEventCallBack();
	void SaveWaterRefractionIndexEvent(const cutfObject* pObject, s32 iEventId); 
	void BankDeleteWaterRefractionIndexEventCallBack();

	//interior loading
	void BankSaveLoadInteriorCallBack();
	void SaveInteriorLoadEvent(const cutfObject* pObject, s32 EventId); 
	void PopulateInteriorLoadEventList(const cutfObject* pObject); 
	void UpdateInteriorPositionSelector(); 
	void BankDeleteLoadInteriorsCallBack(); 
	void UpdateCurrentCutName(); 
	void UpdateFirstPersonBlendOutDuration(); 

	bkCombo* m_pLoadInteriorIndexCombo;

	atArray <cutfEvent*> m_LoadInteriorIndexEvents;	//Array of events
	s32 m_LoadInteriorIndexId; 



	
	void BankSaveTimeCycleParamsCallBack();
	void SaveTimeCycleParamsEvent(const cutfObject* pObject); 
	float ValidateTimeCycleParams(float &param);
	void SaveAllCoCModifiers(); 

	//dof Override
	void SaveCoCModifier();
	void BankSaveAllCoCModifiersCallBack();
	void DeleteCoCModifierCallBack(); 
	void DeleteAllCoCModifierOverrideCallBack(); 

	//void DeleteTimeOfDayDofOverride(); 
	//void DeleteTimeOfDayDofOverrideCB(); 

	void BankSaveCoCModifierCallBack(); 
	//void PopulateTimeOfDofModifiers(); 

	bkCombo* m_pPerCutCoCModifiersCombo;
	bkCombo* m_AllCoCModifiersCombo; 
	s32 m_iCoCModifierIndex;
	s32 m_iAllCoCModifierIndex; 
	bkText* m_pSaveStatusText; 
	char m_saveStatus[64]; 

	typedef Functor2<cutfCutsceneFile2*, atHashString>	CCutfileIteratorCallback;

	enum cutfileValidateProcess
	{
		CVP_SHADOW_EVENTS= 0,
		CVP_FIX_SHADOW_EVENTS, 
		CVP_LIGHT_EVENTS,
		CVP_MAX
};

	static bool m_cutfileValidationProcesses[CVP_MAX]; 

	bool ValidateShadowData(cutfCutsceneFile2* pCutfile, atHashString sceneName, const atVector<CameraCutBlockingTagInfo> &BlockingTagInfo, bool fixupEvents); 
	bool ValidateLightData(cutfCutsceneFile2* pCutfile, atHashString sceneName, const atVector<CameraCutBlockingTagInfo> &BlockingTagInfo); 
	void BatchProcessCutfilesInMemory();
	void IterateCutfile(CCutfileIteratorCallback callback); 
	void ProcessCutfilesInMemory(cutfCutsceneFile2* pCutfile, atHashString sceneName);
	void ValidateCameraCutData(cutfCutsceneFile2* pCutfile, atHashString sceneName, atVector<CameraCutBlockingTagInfo> &BlockingTagInfo); 
	void ValidateEventInThisScene(); 
	void AddCutAttributesToShadowEvents(cutfCutsceneFile2* pCutfile, atHashString sceneName); 

	s32 m_NumberOfFailedScene;

	float m_firstPersonCameraBlendDuration; 
	s32 m_firstPersonBlendOutComboIndex; 
	bkSlider* m_FirstPersonBlendOutSlider; 
	bkCombo* m_firstPersonBlendOutComboSlider; 
	atArray<cutfEvent*> m_BlendOutEvents; 
	bkText* m_firstPersonEventStatus; 
	char m_firstPersonStatusText[256]; 

	bool CanAddEventFirstPersonCameraEvent(); 
	void DeleteFirstPersonCameraEvent(); 
	void AddFirstPersonBlendOutEvent(); 
	void AddFirstPersonCatchUpEvent(); 
	void SaveFirstPersonCameraEvents(); 
	void PopulateFirstPersonEventCameraComboBox();


public:
	void UpdateCoCModifierEventOverridesCB(); 
	void UpdateAllCoCModifierEventOverridesCB(); 
	void UpdateTimeCycleParamsCB(); 

	bkCombo* m_pCoCModifiersCameraCutCombo; 
	s32 m_iCoCModifiersCamerCutComboIndexId; 

	bkCombo* m_pTimeCycleCameraCutCombo;
	s32 m_iTimecycleEventsIndexId; 

};

#endif 

#endif //__BANK
