///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	Checkpoints.h
//	BY	: 	Mark Nicholson (Adapted from original by Obbe)
//	FOR	:	Rockstar North
//	ON	:	01 Aug 2008
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

#ifndef CHECKPOINTS_H
#define	CHECKPOINTS_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////													

// rage
#include "vector/color32.h"
#include "vectormath/classes.h"

// game


///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////	


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////													

#define CHECKPOINTS_MAX					(64)


///////////////////////////////////////////////////////////////////////////////
//  ENUMS
///////////////////////////////////////////////////////////////////////////////	

enum CheckpointType_e
{
	CHECKPOINTTYPE_INVALID							= -1,

	// ground races - positioned at base of cylinder
	CHECKPOINTTYPE_RACE_GROUND_CHEVRON_1_BASE		= 0,
	CHECKPOINTTYPE_RACE_GROUND_CHEVRON_2_BASE,
	CHECKPOINTTYPE_RACE_GROUND_CHEVRON_3_BASE,
	CHECKPOINTTYPE_RACE_GROUND_LAP_BASE,
	CHECKPOINTTYPE_RACE_GROUND_FLAG_BASE,
	CHECKPOINTTYPE_RACE_GROUND_PIT_LANE_BASE,

	// ground races - positioned at centre of chevron/flag etc
	CHECKPOINTTYPE_RACE_GROUND_CHEVRON_1,
	CHECKPOINTTYPE_RACE_GROUND_CHEVRON_2,
	CHECKPOINTTYPE_RACE_GROUND_CHEVRON_3,
	CHECKPOINTTYPE_RACE_GROUND_LAP,
	CHECKPOINTTYPE_RACE_GROUND_FLAG,
	CHECKPOINTTYPE_RACE_GROUND_PIT_LANE,

	// air races
	CHECKPOINTTYPE_RACE_AIR_CHEVRON_1,
	CHECKPOINTTYPE_RACE_AIR_CHEVRON_2,
	CHECKPOINTTYPE_RACE_AIR_CHEVRON_3,
	CHECKPOINTTYPE_RACE_AIR_LAP,
	CHECKPOINTTYPE_RACE_AIR_FLAG,

	// water races
	CHECKPOINTTYPE_RACE_WATER_CHEVRON_1,
	CHECKPOINTTYPE_RACE_WATER_CHEVRON_2,
	CHECKPOINTTYPE_RACE_WATER_CHEVRON_3,
	CHECKPOINTTYPE_RACE_WATER_LAP,
	CHECKPOINTTYPE_RACE_WATER_FLAG,

	// triathlon running races
	CHECKPOINTTYPE_RACE_TRI_RUN_CHEVRON_1,
	CHECKPOINTTYPE_RACE_TRI_RUN_CHEVRON_2,
	CHECKPOINTTYPE_RACE_TRI_RUN_CHEVRON_3,
	CHECKPOINTTYPE_RACE_TRI_RUN_LAP,
	CHECKPOINTTYPE_RACE_TRI_RUN_FLAG,

	// triathlon swimming races
	CHECKPOINTTYPE_RACE_TRI_SWIM_CHEVRON_1,
	CHECKPOINTTYPE_RACE_TRI_SWIM_CHEVRON_2,
	CHECKPOINTTYPE_RACE_TRI_SWIM_CHEVRON_3,
	CHECKPOINTTYPE_RACE_TRI_SWIM_LAP,
	CHECKPOINTTYPE_RACE_TRI_SWIM_FLAG,

	// triathlon cycling races
	CHECKPOINTTYPE_RACE_TRI_CYCLE_CHEVRON_1,
	CHECKPOINTTYPE_RACE_TRI_CYCLE_CHEVRON_2,
	CHECKPOINTTYPE_RACE_TRI_CYCLE_CHEVRON_3,
	CHECKPOINTTYPE_RACE_TRI_CYCLE_LAP,
	CHECKPOINTTYPE_RACE_TRI_CYCLE_FLAG,

	CHECKPOINTTYPE_RACE_LAST = CHECKPOINTTYPE_RACE_TRI_CYCLE_FLAG,

	// plane school
	CHECKPOINTTYPE_PLANE_FLAT,
	CHECKPOINTTYPE_PLANE_SIDE_L,
	CHECKPOINTTYPE_PLANE_SIDE_R,
	CHECKPOINTTYPE_PLANE_INVERTED,

	// heli 
	CHECKPOINTTYPE_HELI_LANDING,

	// parachute
	CHECKPOINTTYPE_PARACHUTE_RING,
	CHECKPOINTTYPE_PARACHUTE_LANDING,

	// gang locators
	CHECKPOINTTYPE_GANG_LOCATE_LOST,
	CHECKPOINTTYPE_GANG_LOCATE_VAGOS,
	CHECKPOINTTYPE_GANG_LOCATE_COPS,

	// race locators
	CHECKPOINTTYPE_RACE_LOCATE_GROUND,
	CHECKPOINTTYPE_RACE_LOCATE_AIR,
	CHECKPOINTTYPE_RACE_LOCATE_WATER,

	// multiplayer locators
	CHECKPOINTTYPE_MP_LOCATE_1,
	CHECKPOINTTYPE_MP_LOCATE_2,
	CHECKPOINTTYPE_MP_LOCATE_3,
	CHECKPOINTTYPE_MP_CREATOR_TRIGGER,

	// freemode 
	CHECKPOINTTYPE_MONEY,

	// misc
	CHECKPOINTTYPE_BEAST,

	// transforms
	CHECKPOINTTYPE_TRANSFORM,
	CHECKPOINTTYPE_TRANSFORM_PLANE,
	CHECKPOINTTYPE_TRANSFORM_HELICOPTER,
	CHECKPOINTTYPE_TRANSFORM_BOAT,
	CHECKPOINTTYPE_TRANSFORM_CAR,
	CHECKPOINTTYPE_TRANSFORM_BIKE,
	CHECKPOINTTYPE_TRANSFORM_PUSH_BIKE,
	CHECKPOINTTYPE_TRANSFORM_TRUCK,
	CHECKPOINTTYPE_TRANSFORM_PARACHUTE,
	CHECKPOINTTYPE_TRANSFORM_THRUSTER,

	// warps
	CHECKPOINTTYPE_WARP,

	//
	CHECKPOINTTYPE_MAX
};


///////////////////////////////////////////////////////////////////////////////
//  STRUCTURES
///////////////////////////////////////////////////////////////////////////////

typedef struct Checkpoint_t
{
	Vec3V				vPos;
	Vec3V				vLookAtPos;
	Vec4V				vClippingPlane;
	bool				isActive;
	u16					scriptRefIndex;
	u8					num;
	CheckpointType_e	type;
	float				scale;
	float				insideCylinderHeightScale;
	float				insideCylinderScale;
	float				cylinderHeightMin;
	float				cylinderHeightMax;
	float				cylinderHeightDist;
	Color32				col;
	Color32				col2;
	bool				forceOldArrowPointing;
	bool				decalRotAlignedToCamRot;
	bool				preventRingFacingCam;
	bool				forceDirection;

} Checkpoint_t;


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

class CCheckpoints
{	
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		void 				Init					(unsigned initMode);
		void				Shutdown				(unsigned shutdownMode);

		void				Update					();

		int					Add						(CheckpointType_e type, Vec3V_In vPosition, Vec3V_In vPointDir, float scale, u8 colR, u8 colG, u8 colB, u8 colA, u8 num);
		void 				Delete					(s32 id);

		Checkpoint_t&		GetCheckpoint			(s32 id) {return m_checkpoints[id];}

		void				SetInsideCylinderHeightScale(s32 id, float scale);
		void				SetInsideCylinderScale	(s32 id, float scale);
		void				SetCylinderHeight		(s32 id, float cylinderHeightMin, float cylinderHeightMax, float cylinderHeightDist);
		void				SetRGBA					(s32 id, u8 colR, u8 colG, u8 colB, u8 colA);
		void				SetRGBA2				(s32 id, u8 colR, u8 colG, u8 colB, u8 colA);
		void				SetClipPlane			(s32 id, Vec3V_In vPlanePosition, Vec3V_In vPlaneNormal);
		void				SetClipPlane			(s32 id, Vec4V_In vPlaneEq);
		Vec4V_Out			GetClipPlane			(s32 id);
		void				SetForceOldArrowPointing(s32 id);
		void				SetDecalRotAlignedToCamRot(s32 id);
		void				SetPreventRingFacingCam	(s32 id);
		void				SetForceDirection		(s32 id);
		void				SetDirection			(s32 id, Vec3V_In vPointDir);

#if __BANK
		void				InitWidgets				();
		void 				RenderDebug				();
#endif

	private: //////////////////////////

#if __BANK
static	void				DebugAddAtPlayerPos		();
static	void				DebugAddAtGroundPos		();
#endif

	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		Checkpoint_t 		m_checkpoints			[CHECKPOINTS_MAX];

#if __BANK
		s32					m_numActiveCheckpoints;

		bool				m_pointAtPlayer;
		bool				m_disableRender;
		bool				m_debugRender;
		s32					m_debugRenderId;

		CheckpointType_e	m_debugCheckpointType;
		float				m_debugCheckpointScale;	
		float				m_debugCheckpointInsideCylinderHeightScale;	
		float				m_debugCheckpointInsideCylinderScale;	
		float				m_debugCheckpointCylinderHeightMin;
		float				m_debugCheckpointCylinderHeightMax;
		float				m_debugCheckpointCylinderHeightDist;
		u8					m_debugCheckpointColR;
		u8					m_debugCheckpointColG;
		u8					m_debugCheckpointColB;
		u8					m_debugCheckpointColA;
		u8					m_debugCheckpointColR2;
		u8					m_debugCheckpointColG2;
		u8					m_debugCheckpointColB2;
		u8					m_debugCheckpointColA2;
		float				m_debugCheckpointPlaneNormalX;
		float				m_debugCheckpointPlaneNormalY;
		float				m_debugCheckpointPlaneNormalZ;
		float				m_debugCheckpointPlanePosX;
		float				m_debugCheckpointPlanePosY;
		float				m_debugCheckpointPlanePosZ;

		bool				m_debugCheckpointForceDirection;

		u8					m_debugCheckpointNum;
#endif

}; 



///////////////////////////////////////////////////////////////////////////////
//  EXTERNS
///////////////////////////////////////////////////////////////////////////////

extern	CCheckpoints		g_checkpoints;




#endif // CHECKPOINTS_H





