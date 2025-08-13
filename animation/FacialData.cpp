// 
// animation/FacialData.cpp
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved. 
// 

#include "animation/FacialData.h"

#include "fwanimation/facialclipsetgroups.h"
#include "animation/MovePed.h"
#include "debug/debugscene.h"
#include "Peds/ped.h"
#include "cutscene/CutSceneManagerNew.h"
#include "ModelInfo/PedModelInfo.h"

AI_OPTIMISATIONS()
ANIM_OPTIMISATIONS()

const fwMvClipId FACIAL_CLIP_DEAD_1("dead_1",0x443B7D10);
const fwMvClipId FACIAL_CLIP_DEAD_2("dead_2",0x3209D8AD);
const fwMvClipId FACIAL_CLIP_DIE_1("die_1",0x2D8ED01E);
const fwMvClipId FACIAL_CLIP_DIE_2("die_2",0xE7D1C4A5);
const fwMvClipId FACIAL_CLIP_MOOD_AIMING_1("mood_aiming_1",0x834CA961);
const fwMvClipId FACIAL_CLIP_MOOD_ANGRY_1("mood_angry_1",0x17B74CFD);
const fwMvClipId FACIAL_CLIP_MOOD_HAPPY_1("mood_happy_1",0x94D9B795);
const fwMvClipId FACIAL_CLIP_MOOD_INJURED_1("mood_injured_1",0xA138CC3E);
const fwMvClipId FACIAL_CLIP_MOOD_NORMAL_1("mood_normal_1",0xBD789759);
const fwMvClipId FACIAL_CLIP_MOOD_SKYDIVE_1("mood_skydive_1",0xe3d8b68b);
const fwMvClipId FACIAL_CLIP_MOOD_STRESSED_1("mood_stressed_1",0xF5B477B2);
const fwMvClipId FACIAL_CLIP_MOOD_EXCITED_1("mood_excited_1",0x38CA1CF7);
const fwMvClipId FACIAL_CLIP_MOOD_FRUSTRATED_1("mood_frustrated_1",0x846FB7F4);
const fwMvClipId FACIAL_CLIP_MOOD_TALKING_1("mood_talking_1",0x687876D8);
const fwMvClipId FACIAL_CLIP_MOOD_DANCING_LOW("mood_dancing_low",0x22A7B017);
const fwMvClipId FACIAL_CLIP_MOOD_DANCING_MED("mood_dancing_med",0x150CB9F6);
const fwMvClipId FACIAL_CLIP_MOOD_DANCING_HIGH("mood_dancing_High",0xC4C2CEEF);
const fwMvClipId FACIAL_CLIP_MOOD_DANCING_TRANCE("mood_dancing_trance",0xF10DE875);
const fwMvClipId FACIAL_CLIP_PAIN_1("pain_1",0xA6BABF70);
const fwMvClipId FACIAL_CLIP_PAIN_2("pain_2",0xB4C55B85);
const fwMvClipId FACIAL_CLIP_PAIN_3("pain_3",0x54791AF2);
const fwMvClipId FACIAL_CLIP_BURNING_1("burning_1",0x0aa7e6ef);
const fwMvClipId FACIAL_CLIP_ELECTROCUTED_1("electrocuted_1",0x5705c60d);
const fwMvClipId FACIAL_CLIP_KNOCKOUT_1("mood_knockout_1",0x997ea314);
const fwMvClipId FACIAL_CLIP_COUGHING_1("coughing_1",0x0d7ab534);

const fwMvClipId FACIAL_CLIP_ANGRY("mood_angry_1",0x17B74CFD);
const fwMvClipId FACIAL_CLIP_FIRE_WEAPON("pain_3",0x54791AF2);
const fwMvClipId FACIAL_CLIP_SHOCKED("mood_stressed_1",0xF5B477B2);
const fwMvClipId FACIAL_CLIP_DRIVE_FAST_1("mood_drivefast_1", 0xfb0d5a21);

const fwMvRequestId OneShotFacialRequestId("FacialOneShotRequest",0x7BF8BB68);
const fwMvFlagId OneShotFacialBlendOutFlagId("FacialOneShot_NoFacialOneShot",0x883F1068);

// Once we swapped facial idle how soon before we try and play a variation?
const float MIN_TIME_UNTIL_NEXT_FACIAL_ANIM_QUICK = 0.1f;
const float MAX_TIME_UNTIL_NEXT_FACIAL_ANIM_QUICK = 1.0f;

// Default facial idle blend time
const float DEFAULT_FACIAL_IDLE_BLEND_TIME = SLOW_BLEND_DURATION;

// Reset the facial idle animation
void  CFacialDataComponent::ResetFacialIdleAnimation(CPed* pPed)
{
	SetFacialIdleClipID(pPed, FACIAL_CLIP_MOOD_NORMAL_1);

	//Note that the facial idle clip has no longer been set explicitly.
	m_uFlags.ClearFlag(HasFacialIdleClipBeenSetExplicitly);
}

fwMvClipSetId CFacialDataComponent::GetDefaultFacialClipSetId(CPed* pPed)
{
	if (pPed)
	{
		CPedModelInfo* pPedModelInfo = pPed->GetPedModelInfo();
		if (pPedModelInfo)
		{
			fwFacialClipSetGroup* pFacialClipSetGroup = fwFacialClipSetGroupManager::GetFacialClipSetGroup(pPedModelInfo->GetFacialClipSetGroupId());
			if (pFacialClipSetGroup)
			{
				fwMvClipSetId facialBaseClipSetId(pFacialClipSetGroup->GetBaseClipSetName());
				return facialBaseClipSetId;
			}
		}
	}

	return CLIP_SET_ID_INVALID;
}

u32 CFacialDataComponent::GetFacialIdleAnimOverrideClipNameHash(void)
{
	return m_facialIdleAnimOverrideClipNameHash.GetHash(); 
}

u32 CFacialDataComponent::GetFacialIdleAnimOverrideClipDictNameHash(void)
{
	return m_facialIdleAnimOverrideClipDictNameHash.GetHash();
}

// Resets the facial clipset back to the default of the ped
void CFacialDataComponent::SetFacialClipsetToDefault(CPed *pPed)
{
	// Get the ped's model info, which is where we get the default from
	if (pPed)
	{
		CPedModelInfo* pPedModelInfo = pPed->GetPedModelInfo();
		if (pPedModelInfo)
		{
			fwFacialClipSetGroup* pFacialClipSetGroup = fwFacialClipSetGroupManager::GetFacialClipSetGroup(pPedModelInfo->GetFacialClipSetGroupId());
			if (pFacialClipSetGroup)
			{
				fwMvClipSetId facialBaseClipSetId(pFacialClipSetGroup->GetBaseClipSetName());
				Assertf(fwClipSetManager::GetClipSet(facialBaseClipSetId), "%s: Unrecognised facial base clipset, could not set facial clipset back to default", pFacialClipSetGroup->GetBaseClipSetName().GetCStr());
				SetFacialClipSet(pPed, facialBaseClipSetId);
			}
		}
	}
}

// Facial Idle overriding
// NOTES
// clipName / clipDictionaryName will not be available for use on a remote machine, we'll only have the hashes that are sent over the network.
void CFacialDataComponent::SetFacialIdleAnimOverride(CPed *pPed, u32 const clipHash, u32 const clipDictHash, char const * ASSERT_ONLY(pOverrideIdleClipName) /* = NULL */, char const * ASSERT_ONLY(pOverrideIdleClipDictName) /* = NULL */)
{
	if (pPed)
	{
		const crClip* pClip = NULL;
		if (clipDictHash != 0)
		{
			s32 dictIndex = fwAnimManager::FindSlotFromHashKey(clipDictHash).Get();

			if(dictIndex >= 0)
			{
				// Get clip from the specified dictionary
				pClip = fwAnimManager::GetClipIfExistsByDictIndex(dictIndex, clipHash);
				animAssertf(pClip, "SET_FACIAL_IDLE_ANIM_OVERRIDE - Clip not found in specified dictionary. Check that dictionary: %d / %s is loaded and anim: %d / %s is contained within the dictionary", clipDictHash, pOverrideIdleClipDictName, clipHash, pOverrideIdleClipName);
			}
		}
		else
		{
			// Get clip from the ped's current facial clipset
			fwMvClipSetId facialClipSet = pPed->GetFacialData()->GetFacialClipSet();
			if (facialClipSet != CLIP_SET_ID_INVALID)
			{
				pClip = fwAnimManager::GetClipIfExistsBySetId(facialClipSet, fwMvClipId(clipHash));
				animAssertf(pClip, "SET_FACIAL_IDLE_ANIM_OVERRIDE - Clip not found in ped's facial clipset. Check that anim: %d / %s is contained within the clipset", clipHash, pOverrideIdleClipName);
			}
		}

		if (pClip)
		{
			// Play the facial idle
			CMovePed& move = pPed->GetMovePed();
			move.SetFacialIdleClip(pClip, NORMAL_BLEND_DURATION);

			// Cache the info used for networking...
			m_facialIdleAnimOverrideClipNameHash.SetHash(clipHash);
			m_facialIdleAnimOverrideClipDictNameHash.SetHash(clipDictHash);

			// Lock
			m_bFacialIdleLocked = true;
		}
	}
}

void CFacialDataComponent::ClearFacialIdleAnimOverride(CPed *pPed)
{
	if (pPed)
	{
		// Unlock
		m_bFacialIdleLocked = false;

		// Wipe the clip
		m_facialIdleAnimOverrideClipNameHash.SetHash(0);
		m_facialIdleAnimOverrideClipDictNameHash.SetHash(0);

		// Push on a new facial idle
		OnFacialIdleChanged(pPed);
	}
}

void CFacialDataComponent::SetFacialIdleClipID(CPed *pPed, const fwMvClipId &facialIdleClipId)
{ 
	bool facialIdleChanged = (facialIdleClipId!=m_facialIdleClipId) || !pPed->GetMovePed().IsPlayingStandardFacial();
	m_facialIdleClipId = facialIdleClipId; 
	if (facialIdleChanged)
		OnFacialIdleChanged(pPed); 
	
	//Note that the facial idle clip has been set explicitly.
	m_uFlags.SetFlag(HasFacialIdleClipBeenSetExplicitly);
}

void CFacialDataComponent::SetFacialIdleMode(CPed *pPed, ePedFacialIdleMode mode)
{
	static fwMvFlagId s_RagdollModeFlag("UseRagdollFacial",0x81CDAA8E);

	pPed->GetMovePed().SetFlag(s_RagdollModeFlag, mode==kModeRagdoll);

	OnFacialIdleChanged(pPed);
}

void CFacialDataComponent::OnFacialIdleChanged(CPed* pPed)
{
	if (pPed)
	{
		PlayFacialIdleClip(*pPed, m_facialClipSetId, m_facialIdleClipId, DEFAULT_FACIAL_IDLE_BLEND_TIME);

		// Idle has changed, may need new variations to play
		ResetTimeUntilNextFacialVariation(MIN_TIME_UNTIL_NEXT_FACIAL_ANIM_QUICK, MAX_TIME_UNTIL_NEXT_FACIAL_ANIM_QUICK, true);
	}
}

void CFacialDataComponent::PlayFacialAnim(CPed *pPed, const fwMvClipId &facialClipId)
{
	if (pPed && pPed->GetAnimDirector() && m_facialClipSetId != CLIP_SET_ID_INVALID && facialClipId != CLIP_ID_INVALID)
	{
		CMovePed& move = pPed->GetMovePed();
		const crClip* pClip = fwClipSetManager::GetClip(m_facialClipSetId, facialClipId, true);

		if (pClip)
		{
			//@@: location CFACIALDATACOMPONENT_PLAYFACIALANIM
			move.PlayOneShotFacialClip(pClip, NORMAL_BLEND_DURATION);
		}
	}
}

void CFacialDataComponent::PlayFacialAnimByClip(CPed *pPed, const crClip* pClip)
{
	if (pPed && pPed->GetAnimDirector() && pClip)
	{
		CMovePed& move = pPed->GetMovePed();
		move.PlayOneShotFacialClip(pClip, NORMAL_BLEND_DURATION);
	}
}

void CFacialDataComponent::PlayPainFacialAnim(CPed *pPed, s32 which /* = -1 */)
{
	const int numClips = 3;
	fwMvClipId painClips[numClips] = {
		FACIAL_CLIP_PAIN_1,
		FACIAL_CLIP_PAIN_2,
		FACIAL_CLIP_PAIN_3
	};

	if (m_facialClipSetId!=CLIP_SET_ID_INVALID)
	{
		// if the specified clip is -1, select a random clip from the list available
		s32 clipIndex = which;

		// TODO - Allow for some of them being missing
		if (clipIndex==-1)
		{
			clipIndex = fwRandom::GetRandomNumberInRange(0, numClips-1);
		}

		if (animVerifyf(clipIndex >= 0 && clipIndex < numClips, "CFacialDataComponent::PlayPainFacialAnim - clipIndex out of range"))
		{
			PlayFacialAnim(pPed, painClips[clipIndex] );
		}
	}
};

void CFacialDataComponent::PlayDyingFacialAnim(CPed *pPed, int which /* = -1 */)
{
	const int numClips = 2;
	fwMvClipId dyingClips[numClips] = {
		FACIAL_CLIP_DIE_1,
		FACIAL_CLIP_DIE_2
	};

	if (m_facialClipSetId!=CLIP_SET_ID_INVALID)
	{
		// if the specified clip is -1, select a random clip from the list available
		s32 clipIndex = which;

		if (clipIndex==-1)
		{
			clipIndex = fwRandom::GetRandomNumberInRange(0, numClips-1);
		}

		if (animVerifyf(clipIndex >= 0 && clipIndex < numClips, "CFacialDataComponent::PlayDyingFacialAnim - clipIndex out of range"))
		{
			PlayFacialAnim(pPed, dyingClips[clipIndex] );
		}
	}
}

void CFacialDataComponent::ProcessFacialHashKey(CPed *pPed, u32 animHashKey, u32 UNUSED_PARAM(duration),  bool UNUSED_PARAM(fromAudio), bool UNUSED_PARAM(speaker))
{
	if (pPed)
	{
		fwMvClipSetId clipSetID = GetFacialClipSet();
		if (clipSetID != CLIP_SET_ID_INVALID)
		{
			fwMvClipId facialClipId(animHashKey);
			const crClip* pClip = fwClipSetManager::GetClip(clipSetID, facialClipId, true);

			if (pClip)
			{
				// Play it
				pPed->GetMovePed().PlayOneShotFacialClip(pClip, NORMAL_BLEND_DURATION);
			}
		}
	}
}

void CFacialDataComponent::ProcessPreTasks(CPed& rPed)
{
	//Process the facial idle requests.
	ProcessPreFacialIdleRequests(rPed);
}

void CFacialDataComponent::ProcessPostTasks(CPed& rPed)
{
	//Process the facial idle requests.
	ProcessPostFacialIdleRequests(rPed);
}

void CFacialDataComponent::RequestFacialIdleClip(FacialIdleClipType nFacialIdleClipRequest)
{
	//Ensure facial idle clip requests are valid.
	if(!animVerifyf(m_uFlags.IsFlagSet(AreFacialIdleClipRequestsValid), "Facial idle clip requests are invalid."))
	{
		return;
	}

	//Ensure the facial idle clip request is higher priority than the current.
	if(nFacialIdleClipRequest <= m_nFacialIdleClipRequest)
	{
		return;
	}
	
	//Set the facial idle clip request.
	m_nFacialIdleClipRequest = nFacialIdleClipRequest;
}

void CFacialDataComponent::Update(CPed& rPed, float fTimeStep)
{
	//Update the facial variations.
	UpdateFacialVariations(rPed, fTimeStep);
}

fwMvClipId CFacialDataComponent::ChooseClipForFacialIdleClipType(FacialIdleClipType nFacialIdleClipType) const
{
	//Check the facial idle clip type.
	switch(nFacialIdleClipType)
	{
		case FICT_Normal:
		{
			return FACIAL_CLIP_MOOD_NORMAL_1;
		}
		case FICT_Excited:
		{
			return FACIAL_CLIP_MOOD_EXCITED_1;
		}
		case FICT_Frustrated:
		{
			return FACIAL_CLIP_MOOD_FRUSTRATED_1;
		}
		case FICT_Talking:
		{
			return FACIAL_CLIP_MOOD_TALKING_1;
		}
		case FICT_Stressed:
		{
			return FACIAL_CLIP_MOOD_STRESSED_1;
		}
		case FICT_DriveFast:
		{
			return FACIAL_CLIP_DRIVE_FAST_1;
		}
		case FICT_Angry:
		{
			return FACIAL_CLIP_MOOD_ANGRY_1;
		}
		case FICT_Aiming:
		{
			return FACIAL_CLIP_MOOD_AIMING_1;
		}
		case FICT_Skydive:
		{
			return FACIAL_CLIP_MOOD_SKYDIVE_1;
		}
		case FICT_Injured:
		{
			return FACIAL_CLIP_MOOD_INJURED_1;
		}
		case FICT_Pain:
		{
			float fRandom = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
			if(fRandom <= 0.33f)
			{
				return FACIAL_CLIP_PAIN_1;
			}
			else if(fRandom <= 0.66f)
			{
				return FACIAL_CLIP_PAIN_2;
			}
			else
			{
				return FACIAL_CLIP_PAIN_3;
			}
		}
		case FICT_Die:
		{
			if(fwRandom::GetRandomTrueFalse())
			{
				return FACIAL_CLIP_DIE_1;
			}
			else
			{
				return FACIAL_CLIP_DIE_2;
			}
		}
		case FICT_Dead:
		{
			if(fwRandom::GetRandomTrueFalse())
			{
				return FACIAL_CLIP_DEAD_1;
			}
			else
			{
				return FACIAL_CLIP_DEAD_2;
			}
		}
		case FICT_OnFire:
		{
			return FACIAL_CLIP_BURNING_1;
		}
		case FICT_Electrocution:
		{
			return FACIAL_CLIP_ELECTROCUTED_1;
		}
		case FICT_KnockedOut:
		{
			return FACIAL_CLIP_KNOCKOUT_1;
		}
		case FICT_Coughing:
		{
			return FACIAL_CLIP_COUGHING_1;
		}
		case FICT_None:
		default:
		{
			animAssertf(false, "The facial idle clip type is invalid: %d.", nFacialIdleClipType);
			return CLIP_ID_INVALID;
		}
	}
}


CFacialDataComponent::FacialIdleClipType CFacialDataComponent::GetCurrentFacialIdleClipType() const
{
	return GetFacialIdleClipType(m_facialIdleClipId);
}

CFacialDataComponent::FacialIdleClipType CFacialDataComponent::GetFacialIdleClipType(fwMvClipId facialIdleClipId) const
{
	//Check the facial idle clip.
	if(facialIdleClipId == FACIAL_CLIP_MOOD_NORMAL_1)
	{
		return FICT_Normal;
	}
	else if (facialIdleClipId == FACIAL_CLIP_MOOD_EXCITED_1)
	{
		return FICT_Excited;
	}
	else if (facialIdleClipId == FACIAL_CLIP_MOOD_FRUSTRATED_1)
	{
		return FICT_Frustrated;
	}
	else if (facialIdleClipId == FACIAL_CLIP_MOOD_TALKING_1)
	{
		return FICT_Talking;
	}
	else if(facialIdleClipId == FACIAL_CLIP_MOOD_STRESSED_1)
	{
		return FICT_Stressed;
	}
	else if(facialIdleClipId == FACIAL_CLIP_DRIVE_FAST_1)
	{
		return FICT_DriveFast;
	}
	else if(facialIdleClipId == FACIAL_CLIP_MOOD_ANGRY_1)
	{
		return FICT_Angry;
	}
	else if(facialIdleClipId == FACIAL_CLIP_MOOD_AIMING_1)
	{
		return FICT_Aiming;
	}
	else if(facialIdleClipId == FACIAL_CLIP_MOOD_SKYDIVE_1)
	{
		return FICT_Skydive;
	}
	else if(facialIdleClipId == FACIAL_CLIP_MOOD_INJURED_1)
	{
		return FICT_Injured;
	}
	else if(facialIdleClipId == FACIAL_CLIP_PAIN_1 || facialIdleClipId == FACIAL_CLIP_PAIN_2 || facialIdleClipId == FACIAL_CLIP_PAIN_3)
	{
		return FICT_Pain;
	}
	else if(facialIdleClipId == FACIAL_CLIP_DIE_1 || facialIdleClipId == FACIAL_CLIP_DIE_2)
	{
		return FICT_Die;
	}
	else if(facialIdleClipId == FACIAL_CLIP_DEAD_1 || facialIdleClipId == FACIAL_CLIP_DEAD_2)
	{
		return FICT_Dead;
	}
	else if (facialIdleClipId == FACIAL_CLIP_ELECTROCUTED_1)
	{
		return FICT_Electrocution;
	}
	else if (facialIdleClipId == FACIAL_CLIP_BURNING_1)
	{
		return FICT_OnFire;
	}
	else if(facialIdleClipId == FACIAL_CLIP_KNOCKOUT_1)
	{
		return FICT_KnockedOut;
	}
	else if(facialIdleClipId == FACIAL_CLIP_COUGHING_1)
	{
		return FICT_Coughing;
	}
	else
	{
		animAssertf(false, "Unknown idle clip id: %s.", facialIdleClipId.GetCStr());
		return FICT_None;
	}
}

void CFacialDataComponent::MakeDefaultFacialIdleClipRequests(const CPed& rPed)
{
	//Make the normal request.
	RequestFacialIdleClip(FICT_Normal);
	
	if (rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_HitByTranqWeapon))
	{
		if (rPed.GetIsDeadOrDying())
		{
			RequestFacialIdleClip(FICT_KnockedOut);
		}
		else
		{
			RequestFacialIdleClip(FICT_Pain);
		}
	}
	//Check if the ped is hurt.
	else if(rPed.HasHurtStarted())
	{
		//Make the injured request.
		RequestFacialIdleClip(FICT_Injured);
	}
}

void CFacialDataComponent::ProcessPreFacialIdleRequests(CPed& rPed)
{
	//Note that facial idle clip requests are valid.
	m_uFlags.SetFlag(AreFacialIdleClipRequestsValid);

	//Clear the facial idle clip request.
	m_nFacialIdleClipRequest = FICT_None;
	
	//Make the default facial idle clip requests.
	MakeDefaultFacialIdleClipRequests(rPed);
}

void CFacialDataComponent::ProcessPostFacialIdleRequests(CPed& rPed)
{
	//Note that facial idle clip requests are no longer valid.
	m_uFlags.ClearFlag(AreFacialIdleClipRequestsValid);
	
	//Ensure the facial idle clip has not been set explicity.
	if(m_uFlags.IsFlagSet(HasFacialIdleClipBeenSetExplicitly))
	{
		return;
	}
	
	//Ensure the facial idle clip request is valid.
	if(m_nFacialIdleClipRequest == FICT_None)
	{
		return;
	}
	
	//Ensure the facial idle clip type is changing.
	FacialIdleClipType nCurrentFacialIdleClipType = GetCurrentFacialIdleClipType();
	if(nCurrentFacialIdleClipType == m_nFacialIdleClipRequest)
	{
		return;
	}
	
	//Choose a clip for the facial idle clip type.
	fwMvClipId facialIdleClipId = ChooseClipForFacialIdleClipType(m_nFacialIdleClipRequest);
	if(facialIdleClipId == CLIP_ID_INVALID)
	{
		return;
	}
	
	//Set the facial idle clip.
	SetFacialIdleClipID(&rPed, facialIdleClipId);

	//Note that the facial idle clip has not been set explicitly.
	m_uFlags.ClearFlag(HasFacialIdleClipBeenSetExplicitly);
}

//////////////////////////////////////////////////////////////////////////
// FACIAL VARIATIONS
//////////////////////////////////////////////////////////////////////////

// Normal variations are much lower priority than the other moods - angry, aiming etc.
#define IMPORTANCE_MOOD_NORMAL (1.0f)
#define IMPORTANCE_MOOD_STRESSED (2.0f)
#define IMPORTANCE_MOOD_OTHER (10.0f)

// On-screen peds are more important
#define IMPORTANCE_MULITPLIER_ONSCREEN (2.0f)

// Multiplier for the player ped to make their variation requests more important
// Note: Currently no boost.
#define IMPORTANCE_MULITPLIER_PLAYER (1.0f)

void CFacialDataComponent::UpdateFacialVariations(CPed& rPed, float fTimeStep)
{
	// Do not kick off any facial variations if:
	// * A cutscene is playing
	// * Ped is dead
	// * Zero timestep
	// * Facial idle is currently locked (overridden)
	if ((CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsCutscenePlayingBack()) || rPed.IsDead() || fTimeStep <= 0.0f || m_facialClipSetId == CLIP_SET_ID_INVALID || m_bFacialIdleLocked)
	{
		return;
	}

	// Keep requesting the facial variations if necessary
	VariationMood eMood = MoodNormal;
	float fRequestImportance = IMPORTANCE_MOOD_OTHER;

	if (m_facialIdleClipId == FACIAL_CLIP_MOOD_AIMING_1)
	{
		eMood = MoodAiming;
	}
	else if (m_facialIdleClipId == FACIAL_CLIP_MOOD_EXCITED_1)
	{
		eMood = MoodStressed;
	}
	else if (m_facialIdleClipId == FACIAL_CLIP_MOOD_FRUSTRATED_1)
	{
		eMood = MoodFrustrated;
	}
	else if (m_facialIdleClipId == FACIAL_CLIP_MOOD_TALKING_1)
	{
		eMood = MoodTalking;
	}
	else if (m_facialIdleClipId == FACIAL_CLIP_MOOD_ANGRY_1)
	{
		eMood = MoodAngry;
	}
	else if (m_facialIdleClipId == FACIAL_CLIP_MOOD_HAPPY_1)
	{
		eMood = MoodHappy;
	}
	else if (m_facialIdleClipId == FACIAL_CLIP_MOOD_INJURED_1)
	{
		eMood = MoodInjured;
	}
	else if (m_facialIdleClipId == FACIAL_CLIP_MOOD_NORMAL_1)
	{
		eMood = MoodNormal;
		fRequestImportance = IMPORTANCE_MOOD_NORMAL;
	}
	else if (m_facialIdleClipId == FACIAL_CLIP_MOOD_STRESSED_1)
	{
		eMood = MoodStressed;
		fRequestImportance = IMPORTANCE_MOOD_STRESSED;
	}
	else
	{
		// Not a facial idle we care about
		return;
	}

	// Importance multipliers...
	if (rPed.IsPlayer())
	{
		fRequestImportance *= IMPORTANCE_MULITPLIER_PLAYER;
	}

	if (rPed.GetIsOnScreen())
	{
		fRequestImportance *= IMPORTANCE_MULITPLIER_ONSCREEN;
	}

	// Request the mood variations, and get a pointer to the variation info (contains all the parameters)
	const fwFacialClipSetVariationInfo* pVariationInfo = fwFacialClipSetGroupManager::RequestVariationsAndGetVariationInfo(rPed.GetPedModelInfo()->GetFacialClipSetGroupId(), eMood, fRequestImportance);

	//Ensure the time until the next facial variation has expired.
	m_fTimeUntilNextFacialAnimChange -= fTimeStep;
	if(m_fTimeUntilNextFacialAnimChange > 0.0f || !pVariationInfo)
	{
		return;
	}

	// What type of animation should we play?  Base or variation?
	if (m_bUseVariationNextAnimChange)
	{
		// VARIATION
		// Attempt to get a variation clipset and clip
		fwMvClipSetId varClipSetId = CLIP_SET_ID_INVALID;
		fwMvClipId varClipId = CLIP_ID_INVALID;
		fwFacialClipSetGroupManager::GetVariationClipSetAndClip(rPed.GetPedModelInfo()->GetFacialClipSetGroupId(), eMood, varClipSetId, varClipId);

		// If we got a variation, play it, otherwise try again soon
		if (varClipSetId != CLIP_SET_ID_INVALID && varClipId != CLIP_ID_INVALID)
		{
			PlayFacialIdleClip(rPed, varClipSetId, varClipId, pVariationInfo->GetBlendInTime());

			//Reset the time until the next facial variation.
			ResetTimeUntilNextFacialVariation(pVariationInfo->GetMinimumPlaybackTime(), pVariationInfo->GetMaximumPlaybackTime(), false);
		}
		else
		{
			// Couldn't get a variation so play the base facial anim.
			PlayFacialIdleClip(rPed, m_facialClipSetId, m_facialIdleClipId, pVariationInfo->GetBlendOutTime());

			// Keep quickly trying for that variation...
			ResetTimeUntilNextFacialVariation(MIN_TIME_UNTIL_NEXT_FACIAL_ANIM_QUICK, MIN_TIME_UNTIL_NEXT_FACIAL_ANIM_QUICK, true);
		}
	}
	else
	{
		// BASE
		// Note GetBlendOutTime() is for the variation, so it is used as the blend in time for the base.
		PlayFacialIdleClip(rPed, m_facialClipSetId, m_facialIdleClipId, pVariationInfo->GetBlendOutTime());
		ResetTimeUntilNextFacialVariation(pVariationInfo->GetMinimumBasePlaybackTime(), pVariationInfo->GetMaximumBasePlaybackTime(), true);
	}
}

void CFacialDataComponent::PlayFacialIdleClip(CPed& rPed, fwMvClipSetId facialIdleClipSetId, fwMvClipId facialIdleClipId, float fBlendTime)
{
	// Early out if the facial idle is currently locked (overridden)
	if (m_bFacialIdleLocked)
	{
		return;
	}

	// Early out if we're trying to play the same anim that's on there or if the clipset or clip is invalid
	if (facialIdleClipSetId == CLIP_SET_ID_INVALID || facialIdleClipId == CLIP_ID_INVALID || (facialIdleClipSetId == m_lastPlayedFacialIdleClipSetId && facialIdleClipId == m_lastPlayedFacialIdleClipId))
	{
		return;
	}

	if (rPed.GetAnimDirector())
	{
		const crClip* pClip = fwClipSetManager::GetClip(facialIdleClipSetId, facialIdleClipId, true);
		animAssertf(pClip, "Unable to facial idle clip : %s (Model Name: %s)", fwClipSetManager::GetRequestFailReason(facialIdleClipSetId, facialIdleClipId).c_str(), rPed.GetModelName());

		if (pClip)
		{
			CMovePed& move = rPed.GetMovePed();
			move.SetFacialIdleClip(pClip, fBlendTime);

			// Store what we started playing
			m_lastPlayedFacialIdleClipSetId = facialIdleClipSetId;
			m_lastPlayedFacialIdleClipId = facialIdleClipId;
		}
	}
}

void CFacialDataComponent::ResetTimeUntilNextFacialVariation(float fMin, float fMax, bool bNextChangeShouldBeAVariation)
{
	//Reset the time until the next facial variation.
	m_fTimeUntilNextFacialAnimChange = fwRandom::GetRandomNumberInRange(fMin, fMax);

	m_bUseVariationNextAnimChange = bNextChangeShouldBeAVariation;
}
