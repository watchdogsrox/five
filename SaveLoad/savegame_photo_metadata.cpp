
#include "SaveLoad/savegame_photo_metadata.h"


// Rage headers
#include "rline/rl.h"
#include "vector/vector3.h"

// Framework headers
#include "fwsys/gameskeleton.h"

// Game headers
#include "ai/EntityScanner.h"
#include "audio/radioaudioentity.h"
#include "frontend/MobilePhone.h"
#include "game/user.h"
#include "Network/NetworkInterface.h"
#include "Network/Live/NetworkClan.h"
#include "Network/Sessions/NetworkGameConfig.h"
#include "Network/Sessions/NetworkSession.h"
#include "Peds/PedIntelligence.h"
#include "renderer/occlusion.h"
#include "SaveLoad/savegame_channel.h"
#include "scene/world/GameWorld.h"
#include "script/script.h"
#include "text/messages.h"
#include "text/TextConversion.h"


// Constants
static const s32 MAX_NUM_VISIBLE_VEHICLES = 16;
static const s32 MAX_NUM_VISIBLE_WEAPONS = 16;
static const s32 MAX_NUM_VISIBLE_PLAYERS = 16;
static const s32 MAX_NUM_VISIBLE_SINGLE_PLAYER_CHARACTERS = 16;

PARAM(photoUseLimeLight, "[savegame_photo] Use limelight instead of akamai");


const char *CSavegamePhotoMetadata::GetSongTitle() const
{
	if (m_SongId != InvalidSongId)
	{
		char songTitleTextKey[16];

		formatf(songTitleTextKey, "%uS", m_SongId);
		if (TheText.DoesTextLabelExist(songTitleTextKey))
		{
			return TheText.Get(songTitleTextKey);
		}
	}

	return "";
}


const char *CSavegamePhotoMetadata::GetSongArtist() const
{
	if (m_SongId != InvalidSongId)
	{
		char artistTextKey[16];

		formatf(artistTextKey, "%uA", m_SongId);
		if (TheText.DoesTextLabelExist(artistTextKey))
		{
			return TheText.Get(artistTextKey);
		}
	}

	return "";
}


void CSavegamePhotoMetadata::FillContents()
{
	Vector3	vPlayerCoords = CGameWorld::FindLocalPlayerCoors();
	m_fPlayerX = vPlayerCoords.x;
	m_fPlayerY = vPlayerCoords.y;
	m_fPlayerZ = vPlayerCoords.z;

//	CTextConversion::charToAscii(CUserDisplay::AreaName.GetName(), m_AreaName, NELEM(m_AreaName));
//	CTextConversion::charToAscii(CUserDisplay::StreetName.GetName(), m_StreetName, NELEM(m_StreetName));

	m_AreaNameTextKey = CUserDisplay::AreaName.GetNameTextKey();
	m_AreaNameTextKey.Uppercase();

	m_HashOfStreetNameTextKey = CUserDisplay::StreetName.GetHashOfNameTextKey();

//	if (CMessages::IsMissionTitleActive() && CLoadingText::IsActive())
	if(CPauseMenu::GetCurrenMissionActive())
	{
//		safecpy(m_SinglePlayerMissionName, CLoadingText::GetText(), NELEM(m_SinglePlayerMissionName));
		if (!CPauseMenu::GetCurrentMissionLabelIsALiteralString())
		{
			m_SinglePlayerMissionName = CPauseMenu::GetCurrentMissionLabel();	//	This is the text key (not the translated text itself)
		}
		else
		{	//	The mission labels of Multiplayer UGC missions will be literal strings
			m_SinglePlayerMissionName.Clear();
		}
	}
	else
	{
//		safecpy(m_SinglePlayerMissionName, "", NELEM(m_SinglePlayerMissionName));
		m_SinglePlayerMissionName.Clear();
	}

	m_RadioStationName = "";
	if (g_RadioAudioEntity.IsPlayerRadioActive() && g_RadioAudioEntity.GetPlayerRadioStation())
	{
		m_RadioStationName = g_RadioAudioEntity.GetPlayerRadioStation()->GetName();
	}

	m_SongId = g_RadioAudioEntity.GetAudibleTrackTextId();


	m_PrimaryClanIdOfPlayer = 0;

	if (CNetwork::IsGameInProgress())
	{
		m_MpSessionID = CNetwork::GetNetworkSession().GetSnSession().GetSessionId();

		NetworkClan& tClan = CLiveManager::GetNetworkClan();
		if(tClan.HasPrimaryClan())
		{
			m_PrimaryClanIdOfPlayer = tClan.GetPrimaryClan()->m_Id;
		}
	}
	else
	{
		m_MpSessionID = 0;	//	Can I use 0 for an invalid session ID?
	}

	m_UgcID = CTheScripts::GetContentIdOfCurrentUGCMission();


	if (CNetwork::IsGameInProgress())
	{
		m_GameMode = 1;
	}
	else
	{
		m_GameMode = 0;
	}

	//	Game Time
	m_GameTimeHour = CClock::GetHour();
	m_GameTimeMinute = CClock::GetMinute();
	m_GameTimeSecond = CClock::GetSecond();

	m_GameTimeDayOfMonth = CClock::GetDay();
	m_GameTimeMonth = CClock::GetMonth() + 1;	//	Add 1 so that our range is [1,12]. Requested by Rob Trickey by email on 01/05/2013
	m_GameTimeYear = CClock::GetYear();

	// Meme content is flagged later, rather than when we fill content.
	m_containsMemeText = false;

	m_bMugshot = false;
	m_Akamai = CSavegamePhotoMetadata::IsAkamaiEnabled();

	m_UniqueId.Reset();	//	I'll call GenerateNewUniqueId() later once I'm sure that the player is signed in

	m_ImageSignature = 0;	//	This will be set by a later call to SetImageSignature()

	u64 posixTime = rlGetPosixTime();
	photoAssertf( (posixTime >> 32) == 0, "CSavegamePhotoMetadata::FillContents - expected the top 32 bits of the current POSIX time to be 0");
	m_PosixCreationTime = (u32) (posixTime & 0xffffffff);


#if PHOTO_METADATA_STORES_WIDESCREEN_FLAG
	if (CHudTools::GetWideScreen())
	{
		m_bIsWidescreen = true;
	}
	else
	{
		m_bIsWidescreen = false;
	}
#endif	//	PHOTO_METADATA_STORES_WIDESCREEN_FLAG

#if PHOTO_METADATA_STORES_SELFIE_FLAG
	if (CPhoneMgr::CamGetSelfieModeState())
	{
		m_bIsSelfie = true;
	}
	else
	{
		m_bIsSelfie = false;
	}
#endif	//	PHOTO_METADATA_STORES_SELFIE_FLAG


	if (CTheScripts::GetIsInDirectorMode())
	{
		m_bIsInDirectorMode = true;
	}
	else
	{
		m_bIsInDirectorMode = false;
	}

	m_bTakenInRockstarEditor = false;

	m_ArenaTheme = 0;			//	These will be set later by the camera script 
	m_ArenaVariation = 0;		//	if the player is inside an arena in Multiplayer

	m_bOnIslandX = false;		//	This flag will be set later by the camera script
								//	if the player is on the Island that has been added for the Island Heist DLC

#if PHOTO_METADATA_STORES_NEARBY_VEHICLES
	CompileTimeAssert(CVehicleScanner::MAX_NUM_ENTITIES == MAX_NUM_VISIBLE_VEHICLES);
	m_AtArrayOfVisibleVehicles.Reset();
#endif	//	PHOTO_METADATA_STORES_NEARBY_VEHICLES

#if PHOTO_METADATA_STORES_NEARBY_WEAPONS
	CompileTimeAssert(CPedScanner::MAX_NUM_ENTITIES == MAX_NUM_VISIBLE_WEAPONS);
	m_AtArrayOfVisibleWeapons.Reset();
	s32 number_of_visible_weapons = 0;
#endif	//	PHOTO_METADATA_STORES_NEARBY_WEAPONS

#if PHOTO_METADATA_STORES_NEARBY_MP_GAMERTAGS
	CompileTimeAssert(CPedScanner::MAX_NUM_ENTITIES == MAX_NUM_VISIBLE_PLAYERS);
	m_AtArrayOfGamerTags.Reset();
	s32 number_of_visible_players = 0;
#endif	//	PHOTO_METADATA_STORES_NEARBY_MP_GAMERTAGS

#if PHOTO_METADATA_STORES_NEARBY_SP_CHARS
	CompileTimeAssert(CPedScanner::MAX_NUM_ENTITIES == MAX_NUM_VISIBLE_SINGLE_PLAYER_CHARACTERS);
	m_AtArrayOfSinglePlayerCharacters.Reset();
	s32 number_of_single_player_characters = 0;
#endif	//	PHOTO_METADATA_STORES_NEARBY_SP_CHARS


	CPlayerInfo* pPlayerInfo = CGameWorld::GetMainPlayerInfo();
	CPed *pPlayerPed = pPlayerInfo ? pPlayerInfo->GetPlayerPed() : NULL;

	if (pPlayerPed)
	{
#if PHOTO_METADATA_STORES_NEARBY_VEHICLES
		AddNearbyVehiclesToArrayOfVisibleVehicles(pPlayerPed);
#endif	//	PHOTO_METADATA_STORES_NEARBY_VEHICLES

		if (CPhoneMgr::CamGetSelfieModeState())
		{
			photoAssertf(IsEntityVisible(pPlayerPed, false), "CSavegamePhotoMetadata::FillContents - Phone camera is in selfie mode, but player isn't visible");

#if PHOTO_METADATA_STORES_NEARBY_MP_GAMERTAGS
			AddPedToGamerTagArray(pPlayerPed, number_of_visible_players);
#endif	//	PHOTO_METADATA_STORES_NEARBY_MP_GAMERTAGS

#if PHOTO_METADATA_STORES_NEARBY_SP_CHARS
			AddPedToSinglePlayerCharacterArray(pPlayerPed, number_of_single_player_characters);
#endif	//	PHOTO_METADATA_STORES_NEARBY_SP_CHARS
		}

		CEntityScannerIterator pedIterator = pPlayerPed->GetPedIntelligence()->GetNearbyPeds();
		CEntity* pEntity = pedIterator.GetFirst();
		while(pEntity)
		{
			if (IsEntityVisible(pEntity, false))
			{
				if (pEntity->GetIsTypePed())
				{
					CPed *pPed = (CPed*) pEntity;

#if PHOTO_METADATA_STORES_NEARBY_WEAPONS
					AddPedWeaponToArray(pPed, number_of_visible_weapons);
#endif	//	PHOTO_METADATA_STORES_NEARBY_WEAPONS

#if PHOTO_METADATA_STORES_NEARBY_MP_GAMERTAGS
					AddPedToGamerTagArray(pPed, number_of_visible_players);
#endif	//	PHOTO_METADATA_STORES_NEARBY_MP_GAMERTAGS

#if PHOTO_METADATA_STORES_NEARBY_SP_CHARS
					AddPedToSinglePlayerCharacterArray(pPed, number_of_single_player_characters);
#endif	//	PHOTO_METADATA_STORES_NEARBY_SP_CHARS
				}
			}

			pEntity = pedIterator.GetNext();
		}
	}	//	if (pPlayerPed)
}


void CSavegamePhotoMetadata::SetArenaThemeAndVariation(u32 arenaTheme, u32 arenaVariation)
{
	m_ArenaTheme = arenaTheme;
	m_ArenaVariation = arenaVariation;
}

void CSavegamePhotoMetadata::SetOnIslandX(bool bOnIslandX)
{
	m_bOnIslandX = bOnIslandX;
}

void CSavegamePhotoMetadata::GenerateNewUniqueId()
{
	m_UniqueId.GenerateNewUniqueId();
}

void CSavegamePhotoMetadata::SetImageSignature(u64 signature)
{
	m_ImageSignature = signature;
}

bool CSavegamePhotoMetadata::GetAkamai() const
{
	return m_Akamai;
}

void CSavegamePhotoMetadata::SetAkamai(const bool akamai)
{
	m_Akamai = akamai;
}

bool CSavegamePhotoMetadata::IsAkamaiEnabled()
{
	return NOTFINAL_ONLY(!PARAM_photoUseLimeLight.Get() &&)
		Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("PHOTO_AKAMAI", 0x1973A89E), true);
}

bool CSavegamePhotoMetadata::ShouldCorrectAkamaiOnLocalLoad()
{
	return Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("PHOTO_AKAMAI_FIX_META_ON_LOAD", 0x850243CE), true);
}

bool CSavegamePhotoMetadata::ShouldCorrectAkamaiOnUpload()
{
	return Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("PHOTO_AKAMAI_FIX_META_ON_UPLOAD", 0x7EFC139A), true);
}

#define LOCATION_KEY						"loc"
#define AREA_KEY							"area"
#define STREET_KEY							"street"
#define SINGLE_PLAYER_MISSION_NAME_KEY		"nm"

#define RADIO_STATION_KEY					"rds"
#define SONG_KEY							"scr"



/*
	if (bSuccess)
	{
		if (CNetwork::IsGameInProgress())
		{
			switch (CNetwork::GetNetworkSession().GetMatchConfig().GetGameMode())
		}
		else
		{
			bSuccess = rsonWriter.WriteString("mode", "SP");
		}
	}
*/
//	"mode" - string indicating the game mode, e.g. (SP, FREEMODE, DEATHMATCH, C&C etc)
//	In code, we only know about the containing mode at the moment (freemode). 
//	I’m working on transition sessions with Bobby at the moment, from that you can grab an activity type which will let you know the sub-mode (DM, race, base-jump, etc)
#define GAME_MODE_KEY						"mode"


//	"mid": "unique mission id e.g. 501276516e8d7d22fd67c32d" Mission UGC ID (free missions only)
//	Script would need to give us this. They’ll know – just give them a script command to fill this in.
#define UGC_MISSION_UNIQUE_ID_KEY			"mid"

//	"sid" - string containing session id (MP only)
//	CNetwork::GetNetworkSession().GetSnSession().GetSessionId()
#define MP_SESSION_ID_KEY					"sid"

#define MP_CLAN_ID_KEY						"crewid"

#define GAME_TIME_KEY						"time"
#define GAME_TIME_HOUR						"hour"
#define GAME_TIME_MINUTE					"minute"
#define GAME_TIME_SECOND					"second"
#define GAME_TIME_DAY						"day"
#define GAME_TIME_MONTH						"month"
#define GAME_TIME_YEAR						"year"

#define VEHICLE_MODELS_KEY					"veh"		//	Array of hashes of model names of visible vehicles
#define WEAPON_HASH_ARRAY_KEY				"weap"		//	Array of hashes of weapons equipped by visible peds
#define GAMERTAG_ARRAY_KEY					"plyrs"		//	Array of strings containing the User IDs of visible players - Bug 1927221
#define SINGLE_PLAYER_CHARACTERS_KEY		"char"		//	Array of strings containing text labels of single player character names

#define WIDESCREEN_KEY						"ws"		//	A flag to say whether the photo was taken on a console with the display set to widescreen
#define SELFIE_KEY							"slf"		//	A flag to say whether the photo is a selfie

#define DIRECTOR_KEY						"drctr"		//	A flag to say whether the photo was taken during director mode

#define ROCKSTAR_EDITOR_KEY					"rsedtr"	//	A flag to say whether the photo was taken in the Rockstar Editor
#define AKAMAI_KEY							"cv"

//	game time
//	real world time

//	"meme" - Flag indicating if this was generated via the meme editor
#define MEME_TEXT_KEY						"meme"

//	"mug" - Flag to say whether the photo is a mugshot taken by James Adwick's MP character creation script
#define MUGSHOT_KEY							"mug"

//	"uid" - a unique Id which we use to check if a local photo also exists on the cloud
#define UNIQUE_PHOTO_ID_KEY					"uid"

// "sign" - a hash value calculated from the image data. When loading, this can be used to verify that the photo is not a dodgy image that was created outside the game.
#define IMAGE_SIGNATURE_KEY					"sign"

//	"creat" - POSIX creation time
#define POSIX_CREATION_TIME_KEY					"creat"

// Store the arena theme and variation if the player took the photo while inside an arena in Multiplayer
//	A theme index of 0 means that the player was not in an arena
#define ARENA_THEME_INDEX_KEY				"arena_t"
#define ARENA_VARIATION_INDEX_KEY			"arena_v"

#define ON_ISLAND_X_KEY						"onislandx"

#if __BANK
bool CSavegamePhotoMetadata::sm_bDisplayBoundingRectanglesForPeds = false;

bool CSavegamePhotoMetadata::sm_bSaveLocation = true;
bool CSavegamePhotoMetadata::sm_bSaveArea = true;
bool CSavegamePhotoMetadata::sm_bSaveStreet = true;
bool CSavegamePhotoMetadata::sm_bSaveMissionName = true;
bool CSavegamePhotoMetadata::sm_bSaveSong = true;
bool CSavegamePhotoMetadata::sm_bSaveRadioStation = true;
bool CSavegamePhotoMetadata::sm_bSaveMpSessionID = true;
bool CSavegamePhotoMetadata::sm_bSaveClanID = true;
bool CSavegamePhotoMetadata::sm_bSaveUgcID = true;
bool CSavegamePhotoMetadata::sm_bSaveTheGameMode = true;
bool CSavegamePhotoMetadata::sm_bSaveTheGameTime = true;
bool CSavegamePhotoMetadata::sm_bSaveTheMemeFlag = true;
bool CSavegamePhotoMetadata::sm_bSaveTheUniqueId = true;
bool CSavegamePhotoMetadata::sm_bSaveThePosixCreationTime = true;
bool CSavegamePhotoMetadata::sm_bSaveArenaThemeAndVariation = true;
bool CSavegamePhotoMetadata::sm_bSaveOnIslandX = true;
#endif	//	__BANK

bool CSavegamePhotoMetadata::WriteMetadataToString(char *pStringToFill, u32 maxLengthOfString, bool BANK_ONLY(bDebugLessData)) const
{
	memset(pStringToFill, 0, maxLengthOfString);
	RsonWriter rsonWriter(pStringToFill, maxLengthOfString, RSON_FORMAT_JSON);
	rsonWriter.Reset();

	bool bSuccess = rsonWriter.Begin(NULL, NULL);	//	Top-level structure to contain all the rest of the data

#if __BANK
	if ( (bDebugLessData == false) || (sm_bSaveLocation) )
#endif	//	__BANK
	{
		if (bSuccess)
		{
			bSuccess = rsonWriter.Begin(LOCATION_KEY, NULL);
		}

		if (bSuccess)
		{
			bSuccess = rsonWriter.WriteFloat("x", m_fPlayerX);

			if (bSuccess)
			{
				bSuccess = rsonWriter.WriteFloat("y", m_fPlayerY);
			}

			if (bSuccess)
			{
				bSuccess = rsonWriter.WriteFloat("z", m_fPlayerZ);
			}
		}

		if (bSuccess)
		{
			bSuccess = rsonWriter.End();
		}
	}

//	"area" - string with the area name of the image location
//	Change the following three to text labels instead of using the translated text
//	CPopZone::m_associatedTextId is stored as a u64. Other code is casting to a char* though so hopefully I can treat it as a string
//	It looks like CPathNode::m_streetNameHash is stored as a hash though so the json file will either need to include this u32 or the translated name

#if __BANK
	if ( (bDebugLessData == false) || (sm_bSaveArea) )
#endif	//	__BANK
	{
		if (bSuccess)
		{
			bSuccess = rsonWriter.WriteString(AREA_KEY, m_AreaNameTextKey);
		}
	}

#if __BANK
	if ( (bDebugLessData == false) || (sm_bSaveStreet) )
#endif	//	__BANK
	{
		if (bSuccess)
		{
			bSuccess = rsonWriter.WriteUns(STREET_KEY, m_HashOfStreetNameTextKey);
		}
	}

#if __BANK
	if ( (bDebugLessData == false) || (sm_bSaveMissionName) )
#endif	//	__BANK
	{
		//	"nm" - string containing the name of the SP mission (SP missions only)
		if (bSuccess)
		{
			bSuccess = rsonWriter.WriteString(SINGLE_PLAYER_MISSION_NAME_KEY, m_SinglePlayerMissionName);
		}
	}


#if __BANK
	if ( (bDebugLessData == false) || (sm_bSaveRadioStation) )
#endif	//	__BANK
	{
		if (bSuccess)
		{
			bSuccess = rsonWriter.WriteString(RADIO_STATION_KEY, m_RadioStationName);
		}
	}

#if __BANK
	if ( (bDebugLessData == false) || (sm_bSaveSong) )
#endif	//	__BANK
	{
		if (bSuccess)
		{
			bSuccess = rsonWriter.WriteUns(SONG_KEY, m_SongId);
		}
	}

#if __BANK
	if ( (bDebugLessData == false) || (sm_bSaveMpSessionID) )
#endif	//	__BANK
	{
		if (bSuccess)
		{
			bSuccess = rsonWriter.WriteHex64(MP_SESSION_ID_KEY, m_MpSessionID);	//	Or should this be WriteUns64?
		}
	}

#if __BANK
	if ( (bDebugLessData == false) || (sm_bSaveClanID) )
#endif	//	__BANK
	{
		if (bSuccess)
		{
			bSuccess = rsonWriter.WriteInt64(MP_CLAN_ID_KEY, m_PrimaryClanIdOfPlayer);
		}
	}
	
#if __BANK
	if ( (bDebugLessData == false) || (sm_bSaveUgcID) )
#endif	//	__BANK
	{
		if (bSuccess)
		{
			bSuccess = rsonWriter.WriteString(UGC_MISSION_UNIQUE_ID_KEY, m_UgcID);
		}
	}


#if __BANK
	if ( (bDebugLessData == false) || (sm_bSaveTheGameMode) )
#endif	//	__BANK
	{
		if (bSuccess)
		{
			bSuccess = rsonWriter.WriteString(GAME_MODE_KEY, GetStringFromGameMode(m_GameMode));
		}
	}

#if __BANK
	if ( (bDebugLessData == false) || (sm_bSaveTheMemeFlag) )
#endif	//	__BANK
	{
		if (bSuccess)
		{
			bSuccess = rsonWriter.WriteBool( MEME_TEXT_KEY, m_containsMemeText );
		}
	}

	if (bSuccess)
	{
		bSuccess = rsonWriter.WriteBool( MUGSHOT_KEY, m_bMugshot );
	}

#if __BANK
	if ( (bDebugLessData == false) || (sm_bSaveTheUniqueId) )
#endif	//	__BANK
	{
		if (bSuccess)
		{
			bSuccess = rsonWriter.WriteInt( UNIQUE_PHOTO_ID_KEY, m_UniqueId.GetValue() );
		}
	}

//	Game Time
#if __BANK
	if ( (bDebugLessData == false) || (sm_bSaveTheGameTime) )
#endif	//	__BANK
	{
		if (bSuccess)
		{
			bSuccess = rsonWriter.Begin(GAME_TIME_KEY, NULL);
		}

		if (bSuccess)
		{
			bSuccess = rsonWriter.WriteInt(GAME_TIME_HOUR, m_GameTimeHour);

			if (bSuccess)
			{
				bSuccess = rsonWriter.WriteInt(GAME_TIME_MINUTE, m_GameTimeMinute);
			}

			if (bSuccess)
			{
				bSuccess = rsonWriter.WriteInt(GAME_TIME_SECOND, m_GameTimeSecond);
			}

			if (bSuccess)
			{
				bSuccess = rsonWriter.WriteInt(GAME_TIME_DAY, m_GameTimeDayOfMonth);
			}

			if (bSuccess)
			{
				bSuccess = rsonWriter.WriteInt(GAME_TIME_MONTH, m_GameTimeMonth);
			}

			if (bSuccess)
			{
				bSuccess = rsonWriter.WriteInt(GAME_TIME_YEAR, m_GameTimeYear);
			}
		}

		if (bSuccess)
		{
			bSuccess = rsonWriter.End();
		}
	}


#if __BANK
	if ( (bDebugLessData == false) || (sm_bSaveThePosixCreationTime) )
#endif	//	__BANK
	{
		if (bSuccess)
		{
			bSuccess = rsonWriter.WriteInt(POSIX_CREATION_TIME_KEY, (s32) m_PosixCreationTime);
		}
	}

#if __BANK
	if ( (bDebugLessData == false) || (sm_bSaveArenaThemeAndVariation) )
#endif	//	__BANK
	{
		if (m_ArenaTheme != 0)	//	Only save the arena theme and variation if the theme is non-zero (i.e. the photo was taken in an arena)
		{
			if (bSuccess)
			{
				bSuccess = rsonWriter.WriteUns(ARENA_THEME_INDEX_KEY, m_ArenaTheme);
			}

			if (bSuccess)
			{
				bSuccess = rsonWriter.WriteUns(ARENA_VARIATION_INDEX_KEY, m_ArenaVariation);
			}
		}
	}


#if PHOTO_METADATA_STORES_WIDESCREEN_FLAG
	if (bSuccess)
	{
		bSuccess = rsonWriter.WriteBool(WIDESCREEN_KEY, m_bIsWidescreen);
	}
#endif	//	PHOTO_METADATA_STORES_WIDESCREEN_FLAG

#if PHOTO_METADATA_STORES_SELFIE_FLAG
	if (bSuccess)
	{
		bSuccess = rsonWriter.WriteBool(SELFIE_KEY, m_bIsSelfie);
	}
#endif	//	PHOTO_METADATA_STORES_SELFIE_FLAG

	if (bSuccess)
	{
		bSuccess = rsonWriter.WriteBool(DIRECTOR_KEY, m_bIsInDirectorMode);
	}

	if (bSuccess)
	{
		bSuccess = rsonWriter.WriteBool(ROCKSTAR_EDITOR_KEY, m_bTakenInRockstarEditor);
	}

	if (bSuccess)
	{
		bSuccess = rsonWriter.WriteBool(AKAMAI_KEY, m_Akamai);
	}

#if __BANK
	if ( (bDebugLessData == false) || (sm_bSaveOnIslandX) )
#endif	//	__BANK
	{
		if (m_bOnIslandX)	//	Only save the flag if it's true (i.e. the photo was taken on the Heist Island)
		{
			if (bSuccess)
			{
				bSuccess = rsonWriter.WriteBool(ON_ISLAND_X_KEY, m_bOnIslandX);
			}
		}
	}

	if (bSuccess)
	{
		bSuccess = rsonWriter.WriteUns64(IMAGE_SIGNATURE_KEY, m_ImageSignature);
	}

#if PHOTO_METADATA_STORES_NEARBY_VEHICLES
	WriteVehiclesToJson(rsonWriter, bSuccess);
#endif	//	PHOTO_METADATA_STORES_NEARBY_VEHICLES

#if PHOTO_METADATA_STORES_NEARBY_WEAPONS
	WriteWeaponsToJson(rsonWriter, bSuccess);
#endif	//	PHOTO_METADATA_STORES_NEARBY_WEAPONS

#if PHOTO_METADATA_STORES_NEARBY_MP_GAMERTAGS
	WriteGamerTagsToJson(rsonWriter, bSuccess);
#endif	//	PHOTO_METADATA_STORES_NEARBY_MP_GAMERTAGS

#if PHOTO_METADATA_STORES_NEARBY_SP_CHARS
	WriteSinglePlayerCharacterTextLabelsToJson(rsonWriter, bSuccess);
#endif	//	PHOTO_METADATA_STORES_NEARBY_SP_CHARS

	if (bSuccess)
	{
		bSuccess = rsonWriter.End();	//	Close the top-level structure
	}

	return bSuccess;
}

void CSavegamePhotoMetadata::GetPhotoLocation(Vector3& vLocation) const
{
	vLocation.x = m_fPlayerX;
	vLocation.y = m_fPlayerY;
	vLocation.z = m_fPlayerZ;
}

void CSavegamePhotoMetadata::ReadStringFromMetadata(RsonReader &rsonReader, RsonReader &valueReader, const char *pMemberName, atString &stringToFill)
{
	char stringFromMetadata[256];

	safecpy(stringFromMetadata, "", NELEM(stringFromMetadata));
	if (rsonReader.GetMember(pMemberName, &valueReader))
	{
		valueReader.AsString(stringFromMetadata, NELEM(stringFromMetadata));
	}

	if (strlen(stringFromMetadata) > 0)
	{
		stringToFill = stringFromMetadata;
	}
	else
	{
		stringToFill.Clear();
	}
}

bool CSavegamePhotoMetadata::ReadMetadataFromString(const char *pString)
{
	if (pString == NULL)
	{
		photoAssertf(0, "CSavegamePhotoMetadata::ReadMetadataFromString - pString is NULL");
		return false;
	}

	u32 lengthOfString = ustrlen(pString);

	// validation does not appreciate a null terminator
	if ( (lengthOfString > 0) && (pString[lengthOfString - 1] == '\0') )
	{
		photoDisplayf("CSavegamePhotoMetadata::ReadMetadataFromString - reduce string length by 1 so that the null terminator is ignored");
		lengthOfString -= 1;
	}

	// sometimes, the closing ']' CDATA blocks are included in the data size
	while ( (lengthOfString > 0) && (pString[lengthOfString - 1] == ']') )
	{
		photoDisplayf("CSavegamePhotoMetadata::ReadMetadataFromString - reduce string length by 1 so that a ']' is ignored");
		lengthOfString -= 1;
	}


	if (!RsonReader::ValidateJson(pString, lengthOfString))
	{
		photoDisplayf("CSavegamePhotoMetadata::ReadMetadataFromString - json data is not valid. The string is %s", pString);
		photoAssertf(0, "CSavegamePhotoMetadata::ReadMetadataFromString - json data is not valid. The string is %s", pString);
		return false;
	}

	RsonReader rsonReader(pString, lengthOfString);

	m_fPlayerX = 0.0f;
	m_fPlayerY = 0.0f;
	m_fPlayerZ = 0.0f;

	RsonReader valueReader;
	if (rsonReader.GetMember(LOCATION_KEY, &valueReader))
	{
		RsonReader coordReader;
		float fCoord;
		if (valueReader.GetMember("x", &coordReader))
		{
			if (coordReader.AsFloat(fCoord))
			{
				m_fPlayerX = fCoord;
			}
		}

		if (valueReader.GetMember("y", &coordReader))
		{
			if (coordReader.AsFloat(fCoord))
			{
				m_fPlayerY = fCoord;
			}
		}

		if (valueReader.GetMember("z", &coordReader))
		{
			if (coordReader.AsFloat(fCoord))
			{
				m_fPlayerZ = fCoord;
			}
		}
	}

	ReadStringFromMetadata(rsonReader, valueReader, AREA_KEY, m_AreaNameTextKey);

	m_HashOfStreetNameTextKey = 0;
	if (rsonReader.GetMember(STREET_KEY, &valueReader))
	{
		valueReader.AsUns(m_HashOfStreetNameTextKey);
	}

	ReadStringFromMetadata(rsonReader, valueReader, SINGLE_PLAYER_MISSION_NAME_KEY, m_SinglePlayerMissionName);

	ReadStringFromMetadata(rsonReader, valueReader, RADIO_STATION_KEY, m_RadioStationName);

	m_SongId = InvalidSongId;
	if (rsonReader.GetMember(SONG_KEY, &valueReader))
	{
		valueReader.AsUns(m_SongId);
	}

	m_MpSessionID = 0;
	if (rsonReader.GetMember(MP_SESSION_ID_KEY, &valueReader))
	{
		valueReader.AsUns64(m_MpSessionID);
	}

	m_PrimaryClanIdOfPlayer = 0;
	if (rsonReader.GetMember(MP_CLAN_ID_KEY, &valueReader))
	{
		valueReader.AsInt64(m_PrimaryClanIdOfPlayer);
	}

	ReadStringFromMetadata(rsonReader, valueReader, UGC_MISSION_UNIQUE_ID_KEY, m_UgcID);

	
	atString gameModeString;
	ReadStringFromMetadata(rsonReader, valueReader, GAME_MODE_KEY, gameModeString);
	m_GameMode = GetGameModeFromString(gameModeString.c_str());

	m_containsMemeText = false;
	if (rsonReader.GetMember(MEME_TEXT_KEY, &valueReader))
	{
		valueReader.AsBool(m_containsMemeText);
	}

	m_bMugshot = false;
	if (rsonReader.GetMember(MUGSHOT_KEY, &valueReader))
	{
		valueReader.AsBool(m_bMugshot);
	}


	m_UniqueId.Reset();
	if (rsonReader.GetMember(UNIQUE_PHOTO_ID_KEY, &valueReader))
	{
		int nUniqueId = 0;
		valueReader.AsInt(nUniqueId);
		m_UniqueId.Set(nUniqueId, false);
	}

	m_GameTimeHour = 0;
	m_GameTimeMinute = 0;
	m_GameTimeSecond = 0;

	m_GameTimeDayOfMonth = 1;
	m_GameTimeMonth = 1;
	m_GameTimeYear = 2000;
	if (rsonReader.GetMember(GAME_TIME_KEY, &valueReader))
	{
		RsonReader timeReader;
		int timeValue;

		if (valueReader.GetMember(GAME_TIME_HOUR, &timeReader))
		{
			if (timeReader.AsInt(timeValue))
			{
				m_GameTimeHour = timeValue;
			}
		}

		if (valueReader.GetMember(GAME_TIME_MINUTE, &timeReader))
		{
			if (timeReader.AsInt(timeValue))
			{
				m_GameTimeMinute = timeValue;
			}
		}

		if (valueReader.GetMember(GAME_TIME_SECOND, &timeReader))
		{
			if (timeReader.AsInt(timeValue))
			{
				m_GameTimeSecond = timeValue;
			}
		}


		if (valueReader.GetMember(GAME_TIME_DAY, &timeReader))
		{
			if (timeReader.AsInt(timeValue))
			{
				m_GameTimeDayOfMonth = timeValue;
			}
		}

		if (valueReader.GetMember(GAME_TIME_MONTH, &timeReader))
		{
			if (timeReader.AsInt(timeValue))
			{
				m_GameTimeMonth = timeValue;
			}
		}

		if (valueReader.GetMember(GAME_TIME_YEAR, &timeReader))
		{
			if (timeReader.AsInt(timeValue))
			{
				m_GameTimeYear = timeValue;
			}
		}
	}

	m_PosixCreationTime = 0;
	if (rsonReader.GetMember(POSIX_CREATION_TIME_KEY, &valueReader))
	{
		s32 signedTime = 0;
		valueReader.AsInt(signedTime);
		m_PosixCreationTime = (u32) signedTime;
	}

	m_ArenaTheme = 0;
	if (rsonReader.GetMember(ARENA_THEME_INDEX_KEY, &valueReader))
	{
		valueReader.AsUns(m_ArenaTheme);
	}

	m_ArenaVariation = 0;
	if (rsonReader.GetMember(ARENA_VARIATION_INDEX_KEY, &valueReader))
	{
		valueReader.AsUns(m_ArenaVariation);
	}



#if PHOTO_METADATA_STORES_WIDESCREEN_FLAG
	m_bIsWidescreen = true;
	if (rsonReader.GetMember(WIDESCREEN_KEY, &valueReader))
	{
		valueReader.AsBool(m_bIsWidescreen);
	}
#endif	//	PHOTO_METADATA_STORES_WIDESCREEN_FLAG

#if PHOTO_METADATA_STORES_SELFIE_FLAG
	m_bIsSelfie = false;
	if (rsonReader.GetMember(SELFIE_KEY, &valueReader))
	{
		valueReader.AsBool(m_bIsSelfie);
	}
#endif	//	PHOTO_METADATA_STORES_SELFIE_FLAG

	m_bIsInDirectorMode = false;
	if (rsonReader.GetMember(DIRECTOR_KEY, &valueReader))
	{
		valueReader.AsBool(m_bIsInDirectorMode);
	}

	m_bTakenInRockstarEditor = false;
	if (rsonReader.GetMember(ROCKSTAR_EDITOR_KEY, &valueReader))
	{
		valueReader.AsBool(m_bTakenInRockstarEditor);
	}

	m_bOnIslandX = false;
	if (rsonReader.GetMember(ON_ISLAND_X_KEY, &valueReader))
	{
		valueReader.AsBool(m_bOnIslandX);
	}

	m_Akamai = false;
	if (rsonReader.GetMember(AKAMAI_KEY, &valueReader))
	{
		valueReader.AsBool(m_Akamai);
	}

	m_ImageSignature = 0;
	if (rsonReader.GetMember(IMAGE_SIGNATURE_KEY, &valueReader))
	{
		valueReader.AsUns64(m_ImageSignature);
	}

#if PHOTO_METADATA_STORES_NEARBY_VEHICLES
	ReadVehiclesFromJson(rsonReader);
#endif	//	PHOTO_METADATA_STORES_NEARBY_VEHICLES

#if PHOTO_METADATA_STORES_NEARBY_WEAPONS
	ReadWeaponsFromJson(rsonReader);
#endif	//	PHOTO_METADATA_STORES_NEARBY_WEAPONS

#if PHOTO_METADATA_STORES_NEARBY_MP_GAMERTAGS
	ReadGamerTagsFromJson(rsonReader);
#endif	//	PHOTO_METADATA_STORES_NEARBY_MP_GAMERTAGS

#if PHOTO_METADATA_STORES_NEARBY_SP_CHARS
	ReadSinglePlayerCharacterTextLabelsFromJson(rsonReader);
#endif	//	PHOTO_METADATA_STORES_NEARBY_SP_CHARS

	return true;
}


const char *CSavegamePhotoMetadata::GetStringFromGameMode(u8 gameMode) const
{
	switch (gameMode)
	{
		case 0 :
			return "SP";
//			break;

		case 1 :
			return "FREEMODE";
//			break;
	}

	return "SP";
}

u8 CSavegamePhotoMetadata::GetGameModeFromString(const char *pGameModeString) const
{
	if (stricmp(pGameModeString, "FREEMODE") == 0)
	{
		return 1;
	}

	return 0;
}

#if __BANK
//	Add the lengths of all atStrings to the sizeof(CSavegamePhotoMetadata)
u32 CSavegamePhotoMetadata::GetSize() const
{
	u32 returnSize = sizeof(CSavegamePhotoMetadata);

	returnSize += m_AreaNameTextKey.GetLength();

	returnSize += m_SinglePlayerMissionName.GetLength();

	returnSize += m_RadioStationName.GetLength();

	returnSize += m_UgcID.GetLength();

	return returnSize;
}


void CSavegamePhotoMetadata::UpdateDebug()
{
	if (sm_bDisplayBoundingRectanglesForPeds)
	{
		CPlayerInfo* pPlayerInfo = CGameWorld::GetMainPlayerInfo();
		CPed *pPlayerPed = pPlayerInfo ? pPlayerInfo->GetPlayerPed() : NULL;

		if (pPlayerPed)
		{
			CEntityScannerIterator pedIterator = pPlayerPed->GetPedIntelligence()->GetNearbyPeds();
			CEntity* pEntity = pedIterator.GetFirst();
			while(pEntity)
			{
				IsEntityVisible(pEntity, true);

				pEntity = pedIterator.GetNext();
			}
		}	//	if (pPlayerPed)
	}
}

#endif	//	__BANK




void CSavegamePhotoMetadata::GetNormalisedScreenCoordFromWorldCoord(const Vector3 &vecWorldPos, float &NormalisedX, float &NormalisedY)
{
	const grcViewport *pCurrentGrcViewport = gVpMan.GetCurrentGameGrcViewport();

	float fWindowX = 0.0f, fWindowY = 0.0f;
	pCurrentGrcViewport->Transform(RCC_VEC3V(vecWorldPos), fWindowX, fWindowY);

	fWindowX=( fWindowX*grcViewport::GetDefaultScreen()->GetWidth() ) /pCurrentGrcViewport->GetWidth();
	fWindowY=( fWindowY*grcViewport::GetDefaultScreen()->GetHeight() ) /pCurrentGrcViewport->GetHeight();

	NormalisedX = fWindowX / pCurrentGrcViewport->GetWidth();
	NormalisedY = fWindowY / pCurrentGrcViewport->GetHeight();

	if (NormalisedX < 0.0f)
	{
		NormalisedX = 0.0f;
	}

	if (NormalisedX > 1.0f)
	{
		NormalisedX = 1.0f;
	}


	if (NormalisedY < 0.0f)
	{
		NormalisedY = 0.0f;
	}

	if (NormalisedY > 1.0f)
	{
		NormalisedY = 1.0f;
	}
}

void CSavegamePhotoMetadata::UpdateLimitsOfRectangle(const Vector3 &vecWorldPos, float &MinX, float &MinY, float &MaxX, float &MaxY)
{
	float normalisedX = 0.0f;
	float normalisedY = 0.0f;

	GetNormalisedScreenCoordFromWorldCoord(vecWorldPos, normalisedX, normalisedY);

	if (normalisedX < MinX)
	{
		MinX = normalisedX;
	}

	if (normalisedX > MaxX)
	{
		MaxX = normalisedX;
	}

	if (normalisedY < MinY)
	{
		MinY = normalisedY;
	}

	if (normalisedY > MaxY)
	{
		MaxY = normalisedY;
	}
}

float CSavegamePhotoMetadata::GetScreenAreaOfBoundingBox(const spdAABB &aabb, bool BANK_ONLY(bDisplayBoundingRectangle))
{
	float screenArea = 0.0f;

	const Vector3 vMin = VEC3V_TO_VECTOR3(aabb.GetMin());
	const Vector3 vMax = VEC3V_TO_VECTOR3(aabb.GetMax());

	float fBoxMinX = 1.0f;
	float fBoxMaxX = 0.0f;

	float fBoxMinY = 1.0f;
	float fBoxMaxY = 0.0f;

	Vector3 vCorners[8] = {
		Vector3(vMin.x, vMin.y, vMin.z), Vector3(vMax.x, vMin.y, vMin.z), Vector3(vMax.x, vMax.y, vMin.z), Vector3(vMin.x, vMax.y, vMin.z),
		Vector3(vMin.x, vMin.y, vMax.z), Vector3(vMax.x, vMin.y, vMax.z), Vector3(vMax.x, vMax.y, vMax.z), Vector3(vMin.x, vMax.y, vMax.z)
	};

	for(u32 v=0; v<8; v++)
	{
		UpdateLimitsOfRectangle(vCorners[v], fBoxMinX, fBoxMinY, fBoxMaxX, fBoxMaxY);
	}

	if ( (fBoxMinX < 1.0f) && (fBoxMaxX > 0.0f) && (fBoxMinY < 1.0f) && (fBoxMaxY > 0.0f) )
	{
		screenArea = (fBoxMaxX - fBoxMinX) * (fBoxMaxY - fBoxMinY);
#if __BANK
		if (bDisplayBoundingRectangle)
		{
			grcDebugDraw::Quad(Vec2V(fBoxMinX, fBoxMinY), Vec2V(fBoxMinX, fBoxMaxY), Vec2V(fBoxMaxX, fBoxMaxY), Vec2V(fBoxMaxX, fBoxMinY), Color32(1.0f, 0.0f, 0.0f, 0.5f), false);
		}
#endif	//	__BANK
	}

	return screenArea;
}


bool CSavegamePhotoMetadata::IsEntityVisible(CEntity *pEntity, bool bDisplayDebug)
{
	const float fMinimumScreenAreaForPeds = 0.001f;	//		//	This seemed a reasonable figure when I tested the player ped. If we need to deal with vehicles then I'll probably need a separate number for them.

	if (pEntity->GetIsVisible())
	{
		spdAABB		tempBox;
		const spdAABB &aabb = pEntity->GetBoundBox( tempBox );
		if (COcclusion::IsBoxVisible( aabb ))
		{
			float fScreenArea = GetScreenAreaOfBoundingBox(aabb, bDisplayDebug);

#if __BANK
			if (bDisplayDebug)
			{
				const Vector3 vecEntityPosition = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
				const Vector3 vecDebugWorldCoors = vecEntityPosition + Vector3(0,0,1.0f);

				char debugText[16];
				formatf(debugText, 16, "%f", fScreenArea);
				grcDebugDraw::Text(vecDebugWorldCoors, Color32(1.0f, 0.0f, 0.0f, 0.5f), debugText);
			}
#endif	//	__BANK

			if (fScreenArea > fMinimumScreenAreaForPeds)
			{
				return true;
			}
		}
	}

	return false;
}


#if PHOTO_METADATA_STORES_NEARBY_SP_CHARS

const u32 MAX_LENGTH_OF_TEXT_LABEL_FOR_CHARACTERS_NAME = 16;

//	I searched for all ped models beginning with IG_ in model_enums.sch
//	These models aren't listed in Initialise_CharSheet_Global_Variables_On_Startup()
//	Would I need to add their names to the text files?
// IG_BALLASOG=-1492432238		//	Paradise2, Franklin0, Lamar1
// IG_BANKMAN=-1868718465		//	finale_heist2A
// IG_BESTMEN=1464257942		//	re_stag_do
// IG_BRAD=-1111799518			//	Prologue1
// IG_BRIDE=1633872967			//	re_hitch_lift and re_stag_do
// IG_CAR3GUY1=-2063996617		//	carsteal1
// IG_CAR3GUY2=1975732938		//	carsteal1
// IG_CASEY=-520477356			//	finale_heist2A
// IG_CHRISFORMAGE=678319271	//	only used in multiplayer net_freemode_cut.sch
// IG_CLAY=1825562762			//	Trevor1
// IG_CLAYPAIN					//	Franklin1
// IG_DALE=1182012905			//	Not referenced in script
// IG_FABIEN=-795819184			//	Family5 and Family6
// IG_FBISUIT_01=988062523		//	FBI2
// IG_GROOM=-20018299			//	Not referenced in script
// IG_HUNTER=-837606178			//	TheLastOne.sc and initial_scenes_TheLastOne.sch
// IG_JANET=225287241			//	Chinese1, Chinese2, Minute1
// IG_JAY_NORRIS=2050158196		//	Lester1
// IG_JEWELASS=257763003		//	jewelry_heist.sc and jewelry_setup1.sc
// IG_JOHNNYKLEBITZ=-2016771922	//	Trevor1.sc
// IG_KERRYMCINTOSH=1530648845	//	initial_scenes_Nigel.sch
// IG_LIFEINVAD_02=666718676	//	Not referenced in script
// IG_MAGENTA=-52653814			//	Lamar1
// IG_MICHELLE=-1080659212		//	Not referenced in script
// IG_MILTON=-886023758			//	carsteal3, Michael4 and Solomon1
// IG_MRK=-304305299			//	FBI2 and FBI3
// IG_MRSPHILLIPS=946007720		//	Not referenced in script
// IG_NATALIA=-568861381		//	Not referenced in script
// IG_OLD_MAN1A=1906124788		//	Chinese1, Chinese2, trigger_scene_chinese1.sch and trigger_scene_chinese2.sch
// IG_OLD_MAN2=-283816889		//	Chinese1, Chinese2, trigger_scene_chinese1.sch and trigger_scene_chinese2.sch
// IG_ONEIL=768005095			//	Not referenced in script
// IG_PAPER=-1717894970			//	Michael3 and Michael3_stage_firstarea.sch
// IG_PRIEST=1681385341			//	Not referenced in script
// IG_PROLSEC_02=666086773		//	Prologue1
// IG_RAMP_GANG=-449965460		//	rampagev4_include.sch and initial_scenes_rampage.sch
// IG_RAMP_HIC=1165307954		//	rampagev4_include.sch and initial_scenes_rampage.sch
// IG_RAMP_HIPSTER=-554721426	//	rampagev4_include.sch and initial_scenes_rampage.sch
// IG_RAMP_MEX=-424905564		//	rampagev4_include.sch and initial_scenes_rampage.sch
// IG_ROCCOPELOSI=-709209345	//	Solomon1, Solomon2 and trigger_scene_solomon2.sch
// IG_RUSSIANDRUNK=1024089777	//	Chinese1 and trigger_scene_chinese1.sch
// IG_SCREEN_WRITER=-1689993	//	Not referenced in script
// IG_TALINA=-409745176			//	re_crashrescue
// IG_TAOSTRANSLATOR=2089096292 //	Chinese1, Chinese2 and trigger_scene_chinese2.sch
// IG_TERRY=1728056212			//	Trevor1
// IG_TOMEPSILON=-847807830		//	Epsilon6, Epsilon7, epsDesert and initial_scenes_Epsilon.sch
// IG_TONYA=-892841148			//	gpb_Tonya, ambient_TonyaCall, ambient_TonyaCall2, ambient_TonyaCall5, Tonya1, Tonya2, Tonya5, initial_scenes_Tonya.sch
// IG_TRAFFICWARDEN=1461287021	//	jewelry_heist
// IG_TYLERDIX=1382414087		//	Nigel1B and initial_scenes_Nigel.sch
// IG_ZIMBOR=188012277			//	Not referenced in script

const char *CSavegamePhotoMetadata::GetTextLabelForPedModel(const CPed *pPed)
{
	if (pPed)
	{
		CBaseModelInfo* pModel = CModelInfo::GetBaseModelInfo(pPed->GetModelId());
		if (Verifyf(pModel, "CSavegamePhotoMetadata::GetTextLabelForPedModel - couldn't find model for the ped's model index"))
		{
			//	I copied this data from Steve Taylor's Initialise_CharSheet_Global_Variables_On_Startup() in charsheet_public.sch

			switch (pModel->GetHashKey())
			{
			case 0xD7114C9 :	//	( ATSTRINGHASH("PLAYER_ZERO", 0xD7114C9), "CELL_101");			//	CHAR_MICHAEL
				return "CELL_101";

			case 0x9b810fa2 :	//	( ATSTRINGHASH("PLAYER_TWO", 0x9b810fa2), "CELL_102");			//	CHAR_TREVOR
				return "CELL_102";

			case 0x9b22dbaf :	//	( ATSTRINGHASH("PLAYER_ONE", 0x9b22dbaf), "CELL_103");			//	CHAR_FRANKLIN
				return "CELL_103";

				//Virtual multiplayer character - CHAR_MULTIPLAYER - A_M_Y_BeachVesp_01, "CELL_197"

			case 0x4da6e849 :	//	( ATSTRINGHASH("IG_LESTERCREST", 0x4da6e849), "CELL_111");		//	FBI dude - CHAR_LESTER
				return "CELL_111";

				//	Ask Steve about this one - CHAR_LESTER_DEATHWISH - IG_LESTERCREST, "CELL_E_211" "Lester Deathwish"

			case 0x14ec17ea :	//	( ATSTRINGHASH("A_C_CHOP", 0x14ec17ea), "CELL_E_225");				//	Chop
				return "CELL_E_225";

			case 0x570462b9 :	//	( ATSTRINGHASH("IG_JIMMYDISANTO", 0x570462b9), "CELL_124");			//	CHAR_JIMMY
				return "CELL_124";

			case 0xde352a35 :	//	( ATSTRINGHASH("IG_TRACYDISANTO", 0xde352a35), "CELL_125");			//	CHAR_TRACEY
				return "CELL_125";

			case 0x400aec41 :	//	( ATSTRINGHASH("IG_ABIGAIL", 0x400aec41), "CELL_E_240");				//	CHAR_ABIGAIL
				return "CELL_E_240";

			case 0x6d1e15f7 :	//	( ATSTRINGHASH("IG_AMANDATOWNLEY", 0x6d1e15f7), "CELL_126");			//	CHAR_AMANDA
				return "CELL_126";

			case 0x4c7b2f05 :	//	( ATSTRINGHASH("IG_SIEMONYETARIAN", 0x4c7b2f05), "CELL_127");			//	CHAR_SIMEON
				return "CELL_127";

			case 0x65b93076 :	//	( ATSTRINGHASH("IG_LAMARDAVIS", 0x65b93076), "CELL_128");			//	CHAR_LAMAR
				return "CELL_128";

			case 0xbd006af1 :	//	( ATSTRINGHASH("IG_NERVOUSRON", 0xbd006af1), "CELL_129");			//	CHAR_RON
				return "CELL_129";

			case 0xdc5c5ea5 :	//	( ATSTRINGHASH("IG_TAOCHENG", 0xdc5c5ea5), "CELL_133");			//	CHAR_CHENG
				return "CELL_133";

				//	Ask if Saeeda has a specific model - CHAR_SAEEDA - IG_TAOCHENG, "CELL_E_281"

			case 0x382121c8 :	//	( ATSTRINGHASH("IG_STEVEHAINS", 0x382121c8), "CELL_134");			//	CHAR_STEVE
				return "CELL_134";

			case 0x92991b72 :	//	( ATSTRINGHASH("IG_WADE", 0x92991b72), "CELL_135");			//	CHAR_WADE
				return "CELL_135";

			case 0xa23b5f57 :	//	( ATSTRINGHASH("IG_TENNISCOACH", 0xa23b5f57), "CELL_136");			//	CHAR_TENNIS_COACH
				return "CELL_136";

			case 0x86bdfe26 :	//	( ATSTRINGHASH("IG_SOLOMON", 0x86bdfe26), "CELL_137");			//	CHAR_SOLOMON
				return "CELL_137";

			case 0xdfe443e5 :	//	( ATSTRINGHASH("IG_LAZLOW", 0xdfe443e5), "CELL_138");			//	CHAR_LAZLOW
				return "CELL_138";

				//	CHAR_ESTATE_AGENT - A_M_Y_BUSINESS_01, "CELL_139" - Could an ambient ped use this model and be mistaken for this character?

			case 0x7461a0b0 :	//	( ATSTRINGHASH("IG_DEVIN", 0x7461a0b0), "CELL_142");			//	CHAR_DEVIN
				return "CELL_142";

			case 0x15cd4c33 :	//	( ATSTRINGHASH("IG_DAVENORTON", 0x15cd4c33), "CELL_143");			//	CHAR_DAVE
				return "CELL_143";

				//	CHAR_MARTIN - A_M_Y_BUSINESS_01, "CELL_144" - Could an ambient ped use this model and be mistaken for this character?

			case 0xb1b196b2 :	//	( ATSTRINGHASH("IG_FLOYD", 0xb1b196b2), "CELL_145");			//	CHAR_FLOYD
				return "CELL_145";

				//	CHAR_GAYMILITARY - A_M_Y_BUSINESS_01, "CELL_146" - Could an ambient ped use this model and be mistaken for this character?
				//	CHAR_OSCAR - G_M_Y_MexGoon_02, "CELL_164" - Could an ambient ped use this model and be mistaken for this character?

			case 0xaae4ea7b :	//	( ATSTRINGHASH("IG_CHENGSR", 0xaae4ea7b), "CELL_200");			//	CHAR_CHENGSR
				return "CELL_200";

			case 0xcbfc0df5 :	//	( ATSTRINGHASH("IG_DRFRIEDLANDER", 0xcbfc0df5), "CELL_121");			//	CHAR_DR_FRIEDLANDER
				return "CELL_121";

			case 0x36984358 :	//	( ATSTRINGHASH("IG_STRETCH", 0x36984358), "CELL_122");			//	CHAR_STRETCH
				return "CELL_122";

			case 0x26a562b7 :	//	( ATSTRINGHASH("IG_ORTEGA", 0x26a562b7), "CELL_123");			//	CHAR_ORTEGA
				return "CELL_123";

				//	CHAR_ONEIL - A_M_M_FARMER_01, "CELL_E_208" - Could an ambient ped use this model and be mistaken for this character?

			case 0xc56e118c :	//	( ATSTRINGHASH("IG_PATRICIA", 0xc56e118c), "CELL_E_210");			//	CHAR_PATRICIA
				return "CELL_E_210";

			case 0x0d810489 :	//	( ATSTRINGHASH("IG_TANISHA", 0x0d810489), "CELL_E_218");			//	CHAR_TANISHA
				return "CELL_E_218";

			case 0x820b33bd :	//	( ATSTRINGHASH("IG_DENISE", 0x820b33bd), "CELL_E_226");			//	CHAR_DENISE
				return "CELL_E_226";

			case 0xaf03dde1 :	//	( ATSTRINGHASH("IG_MOLLY", 0xaf03dde1), "CELL_E_227");			//	CHAR_MOLLY
				return "CELL_E_227";

			case 0x5389a93c :	//	( ATSTRINGHASH("IG_LIFEINVAD_01", 0x5389a93c), "CELL_E_217");			//	CHAR_RICKIE
				return "CELL_E_217";

			case 0x49eadbf6 :	//	( ATSTRINGHASH("IG_CHEF", 0x49eadbf6), "CELL_E_224");			//	CHAR_CHEF
				return "CELL_E_224";

				//Random Characters.
			case 0x2f8845a3 :	//	( ATSTRINGHASH("IG_BARRY", 0x2f8845a3), "CELL_147");			//	CHAR_BARRY
				return "CELL_147";

			case 0xbda21e5c :	//	( ATSTRINGHASH("IG_BEVERLY", 0xbda21e5c), "CELL_148");			//	CHAR_BEVERLY
				return "CELL_148";

				//	CHAR_BLIMP - IG_BEVERLY, "CELL_E_279"
				//	CHAR_CRIS - S_M_M_HIGHSEC_01, "CELL_166" - Could an ambient ped use this model and be mistaken for this character?

			case 0x9c2db088 :	//	( ATSTRINGHASH("IG_DOM", 0x9c2db088), "CELL_150");			//	CHAR_DOM
				return "CELL_150";

			case 0x65978363 :	//	( ATSTRINGHASH("IG_HAO", 0x65978363), "CELL_E_246");			//	CHAR_HAO
				return "CELL_E_246";

			case 0xe6631195 :	//	( ATSTRINGHASH("IG_CLETUS", 0xe6631195), "CELL_167");			//	Known as "Cletus" - CHAR_HUNTER
				return "CELL_167";

			case 0xeda0082d :	//	( ATSTRINGHASH("IG_JIMMYBOSTON", 0xeda0082d), "CELL_151");			//	CHAR_JIMMY_BOSTON
				return "CELL_151";

			case 0xbe204c9b :	//	( ATSTRINGHASH("IG_JOEMINUTEMAN", 0xbe204c9b), "CELL_152");			//	CHAR_JOE
				return "CELL_152";

			case 0xe11a9fb4 :	//	( ATSTRINGHASH("IG_JOSEF", 0xe11a9fb4), "CELL_153");			//	CHAR_JOSEF
				return "CELL_153";

			case 0x799e9eee :	//	( ATSTRINGHASH("IG_JOSH", 0x799e9eee), "CELL_154");			//	CHAR_JOSH
				return "CELL_154";

			case 0xfd418e10 :	//	( ATSTRINGHASH("IG_MANUEL", 0xfd418e10), "CELL_156");			//	CHAR_MANUEL
				return "CELL_156";

			case 0x188232d0 :	//	( ATSTRINGHASH("IG_MARNIE", 0x188232d0), "CELL_157");			//	CHAR_MARNIE
				return "CELL_157";

			case 0xa36f9806 :	//	( ATSTRINGHASH("IG_MARYANN", 0xa36f9806), "CELL_158");			//	CHAR_MARY_ANN
				return "CELL_158";

			case 0x3be8287e :	//	( ATSTRINGHASH("IG_MAUDE", 0x3be8287e), "CELL_E_244");			//	CHAR_MAUDE
				return "CELL_E_244";

			case 0x1e04a96b :	//	( ATSTRINGHASH("IG_MRS_THORNHILL", 0x1e04a96b), "CELL_161");			//	CHAR_MRS_THORNHILL
				return "CELL_161";

			case 0xc8b7167d :	//	( ATSTRINGHASH("IG_NIGEL", 0xc8b7167d), "CELL_162");			//	CHAR_NIGEL
				return "CELL_162";

			case 0x61d4c771 :	//	( ATSTRINGHASH("IG_ORLEANS", 0x61d4c771), "CELL_168");			//	CHAR_SASQUATCH
				return "CELL_168";

			case 0x7ef440db :	//	( ATSTRINGHASH("IG_ASHLEY", 0x7ef440db), "CELL_E_202");			//	CHAR_ASHLEY
				return "CELL_E_202";

			case 0x47e4eea0 :	//	( ATSTRINGHASH("IG_ANDREAS", 0x47e4eea0), "CELL_E_205");			//	CHAR_ANDREAS
				return "CELL_E_205";

			case 0xda890932 :	//	( ATSTRINGHASH("IG_DREYFUSS", 0xda890932), "CELL_E_206");			//	CHAR_DREYFUSS
				return "CELL_E_206";

			case 0x60e6a7d8 :	//	( ATSTRINGHASH("IG_OMEGA", 0x60e6a7d8), "CELL_E_207");			//	CHAR_OMEGA
				return "CELL_E_207";

				// Could an ambient ped use one of these models and be mistaken for one of these characters?
				//	CHAR_DOMESTIC_GIRL - A_M_Y_BeachVesp_01, "CELL_140"
				//Now known as Ursula - CHAR_HITCHER_GIRL - A_F_Y_HIKER_01, "CELL_141"
				//	CHAR_MECHANIC - S_M_Y_XMECH_02, "CELL_180"

				//	How can we differentiate the stripper models
				//	CHAR_STRIPPER_JULIET - S_F_Y_STRIPPER_01, "CELL_112"
				//	CHAR_STRIPPER_NIKKI - S_F_Y_STRIPPER_02, "CELL_113"
				//	CHAR_STRIPPER_CHASTITY - S_F_Y_STRIPPER_01, "CELL_114"
				//	CHAR_STRIPPER_CHEETAH - S_F_Y_STRIPPER_02, "CELL_115"
				//	CHAR_STRIPPER_SAPPHIRE - S_F_Y_STRIPPER_01, "CELL_116"
				//	CHAR_STRIPPER_INFERNUS - S_F_Y_STRIPPER_02, "CELL_117"
				//	CHAR_STRIPPER_FUFU - S_F_Y_STRIPPER_01, "CELL_118"
				//	CHAR_STRIPPER_PEACH - S_F_Y_STRIPPER_02, "CELL_119"

				// Could an ambient ped use one of these models and be mistaken for one of these characters?
				//	CHAR_BROKEN_DOWN_GIRL - A_F_Y_Fitness_02, "CELL_120"
				//	CHAR_ANTONIA - A_F_Y_Hipster_01, "CELL_E_280"
				//	CHAR_TAXI_LIZ - A_F_Y_EASTSA_03, "CELL_E_201"
				// Towing ped, Tonya //	CHAR_TOW_TONYA - A_F_M_FatBla_01, "CELL_E_223"


				//	The following peds all use the same model. Are they never seen? Do they only speak on the phone?
				//Special Number - see bug 1020339
				//	CHAR_LS_CUSTOMS - A_M_Y_BeachVesp_01, "CELL_E_209"
				//	CHAR_AMMUNATION - A_M_Y_BeachVesp_01, "CELL_E_220"

				//MP Contacts
				// ...bosses
				// Lost: Al Carter (which is also the name of the Lost Mechanic so slight discrepency there)
				//	CHAR_MP_BIKER_BOSS - A_M_Y_BeachVesp_01, "CELL_174"
				//	CHAR_MP_FAM_BOSS - A_M_Y_BeachVesp_01, "CELL_176"
				// Vagos: Edgar Claros //	CHAR_MP_MEX_BOSS - A_M_Y_BeachVesp_01, "CELL_177"
				//	CHAR_MP_PROF_BOSS - A_M_Y_BeachVesp_01, "CELL_178"
				// ...lieutenants
				//	CHAR_MP_MEX_LT - A_M_Y_BeachVesp_01, "CELL_E_204"
				// ...car mechanic contacts                                          
				//	CHAR_MP_BIKER_MECHANIC - A_M_Y_BeachVesp_01, "CELL_173"
				// ...docks contacts
				//	CHAR_MP_MEX_DOCKS - A_M_Y_BeachVesp_01, "CELL_165"
				//	CHAR_MP_STRETCH - A_M_Y_BeachVesp_01, "CELL_172"
				// MP Freemode
				// ...general contact (used in intro)
				//	CHAR_MP_FM_CONTACT - A_M_Y_BeachVesp_01, "CELL_E_215"

				// ...brucie (used as a special ability contact)
				//	CHAR_MP_BRUCIE - A_M_Y_BeachVesp_01, "CELL_E_216"

				// ...merryweather (used as a special ability contact)  
				//	CHAR_MP_MERRYWEATHER - A_M_Y_BeachVesp_01, "CELL_E_221"

				// ...gerald (used as a Contact Mission contact)  
				//	CHAR_MP_GERALD - A_M_Y_BeachVesp_01, "CELL_E_228"

				// ...mechanic (used as a special ability contact) //Los Santos Customs at present.
				//	CHAR_MP_MECHANIC - A_M_Y_BeachVesp_01, "CELL_E_MP0"

				// ...Julio Fabrizio
				//	CHAR_MP_JULIO - A_M_Y_BeachVesp_01, "CELL_E_242"

				// ...Strip Club PR  //Listed as promotion@V-Unicorn.
				//	CHAR_MP_STRIPCLUB_PR - A_M_Y_BeachVesp_01, "CELL_E_243"

				// ...snitches
				// KGM 28/3/12: Generic MP Snitch - Corey Parker
				//	CHAR_MP_SNITCH - A_M_Y_BeachVesp_01, "CELL_169"

				// ...other mission flow contacts
				//	CHAR_MP_FIB_CONTACT - A_M_Y_BeachVesp_01, "CELL_184"
				//	CHAR_MP_ARMY_CONTACT - A_M_Y_BeachVesp_01, "CELL_185"

				//Pegasus vehicle delivery service - see bug #1300310
				//	CHAR_PEGASUS_DELIVERY - A_M_Y_BeachVesp_01, "CELL_E_247"

				//	CHAR_LIFEINVADER - A_M_Y_BeachVesp_01, "CELL_E_276"

				// ...special characters for MP missions
				//	CHAR_MP_ROBERTO - A_M_Y_BeachVesp_01, "CELL_182"
				//	CHAR_MP_RAY_LAVOY - A_M_Y_BeachVesp_01, "CELL_183"

			}	//	switch (pModel->GetHashKey())
		}	//	if (Verifyf(pModel
	}	//	if (pPed)

	return "";
}


void CSavegamePhotoMetadata::AddPedToSinglePlayerCharacterArray(CPed *pPed, s32 &number_of_single_player_characters)
{
	const char *pTextLabelOfCharacterName = GetTextLabelForPedModel(pPed);
	if (strlen(pTextLabelOfCharacterName) > 0)
	{
		if (number_of_single_player_characters < MAX_NUM_VISIBLE_SINGLE_PLAYER_CHARACTERS)
		{
			atString characterNameTextLabel(pTextLabelOfCharacterName);
			m_AtArrayOfSinglePlayerCharacters.PushAndGrow(characterNameTextLabel, 1);

			number_of_single_player_characters++;
		}
	}
}


void CSavegamePhotoMetadata::WriteSinglePlayerCharacterTextLabelsToJson(RsonWriter &rsonWriter, bool &bSuccess) const
{
	if (bSuccess)
	{
		char arrayOfCharacterNames[MAX_NUM_VISIBLE_SINGLE_PLAYER_CHARACTERS][MAX_LENGTH_OF_TEXT_LABEL_FOR_CHARACTERS_NAME];

		const u32 sizeOfCharacterNameArray = m_AtArrayOfSinglePlayerCharacters.GetCount();
		u32 number_of_visible_single_player_characters = 0;
		for (u32 characterNameLoop = 0; characterNameLoop < sizeOfCharacterNameArray; characterNameLoop++)
		{
			if (m_AtArrayOfSinglePlayerCharacters[characterNameLoop].GetLength() != 0)
			{
				safecpy(arrayOfCharacterNames[number_of_visible_single_player_characters], m_AtArrayOfSinglePlayerCharacters[characterNameLoop].c_str(), MAX_LENGTH_OF_TEXT_LABEL_FOR_CHARACTERS_NAME);
				number_of_visible_single_player_characters++;
			}
		}

		if (number_of_visible_single_player_characters > 0)
		{
			bSuccess = rsonWriter.WriteArray(SINGLE_PLAYER_CHARACTERS_KEY, arrayOfCharacterNames, number_of_visible_single_player_characters);
		}
	}
}


void CSavegamePhotoMetadata::ReadSinglePlayerCharacterTextLabelsFromJson(RsonReader &rsonReader)
{
	RsonReader valueReader;
	u32 loop = 0;

	m_AtArrayOfSinglePlayerCharacters.Reset();

	if (rsonReader.GetMember(SINGLE_PLAYER_CHARACTERS_KEY, &valueReader))
	{
		char arrayOfCharacterNames[MAX_NUM_VISIBLE_SINGLE_PLAYER_CHARACTERS][MAX_LENGTH_OF_TEXT_LABEL_FOR_CHARACTERS_NAME];
		for (loop = 0; loop < MAX_NUM_VISIBLE_SINGLE_PLAYER_CHARACTERS; loop++)
		{
			safecpy(arrayOfCharacterNames[loop], "", MAX_LENGTH_OF_TEXT_LABEL_FOR_CHARACTERS_NAME);
		}


		//	AsArray returns the number of entries that have been read
		valueReader.AsArray(arrayOfCharacterNames, MAX_NUM_VISIBLE_SINGLE_PLAYER_CHARACTERS);

		u32 number_of_character_text_labels_read_from_json = 0;
		for (loop = 0; loop < MAX_NUM_VISIBLE_SINGLE_PLAYER_CHARACTERS; loop++)
		{
			if (arrayOfCharacterNames[loop][0] != '\0')
			{
				number_of_character_text_labels_read_from_json++;
			}
		}

		if (number_of_character_text_labels_read_from_json > 0)
		{
			m_AtArrayOfSinglePlayerCharacters.Reserve(number_of_character_text_labels_read_from_json);

			for (loop = 0; loop < MAX_NUM_VISIBLE_SINGLE_PLAYER_CHARACTERS; loop++)
			{
				if (arrayOfCharacterNames[loop][0] != '\0')
				{
					m_AtArrayOfSinglePlayerCharacters.Append() = arrayOfCharacterNames[loop];
				}
			}
		}
	}

#if __DEV
	const u32 sizeOfTextLabelArray = m_AtArrayOfSinglePlayerCharacters.GetCount();
	for (loop = 0; loop < sizeOfTextLabelArray; loop++)
	{
		photoDisplayf("CSavegamePhotoMetadata::ReadSinglePlayerCharacterTextLabelsFromJson - names of visible single player characters - array element %u has name %s", loop, TheText.Get(m_AtArrayOfSinglePlayerCharacters[loop]));
	}
#endif	//	__DEV
}

#endif	//	PHOTO_METADATA_STORES_NEARBY_SP_CHARS



#if PHOTO_METADATA_STORES_NEARBY_WEAPONS

const u32 hashForInvalidWeapon = ATSTRINGHASH("WT_INVALID", 0xbfed8500);

void CSavegamePhotoMetadata::AddPedWeaponToArray(CPed *pPed, s32 &number_of_visible_weapons)
{
	if (number_of_visible_weapons < MAX_NUM_VISIBLE_WEAPONS)
	{
		if (pPed->GetWeaponManager())
		{
			if (pPed->GetWeaponManager()->GetEquippedWeaponObjectHash())
			{
				if (pPed->GetWeaponManager()->GetEquippedWeaponInfo())
				{
					//					m_ArrayOfVisibleWeapons[number_of_visible_weapons] = pPed->GetWeaponManager()->GetEquippedWeaponObjectHash();

					u32 humanNameHash = pPed->GetWeaponManager()->GetEquippedWeaponInfo()->GetHumanNameHash();

					if ( (humanNameHash != hashForInvalidWeapon) && (TheText.DoesTextLabelExist(humanNameHash)) )
					{
						m_AtArrayOfVisibleWeapons.PushAndGrow(humanNameHash, 1);
						photoDisplayf("CSavegamePhotoMetadata::AddPedWeaponToArray - visible equipped weapons - array element %d has name %s", number_of_visible_weapons, TheText.Get(humanNameHash, "weapon in photo"));
						number_of_visible_weapons++;
					}
					else
					{
						photoDisplayf("CSavegamePhotoMetadata::AddPedWeaponToArray - visible equipped weapons - equipped weapon with model hash %u doesn't have a valid entry in the language file", pPed->GetWeaponManager()->GetEquippedWeaponInfo()->GetModelHash());
					}
				}
			}
		}
	}
}

void CSavegamePhotoMetadata::WriteWeaponsToJson(RsonWriter &rsonWriter, bool &bSuccess) const
{
	//	Array of hashes of weapons equipped by visible peds
	if (bSuccess)
	{
		if (m_AtArrayOfVisibleWeapons.GetCount() > 0)
		{
			u32 arrayOfWeaponsToWrite[MAX_NUM_VISIBLE_WEAPONS];
			const u32 numberOfVisibleWeapons = m_AtArrayOfVisibleWeapons.GetCount();

			for (u32 weaponCopyLoop = 0; weaponCopyLoop < numberOfVisibleWeapons; weaponCopyLoop++)
			{
				arrayOfWeaponsToWrite[weaponCopyLoop] = m_AtArrayOfVisibleWeapons[weaponCopyLoop];
			}

			bSuccess = rsonWriter.WriteArray(WEAPON_HASH_ARRAY_KEY, arrayOfWeaponsToWrite, numberOfVisibleWeapons);
		}
	}
}


void CSavegamePhotoMetadata::ReadWeaponsFromJson(RsonReader &rsonReader)
{
	RsonReader valueReader;
	u32 loop = 0;

	//	Array of hashes of weapons equipped by visible peds
	m_AtArrayOfVisibleWeapons.Reset();

	if (rsonReader.GetMember(WEAPON_HASH_ARRAY_KEY, &valueReader))
	{
		u32 arrayOfWeaponsToRead[MAX_NUM_VISIBLE_WEAPONS];
		for (loop = 0; loop < MAX_NUM_VISIBLE_WEAPONS; loop++)
		{
			arrayOfWeaponsToRead[loop] = 0;
		}

		//	AsArray returns the number of entries that have been read
		valueReader.AsArray(arrayOfWeaponsToRead, MAX_NUM_VISIBLE_WEAPONS);

		u32 number_of_visible_weapons_read_from_json = 0;
		for (loop = 0; loop < MAX_NUM_VISIBLE_WEAPONS; loop++)
		{
			if (arrayOfWeaponsToRead[loop] != 0)
			{
				number_of_visible_weapons_read_from_json++;
			}
		}

		if (number_of_visible_weapons_read_from_json > 0)
		{
			m_AtArrayOfVisibleWeapons.Reserve(number_of_visible_weapons_read_from_json);

			for (loop = 0; loop < MAX_NUM_VISIBLE_WEAPONS; loop++)
			{
				if (arrayOfWeaponsToRead[loop] != 0)
				{
					m_AtArrayOfVisibleWeapons.Append() = arrayOfWeaponsToRead[loop];
				}
			}
		}
	}

#if __DEV
	const u32 sizeOfWeaponArray = m_AtArrayOfVisibleWeapons.GetCount();
	for (loop = 0; loop < sizeOfWeaponArray; loop++)
	{
		photoDisplayf("CSavegamePhotoMetadata::ReadWeaponsFromJson - visible equipped weapons - array element %u has name %s", loop, TheText.Get(m_AtArrayOfVisibleWeapons[loop], "weapon in photo"));
	}
#endif	//	__DEV
}

#endif	//	PHOTO_METADATA_STORES_NEARBY_WEAPONS



#if PHOTO_METADATA_STORES_NEARBY_MP_GAMERTAGS
void CSavegamePhotoMetadata::AddPedToGamerTagArray(CPed *pPed, s32 &number_of_visible_players)
{
	if (pPed->IsPlayer())
	{
		//	Or could I get the gamer tag using this?
		//	pPed->GetPlayerInfo()->m_GamerInfo.GetName()
		if (pPed->GetNetworkObject())
		{
			if (pPed->GetNetworkObject()->GetPlayerOwner())
			{
				if (pPed->GetNetworkObject()->GetPlayerOwner()->GetGamerInfo().GetName())
				{
					if (number_of_visible_players < MAX_NUM_VISIBLE_PLAYERS)
					{
						//	If I need to write XUIDs on 360, it looks like I can do this GetGamerInfo().GetGamerHandle().GetXuid(). That will return a u64
						char tempGamerTag[RLROS_MAX_USERID_SIZE];
						pPed->GetNetworkObject()->GetPlayerOwner()->GetGamerInfo().GetGamerHandle().ToUserId(tempGamerTag, sizeof(tempGamerTag));
						atString gamerTag(tempGamerTag);
						m_AtArrayOfGamerTags.PushAndGrow(gamerTag, 1);

						number_of_visible_players++;
					}
				}
			}
		}
	}
}


void CSavegamePhotoMetadata::WriteGamerTagsToJson(RsonWriter &rsonWriter, bool &bSuccess) const
{
	if (bSuccess)
	{
		char arrayOfGamerTags[MAX_NUM_VISIBLE_PLAYERS][RLROS_MAX_USERID_SIZE];

		const u32 sizeOfGamerTagArray = m_AtArrayOfGamerTags.GetCount();
		u32 number_of_visible_players = 0;
		for (u32 gamerTagLoop = 0; gamerTagLoop < sizeOfGamerTagArray; gamerTagLoop++)
		{
			if (m_AtArrayOfGamerTags[gamerTagLoop].GetLength() != 0)
			{
				safecpy(arrayOfGamerTags[number_of_visible_players], m_AtArrayOfGamerTags[gamerTagLoop].c_str(), RLROS_MAX_USERID_SIZE);
				number_of_visible_players++;
			}
		}

		if (number_of_visible_players > 0)
		{
			bSuccess = rsonWriter.WriteArray(GAMERTAG_ARRAY_KEY, arrayOfGamerTags, number_of_visible_players);
		}
	}
}


void CSavegamePhotoMetadata::ReadGamerTagsFromJson(RsonReader &rsonReader)
{
	RsonReader valueReader;
	u32 loop = 0;

	m_AtArrayOfGamerTags.Reset();

	if (rsonReader.GetMember(GAMERTAG_ARRAY_KEY, &valueReader))
	{
		char arrayOfGamerTags[MAX_NUM_VISIBLE_PLAYERS][RLROS_MAX_USERID_SIZE];
		for (loop = 0; loop < MAX_NUM_VISIBLE_PLAYERS; loop++)
		{
			safecpy(arrayOfGamerTags[loop], "", RLROS_MAX_USERID_SIZE);
		}


		//	AsArray returns the number of entries that have been read
		valueReader.AsArray(arrayOfGamerTags, MAX_NUM_VISIBLE_PLAYERS);

		u32 number_of_gamer_tags_read_from_json = 0;
		for (loop = 0; loop < MAX_NUM_VISIBLE_PLAYERS; loop++)
		{
			if (arrayOfGamerTags[loop][0] != '\0')
			{
				number_of_gamer_tags_read_from_json++;
			}
		}

		if (number_of_gamer_tags_read_from_json > 0)
		{
			m_AtArrayOfGamerTags.Reserve(number_of_gamer_tags_read_from_json);

			for (loop = 0; loop < MAX_NUM_VISIBLE_PLAYERS; loop++)
			{
				if (arrayOfGamerTags[loop][0] != '\0')
				{
					m_AtArrayOfGamerTags.Append() = arrayOfGamerTags[loop];
				}
			}
		}
	}

#if __DEV
	const u32 sizeOfGamerTagArray = m_AtArrayOfGamerTags.GetCount();
	for (loop = 0; loop < sizeOfGamerTagArray; loop++)
	{
		photoDisplayf("CSavegamePhotoMetadata::ReadGamerTagsFromJson - gamertags of visible players - array element %u has gamertag %s", loop, m_AtArrayOfGamerTags[loop].c_str());
	}
#endif	//	__DEV
}

#endif	//	PHOTO_METADATA_STORES_NEARBY_MP_GAMERTAGS


#if PHOTO_METADATA_STORES_NEARBY_VEHICLES

void CSavegamePhotoMetadata::AddNearbyVehiclesToArrayOfVisibleVehicles(CPed *pPlayerPed)
{
	s32 number_of_visible_vehicles = 0;

	CEntityScannerIterator vehicleIterator = pPlayerPed->GetPedIntelligence()->GetNearbyVehicles();
	CEntity* pEntity = vehicleIterator.GetFirst();
	while(pEntity && (number_of_visible_vehicles < MAX_NUM_VISIBLE_VEHICLES))
	{
		if (IsEntityVisible(pEntity, false))
		{
			CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(pEntity->GetModelId());
			if (photoVerifyf(pModelInfo, "CSavegamePhotoMetadata::AddNearbyVehiclesToArrayOfVisibleVehicles - couldn't find model for the vehicle's model index"))
			{
				if (pModelInfo->GetModelType() == MI_TYPE_VEHICLE)
				{
					u32 hashOfVehicleName = atStringHash( ((CVehicleModelInfo*) pModelInfo)->GetGameName() );
					m_AtArrayOfVisibleVehicles.PushAndGrow(hashOfVehicleName, 1);

					photoDisplayf("CSavegamePhotoMetadata::AddNearbyVehiclesToArrayOfVisibleVehicles - visible vehicles - array element %d has human name %s", number_of_visible_vehicles, TheText.Get(hashOfVehicleName, "vehicle in photo"));

					number_of_visible_vehicles++;
				}
			}
		}
		pEntity = vehicleIterator.GetNext();
	}
}

void CSavegamePhotoMetadata::WriteVehiclesToJson(RsonWriter &rsonWriter, bool &bSuccess) const
{
	//	Array of hashes of model names of visible vehicles
	if (bSuccess)
	{
		if (m_AtArrayOfVisibleVehicles.GetCount() > 0)
		{
			u32 arrayOfVehiclesToWrite[MAX_NUM_VISIBLE_VEHICLES];
			const u32 numberOfVisibleVehicles = m_AtArrayOfVisibleVehicles.GetCount();

			for (u32 vehicleCopyLoop = 0; vehicleCopyLoop < numberOfVisibleVehicles; vehicleCopyLoop++)
			{
				arrayOfVehiclesToWrite[vehicleCopyLoop] = m_AtArrayOfVisibleVehicles[vehicleCopyLoop];
			}

			bSuccess = rsonWriter.WriteArray(VEHICLE_MODELS_KEY, arrayOfVehiclesToWrite, numberOfVisibleVehicles);
		}
	}
}


void CSavegamePhotoMetadata::ReadVehiclesFromJson(RsonReader &rsonReader)
{
	RsonReader valueReader;
	u32 loop = 0;

	//	Array of hashes of model names of visible vehicles
	m_AtArrayOfVisibleVehicles.Reset();

	if (rsonReader.GetMember(VEHICLE_MODELS_KEY, &valueReader))
	{
		u32 arrayOfVehiclesToRead[MAX_NUM_VISIBLE_VEHICLES];
		for (loop = 0; loop < MAX_NUM_VISIBLE_VEHICLES; loop++)
		{
			arrayOfVehiclesToRead[loop] = 0;
		}

		//	AsArray returns the number of entries that have been read
		valueReader.AsArray(arrayOfVehiclesToRead, MAX_NUM_VISIBLE_VEHICLES);

		u32 number_of_visible_vehicles_read_from_json = 0;
		for (loop = 0; loop < MAX_NUM_VISIBLE_VEHICLES; loop++)
		{
			if (arrayOfVehiclesToRead[loop] != 0)
			{
				number_of_visible_vehicles_read_from_json++;
			}
		}

		if (number_of_visible_vehicles_read_from_json > 0)
		{
			m_AtArrayOfVisibleVehicles.Reserve(number_of_visible_vehicles_read_from_json);

			for (loop = 0; loop < MAX_NUM_VISIBLE_VEHICLES; loop++)
			{
				if (arrayOfVehiclesToRead[loop] != 0)
				{
					m_AtArrayOfVisibleVehicles.Append() = arrayOfVehiclesToRead[loop];
				}
			}
		}
	}

#if __DEV
	const u32 sizeOfVehicleArray = m_AtArrayOfVisibleVehicles.GetCount();
	for (loop = 0; loop < sizeOfVehicleArray; loop++)
	{
		photoDisplayf("CSavegamePhotoMetadata::ReadVehiclesFromJson - visible vehicles - array element %u has human name %s", loop, TheText.Get(m_AtArrayOfVisibleVehicles[loop], "vehicle in photo"));
	}
#endif	//	__DEV
}

#endif	//	PHOTO_METADATA_STORES_NEARBY_VEHICLES

