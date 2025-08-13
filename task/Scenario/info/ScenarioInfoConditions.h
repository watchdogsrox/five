#ifndef INC_SCENARIO_INFO_CONDITIONS_H
#define INC_SCENARIO_INFO_CONDITIONS_H

// Rage headers
#include "ai/aichannel.h"
#include "atl/array.h"
#include "bank/bank.h"
#include "parser/macros.h"
#include "fwutil/rtti.h"

// Game headers
#include "ModelInfo/PedModelInfo.h"
#include "peds/PedFlagsMeta.h"
#include "Task/Movement/JetpackObject.h"

// Forward declarations
class CAmbientModelSet;
class CAmbientPedModelSet;
class CPed;
class CVehicle;

//enums

enum AmbientEventType
{
	AET_No_Type=0,
	AET_Interesting=1,
	AET_Threatening=2,
	AET_Threatened=3,
	AET_In_Place=4,
	AET_Directed_In_Place=5,
	AET_Flinch=6,
};

enum RoleInSyncedScene
{
	RISS_Invalid,
	RISS_Master,
	RISS_Slave
};

enum MovementModeType
{
	MMT_Invalid,
	MMT_Action,
	MMT_Stealth,
	MMT_Any,
};



enum RadioGenrePar
{
	GENRE_GENERIC,
	GENRE_COUNTRY,
	GENRE_HIPHOP,
	GENRE_MEXICAN,
	GENRE_MOTOWN,
	GENRE_PUNK,
};

// Forward declare weather conditions so we can declare them as friends of CScenarioCondition::sScenarioConditionData
class CScenarioConditionRaining;
class CScenarioConditionSunny;
class CScenarioConditionSnowing;
class CScenarioConditionWindy;

////////////////////////////////////////////////////////////////////////////////
// CScenarioCondition
////////////////////////////////////////////////////////////////////////////////

class CScenarioCondition : public datBase
{
	DECLARE_RTTI_BASE_CLASS(CScenarioCondition);
public:

	virtual ~CScenarioCondition() {}

	struct sScenarioConditionData
	{
		sScenarioConditionData()
			: vAmbientEventPosition(V_ZERO)
			, pPed(NULL)
			, pOtherPed(NULL)
			, pTargetVehicle(NULL)
			, fPointHeading(0.0f)
			, iTargetEntryPoint(-1)
			, iScenarioFlags(0)
			, eAmbientEventType(AET_No_Type)
			, m_RoleInSyncedScene(RISS_Invalid)
			, m_ModelId()
			, m_AllowNotSynchronized(true)
			, m_AllowSynchronized(true)
			, m_bCheckUntilFailedWeatherCondition(false)
			, m_bOutFailedWeatherCondition(false)
			, m_bNeedsValidEnterClip(false)
			, m_bIsReaction(false)
			, m_RadioGenrePar(GENRE_GENERIC)
		{

		}

		inline bool GetFailedWeatherCondition() const { aiAssert(m_bCheckUntilFailedWeatherCondition); return m_bOutFailedWeatherCondition; }

		Vec3V					vAmbientEventPosition;
		const CPed*				pPed;
		const CPed*				pOtherPed;
		const CVehicle*			pTargetVehicle;
		float					fPointHeading;
		s32						iTargetEntryPoint;
		s32						iScenarioFlags;
		AmbientEventType		eAmbientEventType;
		RoleInSyncedScene		m_RoleInSyncedScene;
		fwModelId				m_ModelId;
		bool					m_AllowNotSynchronized;
		bool					m_AllowSynchronized;
		bool					m_bCheckUntilFailedWeatherCondition; // If true, will check conditions until a weather condition fails or all conditions are checked.
		bool					m_bNeedsValidEnterClip;
		bool					m_bIsReaction;						// Used to seperate out reaction exits from normal exits.
		RadioGenrePar			m_RadioGenrePar;
#if __ASSERT
		const char*		m_ScenarioName;
#endif

		// Weather classes declared friends so they can change m_bOutFailedWeatherCondition
		friend CScenarioConditionRaining;
		friend CScenarioConditionSunny	;
		friend CScenarioConditionSnowing;
		friend CScenarioConditionWindy	;

	private:
		// This is private so we can enforce m_bCheckUntilFailedWeatherCondition being set by anybody wanting to check this.
		mutable bool	m_bOutFailedWeatherCondition;	// True if this condition failed due to a weather condition. Not reliable unless m_bCheckUntilFailedWeatherCondition is also set.
	};

	// Check condition
	bool Check(const sScenarioConditionData& conditionData) const { return CheckInternal(conditionData) == m_Result; }

protected:

	// Resulting return value
	enum Result
	{
		SCR_True,
		SCR_False,
	};

	// Virtual function for conditional check
	virtual Result CheckInternal(const sScenarioConditionData& UNUSED_PARAM(conditionData)) const { Assertf(false, "CScenarioCondition base type should not be used"); return SCR_False; }

	// Comparison check
	enum Comparison
	{
		SCC_Equal,
		SCC_LessThan,
		SCC_GreaterThan,
	};

	// Compare the value based on the comparison
	template<typename _Type>
	Result Compare(Comparison comparison, _Type t1, _Type t2) const;

#if __BANK
	virtual void PreAddWidgets(bkBank& bank);
	virtual void PostAddWidgets(bkBank& bank);
#endif // __BANK

	Result m_Result;
private:
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

template<typename _Type> 
inline CScenarioCondition::Result CScenarioCondition::Compare(Comparison comparison, _Type t1, _Type t2) const
{
	switch(comparison)
	{
	case SCC_Equal:
		return t1 == t2 ? SCR_True : SCR_False;
	case SCC_LessThan:
		return t1 < t2 ? SCR_True : SCR_False;
	case SCC_GreaterThan:
		return t1 > t2 ? SCR_True : SCR_False;
	default:
		aiAssertf(0, "Unhandled comparison type");
		return SCR_False;
	}
}

////////////////////////////////////////////////////////////////////////////////
// CScenarioConditionSet
////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionSet : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionSet, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;

	typedef atArray<CScenarioCondition*> Conditions;
	Conditions m_Conditions;

private:
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

inline CScenarioConditionSet::Result CScenarioConditionSet::CheckInternal(const sScenarioConditionData& conditionData) const
{
	for(s32 i = 0; i < m_Conditions.GetCount(); i++)
	{
#if __ASSERT
		const char* scenarioName = conditionData.m_ScenarioName;
		if( aiVerifyf(m_Conditions[i], "CScenarioConditionSet null condition %d, scenario type = %s", i, scenarioName)  )
#else
		if( m_Conditions[i] )
#endif // __ASSERT
		{
			if (!m_Conditions[i]->Check(conditionData))
			{
				return SCR_False;
			}
		}
	}

	return SCR_True;
}

////////////////////////////////////////////////////////////////////////////////
// CScenarioConditionSetOr
////////////////////////////////////////////////////////////////////////////////
class CScenarioConditionSetOr : public CScenarioConditionSet
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionSetOr, CScenarioConditionSet);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

inline CScenarioConditionSetOr::Result CScenarioConditionSetOr::CheckInternal(const sScenarioConditionData& conditionData) const
{
	for(s32 i = 0; i < m_Conditions.GetCount(); i++)
	{
#if __ASSERT
		const char* scenarioName = conditionData.m_ScenarioName;
		if( aiVerifyf(m_Conditions[i], "CScenarioConditionSetOr null condition %d, scenario type = %s", i, scenarioName)  )
#else
		if( m_Conditions[i] )
#endif // __ASSERT
		{
			if (m_Conditions[i]->Check(conditionData))
			{
				return SCR_True;
			}
		}
	}

	return SCR_False;
}


////////////////////////////////////////////////////////////////////////////////
// CScenarioConditionWorld
////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionWorld : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionWorld, CScenarioCondition);
public:
	virtual bool	 IsWeatherCondition() const { return false; }
protected:
	virtual Result CheckInternal(const sScenarioConditionData& UNUSED_PARAM(conditionData)) const { return SCR_False; }
private:
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CScenarioConditionPopulation
////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionPopulation : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionPopulation, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& UNUSED_PARAM(conditionData)) const { return SCR_False; }
	CPedModelInfo* GetPedModelInfo(const sScenarioConditionData& conditionData) const;
private:
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CScenarioConditionWorldSet
////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionWorldSet : public CScenarioConditionWorld
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionWorldSet, CScenarioConditionWorld);

	virtual bool IsWeatherCondition() const;
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	typedef atArray<CScenarioConditionWorld*> Conditions;
	Conditions m_Conditions;
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

inline CScenarioConditionWorldSet::Result CScenarioConditionWorldSet::CheckInternal(const sScenarioConditionData& conditionData) const
{
	CScenarioConditionWorldSet::Result checkResult = SCR_True;

	for(s32 i = 0; i < m_Conditions.GetCount(); i++)
	{
#if __ASSERT
		const char* scenarioName = conditionData.m_ScenarioName;
		if( aiVerifyf(m_Conditions[i], "CScenarioConditionWorldSet null condition %d, scenario type = %s", i, scenarioName)  )
#else
		if( m_Conditions[i] )
#endif // __ASSERT
		{
			if (!m_Conditions[i]->Check(conditionData))
			{
				checkResult = SCR_False;
				// If CheckUntilFailedWeatherCondition is true, we need to check every condition until a weather condition fails.
				// Otherwise we're free to return immediately.
				if (!conditionData.m_bCheckUntilFailedWeatherCondition || conditionData.GetFailedWeatherCondition())
				{
					return checkResult;
				}
			}
		}
	}

	return checkResult;
}

inline bool	CScenarioConditionWorldSet::IsWeatherCondition() const
{
	for(s32 i = 0; i < m_Conditions.GetCount(); i++)
	{
		if(m_Conditions[i]->IsWeatherCondition())
		{
			return true;
		}
	}

	return false;
}


////////////////////////////////////////////////////////////////////////////////
// Conditions
////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionPlayingAnim : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionPlayingAnim, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	atHashWithStringNotFinal m_ClipSet;
	atHashWithStringNotFinal m_Anim;
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionModel : public CScenarioConditionPopulation
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionModel, CScenarioConditionPopulation);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	CAmbientPedModelSet* m_ModelSet;
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionSpeed : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionSpeed, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	enum Speed
	{
		Stand, Walk, Run, Sprint,
	};
	Speed m_Speed;
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionCrouched : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionCrouched, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionInCover : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionInCover, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionHealth : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionHealth, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	f32 m_Health;
	Comparison m_Comparison;
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionEquippedWeapon : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionEquippedWeapon, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	atHashWithStringNotFinal m_Weapon;
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionWet : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionWet, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionOutOfBreath : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionOutOfBreath, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionJustGotUp : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionJustGotUp, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionAlert : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionAlert, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionIsPlayer : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionIsPlayer, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionIsPlayerInMultiplayerGame : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionIsPlayerInMultiplayerGame, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionIsMale : public CScenarioConditionPopulation
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionIsMale, CScenarioConditionPopulation);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionIsPlayerTired : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionIsPlayerTired, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionDistanceToPlayer : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionDistanceToPlayer, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	f32 m_Range;
	Comparison m_Comparison;
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionRaining : public CScenarioConditionWorld
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionRaining, CScenarioConditionWorld);
public:
	virtual bool	 IsWeatherCondition() const { return true; }
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionSunny : public CScenarioConditionWorld
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionSunny, CScenarioConditionWorld);
public:
	virtual bool	 IsWeatherCondition() const { return true; }
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionSnowing : public CScenarioConditionWorld
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionSnowing, CScenarioConditionWorld);
public:
	virtual bool	 IsWeatherCondition() const { return true; }
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionWindy : public CScenarioConditionWorld
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionWindy, CScenarioConditionWorld);
public:
	virtual bool	 IsWeatherCondition() const { return true; }
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionTime : public CScenarioConditionWorld
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionTime, CScenarioConditionWorld);
public:
	CScenarioConditionTime();
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
#if __BANK
	virtual void PreAddWidgets(bkBank& bank);
#endif // __BANK
private:
#if __BANK
	void PresetChangedCB();
#endif // __BANK

	enum Time
	{
		Hour_0	= BIT0,
		Hour_1	= BIT1,
		Hour_2	= BIT2,
		Hour_3	= BIT3,
		Hour_4	= BIT4,
		Hour_5	= BIT5,
		Hour_6	= BIT6,
		Hour_7	= BIT7,
		Hour_8	= BIT8,
		Hour_9	= BIT9,
		Hour_10	= BIT10,
		Hour_11	= BIT11,
		Hour_12	= BIT12,
		Hour_13	= BIT13,
		Hour_14	= BIT14,
		Hour_15	= BIT15,
		Hour_16	= BIT16,
		Hour_17	= BIT17,
		Hour_18	= BIT18,
		Hour_19	= BIT19,
		Hour_20	= BIT20,
		Hour_21	= BIT21,
		Hour_22	= BIT22,
		Hour_23	= BIT23,
	};
	s32 m_Hours;

#if __BANK
	enum TimePresets
	{
		Unset,
		Morning,
		Afternoon,
		Evening,
		Night,

		TP_Max,
	};
	// Rag combo index
	s32 m_iComboIndex;
#endif // __BANK

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionInInterior : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionInInterior, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionHasProp : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionHasProp, CScenarioCondition);
public:
	static strLocalIndex GetPropModelIndex(const CPed* pPed);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	atHashWithStringNotFinal m_PropSet;
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionHasNoProp : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionHasNoProp, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionIsPanicking : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionIsPanicking, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CVehicleConditionEntryPointHasOpenableDoor : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CVehicleConditionEntryPointHasOpenableDoor, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CVehicleConditionRoofState : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CVehicleConditionRoofState, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;

private:

	enum RoofState
	{
		NoRoof				= BIT0, 
		HasRoof				= BIT1,
		RoofIsDown			= BIT2, 
		RoofIsUp			= BIT3
	};

	s32 m_RoofState;

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionArePedConfigFlagsSet : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionArePedConfigFlagsSet, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:

	ePedConfigFlagsBitSet m_ConfigFlags;

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionArePedConfigFlagsSetOnOtherPed : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionArePedConfigFlagsSetOnOtherPed, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:

	ePedConfigFlagsBitSet m_ConfigFlags;

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionIsMultiplayerGame : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionIsMultiplayerGame, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionOnStraightPath : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionOnStraightPath, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;

	float	m_MinDist;

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionHasParachute : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionHasParachute, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

#if ENABLE_JETPACK

class CScenarioConditionUsingJetpack : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionUsingJetpack, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	PAR_PARSABLE;
};

#endif

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionIsSwat : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionIsSwat, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////


class CScenarioConditionAffluence : public CScenarioConditionPopulation
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionAffluence, CScenarioConditionPopulation);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	Affluence m_Affluence;
	PAR_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionTechSavvy : public CScenarioConditionPopulation
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionTechSavvy, CScenarioConditionPopulation);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	TechSavvy m_TechSavvy;
	PAR_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionAmbientEventTypeCheck : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionAmbientEventTypeCheck, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	AmbientEventType m_Type;
	PAR_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionAmbientEventDirection : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionAmbientEventDirection, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	Vector3	m_Direction;
	float	m_Threshold;
	bool	m_FlattenZ;
	PAR_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionRoleInSyncedScene : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionRoleInSyncedScene, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;

	RoleInSyncedScene	m_Role;

	PAR_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionInVehicleOfType : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionInVehicleOfType, CScenarioCondition);

protected:

	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;

private:

	CAmbientModelSet* m_VehicleModelSet;

	PAR_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionIsRadioPlaying : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionIsRadioPlaying, CScenarioCondition);

protected:

	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;

	PAR_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionIsRadioPlayingMusic : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionIsRadioPlayingMusic, CScenarioCondition);

protected:

	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;

	PAR_PARSABLE;
};

///////////////////////////////////////////////////////////////////////////////

class CScenarioConditionInVehicleSeat : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionInVehicleSeat, CScenarioCondition);

protected:

	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;

private:

	s8 m_SeatIndex;

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionAttachedToPropOfType : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionAttachedToPropOfType, CScenarioCondition);

protected:

	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;

private:

	CAmbientModelSet* m_PropModelSet;

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionFullyInIdle : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionFullyInIdle, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionPedHeading : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionPedHeading, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	float m_TurnAngleDegrees;
	float m_ThresholdDegrees;
	PAR_PARSABLE;
};

///////////////////////////////////////////////////////////////////////////////

class CScenarioConditionInStationaryVehicleScenario : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionInStationaryVehicleScenario, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	float	m_MinimumWaitTime;
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionPhoneConversationAvailable : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionPhoneConversationAvailable, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioPhoneConversationInProgress : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioPhoneConversationInProgress, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
public:
	static Result CheckPhoneConversationInProgress(const sScenarioConditionData& conditionData);
private:
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionCanStartNewPhoneConversation : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionCanStartNewPhoneConversation, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionPhoneConversationStarting : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionPhoneConversationStarting, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionMovementModeType : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionMovementModeType, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	MovementModeType m_MovementModeType;
	atHashWithStringNotFinal m_MovementModeIdle;
	bool m_HighEnergy;
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionHasComponentWithFlag : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionHasComponentWithFlag, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	u32 m_PedComponentFlag;		// Really something from ePedCompFlags.
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionPlayerHasSpaceForIdle : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionPlayerHasSpaceForIdle, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionCanPlayInCarIdle : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionCanPlayInCarIdle, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionBraveryFlagSet : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionBraveryFlagSet, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	eBraveryFlags m_Flag;
	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionOnFootClipSet : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionOnFootClipSet, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:

	atHashWithStringNotFinal m_ClipSet;
	PAR_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionWorldPosWithinSphere : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionWorldPosWithinSphere, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
private:
	spdSphere m_Sphere;
	PAR_PARSABLE;
};

/////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionHasHighHeels : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionHasHighHeels, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
	PAR_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionIsReaction : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionIsReaction, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
	PAR_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionIsHeadbobbingToRadioMusicEnabled : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionIsHeadbobbingToRadioMusicEnabled, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
	PAR_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionHeadbobMusicGenre : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionHeadbobMusicGenre, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
	RadioGenrePar m_RadioGenre;
	PAR_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionIsTwoHandedWeaponEquipped : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionIsTwoHandedWeaponEquipped, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
	PAR_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////////////

class CScenarioConditionUsingLRAltClipset : public CScenarioCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CScenarioConditionUsingLRAltClipset, CScenarioCondition);
protected:
	virtual Result CheckInternal(const sScenarioConditionData& conditionData) const;
	PAR_PARSABLE;
};

#endif // INC_SCENARIO_INFO_CONDITIONS_H
