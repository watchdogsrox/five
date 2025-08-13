// ===========================================
// debug/textureviewer/textureviewersearch.cpp
// (c) 2010 RockstarNorth
// ===========================================

#include "debug/textureviewer/textureviewerprivate.h"

#if __BANK

// Rage headers
#include "atl/string.h"
#include "grcore/image.h"
#include "file/packfile.h"
#include "system/memops.h"

#include "rmptfx/ptxfxlist.h"
#include "system/alloca.h"
#include "vfx/ptfx/ptfxmanager.h"

// Framework headers
#include "fwutil/xmacro.h"
#include "fwsys/fileexts.h"
#include "fwscene/stores/txdstore.h"
#include "fwscene/stores/dwdstore.h"
#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/fragmentstore.h"
#include "fwscene/stores/mapdatastore.h"
#include "streaming/streaming.h"
#include "streaming/streamingengine.h"
#include "streaming/packfilemanager.h"

// Game headers
#include "debug/DebugArchetypeProxy.h"
#include "modelinfo/modelinfo.h"
#include "modelinfo/pedmodelinfo.h"
#include "modelinfo/vehiclemodelinfo.h"
#include "streaming/streaming.h"
#include "scene/loader/MapData.h"
#include "peds/rendering/pedvariationstream.h"
#include "peds/ped.h"

#include "debug/textureviewer/textureviewerutil.h"
#include "debug/textureviewer/textureviewersearch.h"

#if DEBUG_TEXTURE_VIEWER_ENTITY_FILTERING
	// yes, some of these are redundant, but i'm including every header file which defines a CEntity-derived class
	#include "renderer/Lights/LightEntity.h"
	#include "vfx/particles/PtFxEntity.h"
	#include "Objects/DummyObject.h"
	#include "Objects/object.h"
	#include "Objects/Door.h"
	#include "scene/entities/compEntity.h"
	#include "scene/Building.h"
	#include "scene/portals/InteriorInst.h"
	#include "scene/portals/PortalInst.h"
	#include "scene/DynamicEntity.h"
	#include "scene/AnimatedBuilding.h"
	#include "scene/Physical.h"
	#include "Vehicles/vehicle.h"
	#include "Vehicles/Automobile.h"
	#include "Vehicles/Heli.h"
	#include "Vehicles/Planes.h"
	#include "Vehicles/Trailer.h"
	#include "Vehicles/Bike.h"
	#include "Vehicles/Boat.h"
	#include "Vehicles/Submarine.h"
	#include "Vehicles/train.h"
	#include "Peds/ped.h"
	#include "pickups/Pickup.h"
	#include "weapons/projectiles/Projectile.h"
#endif // DEBUG_TEXTURE_VIEWER_ENTITY_FILTERING

// class CEntity --- (NO POOL)
// |
// +---class CLightEntity
// |
// +---class CCompEntity
// |
// +---class CPtFxSortedEntity
// |
// +---class CDummyObject
// |
// +---class CBuilding
// |   |
// |   +---class CInteriorInst
// |   |
// |   +---class CPortalInst
// |
// +---class CDynamicEntity --- (NO POOL)
//     |
//     +---class CAnimatedBuilding
//     |
//     +---class CPhysical --- (NO POOL)
//         |
//         +---class CVehicle
//         |   |
//         |   +---class CAutomobile
//         |   |   |
//         |   |   +---class CRotaryWingAircraft
//         |   |   |   |
//         |   |   |   +---class CHeli
//         |   |   |   |
//         |   |   |   +---class CAutogyro
//         |   |   |
//         |   |   +---class CPlane
//         |   |   |
//         |   |   +---class CTrailer
//         |   |
//         |   +---class CBike
//         |   |
//         |   +---class CBoat
//         |   |
//         |   +---class CSubmarine
//         |   |
//         |   +---class CTrain
//         |
//         +---class CPed
//         |   |
//         |   +---class CCutSceneActor XXXX
//         |   |
//         |   +---class CPlayerSettingsObject
//         |
//         +---class CObject
//             |
//             +---class CDoor
//             |
//             +---class CPickup
//             |
//             +---class CProjectile
//             |
//             +---class CCutSceneObject
//                 |
//                 +---class CCutSceneProp
//                 |
//                 +---class CCutSceneActor
//                 |
//                 +---class CCutSceneVehicle
//
// CEntity
// CEntity CLightEntity
// CEntity CCompEntity
// CEntity CPtFxSortedEntity
// CEntity CDummyObject
// CEntity CBuilding
// CEntity CBuilding CInteriorInst
// CEntity CBuilding CPortalInst
// CEntity CDynamicEntity
// CEntity CDynamicEntity CAnimatedBuilding
// CEntity CDynamicEntity CPhysical
// CEntity CDynamicEntity CPhysical CVehicle
// CEntity CDynamicEntity CPhysical CVehicle CAutomobile
// CEntity CDynamicEntity CPhysical CVehicle CAutomobile CRotaryWingAircraft
// CEntity CDynamicEntity CPhysical CVehicle CAutomobile CRotaryWingAircraft CHeli
// CEntity CDynamicEntity CPhysical CVehicle CAutomobile CRotaryWingAircraft CAutogyro
// CEntity CDynamicEntity CPhysical CVehicle CAutomobile CPlane
// CEntity CDynamicEntity CPhysical CVehicle CAutomobile CTrailer
// CEntity CDynamicEntity CPhysical CVehicle CBike
// CEntity CDynamicEntity CPhysical CVehicle CBoat
// CEntity CDynamicEntity CPhysical CVehicle CSubmarine
// CEntity CDynamicEntity CPhysical CVehicle CTrain
// CEntity CDynamicEntity CPhysical CPed
// CEntity CDynamicEntity CPhysical CPed CCutSceneActor (xxxx)
// CEntity CDynamicEntity CPhysical CPed CPlayerSettingsObject
// CEntity CDynamicEntity CPhysical CObject
// CEntity CDynamicEntity CPhysical CObject CDoor
// CEntity CDynamicEntity CPhysical CObject CPickup
// CEntity CDynamicEntity CPhysical CObject CProjectile
// CEntity CDynamicEntity CPhysical CObject CCutSceneObject
// CEntity CDynamicEntity CPhysical CObject CCutSceneObject CCutSceneProp
// CEntity CDynamicEntity CPhysical CObject CCutSceneObject CCutSceneActor
// CEntity CDynamicEntity CPhysical CObject CCutSceneObject CCutSceneVehicle

// CEntity
// CLightEntity
// CCompEntity
// CPtFxSortedEntity
// CDummyObject
// CBuilding
// CInteriorInst
// CPortalInst
// CDynamicEntity
// CAnimatedBuilding
// CPhysical
// CVehicle
// CAutomobile
// CRotaryWingAircraft
// CHeli
// CAutogyro
// CPlane
// CTrailer
// CBike
// CBoat
// CSubmarine
// CTrain
// CPed
// CCutSceneActor (xxxx)
// CPlayerSettingsObject
// CObject
// CDoor
// CPickup
// CProjectile
// CCutSceneObject
// CCutSceneProp
// CCutSceneActor
// CCutSceneVehicle

DEBUG_TEXTURE_VIEWER_OPTIMISATIONS()

// ================================================================================================

template <typename T> __forceinline T* _const_cast(const T* t) { return const_cast<T*>(t); }

__COMMENT(static) const char* CAssetRef::GetAssetTypeName(eAssetType assetType, bool bAbbrev)
{
	switch (assetType)
	{
	case AST_None          : return "";
	case AST_TxdStore      : return bAbbrev ? "Txd"  : "TxdStore";
	case AST_DwdStore      : return bAbbrev ? "Dwd"  : "DwdStore";
	case AST_DrawableStore : return bAbbrev ? "Draw" : "DrawableStore";
	case AST_FragmentStore : return bAbbrev ? "Frag" : "FragmentStore";
	case AST_ParticleStore : return bAbbrev ? "Part" : "ParticleStore";
	}

	return "";
}

const char* CAssetRef::GetAssetName() const
{
	if (m_assetName.length() > 0) // prefer m_assetName ..
	{
		return m_assetName.c_str();
	}

	if (m_assetIndex_ != INDEX_NONE) // .. else get name from index
	{
		switch (m_assetType)
		{
		case AST_None          : break;
		case AST_TxdStore      : return g_TxdStore     .GetName(m_assetIndex_);
		case AST_DwdStore      : return g_DwdStore     .GetName(m_assetIndex_);
		case AST_DrawableStore : return g_DrawableStore.GetName(m_assetIndex_);
		case AST_FragmentStore : return g_FragmentStore.GetName(m_assetIndex_);
		case AST_ParticleStore : return g_ParticleStore.GetName(m_assetIndex_);
		}
	}

	return "";
}

eAssetType CAssetRef::GetAssetType() const
{
	return m_assetType;
}

strLocalIndex CAssetRef::GetAssetIndex() const
{
	if (m_assetName.length() > 0) // prefer to get index from name ..
	{
		switch (m_assetType)
		{
		case AST_None          : break;
		case AST_TxdStore      : return g_TxdStore     .FindSlot(m_assetName.c_str());
		case AST_DwdStore      : return g_DwdStore     .FindSlot(m_assetName.c_str());
		case AST_DrawableStore : return g_DrawableStore.FindSlot(m_assetName.c_str());
		case AST_FragmentStore : return g_FragmentStore.FindSlot(m_assetName.c_str());
		case AST_ParticleStore : return g_ParticleStore.FindSlot(m_assetName.c_str());
		}
	}

	return m_assetIndex_; // .. else return index directly
}

int CAssetRef::GetSortKey(int assetEntry) const
{
	return (((int)m_assetType) << 20) | (GetAssetIndex().Get() << 5) | (assetEntry & 0x31);
}

// e.g. "Txd:2331"
// e.g. "Dwd:722_5*"
// max length should be 12, e.g. "Frag:1113_8*"
atString CAssetRef::GetString(int assetEntry) const
{
	const strLocalIndex assetIndex = GetAssetIndex(); // might have changed
	atString result;

	if (assetIndex != m_assetIndex_) // asset index changed? wtf
	{
		result += "!??";
	}

	result += atVarString("%s:%d", GetAssetTypeName(m_assetType, true), assetIndex.Get() );

	if (assetEntry != INDEX_NONE)
	{
		result += atVarString("_%d", assetEntry);
	}

	if (!IsInStreamingImage(m_assetType, assetIndex))
	{
		result += "*";
	}

	return result;
}

atString CAssetRef::GetDesc(int assetEntry) const
{
	if (!IsNULL())
	{
		return atVarString("%s(%s)", GetString(assetEntry).c_str(), GetAssetName());
	}

	return atVarString("%s:NONE", GetAssetTypeName(m_assetType, true));
}

int CAssetRef::GetRefCount() const
{
	const strLocalIndex assetIndex = strLocalIndex(GetAssetIndex());

	if (IsValidAssetSlot(m_assetType, assetIndex))
	{
		switch (m_assetType)
		{
		case AST_None          : break;
		case AST_TxdStore      : { return g_TxdStore     .GetNumRefs(assetIndex); } // same as fwTxd::GetRefCount
		case AST_DwdStore      : { return g_DwdStore     .GetNumRefs(assetIndex); } // same as   Dwd::GetRefCount
		case AST_DrawableStore : { return g_DrawableStore.GetNumRefs(assetIndex); }
		case AST_FragmentStore : { return g_FragmentStore.GetNumRefs(assetIndex); }
		case AST_ParticleStore : { return g_ParticleStore.GetNumRefs(assetIndex); }
		}
	}

	return 0;
}

void CAssetRef::AddRef(const CAssetRefInterface& ari) const
{
	if (!IsNULL())
	{
		if (ari.m_verbose)
		{
			Displayf("CAssetRef: %s <- AddRef", GetDesc().c_str());
		}

		const strLocalIndex assetIndex = strLocalIndex(GetAssetIndex());

		switch (m_assetType)
		{
		case AST_None          : break;
		case AST_TxdStore      : g_TxdStore     .AddRef(assetIndex, REF_OTHER); break;
		case AST_DwdStore      : g_DwdStore     .AddRef(assetIndex, REF_OTHER); break;
		case AST_DrawableStore : g_DrawableStore.AddRef(assetIndex, REF_OTHER); break;
		case AST_FragmentStore : g_FragmentStore.AddRef(assetIndex, REF_OTHER); break;
		case AST_ParticleStore : g_ParticleStore.AddRef(assetIndex, REF_OTHER); break;
		}

		ari.m_assetRefs.PushAndGrow(*this);
	}
}

void CAssetRef::RemoveRefWithoutDelete(bool bVerbose) const
{
	if (!IsNULL())
	{
		if (bVerbose)
		{
			Displayf("CAssetRef: %s <- RemoveRefWithoutDelete", GetDesc().c_str());
		}

		const strLocalIndex assetIndex = strLocalIndex(GetAssetIndex()); // might have changed

		switch (m_assetType)
		{
		case AST_None          : break;
		case AST_TxdStore      : g_TxdStore     .RemoveRefWithoutDelete(assetIndex, REF_OTHER); break;
		case AST_DwdStore      : g_DwdStore     .RemoveRefWithoutDelete(assetIndex, REF_OTHER); break;
		case AST_DrawableStore : g_DrawableStore.RemoveRefWithoutDelete(assetIndex, REF_OTHER); break;
		case AST_FragmentStore : g_FragmentStore.RemoveRefWithoutDelete(assetIndex, REF_OTHER); break;
		case AST_ParticleStore : g_ParticleStore.RemoveRefWithoutDelete(assetIndex, REF_OTHER); break;
		}
	}
}

__COMMENT(static) bool CAssetRef::IsValidAssetSlot(eAssetType assetType, strLocalIndex assetIndex)
{
	if (assetIndex != INDEX_NONE)
	{
		switch (assetType)
		{
		case AST_None          : break;
		case AST_TxdStore      : return g_TxdStore     .IsValidSlot(assetIndex);
		case AST_DwdStore      : return g_DwdStore     .IsValidSlot(assetIndex);
		case AST_DrawableStore : return g_DrawableStore.IsValidSlot(assetIndex);
		case AST_FragmentStore : return g_FragmentStore.IsValidSlot(assetIndex);
		case AST_ParticleStore : return g_ParticleStore.IsValidSlot(assetIndex);
		}
	}

	return false;
}

__COMMENT(static) bool CAssetRef::IsInStreamingImage(eAssetType assetType, strLocalIndex assetIndex)
{
	if (IsValidAssetSlot(assetType, assetIndex))
	{
		switch (assetType)
		{
		case AST_None          : break;
		case AST_TxdStore      : return g_TxdStore     .IsObjectInImage(assetIndex);
		case AST_DwdStore      : return g_DwdStore     .IsObjectInImage(assetIndex);
		case AST_DrawableStore : return g_DrawableStore.IsObjectInImage(assetIndex);
		case AST_FragmentStore : return g_FragmentStore.IsObjectInImage(assetIndex);
		case AST_ParticleStore : return g_ParticleStore.IsObjectInImage(assetIndex);
		}
	}

	return false;
}

__COMMENT(static) strIndex CAssetRef::GetStreamingIndex(eAssetType assetType, strLocalIndex assetIndex)
{
	if (IsValidAssetSlot(assetType, assetIndex))
	{
		switch (assetType)
		{
		case AST_None          : break;
		case AST_TxdStore      : return g_TxdStore     .GetStreamingIndex(assetIndex);
		case AST_DwdStore      : return g_DwdStore     .GetStreamingIndex(assetIndex);
		case AST_DrawableStore : return g_DrawableStore.GetStreamingIndex(assetIndex);
		case AST_FragmentStore : return g_FragmentStore.GetStreamingIndex(assetIndex);
		case AST_ParticleStore : return g_ParticleStore.GetStreamingIndex(assetIndex);
		}
	}

	return strIndex(strIndex::INVALID_INDEX);
}

__COMMENT(static) int CAssetRef::GetParentTxdIndex(eAssetType assetType, strLocalIndex assetIndex)
{
	if (IsValidAssetSlot(assetType, assetIndex))
	{
		switch (assetType)
		{
		case AST_None          : break;
		case AST_TxdStore      : return g_TxdStore     .GetParentTxdSlot   (assetIndex).Get();
		case AST_DwdStore      : return g_DwdStore     .GetParentTxdForSlot(assetIndex).Get();
		case AST_DrawableStore : return g_DrawableStore.GetParentTxdForSlot(assetIndex).Get();
		case AST_FragmentStore : return g_FragmentStore.GetParentTxdForSlot(assetIndex).Get();
		case AST_ParticleStore : break;
		}
	}

	return INDEX_NONE;
}

__COMMENT(static) const char* CAssetRef::GetRPFName(eAssetType assetType, strLocalIndex assetIndex)
{
	if (IsInStreamingImage(assetType, assetIndex))
	{
		const strStreamingInfo* streamingInfo = strStreamingEngine::GetInfo().GetStreamingInfo(strIndex(GetStreamingIndex(assetType, assetIndex)));
		const strStreamingFile* streamingFile = strPackfileManager::GetImageFileFromHandle(streamingInfo->GetHandle());

		if (streamingFile)
		{
			return streamingFile->m_packfile->GetDebugName();
		}
	}

	return "";
}

__COMMENT(static) const char* CAssetRef::GetRPFPathName(eAssetType assetType, strLocalIndex assetIndex)
{
	if (IsInStreamingImage(assetType, assetIndex))
	{
		const strStreamingInfo* streamingInfo = strStreamingEngine::GetInfo().GetStreamingInfo(strIndex(GetStreamingIndex(assetType, assetIndex)));
		const strStreamingFile* streamingFile = strPackfileManager::GetImageFileFromHandle(streamingInfo->GetHandle());

		if (streamingFile)
		{
			const char* name = streamingFile->m_name.GetCStr();

			// skip prefix
			{
				const char* prefix = "platform:/";
				static int prefixLen = istrlen(prefix);

				if (strnicmp(name, prefix, prefixLen) == 0)
				{
					name += prefixLen;
				}
			}

			return name;
		}
	}

	return "";
}

bool CAssetRef::IsAssetLoaded() const
{
	switch (m_assetType)
	{
	case AST_None          : break;
	case AST_TxdStore      : return g_TxdStore     .GetSafeFromIndex(m_assetIndex_) != NULL;
	case AST_DwdStore      : return g_DwdStore     .GetSafeFromIndex(m_assetIndex_) != NULL;
	case AST_DrawableStore : return g_DrawableStore.GetSafeFromIndex(m_assetIndex_) != NULL;
	case AST_FragmentStore : return g_FragmentStore.GetSafeFromIndex(m_assetIndex_) != NULL;
	case AST_ParticleStore : return g_ParticleStore.GetSafeFromIndex(m_assetIndex_) != NULL;
	}

	return false;
}

int CAssetRef::GetStreamingAssetSize(bool bVirtual) const
{
	if (IsInStreamingImage())
	{
		strIndex index = strIndex(GetStreamingIndex());
		const strStreamingInfo* streamingInfo = strStreamingEngine::GetInfo().GetStreamingInfo(index);
		return bVirtual ? streamingInfo->ComputeVirtualSize(index, true) : streamingInfo->ComputePhysicalSize(index, true);
	}

	return 0;
}

__COMMENT(static) void CAssetRef::Stream_TxdStore(int assetIndex, bool* bStreamed)
{
	g_TxdStore.StreamingRequest(strLocalIndex(assetIndex), STRFLAG_DONTDELETE|STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
	CStreaming::LoadAllRequestedObjects(true);
	g_TxdStore.ClearRequiredFlag(assetIndex, STRFLAG_DONTDELETE);
	if (bStreamed && g_TxdStore.GetSafeFromIndex(strLocalIndex(assetIndex))) { *bStreamed = true; }
}

__COMMENT(static) void CAssetRef::Stream_DwdStore(int assetIndex, bool* bStreamed)
{
	g_DwdStore.StreamingRequest(strLocalIndex(assetIndex), STRFLAG_DONTDELETE|STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
	CStreaming::LoadAllRequestedObjects(true);
	g_DwdStore.ClearRequiredFlag(assetIndex, STRFLAG_DONTDELETE);
	if (bStreamed && g_DwdStore.GetSafeFromIndex(strLocalIndex(assetIndex))) { *bStreamed = true; }
}

__COMMENT(static) void CAssetRef::Stream_DrawableStore(int assetIndex, bool* bStreamed)
{
	g_DrawableStore.StreamingRequest(strLocalIndex(assetIndex), STRFLAG_DONTDELETE|STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
	CStreaming::LoadAllRequestedObjects(true);
	g_DrawableStore.ClearRequiredFlag(assetIndex, STRFLAG_DONTDELETE);
	if (bStreamed && g_DrawableStore.GetSafeFromIndex(strLocalIndex(assetIndex))) { *bStreamed = true; }
}

__COMMENT(static) void CAssetRef::Stream_FragmentStore(int assetIndex, bool* bStreamed)
{
	g_FragmentStore.StreamingRequest(strLocalIndex(assetIndex), STRFLAG_DONTDELETE|STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
	CStreaming::LoadAllRequestedObjects(true);
	g_FragmentStore.ClearRequiredFlag(assetIndex, STRFLAG_DONTDELETE);
	if (bStreamed && g_FragmentStore.GetSafeFromIndex(strLocalIndex(assetIndex))) { *bStreamed = true; }
}

__COMMENT(static) void CAssetRef::Stream_ParticleStore(int assetIndex, bool* bStreamed)
{
	g_ParticleStore.StreamingRequest(strLocalIndex(assetIndex), STRFLAG_DONTDELETE|STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
	CStreaming::LoadAllRequestedObjects(true);
	g_ParticleStore.ClearRequiredFlag(assetIndex, STRFLAG_DONTDELETE);
	if (bStreamed && g_ParticleStore.GetSafeFromIndex(strLocalIndex(assetIndex))) { *bStreamed = true; }
}

// ================================================================================================

CAssetRefInterface::CAssetRefInterface(atArray<CAssetRef>& assetRefs, bool bVerbose) : m_assetRefs(assetRefs), m_verbose(bVerbose)
{
}

// ================================================================================================

CTxdRef::CTxdRef(eAssetType assetType, int assetIndex, int assetEntry, const char* info) : CAssetRef(assetType, assetIndex)
	, m_assetEntry(assetEntry)
	, m_info      (info      )
{
	Assert(assetEntry < 100); // for some reason i've been hitting dwd entries that are 255, which is wrong ..
	Assert(assetType != AST_DwdStore || assetEntry != INDEX_NONE);
}

int CTxdRef::GetSortKey() const
{
	return CAssetRef::GetSortKey(m_assetEntry);
}

atString CTxdRef::GetString() const
{
	return CAssetRef::GetString(m_assetEntry);
}

atString CTxdRef::GetDesc() const
{
	atString desc = CAssetRef::GetDesc(m_assetEntry);

	if (m_info.length() > 0)
	{
		desc += " ";
		desc += m_info;
	}

	return desc;
}

static const fwTxd* _GetTxd_TxdStore(const CTxdRef& ref)
{
	Assert(ref.GetAssetType() == AST_TxdStore);
	Assert(ref.m_assetEntry == INDEX_NONE);

	// this was causing dummy txds to look like their parents .. not so good
	//return g_TxdStore.GetSafeFromIndex(ref.GetAssetIndex());

	const strLocalIndex i = strLocalIndex(ref.GetAssetIndex());

	if (g_TxdStore.IsValidSlot(i))
	{
		return g_TxdStore.Get(i);
	}

	return NULL;
}

static const fwTxd* _GetTxd_DwdStore(const CTxdRef& ref)
{
	Assert(ref.GetAssetType() == AST_DwdStore);
	Assert(ref.m_assetEntry != INDEX_NONE);

	const Dwd* dwd = g_DwdStore.GetSafeFromIndex(strLocalIndex(ref.GetAssetIndex()));

	if (dwd)
	{
		const Drawable* drawable = dwd->GetEntry(ref.m_assetEntry);

		if (drawable)
		{
			return drawable->GetTexDictSafe();
		}
	}

	return NULL;
}

static const fwTxd* _GetTxd_DrawableStore(const CTxdRef& ref)
{
	Assert(ref.GetAssetType() == AST_DrawableStore);
	Assert(ref.m_assetEntry == INDEX_NONE);

	const Drawable* drawable = g_DrawableStore.GetSafeFromIndex(strLocalIndex(ref.GetAssetIndex()));

	if (drawable)
	{
		return drawable->GetTexDictSafe();
	}

	return NULL;
}

static const fwTxd* _GetTxd_FragmentStore(const CTxdRef& ref)
{
	Assert(ref.GetAssetType() == AST_FragmentStore);

	const Fragment* fragment = g_FragmentStore.GetSafeFromIndex(strLocalIndex(ref.GetAssetIndex()));

	if (fragment)
	{
		const Drawable* drawable = fragment->GetCommonDrawable();
		
		if (ref.m_assetEntry >= 0)
		{
			drawable = fragment->GetExtraDrawable(ref.m_assetEntry);
		}
		else if (ref.m_assetEntry == -2)
		{
			drawable = fragment->GetClothDrawable();
		}

		if (drawable)
		{
			return drawable->GetTexDictSafe();
		}
	}

	return NULL;
}

static const fwTxd* _GetTxd_ParticleStore(const CTxdRef& ref)
{
	Assert(ref.GetAssetType() == AST_ParticleStore);
	Assert(ref.m_assetEntry == INDEX_NONE);

	ptxFxList* particle = g_ParticleStore.GetSafeFromIndex(strLocalIndex(ref.GetAssetIndex()));

	if (particle)
	{
		return particle->GetTextureDictionary();
	}

	return NULL;
}

const fwTxd* CTxdRef::GetTxd() const
{
	switch (m_assetType)
	{
	case AST_None          : break;
	case AST_TxdStore      : return _GetTxd_TxdStore     (*this);
	case AST_DwdStore      : return _GetTxd_DwdStore     (*this);
	case AST_DrawableStore : return _GetTxd_DrawableStore(*this);
	case AST_FragmentStore : return _GetTxd_FragmentStore(*this);
	case AST_ParticleStore : return _GetTxd_ParticleStore(*this);
	}

	return NULL;
}

const fwTxd* CTxdRef::LoadTxd(bool bVerbose, bool* bStreamed)
{
	const fwTxd* txd = GetTxd();

	if (txd)
	{
		if (bVerbose)
		{
			Displayf("CTxdRef::LoadTxd: %s -> already loaded", GetDesc().c_str());
		}

		return txd;
	}
	else
	{
		const strLocalIndex assetIndex = GetAssetIndex(); // might have changed
		const bool bIsInStreamingImage = IsInStreamingImage(m_assetType, assetIndex);

		if (bIsInStreamingImage)
		{
			switch (m_assetType)
			{
			case AST_None          : break;
			case AST_TxdStore      : Stream_TxdStore     (assetIndex.Get(), bStreamed); break;
			case AST_DwdStore      : Stream_DwdStore     (assetIndex.Get(), bStreamed); break;
			case AST_DrawableStore : Stream_DrawableStore(assetIndex.Get(), bStreamed); break;
			case AST_FragmentStore : Stream_FragmentStore(assetIndex.Get(), bStreamed); break;
			case AST_ParticleStore : Stream_ParticleStore(assetIndex.Get(), bStreamed); break;
			}

			txd = GetTxd(); // try again ..

			if (bVerbose)
			{
				Displayf("CTxdRef::LoadTxd: %s -> [STREAMING] -> %s", GetDesc().c_str(), txd != NULL ? "ok" : "failed!");
			}

			return txd;
		}

		// not in streaming image .. maybe it's a file?
		// need to re-test this codepath!
		// try loading from "platform:/textures/"
		if (m_assetName.length() > 0 && m_assetIndex_ == INDEX_NONE)
		{
			const char* name = m_assetName.c_str();

			if (m_assetType == AST_TxdStore)
			{
				const atVarString path("platform:/textures/%s.%s", name, TXD_FILE_EXT);

				if (ASSET.Exists(path.c_str(), NULL))
				{
					const strLocalIndex slot = strLocalIndex(g_TxdStore.AddSlot(name));

					if (slot != INDEX_NONE)
					{
						//pgRscBuilder::sm_bAssertSuppressionSecret = true;
						const bool bLoaded = g_TxdStore.LoadFile(slot, path.c_str());
						//pgRscBuilder::sm_bAssertSuppressionSecret = false;

						if (bLoaded)
						{
							m_assetIndex_ = slot;
							txd = GetTxd();

							if (bVerbose)
							{
								Displayf("CTxdRef::LoadTxd: %s -> [TXDFILE] -> %s", GetDesc().c_str(), txd != NULL ? "ok" : "failed!");
							}

							return txd;
						}
					}
				}
			}
		}
	}

	if (bVerbose)
	{
		Displayf("CTxdRef::LoadTxd: %s -> failed!", GetDesc().c_str());
	}

	return NULL;
}

// ================================================================================================

CTextureRef::CTextureRef(const CTxdRef& ref, int txdEntry, const grcTexture* tempTexture) : CTxdRef(ref)
	, m_txdEntry   (txdEntry   )
	, m_tempTexture(tempTexture)
{}

CTextureRef::CTextureRef(eAssetType assetType, int assetIndex, int assetEntry, const char* info, int txdEntry, const grcTexture* tempTexture) : CTxdRef(assetType, assetIndex, assetEntry, info)
	, m_txdEntry   (txdEntry   )
	, m_tempTexture(tempTexture)
{}

// ================================================================================================

CAssetSearchParams::CAssetSearchParams()
{
	sysMemSet(this, 0, sizeof(*this));

	m_exactNameMatch                  = false;
	m_showEmptyTxds                   = false; // ?
	m_showStreamable                  = true;
	m_selectModelUseShowTxdFlags      = false; // maybe get rid of this eventually, seems to be unnecessary
	m_findDependenciesWhilePopulating = false;

#if DEBUG_TEXTURE_VIEWER_ENTITY_FILTERING
	m_entityLODFilter = ELODSP_ALL;

	for (int i = 0; i < ENTITY_TYPE_TOTAL; i++)
	{
		m_entityTypeFlags[i] = true;
	}
#endif // DEBUG_TEXTURE_VIEWER_ENTITY_FILTERING

	m_modelInfoShowProps            = MISP_ALL;
	m_modelInfoShowLoadedModelsOnly = true;

	for (int i = 0; i < _MI_TYPE_COUNT; i++)
	{
		m_modelInfoTypeFlags[i] = true;
	}

	m_scanTxdStore      = true;
	m_scanDwdStore      = true;
	m_scanDrawableStore = true;
	m_scanFragmentStore = true;
	m_scanParticleStore = true;

	m_createTextureCRCs = false;
}

// ================================================================================================

#if __ASSERT
static int _GetNumTxds(const Dwd* dwd, bool bCountEmptyTxds = true)
{
	int numTxds = 0;

	for (int i = 0; i < dwd->GetCount(); i++)
	{
		const Drawable* drawable = dwd->GetEntry(i);

		if (drawable)
		{
			const fwTxd* txd = drawable->GetTexDictSafe();

			if (txd)
			{
				if (bCountEmptyTxds || txd->GetCount() > 0)
				{
					numTxds++;
				}
			}
		}
	}

	return numTxds;
}
#endif // __ASSERT

static void _GetAssociatedTxds_ModelInfo_Push(atArray<CTxdRef>& refs, const CAssetSearchParams* asp, const CTxdRef& ref)
{
	if (asp)
	{
		const fwTxd* txd = ref.GetTxd();

		if (txd)
		{
			if (txd->GetCount() > 0 || asp->m_showEmptyTxds)
			{
				// txd is non-empty, fall through and push the ref
			}
			else
			{
				return;
			}
		}
		else // txd is NULL (not loaded)
		{
			if (asp->m_showStreamable && ref.IsInStreamingImage())
			{
				CTxdRef temp = ref;
				temp.m_info += "?";
				refs.PushAndGrow(temp);
			}

			return;
		}
	}

	if (refs.Find(ref) == -1)
	{
		refs.PushAndGrow(ref);
	}
}

#if DEBUG_TEXTURE_VIEWER_SHADER_EDIT_INTEGRATION_OLD

static atMap<const CBaseModelInfo*,bool> s_shaderEditHackModelInfoMap;
static int                               s_shaderEditHackTxdSlot = INDEX_NONE;

void SetShaderEditHackModelInfo(const CBaseModelInfo* modelInfo)
{
	s_shaderEditHackModelInfoMap[modelInfo] = true;
}

void SetShaderEditHackTxdSlot(int slot)
{
	s_shaderEditHackTxdSlot = slot;
}

#endif // DEBUG_TEXTURE_VIEWER_SHADER_EDIT_INTEGRATION_OLD

// for debugging, turn these on and off to control which codepaths are active
#define _GetAssociatedTxds_ModelInfo_TxdStore       (1)
#define _GetAssociatedTxds_ModelInfo_DwdStore       (1)
#define _GetAssociatedTxds_ModelInfo_DrawableStore  (1)
#define _GetAssociatedTxds_ModelInfo_FragmentStore  (1)
#define _GetAssociatedTxds_ModelInfo_ParticleStore  (1)
#define _GetAssociatedTxds_ModelInfo_PedStreaming   (1)
#define _GetAssociatedTxds_ModelInfo_PedHD          (1) // PED_HD_TXD
#define _GetAssociatedTxds_ModelInfo_PedComp        (1)
#define _GetAssociatedTxds_ModelInfo_PedProp        (1)
#define _GetAssociatedTxds_ModelInfo_VehicleHD      (1) // VEH_HD_TXD, VEH_HD_FRAG
#define _GetAssociatedTxds_ModelInfo_ShaderEditHack (1)

// silly wrapper, tricks are for kids
template <typename T> static int Archetype_SafeGetDrawDictEntry_IfLoaded(const T* modelInfo);

template <> int Archetype_SafeGetDrawDictEntry_IfLoaded<CDebugArchetypeProxy>(const CDebugArchetypeProxy* modelInfo) { return modelInfo->SafeGetDrawDictEntry_IfLoaded(); }
template <> int Archetype_SafeGetDrawDictEntry_IfLoaded<CBaseModelInfo>(const CBaseModelInfo* modelInfo) { return (modelInfo && modelInfo->GetHasLoaded()) ? modelInfo->SafeGetDrawDictEntry() : -1; }

template <typename T> class ArchetypeTypes {};

template <> class ArchetypeTypes<CDebugArchetypeProxy>
{
public:
	typedef CDebugPedArchetypeProxy PedArchetypeType;
	typedef CDebugVehicleArchetypeProxy VehicleArchetypeType;
};
template <> class ArchetypeTypes<CBaseModelInfo>
{
public:
	typedef CPedModelInfo PedArchetypeType;
	typedef CVehicleModelInfo VehicleArchetypeType;
};

template <typename T> static void GetAssociatedTxds_ModelInfo_internal(atArray<CTxdRef>& refs, const T* modelInfo, const CEntity* entity, const CAssetSearchParams* optional)
{
	if (_GetAssociatedTxds_ModelInfo_TxdStore && modelInfo->GetAssetParentTxdIndex() != INDEX_NONE)
	{
		const strLocalIndex txdIndex = strLocalIndex(modelInfo->GetAssetParentTxdIndex());

		_GetAssociatedTxds_ModelInfo_Push(refs, optional, CTxdRef(AST_TxdStore, txdIndex.Get(), INDEX_NONE, ""));

		if (g_TxdStore.IsValidSlot(txdIndex))
		{
			for (int parentNum = 0, parentTxdIndex = g_TxdStore.GetParentTxdSlot(txdIndex).Get(); parentTxdIndex != INDEX_NONE; parentTxdIndex = g_TxdStore.GetParentTxdSlot(strLocalIndex(parentTxdIndex)).Get())
			{
				_GetAssociatedTxds_ModelInfo_Push(refs, optional, CTxdRef(AST_TxdStore, parentTxdIndex, INDEX_NONE, atVarString("TXDPARENT_%d", parentNum++)));
			}
		}
	}

	if (_GetAssociatedTxds_ModelInfo_DwdStore && modelInfo->SafeGetDrawDictIndex() != INDEX_NONE)
	{
		const strLocalIndex dwdIndex = strLocalIndex(modelInfo->SafeGetDrawDictIndex());
		const int dwdEntry = Archetype_SafeGetDrawDictEntry_IfLoaded(modelInfo);//modelInfo->SafeGetDrawDictEntry_IfLoaded();

		if (dwdEntry != INDEX_NONE &&
			dwdEntry != 255) // TODO -- something if f*&ked up with one of the modelinfo's, dwdEntry is 255 but the drawable type is DT_DRAWABLEDICTIONARY
		{
			_GetAssociatedTxds_ModelInfo_Push(refs, optional, CTxdRef(AST_DwdStore, dwdIndex.Get(), dwdEntry, ""));
		}

		const strLocalIndex dwdParentTxdIndex = strLocalIndex(g_DwdStore.GetParentTxdForSlot(dwdIndex));

		if (dwdParentTxdIndex != INDEX_NONE &&
			dwdParentTxdIndex != modelInfo->GetAssetParentTxdIndex())
		{
			_GetAssociatedTxds_ModelInfo_Push(refs, optional, CTxdRef(AST_TxdStore, dwdParentTxdIndex.Get(), INDEX_NONE, "DWDPARENT"));

			if (g_TxdStore.IsValidSlot(dwdParentTxdIndex))
			{
				for (int parentNum = 0, parentTxdIndex = g_TxdStore.GetParentTxdSlot(dwdParentTxdIndex).Get(); parentTxdIndex != INDEX_NONE; parentTxdIndex = g_TxdStore.GetParentTxdSlot(strLocalIndex(parentTxdIndex)).Get())
				{
					_GetAssociatedTxds_ModelInfo_Push(refs, optional, CTxdRef(AST_TxdStore, parentTxdIndex, INDEX_NONE, atVarString("DWDPARENT_%d", parentNum++)));
				}
			}
		}
	}

	if (_GetAssociatedTxds_ModelInfo_DrawableStore && modelInfo->SafeGetDrawableIndex() != INDEX_NONE)
	{
		const strLocalIndex drawableIndex = strLocalIndex(modelInfo->SafeGetDrawableIndex());

		_GetAssociatedTxds_ModelInfo_Push(refs, optional, CTxdRef(AST_DrawableStore, drawableIndex.Get(), INDEX_NONE, ""));

		const int parentTxdIndex = g_DrawableStore.GetParentTxdForSlot(drawableIndex).Get();

		if (parentTxdIndex != INDEX_NONE &&
			parentTxdIndex != modelInfo->GetAssetParentTxdIndex())
		{
			_GetAssociatedTxds_ModelInfo_Push(refs, optional, CTxdRef(AST_TxdStore, parentTxdIndex, INDEX_NONE, "DRAWPARENT"));
		}
	}

	if (_GetAssociatedTxds_ModelInfo_FragmentStore && modelInfo->SafeGetFragmentIndex() != INDEX_NONE)
	{
		const strLocalIndex fragmentIndex = strLocalIndex(modelInfo->SafeGetFragmentIndex());

		// common drawable
		{
			_GetAssociatedTxds_ModelInfo_Push(refs, optional, CTxdRef(AST_FragmentStore, fragmentIndex.Get(), INDEX_NONE, "COMMON"));
		}

		const Fragment* fragment = g_FragmentStore.GetSafeFromIndex(fragmentIndex);

		if (fragment)
		{
			for (int i = 0; i < fragment->GetNumExtraDrawables(); i++) // extra drawables
			{
				_GetAssociatedTxds_ModelInfo_Push(refs, optional, CTxdRef(AST_FragmentStore, fragmentIndex.Get(), i, atVarString("EXTRA[%s]", fragment->GetExtraDrawableName(i))));
			}
		}
		else
		{
			// [SEARCH ISSUE] fragment not loaded, can't determine if extra drawables are present ..
		}

		const int parentTxdIndex = g_FragmentStore.GetParentTxdForSlot(fragmentIndex).Get();

		if (parentTxdIndex != INDEX_NONE &&
			parentTxdIndex != modelInfo->GetAssetParentTxdIndex())
		{
			_GetAssociatedTxds_ModelInfo_Push(refs, optional, CTxdRef(AST_TxdStore, parentTxdIndex, INDEX_NONE, "FRAGPARENT"));
		}
	}

	if (_GetAssociatedTxds_ModelInfo_ParticleStore && modelInfo->GetPtFxAssetSlot() != INDEX_NONE)
	{
		const int particleIndex = modelInfo->GetPtFxAssetSlot();

		_GetAssociatedTxds_ModelInfo_Push(refs, optional, CTxdRef(AST_ParticleStore, particleIndex, INDEX_NONE, ""));
	}

	if (modelInfo->GetModelType() == MI_TYPE_PED)
	{
		const typename ArchetypeTypes<T>::PedArchetypeType* pedModelInfo = static_cast<const typename ArchetypeTypes<T>::PedArchetypeType*>(modelInfo);

		if (_GetAssociatedTxds_ModelInfo_PedStreaming && entity && pedModelInfo->GetIsStreamedGfx())
		{
			CPedStreamRenderGfx* gfx = ((CPed*)entity)->GetPedDrawHandler().GetPedRenderGfx();
			if (gfx)
			{
				for (int i = 0; i < PV_MAX_COMP; i++)
				{
					const atVarString info("PED[%s]", varSlotNames[i]);
#if RDR_VERSION
					for( u32 txSlot=0; txSlot<PV_NUM_SHADER_TEX_SLOTS; ++txSlot )
#endif // RDR_VERSION
					{
						if (gfx->m_txdIdx[i]RDR_ONLY([txSlot]) != INDEX_NONE)
						{
#if __ASSERT
							const fwTxd* txd = g_TxdStore.GetSafeFromIndex(strLocalIndex(gfx->m_txdIdx[i]RDR_ONLY([txSlot])));

							if (txd)
							{
								Assertf(txd->GetCount() > 0     , "%s \"%s\" txd has %d entries! expected > 0", info.c_str(), modelInfo->GetModelName(), txd->GetCount());
								Assertf(txd->GetParent() == NULL, "%s \"%s\" txd has parent!", info.c_str(), modelInfo->GetModelName());
							}
#endif // __ASSERT
							_GetAssociatedTxds_ModelInfo_Push(refs, optional, CTxdRef(AST_TxdStore, gfx->m_txdIdx[i]RDR_ONLY([txSlot]), INDEX_NONE, info));
						}
					}

					if (gfx->m_dwdIdx[i] != INDEX_NONE)
					{
#if __ASSERT
						const Dwd* dwd = g_DwdStore.GetSafeFromIndex(strLocalIndex(gfx->m_dwdIdx[i]));
						
						if (dwd) // if we hit this assert, maybe just check that there is only 1 entry with a txd in it?
						{
							Assertf(dwd->GetCount() == 1, "%s \"%s\" dwd has %d entries! expected 1", info.c_str(), modelInfo->GetModelName(), dwd->GetCount());
						}
#endif // __ASSERT
						_GetAssociatedTxds_ModelInfo_Push(refs, optional, CTxdRef(AST_DwdStore, gfx->m_dwdIdx[i], 0, info)); // assume 0th entry
					}
				}
			}
			else
			{
				// [SEARCH ISSUE]
			}
		}

		if (_GetAssociatedTxds_ModelInfo_PedHD)
		{
			const int pedHDTxdIndex = pedModelInfo->GetHDTxdIndex();

			if (pedHDTxdIndex != INDEX_NONE)
			{
				_GetAssociatedTxds_ModelInfo_Push(refs, optional, CTxdRef(AST_TxdStore, pedHDTxdIndex, INDEX_NONE, "PED_HD_TXD"));
			}
		}

		if (_GetAssociatedTxds_ModelInfo_PedComp) // ped comp (txd's in dwd->GetParentTxdForSlot, not in drawables themselves)
		{
			const strLocalIndex pedCompDwdIndex = strLocalIndex(pedModelInfo->GetPedComponentFileIndex());

			if (pedCompDwdIndex != INDEX_NONE)
			{
#if __ASSERT
				const Dwd* dwd        = g_DwdStore.GetSafeFromIndex(pedCompDwdIndex);
				const int  dwdNumTxds = dwd == NULL ? 0 : _GetNumTxds(dwd);

				Assertf(dwdNumTxds == 0, "PED_COMP \"%s\" dwd has %d txds, expected only parent txd!", modelInfo->GetModelName(), dwdNumTxds);
#endif // __ASSERT

				const strLocalIndex pedCompTxdIndex = strLocalIndex(g_DwdStore.GetParentTxdForSlot(pedCompDwdIndex));

				if (pedCompTxdIndex != INDEX_NONE)
				{
					_GetAssociatedTxds_ModelInfo_Push(refs, optional, CTxdRef(AST_TxdStore, pedCompTxdIndex.Get(), INDEX_NONE, "PED_COMP"));

					for (int parentNum = 0, parentTxdIndex = g_TxdStore.GetParentTxdSlot(pedCompTxdIndex).Get(); parentTxdIndex != INDEX_NONE; parentTxdIndex = g_TxdStore.GetParentTxdSlot(strLocalIndex(parentTxdIndex)).Get())
					{
						_GetAssociatedTxds_ModelInfo_Push(refs, optional, CTxdRef(AST_TxdStore, parentTxdIndex, INDEX_NONE, atVarString("PED_COMP_%d", parentNum++)));
					}
				}
			}
		}

		if (_GetAssociatedTxds_ModelInfo_PedProp) // ped prop (txd's in dwd->GetParentTxdForSlot, not in drawables themselves)
		{
			const strLocalIndex pedPropDwdIndex = strLocalIndex(pedModelInfo->GetPropsFileIndex());

			if (pedPropDwdIndex != INDEX_NONE)
			{
#if __ASSERT
				const Dwd* dwd        = g_DwdStore.GetSafeFromIndex(pedPropDwdIndex);
				const int  dwdNumTxds = dwd == NULL ? 0 : _GetNumTxds(dwd);

				Assertf(dwdNumTxds == 0, "PED_PROP \"%s\" dwd has %d txds, expected only parent txd!", modelInfo->GetModelName(), dwdNumTxds);
#endif // __ASSERT

				const strLocalIndex pedPropTxdIndex = g_DwdStore.GetParentTxdForSlot(pedPropDwdIndex);

				if (pedPropTxdIndex != INDEX_NONE)
				{
					_GetAssociatedTxds_ModelInfo_Push(refs, optional, CTxdRef(AST_TxdStore, pedPropTxdIndex.Get(), INDEX_NONE, "PED_PROP"));

					for (int parentNum = 0, parentTxdIndex = g_TxdStore.GetParentTxdSlot(pedPropTxdIndex).Get(); parentTxdIndex != INDEX_NONE; parentTxdIndex = g_TxdStore.GetParentTxdSlot(strLocalIndex(parentTxdIndex)).Get())
					{
						_GetAssociatedTxds_ModelInfo_Push(refs, optional, CTxdRef(AST_TxdStore, parentTxdIndex, INDEX_NONE, atVarString("PED_PROP_%d", parentNum++)));
					}
				}
			}
			else if (pedModelInfo->HasStreamedProps())
			{
				const char* propStreamFolder = pedModelInfo->GetPropStreamFolder();
				const CPedPropData &pedPropData = ((CPed*)entity)->GetPedDrawHandler().GetPropData();
				for(int Idx = 0; Idx < MAX_PROPS_PER_PED; ++Idx)
				{
					const CPedPropData::SinglePropData &propData = pedPropData.GetPedPropData(Idx);
					const s32			propIdx  = propData.m_propModelID;
					const s32			texIdx   = propData.m_propTexID;
					const eAnchorPoints	anchorId = propData.m_anchorID;
					if ( propIdx != PED_PROP_NONE && texIdx != PED_PROP_NONE && anchorId != ANCHOR_NONE )
					{
						u32 propFolderHash = 0;
						u32 streamingTxdHash_diff   = 0;
						u32 streamingTxdHash_normal = 0;
						u32 streamingTxdHash_spec   = 0;
						char propIdStr[4]    = {0};
						char textureIdStr[2] = {0};

						propFolderHash = atPartialStringHash(propStreamFolder, propFolderHash);
						propFolderHash = atPartialStringHash("/", propFolderHash);
						propFolderHash = atPartialStringHash(propSlotNames[anchorId], propFolderHash);
						propFolderHash = atPartialStringHash("_", propFolderHash);

						CPedVariationStream::CustomIToA3(propIdx, propIdStr);
						textureIdStr[0] = (char)texIdx + 'a';
						textureIdStr[1] = '\0';
						
						streamingTxdHash_diff   = atPartialStringHash("diff_", propFolderHash);
						streamingTxdHash_diff   = atPartialStringHash(propIdStr, streamingTxdHash_diff);
						streamingTxdHash_diff   = atPartialStringHash("_", streamingTxdHash_diff);
						streamingTxdHash_diff   = atPartialStringHash(textureIdStr, streamingTxdHash_diff);
						streamingTxdHash_diff   = atFinalizeHash(streamingTxdHash_diff);
						streamingTxdHash_normal = atPartialStringHash("normal_", propFolderHash);
						streamingTxdHash_normal = atPartialStringHash(propIdStr, streamingTxdHash_diff);
						streamingTxdHash_normal = atFinalizeHash(streamingTxdHash_diff);
						streamingTxdHash_spec   = atPartialStringHash("spec_", propFolderHash);
						streamingTxdHash_spec   = atPartialStringHash(propIdStr, streamingTxdHash_diff);
						streamingTxdHash_spec   = atFinalizeHash(streamingTxdHash_diff);
						
						strLocalIndex txdSlotId_diff   = g_TxdStore.FindSlot(streamingTxdHash_diff);
						strLocalIndex txdSlotId_normal = g_TxdStore.FindSlot(streamingTxdHash_normal);
						strLocalIndex txdSlotId_spec   = g_TxdStore.FindSlot(streamingTxdHash_spec);

						if (txdSlotId_diff != INDEX_NONE)
						{
							_GetAssociatedTxds_ModelInfo_Push(refs, optional, CTxdRef(AST_TxdStore, txdSlotId_diff.Get(), INDEX_NONE, atVarString("PED_PROP[%s]_diff", propSlotNamesClean[anchorId])));
						}
						if (txdSlotId_normal != INDEX_NONE)
						{
							_GetAssociatedTxds_ModelInfo_Push(refs, optional, CTxdRef(AST_TxdStore, txdSlotId_normal.Get(), INDEX_NONE, atVarString("PED_PROP[%s]_normal", propSlotNamesClean[anchorId])));
						}
						if (txdSlotId_spec != INDEX_NONE)
						{
							_GetAssociatedTxds_ModelInfo_Push(refs, optional, CTxdRef(AST_TxdStore, txdSlotId_spec.Get(), INDEX_NONE, atVarString("PED_PROP[%s]_spec", propSlotNamesClean[anchorId])));
						}
					}
				}
			}
		}
	}
	else if (modelInfo->GetModelType() == MI_TYPE_VEHICLE)
	{
		if (_GetAssociatedTxds_ModelInfo_VehicleHD)
		{
			const typename ArchetypeTypes<T>::VehicleArchetypeType* vehModelInfo = static_cast<const typename ArchetypeTypes<T>::VehicleArchetypeType*>(modelInfo);

			const int vehHDTxdIndex      = vehModelInfo->GetHDTxdIndex();
			const int vehHDFragmentIndex = vehModelInfo->GetHDFragmentIndex();

			if (vehHDTxdIndex != INDEX_NONE)
			{
				_GetAssociatedTxds_ModelInfo_Push(refs, optional, CTxdRef(AST_TxdStore, vehHDTxdIndex, INDEX_NONE, "VEH_HD_TXD"));
			}

			if (vehHDFragmentIndex != INDEX_NONE)
			{
				_GetAssociatedTxds_ModelInfo_Push(refs, optional, CTxdRef(AST_FragmentStore, vehHDFragmentIndex, INDEX_NONE, "VEH_HD_FRAG"));
			}
		}
	}

#if DEBUG_TEXTURE_VIEWER_SHADER_EDIT_INTEGRATION_OLD
	if (_GetAssociatedTxds_ModelInfo_ShaderEditHack)
	{
		if (s_shaderEditHackTxdSlot != INDEX_NONE && s_shaderEditHackModelInfoMap.Access(modelInfo))
		{
			_GetAssociatedTxds_ModelInfo_Push(refs, optional, CTxdRef(AST_TxdStore, s_shaderEditHackTxdSlot, INDEX_NONE, ""));
		}
	}
#endif // DEBUG_TEXTURE_VIEWER_SHADER_EDIT_INTEGRATION_OLD
}

void GetAssociatedTxds_ModelInfo(atArray<CTxdRef>& refs, const CDebugArchetypeProxy* modelInfo, const CEntity* entity, const CAssetSearchParams* optional)
{
	GetAssociatedTxds_ModelInfo_internal(refs, modelInfo, entity, optional);
}

void GetAssociatedTxds_ModelInfo(atArray<CTxdRef>& refs, const CBaseModelInfo* modelInfo, const CEntity* entity, const CAssetSearchParams* optional)
{
	GetAssociatedTxds_ModelInfo_internal(refs, modelInfo, entity, optional);
}

/*
* TxdStore[index] - If GetSafeFromIndex returns non-NULL, then either it's valid or it's empty
(if GetCount returns zero). If GetSafeFromIndex returns NULL, then either it's an invalid index in
which case it will never be valid (unless it gets loaded as a file later, but don't worry about
that), or perhaps it's just not streamed in yet. In this case, we can call IsInStreamingImage and
if that returns true then we might be able to stream it in (in which case it would become valid,
but possibly empty) or the streaming may fail in which case we can assume it's invalid. Txds have
a parent (a fwTxd*) which may or may not be NULL .. I'm assuming that if the txd is loaded then
the parents (and parents of parents) will also be loaded, but I'm not 100% sure (and I assume
if you stream in a txd manually then it will stream in all the parents as well, and if you addref
a txd it will addref the parents?) Either way, you can't get the parent unless the txd is loaded
unfortunately, but you can iterate over all the txd's in g_TxdStore and compare them to the parent
txd pointer to find the index (slot) of the parent, which is not very efficient but it works. Also
I'm not sure if a txd is in a streaming image that means that the parents are in the same
streaming image .. that would seem to make sense but who knows.

* DwdStore[index] - If GetSafeFromIndex returns non-NULL, then it's valid and should contain 0 or
more drawables. If GetSafeFromIndex returns NULL, then either it's an invalid index in
which case it will never be valid (unless it gets loaded as a file later, but don't worry about
that), or perhaps it's just not streamed in yet. In this case, we can call IsInStreamingImage and
if that returns true then we might be able to stream it in (in which case it would become valid,
but possibly empty) or the streaming may fail in which case we can assume it's invalid. If a dwd
is valid then each of the entries (Drawable's) may or may not contain a non-NULL txd. If they
contain a non-NULL txd it may or may not be empty (have zero textures in it). But in either case
the txd's in the dwd cannot be streamed in because the resource is the dwd itself. However, dwd's
also have a parent txd index (slot), which is accessible through g_DwdStore regardless of whether
the dwd is loaded or not. I'm not sure if the parent txd is always loaded (or addref'd) when the
dwd is loaded. Note that this is the only case (that I'm aware of) where an asset in one asset
store (g_DwdStore) is associated with or depends on an asset in a different asset store
(g_TxdStore), so this requires careful handling.

* DrawableStore[index] - Similar to DwdStore, execpt that instead of getting a dictionary of
drawables you get a single Drawable, which may or may not have a non-NULL txd which in turn
may or may not have textures in it. There is no parent txd.

* FragmentStore[index] - Kind of similar to DwdStore, a fragment has a single "common drawable"
and 0 or more "extra drawables", all these drawables behave exactly like the drawables you get
from DrawableStore. The extra drawables each have an associated name (string), but I haven't seen
any of these in game yet so I don't know what they are.

* ParticleStore[index] - Actually, this is a store of PtxFxList objects, each of which contains
a txd and a dwd object (although they are typedef'd to something particle-specific, they are
physically identical to fwTxd and Dwd types). There are only about 8 of these particle assets
currently, and the total number of textures in their txds is 109 so this is a much smaller store
than the others. The dwd's contain drawables (I forget how many) but the drawables in the dwd's
do not contain any txd's in their shader groups. So when a CTxdRef has type AST_ParticleStore, it
is assumed to refer to the txd in the particle asset, not the dwd - if txd's start appearing in
the particle dwd's then we could use the m_assetEntry to index into the appropriate dwd entry but
currently this doesn't seem necessary.

* PEDS - if you have a CBaseModeInfo, you can get the model type and if it's a PED then you can
cast the modelinfo to a CPedModelInfo and get additional crap from that. In particular, if you
also have a CEntity along with the modelinfo, and ped->GetIsStreamedGfx() returns true then you
can get the CPedStreamRenderGfx by casting the entity to a CPed and calling GetPedDrawHandler and
GetPedRenderGfx. From there you can get PV_MAX_COMP txd's and dwd's (the dwd's will each have a
single drawable with a single txd in them, I don't know why they store them as dwd's, seems kind
of silly ..) Also, even without the entity, you can call GetPedComponentFileIndex on the ped to
get a dwd slot which (if it's not -1) can be used to get a parent txd slot via g_DwdStore which
may contain textures. The dwd itself should not contain txd's in any of the drawables however
(I'm not 100% sure if the dwd itself is even necessarily valid or contains any drawables, since
getting the parent txd is done through g_DwdStore itself). Similarly, you can also call
GetPropsFileIndex on the ped to get a dwd index which can be used to get a parent txd index
containing textures.

* VEHICLES - If the have a CBaseModelInfo, you can get the model type and if it's a VEHICLE then
you can cast the modelinfo to a CVehicleModelInfo and from there you can get the HD txd index
and HD fragment index .. supposedly these are "high detail" assets, but I don't think they are
being used yet. I have asserts which will fire if they are not -1, so hopefully when these HD
things are implemented I'll know so I can handle them appropriately.

[UPDATE] - I found a vehicle that has an HD txd (and an HD fragment, but the fragment does not
contain any textures). The model is "sultan2_lo" and it's HD txd is "sultan2_hi".

* Searching - There are three main ways to "search" for assets and populate the list (in addition
to clicking and selecting), using the three top buttons "Search Models", "Search Txds" and "Search
Textures". Searching for models (actually CBaseModelInfo's) simply iterates over the list of
modelinfo's and adds a list entry for every modelinfo which passes the search criteria. Selecting
one of the modelinfo list entries will "expand" it by repopulating the list with the associated
txd's for that modelinfo. Note that "associated txd's" is a rather complex bit of logic, not quite
as simple as it sounds. Also note that clicking with the mouse to populate the list with modelinfo
entries provides not just modelinfo's but CEntity's as well, which can provide more associated
txd's when selected in certain cases (e.g. peds).

* What are "associated txd's"? Well .. pretty much any txd which I think might be involved in
rendering the modelinfo. The CBaseModelInfo class has an associated txd index (into the TxdStore),
as well as an associated drawable .. the drawable can come from either the DrawableStore, or
the FragmentStore, or the DwdStore (if it comes from the DwdStore then there is also a specific
entry index into the dwd, so there is always only one "drawable" associated with the modelinfo).
The drawable (specifically rmcDrawable) has a shadergroup which sometimes has a txd in it, but not
always. CBaseModelInfo also has an associated ptxFxList index but so far these seem to always be
-1. In addition to these direct asset references, there are some indirect ones .. any txd in the
TxdStore can have a parent txd pointer (which will also be in the TxdStore if it's not NULL), and
parent txd's can have parents themselves etc. Note that txds in rmcDrawable's shadergroups do NOT
have parents. However, the DrawableStore, FragmentStore and DwdStore themselves refer to a parent
txd index (in the TxdStore) for each asset slot, so in effect any of the assets except for
particle (ptxFxList) assets can potentially have a parent txd. I'm pretty sure that parent txd's
that are parents of assets that are not themselves txd's (e.g. a parent txd of a dwd asset) will
not have parents of their own, but who knows. There's more .. fragment assets can potentially have
multiple drawables - they always have a "common drawable", but also have zero or more "extra
drawables", each of which could potentially have a txd in its shadergroup. So far I have not seen
any fragments with extra drawables, so this might be an older system, or something else .. there
is also the concept of "child fragments" which live in the physics LOD of CDynamicEntity (not
modelinfo at all!), each of these can potentially have either a "damaged entity" or an "undamaged
entity" or both - these entities are actually rmcDrawables (or some sort of fragDrawable that
inherits from rmcDrawable .. whatever) which could potentially contain txds in their shadergroups.
Again, however, so far I have not seen any txds in these .. even in entities with many damaged and
undamaged frag children. There are still more .. if the modelinfo is a MI_TYPE_PED, then it can
be cast to a CPed and it will have (potentially) dwd's with associated parent txd's in the
component file and props file, and a whole bunch of txds and dwds which can be found through
GetPedRenderGfx, see above description of PEDs .. and likewise for MI_TYPE_VEHICLE with the HD txd
and HD fragment indices. Is that it? Are we done yet?

* There are some obscure fields at the bottom of the search params group called "filter", these
can be used to limit the search to modelinfo's which are associated with a particular asset. If
the Filter Asset Type is "None" then this filter is not active, otherwise it is used to find an
asset with the name given by Filter Asset Name. Usually when using this filter you will want to
keep the Search string empty (not the Filter Asset Name!) so all names are considered. Also, if
Filter Asset Type is not "None" but Filter Asset Name is empty, the filter will consider
modelinfo's which are associated with *any* asset of the type specified by Filter Asset Type. I've
implemented the search filter for searching txd assets as well as modelinfo's, but only in some
specific ways (for example, searching the DwdStore for assets which are associated with a
particular fragment doesn't make any sense ..) so it's best to turn this crap off unless you're
searching for modelinfo's (and you really understand how the search filter works).

* Searching for txds iterates over each of the asset stores (txd/dwd/drawable/fragment/particle)
and considers each asset which matches the search criteria .. by "considering" the asset, it tries
to find all the "associated txd's" for that asset and adds list entries for them to the list. This
can result in duplicate list entries, for example searching for txd's with the name "vehshare"
will find the txd called "vehshare" but also "vehshare_truck" whose parent txd is "vehshare" so
"vehshare" will appear twice (need to verify this particular case). I'm not quite happy with the
way that searching for txd's works currently, to be honest. It might be better to call this
"searching for assets", and instead of trying to populate the list with all the associated txds
for all the assets that were found, simply populating the list with a single entry per asset and
display the associated txds as part of the description so they could be searched separately. Maybe
searching for assets would populate the list with not _Txd entries (CDTVEntry_Txd) but a more
generic _Asset entry, which would have no relevant preview (?) but selecting it would expand it
to a list of all the associated txd's for that asset. What I don't like about this is that it adds
an additional indirection so if you know you want a particular txd (or the txd in a particular
drawable, etc.) it would take an extra step to get to it. However, having _Asset entries could
potentially be quite useful - as well as simplifying the search logic, we could display more
detailed descriptive information when they are selected, not necessarily related to textures or
txds, for example a dwd asset could display the total size of the asset itself, or the number of
drawable entries which contain empty vs non-empty txds in their shadergroups.

[UPDATE] - For now, searching for txds invokes Populate_SearchTxds which calls FindAssets_* for
each of the five asset stores and then GetAssociatedTxds_* for each slot returned by FindAssets_*.
It might be adequate for now to simply force GetAssociatedTxds_* to skip txd parents when invoked
from Populate_SearchTxds. So I'll add a bool parameter "bIgnoreParentTxds", maybe this will be
good enough?

TODO --

* Streaming Iterators - It might be quite useful to have the ability to search for assets that
satisfy some particular search criteria not just amongst the assets which happen to be loaded
currently but all the assets which are registered with the streaming system. Some types of
searches such as searching for assets with a particular name, or assets whose parent txd index is
some particular index can be done without loading or streaming in anything because that info is
always available. However many types of searches require the assets themselves to be in memory,
for example searching for txds which contain textures that are 1024x1024 or larger, or searching
for textures of a certain name, or searching for fragments that contain extra drawables. I have
a system for iterating over all the assets of a particular type, using a template class called
CDTVStreamingIterator. I also have some simple test cases set up for txd/dwd/drawable/fragment
iterating through the Streaming Iterator group in the texture viewer bank .. however it doesn't
seem to work very well. For TxdStore it works ok (although a lot of assets which are supposedly
registered with the streaming system fail to actually stream in), but for the other asset stores
it seems to fail in strange ways. There are a bunch of asserts that get hit as well (supposedly
they are art errors, but who knows). I would like to make the CDTVStreamingIterator actually work
nicely for all the asset stores. Also, it would be nice to iterate over, say, all textures in all
txds in all asset stores in one go. Maybe that's too complicated ..

TODO --

* Entity iterators - Another type of searching which would be quite useful would be searching
entities, for example searching CDynamicEntity's for frag children which might contain txds in
their drawable's shadergroups. There is a "ForAll" method of fwPool (fwBasePool?) which can be
used to iterate over a specific pool, but iterating over all the pools that inherit from a
particular entity type is still a bit of a pain. In the case of CDynamicEntity, I would need to
iterate over CObject, CPed, CVehicle and CAnimatedBuilding to hit all of them, which is not
immediately obvious.

TODO --

* Why are there empty txd's? Isn't this a waste of memory? e.g. there are rmcDrawables in the
various asset stores which contain shadergroups which contain txd pointers which are not NULL,
but GetCount() returns zero. Seems like maybe we could remove these txds entirely and save us
some bytes ..
*/

void GetAssociatedTxds_TxdStore(atArray<CTxdRef>& refs, int assetIndex, const CAssetSearchParams& asp, const char* info, bool bIgnoreParentTxds)
{
	if (assetIndex != INDEX_NONE)
	{
		const fwTxd* txd = g_TxdStore.GetSafeFromIndex(strLocalIndex(assetIndex));

		if (txd)
		{
			if (asp.m_showEmptyTxds || txd->GetCount() > 0)
			{
				refs.PushAndGrow(CTxdRef(AST_TxdStore, assetIndex, INDEX_NONE, info));
			}

			if (!bIgnoreParentTxds) // parent txd's
			{
				for (int parentNum = 0, parentTxdIndex = g_TxdStore.GetParentTxdSlot(strLocalIndex(assetIndex)).Get(); parentTxdIndex != INDEX_NONE; parentTxdIndex = g_TxdStore.GetParentTxdSlot(strLocalIndex(parentTxdIndex)).Get())
				{
					refs.PushAndGrow(CTxdRef(AST_TxdStore, parentTxdIndex, INDEX_NONE, atVarString("%sTXDPARENT_%d", info, parentNum++)));
				}
			}
		}
		else if (asp.m_showStreamable && CAssetRef::IsInStreamingImage(AST_TxdStore, strLocalIndex(assetIndex)))
		{
			refs.PushAndGrow(CTxdRef(AST_TxdStore, assetIndex, INDEX_NONE, atVarString("%s?", info))); // [SEARCH ISSUE] txd not loaded, don't know if there are parent txds
		}
	}
}

void GetAssociatedTxds_DwdStore(atArray<CTxdRef>& refs, strLocalIndex assetIndex, const CAssetSearchParams& asp, const char* info, bool bIgnoreParentTxds)
{
	if (assetIndex != INDEX_NONE)
	{
		const Dwd* dwd = g_DwdStore.GetSafeFromIndex(assetIndex);

		if (dwd)
		{
			for (int i = 0; i < dwd->GetCount(); i++)
			{
				const Drawable* drawable = dwd->GetEntry(i);

				if (drawable)
				{
					const fwTxd* txd = drawable->GetTexDictSafe();

					if (txd && (txd->GetCount() > 0 || asp.m_showEmptyTxds))
					{
						refs.PushAndGrow(CTxdRef(AST_DwdStore, assetIndex.Get(), i, info));
					}
				}
			}
		}
		else if (asp.m_showStreamable && CAssetRef::IsInStreamingImage(AST_DwdStore, assetIndex))
		{
			refs.PushAndGrow(CTxdRef(AST_DwdStore, assetIndex.Get(), 0, atVarString("%s?", info))); // [SEARCH ISSUE] dwd not loaded, assume one drawable?
		}

		if (!bIgnoreParentTxds) // parent
		{
			GetAssociatedTxds_TxdStore(refs, g_DwdStore.GetParentTxdForSlot(assetIndex).Get(), asp, atVarString("%sDWDPARENT", info));
		}
	}
}

void GetAssociatedTxds_DrawableStore(atArray<CTxdRef>& refs, int assetIndex, const CAssetSearchParams& asp, const char* info, bool bIgnoreParentTxds)
{
	if (assetIndex != INDEX_NONE)
	{
		const Drawable* drawable = g_DrawableStore.GetSafeFromIndex(strLocalIndex(assetIndex));

		if (drawable)
		{
			const fwTxd* txd = drawable->GetTexDictSafe();

			if (txd && (txd->GetCount() > 0 || asp.m_showEmptyTxds))
			{
				refs.PushAndGrow(CTxdRef(AST_DrawableStore, assetIndex, INDEX_NONE, info));
			}
		}
		else if (asp.m_showStreamable && CAssetRef::IsInStreamingImage(AST_DrawableStore, strLocalIndex(assetIndex)))
		{
			refs.PushAndGrow(CTxdRef(AST_DrawableStore, assetIndex, INDEX_NONE, atVarString("%s?", info)));
		}

		if (!bIgnoreParentTxds) // parent
		{
			GetAssociatedTxds_TxdStore(refs, g_DrawableStore.GetParentTxdForSlot(strLocalIndex(assetIndex)).Get(), asp, atVarString("%sDRAWPARENT", info));
		}
	}
}

void GetAssociatedTxds_FragmentStore(atArray<CTxdRef>& refs, int assetIndex, const CAssetSearchParams& asp, const char* info, bool bIgnoreParentTxds)
{
	if (assetIndex != INDEX_NONE)
	{
		const Fragment* fragment = g_FragmentStore.GetSafeFromIndex(strLocalIndex(assetIndex));

		if (fragment)
		{
			// common drawable
			{
				const fwTxd* txd = fragment->GetCommonDrawable()->GetTexDictSafe();

				if (txd && (txd->GetCount() > 0 || asp.m_showEmptyTxds))
				{
					refs.PushAndGrow(CTxdRef(AST_FragmentStore, assetIndex, INDEX_NONE, info));
				}
			}

			if (fragment->GetClothDrawable()) // cloth drawable
			{
				const fwTxd* txd = fragment->GetClothDrawable()->GetTexDictSafe();

				if (txd && (txd->GetCount() > 0 || asp.m_showEmptyTxds))
				{
					refs.PushAndGrow(CTxdRef(AST_FragmentStore, assetIndex, -2, atVarString("%sCLOTH", info)));
				}
			}

			for (int i = 0; i < fragment->GetNumExtraDrawables(); i++) // extra drawables
			{
				const fwTxd* txd = fragment->GetExtraDrawable(i)->GetTexDictSafe();

				if (txd && (txd->GetCount() > 0 || asp.m_showEmptyTxds))
				{
					refs.PushAndGrow(CTxdRef(AST_FragmentStore, assetIndex, i, atVarString("%sEXTRA[%s]", info, fragment->GetExtraDrawableName(i))));
				}
			}
		}
		else if (asp.m_showStreamable && CAssetRef::IsInStreamingImage(AST_FragmentStore, strLocalIndex(assetIndex)))
		{
			refs.PushAndGrow(CTxdRef(AST_FragmentStore, assetIndex, INDEX_NONE, atVarString("%s?", info))); // [SEARCH ISSUE] fragment not loaded, can't determine if extra drawables are present ..
		}

		if (!bIgnoreParentTxds) // parent
		{
			GetAssociatedTxds_TxdStore(refs, g_FragmentStore.GetParentTxdForSlot(strLocalIndex(assetIndex)).Get(), asp, atVarString("%sFRAGPARENT", info));
		}
	}
}

void GetAssociatedTxds_ParticleStore(atArray<CTxdRef>& refs, int assetIndex, const CAssetSearchParams& asp, const char* info, bool bIgnoreParentTxds)
{
	if (assetIndex != INDEX_NONE)
	{
		ptxFxList* particle = g_ParticleStore.GetSafeFromIndex(strLocalIndex(assetIndex));

		if (particle)
		{
			const fwTxd* txd = particle->GetTextureDictionary();
#if __ASSERT
			const int numTxdsInDwd = _GetNumTxds(particle->GetModelDictionary(), true);
			Assertf(numTxdsInDwd == 0, "particle \"%s\" has %d txds in its dwd, need to handle this properly!", g_ParticleStore.GetName(strLocalIndex(assetIndex)), numTxdsInDwd);
#endif // __ASSERT

			if (txd && (txd->GetCount() > 0 || asp.m_showEmptyTxds))
			{
				refs.PushAndGrow(CTxdRef(AST_ParticleStore, assetIndex, INDEX_NONE, info));
			}
		}
		else if (asp.m_showStreamable && CAssetRef::IsInStreamingImage(AST_ParticleStore, strLocalIndex(assetIndex)))
		{
			refs.PushAndGrow(CTxdRef(AST_ParticleStore, assetIndex, INDEX_NONE, atVarString("%s?", info)));
		}

		if (!bIgnoreParentTxds) // parent
		{
#if 0
			GetAssociatedTxds_TxdStore(refs, g_ParticleStore.GetParentTxdForSlot(assetIndex), asp, atVarString("%sPARTPARENT", info));
#endif
		}
	}
}

// ================================================================================================

static bool FindTextures_CheckSizeAndFlags(
	const grcTexture* texture,
	const char* format,
	int minSize,
	bool bConstantOnly,
	bool bRedundantOnly,
	int parentTxdSlot,
	u32 conversionFlagsRequired,
	u32 conversionFlagsSkipped
	)
{
	if (format && !StringMatch(grcImage::GetFormatString((grcImage::Format)texture->GetImageFormat()), format))
	{
		return false;
	}

	if (Max<int>(texture->GetWidth(), texture->GetHeight()) < minSize)
	{
		return false;
	}

	const u32 conversionFlags = (u32)texture->GetConversionFlags();

	if ((conversionFlags & conversionFlagsRequired) != conversionFlagsRequired)
	{
		return false;
	}

	if ((conversionFlags & conversionFlagsSkipped) != 0)
	{
		return false;
	}

	if (bConstantOnly)
	{
		if (!IsTextureConstant(texture))
		{
			return false;
		}
	}

	if (bRedundantOnly)
	{
		bool bIsRedundant = false;

		while (parentTxdSlot != INDEX_NONE)
		{
			const fwTxd* parentTxd = g_TxdStore.Get(strLocalIndex(parentTxdSlot));

			for (int k = 0; k < parentTxd->GetCount(); k++)
			{
				const grcTexture* parentTexture = parentTxd->GetEntry(k);

				// check name only .. hash is quite slow (assume we'll fix name collisions later)
				if (parentTexture && stricmp(parentTexture->GetName(), texture->GetName()) == 0)
				{
					bIsRedundant = true;
					break;
				}
			}

			if (bIsRedundant)
			{
				break;
			}
			else
			{
				parentTxdSlot = g_TxdStore.GetParentTxdSlot(strLocalIndex(parentTxdSlot)).Get();
			}
		}

		if (!bIsRedundant)
		{
			return false;
		}
	}
	
	return true;
}

#if EFFECT_PRESERVE_STRINGS

// create a map of grcTextures that are used in a particular shader var
static void FindTextures_BuildShaderUsageMap(atMap<const grcTexture*,bool>& map, const Drawable* drawable, const char* searchStr, const CAssetSearchParams& asp, const char* usage, bool bReportIneffectiveTextures)
{
	if (drawable)
	{
		const grmShaderGroup* shaderGroup = &drawable->GetShaderGroup();

		for (int i = 0; i < shaderGroup->GetCount(); i++)
		{
			const grmShader* shader = shaderGroup->GetShaderPtr(i);

			for (int j = 0; j < shader->GetVarCount(); j++)
			{
				const grcEffectVar var = shader->GetVarByIndex(j);
				const char* name = NULL;
				grcEffect::VarType type;
				int annotationCount = 0;
				bool isGlobal = false;

				shader->GetVarDesc(var, name, type, annotationCount, isGlobal);

				if (type == grcEffect::VT_TEXTURE && StringMatch(name, usage))
				{
					grcTexture* texture = NULL;
					shader->GetVar(var, texture);

					if (texture && StringMatch(texture->GetName(), searchStr, asp.m_exactNameMatch))
					{
						map[texture] = true;

						(void)bReportIneffectiveTextures;
#if DEBUG_TEXTURE_VIEWER_FIND_INEFFECTIVE_TEXTURES
						if (bReportIneffectiveTextures)
						{
							const float threshold = 0.02f;

							if (stricmp(usage, "BumpTex") == 0)
							{
								float bumpiness = 1.0f;

								for (int j2 = 0; j2 < shader->GetVarCount(); j2++)
								{
									const grcEffectVar var2 = shader->GetVarByIndex(j2);
									const char* name2 = NULL;
									grcEffect::VarType type2;
									int annotationCount2 = 0;
									bool isGlobal2 = false;

									shader->GetVarDesc(var2, name2, type2, annotationCount2, isGlobal2);

									if (type2 == grcEffect::VT_FLOAT && stricmp(name2, "Bumpiness") == 0)
									{
										shader->GetVar(var2, bumpiness);
									}
								}

								if (bumpiness <= threshold)
								{
									Displayf(
										"%s \"%s\" (%s %dx%d%s) used by shader %s with Bumpiness=%f!",
										usage,
										GetFriendlyTextureName(texture).c_str(),
										grcImage::GetFormatString((grcImage::Format)texture->GetImageFormat()),
										texture->GetWidth(),
										texture->GetHeight(),
										IsTextureConstant(texture) ? " - no variation" : "",
										shader->GetName(),
										bumpiness
									);
								}
							}
							else if (stricmp(usage, "SpecularTex") == 0)
							{
								float   specIntensity = 1.0f;
								Vector3 specIntensityMask(1.0f, 0.0f, 0.0f);

								for (int j2 = 0; j2 < shader->GetVarCount(); j2++)
								{
									const grcEffectVar var2 = shader->GetVarByIndex(j2);
									const char* name2 = NULL;
									grcEffect::VarType type2;
									int annotationCount2 = 0;
									bool isGlobal2 = false;

									shader->GetVarDesc(var2, name2, type2, annotationCount2, isGlobal2);

									if (type2 == grcEffect::VT_FLOAT && stricmp(name2, "SpecularColor") == 0)
									{
										shader->GetVar(var2, specIntensity);
									}
									else if (type2 == grcEffect::VT_VECTOR3 && stricmp(name2, "SpecularMapIntensityMask") == 0)
									{
										shader->GetVar(var2, specIntensityMask);
									}
								}

								if (specIntensity*MaxElement(RCC_VEC3V(specIntensityMask)).Getf() <= threshold)
								{
									Displayf(
										"%s \"%s\" (%s %dx%d%s) used by shader %s with SpecularColor=%f, SpecularMapIntensityMask=%f,%f,%f!",
										usage,
										GetFriendlyTextureName(texture).c_str(),
										grcImage::GetFormatString((grcImage::Format)texture->GetImageFormat()),
										texture->GetWidth(),
										texture->GetHeight(),
										IsTextureConstant(texture) ? " - no variation" : "",
										shader->GetName(),
										specIntensity,
										specIntensityMask.x,
										specIntensityMask.y,
										specIntensityMask.z
									);
								}
							}
						}
#endif // DEBUG_TEXTURE_VIEWER_FIND_INEFFECTIVE_TEXTURES
					}
				}
			}
		}
	}
}

static void FindTextures_BuildShaderUsageMapAll(atMap<const grcTexture*,bool>& map, const char* searchStr, const CAssetSearchParams& asp, const char* usage, bool bReportIneffectiveTextures)
{
	if (usage && usage[0] != '\0')
	{
		for (int i = 0; i < g_DwdStore.GetSize(); i++)
		{
			if (g_DwdStore.IsValidSlot(strLocalIndex(i)))
			{
				const Dwd* dwd = g_DwdStore.Get(strLocalIndex(i));

				if (dwd)
				{
					for (int j = 0; j < dwd->GetCount(); j++)
					{
						FindTextures_BuildShaderUsageMap(map, dwd->GetEntry(j), searchStr, asp, usage, bReportIneffectiveTextures);
					}
				}
			}
		}

		for (int i = 0; i < g_DrawableStore.GetSize(); i++)
		{
			if (g_DrawableStore.IsValidSlot(strLocalIndex(i)))
			{
				FindTextures_BuildShaderUsageMap(map, g_DrawableStore.Get(strLocalIndex(i)), searchStr, asp, usage, bReportIneffectiveTextures);
			}
		}

		for (int i = 0; i < g_FragmentStore.GetSize(); i++)
		{
			if (g_FragmentStore.IsValidSlot(strLocalIndex(i)))
			{
				const Fragment* fragment = g_FragmentStore.Get(strLocalIndex(i));

				if (fragment)
				{
					FindTextures_BuildShaderUsageMap(map, fragment->GetCommonDrawable(), searchStr, asp, usage, bReportIneffectiveTextures);
					FindTextures_BuildShaderUsageMap(map, fragment->GetClothDrawable(), searchStr, asp, usage, bReportIneffectiveTextures);

					for (int j = 0; j < fragment->GetNumExtraDrawables(); j++)
					{
						FindTextures_BuildShaderUsageMap(map, fragment->GetExtraDrawable(j), searchStr, asp, usage, bReportIneffectiveTextures);
					}
				}
			}
		}

		for (int i = 0; i < g_ParticleStore.GetSize(); i++)
		{
			if (g_ParticleStore.IsValidSlot(strLocalIndex(i)))
			{
				ptxFxList* ptx = g_ParticleStore.Get(strLocalIndex(i));

				if (ptx)
				{
					const Dwd* ptxDwd = ptx->GetModelDictionary();

					if (ptxDwd)
					{
						for (int j = 0; j < ptxDwd->GetCount(); j++)
						{
							FindTextures_BuildShaderUsageMap(map, ptxDwd->GetEntry(j), searchStr, asp, usage, bReportIneffectiveTextures);
						}
					}
				}
			}
		}
	}
}

#endif // EFFECT_PRESERVE_STRINGS

void FindTextures(
	atArray<CTextureRef>& refs,
	const char* searchStr,
	const CAssetSearchParams& asp,
	int maxCount,
	int minSize,
	bool bConstantOnly,
	bool bRedundantOnly,
	bool bReportNonUniqueTextureMemory,
	u32 conversionFlagsRequired,
	u32 conversionFlagsSkipped,
#if EFFECT_PRESERVE_STRINGS
	const char* usage,
	const char* usageExclusion,
#else
	const char* ,
	const char* ,
#endif
	const char* format,
	bool bVerbose
	)
{
	atMap<u32,bool> uniqueTextureHashMap; // used to report non-unique textures 
	int             uniqueTextureMemory = 0;
	int             uniqueTextureCount  = 0;
	int             totalTextureMemory  = 0;
	int             totalTextureCount   = 0;

	atMap<const grcTexture*,bool> usageExclusionMap; // textures which are used by other usages
	atMap<const grcTexture*,bool> usageMap; // textures which are used by shader var usage
	bool                          usageMapValid = false;

#if EFFECT_PRESERVE_STRINGS
	if (usage && usage[0] != '\0')
	{
		FindTextures_BuildShaderUsageMapAll(usageExclusionMap, searchStr, asp, usageExclusion, false);
		FindTextures_BuildShaderUsageMapAll(usageMap, searchStr, asp, usage, true);

		usageMapValid = true;
	}
#endif // EFFECT_PRESERVE_STRINGS

	if (asp.m_scanTxdStore)
	{
		if (bVerbose) { Displayf("FindTextures: scanning g_TxdStore .."); }

		for (int i = 0; i < g_TxdStore.GetSize(); i++)
		{
			const fwTxd* txd = g_TxdStore.GetSafeFromIndex(strLocalIndex(i));

			if (txd && !g_TxdStore.GetSlot(strLocalIndex(i))->m_isDummy)
			{
				for (int k = 0; k < txd->GetCount(); k++)
				{
					const grcTexture* texture = txd->GetEntry(k);

					if (texture)
					{
						if (!StringMatch(texture->GetName(), searchStr, asp.m_exactNameMatch))
						{
							continue;
						}

						if (!FindTextures_CheckSizeAndFlags(texture, format, minSize, bConstantOnly, bRedundantOnly, g_TxdStore.GetParentTxdSlot(strLocalIndex(i)).Get(), conversionFlagsRequired, conversionFlagsSkipped))
						{
							continue;
						}

						if (usageMapValid && (usageMap.Access(texture) == NULL || usageExclusionMap.Access(texture) != NULL))
						{
							continue;
						}

						if (bReportNonUniqueTextureMemory)
						{
							const u32 hash = GetTextureHash(texture, false, -1);

							if (uniqueTextureHashMap.Access(hash) == NULL)
							{
								uniqueTextureHashMap[hash] = true;
								uniqueTextureMemory += texture->GetPhysicalSize();
								uniqueTextureCount++;
							}

							totalTextureMemory += texture->GetPhysicalSize();
							totalTextureCount++;
						}

						if (refs.size() + 1 >= maxCount)
						{
							Displayf("FindTextures: maxCount (%d) exceeded!", maxCount);
							return;
						}

						refs.PushAndGrow(CTextureRef(AST_TxdStore, i, INDEX_NONE, "", k, texture));
					}
				}
			}
			else
			{
				// [SEARCH ISSUE] txd not loaded
			}
		}
	}

	if (asp.m_scanDwdStore)
	{
		if (bVerbose) { Displayf("FindTextures: found %d refs so far", refs.GetCount()); }
		if (bVerbose) { Displayf("FindTextures: scanning g_DwdStore .."); }

		for (int i = 0; i < g_DwdStore.GetSize(); i++)
		{
			const Dwd* dwd = g_DwdStore.GetSafeFromIndex(strLocalIndex(i));

			if (dwd)
			{
				for (int j = 0; j < dwd->GetCount(); j++)
				{
					const Drawable* drawable = dwd->GetEntry(j);

					if (drawable)
					{
						const fwTxd* drawableTxd = drawable->GetTexDictSafe();

						if (drawableTxd)
						{
							for (int k = 0; k < drawableTxd->GetCount(); k++)
							{
								const grcTexture* texture = drawableTxd->GetEntry(k);

								if (texture)
								{
									if (!StringMatch(texture->GetName(), searchStr, asp.m_exactNameMatch))
									{
										continue;
									}  

									if (!FindTextures_CheckSizeAndFlags(texture, format, minSize, bConstantOnly, bRedundantOnly, g_DwdStore.GetParentTxdForSlot(strLocalIndex(i)).Get(), conversionFlagsRequired, conversionFlagsSkipped))
									{
										continue;
									}

									if (usageMapValid && (usageMap.Access(texture) == NULL || usageExclusionMap.Access(texture) != NULL))
									{
										continue;
									}

									if (bReportNonUniqueTextureMemory)
									{
										const u32 hash = GetTextureHash(texture, false, -1);

										if (uniqueTextureHashMap.Access(hash) == NULL)
										{
											uniqueTextureHashMap[hash] = true;
											uniqueTextureMemory += texture->GetPhysicalSize();
											uniqueTextureCount++;
										}

										totalTextureMemory += texture->GetPhysicalSize();
										totalTextureCount++;
									}

									if (refs.size() + 1 >= maxCount)
									{
										Displayf("FindTextures: maxCount (%d) exceeded!", maxCount);
										return;
									}

									refs.PushAndGrow(CTextureRef(AST_DwdStore, i, j, "", k, texture));
								}
							}
						}
					}
				}
			}
			else
			{
				// [SEARCH ISSUE] dwd not loaded
			}
		}
	}

	if (asp.m_scanDrawableStore)
	{
		if (bVerbose) { Displayf("FindTextures: found %d refs so far", refs.GetCount()); }
		if (bVerbose) { Displayf("FindTextures: scanning g_DrawableStore .."); }

		for (int i = 0; i < g_DrawableStore.GetSize(); i++)
		{
			const Drawable* drawable = g_DrawableStore.GetSafeFromIndex(strLocalIndex(i));

			if (drawable)
			{
				const fwTxd* drawableTxd = drawable->GetTexDictSafe();

				if (drawableTxd)
				{
					for (int k = 0; k < drawableTxd->GetCount(); k++)
					{
						const grcTexture* texture = drawableTxd->GetEntry(k);

						if (texture)
						{
							if (!StringMatch(texture->GetName(), searchStr, asp.m_exactNameMatch))
							{
								continue;
							}

							if (!FindTextures_CheckSizeAndFlags(texture, format, minSize, bConstantOnly, bRedundantOnly, g_DrawableStore.GetParentTxdForSlot(strLocalIndex(i)).Get(), conversionFlagsRequired, conversionFlagsSkipped))
							{
								continue;
							}

							if (usageMapValid && (usageMap.Access(texture) == NULL || usageExclusionMap.Access(texture) != NULL))
							{
								continue;
							}

							if (bReportNonUniqueTextureMemory)
							{
								const u32 hash = GetTextureHash(texture, false, -1);

								if (uniqueTextureHashMap.Access(hash) == NULL)
								{
									uniqueTextureHashMap[hash] = true;
									uniqueTextureMemory += texture->GetPhysicalSize();
									uniqueTextureCount++;
								}

								totalTextureMemory += texture->GetPhysicalSize();
								totalTextureCount++;
							}

							if (refs.size() + 1 >= maxCount)
							{
								Displayf("FindTextures: maxCount (%d) exceeded!", maxCount);
								return;
							}

							refs.PushAndGrow(CTextureRef(AST_DrawableStore, i, INDEX_NONE, "", k, texture));
						}
					}
				}
			}
			else
			{
				// [SEARCH ISSUE] drawable not loaded
			}
		}
	}

	if (asp.m_scanParticleStore)
	{
		if (bVerbose) { Displayf("FindTextures: found %d refs so far", refs.GetCount()); }
		if (bVerbose) { Displayf("FindTextures: scanning g_ParticleStore .."); }

		for (int i = 0; i < g_ParticleStore.GetSize(); i++)
		{
			ptxFxList* particle = g_ParticleStore.GetSafeFromIndex(strLocalIndex(i));

			if (particle)
			{
				const fwTxd* txd = particle->GetTextureDictionary();
#if __ASSERT
				const int numTxdsInDwd = _GetNumTxds(particle->GetModelDictionary(), true);
				Assertf(numTxdsInDwd == 0, "particle \"%s\" has %d txds in its dwd, need to handle this properly!", g_ParticleStore.GetName(strLocalIndex(i)), numTxdsInDwd);
#endif // __ASSERT

				if (txd)
				{
					for (int k = 0; k < txd->GetCount(); k++)
					{
						const grcTexture* texture = txd->GetEntry(k);

						if (texture)
						{
							if (!StringMatch(texture->GetName(), searchStr, asp.m_exactNameMatch))
							{
								continue;
							}

							if (!FindTextures_CheckSizeAndFlags(texture, format, minSize, bConstantOnly, bRedundantOnly, INDEX_NONE, conversionFlagsRequired, conversionFlagsSkipped))
							{
								continue;
							}

							if (usageMapValid && (usageMap.Access(texture) == NULL || usageExclusionMap.Access(texture) != NULL))
							{
								continue;
							}

							if (bReportNonUniqueTextureMemory)
							{
								const u32 hash = GetTextureHash(texture, false, -1);

								if (uniqueTextureHashMap.Access(hash) == NULL)
								{
									uniqueTextureHashMap[hash] = true;
									uniqueTextureMemory += texture->GetPhysicalSize();
									uniqueTextureCount++;
								}

								totalTextureMemory += texture->GetPhysicalSize();
								totalTextureCount++;
							}

							if (refs.size() + 1 >= maxCount)
							{
								Displayf("FindTextures: maxCount (%d) exceeded!", maxCount);
								return;
							}

							refs.PushAndGrow(CTextureRef(AST_ParticleStore, i, INDEX_NONE, "", k, texture));
						}
					}
				}
			}
			else
			{
				// [SEARCH ISSUE] particle not loaded
			}
		}
	}

	if (bReportNonUniqueTextureMemory)
	{
		Displayf(
			"FindTextures: found %d textures (%.2fMB), %d unique (%.2fMB) .. %.2f%%/%.2f%%",
			totalTextureCount,
			(float)totalTextureMemory/(1024.0f*1024.0f),
			uniqueTextureCount,
			(float)uniqueTextureMemory/(1024.0f*1024.0f),
			100.0f*(float)uniqueTextureCount/(float)totalTextureCount,
			100.0f*(float)uniqueTextureMemory/(float)totalTextureMemory
		);
	}

	if (1)
	{
		if (bVerbose) { Displayf("FindTextures: found %d refs so far", refs.GetCount()); }
	}
}

static bool _IsMatchingFilterAssetIndex(int filterAssetIndex, int assetIndex)
{
	if (filterAssetIndex == INDEX_NONE)
	{
		if (assetIndex != INDEX_NONE)
		{
			return true; // matches "any"
		}
	}
	else if (filterAssetIndex == assetIndex)
	{
		return true; // matches filter asset index
	}

	return false; // no dice.
}

#if DEBUG_TEXTURE_VIEWER_ENTITY_FILTERING

static void FindModelsMatchingEntityFlags(u8 *flags, const CAssetSearchParams& asp)
{
	const int maxProxies = CDebugArchetype::GetMaxDebugArchetypeProxies();
	sysMemSet(flags, 0, (maxProxies + 7)/8);

	fwEntityContainer::Pool* pool = fwEntityContainer::GetPool();

	if (pool)
	{
		for (int i = 0; i < pool->GetSize(); i++)
		{
			fwEntityContainer* container = pool->GetSlot(i);

			if (container)
			{
				atArray<fwEntity*> entities;

				container->GetEntityArray(entities);

				for (int j = 0; j < entities.size(); j++)
				{
					CEntity* entity = (CEntity*)entities[j];

					if (entity)
					{
						Assert(entity->GetType() >= 0 && entity->GetType() < ENTITY_TYPE_TOTAL);

						if (!asp.m_entityTypeFlags[entity->GetType()])
						{
							continue;
						}

						if (!entity->GetIsVisible())
						{
							continue;
						}

						const int proxyIndex = CDebugArchetypeProxy::FindProxyIndex(entity->GetBaseModelInfo()->GetHashKey());

						if (proxyIndex != -1)
						{
							Assert(proxyIndex >= 0 && proxyIndex < maxProxies);

							bool bValid = false;

							switch (asp.m_entityLODFilter)
							{
							case ELODSP_ALL     : bValid = true; break;
							case ELODSP_HD_ONLY : bValid = (entity->GetLodData().IsHighDetail()); break;
							case ELODSP_HD_NONE : bValid = (!entity->GetLodData().IsHighDetail()); break;
							}

							if (bValid)
							{
								flags[proxyIndex/8] |= BIT(proxyIndex%8);
							}
						}
					}
				}
			}
		}
	}
}

#endif // DEBUG_TEXTURE_VIEWER_ENTITY_FILTERING

void FindAssets_ModelInfo(atArray<u32>& slots, const char* searchStr, const char* rpf, const CAssetSearchParams& asp, int maxCount)
{
	const int maxProxies = CDebugArchetype::GetMaxDebugArchetypeProxies();
#if DEBUG_TEXTURE_VIEWER_ENTITY_FILTERING
	u8* flags = Alloca(u8, (maxProxies + 7)/8);
	bool bFlagsValid = false;

	for (int i = 0; i < ENTITY_TYPE_TOTAL; i++)
	{
		if (!asp.m_entityTypeFlags[i])
		{
			bFlagsValid = true;
			break;
		}
	}

	if (bFlagsValid)
	{
		FindModelsMatchingEntityFlags(flags, asp);
	}
#endif // DEBUG_TEXTURE_VIEWER_ENTITY_FILTERING

	if (0) // finding the broken modelinfo with dwdEntry = 255
	{
		for (u32 i = 0; i < maxProxies; i++)
		{
			const CDebugArchetypeProxy* modelInfo = CDebugArchetype::GetDebugArchetypeProxy(i);

			if (modelInfo == NULL) // this can happen, it seems .. not sure why
			{
				continue;
			}

			if (modelInfo->SafeGetDrawDictIndex() != INDEX_NONE &&
				modelInfo->SafeGetDrawDictEntry_IfLoaded() == 255)
			{
				Displayf(
					"ModelInfo[%d] \"%s\" refers to %s but dwdEntry is 255",
					i,
					modelInfo->GetModelName(),
					CAssetRef(AST_DwdStore, modelInfo->SafeGetDrawDictIndex()).GetDesc().c_str()
				);
			}
		}
	}

	int filterAssetIndex = INDEX_NONE;
	int filterDwdEntry   = asp.m_filterDwdEntryEnabled ? asp.m_filterDwdEntry : INDEX_NONE;

	if (asp.m_filterAssetName[0] != '\0')
	{
		switch (asp.m_filterAssetType) // recalculate filter asset index as it might have changed
		{
		case AST_None          : break;
		case AST_TxdStore      : filterAssetIndex = g_TxdStore     .FindSlot(asp.m_filterAssetName).Get(); break;
		case AST_DwdStore      : filterAssetIndex = g_DwdStore     .FindSlot(asp.m_filterAssetName).Get(); break;
		case AST_DrawableStore : filterAssetIndex = g_DrawableStore.FindSlot(asp.m_filterAssetName).Get(); break;
		case AST_FragmentStore : filterAssetIndex = g_FragmentStore.FindSlot(asp.m_filterAssetName).Get(); break;
		case AST_ParticleStore : filterAssetIndex = g_ParticleStore.FindSlot(asp.m_filterAssetName).Get(); break;
		}

		if (asp.m_filterAssetType != AST_None && filterAssetIndex == INDEX_NONE)
		{
			Displayf("FindAssets_ModelInfo: filter asset \"%s\" not found in [%s]", asp.m_filterAssetName, CAssetRef::GetAssetTypeName((eAssetType)asp.m_filterAssetType));
			return;
		}
	}

	const int indexMin = Max<int>(asp.m_scanAssetIndexRangeEnabled ? asp.m_scanAssetIndexRange[0] : 0, 0);
	const int indexMax = Min<int>(asp.m_scanAssetIndexRangeEnabled ? asp.m_scanAssetIndexRange[1] : 999999, maxProxies - 1);

	for (u32 i = indexMin; i <= indexMax; i++)
	{
		const CDebugArchetypeProxy* modelInfo = CDebugArchetype::GetDebugArchetypeProxy(i);

		if (modelInfo == NULL) // this can happen, it seems .. not sure why
		{
			continue;
		}

#if DEBUG_TEXTURE_VIEWER_ENTITY_FILTERING
		if (bFlagsValid && (flags[i/8] & BIT(i%8)) == 0)
		{
			continue;
		}
#endif // DEBUG_TEXTURE_VIEWER_ENTITY_FILTERING

		if (asp.m_modelInfoShowLoadedModelsOnly && !modelInfo->GetHasLoaded())
		{
			continue;
		}

		if ((asp.m_modelInfoShowProps == MISP_PROPS_ONLY    && modelInfo->GetIsProp() == 0) ||
			(asp.m_modelInfoShowProps == MISP_NONPROPS_ONLY && modelInfo->GetIsProp() != 0))
		{
			continue;
		}

		if (StringMatch(modelInfo->GetModelName(), searchStr, asp.m_exactNameMatch))
		{
			if (rpf[0] != '\0')
			{
				const char* rpfPath = "";

				switch ((int)modelInfo->GetDrawableType())
				{
				case fwArchetype::DT_FRAGMENT          : rpfPath = CAssetRef(AST_FragmentStore, modelInfo->SafeGetFragmentIndex()).GetRPFPathName(); break;
				case fwArchetype::DT_DRAWABLE          : rpfPath = CAssetRef(AST_DrawableStore, modelInfo->SafeGetDrawableIndex()).GetRPFPathName(); break;
				case fwArchetype::DT_DRAWABLEDICTIONARY: rpfPath = CAssetRef(AST_DwdStore     , modelInfo->SafeGetDrawDictIndex()).GetRPFPathName(); break;
				}

				if (rpfPath[0] == '\0' || !StringMatch(rpfPath, rpf))
				{
					continue;
				}
			}

			const u32 type = modelInfo->GetModelType();

			if (type < CAssetSearchParams::_MI_TYPE_COUNT) // should be in this range, unless someone added a new model type
			{
				if (!asp.m_modelInfoTypeFlags[type])
				{
					continue;
				}
			}

			if (asp.m_filterAssetType != AST_None) // filter
			{
				bool bMatching = true;

				switch (asp.m_filterAssetType)
				{
				case AST_None          : break;
				case AST_TxdStore      : bMatching = _IsMatchingFilterAssetIndex(filterAssetIndex, modelInfo->GetAssetParentTxdIndex()); break;
				case AST_DwdStore      : bMatching = _IsMatchingFilterAssetIndex(filterAssetIndex, modelInfo->SafeGetDrawDictIndex  ()); break;
				case AST_DrawableStore : bMatching = _IsMatchingFilterAssetIndex(filterAssetIndex, modelInfo->SafeGetDrawableIndex  ()); break;
				case AST_FragmentStore : bMatching = _IsMatchingFilterAssetIndex(filterAssetIndex, modelInfo->SafeGetFragmentIndex  ()); break;
				case AST_ParticleStore : bMatching = _IsMatchingFilterAssetIndex(filterAssetIndex, modelInfo->GetPtFxAssetSlot      ()); break;
				}

				if (!bMatching)
				{
					continue;
				}
				else if (asp.m_filterAssetType == AST_DwdStore && filterDwdEntry != INDEX_NONE && filterDwdEntry != modelInfo->SafeGetDrawDictEntry_IfLoaded())
				{
					continue;
				}
			}

			if (slots.size() + 1 >= maxCount)
			{
				Displayf("FindAssets_ModelInfo: maxCount (%d) exceeded!", maxCount);
				return;
			}

			slots.PushAndGrow(i);
		}
	}
}

void FindAssets_TxdStore(atArray<int>& slots, const char* searchStr, const char* rpf, const CAssetSearchParams& asp, int maxCount, bool bOnlyIfLoaded)
{
	const int indexMin = Max<int>(asp.m_scanAssetIndexRangeEnabled ? asp.m_scanAssetIndexRange[0] : 0, 0);
	const int indexMax = Min<int>(asp.m_scanAssetIndexRangeEnabled ? asp.m_scanAssetIndexRange[1] : 999999, g_TxdStore.GetSize() - 1);

	for (int i = indexMin; i <= indexMax; i++)
	{
		if (!g_TxdStore.IsValidSlot(strLocalIndex(i)))
		{
			continue;
		}

		const char *txdName = g_TxdStore.GetName(strLocalIndex(i));
		if (txdName && StringMatch(txdName , searchStr, asp.m_exactNameMatch) && (!bOnlyIfLoaded || g_TxdStore.GetSafeFromIndex(strLocalIndex(i)) != NULL))
		{
			if (rpf[0] != '\0')
			{
				const char* rpfPath = CAssetRef(AST_TxdStore, i).GetRPFPathName();

				if (rpfPath[0] == '\0' || !StringMatch(rpfPath, rpf))
				{
					continue;
				}
			}

			if (slots.size() + 1 >= maxCount)
			{
				Displayf("FindAssets_TxdStore: maxCount (%d) exceeded!", maxCount);
				return;
			}

			slots.PushAndGrow(i);
		}
	}
}

void FindAssets_DwdStore(atArray<int>& slots, const char* searchStr, const char* rpf, const CAssetSearchParams& asp, int maxCount, bool bOnlyIfLoaded)
{
	const int indexMin = Max<int>(asp.m_scanAssetIndexRangeEnabled ? asp.m_scanAssetIndexRange[0] : 0, 0);
	const int indexMax = Min<int>(asp.m_scanAssetIndexRangeEnabled ? asp.m_scanAssetIndexRange[1] : 999999, g_DwdStore.GetSize() - 1);

	for (int i = indexMin; i <= indexMax; i++)
	{
		if (!g_DwdStore.IsValidSlot(strLocalIndex(i)))
		{
			continue;
		}

		if (StringMatch(g_DwdStore.GetName(strLocalIndex(i)), searchStr, asp.m_exactNameMatch) && (!bOnlyIfLoaded || g_DwdStore.GetSafeFromIndex(strLocalIndex(i)) != NULL))
		{
			if (rpf[0] != '\0')
			{
				const char* rpfPath = CAssetRef(AST_DwdStore, i).GetRPFPathName();

				if (rpfPath[0] == '\0' || !StringMatch(rpfPath, rpf))
				{
					continue;
				}
			}

			if (slots.size() + 1 >= maxCount)
			{
				Displayf("FindAssets_DwdStore: maxCount (%d) exceeded!", maxCount);
				return;
			}

			slots.PushAndGrow(i);
		}
	}
}

void FindAssets_DrawableStore(atArray<int>& slots, const char* searchStr, const char* rpf, const CAssetSearchParams& asp, int maxCount, bool bOnlyIfLoaded)
{
	const int indexMin = Max<int>(asp.m_scanAssetIndexRangeEnabled ? asp.m_scanAssetIndexRange[0] : 0, 0);
	const int indexMax = Min<int>(asp.m_scanAssetIndexRangeEnabled ? asp.m_scanAssetIndexRange[1] : 999999, g_DrawableStore.GetSize() - 1);

	for (int i = indexMin; i <= indexMax; i++)
	{
		if (!g_DrawableStore.IsValidSlot(strLocalIndex(i)))
		{
			continue;
		}

		if (StringMatch(g_DrawableStore.GetName(strLocalIndex(i)), searchStr, asp.m_exactNameMatch) && (!bOnlyIfLoaded || g_DrawableStore.GetSafeFromIndex(strLocalIndex(i)) != NULL))
		{
			if (rpf[0] != '\0')
			{
				const char* rpfPath = CAssetRef(AST_DrawableStore, i).GetRPFPathName();

				if (rpfPath[0] == '\0' || !StringMatch(rpfPath, rpf))
				{
					continue;
				}
			}

			if (slots.size() + 1 >= maxCount)
			{
				Displayf("FindAssets_DrawableStore: maxCount (%d) exceeded!", maxCount);
				return;
			}

			slots.PushAndGrow(i);
		}
	}
}

void FindAssets_FragmentStore(atArray<int>& slots, const char* searchStr, const char* rpf, const CAssetSearchParams& asp, int maxCount, bool bOnlyIfLoaded)
{
	const int indexMin = Max<int>(asp.m_scanAssetIndexRangeEnabled ? asp.m_scanAssetIndexRange[0] : 0, 0);
	const int indexMax = Min<int>(asp.m_scanAssetIndexRangeEnabled ? asp.m_scanAssetIndexRange[1] : 999999, g_FragmentStore.GetSize() - 1);

	for (int i = indexMin; i <= indexMax; i++)
	{
		if (!g_FragmentStore.IsValidSlot(strLocalIndex(i)))
		{
			continue;
		}

		if (StringMatch(g_FragmentStore.GetName(strLocalIndex(i)), searchStr, asp.m_exactNameMatch) && (!bOnlyIfLoaded || g_FragmentStore.GetSafeFromIndex(strLocalIndex(i)) != NULL))
		{
			if (rpf[0] != '\0')
			{
				const char* rpfPath = CAssetRef(AST_FragmentStore, i).GetRPFPathName();

				if (rpfPath[0] == '\0' || !StringMatch(rpfPath, rpf))
				{
					continue;
				}
			}

			if (slots.size() + 1 >= maxCount)
			{
				Displayf("FindAssets_FragmentStore: maxCount (%d) exceeded!", maxCount);
				return;
			}

			slots.PushAndGrow(i);
		}
	}
}

void FindAssets_ParticleStore(atArray<int>& slots, const char* searchStr, const char* rpf, const CAssetSearchParams& asp, int maxCount, bool bOnlyIfLoaded)
{
	const int indexMin = Max<int>(asp.m_scanAssetIndexRangeEnabled ? asp.m_scanAssetIndexRange[0] : 0, 0);
	const int indexMax = Min<int>(asp.m_scanAssetIndexRangeEnabled ? asp.m_scanAssetIndexRange[1] : 999999, g_ParticleStore.GetSize() - 1);

	for (int i = indexMin; i <= indexMax; i++)
	{
		if (!g_ParticleStore.IsValidSlot(strLocalIndex(i)))
		{
			continue;
		}

		if (StringMatch(g_ParticleStore.GetName(strLocalIndex(i)), searchStr, asp.m_exactNameMatch) && (!bOnlyIfLoaded || g_ParticleStore.GetSafeFromIndex(strLocalIndex(i)) != NULL))
		{
			if (rpf[0] != '\0')
			{
				const char* rpfPath = CAssetRef(AST_ParticleStore, i).GetRPFPathName();

				if (rpfPath[0] == '\0' || !StringMatch(rpfPath, rpf))
				{
					continue;
				}
			}

			if (slots.size() + 1 >= maxCount)
			{
				Displayf("FindAssets_ParticleStore: maxCount (%d) exceeded!", maxCount);
				return;
			}

			slots.PushAndGrow(i);
		}
	}
}

void FindAssets_CurrentDependentModelInfos(atArray<u32>& modelInfos, const CTxdRef& ref, const char* searchStr, const CAssetSearchParams& asp)
{
	atArray<u32> slots; FindAssets_ModelInfo(slots, searchStr, "", asp, 99999);

	for (int i = 0; i < slots.size(); i++)
	{
		const u32      modelInfoIndex = slots[i];
		const atString modelInfoDesc  = GetModelInfoDesc(modelInfoIndex);

		atArray<CTxdRef> refs; GetAssociatedTxds_ModelInfo(refs, CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelInfoIndex))), NULL, NULL);

		for (int j = 0; j < refs.size(); j++)
		{
			if (refs[j] == ref)
			{
				modelInfos.PushAndGrow(modelInfoIndex);
			}
		}
	}
}

void FindAssets_CurrentDependentModelInfos(atArray<u32>& modelInfos, const grcTexture* texture, const char* searchStr, const CAssetSearchParams& asp)
{
	atArray<u32> slots; FindAssets_ModelInfo(slots, searchStr, "", asp, 99999);

	for (int i = 0; i < slots.size(); i++)
	{
		const u32      modelInfoIndex = slots[i];
		const atString modelInfoDesc  = GetModelInfoDesc(modelInfoIndex);

		// we could store the shader names here too ..
		const char* shaderName    = "";
		const char* shaderVarName = "";

		if (IsTextureUsedByModel(texture, NULL, CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelInfoIndex))), NULL, &shaderName, &shaderVarName, true))
		{
			modelInfos.PushAndGrow(modelInfoIndex);
		}
	}
}

// for now, a simple and stupid way to compare if two textures are potentially the same (without looking at pixel data)
// note that they could be the same name but different pixel data, or the same pixel data but different name ..
static bool AreTexturesTheSame(const grcTexture* texture1, const grcTexture* texture2)
{
	if (texture1 == texture2)
	{
		return true;
	}
	else if (texture1 == NULL || texture2 == NULL)
	{
		return false; // wtf NULL textures?
	}

	if (texture1->GetWidth      () != texture2->GetWidth      () ||
		texture1->GetHeight     () != texture2->GetHeight     () ||
		texture1->GetImageFormat() != texture2->GetImageFormat())
	{
		return false;
	}

	if (stricmp(texture1->GetName(), texture2->GetName()) != 0)
	{
		return false;
	}

	return true;
}

void FindAssets_CurrentDependentTxdRefs(atArray<CTxdRef>& refs, const grcTexture* texture, bool bShowEmptyTxds)
{
	if (texture)
	{
		for (int i = 0; i < g_TxdStore.GetSize(); i++)
		{
			const fwTxd* txd = g_TxdStore.GetSafeFromIndex(strLocalIndex(i));

			if (txd && !g_TxdStore.GetSlot(strLocalIndex(i))->m_isDummy && (txd->GetCount() > 0 || bShowEmptyTxds))
			{
				for (int k = 0; k < txd->GetCount(); k++)
				{
					if (AreTexturesTheSame(texture, txd->GetEntry(k)))
					{
						refs.PushAndGrow(CTxdRef(AST_TxdStore, i, INDEX_NONE, ""));
					}
				}
			}
		}

		for (int i = 0; i < g_DwdStore.GetSize(); i++)
		{
			const Dwd* dwd = g_DwdStore.GetSafeFromIndex(strLocalIndex(i));

			if (dwd)
			{
				for (int j = 0; j < dwd->GetCount(); j++)
				{
					const Drawable* drawable = dwd->GetEntry(j);

					if (drawable)
					{
						const fwTxd* txd = drawable->GetTexDictSafe();

						if (txd && (txd->GetCount() > 0 || bShowEmptyTxds))
						{
							for (int k = 0; k < txd->GetCount(); k++)
							{
								if (AreTexturesTheSame(texture, txd->GetEntry(k)))
								{
									refs.PushAndGrow(CTxdRef(AST_DwdStore, i, j, ""));
								}
							}
						}
					}
				}
			}
		}

		for (int i = 0; i < g_DrawableStore.GetSize(); i++)
		{
			const Drawable* drawable = g_DrawableStore.GetSafeFromIndex(strLocalIndex(i));

			if (drawable)
			{
				const fwTxd* txd = drawable->GetTexDictSafe();

				if (txd && (txd->GetCount() > 0 || bShowEmptyTxds))
				{
					for (int k = 0; k < txd->GetCount(); k++)
					{
						if (AreTexturesTheSame(texture, txd->GetEntry(k)))
						{
							refs.PushAndGrow(CTxdRef(AST_DrawableStore, i, INDEX_NONE, ""));
						}
					}
				}
			}
		}

		for (int i = 0; i < g_FragmentStore.GetSize(); i++)
		{
			const Fragment* fragment = g_FragmentStore.GetSafeFromIndex(strLocalIndex(i));

			if (fragment)
			{
				// common drawable
				{
					const fwTxd* txd = fragment->GetCommonDrawable()->GetTexDictSafe();

					if (txd && (txd->GetCount() > 0 || bShowEmptyTxds))
					{
						for (int k = 0; k < txd->GetCount(); k++)
						{
							if (AreTexturesTheSame(texture, txd->GetEntry(k)))
							{
								refs.PushAndGrow(CTxdRef(AST_FragmentStore, i, INDEX_NONE, ""));
							}
						}
					}
				}

				if (fragment->GetClothDrawable()) // cloth drawable
				{
					const fwTxd* txd = fragment->GetClothDrawable()->GetTexDictSafe();

					if (txd && (txd->GetCount() > 0 || bShowEmptyTxds))
					{
						for (int k = 0; k < txd->GetCount(); k++)
						{
							if (AreTexturesTheSame(texture, txd->GetEntry(k)))
							{
								refs.PushAndGrow(CTxdRef(AST_FragmentStore, i, -2, ""));
							}
						}
					}
				}

				for (int j = 0; j < fragment->GetNumExtraDrawables(); j++) // extra drawables
				{
					const fwTxd* txd = fragment->GetExtraDrawable(j)->GetTexDictSafe();

					if (txd && (txd->GetCount() > 0 || bShowEmptyTxds))
					{
						for (int k = 0; k < txd->GetCount(); k++)
						{
							if (AreTexturesTheSame(texture, txd->GetEntry(k)))
							{
								refs.PushAndGrow(CTxdRef(AST_FragmentStore, i, j, ""));
							}
						}
					}
				}
			}
		}

		for (int i = 0; i < g_ParticleStore.GetSize(); i++)
		{
			ptxFxList* particle = g_ParticleStore.GetSafeFromIndex(strLocalIndex(i));

			if (particle)
			{
				const fwTxd* txd = particle->GetTextureDictionary();

				if (txd && (txd->GetCount() > 0 || bShowEmptyTxds))
				{
					for (int k = 0; k < txd->GetCount(); k++)
					{
						if (AreTexturesTheSame(texture, txd->GetEntry(k)))
						{
							refs.PushAndGrow(CTxdRef(AST_ParticleStore, i, INDEX_NONE, ""));
						}
					}
				}
			}
		}
	}
}

#if __DEV

void PrintAssets_ModelInfo(const char* searchStr, const CAssetSearchParams& asp)
{
	Displayf("PrintAssets_ModelInfo(\"%s\")", searchStr);

	atArray<u32> slots; FindAssets_ModelInfo(slots, searchStr, "", asp, 99999);

	for (int i = 0; i < slots.size(); i++)
	{
		const strLocalIndex     modelInfoIndex = strLocalIndex(slots[i]);
		const atString modelInfoDesc  = GetModelInfoDesc(modelInfoIndex.Get());

		Displayf("  %s", modelInfoDesc.c_str());
	}
}

void PrintAssets_TxdStore(const char* searchStr, const CAssetSearchParams& asp)
{
	Displayf("PrintAssets_TxdStore(\"%s\")", searchStr);

	atArray<int> slots; FindAssets_TxdStore(slots, searchStr, "", asp, 99999, false);

	for (int i = 0; i < slots.size(); i++)
	{
		Displayf("  %s", CAssetRef(AST_TxdStore, slots[i]).GetDesc().c_str());
	}
}

void PrintAssets_DwdStore(const char* searchStr, const CAssetSearchParams& asp)
{
	Displayf("PrintAssets_DwdStore(\"%s\")", searchStr);

	atArray<int> slots; FindAssets_DwdStore(slots, searchStr, "", asp, 99999, false);

	for (int i = 0; i < slots.size(); i++)
	{
		Displayf("  %s", CAssetRef(AST_DwdStore, slots[i]).GetDesc().c_str());
	}
}

void PrintAssets_DrawableStore(const char* searchStr, const CAssetSearchParams& asp)
{
	Displayf("PrintAssets_DrawableStore(\"%s\")", searchStr);

	atArray<int> slots; FindAssets_DrawableStore(slots, searchStr, "", asp, 99999, false);

	for (int i = 0; i < slots.size(); i++)
	{
		Displayf("  %s", CAssetRef(AST_DrawableStore, slots[i]).GetDesc().c_str());
	}
}

void PrintAssets_FragmentStore(const char* searchStr, const CAssetSearchParams& asp)
{
	Displayf("PrintAssets_FragmentStore(\"%s\")", searchStr);

	atArray<int> slots; FindAssets_FragmentStore(slots, searchStr, "", asp, 99999, false);

	for (int i = 0; i < slots.size(); i++)
	{
		Displayf("  %s", CAssetRef(AST_FragmentStore, slots[i]).GetDesc().c_str());
	}
}

void PrintAssets_ParticleStore(const char* searchStr, const CAssetSearchParams& asp)
{
	Displayf("PrintAssets_ParticleStore(\"%s\")", searchStr);

	atArray<int> slots; FindAssets_ParticleStore(slots, searchStr, "", asp, 99999, false);

	for (int i = 0; i < slots.size(); i++)
	{
		Displayf("  %s", CAssetRef(AST_ParticleStore, slots[i]).GetDesc().c_str());
	}
}

#endif // __DEV
#endif // __BANK
