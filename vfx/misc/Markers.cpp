///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	Markers.cpp
//	BY	: 	Mark Nicholson (Adapted from original by Obbe)
//	FOR	:	Rockstar North
//	ON	:	01 Aug 2008
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "Markers.h"

// rage
#include "grcore/allocscope.h"
#include "grcore/effect_values.h"
#include "math/amath.h"
#include "rmptfx/ptxeffectinst.h"
#include "system/xtl.h"

// framework
#include "grcore/debugdraw.h"
#include "fwdebug/picker.h"
#include "fwmaths/angle.h"
#include "fwsys/timer.h"
#include "vfx/channel.h"
#include "vfx/ptfx/ptfxreg.h"
#include "vfx/vfxwidget.h"

// game
#include "Camera/CamInterface.h"
#include "Control/GameLogic.h"
#include "Core/Game.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "FrontEnd/MobilePhone.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "Peds/PlayerInfo.h"
#include "Peds/ped.h"
#include "Renderer/PostProcessFX.h"
#include "Renderer/PostProcessFXHelper.h"
#include "Renderer/RenderTargets.h"
#include "Renderer/Water.h"
#include "Renderer/Util/Util.h"
#include "Scene/World/GameWorld.h"
#include "Streaming/Streaming.h"
#include "Vfx/Particles/PtFxManager.h"
#include "Vfx/VfxHelper.h"

///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_MISC_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

#define USE_ADJACENCY_INFORMATION		(1)

dev_float			MARKERS_BOUNCE_SPEED				= 10.0f;
dev_float			MARKERS_BOUNCE_HEIGHT				= 0.1f; 
dev_float			MARKERS_ROTATION_SPEED				= -TWO_PI / 6.0f;

///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

CMarkers			g_markers;

#if USE_ADJACENCY_INFORMATION

struct AdjacencyContext
{
	sysIpcThreadId thread;
	bool alive;
	grmGeometry *geometry;
	grmGeometry::AdjacencyStatus status;
	
	AdjacencyContext(): thread(0), alive(false), geometry(NULL), status(grmGeometry::AS_UNEXPECTED_ERROR) {}
	void reset() { alive = false; geometry = NULL; }
	bool is_free() const { return !alive && geometry == NULL; }
};

static atFixedArray<AdjacencyContext, 10> g_AdjacencyGenerationThreads;

#endif //USE_ADJACENCY_INFORMATION
	
///////////////////////////////////////////////////////////////////////////////
//  STATIC VARIABLES
///////////////////////////////////////////////////////////////////////////////

grcRasterizerStateHandle CMarkers::ms_rasterizerStateHandle = grcStateBlock::RS_Invalid;
grcRasterizerStateHandle CMarkers::ms_invertRasterizerStateHandle = grcStateBlock::RS_Invalid;
grcDepthStencilStateHandle CMarkers::ms_depthStencilStateHandle = grcStateBlock::DSS_Invalid;
grcDepthStencilStateHandle CMarkers::ms_invertDepthStencilStateHandle = grcStateBlock::DSS_Invalid;
grcDepthStencilStateHandle CMarkers::ms_invertP2DepthStencilStateHandle = grcStateBlock::DSS_Invalid;
grcBlendStateHandle CMarkers::ms_blendStateHandle = grcStateBlock::BS_Invalid;
grcBlendStateHandle CMarkers::ms_invertBlendStateHandle = grcStateBlock::BS_Invalid;

grcDepthStencilStateHandle CMarkers::ms_exitDepthStencilStateHandle = grcStateBlock::DSS_Invalid;
grcBlendStateHandle CMarkers::ms_exitBlendStateHandle = grcStateBlock::BS_Invalid;

#if __BANK
bool CMarkers::g_bDisplayMarkers = true;
#endif 


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CMarkers
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void			CMarkers::Init						(unsigned initMode)
{	
    if (initMode == INIT_CORE)
    {
	    // initialise some variables
	    m_currBounce = 0.0f;
		m_currRotate = 0.0f;
		
	    // set up the type model names
	    m_markerTypes[MARKERTYPE_CONE].modelName.SetFromString(					"PROP_MK_CONE");
	    m_markerTypes[MARKERTYPE_CYLINDER].modelName.SetFromString(				"PROP_MK_CYLINDER");
	    m_markerTypes[MARKERTYPE_ARROW].modelName.SetFromString(				"PROP_MK_ARROW_3D");
	    m_markerTypes[MARKERTYPE_ARROW_FLAT].modelName.SetFromString(			"PROP_MK_ARROW_FLAT");
		m_markerTypes[MARKERTYPE_FLAG].modelName.SetFromString(					"PROP_MK_FLAG_2");
		m_markerTypes[MARKERTYPE_RING_FLAG].modelName.SetFromString(			"PROP_MK_FLAG");
		m_markerTypes[MARKERTYPE_RING].modelName.SetFromString(					"PROP_MK_RING");
		m_markerTypes[MARKERTYPE_PLANE].modelName.SetFromString(				"PROP_MK_PLANE");
		m_markerTypes[MARKERTYPE_BIKE_LOGO_1].modelName.SetFromString(			"PROP_MK_BIKE_LOGO_1");
		m_markerTypes[MARKERTYPE_BIKE_LOGO_2].modelName.SetFromString(			"PROP_MK_BIKE_LOGO_2");
		m_markerTypes[MARKERTYPE_NUM_0].modelName.SetFromString(				"PROP_MK_NUM_0");
		m_markerTypes[MARKERTYPE_NUM_1].modelName.SetFromString(				"PROP_MK_NUM_1");
		m_markerTypes[MARKERTYPE_NUM_2].modelName.SetFromString(				"PROP_MK_NUM_2");
		m_markerTypes[MARKERTYPE_NUM_3].modelName.SetFromString(				"PROP_MK_NUM_3");
		m_markerTypes[MARKERTYPE_NUM_4].modelName.SetFromString(				"PROP_MK_NUM_4");
		m_markerTypes[MARKERTYPE_NUM_5].modelName.SetFromString(				"PROP_MK_NUM_5");
		m_markerTypes[MARKERTYPE_NUM_6].modelName.SetFromString(				"PROP_MK_NUM_6");
		m_markerTypes[MARKERTYPE_NUM_7].modelName.SetFromString(				"PROP_MK_NUM_7");
		m_markerTypes[MARKERTYPE_NUM_8].modelName.SetFromString(				"PROP_MK_NUM_8");
		m_markerTypes[MARKERTYPE_NUM_9].modelName.SetFromString(				"PROP_MK_NUM_9");
		m_markerTypes[MARKERTYPE_CHEVRON_1].modelName.SetFromString(			"PROP_MK_RACE_CHEVRON_01");
		m_markerTypes[MARKERTYPE_CHEVRON_2].modelName.SetFromString(			"PROP_MK_RACE_CHEVRON_02");
		m_markerTypes[MARKERTYPE_CHEVRON_3].modelName.SetFromString(			"PROP_MK_RACE_CHEVRON_03");
		m_markerTypes[MARKERTYPE_RING_FLAT].modelName.SetFromString(			"PROP_MK_RING_FLAT");
		m_markerTypes[MARKERTYPE_LAP].modelName.SetFromString(					"PROP_MK_LAP");
		m_markerTypes[MARKERTYPE_HALO].modelName.SetFromString(					"PROP_MP_HALO");
		m_markerTypes[MARKERTYPE_HALO_POINT].modelName.SetFromString(			"PROP_MP_HALO_POINT");
		m_markerTypes[MARKERTYPE_HALO_ROTATE].modelName.SetFromString(			"PROP_MP_HALO_ROTATE");
		m_markerTypes[MARKERTYPE_SPHERE].modelName.SetFromString(				"PROP_MK_SPHERE");
		m_markerTypes[MARKERTYPE_MONEY].modelName.SetFromString(				"PROP_MK_MONEY");
		m_markerTypes[MARKERTYPE_LINES].modelName.SetFromString(				"PROP_MK_LINES");
		m_markerTypes[MARKERTYPE_BEAST].modelName.SetFromString(				"PROP_MK_BEAST");
		m_markerTypes[MARKERTYPE_QUESTION_MARK].modelName.SetFromString(		"PROP_MK_RANDOM_TRANSFORM");
		m_markerTypes[MARKERTYPE_TRANSFORM_PLANE].modelName.SetFromString(		"PROP_MK_TRANSFORM_PLANE");
		m_markerTypes[MARKERTYPE_TRANSFORM_HELICOPTER].modelName.SetFromString(	"PROP_MK_TRANSFORM_HELICOPTER");
		m_markerTypes[MARKERTYPE_TRANSFORM_BOAT].modelName.SetFromString(		"PROP_MK_TRANSFORM_BOAT");
		m_markerTypes[MARKERTYPE_TRANSFORM_CAR].modelName.SetFromString(		"PROP_MK_TRANSFORM_CAR");
		m_markerTypes[MARKERTYPE_TRANSFORM_BIKE].modelName.SetFromString(		"PROP_MK_TRANSFORM_BIKE");
		m_markerTypes[MARKERTYPE_TRANSFORM_PUSH_BIKE].modelName.SetFromString(	"PROP_MK_TRANSFORM_PUSH_BIKE");
		m_markerTypes[MARKERTYPE_TRANSFORM_TRUCK].modelName.SetFromString(		"PROP_MK_TRANSFORM_TRUCK");
		m_markerTypes[MARKERTYPE_TRANSFORM_PARACHUTE].modelName.SetFromString(	"PROP_MK_TRANSFORM_PARACHUTE");
		m_markerTypes[MARKERTYPE_TRANSFORM_THRUSTER].modelName.SetFromString(	"PROP_MK_TRANSFORM_THRUSTER");
		m_markerTypes[MARKERTYPE_WARP].modelName.SetFromString(					"PROP_MK_WARP");
		m_markerTypes[MARKERTYPE_BOXES].modelName.SetFromString(				"PROP_ARENA_ICON_BOXMK");
		m_markerTypes[MARKERTYPE_PIT_LANE].modelName.SetFromString(				"PROP_AC_PIT_LANE_BLIP");

// 	    // set up the type particle effect names
// 	    m_markerTypes[MARKERTYPE_CONE].ptFxHashName						= (u32)0;//atHashWithStringNotFinal("marker_cone");
// 	    m_markerTypes[MARKERTYPE_CYLINDER].ptFxHashName					= (u32)0;//atHashWithStringNotFinal("marker_cylinder");
// 	    m_markerTypes[MARKERTYPE_ARROW].ptFxHashName					= (u32)0;//atHashWithStringNotFinal("marker_arrow");
// 	    m_markerTypes[MARKERTYPE_ARROW_FLAT].ptFxHashName				= (u32)0;//atHashWithStringNotFinal("marker_arrow_flat");
// 	    m_markerTypes[MARKERTYPE_FLAG].ptFxHashName						= (u32)0;//atHashWithStringNotFinal("marker_flag");
// 		m_markerTypes[MARKERTYPE_RING_FLAG].ptFxHashName				= (u32)0;
// 		m_markerTypes[MARKERTYPE_RING].ptFxHashName						= (u32)0;//atHashWithStringNotFinal("marker_ring");
// 		m_markerTypes[MARKERTYPE_PLANE].ptFxHashName					= (u32)0;
// 		m_markerTypes[MARKERTYPE_BIKE_LOGO_1].ptFxHashName				= (u32)0;
// 		m_markerTypes[MARKERTYPE_BIKE_LOGO_2].ptFxHashName				= (u32)0;
// 		m_markerTypes[MARKERTYPE_NUM_0].ptFxHashName					= (u32)0;
// 		m_markerTypes[MARKERTYPE_NUM_1].ptFxHashName					= (u32)0;
// 		m_markerTypes[MARKERTYPE_NUM_2].ptFxHashName					= (u32)0;
// 		m_markerTypes[MARKERTYPE_NUM_3].ptFxHashName					= (u32)0;
// 		m_markerTypes[MARKERTYPE_NUM_4].ptFxHashName					= (u32)0;
// 		m_markerTypes[MARKERTYPE_NUM_5].ptFxHashName					= (u32)0;
// 		m_markerTypes[MARKERTYPE_NUM_6].ptFxHashName					= (u32)0;
// 		m_markerTypes[MARKERTYPE_NUM_7].ptFxHashName					= (u32)0;
// 		m_markerTypes[MARKERTYPE_NUM_8].ptFxHashName					= (u32)0;
// 		m_markerTypes[MARKERTYPE_NUM_9].ptFxHashName					= (u32)0;
// 		m_markerTypes[MARKERTYPE_CHEVRON_1].ptFxHashName				= (u32)0;
// 		m_markerTypes[MARKERTYPE_CHEVRON_2].ptFxHashName				= (u32)0;
// 		m_markerTypes[MARKERTYPE_CHEVRON_3].ptFxHashName				= (u32)0;
// 		m_markerTypes[MARKERTYPE_RING_FLAT].ptFxHashName				= (u32)0;
// 		m_markerTypes[MARKERTYPE_LAP].ptFxHashName						= (u32)0;
//		m_markerTypes[MARKERTYPE_HALO].ptFxHashName						= (u32)0;
//		m_markerTypes[MARKERTYPE_HALO_POINT].ptFxHashName				= (u32)0;
//		m_markerTypes[MARKERTYPE_HALO_ROTATE].ptFxHashName				= (u32)0;
//		m_markerTypes[MARKERTYPE_SPHERE].ptFxHashName					= (u32)0;
//		m_markerTypes[MARKERTYPE_MONEY].ptFxHashName					= (u32)0;
//		m_markerTypes[MARKERTYPE_LINES].ptFxHashName					= (u32)0;
//		m_markerTypes[MARKERTYPE_BEAST].ptFxHashName					= (u32)0;
//		m_markerTypes[MARKERTYPE_QUESTION_MARK].ptFxHashName			= (u32)0;
//		m_markerTypes[MARKERTYPE_TRANSFORM_PLANE].ptFxHashName			= (u32)0;
//		m_markerTypes[MARKERTYPE_TRANSFORM_HELICOPTER].ptFxHashName		= (u32)0;
//		m_markerTypes[MARKERTYPE_TRANSFORM_BOAT].ptFxHashName			= (u32)0;
//		m_markerTypes[MARKERTYPE_TRANSFORM_CAR].ptFxHashName			= (u32)0;
//		m_markerTypes[MARKERTYPE_TRANSFORM_BIKE].ptFxHashName			= (u32)0;
//		m_markerTypes[MARKERTYPE_TRANSFORM_PUSH_BIKE].ptFxHashName		= (u32)0;
//		m_markerTypes[MARKERTYPE_TRANSFORM_TRUCK].ptFxHashName			= (u32)0;
//		m_markerTypes[MARKERTYPE_TRANSFORM_PARACHUTE].ptFxHashName		= (u32)0;
//		m_markerTypes[MARKERTYPE_TRANSFORM_THRUSTER].ptFxHashName		= (u32)0;
//		m_markerTypes[MARKERTYPE_WARP].ptFxHashName						= (u32)0;
//		m_markerTypes[MARKERTYPE_BOXES].ptFxHashName					= (u32)0;
//		m_markerTypes[MARKERTYPE_PIT_LANE].ptFxHashName					= (u32)0;

	    // debug stuff
#if __BANK
	    // set up the type names
	    strcpy(m_markerTypes[MARKERTYPE_CONE].typeName,					"CONE");
	    strcpy(m_markerTypes[MARKERTYPE_CYLINDER].typeName,				"CYLINDER");
	    strcpy(m_markerTypes[MARKERTYPE_ARROW].typeName,				"ARROW 3D");
	    strcpy(m_markerTypes[MARKERTYPE_ARROW_FLAT].typeName,			"ARROW_FLAT");
	    strcpy(m_markerTypes[MARKERTYPE_FLAG].typeName,					"FLAG");
		strcpy(m_markerTypes[MARKERTYPE_RING_FLAG].typeName,			"RING FLAG");
		strcpy(m_markerTypes[MARKERTYPE_RING].typeName,					"RING");
		strcpy(m_markerTypes[MARKERTYPE_PLANE].typeName,				"PLANE");
		strcpy(m_markerTypes[MARKERTYPE_BIKE_LOGO_1].typeName,			"BIKE LOGO 1");
		strcpy(m_markerTypes[MARKERTYPE_BIKE_LOGO_2].typeName,			"BIKE LOGO 2");
		strcpy(m_markerTypes[MARKERTYPE_NUM_0].typeName,				"NUM 0");
		strcpy(m_markerTypes[MARKERTYPE_NUM_1].typeName,				"NUM 1");
		strcpy(m_markerTypes[MARKERTYPE_NUM_2].typeName,				"NUM 2");
		strcpy(m_markerTypes[MARKERTYPE_NUM_3].typeName,				"NUM 3");
		strcpy(m_markerTypes[MARKERTYPE_NUM_4].typeName,				"NUM 4");
		strcpy(m_markerTypes[MARKERTYPE_NUM_5].typeName,				"NUM 5");
		strcpy(m_markerTypes[MARKERTYPE_NUM_6].typeName,				"NUM 6");
		strcpy(m_markerTypes[MARKERTYPE_NUM_7].typeName,				"NUM 7");
		strcpy(m_markerTypes[MARKERTYPE_NUM_8].typeName,				"NUM 8");
		strcpy(m_markerTypes[MARKERTYPE_NUM_9].typeName,				"NUM 9");
		strcpy(m_markerTypes[MARKERTYPE_CHEVRON_1].typeName,			"CHEVRON 1");
		strcpy(m_markerTypes[MARKERTYPE_CHEVRON_2].typeName,			"CHEVRON 2");
		strcpy(m_markerTypes[MARKERTYPE_CHEVRON_3].typeName,			"CHEVRON 3");
		strcpy(m_markerTypes[MARKERTYPE_RING_FLAT].typeName,			"RING FLAT");
		strcpy(m_markerTypes[MARKERTYPE_LAP].typeName,					"LAP");
		strcpy(m_markerTypes[MARKERTYPE_HALO].typeName,					"HALO");
		strcpy(m_markerTypes[MARKERTYPE_HALO_POINT].typeName,			"HALO POINT");
		strcpy(m_markerTypes[MARKERTYPE_HALO_ROTATE].typeName,			"HALO ROTATE");
		strcpy(m_markerTypes[MARKERTYPE_SPHERE].typeName,				"SPHERE");
		strcpy(m_markerTypes[MARKERTYPE_MONEY].typeName,				"MONEY");
		strcpy(m_markerTypes[MARKERTYPE_LINES].typeName,				"LINES");
		strcpy(m_markerTypes[MARKERTYPE_BEAST].typeName,				"BEAST");
		strcpy(m_markerTypes[MARKERTYPE_QUESTION_MARK].typeName,		"QUESTION MARK");
		strcpy(m_markerTypes[MARKERTYPE_TRANSFORM_PLANE].typeName,		"TRANSFORM PLANE");
		strcpy(m_markerTypes[MARKERTYPE_TRANSFORM_HELICOPTER].typeName,	"TRANSFORM HELICOPTER");
		strcpy(m_markerTypes[MARKERTYPE_TRANSFORM_BOAT].typeName,		"TRANSFORM BOAT");
		strcpy(m_markerTypes[MARKERTYPE_TRANSFORM_CAR].typeName,		"TRANSFORM CAR");
		strcpy(m_markerTypes[MARKERTYPE_TRANSFORM_BIKE].typeName,		"TRANSFORM BIKE");
		strcpy(m_markerTypes[MARKERTYPE_TRANSFORM_PUSH_BIKE].typeName,	"TRANSFORM PUSH_BIKE");
		strcpy(m_markerTypes[MARKERTYPE_TRANSFORM_TRUCK].typeName,		"TRANSFORM TRUCK");
		strcpy(m_markerTypes[MARKERTYPE_TRANSFORM_PARACHUTE].typeName,	"TRANSFORM PARACHUTE");
		strcpy(m_markerTypes[MARKERTYPE_TRANSFORM_THRUSTER].typeName,	"TRANSFORM THRUSTER");
		strcpy(m_markerTypes[MARKERTYPE_WARP].typeName,					"WARP");
		strcpy(m_markerTypes[MARKERTYPE_BOXES].typeName,				"BOX");
		strcpy(m_markerTypes[MARKERTYPE_PIT_LANE].typeName,				"PIT LANE");

	    m_disableRender = false;
	    m_pointAtPlayer = false;
		m_disableClippingPlane = false;
		m_renderBoxOnPickerEntity = false;
		m_forceEntityRotOrder = false;

	    m_debugRender = false;
	    m_debugRenderId = -1;

	    m_debugMarkerActive = false;
	    m_debugMarkerType = MARKERTYPE_CONE;
	    m_debugMarkerScaleX = 1.0f;
	    m_debugMarkerScaleY = 1.0f;
	    m_debugMarkerScaleZ = 1.0f;
	    m_debugMarkerColR = 255;
	    m_debugMarkerColG = 100;
	    m_debugMarkerColB = 0;
	    m_debugMarkerColA = 100;
	    m_debugMarkerBounce = true;
	    m_debugMarkerRotate = false;
	    m_debugMarkerFaceCam = false;
		m_debugMarkerInvert = false;
		m_debugMarkerFacePlayer = false;
		m_debugMarkerTrackPlayer = false;
		m_debugOverrideTexture = false;
		m_debugMarkerClip = false;
		m_debugMarkerDirX = 0.0f;
		m_debugMarkerDirY = 0.0f;
		m_debugMarkerDirZ = 0.0f;
	    m_debugMarkerRotX = 0.0f;
	    m_debugMarkerRotY = 0.0f;
		m_debugMarkerRotZ = 0.0f;
		m_debugMarkerRotCamTilt = 0.0f;
		m_debugMarkerPlaneNormalX = 0.0f;
		m_debugMarkerPlaneNormalY = 0.0f;
		m_debugMarkerPlaneNormalZ = 1.0f;
		m_debugMarkerPlanePosX = 0.0f;
		m_debugMarkerPlanePosY = 0.0f;
		m_debugMarkerPlanePosZ = 0.0f;
#endif

		// init render state blocks
		grcRasterizerStateDesc rsd;
		rsd.CullMode = grcRSV::CULL_NONE;
		rsd.FillMode = grcRSV::FILL_SOLID;
		ms_rasterizerStateHandle = grcStateBlock::CreateRasterizerState(rsd);

		rsd.CullMode = grcRSV::CULL_FRONT;
		ms_invertRasterizerStateHandle = grcStateBlock::CreateRasterizerState(rsd);
		
		grcDepthStencilStateDesc depthStencilDesc;
		depthStencilDesc.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESS);
		depthStencilDesc.DepthWriteMask = false;
		ms_depthStencilStateHandle = grcStateBlock::CreateDepthStencilState(depthStencilDesc);

		depthStencilDesc.StencilEnable = true;
		depthStencilDesc.FrontFace.StencilFunc = grcRSV::CMP_EQUAL;
		depthStencilDesc.BackFace = depthStencilDesc.FrontFace;
		ms_invertP2DepthStencilStateHandle = grcStateBlock::CreateDepthStencilState(depthStencilDesc);

		depthStencilDesc.StencilEnable = true;
		depthStencilDesc.FrontFace.StencilFunc = grcRSV::CMP_ALWAYS;
		depthStencilDesc.FrontFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;
		depthStencilDesc.BackFace = depthStencilDesc.FrontFace;
		depthStencilDesc.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_GREATEREQUAL);
		ms_invertDepthStencilStateHandle = grcStateBlock::CreateDepthStencilState(depthStencilDesc);
		
		grcBlendStateDesc blendStateDesc;
		blendStateDesc.BlendRTDesc[0].BlendEnable = true;
		blendStateDesc.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_SRCALPHA;
		blendStateDesc.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
		blendStateDesc.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
		ms_blendStateHandle = grcStateBlock::CreateBlendState(blendStateDesc);

		blendStateDesc.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
		ms_invertBlendStateHandle = grcStateBlock::CreateBlendState(blendStateDesc);

		// init exit render state blocks
		grcBlendStateDesc bsd;
		bsd.BlendRTDesc[0].BlendEnable = 1;
		bsd.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
		bsd.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_SRCALPHA;
		bsd.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
		ms_exitBlendStateHandle = grcStateBlock::CreateBlendState(bsd);

		grcDepthStencilStateDesc dssd;
		dssd.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
		ms_exitDepthStencilStateHandle = grcStateBlock::CreateDepthStencilState(dssd);

		// init the drawable pointers
		for (s32 i=0; i<MARKERTYPE_MAX; i++)
		{
			m_markerTypes[i].pDrawable = NULL;
		}
    }
    else if (initMode == INIT_SESSION)
    {
	    // request the models for the marker types
	    for (s32 i=0; i<MARKERTYPE_MAX; i++)
	    {
			u32 modelIndex = CModelInfo::GetModelIdFromName(m_markerTypes[i].modelName).GetModelIndex();
			if (!CModelInfo::IsValidModelInfo(modelIndex))
			{
				vfxAssertf(0, "marker %s has an invalid model info", m_markerTypes[i].modelName.GetCStr());
				continue;
			}

			fwModelId modelId((strLocalIndex(modelIndex)));
		    CModelInfo::RequestAssets(modelId, STRFLAG_DONTDELETE);
	    }
    }
}



///////////////////////////////////////////////////////////////////////////////
//  InitSessionSync
///////////////////////////////////////////////////////////////////////////////

void			CMarkers::InitSessionSync			()
{	
	// get the drawables and shader info for the model types
	for (s32 i=0; i<MARKERTYPE_MAX; i++)
	{
		u32 modelIndex = CModelInfo::GetModelIdFromName(m_markerTypes[i].modelName).GetModelIndex();
		if (!CModelInfo::IsValidModelInfo(modelIndex))
		{
			continue;
		}

		// get the model info
		CBaseModelInfo* pModelInfo = (CBaseModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex)));
		vfxAssertf(pModelInfo, "CMarkers::Init - couldn't get model info for %s", m_markerTypes[i].modelName.GetCStr());

		// store the min z of the bounding box so we know how high above this the pivot point is
		m_markerTypes[i].bbMinZ = pModelInfo->GetBoundingBoxMin().z;

		// get the drawable from the model info
		vfxAssertf(m_markerTypes[i].pDrawable==NULL, "non NULL pointer found when setting up the marker drawable");
		m_markerTypes[i].pDrawable = pModelInfo->GetDrawable();
		vfxAssertf(m_markerTypes[i].pDrawable, "CMarkers::Init - couldn't get the drawable for %s", m_markerTypes[i].modelName.GetCStr());

		// store some shader group variables
		grmShaderGroup* pShaderGroup = &m_markerTypes[i].pDrawable->GetShaderGroup();
		m_markerTypes[i].diffuseColorVar = pShaderGroup->LookupVar("DiffuseColor",true);
		m_markerTypes[i].clippingPlaneVar = pShaderGroup->LookupVar("ClippingPlane",true);
		m_markerTypes[i].targetSizeVar = pShaderGroup->LookupVar("TargetSize",false);
		// store shader 0 difuse Texture 
		const grmShader &shader = pShaderGroup->GetShader(0);
#if MARKERS_MANUAL_DEPTH_TEST
		m_markerTypes[i].distortionParamsVar = pShaderGroup->LookupVar("DistortionParams", true);
		m_markerTypes[i].manualDepthTestVar = pShaderGroup->LookupVar("ManualDepthTest", true);
		m_markerTypes[i].depthTextureVar = shader.LookupVar("DepthSampler",true);
#endif
		m_markerTypes[i].diffuseTextureVar = shader.LookupVar("DiffuseSampler",true);
		
		grcTexture *tex = NULL;
		shader.GetVar(m_markerTypes[i].diffuseTextureVar, tex);
		m_markerTypes[i].diffuseTexture = tex;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void			CMarkers::Shutdown						(unsigned shutdownMode)
{	
	if (shutdownMode == SHUTDOWN_SESSION)
	{
		for (s32 i=0; i<MARKERTYPE_MAX; i++)
		{
			m_markerTypes[i].pDrawable = NULL;
		}
	}
#if USE_ADJACENCY_INFORMATION
	if (shutdownMode == SHUTDOWN_CORE)
	{
		for(int k=0; k < g_AdjacencyGenerationThreads.GetCount(); ++k)
		{
			g_AdjacencyGenerationThreads[k].alive = false;
			if (g_AdjacencyGenerationThreads[k].status == grmGeometry::AS_IN_PROGRESS)
			{
				//sysIpcKillThread
				sysIpcWaitThreadExit(g_AdjacencyGenerationThreads[k].thread);
			}
		}
		g_AdjacencyGenerationThreads.clear();
	}
#endif //USE_ADJACENCY_INFORMATION
}


///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void			CMarkers::Update						()
{
	// calculate the current bounce height and rotation speed
	if (!fwTimer::IsGamePaused())
	{
		m_currBounce += MARKERS_BOUNCE_SPEED * fwTimer::GetTimeStep();
		m_currRotate = fwAngle::LimitRadianAngleSafe(m_currRotate + MARKERS_ROTATION_SPEED * fwTimer::GetTimeStep());
	}
	m_currBounceHeight = rage::Sinf(m_currBounce) * MARKERS_BOUNCE_HEIGHT;

#if __BANK
	// update the debug marker
	if (m_debugMarkerActive)
	{
		Vec3V vPlayerCoords = VECTOR3_TO_VEC3V(CGameWorld::FindLocalPlayerCoors());

		Vec3V vDir = Vec3V(m_debugMarkerDirX, m_debugMarkerDirY, m_debugMarkerDirZ);	

		if (m_debugMarkerFacePlayer)
		{
			vDir = vPlayerCoords - m_vDebugMarkerPos;
		}
	
		if (m_debugMarkerTrackPlayer)
		{
			// set the marker position to be that of the player
			m_vDebugMarkerPos = vPlayerCoords;

			// probe the ground heights around the player
			ScalarV vOffsetDist(V_HALF);
			ScalarV vTwoOffsetDist = vOffsetDist*ScalarV(V_TWO);
			Vec3V vRight = Vec3V(V_X_AXIS_WZERO);
			Vec3V vForward = Vec3V(V_Y_AXIS_WZERO);
			Vec3V vProbeOffsets[4] = {Vec3V(vForward*vOffsetDist), Vec3V(-vForward*vOffsetDist), Vec3V(vRight*vOffsetDist),	Vec3V(-vRight*vOffsetDist)};

			bool probeFailed = false;
			float zGround[4] = {0.0f, 0.0f, 0.0f, 0.0f};
			for (int i=0; i<4; i++)
			{
				WorldProbe::CShapeTestHitPoint probeResult;
				WorldProbe::CShapeTestResults probeResults(probeResult);
				Vec3V vStartPos = m_vDebugMarkerPos + vProbeOffsets[i];
				Vec3V vEndPos = vStartPos - Vec3V(0.0f, 0.0f, 2.5f);

				WorldProbe::CShapeTestProbeDesc probeDesc;
				probeDesc.SetStartAndEnd(RCC_VECTOR3(vStartPos), RCC_VECTOR3(vEndPos));
				probeDesc.SetResultsStructure(&probeResults);
				probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_MOVER);
				if (WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
				{
					zGround[i] = probeResults[0].GetHitPosition().z;
				}
				else
				{
					probeFailed = true;
				}
			}

			// set the rotations
			if (probeFailed)
			{
				m_debugMarkerRotX = 0.0f;
				m_debugMarkerRotY = 0.0f;
			}
			else
			{
				m_debugMarkerRotX = atan((zGround[0]-zGround[1])/vTwoOffsetDist.Getf());
				m_debugMarkerRotY = atan((zGround[3]-zGround[2])/vTwoOffsetDist.Getf());
			}

			m_debugMarkerRotZ = CGameWorld::FindLocalPlayerHeading();
		}

		s32 txdIdx = -1;
		s32 texIdx = -1;
		
		if( m_debugOverrideTexture )
		{
			u32 txdSlot = CShaderLib::GetTxdId();
			txdIdx = txdSlot;
			texIdx = g_TxdStore.Get(strLocalIndex(txdSlot))->LookupLocalIndex(ATSTRINGHASH("flare_chromatic", 0x266e84f5));
		}
		
		MarkerInfo_t markerInfo;
		markerInfo.type = m_debugMarkerType;
		markerInfo.vPos = m_vDebugMarkerPos;
		markerInfo.vDir = vDir;
		markerInfo.vRot = Vec4V(m_debugMarkerRotX, m_debugMarkerRotY, m_debugMarkerRotZ, m_debugMarkerRotCamTilt);	
		markerInfo.vScale = Vec3V(m_debugMarkerScaleX, m_debugMarkerScaleY, m_debugMarkerScaleZ);
		markerInfo.col = Color32(m_debugMarkerColR, m_debugMarkerColG, m_debugMarkerColB, m_debugMarkerColA);
		markerInfo.txdIdx = txdIdx;
		markerInfo.texIdx = texIdx;
		markerInfo.bounce = m_debugMarkerBounce;
		markerInfo.rotate = m_debugMarkerRotate;
		markerInfo.faceCam = m_debugMarkerFaceCam;
		markerInfo.invert = m_debugMarkerInvert;
		
		markerInfo.clip	= m_debugMarkerClip;
		Vec3V planeNormal = Normalize(Vec3V(m_debugMarkerPlaneNormalX, m_debugMarkerPlaneNormalY, m_debugMarkerPlaneNormalZ));
		Vec3V planePos = Vec3V(m_debugMarkerPlanePosX, m_debugMarkerPlanePosY, m_debugMarkerPlanePosZ);
		ScalarV planeDist = Negate(Dot(planePos, planeNormal));
		markerInfo.vClippingPlane = Vec4V(planeNormal, planeDist);

		g_markers.Register(markerInfo);
	}

	// bounding box marker on picker entity
	if (m_renderBoxOnPickerEntity)
	{
		CEntity* pSelectedEntity = static_cast<CEntity*>(g_PickerManager.GetSelectedEntity()); 
		if (pSelectedEntity)
		{
			Vector3 vRot = VEC3_ZERO; 
			Matrix34 vMtx = MAT34V_TO_MATRIX34(pSelectedEntity->GetMatrix());
			vRot = CScriptEulers::MatrixToEulers(vMtx, EULER_YXZ);

			float width = 1.0f;
			float depth = 1.0f;
			CBaseModelInfo* pModelInfo = pSelectedEntity->GetBaseModelInfo();
			if (pModelInfo)
			{
				Vector3 vBBMin = pModelInfo->GetBoundingBoxMin();
				Vector3 vBBMax = pModelInfo->GetBoundingBoxMax();
				width = vBBMax.x - vBBMin.x;
				depth = vBBMax.y - vBBMin.y;
			}

			MarkerInfo_t markerInfo;
			markerInfo.type = MARKERTYPE_BOXES;
			markerInfo.vPos = VECTOR3_TO_VEC3V(vMtx.d);
			markerInfo.vDir = Vec3V(V_ZERO);
			markerInfo.vRot = Vec4V(vRot.x, vRot.y, vRot.z, 0.0f);
			markerInfo.vScale = Vec3V(width, depth, 5.0f);
			markerInfo.col = Color32(255, 0, 0, 128);
			markerInfo.txdIdx = -1;
			markerInfo.texIdx = -1;
			markerInfo.bounce = false;
			markerInfo.rotate = false;
			markerInfo.faceCam = false;
			markerInfo.invert = false;
			markerInfo.usePreAlphaDepth = false;
			markerInfo.matchEntityRotOrder = false;

			g_markers.Register(markerInfo);
		}
	}
#endif

	// update the markers
	Vec3V vCameraPos = RCC_VEC3V(camInterface::GetPos());

#if __BANK
	m_numActiveMarkers = 0;
#endif
	for (s32 i=0; i<MARKERS_MAX; i++)
	{
		if (m_markers[i].isActive)
		{
			// check the type is valid
			vfxAssertf(m_markers[i].info.type != MARKERTYPE_INVALID, "CMarkers::Update - invalid marker type");

			// initialise the z offset
			float zOffset = 0.0f;

			// make the marker sit on top of any water that it could be underneath
// 			float waterZ;
// 			if (Water::GetWaterLevelNoWaves(RCC_VECTOR3(m_markers[i].info.vPos), &waterZ, 12.0f, 0.0f, NULL))
// 			{
// 				if (Abs(waterZ-m_markers[i].info.vPos.GetZf()) < 12.0f)
// 				{
// 					zOffset += waterZ;
// 				}
// 			}

			// apply any bounce to the z offset
			if (m_markers[i].info.bounce)
			{
				zOffset += m_currBounceHeight;
			}
			
			// calc the render matrix
			m_markers[i].renderMtx = Mat34V(V_IDENTITY);
			CalcMtx(m_markers[i].renderMtx, m_markers[i].info.vPos, m_markers[i].info.vDir, m_markers[i].info.faceCam, m_markers[i].info.rotate);

			// apply any additional rotations
			if (m_markers[i].info.matchEntityRotOrder BANK_ONLY(|| m_forceEntityRotOrder))
			{
				// match the entity rotation order
				Mat34V vMtxA;
				CScriptEulers::MatrixFromEulers(RC_MATRIX34(vMtxA), Vector3(m_markers[i].info.vRot.GetXf(), m_markers[i].info.vRot.GetYf(), m_markers[i].info.vRot.GetZf()), EULER_YXZ);
				Transform(m_markers[i].renderMtx, vMtxA, m_markers[i].renderMtx);
			}
			else
			{
				// legacy rotation order so that existing rotations don't stop working
			if (m_markers[i].info.vRot.GetYf()!=0.0f)
			{
				Mat34V yRotMat;
				Mat34VFromAxisAngle(yRotMat, m_markers[i].renderMtx.GetCol1(), m_markers[i].info.vRot.GetY());
				Transform(m_markers[i].renderMtx, yRotMat, m_markers[i].renderMtx);
			}

			if (m_markers[i].info.vRot.GetXf()!=0.0f)
			{
				Mat34V xRotMat;
				Mat34VFromAxisAngle(xRotMat, m_markers[i].renderMtx.GetCol0(), m_markers[i].info.vRot.GetX());
				Transform(m_markers[i].renderMtx, xRotMat, m_markers[i].renderMtx);
			}

			if (m_markers[i].info.vRot.GetZf()!=0.0f)
			{
				Mat34V zRotMat;
				Mat34VFromAxisAngle(zRotMat, m_markers[i].renderMtx.GetCol2(), m_markers[i].info.vRot.GetZ());
				Transform(m_markers[i].renderMtx, zRotMat, m_markers[i].renderMtx);			
			}
			}

			// camera tilt rotation
			if (m_markers[i].info.vRot.GetWf()!=0.0f)
			{
				Mat34V wRotMat;
				Mat34VFromAxisAngle(wRotMat, VECTOR3_TO_VEC3V(camInterface::GetRight()), m_markers[i].info.vRot.GetW());
				Transform(m_markers[i].renderMtx, wRotMat, m_markers[i].renderMtx);	

// 				static float CAM_TILT_ANGLE = 0.5f;
// 				// tilt toward camera slightly
// 				Mat34V vRotateMtx;
// 				Mat34VFromAxisAngle(vRotateMtx, VECTOR3_TO_VEC3V(camInterface::GetRight()), ScalarVFromF32(CAM_TILT_ANGLE));
// 				Transform(vMtx, vRotateMtx, vMtx);
			}

			// scale the matrix
			Mat34V vScaleMtx = Mat34V(V_IDENTITY);
			Scale(vScaleMtx, m_markers[i].info.vScale, vScaleMtx);
			Transform(m_markers[i].renderMtx, m_markers[i].renderMtx, vScaleMtx);	

			// position the matrix
			m_markers[i].renderMtx.SetCol3(Vec3V(m_markers[i].info.vPos.GetXY(), m_markers[i].info.vPos.GetZ() + ScalarVFromF32(zOffset)));

			// calc the square distance to the camera
			m_markers[i].distSqrToCam = DistSquared(vCameraPos, m_markers[i].info.vPos).Getf();

			// update particle effects
			// NOTE: if these get re-added then they are being registered using the marker class so proper
			//       code needs implemented to detect markers being finished with so ptfxReg::UnRegister(CMarker&) can be called
// 			if (m_markerTypes[m_markers[i].info.type].ptFxHashName != 0)
// 			{
// 				bool justCreated;
// 				ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(this, i, false, m_markerTypes[m_markers[i].info.type].ptFxHashName, justCreated);
// 				if (pFxInst)
// 				{
// 					pFxInst->SetBaseMtx(m_markers[i].renderMtx);
// 
// 					if (justCreated)
// 					{
// 						pFxInst->Start();
// 					}
// 				}
// 			}

			// count the number of active markers
#if __BANK
			m_numActiveMarkers++;
#endif
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Render
///////////////////////////////////////////////////////////////////////////////

int				CMarkers::RenderSorter					(const void* pA, const void* pB)
{
	// get the events
	const Marker_t& markerA = **(const Marker_t**)pA;
	const Marker_t& markerB = **(const Marker_t**)pB;

	// sort on event index
	return (markerA.distSqrToCam<=markerB.distSqrToCam) ? 1 : -1;
}

void			CMarkers::Render						()
{
	bool doRender = CVfxHelper::ShouldRenderInGameUI();

	// debug 
#if __BANK
	if (m_disableRender || !CMarkers::g_bDisplayMarkers)
	{
		doRender = false;
	}
#endif

	// do the actual rendering
	if (doRender)
	{
		// sort the active markers
		Marker_t* pSortBuffer[MARKERS_MAX];

		int numMarkersToDraw = 0;
		for (s32 i=0; i<MARKERS_MAX; i++)
		{
			if (m_markers[i].isActive)
			{
				pSortBuffer[numMarkersToDraw++] = &m_markers[i];
			}
		}

		if (numMarkersToDraw>0)
		{
			qsort(pSortBuffer, numMarkersToDraw, sizeof(const Marker_t*), RenderSorter);

			DLC_Add(RenderInit);

			// render
			for (s32 i=0; i<numMarkersToDraw; i++)
			{
				// render the marker
				Vec4V vCol(pSortBuffer[i]->info.col.GetRedf(), pSortBuffer[i]->info.col.GetGreenf(), pSortBuffer[i]->info.col.GetBluef(), pSortBuffer[i]->info.col.GetAlphaf());
				Vec4V vClippingPlane = (pSortBuffer[i]->info.clip) ? pSortBuffer[i]->info.vClippingPlane : Vec4V(V_ZERO);

#if __BANK
				if (m_disableClippingPlane)
				{
					vClippingPlane = Vec4V(V_ZERO);
				}
#endif

				DLC_Add(RenderMarker, pSortBuffer[i]->info.type, vCol, vClippingPlane, pSortBuffer[i]->renderMtx, pSortBuffer[i]->info.txdIdx, pSortBuffer[i]->info.texIdx, pSortBuffer[i]->info.invert, pSortBuffer[i]->info.vScale, pSortBuffer[i]->info.usePreAlphaDepth);
			}

			DLC_Add(RenderEnd);
		}
	}

#if __BANK
	RenderDebug();
#endif

	// the markers are all rendered - delete them and wait for the next frame's markers to be added
	DeleteAll();
}


///////////////////////////////////////////////////////////////////////////////
//  Register
///////////////////////////////////////////////////////////////////////////////

s32			CMarkers::Register					(MarkerInfo_t& markerInfo)
{
#if __ASSERT
	static u32 lastFrameMarkerRegistered = 0;
	u32 currFrame = fwTimer::GetFrameCount();
	if (currFrame!=lastFrameMarkerRegistered)
	{
		for (s32 i=0; i<MARKERS_MAX; i++)
		{
			vfxAssertf(m_markers[i].isActive==false, "registering first marker of a new frame but the marker list isn't empty (last:%d, curr:%d", lastFrameMarkerRegistered, currFrame);
		}
	}
	lastFrameMarkerRegistered = currFrame;
#endif

	s32 id = FindFirstFreeSlot();
	if (id == -1)
	{
		vfxAssertf(0, "CMarkers::Register - cannot find free slot for marker");
		return id;
	}

	m_markers[id].isActive	= true;
	m_markers[id].info = markerInfo;

	return id;
}


///////////////////////////////////////////////////////////////////////////////
//  Delete
///////////////////////////////////////////////////////////////////////////////

void			CMarkers::Delete						(s32 id)
{
	// check the index is in range
	if (id<0 || id>MARKERS_MAX-1)
	{
		vfxAssertf(0, "CMarkers::Delete - id out of range");
		return;
	} 

	m_markers[id].isActive = false;
} 


///////////////////////////////////////////////////////////////////////////////
//  DeleteAll
///////////////////////////////////////////////////////////////////////////////

void			CMarkers::DeleteAll					()
{
	for (s32 i=0; i<MARKERS_MAX; i++)
	{
		m_markers[i].isActive = false;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  FindFirstFreeSlot
///////////////////////////////////////////////////////////////////////////////

s32			CMarkers::FindFirstFreeSlot			()
{
	for (s32 i=0; i<MARKERS_MAX; i++)
	{
		if (!m_markers[i].isActive)
		{
			return i;
		}
	}

	return -1;
}


///////////////////////////////////////////////////////////////////////////////
//  GetClosestMarkerMtx
///////////////////////////////////////////////////////////////////////////////

bool 		CMarkers::GetClosestMarkerMtx		(MarkerType_e markerType, Vec3V_In vPos, Mat34V_InOut vMtx)
{
	s32 closestIdx = -1;
	float closestDistSqr = FLT_MAX;
	for (s32 i=0; i<MARKERS_MAX; i++)
	{
		if (m_markers[i].isActive && m_markers[i].info.type==markerType)
		{
			float distSqr = DistSquared(vPos, m_markers[i].info.vPos).Getf();
			if (distSqr < closestDistSqr)
			{
				closestDistSqr = distSqr;
				closestIdx = i;
			}
		}
	}

	if (closestIdx>-1)
	{
		vMtx = m_markers[closestIdx].renderMtx;
		return true;
	}

	return false;
}


// Only need one of these, not one per thread, since there can't be two
// subrender threads processing markers concurrently (there is only one marker
// batch per frame).
GRC_ALLOC_SCOPE_SINGLE_THREADED_DECLARE(static,s_MarkersAllocScope)

///////////////////////////////////////////////////////////////////////////////
//  RenderInit
///////////////////////////////////////////////////////////////////////////////

void			CMarkers::RenderInit					()
{
	GRC_ALLOC_SCOPE_SINGLE_THREADED_PUSH(s_MarkersAllocScope)

 	grcBindTexture(NULL);
	grcViewport::SetCurrentWorldIdentity();
	grcLightState::SetEnabled(false);

	grcStateBlock::SetRasterizerState(ms_rasterizerStateHandle);
	grcStateBlock::SetBlendState(ms_blendStateHandle);
	grcStateBlock::SetDepthStencilState(ms_depthStencilStateHandle);
}


///////////////////////////////////////////////////////////////////////////////
//  GenerateAdjacency
///////////////////////////////////////////////////////////////////////////////

#if USE_ADJACENCY_INFORMATION
static void AdjacencyThread(void *data)
{
	AdjacencyContext *const context = reinterpret_cast<AdjacencyContext*>(data);
	context->status = context->geometry->GenerateAdjacency(context->alive);
}

static bool GenerateAdjacency(rmcDrawable *const drawable)
{
	// clean-up pass
	for(int k=0; k < g_AdjacencyGenerationThreads.GetCount(); ++k)
	{
		if (g_AdjacencyGenerationThreads[k].is_free())
		{
			continue;
		}
		switch (g_AdjacencyGenerationThreads[k].status)
		{
		case grmGeometry::AS_UNSUPPORTED_PLATFORM:
		case grmGeometry::AS_UNSUPPORTED_PRIMITIVE_TYPE:
		case grmGeometry::AS_UNEXPECTED_ERROR:
			#if BASE_DEBUG_NAME
			static char buffer[32];
			Errorf("Adjacency thread[%d] failed with status: %d, was processing object %s",
				k, g_AdjacencyGenerationThreads[k].status, drawable->GetDebugName(buffer, sizeof(buffer)));
			#endif
			// this will restart the thread
			g_AdjacencyGenerationThreads[k].reset();
			break;
		case grmGeometry::AS_ALREADY_DONE:
		case grmGeometry::AS_GENERATED:
			Assert(g_AdjacencyGenerationThreads[k].geometry->GetPrimitiveType() == drawTrisAdj);
			// success
			g_AdjacencyGenerationThreads[k].reset();
			break;
		case grmGeometry::AS_IN_PROGRESS:
			// skip
			break;
		}
	}

	// job processing pass
	const rmcLod& lod = drawable->GetLodGroup().GetLod(0);
	bool success = true;

	for (int i=0; i<lod.GetCount(); i++) {
		grmModel *const model = lod.GetModel(i);
		if (!model)
			continue;

		for (int j=0; j<model->GetUntessellatedGeometryCount(); ++j) {
			grmGeometry *const geometry = &model->GetGeometry(j);
			if (geometry->GetPrimitiveType() != drawTris)
			{
				continue;
			}
			// search the geometry among existing ones
			int free_id = -1;
			int match = 0;
			while(match < g_AdjacencyGenerationThreads.GetCount() && g_AdjacencyGenerationThreads[match].geometry != geometry)
			{
				if (g_AdjacencyGenerationThreads[match].is_free())
				{
					free_id = match;
				}
				++match;
			}
			// create a new thread
			const bool is_processing = (match < g_AdjacencyGenerationThreads.GetCount());
			const bool is_thread_available = (free_id>=0 || !g_AdjacencyGenerationThreads.IsFull());
			if (!is_processing && is_thread_available)
			{
				AdjacencyContext &context = free_id>=0 ? g_AdjacencyGenerationThreads[free_id] : g_AdjacencyGenerationThreads.Append();
				context.alive = true;
				context.geometry = geometry;
				context.status = grmGeometry::AS_IN_PROGRESS;
				context.thread = sysIpcCreateThread(AdjacencyThread, &context, 1024, PRIO_BELOW_NORMAL, "Adjacency Generator");
			}
			success = false;
		}
	}

	return success;
}

#endif //USE_ADJACENCY_INFORMATION

///////////////////////////////////////////////////////////////////////////////
//  RenderMarker
///////////////////////////////////////////////////////////////////////////////

void			CMarkers::RenderMarker				(MarkerType_e type, Vec4V_In vColour, Vec4V_In vClippingPlane, Mat34V_In renderMtx, s32 txdIdx, s32 texIdx, bool invert, Vec3V_In vScale, bool usePreAlphaDepth)
{
	rmcDrawable* pDrawable = g_markers.m_markerTypes[type].pDrawable;

	if (pDrawable)
	{
#if USE_ADJACENCY_INFORMATION
		if (!GenerateAdjacency(pDrawable))
			return;
#endif //USE_ADJACENCY_INFORMATION

		// set the shader variables
		grmShaderGroup* pShaderGroup = &pDrawable->GetShaderGroup();
		pShaderGroup->SetVar(g_markers.m_markerTypes[type].diffuseColorVar, RCC_VECTOR4(vColour));
		pShaderGroup->SetVar(g_markers.m_markerTypes[type].clippingPlaneVar, RCC_VECTOR4(vClippingPlane));
		pShaderGroup->SetVar(g_markers.m_markerTypes[type].targetSizeVar, Vector4((float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, 0.f, 0.f));

		bool replacedTexture = false;
		grmShader &shader = pShaderGroup->GetShader(0);

		const bool useManualTest = WIN32PC_ONLY(GRCDEVICE.IsReadOnlyDepthAllowed() &&) !invert;
#if MARKERS_MANUAL_DEPTH_TEST
		grcRenderTarget*	depth = usePreAlphaDepth ? CRenderTargets::GetDepthBuffer_PreAlpha() : CRenderTargets::GetDepthResolved();

		pShaderGroup->SetVar(g_markers.m_markerTypes[type].distortionParamsVar, PostFX::PostFXParamBlock::GetRenderThreadParams().m_distortionParams);
		pShaderGroup->SetVar(g_markers.m_markerTypes[type].manualDepthTestVar, static_cast<float>(useManualTest));
		shader.SetVar(g_markers.m_markerTypes[type].depthTextureVar, useManualTest ? depth : grcTexture::NoneWhite);
#endif // MARKERS_MANUAL_DEPTH_TEST

		if( txdIdx > -1 )
		{
			vfxAssertf(texIdx > -1,"Bad texture index for marker");

			const fwTxd *txd = g_TxdStore.Get(strLocalIndex(txdIdx));
			vfxAssertf(txd,"NULL texture dictionary for marker");

			grcTexture *tex = txd->GetEntry(texIdx);
			vfxAssertf(tex,"NULLtexture for marker");

			shader.SetVar(g_markers.m_markerTypes[type].diffuseTextureVar, tex);

			replacedTexture = true;
		}
				

		if( invert )
		{
			grcLightState::SetEnabled(true);

			Vec3V camPos = grcViewport::GetCurrent()->GetCameraMtx().d();
			Vec3V markerPos = renderMtx.d();
			ScalarV dist = MagSquared(camPos - markerPos);
			ScalarV scale = Max(Max(vScale.GetX(),vScale.GetY()),vScale.GetZ()) + ScalarV(V_HALF);
			const BoolV camTest = IsLessThan(dist,scale*scale);
			bool camIn = camTest.Getb();
			
			if( camIn )
			{
				grcStateBlock::SetRasterizerState(ms_rasterizerStateHandle);
				grcStateBlock::SetDepthStencilState(ms_invertDepthStencilStateHandle);
				grcStateBlock::SetBlendState(ms_blendStateHandle);
			}
			else
			{
				grcStateBlock::SetRasterizerState(ms_invertRasterizerStateHandle);
				grcStateBlock::SetDepthStencilState(ms_invertDepthStencilStateHandle);
				grcStateBlock::SetBlendState(ms_invertBlendStateHandle);

				// draw the marker
				pDrawable->Draw(RCC_MATRIX34(renderMtx), 0xFFFFFFFF, 0, 0);

				grcStateBlock::SetRasterizerState(ms_rasterizerStateHandle);
				grcStateBlock::SetDepthStencilState(ms_invertP2DepthStencilStateHandle);
				grcStateBlock::SetBlendState(ms_blendStateHandle);
			}
		}
		else
		{
			grcLightState::SetEnabled(false);
			if (useManualTest)
			{
				grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
			}
		}

		// draw the marker
		pDrawable->Draw(RCC_MATRIX34(renderMtx), 0xFFFFFFFF, 0, 0);
		
		if( replacedTexture )
		{
			// Reset texture for the next guy
			shader.SetVar(g_markers.m_markerTypes[type].diffuseTextureVar, g_markers.m_markerTypes[type].diffuseTexture);
		}
		
		if( invert || useManualTest )
		{
			grcStateBlock::SetRasterizerState(ms_rasterizerStateHandle);
			grcStateBlock::SetDepthStencilState(ms_depthStencilStateHandle);
		}
		
	}
	else
	{
		vfxAssertf(0, "CMarkers::RenderMarker - couldn't get drawable");
	}
}


///////////////////////////////////////////////////////////////////////////////
//  RenderEnd
///////////////////////////////////////////////////////////////////////////////

void			CMarkers::RenderEnd					()
{
	grcStateBlock::SetBlendState(ms_exitBlendStateHandle);
	grcStateBlock::SetDepthStencilState(ms_exitDepthStencilStateHandle);
	GRC_ALLOC_SCOPE_SINGLE_THREADED_POP(s_MarkersAllocScope)
}


///////////////////////////////////////////////////////////////////////////////
//  RenderDebug
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CMarkers::RenderDebug				()
{
	if (!m_debugRender)
	{
		return;
	}

	for (s32 i=0; i<MARKERS_MAX; i++)
	{
		if (m_markers[i].isActive && (m_debugRenderId==i || m_debugRenderId==-1))
		{
			// remove any scale from the matrix
			Mat34V debugMtx = m_markers[i].renderMtx;
			debugMtx.SetCol0(Normalize(debugMtx.GetCol0()));
			debugMtx.SetCol1(Normalize(debugMtx.GetCol1()));
			debugMtx.SetCol2(Normalize(debugMtx.GetCol2()));

			// render the matrix
			grcDebugDraw::Axis(debugMtx, 1.0f);

			if (m_markers[i].info.clip)
			{
				Vec4V clippingPlane = m_markers[i].info.vClippingPlane;

				Vec3V planeNormal = clippingPlane.GetXYZ();
				const Vec3V planeOrigin = PlaneProject(clippingPlane, debugMtx.d());

				// Build orthonormal basis
				Vec3V x;
				Vec3V y;
				Vec3V z = planeNormal;

				x = Cross(z, FindMinAbsAxis(z));
				x = Normalize(x);
				y = Cross(x, z);

				Mat34V planeMtx;
				planeMtx.Seta(x);
				planeMtx.Setb(y);
				planeMtx.Setc(z);
				planeMtx.Setd(planeOrigin);

				grcDebugDraw::BoxOriented(
					Negate(m_markers[i].info.vScale) * Vec3V(1.0f, 1.0f, 0.01f),
					m_markers[i].info.vScale * Vec3V(1.0f, 1.0f, 0.01f),
					planeMtx,
					Color32(255, 255, 255, 128),
					true);
			}
			
		}
	}
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  CalcMtx
///////////////////////////////////////////////////////////////////////////////

void			CMarkers::CalcMtx						(Mat34V_InOut vMtx, Vec3V_In vPos, Vec3V_In vDirIn, bool faceCam, bool rotate)
{
	Vec3V vDir = vDirIn;

#if __BANK
	if (m_pointAtPlayer)
	{
		// override dir to point at the player
		Vec3V vPlayerPos = VECTOR3_TO_VEC3V(CGameWorld::FindLocalPlayerCoors());
		vDir = vPlayerPos - vPos;
	}
#endif

	// override direction to face the camera if required
	if (faceCam)
	{
		Vec3V vCameraPos = RCC_VEC3V(camInterface::GetPos());
		vDir = vCameraPos - vPos;
		vDir.SetZ(ScalarV(V_ZERO));
	}
	
	if (rotate)
	{
		vfxAssertf(faceCam == false, "Marker is trying to face camera and rotate at the same time");
		float cost,sint;
		rage::cos_and_sin( cost,sint,m_currRotate );
		vDir.SetX(ScalarV(cost));
		vDir.SetY(ScalarV(sint));
		vDir.SetZ(ScalarV(V_ZERO));
	}
	
	// calc a look at matrix
	if (IsZeroAll(vDir)==false)
	{
		const Vec3V nVDir = -Normalize(vDir);
		const Vec3V axisY = Vec3V(V_Y_AXIS_WZERO);
		const Vec3V axisZ = Vec3V(V_Z_AXIS_WZERO);
		
		const ScalarV diffZ = ScalarV(1.0f) - Abs(nVDir.GetZ());
		const BoolV zTest = IsLessThan(diffZ,ScalarV(V_FLT_SMALL_1));

		const Vec3V align = SelectFT(zTest, axisZ, axisY);
		
		CVfxHelper::CreateMatFromVecYAlignUnSafe(vMtx, Vec3V(V_ZERO), nVDir, align);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  InitWidgets
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CMarkers::InitWidgets			()
{
	const char* typeNames[MARKERTYPE_MAX];
	for (s32 i=0; i<MARKERTYPE_MAX; i++)
	{
		typeNames[i] = m_markerTypes[i].typeName;
	}

	bkBank* pVfxBank = vfxWidget::GetBank();
	pVfxBank->PushGroup("Markers", false);
	{
		pVfxBank->AddTitle("INFO");
		char txt[128];
		sprintf(txt, "Num Active (%d)", MARKERS_MAX);
		pVfxBank->AddSlider(txt, &m_numActiveMarkers, 0, MARKERS_MAX, 0);

#if __DEV
		pVfxBank->AddTitle("");
		pVfxBank->AddTitle("SETTINGS");
		pVfxBank->AddSlider("Bounce Speed", &MARKERS_BOUNCE_SPEED, 0.0f, 100.0f, 0.5f, NullCB, "");
		pVfxBank->AddSlider("Bounce Height", &MARKERS_BOUNCE_HEIGHT, 0.0f, 1.0f, 0.05f, NullCB, "");
		pVfxBank->AddSlider("Rotate Speed", &MARKERS_ROTATION_SPEED, -10.0f, 10.0f, 0.05f, NullCB, "");
#endif

		pVfxBank->AddTitle("");
		pVfxBank->AddTitle("DEBUG");
		pVfxBank->AddToggle("Disable Render", &m_disableRender);
		pVfxBank->AddToggle("Debug Render", &m_debugRender);
		pVfxBank->AddSlider("Debug Render Id", &m_debugRenderId, -1, MARKERS_MAX-1, 1);
		pVfxBank->AddToggle("Point At Player", &m_pointAtPlayer);
		pVfxBank->AddToggle("Disable Clipping Plane", &m_disableClippingPlane);
		pVfxBank->AddToggle("Render Bounding Box on Picker Entity", &m_renderBoxOnPickerEntity);
		pVfxBank->AddToggle("Force Entity Rotation Order", &m_forceEntityRotOrder);
		
		pVfxBank->AddTitle("");
		pVfxBank->AddTitle("DEBUG MARKER");
		pVfxBank->AddButton("Add", DebugAdd);
		pVfxBank->AddButton("Remove", DebugRemove);
		pVfxBank->AddCombo("Type", (s32*)&m_debugMarkerType, MARKERTYPE_MAX, typeNames);
		pVfxBank->AddSlider("Scale X", &m_debugMarkerScaleX, 0.0, 250.0, 0.1f);
		pVfxBank->AddSlider("Scale Y", &m_debugMarkerScaleY, 0.0, 250.0, 0.1f);
		pVfxBank->AddSlider("Scale Z", &m_debugMarkerScaleZ, 0.0, 250.0, 0.1f);
		pVfxBank->AddSlider("Col R", &m_debugMarkerColR, 0, 255, 1);
		pVfxBank->AddSlider("Col G", &m_debugMarkerColG, 0, 255, 1);
		pVfxBank->AddSlider("Col B", &m_debugMarkerColB, 0, 255, 1);
		pVfxBank->AddSlider("Col A", &m_debugMarkerColA, 0, 255, 1);
		pVfxBank->AddToggle("Bounce", &m_debugMarkerBounce);
		pVfxBank->AddToggle("Rotate", &m_debugMarkerRotate);
		pVfxBank->AddToggle("Invert", &m_debugMarkerInvert);
		pVfxBank->AddToggle("Face Cam", &m_debugMarkerFaceCam);
		pVfxBank->AddToggle("Face Player", &m_debugMarkerFacePlayer);
		pVfxBank->AddToggle("Track Player", &m_debugMarkerTrackPlayer);
		pVfxBank->AddToggle("Override Texture", &m_debugOverrideTexture);
		pVfxBank->AddToggle("Clip", &m_debugMarkerClip);
		pVfxBank->AddSlider("Direction X", &m_debugMarkerDirX, -1.0f, 1.0f, 0.01f);
		pVfxBank->AddSlider("Direction Y", &m_debugMarkerDirY, -1.0f, 1.0f, 0.01f);
		pVfxBank->AddSlider("Direction Z", &m_debugMarkerDirZ, -1.0f, 1.0f, 0.01f);
		pVfxBank->AddSlider("Rotate X", &m_debugMarkerRotX, -TWO_PI, TWO_PI, HALF_PI);
		pVfxBank->AddSlider("Rotate Y", &m_debugMarkerRotY, -TWO_PI, TWO_PI, HALF_PI);
		pVfxBank->AddSlider("Rotate Z", &m_debugMarkerRotZ, -TWO_PI, TWO_PI, HALF_PI);
		pVfxBank->AddSlider("Rotate Cam Tilt", &m_debugMarkerRotCamTilt, -TWO_PI, TWO_PI, HALF_PI);
		pVfxBank->AddSlider("Clip Plane Pos X", &m_debugMarkerPlanePosX, -16000.0f, 16000.0f, 0.01f);
		pVfxBank->AddSlider("Clip Plane Pos Y", &m_debugMarkerPlanePosY, -16000.0f, 16000.0f, 0.01f);
		pVfxBank->AddSlider("Clip Plane Pos Z", &m_debugMarkerPlanePosZ, -16000.0f, 16000.0f, 0.01f);
		pVfxBank->AddSlider("Clip Plane Normal X", &m_debugMarkerPlaneNormalX, -1.0f, 1.0f, 0.01f);
		pVfxBank->AddSlider("Clip Plane Normal Y", &m_debugMarkerPlaneNormalY, -1.0f, 1.0f, 0.01f);
		pVfxBank->AddSlider("Clip Plane Normal Z", &m_debugMarkerPlaneNormalZ, -1.0f, 1.0f, 0.01f);

		pVfxBank->AddTitle("");
		pVfxBank->AddTitle("MARKERS MODELS");
		pVfxBank->AddButton("Reload", Reload);
	}
	pVfxBank->PopGroup();
}
#endif


///////////////////////////////////////////////////////////////////////////////
// Reset
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CMarkers::Reload							()
{
	g_markers.Shutdown(SHUTDOWN_SESSION);
	g_markers.Init(INIT_SESSION);
	CStreaming::LoadAllRequestedObjects();
	g_markers.InitSessionSync();
}
#endif // __BANK


///////////////////////////////////////////////////////////////////////////////
//  DebugAdd
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CMarkers::DebugAdd					()
{
	Vec3V vPlayerCoords = VECTOR3_TO_VEC3V(CGameWorld::FindLocalPlayerCoors());

	g_markers.m_debugMarkerActive = true;
	g_markers.m_vDebugMarkerPos = vPlayerCoords + Vec3V(0.0f, 0.0f, 0.5f);
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  DebugRemove
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CMarkers::DebugRemove				()
{
	g_markers.m_debugMarkerActive = false;
}
#endif









