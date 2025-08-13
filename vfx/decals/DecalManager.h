///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	DecalManager.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	12 May 2010
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

#ifndef DECAL_MANAGER_H
#define	DECAL_MANAGER_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

// rage
#include "atl/array.h"
#include "rline/clan/rlclancommon.h"
#include "vector/color32.h"

// framework
#include "vectormath/classes.h"
#include "vfx/decal/decalasynctaskdescpool.h"
#include "vfx/decal/decalcallbacks.h"
#include "vfx/decal/decalmanager.h"
#include "vfx/decal/decalsettings.h"
#include "vfx/decal/decalsettingsmanager.h"
#include "vfx/systems/VfxLiquid.h"

// game
#include "Control/Replay/ReplaySettings.h"
#include "Network/General/NetworkSerialisers.h"
#include "Scene/RegdRefTypes.h"
#include "Vfx/Decals/DecalDebug.h"


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

#define DECAL_MAX_RENDER_SETTINGS_FILES			(4)

// hard wired decal ids (see decals.dat)
#define DECALID_SNOWBALL						(1010)

#define DECALID_MARKER_GANG_LOST_HOLLOW			(9100)
#define DECALID_MARKER_GANG_LOST_GREY			(9101)
#define DECALID_MARKER_GANG_LOST_WHITE			(9102)
#define DECALID_MARKER_GANG_COPS_WHITE			(9103)
#define DECALID_MARKER_GANG_VAGOS_WHITE			(9104)

#define DECALID_MARKER_RACE_GROUND				(9106)
#define DECALID_MARKER_RACE_AIR					(9107)
#define DECALID_MARKER_RACE_WATER				(9108)

#define DECALID_MARKER_TRI_CYCLE				(9110)
#define DECALID_MARKER_TRI_RUN					(9111)
#define DECALID_MARKER_TRI_SWIM					(9112)

#define DECALID_MARKER_HELI_LANDING				(9115)
#define DECALID_MARKER_PARACHUTE_LANDING		(9116)

#define DECALID_MARKER_MP_LOCATE_1				(9120)
#define DECALID_MARKER_MP_LOCATE_2				(9121)
#define DECALID_MARKER_MP_LOCATE_3				(9122)
#define DECALID_MARKER_MP_CREATOR_TRIGGER		(9123)

//#define DECALID_LASER_SIGHT						(8000)
#define DECALID_TEST_WRAP						(3010)//9998)
#define DECALID_TEST_ROUND						(1010)//9999)

#define DECALID_SCARAB_WHEEL_TRACKS				(14510)
#define DECALID_MINITANK_WHEEL_TRACKS			(14511)

#define DECALID_VEHICLE_BADGE_FIRST				(10000)
#define	DECALID_VEHICLE_BADGE_NUM				(32)

#define	MAX_VEHICLE_BADGE_REQUESTS				(8)

#define DECAL_RENDER_PASS_VEHICLE_BADGE_LOCAL_PLAYER		(DECAL_RENDER_PASS_EXTRA_1) // Player vehicle.
#define DECAL_RENDER_PASS_VEHICLE_BADGE_NOT_LOCAL_PLAYER	(DECAL_RENDER_PASS_EXTRA_2) // Non-player vehicles.
#define DECAL_RENDER_PASS_MISC								(DECAL_RENDER_PASS_DEFERRED) // Blood, markers etc (anything other than the above).


///////////////////////////////////////////////////////////////////////////////
//  ENUMERATIONS
///////////////////////////////////////////////////////////////////////////////

enum DecalTrailPiece_e
{
	DECALTRAIL_IN								= 0,
	DECALTRAIL_MID,
	DECALTRAIL_OUT
};

enum DecalRequestState_e
{
	DECALREQUESTSTATE_NOT_ACTIVE				= 0,
	DECALREQUESTSTATE_REQUESTING_BADGE,
	DECALREQUESTSTATE_APPLYING_DECAL,
	DECALREQUESTSTATE_SUCCEEDED,
	DECALREQUESTSTATE_FAILED,
	DECALREQUESTSTATE_FAILED_CANT_GET_CLAN_TEXTURE,
	DECALREQUESTSTATE_FAILED_CANT_ADD_DATA_SLOT,
	DECALREQUESTSTATE_FAILED_VEHICLE_NO_LONGER_VALID,
	DECALREQUESTSTATE_FAILED_VEHICLE_BONE_NOT_VALID,
	DECALREQUESTSTATE_FAILED_VEHICLE_BONE_IS_ZERO,
	DECALREQUESTSTATE_FAILED_VEHICLE_PROBE_FAILED,
	DECALREQUESTSTATE_FAILED_DECAL_DIDNT_APPLY
};


///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////

enum   VfxLiquidType_e;

struct CDecalAsyncTaskDesc;
struct VfxBloodInfo_s;
struct VfxCollInfo_s;
struct VfxMaterialInfo_s;
struct VfxWeaponInfo_s;

class  CEntity;
class  CSmashGroup;
class  CVehicle;


///////////////////////////////////////////////////////////////////////////////
//  STRUCTURES
///////////////////////////////////////////////////////////////////////////////

struct scriptedTrailData_s
{
	bool m_isActive;
	DecalType_e m_decalType;
	VfxLiquidType_e m_liquidType;
	Color32 m_colour;
	s32 m_renderSettingsIndex;
	float m_width;
	float m_currLength;
	float m_prevAlphaMult;
	Vec3V m_prevPos;
	Vec3V m_joinVerts[4];
};


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

//  CVehicleBadgeDesc  ////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

class CVehicleBadgeDesc
{
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		void Init();
		void Set(EmblemDescriptor& emblemDesc);
		void Set(atHashWithStringNotFinal txdHashName, atHashWithStringNotFinal texHashName);
#if GTA_REPLAY
		void Set(EmblemDescriptor& emblemDesc, atHashWithStringNotFinal txdHashName, atHashWithStringNotFinal texHashName);
#endif

		bool Request() const;
		grcTexture* GetTexture();
		void Release() const;

		// helpers
		void AddTextureRef();
		void RemoveTextureRef();

		// accessors
		const EmblemDescriptor& GetEmblemDesc() const {return m_emblemDesc;}
		atHashWithStringNotFinal GetTxdHashName() const {return m_txdHashName;}	
		atHashWithStringNotFinal GetTexHashName() const {return m_texHashName;}		

		// queries
		bool IsUsingEmblem() const {return m_emblemDesc.IsValid();}
		bool IsFree() const;
		bool IsValid() const;
		bool IsEqual(const CVehicleBadgeDesc& other) const;

		// network
		void Serialise(CSyncDataBase& serialiser);


	private: //////////////////////////

		// helpers
		bool RequestTxd() const;
		bool HasTxdLoaded() const;


	///////////////////////////////////
	// VARIABLES 
	///////////////////////////////////	

	private: //////////////////////////

		EmblemDescriptor m_emblemDesc;
		atHashWithStringNotFinal m_txdHashName;
		atHashWithStringNotFinal m_texHashName;
		grcTexture* m_pTexture;

};


//  CVehicleBadgeRequestData  /////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

class CVehicleBadgeRequestData
{
	///////////////////////////////////
	// VARIABLES 
	///////////////////////////////////	

	public: ///////////////////////////

		DecalRequestState_e m_currState;
		CVehicleBadgeDesc m_badgeDesc;
		RegdConstVeh m_regdVeh;
		s32 m_boneIndex;
		Vec3V m_vOffsetPos;
		Vec3V m_vDir;
		Vec3V m_vSide;
		float m_size;
		s32 m_decalId;
		s32 m_numFramesSinceRequest;
		bool m_removeOnceComplete;
		bool m_isForLocalPlayer;
		u32 m_badgeIndex;
		u8 m_alpha;

};


//  CVehicleBadgeData  ////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

class CVehicleBadgeData
{
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		void Init();

		void AddRef();
		void RemoveRef(s32 decalId);

		// accessors
		void SetVehicleBadgeDesc(CVehicleBadgeDesc& desc) {m_badgeDesc = desc;}
		CVehicleBadgeDesc& GetVehicleBadgeDesc() {return m_badgeDesc;}
		s32 GetRefCount() const {return m_refCount;}


	///////////////////////////////////
	// VARIABLES 
	///////////////////////////////////	

	private: //////////////////////////

		CVehicleBadgeDesc m_badgeDesc;
		s32 m_refCount;

};


//  CVehicleBadgeRequestMgr  //////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

class CVehicleBadgeManager
{	
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		void Init();
		void Update();
		void Shutdown();

		bool Request(const CVehicle* pVehicle, const CVehicleBadgeDesc& badgeDesc, s32 boneIndex, Vec3V_In vOffsetPos, Vec3V_In vDir, Vec3V_In vSide, float size, bool isForLocalPlayer, u32 badgeIndex, u8 alpha);
		s32 AddData(CVehicleBadgeDesc& badgeDesc);
		void Remove(const CVehicle* pVehicle, const u32 badgeIndex);
		bool Abort(const CVehicle* pVehicle);

		DecalRequestState_e GetRequestState(const CVehicle* pVehicle, const u32 badgeIndex);

#if __BANK
		void RemoveCompletedRequests();
#endif
		

	///////////////////////////////////
	// VARIABLES 
	///////////////////////////////////	

	private: //////////////////////////

		void CleanUpAfterFailure(CVehicleBadgeRequestData* pRequest);

		atFixedArray<CVehicleBadgeRequestData, MAX_VEHICLE_BADGE_REQUESTS> m_requests;
		atFixedArray<CVehicleBadgeData, DECALID_VEHICLE_BADGE_NUM> m_vehicleBadgeData;

};


//  CDecalManager  ////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

class CDecalManager
{	
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		// main interface
		void					Init						(unsigned initMode);
		void					InitDefinitions				();
#if RSG_PC
		static void				ResetShaders				();
#endif
		void					Shutdown    				(unsigned shutdownMode);
		void					UpdateCompleteAsync			();
		void					UpdateStartAsync			(float deltaTime);
		void					Render						(decalRenderPass firstRenderPass, decalRenderPass lastRenderPass, bool bIsForwardPass, bool bResetRenderCtxIndex, bool bDynamicOnly);

		decalAsyncTaskDescBase* AllocAsyncTaskDesk          ();
		void                    FreeAsyncTaskDesc           (decalAsyncTaskDescBase* desc);

		int						AddBang						(const VfxMaterialInfo_s& vfxMaterialInfo, const VfxCollInfo_s& vfxCollInfo);
		int						AddBloodSplat				(const VfxBloodInfo_s& vfxBloodInfo, const VfxCollInfo_s& vfxCollInfo, float width, bool doSprayDecal, bool doMistDecal);
		int						AddFootprint				(int decalRenderSettingIndex, int decalRenderSettingCount, float width, float length, Color32 col, phMaterialMgr::Id mtlId, Vec3V_In vPos, Vec3V_In vNormal, Vec3V_In vSide, bool isLeftFoot, float life);
		int						AddGeneric					(DecalType_e decalType, s32 renderSettingIndex, s32 renderSettingCount, Vec3V_In vPos, Vec3V_In vDir, Vec3V_In vSide, float width, float height, float depth, float totalLife, float fadeInTime, float uvMultStart, float uvMultEnd, float uvMultTime, Color32 col, bool flipU, bool flipV, float duplicateRejectDist, bool isScripted, bool isDynamic REPLAY_ONLY(, float startLife));
		int						AddPool						(VfxLiquidType_e liquidType, s32 renderSettingIndex, s32 renderSettingCount, Vec3V_In vPosition, Vec3V_In vNormal, float startSize, float endSize, float growRate, u8 colnMtlId, Color32 col, bool isScripted);
		int						AddScuff					(CEntity* pEntity, s32 renderSettingsIndex, Vec3V_In vPtA, Vec3V_InOut vPtB, Vec3V_In vNormal, float width, Color32 col, float life, u8 mtlId, float maxOverlayRadius, float duplicateRejectDist, bool REPLAY_ONLY(disableFadeInForReplay));		
		int						AddTrail					(DecalType_e decalType, VfxLiquidType_e liquidType, s32 renderSettingsIndex, Vec3V_In vPtA, Vec3V_InOut vPtB, Vec3V_In vNormal, float width, Color32 col, u8 alphaBack, float life, u8 mtlId, float& currLength, Vec3V_Ptr pvJoinVerts, bool isScripted, DecalTrailPiece_e trailPiece=DECALTRAIL_MID);
		int						AddVehicleBadge				(const CVehicle* pVehicle, s32 componentId, s32 renderSettingIndex, s32 renderSettingCount, Vec3V_In vPos, Vec3V_In vDir, Vec3V_In vSide, float size, bool isForLocalPlayer, u32 badgeIndex = 0, u8 alpha = 200);
		int						AddWeaponImpact				(const VfxWeaponInfo_s& vfxWeaponInfo, const VfxCollInfo_s& vfxCollInfo, void* pSmashGroup=NULL, CEntity* pSmashEntity=NULL, s32 smashMatrixId=0, bool applyBulletproofGlassTextureSwap=false, bool lookupCentreOfVehicleFromBone=false);

		//int						RegisterLaserSight			(Vec3V_In vPos, Vec3V_In vDir);
		int						RegisterMarker				(s32 renderSettingIndex, s32 renderSettingCount, Vec3V_In vPos, Vec3V_In vDir, Vec3V_In vSide, float width, float height, Color32 col, bool isLongRange, bool isDynamic, bool alignToCamRot);

		s32						GetRenderSettingsIndex		(u32 renderSettingIndex, u32 renderSettingCount);
		
		// utility functions wrapper
		bool					IsDecalPending				(int decalId) const {return DECALMGR.IsDecalPending(decalId);}

		void 					Wash						(float amount, const fwEntity* pEntity, bool dueToRain) {DECALMGR.Wash(amount, pEntity, dueToRain);}
		void 					WashInRange					(float amount, Vec3V_In pos, ScalarV_In range, const fwEntity* pEntity) {DECALMGR.WashInRange(amount, pos, range, pEntity);}

		float					GetWashLevel				(s32 decalId) {return DECALMGR.GetWashLevel(decalId);}
		bool					IsAlive						(s32 decalId) {return DECALMGR.IsAlive(decalId);}
		void					Remove						(s32 decalId) {DECALMGR.Remove(decalId);}
		void 					Remove						(const fwEntity* pEntity, const int boneIndex=-1, const void* pSmashGroup=NULL, u32 exceptionTypeFlags=0);
		void 					RemoveFacing				(Vec3V_In vDir, const fwEntity* pEntity, const void* pSmashGroup=NULL) {DECALMGR.RemoveFacing(vDir, pEntity, pSmashGroup);}
		int 					RemoveInRange				(Vec3V_In vPos, ScalarV_In range, const fwEntity* pEntity, const void* pSmashGroup=NULL) {return DECALMGR.RemoveInRange(vPos, range, pEntity, pSmashGroup);}
		void					FadeInRange					(Vec3V_In vPos, ScalarV_In range, float fadeTime, const fwEntity* pEntity, const void* pSmashGroup=NULL) {DECALMGR.FadeInRange(vPos, range, fadeTime, pEntity, pSmashGroup);}

		void					UpdateAttachEntity			(const fwEntity* pEntityFrom, fwEntity* pEntityTo) {DECALMGR.UpdateAttachEntity(pEntityFrom, pEntityTo);}

		// Update vehicle broken glass decals with mesh changes
		bool					VehicleGlassStateChanged	(const fwEntity* pAttachEntity, void* pSmashGroup) { return DECALMGR.VehicleGlassStateChanged(pAttachEntity, pSmashGroup); }

		bool					IsPointInLiquid				(u32 decalTypeFlags, s8& liquidType, Vec3V_In vPos, float minDecalSize, bool removeDecal, bool fadeDecal, float fadeTime, u32& foundDecalTypeFlag, float distThresh=0.01f) {return DECALMGR.IsPointInLiquid(decalTypeFlags, liquidType, vPos, minDecalSize, removeDecal, fadeDecal, fadeTime, foundDecalTypeFlag, distThresh);}
		bool					IsPointWithin				(u32 decalTypeFlags, s8 liquidType, Vec3V_In vPos, float minDecalSize, bool removeDecal, bool fadeDecal, float fadeTime, u32& foundDecalTypeFlag, float distThresh=0.01f) {return DECALMGR.IsPointWithin(decalTypeFlags, liquidType, vPos, minDecalSize, removeDecal, fadeDecal, fadeTime, foundDecalTypeFlag, distThresh);}
		bool					FindClosest					(u32 decalTypeFlags, s8 liquidType, Vec3V_In vPos, float range, float minDecalSize, bool removeDecal, bool fadeDecal, float fadeTime, Vec3V_InOut vClosestPosWld, fwEntity** ppClosestEntity, u32& foundDecalTypeFlag) {return DECALMGR.FindClosest(decalTypeFlags, liquidType, vPos, range, minDecalSize, removeDecal, fadeDecal, fadeTime, vClosestPosWld, ppClosestEntity, foundDecalTypeFlag);}
		bool					FindFarthest				(u32 decalTypeFlags, s8 liquidType, Vec3V_In vPos, float range, float minDecalSize, bool removeDecal, bool fadeDecal, float fadeTime, Vec3V_InOut vFarthestPosWld, fwEntity** ppFarthestEntity, u32& foundDecalTypeFlag) {return DECALMGR.FindFarthest(decalTypeFlags, liquidType, vPos, range, minDecalSize, removeDecal, fadeDecal, fadeTime, vFarthestPosWld, ppFarthestEntity, foundDecalTypeFlag);}
		decalBucket*			FindFirstBucket				(u32 decalTypeFlags, const fwEntity* pEntity) {return DECALMGR.FindFirstBucket(decalTypeFlags, pEntity);}

		bool					FindRenderSettingInfo		(u32 renderSettingId, s32& renderSettingIndex, s32& renderSettingCount);

#if GTA_REPLAY
		bool					RequestReplayVehicleBadge	(const CVehicle* pVehicle, const CVehicleBadgeDesc& badgeDesc, s32 boneIndex, Vec3V_In vOffsetPos, Vec3V_In vDir, Vec3V_In vSide, float size, bool isForLocalPlayer, u32 badgeIndex, u8 alpha);
#endif // GTA_REPLAY

		// script
		bool					ProcessVehicleBadge			(const CVehicle* pVehicle, const CVehicleBadgeDesc& badgeDesc, s32 boneIndex, Vec3V_In vOffsetPos, Vec3V_In vDir, Vec3V_In vSide, float size, bool isForLocalPlayer, u32 badgeIndex, u8 alpha);
		void					RemoveAllVehicleBadges		(const CVehicle* pVehicle);
		void					RemoveVehicleBadge			(const CVehicle* pVehicle, const u32 badgeIndex);
		DecalRequestState_e		GetVehicleBadgeRequestState	(const CVehicle* pVehicle, const u32 badgeIndex);
		bool					DoesVehicleHaveBadge		(const CVehicle* pVehicle, u32 badgeIndex);
		bool					AbortVehicleBadgeRequests	(const CVehicle* pVehicle);
#if __BANK
		void					RemoveCompletedVehicleBadgeRequests();
#endif

		void					StartScriptedTrail			(DecalType_e decalType, VfxLiquidType_e liquidType, s32 renderSettingsIndex, float width, Color32 col);
		void					AddScriptedTrailInfo		(Vec3V_In vPos, float alphaMult, bool isScripted);
		void					EndScriptedTrail			();

		void					PatchDiffuseMap				(u32 renderSettingId, grcTexture* pTexture) {decalSettingsManager::PatchDiffuseMap(renderSettingId, pTexture);}

		void					SetDisableFootprints		(bool val, scrThreadId scriptThreadId);
		void					SetDisableCompositeShotgunImpacts(bool val, scrThreadId scriptThreadId);
		void					SetDisableScuffs			(bool val, scrThreadId scriptThreadId);
		void					RemoveScript				(scrThreadId scriptThreadId);

		// flags
		void					SetDisablePetrolDecalsIgniting(bool val) {m_disablePetrolDecalsIgniting = val;}
		void					SetDisablePetrolDecalsIgnitingThisFrame() {m_disablePetrolDecalsIgnitingThisFrame = true;}
		bool					GetDisablePetrolDecalsIgniting() {return m_disablePetrolDecalsIgniting || m_disablePetrolDecalsIgnitingThisFrame;}

		void					SetDisablePetrolDecalsRecyclingThisFrame() {m_disablePetrolDecalsRecyclingThisFrame = true;}
		bool					GetDisablePetrolDecalsRecycling() {return m_disablePetrolDecalsRecyclingThisFrame;}

		void					SetDisableRenderingThisFrame() {m_disableRenderingThisFrameUT = true;}

		void					ApplyWheelTrailHackForArenaMode(bool enable);

		// debug functions
#if __BANK
		void					InitWidgets					();
		void					RenderDebug					();
		CDecalDebug&			GetDebugInterface			()					{return m_decalDebug;}
		void					ReloadSettings				();
#endif

#if GTA_REPLAY
		int						AddSettingsReplay			(CEntity* pEntity, decalUserSettings& settings);
		void ResetReplayDecalEvents();
		int ProcessMergedDecals(int decalId);
		float ProcessUVMultChanges(int decalId, Vec3V vPos);
#endif // GTA_REPLAY
		
#if APPLY_DOF_TO_ALPHA_DECALS
		grcBlendStateHandle		GetStaticForwardBlendState() {return m_StaticForwardBlendState;}
#endif //APPLY_DOF_TO_ALPHA_DECALS

		static	grcBlendStateHandle	OverrideStaticBlendStateFunctor(decalRenderPass pass);		

		static void DecalMerged(int oldId, int newId);
		static void LiquidUVMultChanged(int decalId, float uvMult, Vec3V_In pos);

	private: //////////////////////////

		void					InitSettings				();
		int						AddSettings					(CEntity* pEntity, decalUserSettings& settings);
		void					SetJoinVerts				(decalPipelineSettings& pipelineSettings, Vec3V_Ptr pvJoinVerts);
		void					GetJoinVerts				(decalPipelineSettings& pipelineSettings, Vec3V_Ptr pvJoinVerts);



	///////////////////////////////////
	// VARIABLES 
	///////////////////////////////////	

	private: //////////////////////////

		decalAsyncTaskDescPool<CDecalAsyncTaskDesc> m_asyncTaskDescPool;

		int						m_numNonPlayerVehicleBangsScuffsThisFrame;

		// flags
		bool					m_disablePetrolDecalsIgniting;
		bool					m_disablePetrolDecalsIgnitingThisFrame;
		bool					m_disablePetrolDecalsRecyclingThisFrame;
		bool					m_disableRenderingThisFrameUT;  // update thread copy
		bool					m_disableRenderingThisFrameRT;  // render thread copy

		bool                    m_hasRunCompleteAsync;

		// script
		scriptedTrailData_s		m_scriptedTrailData;

		scrThreadId				m_disableFootprintsScriptThreadId;
		bool					m_disableFootprintsFromScript;

		scrThreadId				m_disableCompositeShotgunImpactsScriptThreadId;
		bool					m_disableCompositeShotgunImpactsFromScript;

		scrThreadId				m_disableScuffsScriptThreadId;
		bool					m_disableScuffsFromScript;

		// vehicle badges
		CVehicleBadgeManager	m_vehicleBadgeMgr;

		// debug variables
#if __BANK
		CDecalDebug				m_decalDebug;
#endif

#if APPLY_DOF_TO_ALPHA_DECALS
		grcBlendStateHandle		m_StaticForwardBlendState;
#endif //APPLY_DOF_TO_ALPHA_DECALS

#if GTA_REPLAY
		#define MAX_REPLAY_MERGED_DECAL_EVENTS		64
		#define MAX_REPLAY_UV_MULT_CHANGES			32

		struct replayMergedDecalEvent
		{
			s32 oldDecalId;
			s32 newDecalId;
		};
		struct replayUVMultChanges
		{
			s32 decalId;
			float uvMult;
			Vec3V pos;
		};

		atFixedArray< replayMergedDecalEvent, MAX_REPLAY_MERGED_DECAL_EVENTS>  m_ReplayMergedDecalEvents; 
		atFixedArray< replayUVMultChanges, MAX_REPLAY_UV_MULT_CHANGES>  m_ReplayUVMultChanges; 
#endif //GTA_REPLAY
}; 



///////////////////////////////////////////////////////////////////////////////
//	EXTERNS
///////////////////////////////////////////////////////////////////////////////

extern CDecalManager g_decalMan;

extern dev_u8 DECAL_VEHICLE_BADGE_TINT_R;
extern dev_u8 DECAL_VEHICLE_BADGE_TINT_G;
extern dev_u8 DECAL_VEHICLE_BADGE_TINT_B;

extern dev_float DECAL_BLOOD_SPLAT_DRD_WIDTH_MULT;


#endif // DECAL_MANAGER_H

