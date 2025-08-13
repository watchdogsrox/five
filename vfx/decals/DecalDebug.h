///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	DecalDebug.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	13 May 2010
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

#ifndef DECAL_DEBUG_H
#define	DECAL_DEBUG_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////													

// rage

// framework
#include "vfx/decal/decalsettings.h"

// game
#include "Vfx/Decals/DecalCallbacks.h"


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////													

#define DECAL_SHOW_BROKEN_FRAG_MTX_PROBLEM				(0 && __BANK)
#if DECAL_SHOW_BROKEN_FRAG_MTX_PROBLEM
#define DECAL_SHOW_BROKEN_FRAG_MTX_PROBLEM_ONLY(x)		x
#else
#define DECAL_SHOW_BROKEN_FRAG_MTX_PROBLEM_ONLY(x)
#endif


#if __BANK


///////////////////////////////////////////////////////////////////////////////
//  ENUMERATIONS
///////////////////////////////////////////////////////////////////////////////													


///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////

namespace rage
{
	class fwEntity;
}


///////////////////////////////////////////////////////////////////////////////
//  TYPEDEFS
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

//  CDecalDebug  //////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

class CDecalDebug
{	
	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		// main functions
		void					Init						();	
		void					InitWidgets					();

		void					Update						();	

		// access functions
		bool					GetDisableDecalType				(DecalType_e type)	{return m_disableDecalType[type];}
		bool					GetDisableDuplicateRejectDistances()				{return m_disableDuplicateRejectDistances;}
		bool					GetDisableOverlays				()					{return m_disableOverlays;}
		bool					GetAlwaysUseDrawableIfAvailable	()					{return m_alwaysUseDrawableIfAvailable;}
		bool					GetForceVehicleLod				()					{return m_forceVehicleLod;}
		bool					GetDisableVehicleShaderTest		()					{return m_disableVehicleShaderTest;}
		bool					GetDisableVehicleBoneTest		()					{return m_disableVehicleBoneTest;}
		bool					GetUseTestDiffuseMap			()					{return m_useTestDiffuseMap;}
		bool					GetUseTestDiffuseMap2			()					{return m_useTestDiffuseMap2;}
		bool					GetOverrideDiffuseMap			()					{return m_overrideDiffuseMap;}
		s32						GetOverrideDiffuseMapId			()					{return m_overrideDiffuseMapId;}
		float					GetSizeMult						()					{return m_sizeMult;}
		bool					GetVehicleBadgeProbeDebug		()					{return ms_vehicleBadgeProbeDebug;}
		bool					GetVehicleBadgeSimulateCloudIssues()				{return ms_vehicleBadgeSimulateCloudIssues;}
		bool					IsWaterBodyCheckEnabled			()					{return m_enableUnderwaterCheckWaterBody;}
		ScalarV_Out				GetWaterBodyCheckHeightDiffMin	()					{return m_underwaterCheckWaterBodyHeightDiffMin;}

		int						GetMaxNonPlayerVehicleBangsScuffsPerFrame()			{return ms_maxNonPlayerVehicleBangsScuffsPerFrame;}

		static void				WashAroundPlayer			();
		static void				WashPlayerVehicle			();

		static void				RemoveAroundPlayer			();
		static void				RemoveFromPlayerVehicle		();

		static void				PlaceUnderPlayer			();
		static void				RemoveFromUnderPlayer		();

		static void				PlaceInfrontOfCamera		();
		static void				PlaceBloodSplatInfrontOfCamera();
		static void				PlaceBloodPoolInfrontOfCamera();
		static void				PlaceBangInfrontOfCamera	();
		static void				PlacePlayerVehicleBadge		();
		static void				RemovePlayerVehicleBadge	();
		static void				AbortPlayerVehicleBadge		();
		static void				RemoveThenPlacePlayerVehicleBadge();
		static void				RemoveCompletedVehicleBadgeRequests();

		static void				PlaceTestSkidmark			();
		static void				PlaceTestPetrolTrail		();

		static void				PatchDiffuseMap				();
		static void				UnPatchDiffuseMap			();

		static void				StoreEntityA				();
		static void				StoreEntityB				();
		static void				MoveFromAToB				();
		static void				MoveFromBToA				();

		static void				ReloadSettings				();

		static void				BadgeIndexCallback			();
		static void				BadgeSliderCallback			();

#if DECAL_SHOW_BROKEN_FRAG_MTX_PROBLEM
		static void				OutputPlayerVehicleWindscreenMatrix();
#endif

	private: //////////////////////////


	///////////////////////////////////
	// VARIABLES 
	///////////////////////////////////	

	private: //////////////////////////

		bool					m_disableDecalType[DECALTYPE_NUM];
		bool					m_disableDuplicateRejectDistances;
		bool					m_disableOverlays;
		bool					m_alwaysUseDrawableIfAvailable;
		bool					m_forceVehicleLod;
		bool					m_disableVehicleShaderTest;
		bool					m_disableVehicleBoneTest;
		bool					m_useTestDiffuseMap;
		bool					m_useTestDiffuseMap2;
		bool					m_overrideDiffuseMap;
		s32						m_overrideDiffuseMapId;
		//bool					m_emitPlayerLaserSight;
		bool					m_emitPlayerMarker;
		float					m_playerMarkerSize;
		float					m_playerMarkerAlpha;
		bool					m_playerMarkerPulse;
		float					m_sizeMult;
		bool					m_reloadSettings;
		ScalarV					m_underwaterCheckWaterBodyHeightDiffMin;
		bool					m_enableUnderwaterCheckWaterBody;

		static s32				ms_patchDiffuseMapId;
		static char				ms_patchDiffuseMapName[64];
		static int				ms_decalUnderPlayerId;

		static int				ms_maxNonPlayerVehicleBangsScuffsPerFrame;

		static int				ms_numberOfBadges;
		static int				ms_vehicleBadgeIndex;
		//data used by the debug Widget
		static int				ms_vehicleBadgeBoneIndex;
		static float			ms_vehicleBadgeOffsetX;
		static float			ms_vehicleBadgeOffsetY;
		static float			ms_vehicleBadgeOffsetZ;
		static float			ms_vehicleBadgeDirX;
		static float			ms_vehicleBadgeDirY;
		static float			ms_vehicleBadgeDirZ;
		static float			ms_vehicleBadgeSideX;
		static float			ms_vehicleBadgeSideY;
		static float			ms_vehicleBadgeSideZ;
		static float			ms_vehicleBadgeSize;
		static u8				ms_vehicleBadgeAlpha;
		//data sent to the vehicle badge manager / decal manager
		static int				ms_vehicleBadgeBoneIndices[DECAL_NUM_VEHICLE_BADGES];
		static Vec3V			ms_vehicleBadgeOffsets[DECAL_NUM_VEHICLE_BADGES];		
		static Vec3V			ms_vehicleBadgeDirs[DECAL_NUM_VEHICLE_BADGES];		
		static Vec3V			ms_vehicleBadgeSides[DECAL_NUM_VEHICLE_BADGES];		
		static float			ms_vehicleBadgeSizes[DECAL_NUM_VEHICLE_BADGES];	
		static u8				ms_vehicleBadgeAlphas[DECAL_NUM_VEHICLE_BADGES];
		static char				ms_vehicleBadgeTxdName[64];
		static char				ms_vehicleBadgeTextureName[64];

		bool					ms_vehicleBadgeProbeDebug;
		bool					ms_vehicleBadgeSimulateCloudIssues;

		static fwEntity*		ms_pEntityA;
		static fwEntity*		ms_pEntityB;


}; 

#endif // __BANK
#endif // DECAL_DEBUG_H


