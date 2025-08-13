
#ifndef __MINIMAP_RENDER_THREAD_H__
#define __MINIMAP_RENDER_THREAD_H__

// rage:
#include "atl/array.h"
#include "rmcore/drawable.h"
#include "vector/color32.h"
#include "vector/vector2.h"
#include "vector/vector3.h"

// game:
#include "Frontend/MiniMap.h"
#include "Frontend/MiniMapCommon.h"
#include "system/TamperActions.h"	//	for GPS_NUM_SLOTS
#include "vehicleAi/pathfind.h"	//	for GPS_NUM_SLOTS


#define __STREAMED_SUPERTILE_FILES	(1)


//	It doesn't look like this is currently being called
//	I'm not sure how we could iterate over peds on the render thread
#define __RENDER_PED_VISUAL_FIELD	(0)

// Forward declarations
struct sSonarBlipStructRT;
//	class CMiniMapBlip;
//	class CNodeAddress;	//	conflicts with pathfind.h
class GFxValue;


#if __STREAMED_SUPERTILE_FILES



class CSupertile
{
public:
	void Init(u32 Row, u32 Column);
	void Shutdown(unsigned shutdownMode);

	void Update(bool bShouldBeLoaded, GFxValue &TileLayerContainer);
	void ProcessAtEndOfFrame(s32 iParentMovieId);

	void HandleIsLoadedCallback(const GFxValue &gfxVal);


	void Render(int pass);
#if ENABLE_FOG_OF_WAR 
	void RenderFoW();
	static void RenderIslandFoW();
#endif // ENABLE_FOG_OF_WAR	

private:
// Private functions
	void Remove();

// Private data
	u32 m_SupertileRow;
	u32 m_SupertileColumn;
	bool m_bFullyLoaded;

	strRequest m_dwdRequest;
	atRangeArray<atRangeArray<pgRef<rmcDrawable>, WIDTH_OF_DWD_SUPERTILE>, HEIGHT_OF_DWD_SUPERTILE> m_Foregrounds;
	pgRef<rmcDrawable> m_Background;
	pgRef<rmcDrawable> m_Sea;

	bool m_bStreamThisSupertile;
	bool m_bRemoveThisSupertile;
	bool m_bPreviousValueOfShouldBeLoaded;
};

class CSupertiles
{
public:
	void Init(unsigned initMode);
	void Shutdown(unsigned shutdownMode);

	bool Update(const Vector2 &vCurrentTile, const Vector2 &vTilesAroundPlayer, GFxValue &TileLayerContainer, bool bBitmapOnly);
	void ProcessAtEndOfFrame(s32 iParentMovieId);

	void HandleIsLoadedCallback(s32 Row, s32 Column, const GFxValue &gfxVal);

	static spdRect GetMiniTileWorldSpaceBounds(s32 superTileRow, s32 superTileCol, s32 miniTileRow, s32 miniTileColumn);
	static spdRect GetSuperTileWorldSpaceBounds(s32 row, s32 col);

	void Render();
#if ENABLE_FOG_OF_WAR
	void RenderFoW();
#endif // ENABLE_FOG_OF_WAR
	
private:
	CSupertile m_Supertiles[NUMBER_OF_SUPERTILE_ROWS][NUMBER_OF_SUPERTILE_COLUMNS];

	s32 m_PreviousTopTile;
	s32 m_PreviousBottomTile;
	s32 m_PreviousLeftTile;
	s32 m_PreviousRightTile;
};
#endif	//	__STREAMED_SUPERTILE_FILES

struct sConesAttachedToBlips
{
	s32 m_iActualIdOfBlip;
	bool m_bConeShouldBeRemovedFromStage;
};

enum
{
	MINIMAP_ROOT_MAP_TRANSPARENT = 0,
	MINIMAP_ROOT_LAYER_MASKED,
	MINIMAP_ROOT_LAYER_UNMASKED,
	MAX_MINIMAP_ROOT_LAYERS
};

enum
{
	MINIMAP_LAYER_BACKGROUND = 0,
	MINIMAP_LAYER_FOREGROUND,
	MAX_MINIMAP_LAYERS
};

class CMiniMap_RenderThread
{
public:
	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

	static void SetupContainers();
	static void RemoveContainers();

	static void SetMovieId(s32 iLayer, s32 movieId) { ms_iMovieId[iLayer] = movieId; }

	static void SetMiniMapRenderState(const sMiniMapRenderStateStruct &newRenderState);

	static void RenderPauseMap(Vector2 vBackgroundPos, Vector2 vBackgroundSize, float fAlphaFader, float fBlipFader, float fSizeScalar);
	static void Render();

	static void AddSonarBlipToStage(sSonarBlipStructRT *pSonarBlip);

#if ENABLE_FOG_OF_WAR
	static void FOWRenderQuadLocation(Vec4f *quads, int numQuads);
	static void FOWClear();
	static void FOWReveal();
	static void FOWDownSample();

	static void RenderToFoW(CBlipComplex *pBlip);

#if __D3D11
	static void CopyFoWForCPURead();
	static void ReadBackFoWFromStaging();
	static void UploadFoWTextureData(WIN32PC_ONLY(bool bForce = false));
#if RSG_PC
	static bool IsRenderMultiGPU();
	static void ResetMultiGPUCount()	{ ms_iMultiGPUCount = GRCDEVICE.GetGPUCount(true); }
#endif
#endif // __D3D11
#endif // ENABLE_FOG_OF_WAR	
	static bool UpdateIndividualBlip(CBlipComplex *pBlip, s32 iMapRotation, bool bRangeCheckOnly);

	static void RenderNavPath(s32 iRouteNum, s32 iNumPoints, Vector3 *vPoints, rage::Color32 colour, float fOffsetX, float fOffsetY, bool bClipFirstLine, float fClipX, float fClipY, float fRoadWidthMiniMap, float fRoadWidthPauseMap);

	static void ClearNavPath(s32 iRouteNum);

	static bool CanDisplayGpsLine(bool bHideWhenZoomed);

	static void CheckIncomingFunctions(atHashWithStringBank methodName, const GFxValue* args);

	static bool DoesRenderedBlipExist(s32 BlipIndex);

	static bool RemoveBlipFromStage(CBlipComplex *pBlip, bool bFromUpdateThreadInShutdownSession);

	static bool GetIsInsideInterior();

	static bool IsInteriorSetupFromScript() { return ms_bInteriorWasSetOnPerFrame; }
	static bool IsInteriorMovieActiveAndSetupOnStage() { return ms_bInteriorMovieIdFullySetup; }
	static bool IsInteriorMovieActive() { return ms_iInteriorMovieId != -1; }

	static void ProcessAtEndOfFrame();

	static bool HasRenderThreadProcessedThisMinimapModeState(eSETUP_STATE StateToCheck);
	static bool IsInPauseMap() { return ms_MiniMapRenderState.m_bIsInPauseMap; }
	static bool IsInGameMiniMapDisplaying();

#if TAMPERACTIONS_ENABLED && TAMPERACTIONS_ROTATEMINIMAP
	// Utilised for the tamper action of rotations; not needed otherwise. 
    static s32 GetMiniMapRenderRotation() { return ms_MiniMapRenderState.m_iRotation;}
#endif
	static Vector2 GetMiniMapRenderPos() { return ms_MiniMapRenderState.m_vCurrentMiniMapPosition; }
	static Vector2 GetMiniMapRenderSize() { return ms_MiniMapRenderState.m_vCurrentMiniMapPosition; }

	static void PrepareForRendering();

	static void ResetBlipConeFlags();
	static void RemoveUnusedBlipConesFromStage();
	static bool AddOrUpdateConeToBlipOnStage(CBlipCone &blipCone);

	static void UpdateVisualDataSettings();
	static void SetMask(bool bSet);

	static void SetupHealthArmourOnStage(bool bBigMapProperties);
	static void UpdateMapVisibility();

	static bool ShouldRenderMiniMap() { return ms_MiniMapRenderState.m_bShouldRenderMiniMap; }

	static const Vector2& GetOriginalStageSize() { return ms_vOriginalStageSize; }
	static const GMatrix3D& GetLocalToScreenMatrix_UT() { return ms_LocalToScreen.GetUpdateBuf(); }

	static void RenderBitmaps();
#if ENABLE_FOG_OF_WAR
	static inline Vector2 WorldToFowCoord(float x, float y)
	{
		Vector2 result;

		if(CMiniMap::AreFowCustomWorldExtentsEnabled())
		{
			result.x = Saturate( (x - CMiniMap::GetFowCustomWorldX())/(CMiniMap::GetFowCustomWorldW()));
			result.y = Saturate( (-y - CMiniMap::GetFowCustomWorldY())/(CMiniMap::GetFowCustomWorldH()));
		}
		else
		{
			result.x = Saturate( (x - CMiniMap::sm_Tunables.FogOfWar.fWorldX)/(CMiniMap::sm_Tunables.FogOfWar.fWorldW));
			result.y = Saturate( (-y - CMiniMap::sm_Tunables.FogOfWar.fWorldY)/(CMiniMap::sm_Tunables.FogOfWar.fWorldH));
		}

		return result;
	}

	static inline Vector2 FowToWorldCoord(float x, float y)
	{
		Vector2 result;

		if(CMiniMap::AreFowCustomWorldExtentsEnabled())
		{
			result.x = x * (CMiniMap::GetFowCustomWorldW()) + CMiniMap::GetFowCustomWorldX();
			result.y = -y * (CMiniMap::GetFowCustomWorldH()) - CMiniMap::GetFowCustomWorldY();
		}
		else
		{
			result.x = x * (CMiniMap::sm_Tunables.FogOfWar.fWorldW) + CMiniMap::sm_Tunables.FogOfWar.fWorldX;
			result.y = -y * (CMiniMap::sm_Tunables.FogOfWar.fWorldH) - CMiniMap::sm_Tunables.FogOfWar.fWorldY;
		}

		return result;
	}

#endif // ENABLE_FOG_OF_WAR

	static bank_bool ms_bUseTexturedAlphaAllMovies;
	static bank_bool ms_bUseTextureAlphaBaseMovie;

#if __BANK
	static bool sm_bDebugMaskDisplay;
	static u8 sm_uDebugMaskAlpha;

#if DEBUG_DRAW
	static void RenderDebugVolumesOnUT();
	static bool sm_bDrawCollisionVolumes;	
#endif
#endif
	static BankFloat sm_fMultiheadEdgeFeatherPct;

private:

	static void SetupMiniMapProperties(bool bBigMapProperties);

	static void SetMaskOnLayer(bool bSet, s32 iLayer);
	static void SetLocalStageSize();
	static void CreateRoot(s32 iId);
	static void RemoveRoot(s32 iId);
	static void CreateTerritoryContainer();
	static void RemoveTerritoryContainer();
	static void CreateHealthArmourContainer();
	static void RemoveHealthArmourContainer();
	static void CreateMapContainers();
	static void RemoveMapContainers();
	static void CreateAltimeter();
	static void RemoveAltimeter();
	//	static void CreateRunway();
	static void CreateFreeway();
	static void RemoveFreeway();
	static void UpdateFreeway();
	static void CreateCrosshair();

	static bool IsAreaBlipOutsideMapBounds(const CBlipComplex& rBlip);

	static void SetupGolfCourseLayout();

	static void UpdateGPSDisplay();
	static void UpdateWithActionScriptOnRT(bool bTransition, bool bClearTiles);

	static void UpdateStatesWithActionScriptOnRT();

	static void HandleInteriorAtEndOfFrame();
	static void HandleGolfCourseAtEndOfFrame();

//	static float GetRange(bool bGetTrueRange = false);

	static void ClearMatrix();

	static void RenderWantedOverlay();

	static bool UpdateMapPositionAndScale(bool bForceInstantRangeChange = false);

	//static bool IsFlagSet(CBlipComplex *pBlip, const s32 iQueryFlag);
	static Vector3 GetBlipPositionValue(const CBlipComplex *pBlip);
	static Vector3 GetBlipPositionValueOnStage(const CBlipComplex *pBlip);
	static const Vector3& GetBlipPositionConstRef(const CBlipComplex *pBlip);
	static const char* GetBlipObjectName(const CBlipComplex *pBlip)  { return CMiniMap_Common::GetBlipName(pBlip->iLinkageId); }
	static const BlipLinkage& GetBlipObjectNameId(const CBlipComplex *pBlip) { return pBlip->iLinkageId; }
	static eBLIP_PRIORITY GetBlipPriorityValue(const CBlipComplex *pBlip) { return (eBLIP_PRIORITY)pBlip->priority; }
	static inline bool GetBlipOn3dMap(const CBlipComplex *pBlip) { return pBlip->bBlipIsOn3dMap; }
	static const char* GetBlipNameValue(const CBlipComplex *pBlip);
	static eBLIP_COLOURS GetBlipColourValue(const CBlipComplex *pBlip) { return (eBLIP_COLOURS)pBlip->iColour; }
	static s32 GetBlipAlphaValue(const CBlipComplex *pBlip) {	return pBlip->iAlpha; }
	static Color32 GetBlipSecondaryColourValue(const CBlipComplex *pBlip) { return pBlip->iColourSecondary; }
	static eBLIP_TYPE GetBlipTypeValue(const CBlipComplex *pBlip) { return (eBLIP_TYPE)pBlip->type; }
	static inline u16 GetBlipFlashInterval(const CBlipComplex *pBlip) { return pBlip->iFlashInterval; }
	static GFxValue* GetBlipGfxValue(const CBlipComplex *pBlip);
	static eBLIP_DISPLAY_TYPE GetBlipDisplayValue(const CBlipComplex *pBlip) { return (eBLIP_DISPLAY_TYPE)pBlip->display; }
	static float GetBlipDirectionValue(const CBlipComplex *pBlip) { return pBlip->fDirection; }
	static inline const Vector2& GetBlipScaleValue(const CBlipComplex *pBlip) { return pBlip->vScale; }
	static float GetBlipScalerValue(const CBlipComplex *pBlip, bool bCareAboutType = true);
	static u8 GetBlipCategoryValue(const CBlipComplex *pBlip) { return pBlip->iCategory; }

	static void SetBlipGfxValue(const CBlipComplex *pBlip, GFxValue *pParam);

	static bool GetIsPlacedOn3dMap(const CBlipComplex* pBlip);
	static void SetIsPlacedOn3dMap(const CBlipComplex* pBlip, bool bOnMap);

//	static void SetFlag(CBlipComplex *pBlip, const int flag);
//	static void UnsetFlag(CBlipComplex *pBlip, const int flag);

	static bool ShouldBlipHeightIndicatorFlash(CBlipComplex *pBlip);
	static s32 CheckForHeight(CBlipComplex *pBlip);

	static void GetMiniMapCoordinatesForCone(float inputX, float inputY, float fMapRange, float &outputX, float &outputY);
	static bool RemoveConeFromBlipOnStage(s32 iActualIdOfBlip);
	static void DrawConeForBlip(GFxValue &coneValue, CBlipCone &blipCone);

	static bool ShouldBlipUseWeaponTypeArrows(CBlipComplex const *pBlip);
	static bool ShouldBlipUseAiTypeArrows(CBlipComplex const *pBlip);
	static bool AddHigherLowerBlip(GFxValue *pBlipGfx, CBlipComplex *pBlip, s32 iHighLowBlipId);
	static bool RemoveHigherLowerBlip(GFxValue *pBlipGfx, int iHigherOrLower = 0);

	static bool AddBlipToStage(CBlipComplex *pBlip);

	static s32 DetermineBlipStageDepth(CBlipComplex* pBlip);

#if __BANK
	static void LabelBlipOnStage(GFxValue *pBlipGfx, CBlipComplex *pBlip, bool bOnOff);
#endif // __BANK
	
	static void NumberBlipOnStage(GFxValue *pBlipGfx, CBlipComplex *pBlip);

	static void PulseBlipOnStage(GFxValue *pBlipGfx, CBlipComplex *pBlip);

	static void UpdateTickBlipOnStage(GFxValue *pBlipGfx, CBlipComplex *pBlip);
	static void UpdateGoldTickBlipOnStage(GFxValue *pBlipGfx, CBlipComplex *pBlip);
	static void UpdateForSaleBlipOnStage(GFxValue *pBlipGfx, CBlipComplex *pBlip);
	static void UpdateBlipSecondaryIconOnStage(GFxValue *pBlipGfx, CBlipComplex *pBlip, eBLIP_PROPERTY_FLAGS iconFlag, const char* symbolName, const char* instanceName, int displayDepth);

	static void UpdateHeadingIndicatorOnBlipOnStage(GFxValue *pBlipGfx, CBlipComplex *pBlip, s32 iMapRotation);
	static void UpdateOutlineIndicatorOnBlipOnStage(GFxValue *pBlipGfx, CBlipComplex *pBlip);
	static void UpdateFriendIndicatorOnBlipOnStage(GFxValue *pBlipGfx, CBlipComplex *pBlip);
	static void UpdateCrewIndicatorOnBlipOnStage(GFxValue *pBlipGfx, CBlipComplex *pBlip);

	static void FadeOutDistantBlipOnStage(GFxValue *pBlipGfx, s32 iDisplayAlpha);
	static void ColourBlipOnStage(GFxValue *pBlipGfx, CBlipComplex *pBlip);

	static bool FlashBlipOnStage(GFxValue *pBlipGfx, CBlipComplex *pBlip);

	static void ReInitBlipOnStage(CBlipComplex *pBlip);

	static void UpdateAltimeter();

	static void UpdateSonarSweep();

	static void RemoveInterior();
	static void UpdateInterior();
	static void ProcessPlayerNameDisplayOnBlip(GFxValue *pBlipGfx, CBlipComplex *pBlip, bool bOnEdgeOfRadar);
	static bool IsBlipNameTagWithinBounds(GFxValue *pBlipGfx, CBlipComplex *pBlip);

	static void UpdateTiles(bool bClearAll);

	static void RemoveGolfCourse(bool bFromUpdateThreadInShutdownSession);
	static void UpdateSea(bool bOn);
	static void UpdateGolfCourse();

	static void RenderMiniMap(s32 iId, Vector2 vBackgroundPos, Vector2 vBackgroundSize, float fAlpha, float fSizeScalar);

	static void AdjustVisualStageSpaceWithinMovie(float fAmount);

	static void SetupForBigMap();

	static void SetupForPauseMenu(bool bFullScreen);

	static void SetupForMiniMap();

	static void SetupForGalleryMap();

	static void SetupStateForSupertileRender(Vector2 vBackgroundPos, Vector2 vBackgroundSize);

#if __RENDER_PED_VISUAL_FIELD
	static void RenderPedVisualField();
	static bool ConvertToScreenCoords(Vector2& screenCoordsOut, const Vector3& vWorldCoords);
	static bool ClampInsideLimits(Vector2 &vFrom, Vector2 &vTo);
	static void	DrawLine( const Vector3 &vFrom, const Vector3 &vTo, const Color32 colour );
	static bool	DrawPoly( const Vector3 &p1, const Vector3 &p2, const Vector3 &p3, const Color32 colour );
	static void	DrawPedVisualField(CPed* pPed);
	static void	DrawDeadPed(CPed* pPed);
#endif	//	__RENDER_PED_VISUAL_FIELD

#if __DEV
	static bool VerifyComponents();
#endif

#if SUPPORT_MULTI_MONITOR
	static void DrawFeatheredRect(Vector2 vBottomLeft, Vector2 vTopRight, Color32 middleColor);
#endif


// Private variables
	static atArray<sConesAttachedToBlips> ms_ConesAttachedToBlips;

	static sMiniMapRenderStateStruct ms_MiniMapRenderState;

	//	Blend States used when rendering the minimap
	static grcBlendStateHandle ms_StandardMiniMapRenderBlendStateHandle;
	static grcBlendStateHandle ms_ClearAlphaMiniMapRenderBlendStateHandle;
	static grcBlendStateHandle ms_CopyAAMiniMapRenderBlendStateHandle;
	static float ms_MiniMapAlpha;

	static grcSamplerStateHandle ms_MinimapBitmapSamplerStateHandle;
	static grcRasterizerStateHandle ms_MinimapBitmapRasterizerStateHandle;
#if __DEV
	static grcSamplerStateHandle ms_MinimapBitmapSamplerStateNearestHandle;
#endif
#if ENABLE_FOG_OF_WAR 
	static grcBlendStateHandle ms_MinimapFoWBlendStateHandle;
#if __D3D11
	static bool ms_bUpdateFogOfWarData;
	static bool ms_bUploadFOWTextureData;
#endif
#endif // ENABLE_FOG_OF_WAR
	
	static bool AreAllMiniMapMovieLayersActive() { return (ms_iMovieId[MINIMAP_MOVIE_BACKGROUND] != -1 && ms_iMovieId[MINIMAP_MOVIE_FOREGROUND] != -1); }

	static s32 ms_iMovieId[MAX_MINIMAP_MOVIE_LAYERS];

	static Vector2 ms_vOriginalStageSize;
	static Vector2 ms_vStageSize;

	static CDblBuf<GMatrix3D> ms_LocalToScreen;

	static GFxValue asBitmap;

	//
	// Global GFXVALUES - these are global since they are updated every frame or changed often.  If they are not then we could do
	// a GetMember() instead of storing the gfxvalue, which we should try and do if possible
	//
	static atFixedBitSet<MAX_NUM_BLIPS> bBlipIsOn3dLayer;
	static GFxValue *pRenderedBlipObject[MAX_NUM_BLIPS];
	static GFxValue ms_asRootContainer[MAX_MINIMAP_ROOT_LAYERS];
	static GFxValue ms_asBaseLayerContainer[MAX_MINIMAP_LAYERS];
	static GFxValue ms_asBaseOverlay3D[MAX_MINIMAP_LAYERS];

	struct AltimeterDisplay
	{
		GFxValue m_asContainerMc;
		GFxValue m_asGaugeMc;
		GFxValue m_asRollMc;
	};
	static AltimeterDisplay ms_Altimeter;
	static GFxValue ms_asMapContainerMc;
	static GFxValue ms_asBlipContainerMc;
	static GFxValue ms_asGpsLayer[GPS_NUM_SLOTS];
	static GFxValue ms_asMapObject;
	static GFxValue ms_asBlipLayer3D;
	static GFxValue ms_asInteriorMc;
	static GFxValue ms_asInteriorMovie;
	static GFxValue ms_asGolfCourseMc;
	static GFxValue ms_asGolfCourseMovie;
	static GFxValue ms_asGolfCourseHole;
	static GFxValue ms_asTileLayerContainer;
	static GFxValue ms_asBitmapLayerContainer;
	static GFxValue ms_asFreewayMovieClip;

	static float ms_fBlipMapRange;
	static float ms_fBlipMapAngle;


#if __STREAMED_SUPERTILE_FILES
	static CSupertiles ms_Supertiles;
#endif	//	__STREAMED_SUPERTILE_FILES

	static sMiniMapInterior ms_PreviousInterior;

	static s32 ms_iPreviousGolfCourse;

	static bool ms_bInteriorMovieIdFullySetup;
	static bool ms_bInteriorWasSetOnPerFrame;
	static s32 ms_iInteriorMovieId;
	static s32 ms_iGolfCourseMovieId;

	static bool ms_bRunwayBlipsAreDisplaying;

	static bool ms_bRemoveInteriorMovie;
	static bool ms_bStreamInteriorMovie;

	static bool ms_bRemoveGolfCourseMovie;
	static bool ms_bStreamGolfCourseMovie;

	static grcTexture *ms_MaskTextureSm;
	static grcTexture *ms_MaskTextureLg;
	static grcTexture *ms_MaskTextureCnCSm;
	static grcTexture *ms_MaskTextureCnCLg;

#if RSG_PC
	struct extraquad
	{
		Vec4f pos;
		unsigned int flags;
	};

	static extraquad ms_quads[MAX_FOW_TARGETS*(MAX_FOG_SCRIPT_REVEALS + 1)];
	static bool ms_gotQuads;
	
	static void FOWRenderCatchupQuads();

	static int ms_iMultiGPUCount;
#endif
};

#endif // #ifndef __MINIMAP_RENDER_THREAD_H__

// eof


