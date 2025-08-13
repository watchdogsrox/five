#ifndef INC_CONDITIONAL_ANIMS_H
#define INC_CONDITIONAL_ANIMS_H

// Rage headers
#include "atl/array.h"
#include "atl/hashstring.h"
#include "atl/string.h"

// Framework
#include "fwanimation/clipsets.h"
#include "fwutil/Flags.h"

// Game
#include "Animation/AnimDefines.h"
#include "Task/Scenario/Info/ScenarioInfoConditions.h"

// Forward declarations
class CDynamicEntity;
class CAmbientPropModelSet;

////////////////////////////////////////////////////////////////////////////////
// CConditionalClipSet
////////////////////////////////////////////////////////////////////////////////

typedef atArray<CScenarioCondition*> ConditionsArray;

class CConditionalHelpers
{
public:

	static bool CheckConditionsHelper( const ConditionsArray & conditions , const CScenarioCondition::sScenarioConditionData& conditionData, s32 * firstFailureCondition );
	static int GetImmediateConditionsHelper(const ConditionsArray& conditions, const CScenarioCondition** pArrayOut, int maxNumber);

};

//TODO: compress data better ... 
class CConditionalClipSetVFX
{
public:
	atHashWithStringNotFinal m_Name;
	atHashWithStringNotFinal m_fxName;
	Vector3 m_offsetPosition;
	Vector3 m_eulerRotation;
	eAnimBoneTag m_boneTag;
	float m_scale;
	int m_probability;
	Color32 m_color;

	PAR_SIMPLE_PARSABLE;
};

class CConditionalClipSet
{
public:

	~CConditionalClipSet();

	enum eActionFlags
	{
		RemoveWeapons							= BIT0,
		IsSkyDive									= BIT1,
		IsArrest									= BIT2,
		IsProvidingCover					= BIT3,
		IsSynchronized						= BIT4,
		EndsInWalk								= BIT5,
		ConsiderOrientation				= BIT6,
		MobilePhoneConversation		= BIT7,
		ReplaceStandardJack				= BIT8,
		ForceIdleThroughBlendOut	= BIT9
	};

	fwMvClipSetId GetClipSetId() const { return fwMvClipSetId(GetClipSetHash()); }
	u32 GetClipSetHash() const { return m_ClipSet.GetHash(); }

	atHashWithStringNotFinal GetAssociatedSpeech() const { return m_AssociatedSpeech; }

	bool CheckConditions( const CScenarioCondition::sScenarioConditionData& conditionData ) const;
	int GetImmediateConditions(const CScenarioCondition** pArrayOut, int maxNumber) const;

#if !__FINAL
	const char * GetClipSetName() const { return m_ClipSet.GetCStr(); }
#endif

	strLocalIndex GetAnimDictIndex() const 
	{ 
		return strLocalIndex(fwClipSetManager::GetClipDictionaryIndex( fwMvClipSetId(m_ClipSet) ));
	}

	bool GetRemoveWeapons() const							{ return m_ActionFlags.IsFlagSet(RemoveWeapons); }
	bool GetIsSkyDive() const									{ return m_ActionFlags.IsFlagSet(IsSkyDive); }
	bool GetIsArrest() const									{ return m_ActionFlags.IsFlagSet(IsArrest); }
	bool GetIsProvidingCover() const					{ return m_ActionFlags.IsFlagSet(IsProvidingCover); }
	bool GetIsSynchronized() const						{ return m_ActionFlags.IsFlagSet(IsSynchronized); }
	bool GetEndsInWalk() const								{ return m_ActionFlags.IsFlagSet(EndsInWalk); }
	bool GetConsiderOrientation() const				{ return m_ActionFlags.IsFlagSet(ConsiderOrientation); }
	bool IsMobilePhoneConversation() const		{ return m_ActionFlags.IsFlagSet(MobilePhoneConversation); }
	bool GetReplaceStandardJack() const				{ return m_ActionFlags.IsFlagSet(ReplaceStandardJack); }
	bool GetForceIdleThroughBlendOut() const	{ return m_ActionFlags.IsFlagSet(ForceIdleThroughBlendOut); }

private:

	// Conditions on a per-anim-set basis
	ConditionsArray m_Conditions;

	// Flags to do extra things after the conditions have passed
	fwFlags32 m_ActionFlags;

	// The animation set to use
	atHashWithStringNotFinal m_ClipSet;

	// If specified, the speech category to attempt to say
	atHashWithStringNotFinal m_AssociatedSpeech;
	
	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CScenarioTransitionInfo
////////////////////////////////////////////////////////////////////////////////

class CScenarioTransitionInfo
{
public:
	
	inline atHashWithStringNotFinal GetTransitionToScenarioType() const { return m_TransitionToScenario; }
	inline atHashWithStringNotFinal GetTransitionToScenarioConditionalAnims() const { return m_TransitionToScenarioConditionalAnims; }

private:

	atHashWithStringNotFinal m_TransitionToScenario;
	atHashWithStringNotFinal m_TransitionToScenarioConditionalAnims;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CConditionalAnims
////////////////////////////////////////////////////////////////////////////////

class CConditionalAnims
{
public:

	typedef enum
	{
		AT_BASE,
		AT_ENTER,
		AT_EXIT,
		AT_VARIATION,
		AT_REACTION,
		AT_PANIC_BASE,
		AT_PANIC_INTRO,
		AT_PANIC_OUTRO,
		AT_PANIC_VARIATION,
		AT_PANIC_EXIT,
		AT_COUNT
	} eAnimType;

#if __BANK
	static const char* GetTypeAsStr(CConditionalAnims::eAnimType animType)
	{
		switch (animType)
		{
		case CConditionalAnims::AT_BASE:			return "AT_BASE";
		case CConditionalAnims::AT_ENTER:			return "AT_ENTER";
		case CConditionalAnims::AT_EXIT:			return "AT_EXIT";
		case CConditionalAnims::AT_VARIATION:		return "AT_VARIATION";
		case CConditionalAnims::AT_REACTION:		return "AT_REACTION";
		case CConditionalAnims::AT_PANIC_BASE:		return "AT_PANIC_BASE";
		case CConditionalAnims::AT_PANIC_INTRO:		return "AT_PANIC_INTRO";
		case CConditionalAnims::AT_PANIC_OUTRO:		return "AT_PANIC_OUTRO";
		case CConditionalAnims::AT_PANIC_VARIATION:	return "AT_PANIC_VARIATION";
		case CConditionalAnims::AT_PANIC_EXIT:		return "AT_PANIC_EXIT";
		default:
			aiAssert(false);
			break;
		}
		return NULL;
	}
#endif

	typedef atArray<CConditionalClipSet*> ConditionalClipSetArray;

	CConditionalAnims();
	~CConditionalAnims();

	const ConditionalClipSetArray * GetClipSetArray( CConditionalAnims::eAnimType animType ) const 
	{
		switch (animType)
		{
			case CConditionalAnims::AT_BASE:
				return &m_BaseAnims;
			case CConditionalAnims::AT_ENTER:
				return &m_Enters;
			case CConditionalAnims::AT_EXIT:
				return &m_Exits;
			case CConditionalAnims::AT_VARIATION:
				return &m_Variations;
			case CConditionalAnims::AT_REACTION:
				return &m_Reactions;
			case CConditionalAnims::AT_PANIC_BASE:
				return &m_PanicBaseAnims;
			case CConditionalAnims::AT_PANIC_INTRO:
				return &m_PanicIntros;
			case CConditionalAnims::AT_PANIC_OUTRO:
				return &m_PanicOutros;
			case CConditionalAnims::AT_PANIC_VARIATION:
				return &m_PanicVariations;
			case CConditionalAnims::AT_PANIC_EXIT:
				return &m_PanicExits;
			default:
				aiAssert(false);
				break;
		}

		return NULL;
	}

	bool CheckClipSet( const ConditionalClipSetArray & anims, const CScenarioCondition::sScenarioConditionData& conditionData ) const;
	bool CheckConditions( const CScenarioCondition::sScenarioConditionData& conditionData, s32 * firstFailureCondition ) const;

	// Check only those conditions that can be checked before a ped is actually created - i.e. ones that deal with a ped model index not an actual ped.
	bool CheckPopulationConditions( const CScenarioCondition::sScenarioConditionData& conditionData ) const;
	bool CheckPropConditions( const CScenarioCondition::sScenarioConditionData& conditionData ) const;
	int GetImmediateConditions(u32 clipSetHash, const CScenarioCondition** pArrayOut, int maxNumber, eAnimType animType) const;

	static const CConditionalClipSet * ChooseClipSet( const ConditionalClipSetArray & anims, const CScenarioCondition::sScenarioConditionData& conditionData );
#if __BANK
	const CConditionalClipSet * ChooseNextClipSet(const fwMvClipSetId& lastClipSet, const ConditionalClipSetArray & anims, const CScenarioCondition::sScenarioConditionData& conditionData ) const;
#endif

	u32 GetPropSetHash() const;

	bool GetCreatePropInLeftHand() const { return m_Flags.IsFlagSet(SpawnInLeftHand); }

	// PURPOSE:	Get the probability value we should use, in the context passed in by the user. Normally
	//			this will just be m_Probability, but under certain conditions, it may be m_SpecialConditionProbability.
	float GetConditionalProbability(const CScenarioCondition::sScenarioConditionData& conditionData) const;

	bool ShouldDestroyPropInsteadOfDropping() const { return m_Flags.IsFlagSet(DestroyPropInsteadOfDrop); }
	bool ShouldIgnoreLowPriShockingEvents() const { return m_Flags.IsFlagSet(IgnoreLowPriShockingEvents); }
	bool IsMobilePhoneConversation() const		{ return m_Flags.IsFlagSet(MobilePhoneConversation); }
	bool IsForceBaseUntilIdleStreams() const	{ return m_Flags.IsFlagSet(ForceBaseUntilIdleStreams); }
	bool ShouldNotPickOnEnter() const			{ return m_Flags.IsFlagSet(DontPickOnEnter); }

	bool GetOverrideOnFootClipSetWithBase() const { return (m_Flags.IsFlagSet(OverrideOnFootClipSetWithBase)); }
	bool GetOverrideWeaponClipSetWithBase() const { return (m_Flags.IsFlagSet(OverrideWeaponClipSetWithBase)); }

	float GetNextIdleTime() const { return m_NextIdleTime; }
	float GetChanceOfSpawningWithAnything() const { return m_ChanceOfSpawningWithAnything; }
	float GetBlendInDelta() const { return m_BlendInDelta; }
	float GetBlendOutDelta() const { return m_BlendOutDelta; }

	fwMvClipSetId GetGestureClipSetId() const { return m_GestureClipSetId; }
	fwMvClipId GetLowLodBaseAnim() const { return m_LowLodBaseAnim; }

#if !__FINAL
	bool HasAnimationData() const;
#endif

	enum eConditionalAnimFlags
	{
		SpawnInLeftHand					= BIT0,
		OverrideOnFootClipSetWithBase	= BIT1,
		DestroyPropInsteadOfDrop		= BIT2,	// Don't drop the prop if the next task doesn't want it; destroy it instead. For use with small props.
		IgnoreLowPriShockingEvents		= BIT3,	// Don't react to low priority shocking events (such as seeing a running ped) if playing these animations.
		MobilePhoneConversation			= BIT4, // Is this conditional anim a phone conversation? [7/19/2012 mdawe]
		OverrideWeaponClipSetWithBase	= BIT5, // Override the Move task's weapon clipset with the base animation.
		ForceBaseUntilIdleStreams		= BIT6, // Keep playing the base clip until an idle streams when the base's conditions stop being met.
		DontPickOnEnter					= BIT7	// Not to be used as an AT_ENTER anim type
	};

	const char* GetName() const { return m_Name.TryGetCStr(); }
	const char* GetPropSetName() const { return m_PropSet.TryGetCStr(); }

	u32 GetNameHash() const { return m_Name.GetHash(); }

	int GetVFXCount() const { return m_VFXData.GetCount(); }
	const CConditionalClipSetVFX& GetVFX(int idx) const { Assert(idx >= 0); Assert(idx < m_VFXData.GetCount()); Assert(m_VFXData[idx]); return *m_VFXData[idx]; }
	const CConditionalClipSetVFX* GetVFXByName(atHashWithStringNotFinal name) const;

	float GetVFXCullRange() const { return m_VFXCullRange; }

	const CScenarioTransitionInfo* GetScenarioTransitionInfo() const { return m_TransitionInfo; }

private:

	atHashWithStringNotFinal m_Name;
	atHashWithStringNotFinal m_PropSet;
	fwFlags32 m_Flags;
	float m_Probability;

	// PURPOSE:	Pointer to an owned optional CScenarioCondition, which if fulfilled will activate
	//			the "special condition probability".
	CScenarioCondition*	m_SpecialCondition;

	// PURPOSE:	If the optional special condition is fulfilled, this is the probability to use instead
	//			of m_Probability.
	float				m_SpecialConditionProbability;

	float m_NextIdleTime;
	float m_ChanceOfSpawningWithAnything;
	float m_BlendInDelta;
	float m_BlendOutDelta;

	fwMvClipSetId m_GestureClipSetId;

	// PURPOSE:	Name of animation in the CTaskAmbientClips::Tunables::m_LowLodBaseClipSetId set, to use if the regular base animations are not streamed in.
	fwMvClipId m_LowLodBaseAnim;

	// VFX used by this conditional clip set
	atArray<CConditionalClipSetVFX*> m_VFXData;

	// PURPOSE:	Distance at which the effects associated with these animations may be culled at a high level, for performance reasons.
	float m_VFXCullRange;

	// this scenario will use this info to chain to a new scenario if available
	CScenarioTransitionInfo* m_TransitionInfo;

	// Conditions that we use these anims
	ConditionsArray m_Conditions;

	// This will play under the variation anims
	ConditionalClipSetArray m_BaseAnims;

	// Intro animations - to transition into the scenario
	ConditionalClipSetArray m_Enters;

	// Outro animations - to transition out of the scenario
	ConditionalClipSetArray m_Exits;

	// Variations - idle variations
	ConditionalClipSetArray m_Variations;
	
	// Reactions - idle variations
	ConditionalClipSetArray m_Reactions;

	// Basic idle for panic
	ConditionalClipSetArray m_PanicBaseAnims;

	// Intro into the panic base
	ConditionalClipSetArray m_PanicIntros;

	// Outro from the panic base (back to just a normal idle)
	ConditionalClipSetArray m_PanicOutros;

	// Variations - panic idle variations
	ConditionalClipSetArray m_PanicVariations;

	// Exit directly into a flee 
	ConditionalClipSetArray m_PanicExits;

	PAR_SIMPLE_PARSABLE;
};


////////////////////////////////////////////////////////////////////////////////
// CConditionalAnimsGroup
////////////////////////////////////////////////////////////////////////////////

class CConditionalAnimsGroup
{
public:

	typedef atArray<CConditionalAnims> ConditionalAnimsArray;

	u32 GetHash() const { return m_Name.GetHash(); }
	NOTFINAL_ONLY(const char * GetName() const { return m_Name.GetCStr(); })
	const CConditionalAnims * GetAnims(u32 index) const { return &m_ConditionalAnims[index]; }
	const u32 GetNumAnims() const { return m_ConditionalAnims.size(); }

	// Return true if there is at least one conditional anim set that qualifies.
	bool CheckForMatchingConditionalAnims(const CScenarioCondition::sScenarioConditionData& rConditionData) const;

	// Return true if there is at least one conditional anim that qualifies (but only check conditions that only require a model index)
	bool CheckForMatchingPopulationConditionalAnims(const CScenarioCondition::sScenarioConditionData& rConditionData) const;

#if !__FINAL
	bool HasAnimationData() const;
	bool HasProp() const;
#endif

private:

	atHashWithStringNotFinal m_Name;
	ConditionalAnimsArray m_ConditionalAnims; // (eg: for wander an array of PHONE, TEXT, COFFEE, UMBRELLA)

	PAR_SIMPLE_PARSABLE;
};


#endif // INC_CONDITIONAL_ANIMS_H
