#ifndef _PAVEMENT_FLOODFILL_HELPER_H
#define _PAVEMENT_FLOODFILL_HELPER_H

#include "Pathserver/PathServer.h"

class CPed;

class CPavementFloodFillHelper
{
public:
	enum ePavementStatus {
		SEARCH_NOT_STARTED,
		SEARCH_NOT_FINISHED,
		HAS_PAVEMENT,
		NO_PAVEMENT,
		MAX_NUM_PAVEMENT_STATUS
	};

	CPavementFloodFillHelper();
	~CPavementFloodFillHelper();

	void StartPavementFloodFillRequest(CPed* pPed);
	void StartPavementFloodFillRequest(CPed* pPed, const Vector3& position);
	void UpdatePavementFloodFillRequest(CPed* pPed);
	void UpdatePavementFloodFillRequest(CPed* pPed, const Vector3& position);
	void CleanupPavementFloodFillRequest();

	bool IsSearchNotStarted() const { return m_ePavementStatus == SEARCH_NOT_STARTED; }
	bool IsSearchActive() const			{ return m_ePavementStatus == SEARCH_NOT_FINISHED; }
	bool HasPavement() const				{ return m_ePavementStatus == HAS_PAVEMENT; }
	bool HasNoPavement() const			{ return m_ePavementStatus == NO_PAVEMENT;  }

	void SetSearchRadius(float fRadius) { m_fOverrideRadius = fRadius; }

	void ResetSearchResults() { aiAssert(m_ePavementStatus != SEARCH_NOT_FINISHED); m_ePavementStatus = SEARCH_NOT_STARTED; }
private:
	float GetSearchRadius() const;

	TPathHandle m_hPathHandle;
	ePavementStatus m_ePavementStatus;
	float m_fOverrideRadius;

	static const float ms_fDefaultPavementSearchRadius;
};

#endif // _PAVEMENT_FLOODFILL_HELPER_H