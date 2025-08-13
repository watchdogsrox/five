//
// task/Combat/Cover/coverfilter.h
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef TASK_COMBAT_COVER_COVERFILTER_H
#define TASK_COMBAT_COVER_COVERFILTER_H

#include "data/base.h"
#include "vectormath/classes.h"

class CCoverPoint;
class CDefensiveArea;
class CPed;
class CTaskCombat;

//-----------------------------------------------------------------------------

/*
PURPOSE
	Base class for cover point filters, i.e. a class that has the chance to
	reject cover points based on various factors. Unlike a loose callback function,
	the filter can also contain data that remains constant while evaluating
	a set of cover points.
*/
class CCoverPointFilter : public datBase
{
public:
	// PURPOSE:	Check if a given cover point is to be considered valid.
	// PARAMS:	coverPoint			- The cover point to evaluate.
	//			coverPointCoords	- Coordinates of the cover point.
	// RETURNS:	True if the cover point is considered valid.
	virtual bool CheckValidity(const CCoverPoint& coverPoint, Vec3V_ConstRef coverPointCoords) const = 0;
};

//-----------------------------------------------------------------------------

/*
PURPOSE
	A helper class for constructing an object derived from CCoverPointFilter
	on the stack.
*/
class CCoverPointFilterInstance
{
public:
	typedef bool (CoverPointFilterConstructFn)(CCoverPointFilter* pFilterOut, int numBytesAvailable, void* pData);

	// PURPOSE:	Max allowed size of any subclass of CCoverPointFilter for them to
	//			be able to use this helper class. Increasing this is only expected to increase
	//			temporary stack space usage, so should be safe to bump up if needed.
	static const int kMaxSize = 128;

	CCoverPointFilter* Construct(CoverPointFilterConstructFn constructFn, void* pData)
	{
		CompileTimeAssert(sizeof(Vec4V) == 16);
		CompileTimeAssert((kMaxSize & 15) == 0);

		CCoverPointFilter* pFilter = reinterpret_cast<CCoverPointFilter*>(&m_Space);
		if(constructFn == NULL || !constructFn(pFilter, sizeof(m_Space), pData))
		{
			pFilter = NULL;
		}
		return pFilter;
	}

protected:
	Vec4V	m_Space[kMaxSize/16];
};

//-----------------------------------------------------------------------------

/*
PURPOSE
	An extra level of cover point filter base class to store some commonly used
	variables and functions
*/
class CCoverPointFilterBase : public CCoverPointFilter
{
public:

	explicit CCoverPointFilterBase(CPed& ped);

	virtual bool CheckValidity(const CCoverPoint& coverPoint, Vec3V_ConstRef coverPointCoords) const;

	static bool Construct(CCoverPointFilter* pFilterOut, int numBytesAvailable, void* pData);

protected:

	bool IsPointTooCloseToTarget(Vec3V_ConstRef vCoverPointCoords) const;

	CDefensiveArea*	m_pDefensiveArea;
	CPed*			m_pPed;
	Vec3V			m_PrimaryTargetPos;
	ScalarV			m_scMinDistanceToTargetSq;
	bool			m_bMaintainMinDistanceToTarget;
	bool			m_bIsTargetInDefensiveArea;
	bool			m_bCanCheckAttackWindows;
};

//-----------------------------------------------------------------------------

/*
PURPOSE
	Standard cover point filter used by CTaskCombat and a few other classes.
NOTES
	This logic used to be in CTaskCombat::CheckCoverPointForValidity().
*/
class CCoverPointFilterTaskCombat : public CCoverPointFilterBase
{
public:
	explicit CCoverPointFilterTaskCombat(CPed& ped);

	virtual bool CheckValidity(const CCoverPoint& coverPoint, Vec3V_ConstRef vCoverPointCoords) const;

	static bool Construct(CCoverPointFilter* pFilterOut, int numBytesAvailable, void* pData);

protected:
	CTaskCombat*	m_pCombatTask;
	Vec3V			m_PedPos;

	float			m_AttackWindowMaxSquared;
	float			m_AttackWindowMinSquared;
	float			m_MinDistanceForAltCoverSquared;
};

//-----------------------------------------------------------------------------

/*
PURPOSE
	Cover point filter used that rejects points outside of a ped's defensive area (if any).
*/
class CCoverPointFilterDefensiveArea : public CCoverPointFilterBase
{
public:
	explicit CCoverPointFilterDefensiveArea(CPed& ped);

	virtual bool CheckValidity(const CCoverPoint& coverPoint, Vec3V_ConstRef vCoverPointCoords) const;

	static bool Construct(CCoverPointFilter* pFilterOut, int numBytesAvailable, void* pData);

protected:
};

//-----------------------------------------------------------------------------

#endif	// TASK_COMBAT_COVER_COVERFILTER_H

// End of file 'task/Combat/Cover/coverfilter.h'
