#ifndef __UGC_QUERY_HELPER_H__
#define __UGC_QUERY_HELPER_H__

// game includes
#include "network/Cloud/UserContentManager.h"

namespace rage
{
	class bkBank;
	class rlCloudMemberId;
}

class UGCQueryHelper
{
public:
	UGCQueryHelper(): m_pQueryName(NULL), m_contentType(RLUGC_CONTENT_TYPE_UNKNOWN) {}
	UGCQueryHelper(rlUgcContentType type, const char* pQueryName) {Init(type, pQueryName);}
	~UGCQueryHelper() {Reset();}

	void Init(rlUgcContentType type, const char* pQueryName);
	void Reset();

	void Trigger(const char* rParams, int numParams);

	netStatus& GetNetStatus() {return m_ugcQueryStatus;}
	rlUgc::QueryContentCreatorsDelegate& GetDelegate() {return m_ugcDelegate;}

private:
	const char* m_pQueryName;
	rlUgcContentType m_contentType;

	netStatus m_ugcQueryStatus;
	rlUgc::QueryContentCreatorsDelegate m_ugcDelegate;
};

#endif
