//
// name:		NetworkFXId.h
// description:	Network FX id - a unique id used to identify explosions, fire and projectiles across the network
// written by:	John Gurney
//

#ifndef NETWORK_FX_ID_H
#define NETWORK_FX_ID_H

#include "data/bitbuffer.h"
#include "fwnet/nettypes.h"
#include "fwnet/netchannel.h"

typedef u16 NetworkFXId;

#define INVALID_NETWORK_FX_ID	0

class CNetFXIdentifier
{
public:

	CNetFXIdentifier() :
    m_playerOwner(INVALID_PLAYER_INDEX),
    m_fxId(INVALID_NETWORK_FX_ID)
	{

	}

	CNetFXIdentifier(PhysicalPlayerIndex player, NetworkFXId fxId) :
	m_playerOwner(player),
	m_fxId(fxId)
	{
		gnetAssert(player != INVALID_PLAYER_INDEX);
		gnetAssert(fxId != INVALID_NETWORK_FX_ID);
	}

	CNetFXIdentifier(const CNetFXIdentifier& identifier) :
	m_playerOwner(identifier.m_playerOwner),
	m_fxId(identifier.m_fxId)
	{
	}

	CNetFXIdentifier(const CNetFXIdentifier* pIdentifier) :
	m_playerOwner(INVALID_PLAYER_INDEX),
	m_fxId(INVALID_NETWORK_FX_ID)
	{
		if (pIdentifier)
		{
			m_playerOwner = pIdentifier->m_playerOwner;
			m_fxId = pIdentifier->m_fxId;

			gnetAssert(m_playerOwner != INVALID_PLAYER_INDEX);
			gnetAssert(m_fxId != INVALID_NETWORK_FX_ID);
		}
	}

	void Reset()
	{
		m_playerOwner = INVALID_PLAYER_INDEX;
		m_fxId = INVALID_NETWORK_FX_ID;
	}

	void Set(PhysicalPlayerIndex player, NetworkFXId id)
	{
		m_playerOwner = player;
		m_fxId = id;

		gnetAssert(m_playerOwner != INVALID_PLAYER_INDEX);
		gnetAssert(m_fxId != INVALID_NETWORK_FX_ID);
	}

	void SetPlayerOwner(PhysicalPlayerIndex player)
	{
		m_playerOwner = player;
		gnetAssert(m_playerOwner != INVALID_PLAYER_INDEX);
	}

	PhysicalPlayerIndex GetPlayerOwner() const	{ return m_playerOwner;}
	NetworkFXId GetFXId() const					{ return m_fxId; }

	bool IsValid() const { return m_playerOwner != INVALID_PLAYER_INDEX; }
	bool IsClone() const;

	bool operator==(const CNetFXIdentifier& identifier)
	{
		return (IsValid() &&
				identifier.IsValid() &&
				identifier.m_playerOwner == m_playerOwner &&
				identifier.m_fxId == m_fxId);
	}

	template <class Serialiser> void Serialise(datBitBuffer &messageBuffer)
	{
		Serialiser serialiser(messageBuffer);

		SERIALISE_UNSIGNED(serialiser, m_fxId, sizeof(NetworkFXId)<<3, "FX ID");

		// player owner is not serialized,  we can deduce that from the connection (plus the player index differs from machine to machine)
	}

	static NetworkFXId GetNextFXId();

protected:

	PhysicalPlayerIndex	m_playerOwner;		// the player which created this FX
	NetworkFXId			m_fxId;				// the id identifying this FX over the internet

	static NetworkFXId	ms_lastFXId;		// incrementing fx id, each fx is assigned its own id
};

#endif  // NETWORK_FX_ID_H
