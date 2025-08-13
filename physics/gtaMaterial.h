///////////////////////////////////////////////////////////////////////////////
//  
//	FILE:	GtaMaterial.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	28 June 2005
//
///////////////////////////////////////////////////////////////////////////////

#ifndef GTA_MATERIAL_H
#define GTA_MATERIAL_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

// rage
#include "phcore/material.h"

//game
#include "basetypes.h"


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

#define CHECK_VFXGROUP_DATA			(0)


///////////////////////////////////////////////////////////////////////////////
//  ENUMERATED TYPES
///////////////////////////////////////////////////////////////////////////////

// material flags
enum
{
	MTLFLAG_SEE_THROUGH				= 0,										// ai probes pass through this poly
	MTLFLAG_SHOOT_THROUGH,														// weapon probes pass through this poly
	MTLFLAG_SHOOT_THROUGH_FX,													// weapon probes report a collision for this poly - even though the probe will pass through it (used for vfx/sfx)
	MTLFLAG_NO_DECAL,															// no projected texture decal is applied  
	MTLFLAG_POROUS,																// whether the material is porous (will soak liquids)
	MTLFLAG_HEATS_TYRE,															// whether the material causes the tyre to heat up when doing a burnout

	MTLFLAG_NUM_FLAGS
};


// poly flags
// NOTE: This must line up with /framework/tools/src/cli/ragebuilder/gtamaterialmgr.cpp polyflags
enum
{
	POLYFLAG_STAIRS					= 0,
	POLYFLAG_NOT_CLIMBABLE,
	POLYFLAG_SEE_THROUGH,
	POLYFLAG_SHOOT_THROUGH,
	POLYFLAG_NOT_COVER,
	POLYFLAG_WALKABLE_PATH,
	POLYFLAG_NO_CAM_COLLISION,
	POLYFLAG_SHOOT_THROUGH_FX,
	POLYFLAG_NO_DECAL,
	POLYFLAG_NO_NAVMESH,
	POLYFLAG_NO_RAGDOLL,
	POLYFLAG_RESERVED_FOR_TOOLS,							// This flag is used by tools only, and is should always be cleared on resources. 
	POLYFLAG_VEHICLE_WHEEL = POLYFLAG_RESERVED_FOR_TOOLS,	// This flag is only used at runtime so it takes the same slot as the tools only flag POLYFLAG_RESERVED_FOR_TOOLS
	POLYFLAG_NO_PTFX,
	POLYFLAG_TOO_STEEP_FOR_PLAYER,
	POLYFLAG_NO_NETWORK_SPAWN,
	POLYFLAG_NO_CAM_COLLISION_ALLOW_CLIPPING,

	POLYFLAG_NUM_FLAGS															// only 16 bits available
};


// vfx groups (also defined in VfxPedInfo.psc)
enum VfxGroup_e
{ 
	VFXGROUP_UNDEFINED				= -1,

	VFXGROUP_VOID					= 0,
	VFXGROUP_GENERIC,

	VFXGROUP_CONCRETE,
	VFXGROUP_CONCRETE_DUSTY,
	VFXGROUP_TARMAC,
	VFXGROUP_TARMAC_BRITTLE,
	VFXGROUP_STONE,
	VFXGROUP_BRICK,
	VFXGROUP_MARBLE,
	VFXGROUP_PAVING,
	VFXGROUP_SANDSTONE,
	VFXGROUP_SANDSTONE_BRITTLE,
			 
	VFXGROUP_SAND_LOOSE,
	VFXGROUP_SAND_COMPACT,
	VFXGROUP_SAND_WET,
	VFXGROUP_SAND_UNDERWATER,
	VFXGROUP_SAND_DEEP,
	VFXGROUP_SAND_WET_DEEP,
	VFXGROUP_ICE,
	VFXGROUP_SNOW_LOOSE,
	VFXGROUP_SNOW_COMPACT,
	VFXGROUP_GRAVEL,
	VFXGROUP_GRAVEL_DEEP,
	VFXGROUP_DIRT_DRY,
	VFXGROUP_MUD_SOFT,
	VFXGROUP_MUD_DEEP,
	VFXGROUP_MUD_UNDERWATER,
	VFXGROUP_CLAY,
			 
	VFXGROUP_GRASS,
	VFXGROUP_GRASS_SHORT,
	VFXGROUP_HAY,
	VFXGROUP_BUSHES,
	VFXGROUP_TREE_BARK,
	VFXGROUP_LEAVES,
			 
	VFXGROUP_METAL,
			 
	VFXGROUP_WOOD,
	VFXGROUP_WOOD_DUSTY,
	VFXGROUP_WOOD_SPLINTER,
			 
	VFXGROUP_CERAMIC,
	VFXGROUP_CARPET_FABRIC,
	VFXGROUP_CARPET_FABRIC_DUSTY,
	VFXGROUP_PLASTIC,
	VFXGROUP_PLASTIC_HOLLOW,
	VFXGROUP_RUBBER,
	VFXGROUP_LINOLEUM,
	VFXGROUP_PLASTER_BRITTLE,
	VFXGROUP_CARDBOARD,
	VFXGROUP_PAPER,
	VFXGROUP_FOAM,
	VFXGROUP_FEATHERS,
	VFXGROUP_TVSCREEN,
			 
	VFXGROUP_GLASS,
	VFXGROUP_GLASS_BULLETPROOF,
			 
	VFXGROUP_CAR_METAL,
	VFXGROUP_CAR_PLASTIC,
	VFXGROUP_CAR_GLASS,

	VFXGROUP_PUDDLE,

	VFXGROUP_LIQUID_WATER,
	VFXGROUP_LIQUID_BLOOD,
	VFXGROUP_LIQUID_OIL,
	VFXGROUP_LIQUID_PETROL,
	VFXGROUP_LIQUID_MUD,

	VFXGROUP_FRESH_MEAT,
	VFXGROUP_DRIED_MEAT,
			 
	VFXGROUP_PED_HEAD,
	VFXGROUP_PED_TORSO,
	VFXGROUP_PED_LIMB,
	VFXGROUP_PED_FOOT,
	VFXGROUP_PED_CAPSULE,

	NUM_VFX_GROUPS,
};


// vfx disturbance type
enum VfxDisturbanceType_e
{
	VFX_DISTURBANCE_UNDEFINED		= -1,

	VFX_DISTURBANCE_DEFAULT			= 0,
	VFX_DISTURBANCE_SAND,
	VFX_DISTURBANCE_DIRT,
	VFX_DISTURBANCE_WATER,
	VFX_DISTURBANCE_FOLIAGE,

	NUM_VFX_DISTURBANCES,
};


// material class type
enum 
{
	GTA_MATERIAL = 1
};




extern const char* g_fxGroupsList[NUM_VFX_GROUPS];


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////

class ALIGNAS(16) phMaterialGta : public phMaterial
{
	///////////////////////////////////
	// DEFINES 
	///////////////////////////////////


	#define	DEFAULT_VFXGROUP			(VFXGROUP_VOID)
	#define	DEFAULT_VFXDISTURBANCETYPE	(VFX_DISTURBANCE_DEFAULT)
	#define	DEFAULT_REACT_WEAPON_TYPE	(0)

	#define DEFAULT_DENSITY				(1.0f)
	#define DEFAULT_TYRE_GRIP			(1.0f)
	#define DEFAULT_WET_MULT			(-0.25f)

	#define DEFAULT_VIBRATION			(0.0f)
	#define DEFAULT_SOFTNESS			(0.0f)
	#define DEFAULT_NOISINESS			(0.0f)


	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////

	public: ///////////////////////////

							phMaterialGta			();

		void				LoadGtaMtlData			(char* pLine, float versionNumber);

		void				SetVfxGroup				(VfxGroup_e val)			{m_vfxGroup = val;}	
		VfxGroup_e			GetVfxGroup				() const					{return m_vfxGroup;}
		const VfxGroup_e *	GetVfxGroupPtr			() const					{return &m_vfxGroup;}
static	VfxGroup_e			FindVfxGroup			(char* pName);

		void				SetVfxDisturbanceType	(VfxDisturbanceType_e val)	{m_vfxDisturbanceType = val;}	
		VfxDisturbanceType_e GetVfxDisturbanceType	()							{return m_vfxDisturbanceType;}
static	VfxDisturbanceType_e FindVfxDisturbanceType	(char* pName);

		void				SetReactWeaponType		(u32 val)					{m_reactWeaponType = val;}
		u32					GetReactWeaponType		()							{return m_reactWeaponType;}
static	u32					FindReactWeaponType		(char* pName);

		void				SetRumbleProfile		(const char* pName);
		s32					GetRumbleProfileIndex	()							{return m_rumbleProfileIndex;}

		void				SetDensity				(float val)					{m_density = val;}
		float				GetDensity				()							{return m_density;}
		void				SetTyreGrip				(float val)					{m_tyreGrip	= val;}	
		float 				GetTyreGrip				()							{return m_tyreGrip;}
		void				SetWetGripAdjust		(float val)					{m_wetGripAdjust = val;}
		float 				GetWetGripAdjust		()							{return 1.0f + (m_wetGripAdjust);}
        void				SetTyreDrag     		(float val)					{m_tyreDrag = val;}
        float 				GetTyreDrag     		()							{return m_tyreDrag;}
        void				SetTopSpeedMult    		(float val)					{m_topSpeedMult = val;}
        float 				GetTopSpeedMult  		()							{return m_topSpeedMult;}
		
		void				SetVibration			(float val)					{m_vibration = val;}	
		float				GetVibration			()							{return m_vibration;}
		void				SetSoftness				(float val)					{m_softness = val;}	
		float				GetSoftness				()							{return m_softness;}
		void				SetNoisiness			(float val)					{m_noisiness = val;}	
		float				GetNoisiness			()							{return m_noisiness;}

		void				SetPenetrationResistance(float val)					{m_penetrationResistance = val;}	
		float				GetPenetrationResistance()							{return m_penetrationResistance;}

		// flags
		void				SetFlag					(u32 flagId)				{m_flags |= (1<<flagId);}
		void				ClearFlag				(u32 flagId)				{m_flags &= ~(1<<flagId);};
		bool				GetFlag					(u32 flagId)				{return (m_flags & (1<<flagId)) != 0;}


	private: //////////////////////////


	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		VfxGroup_e			m_vfxGroup;
		VfxDisturbanceType_e m_vfxDisturbanceType;
		u32					m_reactWeaponType;										// the weapon type to apply damage with
		s32					m_rumbleProfileIndex;

		float				m_density;
		float				m_tyreGrip;						
		float				m_wetGripAdjust;
        float				m_tyreDrag;	
        float               m_topSpeedMult;

		float				m_vibration;
		float				m_softness;
		float				m_noisiness;
		
		float				m_penetrationResistance;

		// flags
		u32					m_flags;

} ;  // phMaterialGta


#endif // GTA_MATERIAL_H


///////////////////////////////////////////////////////////////////////////////
