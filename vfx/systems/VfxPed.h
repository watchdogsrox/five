///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxPed.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	01 June 2006
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

#ifndef VFX_PED_H
#define	VFX_PED_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////													

// rage
#include "script/thread.h"

// game
#include "Animation/AnimBones.h"
#include "Physics/GtaMaterial.h"
#include "Physics/GtaMaterialManager.h"
#include "Vfx/Systems/VfxMaterial.h"
#include "Vfx/Metadata/VfxPedInfo.h"


///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////	

namespace rage
{
	class crSkeleton;
	class grcTexture;
}

class CObject;
class CPed;


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////													

#define VFXPED_MAX_VFX_SKELETON_BONES		(20)


///////////////////////////////////////////////////////////////////////////////
//  ENUMS
///////////////////////////////////////////////////////////////////////////////	

// ped fx offsets for registered systems that play on the same ped
enum PtFxPedOffsets_e
{
	PTFXOFFSET_PED_WATER_BUBBLES				= 0,
	PTFXOFFSET_PED_WATER_WADE,
	PTFXOFFSET_PED_WATER_WADE_LAST				= PTFXOFFSET_PED_WATER_WADE + VFXPED_MAX_VFX_SKELETON_BONES,
	PTFXOFFSET_PED_WATER_TRAIL,
	PTFXOFFSET_PED_WATER_TRAIL_LAST				= PTFXOFFSET_PED_WATER_TRAIL + VFXPED_MAX_VFX_SKELETON_BONES,
	PTFXOFFSET_PED_WATER_DRIP,
	PTFXOFFSET_PED_WATER_DRIP_LAST				= PTFXOFFSET_PED_WATER_DRIP + VFXPED_MAX_VFX_SKELETON_BONES,
	PTFXOFFSET_PED_SURFACE_WADE,
	PTFXOFFSET_PED_SURFACE_WADE_LAST			= PTFXOFFSET_PED_SURFACE_WADE + VFXPEDLIMBID_NUM,
	PTFXOFFSET_PED_BLOOD_MIST,
	PTFXOFFSET_PED_BLOOD_DRIPS,
	PTFXOFFSET_PED_MELEE_TAKEDOWN,
	PTFXOFFSET_PED_UNDERWATER_DISTURB,

	// gadgets
	PTFXOFFSET_PED_PARACHUTE_SMOKE,
	PTFXOFFSET_PED_PARACHUTE_PED_TRAIL,
	PTFXOFFSET_PED_PARACHUTE_PED_TRAIL_LAST		= PTFXOFFSET_PED_PARACHUTE_PED_TRAIL + VFXPED_MAX_VFX_SKELETON_BONES,
	PTFXOFFSET_PED_PARACHUTE_CANOPY_TRAIL,
	PTFXOFFSET_PED_PARACHUTE_CANOPY_TRAIL_2,
	PTFXOFFSET_PED_JETPACK_THRUST,
	PTFXOFFSET_PED_JETPACK_THRUST_2,
	PTFXOFFSET_PED_JETPACK_THRUST_3,
	PTFXOFFSET_PED_JETPACK_THRUST_4,

};


///////////////////////////////////////////////////////////////////////////////
//  STRUCTURES
///////////////////////////////////////////////////////////////////////////////	

struct VfxPedLimbInfo_s
{
	Vec3V						vContactPos;											// the contact positions of any intersecting limb
	float						depth;													// the deepest depth of each limb
	bool						isIntersecting;											// whether each limb intersects the top surface
	s32							vfxSkeletonBoneIdx;										// the index into the vfx skeleton of any intersecting limb
};

struct VfxMeleeTakedownInfo_s
{
	atHashWithStringNotFinal ptFxHashName;
	atHashWithStringNotFinal dmgPackHashName;
	CEntity* pAttachEntity;
	u32		attachBoneTag;
	Vec3V	vOffsetPos;
	QuatV	vOffsetRot;
	float	scale;
};


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

class CVfxPed
{	
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		void				Init							(unsigned initMode);
		void				Shutdown						(unsigned shutdownMode);

		void				RemoveScript					(scrThreadId scriptThreadId);

		// settings
		void				SetFootVfxLodRangeScale			(float val, scrThreadId scriptThreadId);
		float				GetFootVfxLodRangeScale			()		{return m_footVfxLodRangeScale;}

		void				SetFootPtFxOverrideHashName		(atHashWithStringNotFinal hashName, scrThreadId scriptThreadId);

		void				SetUseSnowFootVfxWhenUnsheltered(bool enabled)	{m_useSnowFootVfxWhenUnsheltered = enabled;}
		bool				GetUseSnowFootVfxWhenUnsheltered()				{ return m_useSnowFootVfxWhenUnsheltered;}

		// 
		void		 		ProcessVfxPedFootStep			(CPed* pPed, phMaterialMgr::Id pedFootMtlId, VfxGroup_e decalVfxGroup, VfxGroup_e ptFxVfxGroup, Mat34V_In footMtx, const u32 foot, float decalLife, float decalAlpha, bool isLeft, bool isHind);
		void				ProcessVfxPedSurfaceWade		(CPed* pPed, VfxGroup_e vfxGroup);
		void				ProcessVfxPedParachutePedTrails	(CPed* pPed);
		void				ProcessVfxPedParachuteCanopyTrails(CPed* pPed, CObject* pParachuteObj, s32 canopyBoneIndexL, s32 canopyBoneIndexR);

		// particle systems
		void		 		TriggerPtFxPedFootStep			(CPed* pPed, const VfxPedFootPtFxInfo* pVfxPedFootPtFxInfo, Mat34V_In vFootStepMtx, const u32 foot, float depthEvo);
		void		 		TriggerPtFxPedBreath			(CPed* pPed, s32 boneIndex);
//		void		 		TriggerPtFxPedExhaleSmoke		(CPed* pPed, s32 boneIndex);

		void				UpdatePtFxPedWetness			(CPed* pPed, int vfxSkeletonBoneIdx, float outOfWaterTimer);
		void		 		UpdatePtFxPedBreathWater		(CPed* pPed, s32 boneIndex);
		void		 		UpdatePtFxSurfaceWade			(CPed* pPed, const VfxPedWadeInfo* pVfxPedWadeInfo, const VfxPedLimbInfo_s& limbInfo, VfxPedLimbId_e limbId, Vec3V_In vDir, Vec3V_In vNormal, float depthEvo, float speedEvo, float sizeEvo);
		void		 		UpdatePtFxSurfaceWadeDist		(CPed* pPed, VfxPedLimbId_e limbId, float distSqrToCam);

		void				TriggerPtFxShot					(CPed* pPed, Vec3V_In vPositionWld, Vec3V_In vNormalWld);

		void				TriggerPtFxParachuteDeploy		(CPed* pPed);
		void				UpdatePtFxParachuteSmoke		(CPed* pPed, Color32 colour);
		void				UpdatePtFxParachutePedTrail		(CPed* pPed, const VfxPedParachutePedTrailInfo* pVfxPedParachutePedTrailInfo, int id);
		void				UpdatePtFxParachuteCanopyTrail	(CPed* pPed, const CVfxPedInfo* pVfxPedInfo, CObject* pParachuteObj, s32 canopyBoneIndex, int id);

		void				TriggerPtFxMeleeTakedown		(const VfxMeleeTakedownInfo_s* pMeleeTakedownInfo);
		void				UpdatePtFxMeleeTakedown			(const VfxMeleeTakedownInfo_s* pMeleeTakedownInfo);

		void				UpdatePtFxPedUnderwaterDisturb	(CPed* pPed, Vec3V_In vPos, Vec3V_In vNormal, VfxDisturbanceType_e vfxDisturbanceType);

		// debug functions
#if __BANK
		void				InitWidgets						();
		void		 		RenderDebug						();
		bool				GetDisableWadeTextures			()							{return m_disableWadeTextures;}
#endif


	private: //////////////////////////

		//
		bool		 		GetBonePlaneData				(const CPed& ped, Vec4V_In vPlane, eAnimBoneTag boneTagA, eAnimBoneTag boneTagB, Vec3V_InOut vContactPos, float* pBoneDepth);

		// debug functions
#if __BANK
static	void				Reset							();
		bool				GetDebugFootPrints				()							{return m_renderDebugFootPrints;}
#endif


	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		// settings
		scrThreadId			m_footVfxLodRangeScaleScriptThreadId;
		float				m_footVfxLodRangeScale;
		scrThreadId			m_footPtFxOverrideHashNameScriptThreadId;
		atHashWithStringNotFinal m_footPtFxOverrideHashName;
		bool				m_useSnowFootVfxWhenUnsheltered;

		// debug variables
#if __BANK
		bool				m_bankInitialised;

		// vfx skeleton
		bool				m_renderDebugVfxSkeleton;
		float				m_renderDebugVfxSkeletonBoneScale;
		int					m_renderDebugVfxSkeletonBoneIdx;
		bool				m_renderDebugVfxSkeletonBoneWadeInfo;
		bool				m_renderDebugVfxSkeletonBoneWaterInfo;

		// footprints
		bool				m_renderDebugFootPrints;
		
		// wades
		bool				m_renderDebugWades;
		bool				m_disableWadeLegLeft;
		bool				m_disableWadeLegRight;
		bool				m_disableWadeArmLeft;
		bool				m_disableWadeArmRight;
		bool				m_disableWadeSpine;
		bool				m_disableWadeTrails;
		bool				m_disableWadeTextures;
		bool				m_disableWadeParticles;
		bool				m_useZUpWadeTexNormal;
#endif

}; // CVfxPed


///////////////////////////////////////////////////////////////////////////////
//  EXTERNS
///////////////////////////////////////////////////////////////////////////////

extern	CVfxPed				g_vfxPed;

extern	dev_float			VFXPED_BREATH_RATE_BASE;
extern	dev_float			VFXPED_BREATH_RATE_RUN;
extern	dev_float			VFXPED_BREATH_RATE_SPRINT;

extern	dev_float			VFXPED_BREATH_RATE_SPEED_SLOW_DOWN;
extern	dev_float			VFXPED_BREATH_RATE_SPEED_RUN;
extern	dev_float			VFXPED_BREATH_RATE_SPEED_SPRINT;


#endif // VFX_PED_H


///////////////////////////////////////////////////////////////////////////////
