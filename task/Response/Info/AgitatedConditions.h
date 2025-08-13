#ifndef INC_AGITATED_INFO_CONDITIONS_H
#define INC_AGITATED_INFO_CONDITIONS_H

// Rage headers
#include "atl/array.h"
#include "data/base.h"
#include "fwutil/rtti.h"
#include "parser/macros.h"

// Game headers
#include "task/Response/Info/AgitatedEnums.h"

// Forward declarations
class CAmbientPedModelSet;
class CPed;
class CTaskAgitated;

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionContext
////////////////////////////////////////////////////////////////////////////////

struct CAgitatedConditionContext
{
	const CTaskAgitated* m_Task;
};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedCondition
////////////////////////////////////////////////////////////////////////////////

class CAgitatedCondition : public datBase
{

	DECLARE_RTTI_BASE_CLASS(CAgitatedCondition);
	
public:

	virtual ~CAgitatedCondition() {}
	
	virtual bool Check(const CAgitatedConditionContext& aContext) const = 0;
	
private:

	PAR_PARSABLE;
	
};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionMulti
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionMulti : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionMulti, CAgitatedCondition);

public:

	CAgitatedConditionMulti();
	virtual ~CAgitatedConditionMulti();
	
protected:

	atArray<CAgitatedCondition *> m_Conditions;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionAnd
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionAnd : public CAgitatedConditionMulti
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionAnd, CAgitatedConditionMulti);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionOr
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionOr : public CAgitatedConditionMulti
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionOr, CAgitatedConditionMulti);
	
protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionNot
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionNot : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionNot, CAgitatedCondition);
	
public:

	CAgitatedConditionNot();
	virtual ~CAgitatedConditionNot();

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	CAgitatedCondition* m_Condition;

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionTimeout
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionTimeout : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionTimeout, CAgitatedCondition);
		
protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;
	
private:
	
	float m_Time;
	float m_MinTime;
	float m_MaxTime;

	PAR_PARSABLE;
	
};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionCanCallPolice
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionCanCallPolice : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionCanCallPolice, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionCanFight
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionCanFight : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionCanFight, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionCanHurryAway
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionCanHurryAway : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionCanHurryAway, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionCanStepOutOfVehicle
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionCanStepOutOfVehicle : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionCanStepOutOfVehicle, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionCanWalkAway
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionCanWalkAway : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionCanWalkAway, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionCheckBraveryFlags
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionCheckBraveryFlags : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionCheckBraveryFlags, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	u32 m_Flags;

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionHasBeenHostileFor
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionHasBeenHostileFor : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionHasBeenHostileFor, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	float m_MinTime;
	float m_MaxTime;

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionHasContext
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionHasContext : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionHasContext, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	u32 m_Context;

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionHasFriendsNearby
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionHasFriendsNearby : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionHasFriendsNearby, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	u8 m_Min;

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionHasLeader
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionHasLeader : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionHasLeader, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionHasLeaderBeenFightingFor
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionHasLeaderBeenFightingFor : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionHasLeaderBeenFightingFor, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	float m_MinTime;
	float m_MaxTime;

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionHasVehicle
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionHasVehicle : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionHasVehicle, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsAGunPulled
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsAGunPulled : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsAGunPulled, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsAgitatorArmed
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsAgitatorArmed : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsAgitatorArmed, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsAgitatorEnteringVehicle
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsAgitatorEnteringVehicle : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsAgitatorEnteringVehicle, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsAgitatorInOurTerritory
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsAgitatorInOurTerritory : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsAgitatorInOurTerritory, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsAgitatorInVehicle
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsAgitatorInVehicle : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsAgitatorInVehicle, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsAgitatorInjured
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsAgitatorInjured : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsAgitatorInjured, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsAgitatorMovingAway
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsAgitatorMovingAway : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsAgitatorMovingAway, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	float m_Dot;

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsAngry
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsAngry : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsAngry, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	float m_Threshold;

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsArgumentative
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsArgumentative : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsArgumentative, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsAvoiding
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsAvoiding : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsAvoiding, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsBecomingArmed
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsBecomingArmed : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsBecomingArmed, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsBumped
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsBumped : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsBumped, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsBumpedByVehicle
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsBumpedByVehicle : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsBumpedByVehicle, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsBumpedInVehicle
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsBumpedInVehicle : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsBumpedInVehicle, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsConfrontational
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsConfrontational : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsConfrontational, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsConfronting
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsConfronting : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsConfronting, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsContext
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsContext : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsContext, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	atHashWithStringNotFinal m_Context;

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsDodged
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsDodged : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsDodged, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsDodgedVehicle
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsDodgedVehicle : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsDodgedVehicle, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsDrivingVehicle
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsDrivingVehicle : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsDrivingVehicle, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};
////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsExitingScenario
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsExitingScenario : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsExitingScenario, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};


////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsFacing
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsFacing : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsFacing, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsFearful
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsFearful : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsFearful, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	float m_Threshold;

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsFighting
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsFighting : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsFighting, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsFollowing
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsFollowing : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsFollowing, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsFleeing
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsFleeing : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsFleeing, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsFlippingOff
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsFlippingOff : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsFlippingOff, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsFriendlyTalking
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsFriendlyTalking : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsFriendlyTalking, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsGettingUp
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsGettingUp : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsGettingUp, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsGriefing
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsGriefing : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsGriefing, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsGunAimedAt
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsGunAimedAt : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsGunAimedAt, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsHarassed
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsHarassed : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsHarassed, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsHash
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsHash : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsHash, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	atHashWithStringNotFinal m_Value;

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsHostile
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsHostile : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsHostile, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsHurryingAway
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsHurryingAway : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsHurryingAway, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsInjured
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsInjured : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsInjured, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsInsulted
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsInsulted : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsInsulted, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsIntervene
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsIntervene : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsIntervene, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsIntimidate
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsIntimidate : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsIntimidate, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsInVehicle
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsInVehicle : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsInVehicle, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsLastAgitationApplied
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsLastAgitationApplied : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsLastAgitationApplied, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	AgitatedType m_Type;

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsLawEnforcement
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsLawEnforcement : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsLawEnforcement, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsLeaderAgitated
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsLeaderAgitated : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsLeaderAgitated, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsLeaderFighting
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsLeaderFighting : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsLeaderFighting, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsLeaderInState
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsLeaderInState : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsLeaderInState, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	atHashWithStringNotFinal m_State;

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsLeaderStill
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsLeaderStill : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsLeaderStill, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsLeaderTalking
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsLeaderTalking : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsLeaderTalking, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsLeaderUsingResponse
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsLeaderUsingResponse : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsLeaderUsingResponse, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	atHashWithStringNotFinal m_Name;

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsLoitering
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsLoitering : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsLoitering, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsMale
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsMale : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsMale, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsOutsideClosestDistance
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsOutsideClosestDistance : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsOutsideClosestDistance, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	float m_Distance;

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsOutsideDistance
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsOutsideDistance : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsOutsideDistance, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	float m_Distance;

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsProvoked
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsProvoked : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsProvoked, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	bool m_IgnoreHostility;

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsRanting
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsRanting : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsRanting, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsSirenOn
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsSirenOn : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsSirenOn, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

///////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsStanding
///////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsStanding : public CAgitatedCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsStanding, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsTalking
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsTalking : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsTalking, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsTargetDoingAMeleeMove
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsTargetDoingAMeleeMove : public CAgitatedCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsTargetDoingAMeleeMove, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsWandering
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsWandering : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsWandering, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsUsingScenario
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsUsingScenario : public CAgitatedCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsUsingScenario, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsTerritoryIntruded
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsTerritoryIntruded : public CAgitatedCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsTerritoryIntruded, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIntruderLeft
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIntruderLeft : public CAgitatedCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIntruderLeft, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;
};

///////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsUsingTerritoryScenario
///////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsUsingTerritoryScenario : public CAgitatedCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsUsingTerritoryScenario, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;
};

///////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsPlayingAmbientsInScenario
///////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsPlayingAmbientsInScenario : public CAgitatedCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsPlayingAmbientsInScenario, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;
};

///////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsReadyForScenarioResponse
///////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsReadyForScenarioResponse : public CAgitatedCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsReadyForScenarioResponse, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;
};

///////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsUsingRagdoll
///////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsUsingRagdoll : public CAgitatedCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsUsingRagdoll, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;
};

///////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionRandom
///////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionRandom : public CAgitatedCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionRandom, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	float m_Chances;

	PAR_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionWasLeaderHit
////////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionWasLeaderHit : public CAgitatedCondition
{

	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionWasLeaderHit, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;

};

///////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionWasRecentlyBumpedWhenStill
///////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionWasRecentlyBumpedWhenStill : public CAgitatedCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionWasRecentlyBumpedWhenStill, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	float m_MaxTime;

	PAR_PARSABLE;
};

///////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionWasUsingTerritorialScenario
///////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionWasUsingTerritorialScenario : public CAgitatedCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionWasUsingTerritorialScenario, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;
};

///////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionHasPavement
///////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionHasPavement : public CAgitatedCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionHasPavement, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;
};

///////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsCallingPolice
///////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsCallingPolice : public CAgitatedCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsCallingPolice, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;
};

///////////////////////////////////////////////////////////////////////////////
// CAgitatedConditionIsSwimming
///////////////////////////////////////////////////////////////////////////////

class CAgitatedConditionIsSwimming : public CAgitatedCondition
{
	DECLARE_RTTI_DERIVED_CLASS(CAgitatedConditionIsSwimming, CAgitatedCondition);

protected:

	virtual bool Check(const CAgitatedConditionContext& aContext) const;

private:

	PAR_PARSABLE;
};

#endif // INC_AGITATED_INFO_CONDITIONS_H
