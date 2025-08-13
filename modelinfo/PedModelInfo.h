//
//
//    Filename: PedModelInfo.h
//     Creator: Adam Fowler
//     $Author: $
//       $Date: $
//   $Revision: $
// Description: Class describing a Pedestrian model
//
//
#ifndef INC_PED_MODELINFO_H_
#define INC_PED_MODELINFO_H_

// Rage headers
#include "atl/bitset.h"
#include "fragment/manager.h"
#include "fwanimation/animdefines.h"
#include "fwmaths/random.h"
#include "fwutil/Flags.h"
#include "parser/macros.h"
#include "vector/Vector3.h"

// Gta headers
#include "Animation/animdefines.h"
#include "audio/gameobjects.h"
#include "modelinfo/BaseModelInfo.h"
#include "peds/PedType.h"
#include "peds/PlayerSpecialAbility.h"
#include "animation/AnimBones.h"
#include "debug/DebugScene.h"
#include "scene/loader/MapData_Misc.h"
#include "stats/StatsTypes.h"
#include "vfx/metadata/vfxpedinfo.h"

//pargen headers
#include "scene/loader/Map_TxdParents.h"

#define PED_GRAVITY_FACTOR			(1.0f)
#define PED_DRAG_FACTOR				(0.005f)
#define PED_MAX_ANG_SPEED			(8.0f * PI)

//THESE ARE NO LONGER CONSTANT AND NEED TO BE DEPRECATED
#define PED_HUMAN_RADIUS				(CPedModelInfo::ms_fPedRadius)
#define PED_HUMAN_GROUNDTOROOTOFFSET	(1.0f)
#define PED_HUMAN_HEAD_HEIGHT			(0.82f)

#define PED_CURVEDGEOM_FOR_MAIN_CAPSULE		(0)
#define PED_CURVEDGEOM_FOR_MAIN_MARGIN		(0.1f)

#define PED_USE_CAPSULE_PROBES		1
#define PED_USE_SHAPETEST_CAPSULE	0
#define PED_USE_EXTRA_BOUND			2	// this is used as the index to the bound in the composite, so if you turn capsule_probes off you need to make this 1
#define PLAYER_USE_LOWER_LEG_BOUND	3	// this is used as the index to the bound in the composite, so if you turn capsule_probes or extra_bound off you need to make this 2 and if you turn both off you need to make this 1
#define PLAYER_USE_FPS_HEAD_BOUND	4	// this is used as the index to the bound in the composite, so if you turn capsule_probes or extra_bound off you need to make this 3 and if you turn both off you need to make this 1

#define PED_COW_HEAD_BOUND			4

#define PLAYER_USE_CAPSULE_LEG_BOUND 1

#define PED_APPLY_HORSE_PROP_BLOCKER 1	//TMS: Getting strong reactions to low props, need to rethink this somewhat

#if PLAYER_USE_LOWER_LEG_BOUND
#define PLAYER_USE_LOWER_LEG_BOUND_ONLY(x) x
#else
#define PLAYER_USE_LOWER_LEG_BOUND_ONLY(x)
#endif

#define PLAYER_LEGS_SEGMENTS	(8)
#define PLAYER_LEGS_TOP			(-0.3f)
#define PLAYER_LEGS_BOTTOM		(-0.8f)
#define PLAYER_LEGS_MARGIN		(0.1f)

#define PED_LOD_THRESHOLD				(15)
#define PED_LOW_LOD_THRESHOLD			(30)
#define PED_SUPER_LOD_THRESHOLD			(80)

#define PED_SUPER_LOD_CLAMP				(40)

#define PED_MAX_NUM_RESIDENT_TXDS		(5)


class CPedVariationInfoCollection;
class CModelSeatInfo;
class CVehicleLayoutInfo;
class CBaseCapsuleInfo;
class CPedVariationInfo;

#if USE_GEOMETRY_CURVED
namespace rage{
	class phBoundCurvedGeometry;
}
#endif

#if __BANK
struct sDebugSize;
#endif // __BAK

enum ePedPieceTypes{
	PED_COL_SPHERE_LEG = 0,
	PED_COL_SPHERE_MID,
	PED_COL_SPHERE_HEAD,
	
	PED_SPHERE_CHEST,
	PED_SPHERE_MIDSECTION,
	PED_SPHERE_UPPERARM_L,
	PED_SPHERE_UPPERARM_R,
	PED_SPHERE_LOWERARM_L,
	PED_SPHERE_LOWERARM_R,

	PED_SPHERE_UPPERLEG_L,
	PED_SPHERE_UPPERLEG_R,
	PED_SPHERE_LOWERLEG_L,
	PED_SPHERE_LOWERLEG_R,
	PED_SPHERE_HEAD
};

enum ePedModelInfoFlags {
	PED_MINFO_FLAG_BUYSDRUGS = 0,
};


enum ePedVehicleTypes {
	PED_DRIVES_POOR_CAR = 0,
	PED_DRIVES_AVERAGE_CAR,
	PED_DRIVES_RICH_CAR,
	PED_DRIVES_BIG_CAR,
	PED_DRIVES_MOTORCYCLE,
	PED_DRIVES_BOAT,

	PED_DRIVES_MAX
};

#define PED_WATER_SAMPLES_STD (2)

//Animated samples at start followed by ragdoll samples
enum ePedWaterSamples
{
	WS_PED_ANIMATED_SAMPLE_START	= 0,
	WS_PED_RAGDOLL_SAMPLE_START		= PED_WATER_SAMPLES_STD
};

enum ThermalBehaviour
{
	TB_DEAD=0,
	TB_COLD=1,
	TB_WARM=2,
	TB_HOT=3,
	TB_COUNT=4,
};

enum Affluence
{
	AFF_POOR=0,
	AFF_AVERAGE=1,
	AFF_RICH=2,
};

enum TechSavvy
{
	TS_LOW=0,
	TS_HIGH=1,
};

enum DefaultSpawnPreference
{
	DSP_AERIAL=0,
	DSP_AQUATIC,
	DSP_GROUND_WILDLIFE,
	DSP_NORMAL,
};

enum AgitationReactionType
{
	ART_Nothing,
	ART_Ignore,
	ART_Loiter,
	ART_Intimidate,
	ART_Combat,
	ART_TerritoryIntrusion,
	ART_Max,
};

#define MAX_AUDIO_GROUP_MAPS 5

struct CAudioGroupMap
{
	s32 m_iHeadComponent;
	u32 m_audioGroupHash;
};

enum eExternallyDrivenDOFs
{
	EMPTY = BIT(0),
	HIGH_HEELS = BIT(1),
	COLLAR = BIT(2),
};

enum eBraveryFlags
{
	BF_INTERVENE_ON_MELEE_ACTION			= BIT(0),		// > 2
	BF_DONT_RUN_ON_MELEE_ATTACK				= BIT(1),		// > 2
	BF_WATCH_ON_CAR_STOLEN					= BIT(2),		// > 1
	BF_INTIMIDATE_PLAYER					= BIT(3),		// > 2
	BF_GET_PISSED_WHEN_HIT_BY_CAR			= BIT(4),		// > 2
	BF_DONT_SCREAM_ON_FLEE					= BIT(5),		// > 2
	BF_DONT_SAY_PANIC_ON_FLEE				= BIT(6),		// > 2
	BF_REACT_ON_COMBAT						= BIT(7),		// > 2
	BF_PLAY_CAR_HORN						= BIT(8),		// > 2
	BF_ARGUMENTATIVE						= BIT(9),		// > 2
	BF_CONFRONTATIONAL						= BIT(10),		// > 2
	BF_LIMIT_COMBATANTS						= BIT(11),		// > 2
	BF_PURSUE_WHEN_HIT_BY_CAR				= BIT(12),		// > 2
	BF_COWARDLY_FOR_SHOCKING_EVENTS			= BIT(13),
	BF_BOOST_BRAVERY_IN_GROUP				= BIT(14),
	BF_CAN_ACCELERATE_IN_CAR				= BIT(15),
	BF_CAN_GET_OUT_WHEN_HIT_BY_CAR			= BIT(16),
	BF_AGGRESSIVE_AFTER_RUNNING_PED_OVER	= BIT(17),
	BF_CAN_FLEE_WHEN_HIT_BY_CAR				= BIT(18),
	BF_ALLOW_CONFRONT_FOR_TERRITORY_REACTIONS = BIT(19),
	BF_DONT_FORCE_FLEE_COMBAT				= BIT(20)
};

enum eSexinessFlags
{
	SF_JEER_AT_HOT_PED					= BIT(0),		// > 1
	SF_JEER_SCENARIO_ANIM				= BIT(1),		// > 1
	SF_HOT_PERSON						= BIT(2),		// > 1
};

enum eAgilityFlags
{
	AF_CAN_DIVE							= BIT(0),		// > 1
	AF_AVOID_IMMINENT_DANGER			= BIT(1),		// > 2
	AF_RAGDOLL_BRACE_STRONG				= BIT(2),		// > 2
	AF_RAGDOLL_ON_FIRE_STRONG			= BIT(3),		// > 2
	AF_RAGDOLL_HIGH_FALL_STRONG			= BIT(4),		// > 4
	AF_RECOVER_BALANCE					= BIT(5),		// > 2
	AF_GET_UP_FAST						= BIT(6),		// > 3
	AF_BALANCE_STRONG					= BIT(7),		// > 3
	AF_STRONG_WITH_HEAVY_WEAPON			= BIT(8),		// > 3
	AF_DONT_FLINCH_ON_EXPLOSION			= BIT(9),		// > 2
	AF_DONT_FLINCH_ON_MELEE				= BIT(10),		// > 2
};

enum eCriminalityFlags
{
    CF_JACKING     						= BIT(0),		// > 1
	CF_ALLOWED_COP_PURSUIT				= BIT(1),
};

enum eSuperlodType
{
	SLOD_HUMAN,
	SLOD_SMALL_QUADRUPED,
	SLOD_LARGE_QUADRUPED,
	SLOD_NULL,
	SLOD_KEEP_LOWEST,

	SLOD_MAX
};
CompileTimeAssert(SLOD_MAX <= 8);

// Peds have different streaming "slots", allowing the game to
// potentially stream in 2 large peds and 2 smaller peds using scenarios.
enum eScenarioPopStreamingSlot
{
	SCENARIO_POP_STREAMING_NORMAL = 0,
	SCENARIO_POP_STREAMING_SMALL,

	NUM_SCENARIO_POP_SLOTS
};

class CPedModelInfo;
struct PedHDStreamReq
{
	strRequestArray<1>		reqs;
	CPedModelInfo*			targetModelInfo;
};

class CPedModelInfo : public CBaseModelInfo
{
public:

	// Intermediate struct used when constructing this class from xml (ie peds.xml)
	struct InitData {
		InitData	();
		void		Init();
		ConstString m_Name;
		ConstString m_PropsName;

		atHashString m_Pedtype;
		atHashString m_CreatureMetadataName;
		atHashString m_ClipDictionaryName;
		atHashString m_BlendShapeFileName;
		atHashString m_ExpressionSetName;
		atHashString m_ExpressionDictionaryName;
		atHashString m_ExpressionName;
		atHashString m_MovementClipSet;
		atArray<atHashString> m_MovementClipSets;
		atHashString m_StrafeClipSet;
		atHashString m_MovementToStrafeClipSet;
		atHashString m_InjuredStrafeClipSet;
		atHashString m_FullBodyDamageClipSet;
		atHashString m_AdditiveDamageClipSet;
		atHashString m_DefaultGestureClipSet;
		atHashString m_DefaultVisemeClipSet;
		atHashString m_SidestepClipSet;
		atHashString m_FacialClipsetGroupName;
		atHashString m_PoseMatcherName;
		atHashString m_PoseMatcherProneName;
		atHashString m_GetupSetHash;
		atHashString m_DecisionMakerName;
		atHashString m_MotionTaskDataSetName;
		atHashString m_DefaultTaskDataSetName;
		atHashString m_PedCapsuleName;
		atHashString m_PedLayoutName;
		atHashString m_PedComponentSetName;
		atHashString m_PedComponentClothName;
		atHashString m_PedIKSettingsName;
		atHashString m_TaskDataName;
		atHashString m_RelationshipGroup;
		atHashString m_NavCapabilitiesName;
		atHashString m_PerceptionInfo;
		atHashString m_DefaultBrawlingStyle;
		atHashString m_DefaultUnarmedWeapon;
		atHashString m_PedVoiceGroup;
		atHashString m_AnimalAudioObject;
		atHashString m_Personality;
		atHashString m_CombatInfo;
		atHashString m_VfxInfoName;
		atHashString m_AmbientClipsForFlee;

		bool		m_IsStreamedGfx;
		bool		m_AmbulanceShouldRespondTo;
		bool		m_CanRideBikeWithNoHelmet;
		bool		m_CanSpawnInCar;
		bool		m_IsHeadBlendPed;
		bool		m_CausesRumbleWhenCollidingWithPlayer;
		bool		m_bOnlyBulkyItemVariations;
		RadioGenre	m_Radio1;
		RadioGenre	m_Radio2;

		float		m_FUpOffset,m_RUpOffset,m_FFrontOffset, m_RFrontOffset;

		float		m_MinActivationImpulse;
		float		m_Stubble;
		float		m_HDDist;
		float		m_TargetingThreatModifier;
		float		m_KilledPerceptionRangeModifer;
		u32			m_Sexiness;
		u8			m_Age;
		u8			m_MaxPassengersInCar;
		u8			m_ExternallyDrivenDOFs;

		SpecialAbilityType m_AbilityType;
		
		ThermalBehaviour m_ThermalBehaviour;

		eSuperlodType	m_SuperlodType;

		// Ambient spawning information for this model type.
		eScenarioPopStreamingSlot	m_ScenarioPopStreamingSlot;
		DefaultSpawnPreference		m_DefaultSpawningPreference;
		float						m_DefaultRemoveRangeMultiplier;
		bool						m_AllowCloseSpawning;
		
		PAR_SIMPLE_PARSABLE;
	};
	struct InitDataList {
		atArray<InitData> m_InitDatas;
		atString m_residentTxd;
		atArray<atString> m_residentAnims;
		atArray <CTxdRelationship> m_txdRelationships;
		atArray <CMultiTxdRelationship> m_multiTxdRelationships;
		PAR_SIMPLE_PARSABLE;
	};

	// The following fields come from the pedpersonality.meta
	
	class PersonalityThreatResponse {
	public:
	
		class Action {
		public:
		
			class Weights {
			public:
				Weights() {}
				float m_Fight;
				float m_Flee;

				PAR_SIMPLE_PARSABLE;
			};
			
			Action() {}
			Weights m_Weights;
			
			PAR_SIMPLE_PARSABLE;
		};
		
		class Fight {
		public:
		
			class Weights {
			public:
				Weights() {}
				float m_KeepWeapon;
				float m_MatchTargetWeapon;
				float m_EquipBestWeapon;
				
				PAR_SIMPLE_PARSABLE;
			};
			
			Fight() {}
			Weights m_Weights;
			float	m_ProbabilityDrawWeaponWhenLosing;
			
			PAR_SIMPLE_PARSABLE;
		};
		
		PersonalityThreatResponse() {}
		Action		m_Action;
		Fight		m_Fight;
		
		PAR_SIMPLE_PARSABLE;
	};

	class PersonalityFleeDuringCombat {
	public:
		PersonalityFleeDuringCombat() {}
		bool m_Enabled;
		float m_ChancesWhenBuddyKilledWithScaryWeapon;
		PAR_SIMPLE_PARSABLE;
	};
	
	class PersonalityBravery {
	public:
		u32 GetNameHash(){return m_Name.GetHash();}
		PersonalityBravery();
		atHashString m_Name;
		u32 m_Flags;
		float m_TakedownProbability;
		PersonalityThreatResponse m_ThreatResponseUnarmed;
		PersonalityThreatResponse m_ThreatResponseMelee;
		PersonalityThreatResponse m_ThreatResponseArmed;
		PersonalityFleeDuringCombat m_FleeDuringCombat;
		PAR_SIMPLE_PARSABLE;
	};

	class PersonalityAgility {
	public:
		PersonalityAgility();
		u32 m_Flags;
		float m_MovementCostModifier;
		PAR_SIMPLE_PARSABLE;
	};

    class PersonalityCriminality {
    public:
		u32 GetNameHash(){return m_Name.GetHash();}
        PersonalityCriminality();
        atHashString m_Name;
        u32 m_Flags;
        PAR_SIMPLE_PARSABLE;
    };

	class PersonalityMovementModes
	{
	public:

		struct sUnholsterClipData
		{
			struct sUnholsterClip
			{
				u32 GetNameHash(){return m_Clip.GetHash();}
				atArray<atHashWithStringNotFinal> m_Weapons;
				fwMvClipId m_Clip;

				PAR_SIMPLE_PARSABLE;
			};

			atHashWithStringNotFinal m_Name;
			atArray<sUnholsterClip> m_UnholsterClips;
			u32 GetNameHash(){return m_Name.GetHash();}
			PAR_SIMPLE_PARSABLE;
		};

		enum MovementModes
		{
			MM_Invalid = -1,
			MM_Action = 0,
			MM_Stealth,
			MM_Max,
		};

		class MovementMode
		{
		public:

			struct ClipSets
			{
				ClipSets();
				void Clear();
				bool IsValid() const;
				fwMvClipSetId GetIdleTransitionClipSet() const;
				fwMvClipId GetUnholsterClip(atHashWithStringNotFinal drawingWeapon) const;

				ClipSets& operator=(const ClipSets& other) 
				{ 
					m_MovementClipSetId = other.m_MovementClipSetId; 
					m_WeaponClipSetId = other.m_WeaponClipSetId; 
					m_WeaponClipFilterId = other.m_WeaponClipFilterId;
					m_UpperBodyShadowExpressionEnabled = other.m_UpperBodyShadowExpressionEnabled;
					m_UpperBodyFeatheredLeanEnabled = other.m_UpperBodyFeatheredLeanEnabled;
					m_UseWeaponAnimsForGrip = other.m_UseWeaponAnimsForGrip;
					m_UseLeftHandIk = other.m_UseLeftHandIk;
					m_IdleTransitions = other.m_IdleTransitions;
					m_UnholsterClipSetId = other.m_UnholsterClipSetId;
					m_UnholsterClipData = other.m_UnholsterClipData;
					m_UnholsterClipDataPtr = other.m_UnholsterClipDataPtr;
					m_IdleTransitionBlendOutTime = other.m_IdleTransitionBlendOutTime;
					return *this;
				}

				fwMvClipSetId m_MovementClipSetId;
				fwMvClipSetId m_WeaponClipSetId;
				fwMvFilterId m_WeaponClipFilterId;
				bool m_UpperBodyShadowExpressionEnabled;
				bool m_UpperBodyFeatheredLeanEnabled;
				bool m_UseWeaponAnimsForGrip;
				bool m_UseLeftHandIk;
				float m_IdleTransitionBlendOutTime;
				atArray<fwMvClipSetId> m_IdleTransitions;
				fwMvClipSetId m_UnholsterClipSetId;
				atHashWithStringNotFinal m_UnholsterClipData;
				const sUnholsterClipData* m_UnholsterClipDataPtr;

				PAR_SIMPLE_PARSABLE;
			};

			MovementMode() {}

			bool IsValid(const CPed* pPed, u32 uWeaponHash) const;
			const ClipSets& GetRandomClipSets() const { return m_ClipSets[fwRandom::GetRandomNumberInRange(0, m_ClipSets.GetCount())]; }
			const atArray<ClipSets>& GetClipSets() const { return m_ClipSets; }

			void PostLoad();

		private:
			atArray<atHashWithStringNotFinal> m_Weapons;
			atArray<ClipSets> m_ClipSets;

			PAR_SIMPLE_PARSABLE;
		};

		PersonalityMovementModes();

#if !__FINAL
		const char* GetName() const { return m_Name.TryGetCStr(); }
#endif // !__FINAL
		u32 GetNameHash() const { return m_Name.GetHash(); }
		// Get the movement mode for the ped
		const MovementMode& GetMovementMode(MovementModes mm, const s32 iIndex) const { return m_MovementModes[mm][iIndex]; }
		s32 GetMovementModeCount(MovementModes mm) const { return m_MovementModes[mm].GetCount(); }
		const MovementMode* FindMovementMode(const CPed* pPed, MovementModes mm, const u32 uWeaponHash, s32& iIndex) const;

		u32 GetLastBattleEventHighEnergyStartTimeMS() const { return (u32)(m_LastBattleEventHighEnergyStartTime*1000.0f); }
		u32 GetLastBattleEventHighEnergyEndTimeMS() const { return (u32)(m_LastBattleEventHighEnergyEndTime*1000.0f); }
	
		void PostLoad();

	public:
		atArray<MovementMode>	m_MovementModes[MM_Max];		
	private:
		atHashString			m_Name;
		float m_LastBattleEventHighEnergyStartTime;
		float m_LastBattleEventHighEnergyEndTime;

		PAR_SIMPLE_PARSABLE;
	};


	class PersonalityData {
	public:
		PersonalityData();
		void		Init();

		u32			GetPersonalityNameHash() const			{ return m_PersonalityName.GetHash(); }
		u32			GetDefaultWeaponLoadoutHash() const		{ return m_DefaultWeaponLoadout.GetHash(); }
#if !__FINAL
		const char* GetPersonalityName() const				{ return m_PersonalityName.GetCStr(); }
		const char* GetBraveryName() const					{ return m_Bravery.GetCStr(); }
		const char* GetDefaultWeaponLoadoutName() const		{ return m_DefaultWeaponLoadout.GetCStr(); }
#endif

		float		GetAttackStrengthMin() const			{ return m_AttackStrengthMin; }
		void		SetAttackStrengthMin(float f)			{ m_AttackStrengthMin = f; }
		float		GetAttackStrengthMax() const			{ return m_AttackStrengthMax; }
		void		SetAttackStrengthMax(float f)			{ m_AttackStrengthMax = f; }
		float		GetAttackStrength(u16 seed) const		{ return (((float)(seed)/65535.0f) * (m_AttackStrengthMax-m_AttackStrengthMin)) + m_AttackStrengthMin; }

		float		GetStaminaEfficiency() const			{ return m_StaminaEfficiency; }
		void		SetStaminaEfficiency(float f)			{ m_StaminaEfficiency = f; }
		float		GetArmourEfficiency() const				{ return m_ArmourEfficiency; }
		void		SetArmourEfficiency(float f)			{ m_ArmourEfficiency = f; }
		float		GetHealthRegenEfficiency() const		{ return m_HealthRegenEfficiency; }
		void		SetHealthRegenEfficiency(float f)		{ m_HealthRegenEfficiency = f; }
		float		GetExplosiveDamageMod() const			{ return m_ExplosiveDamageMod; }
		void		SetExplosiveDamageMod(float f)			{ m_ExplosiveDamageMod = f; }
		float		GetHandGunDmgMod() const				{ return m_HandGunDamageMod; }
		void		SetHandGunDmgMod(float f)				{ m_HandGunDamageMod = f; }
		float		GetRifleDmgMod() const					{ return m_RifleDamageMod; }
		void		SetRifleDmgMod(float f)					{ m_RifleDamageMod = f; }
		float		GetSmgDamageMod() const					{ return m_SmgDamageMod; }
		void		SetSmgDamageMod(float f)				{ m_SmgDamageMod = f; }
		float		GetPopulationFleeMod() const			{ return m_PopulationFleeMod; }
		void		SetPopulationFleeMod(float f)			{ m_PopulationFleeMod = f; }
		float		GetHotwireRate() const					{ return m_HotwireRate; }
		void		SetHotwireRate(float f)					{ m_HotwireRate = f; }		

		u32			GetMotivationMin() const				{ return m_MotivationMin; }
		void		SetMotivationMin(u32 v)					{ m_MotivationMin = v; }
		u32			GetMotivationMax() const				{ return m_MotivationMax; }
		void		SetMotivationMax(u32 v)					{ m_MotivationMax = v; }
		u32			GetMotivation(u16 seed) const			{ float num = seed / (float)(RAND_MAX_16+1); return (u32)(num * ((s32)m_MotivationMax - (s32)m_MotivationMin)) + (s32)m_MotivationMin;}

		bool		DrivesVehicleType(ePedVehicleTypes type) const { return m_VehicleTypes.IsSet(type); }
		void		SetVehicleType(ePedVehicleTypes type)	{ m_VehicleTypes.Set(type); }
	
		u8			GetDrivingAbilityMin() const			{ return m_DrivingAbilityMin; }
		void		SetDrivingAbilityMin(u8 v)				{ m_DrivingAbilityMin = v; }
		u8			GetDrivingAbilityMax() const			{ return m_DrivingAbilityMax; }
		void		SetDrivingAbilityMax(u8 v)				{ m_DrivingAbilityMax = v; }
		u8			GetDrivingAbility(u32 seed) const		{ float num = seed / (float)(RAND_MAX_16+1); return (u8)(num * ((s32)m_DrivingAbilityMax - (s32)m_DrivingAbilityMin)) + (s32)m_DrivingAbilityMin; }

		u8			GetDrivingAggressivenessMin() const		{ return m_DrivingAggressivenessMin; }
		void		SetDrivingAggressivenessMin(u8 v)		{ m_DrivingAggressivenessMin = v; }
		u8			GetDrivingAggressivenessMax() const		{ return m_DrivingAggressivenessMax; }
		void		SetDrivingAggressivenessMax(u8 v)		{ m_DrivingAggressivenessMax = v; }
		u8			GetDrivingAggressiveness(u32 seed) const{ float num = seed / (float)(RAND_MAX_16+1); return (u8)(num * ((s32)m_DrivingAggressivenessMax - (s32)m_DrivingAggressivenessMin)) + (s32)m_DrivingAggressivenessMin;  }

		bool		GetIsMale() const						{ return m_IsMale; }
		void		SetIsMale(bool v)						{ m_IsMale = v; }

		bool		GetIsHuman() const						{ return m_IsHuman; }
		void		SetIsHuman(bool v)						{ m_IsHuman = v; }

		Affluence	GetAffluence() const					{return m_Affluence; }
		void		SetAffluence(Affluence a)				{m_Affluence = a; }

		TechSavvy	GetTechSavvy() const					{return m_TechSavvy; }
		void		SetTechSavvy(TechSavvy t)				{m_TechSavvy = t; }

		bool		GetShouldRewardMoneyOnDeath() const		{ return m_ShouldRewardMoneyOnDeath; }
		void		SetShouldRewardMoneyOnDeath(bool v)		{ m_ShouldRewardMoneyOnDeath = v; }

		fwMvClipSetId GetMeleeMovementDictionaryGroupId() const { return CLIP_SET_MOVE_MELEE; }

		bool		CheckBraveryFlags(u32 flags) const		{ return (CPedModelInfo::GetPersonalityBravery(m_Bravery.GetHash())->m_Flags & flags) == flags; }
		u32			GetBraveryFlags() const					{ return CPedModelInfo::GetPersonalityBravery(m_Bravery.GetHash())->m_Flags; }
		
		float GetTakedownProbability() const				{ return CPedModelInfo::GetPersonalityBravery(m_Bravery.GetHash())->m_TakedownProbability; }
	
		const PersonalityThreatResponse&	GetThreatResponseUnarmed() const	{ return CPedModelInfo::GetPersonalityBravery(m_Bravery.GetHash())->m_ThreatResponseUnarmed;	}
		const PersonalityThreatResponse&	GetThreatResponseMelee() const		{ return CPedModelInfo::GetPersonalityBravery(m_Bravery.GetHash())->m_ThreatResponseMelee;		}
		const PersonalityThreatResponse&	GetThreatResponseArmed() const		{ return CPedModelInfo::GetPersonalityBravery(m_Bravery.GetHash())->m_ThreatResponseArmed;		}
		const PersonalityFleeDuringCombat&	GetFleeDuringCombat() const			{ return CPedModelInfo::GetPersonalityBravery(m_Bravery.GetHash())->m_FleeDuringCombat;			}
		
		atHashString	GetAgitatedPersonality() const		{ return m_AgitatedPersonality; }

		bool		CheckAgilityFlags(u32 flags) const		{ return (m_Agility.m_Flags & flags) == flags; }
		u32			GetAgilityFlags() const					{ return m_Agility.m_Flags; }
		float		GetMovementCostModifier() const			{ return m_Agility.m_MovementCostModifier; }

        bool		CheckCriminalityFlags(u32 flags) const	{ modelinfoAssertf(m_pCriminality, "Criminality hasn't been cached\n"); return (m_pCriminality->m_Flags & flags) == flags; }
        u32			GetCriminalityFlags() const				{ modelinfoAssertf(m_pCriminality, "Criminality hasn't been cached\n"); return m_pCriminality->m_Flags; }

		bool		GetIsGang()	const						{ return m_IsGang; }

		bool		GetIsSecurity() const					{ return m_IsSecurity; }

		bool		GetIsWeird() const						{ return m_IsWeird; }

		bool		GetIsDangerousAnimal() const			{ return m_IsDangerousAnimal; }

		bool		GetCausesRumbleWhenCollidesWithPlayer() const	{ return m_CausesRumbleWhenCollidesWithPlayer; }

		bool		AllowSlowCruisingWithMusic() const		{ return m_AllowSlowCruisingWithMusic; }

		bool		AllowRoadCrossHurryOnLightChange() const { return m_AllowRoadCrossHurryOnLightChange; }
		
		u32			GetAgitationTriggersHash() const		{ return m_AgitationTriggers.GetHash(); }

		const PersonalityMovementModes* GetMovementModes() const{ return m_MovementModesPtr; }

		atHashString GetHealthConfigHash() const {return m_HealthConfigHash;}

		u32 GetNumWeaponAnimations() const { return m_WeaponAnimations.GetCount(); }
		atHashString GetWeaponAnimations(u32 nIndex) const { Assert(nIndex < GetNumWeaponAnimations()); return m_WeaponAnimations[nIndex]; }

		atHashString GetAmbientAudio() const { return m_AmbientAudio; }

		atHashString GetWitnessPersonality() const { return m_WitnessPersonality; }

		u32 GetNameHash() { return m_PersonalityName.GetHash(); }

		atHashString GetWeaponAnimsFPSIdle() const { return m_WeaponAnimsFPSIdle; }
		atHashString GetWeaponAnimsFPSRNG() const { return m_WeaponAnimsFPSRNG; }
		atHashString GetWeaponAnimsFPSLT() const { return m_WeaponAnimsFPSLT; }
		atHashString GetWeaponAnimsFPSScope() const { return m_WeaponAnimsFPSScope; }

		void PostLoad();

	private:
		atHashString					m_PersonalityName;
		atHashString					m_DefaultWeaponLoadout;
		atHashString					m_Bravery;
		atHashString					m_AgitatedPersonality;
		atHashString					m_Criminality;
		atHashString					m_AgitationTriggers;
		atHashString					m_HealthConfigHash;
		atArray<atHashString>			m_WeaponAnimations;
		atHashString					m_AmbientAudio;
		atHashString					m_WitnessPersonality;
		PersonalityAgility				m_Agility;
		float							m_AttackStrengthMin;
		float						    m_AttackStrengthMax;
		float							m_StaminaEfficiency;
		float							m_ArmourEfficiency;
		float							m_HealthRegenEfficiency;
		float							m_ExplosiveDamageMod;
		float							m_HandGunDamageMod;
		float							m_RifleDamageMod;
		float							m_SmgDamageMod;
		float							m_PopulationFleeMod;
		float							m_HotwireRate;
		u32							    m_MotivationMin; 
		u32							    m_MotivationMax;
		rage::atFixedBitSet<8>			m_VehicleTypes;
		u8								m_DrivingAbilityMin;
		u8								m_DrivingAbilityMax;
		u8								m_DrivingAggressivenessMin;
		u8								m_DrivingAggressivenessMax;
		bool							m_IsMale;
		bool							m_IsHuman;
		bool							m_ShouldRewardMoneyOnDeath;
		bool							m_IsGang;
		bool							m_IsSecurity;
		bool							m_IsWeird;
		bool							m_IsDangerousAnimal;
		bool							m_CausesRumbleWhenCollidesWithPlayer;
		bool							m_AllowSlowCruisingWithMusic;
		bool							m_AllowRoadCrossHurryOnLightChange;
		Affluence						m_Affluence;
		TechSavvy						m_TechSavvy;
		atHashWithStringNotFinal		m_MovementModes;
		const PersonalityMovementModes*	m_MovementModesPtr;
		PersonalityCriminality*			m_pCriminality;
		atHashString					m_WeaponAnimsFPSIdle;
		atHashString					m_WeaponAnimsFPSRNG;
		atHashString					m_WeaponAnimsFPSLT;
		atHashString					m_WeaponAnimsFPSScope;
		PAR_SIMPLE_PARSABLE;
	};


	struct PersonalityDataList {
		void AppendToUnholsterClips(atArray<CPedModelInfo::PersonalityMovementModes::sUnholsterClipData>& appendFrom);
		void AppendToMovementModes(atArray<CPedModelInfo::PersonalityMovementModes>& appendFrom);
		atArray<CPedModelInfo::PersonalityMovementModes::sUnholsterClipData> m_MovementModeUnholsterData;
		atArray<CPedModelInfo::PersonalityMovementModes> m_MovementModes;
		atArray<CPedModelInfo::PersonalityCriminality> m_CriminalityTypes;
		atArray<CPedModelInfo::PersonalityBravery> m_BraveryTypes;
		atArray<CPedModelInfo::PersonalityData> m_PedPersonalities;
		PAR_SIMPLE_PARSABLE;
	};

public:
	CPedModelInfo() {m_type = MI_TYPE_PED;}
	
	virtual void Init();
	virtual void Shutdown();
	virtual void ShutdownExtra();

	virtual void InitMasterDrawableData(u32 modelIdx);
	virtual void DeleteMasterDrawableData();

	virtual void InitMasterFragData(u32 modelIdx);
	virtual void DeleteMasterFragData();
	virtual void SetPhysics(phArchetypeDamp* pPhysics);

	// Accessor function for the Seat info
	const CModelSeatInfo*	GetModelSeatInfo() const {return m_pSeatInfo;}
	
	void Init(const InitData & rInitData);

	void SetPropsFile(const char* pName);
	s32 GetPropsFileHashcode(void) const {return(m_propsFilenameHashcode);}
	s32 GetPropsFileIndex() const {return m_propsFileIndex;}

	void SetPedComponentFile(const char* pName);
	s32 GetPedComponentFileIndex() const {return m_pedCompFileIndex;}

	void SetPedMetaDataFile(const char* pName);
	void SetPedMetaDataFile(s32 index);
	void RemovePedMetaDataFile(const char* pName);
	void RemovePedMetaDataFile(s32 index);
	s32 GetNumPedMetaDataFiles() const { return m_pedMetaDataFileIndices.GetCount(); }
	s32 GetPedMetaDataFileIndex(u32 index) const {return index < m_pedMetaDataFileIndices.GetCount() ? m_pedMetaDataFileIndices[index] : -1;}

	void SetExpressionSet(fwMvExpressionSetId expressionSet) {m_expressionSet = expressionSet;}
	fwMvExpressionSetId GetExpressionSet() const {return m_expressionSet;}

	void SetFacialClipSetGroupId(fwMvFacialClipSetGroupId facialClipSetGroup) {m_facialClipSetGroupId = facialClipSetGroup;}
	fwMvFacialClipSetGroupId GetFacialClipSetGroupId() const {return m_facialClipSetGroupId;}

private:
	void SetMovementClipSet(fwMvClipSetId movementClipSet) {m_movementClipSet = movementClipSet;}
	void SetMovementClipSets(const atArray<atHashString>& movementClipSets);
public:
	fwMvClipSetId GetMovementClipSet() const {return m_movementClipSet;}
	const atArray<fwMvClipSetId>& GetMovementClipSets() const {return m_movementClipSets;}
	fwMvClipSetId GetRandomMovementClipSet(const CPed* pPed) const;

	void SetStrafeClipSet(fwMvClipSetId strafeClipSet) {m_strafeClipSet = strafeClipSet;}
	fwMvClipSetId GetStrafeClipSet() const {return m_strafeClipSet;}

	void SetMovementToStrafeClipSet(fwMvClipSetId movementToStrafeClipSet) {m_movementToStrafeClipSet = movementToStrafeClipSet;}
	fwMvClipSetId GetMovementToStrafeClipSet() const {return m_movementToStrafeClipSet;}

	void SetInjuredStrafeClipSet(fwMvClipSetId injuredStrafeClipSet) {m_injuredStrafeClipSet = injuredStrafeClipSet;}
	fwMvClipSetId GetInjuredStrafeClipSet() const {return m_injuredStrafeClipSet;}

	fwMvClipSetId GetAppropriateStrafeClipSet(const CPed* pPed) const;

	void SetFullBodyDamageClipSet(fwMvClipSetId fullBodyDamageClipSet) {m_fullBodyDamageClipSet = fullBodyDamageClipSet;}
	fwMvClipSetId GetFullBodyDamageClipSet() const {return m_fullBodyDamageClipSet;}

	void SetAdditiveDamageClipSet(fwMvClipSetId additiveDamageClipSet) {m_additiveDamageClipSet = additiveDamageClipSet;}
	fwMvClipSetId GetAdditiveDamageClipSet() const {return m_additiveDamageClipSet;}

	void SetDefaultGestureClipSet(fwMvClipSetId defaultGestureClipSet) 
	{
		modelinfoAssert(defaultGestureClipSet != rage::CLIP_SET_ID_INVALID);
		m_defaultGestureClipSet = defaultGestureClipSet;
	}
	fwMvClipSetId GetDefaultGestureClipSet() const {return m_defaultGestureClipSet ;}

	void SetDefaultVisemeClipSet(fwMvClipSetId defaultVisemeClipSet) 
	{
		modelinfoAssert(defaultVisemeClipSet != rage::CLIP_SET_ID_INVALID);
		m_defaultVisemeClipSet = defaultVisemeClipSet;
	}
	fwMvClipSetId GetDefaultVisemeClipSet() const {return m_defaultVisemeClipSet;}

	fwMvClipSetId GetSidestepClipSet() const { return m_SidestepClipSet; }

	// Used mainly for quadrupeds who need a species-specific ragdoll blendout anim
	atHashString GetGetupSet() const { return m_GetupSetHash; }
	void SetGetupSet(atHashString sGetupSet) { m_GetupSetHash = sGetupSet; }

	void SetIsPlayerModel(bool val){m_isPlayerModel = val;}
	bool GetIsPlayerModel() const { return m_isPlayerModel;}

	void SetDefaultPedType(ePedType type) {m_defaultPedType = type;}
	ePedType GetDefaultPedType() const {return m_defaultPedType;}
	bool	GetIsCop() const {return (m_defaultPedType == PEDTYPE_COP);}
	bool	GetIsGang() const {return (m_defaultPedType >= PEDTYPE_GANG_ALBANIAN && m_defaultPedType <= PEDTYPE_GANG_PUERTO_RICAN);}

	void SetRace(u8 Race) { m_Race = Race; }
	u8 GetRace() const { return(m_Race); }

	// radio stations
	void	SetRadioStations(s32 radio1, s32 radio2) {m_radio1 = static_cast<s8>(radio1); m_radio2 = static_cast<s8>(radio2);}
	s32	GetFirstRadioStation() const {return m_radio1;}
	s32	GetSecondRadioStation() const {return m_radio2;}

	void SetFOffsets(float upOffset,float frontOffset) {m_FUpOffset = upOffset;m_FFrontOffset = frontOffset;}
	void SetROffsets(float upOffset,float frontOffset) {m_RUpOffset = upOffset;m_RFrontOffset = frontOffset;}
	f32 GetFUpOffset() const {return m_FUpOffset;}
	f32 GetRUpOffset() const {return m_RUpOffset;}
	f32 GetFFrontOffset() const {return m_FFrontOffset;}
	f32 GetRFrontOffset() const {return m_RFrontOffset;}


	float GetMinActivationImpulse() const { return m_MinActivationImpulse; }
	void SetMinActivationImpulse(float newVal) { m_MinActivationImpulse = newVal; } 
	
	float GetStubble() const { return m_Stubble; }
	void SetStubble(float newVal) { m_Stubble = newVal; } 

	s32	GetNumTimesOnFoot() const { return m_NumTimesOnFoot; }
	void	SetNumTimesOnFoot(s32 newVal) { m_NumTimesOnFoot = newVal; } 
	s32	GetNumTimesInCar() const { return m_NumTimesInCar; }
	void	SetNumTimesInCar(s32 newVal) { m_NumTimesInCar = newVal; } 
	s32	GetNumTimesOnFootAndInCar() const { return m_NumTimesOnFoot+m_NumTimesInCar; }
	void	SetNumTimesInReusePool(u8 newVal) { Assert(newVal <= (MAX_UINT8-1)); m_NumTimesInReusePool = newVal; }
	u8	GetNumTimesInReusePool() const { return m_NumTimesInReusePool; }

	s32	GetNumTimesUsed() const { return m_NumTimesUsed; }
	void	ResetNumTimesUsed() { m_NumTimesUsed = 0; }
	void	IncreaseNumTimesUsed() { m_NumTimesUsed = MIN(m_NumTimesUsed+1, 120); }

	u32	GetLastTimeUsed() const { return m_LastTimeUsed; }
	void	SetLastTimeUsed(u32 LastUsed) { m_LastTimeUsed = LastUsed; }

	bool	GetCountedAsScenarioPed() const { return m_CountedAsScenarioPed; }
	void	SetCountedAsScenarioPed(bool bValue) { m_CountedAsScenarioPed = bValue; }

	void SetMotionTaskDataSetHash(const atHashString& motionTaskDataSet);
	inline atHashString GetMotionTaskDataSetHash() const {return m_motionTaskDataSetHash; }

	void SetDefaultTaskDataSetHash(const atHashString& defaultTaskDataSet);
	inline atHashString GetDefaultTaskDataSetHash() const {return m_defaultTaskDataSetHash; }

	void SetPedCapsuleHash(const atHashString& pedCapsule);
	inline atHashString GetPedCapsuleHash() const {return m_pedCapsuleHash;}

	void SetLayoutHash(const atHashString& pedLayoutName);		
	const CVehicleLayoutInfo* GetLayoutInfo() const;	

	void SetPedComponentSetHash(const atHashString& pedComponentSetName);
	inline atHashString GetPedComponentSetHash() const {return m_pedComponentSetHash;}

	void SetPedComponentClothHash(const atHashString& pedComponentClothName);
	inline atHashString GetPedComponentClothHash() const {return m_pedComponentClothHash;}	

	void SetPedIKSettingsHash(const atHashString& pedIKSettingsName);
	inline atHashString GetPedIKSettingsHash() const {return m_pedIKSettingsHash;}
	void SetTaskDataHash(const atHashString& taskDataName);
	inline atHashString GetTaskDataHash() const {return m_TaskDataHash;}

	void SetDecisionMakerHash(const atHashString& decisionMaker);
	inline atHashString GetDecisionMakerHash() {return m_decisionMakerHash;}

	void SetRelationshipGroupHash(const atHashString& relationshipGroupName);
	inline atHashString GetRelationshipGroupHash() {return m_relationshipGroupHash;}

	void SetNavCapabilitiesHash(const atHashString& navCapabilitiesName);
	inline atHashString GetNavCapabilitiesHash() const {return m_navCapabilitiesHash;}

	void SetPerceptionHash(const atHashString& perceptionName);
	inline atHashString GetPerceptionHash() const {return m_perceptionHash;}

	void SetSpecialAbilityType(SpecialAbilityType abilityType) { m_pedAbilityType = abilityType; }
	inline SpecialAbilityType GetSpecialAbilityType() const {return m_pedAbilityType;}

	void SetThermalBehaviour(ThermalBehaviour thermalBehaviour) { m_ThermalBehaviour = thermalBehaviour; }
	inline ThermalBehaviour GetThermalBehaviour() const {return (ThermalBehaviour)m_ThermalBehaviour;}

	void SetSuperlodType(eSuperlodType superlodType) { m_SuperlodType = superlodType; }
	inline eSuperlodType GetSuperlodType() const {return (eSuperlodType)m_SuperlodType;}

	void SetNumAvailableLODs(u8 numAvail) { m_numAvailableLODs = numAvail; }
	inline u8 GetNumAvailableLODs() { return(m_numAvailableLODs); }

	void SetDefaultBrawlingStyleHash(const atHashString& brawlingStyle);
	inline atHashString GetDefaultBrawlingStyle() const { return m_DefaultBrawlingStyle; }

	void SetDefaultWeaponHash(const atHashString& weaponType);
	inline atHashString GetDefaultWeapon() const { return m_DefaultUnarmedWeapon; }

	CVfxPedInfo* GetVfxInfo() {return m_pVfxInfo;}
	void SetVfxInfo(u32 vfxInfoHashName) {m_pVfxInfo = g_vfxPedInfoMgr.GetInfo(vfxInfoHashName);}
#if __DEV
	void SetVfxInfo(CVfxPedInfo* pVfxPedInfo) {m_pVfxInfo = pVfxPedInfo;}
#endif

//	void CreateHitColModelSkinned(crSkeleton* pSkeleton);
//	CColModel *AnimatePedColModelSkinned(crSkeleton* pSkeleton);
//	CColModel *AnimatePedColModelSkinnedWorld(crSkeleton* pSkeleton);

	void InitPedData();
	void InitPhys();
	virtual void InitWaterSamples();

	float GetQuadrupedProbeDepth() const;
	const Vector3& GetQuadrupedProbeTopPosition(int iCapsule) const;
	void SetQuadrupedProbeDepth(float fDepth);
	void SetQuadrupedProbeTopPosition(int iCapsule, const Vector3 &rPos);

	inline void AddPedModelRef() { m_numPedModelRefs++; }
	inline void RemovePedModelRef() { m_numPedModelRefs--; modelinfoAssert(m_numPedModelRefs>=0); }
	inline s32 GetNumPedModelRefs() const {return m_numPedModelRefs;}

	void SetAudioPedType(s16 apt) {m_AudioPedType=apt;}
	s16 GetAudioPedType(void) const {return(m_AudioPedType);}
	void SetFirstVoice(s16 fv) {m_FirstVoice=fv;}
	s16 GetFirstVoice(void) const {return(m_FirstVoice);}
	void SetLastVoice(s16 lv) {m_LastVoice=lv;}
	s16 GetLastVoice(void) const {return(m_LastVoice);}
	void SetNextVoice(s16 nv) {m_NextVoice=nv;}
	s16 GetNextVoice(void) const {return(m_NextVoice);}
	void IncrementVoice(void);

	void SetPedVoiceGroup(u32 pedVoiceGroupHash) {m_PedVoiceGroupHash = pedVoiceGroupHash;}
	u32  GetPedVoiceGroup(void) const {return(m_PedVoiceGroupHash);}
	void SetAnimalAudioObject(u32 animalAudioObjectHash) {m_AnimalAudioObjectHash = animalAudioObjectHash;}
	u32  GetAnimalAudioObject(void) const {return(m_AnimalAudioObjectHash);}

	const PersonalityData& GetPersonalitySettings() const { modelinfoAssertf(m_PersonalityIdx > -1, "No (or invalid) personality set for ped '%s'", GetModelName()); return GetPersonality(m_PersonalityIdx); }
	void SetPersonalitySettingsIdx(s16 idx) { m_PersonalityIdx = idx; }

	void SetCombatInfoHash(u32 combatInfoHash) { m_CombatInfoHash = combatInfoHash; }
	u32  GetCombatInfoHash(void) const { return m_CombatInfoHash; }

	bool IsMale() const { return GetPersonalitySettings().GetIsMale(); }
	bool IsUsingFemaleSkeleton() const;

	CPedVariationInfoCollection* GetVarInfo(void) {return(m_pPedVarInfoCollection);}
	const CPedVariationInfoCollection* GetVarInfo(void) const {return(m_pPedVarInfoCollection);}

	// Load all weapon data files, including those from the current episodic pack (if any)
	static void LoadPedPersonalityData();

	// Post process the data once loaded
	static void PostLoadPedPersonalityData();

	// Load all ped perception data files, including those from the current episodic pack (if any)
	static void LoadPedPerceptionData();
	
	void ClearCountedAsScenarioPed();

	// Set the audio group maps between peds with particular components
	void AddAudioGroupMap( s32 iHeadComponent, u32 iAudioHash );

	// Get the group that maps between peds with particular components
	bool GetAudioGroupForHeadComponent( s32 iHeadComponent, u32& iOutAudioHash );

#if __DEV
	static void InitPedBuoyancyWidget();//called by cphysics when the physics bank has been setup.
#endif

	static void SetupPedBuoyancyInfo(CBuoyancyInfo& buoyancyInfo, const CBaseCapsuleInfo* pCapsuleInfo, const fragType& fragType, bool bIsWeightless);	

	phBoundComposite* GeneratePedBound();

	bool	GetIsStreamedGfx(void) const { return(m_isStreamedGraphics == 1); }
	void	SetIsStreamedGfx(bool bSet) { m_isStreamedGraphics = bSet; }

	bool	GetAmbulanceShouldRespondTo() const {return(m_AmbulanceShouldRespondTo == 1);}
	void	SetAmbulanceShouldRespondTo(bool bSet) { m_AmbulanceShouldRespondTo = bSet;	}

	bool	GetCanRideBikeWithNoHelmet(void) const { return(m_CanRideBikeWithNoHelmet == 1); }
	void	SetCanRideBikeWithNoHelmet(bool bSet) { m_CanRideBikeWithNoHelmet = bSet; }

	bool	GetCanSpawnInCar(void) const { return m_CanSpawnInCar; }
	void  SetCanSpawnInCar(bool bSet) { m_CanSpawnInCar = bSet; }
 
	bool  GetOnlyBulkyItemVariations() const		{ return m_bOnlyBulkyItemVariations; }
	void  SetOnlyBulkyItemVariations(bool bSet) { m_bOnlyBulkyItemVariations = bSet; }

	void		SetStreamFolder(char* pFolderName) { m_StreamFolder = pFolderName; }
	const char* GetStreamFolder() const  { return m_StreamFolder; }
	void		SetPropStreamFolder(const char* pFolderName) { m_propStreamFolder = pFolderName; }
	const char* GetPropStreamFolder() const  { return m_propStreamFolder; }
	bool		HasStreamedProps() const { return strcmp(GetPropStreamFolder(), "null") != 0; }

	bool		HasHelmetProp() const;

	eScenarioPopStreamingSlot GetScenarioPedStreamingSlot() const { return m_ScenarioPedStreamingSlot; }


	// Spawn location preferences.
	bool		ShouldSpawnInAirByDefault()				const { return m_DefaultSpawningPreference == DSP_AERIAL; }
	bool		ShouldSpawnInWaterByDefault()			const { return m_DefaultSpawningPreference == DSP_AQUATIC; }
	bool		ShouldSpawnAsWildlifeByDefault()		const { return m_DefaultSpawningPreference == DSP_GROUND_WILDLIFE; }

	float		GetDefaultRemoveRangeMultiplier()		const {	return m_DefaultRemoveRangeMultiplier; }

	// Disregard the normal inview / in range checks for this ped.
	bool		AllowsCloseSpawning()					const { return m_AllowCloseSpawning; }

	void		SetCanStreamOutAsOldest(bool bCanStreamOut)	  { m_AllowStreamOutAsOldest = bCanStreamOut; }
	bool		CanStreamOutAsOldest()					const { return m_AllowStreamOutAsOldest; }

	// PURPOSE: Add a reference to this archetype and add ref to any HD assets loaded
	virtual void AddHDRef(bool) {}

	// HD stuff
	void		SetupHDFiles(const char* pName);
	void		ConfirmHDFiles();

	s32	GetHDTxdIndex() const { return m_HDtxdIdx; }
	bool		GetAreHDFilesLoaded() const;

	void		SetHDDist(float HDDist) { m_HDDist = HDDist; }
	float		GetHDDist() { return m_HDDist; }
	bool		GetHasHDFiles() const { return (m_HDDist > 0.0f); }

	void		SetTargetingThreatModifier(float fModifier) { m_TargetingThreatModifier = fModifier; }
	float		GetTargetingThreatModifier() { return m_TargetingThreatModifier; }

	// If positive, this range overrides the default perception ranges for things like seen dead body, seen ped killed, ped injured, and ped shot.
	// The rationale is that small peds like rats probably shouldn't be noticed from as far away as big things like deer.
	void		SetKilledPerceptionRangeModifer(float fModifier)	{ m_KilledPerceptionRangeModifer = fModifier; }
	float		GetKilledPerceptionRangeModifer() const				{ return m_KilledPerceptionRangeModifer; }

	bool		CheckSexinessFlags(u32 flags) const		{ return (m_Sexiness & flags) == flags; }
	u32			GetSexinessFlags() const				{ return m_Sexiness; }	
	void		SetSexinessFlags(u32 flags)				{ m_Sexiness = flags; }

	u8			GetAge() const							{ return m_Age; } 
	void		SetAge(u8 v)							{ m_Age = v; }

	u16			GetLightFxBoneIdx() const				{ return (u16)m_LightFxBoneIdx; }
	u16			GetLightFxBoneSwitchIdx() const			{ return (u16)m_LightFxBoneSwitchIdx; }

	u16			GetLightMpBoneIdx() const				{ return (u16)m_LightMpBoneIdx; }

	u8			GetMaxPassengersInCar() const			{ return m_MaxPassengersInCar; }
	void		SetMaxPassengersInCar(u8 maxPassengers ){ m_MaxPassengersInCar = maxPassengers; }
	
	u8			GetExternallyDrivenDOFs() const			{ return m_ExternallyDrivenDOFs; }	
	void		SetExternallyDrivenDOFs(u8 externallyDrivenDOFs ){ m_ExternallyDrivenDOFs = externallyDrivenDOFs; }

	crFrameFilter* GetInvalidRagdollFilter()			{ return m_pInvalidRagdollFilter; }

	void		AddPedInstance(CPed* ped);
	void		RemovePedInstance(CPed* ped);
	CPed*		GetPedInstance(u32 index);
	u32			GetNumPedInstances() const				{ return m_pedInstances.GetCount(); }
	float		GetDistanceSqrToClosestInstance(const Vector3& pos);

	atHashString	GetAmbientClipsForFlee() const		{ return m_AmbientClipsForFlee; }

	// Parse in a peds.meta file.
	static bool LoadPedMetaFile(const char* szFileName, bool permanent, s32 mapTypeDefIndex = fwFactory::GLOBAL);

	static void AppendPedPersonalityFile(const char* szFileName);

#if __BANK
	static void	DumpPedInstanceDataCB();
    void DumpAveragePedSize();
    static atArray<sDebugSize> ms_pedSizes;
#endif

protected:

	// Parse in a pedpersonality.meta file.
	static	void LoadPedPersonalityFile(const char* szFileName);

	//load seatInfo if ridable
	void InitLayout( crSkeletonData *pSkelData );

	void SetInvalidRagdollFilter(crFrameFilter *pFrameFilter);

	template <typename T>
	static void AppendUniques(rage::atArray<T> &to, rage::atArray<T> &from)
	{
		for(int i = 0; i < from.GetCount(); i++)
		{
			bool appendThis	= true;
			for(int j=0;j<to.GetCount();j++)
			{
				if(to[j].GetNameHash()==from[i].GetNameHash())
				{
					appendThis=false;
					break;
				}
			}
			if(appendThis)
			{
				to.PushAndGrow(from[i]);
			}
		}
	}
	
	template <typename T>
	static void AppendArray(rage::atArray<T> &to, const rage::atArray<T> &from)
	{
		for(int i = 0; i < from.GetCount(); i++)
		{
			to.PushAndGrow(from[i]);
		}
	}

//	static RwObjectNameIdAssocation m_pPedIds[];
//	static colModelNodeInfo m_pColNodeInfos[];
	Vector3 m_ProbeTopPosition[2];
	float m_ProbeDepth;

	s32		m_propsFileIndex;
	s32		m_propsFilenameHashcode;

	s32		m_pedCompFileIndex;
	atArray<s32> m_pedMetaDataFileIndices;
	fwMvClipSetId m_movementClipSet;
	atArray<fwMvClipSetId> m_movementClipSets;
	fwMvClipSetId m_strafeClipSet;
	fwMvClipSetId m_movementToStrafeClipSet;
	fwMvClipSetId m_injuredStrafeClipSet;
	fwMvClipSetId m_fullBodyDamageClipSet;
	fwMvClipSetId m_additiveDamageClipSet;
	fwMvClipSetId m_defaultGestureClipSet;
	//fwMvClipSetId m_phoneGestureClipSet;
	fwMvClipSetId m_defaultVisemeClipSet;
	fwMvClipSetId m_SidestepClipSet;
	fwMvExpressionSetId m_expressionSet;
	fwMvFacialClipSetGroupId m_facialClipSetGroupId;
	atHashString  m_GetupSetHash;
	//atHashString  m_PoseMatcherName;
	bool		m_isPlayerModel;
	ePedType	m_defaultPedType;
//	ePedStats	m_defaultPedStats;
	float	m_MinActivationImpulse;
	float	m_Stubble;
	u32		m_LastTimeUsed;
	u16		m_drivesWhichCars;
	s16		m_PersonalityIdx;
	s32		m_NumTimesUsed;
	s32		m_NumTimesOnFoot;				// How many times is this ped used? (the modelinfo refs are unreliable)
	s32		m_NumTimesInCar;				// How many times is this ped used? (the modelinfo refs are unreliable)
	s32		m_numPedModelRefs;
	//	CColModel*	m_pHitColModel;
	s8		m_radio1, m_radio2;
	f32		m_FUpOffset, m_FFrontOffset,m_RUpOffset,m_RFrontOffset;
	u8		m_Race;
	u8		m_NumTimesInReusePool;
	s16		m_AudioPedType;
	s16		m_FirstVoice, m_LastVoice;
	s16		m_NextVoice;

	u32		m_PedVoiceGroupHash;
	u32		m_AnimalAudioObjectHash;
	u32		m_CombatInfoHash;
	u32		m_Sexiness;
	u16		m_LightFxBoneIdx;
	u16		m_LightFxBoneSwitchIdx;
	u16		m_LightMpBoneIdx;
	u8		m_Age;	
	u8		m_MaxPassengersInCar;
	u8		m_ExternallyDrivenDOFs;

	CAudioGroupMap	m_audioGroupMaps[MAX_AUDIO_GROUP_MAPS];
	s32			m_iAudioGroupCount;

	atString		m_StreamFolder;
	atString		m_propStreamFolder;

	eScenarioPopStreamingSlot		m_ScenarioPedStreamingSlot;
	DefaultSpawnPreference			m_DefaultSpawningPreference;
	float							m_DefaultRemoveRangeMultiplier;
	bool							m_AllowCloseSpawning;
	bool							m_AllowStreamOutAsOldest;



	//optional seat info component if this is rideable
	CModelSeatInfo	*m_pSeatInfo;

	// vfx info
	CVfxPedInfo* m_pVfxInfo;

	crFrameFilter* m_pInvalidRagdollFilter;

	// HD streaming
	float		m_HDDist;
	s32			m_HDtxdIdx;
	fwPtrListSingleLink m_HDInstanceList;

	// Targeting
	float m_TargetingThreatModifier;

	// Modifiers for other peds to notice this ped dying.
	float m_KilledPerceptionRangeModifer;

	atArray<u8> m_pedInstances; // NOTE: ped pool size cannot be larger than a u8

	atHashString m_AmbientClipsForFlee;

//	phArchetype *m_pRagdollArch;
//	s16 m_physicsDictionaryIndexRagdoll;
public:

	static void Update();

	// variation data for setting different tex on the model
	CPedVariationInfoCollection* m_pPedVarInfoCollection;

	atHashString m_motionTaskDataSetHash;
	atHashString m_defaultTaskDataSetHash;
	atHashString m_pedCapsuleHash;
	atHashString m_LayoutHash;
	atHashString m_pedComponentSetHash;
	atHashString m_pedComponentClothHash;
	atHashString m_pedIKSettingsHash;
	atHashString m_TaskDataHash;
	atHashString m_decisionMakerHash;
	atHashString m_relationshipGroupHash;
	atHashString m_navCapabilitiesHash;
	atHashString m_perceptionHash;
	atHashString m_DefaultBrawlingStyle;
	atHashString m_DefaultUnarmedWeapon;

	SpecialAbilityType m_pedAbilityType;

	bool			m_bIs_IG_Ped;
	u8				m_numAvailableLODs;

	u8		m_CountedAsScenarioPed : 1;			// This ped model has been streamed in specifically for a scenario.
	u8		m_isStreamedGraphics : 1;			// models for this ped are to be streamed individually instead of in a pack
	u8		m_AmbulanceShouldRespondTo : 1;
	u8		m_ThermalBehaviour : 2;
	u8		m_SuperlodType : 3;
	s8		m_isRequestedAsDriver;				// keep track of how many car types actually want this ped as a driver
	u8		m_CanRideBikeWithNoHelmet : 1;
	u8		m_CanSpawnInCar : 1;
	u8		m_bIsHeadBlendPed : 1;
	u8    m_bOnlyBulkyItemVariations : 1; // At least one part of this ped model has only BulkyItem variations.
	u8		m_pad : 4;

public:
	static dev_float ms_fPedRadius;
#if __BANK
	static void PedDataChangedCB();
	void RefreshPedBoundsCB();
	void RefreshPedBound(phBound* pBound);
#endif

	static void InitClass(unsigned initMode);
	static void ShutdownClass(unsigned shutdownMode);

	static void SetupCommonData();
	static void DeleteCommonData();

	static void AddResidentObject(strLocalIndex index, int module);
	static const atArray<strIndex>& GetResidentObjects() {return ms_residentObjects;}
	static void SetCommonTxd(s32 txd) { ms_commonTxd = txd; }
	static s32 GetCommonTxd() { return ms_commonTxd; }

	static const PersonalityData& GetPersonality(s32 idx) { return ms_personalities.m_PedPersonalities[idx]; }
	static const PersonalityBravery* GetPersonalityBravery(u32 hash);
    static PersonalityCriminality* GetPersonalityCriminality(u32 hash);
	static const PersonalityMovementModes* FindPersonalityMovementModes(u32 hash);
	static const PersonalityMovementModes::sUnholsterClipData* FindPersonalityMovementModeUnholsterData(u32 hash);
	static const s16 FindPersonalityFromHash(u32 hash);

#if __BANK
	static void	AddWidgets(bkBank& bank);
	static void	SaveCallback();
	static void	AddNewPersonalityCallback();
	static void	AddNewBraveryCallback();
    static void	AddNewCriminalityCallback();
	static void RefreshWidgets();
	static void RemovePersonalityCallback(CallbackData obj, CallbackData clientData);
	static void RemoveBraveryCallback(CallbackData obj, CallbackData clientData);
    static void RemoveCriminalityCallback(CallbackData obj, CallbackData clientData);

	static bkGroup* ms_bankGroup;
#endif

	static atArray<strIndex> ms_residentObjects;
	static s32 ms_commonTxd;

	static PersonalityDataList ms_personalities;

	static fwPtrListSingleLink ms_HDStreamRequests;
};

inline void CPedModelInfo::AddResidentObject(strLocalIndex index, int module)
{
	strIndex streamingIndex = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(module)->GetStreamingIndex(index);
	ms_residentObjects.PushAndGrow(streamingIndex);
}


//-------------------------------------------
// SPedPerceptionInfo
// Struct for datatypes for sensing model.
//-------------------------------------------

struct SPedPerceptionInfo {
public:
	atHashString m_Name;
	float	m_HearingRange;
	float	m_EncroachmentRange;
	float	m_EncroachmentCloseRange;
	float	m_SeeingRange;
	float	m_SeeingRangePeripheral;
	float	m_VisualFieldMinAzimuthAngle;
	float	m_VisualFieldMaxAzimuthAngle;
	float	m_VisualFieldMinElevationAngle;
	float	m_VisualFieldMaxElevationAngle;
	float	m_CentreFieldOfGazeMaxAngle;
	bool	m_CanAlwaysSenseEncroachingPlayers;
	bool	m_PerformEncroachmentChecksIn3D;
	PAR_SIMPLE_PARSABLE;
};

//----------------------------------------------
// CPedPerceptionInfoManager
// Singleton manager class for perception types
//-----------------------------------------------
class CPedPerceptionInfoManager
{
public:
	// Initialise
	static void Init(const char* pFileName);

	// Shutdown
	static void Shutdown();

	// Accessor for perception types based on hashes
	static const SPedPerceptionInfo* GetPerception(u32 hash)
	{
		for(s32 i = 0; i < m_PedPerceptionInfoManagerInstance.m_aPedPerceptionInfoData.GetCount(); ++i)
		{
			if (m_PedPerceptionInfoManagerInstance.m_aPedPerceptionInfoData[i].m_Name.GetHash() == hash)
			{
				return &m_PedPerceptionInfoManagerInstance.m_aPedPerceptionInfoData[i];
			}
		}
		return NULL;
	}

private:

	// Delete the data
	static void Reset();

	// Load the data
	static void Load(const char* pFileName);

	// Array containing the perception structs
	atArray<SPedPerceptionInfo> m_aPedPerceptionInfoData;

	// Singleton manager instance
	static CPedPerceptionInfoManager m_PedPerceptionInfoManagerInstance;

	PAR_SIMPLE_PARSABLE;
};

#endif // INC_PED_MODELINFO_H_
