// 
// animation/FacialData.h
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved. 
// 

#ifndef FACIAL_DATA_H
#define FACIAL_DATA_H

// Rage headers
#include "fwanimation/animdefines.h"
#include "fwanimation/directorcomponentfacialrig.h"
#include "fwutil/Flags.h"

// Game headers
#include "animation/animdefines.h"

class CPed;

extern const fwMvClipId FACIAL_CLIP_DEAD_1;
extern const fwMvClipId FACIAL_CLIP_DEAD_2;
extern const fwMvClipId FACIAL_CLIP_DIE_1;
extern const fwMvClipId FACIAL_CLIP_DIE_2;
extern const fwMvClipId FACIAL_CLIP_MOOD_AIMING_1;
extern const fwMvClipId FACIAL_CLIP_MOOD_ANGRY_1;
extern const fwMvClipId FACIAL_CLIP_MOOD_HAPPY_1;
extern const fwMvClipId FACIAL_CLIP_MOOD_INJURED_1;
extern const fwMvClipId FACIAL_CLIP_MOOD_NORMAL_1;
extern const fwMvClipId FACIAL_CLIP_MOOD_SKYDIVE_1;
extern const fwMvClipId FACIAL_CLIP_MOOD_STRESSED_1;
extern const fwMvClipId FACIAL_CLIP_MOOD_EXCITED_1;
extern const fwMvClipId FACIAL_CLIP_MOOD_FRUSTRATED_1;
extern const fwMvClipId FACIAL_CLIP_MOOD_TALKING_1;
extern const fwMvClipId FACIAL_CLIP_MOOD_DANCING_LOW;
extern const fwMvClipId FACIAL_CLIP_MOOD_DANCING_MED;
extern const fwMvClipId FACIAL_CLIP_MOOD_DANCING_HIGH;
extern const fwMvClipId FACIAL_CLIP_MOOD_DANCING_TRANCE;
extern const fwMvClipId FACIAL_CLIP_PAIN_1;
extern const fwMvClipId FACIAL_CLIP_PAIN_2;
extern const fwMvClipId FACIAL_CLIP_PAIN_3;
extern const fwMvClipId FACIAL_CLIP_BURNING_1;
extern const fwMvClipId FACIAL_CLIP_ELECTROCUTED_1;
extern const fwMvClipId FACIAL_CLIP_KNOCKOUT_1;
extern const fwMvClipId FACIAL_CLIP_COUGHING_1;

extern const fwMvClipId FACIAL_CLIP_ANGRY;
extern const fwMvClipId FACIAL_CLIP_FIRE_WEAPON;
extern const fwMvClipId FACIAL_CLIP_SHOCKED;
extern const fwMvClipId FACIAL_CLIP_DRIVE_FAST_1;

extern const float MIN_TIME_UNTIL_NEXT_FACIAL_ANIM_QUICK;
extern const float MAX_TIME_UNTIL_NEXT_FACIAL_ANIM_QUICK;
extern const float MIN_BASE_FACIAL_ANIM_PLAYBACK_TIME;
extern const float MAX_BASE_FACIAL_ANIM_PLAYBACK_TIME;

class CFacialDataComponent
{

public:

	//Note: These are organized in order of priority.
	enum FacialIdleClipType
	{
		FICT_None,
		FICT_Normal,
		FICT_Excited,
		FICT_Frustrated,
		FICT_Talking,		
		FICT_DriveFast,
		FICT_Stressed,
		FICT_Angry,
		FICT_DancingLow,
		FICT_DancingMed,
		FICT_DancingHigh,
		FICT_Aiming,
		FICT_Skydive,
		FICT_Injured,
		FICT_Pain,
		FICT_Die,
		FICT_Dead,		
		FICT_OnFire,
		FICT_Electrocution,
		FICT_KnockedOut,
		FICT_Coughing,
	};

private:

	enum Flags
	{
		HasFacialIdleClipBeenSetExplicitly	= BIT0,
		AreFacialIdleClipRequestsValid		= BIT1,
	};

public:
	CFacialDataComponent()
		: m_facialClipSetId(CLIP_SET_ID_INVALID)
		, m_facialIdleClipId(FACIAL_CLIP_MOOD_NORMAL_1)
		, m_fTimeUntilNextFacialAnimChange(0.0f)
		, m_bUseVariationNextAnimChange(true)
		, m_nFacialIdleClipRequest(FICT_None)
		, m_uFlags(0)
		, m_lastPlayedFacialIdleClipSetId(CLIP_SET_ID_INVALID)
		, m_lastPlayedFacialIdleClipId(CLIP_ID_INVALID)
		, m_bFacialIdleLocked(false)
	{
		m_facialIdleAnimOverrideClipNameHash.SetHash(0);
		m_facialIdleAnimOverrideClipDictNameHash.SetHash(0);

		ResetTimeUntilNextFacialVariation(MIN_TIME_UNTIL_NEXT_FACIAL_ANIM_QUICK, MAX_TIME_UNTIL_NEXT_FACIAL_ANIM_QUICK, true);
	}

	~CFacialDataComponent()
	{}

	// Given a valid facial animation index and animation hash key
	void ProcessFacialHashKey(CPed *pPed, u32 animHashKey, u32 duration, bool fromAudio = false, bool speaker = true);

	// Reset the facial idle animation
	void ResetFacialIdleAnimation(CPed* pPed);

	///// Optionally a specific pain animation can be chosen (-1 selects at random).
	void PlayPainFacialAnim(CPed *pPed, int which = -1);

	///// Optionally a specific dying animation can be chosen (-1 selects at random).
	void PlayDyingFacialAnim(CPed *pPed, int which = -1);

	// Returns the current facial animation set for this ped
	inline fwMvClipSetId GetFacialClipSet(void) const { return m_facialClipSetId; }
	void SetFacialClipSet(CPed *pPed, const fwMvClipSetId &facialClipSetId) {
		Assert((facialClipSetId != CLIP_SET_ID_INVALID));
		bool facialIdleChanged = (facialClipSetId!=m_facialClipSetId);
		m_facialClipSetId = facialClipSetId; 
		if (facialIdleChanged)
			OnFacialIdleChanged(pPed);
	}

	fwMvClipSetId GetDefaultFacialClipSetId(CPed* pPed);

	void SetFacialClipsetToDefault(CPed *pPed);

	// Facial Idle overriding
	void SetFacialIdleAnimOverride(CPed *pPed, u32 const clipDictHash, u32 const clipNameHash, char const * clipName = NULL, char const * clipDictionaryName = NULL);
	void ClearFacialIdleAnimOverride(CPed *pPed);
	u32 GetFacialIdleAnimOverrideClipDictNameHash(void); 
	u32 GetFacialIdleAnimOverrideClipNameHash(void); 

	// Returns the current facial idle animation id for this ped
	inline fwMvClipId GetFacialIdleClipID(void) const		{ return m_facialIdleClipId; }
	void SetFacialIdleClipID(CPed *pPed, const fwMvClipId &facialIdleClipId);

	// Play the requested facial animation from the peds facial clip set (if it exists). The anim will play once, overriding the idle animation, and then blend out.
	void PlayFacialAnim(CPed *pPed, const fwMvClipId &facialClipId);
	void PlayFacialAnimByClip(CPed *pPed, const crClip* pClip);

	enum ePedFacialIdleMode
	{
		kModeAnimated=0,
		kModeRagdoll
	};

	void SetFacialIdleMode(CPed *pPed, ePedFacialIdleMode mode);
	
	void ProcessPreTasks(CPed& rPed);
	void ProcessPostTasks(CPed& rPed);
	void RequestFacialIdleClip(FacialIdleClipType nFacialIdleClipRequest);
	void Update(CPed& rPed, float fTimeStep);

private:

	fwMvClipId			ChooseClipForFacialIdleClipType(FacialIdleClipType nFacialIdleClipType) const;
	FacialIdleClipType	GetCurrentFacialIdleClipType() const;
	FacialIdleClipType	GetFacialIdleClipType(fwMvClipId facialIdleClipId) const;
	void				MakeDefaultFacialIdleClipRequests(const CPed& rPed);
	void				OnFacialIdleChanged(CPed *pPed);
	void				ProcessPreFacialIdleRequests(CPed& rPed);
	void				ProcessPostFacialIdleRequests(CPed& rPed);
	void				ResetTimeUntilNextFacialVariation(float fMin, float fMax, bool bNextChangeShouldBeAVariation);
	void				UpdateFacialVariations(CPed& rPed, float fTimeStep);
	void				PlayFacialIdleClip(CPed& rPed, fwMvClipSetId facialIdleClipSetId, fwMvClipId facialIdleClipId, float fBlendTime);

	fwMvClipSetId		m_facialClipSetId;
	fwMvClipId			m_facialIdleClipId;
	float				m_fTimeUntilNextFacialAnimChange;
	bool				m_bUseVariationNextAnimChange;
	FacialIdleClipType	m_nFacialIdleClipRequest;
	fwFlags8			m_uFlags;

	fwMvClipSetId		m_lastPlayedFacialIdleClipSetId;
	fwMvClipId			m_lastPlayedFacialIdleClipId;

	atHashString		m_facialIdleAnimOverrideClipNameHash;
	atHashString		m_facialIdleAnimOverrideClipDictNameHash;

	bool				m_bFacialIdleLocked; // Used when we override the facial idle, prevents any further changes from mood changes, variations etc.
};

#endif
