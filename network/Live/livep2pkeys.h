//
// livep2pkeys.h
//
// Copyright (C) 1999-2018 Rockstar Games.  All Rights Reserved.
//

#ifndef GTA5_LIVE_P2P_KEYS_H
#define GTA5_LIVE_P2P_KEYS_H

#include "net/net.h"
#include "net/crypto.h"

class CLiveP2pKeys
{
public:
	static bool GetP2pKeySalt(netP2pCrypt::HmacKeySalt& keySalt);

private:
#if RSG_ORBIS
	static const netP2pCrypt::HmacKeySalt sm_P2pKeySaltPs4;
#endif

#if RSG_DURANGO
	static const netP2pCrypt::HmacKeySalt sm_P2pKeySaltXb1;
#endif

#if RSG_PC
	static const netP2pCrypt::HmacKeySalt sm_P2pKeySaltPc;
#endif

#if NET_CROSS_PLATFORM_MP
	static const netP2pCrypt::HmacKeySalt sm_P2pKeySaltCrossPlatform;
#endif
};
#endif
