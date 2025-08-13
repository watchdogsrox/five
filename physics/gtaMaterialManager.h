///////////////////////////////////////////////////////////////////////////////
//  
//	FILE:	GtaMaterialManager.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	04 October 2006
//
///////////////////////////////////////////////////////////////////////////////

#ifndef GTA_MATERIAL_MANAGER_H
#define GTA_MATERIAL_MANAGER_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

// rage
#include "parser/macros.h"
#include "phCore/materialmgr.h"

// game
#include "GtaMaterial.h"

#if !__SPU

	// rage

	// game
#	include "GtaMaterialDebug.h"

#endif


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

#define PGTAMATERIALMGR		((phMaterialMgrGta*)&MATERIALMGR)


///////////////////////////////////////////////////////////////////////////////
//  TYPEDEFS
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

#if !__SPU
class phRumbleProfile
{
public:
	phRumbleProfile() : m_nextTriggerTime(0), m_minVelocity(0), m_maxVelocity(0), m_minVelocityFrequency(0) {}

	u32 m_nextTriggerTime;

	// serialized variables
	atHashString m_profileName;

	float m_triggerProbability;
	float m_minVelocity;
	float m_maxVelocity;
	u32 m_triggerInterval;

	u16 m_durationMin;
	u16 m_durationMax;

	u8 m_minVelocityFrequency;
	u8 m_frequencyMin_1;
	u8 m_frequencyMax_1;

	u8 m_frequencyMin_2;
	u8 m_frequencyMax_2;

	PAR_SIMPLE_PARSABLE;
};
#endif


//  phMaterialMgrGta  //////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////

class phMaterialMgrGta : public phMaterialMgr
{
public:
	// access functions - main properties
	static inline Id		UnpackMtlId			(Id packedId) {return (packedId & 0xff);}
	static inline s32		UnpackProcId		(Id packedId) {return static_cast<s32>((packedId>>8)  & 0xff);}
	static inline s32		UnpackRoomId		(Id packedId) {return static_cast<s32>((packedId>>16) & 0x1f);}
	static inline s32		UnpackPedDensity	(Id packedId) {return static_cast<s32>((packedId>>21) & 0x07);}
#if HACK_GTA4_64BIT_MATERIAL_ID_COLORS
	static inline s32		UnpackPolyFlags		(Id packedId) {return static_cast<s32>((packedId>>24) & 0xffff);}
	static inline s32		UnpackMtlColour		(Id packedId) {return static_cast<s32>((packedId>>40) & 0xff);}
#else
	static inline s32		UnpackPolyFlags		(Id packedId) {return static_cast<s32>((packedId>>24) & 0xff);}
#endif

	static inline Id		GetMaterialMask()	{return UnpackMtlId((Id)(-1));}

#if __BANK
	// for edit/tweaking purposes only:
	static inline Id		PackProcId			(Id packedId, u8 procId)			{	packedId &= ~(((Id)0xff)<<8); packedId |= (((Id)procId)<<8); return packedId;}
#endif

	// access functions - poly properties (from a packed Id)
	static bool GetPolyFlagStairs           (Id packedId)   {return (UnpackPolyFlags(packedId) & BIT(POLYFLAG_STAIRS))                  != 0;}
	static bool GetPolyFlagNotClimbable     (Id packedId)   {return (UnpackPolyFlags(packedId) & BIT(POLYFLAG_NOT_CLIMBABLE))           != 0;}
	static bool GetPolyFlagNotCover         (Id packedId)   {return (UnpackPolyFlags(packedId) & BIT(POLYFLAG_NOT_COVER))               != 0;}
	static bool GetPolyFlagWalkablePath     (Id packedId)   {return (UnpackPolyFlags(packedId) & BIT(POLYFLAG_WALKABLE_PATH))           != 0;}
	static bool GetPolyFlagVehicleWheel     (Id packedId)   {return (UnpackPolyFlags(packedId) & BIT(POLYFLAG_VEHICLE_WHEEL))           != 0;}
	static bool GetPolyFlagNoCamCollision   (Id packedId)   {return (UnpackPolyFlags(packedId) & BIT(POLYFLAG_NO_CAM_COLLISION))        != 0;}
	static bool GetPolyFlagNoDecal          (Id packedId)   {return (UnpackPolyFlags(packedId) & BIT(POLYFLAG_NO_DECAL))                != 0;}
	static bool GetPolyFlagNoNavmesh        (Id packedId)   {return (UnpackPolyFlags(packedId) & BIT(POLYFLAG_NO_NAVMESH))              != 0;}
	static bool GetPolyFlagNoRagdoll        (Id packedId)   {return (UnpackPolyFlags(packedId) & BIT(POLYFLAG_NO_RAGDOLL))              != 0;}
	static bool GetPolyFlagNoPtFx           (Id packedId)   {return (UnpackPolyFlags(packedId) & BIT(POLYFLAG_NO_PTFX))                 != 0;}
	static bool GetPolyFlagTooSteepForPlayer(Id packedId)   {return (UnpackPolyFlags(packedId) & BIT(POLYFLAG_TOO_STEEP_FOR_PLAYER))    != 0;}
	static bool GetPolyFlagNoNetworkSpawn   (Id packedId)   {return (UnpackPolyFlags(packedId) & BIT(POLYFLAG_NO_NETWORK_SPAWN))        != 0;}
	static bool GetPolyFlagShootThroughFx   (Id packedId)   {return (UnpackPolyFlags(packedId) & BIT(POLYFLAG_SHOOT_THROUGH_FX))        != 0;}

	// query functions
	static inline bool GetIsGlass(VfxGroup_e grp) {return grp==VFXGROUP_GLASS || grp==VFXGROUP_CAR_GLASS || grp==VFXGROUP_GLASS_BULLETPROOF;}


#if !__SPU
	class RumbleProfileList
	{
	public:
		atArray<phRumbleProfile> m_rumbleProfiles;
		PAR_SIMPLE_PARSABLE;
	};

	///////////////////////////////////
	// TYPEDEFS 
	///////////////////////////////////

	typedef atMap<ConstString, phMaterialGta*>	MtlMapType;
	typedef atArray<phMaterialGta>				MtlArrayType;


	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

		// class creation function
static	phMaterialMgrGta*	Create						();

		// base class override functions
virtual void				Load						(int reservedSlots = 0);
virtual void				Destroy						();
virtual Id 					GetMaterialId				(const phMaterial& material) const;
virtual const phMaterial&	GetDefaultMaterial			() const;
virtual const phMaterial&	FindMaterial				(const char* packedName) const;

#if __PS3
virtual void				FillInSpuWorkUnit			(PairListWorkUnitInput& wuInput);
#endif


		const phMaterialGta* GetMaterialArray           () const                            {return m_array.GetElements();}


		// access functions - poly properties (from a packed Id)
		bool				GetPolyFlagSeeThrough		(Id packedId) const					{BANK_ONLY(if (m_mtlDebug.GetDisableSeeThrough()) return false; else) return (UnpackPolyFlags(packedId) &	BIT(POLYFLAG_SEE_THROUGH)) != 0;}
		bool				GetPolyFlagShootThrough		(Id packedId) const					{BANK_ONLY(if (m_mtlDebug.GetDisableShootThrough()) return false; else) return (UnpackPolyFlags(packedId) &	BIT(POLYFLAG_SHOOT_THROUGH)) != 0;}

		// access functions - mixed properties
		bool				GetIsSeeThrough				(Id packedId) const					{return GetMtlFlagSeeThrough(packedId) || GetPolyFlagSeeThrough(packedId);}
		bool				GetIsShootThrough			(Id packedId) const					{return GetPolyFlagShootThroughFx(packedId) || GetPolyFlagShootThrough(packedId);}
		bool				GetIsShootThroughFx			(Id packedId) const					{return GetPolyFlagShootThroughFx(packedId);}
		bool				GetIsNotCover				(Id packedId) const					{return GetIsShootThrough(packedId) || GetPolyFlagNotCover(packedId);}
		bool				GetIsNoDecal				(Id packedId) const					{return GetMtlFlagNoDecal(packedId) || GetPolyFlagNoDecal(packedId);}

		// access functions - material properties (from an unpacked material Id)
		VfxGroup_e			GetMtlVfxGroup				(Id mtlId) const					{return ((phMaterialGta*)&GetMaterial(mtlId))->GetVfxGroup();}
		VfxDisturbanceType_e GetMtlVfxDisturbanceType	(Id mtlId) const					{return ((phMaterialGta*)&GetMaterial(mtlId))->GetVfxDisturbanceType();}
		u32					GetMtlReactWeaponType		(Id mtlId) const					{return ((phMaterialGta*)&GetMaterial(mtlId))->GetReactWeaponType();}

		float 				GetMtlDensity				(Id mtlId) const					{return ((phMaterialGta*)&GetMaterial(mtlId))->GetDensity();}
		float 				GetMtlTyreGrip				(Id mtlId) const					{return ((phMaterialGta*)&GetMaterial(mtlId))->GetTyreGrip();}
		float 				GetMtlWetGripAdjust			(Id mtlId) const					{return ((phMaterialGta*)&GetMaterial(mtlId))->GetWetGripAdjust();}
		float 				GetMtlTyreDrag  			(Id mtlId) const					{return ((phMaterialGta*)&GetMaterial(mtlId))->GetTyreDrag();}
        float 				GetMtlTopSpeedMult 			(Id mtlId) const					{return ((phMaterialGta*)&GetMaterial(mtlId))->GetTopSpeedMult();}

		float				GetMtlVibration				(Id mtlId) const					{return ((phMaterialGta*)&GetMaterial(mtlId))->GetVibration();}
		float				GetMtlSoftness				(Id mtlId) const					{return ((phMaterialGta*)&GetMaterial(mtlId))->GetSoftness();}
		float				GetMtlNoisiness				(Id mtlId) const					{return ((phMaterialGta*)&GetMaterial(mtlId))->GetNoisiness();}

		float				GetMtlPenetrationResistance	(Id mtlId) const					{BANK_ONLY(if (m_mtlDebug.GetZeroPenetrationResistance()) return 0.0f; else) return ((phMaterialGta*)&GetMaterial(mtlId))->GetPenetrationResistance();}

		bool				GetMtlFlagSeeThrough		(Id mtlId) const					{BANK_ONLY(if (m_mtlDebug.GetDisableSeeThrough()) return false; else) return ((phMaterialGta*)&GetMaterial(mtlId))->GetFlag(MTLFLAG_SEE_THROUGH);}
		bool				GetMtlFlagShootThrough		(Id mtlId) const					{BANK_ONLY(if (m_mtlDebug.GetDisableShootThrough()) return false; else) return ((phMaterialGta*)&GetMaterial(mtlId))->GetFlag(MTLFLAG_SHOOT_THROUGH);}	
		bool				GetMtlFlagShootThroughFx	(Id mtlId) const					{return ((phMaterialGta*)&GetMaterial(mtlId))->GetFlag(MTLFLAG_SHOOT_THROUGH_FX);}	
		bool				GetMtlFlagNoDecal			(Id mtlId) const					{return ((phMaterialGta*)&GetMaterial(mtlId))->GetFlag(MTLFLAG_NO_DECAL);}
		bool				GetMtlFlagIsPorous			(Id mtlId) const					{return ((phMaterialGta*)&GetMaterial(mtlId))->GetFlag(MTLFLAG_POROUS);}					
		bool				GetMtlFlagHeatsTyre			(Id mtlId) const					{return ((phMaterialGta*)&GetMaterial(mtlId))->GetFlag(MTLFLAG_HEATS_TYRE);}					

		phRumbleProfile*	GetMtlRumbleProfile			(Id mtlId);

		// query functions
		bool				GetIsEmissive				(Id mtlId) const					{return (UnpackMtlId(mtlId)==g_idEmissiveGlass || UnpackMtlId(mtlId)==g_idEmissivePlastic);}
		bool				GetIsGlass					(Id mtlId) const					{return GetIsGlass(GetMtlVfxGroup(mtlId));}
		bool				GetIsSmashableGlass			(Id mtlId) const					{return (GetMtlVfxGroup(mtlId)==VFXGROUP_CAR_GLASS);}
		bool				GetIsPed					(Id mtlId) const					{return (GetMtlVfxGroup(mtlId)==VFXGROUP_PED_HEAD || GetMtlVfxGroup(mtlId)==VFXGROUP_PED_TORSO ||
																									 GetMtlVfxGroup(mtlId)==VFXGROUP_PED_LIMB || GetMtlVfxGroup(mtlId)==VFXGROUP_PED_FOOT);}
		bool				GetIsRoad					(Id mtlId) const					{return (GetMtlVfxGroup(mtlId)==VFXGROUP_TARMAC || GetMtlVfxGroup(mtlId)==VFXGROUP_TARMAC_BRITTLE);}
		bool				GetIsSand					(Id mtlId) const					{return (GetMtlVfxGroup(mtlId)==VFXGROUP_SAND_LOOSE || GetMtlVfxGroup(mtlId)==VFXGROUP_SAND_COMPACT || GetMtlVfxGroup(mtlId)==VFXGROUP_SAND_WET);}
		bool				GetIsDeepNonWading			(Id mtlId) const					{return (GetMtlVfxGroup(mtlId)==VFXGROUP_MUD_DEEP || GetMtlVfxGroup(mtlId)==VFXGROUP_GRAVEL_DEEP);}
		bool				GetIsDeepWading				(Id mtlId) const					{return (UnpackMtlId(mtlId)==g_idSnowDeep || UnpackMtlId(mtlId)==g_idMarshDeep || UnpackMtlId(mtlId)==g_idSandDryDeep || UnpackMtlId(mtlId)==g_idSandWetDeep);}
		
		// misc functions
		void				PackPolyFlag				(Id& packedId, u32 flag) const	{packedId |= (static_cast<Id>(BIT(flag)) << 24);}
		Id					GetPackedPolyFlagValue		(u32 flag) const;

		s32					GetRumbleProfileIndex		(const char* pName) const;
		void				LoadRumbleProfiles			();


		// debug functions
#if __BANK
		phMaterialDebug&	GetDebugInterface			()									{return m_mtlDebug;}
#endif


	protected: ////////////////////////

virtual Id					FindMaterialIdImpl			(const char* mtlName) const;


	private: //////////////////////////

							phMaterialMgrGta			();
virtual						~phMaterialMgrGta			();

virtual phMaterial&			AllocateMaterial			(const char* name);
		s32					PreLoadGtaMtlFile			(const char* name);
		void				LoadGtaMtlFile				(const char* name);
		void				LoadGtaMtlOverridePairFile	(const char* name);

		// unpacking functions
		void				UnpackMtlName				(const char* packedName, char* mtlName) const;
		void				UnpackProcName				(const char* packedName, char* procName) const;
		s32					UnpackRoomId				(const char* packedName) const;
		s32					UnpackPedDensity			(const char* packedName) const;
		s32					UnpackPolyFlags				(const char* packedName) const;


	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	public: ///////////////////////////

		// cached material ids - heli
		phMaterialMgr::Id	g_idDefault;
		phMaterialMgr::Id	g_idConcrete;

		// cached material ids - road
		phMaterialMgr::Id	g_idTarmac;

		// cached material ids - wheels
		phMaterialMgr::Id	g_idRubber;
		phMaterialMgr::Id	g_idCarVoid;
		phMaterialMgr::Id	g_idCarMetal;
		phMaterialMgr::Id	g_idCarEngine;
		phMaterialMgr::Id	g_idTrainTrack;

		// cached material ids - clear
		phMaterialMgr::Id	g_idCarSoftTopClear;
		phMaterialMgr::Id	g_idPlasticClear;
		phMaterialMgr::Id	g_idPlasticHollowClear;
		phMaterialMgr::Id	g_idPlasticHighDensityClear;

		// cached material ids - peds
		phMaterialMgr::Id	g_idPhysPedCapsule;

		// NM materials
		phMaterialMgr::Id	g_idButtocks;				// first ped material offset				
		phMaterialMgr::Id	g_idThighLeft;					
		phMaterialMgr::Id	g_idShinLeft;				
		phMaterialMgr::Id	g_idFootLeft;					
		phMaterialMgr::Id	g_idThighRight;					
		phMaterialMgr::Id	g_idShinRight;					
		phMaterialMgr::Id	g_idFootRight;				
		phMaterialMgr::Id	g_idSpine0;						
		phMaterialMgr::Id	g_idSpine1;						
		phMaterialMgr::Id	g_idSpine2;						
		phMaterialMgr::Id	g_idSpine3;						
		phMaterialMgr::Id	g_idClavicleLeft;				
		phMaterialMgr::Id	g_idUpperArmLeft;				
		phMaterialMgr::Id	g_idLowerArmLeft;				
		phMaterialMgr::Id	g_idHandLeft;				
		phMaterialMgr::Id	g_idClavicleRight;				
		phMaterialMgr::Id	g_idUpperArmRight;				
		phMaterialMgr::Id	g_idLowerArmRight;				
		phMaterialMgr::Id	g_idHandRight;					
		phMaterialMgr::Id	g_idNeck;						
		phMaterialMgr::Id	g_idHead;	
		phMaterialMgr::Id	g_idAnimalDefault;				

		// new materials
		phMaterialMgr::Id	g_idConcretePavement;
		phMaterialMgr::Id	g_idBrickPavement;

		// Special material used to identify frag bounds used only for cover shape tests and disabled when the frag breaks.
		phMaterialMgr::Id	g_idPhysDynamicCoverBound;

		// cached material ids - smashable
		phMaterialMgr::Id	g_idGlassShootThrough;
		phMaterialMgr::Id	g_idGlassBulletproof;
		phMaterialMgr::Id	g_idGlassOpaque;

		phMaterialMgr::Id	g_idCarGlassWeak;
		phMaterialMgr::Id 	g_idCarGlassMedium;
		phMaterialMgr::Id	g_idCarGlassStrong;
		phMaterialMgr::Id	g_idCarGlassBulletproof;
		phMaterialMgr::Id	g_idCarGlassOpaque;

		// cached material ids - decals
		phMaterialMgr::Id	g_idEmissiveGlass;
		phMaterialMgr::Id	g_idEmissivePlastic;
		phMaterialMgr::Id	g_idTreeBark;

		// cached material ids - deep surfaces
		phMaterialMgr::Id	g_idSnowDeep;
		phMaterialMgr::Id	g_idMarshDeep;
		phMaterialMgr::Id	g_idSandDryDeep;
		phMaterialMgr::Id	g_idSandWetDeep;

		// cached material ids - special online vehicle materials
		phMaterialMgr::Id	g_idPhysVehicleSpeedUp;
		phMaterialMgr::Id	g_idPhysVehicleSlowDown;
		phMaterialMgr::Id	g_idPhysVehicleRefill;
		phMaterialMgr::Id	g_idPhysVehicleBoostCancel;
		phMaterialMgr::Id	g_idPhysVehicleTyreBurst;

		// cached material ids - stunt races
		phMaterialMgr::Id	g_idStuntRampSurface;
		phMaterialMgr::Id	g_idStuntTargetA;
		phMaterialMgr::Id	g_idStuntTargetB;
		phMaterialMgr::Id	g_idStuntTargetC;

		// cached material ids - special online prop materials
		phMaterialMgr::Id	g_idPhysPropPlacement;

	private: //////////////////////////

		MtlMapType			m_map;
		MtlArrayType		m_array;

		float				m_version;

		RumbleProfileList	m_rumbleProfileList;

		// debug variables
#if __BANK
		phMaterialDebug		m_mtlDebug;
#endif

#endif //!__SPU...
};


#endif // GTA_MATERIAL_MANAGER_H


///////////////////////////////////////////////////////////////////////////////
