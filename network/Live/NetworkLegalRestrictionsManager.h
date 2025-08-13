//
// NetworkLegalRestrictionsManager.h
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_NETWORK_LEGAL_RESTRICTIONS_H_
#define INC_NETWORK_LEGAL_RESTRICTIONS_H_

// Rage headers
#include "atl/hashstring.h"
#include "atl/array.h"
#include "atl/singleton.h"
#include "rline/socialclub/rlsocialclubcommon.h"
#include "net/status.h"

//PURPOSE: Download and manage network restrictions data.
class NetworkLegalRestrictionsManager
{
	static const u32 GAMBLING_RESTRICTED_GEOGRAPHIC;
	static const u32 GTAO_CASINO_PURCHASE_CHIPS;
	static const u32 GTAO_CASINO_HOUSE;
	static const u32 MAX_NUM_RESTRICTIONS;
	static const u32 MAX_NUM_RETRIALS;
	static const u32 BACKOFF_MSTIMER;

public:
	NetworkLegalRestrictionsManager();
	~NetworkLegalRestrictionsManager() { Shutdown(); }

	void Init();
	void Update();
	void Cancel();
	void Shutdown();

	bool        IsPending( ) const { return m_Status.Pending(); }
	bool          IsValid( ) const { return m_IsValid; }

	void UpdateRestrictions();

	const rlLegalTerritoryRestriction& Access(const u32 restrictionId, const u32 gameType) const;
	const rlLegalTerritoryRestriction& Gambling(const u32 gameType) const { return Access(GAMBLING_RESTRICTED_GEOGRAPHIC, gameType);}
	bool CanGamble(const u32 gameType = 0, const bool pvcAllowed = false) const;

	bool  IsAnyEvcAllowed( ) const { return CanGamble(0, false); }
	bool  IsAnyPvcAllowed( ) const { return CanGamble(0, true); }

	bool  EvcOnlyOnCasinoChipsTransactions( ) const;

private:
	bool m_IsValid;
	bool m_Update;
	u32 m_NumRestrictions;
	u32 m_NumFailedRetrials;
	u32 m_BackoffTimer;
	netStatus m_Status;
	atArray< rlLegalTerritoryRestriction > m_Restrictions;
};
typedef atSingleton< NetworkLegalRestrictionsManager > NetworkLegalRestrictionsInst;
#define NETWORK_LEGAL_RESTRICTIONS NetworkLegalRestrictionsInst::GetInstance()

#endif // INC_NETWORK_LEGAL_RESTRICTIONS_H_

//eof
