/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : MultiplayerGamerTagHud.cpp
// PURPOSE : manages the Scaleform multiplayer gamer tag hud
// AUTHOR  : Derek Payne
// STARTED : 08/03/2012
//
/////////////////////////////////////////////////////////////////////////////////

// fw:
#include "fwnet/nettypes.h"
#include "fwsys/gameskeleton.h"

// game:
#include "Frontend/MultiplayerGamerTagHud.h"

#include "camera/CamInterface.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/viewports/Viewport.h"
#include "Frontend/hud_colour.h"
#include "Peds/ped.h"
#include "Network/Live/livemanager.h"
#include "Network/Live/NetworkClan.h"
#include "network/NetworkInterface.h"
#include "network/Players/NetGamePlayer.h"
#include "Frontend/HudTools.h"
#include "Frontend/Scaleform/ScaleFormMgr.h"
#include "Frontend/PauseMenu.h"
#include "frontend/ui_channel.h"
#include "renderer/PostProcessFXHelper.h"
#include "scene/world/GameWorld.h"
#include "script/script_hud.h"
#include "Vfx/VfxHelper.h"

//FRONTEND_OPTIMISATIONS();
//OPTIMISATIONS_OFF();

#define GAMER_INFO_FILENAME "mp_gamer_info"
#define GAMERTAG_CENTER_OBJECT "GAMERTAG_CENTER"
#define GAMERTAG_LEFT_OBJECT "GAMERTAG_LEFT"

#define HEALTH_ARMOUR_NAME "HealthArmour"
#define HEALTH_NO_ARMOUR_NAME "HealthNoArmour"
#if ARCHIVED_SUMMER_CONTENT_ENABLED
#define GAMER_INFO_FILENAME_CNC "CNC_GAMER_INFO"
#define HEALTH_CNC_NO_ARMOUR_NAME "HealthCNC"
#define HEALTH_CNC_NO_ARMOUR_NO_ENDURANCE_NAME "HealthNoEnduranceCNC"
#endif
#define MOVIE_DEPTH_ROOT			(1)

#if !__FINAL
PARAM(enableFarGamerTagDistance, "Makes gamer tags work at superfar distances.");
#endif

float CMultiplayerGamerTagHud::sm_fVerticalOffset = 0.0f;
bank_float CMultiplayerGamerTagHud::sm_iconSpacing = 2.0f;
bank_float CMultiplayerGamerTagHud::sm_afterGamerNameSpacing = 4.0f;
bank_float CMultiplayerGamerTagHud::sm_aboveGamerNameSpacing = -19.0f;
bank_float CMultiplayerGamerTagHud::sm_minScaleSize = 0.8f;
bank_float CMultiplayerGamerTagHud::sm_maxScaleSize = 1.0f;
bank_float CMultiplayerGamerTagHud::sm_nearScalingDistance = 10.0f;
bank_float CMultiplayerGamerTagHud::sm_farScalingDistance = 50.0f;
bank_float CMultiplayerGamerTagHud::sm_maxVisibleDistance = 100.0f;
bank_float CMultiplayerGamerTagHud::sm_maxVisibleSpectatingDistance = 200.0f;
bank_float CMultiplayerGamerTagHud::sm_leftIconXOffset = 50.0f;
bank_float CMultiplayerGamerTagHud::sm_4x3Scaler = 120.0f;
bank_float CMultiplayerGamerTagHud::sm_bikeOffset = -.08f;
bank_float CMultiplayerGamerTagHud::sm_defaultVehicleOffset = -.5f;
bank_float CMultiplayerGamerTagHud::sm_vehicleXSpacing = 0.035f;
bank_float CMultiplayerGamerTagHud::sm_vehicleYSpacing = 0.023f;
bank_s32 CMultiplayerGamerTagHud::sm_minSeatCountToGridNames = 2;

bool CMultiplayerGamerTagHud::sm_staticDataInited = false;
float CMultiplayerGamerTagHud::sm_tagWidths[MAX_MP_TAGS] = {0.0f};
eMP_TAG CMultiplayerGamerTagHud::sm_tagOrder[MAX_MP_TAGS] = {MP_TAG_GAMER_NAME};

struct GamerDepthInfo
{
	float m_depth;
	s16 m_playerIndex;

	GamerDepthInfo(): m_playerIndex(-1), m_depth(-1.0f) {}
	GamerDepthInfo(float depth, s16 playerIndex): m_playerIndex(playerIndex), m_depth(depth) {}

	static int DepthCompare(const GamerDepthInfo* info1, const GamerDepthInfo* info2)
	{
		return (info1->m_depth < info2->m_depth) ? 1 : -1;
	}
};

void sGamerTag::Reset()
{
	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on UT
	{
		uiAssertf(0, "sGamerTag::Reset can only be called on the UpdateThread!");
		return;
	}

	m_rtDepth = -1;
	m_reinitCrewTag = false;

	memset(m_playerName, 0, RL_MAX_DISPLAY_NAME_BUF_SIZE);
	memset(m_refName, 0, MAX_REF_NAME_SIZE);
	memset(m_bigtext, 0, MAX_BIG_TEXT_SIZE);

	memset(m_crewTag, 0, NetworkClan::FORMATTED_CLAN_TAG_LEN);
	NetworkClan::GetUIFormattedClanTag(false, true, "", -1, CHudColour::GetRGBA(HUD_COLOUR_WHITE), m_crewTag, NetworkClan::FORMATTED_CLAN_TAG_LEN);

	m_gamerTagWidth = 0.0f;

	m_utPhysical = NULL;
	m_reinitPositions = true;
	m_reinitPosDueToAlpha = false;
	m_reinitBigText = false;
	m_useVehicleHealth = false;
	BANK_ONLY(m_debugCloneTagFromPedForVehicle = false;)
	m_initPending = false;
	m_isActive = false;
	m_needsPositionUpdate = true;
	m_activeReinit.GetWriteBuf() = false;
	m_removeGfxValuePending = false;
	m_destroyPending = false;
	m_renderWhenPaused = false;
	m_hiddenDueToBigVehicle = false;
	m_showEvenInBigVehicles = false;

	m_bUsePointHealth = false;
	m_iPoints = -1;
	m_iMaxPoints = -1;

	m_TagReinitFlags.GetWriteBuf().Reset();

	for (s32 j = 0; j < MAX_MP_TAGS; j++)
	{
		m_updateAlpha[j] = false;
		m_visibleFlag[j] = false;
		m_value[j] = -1;
		m_colour[j] = HUD_COLOUR_INVALID;
		m_alpha[j] = 0;
	}

	m_healthBarColourDirty.GetWriteBuf() = true;
	m_healthBarColour = HUD_COLOUR_WHITE;

	m_visible.GetWriteBuf() = false;
	m_scale.GetWriteBuf() = 1.0f;
	m_screenPos.GetWriteBuf() = Vector2(-1.0f,-1.0f);
	m_playerPos.GetWriteBuf() = Vector3(-1.0f,-1.0f,-1.0f);

	m_healthInfo.GetWriteBuf().Reset();
	m_previousHealthInfo.Reset();

	if (m_asGamerTagContainerMc.IsDefined())
	{
		m_asGamerTagContainerMc.Invoke("removeMovieClip");
		m_asGamerTagContainerMc.SetUndefined();
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// __BANK - all debug stuff shown appear under here
/////////////////////////////////////////////////////////////////////////////////////
#if __BANK
static s32 iDebugGamerIndex = MAX_NUM_PHYSICAL_PLAYERS;
static s32 iDebugTagId = -1;
static s32 iDebugTagValue = 0;
static int iDebugHealthPercent = -1;
static int iDebugArmorPercent = -1;
static int iDebugCurrentPoints = -1;
static int iDebugMaxPoints = -1;
static s32 iDebugTagAlpha = 255;
static u16 uDebugCurrentFakePassengerSeatIndex = 0;
static u16 uDebugNumFakeVehiclePassengers = 0;
static bool bDebugFillVehicleWithGamerTags = false;
static bool bDebugUseVehicleHealth = false;
static bool bDebugUsePointHealth = false;
static bool bDebugIsCrewTagPrivate = true;
static bool bDebugCrewTagContainsRockstar = true;
static bool bDebugAddToPeds = false;
static s32 iDebugCrewRank = 0;
static u8 uDebugCrewR = 255;
static u8 uDebugCrewG = 255;
static u8 uDebugCrewB = 255;
static char cDebugGamerTag[RL_MAX_DISPLAY_NAME_BUF_SIZE] = {0};
static char cDebugClanTag[RL_CLAN_TAG_MAX_CHARS] = {0};
static char cDebugBigText[MAX_BIG_TEXT_SIZE] = {0};
static char cDebugTagName[100] = {0};
static s32 iDebugVehicleUsageCount = -1;
static float fDebugVerticalOffset = 0.0f;

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMultiplayerGamerTagHud::InitWidgets()
// PURPOSE: inits the ui bank widget and "Create" button
/////////////////////////////////////////////////////////////////////////////////////
void CMultiplayerGamerTagHud::InitWidgets()
{
	bkBank *pBank = BANKMGR.FindBank(UI_DEBUG_BANK_NAME);

	formatf(cDebugGamerTag, RL_MAX_DISPLAY_NAME_BUF_SIZE, "xxx xxx C xxx xx");
	formatf(cDebugClanTag, RL_CLAN_TAG_MAX_CHARS, "Test");

	if (!pBank)  // create the bank if not found
	{
		pBank = &BANKMGR.CreateBank(UI_DEBUG_BANK_NAME);
	}

	if (pBank)
	{
		pBank->AddButton("Create MP Gamer Tag widgets", &CMultiplayerGamerTagHud::CreateBankWidgets);
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMultiplayerGamerTagHud::CreateBankWidgets()
// PURPOSE: creates the bank widget
/////////////////////////////////////////////////////////////////////////////////////
void CMultiplayerGamerTagHud::CreateBankWidgets()
{
	static bool bBankCreated = false;

	bkBank *bank = BANKMGR.FindBank(UI_DEBUG_BANK_NAME);

	if ((!bBankCreated) && (bank))
	{
		UpdateDebugTagSelected();

		bank->PushGroup("MP Gamer Tags");

		bank->AddToggle("Give Random peds tags", &bDebugAddToPeds);
		bank->AddToggle("Tag Widths Inited", &sm_staticDataInited);
		bank->AddSlider("Max Visible Distance", &CMultiplayerGamerTagHud::sm_maxVisibleDistance, 0.0f, 100000.0f, 0.1f);
		bank->AddSlider("Max Spectating Visible Distance", &CMultiplayerGamerTagHud::sm_maxVisibleSpectatingDistance, 0.0f, 100000.0f, 0.1f);
		bank->AddSlider("Near Scaling Distance", &CMultiplayerGamerTagHud::sm_nearScalingDistance, 0.0f, 200.0f, 0.1f);
		bank->AddSlider("Far Scaling Distance", &CMultiplayerGamerTagHud::sm_farScalingDistance, 0.0f, 200.0f, 0.1f);
		bank->AddSlider("Min Scale Size", &CMultiplayerGamerTagHud::sm_minScaleSize, 0.0f, 2.0f, 0.01f);
		bank->AddSlider("Max Scale Size", &CMultiplayerGamerTagHud::sm_maxScaleSize, 0.0f, 2.0f, 0.01f);
		bank->AddSlider("Left Icons X Offset", &CMultiplayerGamerTagHud::sm_leftIconXOffset, -100.0f, 100.0f, 1.0f);
		bank->AddSlider("4x3 Scaler", &CMultiplayerGamerTagHud::sm_4x3Scaler, 0.0f, 1000.0f, 1.0f);
		bank->AddSlider("Default Vehicle Offset", &CMultiplayerGamerTagHud::sm_defaultVehicleOffset, -1.0f, 1.0f, 0.001f);
		bank->AddSlider("Bike Offset", &CMultiplayerGamerTagHud::sm_bikeOffset, -1.0f, 1.0f, 0.001f);
		bank->AddSlider("Min Seat Count To Grid Names", &CMultiplayerGamerTagHud::sm_minSeatCountToGridNames, 1, 10, 1);
		bank->AddSlider("Vehicle X Spacing", &CMultiplayerGamerTagHud::sm_vehicleXSpacing, -1.0f, 1.0f, 0.001f);
		bank->AddSlider("Vehicle Y Spacing", &CMultiplayerGamerTagHud::sm_vehicleYSpacing, -1.0f, 1.0f, 0.001f);
		bank->AddSlider("Vehicle Usage Count", &iDebugVehicleUsageCount, -1, MAX_PLAYERS_WITH_GAMER_TAGS-1, 1);
		bank->AddSlider("Icon Spacing", &CMultiplayerGamerTagHud::sm_iconSpacing, -50.0f, 50.0f, 0.01f);
		bank->AddSlider("After Gamer Name Spacing", &CMultiplayerGamerTagHud::sm_afterGamerNameSpacing, -50.0f, 50.0f, 0.01f);
		bank->AddSlider("Above Gamer Name Spacing", &CMultiplayerGamerTagHud::sm_aboveGamerNameSpacing, -50.0f, 50.0f, 0.01f);

		bank->AddSeparator();

		bank->AddButton("Create Tag for player", &CMultiplayerGamerTagHud::CreateDebugTagForPlayer);
		bank->AddButton("Remove Tag for player", &CMultiplayerGamerTagHud::RemoveDebugTagForPlayer);
		bank->AddButton("Create Tag for vehicle", &CMultiplayerGamerTagHud::CreateDebugTagForVehicle);
		bank->AddButton("Update Crew Tag", &CMultiplayerGamerTagHud::UpdateDebugCrewTag);
		bank->AddToggle("Clone Player Tag for All Vehicle Seats", &bDebugFillVehicleWithGamerTags);
		bank->AddSlider("Gamer Index", &iDebugGamerIndex, 0, MAX_PLAYERS_WITH_GAMER_TAGS-1, 1, &CMultiplayerGamerTagHud::UpdateDebugTagSelected);
		bank->AddToggle("Crew Tag Private", &bDebugIsCrewTagPrivate);
		bank->AddToggle("Crew Tag Contains R*", &bDebugCrewTagContainsRockstar);
		bank->AddSlider("Crew Hierarchy", &iDebugCrewRank, 0, 5, 1);
		bank->AddSlider("Crew Color R", &uDebugCrewR, 0, 255, 1);
		bank->AddSlider("Crew Color G", &uDebugCrewG, 0, 255, 1);
		bank->AddSlider("Crew Color B", &uDebugCrewB, 0, 255, 1);
		bank->AddText("Gamer Tag", cDebugGamerTag, RL_MAX_DISPLAY_NAME_BUF_SIZE);
		bank->AddText("Clan Tag", cDebugClanTag, RL_CLAN_TAG_MAX_CHARS);

		bank->AddSeparator();

		bank->AddSlider("Tag ID", &iDebugTagId, -1, (s32)MAX_MP_TAGS-1, 1, &CMultiplayerGamerTagHud::UpdateDebugTagName);
		bank->AddText("Tag Info", cDebugTagName, sizeof(cDebugTagName));
		bank->AddToggle("Use Vehicle Health", &bDebugUseVehicleHealth, &CMultiplayerGamerTagHud::UpdateDebugTagUseVehicleHealth);
		bank->AddToggle("Use Point Health", &bDebugUsePointHealth, &CMultiplayerGamerTagHud::UpdateDebugTagUsePointHealth);

		bank->AddButton("Set Tag Visibility ON", &CMultiplayerGamerTagHud::ToggleDebugTagVisiblityOn);
		bank->AddButton("Set Tag Visibility OFF", &CMultiplayerGamerTagHud::ToggleDebugTagVisiblityOff);

		bank->AddSlider("Set Tag Alpha", &iDebugTagAlpha, 0, 255, 1, &CMultiplayerGamerTagHud::SetDebugTagAlpha);
		bank->AddSlider("Tag Value", &iDebugTagValue, 0, 10, 1);
		bank->AddSlider("Health Percent", &iDebugHealthPercent, -1, 100, 1);
		bank->AddSlider("Armor Percent", &iDebugArmorPercent, -1, 100, 1);
		bank->AddSlider("Current Points", &iDebugCurrentPoints, -1, 500, 1);
		bank->AddSlider("Max Points", &iDebugMaxPoints, -1, 500, 1);

		bank->AddButton("Set Tag Value", &CMultiplayerGamerTagHud::SetDebugTagValue);

		bank->AddText("Big Text", cDebugBigText, MAX_BIG_TEXT_SIZE);
		bank->AddButton("Update Big Text", &CMultiplayerGamerTagHud::SetDebugBigText);

		bank->AddText("Vertical Screen Offset", &fDebugVerticalOffset, false);
		bank->AddButton("Set Screen Offset", &CMultiplayerGamerTagHud::UpdateDebugTagVerticalOffset);

		bank->PopGroup();

		bBankCreated = true;
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMultiplayerGamerTagHud::ShutdownWidgets()
// PURPOSE: removes the bank widget
/////////////////////////////////////////////////////////////////////////////////////
void CMultiplayerGamerTagHud::ShutdownWidgets()
{
	bkBank *pBank = BANKMGR.FindBank(UI_DEBUG_BANK_NAME);

	if (pBank)
	{
		pBank->Destroy();
	}
}

void CMultiplayerGamerTagHud::UpdateDebugFakePassengerTags()
{
	uDebugCurrentFakePassengerSeatIndex = 0;

	int localIndex = UT_GetLocalPlayerTag();
	if(localIndex != GAMER_TAG_INVALID_INDEX)
	{
		// Create new tag and clone from local player
		CPhysical* pLocalPhys = UT_GetPhysical(localIndex);
		if(pLocalPhys)
		{
			const CPed *pLocalPed = pLocalPhys->GetIsTypePed() ? reinterpret_cast<const CPed*>(pLocalPhys) : NULL;

			if(pLocalPed && pLocalPed->GetIsInVehicle())
			{
				if(bDebugFillVehicleWithGamerTags)
				{
					CVehicle* pVehicle = pLocalPed->GetMyVehicle();
					const CSeatManager* pSeatManager = pVehicle->GetSeatManager();
					if(pSeatManager)
					{
						uDebugNumFakeVehiclePassengers = (u16)pSeatManager->GetMaxSeats()-1;
						for(int i = 0; i < uDebugNumFakeVehiclePassengers; ++i)
						{
							char fakeName[128];
							sprintf(fakeName, "%s%i", m_gamerTags[localIndex].m_playerName, i);
							int cloneIndex = UT_CreateFakePlayerTag(pLocalPhys, fakeName, m_gamerTags[localIndex].m_crewTag, true);
							UT_SetGamerTagVisibility(cloneIndex, MP_TAG_GAMER_NAME, true);

							if(cloneIndex != GAMER_TAG_INVALID_INDEX)
							{
								m_gamerTags[cloneIndex].m_debugCloneTagFromPedForVehicle = true;
							}
						}

						// Force show the local player's name
						UT_SetGamerTagVisibility(localIndex, MP_TAG_GAMER_NAME, true);
					}

					// Clear flag so this only happens once
					bDebugFillVehicleWithGamerTags = false;
				}
			}
			else
			{
				for(int i = 0; i < MAX_PLAYERS_WITH_GAMER_TAGS; ++i)
				{
					if(m_gamerTags[i].m_debugCloneTagFromPedForVehicle && !m_gamerTags[i].IsStateShuttingDown())
					{
						UT_RemovePlayerTag(i);
					}
				}

				uDebugNumFakeVehiclePassengers = 0;
			}
		}
	}
}

void CMultiplayerGamerTagHud::CreateDebugTagForPlayer()
{
	CMultiplayerGamerTagHud::Init(INIT_SESSION);

	if(SMultiplayerGamerTagHud::IsInstantiated())
	{
		char crewTagBuffer[NetworkClan::FORMATTED_CLAN_TAG_LEN] = {0};
		NetworkClan::GetUIFormattedClanTag(!bDebugIsCrewTagPrivate, bDebugCrewTagContainsRockstar, cDebugClanTag, iDebugCrewRank, CRGBA(uDebugCrewR, uDebugCrewG, uDebugCrewB), crewTagBuffer, NetworkClan::FORMATTED_CLAN_TAG_LEN);

		SMultiplayerGamerTagHud::GetInstance().UT_CreateFakePlayerTag(CGameWorld::FindLocalPlayer(), cDebugGamerTag, crewTagBuffer);
	}

	UpdateDebugTagName();
}

void CMultiplayerGamerTagHud::RemoveDebugTagForPlayer()
{
	if(SMultiplayerGamerTagHud::IsInstantiated())
	{
		SMultiplayerGamerTagHud::GetInstance().UT_RemovePlayerTag(DEBUG_GAMER_TAG);
	}

	CMultiplayerGamerTagHud::Shutdown(SHUTDOWN_SESSION);
}

void CMultiplayerGamerTagHud::CreateDebugTagForVehicle()
{
	CMultiplayerGamerTagHud::Init(INIT_SESSION);

	if(SMultiplayerGamerTagHud::IsInstantiated())
	{
		CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
		CVehicle* pVehicle = pPlayerPed ? pPlayerPed->GetVehiclePedInside() : NULL;

		if(pVehicle)
		{
			s32 tagId = SMultiplayerGamerTagHud::GetInstance().UT_CreateFakePlayerTag(pVehicle, "", "");
			SMultiplayerGamerTagHud::GetInstance().UT_SetGamerTagVisibility(tagId, MP_TAG_GAMER_NAME, true);
			SMultiplayerGamerTagHud::GetInstance().UT_SetGamerTagVisibility(tagId, MP_TAG_HEALTH_BAR, true);
		}
	}

	UpdateDebugTagName();
}

void CMultiplayerGamerTagHud::UpdateDebugTagName()
{
	if (iDebugTagId == -1)
	{
		formatf(cDebugTagName, sizeof(cDebugTagName), "INVALID");
	}
	else
	{
		if(SMultiplayerGamerTagHud::IsInstantiated())
		{
			CMultiplayerGamerTagHud& rMPGamerTagHud = SMultiplayerGamerTagHud::GetInstance();

			bool isVisible=false;
			eTAG_FLAGS tagFlags = TF_NONE;
			if(rMPGamerTagHud.m_movie.IsActive())
			{
				const sGamerTag* pTag = rMPGamerTagHud.UT_GetActiveGamerTag(iDebugGamerIndex);
				if(pTag)
				{
					isVisible = pTag->m_visibleFlag[iDebugTagId];
					float alpha = static_cast<float>(pTag->m_alpha[iDebugTagId]);
					iDebugTagAlpha = static_cast<int>(alpha*255.0f/100.0f);
				}

				tagFlags = rMPGamerTagHud.m_rtTagFlags[iDebugTagId];
			}

			formatf(cDebugTagName, sizeof(cDebugTagName), "%s(valid=%d, vis=%d, flags=%d)", rMPGamerTagHud.m_GamerTagNames[iDebugTagId], (tagFlags & TF_LOCATION_MASK)?1:0, isVisible, tagFlags);
		}
	}
}

void CMultiplayerGamerTagHud::UpdateDebugTagSelected()
{
	if(SMultiplayerGamerTagHud::IsInstantiated())
	{
		CMultiplayerGamerTagHud& rMPGamerTagHud = SMultiplayerGamerTagHud::GetInstance();
		if(rMPGamerTagHud.m_movie.IsActive())
		{
			const sGamerTag* pTag = rMPGamerTagHud.UT_GetActiveGamerTag(iDebugGamerIndex);
			if(pTag)
			{
				bDebugUseVehicleHealth = pTag->m_useVehicleHealth;
				iDebugHealthPercent = pTag->m_previousHealthInfo.m_iHealthPercentageValue;
				iDebugArmorPercent = pTag->m_previousHealthInfo.m_iArmourPercentageValue;
			}
		}
	}

	UpdateDebugTagName();
}

void CMultiplayerGamerTagHud::UpdateDebugCrewTag()
{
	if(SMultiplayerGamerTagHud::IsInstantiated())
	{
		char crewTagBuffer[NetworkClan::FORMATTED_CLAN_TAG_LEN] = {0};
		NetworkClan::GetUIFormattedClanTag(!bDebugIsCrewTagPrivate, bDebugCrewTagContainsRockstar, cDebugClanTag, iDebugCrewRank, CRGBA(uDebugCrewR, uDebugCrewG, uDebugCrewB), crewTagBuffer, NetworkClan::FORMATTED_CLAN_TAG_LEN);

		SMultiplayerGamerTagHud::GetInstance().UT_SetGamerTagCrewDetails(iDebugGamerIndex, crewTagBuffer);
	}
}

void CMultiplayerGamerTagHud::UpdateDebugTagVerticalOffset()
{
	if(SMultiplayerGamerTagHud::IsInstantiated())
	{
		SMultiplayerGamerTagHud::GetInstance().UT_SetGamerTagVerticalOffset(fDebugVerticalOffset);
	}
}

void CMultiplayerGamerTagHud::UpdateDebugTagUseVehicleHealth()
{
	if(SMultiplayerGamerTagHud::IsInstantiated())
	{
		SMultiplayerGamerTagHud::GetInstance().UT_SetGamerTagsShouldUseVehicleHealth(iDebugGamerIndex, bDebugUseVehicleHealth);
	}
}

void CMultiplayerGamerTagHud::UpdateDebugTagUsePointHealth()
{
	if(SMultiplayerGamerTagHud::IsInstantiated())
	{
		SMultiplayerGamerTagHud::GetInstance().UT_SetGamerTagsShouldUsePointHealth(iDebugGamerIndex, bDebugUsePointHealth);
	}
}

void CMultiplayerGamerTagHud::ToggleDebugTagVisiblityOn()
{
	if (iDebugTagId == -1)
		return;

	if(SMultiplayerGamerTagHud::IsInstantiated())
	{
		SMultiplayerGamerTagHud::GetInstance().UT_SetGamerTagVisibility(iDebugGamerIndex, (eMP_TAG)iDebugTagId, true);
	}

	UpdateDebugTagName();
}

void CMultiplayerGamerTagHud::ToggleDebugTagVisiblityOff()
{
	if (iDebugTagId == -1)
		return;

	if(SMultiplayerGamerTagHud::IsInstantiated())
	{
		SMultiplayerGamerTagHud::GetInstance().UT_SetGamerTagVisibility(iDebugGamerIndex, (eMP_TAG)iDebugTagId, false);
	}

	UpdateDebugTagName();
}

void CMultiplayerGamerTagHud::SetDebugTagAlpha()
{
	if (iDebugTagId == -1)
		return;

	if(SMultiplayerGamerTagHud::IsInstantiated())
	{
		SMultiplayerGamerTagHud::GetInstance().UT_SetGamerTagValueHudAlpha(iDebugGamerIndex, (eMP_TAG)iDebugTagId, iDebugTagAlpha);
	}
}

void CMultiplayerGamerTagHud::SetDebugTagValue()
{
	if (iDebugTagId == -1)
		return;

	if(SMultiplayerGamerTagHud::IsInstantiated())
	{
		SMultiplayerGamerTagHud::GetInstance().UT_SetGamerTagValueInt(iDebugGamerIndex, (eMP_TAG)iDebugTagId, iDebugTagValue);
	}
}

void CMultiplayerGamerTagHud::SetDebugBigText()
{
	if (iDebugTagId == -1)
		return;

	if(SMultiplayerGamerTagHud::IsInstantiated())
	{
		SMultiplayerGamerTagHud::GetInstance().UT_SetBigText(iDebugGamerIndex, cDebugBigText);
	}
}

#endif // __BANK

CMultiplayerGamerTagHud::CMultiplayerGamerTagHud() :
	m_rootCreated(false),
	m_deletePending(false)
{
	m_movie.CreateMovie(SF_BASE_CLASS_HUD, GAMER_INFO_FILENAME, Vector2(0.0f, 0.0f), Vector2(1.0f, 1.0f));

#if !__NO_OUTPUT
#define GAMER_TAG_OBJ(val, asName, defaultTags) m_GamerTagNames[val] = #val;
	GAMER_TAG_LIST
#undef GAMER_TAG_OBJ
#endif //!__NO_OUTPUT

#define GAMER_TAG_OBJ(val, asName, defaultTags) m_GamerTagASNames[val] = #asName;
	GAMER_TAG_LIST
#undef GAMER_TAG_OBJ

#define GAMER_TAG_OBJ(val, asName, defaultTags) m_rtTagFlags[val] = defaultTags;
	GAMER_TAG_LIST
#undef GAMER_TAG_OBJ


	for (s16 i = 0; i < MAX_PLAYERS_WITH_GAMER_TAGS; i++)
	{
		m_gamerTags[i].Reset();

		m_gamerTags[i].m_rtDepth = i;
	}
}

CMultiplayerGamerTagHud::~CMultiplayerGamerTagHud()
{
	m_renderingSection.Lock();

	{ // scope for autolock
		CScaleformMgr::AutoLock lock(m_movie.GetMovieID());
	

		for (s32 i = 0; i < MAX_PLAYERS_WITH_GAMER_TAGS; i++)
		{
			if (!m_gamerTags[i].IsStateUndefined())
			{
				m_gamerTags[i].Reset();
			}
		}

		UT_RemoveRoot();

	}
	m_movie.RemoveMovie();

	m_renderingSection.Unlock();
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMultiplayerGamerTagHud::Init
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CMultiplayerGamerTagHud::Init(unsigned initMode)
{
	if (initMode == INIT_SESSION)
	{
		if(!SMultiplayerGamerTagHud::IsInstantiated())
		{
			SMultiplayerGamerTagHud::Instantiate();
		}

		uiDebugf1("Gamertag system active and ready");
	}
}

bool CMultiplayerGamerTagHud::IsMovieActive() const
{
	return m_rootCreated && m_asGamerTagLayerContainer.IsDefined() && m_asRootContainer.IsDefined() && m_movie.IsActive();
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMultiplayerGamerTagHud::Shutdown
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CMultiplayerGamerTagHud::Shutdown(unsigned shutdownMode)
{
	if (shutdownMode == SHUTDOWN_SESSION)
	{
		if (SMultiplayerGamerTagHud::IsInstantiated())
		{
			gRenderThreadInterface.Flush();  // flush the RT

			SMultiplayerGamerTagHud::Destroy();
		}

		uiDebugf1("Gamertag system fully shutdown");
	}
}


void CMultiplayerGamerTagHud::UpdateAtEndOfFrame()
{
	if (SMultiplayerGamerTagHud::IsInstantiated() && SMultiplayerGamerTagHud::GetInstance().IsDeletePending())
	{
		uiDebugf1("CMultiplayerGamerTagHud::UpdateAtEndOfFrame - Start Shutdown");
		SMultiplayerGamerTagHud::Destroy();
		uiDebugf1("CMultiplayerGamerTagHud::UpdateAtEndOfFrame - Finished Shutdown");
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMultiplayerGamerTagHud::CreatePlayerTagForPed
// PURPOSE:	creates the tag for the ped passed in into the iplayernum in the array
/////////////////////////////////////////////////////////////////////////////////////
void CMultiplayerGamerTagHud::UT_CreatePlayerTagHelper(s32 iPlayerNum, CPhysical *pPhys, const char *pPlayerName, const char* pFormattedCrewTag)
{
	if (uiVerify(CSystem::IsThisThreadId(SYS_THREAD_UPDATE)) && uiVerify(m_movie.IsActive()))
	{
#if __BANK
		bool bDebugTag = false;

		if (iPlayerNum == DEBUG_GAMER_TAG)
		{
			bDebugTag = true;

			iPlayerNum = MAX_NUM_PHYSICAL_PLAYERS;
		}
#endif

		if (BANK_ONLY(bDebugTag ||) uiVerify(NetworkInterface::IsGameInProgress()))
		{
			uiDisplayf("CMultiplayerGamerTagHud::UT_CreatePlayerTagHelper - Gamertag for player %d has been requested to be set up", iPlayerNum);

			if (uiVerifyf(0 <= iPlayerNum && iPlayerNum < MAX_PLAYERS_WITH_GAMER_TAGS, "CMultiplayerGamerTagHud::UT_CreatePlayerTagHelper - iPlayerNum=%d, MAX_PLAYERS_WITH_GAMER_TAGS=%d", iPlayerNum, MAX_PLAYERS_WITH_GAMER_TAGS))
			{
				sGamerTag& rGamerTag = m_gamerTags[iPlayerNum];
				if (uiVerifyf(rGamerTag.IsStateUndefined(), "CMultiplayerGamerTagHud::UT_CreatePlayerTagHelper - Gamer Tag %s(%d) already in use. initPending=%d, active=%d, removePending=%d, destroyPending=%d. Local Gamer is %s",
					pPlayerName, iPlayerNum, rGamerTag.IsStateInitPending(), rGamerTag.IsStateActive(), rGamerTag.IsStateRemovePending(), rGamerTag.IsStateDestroyPending(), NetworkInterface::GetActiveGamerInfo()->GetName()))
				{
					rGamerTag.m_utPhysical = pPhys;
					rGamerTag.m_visible.GetWriteBuf() = false;
					rGamerTag.m_scale.GetWriteBuf() = 1.0f;
					rGamerTag.m_playerPos.GetWriteBuf() = Vector3(-1.0f,-1.0f,-1.0f);
					rGamerTag.m_screenPos.GetWriteBuf() = Vector2(-1.0f,-1.0f);
					rGamerTag.m_reinitPositions = true;

					formatf(rGamerTag.m_playerName, RL_MAX_DISPLAY_NAME_BUF_SIZE, "%s", pPlayerName);
					formatf(rGamerTag.m_refName, MAX_REF_NAME_SIZE, "MP_GAMER_INFO_%d", iPlayerNum);

					UT_SetGamerTagCrewDetails(iPlayerNum, pFormattedCrewTag); 

					m_renderingSection.Lock();
					rGamerTag.SetStateInitPending();
					m_renderingSection.Unlock();
				}
			}
			else
			{
				uiAssertf(0, "CMultiplayerGamerTagHud::UT_CreatePlayerTagHelper - GamerTag: Too many players with gamer tags so couldn't make one for %s(%d) - total possible %d (%d MP chars, %d fake)", pPlayerName, iPlayerNum, MAX_PLAYERS_WITH_GAMER_TAGS, MAX_NUM_PHYSICAL_PLAYERS, MAX_FAKE_GAMER_TAG_PLAYERS);
			}
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMultiplayerGamerTagHud::CreatePlayerTag
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CMultiplayerGamerTagHud::UT_CreatePlayerTag(s32 iPlayerNum, const char *pPlayerName, const char* pFormattedCrewTag)
{
	if (uiVerify(CSystem::IsThisThreadId(SYS_THREAD_UPDATE)) && uiVerify(m_movie.IsActive()))
	{
#if __BANK
		bool bDebugTag = false;

		if (iPlayerNum == DEBUG_GAMER_TAG)
		{
			bDebugTag = true;
		}

		if (bDebugTag)
		{
			UT_CreatePlayerTagHelper(iPlayerNum, CGameWorld::FindLocalPlayer(), pPlayerName, pFormattedCrewTag);
		}
		else
#endif
		{
			// ensure we have a net player to create it on
			CNetGamePlayer* pNetPlayer = NetworkInterface::GetPhysicalPlayerFromIndex(static_cast<PhysicalPlayerIndex>(iPlayerNum));

			if (pNetPlayer)
			{
				UT_CreatePlayerTagHelper(iPlayerNum, NULL, pPlayerName, pFormattedCrewTag);
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMultiplayerGamerTagHud::CreateFakePlayerTag
// PURPOSE:	creates a tag for a fake player
/////////////////////////////////////////////////////////////////////////////////////
s32 CMultiplayerGamerTagHud::UT_CreateFakePlayerTag(CPhysical *pPhys, const char *pPlayerName, const char* pFormattedCrewTag, bool bIgnoreDuplicates)
{
#if __ASSERT
	int numActive = 0;
	int numRemovePending = 0;
	int numDeletePending = 0;
#endif

	if (uiVerify(CSystem::IsThisThreadId(SYS_THREAD_UPDATE)) && uiVerify(m_movie.IsActive()) && uiVerify(pPhys))
	{
		if(!bIgnoreDuplicates)
		{
			for (s32 i = MAX_NUM_PHYSICAL_PLAYERS; i < MAX_PLAYERS_WITH_GAMER_TAGS; i++)
			{
				if (!m_gamerTags[i].IsStateUndefined())
				{
					if(!uiVerifyf(pPhys != m_gamerTags[i].m_utPhysical.Get(), "CMultiplayerGamerTagHud::UT_CreateFakePlayerTag - Trying to create a FakePlayerTag for a Entity that already has one."))
					{
						return i;
					}
				}
			}
		}
	
		for (s32 j = MAX_NUM_PHYSICAL_PLAYERS; j < MAX_PLAYERS_WITH_GAMER_TAGS; j++)
		{
			if (m_gamerTags[j].IsStateUndefined())
			{
				UT_CreatePlayerTagHelper(j, pPhys, pPlayerName, pFormattedCrewTag);
				UT_SetGamerTagVisibility( j, MP_TAG_GAMER_NAME, true );
		
				return j;
			}
#if __ASSERT
			else if (m_gamerTags[j].IsStateActive())
			{
				++numActive;
			}
			else if (m_gamerTags[j].IsStateRemovePending())
			{
				++numRemovePending;
			}
			else if (m_gamerTags[j].IsStateDestroyPending())
			{
				++numDeletePending;
			}
#endif
		}
	}
	
	uiAssertf(0, "CMultiplayerGamerTagHud::UT_CreateFakePlayerTag - GamerTag: Too many fake players added - invalid=%d, max=%d, numActive=%d, numRemoving=%d, numDeletePending=%d", GAMER_TAG_INVALID_INDEX, MAX_FAKE_GAMER_TAG_PLAYERS, numActive, numRemovePending, numDeletePending);
	return GAMER_TAG_INVALID_INDEX;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMultiplayerGamerTagHud::RemovePlayerTag
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CMultiplayerGamerTagHud::UT_RemovePlayerTag(s32 iPlayerNum)
{
	if (uiVerify(CSystem::IsThisThreadId(SYS_THREAD_UPDATE)) && uiVerify(m_movie.IsActive()))
	{
#if __BANK
		bool bDebugTag = false;

		if (iPlayerNum == DEBUG_GAMER_TAG)
		{
			bDebugTag = true;

			iPlayerNum = MAX_NUM_PHYSICAL_PLAYERS;
		}
#endif

		if (BANK_ONLY(bDebugTag ||) uiVerify(NetworkInterface::IsGameInProgress()))
		{
			if (uiVerifyf(0 <= iPlayerNum && iPlayerNum < MAX_PLAYERS_WITH_GAMER_TAGS, "CMultiplayerGamerTagHud::UT_RemovePlayerTag - iPlayerNum(%d) is out of range, MAX_PLAYERS_WITH_GAMER_TAGS=%d", iPlayerNum, MAX_PLAYERS_WITH_GAMER_TAGS))
			{
				sGamerTag& rGamerTag = m_gamerTags[iPlayerNum];

				if (uiVerifyf(!rGamerTag.IsStateUndefined(), "CMultiplayerGamerTagHud::UT_RemovePlayerTag - iPlayerNum(%d) doesn't have a gamer tag setup. Local Player is %s", iPlayerNum, NetworkInterface::GetActiveGamerInfo()->GetName()) &&
					uiVerifyf(!rGamerTag.IsStateShuttingDown(), "CMultiplayerGamerTagHud::UT_RemovePlayerTag - iPlayerNum(%d) already has his gamertag shutting down. removePending=%d, destroyPending=%d. Local Player is %s",
							iPlayerNum, rGamerTag.IsStateRemovePending(), rGamerTag.IsStateDestroyPending(), NetworkInterface::GetActiveGamerInfo()->GetName()))
				{
					m_renderingSection.Lock();
					rGamerTag.SetStateRemovePending();
					m_renderingSection.Unlock();
				}
			}
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMultiplayerGamerTagHud::UT_IsGamerTagActive
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
bool CMultiplayerGamerTagHud::UT_IsGamerTagActive(s32 iPlayerNum)
{
	bool isActive = false;
	if (uiVerify(CSystem::IsThisThreadId(SYS_THREAD_UPDATE)) && SMultiplayerGamerTagHud::IsInstantiated())  // only on UT
	{
		CMultiplayerGamerTagHud& rHud = SMultiplayerGamerTagHud::GetInstance();
		if (rHud.m_movie.IsActive())
		{
			if (uiVerifyf(0 <= iPlayerNum && iPlayerNum < MAX_PLAYERS_WITH_GAMER_TAGS, "CMultiplayerGamerTagHud::UT_IsGamerTagActive - iPlayerNum(%d) is out of range, MAX_PLAYERS_WITH_GAMER_TAGS=%d", iPlayerNum, MAX_PLAYERS_WITH_GAMER_TAGS))
			{
				isActive = rHud.m_gamerTags[iPlayerNum].IsStateActive() || rHud.m_gamerTags[iPlayerNum].IsStateInitPending();
			}
		}
	}

	return isActive;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMultiplayerGamerTagHud::UT_IsGamerTagFree
// PURPOSE:	Returns true if a gamer tag can be added to this slot.
/////////////////////////////////////////////////////////////////////////////////////
bool CMultiplayerGamerTagHud::UT_IsGamerTagFree(s32 iPlayerNum)
{
	bool isFree = false;
	if (uiVerify(CSystem::IsThisThreadId(SYS_THREAD_UPDATE)) && SMultiplayerGamerTagHud::IsInstantiated())  // only on UT
	{
		CMultiplayerGamerTagHud& rHud = SMultiplayerGamerTagHud::GetInstance();
		if (uiVerify(rHud.m_movie.IsActive()))
		{
			if (uiVerifyf(0 <= iPlayerNum && iPlayerNum < MAX_PLAYERS_WITH_GAMER_TAGS, "CMultiplayerGamerTagHud::UT_IsGamerTagActive - iPlayerNum(%d) is out of range, MAX_PLAYERS_WITH_GAMER_TAGS=%d", iPlayerNum, MAX_PLAYERS_WITH_GAMER_TAGS))
			{
				isFree = rHud.m_gamerTags[iPlayerNum].IsStateUndefined();
			}
		}
	}

	return isFree;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMultiplayerGamerTagHud::SetColourToGfxvalue
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CMultiplayerGamerTagHud::RT_SetColourToGfxvalue(GFxValue *pDisplayObject, Color32 col)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		uiAssertf(0, "CMultiplayerGamerTagHud::RT_SetColourToGfxvalue can only be called on the RenderThread!");
		return;
	}

	if (pDisplayObject)
	{
		GFxValue::DisplayInfo oldDisplayInfo;
		pDisplayObject->GetDisplayInfo(&oldDisplayInfo);
		Double alpha = oldDisplayInfo.GetAlpha();

		pDisplayObject->SetColorTransform(col);

		GFxValue::DisplayInfo newDisplayInfo;
		newDisplayInfo.SetAlpha(alpha);
		pDisplayObject->SetDisplayInfo(newDisplayInfo);

//		Displayf("r = %d g = %d b = %d a = %d", col.GetRed(), col.GetGreen(), col.GetBlue(), col.GetAlpha());
	}
}


bool CMultiplayerGamerTagHud::RT_InitAndVerifyAllComponentGfxValues(s32 iPlayerId, GFxValue& centerIcons, GFxValue& leftIcons)
{
	bool isValid = false;
	uiDebugf1("CMultiplayerGamerTagHud::RT_InitAndVerifyAllComponentGfxValues - Begin");

	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		uiAssertf(0, "CMultiplayerGamerTagHud::RT_InitAndVerifyAllComponentGfxValues - can only be called on the RenderThread!");
		return isValid;
	}

	if (!uiVerify(m_movie.IsActive()))
		return isValid;

	ASSERT_ONLY(sGamerTag& rGamerTag = m_gamerTags[iPlayerId]);
	uiAssertf(rGamerTag.m_asGamerTagContainerMc.IsDefined(), "CMultiplayerGamerTagHud::RT_InitAndVerifyAllComponentGfxValues - Gamertag movied failed to be created for player %d", iPlayerId);
	uiAssertf(rGamerTag.m_asGamerTagContainerMc.HasMember("healthArmour"), "CMultiplayerGamerTagHud::RT_InitAndVerifyAllComponentGfxValues - Gamertag value healthArmour is invalid for player %d!", iPlayerId);
	uiAssertf(rGamerTag.m_asGamerTagContainerMc.HasMember("BIG_TEXT"), "CMultiplayerGamerTagHud::RT_InitAndVerifyAllComponentGfxValues - Gamertag value BIG_TEXT is invalid for player %d!", iPlayerId);
	{
		isValid = true;

		for (s32 iTag = 0; iTag < MAX_MP_TAGS; iTag++)
		{
			eTAG_FLAGS flags = m_rtTagFlags[iTag];

			if((flags & TF_LOCATION_MASK) == 0)
			{
				uiDebugf1("CMultiplayerGamerTagHud::RT_InitAndVerifyAllComponentGfxValues - Initing Tag %s.", m_GamerTagNames[iTag]);
				RT_InitComponentGfxValue(iPlayerId, (eMP_TAG)iTag, centerIcons, leftIcons);
			}
#if __DEV
			else
			{
				uiDebugf1("CMultiplayerGamerTagHud::RT_InitAndVerifyAllComponentGfxValues - Tag %s already inited, shouldExist=%d, left=%d, center=%d, baseLevel=%d, locMask=%d, TF_LOCATION_MASK=%d.",
					m_GamerTagNames[iTag], (flags&TF_SHOULD_EXIST)?1:0, (flags&TF_LEFT_ROW)?1:0, (flags&TF_CENTER_ROW)?1:0, (flags&TF_BASE_LEVEL)?1:0, flags&TF_LOCATION_MASK, TF_LOCATION_MASK);

				if(flags & TF_SHOULD_EXIST)
				{
					GFxValue componentGfxValue;
					RT_GetComponentGfxValue(&componentGfxValue, iPlayerId, static_cast<eMP_TAG>(iTag), centerIcons, leftIcons);
					uiAssertf(componentGfxValue.IsDefined(), "CMultiplayerGamerTagHud::RT_InitAndVerifyAllComponentGfxValues - Gamertag invalid! Code/Script Name=%s(%d), AS Name=%s", m_GamerTagNames[iTag], iTag, m_GamerTagASNames[iTag]);
				}
			}
#endif //__DEV
		}
	}

	uiDebugf1("CMultiplayerGamerTagHud::RT_InitAndVerifyAllComponentGfxValues - Gamertag: VerifyAllComponentGfxValues - Complete");
	return isValid;
}

void CMultiplayerGamerTagHud::RT_InitComponentGfxValue(s32 iPlayerId, eMP_TAG iTag, GFxValue& centerIcons, GFxValue& leftIcons)
{
	const char* asName = m_GamerTagASNames[iTag];

	if(centerIcons.HasMember(asName))
	{
		m_rtTagFlags[iTag] = static_cast<eTAG_FLAGS>(m_rtTagFlags[iTag] | TF_CENTER_ROW);
	}
	else if(leftIcons.HasMember(asName))
	{
		m_rtTagFlags[iTag] = static_cast<eTAG_FLAGS>(m_rtTagFlags[iTag] | TF_LEFT_ROW);
	}
	else if(m_gamerTags[iPlayerId].m_asGamerTagContainerMc.HasMember(asName))
	{
		m_rtTagFlags[iTag] = static_cast<eTAG_FLAGS>(m_rtTagFlags[iTag] | TF_BASE_LEVEL);
	}
#if __ASSERT
	else
	{
		if(m_rtTagFlags[iTag] & TF_SHOULD_EXIST)
		{
			uiAssertf(0, "CMultiplayerGamerTagHud::RT_InitComponentGfxValue - Gamertag invalid! Code/Script Name=%s(%d), AS Name=%s", m_GamerTagNames[iTag], iTag, asName);
		}
	}
#endif
}

void CMultiplayerGamerTagHud::RT_GetComponentGfxValue(GFxValue *pComponentGfxValue, s32 iPlayerId, eMP_TAG iTag, GFxValue& centerIcons, GFxValue& leftIcons, bool ASSERT_ONLY(bHasToExist) /*= true*/)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		uiAssertf(0, "CMultiplayerGamerTagHud::RT_GetComponentGfxValue - can only be called on the RenderThread!");
		return;
	}

	if (!uiVerify(m_movie.IsActive()))
		return;

	if(!uiVerify(0 <= iTag && iTag < MAX_MP_TAGS))
		return;

	sGamerTag& rGamerTag = m_gamerTags[iPlayerId];
	if (rGamerTag.m_asGamerTagContainerMc.IsDefined())
	{
		bool bSuccess = false;
		const char* asName = m_GamerTagASNames[iTag];
		eTAG_FLAGS flags = m_rtTagFlags[iTag];


		if((flags & TF_LOCATION_MASK) == 0)
		{
			RT_InitComponentGfxValue(iPlayerId, (eMP_TAG)iTag, centerIcons, leftIcons);
			flags = m_rtTagFlags[iTag];
		}

		if(flags & TF_CENTER_ROW)
		{
			if(uiVerifyf(centerIcons.IsDefined(), "CMultiplayerGamerTagHud::RT_GetComponentGfxValue - Failed to get the Above Icons for player %d", iPlayerId))
			{
				bSuccess = centerIcons.GetMember(asName, pComponentGfxValue);
			}
		}
		else if(flags & TF_LEFT_ROW)
		{
			if(uiVerifyf(leftIcons.IsDefined(), "CMultiplayerGamerTagHud::RT_GetComponentGfxValue - Failed to get the Left Icons for player %d", iPlayerId))
			{
				bSuccess = leftIcons.GetMember(asName, pComponentGfxValue);
			}
		}
		else if(flags & TF_BASE_LEVEL)
		{
			bSuccess = rGamerTag.m_asGamerTagContainerMc.GetMember(asName, pComponentGfxValue);
		}

#if __ASSERT
		if(bHasToExist || ((flags & TF_SHOULD_EXIST) != 0))
		{
			uiAssertf(bSuccess, "CMultiplayerGamerTagHud::RT_GetComponentGfxValue - Gamertag invalid! Code/Script Name=%s(%d), AS Name=%s", m_GamerTagNames[iTag], iTag, asName);
		}
#endif
	}
	else
	{
		uiAssertf(0, "CMultiplayerGamerTagHud::RT_GetComponentGfxValue - Gamertag Value asGamerTagContainerMc is undefined for player %d.", iPlayerId);
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMultiplayerGamerTagHud::GetGamerTagVisibility
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
bool CMultiplayerGamerTagHud::RT_GetGamerTagVisibility(s32 iPlayerId, eMP_TAG iTag)
{
	if (!uiVerify(m_movie.IsActive()))
		return false;

	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		uiAssertf(0, "CMultiplayerGamerTagHud::RT_GetGamerTagVisibility - can only be called on the RenderThread!");
		return false;
	}

	if (iPlayerId < MAX_PLAYERS_WITH_GAMER_TAGS)
	{
		if(uiVerifyf(m_gamerTags[iPlayerId].m_asGamerTagContainerMc.HasMember(GAMERTAG_CENTER_OBJECT), "CMultiplayerGamerTagHud::RT_InitComponentGfxValue - Gamertag value " GAMERTAG_CENTER_OBJECT " is invalid for player %d!", iPlayerId) &&
			uiVerifyf(m_gamerTags[iPlayerId].m_asGamerTagContainerMc.HasMember(GAMERTAG_LEFT_OBJECT), "CMultiplayerGamerTagHud::RT_InitComponentGfxValue - Gamertag value " GAMERTAG_LEFT_OBJECT " is invalid for player %d!", iPlayerId))
		{
			GFxValue centerIcons, leftIcons;
			m_gamerTags[iPlayerId].m_asGamerTagContainerMc.GetMember(GAMERTAG_CENTER_OBJECT, &centerIcons);
			m_gamerTags[iPlayerId].m_asGamerTagContainerMc.GetMember(GAMERTAG_LEFT_OBJECT, &leftIcons);

			GFxValue componentGfxValue;

			RT_GetComponentGfxValue(&componentGfxValue, iPlayerId, iTag, centerIcons, leftIcons);

			if (componentGfxValue.IsDefined())
			{
				GFxValue::DisplayInfo theDisplayInfo;
				componentGfxValue.GetDisplayInfo(&theDisplayInfo);

				return theDisplayInfo.GetVisible();
			}
		}
	}

	return false;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMultiplayerGamerTagHud::SetGamerTagVisibility
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CMultiplayerGamerTagHud::UT_SetGamerTagVisibility(s32 iPlayerId, eMP_TAG iTag, bool bSetVisible, bool bEvenInBigVehicles)
{
	if (uiVerify(CSystem::IsThisThreadId(SYS_THREAD_UPDATE) && uiVerify(m_movie.IsActive())))
	{
		if (uiVerifyf(0<=iPlayerId && iPlayerId<MAX_PLAYERS_WITH_GAMER_TAGS, "CMultiplayerGamerTagHud::UT_SetGamerTagValueInt - iPlayerId=%d, max=%d", iPlayerId, MAX_PLAYERS_WITH_GAMER_TAGS) &&
			uiVerifyf(iTag < MAX_MP_TAGS, "CMultiplayerGamerTagHud::UT_SetGamerTagVisibility - iTag=%d, max=%d", iTag, MAX_MP_TAGS))
		{
			if(uiVerifyf(m_gamerTags[iPlayerId].IsStateActive() || m_gamerTags[iPlayerId].IsStateInitPending(), "CMultiplayerGamerTagHud::UT_SetGamerTagVisibility - Gametag for player %d isn't active yet. removePending=%d, destroyPending=%d", iPlayerId, m_gamerTags[iPlayerId].IsStateRemovePending(), m_gamerTags[iPlayerId].IsStateDestroyPending()))
			{
				// If the crew tag is empty, don't show it, even if script says to.
				if(iTag == MP_TAG_CREW_TAG && m_gamerTags[iPlayerId].m_crewTag[0] == '\0')
				{
					bSetVisible = false;
				}

				m_gamerTags[iPlayerId].m_showEvenInBigVehicles = bEvenInBigVehicles;
	
				if (m_gamerTags[iPlayerId].m_visibleFlag[iTag] != bSetVisible)
				{
					uiDebugf2("CMultiplayerGamerTagHud::UT_SetGamerTagVisibility - iPlayerId=%d, bSetVisible=%d, tag=%s(%d)", iPlayerId, bSetVisible, m_GamerTagNames[iTag], iTag);
	
					m_gamerTags[iPlayerId].m_visibleFlag[iTag] = bSetVisible;
					m_gamerTags[iPlayerId].m_TagReinitFlags.GetWriteBuf().Set(iTag);
					m_gamerTags[iPlayerId].SetStateReinit();
				}
			}
		}
	}
}

void CMultiplayerGamerTagHud::UT_SetBigText(s32 iPlayerId, const char* text)
{
	if (uiVerify(CSystem::IsThisThreadId(SYS_THREAD_UPDATE) && uiVerify(m_movie.IsActive())))
	{
		if (uiVerifyf(0<=iPlayerId && iPlayerId<MAX_PLAYERS_WITH_GAMER_TAGS, "CMultiplayerGamerTagHud::UT_SetBigText - iPlayerId=%d, max=%d", iPlayerId, MAX_PLAYERS_WITH_GAMER_TAGS))
		{
			if(uiVerifyf(m_gamerTags[iPlayerId].IsStateActive() || m_gamerTags[iPlayerId].IsStateInitPending(), "CMultiplayerGamerTagHud::UT_SetBigText - Gametag for player %d isn't active yet. removePending=%d, destroyPending=%d", iPlayerId, m_gamerTags[iPlayerId].IsStateRemovePending(), m_gamerTags[iPlayerId].IsStateDestroyPending()) &&
				!m_gamerTags[iPlayerId].m_reinitBigText)
			{
				m_gamerTags[iPlayerId].m_reinitBigText = true;
				formatf(m_gamerTags[iPlayerId].m_bigtext, MAX_BIG_TEXT_SIZE, "%s", text);
			}
		}
	}
}

const sGamerTag* CMultiplayerGamerTagHud::UT_GetActiveGamerTag(s32 iPlayerId)
{
	const sGamerTag* pRetTag = NULL;

	if (uiVerify(CSystem::IsThisThreadId(SYS_THREAD_UPDATE) && uiVerify(m_movie.IsActive())))
	{
		if ( uiVerifyf(0<=iPlayerId && iPlayerId<MAX_PLAYERS_WITH_GAMER_TAGS, "CMultiplayerGamerTagHud::UT_GetActiveGamerTag - iPlayerId=%d, max=%d", iPlayerId, MAX_PLAYERS_WITH_GAMER_TAGS ))
		{
			if (m_gamerTags[iPlayerId].IsStateActive() || m_gamerTags[iPlayerId].IsStateInitPending())
			{
				pRetTag = &m_gamerTags[iPlayerId];
			}
		}
	}

	return pRetTag;
}

bool CMultiplayerGamerTagHud::UT_IsReinitingGamerNameAndCrewTag(s32 iPlayerId)
{
	if (uiVerify(CSystem::IsThisThreadId(SYS_THREAD_UPDATE) && uiVerify(m_movie.IsActive())))
	{
		if (uiVerifyf(0<=iPlayerId && iPlayerId<MAX_PLAYERS_WITH_GAMER_TAGS, "CMultiplayerGamerTagHud::UT_IsReinitingGamerNameAndCrewTag - iPlayerId=%d, max=%d", iPlayerId, MAX_PLAYERS_WITH_GAMER_TAGS))
		{
			return m_gamerTags[iPlayerId].m_reinitCrewTag;
		}
	}

	return false;
}

void CMultiplayerGamerTagHud::UT_SetGamerName(s32 iPlayerId, const char *pPlayerName)
{
	if (uiVerify(CSystem::IsThisThreadId(SYS_THREAD_UPDATE) && uiVerify(m_movie.IsActive())))
	{
		if (uiVerify(pPlayerName) &&
			uiVerifyf(0<=iPlayerId && iPlayerId<MAX_PLAYERS_WITH_GAMER_TAGS, "CMultiplayerGamerTagHud::UT_SetGamerName - iPlayerId=%d, max=%d", iPlayerId, MAX_PLAYERS_WITH_GAMER_TAGS))
		{
			sGamerTag& rGamerTag = m_gamerTags[iPlayerId];

			if(stricmp(rGamerTag.m_playerName, pPlayerName) != 0 &&
				uiVerifyf(!rGamerTag.m_reinitCrewTag, "CMultiplayerGamerTagHud::UT_SetGamerName - iPlayerId=%d is already adjusting his player name and crew info.", iPlayerId))
			{
				formatf(rGamerTag.m_playerName, RL_MAX_DISPLAY_NAME_BUF_SIZE, "%s", pPlayerName);
				rGamerTag.m_reinitCrewTag = true;
			}
		}
	}
}

void CMultiplayerGamerTagHud::UT_SetGamerTagVerticalOffset(float fVerticalOffset)
{
	if (uiVerify(CSystem::IsThisThreadId(SYS_THREAD_UPDATE) && uiVerify(m_movie.IsActive())))
	{
		sm_fVerticalOffset = fVerticalOffset;
	}
}

void CMultiplayerGamerTagHud::UT_SetGamerTagCrewDetails(s32 iPlayerId, const char* pFormattedCrewTag)
{
	if (uiVerify(CSystem::IsThisThreadId(SYS_THREAD_UPDATE) && uiVerify(m_movie.IsActive())))
	{
		if (uiVerifyf(0<=iPlayerId && iPlayerId<MAX_PLAYERS_WITH_GAMER_TAGS, "CMultiplayerGamerTagHud::UT_SetGamerTagCrewDetails - iPlayerId=%d, max=%d", iPlayerId, MAX_PLAYERS_WITH_GAMER_TAGS))
		{
			sGamerTag& rGamerTag = m_gamerTags[iPlayerId];

			if(uiVerifyf(!rGamerTag.m_reinitCrewTag, "CMultiplayerGamerTagHud::UT_SetGamerTagCrewDetails - iPlayerId=%d is already adjusting his player name and crew info.", iPlayerId))
			{
				formatf(rGamerTag.m_crewTag, NetworkClan::FORMATTED_CLAN_TAG_LEN, "%s", pFormattedCrewTag);
				rGamerTag.m_reinitCrewTag = true;
			}
		}
	}
}

void CMultiplayerGamerTagHud::RT_ReinitGamerTagCrewDetails(s32 iPlayerId)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on UT
	{
		uiAssertf(0, "CMultiplayerGamerTagHud::RT_ReinitGamerTagCrewDetails - can only be called on the RenderThread!");
		return;
	}

	if (!uiVerify(m_movie.IsActive()))
		return;

	sGamerTag& rGamerTag = m_gamerTags[iPlayerId];

	GFxValue args[2];
	args[0].SetString(rGamerTag.m_playerName);
	args[1].SetString(rGamerTag.m_crewTag);

	GFxValue result;
	rGamerTag.m_asGamerTagContainerMc.Invoke("SET_GAMERNAME_AND_PACKED_CREW_TAG", &result, args, COUNTOF(args));
	rGamerTag.m_reinitCrewTag = false;


	GFxValue centerIcons, leftIcons;
	rGamerTag.m_asGamerTagContainerMc.GetMember(GAMERTAG_CENTER_OBJECT, &centerIcons);
	rGamerTag.m_asGamerTagContainerMc.GetMember(GAMERTAG_LEFT_OBJECT, &leftIcons);

	GFxValue componentGfxValue;
	RT_GetComponentGfxValue(&componentGfxValue, iPlayerId, MP_TAG_GAMER_NAME, centerIcons, leftIcons, false);
	if (!componentGfxValue.IsUndefined())
	{
		GFxValue gamerNameWidth;
		componentGfxValue.GetMember("textWidth", &gamerNameWidth);

		rGamerTag.m_gamerTagWidth = static_cast<float>(gamerNameWidth.GetNumber());
	}

	uiDisplayf("CMultiplayerGamerTagHud::RT_ReinitGamerTagCrewDetails - iPlayerId=%d, GamerTag='%s', ClanTag='%s'", iPlayerId, rGamerTag.m_playerName, rGamerTag.m_crewTag);
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMultiplayerGamerTagHud::UT_SetAllGamerTagsVisibility
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CMultiplayerGamerTagHud::UT_SetAllGamerTagsVisibility(s32 iPlayerId, bool bSetVisible)
{
	if (uiVerify(CSystem::IsThisThreadId(SYS_THREAD_UPDATE) && uiVerify(m_movie.IsActive())))
	{
		if (uiVerifyf(0<=iPlayerId && iPlayerId<MAX_PLAYERS_WITH_GAMER_TAGS, "CMultiplayerGamerTagHud::UT_SetAllGamerTagsVisibility - iPlayerId=%d, max=%d", iPlayerId, MAX_PLAYERS_WITH_GAMER_TAGS))
		{
			if(uiVerifyf(m_gamerTags[iPlayerId].IsStateActive() || m_gamerTags[iPlayerId].IsStateInitPending(), "CMultiplayerGamerTagHud::UT_SetAllGamerTagsVisibility - Gametag for player %d isn't active yet. removePending=%d, destroyPending=%d", iPlayerId, m_gamerTags[iPlayerId].IsStateRemovePending(), m_gamerTags[iPlayerId].IsStateDestroyPending()))
			{
				uiDebugf2("CMultiplayerGamerTagHud::UT_SetAllGamerTagsVisibility - iPlayerId=%d, bSetVisible=%d", iPlayerId, bSetVisible);
				for(int iTag=0; iTag<MAX_MP_TAGS; ++iTag)
				{
					if (m_gamerTags[iPlayerId].m_visibleFlag[iTag] != bSetVisible)
					{
						m_gamerTags[iPlayerId].m_visibleFlag[iTag] = bSetVisible;
						m_gamerTags[iPlayerId].m_TagReinitFlags.GetWriteBuf().Set(iTag);
						m_gamerTags[iPlayerId].SetStateReinit();
					}
				}
	
				// If the crew tag is empty, don't show it, even if script says to.
				if(m_gamerTags[iPlayerId].m_crewTag[0] == '\0')
				{
					m_gamerTags[iPlayerId].m_visibleFlag[MP_TAG_CREW_TAG] = false;
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMultiplayerGamerTagHud::UT_SetGamerTagsCanRenderWhenPaused
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CMultiplayerGamerTagHud::UT_SetGamerTagsCanRenderWhenPaused(s32 iPlayerId, bool bCanRenderWhenPaused)
{
	if (uiVerify(CSystem::IsThisThreadId(SYS_THREAD_UPDATE) && uiVerify(m_movie.IsActive())))
	{
		if (uiVerifyf(0<=iPlayerId && iPlayerId<MAX_PLAYERS_WITH_GAMER_TAGS, "CMultiplayerGamerTagHud::UT_SetGamerTagsCanRenderWhenPaused - iPlayerId=%d, max=%d", iPlayerId, MAX_PLAYERS_WITH_GAMER_TAGS))
		{
			if(uiVerifyf(m_gamerTags[iPlayerId].IsStateActive() || m_gamerTags[iPlayerId].IsStateInitPending(), "CMultiplayerGamerTagHud::UT_SetGamerTagsCanRenderWhenPaused - Gametag for player %d isn't active yet. removePending=%d, destroyPending=%d", iPlayerId, m_gamerTags[iPlayerId].IsStateRemovePending(), m_gamerTags[iPlayerId].IsStateDestroyPending()))
			{
				uiDebugf2("CMultiplayerGamerTagHud::UT_SetGamerTagsCanRenderWhenPaused - iPlayerId=%d, bCanRenderWhenPaused=%d", iPlayerId, bCanRenderWhenPaused);
				m_gamerTags[iPlayerId].m_renderWhenPaused = bCanRenderWhenPaused;
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMultiplayerGamerTagHud::UT_SetGamerTagsCanRenderWhenPaused
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CMultiplayerGamerTagHud::UT_SetGamerTagsShouldUseVehicleHealth(s32 iPlayerId, bool bUseVehicleHealth)
{
	if (uiVerify(CSystem::IsThisThreadId(SYS_THREAD_UPDATE) && uiVerify(m_movie.IsActive())))
	{
		if (uiVerifyf(0<=iPlayerId && iPlayerId<MAX_PLAYERS_WITH_GAMER_TAGS, "CMultiplayerGamerTagHud::UT_SetGamerTagsShouldUseVehicleHealth - iPlayerId=%d, max=%d", iPlayerId, MAX_PLAYERS_WITH_GAMER_TAGS))
		{
			if(uiVerifyf(m_gamerTags[iPlayerId].IsStateActive() || m_gamerTags[iPlayerId].IsStateInitPending(), "CMultiplayerGamerTagHud::UT_SetGamerTagsShouldUseVehicleHealth - Gametag for player %d isn't active yet. removePending=%d, destroyPending=%d", iPlayerId, m_gamerTags[iPlayerId].IsStateRemovePending(), m_gamerTags[iPlayerId].IsStateDestroyPending()))
			{
				if(m_gamerTags[iPlayerId].m_useVehicleHealth != bUseVehicleHealth)
				{
					uiDebugf2("CMultiplayerGamerTagHud::UT_SetGamerTagsShouldUseVehicleHealth - iPlayerId=%d, bUseVehicleHealth=%d", iPlayerId, bUseVehicleHealth);
					m_gamerTags[iPlayerId].m_useVehicleHealth = bUseVehicleHealth;
				}
			}
		}
	}
}

void CMultiplayerGamerTagHud::UT_SetGamerTagsShouldUsePointHealth(s32 iPlayerId, bool bUsePointHealth)
{
	if (uiVerify(CSystem::IsThisThreadId(SYS_THREAD_UPDATE) && uiVerify(m_movie.IsActive())))
	{
		if (uiVerifyf(0<=iPlayerId && iPlayerId<MAX_PLAYERS_WITH_GAMER_TAGS, "CMultiplayerGamerTagHud::UT_SetGamerTagsShouldUsePointHealth - iPlayerId=%d, max=%d", iPlayerId, MAX_PLAYERS_WITH_GAMER_TAGS))
		{
			if(uiVerifyf(m_gamerTags[iPlayerId].IsStateActive() || m_gamerTags[iPlayerId].IsStateInitPending(), "CMultiplayerGamerTagHud::UT_SetGamerTagsShouldUsePointHealth - Gametag for player %d isn't active yet. removePending=%d, destroyPending=%d", iPlayerId, m_gamerTags[iPlayerId].IsStateRemovePending(), m_gamerTags[iPlayerId].IsStateDestroyPending()))
			{
				if(m_gamerTags[iPlayerId].m_bUsePointHealth != bUsePointHealth)
				{
					uiDebugf2("CMultiplayerGamerTagHud::UT_SetGamerTagsShouldUsePointHealth - iPlayerId=%d, bUsePointHealth=%d", iPlayerId, bUsePointHealth);
					m_gamerTags[iPlayerId].m_bUsePointHealth = bUsePointHealth;

					if(!bUsePointHealth)
					{
						m_gamerTags[iPlayerId].m_iPoints = -1;
						m_gamerTags[iPlayerId].m_iMaxPoints = -1;
					}
				}
			}
		}
	}
}

void CMultiplayerGamerTagHud::UT_SetGamerTagsPointHealth(s32 iPlayerId, int iPoints, int iMaxPoints)
{
	if (uiVerify(CSystem::IsThisThreadId(SYS_THREAD_UPDATE) && uiVerify(m_movie.IsActive())))
	{
		if (uiVerifyf(0<=iPlayerId && iPlayerId<MAX_PLAYERS_WITH_GAMER_TAGS, "CMultiplayerGamerTagHud::UT_SetGamerTagsPointHealth - iPlayerId=%d, max=%d", iPlayerId, MAX_PLAYERS_WITH_GAMER_TAGS))
		{
			if(uiVerifyf(m_gamerTags[iPlayerId].IsStateActive() || m_gamerTags[iPlayerId].IsStateInitPending(), "CMultiplayerGamerTagHud::UT_SetGamerTagsPointHealth - Gametag for player %d isn't active yet. removePending=%d, destroyPending=%d", iPlayerId, m_gamerTags[iPlayerId].IsStateRemovePending(), m_gamerTags[iPlayerId].IsStateDestroyPending()))
			{
				if(m_gamerTags[iPlayerId].m_bUsePointHealth)
				{
					uiDebugf2("CMultiplayerGamerTagHud::UT_SetGamerTagsPointHealth - iPlayerId=%d, iPoints=%d, iMaxPoints=%d", iPlayerId, iPoints, iMaxPoints);

					m_gamerTags[iPlayerId].m_iPoints = iPoints;
					m_gamerTags[iPlayerId].m_iMaxPoints = iMaxPoints;
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMultiplayerGamerTagHud::UT_SetGamerTagValueInt
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CMultiplayerGamerTagHud::UT_SetGamerTagValueInt(s32 iPlayerId, eMP_TAG iTag, s32 iValue)
{
	if (uiVerify(CSystem::IsThisThreadId(SYS_THREAD_UPDATE) && uiVerify(m_movie.IsActive())))
	{
		if (uiVerifyf(0<=iPlayerId && iPlayerId<MAX_PLAYERS_WITH_GAMER_TAGS, "CMultiplayerGamerTagHud::UT_SetGamerTagValueInt - iPlayerId=%d, max=%d", iPlayerId, MAX_PLAYERS_WITH_GAMER_TAGS) &&
			uiVerifyf(iTag < MAX_MP_TAGS, "CMultiplayerGamerTagHud::UT_SetGamerTagValueInt - iTag=%d, max=%d", iTag, MAX_MP_TAGS))
		{
			if(uiVerifyf(m_gamerTags[iPlayerId].IsStateActive() || m_gamerTags[iPlayerId].IsStateInitPending(), "CMultiplayerGamerTagHud::UT_SetGamerTagValueInt - Gametag for player %d isn't active yet. removePending=%d, destroyPending=%d", iPlayerId, m_gamerTags[iPlayerId].IsStateRemovePending(), m_gamerTags[iPlayerId].IsStateDestroyPending()))
			{
				if (m_gamerTags[iPlayerId].m_value[iTag] != iValue)
				{
					m_gamerTags[iPlayerId].m_value[iTag] = iValue;
					m_gamerTags[iPlayerId].m_TagReinitFlags.GetWriteBuf().Set(iTag);
					m_gamerTags[iPlayerId].SetStateReinit();
				}
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMultiplayerGamerTagHud::SetGamerTagValueHudColourOnGfxValue
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CMultiplayerGamerTagHud::UT_SetGamerTagValueHudColour(s32 iPlayerId, eMP_TAG iTag, eHUD_COLOURS iHudColour)
{
	if (uiVerify(CSystem::IsThisThreadId(SYS_THREAD_UPDATE) && uiVerify(m_movie.IsActive())))
	{
		// Don't mess with the color of these.
		switch(iTag)
		{
		case MP_TAG_HEALTH_BAR:
		case MP_TAG_CREW_TAG:
			uiDebugf2("CMultiplayerGamerTagHud::UT_SetGamerTagValueHudColour - Ignoring color change for iPlayerId=%d, iHudColour=%d, tag=%s(%d)", iPlayerId, iHudColour, m_GamerTagNames[iTag], iTag);
			break;

		default:
			if (uiVerifyf(0<=iPlayerId && iPlayerId<MAX_PLAYERS_WITH_GAMER_TAGS, "CMultiplayerGamerTagHud::UT_SetGamerTagValueHudColour - iPlayerId=%d, max=%d", iPlayerId, MAX_PLAYERS_WITH_GAMER_TAGS) &&
				uiVerifyf(iTag < MAX_MP_TAGS, "CMultiplayerGamerTagHud::UT_SetGamerTagValueHudColour - iTag=%d, max=%d", iTag, MAX_MP_TAGS))
			{
				if(uiVerifyf(m_gamerTags[iPlayerId].IsStateActive() || m_gamerTags[iPlayerId].IsStateInitPending(), "CMultiplayerGamerTagHud::UT_SetGamerTagValueHudColour - Gametag for player %d isn't active yet. removePending=%d, destroyPending=%d", iPlayerId, m_gamerTags[iPlayerId].IsStateRemovePending(), m_gamerTags[iPlayerId].IsStateDestroyPending()) &&
					m_gamerTags[iPlayerId].m_colour[iTag] != iHudColour)
				{
					m_gamerTags[iPlayerId].m_colour[iTag] = iHudColour;
					m_gamerTags[iPlayerId].m_TagReinitFlags.GetWriteBuf().Set(iTag);
					m_gamerTags[iPlayerId].SetStateReinit();
				}
			}

			break;
		};
	}
}

void CMultiplayerGamerTagHud::UT_SetGamerTagHealthBarColour(s32 iPlayerId, eHUD_COLOURS iHudColour)
{
	if (uiVerify(CSystem::IsThisThreadId(SYS_THREAD_UPDATE) && uiVerify(m_movie.IsActive())))
	{
		if (uiVerifyf(0<=iPlayerId && iPlayerId<MAX_PLAYERS_WITH_GAMER_TAGS, "CMultiplayerGamerTagHud::UT_SetGamerTagHealthBarColour - iPlayerId=%d, max=%d", iPlayerId, MAX_PLAYERS_WITH_GAMER_TAGS))
		{
			if(uiVerifyf(m_gamerTags[iPlayerId].IsStateActive() || m_gamerTags[iPlayerId].IsStateInitPending(), "CMultiplayerGamerTagHud::UT_SetGamerTagValueHudColour - Gametag for player %d isn't active yet. removePending=%d, destroyPending=%d", iPlayerId, m_gamerTags[iPlayerId].IsStateRemovePending(), m_gamerTags[iPlayerId].IsStateDestroyPending()))
			{
				m_gamerTags[iPlayerId].m_healthBarColour = iHudColour;
				m_gamerTags[iPlayerId].m_healthBarColourDirty.GetWriteBuf() = true;
				m_gamerTags[iPlayerId].SetStateReinit();
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMultiplayerGamerTagHud::SetGamerTagValueHudAlpha
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CMultiplayerGamerTagHud::UT_SetGamerTagValueHudAlpha(s32 iPlayerId, eMP_TAG iTag, s32 iAlpha)
{	
	if (uiVerify(CSystem::IsThisThreadId(SYS_THREAD_UPDATE) && uiVerify(m_movie.IsActive())))
	{
		if (uiVerifyf(0<=iPlayerId && iPlayerId<MAX_PLAYERS_WITH_GAMER_TAGS, "CMultiplayerGamerTagHud::UT_SetGamerTagValueHudAlpha - iPlayerId=%d, max=%d", iPlayerId, MAX_PLAYERS_WITH_GAMER_TAGS) &&
			uiVerifyf(iTag < MAX_MP_TAGS, "CMultiplayerGamerTagHud::UT_SetGamerTagValueHudAlpha - iTag=%d, max=%d", iTag, MAX_MP_TAGS))
		{
			float fAlpha = (float)iAlpha;
			fAlpha *= 100.0f / 255.0f;
			uiAssertf(0.0f <= fAlpha && fAlpha <= 100.0f, "CMultiplayerGamerTagHud::UT_SetGamerTagValueHudAlpha - fAlpha is out of range for iPlayerId=%d, fAlpha=%f, tag=%s(%d)", iPlayerId, fAlpha, m_GamerTagNames[iTag], iTag);
			fAlpha = MIN(fAlpha, 100.0f);
			fAlpha = MAX(fAlpha, 0.0f);

			u8 uAlpha = (u8)fAlpha;

			if(uiVerifyf(m_gamerTags[iPlayerId].IsStateActive() || m_gamerTags[iPlayerId].IsStateInitPending(), "CMultiplayerGamerTagHud::UT_SetGamerTagValueHudAlpha - Gametag for player %d isn't active yet. removePending=%d, destroyPending=%d", iPlayerId, m_gamerTags[iPlayerId].IsStateRemovePending(), m_gamerTags[iPlayerId].IsStateDestroyPending()) &&
				m_gamerTags[iPlayerId].m_alpha[iTag] != uAlpha)
			{
				uiDebugf2("CMultiplayerGamerTagHud::UT_SetGamerTagValueHudAlpha - iPlayerId=%d, uAlpha=%u, tag=%s(%d)", iPlayerId, uAlpha, m_GamerTagNames[iTag], iTag);
	
				if(uAlpha == 0 || m_gamerTags[iPlayerId].m_alpha[iTag] == 0)
				{
					m_gamerTags[iPlayerId].m_reinitPosDueToAlpha = true;
				}
	
				m_gamerTags[iPlayerId].m_updateAlpha[iTag] = true;
				m_gamerTags[iPlayerId].m_alpha[iTag] = uAlpha;
				m_gamerTags[iPlayerId].m_TagReinitFlags.GetWriteBuf().Set(iTag);
				m_gamerTags[iPlayerId].SetStateReinit();
			}
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMultiplayerGamerTagHud::SetGamerTagValue
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CMultiplayerGamerTagHud::RT_SetGamerTagValueIntOnGfxValue(s32 iPlayerId, eMP_TAG iTag, s32 iValue, GFxValue& centerIcons, GFxValue& leftIcons)
{
	if (!uiVerify(m_movie.IsActive()))
		return;

	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		uiAssertf(0, "CMultiplayerGamerTagHud::RT_SetGamerTagValueIntOnGfxValue - can only be called on the RenderThread!");
		return;
	}

	if (iPlayerId < MAX_PLAYERS_WITH_GAMER_TAGS &&
		iTag < MAX_MP_TAGS && 
		(m_rtTagFlags[iTag] & TF_SHOULD_EXIST) != 0)
	{
		GFxValue componentGfxValue;

		RT_GetComponentGfxValue(&componentGfxValue, iPlayerId, iTag, centerIcons, leftIcons);
		RT_SetGamerTagValueIntOnGfxValue(componentGfxValue, iTag, iValue);
	}
}

void CMultiplayerGamerTagHud::RT_SetGamerTagValueIntOnGfxValue(GFxValue& componentGfxValue, eMP_TAG iTag, s32 iValue)
{
	if (!uiVerify(m_movie.IsActive()))
		return;

	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		uiAssertf(0, "CMultiplayerGamerTagHud::RT_SetGamerTagValueIntOnGfxValue - can only be called on the RenderThread!");
		return;
	}

	if (!componentGfxValue.IsUndefined() &&
		iTag < MAX_MP_TAGS && 
		(m_rtTagFlags[iTag] & TF_SHOULD_EXIST) != 0)
	{
		switch (iTag)
		{
			case MP_TAG_WANTED_LEVEL:
			{
				componentGfxValue.GotoAndStop(iValue+1);
				break;
			}
			case MP_TAG_PACKAGE_LARGE:
			{
				// Get TextField child of parent MovieClip
				GFxValue textChildGfxValue;
				if(uiVerify(componentGfxValue.GetMember("BAG_NUMBER", &textChildGfxValue)))
				{
					// Format int as string
					char finalString[16];
					formatf(finalString, "%d", iValue);

					// Push string to TextField
					textChildGfxValue.SetText(finalString);
				}
				break;
			}
			//case MP_TAG_ROLE:
			//{
			//	componentGfxValue.GotoAndStop(iValue+1);  // starts on frame 2
			//	break;
			//}
			//case MP_TAG_RANK_ICON:
			//{
			//	if(sfVerifyf(iValue > 0 && iValue < 4, "Rank icon type value is not between 1 and 3"))
			//	{
			//		componentGfxValue.GotoAndStop(iValue);
			//	}
			//	break;
			//}
			case MP_TAG_RANK:
			{
				if(sfVerifyf(iValue >= 0, "Rank value %d is invalid", iValue))
				{
					char finalString[32];
					CNumberWithinMessage pArrayOfNumbers[1];
					pArrayOfNumbers[0].Set(iValue);
					CMessages::InsertNumbersAndSubStringsIntoString(TheText.Get("TR_RANKNUM"), pArrayOfNumbers, 1, NULL, 0, finalString, sizeof(finalString));
					componentGfxValue.SetText(finalString);
				}
				break;
			}
			default:
			{
				uiAssertf(0, "GamerTag: Component %d not valid", (s32)iTag);
			}
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMultiplayerGamerTagHud::SetGamerTagValueHudColourOnGfxValue
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CMultiplayerGamerTagHud::RT_SetGamerTagValueHudColourOnGfxValue(GFxValue& componentGfxValue, s32 OUTPUT_ONLY(iPlayerId), eMP_TAG OUTPUT_ONLY(iTag), eHUD_COLOURS iHudColour)
{
	if (!uiVerify(m_movie.IsActive()))
		return;

	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		uiAssertf(0, "CMultiplayerGamerTagHud::RT_SetGamerTagValueHudColourOnGfxValue - can only be called on the RenderThread!");
		return;
	}

	uiDebugf2("CMultiplayerGamerTagHud::RT_SetGamerTagValueHudColourOnGfxValue - iPlayerId=%d, HudColor=%d, tag=%s(%d)", iPlayerId, iHudColour, m_GamerTagNames[iTag], iTag);
	CRGBA hudColour = CHudColour::GetRGB(iHudColour, 100);
	Color32 iColour(hudColour.GetColor());
	RT_SetColourToGfxvalue(&componentGfxValue, iColour);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMultiplayerGamerTagHud::SetGamerTagValueHudAlphaOnGfxValue
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CMultiplayerGamerTagHud::RT_SetGamerTagValueHudAlphaOnGfxValue(GFxValue& componentGfxValue, s32 OUTPUT_ONLY(iPlayerId), eMP_TAG OUTPUT_ONLY(iTag), s32 iAlpha)
{
	if (!uiVerify(m_movie.IsActive()))
		return;

	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		uiAssertf(0, "CMultiplayerGamerTagHud::RT_SetGamerTagValueHudAlphaOnGfxValue - can only be called on the RenderThread!");
		return;
	}

	uiDebugf2("CMultiplayerGamerTagHud::RT_SetGamerTagValueHudAlphaOnGfxValue - iPlayerId=%d, iAlpha=%d, tag=%s(%d)", iPlayerId, iAlpha, m_GamerTagNames[iTag], iTag);

	GFxValue::DisplayInfo newDisplayInfo;
	newDisplayInfo.SetAlpha(iAlpha);
	componentGfxValue.SetDisplayInfo(newDisplayInfo);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMultiplayerGamerTagHud::Update
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CMultiplayerGamerTagHud::Update()
{
	if(SMultiplayerGamerTagHud::IsInstantiated())
	{
		SMultiplayerGamerTagHud::GetInstance().UT_UpdateHelper();
	}
}


s32 CMultiplayerGamerTagHud::UT_GetLocalPlayerTag()
{
	CPhysical *pLocalPlayer = CGameWorld::FindLocalPlayer();

	CMultiplayerGamerTagHud& rHud = SMultiplayerGamerTagHud::GetInstance();
	if (rHud.m_movie.IsActive())
	{
		for (s32 i = 0; i < MAX_PLAYERS_WITH_GAMER_TAGS; i++)
		{
			sGamerTag& rGamerTag = rHud.m_gamerTags[i];

			if (rGamerTag.IsStateActive() || rGamerTag.IsStateInitPending())
			{
				if (rHud.UT_GetPhysical(i) == pLocalPlayer)
				{
					return i;
				}
			}
		}
	}

	return GAMER_TAG_INVALID_INDEX;
}

void CMultiplayerGamerTagHud::UT_UpdateHelper()
{
	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on UT
	{
		uiAssertf(0, "CMultiplayerGamerTagHud::UT_UpdateHelper - can only be called on the UpdateThread!");
		return;
	}

	if(!m_rootCreated)
	{
		if (m_movie.IsActive())
		{
			CScaleformMgr::AutoLock lock(m_movie.GetMovieID());

			UT_CreateRoot();			
		}

		if(!m_rootCreated)
		{
			return;
		}
	}

#if __BANK
	UpdateDebugFakePassengerTags();

	if(bDebugAddToPeds)
	{
		CPed* pNewPed = CPedFactory::GetLastCreatedPed();
		if(pNewPed)
		{
			bool bFound = false;
			s32 emptyIndex = -1;
			s32 tagsUsed = 0;
			static s32 maxTagsUsed = 0;
			for (s32 i = 0; i < MAX_PLAYERS_WITH_GAMER_TAGS; i++)
			{
				CPhysical* pCurr = UT_GetPhysical(i);
				if(pCurr)
				{
					tagsUsed++;
					if(pNewPed == pCurr)
					{
						bFound = true;
					}
				}
				else
				{
					emptyIndex = i;
				}
			}

			if(!bFound && emptyIndex != -1 && tagsUsed < 16)
			{
				char crewTagBuffer[NetworkClan::FORMATTED_CLAN_TAG_LEN] = {0};
				NetworkClan::GetUIFormattedClanTag(!bDebugIsCrewTagPrivate, bDebugCrewTagContainsRockstar, cDebugClanTag, iDebugCrewRank, CRGBA(uDebugCrewR, uDebugCrewG, uDebugCrewB), crewTagBuffer, NetworkClan::FORMATTED_CLAN_TAG_LEN);

				UT_CreatePlayerTagHelper( emptyIndex, pNewPed, cDebugGamerTag, crewTagBuffer );
				UT_SetGamerTagVisibility( emptyIndex, MP_TAG_GAMER_NAME, true );
			}

			maxTagsUsed = MAX(maxTagsUsed, tagsUsed);
			uiDisplayf("CMultiplayerGamerTagHud::UT_UpdateHelper - bDebugAddToPeds=true, tagsUsed=%d, maxTagsUsed=%d", tagsUsed, maxTagsUsed);
		}
	}
#endif

	bool showAtFarDistance = NetworkInterface::IsInSpectatorMode();
	if(!showAtFarDistance)
	{
		CPed *pPlayerPed = CGameWorld::FindLocalPlayer();

		if(pPlayerPed && pPlayerPed->GetIsInVehicle() && pPlayerPed->GetMyVehicle()->GetIsAircraft())
		{
			showAtFarDistance = true;
		}

		if(!showAtFarDistance && camInterface::GetGameplayDirector().IsFirstPersonAiming())
		{
			if(pPlayerPed->GetWeaponManager() && pPlayerPed->GetWeaponManager()->GetEquippedWeaponHasFirstPersonScope())
			{
				showAtFarDistance = true;
			}
		}

		if(CScriptHud::ms_bFakeSpectatorMode)
		{
			showAtFarDistance = true;
		}

		if(CNewHud::GetDisplayMode() == CNewHud::DM_ARCADE_CNC)
		{
			showAtFarDistance = true;
		}
	}

	bool const bIsPaused = CPauseMenu::IsActive();
	bool const c_isFpCam = camInterface::IsDominantRenderedCameraAnyFirstPersonCamera();

	CPed const* playerPed = CPedFactory::GetFactory()->GetLocalPlayer();	
	CVehicle const * playerVehicle = playerPed ? playerPed->GetMyVehicle() : NULL;
	bool const c_isPlayerInAircraft = playerVehicle ? playerVehicle->GetIsAircraft() : false;

	for (s32 i = 0; i < MAX_PLAYERS_WITH_GAMER_TAGS; i++)
	{
		sGamerTag& rGamerTag = m_gamerTags[i];

		if (rGamerTag.IsStateDestroyPending())
		{
			CScaleformMgr::AutoLock lock(m_movie.GetMovieID());
			rGamerTag.Reset();

			m_movie.ForceCollectGarbage();// perform garbage collection once the gamertag has been removed

			uiDisplayf("CMultiplayerGamerTagHud::UT_UpdateHelper - Gamertag for player %d has been removed", i);
		}
		else if (rGamerTag.IsStateActive() || rGamerTag.IsStateInitPending())
		{
			if(!bIsPaused || rGamerTag.m_renderWhenPaused)
			{
				rGamerTag.m_visible.GetWriteBuf() = false;

				CPhysical const* pThisPhys = UT_GetPhysical(i);
				if (pThisPhys)  // if we have a ped to get the position from
				{
					if(c_isFpCam && i == UT_GetLocalPlayerTag())  // if in 1st Person, and this is the local player tag, then we do not render it (fixes 2706081).
					{
						continue;
					}

					int seatIndex = 0;
					bool isHiddenDueToBigVehicle = false;

					CPed const * pThisPed = pThisPhys->GetIsTypePed() ? reinterpret_cast<const CPed*>(pThisPhys) : NULL;
					CEntity const * pEnt = pThisPhys;
					CVehicle const * pVehicle = pThisPhys ? pThisPed->GetMyVehicle() : NULL;

					bool const c_isPlayerInVehicle = pThisPed ? pThisPed->GetIsInVehicle() : false;
					bool const c_isPedInAircraft = pVehicle ? pVehicle->GetIsAircraft() : false;
					bool const c_isPedOnBike = pVehicle ? pVehicle->InheritsFromBike() : false;

					bool const c_adjustForBike = c_isPedOnBike && !c_isFpCam;
					bool const c_adjustForAircraft = c_isPedInAircraft && !c_isFpCam;

					//! If we are in FP, an aircraft and this ped is in the same vehicle as us then adjust the tag based on camera rotation
					bool const c_adjustForEntityRotation = ( c_isFpCam && c_isPlayerInAircraft && pVehicle == playerVehicle );

					// If we're in First-Person in a vehicle, don't do vehicle gamer info adjustments
					if( pThisPed && pThisPed->GetIsInVehicle() && !c_isFpCam )
					{
						if(sm_minSeatCountToGridNames <= pVehicle->GetSeatManager()->CountPedsInSeats(true) BANK_ONLY(|| sm_minSeatCountToGridNames <= uDebugNumFakeVehiclePassengers+1))
						{
							pEnt = pVehicle;
							isHiddenDueToBigVehicle = true;

							const CSeatManager* pSeatManager = pVehicle->GetSeatManager();
							if(pSeatManager)
							{
								seatIndex = pSeatManager->GetPedsSeatIndex(pThisPed);
							}
						}

#if __BANK
						if(iDebugVehicleUsageCount != -1)
						{
							pEnt = pVehicle;
						}

						// Each gamer tag's ped is the player, so the current seatIndex is the local player's seat index.  Let's fake the seat index
						if(rGamerTag.m_debugCloneTagFromPedForVehicle)
						{
							seatIndex = uDebugCurrentFakePassengerSeatIndex;
						}
						uDebugCurrentFakePassengerSeatIndex++;
#endif
					}

#if __BANK
					if(iDebugVehicleUsageCount != -1)
					{
						isHiddenDueToBigVehicle = true;
						seatIndex = iDebugVehicleUsageCount;
					}
#endif

					if( isHiddenDueToBigVehicle != rGamerTag.m_hiddenDueToBigVehicle 
						&& !rGamerTag.m_showEvenInBigVehicles)
					{
						rGamerTag.m_hiddenDueToBigVehicle = isHiddenDueToBigVehicle;
						bool bUsingSpecialHealth = rGamerTag.m_bUsePointHealth || rGamerTag.m_useVehicleHealth;

						for(int visIndex=0; visIndex<MAX_MP_TAGS; ++visIndex)
						{
							if( rGamerTag.m_visibleFlag[visIndex] && 
								visIndex != MP_TAG_GAMER_NAME)
							{
								if(bUsingSpecialHealth && visIndex == MP_TAG_HEALTH_BAR)
								{
									continue;
								}

								rGamerTag.SetStateReinit();
								rGamerTag.m_TagReinitFlags.GetWriteBuf().Set(visIndex);
							}
						}
					}


					float viewableDistance = ( showAtFarDistance || c_adjustForAircraft ) ? sm_maxVisibleSpectatingDistance : sm_maxVisibleDistance;
#if !__FINAL
					if(PARAM_enableFarGamerTagDistance.Get())
					{
						viewableDistance = (sm_maxVisibleDistance < sm_maxVisibleSpectatingDistance) ? sm_maxVisibleSpectatingDistance : sm_maxVisibleDistance;
					}
#endif

					Vector3 offsetPosition = Vector3( 0, 0, pEnt->GetBaseModelInfo() ? pEnt->GetBaseModelInfo()->GetBoundingBoxMax().z : 0.82f );
					offsetPosition.z += GetGameWorldOffset( rGamerTag.m_hiddenDueToBigVehicle, c_adjustForBike );

					rGamerTag.m_playerPos.GetWriteBuf() = VEC3V_TO_VECTOR3(pEnt->GetTransform().GetPosition());

					u32 uVehicleHash = pVehicle ? pVehicle->GetModelNameHash() : 0;

					// Hack to fix the height of the gamer tags when people are in the turret limo (LIMO2)
					const u32 DLC_LIMO_HASH = 4180339789;
					const float fLimoOffset = -0.35f;
					if(rGamerTag.m_hiddenDueToBigVehicle && uVehicleHash == DLC_LIMO_HASH)
					{
						offsetPosition.z += fLimoOffset;
					}

					// Vehciles that requires extra width spacing
					float fExtraVehicleSpacing = 0.0f;
					if(c_isPlayerInVehicle)
					{
						const u32 DLC_DUNE_FAV = 1897744184;
						const u32 RAMP_DUNE = 3467805257;
						const u32 RAMP_DUNE_2 = 3982671785;
						const u32 DUNE_SCIFI = 534258863;
						const u32 REG_DUNE = 2633113103;
						const float fDuneFAVOffset = 0.015f;
						if( uVehicleHash == DLC_DUNE_FAV ||
							uVehicleHash == RAMP_DUNE ||
							uVehicleHash == RAMP_DUNE_2 ||
							uVehicleHash == DUNE_SCIFI ||
							uVehicleHash == REG_DUNE )
						{
							fExtraVehicleSpacing = fDuneFAVOffset;
						}
					}

					if( c_adjustForEntityRotation )
					{
						Matrix34 mat;
						pEnt->GetMatrixCopy(mat);
						mat.Transform3x3( offsetPosition );
					}

					rGamerTag.m_visible.GetWriteBuf() = NetworkUtils::GetScreenCoordinatesForOHD( rGamerTag.m_playerPos.GetWriteBuf(), rGamerTag.m_screenPos.GetWriteBuf(), rGamerTag.m_scale.GetWriteBuf(),
						offsetPosition,
						viewableDistance,
						sm_nearScalingDistance, sm_farScalingDistance, sm_maxScaleSize, sm_minScaleSize );

					rGamerTag.m_screenPos.GetWriteBuf() += GetScreenOffset( rGamerTag.m_hiddenDueToBigVehicle, seatIndex, c_adjustForBike, fExtraVehicleSpacing );
					rGamerTag.m_screenPos.GetWriteBuf().x = CHudTools::CovertToCenterAtAspectRatio(rGamerTag.m_screenPos.GetWriteBuf().x, true);

					if(rGamerTag.m_bUsePointHealth)
					{
						int iCurrentPoints = rGamerTag.m_iPoints;
						int iMaxPoints = rGamerTag.m_iMaxPoints;

#if __BANK
						if(iDebugCurrentPoints >= 0 &&
							iDebugMaxPoints >= 0)
						{
							iCurrentPoints = iDebugCurrentPoints;
							iMaxPoints = iDebugMaxPoints;
						}
#endif // __BANK

						if( iCurrentPoints >= 0 &&
							iMaxPoints >= 0 && iMaxPoints >= iCurrentPoints)
						{							
							int iTeam = int(pThisPed->GetPlayerInfo()->GetArcadeInformation().GetTeam());
							rGamerTag.m_healthInfo.GetWriteBuf().Set(iCurrentPoints, iMaxPoints, 0, 0, 0, 0, iMaxPoints, 0, 0, iTeam, NULL, true);
						}
					}
					else
					{
						if(pVehicle && rGamerTag.m_useVehicleHealth)
						{
							CNewHud::GetHealthInfo(pVehicle, rGamerTag.m_healthInfo.GetWriteBuf());
						}
						else
						{
							CNewHud::GetHealthInfo(pThisPhys, rGamerTag.m_healthInfo.GetWriteBuf());
						}
					}

					rGamerTag.ClearNeedsPositionUpdate();
				}
				else
				{
					UT_RemovePlayerTag(i);
					uiDebugf1("CMultiplayerGamerTagHud::UT_UpdateHelper - MP Tag %d deleted as attached ped no longer exists", i);
				}
			}
		}
	}
}

float CMultiplayerGamerTagHud::GetGameWorldOffset(bool bHiddenDueToBigVehicle, bool bIsPedOnBike)
{
	float heightOffset = 0.f;

	if(bHiddenDueToBigVehicle)
	{
		heightOffset += sm_defaultVehicleOffset;
		if(bIsPedOnBike)
		{
			heightOffset += sm_bikeOffset;
		}
	}

	return heightOffset;
}

Vector2 CMultiplayerGamerTagHud::GetScreenOffset(bool bHiddenDueToBigVehicle, int seatIndex, bool bIsPedOnBike, float fExtraVehicleSpacing)
{
	Vector2 vOffset(0.0f, 0.0f);

	if(bHiddenDueToBigVehicle)
	{
		if(bIsPedOnBike)
		{
			vOffset.y += seatIndex * sm_vehicleYSpacing;
		}
		else
		{
			float fAspect = CHudTools::GetAspectRatio();
			float fExtraAspectSpace = fAspect < (16.0f/9.0f) ? 0.02f : 0.0f;
			float fXSpacing = sm_vehicleXSpacing + fExtraAspectSpace + fExtraVehicleSpacing;

			vOffset.x += (seatIndex%2 == 0) ? -fXSpacing : fXSpacing;
			vOffset.y += (int)(seatIndex/2) * sm_vehicleYSpacing;
		}
	}

	vOffset.y += sm_fVerticalOffset;

	return vOffset;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMultiplayerGamerTagHud::GetPed
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
CPhysical* CMultiplayerGamerTagHud::UT_GetPhysical(int iPlayerId)
{
	CPhysical *pPhys = NULL;

	if(uiVerify(CSystem::IsThisThreadId(SYS_THREAD_UPDATE)))
	{
		if (iPlayerId < MAX_NUM_PHYSICAL_PLAYERS)
		{
			// ensure the ped we are going to use is still valid in MP code/script
			CNetGamePlayer* pNetPlayer = NetworkInterface::GetPhysicalPlayerFromIndex(static_cast<PhysicalPlayerIndex>(iPlayerId));

			if (pNetPlayer && pNetPlayer->GetPlayerPed() && pNetPlayer->IsPhysical() && pNetPlayer->IsValid() && (!pNetPlayer->IsLeaving()))
			{
				pPhys = pNetPlayer->GetPlayerPed();
			}
		}
		else
		{
			pPhys = m_gamerTags[iPlayerId].m_utPhysical;  // if a NPC then use the ped pointer stored
		}
	}

	return pPhys;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMultiplayerGamerTagHud::Render
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CMultiplayerGamerTagHud::Render()
{
	if(SMultiplayerGamerTagHud::IsInstantiated())
	{
		SMultiplayerGamerTagHud::GetInstance().RT_RenderHelper();
	}
}

void CMultiplayerGamerTagHud::RT_RenderHelper()
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		uiAssertf(0, "CMultiplayerGamerTagHud::RT_RenderHelper - can only be called on the RenderThread!");
		return;
	}

	m_renderingSection.Lock();

	if (!IsMovieActive())
	{
		m_renderingSection.Unlock();
		return;
	}

	PF_PUSH_TIMEBAR("CMultiplayerGamerTagHud::RT_RenderHelper");

	bool bAnyMPHudTagsInUse = false;

	bool bIsPaused = CPauseMenu::IsActive() || PAUSEMENUPOSTFXMGR.IsFading();

	RT_SortGamerTags();

	for (s32 i = 0; i < MAX_PLAYERS_WITH_GAMER_TAGS; i++)
	{
		if(m_gamerTags[i].IsStateRemovePending())
		{
			RT_RemoveGfxValueForGamerTag(i);
		}
		else if (m_gamerTags[i].IsStateInitPending())
		{
			if (CNewHud::GetDisplayMode() == CNewHud::DM_ARCADE_CNC)
			{
#if ARCHIVED_SUMMER_CONTENT_ENABLED      
				RT_SetupGfxValueForGamerTag(i, GAMER_INFO_FILENAME_CNC);
#endif
			}
			else
			{
				RT_SetupGfxValueForGamerTag(i, GAMER_INFO_FILENAME);
			}
		}
			
		if (m_gamerTags[i].IsStateActive())
		{
			if(uiVerifyf(m_gamerTags[i].m_asGamerTagContainerMc.HasMember(GAMERTAG_CENTER_OBJECT), "CMultiplayerGamerTagHud::RT_InitComponentGfxValue - Gamertag value " GAMERTAG_CENTER_OBJECT " is invalid for player %d!", i) &&
				uiVerifyf(m_gamerTags[i].m_asGamerTagContainerMc.HasMember(GAMERTAG_LEFT_OBJECT), "CMultiplayerGamerTagHud::RT_InitComponentGfxValue - Gamertag value " GAMERTAG_LEFT_OBJECT " is invalid for player %d!", i))
			{
				GFxValue centerIcons, leftIcons;
				m_gamerTags[i].m_asGamerTagContainerMc.GetMember(GAMERTAG_CENTER_OBJECT, &centerIcons);
				m_gamerTags[i].m_asGamerTagContainerMc.GetMember(GAMERTAG_LEFT_OBJECT, &leftIcons);

				if (m_gamerTags[i].IsStateReinit())
				{
					RT_ReinitGamerTag(i, centerIcons, leftIcons);
				}

				if(!bIsPaused || m_gamerTags[i].m_renderWhenPaused)
				{
					bAnyMPHudTagsInUse = true;
					RT_UpdateActiveGamerTag(i, centerIcons, leftIcons);
				}
			}
		}
	}

	PF_POP_TIMEBAR();

	PF_PUSH_TIMEBAR("CMultiplayerGamerTagHud - RenderMovie");
	if (bAnyMPHudTagsInUse && CVfxHelper::ShouldRenderInGameUI())
	{	
		m_movie.Render();
	}
	PF_POP_TIMEBAR();

	m_renderingSection.Unlock();
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMultiplayerGamerTagHud::SortGamerTags
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CMultiplayerGamerTagHud::RT_SortGamerTags()
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on UT
	{
		uiAssertf(0, "CMultiplayerGamerTagHud::RT_SortGamerTags - can only be called on the RenderThread!");
		return;
	}

	const Vector3 camPos = camInterface::GetPos();
	const Vector3 camDir = camInterface::GetFront();

	atFixedArray<GamerDepthInfo, MAX_PLAYERS_WITH_GAMER_TAGS> aDepthInfos;

	// Building an array of all of the existing gamertags with their distances for sorting.
	for (s16 i = 0; i < MAX_PLAYERS_WITH_GAMER_TAGS; i++)
	{
		if(m_gamerTags[i].IsStateActive())
		{
			float fDepth = (m_gamerTags[i].m_playerPos.GetReadBuf() - camPos).Dot(camDir);
	
			GamerDepthInfo temp(fDepth, i);
			aDepthInfos.Push(temp);
		}
	}

	aDepthInfos.QSort(0, -1, GamerDepthInfo::DepthCompare);

	// Now that the gamer tags have an order, lets move them to where they belong.
	for (s16 depth = 0; depth < aDepthInfos.size(); depth++)
	{
		s32 indexAtDepth = aDepthInfos[depth].m_playerIndex;
		if(m_gamerTags[indexAtDepth].m_rtDepth != depth)
		{
			uiDebugf1("CMultiplayerGamerTagHud::RT_SortGamerTags - %s was at %d, now at %d", m_gamerTags[indexAtDepth].m_refName, m_gamerTags[indexAtDepth].m_rtDepth, depth);

			m_gamerTags[indexAtDepth].m_rtDepth = depth;
			if (!m_gamerTags[indexAtDepth].m_asGamerTagContainerMc.IsUndefined())
			{
				GFxValue args[1];
				args[0].SetNumber(depth);

				GFxValue result;
				m_gamerTags[indexAtDepth].m_asGamerTagContainerMc.Invoke("swapDepths", &result, args, 1);
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMultiplayerGamerTagHud::RT_SetupGfxValueForGamerTag
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CMultiplayerGamerTagHud::RT_SetupGfxValueForGamerTag(int iPlayerId, const char *cGamerInfoFilename)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on UT
	{
		uiAssertf(0, "CMultiplayerGamerTagHud::RT_SetupGfxValueForGamerTag - can only be called on the RenderThread!");
		return;
	}

	sGamerTag& rGamerTag = m_gamerTags[iPlayerId];

	if (uiVerifyf(rGamerTag.m_asGamerTagContainerMc.IsUndefined(), "CMultiplayerGamerTagHud::RT_SetupGfxValueForGamerTag - Gamertag Value already attached for player %d at setup", iPlayerId))
	{
		// Need to set this to an unique depth so it doesn't delete existing movies
		rGamerTag.m_rtDepth = static_cast<s16>(MAX_PLAYERS_WITH_GAMER_TAGS+iPlayerId);
		
		if (uiVerifyf(m_asGamerTagLayerContainer.GFxValue::AttachMovie(&rGamerTag.m_asGamerTagContainerMc, cGamerInfoFilename, rGamerTag.m_refName, rGamerTag.m_rtDepth),
			"CMultiplayerGamerTagHud::RT_SetupGfxValueForGamerTag - Failed to add a gamertag for player %d", iPlayerId))
		{
			bool wasCreated = false;

			if(uiVerifyf(rGamerTag.m_asGamerTagContainerMc.IsDefined(), "CMultiplayerGamerTagHud::RT_SetupGfxValueForGamerTag - Failed to add a gamertag for player %d", iPlayerId))
			{
				if(uiVerifyf(rGamerTag.m_asGamerTagContainerMc.HasMember(GAMERTAG_CENTER_OBJECT), "CMultiplayerGamerTagHud::RT_SetupGfxValueForGamerTag - Failed to find " GAMERTAG_CENTER_OBJECT " when adding gamer tag for player %d", iPlayerId) && 
					uiVerifyf(rGamerTag.m_asGamerTagContainerMc.HasMember(GAMERTAG_LEFT_OBJECT), "CMultiplayerGamerTagHud::RT_SetupGfxValueForGamerTag - Failed to find " GAMERTAG_LEFT_OBJECT " when adding gamer tag for player %d", iPlayerId))
				{
					GFxValue centerIcons, leftIcons;
					rGamerTag.m_asGamerTagContainerMc.GetMember(GAMERTAG_CENTER_OBJECT, &centerIcons);
					rGamerTag.m_asGamerTagContainerMc.GetMember(GAMERTAG_LEFT_OBJECT, &leftIcons);

					if(RT_InitAndVerifyAllComponentGfxValues(iPlayerId, centerIcons, leftIcons))
					{
						RT_ReinitGamerTagCrewDetails(iPlayerId);

						RT_SetGamerTagValueIntOnGfxValue(iPlayerId, MP_TAG_WANTED_LEVEL,	0, centerIcons, leftIcons);
						RT_SetGamerTagValueIntOnGfxValue(iPlayerId, MP_TAG_RANK,			0, centerIcons, leftIcons);
						RT_SetGamerTagValueIntOnGfxValue(iPlayerId, MP_TAG_PACKAGE_LARGE,	0, centerIcons, leftIcons);

						atFixedArray<GamerDepthInfo, MAX_MP_TAGS> aDepthInfos;

						for (s32 j = 0; j < MAX_MP_TAGS; j++)
						{
							GFxValue componentGfxValue;

							RT_GetComponentGfxValue(&componentGfxValue, iPlayerId, (eMP_TAG)j, centerIcons, leftIcons, false);

							if( componentGfxValue.IsDisplayObject() )
							{
								GFxValue::DisplayInfo newDisplayInfo;
								newDisplayInfo.SetVisible(false);
								componentGfxValue.SetDisplayInfo(newDisplayInfo);

								if(!sm_staticDataInited)
								{
									GFxValue iconWidth;
									componentGfxValue.GetMember("_width", &iconWidth);
									if(iconWidth.IsDefined())
									{
										sm_tagWidths[j] = static_cast<float>(iconWidth.GetNumber());
									}

									float depth = 0.0f;
									GFxValue result;
									componentGfxValue.Invoke("getDepth", &result);

									if(result.IsDefined())
									{
										depth = (float)result.GetNumber();
									}

									GamerDepthInfo temp(depth, (s16)j);
									aDepthInfos.Push(temp);
								}
							}
							else
							{
								uiWarningf("CMultiplayerGamerTagHud::RT_SetupGfxValueForGamerTag - eMP_TAG %d is not a display object", j );
							}
						}
						uiDisplayf("CMultiplayerGamerTagHud::RT_SetupGfxValueForGamerTag - Adding Gamertag for iPlayerId=%d", iPlayerId);

						if(!sm_staticDataInited)
						{
							aDepthInfos.QSort(0, -1, GamerDepthInfo::DepthCompare);

							for(int i=0; i<aDepthInfos.size(); ++i)
							{
								sm_tagOrder[i] = static_cast<eMP_TAG>(aDepthInfos[i].m_playerIndex);
							}
						}

						rGamerTag.SetStateActive();
						wasCreated = true;

						sm_staticDataInited = true;
					}
				}
			}

			if(!wasCreated)
			{
				uiDisplayf("CMultiplayerGamerTagHud::RT_SetupGfxValueForGamerTag - Gamertag for iPlayerId=%d failed to be properly created, will try again.", iPlayerId);

				if (uiVerifyf(rGamerTag.m_asGamerTagContainerMc.IsDefined(), "CMultiplayerGamerTagHud::RT_SetupGfxValueForGamerTag - GamerTag movie for player %d was attached, but not defined.", iPlayerId))
				{
					rGamerTag.m_asGamerTagContainerMc.Invoke("removeMovieClip");
					rGamerTag.m_asGamerTagContainerMc.SetUndefined();
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMultiplayerGamerTagHud::RemoveGfxValueForGamerTag
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CMultiplayerGamerTagHud::RT_RemoveGfxValueForGamerTag(int iPlayerId)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on UT
	{
		uiAssertf(0, "CMultiplayerGamerTagHud::RT_RemoveGfxValueForGamerTag can only be called on the RenderThread!");
		return;
	}

	sGamerTag& rGamerTag = m_gamerTags[iPlayerId];

	if (rGamerTag.m_asGamerTagContainerMc.IsDefined())
	{
		uiDebugf1("CMultiplayerGamerTagHud::RT_RemoveGfxValueForGamerTag - Removing for iPlayerId=%d", iPlayerId);
		rGamerTag.m_asGamerTagContainerMc.Invoke("removeMovieClip");
		rGamerTag.m_asGamerTagContainerMc.SetUndefined();
	}

	rGamerTag.SetStateDestroyPending();
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMultiplayerGamerTagHud::ReinitGamerTag
// PURPOSE:	
/////////////////////////////////////////////////////////////////////////////////////
void CMultiplayerGamerTagHud::RT_ReinitGamerTag(int iPlayerId, GFxValue& centerIcons, GFxValue& leftIcons)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on UT
	{
		uiAssertf(0, "CMultiplayerGamerTagHud::RT_ReinitGamerTag can only be called on the RenderThread!");
		return;
	}

	sGamerTag& rGamerTag = m_gamerTags[iPlayerId];
	rGamerTag.ClearStateReinit();

	for (s32 j = 0; j < MAX_MP_TAGS; j++)
	{
		if(!rGamerTag.m_TagReinitFlags.GetReadBuf().IsSet(j))
		{
			continue;
		}

		GFxValue componentGfxValue;

		RT_GetComponentGfxValue(&componentGfxValue, iPlayerId, (eMP_TAG)j, centerIcons, leftIcons, false);

		if (!componentGfxValue.IsDisplayObject())
		{
			// Not all tags need to exist all the time.
			//uiAssertf(0, "Gamertag Value componentGfxValue is undefined for player %d, tag %d", iPlayerId, (eMP_TAG)j);

			continue;
		}

		GFxValue::DisplayInfo currentDisplayInfo;
		componentGfxValue.GetDisplayInfo(&currentDisplayInfo);

		bool bCurrentVisibleFlag = rGamerTag.m_visibleFlag[j];
		if(rGamerTag.m_hiddenDueToBigVehicle &&
			j!=MP_TAG_GAMER_NAME &&
			j!=MP_TAG_GAMER_NAME_NEARBY &&
			j!=MP_TAG_PASSIVE_MODE)
		{
			bCurrentVisibleFlag = false;
		}

		if (bCurrentVisibleFlag != currentDisplayInfo.GetVisible())
		{
			GFxValue::DisplayInfo newDisplayInfo;
			newDisplayInfo.SetVisible(bCurrentVisibleFlag);
			componentGfxValue.SetDisplayInfo(newDisplayInfo);
			rGamerTag.m_reinitPositions = true;
		}

		s32 iCurrentValue = rGamerTag.m_value[j];
		if (iCurrentValue != -1)
		{
			rGamerTag.m_value[j] = -1;

			RT_SetGamerTagValueIntOnGfxValue(componentGfxValue, (eMP_TAG)j, iCurrentValue);
			rGamerTag.m_reinitPositions = true;
		}

		eHUD_COLOURS CurrentColour = rGamerTag.m_colour[j];
		if (CurrentColour != HUD_COLOUR_INVALID)
		{
			rGamerTag.m_colour[j] = HUD_COLOUR_INVALID;
			RT_SetGamerTagValueHudColourOnGfxValue(componentGfxValue, iPlayerId, (eMP_TAG)j, CurrentColour);
		}

		if (rGamerTag.m_updateAlpha[j])
		{
			rGamerTag.m_updateAlpha[j] = false;

			RT_SetGamerTagValueHudAlphaOnGfxValue(componentGfxValue, iPlayerId, (eMP_TAG)j, (s32)rGamerTag.m_alpha[j]);

			if (rGamerTag.m_reinitPosDueToAlpha)
			{
				rGamerTag.m_reinitPosDueToAlpha = false;
				rGamerTag.m_reinitPositions = true;
			}
		}
	}

	if(rGamerTag.m_healthBarColourDirty.GetReadBuf() && rGamerTag.m_healthBarColour != HUD_COLOUR_INVALID)
	{
		GFxValue componentGfxValue;
		RT_GetComponentGfxValue(&componentGfxValue, iPlayerId, MP_TAG_HEALTH_BAR, centerIcons, leftIcons);
		RT_UpdateHealthColorHelper(componentGfxValue, HEALTH_ARMOUR_NAME, rGamerTag.m_healthBarColour);
		RT_UpdateHealthColorHelper(componentGfxValue, HEALTH_NO_ARMOUR_NAME, rGamerTag.m_healthBarColour);		
#if ARCHIVED_SUMMER_CONTENT_ENABLED
		RT_UpdateHealthColorHelper(componentGfxValue, HEALTH_CNC_NO_ARMOUR_NAME, rGamerTag.m_healthBarColour);
		RT_UpdateHealthColorHelper(componentGfxValue, HEALTH_CNC_NO_ARMOUR_NO_ENDURANCE_NAME, rGamerTag.m_healthBarColour);
#endif		
		rGamerTag.m_healthBarColour = HUD_COLOUR_INVALID;
		rGamerTag.m_healthBarColourDirty.GetRenderBuf() = false;
	}

	rGamerTag.m_TagReinitFlags.GetRenderBuf().Reset();
}

void CMultiplayerGamerTagHud::RT_UpdateActiveGamerTag(int iPlayerId, GFxValue& centerIcons, GFxValue& leftIcons)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on UT
	{
		uiAssertf(0, "CMultiplayerGamerTagHud::Rt_UpdateActiveGamerTag can only be called on the RenderThread!");
		return;
	}

	sGamerTag& rGamerTag = m_gamerTags[iPlayerId];
	if(rGamerTag.IsStateNeedsPositionUpdate())
	{
		return;
	}

	if (!uiVerifyf(rGamerTag.m_asGamerTagContainerMc.IsDefined(), "CMultiplayerGamerTagHud::RT_UpdateActiveGamerTag -  - GamerTag movie for player %d was attached, but not defined.", iPlayerId))
	{
		return;
	}
	
	if(rGamerTag.m_reinitCrewTag)
	{
		RT_ReinitGamerTagCrewDetails(iPlayerId);
		rGamerTag.m_reinitPositions = true;
	}

	if(rGamerTag.m_reinitBigText)
	{
		GFxValue bigText;
		RT_GetComponentGfxValue(&bigText, iPlayerId, MP_TAG_BIG_TEXT, centerIcons, leftIcons, true);
		if(bigText.IsDefined() && uiVerifyf(bigText.IsDisplayObject(), "CMultiplayerGamerTagHud::RT_UpdateActiveGamerTag - The BigText object isn't a display object.  Type=%d", bigText.GetType()))
		{
			bigText.SetText(rGamerTag.m_bigtext);
		}
		rGamerTag.m_reinitBigText = false;
	}

	GFxValue::DisplayInfo newDisplayInfo, currentDisplayInfo;
	rGamerTag.m_asGamerTagContainerMc.GetDisplayInfo(&currentDisplayInfo);

	if(rGamerTag.m_visible.GetReadBuf())
	{
		if ((!currentDisplayInfo.GetVisible()) || currentDisplayInfo.GetX() != rGamerTag.m_screenPos.GetReadBuf().x || currentDisplayInfo.GetY() != rGamerTag.m_screenPos.GetReadBuf().y)
		{
			
			#if RSG_PC || RSG_DURANGO || RSG_ORBIS
			float fUIWidth = 1280.f;
			float fUIHeight = 720.f;
			#else
			float fUIWidth =  SCREEN_WIDTH;
			float fUIHeight =  SCREEN_HEIGHT;
			#endif

			newDisplayInfo.SetX((rGamerTag.m_screenPos.GetReadBuf().x)  * fUIWidth);
			newDisplayInfo.SetY((rGamerTag.m_screenPos.GetReadBuf().y) * fUIHeight);

			float scaler = (CHudTools::GetAspectRatioMultiplier() < 1.0f) ? sm_4x3Scaler : 100.0f;
			newDisplayInfo.SetScale(rGamerTag.m_scale.GetReadBuf() * scaler, rGamerTag.m_scale.GetReadBuf() * scaler);

			if (!currentDisplayInfo.GetVisible())
				newDisplayInfo.SetVisible(true);

			rGamerTag.m_asGamerTagContainerMc.SetDisplayInfo(newDisplayInfo);

			// readjust positions if required:
			if (rGamerTag.m_reinitPositions)
			{
				rGamerTag.m_reinitPositions = false;

				bool bVisibilities[MAX_MP_TAGS] = {false};
				GFxValue gfxValues[MAX_MP_TAGS]; // If bVisibilities[x] is true, then gfxValues[x] will be defined and visible.

				s32 iNumberVisibleCenter = 0;
				s32 iNumberVisibleLeft = 0;
				float fCenterWidth = 0.0f;
				float fLeftWidth = 0.0f;

				float gamerTagY = 0.0f;
				float aboveSpacer = 0.0f;

				for (s32 lookupIndex = 0; lookupIndex < MAX_MP_TAGS; ++lookupIndex)
				{
					s32 iCurrTag = sm_tagOrder[lookupIndex];
					RT_GetComponentGfxValue(&gfxValues[iCurrTag], iPlayerId, (eMP_TAG)iCurrTag, centerIcons, leftIcons, false);

					if (gfxValues[iCurrTag].IsDefined())
					{
						GFxValue::DisplayInfo dInfo;
						gfxValues[iCurrTag].GetDisplayInfo(&dInfo);

						if(iCurrTag == MP_TAG_GAMER_NAME)
						{
							gamerTagY = (float)dInfo.GetY();
						}

						if (dInfo.GetVisible())
						{
							bVisibilities[iCurrTag] = true;

							float width = sm_tagWidths[iCurrTag];
							float spacer = sm_iconSpacing;

							if(iCurrTag == MP_TAG_GAMER_NAME || iCurrTag == MP_TAG_GAMER_NAME_NEARBY)
							{
								width = rGamerTag.m_gamerTagWidth;
								spacer = sm_afterGamerNameSpacing;
								aboveSpacer = sm_aboveGamerNameSpacing;
							}
							else if (iCurrTag == MP_TAG_ARROW)
							{
								aboveSpacer = sm_aboveGamerNameSpacing;
							}

							if (m_rtTagFlags[iCurrTag] & TF_CENTER_ROW)
							{
								if(iNumberVisibleCenter)
								{
									width += spacer;
								}

								iNumberVisibleCenter++;
								fCenterWidth += width;
							}
							else if (m_rtTagFlags[iCurrTag] & TF_LEFT_ROW)
							{
								if(iNumberVisibleLeft)
								{
									width += spacer;
								}

								iNumberVisibleLeft++;
								fLeftWidth += width;
							}
						}
					}
				}

				float fStartingPosCenter = -fCenterWidth*0.5f;  // starting here will center it all
				float fStartingPosLeft = fStartingPosCenter - fLeftWidth; //We'll start on the left and move right, so the Voice chat won't shift other icons when people talk.

				for (s32 lookupIndex = 0; lookupIndex < MAX_MP_TAGS; ++lookupIndex)
				{
					s32 iCurrTag = sm_tagOrder[lookupIndex];

					if (bVisibilities[iCurrTag])
					{
						float width = sm_tagWidths[iCurrTag] + sm_iconSpacing;
						float offset = width*0.5f;

						if(iCurrTag == MP_TAG_GAMER_NAME || iCurrTag == MP_TAG_GAMER_NAME_NEARBY)
						{
							width = rGamerTag.m_gamerTagWidth + sm_afterGamerNameSpacing;
							offset = 0.0f;
						}
						else if(iCurrTag == MP_TAG_PACKAGES)
						{
							GFxValue::DisplayInfo iconDisplayInfo;
							iconDisplayInfo.SetY(gamerTagY + aboveSpacer);

							gfxValues[iCurrTag].SetDisplayInfo(iconDisplayInfo);
						}

						if(m_rtTagFlags[iCurrTag] & TF_CENTER_ROW)
						{
							GFxValue::DisplayInfo iconDisplayInfo;
							iconDisplayInfo.SetX(fStartingPosCenter + offset);

							gfxValues[iCurrTag].SetDisplayInfo(iconDisplayInfo);
							fStartingPosCenter += width;
						}
						else if(m_rtTagFlags[iCurrTag] & TF_LEFT_ROW)
						{
							GFxValue::DisplayInfo iconDisplayInfo;
							iconDisplayInfo.SetX(fStartingPosLeft + offset);

							gfxValues[iCurrTag].SetDisplayInfo(iconDisplayInfo);
							fStartingPosLeft += width;
						}
					}
				}
			}
		}

		// Update the health bar.
		GFxValue healthArmour;
		RT_GetComponentGfxValue(&healthArmour, iPlayerId, MP_TAG_HEALTH_BAR, centerIcons, leftIcons, true);
		if (healthArmour.IsDefined() && rGamerTag.m_alpha[MP_TAG_HEALTH_BAR] > 0)
		{
			uiDebugf1("Updating health bar");
			const CNewHud::sHealthInfo& rHealthInfo = rGamerTag.m_healthInfo.GetReadBuf();
			bool bHealthChanged = false;
			bool bArmourChanged = false;
			bool bEnduranceChanged = false;
			int iHealthPercentageValue = rHealthInfo.m_iHealthPercentageValue;
			int iEndurancePercentageValue = rHealthInfo.m_iEndurancePercentageValue;
			int iArmorPercentageValue = rHealthInfo.m_iArmourPercentageValue;

			{				
#if __BANK
				if (!bDebugUsePointHealth)
				{
					if (iDebugHealthPercent != -1)
					{
						iHealthPercentageValue = iDebugHealthPercent;
					}
				}
#endif
				if (iHealthPercentageValue != rGamerTag.m_previousHealthInfo.m_iHealthPercentageValue ||
					rHealthInfo.m_iHealthPercentageCapacity != rGamerTag.m_previousHealthInfo.m_iHealthPercentageCapacity)
				{
					RT_UpdateHealthArmourHelper(healthArmour, iHealthPercentageValue, rHealthInfo.m_iHealthPercentageCapacity, HEALTH_ARMOUR_NAME, "healthBar");
					RT_UpdateHealthArmourHelper(healthArmour, iHealthPercentageValue, rHealthInfo.m_iHealthPercentageCapacity, HEALTH_NO_ARMOUR_NAME, "healthBar");					
#if ARCHIVED_SUMMER_CONTENT_ENABLED
					RT_UpdateHealthArmourHelper(healthArmour, iHealthPercentageValue, rHealthInfo.m_iHealthPercentageCapacity, HEALTH_CNC_NO_ARMOUR_NAME, "healthBar");
					RT_UpdateHealthArmourHelper(healthArmour, iHealthPercentageValue, rHealthInfo.m_iHealthPercentageCapacity, HEALTH_CNC_NO_ARMOUR_NO_ENDURANCE_NAME, "healthBar");
#endif						

					rGamerTag.m_previousHealthInfo.m_iHealthPercentageValue = iHealthPercentageValue;
					rGamerTag.m_previousHealthInfo.m_iHealthPercentageCapacity = rHealthInfo.m_iHealthPercentageCapacity;
					bHealthChanged = true;
				}
			}

			{			
				if (iEndurancePercentageValue != rGamerTag.m_previousHealthInfo.m_iEndurancePercentageValue ||
					rHealthInfo.m_iEndurancePercentageCapacity != rGamerTag.m_previousHealthInfo.m_iEndurancePercentageCapacity)
				{
#if ARCHIVED_SUMMER_CONTENT_ENABLED
					RT_UpdateHealthArmourHelper(healthArmour, iEndurancePercentageValue, rHealthInfo.m_iEndurancePercentageCapacity, HEALTH_CNC_NO_ARMOUR_NAME, "EnduranceBar");
#endif					
					rGamerTag.m_previousHealthInfo.m_iEndurancePercentageValue = iEndurancePercentageValue;
					rGamerTag.m_previousHealthInfo.m_iEndurancePercentageCapacity = rHealthInfo.m_iEndurancePercentageCapacity;
					bEnduranceChanged = true;
				}
			}

			{			
#if __BANK
				if (!bDebugUsePointHealth)
				{
					if (iDebugArmorPercent != -1)
					{
						iArmorPercentageValue = iDebugArmorPercent;
					}
				}
#endif
				if (iArmorPercentageValue != rGamerTag.m_previousHealthInfo.m_iArmourPercentageValue ||
					rHealthInfo.m_iArmourPercentageCapacity != rGamerTag.m_previousHealthInfo.m_iArmourPercentageCapacity)
				{
					RT_UpdateHealthArmourHelper(healthArmour, iArmorPercentageValue, rHealthInfo.m_iArmourPercentageCapacity, HEALTH_ARMOUR_NAME, "armourBar");

					rGamerTag.m_previousHealthInfo.m_iArmourPercentageValue = iArmorPercentageValue;
					rGamerTag.m_previousHealthInfo.m_iArmourPercentageCapacity = rHealthInfo.m_iArmourPercentageCapacity;
					bArmourChanged = true;
				}

				if ((bHealthChanged || (bEnduranceChanged && iEndurancePercentageValue >= 0) || rGamerTag.m_previousHealthInfo.m_iTeam != rHealthInfo.m_iTeam) && (CNewHud::GetDisplayMode() == CNewHud::DM_ARCADE_CNC ))
				{					
					RT_UpdateHealthArmourVisibilityHelper(healthArmour, HEALTH_ARMOUR_NAME, false);
					RT_UpdateHealthArmourVisibilityHelper(healthArmour, HEALTH_NO_ARMOUR_NAME, false);						
#if ARCHIVED_SUMMER_CONTENT_ENABLED
					RT_UpdateHealthArmourVisibilityHelper(healthArmour, HEALTH_CNC_NO_ARMOUR_NAME, rHealthInfo.m_iTeam == (int)eArcadeTeam::AT_CNC_CROOK);
					RT_UpdateHealthArmourVisibilityHelper(healthArmour, HEALTH_CNC_NO_ARMOUR_NO_ENDURANCE_NAME, rHealthInfo.m_iTeam == (int)eArcadeTeam::AT_CNC_COP);
#endif							
				}
				else if ((bHealthChanged || bArmourChanged) && (iArmorPercentageValue > 0))
				{
					RT_UpdateHealthArmourVisibilityHelper(healthArmour, HEALTH_ARMOUR_NAME, true);
					RT_UpdateHealthArmourVisibilityHelper(healthArmour, HEALTH_NO_ARMOUR_NAME, false);					
#if ARCHIVED_SUMMER_CONTENT_ENABLED
					RT_UpdateHealthArmourVisibilityHelper(healthArmour, HEALTH_CNC_NO_ARMOUR_NAME, false);
					RT_UpdateHealthArmourVisibilityHelper(healthArmour, HEALTH_CNC_NO_ARMOUR_NO_ENDURANCE_NAME, false);
#endif						
				}
				else if (bHealthChanged)
				{
					RT_UpdateHealthArmourVisibilityHelper(healthArmour, HEALTH_ARMOUR_NAME, false);
					RT_UpdateHealthArmourVisibilityHelper(healthArmour, HEALTH_NO_ARMOUR_NAME, true);
#if ARCHIVED_SUMMER_CONTENT_ENABLED
					RT_UpdateHealthArmourVisibilityHelper(healthArmour, HEALTH_CNC_NO_ARMOUR_NAME, false);
					RT_UpdateHealthArmourVisibilityHelper(healthArmour, HEALTH_CNC_NO_ARMOUR_NO_ENDURANCE_NAME, false);
#endif						
				}

			}
		}
		else if(healthArmour.IsDefined() && rGamerTag.m_alpha[MP_TAG_HEALTH_BAR] <= 0)
		{
			RT_UpdateHealthArmourVisibilityHelper(healthArmour, HEALTH_ARMOUR_NAME, false);
			RT_UpdateHealthArmourVisibilityHelper(healthArmour, HEALTH_NO_ARMOUR_NAME, false);
#if ARCHIVED_SUMMER_CONTENT_ENABLED
			RT_UpdateHealthArmourVisibilityHelper(healthArmour, HEALTH_CNC_NO_ARMOUR_NAME, false);
#endif				
		}
	}
	else
	{
		newDisplayInfo.SetVisible(false);
		rGamerTag.m_asGamerTagContainerMc.SetDisplayInfo(newDisplayInfo);
	}
}

void CMultiplayerGamerTagHud::RT_UpdateHealthArmourHelper(GFxValue& parent, int percentageValue, int capacity, const char* pBarContainerName, const char* pBarName)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on UT
	{
		uiAssertf(0, "CMultiplayerGamerTagHud::RT_UpdateHealthArmourHelper can only be called on the RenderThread!");
		return;
	}

	if(!uiVerify(parent.IsDefined()) || !uiVerify(pBarName) || !uiVerify(pBarContainerName))
	{
		return;
	}

	const double barScale = 100.0;

	double val = ((double)percentageValue) * (barScale/((double)capacity));

	GFxValue barContainer;
	GFxValue::DisplayInfo healthBarLeftInfo;
	parent.GetMember(pBarContainerName, &barContainer);

	if(uiVerify(barContainer.IsDefined()))
	{
		GFxValue healthBar;
		GFxValue::DisplayInfo healthBarLeftInfo;
		barContainer.GetMember(pBarName, &healthBar);
		healthBar.GetDisplayInfo(&healthBarLeftInfo);
		if ((val >= 0))
		{
			healthBarLeftInfo.SetXScale(MIN(val, 100));
		}
		else
		{
			healthBarLeftInfo.SetXScale(0);
		}
		healthBar.SetDisplayInfo(healthBarLeftInfo);
	}
}

void CMultiplayerGamerTagHud::RT_UpdateHealthArmourVisibilityHelper(GFxValue& parent, const char* pBar, bool isVisible)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on UT
	{
		uiAssertf(0, "CMultiplayerGamerTagHud::RT_UpdateHealthArmourVisibilityHelper can only be called on the RenderThread!");
		return;
	}

	if(!uiVerify(parent.IsDefined()) || !uiVerify(pBar))
	{
		return;
	}

	GFxValue healthBar;
	GFxValue::DisplayInfo healthBarLeftInfo;
	parent.GetMember(pBar, &healthBar);
	healthBar.GetDisplayInfo(&healthBarLeftInfo);
	healthBarLeftInfo.SetVisible(isVisible);
	healthBar.SetDisplayInfo(healthBarLeftInfo);
}

void CMultiplayerGamerTagHud::RT_UpdateHealthColorHelper(GFxValue& parent, const char* pBar, eHUD_COLOURS healthBarColour)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on UT
	{
		uiAssertf(0, "CMultiplayerGamerTagHud::RT_UpdateHealthColorHelper can only be called on the RenderThread!");
		return;
	}

	if(!uiVerify(parent.IsDefined()) || !uiVerify(pBar))
	{
		return;
	}


	GFxValue healthBarContainer;
	parent.GetMember(pBar, &healthBarContainer);

	if (uiVerify(healthBarContainer.IsDefined()))
	{
		CRGBA hudColour = CHudColour::GetRGB(healthBarColour, 100);
		Color32 iColour(hudColour.GetColor());

		GFxValue healthBar;
		healthBarContainer.GetMember("healthBar", &healthBar);
		if(uiVerify(healthBar.IsDefined()))
		{
			RT_SetColourToGfxvalue(&healthBar, iColour);
		}

		GFxValue healthBarTrough;
		healthBarContainer.GetMember("healthBarTrough", &healthBarTrough);
		if(uiVerify(healthBarTrough.IsDefined()))
		{
			Color32 iColourTrough(iColour.GetRedf()*0.6f, iColour.GetGreenf()*0.6f, iColour.GetBluef()*0.6f, iColour.GetAlphaf());
			RT_SetColourToGfxvalue(&healthBarTrough, iColourTrough);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMultiplayerGamerTagHud::CreateRoot()
// PURPOSE: sets up the main root layer
/////////////////////////////////////////////////////////////////////////////////////
void CMultiplayerGamerTagHud::UT_CreateRoot()
{
	if (uiVerify(CSystem::IsThisThreadId(SYS_THREAD_UPDATE)))
	{
		m_renderingSection.Lock();	
		{ // scope for AutoLock
			CScaleformMgr::AutoLock lock(m_movie.GetMovieID());

			GFxValue asMovieObject = CScaleformMgr::GetActionScriptObjectFromRoot(m_movie.GetMovieID());
			if(uiVerify(asMovieObject.IsDefined()) && !m_asRootContainer.IsDefined())
			{
				asMovieObject.CreateEmptyMovieClip(&m_asRootContainer, "asRootContainer", MOVIE_DEPTH_ROOT);
			}

			if(uiVerify(m_asRootContainer.IsDefined()) && !m_asGamerTagLayerContainer.IsDefined())
			{
				if(uiVerify(m_asRootContainer.CreateEmptyMovieClip(&m_asGamerTagLayerContainer, "asGamerTagLayerContainer", 100)))
				{
					m_rootCreated = true;
				}
			}
		}
		m_renderingSection.Unlock();
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMultiplayerGamerTagHud::RemoveRoot()
// PURPOSE: removes the main root layer
/////////////////////////////////////////////////////////////////////////////////////
void CMultiplayerGamerTagHud::UT_RemoveRoot()
{
	if (uiVerify(CSystem::IsThisThreadId(SYS_THREAD_UPDATE)))
	{
		m_renderingSection.Lock();   
		{ // scope for AutoLock
		CScaleformMgr::AutoLock lock(m_movie.GetMovieID());
		if(m_rootCreated)
		{
			m_rootCreated = false;

			if(m_asGamerTagLayerContainer.IsDefined())
			{
				m_asGamerTagLayerContainer.Invoke("removeMovieClip");  // remove the mask movie
				m_asGamerTagLayerContainer.SetUndefined();
			}

			if(m_asRootContainer.IsDefined())
			{
				m_asRootContainer.Invoke("removeMovieClip");  // remove the mask movie
				m_asRootContainer.SetUndefined();
			}
		}
		}
		m_renderingSection.Unlock();
	}
}

// eof
