//
// VehicleModelInfoMods.h
// Data file for vehicle mod settings
//
// Rockstar Games (c) 2013

#ifndef INC_VEHICLE_MODELINFO_MODS_H_
#define INC_VEHICLE_MODELINFO_MODS_H_ 

#include "atl/array.h"
#include "atl/hashstring.h"
#include "data/base.h"
#include "parser/macros.h"
#include "renderer/color.h"
#include "renderer/HierarchyIds.h"
#include "streaming/streamingdefs.h"
//#include "system/noncopyable.h"

//#include "modelinfo/VehicleModelInfoColors.h"
//#include "modelinfo/VehicleModelInfoPlates.h"
//#include "modelinfo/VehicleModelInfoLights.h"
//#include "modelinfo/VehicleModelInfoSirens.h"
#include "modelinfo/VehicleModelInfoEnums.h"

class CVehicleStreamRenderGfx;
class CCustomShaderEffectVehicleType;

#define INVALID_MOD		((u8)-1)
//#define INVALID_KIT		((u16)-1)   // Ambiguous, please use either INVALID_VEHICLE_KIT_INDEX or INVALID_VEHICLE_KIT_ID instead
#define INVALID_KIT_U8	((u8)-1)        // ...unless for save / replay compatibility, then use this one.

class CVehicleModColor
{
public:
	ConstString name;
    u8 col;
    u8 spec;

	PAR_SIMPLE_PARSABLE;
};
    
class CVehicleModPearlescentColors
{
public:
    atArray<CVehicleModColor> m_baseCols;
    atArray<CVehicleModColor> m_specCols;

	PAR_SIMPLE_PARSABLE;
};

class CVehicleModColors
{
public:
    atArray<CVehicleModColor> m_metallic;
    atArray<CVehicleModColor> m_classic;
    atArray<CVehicleModColor> m_matte;
    atArray<CVehicleModColor> m_metals;
    atArray<CVehicleModColor> m_chrome;

    CVehicleModPearlescentColors m_pearlescent;

	PAR_SIMPLE_PARSABLE;
};

class CVehicleModColorsGen9
{
public:
	atArray<CVehicleModColor> m_chameleon;

	PAR_SIMPLE_PARSABLE;
};

// describes a single renderable vehicle mod
class CVehicleModVisible
{
	friend class CVehicleStreamRenderGfx;

public:

	// enum to map bone vehicle bones names to bone ids. only the ones likely to be used for mods are exposed.
	// can add new ones here if needed
	enum eVehicleModBone
	{
		none			= -1,
		chassis			= VEH_CHASSIS,
		bodyshell		= VEH_BODYSHELL,
		bumper_f		= VEH_BUMPER_F,
		bumper_r		= VEH_BUMPER_R,
		wing_rf			= VEH_WING_RF,
		wing_lf			= VEH_WING_LF,
		bonnet			= VEH_BONNET,
		boot			= VEH_BOOT,
		exhaust			= VEH_EXHAUST,
		exhaust_2		= VEH_EXHAUST_2,
		exhaust_3		= VEH_EXHAUST_3,
		exhaust_4		= VEH_EXHAUST_4,
		exhaust_5		= VEH_EXHAUST_5,
		exhaust_6		= VEH_EXHAUST_6,
		exhaust_7		= VEH_EXHAUST_7,
		exhaust_8		= VEH_EXHAUST_8,
		exhaust_9		= VEH_EXHAUST_9,
		exhaust_10		= VEH_EXHAUST_10,
		exhaust_11		= VEH_EXHAUST_11,
		exhaust_12		= VEH_EXHAUST_12,
		exhaust_13		= VEH_EXHAUST_13,
		exhaust_14		= VEH_EXHAUST_14,
		exhaust_15		= VEH_EXHAUST_15,
		exhaust_16		= VEH_EXHAUST_16,
		extra_1			= VEH_EXTRA_1,
		extra_2			= VEH_EXTRA_2,
		extra_3			= VEH_EXTRA_3,
		extra_4			= VEH_EXTRA_4,
		extra_5			= VEH_EXTRA_5,
		extra_6			= VEH_EXTRA_6,
		extra_7			= VEH_EXTRA_7,
		extra_8			= VEH_EXTRA_8,
		extra_9			= VEH_EXTRA_9,
		extra_10		= VEH_EXTRA_10,
		extra_11		= VEH_EXTRA_11,
		extra_12		= VEH_EXTRA_12,
		break_extra_1	= VEH_BREAKABLE_EXTRA_1,
		break_extra_2	= VEH_BREAKABLE_EXTRA_2,
		break_extra_3	= VEH_BREAKABLE_EXTRA_3,
		break_extra_4	= VEH_BREAKABLE_EXTRA_4,
		break_extra_5	= VEH_BREAKABLE_EXTRA_5,
		break_extra_6	= VEH_BREAKABLE_EXTRA_6,
		break_extra_7	= VEH_BREAKABLE_EXTRA_7,
		break_extra_8	= VEH_BREAKABLE_EXTRA_8,
		break_extra_9	= VEH_BREAKABLE_EXTRA_9,
		break_extra_10	= VEH_BREAKABLE_EXTRA_10,
		mod_col_1		= VEH_MOD_COLLISION_1,
		mod_col_2		= VEH_MOD_COLLISION_2,
		mod_col_3		= VEH_MOD_COLLISION_3,
		mod_col_4		= VEH_MOD_COLLISION_4,
		mod_col_5		= VEH_MOD_COLLISION_5,
		mod_col_6		= VEH_MOD_COLLISION_6,
		mod_col_7		= VEH_MOD_COLLISION_7,
		mod_col_8		= VEH_MOD_COLLISION_8,
		mod_col_9		= VEH_MOD_COLLISION_9,
		mod_col_10		= VEH_MOD_COLLISION_10,
		mod_col_11		= VEH_MOD_COLLISION_11,
		mod_col_12		= VEH_MOD_COLLISION_12,
		mod_col_13		= VEH_MOD_COLLISION_13,
		mod_col_14		= VEH_MOD_COLLISION_14,
		mod_col_15		= VEH_MOD_COLLISION_15,
		mod_col_16		= VEH_MOD_COLLISION_16,
		misc_a			= VEH_MISC_A,
		misc_b			= VEH_MISC_B,
		misc_c			= VEH_MISC_C,
		misc_d			= VEH_MISC_D,
		misc_e			= VEH_MISC_E,
		misc_f			= VEH_MISC_F,
		misc_g			= VEH_MISC_G,
		misc_h			= VEH_MISC_H,
		misc_i			= VEH_MISC_I,
		misc_j			= VEH_MISC_J,
		misc_k			= VEH_MISC_K,
		misc_l			= VEH_MISC_L,
		misc_m			= VEH_MISC_M,
		misc_n			= VEH_MISC_N,
		misc_o			= VEH_MISC_O,
		misc_p			= VEH_MISC_P,
		misc_q			= VEH_MISC_Q,
		misc_r			= VEH_MISC_R,
		misc_s			= VEH_MISC_S,
		misc_t			= VEH_MISC_T,
		misc_u			= VEH_MISC_U,
		misc_v			= VEH_MISC_V,
		misc_w			= VEH_MISC_W,
		misc_x			= VEH_MISC_X,
		misc_y			= VEH_MISC_Y,
		misc_z			= VEH_MISC_Z,
		misc_1			= VEH_MISC_1,
		misc_2			= VEH_MISC_2,
		handlebars		= VEH_STEERING_WHEEL,
		swingarm		= VEH_TRANSMISSION_R,
		forks_u			= VEH_SUSPENSION_LF,
		forks_l			= VEH_SUSPENSION_RF,
		headlight_l		= VEH_HEADLIGHT_L,
		headlight_r		= VEH_HEADLIGHT_R,
		indicator_lr	= VEH_INDICATOR_LR,
		indicator_lf	= VEH_INDICATOR_LF,
		indicator_rr	= VEH_INDICATOR_RR,
		indicator_rf	= VEH_INDICATOR_RF,
		taillight_l		= VEH_TAILLIGHT_L,
		taillight_r		= VEH_TAILLIGHT_R,
		window_lf		= VEH_WINDOW_LF,
		window_rf		= VEH_WINDOW_RF,
		window_rr		= VEH_WINDOW_RR,
		window_lr		= VEH_WINDOW_LR,
		window_lm		= VEH_WINDOW_LM,
		window_rm		= VEH_WINDOW_RM,
		hub_lf			= VEH_WHEELHUB_LF,
		hub_rf			= VEH_WHEELHUB_RF,
        windscreen_r    = VEH_WINDSCREEN_R,

		turret_1		= VEH_TURRET_1_MOD,
		turret_2		= VEH_TURRET_2_MOD,
		turret_3		= VEH_TURRET_3_MOD,
		turret_4		= VEH_TURRET_4_MOD,

		mod_a			= VEH_MOD_A,
		mod_b			= VEH_MOD_B,
		mod_c			= VEH_MOD_C,
		mod_d			= VEH_MOD_D,
		mod_e			= VEH_MOD_E,
		mod_f			= VEH_MOD_F,
		mod_g			= VEH_MOD_G,
		mod_h			= VEH_MOD_H,
		mod_i			= VEH_MOD_I,
		mod_j			= VEH_MOD_J,
		mod_k			= VEH_MOD_K,
		mod_l			= VEH_MOD_L,
		mod_m			= VEH_MOD_M,
		mod_n			= VEH_MOD_N,
		mod_o			= VEH_MOD_O,
		mod_p			= VEH_MOD_P,
		mod_q			= VEH_MOD_Q,
		mod_r			= VEH_MOD_R,
		mod_s			= VEH_MOD_S,
		mod_t			= VEH_MOD_T,
		mod_u			= VEH_MOD_U,
		mod_v			= VEH_MOD_V,
		mod_w			= VEH_MOD_W,
		mod_x			= VEH_MOD_X,
		mod_y			= VEH_MOD_Y,
		mod_z			= VEH_MOD_Z,

		mod_aa			= VEH_MOD_AA,
		mod_ab			= VEH_MOD_AB,
		mod_ac			= VEH_MOD_AC,
		mod_ad			= VEH_MOD_AD,
		mod_ae			= VEH_MOD_AE,
		mod_af			= VEH_MOD_AF,
		mod_ag			= VEH_MOD_AG,
		mod_ah			= VEH_MOD_AH,
		mod_ai			= VEH_MOD_AI,
		mod_aj			= VEH_MOD_AJ,
		mod_ak			= VEH_MOD_AK,

		turret_a1		= VEH_TURRET_A1_MOD,
		turret_a2		= VEH_TURRET_A2_MOD,
		turret_a3		= VEH_TURRET_A3_MOD,
		turret_a4		= VEH_TURRET_A4_MOD,

		turret_b1		= VEH_TURRET_B1_MOD,
		turret_b2		= VEH_TURRET_B2_MOD,
		turret_b3		= VEH_TURRET_B3_MOD,
		turret_b4		= VEH_TURRET_B4_MOD,

		// Extra arena mode bones
		rblade_1mod		= VEH_BLADE_R_1_MOD,
		rblade_1fast	= VEH_BLADE_R_1_FAST,

		rblade_2mod		= VEH_BLADE_R_2_MOD,
		rblade_2fast	= VEH_BLADE_R_2_FAST,

		rblade_3mod		= VEH_BLADE_R_3_MOD,
		rblade_3fast	= VEH_BLADE_R_3_FAST,

		fblade_1mod		= VEH_BLADE_F_1_MOD,
		fblade_1fast	= VEH_BLADE_F_1_FAST,

		fblade_2mod		= VEH_BLADE_F_2_MOD,
		fblade_2fast	= VEH_BLADE_F_2_FAST,

		fblade_3mod		= VEH_BLADE_F_3_MOD,
		fblade_3fast	= VEH_BLADE_F_3_FAST,

		sblade_1mod		= VEH_BLADE_S_1_MOD,
		sblade_l_1fast	= VEH_BLADE_S_1_L_FAST,
		sblade_r_1fast	= VEH_BLADE_S_1_R_FAST,

		sblade_2mod		= VEH_BLADE_S_2_MOD,
		sblade_l_2fast	= VEH_BLADE_S_2_L_FAST,
		sblade_r_2fast	= VEH_BLADE_S_2_R_FAST,

		sblade_3mod		= VEH_BLADE_S_3_MOD,
		sblade_l_3fast	= VEH_BLADE_S_3_L_FAST,
		sblade_r_3fast	= VEH_BLADE_S_3_R_FAST,

		spike_1mod		= VEH_SPIKE_1_MOD,
		spike_1ped_col	= VEH_SPIKE_1_PED,
		spike_1car_col	= VEH_SPIKE_1_CAR,

		spike_2mod		= VEH_SPIKE_2_MOD,
		spike_2ped_col	= VEH_SPIKE_2_PED,
		spike_2car_col	= VEH_SPIKE_2_CAR,

		spike_3mod		= VEH_SPIKE_3_MOD,
		spike_3ped_col	= VEH_SPIKE_3_PED,
		spike_3car_col	= VEH_SPIKE_3_CAR,

		scoop_1mod		= VEH_SCOOP_1_MOD,
		scoop_2mod		= VEH_SCOOP_2_MOD,
		scoop_3mod		= VEH_SCOOP_3_MOD,

		miscwobble_1	= VEH_BOBBLE_MISC_1,
		miscwobble_2	= VEH_BOBBLE_MISC_2,
		miscwobble_3	= VEH_BOBBLE_MISC_3,
		miscwobble_4	= VEH_BOBBLE_MISC_4,
		miscwobble_5	= VEH_BOBBLE_MISC_5,
		miscwobble_6	= VEH_BOBBLE_MISC_6,
		miscwobble_7	= VEH_BOBBLE_MISC_7,
		miscwobble_8	= VEH_BOBBLE_MISC_8,

		supercharger_1  = VEH_SUPERCHARGER_1,
		supercharger_2  = VEH_SUPERCHARGER_2,
		supercharger_3  = VEH_SUPERCHARGER_3,

        reversinglight_l = VEH_REVERSINGLIGHT_L,
        reversinglight_r = VEH_REVERSINGLIGHT_R,
	};

	CVehicleModVisible() : m_type(VMT_SPOILER), m_bone(chassis), m_collisionBone(none), m_weight(0), m_turnOffExtra(false), m_disableBonnetCamera(false), m_allowBonnetSlide(true), m_weaponSlot(-1), m_weaponSlotSecondary(-1), m_disableProjectileDriveby(false), m_disableDriveby(false), m_disableDrivebySeat(-1), m_VariationFragIdx(-1), m_VariationFragCseType(NULL)	{}
	~CVehicleModVisible() {}

	u32 GetNameHash() const { return m_modelName.GetHash(); }
	atHashString GetNameHashString() const { return m_modelName; }

	u32 GetNumLinkedModels() const { return m_linkedModels.GetCount(); }
	u32 GetLinkedModelHash(u32 model ) const { return m_linkedModels[model].GetHash(); }
	atHashString GetLinkedModelHashString(u32 model ) const { return m_linkedModels[model]; }

	eVehicleModType GetType() const { return m_type; }
	eVehicleModBone GetBone() const { return m_bone; }
	eVehicleModBone GetCollisionBone() const { return m_collisionBone; }
	u32 GetNumBonesToTurnOff() const { return m_turnOffBones.GetCount(); }
	eVehicleModBone GetBoneToTurnOff(u32 index) const { return m_turnOffBones[index]; }
	eVehicleModCameraPos GetCameraPos() const { return m_cameraPos; }
	u8 GetWeightModifier() const { return m_weight; }
	bool GetTurnOffExtra() const { return m_turnOffExtra; }
	bool IsBonnetCameraDisabled() const { return m_disableBonnetCamera; }
	bool AllowBonnetSlide() const { return m_allowBonnetSlide; }
	s8 GetWeaponSlot() const { return m_weaponSlot; }
	s8 GetWeaponSlotSecondary() const { return m_weaponSlotSecondary; }
	bool IsProjectileDriveByDisabled() const { return m_disableProjectileDriveby; }
	bool IsDriveByDisabled() const { return m_disableDriveby; }
	s32 GetDrivebyDisabledSeat() const { return m_disableDrivebySeat; }
	s32 GetDrivebyDisabledSeatSecondary() const { return m_disableDrivebySeatSecondary; }

	const char* GetModShopLabel() const { return m_modShopLabel.c_str(); }

    float GetAudioApply() const { return m_audioApply; }

private:
	// CVehicleModelInfoVariation:
	strLocalIndex						m_VariationFragIdx;
	CCustomShaderEffectVehicleType*		m_VariationFragCseType;
private:
	void PostLoad();

	atHashString m_modelName;
	ConstString m_modShopLabel;
	atArray<atHashString> m_linkedModels;
	eVehicleModType m_type;
	eVehicleModBone m_bone;
	eVehicleModBone m_collisionBone;
	atArray<eVehicleModBone> m_turnOffBones;
	eVehicleModCameraPos m_cameraPos;
    float m_audioApply;
	u8 m_weight;
	bool m_turnOffExtra;
	bool m_disableBonnetCamera;
	bool m_allowBonnetSlide;
	s8 m_weaponSlot;
	s8 m_weaponSlotSecondary;
	bool m_disableProjectileDriveby;
	bool m_disableDriveby;
	s32 m_disableDrivebySeat;
	s32 m_disableDrivebySeatSecondary;

	PAR_SIMPLE_PARSABLE;
};

// a linked renderable mod that cannot be selected by its own
class CVehicleModLink
{
	friend class CVehicleStreamRenderGfx;

public:
	CVehicleModLink() : m_bone(CVehicleModVisible::chassis), m_turnOffExtra(false), m_VariationFragIdx(-1), m_VariationFragCseType(NULL)	{}
	~CVehicleModLink() {}

	u32 GetNameHash() const { return m_modelName.GetHash(); }
	atHashString GetNameHashString() const { return m_modelName; }

	CVehicleModVisible::eVehicleModBone GetBone() const { return m_bone; }
	bool GetTurnOffExtra() const { return m_turnOffExtra; }

private:
	// CVehicleModelInfoVariation:
	strLocalIndex						m_VariationFragIdx;
	CCustomShaderEffectVehicleType*		m_VariationFragCseType;
private:
	atHashString m_modelName;
	CVehicleModVisible::eVehicleModBone m_bone;
	bool m_turnOffExtra;

	PAR_SIMPLE_PARSABLE;
};

// describes a single stats vehicle mod. it doesn't have a drawable only stat modifiers
class CVehicleModStat
{
public:
	CVehicleModStat() {}
	~CVehicleModStat() {}

	u32 GetModifier() const { return m_modifier; }
	u8 GetWeightModifier() const { return m_weight; }
	eVehicleModType GetType() const { return m_type; }

    float GetAudioApply() const { return m_audioApply; }

	u32 GetIdentifierHash() const { return m_identifier.GetHash(); }
	atHashString GetIdentifierString() const { return m_identifier; }

private:
	atHashString m_identifier;
	eVehicleModType m_type;
	u32 m_modifier;
    float m_audioApply;
	u8 m_weight;

	PAR_SIMPLE_PARSABLE;
};

enum eModKitType
{
	MKT_STANDARD = 0,
	MKT_SPORT,
	MKT_SUV,
	MKT_SPECIAL,

	MKT_MAX
};

typedef u16 VehicleKitId;
enum { INVALID_VEHICLE_KIT_ID = 0xFFFF };
enum { INVALID_VEHICLE_KIT_INDEX = 0xFFFF };

// describes a single vehicle mod kit
class CVehicleKit
{
public:
    struct sSlotNameOverride
    {
        eVehicleModType slot;
        ConstString name;

        PAR_SIMPLE_PARSABLE;
    };

	CVehicleKit() : m_id(INVALID_VEHICLE_KIT_ID) {}
	~CVehicleKit() {}

	u32 GetNameHash() const { return m_kitName.GetHash(); }
	VehicleKitId GetId() const { return m_id; }
	void SetId(VehicleKitId id) { m_id = id; }

	atHashString GetNameHashString() const { return m_kitName; }
	eModKitType GetType() const { return m_kitType; }
	const atArray<CVehicleModVisible>& GetVisibleMods() const { return m_visibleMods; }
	const atArray<CVehicleModLink>& GetLinkMods() const { return m_linkMods; }
	const atArray<CVehicleModStat>& GetStatMods() const { return m_statMods; }

	const CVehicleModLink* FindLinkMod(u32 nameHash, u8& index) const;

    const char* GetModSlotName(eVehicleModType slot) const;
	const char* GetLiveryName(u32 livery) const;
	const char* GetLivery2Name(u32 livery2) const;

private:
	atHashString m_kitName;
	VehicleKitId m_id;
	eModKitType m_kitType;
	atArray<CVehicleModVisible> m_visibleMods;
	atArray<CVehicleModLink> m_linkMods;
	atArray<CVehicleModStat> m_statMods;
    atArray<sSlotNameOverride> m_slotNames;
	atArray<ConstString> m_liveryNames;
	atArray<ConstString> m_livery2Names;

	PAR_SIMPLE_PARSABLE;
};

typedef u32 vehicleWheelId;
enum { INVALID_VEHICLE_WHEEL_ID = 0xFFFFFFFF };

// describes a single vehicle wheel
class CVehicleWheel
{
	friend class CVehicleStreamRenderGfx;

public:
	CVehicleWheel() : m_VariationDrawableIdx(-1), m_VariationDrawableCseType(NULL), m_VariationDrawableVarIdx(-1), m_VariationDrawableVarCseType(NULL) {}
	~CVehicleWheel() {}

	u32 GetNameHash() const { return m_wheelName.GetHash(); }
    atHashString GetNameHashString() const { return m_wheelName; }

    bool HasVariation() const { return m_wheelVariation.GetHash() != 0; }
	u32 GetVariationHash() const { return m_wheelVariation.GetHash(); }
    atHashString GetVariationHashString() const { return m_wheelVariation; }

	float GetRimRadius() const { return m_rimRadius; }

	bool IsRearWheel() const { return m_rear; }

	const char* GetModShopLabel() const { return m_modShopLabel.c_str(); }

	inline vehicleWheelId				GetId() const				{ return GetNameHash(); }

private:
	// CVehicleModelInfoVariation:
	s32									m_VariationDrawableIdx;
	CCustomShaderEffectVehicleType*		m_VariationDrawableCseType;
	s32									m_VariationDrawableVarIdx;
	CCustomShaderEffectVehicleType*		m_VariationDrawableVarCseType;
private:
	atHashString m_wheelName;
	atHashString m_wheelVariation;
	ConstString m_modShopLabel;
	float m_rimRadius;
	bool m_rear;

	PAR_SIMPLE_PARSABLE;
};

#endif // INC_VEHICLE_MODELINFO_MODS_H_ 
