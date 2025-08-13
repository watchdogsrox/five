//
// audio/entitytracker.h
//
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
//

#ifndef AUD_ENTITY_TRACKER_H
#define AUD_ENTITY_TRACKER_H

#include "audio_channel.h"

#include "audioengine/tracker.h"
#include "audioengine/widgets.h"

#include "vector/vector3.h"
// forward declaration of game-specific base entity class
class CEntity;
using namespace rage;
// PURPOSE
//  The custom audTracker for this game - it provides a method of having a
//	sound automatically track the physical position of a game-world object.
//  We now only embed these in relatively high level entities, such as peds and vehicles, to avoid
//  having so many of them.
//  It's now called a PlaceableTracker, as it can live in anything that inherits from Placeable.
class audPlaceableTracker : public audTracker
{
public:
	audPlaceableTracker() : m_ParentPlaceable(NULL)
	{
		m_Orientation.Init();
	}

	virtual ~audPlaceableTracker()
	{
	}

	// PURPOSE
	//  Initializes the tracker. Just stores a pointer to its parent.
	virtual void Init(CEntity *parentPlaceable)
	{
		naAssertf(parentPlaceable, "audPlaceableTracker must be initialised with a valid CEntity");
		m_ParentPlaceable = parentPlaceable;
	}

	// PURPOSE
	//  Returns the parent's Position vector.
	virtual const Vector3 GetPosition() const;

	// PURPOSE
	//  Returns the tracker's orientation.
	virtual audCompressedQuat GetOrientation() const;

	// PURPOSE
	// Calculate the orientation of the tracker
	void CalculateOrientation();

protected:
	CEntity *m_ParentPlaceable;
	audCompressedQuat m_Orientation;
};

#endif // AUD_ENTITY_TRACKER_H
