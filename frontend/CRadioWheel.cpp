#include "CRadioWheel.h"

#include "rline/socialclub/rlsocialclub.h"
#include "rline/rlpresence.h"
#include "rline/rlgamerinfo.h"

#include "audio/northaudioengine.h"
#include "audio/radioaudioentity.h"
#include "audio/scriptaudioentity.h"
#include "frontend/MobilePhone.h"
#include "frontend/NewHud.h"
#include "scene/world/gameworld.h"
#include "Peds/Ped.h"
#include "system/controlMgr.h"
#include "system/control.h"
#include "system/control_mapping.h"
#include "network/Events/NetworkEventTypes.h"
#include "Network/Live/NetworkTelemetry.h"
#include "Network/Live/livemanager.h"
#include "network/network.h"
#include "vehicles/Metadata/VehicleLayoutInfo.h"
#include "vehicles/Metadata/VehicleSeatInfo.h"

FRONTEND_OPTIMISATIONS()
#if RSG_PC && __BANK
	bool CRadioWheel::sm_bDrawWeightedSelector = false;
#endif

enum RadioConstants
{
	Radio_Retuning = -100,
	Radio_Off = -1,
	Radio_Default = 0,
	Radio_Unknown = 1
};

CRadioWheel::CRadioWheel() : CSelectionWheel(250,150) 
	, m_bIsActive(true)
	, m_bVisible(false)
	, m_bRetuning(false)
	, m_bForcedOpen(false)
	, m_ActivatedAudioSlowMo(false)
	, RADIO_STATION_RETUNE_TIME(500)
	, FORCE_ACTIVE_TIME(1000)
	, m_iQueuedTuning(INVALID_STATION)
	, m_iCurrentTrack(Radio_Default)
	, m_iLastSharedTrack(Radio_Unknown)
#if __DEV
	, m_pszDisabledReason(NULL)
#endif
#if __BANK
	, m_bDbgInvertButton(false)
#endif
#if RSG_PC
	, SELECTOR_WEIGHT(20.0f)
	, MOUSE_MAG_SQRD(0.1f * 0.1f)
	, m_bLastInputWasMouse(false)
#endif
{
	Clear();
}

/////////////////////////////////////////////////////////////////////////////////////
// Helper function to get selected entry from a direction
/////////////////////////////////////////////////////////////////////////////////////

void CRadioWheel::Clear(bool bHardSetTime /* = true */ )
{
	CSelectionWheel::ClearBase(bHardSetTime);
	m_iTimeToTuneRadio.Zero();
	m_iForceOpenTimer.Zero();
	m_iCurrentSelection.SetState(INVALID_STATION);
	m_iQueuedTuning.SetState(INVALID_STATION);
	m_iCurrentTrack.SetState(Radio_Default);
	WIN32PC_ONLY(m_vMouseWeightedSelector.Zero());
}

// fix for unity
#undef IF_SDR
#undef    SDR

#if __DEV
#define    SDR(x) SetDisabledReason( "RADIO:  " x )
#define IF_SDR(x) SetDisabledReason( "RADIO:  " x )
bool CRadioWheel::SetDisabledReason(const char* pszWhy )
{
	m_pszDisabledReason = pszWhy;
	return true;
}
#else
// so it'll deadstrip away
#define IF_SDR(x) 1
#define    SDR(x) do { } while (0)
#endif

bool CRadioWheel::CanPlayerControlRadio(CPed *pPlayerPed)
{
	bool bCanControlRadio = false;
	if( pPlayerPed && pPlayerPed->GetIsInVehicle() )
	{
		CVehicle* pPlayerVeh = pPlayerPed->GetMyVehicle();
		if( !pPlayerVeh )
		{
			SDR("No Vehicle");
		}
		else if (pPlayerVeh->GetVehicleAudioEntity()->IsLockedToRadioStation())
		{
			SDR("Radio locked to station");
		}
		else if (!pPlayerVeh->GetVehicleAudioEntity()->GetHasNormalRadio())
		{
			SDR("No Normal Radio");
		}
		else if ( !pPlayerVeh->GetVehicleAudioEntity()->IsRadioEnabled() )
		{
			SDR("Radio Not Enabled");
		}
		else if (pPlayerPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsDuckingInVehicle))
		{
			SDR( "Ducking In Vehicle" );
		}
		else if( pPlayerVeh->IsDriver(pPlayerPed) )
		{
			bCanControlRadio = true;
		}		
		else if(!CNetwork::IsGameInProgress())
		{			
			if( !pPlayerVeh->GetDriver() || pPlayerVeh->GetDriver()->IsPlayer() )
			{
				bCanControlRadio = true;
			}
			else if( CVehicle::IsTaxiModelId( pPlayerVeh->GetModelId() ) )
			{
				bCanControlRadio = true;
			}
			else
			{
				SDR("Not the driver/taxi passenger");
			}
		}
		else
		{
			// Multiplayer - only front seat passengers are allowed to control the radio
			// ... unless reset flag is specifically set from script (B*1912875)
			const s32 iSeatIndex = pPlayerVeh->GetSeatManager()->GetPedsSeatIndex(pPlayerPed);
			if (pPlayerVeh->IsSeatIndexValid(iSeatIndex))
			{
				const CVehicleSeatInfo* pSeatInfo = pPlayerVeh->GetLayoutInfo()->GetSeatInfo(iSeatIndex);
				if((pSeatInfo && pSeatInfo->GetIsFrontSeat()) || pPlayerPed->GetPedResetFlag(CPED_RESET_FLAG_AllowControlRadioInAnySeatInMP))
				{
					bCanControlRadio = true;
				}
			}

			if(!bCanControlRadio)
			{
				SDR("Not the driver/front seat passenger");
			}
		}

	}
	else
		SDR( "Not In Vehicle" );
	
	return bCanControlRadio;
}

void CRadioWheel::GetInputAxis(const ioValue*& LRAxis, const ioValue*& UDAxis) const
{
	CControl& FrontendControl = CControlMgr::GetMainFrontendControl();
	LRAxis = &FrontendControl.GetRadioWheelLeftRight();
	UDAxis = &FrontendControl.GetRadioWheelUpDown();
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CRadioWheel::Update()
// PURPOSE: updates the radio station hud
// NOTE:	the actual control of this should live somewhere else and I plan to move it
/////////////////////////////////////////////////////////////////////////////////////
void CRadioWheel::Update(bool forceShow, bool isHudHidden)
{
#if !NA_RADIO_ENABLED
	return;
#else

	bool bCanControlRadio = false;
	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
	bCanControlRadio = CanPlayerControlRadio(pPlayerPed);

	// Script can enable radio wheel control outside of a vehicle
	bCanControlRadio |= g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::MobileRadioInGame);
	bCanControlRadio |= g_RadioAudioEntity.IsMobileRadioNormallyPermitted();


	if ( (isHudHidden && !forceShow															&& IF_SDR("Hud Is Hidden"))
		|| (!pPlayerPed																		&& IF_SDR("No Player Ped"))
		|| (pPlayerPed->IsDead()															&& IF_SDR("Player Ped is Dead"))
		|| !bCanControlRadio																// disabled reason set above
		|| (!g_AudioEngine.IsGameInControlOfMusicPlayback()									&& IF_SDR("Not in control of music"))
		|| (g_ScriptAudioEntity.IsFlagSet(audScriptAudioFlags::UserRadioControlDisabled)	&& IF_SDR("Script has disabled UserRadioControlDisabled"))
		|| (CPauseMenu::IsActive()															&& IF_SDR("Pause menu active"))
		|| (CPhoneMgr::IsDisplayed()														&& IF_SDR("Phone active"))
		|| (pPlayerPed->GetControlFromPlayer()->IsInputDisabled(INPUT_VEH_RADIO_WHEEL)		&& IF_SDR("INPUT_VEH_RADIO_WHEEL Disabled"))
		|| (CNewHud::IsWeaponWheelActive()													&& IF_SDR("Weapon Wheel Active"))
		)
	{
		Hide();

		return;
	}

#define ONE_FOR_OFF (1)
	s32 iMaxNumRadioStations = audRadioStation::GetNumUnlockedFavouritedStations()+ONE_FOR_OFF;

	CControl* pControl = &CControlMgr::GetMainPlayerControl(pPlayerPed->GetControlFromPlayer()->IsInputDisabled(INPUT_VEH_RADIO_WHEEL));
	const ioValue& relevantControl = pControl->GetVehicleRadioWheel();

#if RSG_PC
	if(pControl->GetUserRadioNextTrack().IsPressed())
	{
		audRadioStation *station = audRadioStation::FindStation("RADIO_19_USER");
		if(station)
		{
			station->NextTrack();
		}
	}
	else if(pControl->GetUserRadioPrevTrack().IsPressed())
	{
audRadioStation *station = audRadioStation::FindStation("RADIO_19_USER");
		if(station)
		{
			station->PrevTrack();
		}
	}
#endif
	// passing in zero because we don't want the stick opening the wheel while you're tapping
	Vector2 stickPos( 0.0f, 0.0f );
	ButtonState buttState = HandleButton(relevantControl, stickPos
#if __BANK
		, m_bDbgInvertButton 
#endif
		);
	bool bJumpThroughToTapped = false;

	if( !m_bVisible.GetState() || m_bForcedOpen.GetState() )
	{
		if(pControl->GetVehicleNextRadioStation().IsPressed() || pControl->GetVehiclePrevRadioStation().IsPressed())
		{
			buttState = eBS_FirstDown;
			m_iTimeToShowTimer.Zero();
			m_bStickPressed = false;
			bJumpThroughToTapped = true;
		}
	}

	// setting it afterwards
	stickPos.Set(GenerateStickInput());
	bool bIsRadioOn = g_RadioAudioEntity.IsPlayerRadioActive() || g_RadioAudioEntity.IsRetuning();

	switch(buttState)
	{
	case eBS_FirstDown:
		if( bIsRadioOn )
		{
			const audRadioStation *currentStationPendingRetune = g_RadioAudioEntity.GetPlayerRadioStationPendingRetune();
			u32 currentIndex = currentStationPendingRetune ? currentStationPendingRetune->GetStationIndex() : g_OffRadioStation;
			m_iCurrentSelection.SetState( StationIndexToRadioWheelIndex(currentIndex));
		}
		else
			m_iCurrentSelection.SetState( OFF_STATION );

		if( !m_iForceOpenTimer.IsStarted() )
		{
			m_iQueuedTuning.SetState(m_iCurrentSelection);
			SetWeightedStationSelector(m_iQueuedTuning.GetState());
		}

		// kick off the stream request for ZiT stuff
		if( !TheText.HasAdditionalTextLoaded(RADIO_WHEEL_TEXT_SLOT) )
			TheText.RequestAdditionalText("TRACKID", RADIO_WHEEL_TEXT_SLOT);
		SDR("Just hit button");
		if(bJumpThroughToTapped)
		{
			buttState = eBS_Tapped;
			// we want to fall through into the tapped state, so skip the break below
		}
		else
		{
			break;
		}

	case eBS_Tapped:
		m_iForceOpenTimer.Start();
		SDR("Was tapped");
		break;

	case eBS_NotPressed:
		if( !m_iForceOpenTimer.IsStarted() || m_iForceOpenTimer.IsComplete(FORCE_ACTIVE_TIME) )
		{
			Hide();
			SDR("Not pressed");
		}
		else
			SDR("Forced open from time");

		break;

	case eBS_HeldEnough:
		m_iForceOpenTimer.Zero();
#if RSG_PC
		HandleWeightedSelector(stickPos);
#endif // RSG_PC
		SDR("Held enough");
		break;

	case eBS_NotHeldEnough:
	default:
		// don't really care about these
		SDR("Not Held Enough");
		break;
	}

#if RSG_ORBIS
	bool bTouchpadSwipe = false;
	if ( buttState == eBS_NotPressed && 
		!pControl->IsInputDisabled(INPUT_VEH_RADIO_WHEEL) && // Use INPUT_VEH_RADIO_WHEEL to detect if we can use weapon wheel as other can be unmapped (always be disabled)
		(CControlMgr::GetPlayerPad()->GetTouchPadGesture().IsSwipe(CTouchPadGesture::UP) ||
		 CControlMgr::GetPlayerPad()->GetTouchPadGesture().IsSwipe(CTouchPadGesture::DOWN)) )
	{
		// -----   code from eBS_FirstDown state.   -----
		if( bIsRadioOn )
		{
			const audRadioStation *currentStationPendingRetune = g_RadioAudioEntity.GetPlayerRadioStationPendingRetune();
			u32 currentIndex = currentStationPendingRetune ? currentStationPendingRetune->GetStationIndex() : g_OffRadioStation;
			m_iCurrentSelection.SetState( StationIndexToRadioWheelIndex(currentIndex));
		}
		else
			m_iCurrentSelection.SetState( OFF_STATION );

		if( !m_iForceOpenTimer.IsStarted() )
			m_iQueuedTuning.SetState(m_iCurrentSelection);

		// kick off the stream request for ZiT stuff
		if( !TheText.HasAdditionalTextLoaded(RADIO_WHEEL_TEXT_SLOT) )
			TheText.RequestAdditionalText("TRACKID", RADIO_WHEEL_TEXT_SLOT);

		// -----   code from eBS_Tapped state.   -----
		m_iForceOpenTimer.Start();
		SDR("TouchPad Swipe");
		bTouchpadSwipe = true;
	}
#endif // RSG_ORBIS

	if( m_iForceOpenTimer.IsStarted() || buttState == eBS_HeldEnough )
	{
		int newSelection = m_iQueuedTuning.GetState();
		if( buttState == eBS_HeldEnough )
		{
			// set the wheel selection inputs to have exclusive control (needed for southpaw controls). NOTE: we actually use the weapon wheel inputs.
			pControl->SetInputExclusive(INPUT_RADIO_WHEEL_LR);
			pControl->SetInputExclusive(INPUT_RADIO_WHEEL_UD);

			if(m_ActivatedAudioSlowMo.SetState(true))
			{
				g_RadioAudioEntity.PlayWheelShowSound();
				audNorthAudioEngine::ActivateSlowMoMode(ATSTRINGHASH("SLOWMO_RADIOWHEEL", 0xD939CA30));
			}
			SetTargetTimeWarp(TW_Slow);

			if (pControl->GetVehiclePrevRadioStation().IsPressed())
			{
				newSelection = (m_iQueuedTuning.GetState() - 1 + iMaxNumRadioStations) % iMaxNumRadioStations;
				SetWeightedStationSelector(newSelection);
			}
			else if (pControl->GetVehicleNextRadioStation().IsPressed())
			{
				newSelection = (m_iQueuedTuning.GetState() + 1) % iMaxNumRadioStations;
				SetWeightedStationSelector(newSelection);
			}
			else
			{
				// need to apply some math so the OFF station is always at the bottom
				float fBaseOffset = (OFF_STATION * (TWO_PI/iMaxNumRadioStations)) - PI;
				newSelection = HandleStick(stickPos, iMaxNumRadioStations, newSelection, fBaseOffset );
			}

		}
		else 
		{
			SetTargetTimeWarp(TW_Normal);
			if(m_ActivatedAudioSlowMo.SetState(false))
			{
				g_RadioAudioEntity.PlayWheelHideSound();
				audNorthAudioEngine::DeactivateSlowMoMode(ATSTRINGHASH("SLOWMO_RADIOWHEEL", 0xD939CA30));
			}
			if( buttState == eBS_Tapped )
			{
				if (pControl->GetVehiclePrevRadioStation().IsPressed())
				{
					newSelection = (m_iQueuedTuning.GetState() - 1 + iMaxNumRadioStations) % iMaxNumRadioStations;
					SetWeightedStationSelector(newSelection);
				}
				else
				{
					newSelection = (m_iQueuedTuning.GetState() + 1) % iMaxNumRadioStations;
					SetWeightedStationSelector(newSelection);
				}
			}
		#if RSG_ORBIS
			else if (bTouchpadSwipe)
			{
				//B*2133435 - allow "Off" station to be selected from swipes
				TUNE_GROUP_BOOL(RADIO_WHEEL, bSkipOffStationOnSwipe, false); 
				if (CControlMgr::GetPlayerPad()->GetTouchPadGesture().IsSwipe(CTouchPadGesture::UP))
				{
					newSelection = (m_iQueuedTuning.GetState() + 1) % iMaxNumRadioStations;
					if (newSelection == OFF_STATION && bSkipOffStationOnSwipe)
					{
						// left/right swipe skips "off" option
						newSelection = (newSelection + 1) % iMaxNumRadioStations;
					}
				}
				else if (CControlMgr::GetPlayerPad()->GetTouchPadGesture().IsSwipe(CTouchPadGesture::DOWN))
				{
					newSelection = m_iQueuedTuning.GetState()-1;
					if (newSelection == OFF_STATION && bSkipOffStationOnSwipe)
					{
						// left/right swipe skips "off" option
						newSelection -= 1;
					}
					if (newSelection < 0)
					{
						newSelection = iMaxNumRadioStations-1;
					}
				}
			}
		#endif // RSG_ORBIS
		}

		bool bForceUpdate = false;

		if( m_bVisible.SetState(true) || m_iStationCount.SetState(iMaxNumRadioStations))
		{
			m_iStationCount.SetState(iMaxNumRadioStations);
			enum { kMaxRadioStationsPerCall = MAX_NUM_PARAMS_IN_SCALEFORM_METHOD - 2 };
			if( CHudTools::BeginHudScaleformMethod(NEW_HUD_RADIO_STATIONS, "SET_RADIO_STATIONS") )
			{
				for (s32 iNum = 0; iNum < Min<s32>(kMaxRadioStationsPerCall, iMaxNumRadioStations); iNum++)
				{
					CScaleformMgr::AddParamString( (iNum == OFF_STATION) ? "MO_RADOFF" : audRadioStation::GetStation(RadioWheelIndexToStationIndex(iNum))->GetName());
				}

				CScaleformMgr::EndMethod();
			}

			if(iMaxNumRadioStations > kMaxRadioStationsPerCall)
			{
				if( CHudTools::BeginHudScaleformMethod(NEW_HUD_RADIO_STATIONS, "SET_ADDITIONAL_RADIO_STATIONS") )
				{
					for (s32 iNum = kMaxRadioStationsPerCall; iNum < iMaxNumRadioStations; iNum++)
					{
						CScaleformMgr::AddParamString( (iNum == OFF_STATION) ? "MO_RADOFF" : audRadioStation::GetStation(RadioWheelIndexToStationIndex(iNum))->GetName());
					}

					CScaleformMgr::EndMethod();
				}
			}

			if( CHudTools::BeginHudScaleformMethod(NEW_HUD_RADIO_STATIONS, "DRAW_RADIO_STATIONS") )
			{
				CScaleformMgr::EndMethod();
			}

			bForceUpdate = true;
		}

		if( m_iQueuedTuning.SetState( newSelection ) || bForceUpdate || m_bForcedOpen.SetState(m_iForceOpenTimer.IsStarted()))
		{
			m_bForcedOpen.SetState(m_iForceOpenTimer.IsStarted());
			if( m_bForcedOpen.GetState() )
			{
				if( CHudTools::BeginHudScaleformMethod(NEW_HUD_RADIO_STATIONS, "QUICK_SELECT_RADIO_STATION") )
				{
					CScaleformMgr::AddParamInt( m_iQueuedTuning.GetState() );
					CScaleformMgr::EndMethod();
				}
			}
			else
			{
				SELECT_RADIO_STATION(m_iQueuedTuning.GetState());
			}

			// Don't trigger the retune sound when showing the UI; only when selecting a new station
			if(!bForceUpdate || m_iForceOpenTimer.IsStarted())
			{
				g_RadioAudioEntity.PlayStationSelectSound(false);
			}
			if( !m_iForceOpenTimer.IsStarted() )
			{
				m_iTimeToTuneRadio.Start();
			}
		}
	}


	// check if we can retune:
	if (m_iQueuedTuning != INVALID_STATION && !g_RadioAudioEntity.IsRetuning())
	{
		const audRadioStation *currentStationPendingRetune = g_RadioAudioEntity.GetPlayerRadioStationPendingRetune();
		const u32 currentIndex = currentStationPendingRetune ? currentStationPendingRetune->GetStationIndex() : g_OffRadioStation;
		if (m_iQueuedTuning != StationIndexToRadioWheelIndex(currentIndex))
		{
			if( m_iTimeToTuneRadio.IsStarted() && m_iTimeToTuneRadio.IsComplete(RADIO_STATION_RETUNE_TIME) && m_iCurrentSelection.SetState(m_iQueuedTuning) )
			{
				m_iTimeToTuneRadio.Zero();
				RetuneStation(m_iCurrentSelection.GetState());
			}
		}
	}


	if( m_bVisible.GetState() && TheText.HasAdditionalTextLoaded(RADIO_WHEEL_TEXT_SLOT) )
	{
		bool bCanShare = false;
		int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
		bool isUserTrack = false;
		u32 playTime = g_RadioAudioEntity.GetPlayingTrackPlayTimeByStationIndex( RadioWheelIndexToStationIndex(m_iQueuedTuning.GetState()) , isUserTrack );
		if(!m_iForceOpenTimer.IsStarted() && RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex) && rlSocialClub::IsConnectedToSocialClub(localGamerIndex))
		{
			//We can only share content if our privileges permit it.
			bCanShare = ContentPrivilegesAreOk() && !isUserTrack;
		}

		if( playTime != 0 && m_iCurrentTrack.SetState( g_RadioAudioEntity.GetPlayingTrackTextIdByStationIndex( RadioWheelIndexToStationIndex(m_iQueuedTuning.GetState()), isUserTrack ) ) )
		{
			m_bRetuning.SetState(false);
			if( CHudTools::BeginHudScaleformMethod(NEW_HUD_RADIO_STATIONS, "SET_CURRENTLY_PLAYING") )
			{
				// where CELL_195 means "Unknown"
#define FILTER_UNKNOWN(inVal) ( inVal&&TheText.DoesTextLabelExist(inVal)?TheText.Get(inVal):"")

#if RSG_PC
				if(isUserTrack)
				{
					CScaleformMgr::AddParamString(audRadioStation::GetUserRadioTrackManager()->GetTrackArtist(audRadioTrack::GetUserTrackIndexFromTextId(m_iCurrentTrack.GetState())));
					CScaleformMgr::AddParamString(audRadioStation::GetUserRadioTrackManager()->GetTrackTitle(audRadioTrack::GetUserTrackIndexFromTextId(m_iCurrentTrack.GetState())));
					CScaleformMgr::AddParamBool(false); // shareable, can't share nothing
					bCanShare = false;
				}
			else
#endif // RSG_PC
				if( m_iCurrentTrack != Radio_Unknown )
				{
					char artistID[16];
					char trackID[16];

					formatf(artistID,"%dA", m_iCurrentTrack.GetState());
					formatf(trackID,"%dS",  m_iCurrentTrack.GetState());
					CScaleformMgr::AddParamString(FILTER_UNKNOWN(artistID)); // artist name
					CScaleformMgr::AddParamString(FILTER_UNKNOWN(trackID)); // track name
					CScaleformMgr::AddParamBool(m_iCurrentTrack != m_iLastSharedTrack && bCanShare);
				}
				else
				{
					CScaleformMgr::AddParamString(""); // artist name
					CScaleformMgr::AddParamString(""); // track name
					CScaleformMgr::AddParamBool(false); // shareable, can't share nothing
				}
				CScaleformMgr::EndMethod();
			}
		}
#if __DEV
		else
		{
			audRadioStation* radioStation = audRadioStation::GetStation(RadioWheelIndexToStationIndex(m_iQueuedTuning.GetState()));

			if(radioStation && radioStation->IsFrozen())
			{
				if( CHudTools::BeginHudScaleformMethod(NEW_HUD_RADIO_STATIONS, "SET_CURRENTLY_PLAYING") )
				{
					CScaleformMgr::AddParamString("(DEV ONLY DEBUG)");
					CScaleformMgr::AddParamString("STATION FROZEN");
					CScaleformMgr::AddParamBool(false);
					CScaleformMgr::EndMethod();
				}
			}
		}
#endif

		// check if player wants to Share
		if( bCanShare && m_iCurrentTrack != Radio_Unknown 
#if RSG_PC
			&& pControl->GetVehicleHandBrake().IsPressed() 
#else
			&& pControl->GetFrontendX().IsPressed() 
#endif
			&& m_iLastSharedTrack.SetState( m_iCurrentTrack.GetState() ) )
		{
			const unsigned trackId = static_cast<unsigned>(m_iLastSharedTrack.GetState());
								
			CNetworkTelemetry::AudTrackTagged( trackId );
			CHudTools::CallHudScaleformMethod(NEW_HUD_RADIO_STATIONS, "SET_AS_SHARED");

			int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
			if(RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
			{
				char trackIdString[32];
				formatf(trackIdString, "%d", trackId);
				rlSocialClub::PostUserFeedActivity(localGamerIndex, "SHARE_SPOTIFY_SONG", 0, trackIdString, NULL);
			}
		}
	}
#endif
}

bool CRadioWheel::ContentPrivilegesAreOk( ) const
{
	bool privilegesOk = true;

	if(!CLiveManager::GetSocialNetworkingSharingPrivileges())
	{
		privilegesOk = false;
	}

	if (!CLiveManager::CheckUserContentPrivileges())
	{
		privilegesOk = false;
	}

#if __XENON

#if !__NO_OUTPUT
	static bool s_privilegesOk = true;
#endif //!__NO_OUTPUT

	//On xbox, we need to check all the profiles that are since in.
	for(int index = 0; index < RL_MAX_LOCAL_GAMERS; index++)
	{
		if (rlPresence::IsSignedIn(index) && !rlGamerInfo::HasUserContentPrivileges(index))
		{
			privilegesOk = false;
		}

#if !__NO_OUTPUT
		if (s_privilegesOk && !privilegesOk)
		{
			gnetDisplay("CRadioWheel::ContentPrivilegesAreOk( ) - Song sharing blocked because user doesn't have CONTENT PRIVILEGE.");
			s_privilegesOk = false;
		}
#endif //!__NO_OUTPUT
	}

#if !__NO_OUTPUT
	if (!s_privilegesOk && privilegesOk)
	{
		gnetDisplay("CRadioWheel::ContentPrivilegesAreOk( ) - Song sharing allowed because user has CONTENT PRIVILEGE.");
		s_privilegesOk = true;
	}
#endif // !__NO_OUTPUT
#endif // __XENON

	return privilegesOk;
}

void CRadioWheel::Hide()
{
	if(m_ActivatedAudioSlowMo.SetState(false))
	{
		g_RadioAudioEntity.PlayWheelHideSound();
		audNorthAudioEngine::DeactivateSlowMoMode(ATSTRINGHASH("SLOWMO_RADIOWHEEL", 0xD939CA30));
	}

	SetTargetTimeWarp(TW_Normal);

	if (m_bVisible.SetState(false))
	{
		CHudTools::CallHudScaleformMethod(NEW_HUD_RADIO_STATIONS, "HIDE");

		// check for queued, but not timed out station change
		// ie, the user pushed the wheel before our retune delay kicked in
		if( m_iQueuedTuning != m_iCurrentSelection )
			RetuneStation(m_iQueuedTuning.GetState());
		Clear(false);

//		if( TheText.HasAdditionalTextLoaded(RADIO_WHEEL_TEXT_SLOT) || TheText.IsRequestingAdditionalText(RADIO_WHEEL_TEXT_SLOT))
//			TheText.FreeTextSlot(RADIO_WHEEL_TEXT_SLOT,false);
	}

}

void CRadioWheel::SetWeightedStationSelector(int WIN32PC_ONLY(newStationIndex))
{
#if RSG_PC
	const s32 iMaxNumRadioStations = audRadioStation::GetNumUnlockedFavouritedStations()+ONE_FOR_OFF;

	float fBaseOffset = (OFF_STATION * (TWO_PI/iMaxNumRadioStations)) - PI;
	float fAngle = TWO_PI * (float(newStationIndex)/iMaxNumRadioStations) - fBaseOffset;
	m_vMouseWeightedSelector.Set( Sinf(fAngle), -Cosf(fAngle));
#endif
}

#if RSG_PC
void CRadioWheel::HandleWeightedSelector(Vector2& ioStickPos)
{
	const ioValue* pUDAxis = nullptr;
	const ioValue* pLRAxis = nullptr;

	GetInputAxis(pLRAxis, pUDAxis);
	const s32 LRDevice = pLRAxis->GetSource().m_DeviceIndex;
	const s32 UDDevice = pUDAxis->GetSource().m_DeviceIndex;

	// due to the remappability of some controls, we can't tell if they touched the mouse recently or not, and how
	// sometimes, the noise of the stick will cause the last device to be the controller, but then we Deadzoned it to zero, so we gotta watch out for that case
	// and other times, if you don't touch the controller nor the mouse, it will revert to an Unknown device for a time.
	switch(LRDevice)
	{
	case ioSource::IOMD_KEYBOARD_MOUSE:
	case ioSource::IOMD_GAME:
		m_bLastInputWasMouse = (UDDevice == ioSource::IOMD_KEYBOARD_MOUSE || UDDevice == ioSource::IOMD_GAME || ioStickPos.y == 0.0f); // moused or deadzoned
		break;
	case ioSource::IOMD_UNKNOWN:
		if( UDDevice != ioSource::IOMD_UNKNOWN && UDDevice != ioSource::IOMD_KEYBOARD_MOUSE && UDDevice != ioSource::IOMD_GAME )
			m_bLastInputWasMouse = false;
		break;

	default:
		if( UDDevice != ioSource::IOMD_UNKNOWN && UDDevice != ioSource::IOMD_KEYBOARD_MOUSE && UDDevice != ioSource::IOMD_GAME
			&& ioStickPos.IsNonZero() )
			m_bLastInputWasMouse = false; 
		break;
		// disable for any other source.
	}

	if( m_bLastInputWasMouse )
	{
		// we want to subtly suggest that the user form a circle with their inputs to rotate the angle
		// thus, we've got some weight on this that we're forcing the user to deal with.
		//if( stickPos.Mag2() >= MOUSE_MAG_SQRD)
		{
			static BankFloat minLerp = 0.8f; // on really low framerates still have SOME degree of history
			m_vMouseWeightedSelector = Lerp(rage::Min(SELECTOR_WEIGHT * fwTimer::GetSystemTimeStep(), minLerp), m_vMouseWeightedSelector, ioStickPos);
			m_vMouseWeightedSelector.Normalize();
		}

#if __BANK
		if( sm_bDrawWeightedSelector )
		{
			const Vector2 origin(CNewHud::GetHudComponentPosition(NEW_HUD_RADIO_STATIONS) + Vector2(0.5f, 0.5f));
			Vector2 scaledCompensator, scaledInput;
			scaledCompensator.Scale(m_vMouseWeightedSelector, 0.25f);
			scaledInput.Scale(ioStickPos, 0.25f);

			scaledCompensator.x /= CHudTools::GetAspectRatio();
			scaledInput.x /= CHudTools::GetAspectRatio();

			scaledCompensator.Add(origin);
			scaledInput.Add(origin);
			grcDebugDraw::Line(origin, scaledCompensator,	Color_MediumBlue);
			grcDebugDraw::Line(origin, scaledInput,			Color_burlywood);
		}
#endif // __BANK

		ioStickPos = m_vMouseWeightedSelector;
	}
}

#endif // RSG_PC

void CRadioWheel::RetuneStation(int newStationId)
{
	if( newStationId == INVALID_STATION )
		return;

	if( newStationId == OFF_STATION )
	{
		audDisplayf("Radio wheel switching off radio");
		g_RadioAudioEntity.SwitchOffRadio();
		g_RadioAudioEntity.PlayRadioOnOffSound();
	}
	else
	{
		audDisplayf("Radio wheel retuning to station %d", newStationId);
		g_RadioAudioEntity.RetuneToStation(audRadioStation::GetStation(RadioWheelIndexToStationIndex(newStationId)));
		CNetworkTelemetry::RetuneToStation(RadioWheelIndexToStationIndex(newStationId));
	}
	CPauseMenu::SetMenuPreference(PREF_RADIO_STATION, RadioWheelIndexToStationIndex(newStationId));

#if NA_RADIO_ENABLED
	if (NetworkInterface::IsNetworkOpen())
	{
		//Trigger a change radio station network event from the non-owner / passenger to the owner / driver.
		CVehicle* playerVehicle = CGameWorld::FindLocalPlayerVehicle();
		if (playerVehicle && playerVehicle->IsNetworkClone())
		{
			u8 adjustedStationId = (u8) newStationId;
			if (newStationId == OFF_STATION)
				adjustedStationId = 255;
			else
				adjustedStationId = (u8)(RadioWheelIndexToStationIndex(newStationId));

			CChangeRadioStationEvent::Trigger(playerVehicle,(u8)adjustedStationId);
		}
		else if(playerVehicle && playerVehicle->GetDriver() == FindPlayerPed())
		{
			g_RadioAudioEntity.SetMPDriverTriggeredRadioChange();
		}
	}
#endif	//NA_RADIO_ENABLED
}

int CRadioWheel::RadioWheelIndexToStationIndex(int iRadioWheelIndex)
{
	if(iRadioWheelIndex == OFF_STATION)
	{
		// As far as everyone else is concerned, the OFF_STATION maps to -1 radio station index
		return -ONE_FOR_OFF;
	}

	return (iRadioWheelIndex > OFF_STATION) ? iRadioWheelIndex-ONE_FOR_OFF:iRadioWheelIndex;
}

int CRadioWheel::StationIndexToRadioWheelIndex(int iStationIndex)
{
	if(iStationIndex == -ONE_FOR_OFF)
	{
		return OFF_STATION;
	}

	return (iStationIndex >= OFF_STATION) ? iStationIndex+ONE_FOR_OFF : iStationIndex;
}

void CRadioWheel::SELECT_RADIO_STATION( int newSelection )
{
	if( CHudTools::BeginHudScaleformMethod(NEW_HUD_RADIO_STATIONS, "SELECT_RADIO_STATION") )
	{
		CScaleformMgr::AddParamInt(newSelection); // radio index
		CScaleformMgr::EndMethod();
	}
}


#if __BANK
void CRadioWheel::CreateBankWidgets(bkBank* pOwningBank)
{
	bkGroup* pGroup = pOwningBank->AddGroup("Radio Wheel");
	CSelectionWheel::CreateBankWidgets(pGroup);
	pGroup->AddToggle("Invert Button Input", &m_bDbgInvertButton);
#if RSG_PC
	pGroup->AddToggle("Draw Weighted Selector", &sm_bDrawWeightedSelector);
	pGroup->AddSlider("Selector Weight percent/second", &SELECTOR_WEIGHT, 0.0f, 10000.0f, 0.01f);
	pGroup->AddSlider("Mouse Mag Sqrd", &MOUSE_MAG_SQRD, 0.0f, 1.0f, 0.01f);
#endif
	pGroup->AddSlider("Radio Retune Time", &RADIO_STATION_RETUNE_TIME, 0, 10000, 33);
	pGroup->AddSlider("Force Active Time", &FORCE_ACTIVE_TIME, 0, 10000, 33);
	pGroup->AddSlider("Current Station", &(m_iCurrentSelection.GetStateRef()), INVALID_STATION, 100,1);
	pGroup->AddSlider("Queued Station", &(m_iQueuedTuning.GetStateRef()), INVALID_STATION, 100,1);

	pGroup->AddSlider("Current Track", &(m_iCurrentTrack.GetStateRef()), INVALID_STATION, 10000,1);
	pGroup->AddSlider("Last Shared Track", &(m_iCurrentTrack.GetStateRef()), INVALID_STATION, 10000,1);
}
#endif	//__BANK

