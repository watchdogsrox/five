/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : MultiplayerGamerTagHud.h
// PURPOSE : manages the Scaleform multiplayer gamer tag hud
// AUTHOR  : Derek Payne
// STARTED : 08/03/2012
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef __MULTIPLAYER_GAMER_TAG_HUD_H__
#define __MULTIPLAYER_GAMER_TAG_HUD_H__

//rage
#include "atl/array.h"
#include "atl/singleton.h"
#include "rline/rl.h"
#include "rline/clan/rlclancommon.h"
#include "system/criticalsection.h"

//game
#include "Frontend/Scaleform/ScaleFormMgr.h"
#include "Frontend/hud_colour.h"
#include "Frontend/MultiplayerGamerTagList.h"
#include "Frontend/NewHud.h"

#include "fwnet/nettypes.h"

#define MAX_FAKE_GAMER_TAG_PLAYERS (20)
#define MAX_PLAYERS_WITH_GAMER_TAGS (MAX_NUM_PHYSICAL_PLAYERS + MAX_FAKE_GAMER_TAG_PLAYERS)

#define MAX_REF_NAME_SIZE (20)
#define MAX_BIG_TEXT_SIZE (10)
#define GAMER_TAG_INVALID_INDEX (-1)
#define DEBUG_GAMER_TAG (-2)

struct sGamerTag
{
	RegdPhy m_utPhysical; // UpdateThreadOnly
	CDblBuf<Vector3> m_playerPos;
	CDblBuf<Vector2> m_screenPos;
	CDblBuf<float> m_scale;
	CDblBuf<bool> m_visible;
	CDblBuf<CNewHud::sHealthInfo> m_healthInfo;

	CNewHud::sHealthInfo m_previousHealthInfo;

	bool m_reinitPositions;
	bool m_reinitPosDueToAlpha;
	bool m_reinitCrewTag;
	bool m_reinitBigText;
	bool m_renderWhenPaused;
	bool m_hiddenDueToBigVehicle;
	bool m_showEvenInBigVehicles;
	bool m_useVehicleHealth;
	BANK_ONLY(bool m_debugCloneTagFromPedForVehicle;)

	bool m_bUsePointHealth;
	int m_iPoints;
	int m_iMaxPoints;

private:
	CDblBuf<bool> m_activeReinit; // Should never be true unless m_isActive is also true.
	bool m_initPending;
	bool m_isActive;
	bool m_needsPositionUpdate; // We won't draw anything until we get a position update from the UT.
	bool m_removeGfxValuePending; // triggered by UT to cleanup the GfxValue, then m_destroyPending is set to true.
	bool m_destroyPending; // set in RT, tells the UT to destroy this gamertag.

public:
	atRangeArray<bool, MAX_MP_TAGS> m_visibleFlag;
	atRangeArray<bool, MAX_MP_TAGS> m_updateAlpha;
	atRangeArray<u8, MAX_MP_TAGS> m_alpha;
	atRangeArray<s32, MAX_MP_TAGS> m_value;
	atRangeArray<eHUD_COLOURS, MAX_MP_TAGS> m_colour;
	eHUD_COLOURS m_healthBarColour;
	float m_gamerTagWidth;

	typedef atFixedBitSet<MAX_MP_TAGS, u32> MPTagBitSet;
	CDblBuf<MPTagBitSet> m_TagReinitFlags;
	CDblBuf<bool> m_healthBarColourDirty;

	char m_playerName[RL_MAX_DISPLAY_NAME_BUF_SIZE];
	char m_crewTag[NetworkClan::FORMATTED_CLAN_TAG_LEN];
	char m_refName[MAX_REF_NAME_SIZE];
	char m_bigtext[MAX_BIG_TEXT_SIZE];
	s16 m_rtDepth; // RenderThreadOnly

	GFxValue m_asGamerTagContainerMc;

	void Reset();
	inline void SetStateInitPending() {m_initPending = true; m_needsPositionUpdate = true;}
	inline void SetStateActive() {m_isActive = true; m_initPending = false;}
	inline void SetStateReinit() { Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE)); m_activeReinit.GetWriteBuf() = true;}
	inline void SetStateRemovePending() { Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE)); m_removeGfxValuePending = true; m_initPending = false; m_isActive = false; m_activeReinit.GetWriteBuf() = false;}
	inline void SetStateDestroyPending() { Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER)); m_destroyPending = true; m_removeGfxValuePending = false; m_initPending = false; m_isActive = false; m_activeReinit.GetRenderBuf() = false;}

	inline void ClearStateReinit() { Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER)); m_activeReinit.GetRenderBuf() = false;}
	inline void ClearNeedsPositionUpdate() {m_needsPositionUpdate = false;}

	inline bool IsStateInitPending() const {return m_initPending;}
	inline bool IsStateActive() const {return m_isActive;}
	inline bool IsStateReinit() const { Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER)); return m_activeReinit.GetReadBuf();}
	inline bool IsStateNeedsPositionUpdate() const {return m_needsPositionUpdate;}
	inline bool IsStateRemovePending() const {return m_removeGfxValuePending;}
	inline bool IsStateDestroyPending() const {return m_destroyPending;}
	inline bool IsStateUndefined() const {return !m_initPending && !m_isActive && !m_removeGfxValuePending && !m_destroyPending;}
	inline bool IsStateShuttingDown() const {return m_removeGfxValuePending || m_destroyPending;}
};


class CMultiplayerGamerTagHud
{
public:
	CMultiplayerGamerTagHud();
	~CMultiplayerGamerTagHud();

#if __BANK
	static void InitWidgets();
	static void CreateBankWidgets();
	static void ShutdownWidgets();
	static void CreateDebugTagForPlayer();
	static void RemoveDebugTagForPlayer();
	static void CreateDebugTagForVehicle();
	static void UpdateDebugTagName();
	static void UpdateDebugTagSelected();
	static void UpdateDebugCrewTag();
	static void UpdateDebugTagVerticalOffset();
	static void UpdateDebugTagUseVehicleHealth();
	static void UpdateDebugTagUsePointHealth();
	static void ToggleDebugTagVisiblityOn();
	static void ToggleDebugTagVisiblityOff();
	static void SetDebugTagAlpha();
	static void SetDebugTagValue();
	static void SetDebugBigText();

	void UpdateDebugFakePassengerTags();
#endif  // __BANK

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);
	static void Update();
	static void Render();
	static void UpdateAtEndOfFrame();
	static bool UT_IsGamerTagActive(s32 iPlayerNum);
	static bool UT_IsGamerTagFree(s32 iPlayerNum);

	static s32 UT_GetLocalPlayerTag();

	bool IsMovieActive() const;

	const sGamerTag* UT_GetActiveGamerTag(s32 iPlayerId);

	void UT_CreatePlayerTag(s32 iPlayerNum, const char *pPlayerName, const char* pFormattedCrewTag);
	s32 UT_CreateFakePlayerTag(CPhysical *pPhys, const char *pPlayerName, const char* pFormattedCrewTag, bool bIgnoreDuplicates = false);
	void UT_RemovePlayerTag(s32 iPlayerNum);
	void UT_SetGamerTagVisibility(s32 iPlayerId, eMP_TAG iTag, bool bSetVisible, bool bEvenInBigVehicles = false);
	void UT_SetGamerTagValueInt(s32 iPlayerId, eMP_TAG iTag, s32 iValue);
	void UT_SetGamerTagValueHudColour(s32 iPlayerId, eMP_TAG iTag, eHUD_COLOURS iHudColour);
	void UT_SetGamerTagHealthBarColour(s32 iPlayerId, eHUD_COLOURS iHudColour);
	void UT_SetGamerTagValueHudAlpha(s32 iPlayerId, eMP_TAG iTag, s32 uAlpha);

	void UT_SetAllGamerTagsVisibility(s32 iPlayerId, bool bSetVisible);
	void UT_SetGamerTagsCanRenderWhenPaused(s32 iPlayerId, bool bCanRenderWhenPaused);
	void UT_SetGamerTagsShouldUseVehicleHealth(s32 iPlayerId, bool bUseVehicleHealth);
	
	void UT_SetGamerTagsShouldUsePointHealth(s32 iPlayerId, bool bUsePointHealth);
	void UT_SetGamerTagsPointHealth(s32 iPlayerId, int iPoints, int iMaxPoints);

	bool UT_IsReinitingGamerNameAndCrewTag(s32 iPlayerId);
	void UT_SetGamerName(s32 iPlayerId, const char *pPlayerName);
	void UT_SetGamerTagCrewDetails(s32 iPlayerId, const char* pFormattedCrewTag);
	void UT_SetGamerTagVerticalOffset(float fVerticalScreenOffset);
	void UT_SetBigText(s32 iPlayerId, const char* text);

	void SetDeletePending() {m_deletePending = true;}
	bool IsDeletePending() {return m_deletePending;}

private:
	void UT_UpdateHelper();
	void RT_RenderHelper();

	void UT_CreatePlayerTagHelper(s32 iPlayerNum, CPhysical *pPhys, const char *pPlayerName, const char* pFormattedCrewTag);

	void UT_CreateRoot();
	void UT_RemoveRoot();

	CPhysical* UT_GetPhysical(int iPlayerId);

	void RT_SetGamerTagValueIntOnGfxValue(s32 iPlayerId, eMP_TAG iTag, s32 iValue, GFxValue& centerIcons, GFxValue& leftIcons);
	void RT_SetGamerTagValueIntOnGfxValue(GFxValue& componentGfxValue, eMP_TAG iTag, s32 iValue);
	void RT_SetGamerTagValueHudColourOnGfxValue(GFxValue& componentGfxValue, s32 iPlayerId, eMP_TAG iTag, eHUD_COLOURS iHudColour);
	void RT_SetGamerTagValueHudAlphaOnGfxValue(GFxValue& componentGfxValue, s32 iPlayerId, eMP_TAG iTag, s32 iAlpha);
	bool RT_GetGamerTagVisibility(s32 iPlayerId, eMP_TAG iTag);
	void RT_GetComponentGfxValue(GFxValue *pComponentGfxValue, s32 iPlayerId, eMP_TAG iTag, GFxValue& centerIcons, GFxValue& leftIcons, bool bHasToExist = true);
	void RT_SetColourToGfxvalue(GFxValue *pDisplayObject, Color32 col);

	void RT_SortGamerTags();
	void RT_SetupGfxValueForGamerTag(int iPlayerId, const char *cGamerInfoFilename);
	void RT_RemoveGfxValueForGamerTag(int iPlayerId);
	void RT_ReinitGamerTag(int iPlayerId, GFxValue& centerIcons, GFxValue& leftIcons);
	void RT_UpdateActiveGamerTag(int iPlayerId, GFxValue& centerIcons, GFxValue& leftIcons);
	void RT_ReinitGamerTagCrewDetails(s32 iPlayerId);

	void RT_UpdateHealthArmourHelper(GFxValue& parent, int percentageValue, int capacity, const char* pBarContainerName, const char* pBarName);
	void RT_UpdateHealthArmourVisibilityHelper(GFxValue& parent, const char* pBar, bool isVisible);
	void RT_UpdateHealthColorHelper(GFxValue& parent, const char* pBar, eHUD_COLOURS healthBarColour);

	bool RT_InitAndVerifyAllComponentGfxValues(s32 iPlayerId, GFxValue& centerIcons, GFxValue& leftIcons);
	void RT_InitComponentGfxValue(s32 iPlayerId, eMP_TAG iTag, GFxValue& centerIcons, GFxValue& leftIcons);

	float GetGameWorldOffset(bool bHiddenDueToBigVehicle, bool bIsPedOnBike);
	Vector2 GetScreenOffset(bool bHiddenDueToBigVehicle, int seatIndex, bool bIsPedOnBike, float fExtraVehicleSpacing);

	static float sm_fVerticalOffset;
	static bank_float sm_iconSpacing;
	static bank_float sm_afterGamerNameSpacing;
	static bank_float sm_aboveGamerNameSpacing;
	static bank_float sm_minScaleSize;
	static bank_float sm_maxScaleSize;
	static bank_float sm_nearScalingDistance;
	static bank_float sm_farScalingDistance;
	static bank_float sm_maxVisibleDistance;
	static bank_float sm_maxVisibleSpectatingDistance;
	static bank_float sm_leftIconXOffset;
	static bank_float sm_4x3Scaler;
	static bank_float sm_bikeOffset;
	static bank_float sm_defaultVehicleOffset;
	static bank_float sm_vehicleXSpacing;
	static bank_float sm_vehicleYSpacing;
	static bank_s32 sm_minSeatCountToGridNames;

	static bool sm_staticDataInited;
	static float sm_tagWidths[MAX_MP_TAGS];
	static eMP_TAG sm_tagOrder[MAX_MP_TAGS];

	bool m_rootCreated;
	bool m_deletePending;

	CScaleformMovieWrapper m_movie;

	GFxValue m_asRootContainer;
	GFxValue m_asGamerTagLayerContainer;

	atRangeArray<sGamerTag, MAX_PLAYERS_WITH_GAMER_TAGS> m_gamerTags;
	sysCriticalSectionToken m_renderingSection;

#if !__NO_OUTPUT
	atRangeArray<const char*, MAX_MP_TAGS> m_GamerTagNames;
#endif
	atRangeArray<const char*, MAX_MP_TAGS> m_GamerTagASNames;
	atRangeArray<eTAG_FLAGS, MAX_MP_TAGS> m_rtTagFlags;
};

typedef atSingleton<CMultiplayerGamerTagHud> SMultiplayerGamerTagHud;

#endif // #ifndef __MULTIPLAYER_GAMER_TAG_HUD_H__

// eof
