//
// gtaArchetype.cpp
//

#include "atl/array_struct.h"

#include "breakableglass/breakable.h"
#include "breakableglass/crackstemplate.h"
#include "fragment/typegroup.h"

#include "fwscene/stores/fragmentstore.h"
#include "vfx/vehicleglass/vehicleglasswindow.h"

#include "physics/gtaArchetype.h"

///////////////////////////////////
// gtaFragType

gtaFragType::gtaFragType(datResource& rsc)
: fragType(rsc),m_lights(rsc,true),m_pTintData(rsc)
{
	fwVehicleGlassWindowData::PointerFixup(rsc, m_vehicleWindowData);

#if __ASSERT
	if (m_PhysicsLODGroup)
	{
		for(int groupIndex = 0; groupIndex != GetPhysics(0)->GetNumChildGroups(); ++ groupIndex)
		{
			fragTypeGroup* group = GetPhysics(0)->GetAllGroups()[groupIndex];
			if(group->GetMadeOfGlass())
			{
				bgPaneModelInfoBase* pModelInfo = GetAllGlassPaneModelInfos()[group->GetGlassPaneModelInfoIndex()];			
				Assert(pModelInfo->m_glassType < bgSCracksTemplate::InstanceRef().GetGlassTypes().GetCount());
				if(pModelInfo->m_glassType > bgSCracksTemplate::InstanceRef().GetGlassTypes().GetCount())
				{
					pModelInfo->m_glassType = 0;
				}
			}
		}
	}
#endif // __ASSERT	
}

gtaFragType::~gtaFragType(void)
{
	g_FragmentStore.CheckIfInUse(this);
}

#if __DECLARESTRUCT
void gtaFragType::DeclareStruct(datTypeStruct &s)
{
	fragType::DeclareStruct(s);

	STRUCT_BEGIN(gtaFragType);
		STRUCT_FIELD(m_lights);
		STRUCT_FIELD_VP(m_vehicleWindowData);
		STRUCT_FIELD(m_pTintData);
	STRUCT_END();
}
#endif

#if !__FINAL
//////////////////////////////////////////////////////////////////////////
namespace rage
{
	EXT_PFD_DECLARE_ITEM(TypeFlag0);
	EXT_PFD_DECLARE_ITEM(TypeFlag1);
	EXT_PFD_DECLARE_ITEM(TypeFlag2);
	EXT_PFD_DECLARE_ITEM(TypeFlag3);
	EXT_PFD_DECLARE_ITEM(TypeFlag4);
	EXT_PFD_DECLARE_ITEM(TypeFlag5);
	EXT_PFD_DECLARE_ITEM(TypeFlag6);
	EXT_PFD_DECLARE_ITEM(TypeFlag7);
	EXT_PFD_DECLARE_ITEM(TypeFlag8);
	EXT_PFD_DECLARE_ITEM(TypeFlag9);
	EXT_PFD_DECLARE_ITEM(TypeFlag10);
	EXT_PFD_DECLARE_ITEM(TypeFlag11);
	EXT_PFD_DECLARE_ITEM(TypeFlag12);
	EXT_PFD_DECLARE_ITEM(TypeFlag13);
	EXT_PFD_DECLARE_ITEM(TypeFlag14);
	EXT_PFD_DECLARE_ITEM(TypeFlag15);
	EXT_PFD_DECLARE_ITEM(TypeFlag16);
	EXT_PFD_DECLARE_ITEM(TypeFlag17);
	EXT_PFD_DECLARE_ITEM(TypeFlag18);
	EXT_PFD_DECLARE_ITEM(TypeFlag19);
	EXT_PFD_DECLARE_ITEM(TypeFlag20);
	EXT_PFD_DECLARE_ITEM(TypeFlag21);
	EXT_PFD_DECLARE_ITEM(TypeFlag22);
	EXT_PFD_DECLARE_ITEM(TypeFlag23);
	EXT_PFD_DECLARE_ITEM(TypeFlag24);
	EXT_PFD_DECLARE_ITEM(TypeFlag25);
	EXT_PFD_DECLARE_ITEM(TypeFlag26);
	EXT_PFD_DECLARE_ITEM(TypeFlag27);
	EXT_PFD_DECLARE_ITEM(TypeFlag28);
	EXT_PFD_DECLARE_ITEM(TypeFlag29);
	EXT_PFD_DECLARE_ITEM(TypeFlag30);
	EXT_PFD_DECLARE_ITEM(TypeFlag31);
	EXT_PFD_DECLARE_ITEM(IncludeFlag0);
	EXT_PFD_DECLARE_ITEM(IncludeFlag1);
	EXT_PFD_DECLARE_ITEM(IncludeFlag2);
	EXT_PFD_DECLARE_ITEM(IncludeFlag3);
	EXT_PFD_DECLARE_ITEM(IncludeFlag4);
	EXT_PFD_DECLARE_ITEM(IncludeFlag5);
	EXT_PFD_DECLARE_ITEM(IncludeFlag6);
	EXT_PFD_DECLARE_ITEM(IncludeFlag7);
	EXT_PFD_DECLARE_ITEM(IncludeFlag8);
	EXT_PFD_DECLARE_ITEM(IncludeFlag9);
	EXT_PFD_DECLARE_ITEM(IncludeFlag10);
	EXT_PFD_DECLARE_ITEM(IncludeFlag11);
	EXT_PFD_DECLARE_ITEM(IncludeFlag12);
	EXT_PFD_DECLARE_ITEM(IncludeFlag13);
	EXT_PFD_DECLARE_ITEM(IncludeFlag14);
	EXT_PFD_DECLARE_ITEM(IncludeFlag15);
	EXT_PFD_DECLARE_ITEM(IncludeFlag16);
	EXT_PFD_DECLARE_ITEM(IncludeFlag17);
	EXT_PFD_DECLARE_ITEM(IncludeFlag18);
	EXT_PFD_DECLARE_ITEM(IncludeFlag19);
	EXT_PFD_DECLARE_ITEM(IncludeFlag20);
	EXT_PFD_DECLARE_ITEM(IncludeFlag21);
	EXT_PFD_DECLARE_ITEM(IncludeFlag22);
	EXT_PFD_DECLARE_ITEM(IncludeFlag23);
	EXT_PFD_DECLARE_ITEM(IncludeFlag24);
	EXT_PFD_DECLARE_ITEM(IncludeFlag25);
	EXT_PFD_DECLARE_ITEM(IncludeFlag26);
	EXT_PFD_DECLARE_ITEM(IncludeFlag27);
	EXT_PFD_DECLARE_ITEM(IncludeFlag28);
	EXT_PFD_DECLARE_ITEM(IncludeFlag29);
	EXT_PFD_DECLARE_ITEM(IncludeFlag30);
	EXT_PFD_DECLARE_ITEM(IncludeFlag31);
}

namespace ArchetypeFlags
{
	// Please keep this in sync with the source near the top of gtaArchetype.h
	const char * sm_BoundFlagName[maxBits] =
	{
		"UNUSED",
		"GTA_MAP_TYPE_WEAPON",
		"GTA_MAP_TYPE_MOVER",
		"GTA_MAP_TYPE_HORSE",
		"GTA_MAP_TYPE_COVER",
		"GTA_MAP_TYPE_VEHICLE",
		"GTA_VEHICLE_NON_BVH_TYPE",
		"GTA_VEHICLE_BVH_TYPE",
		"GTA_BOX_VEHICLE_TYPE",
		"GTA_PED_TYPE",
		"GTA_RAGDOLL_TYPE",
		"GTA_HORSE_TYPE",
		"GTA_HORSE_RAGDOLL_TYPE",
		"GTA_OBJECT_TYPE",
		"GTA_ENVCLOTH_OBJECT_TYPE",
		"GTA_PLANT_TYPE",

		"GTA_PROJECTILE_TYPE",
		"GTA_EXPLOSION_TYPE",
		"GTA_PICKUP_TYPE",
		"GTA_FOLIAGE_TYPE",
		"GTA_FORKLIFT_FORKS_TYPE",

		"GTA_WEAPON_TEST",
		"GTA_CAMERA_TEST",
		"GTA_AI_TEST",
		"GTA_SCRIPT_TEST",
		"GTA_WHEEL_TEST",

		"GTA_GLASS_TYPE",
		"GTA_RIVER_TYPE",
		"GTA_SMOKE_TYPE",
		"GTA_UNSMASHED_TYPE",
		"GTA_STAIR_SLOPE_TYPE",
		"GTA_DEEP_SURFACE_TYPE"
	};

	////////////////////////////////////////////////////////////////////////////////
	const char * GetBoundFlagName(const u32 bit)
	{
		Assert(bit<maxBits);
		if( bit>=maxBits )
			return "OUT OF RANGE";
		return sm_BoundFlagName[bit];
	}

	////////////////////////////////////////////////////////////////////////////////
	void PrintBoundFlags(const u32 flags, const char * pPrefixString, bool onlyPrintFlagsUsed)
	{
		Displayf("%s ========>", pPrefixString);
		for( u32 u=0; u<=maxBits; ++u )
		{
			if(!onlyPrintFlagsUsed ||  (flags & (1<<u)))
				Displayf("\t[%c] %s", ((flags&(1<<u))!=0 ? 'X' : ' '), GetBoundFlagName(u));
		}
	}

	////////////////////////////////////////////////////////////////////////////////
	void SetupCustomBoundFlagNames()
	{
	#if __PFDRAW
	#define FIXUP_NAME(bit) PFD_TypeFlag##bit.SetName(GetBoundFlagName(bit)); \
		PFD_IncludeFlag##bit.SetName(GetBoundFlagName(bit));

		FIXUP_NAME(0);
		FIXUP_NAME(1);
		FIXUP_NAME(2);
		FIXUP_NAME(3);
		FIXUP_NAME(4);
		FIXUP_NAME(5);
		FIXUP_NAME(6);
		FIXUP_NAME(7);
		FIXUP_NAME(8);
		FIXUP_NAME(9);
		FIXUP_NAME(10);
		FIXUP_NAME(11);
		FIXUP_NAME(12);
		FIXUP_NAME(13);
		FIXUP_NAME(14);
		FIXUP_NAME(15);
		FIXUP_NAME(16);
		FIXUP_NAME(17);
		FIXUP_NAME(18);
		FIXUP_NAME(19);
		FIXUP_NAME(20);
		FIXUP_NAME(21);
		FIXUP_NAME(22);
		FIXUP_NAME(23);
		FIXUP_NAME(24);
		FIXUP_NAME(25);
		FIXUP_NAME(26);
		FIXUP_NAME(27);
		FIXUP_NAME(28);
		FIXUP_NAME(29);
		FIXUP_NAME(30);
		FIXUP_NAME(31);
	#endif
	}
}

#endif // !__FINAL
