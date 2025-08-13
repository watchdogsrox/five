// ===========================
// debug/DebugArchetypeProxy.h
// (c) 2012 RockstarNorth
// ===========================

#ifndef _DEBUG_DEBUGARCHETYPEPROXY_H_
#define _DEBUG_DEBUGARCHETYPEPROXY_H_

#define USE_DEBUG_ARCHETYPE_PROXIES (1 && __BANK)

#if __BANK

#include "modelinfo/BaseModelInfo.h"
#include "modelinfo/PedModelInfo.h"
#include "modelinfo/VehicleModelInfo.h"

class CDebugArchetypeProxy;
class CDebugArchetype
{
public:
#if USE_DEBUG_ARCHETYPE_PROXIES
	static bool AreDebugArchetypeProxiesBeingCreated();
	static void CreateDebugArchetypeProxies();
	static void CheckDebugArchetypeProxy(const CBaseModelInfo* pModelInfo);
#endif // USE_DEBUG_ARCHETYPE_PROXIES
	static u32 GetMaxDebugArchetypeProxies(); // use this instead of GetMaxModelInfos
	static const CDebugArchetypeProxy* GetDebugArchetypeProxy(u32 i); // use this instead of GetBaseModelInfo(fwModelId(i))
	static const CDebugArchetypeProxy* GetDebugArchetypeProxyForHashKey(u32 hashKey, int* out_proxyIndex = NULL);
	static const CDebugArchetypeProxy* GetDebugArchetypeProxyForModelInfo(const CBaseModelInfo* pModelInfo, int* out_proxyIndex = NULL);
	static CBaseModelInfo* GetBaseModelInfoForProxy(const CDebugArchetypeProxy* proxy);
};

#endif // __BANK

#if USE_DEBUG_ARCHETYPE_PROXIES

enum
{
	DEBUG_ARCHETYPE_PROXY_STATIC            = BIT(0),
	DEBUG_ARCHETYPE_PROXY_IS_PROP           = BIT(1),
	DEBUG_ARCHETYPE_PROXY_IS_TREE           = BIT(2),
	DEBUG_ARCHETYPE_PROXY_DONT_CAST_SHADOWS = BIT(3),
	DEBUG_ARCHETYPE_PROXY_SHADOW_PROXY      = BIT(4),
	DEBUG_ARCHETYPE_PROXY_HAS_UV_ANIMATION  = BIT(5),
};

class CDebugArchetypeProxy
{
private:
	friend class CDebugArchetype;
	void Create(const CBaseModelInfo* pModelInfo, u32 proxyIndex, int mapTypeSlot, bool bStaticArchetype);
	bool Check(const CBaseModelInfo* pModelInfo) const;

public:
	static int FindProxyIndex(u32 hashKey);

	const char* GetModelName          () const { return m_modelName.GetCStr(); }
	u32         GetHashKey            () const { return m_hashKey; }
	int         GetMapTypeSlot        () const { return m_mapTypeSlot; }
	u8          GetModelType          () const { return m_type; }
	bool        GetIsStaticArchetype  () const { return (m_proxyFlags & DEBUG_ARCHETYPE_PROXY_STATIC           ) != 0; }
	bool        GetIsProp             () const { return (m_proxyFlags & DEBUG_ARCHETYPE_PROXY_IS_PROP          ) != 0; }
	bool        GetIsTree             () const { return (m_proxyFlags & DEBUG_ARCHETYPE_PROXY_IS_TREE          ) != 0; }
	bool        GetDontCastShadows    () const { return (m_proxyFlags & DEBUG_ARCHETYPE_PROXY_DONT_CAST_SHADOWS) != 0; }
	bool        GetIsShadowProxy      () const { return (m_proxyFlags & DEBUG_ARCHETYPE_PROXY_SHADOW_PROXY     ) != 0; }
	bool        GetHasAnimUV          () const { return (m_proxyFlags & DEBUG_ARCHETYPE_PROXY_HAS_UV_ANIMATION ) != 0; }
	float       GetHDTextureDistance  () const { return m_HDTexDistance; }
	float       GetLodDistanceUnscaled() const { return m_lodDistance; }
	bool        GetHasLoaded          () const;

	fwArchetype::DrawableType GetDrawableType() const { return (fwArchetype::DrawableType)m_drawableType; }

	s32 SafeGetDrawableIndex() const;
	s32 SafeGetDrawDictIndex() const;
	s32 SafeGetFragmentIndex() const;

	s32 SafeGetDrawDictEntry_IfLoaded() const;

	s32 GetAssetParentTxdIndex() const;
	s32 GetPtFxAssetSlot() const { return m_ptfxAssetSlot; }

	rmcDrawable* GetDrawable() const;

private:
	atHashString m_modelName;
	u32          m_hashKey;
	s16          m_mapTypeSlot; // -1 for static archetypes
	u16          m_proxyFlags;
	float        m_HDTexDistance;
	float        m_lodDistance;
	u8           m_type;
	u8           m_drawableType;
	s32          m_drawableIndex; // slot index in g_DwdStore, g_DrawableStore or g_FragmentStore
	s32          m_ptfxAssetSlot;
};

class CDebugPedArchetypeProxy : public CDebugArchetypeProxy
{
private:
	friend class CDebugArchetypeProxy;
	void Create(const CPedModelInfo* pedModelInfo);
	bool Check(const CPedModelInfo* pedModelInfo) const;

public:
	s32  GetHDTxdIndex             () const { return m_HDtxdIdx; }
	s32  GetPedComponentFileIndex  () const { return m_pedCompFileIndex; }
	s32  GetPropsFileIndex         () const { return m_propsFileIndex; }
	bool HasStreamedProps          () const { return strcmp(GetPropStreamFolder(), "null") != 0;}
	bool GetIsStreamedGfx          () const { return m_isStreamedGraphics; }
	const char* GetPropStreamFolder() const { return m_propStreamFolder; }

private:
	s32  m_HDtxdIdx;
	s32  m_pedCompFileIndex;
	s32  m_propsFileIndex;
	bool m_isStreamedGraphics : 1;
	const char* m_propStreamFolder;
};

class CDebugVehicleArchetypeProxy : public CDebugArchetypeProxy
{
private:
	friend class CDebugArchetypeProxy;
	void Create(const CVehicleModelInfo* pVehicleModelInfo);
	bool Check(const CVehicleModelInfo* pVehicleModelInfo) const;

public:
	s32 GetHDFragmentIndex() const { return m_HDfragIdx; }
	s32 GetHDTxdIndex     () const { return m_HDtxdIdx; }

private:
	s32 m_HDfragIdx;
	s32 m_HDtxdIdx;
};

class CDebugWeaponArchetypeProxy : public CDebugArchetypeProxy
{
private:
	friend class CDebugArchetypeProxy;
	void Create(const CWeaponModelInfo* pWeaponModelInfo);
	bool Check(const CWeaponModelInfo* pWeaponModelInfo) const;

public:
	s32 GetHDDrawableIndex() const { return m_HDdrawIdx; }
	s32 GetHDTxdIndex     () const { return m_HDtxdIdx; }

private:
	s32 m_HDdrawIdx;
	s32 m_HDtxdIdx;
};

#elif __BANK

class CDebugArchetypeProxy : protected CBaseModelInfo
{
public:
	const char* GetModelName          () const { return CBaseModelInfo::GetModelName(); }
	u8          GetModelType          () const { return CBaseModelInfo::GetModelType(); }
	int         GetMapTypeSlot        () const { return -1; } // not supported
	u32         GetHashKey            () const { return CBaseModelInfo::GetHashKey(); }	
	bool        GetIsStaticArchetype  () const { return true; }
	bool        GetIsProp             () const { return CBaseModelInfo::GetIsProp(); }
	bool        GetIsTree             () const { return CBaseModelInfo::GetIsTree(); }
	bool        GetDontCastShadows    () const { return CBaseModelInfo::GetDontCastShadows(); }
	bool        GetIsShadowProxy      () const { return CBaseModelInfo::GetIsShadowProxy(); }
	bool        GetHasAnimUV          () const { return CBaseModelInfo::GetClipDictionaryIndex() != -1 && CBaseModelInfo::GetHasUvAnimation(); }
	float       GetHDTextureDistance  () const { return CBaseModelInfo::GetHDTextureDistance(); }
	float       GetLodDistanceUnscaled() const { return CBaseModelInfo::GetLodDistanceUnscaled(); }
	bool        GetHasLoaded          () const { return CBaseModelInfo::GetHasLoaded(); }

	DrawableType GetDrawableType() const { return CBaseModelInfo::GetDrawableType(); }

	s32 SafeGetDrawableIndex() const { return CBaseModelInfo::SafeGetDrawableIndex().Get(); }
	s32 SafeGetDrawDictIndex() const { return CBaseModelInfo::SafeGetDrawDictIndex().Get(); }
	s32 SafeGetFragmentIndex() const { return CBaseModelInfo::SafeGetFragmentIndex().Get(); }

	s32 SafeGetDrawDictEntry_IfLoaded() const { return CBaseModelInfo::SafeGetDrawDictEntry().Get(); }

	s32 GetAssetParentTxdIndex() const { return CBaseModelInfo::GetAssetParentTxdIndex().Get(); }
	s32 GetPtFxAssetSlot() const { return CBaseModelInfo::GetPtFxAssetSlot().Get(); }

	rmcDrawable* GetDrawable() const { return CBaseModelInfo::GetDrawable(); }
};

class CDebugPedArchetypeProxy : public CPedModelInfo
{
public:
	s32  GetHDTxdIndex           () const { return CPedModelInfo::GetHDTxdIndex(); }
	s32  GetPedComponentFileIndex() const { return CPedModelInfo::GetPedComponentFileIndex(); }
	s32  GetPropsFileIndex       () const { return CPedModelInfo::GetPropsFileIndex(); }
	bool GetIsStreamedGfx        () const { return CPedModelInfo::GetIsStreamedGfx(); }
};

class CDebugVehicleArchetypeProxy : public CVehicleModelInfo
{
public:
	s32 GetHDFragmentIndex() const { return CVehicleModelInfo::GetHDFragmentIndex(); }
	s32 GetHDTxdIndex     () const { return CVehicleModelInfo::GetHDTxdIndex(); }
};

class CDebugWeaponArchetypeProxy : public CWeaponModelInfo
{
public:
	s32 GetHDDrawableIndex() const { return CWeaponModelInfo::GetHDDrawableIndex(); }
	s32 GetHDTxdIndex     () const { return CWeaponModelInfo::GetHDTxdIndex(); }
};

#endif // __BANK

#endif // _DEBUG_DEBUGARCHETYPEPROXY_H_
