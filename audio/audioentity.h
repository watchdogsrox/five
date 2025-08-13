// 
// audio/audioentity.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef NA_AUDIOENTITY_H
#define NA_AUDIOENTITY_H

// Game
#include "scene/RegdRefTypes.h"
#include "control/replay/ReplaySettings.h"

// Rage
#include "atl/array.h"
#include "audioengine/metadataref.h"
#include "fwaudio/audioentity.h"
#include "fwscene/world/InteriorLocation.h"

#include "system/criticalsection.h"

struct naDeferredSound
{
	naDeferredSound()
	{
		audioEntity = NULL;
		entity = NULL;
		useEnvGroup = false;
		useTracker = false;
		invalidateEntity = false;
		persistAcrossClears = false;
	}

	audSoundInitParams initParams;
	audMetadataRef metadataRef;
	audEntity* audioEntity; 
	RegdConstEnt entity;
	bool useEnvGroup : 1;
	bool useTracker : 1;
	bool invalidateEntity : 1;
	bool persistAcrossClears : 1;
};

class naEnvironmentGroup;

class naAudioEntity : public fwAudioEntity
{
public:
	AUDIO_ENTITY_NAME(naAudioEntity);

	naAudioEntity();
	virtual ~naAudioEntity();

	void Init();
	void Shutdown();

	static void InitClass();

	enum EnvironmentGroupRequestType
	{
		LOD = BIT(0),
		MIXGROUP = BIT(1),
		SHUTDOWN = BIT(2),
		PLAYER_MIXGROUP = BIT(3),
	};
	virtual void CreateEnvironmentGroup(const char* debugIdentifier,u32 type = LOD);
	virtual void RemoveEnvironmentGroup(u32 type = LOD);

	virtual void ProcessAnimEvents();

	void AddEnvironmentGroupRequest(u32 type);
	void RemoveEnvironmentGroupRequest(u32 type);
	virtual naEnvironmentGroup *GetEnvironmentGroup(bool create = false) const;
	virtual naEnvironmentGroup *GetEnvironmentGroup(bool create = false);

	// PURPOSE
	//  For sounds triggered when the audio thread is running, we can't create environmentGroups and audTracker's because the controller will delete
	//  them before they have a chance to add a reference so we end up with bad pointers.
	//  You MUST pass an entity as that's what we use to get the tracker and environment group information from
	// RETURNS
	//	True if the sound was added to the deferred list which is triggered in the FinishUpdate, false otherwise (ie sound doesn't exist)
	bool CreateDeferredSound(const char* soundName, const CEntity* entity, const audSoundInitParams* initParams = NULL,  const bool useEnvGroup = false, const bool useTracker = false, const bool invalidateEntity = false, const bool persistAcrossClears = false);
	bool CreateDeferredSound(const u32 soundHash, const CEntity* entity, const audSoundInitParams* initParams = NULL, const bool useEnvGroup = false, const bool useTracker = false, const bool invalidateEntity = false, const bool persistAcrossClears = false);
	bool CreateDeferredSound(const audMetadataRef metadataRef, const CEntity* entity, const audSoundInitParams* initParams = NULL, const bool useEnvGroup = false, const bool useTracker = false, const bool invalidateEntity = false, const bool persistAcrossClears = false);

	void SetCachedInteriorLocation(const fwInteriorLocation intLoc) { m_CachedIntLoc = intLoc; }
	fwInteriorLocation GetCachedInteriorLocation() const { return m_CachedIntLoc; }

	static void ProcessDeferredSounds();

#if GTA_REPLAY
	static void ClearDeferredSoundsForReplay();
#endif //GTA_REPLAY
	
protected:

	virtual void HandleLoopingAnimEvent(const audAnimEvent & event);
	 
	naEnvironmentGroup* m_EnvironmentGroup;
	u32 m_GroupRequests;
	u16 m_AnimLoopTimeout;
	
	static u16 sm_BaseAnimLoopTimeout;

public:

	static const u32 sm_MaxNumDeferredSounds = 192;

private:

	fwInteriorLocation m_CachedIntLoc;
	static atFixedArray<naDeferredSound, sm_MaxNumDeferredSounds> sm_DeferredSoundList;
	static sysCriticalSectionToken sm_EnvGroupCritSec;
};

#endif



