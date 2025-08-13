#ifndef COMBAT_SITUATION_H
#define COMBAT_SITUATION_H

// Rage headers
#include "fwtl/Pool.h"

// Forward declarations
class CCombatDirector;
class CCombatOrders;
class CPed;
class CTaskCombat;

//////////////////////////////////////////////////////////////////////////
// CCombatSituation
//////////////////////////////////////////////////////////////////////////

class CCombatSituation
{

private:

	CCombatSituation(const CCombatSituation& other);
	CCombatSituation& operator=(const CCombatSituation& other);

public:

	FW_REGISTER_CLASS_POOL(CCombatSituation);
	
	CCombatSituation();
	virtual ~CCombatSituation() = 0;
	
	virtual void	Initialize(CCombatDirector& rDirector);
	virtual void	OnActivate(CCombatDirector& rDirector, CPed& rPed);
	virtual void	OnAdd(CCombatDirector& rDirector, CPed& rPed);
	virtual void	OnDeactivate(CCombatDirector& rDirector, CPed& rPed);
	virtual void	OnRemove(CCombatDirector& rDirector, CPed& rPed);
	virtual void	Uninitialize(CCombatDirector& rDirector);
	virtual void	Update(CCombatDirector& rDirector, const float fTimeStep);
	
#if __BANK
	virtual const char*	GetName() const;
#endif

protected:

	bool			CheckShouldUpdate();
	const CPed*		FindPlayerTarget(const CCombatDirector& rDirector) const;
	bool			GetAttackWindowAndDistanceFromTarget(const CPed& rPed, float& fAttackWindowMin, float& fAttackWindowMax, float& fDistanceFromTarget) const;
	CTaskCombat*	GetCombatTask(const CPed& rPed) const;

protected:

	virtual void Initialize(CCombatDirector& rDirector, CPed& rPed);
	virtual void Initialize(CCombatDirector& rDirector, CPed& rPed, CCombatOrders& rCombatOrders);
	virtual void Uninitialize(CCombatDirector& rDirector, CPed& rPed);
	virtual void Uninitialize(CCombatDirector& rDirector, CPed& rPed, CCombatOrders& rCombatOrders);
	
protected:

	float	m_fTotalTime;
	
private:

	float	m_fTimeSinceLastUpdate;
	float	m_fTimeBetweenUpdates;
		
};

class CCombatSituationNormal : public CCombatSituation
{

public:

	CCombatSituationNormal();
	virtual ~CCombatSituationNormal();

	virtual void	Update(CCombatDirector& rDirector, const float fTimeStep);
	
#if __BANK
	virtual const char*	GetName() const;
#endif

private:

	void	LimitCover(CCombatDirector& rDirector);

};

class CCombatSituationFallBack : public CCombatSituation
{

public:

	CCombatSituationFallBack();
	virtual ~CCombatSituationFallBack();

	virtual void	Update(CCombatDirector& rDirector, const float fTimeStep);
	
	static bool	IsValid(const CCombatDirector& rDirector);

#if __BANK
	virtual const char*	GetName() const;
#endif

private:

	static bool DoesTargetHaveScaryWeapon(const CCombatDirector& rDirector);
	static bool HaveLotsOfPedsDiedRecently(const CCombatDirector& rDirector);

private:

	virtual void Initialize(CCombatDirector& rDirector);
	virtual void Initialize(CCombatDirector& rDirector, CPed& rPed, CCombatOrders& rCombatOrders);

};

class CCombatSituationLull : public CCombatSituation
{

public:

	CCombatSituationLull();
	virtual ~CCombatSituationLull();

	virtual void	Update(CCombatDirector& rDirector, const float fTimeStep);

	static bool	IsValid(const CCombatDirector& rDirector);

#if __BANK
	virtual const char*	GetName() const;
#endif

private:

	virtual void	Initialize(CCombatDirector& rDirector, CPed& rPed, CCombatOrders& rCombatOrders);

};

class CCombatSituationEscalate : public CCombatSituation
{

public:

	CCombatSituationEscalate();
	virtual ~CCombatSituationEscalate();

	virtual void	Update(CCombatDirector& rDirector, const float fTimeStep);

	static bool IsValid(const CCombatDirector& rDirector);

#if __BANK
	virtual const char*	GetName() const;
#endif

private:

	virtual void	Initialize(CCombatDirector& rDirector, CPed& rPed, CCombatOrders& rCombatOrders);

};

#endif // COMBAT_SITUATION_H
