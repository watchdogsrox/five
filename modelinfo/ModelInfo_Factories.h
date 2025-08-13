//
//
//    Filename: ModelInfo.h
//     Creator: Adam Fowler
//     $Author: $
//       $Date: $
//   $Revision: $
// Description: Class containing all the lists of models

//
//
#ifndef INC_ModelInfo_Factories_H_
#define INC_ModelInfo_Factories_H_

#ifndef INC_MODELINFO_H_
#error Modelinfo.h must be included before ModelInfo_Factories.h.
#endif // INC_MODELINFO_H_

// On 360, a simple forward declaration is not good enough to compile the templates.
// We'll do the full include so the store accessors can be properly inlined.
#include "BaseModelInfo.h"
#include "TimeModelInfo.h"
#include "WeaponModelInfo.h"
#include "VehicleModelInfo.h"
#include "PedModelInfo.h"
#include "MloModelInfo.h"
#include "CompEntityModelInfo.h"

//archetype factories
inline fwArchetypeDynamicFactory<CBaseModelInfo>& CModelInfo::GetBaseModelInfoStore()			{ return GetArchetypeFactory<CBaseModelInfo>( MI_TYPE_BASE ); }
inline fwArchetypeDynamicFactory<CTimeModelInfo>& CModelInfo::GetTimeModelInfoStore()			{ return GetArchetypeFactory<CTimeModelInfo>( MI_TYPE_TIME ); }
inline fwArchetypeDynamicFactory<CMloModelInfo>& CModelInfo::GetMloModelInfoStore()					{ return GetArchetypeFactory<CMloModelInfo>( MI_TYPE_MLO ); }
inline fwArchetypeDynamicFactory<CCompEntityModelInfo>& CModelInfo::GetCompEntityModelInfoStore()	{ return GetArchetypeFactory<CCompEntityModelInfo>( MI_TYPE_COMPOSITE ); }

inline fwArchetypeDynamicFactory<CWeaponModelInfo>& CModelInfo::GetWeaponModelInfoStore()			{ return GetArchetypeFactory<CWeaponModelInfo>( MI_TYPE_WEAPON ); }
inline fwArchetypeDynamicFactory<CVehicleModelInfo>& CModelInfo::GetVehicleModelInfoStore()			{ return GetArchetypeFactory<CVehicleModelInfo>( MI_TYPE_VEHICLE ); }
inline fwArchetypeDynamicFactory<CPedModelInfo>& CModelInfo::GetPedModelInfoStore()					{ return GetArchetypeFactory<CPedModelInfo>( MI_TYPE_PED ); }

// extension factories
inline fwArchetypeExtensionFactory<CSpawnPoint>& CModelInfo::GetSpawnPointStore()						{ return Get2dEffectFactory<CSpawnPoint>( EXT_TYPE_SPAWNPOINT ); }
inline fwArchetypeExtensionFactory<CWorldPointAttr>& CModelInfo::GetWorldPointStore()					{ return Get2dEffectFactory<CWorldPointAttr>( EXT_TYPE_WORLDPOINT ); }


#endif // INC_ModelInfo_Factories_H_

