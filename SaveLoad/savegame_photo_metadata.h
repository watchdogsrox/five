
#ifndef SAVEGAME_PHOTO_METADATA_H
#define SAVEGAME_PHOTO_METADATA_H

// Rage headers
#include "atl/string.h"
#include "data/rson.h"
#include "rline/rl.h"
#include "rline/clan/rlclancommon.h"
#include "spatialdata/aabb.h"
#include "Vector/Vector3.h"

// Game headers
#include "game/Clock.h"
#include "SaveLoad/savegame_photo_unique_id.h"

#define PHOTO_METADATA_STORES_NEARBY_VEHICLES		(0)
#define PHOTO_METADATA_STORES_NEARBY_WEAPONS		(0)
#define PHOTO_METADATA_STORES_NEARBY_SP_CHARS		(0)
#define PHOTO_METADATA_STORES_NEARBY_MP_GAMERTAGS	(1)

#define PHOTO_METADATA_STORES_SELFIE_FLAG			(1)
#define PHOTO_METADATA_STORES_WIDESCREEN_FLAG		(0)

// Forward declarations
class CEntity;
class CPed;


class CSavegamePhotoMetadata
{
//	static const u32 ms_MAX_DISPLAY_NAME_CHARS = 256;

public:
	void FillContents();
	void GenerateNewUniqueId();
	void SetImageSignature(u64 signature);
	u64 GetImageSignature() const {	return m_ImageSignature; }

	bool WriteMetadataToString(char *pStringToFill, u32 maxLengthOfString, bool bDebugLessData) const;

	bool ReadMetadataFromString(const char *pString);

	void GetPhotoLocation(Vector3& vLocation) const;

	const char *GetSongTitle() const;
	const char *GetSongArtist() const;

	const CSavegamePhotoUniqueId &GetUniqueId() const { return m_UniqueId; }

	u32 GetPosixCreationTime() const { return m_PosixCreationTime; }

	inline void FlagAsMemeEdited( bool isMeme ) { m_containsMemeText = isMeme; }
	inline bool IsMemeEdited() const { return m_containsMemeText; }

	inline void SetIsMugshot(bool bIsMugshot) { m_bMugshot = bIsMugshot; }
	inline bool GetIsMugshot() const { return m_bMugshot; }

	inline void SetTakenInRockstarEditor(bool bTakenInRockstarEditor) { m_bTakenInRockstarEditor = bTakenInRockstarEditor; }

	void SetArenaThemeAndVariation(u32 arenaTheme, u32 arenaVariation);
	void SetOnIslandX(bool bOnIslandX);

	bool GetAkamai() const;
	void SetAkamai(const bool akamai);

	static bool IsAkamaiEnabled();
	// If true we fix the m_Akamai bool if it doesn't match with the current IsAkamaiEnabled state
	static bool ShouldCorrectAkamaiOnLocalLoad();
	static bool ShouldCorrectAkamaiOnUpload();

#if __BANK
	static void UpdateDebug();

	//	Add the lengths of all atStrings to the sizeof(CSavegamePhotoMetadata)
	u32 GetSize() const;

	static bool sm_bDisplayBoundingRectanglesForPeds;

	static bool sm_bSaveLocation;
	static bool sm_bSaveArea;
	static bool sm_bSaveStreet;
	static bool sm_bSaveMissionName;
	static bool sm_bSaveSong;
	static bool sm_bSaveRadioStation;
	static bool sm_bSaveMpSessionID;
	static bool sm_bSaveClanID;
	static bool sm_bSaveUgcID;
	static bool sm_bSaveTheGameMode;
	static bool sm_bSaveTheGameTime;
	static bool sm_bSaveTheMemeFlag;
	static bool sm_bSaveTheUniqueId;
	static bool sm_bSaveThePosixCreationTime;
	static bool sm_bSaveArenaThemeAndVariation;
	static bool sm_bSaveOnIslandX;
#endif	//	__BANK


private:

	static void GetNormalisedScreenCoordFromWorldCoord(const Vector3 &vecWorldPos, float &NormalisedX, float &NormalisedY);
	static void UpdateLimitsOfRectangle(const Vector3 &vecWorldPos, float &MinX, float &MinY, float &MaxX, float &MaxY);
	static float GetScreenAreaOfBoundingBox(const spdAABB &aabb, bool bDisplayBoundingRectangle);

	static bool IsEntityVisible(CEntity *pEntity, bool bDisplayDebug);

#if PHOTO_METADATA_STORES_NEARBY_WEAPONS
	void AddPedWeaponToArray(CPed *pPed, s32 &number_of_visible_weapons);
	void WriteWeaponsToJson(RsonWriter &rsonWriter, bool &bSuccess) const;
	void ReadWeaponsFromJson(RsonReader &rsonReader);
#endif	//	PHOTO_METADATA_STORES_NEARBY_WEAPONS

#if PHOTO_METADATA_STORES_NEARBY_VEHICLES
	void AddNearbyVehiclesToArrayOfVisibleVehicles(CPed *pPlayerPed);

	void WriteVehiclesToJson(RsonWriter &rsonWriter, bool &bSuccess) const;
	void ReadVehiclesFromJson(RsonReader &rsonReader);
#endif	//	PHOTO_METADATA_STORES_NEARBY_VEHICLES

#if PHOTO_METADATA_STORES_NEARBY_MP_GAMERTAGS
	void AddPedToGamerTagArray(CPed *pPed, s32 &number_of_visible_players);

	void WriteGamerTagsToJson(RsonWriter &rsonWriter, bool &bSuccess) const;
	void ReadGamerTagsFromJson(RsonReader &rsonReader);
#endif	//	PHOTO_METADATA_STORES_NEARBY_MP_GAMERTAGS

#if PHOTO_METADATA_STORES_NEARBY_SP_CHARS
	const char *GetTextLabelForPedModel(const CPed *pPed);
	void AddPedToSinglePlayerCharacterArray(CPed *pPed, s32 &number_of_single_player_characters);

	void WriteSinglePlayerCharacterTextLabelsToJson(RsonWriter &rsonWriter, bool &bSuccess) const;
	void ReadSinglePlayerCharacterTextLabelsFromJson(RsonReader &rsonReader);
#endif	//	PHOTO_METADATA_STORES_NEARBY_SP_CHARS

	void ReadStringFromMetadata(RsonReader &rsonReader, RsonReader &valueReader, const char *pMemberName, atString &stringToFill);

	const char *GetStringFromGameMode( u8 gameMode) const;
	u8 GetGameModeFromString(const char *pGameModeString) const;

private:
	u64 m_MpSessionID;
	u32 m_PosixCreationTime;

	rlClanId m_PrimaryClanIdOfPlayer;

#if PHOTO_METADATA_STORES_NEARBY_VEHICLES
	atArray<u32> m_AtArrayOfVisibleVehicles;
#endif	//	PHOTO_METADATA_STORES_NEARBY_VEHICLES

#if PHOTO_METADATA_STORES_NEARBY_WEAPONS
	atArray<u32> m_AtArrayOfVisibleWeapons;
#endif	//	PHOTO_METADATA_STORES_NEARBY_WEAPONS

#if PHOTO_METADATA_STORES_NEARBY_MP_GAMERTAGS
	atArray<atString> m_AtArrayOfGamerTags;
#endif	//	PHOTO_METADATA_STORES_NEARBY_MP_GAMERTAGS

#if PHOTO_METADATA_STORES_NEARBY_SP_CHARS
	atArray<atString> m_AtArrayOfSinglePlayerCharacters;
#endif	//	PHOTO_METADATA_STORES_NEARBY_SP_CHARS

	atString m_AreaNameTextKey;
	u32 m_HashOfStreetNameTextKey;

	atString m_SinglePlayerMissionName;

	atString m_RadioStationName;

	atString m_UgcID;

	float m_fPlayerX;
	float m_fPlayerY;
	float m_fPlayerZ;

	static const u32 InvalidSongId = 1;	//	audRadioAudioEntity::GetAudibleTrackTextId() returns 1 for kUnknownTrackTextId
	u32 m_SongId;

	CSavegamePhotoUniqueId m_UniqueId;

//	Game Time
	int m_GameTimeHour;				// hours since midnight - [0,23]
	int m_GameTimeMinute;			// minutes after the hour - [0,59]
	int m_GameTimeSecond;			// seconds after the minute - [0,59]

	int m_GameTimeDayOfMonth;		// day of the month - [1,31]
	int m_GameTimeMonth; 			// months since January - [1,12]		I add 1 to the value returned by CClock::GetMonth()
	int m_GameTimeYear;				// year

	u8 m_GameMode;	//	Graeme - I'll update this later, when I can differentiate between the various MP modes

	u32 m_ArenaTheme;
	u32 m_ArenaVariation;

	bool m_containsMemeText;	// Whether the photo was generated through the meme editor

	bool m_bMugshot;			//	True for photos taken in James Adwick's multiplayer character select screen

#if PHOTO_METADATA_STORES_WIDESCREEN_FLAG
	bool m_bIsWidescreen;
#endif	//	PHOTO_METADATA_STORES_WIDESCREEN_FLAG

#if PHOTO_METADATA_STORES_SELFIE_FLAG
	bool m_bIsSelfie;
#endif	//	PHOTO_METADATA_STORES_SELFIE_FLAG

	bool m_bIsInDirectorMode;
	bool m_bTakenInRockstarEditor;

	bool m_bOnIslandX;
	bool m_Akamai;

	u64 m_ImageSignature;
};

#endif	//	SAVEGAME_PHOTO_METADATA_H

