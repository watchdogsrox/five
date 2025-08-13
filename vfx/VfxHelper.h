///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxHelper.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	12 Jun 2008
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

#ifndef VFX_HELPER_H
#define	VFX_HELPER_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////													

// framework
#include "grcore/im.h"
#include "fwanimation/boneids.h"
#include "fwscene/world/interiorlocation.h"
#include "physics/levelbase.h"

// game
#include "Animation/AnimBones.h"
#include "Physics/gtaMaterial.h"
#include "Physics/gtaMaterialManager.h"
#include "Physics/WaterTestHelper.h"
#include "Renderer/Water.h"
#include "Scene/Portals/PortalTracker.h"
#include "Scene/RegdRefTypes.h"
#if !__SPU
#include "Task/System/TaskHelpers.h"
#endif
#include "Weapons/WeaponEnums.h"
#include "control/replay/Replay.h"


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////													

#define VFX_MAX_ASYNC_PROBES				            (16)

#define	VFXRANGE_DEEP_SURFACE_FOOT			            (20.0f)
#define	VFXRANGE_DEEP_SURFACE_FOOT_SQR		            (VFXRANGE_DEEP_SURFACE_FOOT*VFXRANGE_DEEP_SURFACE_FOOT)

#define	VFXRANGE_DEEP_SURFACE_CLONE_PLAYER_FOOT			(100.0f)
#define	VFXRANGE_DEEP_SURFACE_CLONE_PLAYER_FOOT_SQR		(VFXRANGE_DEEP_SURFACE_CLONE_PLAYER_FOOT*VFXRANGE_DEEP_SURFACE_CLONE_PLAYER_FOOT)

#define	VFXRANGE_DEEP_SURFACE_WHEEL			            (25.0f)
#define	VFXRANGE_DEEP_SURFACE_WHEEL_SQR		            (VFXRANGE_DEEP_SURFACE_WHEEL*VFXRANGE_DEEP_SURFACE_WHEEL)

#define	VFXRANGE_DEEP_SURFACE_MTL			            (15.0f)
#define	VFXRANGE_DEEP_SURFACE_MTL_SQR		            (VFXRANGE_DEEP_SURFACE_MTL*VFXRANGE_DEEP_SURFACE_MTL)

#define VFX_SINCOS_LOOKUP_TOTAL_ENTRIES		            (176)

#define NUM_RECENT_TIME_STEPS							(10)


///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////	

namespace rage
{
	class crSkeleton;
	class fwEntity;
}

class CEntity;
class CPed;
class CSmashObject;

///////////////////////////////////////////////////////////////////////////////
//  ENUMERATIONS
///////////////////////////////////////////////////////////////////////////////
enum VfxInteriorTest_e
{
	ITR_NOT_IN_INTERIOR = 0,
	ITR_KNOWN_INTERIOR,
	ITR_UNKNOWN_INTERIOR,
	ITR_NOT_STATIC_BOUNDS,
};

///////////////////////////////////////////////////////////////////////////////
//  STRUCTURES
///////////////////////////////////////////////////////////////////////////////

// VfxCollInfo_s //////////////////////////////////////////////////////////////

struct VfxCollInfo_s
{
	// core data
	Vec3V				vPositionWld;											// the world space position where the collision occurred
	Vec3V				vNormalWld;												// the world space normal of the collision
	Vec3V				vDirectionWld;											// the world space direction of the collision
	RegdEnt				regdEnt;												// pointer to the entity that has been hit
	phMaterialMgr::Id	materialId;												// the material on the entity where the collision occurred
	s32					roomId;													// the room id for the entity where the collision occurred
	s32					componentId;											// the component on the entity where the collision occurred
	float				force;													// the force of the collision
	eWeaponEffectGroup	weaponGroup;											// the weapon group that caused the collision (if any)
	float				armouredGlassWeaponDamage;								// the damage value from the weapon that caused the collision
	bool				armouredGlassShotByPedInsideVehicle;					// the damage has come from a ped inside the vehicle

	bool				isBloody;												// set if there is a nearby headshot so a bloody window smash texture can be applied
	bool				isWater;												// set if the collision is with water so that the water fx group can override the actual one (splashes)
	bool				isExitFx;												// set if this is from a bullet exiting an bounding box
	bool				noPtFx;													// set if no particle effects are required from this collision 
	bool				noPedDamage;											// set if no pedDamage wounds are required from this collision 
	bool				noDecal;												// set if no decal from this collision 
	bool				isSnowball;
	bool				isFMJAmmo;

}; // VfxCollInfo_s


// VfxExpInfo_s ///////////////////////////////////////////////////////////////

struct VfxExpInfo_s
{
	enum
	{
		EXP_FORCE_DETACH = 1,		// indicate if we want to force all glass pieces to detach
		EXP_REMOVE_DETATCHED = 2,	// whether or not detached smashable polys should be removed instantly
	};
	Vec3V				vPositionWld;											// the world space position where the explosion occurred	
	RegdEnt				regdEnt;												// pointer to the entity that has exploded
	float				radius;													// the radius of the explosion
	float				force;													// the force the of the explosion
	u32					flags;													// Explosion variation flags
}; // VfxExpInfo_s


// VfxAsyncProbeResults_s /////////////////////////////////////////////////////

#if !__SPU
struct VfxAsyncProbeResults_s
{
	bool hasIntersected;
	Vec3V vPos;
	Vec3V vNormal;
	u16 physInstLevelIndex;
	u16 physInstGenerationId;
	u16 componentId;
	phMaterialMgr::Id mtlId;
	float tValue;

}; // VfxAsyncProbeResults_s
#endif



///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

#if !__SPU
class CVfxAsyncProbe
{
	friend class CVfxAsyncProbeManager;


	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		void						Init							();
		void						Shutdown						();
		void						SubmitWorldProbe				(Vec3V_In vStartPos, Vec3V_In vEndPos, u32 includeFlags, u32 stateIncludeFlags);
		//void						SubmitEntityProbe				(Vec3V_In vStartPos, Vec3V_In vEndPos, u32 includeFlags, CEntity* pEntity);


	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		CAsyncProbeHelper			m_probe;


}; // CVfxAsyncProbe
#endif


#if !__SPU
class CVfxAsyncProbeManager
{
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		static void					Init							();
		static void					Shutdown						();

		static s8					SubmitWorldProbe				(Vec3V_In vStartPos, Vec3V_In vEndPos, u32 includeFlags, u32 stateIncludeFlags=phLevelBase::STATE_FLAGS_ALL);
		//static s8					SubmitEntityProbe				(Vec3V_In vStartPos, Vec3V_In vEndPos, u32 includeFlags, CEntity* pEntity);
		static bool					QueryProbe						(s8 probeId, VfxAsyncProbeResults_s& probeResults);
		static void					AbortProbe						(s8 probeId);

		static CEntity*				GetEntity						(VfxAsyncProbeResults_s& probeResults);


	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		static CVfxAsyncProbe		ms_probes						[VFX_MAX_ASYNC_PROBES];


}; // CVfxAsyncProbeManager
#endif


class CVfxDeepSurfaceInfo
{
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		void						Init							();
		void						Process							(Vec3V_In vPos, Vec3V_In vNormal, phMaterialMgr::Id mtlId, float rangeSqr);

		bool						IsActive						()						{return GetDepth()>0.0f;}

		Vec3V_Out					GetPosWld						()						{return m_vPosWld;}
		Vec3V_Out					GetNormalWld					()						{return m_vNormalWldXYZDepthW.GetXYZ();}
		float						GetDepth						() const				{return m_vNormalWldXYZDepthW.GetWf();}


	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		Vec3V						m_vPosWld;												// the world space position of the top surface	
		Vec4V						m_vNormalWldXYZDepthW;											// the world space normal of the top surface

}; // CVfxDeepSurfaceInfo


class CVfxHelper
{	
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		static	void				Init							();

		// camera helpers
		static	float				GetDistSqrToCamera				(Vec3V_In vPos);
		static	ScalarV_Out			GetDistSqrToCameraV				(Vec3V_In vPos);	//Separate function for handling ScalarV Values
		static	float				GetDistSqrToCamera				(Vec3V_In vPosA, Vec3V_In vPosB);
		static	void				UpdateCurrFollowCamViewMode		();
		static	bool				HasCameraSwitched				(bool ignoreWhenLookingBehind, bool ignoreWhenTogglingViewMode);
		static	bool				IsBehindCamera					(Vec3V_In vPos);

		// bone matrix helpers
		static	s32					GetMatrixFromBoneTag			(Mat34V_InOut boneMtx, const CEntity* pEntity, eAnimBoneTag boneTag);
		static	void				GetMatrixFromBoneIndex			(Mat34V_InOut boneMtx, bool* isDamaged, const CEntity* pEntity, s32 boneIndex, bool useAltSkeleton=false);
		static	void				GetMatrixFromBoneIndex_Skinned	(Mat34V_InOut boneMtx, bool* isDamaged, const CEntity* pEntity, s32 boneIndex);
		static	void				GetMatrixFromBoneIndex			(Mat34V_InOut boneMtx, const CEntity* pEntity, s32 boneIndex, bool useAltSkeleton=false) {bool ignored; GetMatrixFromBoneIndex(boneMtx, &ignored, pEntity, boneIndex, useAltSkeleton);}
		static	void				GetMatrixFromBoneIndex_Skinned	(Mat34V_InOut boneMtx, const CEntity* pEntity, s32 boneIndex) {bool ignored; GetMatrixFromBoneIndex_Skinned(boneMtx, &ignored, pEntity, boneIndex);}
		static	void				ApplyVehicleSuspensionToMtx		(Mat34V_InOut vMtx, const fwEntity* pEntity);

		// bone matrix helpers for smashables (so we can get an actual matrix back rather than the zeroed matrix that is calced in the render matrices)
		static	void				GetMatrixFromBoneIndex_Smash	(Mat34V_InOut boneMtx, const CEntity* pEntity, s32 boneIndex);
		static	void				GetDefaultToWorldMatrix			(Mat34V_InOut returnMtx, const CEntity& entity, s32 boneIndex);
		static	void				GetDefaultToBoundMatrix			(Mat34V_InOut returnMtx, const CEntity& entity, s32 componentId);
		static	bool				GetIsBoneBroken					(const CEntity& entity, s32 boneIndex);

		// bone matrix helpers for lights
		static	void				GetMatrixFromBoneTag_Lights		(Mat34V_InOut boneMtx, const CEntity* pEntity, s32 boneTag);
		static	void				GetMatrixFromBoneIndex_Lights	(Mat34V_InOut boneMtx, const CEntity* pEntity, s32 boneIndex);

		// vector helpers
		static	bool				GetRandomTangent				(Vec3V_InOut vTangent, Vec3V_In vNormal);
		static	bool				GetRandomTangentAlign			(Vec3V_InOut vTangent, Vec3V_In vNormal, Vec3V_In vAlign);

		// interpolation helpers
		static	float				GetInterpValue					(float currVal, float startVal, float endVal, bool clamp=true);
		static	float				GetRatioValue					(float ratio, float startVal, float endVal, bool clamp=true);

		// matrix creation helpers
		static	void				CreateMatFromVecY				(Mat34V_InOut returnMtx, Vec3V_In vPos, Vec3V_In vDir);
		static	void				CreateMatFromVecYAlign			(Mat34V_InOut returnMtx, Vec3V_In vPos, Vec3V_In vDir, Vec3V_In vAlign);
		static	void				CreateMatFromVecYAlignUnSafe	(Mat34V_InOut returnMtx, Vec3V_In vPos, Vec3V_In vDir, Vec3V_In vAlign);
		static	void				CreateMatFromVecZ				(Mat34V_InOut returnMtx, Vec3V_In vPos, Vec3V_In vDir);
		static	void				CreateMatFromVecZAlign			(Mat34V_InOut returnMtx, Vec3V_In vPos, Vec3V_In vDir, Vec3V_In vAlign);
		static	void				CreateRelativeMat				(Mat34V_InOut returnMtx, Mat34V_In parentMtx, Mat34V_In wldMtx);

		// vector helper
		static	Vec3V_Out 			GetLocalEntityPos				(const CEntity& entity, Vec3V_In vWldPos);
		static	Vec3V_Out 			GetLocalEntityDir				(const CEntity& entity, Vec3V_In vWldDir);
		static	bool 				GetLocalEntityBonePos			(const CEntity& entity, s32 boneIndex, Vec3V_In vWldPos, Vec3V_InOut vLclPos);
		static	bool 				GetLocalEntityBoneDir			(const CEntity& entity, s32 boneIndex, Vec3V_In vWldDir, Vec3V_InOut vLclDir);
		static	bool				GetLocalEntityBonePosDir		(const CEntity& entity, s32 boneIndex, Vec3V_In vWldPos, Vec3V_In vWldDir, Vec3V_InOut vLclPos, Vec3V_InOut vLclDir);

		// interior helpers
		static	void				UpdateCamInteriorLocation			() {ms_camInteriorLocation = CPortalVisTracker::GetInteriorLocation();}
		static	bool				IsCamInInterior						() {return ms_camInteriorLocation.IsValid();}
		static	bool				IsInInterior						(const CEntity* pEntity);
		static	fwInteriorLocation	GetCamInteriorLocation				() {return ms_camInteriorLocation;}
		static	VfxInteriorTest_e	GetInteriorLocationFromStaticBounds	(const CEntity* pIntersectionEntity, s32 interstctionRoomIdx, fwInteriorLocation& interiorLocationOut);
		static	bool				IsEntityInCurrentVehicle			(CEntity* pEntity);
		static	u64					GetHashFromInteriorLocation			(fwInteriorLocation interiorLocation);

		// underwater helpers
		static	bool				IsNearWater						(Vec3V_In vPos);
		static	bool				IsUnderwater					(Vec3V_In vPos, bool bUseCameraWaterZ = false, ScalarV_In vDifferenceThreshold = ScalarV(V_ZERO));
		static	WaterTestResultType	GetWaterDepth					(Vec3V_In vPos, float& waterDepth, fwEntity* pEntity=NULL);
		static	WaterTestResultType	GetWaterZ						(Vec3V_In vPos, float& waterZ, fwEntity* pEntity=NULL);
		static	bool				GetOceanWaterZ					(Vec3V_In vPos, float& oceanWaterZ);
		static	bool				GetOceanWaterZAndSpeedUp		(Vec3V_In vPos, float& oceanWaterZ, float& oceanWaterSpeedUp);
		static	bool				GetRiverWaterZ					(Vec3V_In vPos, float& riverWaterZ);
		static	WaterTestResultType	TestLineAgainstWater			(Vec3V_In vStartPos, Vec3V_In vEndPos, Vec3V_InOut vIntersectionPos);	
		static	bool				TestLineAgainstOceanWater		(Vec3V_In vStartPos, Vec3V_In vEndPos, Vec3V_InOut vOceanWaterPos);	
		static	bool				TestLineAgainstRiverWater		(Vec3V_In vStartPos, Vec3V_In vEndPos, Vec3V_InOut vRiverWaterPos);	

		// ped helpers
		static	phMaterialMgr::Id	GetPedFootMtlId					(CPed* pPed);

		// locator helpers
		static	bool				ShouldRenderInGameUI			();

		// access functions
		static	void				SetGameCamPos					(Vec3V_In vPos)					{ms_vGameCamPos = vPos;}
		static	Vec3V_Out			GetGameCamPos					()								{return ms_vGameCamPos;}
		static	void				SetGameCamForward				(Vec3V_In vPos)					{ms_vGameCamForward = vPos;}
		static	Vec3V_Out			GetGameCamForward				()								{return ms_vGameCamForward;}
		static	void				SetGameCamFOV					(float fov)						{ms_gameCamFOV = fov; ms_vGameCamFOV = ScalarVFromF32(fov);}
		static  ScalarV_Out			GetGameCamFOVScale				()								{return Min(ms_vGameCamFOV * ms_vDefaultFovRecip, ScalarV(V_ONE));}
		static	float				GetGameCamWaterDepth			()								{return Water::GetCameraWaterDepth();}
		static	void				SetLastTimeStep					(float timeStep);
		static	float				GetSmoothedTimeStep				()								{return ms_smoothedTimeStep;}

		//there are cases when camera is not underwater but height of camera is below the water height
		//return a really low number in that case
		//Reason for doing this is that we think that we're underwater by only doing a check against the camera height.
		//By adding the extra check for the IsCameraUnderwater we can fix the above case
		static	float				GetGameCamWaterZ				()								{return (Water::IsCameraUnderwater() || IsGreaterThanOrEqualAll(GetGameCamPos().GetZ(), ScalarVFromF32(Water::GetCameraWaterHeight())))? Water::GetCameraWaterHeight():-99999999999999999.0f;}
#if !__SPU
		static	Vec4V_Out			GetWaterFogParams				();
		static	grcRenderTarget*	GetWaterFogTexture				()								{return Water::GetWaterFogTexture();}
#endif // !__SPU

		static	void				SetPrevTimeStep					(float timeStep)				{ms_prevTimeStep = timeStep;}
		static	float				GetPrevTimeStep					()								{return ms_prevTimeStep;}

		static	void				SetShootoutBoat					(CVehicle* pVehicle)			{ms_pShootoutBoat = pVehicle;}
		static	CVehicle*			GetShootoutBoat					()								{return ms_pShootoutBoat;}
		static	void				SetUsingCinematicCam			(bool val)						{ms_usingCinematicCam = val;}
		static	void				SetCamInVehicleFlag				(bool val)						{ms_camInsideVehicle = val;}
		static	bool				GetCamInVehicleFlag				()								
		{
#if GTA_REPLAY
			if(CReplayMgr::IsEditModeActive())
			{
				return CReplayMgr::GetCamInVehicleFlag();
			}
			else
#endif	
			{
				return ms_camInsideVehicle;
			}
		}

		static  void				SetRenderLastVehicle			(CVehicle* pVehicle)			{ms_pRenderLastVehicle = pVehicle;}
		static	CVehicle*			GetRenderLastVehicle			()								{return ms_pRenderLastVehicle;}
		static	void				SetDontRenderInGameUI			(bool val)						{ms_dontRenderInGameUI = val;}
		static	void				SetForceRenderInGameUI			(bool val)						{ms_forceRenderInGameUI = val;}

		static	bool				IsPlayerInAccurateModeWithScope	();

		//Sin Cos table lookup
		__forceinline static const Vec2V&			GetSinCosValue					(int totalEntries, int index)
		{
			int lerpVal = VFX_SINCOS_LOOKUP_TOTAL_ENTRIES / totalEntries;
			int remappedIndex = index * lerpVal;
			return ms_arrSinCos[remappedIndex];
		}

		//Draw Triangle Fan Helper
		__forceinline static void DrawTriangleFan(const u32 numVerts, const float size, const float uvscale)
		{
			grcBegin(drawTriFan, numVerts);

			//add Center Vert
			grcTexCoord2f(0.5f, 0.5f);
			grcVertex3f(0.0f, 0.0f, 0.0f);
			const u32 numVertsOnCircumference = numVerts - 2;
			const u32 numVertsIterations = numVerts - 1;
			//add numVerts - 1 on circumference
			for(u32 i = 0; i < numVertsIterations; i++)
			{
				//Doing the mod to get the first vertex again on the last iteration
				const Vec2V& vSinCos = GetSinCosValue(numVertsOnCircumference, i % numVertsOnCircumference);
				const float sinAngle = vSinCos.GetXf();
				const float cosAngle = vSinCos.GetYf();
				grcTexCoord2f((cosAngle * uvscale) + 0.5f, (sinAngle * uvscale) + 0.5f);
				grcVertex3f(cosAngle * size, 0.0f, sinAngle * size);
			}

			grcEnd();
		}

		static  void				SetApplySuspensionOnReplay(bool b) { ms_applySuspensionOnReplay = b; }

	private: //////////////////////////

		static	void				GetSkinnedLocalBoneMtx			(Mat34V_InOut skinnedMtx, Mat34V_In objectMtx, u32 boneIndex, const crSkeletonData& skeletonDat);
		static	void				GetGlobalMtx					(const CEntity* pEntity, const crSkeleton* pSkeleton, u32 boneIdx, Mat34V_InOut outMtx, bool useAltSkeleton=false);
		

	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	public: ///////////////////////////


	private: //////////////////////////

		static	Vec3V				ms_vGameCamPos;
		static	Vec3V				ms_vGameCamForward;
		static  ScalarV				ms_vDefaultFovRecip;
		static	float				ms_gameCamFOV;
		static	ScalarV				ms_vGameCamFOV;			// added this to keep a ScalarV copy of ms_gameCamFOV to avoid LHS. Updated when ms_gameCamFOV is updated
		static	s32					ms_prevFollowCamViewMode;
		static	s32					ms_currFollowCamViewMode;
		static	float				ms_prevTimeStep;

		static	RegdVeh				ms_pShootoutBoat;
		static	RegdVeh				ms_pRenderLastVehicle;
		static	bool				ms_usingCinematicCam;
		static	bool				ms_camInsideVehicle;
		static	bool				ms_dontRenderInGameUI;
		static	bool				ms_forceRenderInGameUI;

		static	fwInteriorLocation	ms_camInteriorLocation;

		static	float				ms_recentTimeSteps[NUM_RECENT_TIME_STEPS];
		static	float				ms_smoothedTimeStep;

		// Lookup table for sin/cos values
		static atArray<Vec2V>		ms_arrSinCos;		

		static bool					ms_applySuspensionOnReplay;


}; // CVfxHelper



///////////////////////////////////////////////////////////////////////////////
//  EXTERNS
///////////////////////////////////////////////////////////////////////////////



#endif // VFX_HELPER_H





