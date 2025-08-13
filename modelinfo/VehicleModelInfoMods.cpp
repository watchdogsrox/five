//
// VehicleModelInfoMods.cpp
// Data file for vehicle mod settings
//
// Rockstar Games (c) 2013

#include "modelinfo/VehicleModelInfoMods.h"

#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "bank/combo.h"
#include "modelinfo/VehicleModelInfoVariation.h"

using namespace rage;

const CVehicleModLink* CVehicleKit::FindLinkMod(u32 nameHash, u8& index) const
{
	index = INVALID_MOD;

	for (s32 i = 0; i < m_linkMods.GetCount(); ++i)
	{
		if (m_linkMods[i].GetNameHash() == nameHash)
		{
			index = (u8)i;
			return &m_linkMods[i];
		}
	}

	return NULL;
}

const char* g_renderableSlotNames[VMT_RENDERABLE] = {
	"Spoiler",
	"Front Bumper",
	"Rear Bumper",
	"Skirt",
	"Exhaust", 
	"Chassis", 
	"Grill", 
	"Bonnet", 
	"Left Wing", 
	"Right Wing", 
	"Roof", 
	"",					//Plate Holder 
	"",					//Front Vanity Plate 
	"", "", "", "", "",	//Interior x5 slots
	"",					//Seats 
	"",					//Steering Wheel 
	"",					//Gearknob 
	"",					//Club Plaques 
	"",					//ICE 
	"",					//Trunk 
	"",					//Hydraulics 
	"",					//Engine Detailing 
	"",					//Strut Brace 
	"",					//Engine 
	"", "", "", "",		//Chassis x 4 slots
	"", "",				//Door LF, RF 
	""					//Light Bar
};

const char* CVehicleKit::GetModSlotName(eVehicleModType slot) const
{
    if (!Verifyf(slot < VMT_RENDERABLE, "Invalid slot '%d' passed to GetModSlotName!", slot))
        return NULL;

    for (s32 i = 0; i < m_slotNames.GetCount(); ++i)
    {
        if (m_slotNames[i].slot == slot)
        {
            return m_slotNames[i].name.c_str();
        }
    }
    
    return g_renderableSlotNames[slot];
}

const char* CVehicleKit::GetLiveryName(u32 livery) const
{
	if (livery >= m_liveryNames.GetCount())
		return NULL;

	return m_liveryNames[livery].c_str();
}

const char* CVehicleKit::GetLivery2Name(u32 livery2) const
{
	if (livery2 >= m_livery2Names.GetCount())
		return NULL;

	return m_livery2Names[livery2].c_str();
}
