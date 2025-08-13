//
// name:		NetworkFXId.cpp
// description:	Network FX id - a unique id used to identify explosions, fire and projectiles across the network
// written by:	John Gurney
//

#include "network/General/NetworkFXId.h"
#include "network/NetworkInterface.h"
#include "network/Players/NetworkPlayerMgr.h"

NETWORK_OPTIMISATIONS()

NetworkFXId	CNetFXIdentifier::ms_lastFXId = INVALID_NETWORK_FX_ID;

bool CNetFXIdentifier::IsClone() const
{
	if (IsValid() && NetworkInterface::IsGameInProgress())
	{
		return  m_playerOwner != NetworkInterface::GetLocalPhysicalPlayerIndex();
	}

	return false;
}

NetworkFXId CNetFXIdentifier::GetNextFXId()
{
	ms_lastFXId++;

	if (ms_lastFXId == INVALID_NETWORK_FX_ID)
	{
		ms_lastFXId++;
	}

	return ms_lastFXId;
}

