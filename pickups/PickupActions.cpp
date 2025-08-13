// File header
#include "Pickups/PickupActions.h"

// Rage headers
#include "audio/frontendaudioentity.h"
#include "audioengine/engine.h"
#include "audio/northaudioengine.h"
#include "audio/environment/microphones.h"

// Game headers
#include "Peds/ped.h"
#include "Pickups/Data/PickupDataManager.h"
#include "Pickups/Pickup.h"
#include "System/controlMgr.h"

#include "data/aes_init.h"
AES_INIT_9;

WEAPON_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CPickupActionAudio
//////////////////////////////////////////////////////////////////////////

void CPickupActionAudio::Apply(CPickup* pPickup, CPed* UNUSED_PARAM(pPed)) const
{
	// use frontend audio entity to play the sound. This probably needs changed.
	audSoundInitParams initParams;
	initParams.Position = VEC3V_TO_VECTOR3(pPickup->GetTransform().GetPosition());
	u32 soundHash = AudioRef.GetHash();

	// This is a horrible hack - script have added script triggered sounds for all tiny racers pickup types so 
	// we need to suppress the default pickup sound from triggering in addition to that.
	if(!audNorthAudioEngine::GetMicrophones().IsTinyRacersMicrophoneActive() && !pPickup->ShouldSuppressPickupAudio())
	{
		if(soundHash == g_NullSoundHash || soundHash == ATSTRINGHASH("HUD_DEFAULT_PICK_UP_MASTER", 0xD6C9DE9D))
		{
			CPed* pPed = CGameWorld::FindLocalPlayer();
			if(pPed && pPed->GetIsInVehicle())
			{
				soundHash = ATSTRINGHASH("HUD_DEFAULT_VEH_PICK_UP_MASTER", 0xC6EC8EC8);
			}
			else
			{
				soundHash = ATSTRINGHASH("HUD_DEFAULT_PICK_UP_MASTER", 0xD6C9DE9D);
			}
		}
		g_FrontendAudioEntity.CreateAndPlaySound(soundHash, &initParams);
	}

	// don't record pickup sounds
}

//////////////////////////////////////////////////////////////////////////
// CPickupActionPadShake
//////////////////////////////////////////////////////////////////////////

void CPickupActionPadShake::Apply(CPickup* UNUSED_PARAM(pPickup), CPed* UNUSED_PARAM(pPed)) const
{
	CControlMgr::StartPlayerPadShakeByIntensity(Duration, Intensity);
}

//////////////////////////////////////////////////////////////////////////
// CPickupActionVfx
//////////////////////////////////////////////////////////////////////////

void CPickupActionVfx::Apply(CPickup* UNUSED_PARAM(pPickup), CPed* UNUSED_PARAM(pPed)) const
{

}

//////////////////////////////////////////////////////////////////////////
// CPickupActionGroup
//////////////////////////////////////////////////////////////////////////

void CPickupActionGroup::Apply(CPickup* pPickup, CPed* pPed) const
{
	for (u32 i=0; i<Actions.GetCount(); i++)
	{
		const CPickupActionData* pAction = CPickupDataManager::GetInstance().GetPickupActionData(Actions[i].GetHash());

		if (AssertVerify(pAction))
		{
			pAction->Apply(pPickup, pPed);
		}
	}
}
