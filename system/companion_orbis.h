//--------------------------------------------------------------------------------------
// companion_orbis.h
//--------------------------------------------------------------------------------------
#if RSG_ORBIS

#pragma once

#include "companion.h"
#include "atl/vector.h"

#if COMPANION_APP

#include <libsysmodule.h>
#include <companion_httpd.h>

#pragma comment(lib,"libSceCompanionHttpd_stub_weak.a")

//--------------------------------------------------------------------------------------
// Companion Data class for Orbis
//--------------------------------------------------------------------------------------
class CCompanionDataOrbis : public CCompanionData
{
public:
	CCompanionDataOrbis();
	~CCompanionDataOrbis();

	void Initialise();
	void Update();

	void OnClientConnected();
	void OnClientDisconnected();

	u32 GetFirstClient()
	{
		if (m_clientsList.size() > 0)
		{
			return m_clientsList[0];
		}

		return 0;
	}

	//	Orbis companion callback handler
	static int32_t CompanionHttpdCallback(SceUserServiceUserId userId, const SceCompanionHttpdRequest* httpRequest, SceCompanionHttpdResponse* httpResponse, void* params);

private:

	atVector<u32> m_clientsList;
protected:
};

#endif	//	COMPANION_APP
#endif	//	RSG_ORBIS