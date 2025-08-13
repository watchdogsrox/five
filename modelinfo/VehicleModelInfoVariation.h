//
// VehicleModelInfoVariation.h
// Data file for vehicle customization
//
// Rockstar Games (c) 2011

#ifndef INC_VEHICLE_MODELINFO_VARIATION_H_
#define INC_VEHICLE_MODELINFO_VARIATION_H_

#include "atl/array.h"
#include "atl/bitset.h"
#include "atl/hashstring.h"
#include "data/base.h"
#include "entity/extensionlist.h"
#include "fwdrawlist/drawlistmgr.h"
#include "fwutil/Flags.h"
#include "parser/macros.h"

#include "scene/RegdRefTypes.h"
#include "streaming/streamingmodule.h"
#include "streaming/streamingrequest.h"
#include "modelinfo/VehicleModelInfoColors.h"
#include "modelinfo/VehicleModelInfoPlates.h"

#include "control/replay/ReplaySettings.h"

#define MAX_LINKED_MODS (15)

#define NUM_EXHAUST_BONES (VEH_LAST_EXHAUST - VEH_FIRST_EXHAUST + 1)
#define NUM_EXTRALIGHT_BONES (4)

class CVehicle;
class CCustomShaderEffectVehicleType;
class CScriptPreloadData;

namespace rage {
	class rmcDrawable;
	class fragType;
}

// stores a list of streaming requests for each mod slot
class CVehicleStreamRequestGfx : public fwExtension
{
	enum	state{ VS_INIT, VS_REQ, VS_USE, VS_FREED };

public:
#if !__SPU
	EXTENSIONID_DECL(CVehicleStreamRequestGfx, fwExtension);
#endif

						CVehicleStreamRequestGfx();
						~CVehicleStreamRequestGfx();

	FW_REGISTER_CLASS_POOL(CVehicleStreamRequestGfx); 

	void				RequestsComplete();
	void				Release();
	bool				HaveAllLoaded()						{ return m_reqFrags.HaveAllLoaded() && m_reqDrawables.HaveAllLoaded() && (m_state == VS_REQ) && (m_bDelayCompletion == false); }
	void				PushRequest()						{ Assert((m_state == VS_INIT) || (m_state == VS_FREED)); m_state = VS_REQ; }

	s32					AddFrag(u8 modSlot, const strStreamingObjectName fragName, s32 streamFlags, s32 parentTxd);
	s32					AddDrawable(eVehicleModType modSlot, const strStreamingObjectName drawableName, s32 streamFlags, s32 parentTxd);

	s32					GetFragId(u8 modSlot)	{ Assert(modSlot < VMT_RENDERABLE + MAX_LINKED_MODS); return m_reqFrags.GetRequestId(modSlot); }
	s32					GetDrawableId(eVehicleModType modSlot)	{ Assert(modSlot == VMT_WHEELS || modSlot == VMT_WHEELS_REAR_OR_HYDRAULICS); return m_reqDrawables.GetRequestId(modSlot - VMT_WHEELS); }

	void				SetTargetEntity(CVehicle *pEntity);
	CVehicle*			GetTargetEntity() const				{ return m_pTargetEntity; }

	void				SetDelayCompletion(bool bDelay)		{ m_bDelayCompletion = bDelay; }

private:
	CVehicle*			m_pTargetEntity;
	state				m_state;
	bool				m_bDelayCompletion;

	strRequestArray<VMT_RENDERABLE + MAX_LINKED_MODS>  m_reqFrags;
	strRequestArray<2>  m_reqDrawables;
};

// stores a list of streamed in mods to be used for rendering
class CVehicleStreamRenderGfx : public fwExtension 
{
#if !__NO_OUTPUT
	friend class CVehicleStreamRequestGfx;
#endif

public:
#if !__SPU
	EXTENSIONID_DECL(CVehicleStreamRenderGfx, fwExtension);
#endif

										CVehicleStreamRenderGfx();
										CVehicleStreamRenderGfx(CVehicleStreamRequestGfx* pReq, CVehicle* targetEntity);
										~CVehicleStreamRenderGfx();

	FW_REGISTER_CLASS_POOL(CVehicleStreamRenderGfx); 

	void								ReleaseGfx();

	fragType*							GetFrag(u8 modSlot) const;
	const fragType*						GetFragConst(u8 modSlot) const { return GetFrag(modSlot); }
	CCustomShaderEffectVehicleType*		GetFragCSEType(u8 modSlot) const;
	strLocalIndex						GetFragIndex(u8 modSlot) const;

	rmcDrawable*						GetDrawable(eVehicleModType modSlot) const;
	const rmcDrawable*					GetDrawableConst(eVehicleModType modSlot) const { return GetDrawable(modSlot); }
	CCustomShaderEffectVehicleType*		GetDrawableCSEType(eVehicleModType modSlot) const;
	strLocalIndex						GetDrawableIndex(eVehicleModType modSlot) const;

	float								GetWheelTyreRadiusScale() { return m_wheelTyreRadiusScale[0]; }
	float								GetWheelTyreWidthScale() { return m_wheelTyreWidthScale[0]; }
	float								GetRearWheelTyreRadiusScale() { return m_wheelTyreRadiusScale[1]; }
	float								GetRearWheelTyreWidthScale() { return m_wheelTyreWidthScale[1]; }

	void								OverrideWheelRimRadius(CVehicle* targetEntity) const;
	void								RestoreWheelRimRadius(CVehicle* targetEntity) const;

    void                                StoreModMatrices(CVehicle* pVeh);

	void								AddRefsToGfx();
	bool								ValidateStreamingIndexHasBeenCached();

	void								SetMatrix(u8 slot, const Matrix33& mat) { Assert(slot < VMT_RENDERABLE + MAX_LINKED_MODS); m_boneMatrices[slot] = mat; }
	const Matrix33&						GetMatrix(u8 slot) const { Assert(slot < VMT_RENDERABLE + MAX_LINKED_MODS); return m_boneMatrices[slot]; }
	void								CopyMatrices(const CVehicleStreamRenderGfx& rOther);

	u8									GetExhaustModIndex(eHierarchyId id) const;
	s8									GetExhaustBoneIndex(eHierarchyId id) const;
	u8									GetExtraLightModIndex(eHierarchyId id) const;
	s8									GetExtraLightBoneIndex(eHierarchyId id) const;
    u8                                  GetExtraLightCount() const;

	void AddRef() { m_refCount++; }
	void SetUsed(bool val);

	static void Init();
	static void Shutdown();

	static void RemoveRefs();

private:
	float								m_wheelTyreRadiusScale[2];	
	bool								m_usingWheelVariation[2];	
	
	Matrix33							m_boneMatrices[VMT_RENDERABLE + MAX_LINKED_MODS];

	CVehicleModVisible*					m_varModVisible[VMT_RENDERABLE];
	CVehicleModLink*					m_varModLinked[MAX_LINKED_MODS];
	CVehicleWheel*						m_varWheel[2];

	struct sExtraBoneData
	{
		s8 m_boneIndex;
		u8 m_modIndex;
	};
	sExtraBoneData						m_exhaustBones[NUM_EXHAUST_BONES];
	sExtraBoneData						m_extraLightBones[NUM_EXTRALIGHT_BONES];
	
	float								m_wheelTyreWidthScale[2];
	u16									m_refCount;
	bool								m_used;

#if !__NO_OUTPUT
	RegdVeh								m_targetEntity;
	u32									m_frameCreated;
#endif

	static atArray<CVehicleStreamRenderGfx*> ms_renderRefs[FENCE_COUNT];
	static s32 ms_fenceUpdateIdx;

#if __DEV
public:
	static void PoolFullCallback(void* pItem);
#endif //__DEV

};

#if !__DEV
inline bool CVehicleStreamRenderGfx::ValidateStreamingIndexHasBeenCached() { return true;}
#endif

// class that stores a variation instance generated from all future distribution parameters
class CVehicleVariationInstance
{
	friend class CVehicleModelInfo;
public:
						CVehicleVariationInstance();
						~CVehicleVariationInstance();

	static void			InitSystem();
	static void			ShutdownSystem();
	static void			ProcessStreamRequests();
	static void			RequestVehicleModFiles(CVehicle* pVehicle, s32 flags = 0);

	void				SetKitIndex(u16 kitIndex)
	{
	#if __FINAL
		m_kitIdx = kitIndex;
	#else
		XPARAM(disableVehMods);
		m_kitIdx = PARAM_disableVehMods.Get()? INVALID_VEHICLE_KIT_INDEX : kitIndex;
	#endif
	}

	u16					GetKitIndex() const
	{
	#if __FINAL
		return m_kitIdx;
	#else
		XPARAM(disableVehMods);
		return PARAM_disableVehMods.Get()? INVALID_VEHICLE_KIT_INDEX : m_kitIdx;
	#endif
	}
	const CVehicleKit*	GetKit() const;

    // *ForType functions will require mod indices per slot, e.g. 2 would mean third mod for that mod type
    // while the rest of the setters and getters work on the global mod list on the kit where 2 would mean third
    // mod in that kit (can be any type)
    u32                 GetNumModsForType(eVehicleModType modSlot, const CVehicle* pVehicle) const;
    void                SetModIndexForType(eVehicleModType modSlot, u8 modIndex, CVehicle* pVehicle, bool variation, u32 streamingFlags = 0);
    u8                  GetModIndexForType(eVehicleModType modSlot, const CVehicle* pVehicle, bool& variation) const;

	bool				IsGen9ExclusiveVehicleMod(eVehicleModType modSlot, u8 modIndex, const CVehicle* pVehicle) const;

	void	            SetPreloadModForType(eVehicleModType modSlot, u8 modIndex, CVehicle* pVehicle);
	bool	            HasPreloadModsFinished();
	void	            CleanUpPreloadMods(CVehicle* pVeh);
	void				ClearBonePositions();
	
	void				ApplyDeformation(CVehicle* pParentVehicle, const void* basePtr);

	void				SetBonePosition(u8 modIndex, const Vector3& position) { Assert(modIndex < VMT_RENDERABLE + MAX_LINKED_MODS); m_bonePositions[modIndex] = position; }
	Vector3				GetBonePosition(u8 modIndex) const { Assert(modIndex < VMT_RENDERABLE + MAX_LINKED_MODS); return m_bonePositions[modIndex]; }

	void				UpdateBonePositions(CVehicle* pVehicle);

	void				SetModIndex(eVehicleModType modSlot, u8 modIndex, CVehicle* pVehicle, bool variation, u32 streamingFlags = 0);
	u8					GetModIndex(eVehicleModType modSlot) const			{ return m_mods[modSlot]; }
	bool				SetLinkedModIndex(u8 modIndex, u8& index);
	u8					GetLinkedModIndex(u8 index) const					{ return m_linkedMods[index]; }
	s32					GetModBoneIndex(u8 modSlot) const;

	void				ToggleMod(eVehicleModType modSlot, bool toogleOn);
	bool				IsToggleModOn(eVehicleModType modSlot) const;

	u8					GetNumMods() const			{ return m_numMods; }
	const u8*			GetMods() const				{ return m_mods; }
	const u8*			GetLinkedMods() const		{ return m_linkedMods; }
	void				ClearMods();

#if GTA_REPLAY
	void				ClearLinkedModsForReplay();
#endif //GTA_REPLAY

	void				SetColor1(u8 col)			{ m_color1 = col; }
	void				SetColor2(u8 col)			{ m_color2 = col; }
	void				SetColor3(u8 col)			{ m_color3 = col; }
	void				SetColor4(u8 col)			{ m_color4 = col; }
	void				SetColor5(u8 col)			{ m_color5 = col; }
	void				SetColor6(u8 col)			{ m_color6 = col; }
	u8					GetColor1() const			{ return m_color1; }
	u8					GetColor2() const			{ return m_color2; }
	u8					GetColor3() const			{ return m_color3; }
	u8					GetColor4() const			{ return m_color4; }
	u8					GetColor5() const			{ return m_color5; }
	u8					GetColor6() const			{ return m_color6; }

	Color32				GetNeonColour() const		{ return m_neonColor; }
	void				SetNeonColour(Color32 c)	{ m_neonColor = c; }
	bool				IsNeonLOn() const			{ return m_neonLOn; }
	void				SetNeonLOn(bool on)			{ m_neonLOn = on; }
	bool				IsNeonROn() const			{ return m_neonROn; }
	void				SetNeonROn(bool on)			{ m_neonROn = on; }
	bool				IsNeonFOn() const			{ return m_neonFOn; }
	void				SetNeonFOn(bool on)			{ m_neonFOn = on; }
	bool				IsNeonBOn() const			{ return m_neonBOn; }
	void				SetNeonBOn(bool on)			{ m_neonBOn = on; }

	void				SetSmokeColorR(u8 r)		{ m_smokeColR = r; }
	void				SetSmokeColorG(u8 g)		{ m_smokeColG = g; }
	void				SetSmokeColorB(u8 b)		{ m_smokeColB = b; }
	u8					GetSmokeColorR() const		{ return m_smokeColR; }
	u8					GetSmokeColorG() const		{ return m_smokeColG; }
	u8					GetSmokeColorB() const		{ return m_smokeColB; }

	void				SetWindowTint(u8 col)		{ m_windowTint = col; }
	u8					GetWindowTint() const		{ return m_windowTint; }

	void				SetXenonLightColor(u8 col);
	u8					GetXenonLightColor() const	{ return m_xenonLightColor; }

	u32					GetIdentifierHashForType(eVehicleModType modSlot, u32 index) const;
	u32					GetModifierForType(eVehicleModType modSlot, u32 index) const;
	const char*			GetModShopLabelForType(eVehicleModType modSlot, u32 index, CVehicle* pVehicle) const;

    float               GetAudioApply(eVehicleModType modSlot) const;

	u32					GetEngineModifier() const	    { return GetModifier(VMT_ENGINE); }
	u32					GetBrakesModifier() const	    { return GetModifier(VMT_BRAKES); }
	u32					GetGearboxModifier() const	    { return GetModifier(VMT_GEARBOX); }
    u32					GetArmourModifier() const	    { return GetModifier(VMT_ARMOUR); } 
	u32					GetSuspensionModifier() const	{ return GetModifier(VMT_SUSPENSION); }

	u32					GetTotalWeightModifier() const;
	float				GetArmourDamageMultiplier() const;

	bool				HasCustomWheels() const			{ return m_mods[VMT_WHEELS] != INVALID_MOD; }
	bool				HasCustomRearWheels() const		{ return m_mods[VMT_WHEELS_REAR_OR_HYDRAULICS] != INVALID_MOD && m_canHaveRearWheels; }
	u32					GetHydraulicsModifier() const	{ return GetModifier(VMT_WHEELS_REAR_OR_HYDRAULICS); } //hack, reuse VMT_WHEELS_REAR_OR_HYDRAULICS for VMT_HYDRAULICS
	bool				HasSpoiler() const				{ return m_mods[VMT_SPOILER] != INVALID_MOD; }

	CVehicleStreamRenderGfx* GetVehicleRenderGfx() const { return m_renderGfx; }
	void				SetVehicleRenderGfx(CVehicleStreamRenderGfx* newRenderGfx);

	bool				HideModOnBone(const CVehicle* veh, s32 boneIndex, s8& slotTurnedOff);
	void				ShowModOnBone(const CVehicle* veh, s32 boneIndex);
	bool				IsBoneModed(eHierarchyId hierarchID);
	u8					GetModSlotFromBone(eHierarchyId hierarchID);
	void				ShowAllMods() { m_renderableIsVisible = 0; m_renderableIsVisible = ~m_renderableIsVisible; }
	bool				IsModVisible(u8 modIndex) const { Assert(modIndex < 64); return (m_renderableIsVisible & (BIT_64(modIndex))) != 0; }// added assert to make sure we dont overrun the bitfield
	bool				IsBoneTurnedOff(const CVehicle* veh, s32 boneIndex) const;

	s32					GetBone(u8 slot) const;
	s32					GetBoneForLinkedMod(u8 slot) const;
	s32					GetBoneToTurnOffForSlot(u8 slot, u32 index) const;
	s32					GetNumBonesToTurnOffForSlot(u8 slot) const;
	bool				ShouldDrawOriginalForSlot(CVehicle* veh, u8 slot) const;

	bool				CanHaveRearWheels() const { return m_canHaveRearWheels; }

    void                SetModColor1(eVehicleColorType type, int baseColIndex, int specColIndex);
    void                SetModColor2(eVehicleColorType type, int baseColIndex);
    void                GetModColor1(eVehicleColorType& type, int& baseColIndex, int& specColIndex);
    void                GetModColor2(eVehicleColorType& type, int& baseColIndex);
	const char*			GetModColor1Name(bool spec) const;
	const char*			GetModColor2Name() const;

	eVehicleWheelType	GetWheelType(const CVehicle* pVeh) const;
	void	            SetWheelType(eVehicleWheelType type);

	void				EnableVariation(u32 slot) { m_modVariations |= (1 << slot); }
	void				DisableVariation(u32 slot) { m_modVariations &= ~(1 << slot); }
	bool				HasVariation(u32 slot) const { return (m_modVariations & (1 << slot)) != -0; }

	bool				HaveModsStreamedIn(CVehicle* pVeh);

	void				SetModCollision(CVehicle* pVeh, u32 slot, bool on);

	void				PreRender2();

	bool				HasModMatrixBeenStoredThisFrame(u8 i) const;
	void				SetModMatrixStoredThisFrame(u8 i);

	static int ms_MaxVehicleStreamRequest;
	static int ms_MaxVehicleStreamRender;

private:
	u32					GetModifier(eVehicleModType modSlot) const;
	u32					GetWeightModifer(eVehicleModType modSlot) const;

    u8					GetGlobalModIndex(eVehicleModType modSlot, u8 modIndex, const CVehicle* pVehicle) const;
	Vector3				m_bonePositions[VMT_RENDERABLE + MAX_LINKED_MODS];

	CVehicleStreamRenderGfx* m_renderGfx;

	CScriptPreloadData* m_preloadData;

    eVehicleColorType m_colorType1;
    eVehicleColorType m_colorType2;
	s32 m_baseColorIndex; // these two are stored here, even if we have m_color1/2/3/4 because some color combos might contain the same indices
	s32 m_specColorIndex; // and we need to be able to identify the original combo index set.
	s32 m_baseColorIndex2; // colorType2 can only have a base color
	u64 m_renderableIsVisible;
	Color32 m_neonColor;
	fwFlags64 m_modMatricesStoredThisFrame;
	u16 m_modVariations; // wheels can have optional variations, this bitset keeps track of which have their enabled
	u8 m_mods[VMT_MAX];
	u8 m_linkedMods[MAX_LINKED_MODS];
	u8 m_numMods;
	u16 m_kitIdx;
	u8 m_color1, m_color2, m_color3, m_color4, m_color5, m_color6;
	u8 m_smokeColR, m_smokeColG, m_smokeColB;
	u8 m_windowTint;
	s8 m_wheelType;
	bool m_canHaveRearWheels;
	bool m_neonLOn;
	bool m_neonROn;
	bool m_neonFOn;
	bool m_neonBOn;
	u8 m_xenonLightColor;
	PAR_SIMPLE_PARSABLE;
};
CompileTimeAssert((VMT_RENDERABLE + MAX_LINKED_MODS) < 64); // bumped from 32 - hope this doesn't cause any issues

// class containing per vehicle data
class CVehicleVariationData BANK_ONLY(: public rage::datBase)
{
public:
	CVehicleVariationData();
	~CVehicleVariationData() {}

	const char* GetModelName() const										{ return m_modelName; }
	s32 GetNumColors() const												{ return m_colors.GetCount(); }
	CVehicleModelColorIndices& GetColor(u32 index)							{ return m_colors[index]; }
	const CVehicleModelColorIndices& GetColor(u32 index) const				{ return m_colors[index]; }

	s32 GetNumKits() const													{ return m_kits.GetCount(); }
	atHashString GetKit(u32 index) const									{ return m_kits[index]; }

	u8 GetLightSettings() const												{ return m_lightSettingsIndex; }
	void SetLightSettings(u32 value)										{ m_lightSettingsIndex = (u8)value&0xff; }

	u8 GetSirenSettings() const												{ return m_sirenSettingsIndex; }
	void SetSirenSettings(u32 value)										{ m_sirenSettingsIndex = (u8)value&0xff; }

	vehicleLightSettingsId GetLightSettingsId() const						{ return m_lightSettings; }
	void SetLightSettings(vehicleLightSettingsId id)						{ m_lightSettings = id; }

	u8 GetSirenSettingsId() const											{ return m_sirenSettings; }
	void SetSirenSettingsId(u8 id)											{ m_sirenSettings = id; }

	int GetNumWindowsWithExposedEdges() const								{ return m_windowsWithExposedEdges.GetCount(); }
	bool GetDoesWindowHaveExposedEdges(u32 hierarchyIdNameHash) const		{ for (int i = 0; i < m_windowsWithExposedEdges.GetCount(); i++) { if (m_windowsWithExposedEdges[i] == hierarchyIdNameHash) { return true; } } return false; }

	CVehicleModelPlateProbabilities& GetPlateProbabilities()				{ return m_plateProbabilities; }
	const CVehicleModelPlateProbabilities& GetPlateProbabilities() const 	{ return m_plateProbabilities; }

#if __BANK
	void SetModelName(const char* newName)			{ m_modelName = StringDuplicate(newName); }
	void ReserveColors(const int numColors)			{ m_colors.Reserve(numColors); m_selectedColorBit.Init(numColors); }
	CVehicleModelColorIndices& AppendColor()		{ return m_colors.Append(); }

	CVehicleModelColorIndices& AddColor();
	s32 GetWidgetSelectedColorIndex() const			{ return m_selectedColorBit.GetFirstBitIndex(); }
	void AddWidgets(bkBank & bank, const atArray<const char *>& colorNames, const atArray<const char *>& lightNames, const atArray<const char*>& sirenNames, const CVehicleModelInfoPlates& plateInfo);
	static void WidgetRemoveVehicleColorCB(CallbackData obj, CallbackData clientData);
	void WidgetRemoveVehicleColor(const int colorIndex);
	void WidgetChangedVehicleColor(CallbackData variationIndex);
	void WidgetChangedColorSelection(CallbackData variationIndex);
#endif // __BANK

	void OnPostLoad();
	void OnPreSave();

private:
	const char*							m_modelName;
	atArray<CVehicleModelColorIndices>	m_colors;
	atArray<atHashString>				m_kits;
	atArray<atHashString>				m_windowsWithExposedEdges; // hashed vehicle hierarchy id names
	CVehicleModelPlateProbabilities		m_plateProbabilities;
	u8									m_lightSettingsIndex;
	u8									m_sirenSettingsIndex;
	vehicleLightSettingsId				m_lightSettings;
	sirenSettingsId						m_sirenSettings;

#if __BANK
	atBitSet							m_selectedColorBit;	// bitmask (only one bit set at a time)
#endif // __BANK

	PAR_SIMPLE_PARSABLE;
};

// main class that contains all vehicle variation data
class CVehicleModelInfoVariation BANK_ONLY(: public rage::datBase)
{
public:
	CVehicleModelInfoVariation();
	~CVehicleModelInfoVariation();

	atArray<CVehicleVariationData>& GetVariationData() { return m_variationData; }

#if __BANK
	void AddWidgets(rage::bkBank & bank);
	void RefreshVehicleWidgets();

	void ReshuffleColors(s32 editingIndex);
	void ReshuffleLights(s32 editingIndex);
	void ReshuffleSirens(s32 editingIndex);
#endif // __BANK

	void OnPostLoad();
	void OnPreSave();

private:
	atArray<CVehicleVariationData> m_variationData;


#if __BANK
	void DeleteWidgetNames();
	void WidgetAddVehicleColor();
	void WidgetCopyColorsFromVehicle();

	s32											m_vehicleModelEditingIndex;
	s32											m_vehicleCopyFromIndex;
	bkGroup*									m_vehicleModelBankGroup;
	bkGroup*									m_vehicleCurrentModelBankGroup;
	atArray<const char*>						m_vehicleNames; // extracted for widgets
	atArray<u16>								m_vehicleIndices;
#endif

	PAR_SIMPLE_PARSABLE;
};

#endif // INC_VEHICLE_MODELINFO_VARIATION_H_
