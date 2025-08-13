///////////////////////////////////////////////////////////////////////////////
//  
//	FILE:	GtaMaterial.cpp
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	28 June 2005
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//	INCLUDES
///////////////////////////////////////////////////////////////////////////////

// system 
#include "stdio.h"

// rage
#include "file/token.h"
#include "string/stringhash.h"
#include "system/nelem.h"
#include "system/param.h"

// framework
#include "vfx/channel.h"

// game
#include "Physics/GtaMaterial.h"
#include "physics/gtaMaterialManager.h"
#ifndef NAVGEN_TOOL
#include "Debug/Debug.h"
#endif


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_SYSTEM_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

const char* g_fxGroupsList[] = 
{
	"VOID",
	"GENERIC",

	"CONCRETE",
	"CONCRETE_DUSTY",
	"TARMAC",
	"TARMAC_BRITTLE",
	"STONE",
	"BRICK",
	"MARBLE",
	"PAVING",
	"SANDSTONE",
	"SANDSTONE_BRITTLE",

	"SAND_LOOSE",
	"SAND_COMPACT",
	"SAND_WET",
	"SAND_UNDERWATER",
	"SAND_DEEP",
	"SAND_WET_DEEP",
	"ICE",
	"SNOW_LOOSE",
	"SNOW_COMPACT",
	"GRAVEL",
	"GRAVEL_DEEP",
	"DIRT_DRY",
	"MUD_SOFT",
	"MUD_DEEP",
	"MUD_UNDERWATER",
	"CLAY",

	"GRASS",
	"GRASS_SHORT",
	"HAY",
	"BUSHES",
	"TREE_BARK",
	"LEAVES",

	"METAL",

	"WOOD",
	"WOOD_DUSTY",
	"WOOD_SPLINTER",

	"CERAMIC",
	"CARPET_FABRIC",
	"CARPET_FABRIC_DUSTY",
	"PLASTIC",
	"PLASTIC_HOLLOW",
	"RUBBER",
	"LINOLEUM",
	"PLASTER_BRITTLE",
	"CARDBOARD",
	"PAPER",
	"FOAM",
	"FEATHERS",
	"TVSCREEN",

	"GLASS",
	"GLASS_BULLETPROOF",

	"CAR_METAL",
	"CAR_PLASTIC",
	"CAR_GLASS",

	"PUDDLE",

	"LIQUID_WATER",
	"LIQUID_BLOOD",
	"LIQUID_OIL",
	"LIQUID_PETROL",
	"LIQUID_MUD",

	"FRESH_MEAT",
	"DRIED_MEAT",

	"PED_HEAD",
	"PED_TORSO",
	"PED_LIMB",
	"PED_FOOT",
	"PED_CAPSULE",
};
CompileTimeAssert(NELEM(g_fxGroupsList) == NUM_VFX_GROUPS);

const char* g_vfxDisturbancesList[] = 
{
	"DEFAULT",
	"SAND",
	"DIRT",
	"WATER",
	"FOLIAGE",
};
CompileTimeAssert(NELEM(g_vfxDisturbancesList) == NUM_VFX_DISTURBANCES);

XPARAM(setMaterialDensityScale);

///////////////////////////////////////////////////////////////////////////////
//	CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//	CLASS phMaterialGta
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//	Constructor
///////////////////////////////////////////////////////////////////////////////

				phMaterialGta::phMaterialGta				()
: phMaterial()
{
	m_Type = GTA_MATERIAL;
	m_rumbleProfileIndex = -1;

	SetVfxGroup			(DEFAULT_VFXGROUP);
	SetVfxDisturbanceType(DEFAULT_VFXDISTURBANCETYPE);
	SetReactWeaponType	(DEFAULT_REACT_WEAPON_TYPE);

	SetDensity			(DEFAULT_DENSITY);
	SetTyreGrip			(DEFAULT_TYRE_GRIP);					
	SetWetGripAdjust	(DEFAULT_WET_MULT);			
			
	SetVibration		(DEFAULT_VIBRATION);	
	SetSoftness			(DEFAULT_SOFTNESS);	
	SetNoisiness		(DEFAULT_NOISINESS);

	// flags
	m_flags	= 0;
}


///////////////////////////////////////////////////////////////////////////////
//	LoadGtaMtlData
///////////////////////////////////////////////////////////////////////////////

void			phMaterialGta::LoadGtaMtlData			(char* pLine, float versionNumber)
{
	// vars to read data into
	char materialName[64];
	char filterName[64];
	char vfxGroupName[64];
	char vfxDisturbanceTypeName[64];
	char reactWeaponTypeName[64];
	char rumbleProfileName[64] = {0};
	float rageFriction, rageElasticity;
	float density, typeGrip, wetMult, tyreDrag, topSpeedMult;
	float vibration = DEFAULT_VIBRATION;
	float softness, noisiness;
	float flammability, burnTime, burnStrength;						// these are no longer needed in the latest version
	float penetrationResistance;
	
	// vars to read flag data into
	s32 isSeeThrough, isShootThrough, isShootThroughFx, isNoDecal, isPorous, heatsTyre;

	if (versionNumber>=12.0f)
	{
		// NOTE: This must line up with /framework/tools/src/cli/ragebuilder/gtamaterialmgr.cpp GtaMaterial::LoadGtaMtlData
		sscanf(pLine, "%s %s   %s %s %s %s   %f %f    %f %f %f %f %f   %f %f    %f    %d %d %d %d %d %d",
			materialName,		filterName,
			vfxGroupName,		vfxDisturbanceTypeName,	rumbleProfileName,		reactWeaponTypeName,
			&rageFriction,		&rageElasticity,
			&density,			&typeGrip,				&wetMult,				&tyreDrag,				&topSpeedMult,
			&softness,			&noisiness,
			&penetrationResistance,
			&isSeeThrough,		&isShootThrough,		&isShootThroughFx,		&isNoDecal,				&isPorous,		&heatsTyre);
	}
	else if (versionNumber>=11.0f)
	{
		// NOTE: This must line up with /framework/tools/src/cli/ragebuilder/gtamaterialmgr.cpp GtaMaterial::LoadGtaMtlData
		sscanf(pLine, "%s %s   %s %s %s %s   %f %f    %f %f %f %f %f   %f %f    %f %f %f    %f    %d %d %d %d %d %d",
			materialName,		filterName,
			vfxGroupName,		vfxDisturbanceTypeName,	rumbleProfileName,		reactWeaponTypeName,
			&rageFriction,		&rageElasticity,
			&density,			&typeGrip,				&wetMult,				&tyreDrag,				&topSpeedMult,
			&softness,			&noisiness,
			&flammability,		&burnTime,				&burnStrength,
			&penetrationResistance,
			&isSeeThrough,		&isShootThrough,		&isShootThroughFx,		&isNoDecal,				&isPorous,		&heatsTyre);
	}
	else if (versionNumber>=10.0f)
    {
        sscanf(pLine, "%s %s   %s %s %s %s   %f %f    %f %f %f %f %f   %f %f    %f %f %f    %f    %d %d %d %d %d",
            materialName,		filterName,
            vfxGroupName,		vfxDisturbanceTypeName,	rumbleProfileName,		reactWeaponTypeName,
            &rageFriction,		&rageElasticity,
            &density,			&typeGrip,				&wetMult,				&tyreDrag,				&topSpeedMult,
            &softness,			&noisiness,
            &flammability,		&burnTime,				&burnStrength,
            &penetrationResistance,
            &isSeeThrough,		&isShootThrough,		&isShootThroughFx,		&isNoDecal,				&isPorous);

		heatsTyre = false;
    }
	else if (versionNumber>=9.0f)
	{
		sscanf(pLine, "%s %s   %s %s %s %s   %f %f    %f %f %f %f   %f %f    %f %f %f    %f    %d %d %d %d %d",
			materialName,		filterName,
			vfxGroupName,		vfxDisturbanceTypeName,	rumbleProfileName,		reactWeaponTypeName,
			&rageFriction,		&rageElasticity,
			&density,			&typeGrip,				&wetMult,				&tyreDrag,	
			&softness,			&noisiness,
			&flammability,		&burnTime,				&burnStrength,
			&penetrationResistance,
			&isSeeThrough,		&isShootThrough,		&isShootThroughFx,		&isNoDecal,				&isPorous);

		topSpeedMult = 1.0f;
		heatsTyre = false;
	}
	else if (versionNumber>=8.0f)
    {
        sscanf(pLine, "%s %s   %s %s %s   %f %f    %f %f %f %f   %f %f %f    %f %f %f    %f    %d %d %d %d %d",
            materialName,		filterName,
            vfxGroupName,		vfxDisturbanceTypeName,	reactWeaponTypeName,
            &rageFriction,		&rageElasticity,
            &density,			&typeGrip,				&wetMult,				&tyreDrag,	
            &vibration,			&softness,				&noisiness,
            &flammability,		&burnTime,				&burnStrength,
            &penetrationResistance,
            &isSeeThrough,		&isShootThrough,		&isShootThroughFx,		&isNoDecal,				&isPorous);

		topSpeedMult = 1.0f;
		heatsTyre = false;
    }
	else if (versionNumber>=7.0f)
	{
		sscanf(pLine, "%s %s   %s %s %s   %f %f    %f %f %f    %f %f %f    %f %f %f    %f    %d %d %d %d %d",
			materialName,		filterName,
			vfxGroupName,		vfxDisturbanceTypeName,	reactWeaponTypeName,
			&rageFriction,		&rageElasticity,
			&density,			&typeGrip,				&wetMult,		
			&vibration,			&softness,				&noisiness,
			&flammability,		&burnTime,				&burnStrength,
			&penetrationResistance,
			&isSeeThrough,		&isShootThrough,		&isShootThroughFx,		&isNoDecal,				&isPorous);

        tyreDrag = 0.0f;
		topSpeedMult = 1.0f;
		heatsTyre = false;
	}
	else
	{
		sscanf(pLine, "%s %s   %s %s %s   %f %f    %f %f %f    %f %f %f    %f %f %f    %f    %d %d %d %d",
			materialName,		filterName,
			vfxGroupName,		vfxDisturbanceTypeName,	reactWeaponTypeName,
			&rageFriction,		&rageElasticity,
			&density,			&typeGrip,				&wetMult,		
			&vibration,			&softness,				&noisiness,
			&flammability,		&burnTime,				&burnStrength,
			&penetrationResistance,
			&isSeeThrough,		&isShootThrough,		&isShootThroughFx,		&isNoDecal);

		isPorous = false;
        tyreDrag = 0.0f;
        topSpeedMult = 1.0f;
		heatsTyre = false;
	}

	// set rage properties
	SetFriction(rageFriction);
	SetElasticity(rageElasticity);

	// set gta properties
	VfxGroup_e vfxGroup = FindVfxGroup(vfxGroupName);
	if (vfxGroup==VFXGROUP_UNDEFINED)
	{
		m_vfxGroup = VFXGROUP_GENERIC;
	}
	else
	{
		m_vfxGroup = vfxGroup;
	}

	m_vfxDisturbanceType = FindVfxDisturbanceType(vfxDisturbanceTypeName);
	m_reactWeaponType = FindReactWeaponType(reactWeaponTypeName);

#if !__FINAL
	float fDensityScale = 1.0f;
	if(PARAM_setMaterialDensityScale.Get())
	{
		PARAM_setMaterialDensityScale.Get(fDensityScale);
	}
	m_density = density*fDensityScale;
#else // !__FINAL
	m_density = density;
#endif // !__FINAL
	m_tyreGrip = typeGrip;
	m_wetGripAdjust = wetMult;
    m_tyreDrag = tyreDrag;
    m_topSpeedMult = topSpeedMult;

	if (rumbleProfileName[0] != '\0')
	{
		SetRumbleProfile(rumbleProfileName);
	}
	else
	{
		m_vibration = vibration;
	}

	m_softness = softness;
	m_noisiness = noisiness;

	m_penetrationResistance = penetrationResistance;
	
	if (isSeeThrough)			SetFlag(MTLFLAG_SEE_THROUGH);
	if (isShootThrough)			SetFlag(MTLFLAG_SHOOT_THROUGH);
	if (isShootThroughFx)		SetFlag(MTLFLAG_SHOOT_THROUGH_FX);
	if (isNoDecal)				SetFlag(MTLFLAG_NO_DECAL);
	if (isPorous)				SetFlag(MTLFLAG_POROUS);
	if (heatsTyre)				SetFlag(MTLFLAG_HEATS_TYRE);
}


///////////////////////////////////////////////////////////////////////////////
//	FindVfxGroup
///////////////////////////////////////////////////////////////////////////////

VfxGroup_e		phMaterialGta::FindVfxGroup			(char* pName)
{
	for (s32 i=0; i<NUM_VFX_GROUPS; i++)
	{
		if (strcmp(pName, g_fxGroupsList[i])==0)
		{
			return (VfxGroup_e)i;
		}
	}

	vfxAssertf(false, "Invalid VfxGroup found '%s'", pName);
	return VFXGROUP_UNDEFINED;
}


///////////////////////////////////////////////////////////////////////////////
//	FindVfxDisturbanceType
///////////////////////////////////////////////////////////////////////////////

VfxDisturbanceType_e phMaterialGta::FindVfxDisturbanceType(char* pName)
{
	for (s32 i=0; i<NUM_VFX_DISTURBANCES; i++)
	{
		if (strcmp(pName, g_vfxDisturbancesList[i])==0)
		{
			return (VfxDisturbanceType_e)i;
		}
	}

	vfxAssertf(false, "Invalid vfx disturbance type found '%s'", pName);
	return VFX_DISTURBANCE_UNDEFINED;
}


///////////////////////////////////////////////////////////////////////////////
//	FindReactWeaponType
///////////////////////////////////////////////////////////////////////////////

u32			phMaterialGta::FindReactWeaponType		(char* pName)
{
	// This is our NULL identifier
	if (strcmp(pName, "-")==0)
	{
		return 0;
	}

	return atStringHash(pName);
}

///////////////////////////////////////////////////////////////////////////////
//	SetRumbleProfile
///////////////////////////////////////////////////////////////////////////////

void		phMaterialGta::SetRumbleProfile			(const char* pName)
{
	// This is our NULL identifier
	if (!pName || strcmp(pName, "-")==0)
	{
		return;
	}

	m_rumbleProfileIndex = PGTAMATERIALMGR->GetRumbleProfileIndex(pName);
}



