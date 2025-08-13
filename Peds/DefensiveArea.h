#ifndef DEFENSIVE_AREA_H
#define DEFENSIVE_AREA_H

// Game includes
#include "Peds/Area.h"

//-------------------------------------------------------------------------
// Defines a defensive area used during combat
//-------------------------------------------------------------------------
class CDefensiveArea : public CArea
{

public:

	CDefensiveArea();
	virtual ~CDefensiveArea();

	bool IsPointInDefensiveArea( const Vector3& vPoint, const bool bVisualiseArea = false, const bool bIsDead = false, const bool bEditing = false ) const { return IsPointInArea(vPoint, bVisualiseArea, bIsDead, bEditing); }
	
	// Gets/Sets the default cover direction, direction to be in cover from when no target is near
	Vector3 GetDefaultCoverFromPosition() const { return m_vDefaultCoverFromPosition; }
	void SetDefaultCoverFromPosition(const Vector3& val) { m_vDefaultCoverFromPosition = val; }

	bool GetUseCenterAsGoToPosition() const { return m_bUseCenterAsGoToPosition; }
	void SetUseCenterAsGoToPosition(bool bUseCenter) { m_bUseCenterAsGoToPosition = bUseCenter; }

public:

	static float GetMinRadius() { return sm_fMinRadius; }
	
private:

	virtual void OnReset();
	virtual void OnSetSphere();
	
private:

	Vector3	m_vDefaultCoverFromPosition;
	bool m_bUseCenterAsGoToPosition;

private:

	static float sm_fMinRadius;
	
};

//-------------------------------------------------------------------------
// Manages the defensive areas of a ped
//-------------------------------------------------------------------------
class CDefensiveAreaManager
{
public:

	CDefensiveAreaManager();
	~CDefensiveAreaManager();

	void UpdateDefensiveAreas();

	void SetCurrentDefensiveArea(CDefensiveArea* pDefensiveArea);
	void SwapDefensiveAreaForCoverSearch();
	CDefensiveArea* GetCurrentDefensiveArea() { return m_pCurrentDefensiveArea; }
	CDefensiveArea* GetPrimaryDefensiveArea() { return &m_primaryDefensiveArea; }
	CDefensiveArea* GetSecondaryDefensiveArea(bool bCreateIfInactive = false);
	CDefensiveArea* GetAlternateDefensiveArea();
	CDefensiveArea* GetDefensiveAreaForCoverSearch();

	void ClearPrimaryDefensiveArea();

private:

	// Defines an area the ped needs to defend
	CDefensiveArea m_primaryDefensiveArea;

	// Secondary area to fall back to if there is no cover in the first
	CDefensiveArea* m_pSecondaryDefensiveArea;

	// This is the current cover area (should always point to the primary or secondary)
	CDefensiveArea* m_pCurrentDefensiveArea;

	// This bool tracks if we should be using our secondary defensive area for cover queries
	bool m_bUseSecondaryAreaForCover;
};

#endif //DEFENSIVE_AREA_H
