///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	Checkpoints.cpp
//	BY	: 	Mark Nicholson (Adapted from original by Obbe)
//	FOR	:	Rockstar North
//	ON	:	01 Aug 2008
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "Checkpoints.h"

// Rage headers
#include "bank/bank.h"

// Framework headers
#include "grcore/debugdraw.h"
#include "fwmaths/angle.h"
#include "vfx/channel.h"
#include "vfx/vfxwidget.h"

// Game header
#include "Camera/CamInterface.h"
#include "Core/Game.h"
#include "FrontEnd/MobilePhone.h"
#include "Physics/WorldProbe/WorldProbe.h"
#include "Renderer/PostProcessFXHelper.h"
#include "Scene/World/GameWorld.h"
#include "Vfx/VfxHelper.h"
#include "Vfx/Misc/Markers.h"
#include "Vfx/Decals/DecalManager.h"


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_MISC_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

bank_float		CHECKPOINT_INSIDE_RING_SCALE_CHEVRON		= 0.5f;
bank_float		CHECKPOINT_INSIDE_RING_SCALE_LAP			= 0.5f;
bank_float		CHECKPOINT_INSIDE_RING_SCALE_FLAG			= 1.0f;
bank_float		CHECKPOINT_INSIDE_RING_SCALE_PLANE			= 0.6f;
bank_float		CHECKPOINT_INSIDE_RING_SCALE_MONEY			= 0.8f;
bank_float		CHECKPOINT_INSIDE_RING_SCALE_BEAST			= 0.9f;
bank_float		CHECKPOINT_INSIDE_RING_SCALE_TRANSFORM		= 0.6f;
bank_float		CHECKPOINT_INSIDE_RING_SCALE_WARP			= 0.6f;
bank_float		CHECKPOINT_INSIDE_CYLINDER_SCALE_CHEVRON	= 0.6f;
bank_float		CHECKPOINT_INSIDE_CYLINDER_SCALE_LAP		= 0.6f;
bank_float		CHECKPOINT_INSIDE_CYLINDER_SCALE_FLAG		= 0.6f;
bank_float		CHECKPOINT_INSIDE_CYLINDER_SCALE_PIT_LANE	= 0.6f;
bank_float		CHECKPOINT_INSIDE_CYLINDER_SCALE_NUMBER		= 0.35f;
bank_float		CHECKPOINT_CHEVRON_IN_CYLINDER_HEIGHT		= 0.5f;
bank_float		CHECKPOINT_LAP_IN_CYLINDER_HEIGHT			= 0.5f;
bank_float		CHECKPOINT_FLAG_IN_CYLINDER_HEIGHT			= 0.5f;
bank_float		CHECKPOINT_PIT_LANE_IN_CYLINDER_HEIGHT		= 0.5f;
bank_float		CHECKPOINT_NUMBER_IN_CYLINDER_HEIGHT		= 0.5f;
bank_float		CHECKPOINT_NUMBER_SIDE_OFFSET				= 0.35f;
bank_float		CHECKPOINT_CYLINDER_Z_MIN					= 6.0f;
bank_float		CHECKPOINT_CYLINDER_Z_MAX					= 30.0f;
bank_float		CHECKPOINT_CYLINDER_Z_DIST					= 30.0f;
bank_float		CHECKPOINT_CYLINDER_XY_SCALE_RACES			= 1.0f;
bank_float		CHECKPOINT_CYLINDER_ALPHA_MULT				= 0.5f;
bank_float		CHECKPOINT_CYLINDER_ALPHA_MULT_GROUND_RACES	= 0.65f;
bank_bool		CHECKPOINT_USE_ALTERNATE_CHEVRON_POINTING	= true;
bank_float		CHECKPOINT_ALTERNATE_CHEVRON_CAM_TILT		= 0.5f;
#if __DEV || __BANK
bank_u32		CHECKPOINT_DEFAULT_COL_R					= 255;	
bank_u32		CHECKPOINT_DEFAULT_COL_G					= 100;	
bank_u32		CHECKPOINT_DEFAULT_COL_B					= 0;	
bank_u32		CHECKPOINT_DEFAULT_COL_A					= 50;	
bank_u32		CHECKPOINT_DEFAULT_COL_R2					= 255;	
bank_u32		CHECKPOINT_DEFAULT_COL_G2					= 100;	
bank_u32		CHECKPOINT_DEFAULT_COL_B2					= 0;	
bank_u32		CHECKPOINT_DEFAULT_COL_A2					= 50;	
#endif


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

CCheckpoints		g_checkpoints;

#if __BANK
const char* g_typeNames[CHECKPOINTTYPE_MAX] =
{
	// ground races - positioned at base of cylinder
	"GROUND RACE CHEVRON 1 (BASE)",
	"GROUND RACE CHEVRON 2 (BASE)",	
	"GROUND RACE CHEVRON 3 (BASE)",	
	"GROUND RACE LAP (BASE)",									
	"GROUND RACE FLAG (BASE)",								
	"GROUND RACE PIT LANE (BASE)",

	// ground races - positioned at centre of chevron/flag etc
	"GROUND RACE CHEVRON 1",	
	"GROUND RACE CHEVRON 2",	
	"GROUND RACE CHEVRON 3",	
	"GROUND RACE LAP",							
	"GROUND RACE FLAG",
	"GROUND RACE PIT LANE",

	// air races
	"AIR RACE CHEVRON 1",	
	"AIR RACE CHEVRON 2",
	"AIR RACE CHEVRON 3",
	"AIR RACE LAP",
	"AIR RACE FLAG",

	// water races
	"WATER RACE CHEVRON 1",	
	"WATER RACE CHEVRON 2",	
	"WATER RACE CHEVRON 3",	
	"WATER RACE LAP",	
	"WATER RACE FLAG",

	// triathlon running races
	"TRIATHLON RUNNING CHEVRON 1",	
	"TRIATHLON RUNNING CHEVRON 2",	
	"TRIATHLON RUNNING CHEVRON 3",	
	"TRIATHLON RUNNING LAP",						
	"TRIATHLON RUNNING FLAG",	

	// triathlon swimming races	
	"TRIATHLON SWIMMING CHEVRON 1",	
	"TRIATHLON SWIMMING CHEVRON 2",	
	"TRIATHLON SWIMMING CHEVRON 3",	
	"TRIATHLON SWIMMING LAP",					
	"TRIATHLON SWIMMING FLAG",	

	// triathlon cycling races
	"TRIATHLON CYCLING CHEVRON 1",	
	"TRIATHLON CYCLING CHEVRON 2",	
	"TRIATHLON CYCLING CHEVRON 3",	
	"TRIATHLON CYCLING LAP",					
	"TRIATHLON CYCLING FLAG",

	// plane school
	"PLANE FLAT",
	"PLANE SIDE L",
	"PLANE SIDE R",
	"PLANE INVERTED",

	// heli
	"HELI LANDING",

	// parachute
	"PARACHUTE RING",
	"PARACHUTE LANDING",

	// gang locators
	"LOST GANG LOCATE",				
	"VAGOS GANG LOCATE",					
	"COPS GANG LOCATE",

	// race locators
	"GROUND RACE LOCATE",					
	"AIR RACE LOCATE",				
	"WATER RACE LOCATE",	

	// mp locators
	"MP LOCATE 1",
	"MP LOCATE 2",
	"MP LOCATE 3",	
	"MP CREATOR TRIGGER",

	// freemode 
	"MONEY",

	// misc 
	"BEAST",

	// transforms
	"TRANSFORM",
	"TRANSFORM_PLANE",
	"TRANSFORM_HELICOPTER",
	"TRANSFORM_BOAT",
	"TRANSFORM_CAR",
	"TRANSFORM_BIKE",
	"TRANSFORM_PUSH_BIKE",
	"TRANSFORM_TRUCK",
	"TRANSFORM_PARACHUTE",
	"TRANSFORM_THRUSTER",

	"WARP"
};
#endif


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CCheckpoints
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void			CCheckpoints::Init						(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
#if __BANK
		m_pointAtPlayer = false;
	    m_disableRender = false;
	    m_debugRender = false;
	    m_debugRenderId = -1;

	    m_numActiveCheckpoints = 0;

	    m_debugCheckpointType = CHECKPOINTTYPE_RACE_GROUND_CHEVRON_1_BASE;
	    m_debugCheckpointScale = 1.0f;
		m_debugCheckpointInsideCylinderHeightScale = 1.0f;
		m_debugCheckpointInsideCylinderScale = 1.0f;
		m_debugCheckpointCylinderHeightMin = CHECKPOINT_CYLINDER_Z_MIN;
		m_debugCheckpointCylinderHeightMax = CHECKPOINT_CYLINDER_Z_MAX;
		m_debugCheckpointCylinderHeightDist = CHECKPOINT_CYLINDER_Z_DIST;

		m_debugCheckpointColR = (u8)CHECKPOINT_DEFAULT_COL_R;
		m_debugCheckpointColG = (u8)CHECKPOINT_DEFAULT_COL_G;
		m_debugCheckpointColB = (u8)CHECKPOINT_DEFAULT_COL_B;
		m_debugCheckpointColA = (u8)CHECKPOINT_DEFAULT_COL_A;

		m_debugCheckpointColR2 = (u8)CHECKPOINT_DEFAULT_COL_R2;
		m_debugCheckpointColG2 = (u8)CHECKPOINT_DEFAULT_COL_G2;
		m_debugCheckpointColB2 = (u8)CHECKPOINT_DEFAULT_COL_B2;
		m_debugCheckpointColA2 = (u8)CHECKPOINT_DEFAULT_COL_A2;

		m_debugCheckpointPlaneNormalX = 0.0f;
		m_debugCheckpointPlaneNormalY = 0.0f;
		m_debugCheckpointPlaneNormalZ = 1.0f;
		m_debugCheckpointPlanePosX = 0.0f;
		m_debugCheckpointPlanePosY = 0.0f;
		m_debugCheckpointPlanePosZ = 0.0f;

		m_debugCheckpointNum = 0;
#endif
    }
    else if(initMode == INIT_AFTER_MAP_LOADED)
    {
        for (s32 i = 0; i < CHECKPOINTS_MAX; i++)
	    {
		    Delete(i);
	    }

#if __BANK
	    m_numActiveCheckpoints = 0;
#endif
    }
}


///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void			CCheckpoints::Shutdown					(unsigned UNUSED_PARAM(shutdownMode))
{	
}


///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void			CCheckpoints::Update						()
{
#if __BANK
	if (m_disableRender)
	{
		return;
	}
#endif

	bool isTakingPhoto = CPhoneMgr::CamGetState();
	if (isTakingPhoto)
	{
		return;
	}

	// go through the checkpoints and register them as markers
	for (s32 i=0; i<CHECKPOINTS_MAX; i++)
	{
		Checkpoint_t* pCurrCheckpoint = &m_checkpoints[i];
		if (pCurrCheckpoint->isActive)
		{
#if __BANK
			if (m_pointAtPlayer)
			{
				Vec3V vPlayerCoords = VECTOR3_TO_VEC3V(CGameWorld::FindLocalPlayerCoors());
				pCurrCheckpoint->vLookAtPos = vPlayerCoords;
			}
#endif
			// CHEVRON 1
			if (pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_CHEVRON_1_BASE ||
				pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_CHEVRON_1 || 
				pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_AIR_CHEVRON_1 || 
				pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_WATER_CHEVRON_1 || 
				pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_RUN_CHEVRON_1 ||
				pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_SWIM_CHEVRON_1 ||
				pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_CYCLE_CHEVRON_1)
			{
				float insideScale = 1.0f;
				if (pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_CHEVRON_1_BASE || 
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_CHEVRON_1 || 
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_RUN_CHEVRON_1 ||
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_SWIM_CHEVRON_1 ||
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_CYCLE_CHEVRON_1)
				{
					insideScale = CHECKPOINT_INSIDE_CYLINDER_SCALE_CHEVRON * pCurrCheckpoint->insideCylinderScale;
				}
				else if (pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_AIR_CHEVRON_1)
				{
					insideScale = CHECKPOINT_INSIDE_RING_SCALE_CHEVRON;
				}

				// set up and register the marker
				MarkerInfo_t markerInfo;
				markerInfo.type = MARKERTYPE_CHEVRON_1;
				markerInfo.vPos = pCurrCheckpoint->vPos;
				markerInfo.vDir = pCurrCheckpoint->vLookAtPos - pCurrCheckpoint->vPos;
				markerInfo.vRot = Vec4V(HALF_PI, HALF_PI, 0.0f, 0.0f);
				markerInfo.vScale = Vec3V(pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale);
				markerInfo.col = pCurrCheckpoint->col2;
				markerInfo.clip = IsZeroAll(pCurrCheckpoint->vClippingPlane)==false;
				markerInfo.vClippingPlane = pCurrCheckpoint->vClippingPlane;

				if (CHECKPOINT_USE_ALTERNATE_CHEVRON_POINTING && pCurrCheckpoint->forceOldArrowPointing==false)
				{
					markerInfo.vDir.SetZf(0.0f);
					markerInfo.vRot = Vec4V(HALF_PI, 0.0f, 0.0f, CHECKPOINT_ALTERNATE_CHEVRON_CAM_TILT);
				}

				// adjust the position of the 'base' chevron so the input position defines the base of the chevron
				if (pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_CHEVRON_1_BASE ||
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_RUN_CHEVRON_1 ||
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_SWIM_CHEVRON_1 ||
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_CYCLE_CHEVRON_1)
				{
					float zOffset = (-g_markers.GetBBMinZ(MARKERTYPE_CHEVRON_1) + CHECKPOINT_CHEVRON_IN_CYLINDER_HEIGHT) * pCurrCheckpoint->scale * pCurrCheckpoint->insideCylinderHeightScale;
					markerInfo.vPos.SetZf(pCurrCheckpoint->vPos.GetZf() + zOffset);
				}

				g_markers.Register(markerInfo);
			}
			// CHEVRON_2
			else if (pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_CHEVRON_2_BASE ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_CHEVRON_2 || 
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_AIR_CHEVRON_2 || 
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_WATER_CHEVRON_2 || 
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_RUN_CHEVRON_2 ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_SWIM_CHEVRON_2 ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_CYCLE_CHEVRON_2)
			{
				float insideScale = 1.0f;
				if (pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_CHEVRON_2_BASE || 
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_CHEVRON_2 || 
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_RUN_CHEVRON_2 ||
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_SWIM_CHEVRON_2 ||
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_CYCLE_CHEVRON_2)
				{
					insideScale = CHECKPOINT_INSIDE_CYLINDER_SCALE_CHEVRON * pCurrCheckpoint->insideCylinderScale;
				}
				else if (pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_AIR_CHEVRON_2)
				{
					insideScale = CHECKPOINT_INSIDE_RING_SCALE_CHEVRON;
				}

				// set up and register the marker
				MarkerInfo_t markerInfo;
				markerInfo.type = MARKERTYPE_CHEVRON_2;
				markerInfo.vPos = pCurrCheckpoint->vPos;
				markerInfo.vDir = pCurrCheckpoint->vLookAtPos - pCurrCheckpoint->vPos;
				markerInfo.vRot = Vec4V(HALF_PI, HALF_PI, 0.0f, 0.0f);
				markerInfo.vScale = Vec3V(pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale);
				markerInfo.col = pCurrCheckpoint->col2;
				markerInfo.clip = IsZeroAll(pCurrCheckpoint->vClippingPlane)==false;
				markerInfo.vClippingPlane = pCurrCheckpoint->vClippingPlane;

				if (CHECKPOINT_USE_ALTERNATE_CHEVRON_POINTING && pCurrCheckpoint->forceOldArrowPointing==false)
				{
					markerInfo.vDir.SetZf(0.0f);
					markerInfo.vRot = Vec4V(HALF_PI, 0.0f, 0.0f, CHECKPOINT_ALTERNATE_CHEVRON_CAM_TILT);
				}

				// adjust the position of the 'base' chevron so the input position defines the base of the chevron
				if (pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_CHEVRON_2_BASE ||
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_RUN_CHEVRON_2 ||
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_SWIM_CHEVRON_2 ||
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_CYCLE_CHEVRON_2)
				{
					float zOffset = (-g_markers.GetBBMinZ(MARKERTYPE_CHEVRON_2) + CHECKPOINT_CHEVRON_IN_CYLINDER_HEIGHT) * pCurrCheckpoint->scale * pCurrCheckpoint->insideCylinderHeightScale;
					markerInfo.vPos.SetZf(pCurrCheckpoint->vPos.GetZf() + zOffset);
				}

				g_markers.Register(markerInfo);
			}
			// CHEVRON_3
			else if (pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_CHEVRON_3_BASE ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_CHEVRON_3 || 
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_AIR_CHEVRON_3 || 
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_WATER_CHEVRON_3 || 
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_RUN_CHEVRON_3 ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_SWIM_CHEVRON_3 ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_CYCLE_CHEVRON_3)
			{
				float insideScale = 1.0f;
				if (pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_CHEVRON_3_BASE || 
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_CHEVRON_3 || 
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_RUN_CHEVRON_3 ||
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_SWIM_CHEVRON_3 ||
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_CYCLE_CHEVRON_3)
				{
					insideScale = CHECKPOINT_INSIDE_CYLINDER_SCALE_CHEVRON * pCurrCheckpoint->insideCylinderScale;
				}
				else if (pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_AIR_CHEVRON_3)
				{
					insideScale = CHECKPOINT_INSIDE_RING_SCALE_CHEVRON;
				}

				// set up and register the marker
				MarkerInfo_t markerInfo;
				markerInfo.type = MARKERTYPE_CHEVRON_3;
				markerInfo.vPos = pCurrCheckpoint->vPos;
				markerInfo.vDir = pCurrCheckpoint->vLookAtPos - pCurrCheckpoint->vPos;
				markerInfo.vRot = Vec4V(HALF_PI, HALF_PI, 0.0f, 0.0f);
				markerInfo.vScale = Vec3V(pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale);
				markerInfo.col = pCurrCheckpoint->col2;
				markerInfo.clip = IsZeroAll(pCurrCheckpoint->vClippingPlane)==false;
				markerInfo.vClippingPlane = pCurrCheckpoint->vClippingPlane;

				if (CHECKPOINT_USE_ALTERNATE_CHEVRON_POINTING && pCurrCheckpoint->forceOldArrowPointing==false)
				{
					markerInfo.vDir.SetZf(0.0f);
					markerInfo.vRot = Vec4V(HALF_PI, 0.0f, 0.0f, CHECKPOINT_ALTERNATE_CHEVRON_CAM_TILT);
				}

				// adjust the position of the 'base' chevron so the input position defines the base of the chevron
				if (pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_CHEVRON_3_BASE ||
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_RUN_CHEVRON_3 ||
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_SWIM_CHEVRON_3 ||
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_CYCLE_CHEVRON_3)
				{
					float zOffset = (-g_markers.GetBBMinZ(MARKERTYPE_CHEVRON_3) + CHECKPOINT_CHEVRON_IN_CYLINDER_HEIGHT) * pCurrCheckpoint->scale * pCurrCheckpoint->insideCylinderHeightScale;
					markerInfo.vPos.SetZf(pCurrCheckpoint->vPos.GetZf() + zOffset);
				}

				g_markers.Register(markerInfo);
			}
			// LAP
			else if (pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_LAP_BASE ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_LAP || 
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_AIR_LAP || 
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_WATER_LAP || 
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_RUN_LAP ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_SWIM_LAP ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_CYCLE_LAP)
			{
				float insideScale = 1.0f;
				if (pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_LAP_BASE || 
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_LAP || 
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_RUN_LAP ||
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_SWIM_LAP ||
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_CYCLE_LAP)
				{
					insideScale = CHECKPOINT_INSIDE_CYLINDER_SCALE_LAP * pCurrCheckpoint->insideCylinderScale;
				}
				else if (pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_AIR_LAP)
				{
					insideScale = CHECKPOINT_INSIDE_RING_SCALE_LAP;
				}

				// set up and register the marker
				MarkerInfo_t markerInfo;
				markerInfo.type = MARKERTYPE_LAP;
				markerInfo.vPos = pCurrCheckpoint->vPos;
				markerInfo.vDir = Vec3V(V_ZERO);
				markerInfo.vRot = Vec4V(V_ZERO);
				markerInfo.vScale = Vec3V(pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale);
				markerInfo.col = pCurrCheckpoint->col2;
				markerInfo.faceCam = true;
				markerInfo.clip = IsZeroAll(pCurrCheckpoint->vClippingPlane)==false;
				markerInfo.vClippingPlane = pCurrCheckpoint->vClippingPlane;

				// adjust the position of the 'base' lap so the input position defines the base of the lap
				if (pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_LAP_BASE ||
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_RUN_LAP ||
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_SWIM_LAP ||
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_CYCLE_LAP)
				{
					float zOffset = (-g_markers.GetBBMinZ(MARKERTYPE_LAP) + CHECKPOINT_LAP_IN_CYLINDER_HEIGHT) * pCurrCheckpoint->scale * pCurrCheckpoint->insideCylinderHeightScale;
					markerInfo.vPos.SetZf(pCurrCheckpoint->vPos.GetZf() + zOffset);
				}

				g_markers.Register(markerInfo);
			}
			// FLAG
			else if (pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_FLAG_BASE ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_FLAG || 
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_AIR_FLAG || 
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_WATER_FLAG || 
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_RUN_FLAG ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_SWIM_FLAG ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_CYCLE_FLAG)
			{
				MarkerType_e markerType = MARKERTYPE_FLAG;
				float insideScale = 1.0f;
				if (pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_FLAG_BASE || 
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_FLAG || 
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_RUN_FLAG ||
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_SWIM_FLAG ||
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_CYCLE_FLAG)
				{
					markerType = MARKERTYPE_FLAG;
					insideScale = CHECKPOINT_INSIDE_CYLINDER_SCALE_FLAG * pCurrCheckpoint->insideCylinderScale;
				}
				else if (pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_AIR_FLAG)
				{
					markerType = MARKERTYPE_RING_FLAG;
					insideScale = CHECKPOINT_INSIDE_RING_SCALE_FLAG;
				}
					
				// set up and register the marker
				MarkerInfo_t markerInfo;
				markerInfo.type = markerType;
				markerInfo.vPos = pCurrCheckpoint->vPos;
				markerInfo.vScale = Vec3V(pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale);
				markerInfo.col = pCurrCheckpoint->col2;
				markerInfo.faceCam = true;
				markerInfo.clip = IsZeroAll(pCurrCheckpoint->vClippingPlane)==false;
				markerInfo.vClippingPlane = pCurrCheckpoint->vClippingPlane;

				// adjust the position of the 'base' flag so the input position defines the base of the flag
				if (pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_FLAG_BASE ||
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_RUN_FLAG ||
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_SWIM_FLAG ||
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_CYCLE_FLAG)
				{
					float zOffset = (-g_markers.GetBBMinZ(MARKERTYPE_FLAG) + CHECKPOINT_FLAG_IN_CYLINDER_HEIGHT) * pCurrCheckpoint->scale * pCurrCheckpoint->insideCylinderHeightScale;
					markerInfo.vPos.SetZf(pCurrCheckpoint->vPos.GetZf() + zOffset);
				}

				g_markers.Register(markerInfo);
			}
			// PIT LANE
			else if (pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_PIT_LANE ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_PIT_LANE_BASE)
			{
				MarkerType_e markerType = MARKERTYPE_PIT_LANE;
				float insideScale = CHECKPOINT_INSIDE_CYLINDER_SCALE_PIT_LANE * pCurrCheckpoint->insideCylinderScale;
					
				// set up and register the marker
				MarkerInfo_t markerInfo;
				markerInfo.type = markerType;
				markerInfo.vPos = pCurrCheckpoint->vPos;
				markerInfo.vScale = Vec3V(pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale);
				markerInfo.col = pCurrCheckpoint->col2;
				markerInfo.faceCam = true;
				markerInfo.clip = IsZeroAll(pCurrCheckpoint->vClippingPlane)==false;
				markerInfo.vClippingPlane = pCurrCheckpoint->vClippingPlane;
				
				if (pCurrCheckpoint->type == CHECKPOINTTYPE_RACE_GROUND_PIT_LANE_BASE)
				{
					float zOffset = (-g_markers.GetBBMinZ(MARKERTYPE_FLAG) + CHECKPOINT_PIT_LANE_IN_CYLINDER_HEIGHT) * pCurrCheckpoint->scale * pCurrCheckpoint->insideCylinderHeightScale;
					markerInfo.vPos.SetZf(pCurrCheckpoint->vPos.GetZf() + zOffset);
				}

				g_markers.Register(markerInfo);
			}
			// PLANE
			else if (pCurrCheckpoint->type==CHECKPOINTTYPE_PLANE_FLAT || 
					 pCurrCheckpoint->type==CHECKPOINTTYPE_PLANE_SIDE_L || 
					 pCurrCheckpoint->type==CHECKPOINTTYPE_PLANE_SIDE_R ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_PLANE_INVERTED)
			{
				float insideScale = CHECKPOINT_INSIDE_RING_SCALE_PLANE;

				// calc y rotation
				float yRot = 0.0f;
				if (pCurrCheckpoint->type==CHECKPOINTTYPE_PLANE_SIDE_L)
				{
					yRot = -HALF_PI;
				}
				else if (pCurrCheckpoint->type==CHECKPOINTTYPE_PLANE_SIDE_R)
				{
					yRot = HALF_PI;
				}
				else if (pCurrCheckpoint->type==CHECKPOINTTYPE_PLANE_INVERTED)
				{
					yRot = PI;
				}

				// set up and register the marker
				MarkerInfo_t markerInfo;
				markerInfo.type = MARKERTYPE_PLANE;
				markerInfo.vPos = pCurrCheckpoint->vPos;
				markerInfo.vRot = Vec4V(0.0f, yRot, PI, 0.0f);
				markerInfo.vScale = Vec3V(pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale);
				markerInfo.col = pCurrCheckpoint->col2;
				markerInfo.faceCam = true;
				markerInfo.clip = IsZeroAll(pCurrCheckpoint->vClippingPlane)==false;
				markerInfo.vClippingPlane = pCurrCheckpoint->vClippingPlane;

				if (pCurrCheckpoint->forceDirection)
				{
					markerInfo.faceCam = false;
					markerInfo.vDir = pCurrCheckpoint->vLookAtPos - pCurrCheckpoint->vPos;
				}

				g_markers.Register(markerInfo);
			}
			// NUMBERS
			else if (pCurrCheckpoint->type==CHECKPOINTTYPE_GANG_LOCATE_LOST ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_GANG_LOCATE_VAGOS ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_GANG_LOCATE_COPS)
			{
				float insideScale = CHECKPOINT_INSIDE_CYLINDER_SCALE_NUMBER * CHECKPOINT_INSIDE_CYLINDER_SCALE_NUMBER;

				float zOffset = CHECKPOINT_NUMBER_IN_CYLINDER_HEIGHT * pCurrCheckpoint->scale * pCurrCheckpoint->insideCylinderHeightScale;

				if (pCurrCheckpoint->num<10)
				{
					// set up and register the marker
					MarkerInfo_t markerInfo;
					markerInfo.type = (MarkerType_e)(MARKERTYPE_NUM_0+pCurrCheckpoint->num);
					markerInfo.vPos = pCurrCheckpoint->vPos;
					markerInfo.vPos.SetZf(pCurrCheckpoint->vPos.GetZf() + zOffset);
					markerInfo.vScale = Vec3V(pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale);
					markerInfo.col = pCurrCheckpoint->col2;
					markerInfo.faceCam = true;
					markerInfo.clip = IsZeroAll(pCurrCheckpoint->vClippingPlane)==false;
					markerInfo.vClippingPlane = pCurrCheckpoint->vClippingPlane;

					g_markers.Register(markerInfo);
				}
				else
				{
					Vec3V_ConstRef vCamPos = RCC_VEC3V(camInterface::GetPos());
					Vec3V vCheckToCamVec = vCamPos-pCurrCheckpoint->vPos;
					vCheckToCamVec.SetZ(ScalarV(V_ZERO));
					vCheckToCamVec = Normalize(vCheckToCamVec);

					Vec3V vCamSide = Vec3V(-vCheckToCamVec.GetY(), vCheckToCamVec.GetX(), ScalarV(V_ZERO));
					Vec3V vSideOffset = vCamSide*ScalarVFromF32(pCurrCheckpoint->scale*insideScale*CHECKPOINT_NUMBER_SIDE_OFFSET);

					int numTens = pCurrCheckpoint->num/10;
					int numUnits = pCurrCheckpoint->num-(numTens*10);

					// set up and register the markers
					MarkerInfo_t markerInfo;
					markerInfo.type = (MarkerType_e)(MARKERTYPE_NUM_0+numTens);
					markerInfo.vPos = pCurrCheckpoint->vPos - vSideOffset;
					markerInfo.vPos.SetZf(pCurrCheckpoint->vPos.GetZf() + zOffset);
					markerInfo.vDir = vCheckToCamVec;
					markerInfo.vScale = Vec3V(pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale);
					markerInfo.col = pCurrCheckpoint->col2;
					markerInfo.clip = IsZeroAll(pCurrCheckpoint->vClippingPlane)==false;
					markerInfo.vClippingPlane = pCurrCheckpoint->vClippingPlane;

					MarkerInfo_t markerInfo2;
					markerInfo2.type = (MarkerType_e)(MARKERTYPE_NUM_0+numUnits);
					markerInfo2.vPos = pCurrCheckpoint->vPos + vSideOffset;
					markerInfo2.vPos.SetZf(pCurrCheckpoint->vPos.GetZf() + zOffset);
					markerInfo2.vDir = vCheckToCamVec;
					markerInfo2.vScale = Vec3V(pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale);
					markerInfo2.col = pCurrCheckpoint->col2;
					markerInfo.clip = IsZeroAll(pCurrCheckpoint->vClippingPlane)==false;
					markerInfo2.vClippingPlane = pCurrCheckpoint->vClippingPlane;

					g_markers.Register(markerInfo);
					g_markers.Register(markerInfo2);

				}
			}
			// MONEY
			else if (pCurrCheckpoint->type==CHECKPOINTTYPE_MONEY)
			{
				MarkerType_e markerType = MARKERTYPE_MONEY;
				float insideScale = CHECKPOINT_INSIDE_RING_SCALE_MONEY;

				// set up and register the marker
				MarkerInfo_t markerInfo;
				markerInfo.type = markerType;
				markerInfo.vPos = pCurrCheckpoint->vPos;
				markerInfo.vScale = Vec3V(pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale);
				markerInfo.col = pCurrCheckpoint->col2;
				markerInfo.faceCam = true;
				markerInfo.clip = IsZeroAll(pCurrCheckpoint->vClippingPlane)==false;
				markerInfo.vClippingPlane = pCurrCheckpoint->vClippingPlane;

				g_markers.Register(markerInfo);
			}
			// BEAST
			else if (pCurrCheckpoint->type==CHECKPOINTTYPE_BEAST)
			{
				MarkerType_e markerType = MARKERTYPE_BEAST;
				float insideScale = CHECKPOINT_INSIDE_RING_SCALE_BEAST;

				// set up and register the marker
				MarkerInfo_t markerInfo;
				markerInfo.type = markerType;
				markerInfo.vPos = pCurrCheckpoint->vPos;
				markerInfo.vScale = Vec3V(pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale);
				markerInfo.col = pCurrCheckpoint->col2;
				markerInfo.faceCam = true;
				markerInfo.clip = IsZeroAll(pCurrCheckpoint->vClippingPlane)==false;
				markerInfo.vClippingPlane = pCurrCheckpoint->vClippingPlane;

				g_markers.Register(markerInfo);
			}
			// TRANSFORM
			else if (pCurrCheckpoint->type==CHECKPOINTTYPE_TRANSFORM)
			{
				MarkerType_e markerType = MARKERTYPE_QUESTION_MARK;
				float insideScale = CHECKPOINT_INSIDE_RING_SCALE_TRANSFORM;

				// set up and register the marker
				MarkerInfo_t markerInfo;
				markerInfo.type = markerType;
				markerInfo.vPos = pCurrCheckpoint->vPos;
				markerInfo.vScale = Vec3V(pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale);
				markerInfo.col = pCurrCheckpoint->col2;
				markerInfo.faceCam = true;
				markerInfo.clip = IsZeroAll(pCurrCheckpoint->vClippingPlane)==false;
				markerInfo.vClippingPlane = pCurrCheckpoint->vClippingPlane;

				if (pCurrCheckpoint->forceDirection)
				{
					markerInfo.faceCam = false;
					markerInfo.vDir = pCurrCheckpoint->vLookAtPos - pCurrCheckpoint->vPos;
				}

				g_markers.Register(markerInfo);
			}
			// TRANSFORM_PLANE
			else if (pCurrCheckpoint->type==CHECKPOINTTYPE_TRANSFORM_PLANE)
			{
				MarkerType_e markerType = MARKERTYPE_TRANSFORM_PLANE;
				float insideScale = CHECKPOINT_INSIDE_RING_SCALE_TRANSFORM;

				// set up and register the marker
				MarkerInfo_t markerInfo;
				markerInfo.type = markerType;
				markerInfo.vPos = pCurrCheckpoint->vPos;
				markerInfo.vScale = Vec3V(pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale);
				markerInfo.col = pCurrCheckpoint->col2;
				markerInfo.faceCam = true;
				markerInfo.clip = IsZeroAll(pCurrCheckpoint->vClippingPlane)==false;
				markerInfo.vClippingPlane = pCurrCheckpoint->vClippingPlane;

				if (pCurrCheckpoint->forceDirection)
				{
					markerInfo.faceCam = false;
					markerInfo.vDir = pCurrCheckpoint->vLookAtPos - pCurrCheckpoint->vPos;
				}

				g_markers.Register(markerInfo);
			}
			// TRANSFORM_HELICOPTER
			else if (pCurrCheckpoint->type==CHECKPOINTTYPE_TRANSFORM_HELICOPTER)
			{
				MarkerType_e markerType = MARKERTYPE_TRANSFORM_HELICOPTER;
				float insideScale = CHECKPOINT_INSIDE_RING_SCALE_TRANSFORM;

				// set up and register the marker
				MarkerInfo_t markerInfo;
				markerInfo.type = markerType;
				markerInfo.vPos = pCurrCheckpoint->vPos;
				markerInfo.vScale = Vec3V(pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale);
				markerInfo.col = pCurrCheckpoint->col2;
				markerInfo.faceCam = true;
				markerInfo.clip = IsZeroAll(pCurrCheckpoint->vClippingPlane)==false;
				markerInfo.vClippingPlane = pCurrCheckpoint->vClippingPlane;

				if (pCurrCheckpoint->forceDirection)
				{
					markerInfo.faceCam = false;
					markerInfo.vDir = pCurrCheckpoint->vLookAtPos - pCurrCheckpoint->vPos;
				}

				g_markers.Register(markerInfo);
			}
			// TRANSFORM_BOAT
			else if (pCurrCheckpoint->type==CHECKPOINTTYPE_TRANSFORM_BOAT)
			{
				MarkerType_e markerType = MARKERTYPE_TRANSFORM_BOAT;
				float insideScale = CHECKPOINT_INSIDE_RING_SCALE_TRANSFORM;

				// set up and register the marker
				MarkerInfo_t markerInfo;
				markerInfo.type = markerType;
				markerInfo.vPos = pCurrCheckpoint->vPos;
				markerInfo.vScale = Vec3V(pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale);
				markerInfo.col = pCurrCheckpoint->col2;
				markerInfo.faceCam = true;
				markerInfo.clip = IsZeroAll(pCurrCheckpoint->vClippingPlane)==false;
				markerInfo.vClippingPlane = pCurrCheckpoint->vClippingPlane;

				if (pCurrCheckpoint->forceDirection)
				{
					markerInfo.faceCam = false;
					markerInfo.vDir = pCurrCheckpoint->vLookAtPos - pCurrCheckpoint->vPos;
				}

				g_markers.Register(markerInfo);
			}
			// TRANSFORM_CAR
			else if (pCurrCheckpoint->type==CHECKPOINTTYPE_TRANSFORM_CAR)
			{
				MarkerType_e markerType = MARKERTYPE_TRANSFORM_CAR;
				float insideScale = CHECKPOINT_INSIDE_RING_SCALE_TRANSFORM;

				// set up and register the marker
				MarkerInfo_t markerInfo;
				markerInfo.type = markerType;
				markerInfo.vPos = pCurrCheckpoint->vPos;
				markerInfo.vScale = Vec3V(pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale);
				markerInfo.col = pCurrCheckpoint->col2;
				markerInfo.faceCam = true;
				markerInfo.clip = IsZeroAll(pCurrCheckpoint->vClippingPlane)==false;
				markerInfo.vClippingPlane = pCurrCheckpoint->vClippingPlane;

				if (pCurrCheckpoint->forceDirection)
				{
					markerInfo.faceCam = false;
					markerInfo.vDir = pCurrCheckpoint->vLookAtPos - pCurrCheckpoint->vPos;
				}

				g_markers.Register(markerInfo);
			}
			// TRANSFORM_BIKE
			else if (pCurrCheckpoint->type==CHECKPOINTTYPE_TRANSFORM_BIKE)
			{
				MarkerType_e markerType = MARKERTYPE_TRANSFORM_BIKE;
				float insideScale = CHECKPOINT_INSIDE_RING_SCALE_TRANSFORM;

				// set up and register the marker
				MarkerInfo_t markerInfo;
				markerInfo.type = markerType;
				markerInfo.vPos = pCurrCheckpoint->vPos;
				markerInfo.vScale = Vec3V(pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale);
				markerInfo.col = pCurrCheckpoint->col2;
				markerInfo.faceCam = true;
				markerInfo.clip = IsZeroAll(pCurrCheckpoint->vClippingPlane)==false;
				markerInfo.vClippingPlane = pCurrCheckpoint->vClippingPlane;

				if (pCurrCheckpoint->forceDirection)
				{
					markerInfo.faceCam = false;
					markerInfo.vDir = pCurrCheckpoint->vLookAtPos - pCurrCheckpoint->vPos;
				}

				g_markers.Register(markerInfo);
			}
			// TRANSFORM_PUSH_BIKE
			else if (pCurrCheckpoint->type==CHECKPOINTTYPE_TRANSFORM_PUSH_BIKE)
			{
				MarkerType_e markerType = MARKERTYPE_TRANSFORM_PUSH_BIKE;
				float insideScale = CHECKPOINT_INSIDE_RING_SCALE_TRANSFORM;

				// set up and register the marker
				MarkerInfo_t markerInfo;
				markerInfo.type = markerType;
				markerInfo.vPos = pCurrCheckpoint->vPos;
				markerInfo.vScale = Vec3V(pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale);
				markerInfo.col = pCurrCheckpoint->col2;
				markerInfo.faceCam = true;
				markerInfo.clip = IsZeroAll(pCurrCheckpoint->vClippingPlane)==false;
				markerInfo.vClippingPlane = pCurrCheckpoint->vClippingPlane;

				if (pCurrCheckpoint->forceDirection)
				{
					markerInfo.faceCam = false;
					markerInfo.vDir = pCurrCheckpoint->vLookAtPos - pCurrCheckpoint->vPos;
				}

				g_markers.Register(markerInfo);
			}
			// TRANSFORM_TRUCK
			else if (pCurrCheckpoint->type==CHECKPOINTTYPE_TRANSFORM_TRUCK)
			{
				MarkerType_e markerType = MARKERTYPE_TRANSFORM_TRUCK;
				float insideScale = CHECKPOINT_INSIDE_RING_SCALE_TRANSFORM;

				// set up and register the marker
				MarkerInfo_t markerInfo;
				markerInfo.type = markerType;
				markerInfo.vPos = pCurrCheckpoint->vPos;
				markerInfo.vScale = Vec3V(pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale);
				markerInfo.col = pCurrCheckpoint->col2;
				markerInfo.faceCam = true;
				markerInfo.clip = IsZeroAll(pCurrCheckpoint->vClippingPlane)==false;
				markerInfo.vClippingPlane = pCurrCheckpoint->vClippingPlane;

				if (pCurrCheckpoint->forceDirection)
				{
					markerInfo.faceCam = false;
					markerInfo.vDir = pCurrCheckpoint->vLookAtPos - pCurrCheckpoint->vPos;
				}

				g_markers.Register(markerInfo);
			}
			// TRANSFORM_PARACHUTE
			else if (pCurrCheckpoint->type==CHECKPOINTTYPE_TRANSFORM_PARACHUTE)
			{
				MarkerType_e markerType = MARKERTYPE_TRANSFORM_PARACHUTE;
				float insideScale = CHECKPOINT_INSIDE_RING_SCALE_TRANSFORM;

				// set up and register the marker
				MarkerInfo_t markerInfo;
				markerInfo.type = markerType;
				markerInfo.vPos = pCurrCheckpoint->vPos;
				markerInfo.vScale = Vec3V(pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale);
				markerInfo.col = pCurrCheckpoint->col2;
				markerInfo.faceCam = true;
				markerInfo.clip = IsZeroAll(pCurrCheckpoint->vClippingPlane)==false;
				markerInfo.vClippingPlane = pCurrCheckpoint->vClippingPlane;

				if (pCurrCheckpoint->forceDirection)
				{
					markerInfo.faceCam = false;
					markerInfo.vDir = pCurrCheckpoint->vLookAtPos - pCurrCheckpoint->vPos;
				}

				g_markers.Register(markerInfo);
			}
			// CHECKPOINTTYPE_TRANSFORM_THRUSTER
			else if (pCurrCheckpoint->type==CHECKPOINTTYPE_TRANSFORM_THRUSTER)
			{
				MarkerType_e markerType = MARKERTYPE_TRANSFORM_THRUSTER;
				float insideScale = CHECKPOINT_INSIDE_RING_SCALE_TRANSFORM;

				// set up and register the marker
				MarkerInfo_t markerInfo;
				markerInfo.type = markerType;
				markerInfo.vPos = pCurrCheckpoint->vPos;
				markerInfo.vScale = Vec3V(pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale);
				markerInfo.col = pCurrCheckpoint->col2;
				markerInfo.faceCam = true;
				markerInfo.clip = IsZeroAll(pCurrCheckpoint->vClippingPlane)==false;
				markerInfo.vClippingPlane = pCurrCheckpoint->vClippingPlane;

				if (pCurrCheckpoint->forceDirection)
				{
					markerInfo.faceCam = false;
					markerInfo.vDir = pCurrCheckpoint->vLookAtPos - pCurrCheckpoint->vPos;
				}

				g_markers.Register(markerInfo);
			}
			// WARP
			else if (pCurrCheckpoint->type==CHECKPOINTTYPE_WARP)
			{
				MarkerType_e markerType = MARKERTYPE_WARP;
				float insideScale = CHECKPOINT_INSIDE_RING_SCALE_WARP;

				// set up and register the marker
				MarkerInfo_t markerInfo;
				markerInfo.type = markerType;
				markerInfo.vPos = pCurrCheckpoint->vPos;
				markerInfo.vScale = Vec3V(pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale, pCurrCheckpoint->scale*insideScale);
				markerInfo.col = pCurrCheckpoint->col2;
				markerInfo.faceCam = true;
				markerInfo.clip = IsZeroAll(pCurrCheckpoint->vClippingPlane)==false;
				markerInfo.vClippingPlane = pCurrCheckpoint->vClippingPlane;

				g_markers.Register(markerInfo);
			}

			// RING
			if (pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_AIR_CHEVRON_1 || 
				pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_AIR_CHEVRON_2 ||
				pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_AIR_CHEVRON_3 ||
				pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_AIR_LAP ||
				pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_AIR_FLAG || 
				pCurrCheckpoint->type==CHECKPOINTTYPE_PLANE_FLAT || 
				pCurrCheckpoint->type==CHECKPOINTTYPE_PLANE_SIDE_L || 
				pCurrCheckpoint->type==CHECKPOINTTYPE_PLANE_SIDE_R || 
				pCurrCheckpoint->type==CHECKPOINTTYPE_PLANE_INVERTED ||
				pCurrCheckpoint->type==CHECKPOINTTYPE_PARACHUTE_RING ||
				pCurrCheckpoint->type==CHECKPOINTTYPE_MONEY ||
				pCurrCheckpoint->type==CHECKPOINTTYPE_BEAST ||
				pCurrCheckpoint->type==CHECKPOINTTYPE_TRANSFORM || 
				pCurrCheckpoint->type==CHECKPOINTTYPE_TRANSFORM_PLANE || 
				pCurrCheckpoint->type==CHECKPOINTTYPE_TRANSFORM_HELICOPTER || 
				pCurrCheckpoint->type==CHECKPOINTTYPE_TRANSFORM_BOAT || 
				pCurrCheckpoint->type==CHECKPOINTTYPE_TRANSFORM_CAR || 
				pCurrCheckpoint->type==CHECKPOINTTYPE_TRANSFORM_BIKE || 
				pCurrCheckpoint->type==CHECKPOINTTYPE_TRANSFORM_PUSH_BIKE || 
				pCurrCheckpoint->type==CHECKPOINTTYPE_TRANSFORM_TRUCK || 
				pCurrCheckpoint->type==CHECKPOINTTYPE_TRANSFORM_PARACHUTE || 
				pCurrCheckpoint->type==CHECKPOINTTYPE_TRANSFORM_THRUSTER || 
				pCurrCheckpoint->type==CHECKPOINTTYPE_WARP)
			{
				// set up and register the marker
				MarkerInfo_t markerInfo;
				markerInfo.type = MARKERTYPE_RING;
				markerInfo.vPos = pCurrCheckpoint->vPos;
				markerInfo.vScale = Vec3V(pCurrCheckpoint->scale, pCurrCheckpoint->scale, pCurrCheckpoint->scale);
				markerInfo.col = pCurrCheckpoint->col;
				markerInfo.faceCam = true;
				markerInfo.clip = IsZeroAll(pCurrCheckpoint->vClippingPlane)==false;
				markerInfo.vClippingPlane = pCurrCheckpoint->vClippingPlane;

				if (pCurrCheckpoint->type==CHECKPOINTTYPE_PARACHUTE_RING || pCurrCheckpoint->preventRingFacingCam)
				{
					markerInfo.faceCam = false;
				}

				if (pCurrCheckpoint->forceDirection)
				{
					markerInfo.faceCam = false;
					markerInfo.vDir = pCurrCheckpoint->vLookAtPos - pCurrCheckpoint->vPos;
				}

				g_markers.Register(markerInfo);
			}
			// CYLINDER
			else if (pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_CHEVRON_1_BASE || 
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_CHEVRON_2_BASE || 
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_CHEVRON_3_BASE || 
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_LAP_BASE || 
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_FLAG_BASE ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_PIT_LANE_BASE ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_CHEVRON_1 || 
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_CHEVRON_2 || 
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_CHEVRON_3 || 
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_LAP || 
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_FLAG || 
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_PIT_LANE || 
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_RUN_CHEVRON_1 || 
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_RUN_CHEVRON_2 || 
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_RUN_CHEVRON_3 || 
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_RUN_LAP ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_RUN_FLAG ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_SWIM_CHEVRON_1 || 
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_SWIM_CHEVRON_2 || 
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_SWIM_CHEVRON_3 || 
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_SWIM_LAP ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_SWIM_FLAG ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_CYCLE_CHEVRON_1 || 
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_CYCLE_CHEVRON_2 || 
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_CYCLE_CHEVRON_3 || 
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_CYCLE_LAP ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_CYCLE_FLAG ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_GANG_LOCATE_LOST ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_GANG_LOCATE_VAGOS ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_GANG_LOCATE_COPS || 
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_LOCATE_GROUND ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_LOCATE_AIR ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_LOCATE_WATER)
			{
				// calc the z scale depending on the distance from the camera to the checkpoint
				Vec3V_ConstRef vCamPos = RCC_VEC3V(camInterface::GetPos());
				Vec3V vCheckToCamVec = vCamPos-pCurrCheckpoint->vPos;
				vCheckToCamVec.SetZ(ScalarV(V_ZERO));
				float checkToCamDistSqr = MagSquared(vCheckToCamVec).Getf();

				float distSqr = pCurrCheckpoint->cylinderHeightDist*pCurrCheckpoint->cylinderHeightDist;
				float zScale = pCurrCheckpoint->cylinderHeightMax;
				if (checkToCamDistSqr < distSqr)
				{
					zScale = (pCurrCheckpoint->cylinderHeightMax/distSqr) * checkToCamDistSqr;
					zScale = Max(zScale, pCurrCheckpoint->cylinderHeightMin);
				}

				// calc the xy scale - race checkpoints are fixed scale
				float xyScale = pCurrCheckpoint->scale;
				bool isRaceCheckpoint = pCurrCheckpoint->type <= CHECKPOINTTYPE_RACE_LAST;
				if (isRaceCheckpoint)
				{
					xyScale *= CHECKPOINT_CYLINDER_XY_SCALE_RACES;
				}

				// set up and register the marker
				MarkerInfo_t markerInfo;
				markerInfo.type = MARKERTYPE_CYLINDER;
				markerInfo.vPos = pCurrCheckpoint->vPos;
				markerInfo.vScale = Vec3V(xyScale, xyScale, zScale);
				markerInfo.col = pCurrCheckpoint->col;
				markerInfo.clip = IsZeroAll(pCurrCheckpoint->vClippingPlane)==false;
				markerInfo.vClippingPlane = pCurrCheckpoint->vClippingPlane;

				// different alpha for cylinders
				u8 alpha = 0;
				if (pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_CHEVRON_1_BASE || 
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_CHEVRON_2_BASE || 
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_CHEVRON_3_BASE || 
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_LAP_BASE || 
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_FLAG_BASE ||
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_PIT_LANE_BASE ||
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_CHEVRON_1 || 
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_CHEVRON_2 || 
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_CHEVRON_3 || 
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_LAP || 
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_FLAG ||
					pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_PIT_LANE)
				{
					int alphaInt = (int)(markerInfo.col.GetAlpha()*CHECKPOINT_CYLINDER_ALPHA_MULT_GROUND_RACES);
					if (alphaInt>255)
					{
						alphaInt = 255;
					}
					alpha = static_cast<u8>(alphaInt);			
				}
				else
				{
					alpha = static_cast<u8>(markerInfo.col.GetAlpha()*CHECKPOINT_CYLINDER_ALPHA_MULT);	
				}
				markerInfo.col.SetAlpha(alpha);	

				// offset the position z if this cylinder is placed relative to the chevron/flag inside it
				if (pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_CHEVRON_1 ||
						 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_CHEVRON_2 ||
						 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_CHEVRON_3)
				{
					float zOffset = (-g_markers.GetBBMinZ(MARKERTYPE_CHEVRON_1) + CHECKPOINT_CHEVRON_IN_CYLINDER_HEIGHT) * pCurrCheckpoint->scale * pCurrCheckpoint->insideCylinderHeightScale;
					markerInfo.vPos.SetZf(pCurrCheckpoint->vPos.GetZf() - zOffset);
				}
				else if (pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_LAP)
				{
					float zOffset = (-g_markers.GetBBMinZ(MARKERTYPE_LAP) + CHECKPOINT_LAP_IN_CYLINDER_HEIGHT) * pCurrCheckpoint->scale * pCurrCheckpoint->insideCylinderHeightScale;
					markerInfo.vPos.SetZf(pCurrCheckpoint->vPos.GetZf() - zOffset);
				}
				else if (pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_FLAG)
				{
					float zOffset = (-g_markers.GetBBMinZ(MARKERTYPE_FLAG) + CHECKPOINT_FLAG_IN_CYLINDER_HEIGHT) * pCurrCheckpoint->scale * pCurrCheckpoint->insideCylinderHeightScale;
					markerInfo.vPos.SetZf(pCurrCheckpoint->vPos.GetZf() - zOffset);
				}
				else if (pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_GROUND_PIT_LANE)
				{
					float zOffset = (-g_markers.GetBBMinZ(MARKERTYPE_PIT_LANE) + CHECKPOINT_PIT_LANE_IN_CYLINDER_HEIGHT) * pCurrCheckpoint->scale * pCurrCheckpoint->insideCylinderHeightScale;
					markerInfo.vPos.SetZf(pCurrCheckpoint->vPos.GetZf() - zOffset);
				}

				g_markers.Register(markerInfo);
			}

			// DECALS
			bool isLongRange = false;
			s32 decalRenderSettingIndex = -1;
			s32 decalRenderSettingCount = -1;
			if (pCurrCheckpoint->type==CHECKPOINTTYPE_GANG_LOCATE_LOST)
			{
				g_decalMan.FindRenderSettingInfo(DECALID_MARKER_GANG_LOST_WHITE, decalRenderSettingIndex, decalRenderSettingCount);	
			}
			else if (pCurrCheckpoint->type==CHECKPOINTTYPE_GANG_LOCATE_VAGOS)
			{
				g_decalMan.FindRenderSettingInfo(DECALID_MARKER_GANG_VAGOS_WHITE, decalRenderSettingIndex, decalRenderSettingCount);	
			}
			else if (pCurrCheckpoint->type==CHECKPOINTTYPE_GANG_LOCATE_COPS)
			{
				g_decalMan.FindRenderSettingInfo(DECALID_MARKER_GANG_COPS_WHITE, decalRenderSettingIndex, decalRenderSettingCount);	
			}
			else if (pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_LOCATE_GROUND)
			{
				g_decalMan.FindRenderSettingInfo(DECALID_MARKER_RACE_GROUND, decalRenderSettingIndex, decalRenderSettingCount);	
			}
			else if (pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_LOCATE_AIR)
			{
				g_decalMan.FindRenderSettingInfo(DECALID_MARKER_RACE_AIR, decalRenderSettingIndex, decalRenderSettingCount);	
			}
			else if (pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_LOCATE_WATER)
			{
				g_decalMan.FindRenderSettingInfo(DECALID_MARKER_RACE_WATER, decalRenderSettingIndex, decalRenderSettingCount);	
			}
			else if (pCurrCheckpoint->type==CHECKPOINTTYPE_MP_LOCATE_1)
			{
				g_decalMan.FindRenderSettingInfo(DECALID_MARKER_MP_LOCATE_1, decalRenderSettingIndex, decalRenderSettingCount);
				isLongRange = true;	
			}
			else if (pCurrCheckpoint->type==CHECKPOINTTYPE_MP_LOCATE_2)
			{
				g_decalMan.FindRenderSettingInfo(DECALID_MARKER_MP_LOCATE_2, decalRenderSettingIndex, decalRenderSettingCount);	
				isLongRange = true;
			}
			else if (pCurrCheckpoint->type==CHECKPOINTTYPE_MP_LOCATE_3)
			{
				g_decalMan.FindRenderSettingInfo(DECALID_MARKER_MP_LOCATE_3, decalRenderSettingIndex, decalRenderSettingCount);	
				isLongRange = true;
			}
			else if (pCurrCheckpoint->type==CHECKPOINTTYPE_MP_CREATOR_TRIGGER)
			{
				g_decalMan.FindRenderSettingInfo(DECALID_MARKER_MP_CREATOR_TRIGGER, decalRenderSettingIndex, decalRenderSettingCount);	
				isLongRange = true;
			}
			else if (pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_RUN_CHEVRON_1 ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_RUN_CHEVRON_2 ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_RUN_CHEVRON_3 ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_RUN_LAP ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_RUN_FLAG)
			{
				g_decalMan.FindRenderSettingInfo(DECALID_MARKER_TRI_RUN, decalRenderSettingIndex, decalRenderSettingCount);	
			}
			else if (pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_SWIM_CHEVRON_1 ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_SWIM_CHEVRON_2 ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_SWIM_CHEVRON_3 ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_SWIM_LAP ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_SWIM_FLAG)
			{
				g_decalMan.FindRenderSettingInfo(DECALID_MARKER_TRI_SWIM, decalRenderSettingIndex, decalRenderSettingCount);	
			}
			else if (pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_CYCLE_CHEVRON_1 ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_CYCLE_CHEVRON_2 ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_CYCLE_CHEVRON_3 ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_CYCLE_LAP ||
					 pCurrCheckpoint->type==CHECKPOINTTYPE_RACE_TRI_CYCLE_FLAG)
			{
				g_decalMan.FindRenderSettingInfo(DECALID_MARKER_TRI_CYCLE, decalRenderSettingIndex, decalRenderSettingCount);	
			}
			else if (pCurrCheckpoint->type==CHECKPOINTTYPE_HELI_LANDING)
			{
				g_decalMan.FindRenderSettingInfo(DECALID_MARKER_HELI_LANDING, decalRenderSettingIndex, decalRenderSettingCount);	
				isLongRange = true;
			}
			else if (pCurrCheckpoint->type==CHECKPOINTTYPE_PARACHUTE_LANDING)
			{
				g_decalMan.FindRenderSettingInfo(DECALID_MARKER_PARACHUTE_LANDING, decalRenderSettingIndex, decalRenderSettingCount);	
				isLongRange = true;
			}

			if (decalRenderSettingIndex>=0)
			{
// 				Vec3V vStartPos = pCurrCheckpoint->vPos;
// 				Vec3V vEndPos = pCurrCheckpoint->vPos + Vec3V(0.0f, 0.0f, -2.0f);
// 
//				WorldProbe::CShapeTestHitPoint probeResult;
//				WorldProbe::CShapeTestResults probeResults(probeResult);
// 				WorldProbe::CShapeTestProbeDesc probeDesc;
// 				probeDesc.SetStartAndEnd(RCC_VECTOR3(vStartPos), RCC_VECTOR3(vEndPos));
// 				probeDesc.SetResultsStructure(&probeResults);
// 				probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_WEAPON);
// 				if (WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
// 				{
// 					Vec3V_ConstRef vCamPos = RCC_VEC3V(camInterface::GetPos());
// 					Vec3V vCheckToCamVec = vCamPos-pCurrCheckpoint->vPos;
// 					vCheckToCamVec.SetZ(ScalarV(V_ZERO));
// 					vCheckToCamVec = Normalize(vCheckToCamVec);
// 
// 					Vec3V vPos = probeResults[0].GetHitPositionV();
// 					Vec3V vDir = -probeResults[0].GetHitNormalV();
// 					Vec3V vTangent;
// 					CVfxHelper::GetRandomTangentAlign(vTangent, vDir, -vCheckToCamVec);
// 					g_decalMan.RegisterMarker(decalRenderSettingIndex, decalRenderSettingCount, vPos, vDir, vTangent, pCurrCheckpoint->scale, pCurrCheckpoint->scale, pCurrCheckpoint->col, isLongRange, true);
// 				}

				Vec3V vPos = pCurrCheckpoint->vPos;
				Vec3V vDir = -Vec3V(V_Z_AXIS_WZERO);
				Vec3V vTangent;
				if (decalSettingsManager::GetRenderSettings(decalRenderSettingIndex)->shaderId == DECAL_STATIC_SHADER_ROTATE_TOWARDS_CAMERA)
				{
					vTangent = Vec3V(V_X_AXIS_WZERO);
				}
				else
				{
					if (pCurrCheckpoint->decalRotAlignedToCamRot)
					{
						Vec3V vCamDir = RCC_VEC3V(camInterface::GetFront());
						vCamDir.SetZ(ScalarV(V_ZERO));
						if (IsCloseAll(vCamDir, Vec3V(V_ZERO), Vec3V(V_FLT_SMALL_2)))
						{
							// the front vector is almost directly downwards - use the up vector instead 
							vCamDir = RCC_VEC3V(camInterface::GetUp());
							vCamDir.SetZ(ScalarV(V_ZERO));
						}
						vCamDir = NormalizeSafe(vCamDir, Vec3V(V_X_AXIS_WZERO));
						CVfxHelper::GetRandomTangentAlign(vTangent, vDir, vCamDir);
					}
					else
					{
						Vec3V_ConstRef vCamPos = RCC_VEC3V(camInterface::GetPos());
						Vec3V vCheckToCamVec = vCamPos-pCurrCheckpoint->vPos;
						vCheckToCamVec.SetZ(ScalarV(V_ZERO));
						vCheckToCamVec = NormalizeSafe(vCheckToCamVec, Vec3V(V_X_AXIS_WZERO));
						CVfxHelper::GetRandomTangentAlign(vTangent, vDir, -vCheckToCamVec);
					}
				}

				g_decalMan.RegisterMarker(decalRenderSettingIndex, decalRenderSettingCount, vPos, vDir, vTangent, pCurrCheckpoint->scale, pCurrCheckpoint->scale, pCurrCheckpoint->col, isLongRange, false, pCurrCheckpoint->decalRotAlignedToCamRot);
			}
		}
	}
}




///////////////////////////////////////////////////////////////////////////////
//  Add
///////////////////////////////////////////////////////////////////////////////

int				CCheckpoints::Add							(CheckpointType_e type, Vec3V_In vPos, Vec3V_In vDir, float scale, u8 colR, u8 colG, u8 colB, u8 colA, u8 num)
{
	s32 index = 0;
	bool foundFree = false;
	while ((index<CHECKPOINTS_MAX) && !foundFree)
	{
		if (m_checkpoints[index].isActive==false)
		{
			foundFree = true;
		}
		else
		{
			index++;
		}
	}

	if (!foundFree)
	{
		vfxAssertf(0, "CCheckpoints::Add - no free checkpoints");
		return -1;
	}

	m_checkpoints[index].isActive = true;
	m_checkpoints[index].type = type;
	m_checkpoints[index].scale = scale;
	m_checkpoints[index].insideCylinderHeightScale = 1.0f;
	m_checkpoints[index].insideCylinderScale = 1.0f;
	m_checkpoints[index].cylinderHeightMin = CHECKPOINT_CYLINDER_Z_MIN;
	m_checkpoints[index].cylinderHeightMax = CHECKPOINT_CYLINDER_Z_MAX;
	m_checkpoints[index].cylinderHeightDist = CHECKPOINT_CYLINDER_Z_DIST;
	m_checkpoints[index].vPos = vPos;
	m_checkpoints[index].vLookAtPos = vDir;
	m_checkpoints[index].col = Color32(colR, colG, colB, colA);
	m_checkpoints[index].col2 = Color32(colR, colG, colB, colA);
	m_checkpoints[index].num = num;
	m_checkpoints[index].vClippingPlane = Vec4V(V_ZERO);
	m_checkpoints[index].forceOldArrowPointing = false;
	m_checkpoints[index].decalRotAlignedToCamRot = false;
	m_checkpoints[index].preventRingFacingCam = false;
	m_checkpoints[index].forceDirection = false;

#if __BANK
	m_numActiveCheckpoints++;
#endif

	return index;
}


///////////////////////////////////////////////////////////////////////////////
//  Delete
///////////////////////////////////////////////////////////////////////////////

void			CCheckpoints::Delete						(s32 id)
{
	if ((id<0) || (id>=CHECKPOINTS_MAX))
	{
		vfxAssertf(0, "CCheckpoints::Delete - id is out of range");
		return;
	}

	m_checkpoints[id].isActive						= false;
	m_checkpoints[id].scriptRefIndex				= 0;
	m_checkpoints[id].type							= CHECKPOINTTYPE_INVALID;
	m_checkpoints[id].scale							= 0.0f;
	m_checkpoints[id].insideCylinderHeightScale		= 0.0f;
	m_checkpoints[id].insideCylinderScale			= 1.0f;
	m_checkpoints[id].cylinderHeightMin				= 0.0f;
	m_checkpoints[id].cylinderHeightMax				= 0.0f;
	m_checkpoints[id].cylinderHeightDist			= 0.0f;
	m_checkpoints[id].vPos							= Vec3V(V_ZERO);
	m_checkpoints[id].vLookAtPos					= Vec3V(V_ZERO);
	m_checkpoints[id].num							= 0;
	m_checkpoints[id].vClippingPlane				= Vec4V(V_ZERO);
	m_checkpoints[id].forceOldArrowPointing			= false;
	m_checkpoints[id].decalRotAlignedToCamRot		= false;
	m_checkpoints[id].preventRingFacingCam			= false;
	m_checkpoints[id].forceDirection				= false;
	

#if __BANK
	m_numActiveCheckpoints--;
#endif
}

///////////////////////////////////////////////////////////////////////////////
//  SetInsideCylinderHeightScale
///////////////////////////////////////////////////////////////////////////////

void			CCheckpoints::SetInsideCylinderHeightScale	(s32 id, float scale)
{
	if (vfxVerifyf(id>=0 && id<CHECKPOINTS_MAX, "out of range checkpoint id detected"))
	{
		m_checkpoints[id].insideCylinderHeightScale = scale;
	}
}

///////////////////////////////////////////////////////////////////////////////
//  SetInsideCylinderScale
///////////////////////////////////////////////////////////////////////////////

void			CCheckpoints::SetInsideCylinderScale	(s32 id, float scale)
{
	if (vfxVerifyf(id>=0 && id<CHECKPOINTS_MAX, "out of range checkpoint id detected"))
	{
		m_checkpoints[id].insideCylinderScale = scale;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  SetCylinderHeight
///////////////////////////////////////////////////////////////////////////////

void			CCheckpoints::SetCylinderHeight				(s32 id, float cylinderHeightMin, float cylinderHeightMax, float cylinderHeightDist)
{
	if (vfxVerifyf(id>=0 && id<CHECKPOINTS_MAX, "out of range checkpoint id detected"))
	{
		m_checkpoints[id].cylinderHeightMin = cylinderHeightMin;
		m_checkpoints[id].cylinderHeightMax = cylinderHeightMax;
		m_checkpoints[id].cylinderHeightDist = cylinderHeightDist;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  SetRGBA
///////////////////////////////////////////////////////////////////////////////

void			CCheckpoints::SetRGBA						(s32 id, u8 colR, u8 colG, u8 colB, u8 colA)
{
	if (vfxVerifyf(id>=0 && id<CHECKPOINTS_MAX, "out of range checkpoint id detected"))
	{
		m_checkpoints[id].col.Set(colR, colG, colB, colA);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  SetRGBA2
///////////////////////////////////////////////////////////////////////////////

void			CCheckpoints::SetRGBA2						(s32 id, u8 colR, u8 colG, u8 colB, u8 colA)
{
	if (vfxVerifyf(id>=0 && id<CHECKPOINTS_MAX, "out of range checkpoint id detected"))
	{
		m_checkpoints[id].col2.Set(colR, colG, colB, colA);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  SetClipPlane
///////////////////////////////////////////////////////////////////////////////

void			CCheckpoints::SetClipPlane					(s32 id, Vec3V_In vPlanePosition, Vec3V_In vPlaneNormal_In)
{
	if (vfxVerifyf(id>=0 && id<CHECKPOINTS_MAX, "out of range checkpoint id detected"))
	{
		if (IsZeroAll(vPlaneNormal_In))
		{
			m_checkpoints[id].vClippingPlane = Vec4V(V_ZERO);
		}
		else
		{
			Vec3V vPlaneNormal = Normalize(vPlaneNormal_In);
			ScalarV planeDist = Negate(Dot(vPlanePosition, vPlaneNormal));
			m_checkpoints[id].vClippingPlane = Vec4V(vPlaneNormal, planeDist);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  SetClipPlane
///////////////////////////////////////////////////////////////////////////////

void			CCheckpoints::SetClipPlane					(s32 id, Vec4V_In vPlaneEq)
{
	if (vfxVerifyf(id>=0 && id<CHECKPOINTS_MAX, "out of range checkpoint id detected"))
	{
		m_checkpoints[id].vClippingPlane = vPlaneEq;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  GetClipPlane
///////////////////////////////////////////////////////////////////////////////

Vec4V_Out		CCheckpoints::GetClipPlane					(s32 id)
{
	if (vfxVerifyf(id>=0 && id<CHECKPOINTS_MAX, "out of range checkpoint id detected"))
	{
		return m_checkpoints[id].vClippingPlane;
	}

	return Vec4V(V_ZERO);
}


///////////////////////////////////////////////////////////////////////////////
//  SetForceOldArrowPointing
///////////////////////////////////////////////////////////////////////////////

void			CCheckpoints::SetForceOldArrowPointing		(s32 id)
{
	if (vfxVerifyf(id>=0 && id<CHECKPOINTS_MAX, "out of range checkpoint id detected"))
	{
		m_checkpoints[id].forceOldArrowPointing = true;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  SetDecalRotAlignedToCamRot
///////////////////////////////////////////////////////////////////////////////

void			CCheckpoints::SetDecalRotAlignedToCamRot	(s32 id)
{
	if (vfxVerifyf(id>=0 && id<CHECKPOINTS_MAX, "out of range checkpoint id detected"))
	{
		m_checkpoints[id].decalRotAlignedToCamRot = true;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  SetPreventRingFacingCam
///////////////////////////////////////////////////////////////////////////////

void			CCheckpoints::SetPreventRingFacingCam		(s32 id)
{
	if (vfxVerifyf(id>=0 && id<CHECKPOINTS_MAX, "out of range checkpoint id detected"))
	{
		m_checkpoints[id].preventRingFacingCam = true;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  SetForceDirection
///////////////////////////////////////////////////////////////////////////////

void			CCheckpoints::SetForceDirection				(s32 id)
{
	if (vfxVerifyf(id>=0 && id<CHECKPOINTS_MAX, "out of range checkpoint id detected"))
	{
		m_checkpoints[id].forceDirection = true;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  SetDirection
///////////////////////////////////////////////////////////////////////////////

void			CCheckpoints::SetDirection					(s32 id, Vec3V_In vDir)
{
	if (vfxVerifyf(id>=0 && id<CHECKPOINTS_MAX, "out of range checkpoint id detected"))
	{
		m_checkpoints[id].vLookAtPos = vDir;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  RenderDebug
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CCheckpoints::RenderDebug			()
{
	if (!m_debugRender)
	{
		return;
	}

	for (s32 i=0; i<CHECKPOINTS_MAX; i++)
	{
		if (m_checkpoints[i].isActive && (m_debugRenderId==i || m_debugRenderId==-1))
		{
			// render pos sphere
			Color32 col1(255, 0, 0, 128);
			grcDebugDraw::Sphere(RCC_VECTOR3(m_checkpoints[i].vPos), 0.1f, col1);

			// render look at vector
			if (!IsZeroAll(m_checkpoints[i].vLookAtPos))
			{
				Color32 col2(0, 0, 255, 128);
				Vec3V vDir = m_checkpoints[i].vLookAtPos - m_checkpoints[i].vPos;
				vDir = Normalize(vDir);
				
				Vec3V vEndPos = m_checkpoints[i].vPos+vDir;

				grcDebugDraw::Line(RCC_VECTOR3(m_checkpoints[i].vPos), RCC_VECTOR3(vEndPos), col2);
			}

			// render type
			char displayTxt[128];
			sprintf(displayTxt, "type - %s", g_typeNames[m_checkpoints[i].type]);
			grcDebugDraw::Text(m_checkpoints[i].vPos, Color32(255, 255, 0, 255), 0, 10, displayTxt, false);
		}
	}
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  InitWidgets
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CCheckpoints::InitWidgets			()
{
	bkBank* pVfxBank = vfxWidget::GetBank();
	pVfxBank->PushGroup("Checkpoints", false);
	{
		pVfxBank->AddTitle("INFO");
		char txt[128];
		sprintf(txt, "Num Active (%d)", CHECKPOINTTYPE_MAX);
		pVfxBank->AddSlider(txt, &m_numActiveCheckpoints, 0, CHECKPOINTTYPE_MAX, 0);

		pVfxBank->AddTitle("");
		pVfxBank->AddTitle("CHECKPOINT SETTINGS");
		pVfxBank->AddSlider("Inside Ring Scale (Chevron)", &CHECKPOINT_INSIDE_RING_SCALE_CHEVRON, 0.0f, 2.0f, 0.05f, NullCB, "");
		pVfxBank->AddSlider("Inside Ring Scale (Lap)", &CHECKPOINT_INSIDE_RING_SCALE_LAP, 0.0f, 2.0f, 0.05f, NullCB, "");
		pVfxBank->AddSlider("Inside Ring Scale (Flag)", &CHECKPOINT_INSIDE_RING_SCALE_FLAG, 0.0f, 2.0f, 0.05f, NullCB, "");
		pVfxBank->AddSlider("Inside Ring Scale (Plane)", &CHECKPOINT_INSIDE_RING_SCALE_PLANE, 0.0f, 2.0f, 0.05f, NullCB, "");
		pVfxBank->AddSlider("Inside Ring Scale (Money)", &CHECKPOINT_INSIDE_RING_SCALE_MONEY, 0.0f, 2.0f, 0.05f, NullCB, "");
		pVfxBank->AddSlider("Inside Ring Scale (Beast)", &CHECKPOINT_INSIDE_RING_SCALE_BEAST, 0.0f, 2.0f, 0.05f, NullCB, "");
		pVfxBank->AddSlider("Inside Ring Scale (Transform)", &CHECKPOINT_INSIDE_RING_SCALE_TRANSFORM, 0.0f, 2.0f, 0.05f, NullCB, "");
		pVfxBank->AddSlider("Inside Ring Scale (Warp)", &CHECKPOINT_INSIDE_RING_SCALE_WARP, 0.0f, 2.0f, 0.05f, NullCB, "");
		pVfxBank->AddSlider("Inside Cylinder Scale (Chevron)", &CHECKPOINT_INSIDE_CYLINDER_SCALE_CHEVRON, 0.0f, 2.0f, 0.05f, NullCB, "");
		pVfxBank->AddSlider("Inside Cylinder Scale (Lap)", &CHECKPOINT_INSIDE_CYLINDER_SCALE_LAP, 0.0f, 2.0f, 0.05f, NullCB, "");
		pVfxBank->AddSlider("Inside Cylinder Scale (Flag)", &CHECKPOINT_INSIDE_CYLINDER_SCALE_FLAG, 0.0f, 2.0f, 0.05f, NullCB, "");
		pVfxBank->AddSlider("Inside Cylinder Scale (Pit Lane)", &CHECKPOINT_INSIDE_CYLINDER_SCALE_PIT_LANE, 0.0f, 2.0f, 0.05f, NullCB, "");
		pVfxBank->AddSlider("Inside Cylinder Scale (Number)", &CHECKPOINT_INSIDE_CYLINDER_SCALE_NUMBER, 0.0f, 2.0f, 0.05f, NullCB, "");
		pVfxBank->AddSlider("Chevron In Cylinder Height", &CHECKPOINT_CHEVRON_IN_CYLINDER_HEIGHT, 0.0f, 5.0f, 0.05f, NullCB, "");
		pVfxBank->AddSlider("Lap In Cylinder Height", &CHECKPOINT_LAP_IN_CYLINDER_HEIGHT, 0.0f, 5.0f, 0.05f, NullCB, "");
		pVfxBank->AddSlider("Flag In Cylinder Height", &CHECKPOINT_FLAG_IN_CYLINDER_HEIGHT, 0.0f, 5.0f, 0.05f, NullCB, "");
		pVfxBank->AddSlider("Pit Lane In Cylinder Height", &CHECKPOINT_PIT_LANE_IN_CYLINDER_HEIGHT, 0.0f, 5.0f, 0.05f, NullCB, "");
		pVfxBank->AddSlider("Number In Cylinder Height", &CHECKPOINT_NUMBER_IN_CYLINDER_HEIGHT, 0.0f, 5.0f, 0.05f, NullCB, "");
		pVfxBank->AddSlider("Number Side Offset", &CHECKPOINT_NUMBER_SIDE_OFFSET, 0.0f, 5.0f, 0.05f, NullCB, "");
		pVfxBank->AddSlider("Cylinder Height Min", &CHECKPOINT_CYLINDER_Z_MIN, 0.0f, 50.0f, 0.5f, NullCB, "");
		pVfxBank->AddSlider("Cylinder Height Max", &CHECKPOINT_CYLINDER_Z_MAX, 0.0f, 50.0f, 0.5f, NullCB, "");
		pVfxBank->AddSlider("Cylinder Height Dist", &CHECKPOINT_CYLINDER_Z_DIST, 0.0f, 5000.0f, 10.0f, NullCB, "");
		pVfxBank->AddSlider("Cylinder XY Scale Races", &CHECKPOINT_CYLINDER_XY_SCALE_RACES, 0.0f, 5000.0f, 10.0f, NullCB, "");
		pVfxBank->AddSlider("Cylinder Alpha Mult (Ground Races)", &CHECKPOINT_CYLINDER_ALPHA_MULT_GROUND_RACES, 0.0f, 10.0f, 0.01f, NullCB, "");
		pVfxBank->AddSlider("Cylinder Alpha Mult (Generic)", &CHECKPOINT_CYLINDER_ALPHA_MULT, 0.0f, 1.0f, 0.01f, NullCB, "");
		pVfxBank->AddToggle("Use Alternate Chevron Pointing", &CHECKPOINT_USE_ALTERNATE_CHEVRON_POINTING);
		pVfxBank->AddSlider("Alternate Chevron Cam Tilt", &CHECKPOINT_ALTERNATE_CHEVRON_CAM_TILT, -2.0f, 2.0f, 0.01f, NullCB, "");
		
		pVfxBank->AddSlider("Col R", &CHECKPOINT_DEFAULT_COL_R, 0, 255, 1, NullCB, "");
		pVfxBank->AddSlider("Col G", &CHECKPOINT_DEFAULT_COL_G, 0, 255, 1, NullCB, "");
		pVfxBank->AddSlider("Col B", &CHECKPOINT_DEFAULT_COL_B, 0, 255, 1, NullCB, "");
		pVfxBank->AddSlider("Col A", &CHECKPOINT_DEFAULT_COL_A, 0, 255, 1, NullCB, "");

		pVfxBank->AddSlider("Col R2", &CHECKPOINT_DEFAULT_COL_R2, 0, 255, 1, NullCB, "");
		pVfxBank->AddSlider("Col G2", &CHECKPOINT_DEFAULT_COL_G2, 0, 255, 1, NullCB, "");
		pVfxBank->AddSlider("Col B2", &CHECKPOINT_DEFAULT_COL_B2, 0, 255, 1, NullCB, "");
		pVfxBank->AddSlider("Col A2", &CHECKPOINT_DEFAULT_COL_A2, 0, 255, 1, NullCB, "");

		pVfxBank->AddTitle("");
		pVfxBank->AddTitle("DEBUG");
		pVfxBank->AddToggle("Point At Player", &m_pointAtPlayer);
		pVfxBank->AddToggle("Disable Render", &m_disableRender);
		pVfxBank->AddToggle("Debug Render", &m_debugRender);
		pVfxBank->AddSlider("Debug Render Id", &m_debugRenderId, -1, CHECKPOINTS_MAX-1, 1);

		pVfxBank->AddCombo("Debug Checkpoint Type", (s32*)&m_debugCheckpointType, CHECKPOINTTYPE_MAX, g_typeNames);
		pVfxBank->AddSlider("Debug Checkpoint Scale", &m_debugCheckpointScale, 0.0, 10.0, 0.1f);
		pVfxBank->AddSlider("Debug Checkpoint Inside Cylinder Height Scale", &m_debugCheckpointInsideCylinderHeightScale, 0.0, 100.0, 0.1f);
		pVfxBank->AddSlider("Debug Checkpoint Inside Cylinder Scale", &m_debugCheckpointInsideCylinderScale, 0.0, 100.0, 0.1f);
		pVfxBank->AddSlider("Debug Checkpoint Cylinder Height Min", &m_debugCheckpointCylinderHeightMin, 0.0, 100.0, 0.1f);
		pVfxBank->AddSlider("Debug Checkpoint Cylinder Height Max", &m_debugCheckpointCylinderHeightMax, 0.0, 100.0, 0.1f);
		pVfxBank->AddSlider("Debug Checkpoint Cylinder Height Dist", &m_debugCheckpointCylinderHeightDist, 0.0, 100.0, 0.1f);

		pVfxBank->AddSlider("Debug Checkpoint Colour Red", &m_debugCheckpointColR, 0, 255, 1);
		pVfxBank->AddSlider("Debug Checkpoint Colour Green", &m_debugCheckpointColG, 0, 255, 1);
		pVfxBank->AddSlider("Debug Checkpoint Colour Blue", &m_debugCheckpointColB, 0, 255, 1);
		pVfxBank->AddSlider("Debug Checkpoint Colour Alpha", &m_debugCheckpointColA, 0, 255, 1);
		pVfxBank->AddSlider("Debug Checkpoint Colour Red2", &m_debugCheckpointColR2, 0, 255, 1);
		pVfxBank->AddSlider("Debug Checkpoint Colour Green2", &m_debugCheckpointColG2, 0, 255, 1);
		pVfxBank->AddSlider("Debug Checkpoint Colour Blue2", &m_debugCheckpointColB2, 0, 255, 1);
		pVfxBank->AddSlider("Debug Checkpoint Colour Alpha2", &m_debugCheckpointColA2, 0, 255, 1);
		pVfxBank->AddSlider("Debug Checkpoint Number", &m_debugCheckpointNum, 0, 99, 1);

		pVfxBank->AddSlider("Debug Checkpoint Clip Plane Pos X", &m_debugCheckpointPlanePosX, -16000.0f, 16000.0f, 0.01f);
		pVfxBank->AddSlider("Debug Checkpoint Clip Plane Pos Y", &m_debugCheckpointPlanePosY, -16000.0f, 16000.0f, 0.01f);
		pVfxBank->AddSlider("Debug Checkpoint Clip Plane Pos Z", &m_debugCheckpointPlanePosZ, -16000.0f, 16000.0f, 0.01f);
		pVfxBank->AddSlider("Debug Checkpoint Clip Plane Normal X", &m_debugCheckpointPlaneNormalX, -1.0f, 1.0f, 0.01f);
		pVfxBank->AddSlider("Debug Checkpoint Clip Plane Normal Y", &m_debugCheckpointPlaneNormalY, -1.0f, 1.0f, 0.01f);
		pVfxBank->AddSlider("Debug Checkpoint Clip Plane Normal Z", &m_debugCheckpointPlaneNormalZ, -1.0f, 1.0f, 0.01f);

		pVfxBank->AddToggle("Debug Checkpoint Force Direction", &m_debugCheckpointForceDirection);

		pVfxBank->AddButton("Debug Add At Player Pos", DebugAddAtPlayerPos);
		pVfxBank->AddButton("Debug Add At Ground Pos", DebugAddAtGroundPos);
	}
	pVfxBank->PopGroup();

}
#endif


///////////////////////////////////////////////////////////////////////////////
//  DebugAddAtPlayerPos
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CCheckpoints::DebugAddAtPlayerPos		()
{
	Vec3V vPlayerCoords = VECTOR3_TO_VEC3V(CGameWorld::FindLocalPlayerCoors());

	Vec3V vPlayerPos = vPlayerCoords + Vec3V(0.0f, 0.0f, 0.5f);
	Vec3V vLookAtPos = vPlayerPos + Vec3V(10.0f, 0.0f, 0.0f);

	int idx = g_checkpoints.Add(g_checkpoints.m_debugCheckpointType, vPlayerPos, vLookAtPos, g_checkpoints.m_debugCheckpointScale, g_checkpoints.m_debugCheckpointColR, g_checkpoints.m_debugCheckpointColG, g_checkpoints.m_debugCheckpointColB, g_checkpoints.m_debugCheckpointColA, g_checkpoints.m_debugCheckpointNum);
	if (idx>-1)
	{
		g_checkpoints.SetInsideCylinderHeightScale(idx, g_checkpoints.m_debugCheckpointInsideCylinderHeightScale);
		g_checkpoints.SetInsideCylinderScale(idx, g_checkpoints.m_debugCheckpointInsideCylinderScale);
		g_checkpoints.SetCylinderHeight(idx, g_checkpoints.m_debugCheckpointCylinderHeightMin, g_checkpoints.m_debugCheckpointCylinderHeightMax, g_checkpoints.m_debugCheckpointCylinderHeightDist);
		g_checkpoints.SetRGBA2(idx, g_checkpoints.m_debugCheckpointColR2, g_checkpoints.m_debugCheckpointColG2, g_checkpoints.m_debugCheckpointColB2, g_checkpoints.m_debugCheckpointColA2);
		if (g_checkpoints.m_debugCheckpointPlaneNormalX!=0.0f || g_checkpoints.m_debugCheckpointPlaneNormalY!=0.0f || g_checkpoints.m_debugCheckpointPlaneNormalZ!=0.0f)
		{
			Vec3V planeNormal = Normalize(Vec3V(g_checkpoints.m_debugCheckpointPlaneNormalX, g_checkpoints.m_debugCheckpointPlaneNormalY, g_checkpoints.m_debugCheckpointPlaneNormalZ));
			Vec3V planePos = Vec3V(g_checkpoints.m_debugCheckpointPlanePosX, g_checkpoints.m_debugCheckpointPlanePosY, g_checkpoints.m_debugCheckpointPlanePosZ);
			ScalarV planeDist = Negate(Dot(planePos, planeNormal));
			g_checkpoints.SetClipPlane(idx, Vec4V(planeNormal, planeDist));
		}

		if (g_checkpoints.m_debugCheckpointForceDirection)
		{
			g_checkpoints.SetForceDirection(idx);
		}
	}
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  DebugAddAtGroundPos
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CCheckpoints::DebugAddAtGroundPos		()
{
	Vec3V vPlayerCoords = VECTOR3_TO_VEC3V(CGameWorld::FindLocalPlayerCoors());
	Vec3V vLookAtPos = vPlayerCoords + Vec3V(10.0f, 0.0f, 0.0f);

	Vec3V vStartPos = vPlayerCoords;
	Vec3V vEndPos = vPlayerCoords + Vec3V(0.0f, 0.0f, -2.0f);

	WorldProbe::CShapeTestHitPoint probeResult;
	WorldProbe::CShapeTestResults probeResults(probeResult);
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetStartAndEnd(RCC_VECTOR3(vStartPos), RCC_VECTOR3(vEndPos));
	probeDesc.SetResultsStructure(&probeResults);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_WEAPON);
	if (WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
	{
		int idx = g_checkpoints.Add(g_checkpoints.m_debugCheckpointType, probeResults[0].GetHitPositionV(), vLookAtPos, g_checkpoints.m_debugCheckpointScale, g_checkpoints.m_debugCheckpointColR, g_checkpoints.m_debugCheckpointColG, g_checkpoints.m_debugCheckpointColB, g_checkpoints.m_debugCheckpointColA, g_checkpoints.m_debugCheckpointNum);
		if (idx>-1)
		{
			g_checkpoints.SetInsideCylinderHeightScale(idx, g_checkpoints.m_debugCheckpointInsideCylinderHeightScale);
			g_checkpoints.SetInsideCylinderScale(idx, g_checkpoints.m_debugCheckpointInsideCylinderScale);
			g_checkpoints.SetCylinderHeight(idx, g_checkpoints.m_debugCheckpointCylinderHeightMin, g_checkpoints.m_debugCheckpointCylinderHeightMax, g_checkpoints.m_debugCheckpointCylinderHeightDist);
			g_checkpoints.SetRGBA2(idx, g_checkpoints.m_debugCheckpointColR2, g_checkpoints.m_debugCheckpointColG2, g_checkpoints.m_debugCheckpointColB2, g_checkpoints.m_debugCheckpointColA2);
			if (g_checkpoints.m_debugCheckpointPlaneNormalX!=0.0f || g_checkpoints.m_debugCheckpointPlaneNormalY!=0.0f || g_checkpoints.m_debugCheckpointPlaneNormalZ!=0.0f)
			{
				Vec3V planeNormal = Normalize(Vec3V(g_checkpoints.m_debugCheckpointPlaneNormalX, g_checkpoints.m_debugCheckpointPlaneNormalY, g_checkpoints.m_debugCheckpointPlaneNormalZ));
				Vec3V planePos = Vec3V(g_checkpoints.m_debugCheckpointPlanePosX, g_checkpoints.m_debugCheckpointPlanePosY, g_checkpoints.m_debugCheckpointPlanePosZ);
				ScalarV planeDist = Negate(Dot(planePos, planeNormal));
				g_checkpoints.SetClipPlane(idx, Vec4V(planeNormal, planeDist));
			}
		}
	}
}
#endif
