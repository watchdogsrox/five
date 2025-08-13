
#include "MiniMapRenderThread.h"


// rage:
#include "grcore/allocscope.h"
#include "grcore/quads.h"
#include "scaleform/scaleform.h"
#include "grcore/light.h"
#if DEVICE_EQAA
#include "grcore/fastquad.h"
#endif

// framework:
#include "fwsys/gameskeleton.h"
#include "fwsys/timer.h"
#include "fwmaths/Rect.h"

// game:
#include "camera/viewports/Viewport.h"
#include "Control/gps.h"
#include "debug/DebugScene.h"
#include "frontend/CMapMenu.h"
#include "frontend/ui_channel.h"
#include "frontend/Map/BlipEnums.h"
#include "frontend/Scaleform/ScaleformMgr.h"
#include "Network/Live/livemanager.h"
#include "profile/group.h"
#include "renderer/postprocessFX.h"
#include "renderer/rendertargets.h"
#include "renderer/sprite2d.h"
#include "scene/world/GameWorldHeightMap.h"
#include "script/script.h"
#include "script/script_hud.h"
#include "system/companion.h"
#include "text/TextConversion.h"
#include "timecycle/TimeCycleConfig.h"

RAGE_DEFINE_CHANNEL(supertile)

#define supertileAssertf(cond,fmt,...)				RAGE_ASSERTF(supertile,cond,fmt,##__VA_ARGS__)
#define supertileVerifyf(cond,fmt,...)				RAGE_VERIFYF(supertile,cond,fmt,##__VA_ARGS__)
#define supertileWarningf(fmt,...)					RAGE_WARNINGF(supertile,fmt,##__VA_ARGS__)
#define supertileDisplayf(fmt,...)					RAGE_DISPLAYF(supertile,fmt,##__VA_ARGS__)
#define supertileDebugf1(fmt,...)					RAGE_DEBUGF1(supertile,fmt,##__VA_ARGS__)
#define supertileDebugf2(fmt,...)					RAGE_DEBUGF2(supertile,fmt,##__VA_ARGS__)
#define supertileDebugf3(fmt,...)					RAGE_DEBUGF3(supertile,fmt,##__VA_ARGS__)


#define BLIP_LAYER_PATH "_level0.asRootContainer.asBlipLayerContainer.asBlipContainer.blipLayer"

#define MINIMAP_INITIAL_SCALE (533.333333333333f)

#define MINIMAP_TILE_WIDTH			(MINIMAP_WORLD_SIZE_X / MINIMAP_WORLD_TILE_SIZE_X)
#define MINIMAP_TILE_HEIGHT			(MINIMAP_WORLD_SIZE_Y / MINIMAP_WORLD_TILE_SIZE_Y)

#define DISABLE_HUD_FOR_CONTENT_CONTROLLED_BUILD 0

#define __DEBUG_SUPERTILES (0)  // slightly optimises the supertiles

//
// we move the content of the bigmap internally so that the movie doesnt need to be moved off the edge of the screen to align with the minimap
//
#define __BIGMAP_INTERNAL_ADJUSTMENT_BLIP_RANGE (0.18f)  // blip range offset
#define __BIGMAP_INTERNAL_ADJUSTMENT_CONTAINERS (34.0f)  // amount to move the containers

#if DEBUG_DRAW && __BANK

// because these are processed on subrender threads, we have to queue them up to the UT... to then queue back onto the RT
struct debugVolume
{
	debugVolume() : bCollides(false), type(DVVT_Sphere) {};
	debugVolume(const spdSphere& sphere, Color32 color, bool bCollides_ = false) : asSphere(sphere), bCollides(bCollides_), objColor(color), type(DVVT_Sphere) {};
	debugVolume(const spdOrientedBB& BB, Color32 color, bool bCollides_ = false) : asBB(BB), bCollides(bCollides_), objColor(color), type(DVVT_OBB) {};
	debugVolume(Vec3V_In pt1, Vec3V_In pt2, Color32 color, Color32 color2, bool bCollides_ = false) : asLinePoint1(pt1), asLinePoint2(pt2), bCollides(bCollides_), objColor(color), objColor2(color2), type(DVVT_Line) {};
	debugVolume& operator=(const debugVolume& b)
	{
		type = b.type;
		objColor = b.objColor;
		objColor2 = b.objColor2;
		bCollides = b.bCollides;
		switch(type)
		{
		case DVVT_Sphere:
			asSphere = b.asSphere;
			break;
		case DVVT_OBB:
			asBB = b.asBB;
			break;
		case DVVT_Line:
			asLinePoint1 = b.asLinePoint1;
			asLinePoint2 = b.asLinePoint2;
			break;
		}

		return *this;
	}
	enum volType {
		DVVT_Sphere,
		DVVT_OBB,
		DVVT_Line
	};

	volType type;

	//	union { // durango's being a jerk about this. So we'll get object bloat
	spdSphere asSphere;
	spdOrientedBB asBB;
	Vec3V asLinePoint1;
	Vec3V asLinePoint2;
	//	};


	Color32 objColor;
	Color32 objColor2;
	bool bCollides;
};

static CDblBuf<atArray<debugVolume>> s_DebugDrawVolumes;
bool CMiniMap_RenderThread::sm_bDrawCollisionVolumes;	

void CMiniMap_RenderThread::RenderDebugVolumesOnUT()
{
	if(!sm_bDrawCollisionVolumes)
		return;

	for( int i=0; i < s_DebugDrawVolumes.GetUpdateBuf().GetCount(); ++i )
	{
		debugVolume& vol = s_DebugDrawVolumes.GetUpdateBuf()[i];
		switch(vol.type)
		{
		case debugVolume::DVVT_OBB:
			{
				Mat34V mat(vol.asBB.m_localToWorld);
				grcDebugDraw::BoxOriented(vol.asBB.m_localBox.GetMin(), vol.asBB.m_localBox.GetMax(), mat, vol.objColor, vol.bCollides);
				break;
			}
		case debugVolume::DVVT_Line:
			grcDebugDraw::Line(vol.asLinePoint1, vol.asLinePoint2, vol.objColor, vol.objColor2);
			break;

		case debugVolume::DVVT_Sphere:
			grcDebugDraw::Sphere(vol.asSphere.GetCenter(), vol.asSphere.GetRadiusf(), vol.objColor, vol.bCollides, 1,24);
			break;
		}
	}

	s_DebugDrawVolumes.GetUpdateBuf().Reset();
}

#endif // DEBUG_DRAW && BANK

//
// depth settings on movies:
//
enum
{
	MOVIE_DEPTH_ROOT_CONTAINER = 1,
	MOVIE_DEPTH_ROOT_BLIP_CONTAINER,
	MOVIE_DEPTH_MAP,
	MOVIE_DEPTH_FREEWAY,
	MOVIE_DEPTH_INTERIOR,
	MOVIE_DEPTH_GOLF_COURSE,
	MOVIE_DEPTH_BITMAP,
	MOVIE_DEPTH_TILES,
	MOVIE_DEPTH_TERRITORY,
	MOVIE_DEPTH_RADIUS,
	MOVIE_DEPTH_STEALTH,
	MOVIE_DEPTH_ALTIMETER_DISPLAY,
	MOVIE_DEPTH_RUNWAY,
	MOVIE_DEPTH_GPS,
	MOVIE_DEPTH_MASK = MOVIE_DEPTH_GPS+GPS_NUM_SLOTS,  // allow a gap here for the slots
	MOVIE_DEPTH_HEALTH,
	MOVIE_DEPTH_BLIPS_3D,
	MOVIE_DEPTH_BLIPS_2D,
	MOVIE_DEPTH_TEST_BLIP_3D,
	MOVIE_DEPTH_CROSSHAIR,
	MAX_MOVIE_DEPTHS
};


//
// depth of the blip and its children/siblings
//
enum
{
	BLIP_DEPTH_OUTLINE_INDICATOR = 1,	// outline blip
	BLIP_DEPTH_FRIEND_INDICATOR,		// friend_blip
	BLIP_DEPTH_CREW_INDICATOR,			// crew blip
	BLIP_DEPTH_MAIN_BLIP,				// main_blip
	BLIP_DEPTH_DIRECTION_INDICATOR,		// direction_blip
	BLIP_DEPTH_NUMBER,					// number
	BLIP_DEPTH_TICK,					// tick
	BLIP_DEPTH_SALE_ICON,				// for sale blip
	BLIP_DEPTH_HIGHER_LOWER,			// higher/lower blip
	MAX_BLIP_DEPTH_VALUES	
};

#if	!__FINAL
PARAM(minimap3d, "[code] 3D minimap");
PARAM(minimapRound, "[code] Round 2D minimap");
XPARAM(logMinimapTransitions);
#endif


#if __DEV
extern u32 iTimeTaken;
#endif // __DEV

#if __BANK
extern bool bDisplayInteriorInfoToLog;
extern bool bDebug3DBlips;
//	extern float ms_fTilesAroundPlayer;
//	extern Vector2 ms_vTilesAroundPlayer;
static Vector2 vGameScreenPos(0,0);
extern float fDebugTiltValue;
extern float fDebugOffsetValue;

extern bool noUIAA;
extern bool noUIAAOnBitMap;

bool CMiniMap_RenderThread::sm_bDebugMaskDisplay = false;
u8 CMiniMap_RenderThread::sm_uDebugMaskAlpha = 50;
#endif	//	__BANK
BankFloat CMiniMap_RenderThread::sm_fMultiheadEdgeFeatherPct = 0.012f;

extern float fFreewayDisplayZ;

#define MINIMAP_VISIBLE_TILES_NORTH	(0)  // these are the range of tiles which we will want to display (we ignore tiles outside this area as its unused as its just blank "sea" areas
#define MINIMAP_VISIBLE_TILES_SOUTH	(53)
#define MINIMAP_VISIBLE_TILES_EAST	(47)
#define MINIMAP_VISIBLE_TILES_WEST	(0)

#define WEAPON_BLIP_NAME ("radar_weapon_")  // i need to make these exposed to code really instead of this hack, but it will do for now since the bug is well overdue!
#define HOOP_BLIP_NAME ("radar_ai")  // for bug 1027418

//	BankFloat
extern float ALTIMETER_GROUND_LEVEL_FUDGE_FACTOR;
extern float ALTIMETER_TICK_PIXEL_HEIGHT;

extern float MINIMAP_GPS_DISPLAY_LIMIT;

extern float MAP_BLIP_ROUND_THRESHOLD;

bool bMiniMap3D = true;
bool bMiniMapSquare = true;

//	Static member variables
atArray<sConesAttachedToBlips> CMiniMap_RenderThread::ms_ConesAttachedToBlips;

sMiniMapRenderStateStruct CMiniMap_RenderThread::ms_MiniMapRenderState;

//	Blend States used when rendering the minimap
grcBlendStateHandle CMiniMap_RenderThread::ms_StandardMiniMapRenderBlendStateHandle = grcStateBlock::BS_Invalid;
grcBlendStateHandle CMiniMap_RenderThread::ms_ClearAlphaMiniMapRenderBlendStateHandle = grcStateBlock::BS_Invalid;
grcBlendStateHandle CMiniMap_RenderThread::ms_CopyAAMiniMapRenderBlendStateHandle = grcStateBlock::BS_Invalid;
grcSamplerStateHandle CMiniMap_RenderThread::ms_MinimapBitmapSamplerStateHandle = grcStateBlock::SS_Invalid;
grcRasterizerStateHandle CMiniMap_RenderThread::ms_MinimapBitmapRasterizerStateHandle = grcStateBlock::RS_Invalid;
#if __DEV
grcSamplerStateHandle CMiniMap_RenderThread::ms_MinimapBitmapSamplerStateNearestHandle = grcStateBlock::SS_Invalid;
#endif
#if ENABLE_FOG_OF_WAR 
grcBlendStateHandle CMiniMap_RenderThread::ms_MinimapFoWBlendStateHandle = grcStateBlock::BS_Invalid;
#if __D3D11
bool CMiniMap_RenderThread::ms_bUpdateFogOfWarData= false;
#endif // __D3D11
#endif
float CMiniMap_RenderThread::ms_MiniMapAlpha = 1.0f;

EXT_PF_PAGE(MiniMap);
PF_GROUP(MiniMapRenderThread);
PF_LINK(MiniMap, MiniMapRenderThread);
PF_TIMER(UpdateIndividualBlip, MiniMapRenderThread);
PF_TIMER(AddBlipToStage, MiniMapRenderThread);
PF_TIMER(RemoveBlipFromStage, MiniMapRenderThread);

#if ENABLE_FOG_OF_WAR
float FoWPrevX = 0.0f;
float FoWPrevY = 0.0f;
u32 FowLastFrameRead = 0;

bank_s32 fowTileSize = 16;
bank_s32 fowFrameCount = 33;

#if __BANK
bool DebugDrawFOW = false;
float DebugFOWX1 = 0.6f;
float DebugFOWY1 = 0.5f;
float DebugFOWX2 = 0.8f;
float DebugFOWY2 = 0.85f;

int Fow_WorldX = -3447;
int Fow_WorldY = -7722;
int Fow_WorldW = 7647;
int Fow_WorldH = 11368;

#endif // __BANK

Color32 fowColor(64,64,64,64); // PS3's using a signed blend because lum8
Color32 fowBleepColor(64,64,64,64);
bank_s32 bleepCount = 2;

bank_float fowColorSizeX = 2.5f;
bank_float fowColorSizeY = 2.5f;

bank_float fowBlipColorSizeX = 1.75f;
bank_float fowBlipColorSizeY = 1.75f;

bank_float moveStep = 0.25f;

#if RSG_PC
static int g_curFOWClearGPUCount = 0;
static int g_curFOWRevealGPUCount = 0;
#endif
#endif // ENABLE_FOG_OF_WAR

s32 CMiniMap_RenderThread::ms_iMovieId[MAX_MINIMAP_MOVIE_LAYERS];

Vector2 CMiniMap_RenderThread::ms_vOriginalStageSize;
Vector2 CMiniMap_RenderThread::ms_vStageSize;
CDblBuf<GMatrix3D> CMiniMap_RenderThread::ms_LocalToScreen;

GFxValue CMiniMap_RenderThread::asBitmap;

GFxValue *CMiniMap_RenderThread::pRenderedBlipObject[MAX_NUM_BLIPS];
atFixedBitSet<MAX_NUM_BLIPS> CMiniMap_RenderThread::bBlipIsOn3dLayer;

GFxValue CMiniMap_RenderThread::ms_asRootContainer[MAX_MINIMAP_ROOT_LAYERS];
GFxValue CMiniMap_RenderThread::ms_asBaseLayerContainer[MAX_MINIMAP_LAYERS];
GFxValue CMiniMap_RenderThread::ms_asBaseOverlay3D[MAX_MINIMAP_LAYERS];
GFxValue CMiniMap_RenderThread::ms_asMapContainerMc;
GFxValue CMiniMap_RenderThread::ms_asBlipContainerMc;
GFxValue CMiniMap_RenderThread::ms_asGpsLayer[GPS_NUM_SLOTS];
GFxValue CMiniMap_RenderThread::ms_asMapObject;
GFxValue CMiniMap_RenderThread::ms_asBlipLayer3D;
GFxValue CMiniMap_RenderThread::ms_asInteriorMc;
GFxValue CMiniMap_RenderThread::ms_asInteriorMovie;
GFxValue CMiniMap_RenderThread::ms_asGolfCourseMc;
GFxValue CMiniMap_RenderThread::ms_asGolfCourseMovie;
GFxValue CMiniMap_RenderThread::ms_asGolfCourseHole;
GFxValue CMiniMap_RenderThread::ms_asTileLayerContainer;
GFxValue CMiniMap_RenderThread::ms_asBitmapLayerContainer;
GFxValue CMiniMap_RenderThread::ms_asFreewayMovieClip;

CMiniMap_RenderThread::AltimeterDisplay CMiniMap_RenderThread::ms_Altimeter;

#if __STREAMED_SUPERTILE_FILES
CSupertiles CMiniMap_RenderThread::ms_Supertiles;
#endif	//	__STREAMED_SUPERTILE_FILES

sMiniMapInterior CMiniMap_RenderThread::ms_PreviousInterior;

s32 CMiniMap_RenderThread::ms_iPreviousGolfCourse = GOLF_COURSE_OFF;

s32 CMiniMap_RenderThread::ms_iInteriorMovieId = -1;
bool CMiniMap_RenderThread::ms_bInteriorMovieIdFullySetup = false;
bool CMiniMap_RenderThread::ms_bInteriorWasSetOnPerFrame = false;
s32 CMiniMap_RenderThread::ms_iGolfCourseMovieId = -1;

bool CMiniMap_RenderThread::ms_bRunwayBlipsAreDisplaying = false;

bool CMiniMap_RenderThread::ms_bRemoveInteriorMovie = false;
bool CMiniMap_RenderThread::ms_bStreamInteriorMovie = false;
bool CMiniMap_RenderThread::ms_bRemoveGolfCourseMovie = false;
bool CMiniMap_RenderThread::ms_bStreamGolfCourseMovie = false;

bank_bool CMiniMap_RenderThread::ms_bUseTexturedAlphaAllMovies = false;
bank_bool CMiniMap_RenderThread::ms_bUseTextureAlphaBaseMovie = true;

grcTexture *CMiniMap_RenderThread::ms_MaskTextureSm = NULL;
grcTexture *CMiniMap_RenderThread::ms_MaskTextureLg = NULL;
grcTexture *CMiniMap_RenderThread::ms_MaskTextureCnCSm = NULL;
grcTexture *CMiniMap_RenderThread::ms_MaskTextureCnCLg = NULL;

float CMiniMap_RenderThread::ms_fBlipMapRange = 1.0f;
float CMiniMap_RenderThread::ms_fBlipMapAngle = 0.0f;

#if RSG_PC
CMiniMap_RenderThread::extraquad CMiniMap_RenderThread::ms_quads[MAX_FOW_TARGETS*(MAX_FOG_SCRIPT_REVEALS + 1)];
bool CMiniMap_RenderThread::ms_gotQuads = false;

int CMiniMap_RenderThread::ms_iMultiGPUCount = 0;
#endif

#if __STREAMED_SUPERTILE_FILES

// ****************************** CSupertile ******************************

void CSupertile::Init(u32 Row, u32 Column)
{
	m_SupertileRow = Row;
	m_SupertileColumn = Column;
	m_bStreamThisSupertile = false;
	m_bRemoveThisSupertile = false;
	m_bPreviousValueOfShouldBeLoaded = false;
	m_bFullyLoaded = false;

	Remove(); // Resets everything
}

void CSupertile::Shutdown(unsigned shutdownMode)
{
	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on UT
	{
		sfAssertf(0, "CSupertile::Shutdown can only be called on the UpdateThread!");
		return;
	}

	if (shutdownMode == SHUTDOWN_SESSION)
	{
		supertileDebugf3("CSupertile::Shutdown releasing drawable %d", m_dwdRequest.GetRequestId().Get());
		Remove();

		m_bFullyLoaded = false;
		m_bStreamThisSupertile = false;
		m_bRemoveThisSupertile = false;
		m_bPreviousValueOfShouldBeLoaded = false;
	}
}

void CSupertile::Remove()
{
	m_Background = NULL;
	m_Sea = NULL;
	for(int i = 0; i < HEIGHT_OF_DWD_SUPERTILE; i++)
	{
		for(int j = 0; j < WIDTH_OF_DWD_SUPERTILE; j++)
		{
			m_Foregrounds[i][j] = NULL;
		}
	}
	m_dwdRequest.ClearRequiredFlags(STRFLAG_DONTDELETE);
	m_dwdRequest.ClearMyRequestFlags(STRFLAG_DONTDELETE);
	m_dwdRequest.DelayedRelease();
}


void CSupertile::Update(bool bShouldBeLoaded, GFxValue & UNUSED_PARAM(TileLayerContainer))
{
	if (m_bPreviousValueOfShouldBeLoaded != bShouldBeLoaded)
	{
		if (bShouldBeLoaded)
		{
			m_bStreamThisSupertile = true;
		}
		else
		{
			m_bRemoveThisSupertile = true;
		}
		m_bPreviousValueOfShouldBeLoaded = bShouldBeLoaded;
	}
}

bool g_RenderMinimapSea = false;
bool g_RenderMinimap = true;
Color32 g_MinimapSeaColor(56, 74, 80, 0xff);  // for 1884099

void CSupertile::Render(int pass)
{
	Matrix34 FlattenZ;
	FlattenZ.MakeScale(1.0f, 1.0f, 0.1f);
	FlattenZ.d.Zero();

	if (m_bFullyLoaded && m_dwdRequest.IsValid())
	{
		if (pass == 0)
		{
			PPU_ONLY(grcEffect::SetEdgeViewportCullEnable(false));

			bool vectorSeaAllowed = g_RenderMinimapSea;
#if __BANK
			if (CMiniMap_RenderThread::IsInPauseMap())
			{
				vectorSeaAllowed = vectorSeaAllowed || CMiniMap::DrawVectorSeaPaused(); // normally false, allow switching on
			}
			else
			{
				vectorSeaAllowed = vectorSeaAllowed && CMiniMap::DrawVectorSeaMinimap(); // normally true, allow switching off
			}
#endif

			if (m_Sea && vectorSeaAllowed)
			{
				m_Sea->Draw(FlattenZ, 0xFFFFFFFF, 0);
			}
			if (m_Background)
			{
				m_Background->Draw(FlattenZ, 0xFFFFFFFF, 0);
			}
			PPU_ONLY(grcEffect::SetEdgeViewportCullEnable(true));
		}
		else
		{
			for(int row = 0; row < HEIGHT_OF_DWD_SUPERTILE; row++)
			{
				for(int col = 0; col < WIDTH_OF_DWD_SUPERTILE; col++)
				{
					if (m_Foregrounds[row][col])
					{
						m_Foregrounds[row][col]->Draw(FlattenZ, 0xFFFFFFFF, 0);
					}
				}
			}
		}
	}



#if __BANK
	if (pass == 1 && CMiniMap::DebugDrawTiles())
	{
		spdRect superRect = CSupertiles::GetSuperTileWorldSpaceBounds(m_SupertileRow, m_SupertileColumn);
		Vec2V superCorners[4];
		superRect.GetCorners(superCorners);
		grcColor(Color_black);
		grcBegin(drawLineStrip, 5);
		grcVertex2f(superCorners[0]);
		grcVertex2f(superCorners[1]);
		grcVertex2f(superCorners[2]);
		grcVertex2f(superCorners[3]);
		grcVertex2f(superCorners[0]);
		grcEnd();
		Vec2V center = Average(superCorners[0], superCorners[1]);
		Vector3 center3(center.GetXf(), center.GetYf(), 0.0f);
		char label[64];

		if (m_bFullyLoaded)
		{
			for(int row = 0; row < HEIGHT_OF_DWD_SUPERTILE; row++)
			{
				for(int col = 0; col < WIDTH_OF_DWD_SUPERTILE; col++)
				{
					spdRect rect = CSupertiles::GetMiniTileWorldSpaceBounds(m_SupertileRow, m_SupertileColumn, row, col);

					Vec2V corners[4];
					rect.GetCorners(corners);

					grcColor(m_Foregrounds[row][col] == NULL ? Color_blue : Color_red);
					grcBegin(drawLineStrip, 8);
					grcVertex2f(corners[0]);
					grcVertex2f(corners[1]);
					grcVertex2f(corners[2]);
					grcVertex2f(corners[3]);
					grcVertex2f(corners[0]);
					grcVertex2f(corners[2]);
					grcVertex2f(corners[1]);
					grcVertex2f(corners[3]);
					grcEnd();
				}
			}
			formatf(label, g_DwdStore.GetName(strLocalIndex(m_dwdRequest.GetRequestId())));
		}
		else
		{
			formatf(label, "r%d, c%d", m_SupertileRow, m_SupertileColumn);
		}
		grcDrawLabel(center3, label, true);
	}
#endif

}

void CSupertile::RenderFoW()
{
#if ENABLE_FOG_OF_WAR
	spdRect superRect = CSupertiles::GetSuperTileWorldSpaceBounds(m_SupertileRow, m_SupertileColumn);
	Vec2V superCorners[4];
	superRect.GetCorners(superCorners);
	grcColor(Color32(0xff,0xff,0xff,0xff));
	
	Vec2V superCornersUVs[4];
	
	for(int i=0;i<4;i++)
	{
		Vector2 coords = CMiniMap_RenderThread::WorldToFowCoord(superCorners[i].GetXf(), superCorners[i].GetYf());
		superCornersUVs[i].SetX(coords.x);
		superCornersUVs[i].SetY(coords.y);
	}

	grcDrawMode drawMode = drawQuads;
#if __D3D11
	drawMode = drawTriStrip;
#endif
	grcBegin(drawMode, 4);
	grcTexCoord2f(superCornersUVs[0]);
	grcVertex2f(superCorners[0]);
	grcTexCoord2f(superCornersUVs[1]);
	grcVertex2f(superCorners[1]);

#if __D3D11
	grcTexCoord2f(superCornersUVs[3]);
	grcVertex2f(superCorners[3]);

	grcTexCoord2f(superCornersUVs[2]);
	grcVertex2f(superCorners[2]);
#else
	grcTexCoord2f(superCornersUVs[2]);
	grcVertex2f(superCorners[2]);
	grcTexCoord2f(superCornersUVs[3]);
	grcVertex2f(superCorners[3]);
#endif

	grcEnd();
#endif // ENABLE_FOG_OF_WAR
}

void CSupertile::RenderIslandFoW()
{
#if ENABLE_FOG_OF_WAR

static dev_float ms_IslandWorldX = 2700;	// TODO: customize these via overrides in GetSuperTileWorldSpaceBounds()
static dev_float ms_IslandWorldY = -3150;
static dev_float ms_IslandWorldW = 4000;
static dev_float ms_IslandWorldH = 4000;
	//float superWidth = CMiniMap::sm_Tunables.Tiles.vMiniMapWorldSize.x / (float)(NUMBER_OF_SUPERTILE_COLUMNS);
	//float superHeight = CMiniMap::sm_Tunables.Tiles.vMiniMapWorldSize.y / (float)(NUMBER_OF_SUPERTILE_ROWS);
	//Vec2V superStart(CMiniMap::sm_Tunables.Tiles.vMiniMapWorldStart.x + superWidth * col, CMiniMap::sm_Tunables.Tiles.vMiniMapWorldStart.y - superHeight * row);
	//Vec2V superEnd(CMiniMap::sm_Tunables.Tiles.vMiniMapWorldStart.x + superWidth * (col+1), CMiniMap::sm_Tunables.Tiles.vMiniMapWorldStart.y - superHeight * (row+1));
	Vec2V superStart(ms_IslandWorldX, ms_IslandWorldY);
	Vec2V superEnd(ms_IslandWorldX + ms_IslandWorldW, ms_IslandWorldY - ms_IslandWorldH);
	spdRect superRect(Min(superStart, superEnd), Max(superStart, superEnd));


	Vec2V superCorners[4];
	superRect.GetCorners(superCorners);
	grcColor(Color32(0xff,0xff,0xff,0xff));

	Vec2V superCornersUVs[4];

	for(int i=0;i<4;i++)
	{
		Vector2 coords = CMiniMap_RenderThread::WorldToFowCoord(superCorners[i].GetXf(), superCorners[i].GetYf());
		superCornersUVs[i].SetX(coords.x);
		superCornersUVs[i].SetY(coords.y);
	}

	grcDrawMode drawMode = drawQuads;
#if __D3D11
	drawMode = drawTriStrip;
#endif
	grcBegin(drawMode, 4);
	grcTexCoord2f(superCornersUVs[0]);
	grcVertex2f(superCorners[0]);
	grcTexCoord2f(superCornersUVs[1]);
	grcVertex2f(superCorners[1]);

#if __D3D11
	grcTexCoord2f(superCornersUVs[3]);
	grcVertex2f(superCorners[3]);

	grcTexCoord2f(superCornersUVs[2]);
	grcVertex2f(superCorners[2]);
#else
	grcTexCoord2f(superCornersUVs[2]);
	grcVertex2f(superCorners[2]);
	grcTexCoord2f(superCornersUVs[3]);
	grcVertex2f(superCorners[3]);
#endif

	grcEnd();
#endif // ENABLE_FOG_OF_WAR
}

void CSupertile::ProcessAtEndOfFrame(s32 UNUSED_PARAM(iParentMovieId))
{
	if (m_bRemoveThisSupertile)
	{
		supertileDebugf3("CSupertile::ProcessAtEndOfFrame releasing %d", m_dwdRequest.GetRequestId().Get());
		m_bRemoveThisSupertile = false;
		m_bFullyLoaded = false;
		Remove();
	}
	else if (m_bStreamThisSupertile)
	{
		char cSupertileName[20];
		formatf(cSupertileName, "minimap_%u_%u", m_SupertileColumn, m_SupertileRow);
		supertileDebugf3("CSupertile::ProcessAtEndOfFrame streaming supertile %s", cSupertileName);
		strLocalIndex slot = g_DwdStore.FindSlot(cSupertileName);
		if (slot.Get() >= 0)
		{
			supertileDebugf3("CSupertile::ProcessAtEndOfFrame requesting drawable %s %d", cSupertileName, slot.Get());
			m_dwdRequest.Request(slot, g_DwdStore.GetStreamingModuleId(), STRFLAG_FORCE_LOAD | STRFLAG_PRIORITY_LOAD | STRFLAG_DONTDELETE );
		}
		m_bStreamThisSupertile = false;
	}
	else
	{
		// Poll for load completion
		bool wasLoaded = m_bFullyLoaded;
		m_bFullyLoaded = m_dwdRequest.HasLoaded();

		if (m_bFullyLoaded)
		{
			m_dwdRequest.ClearRequiredFlags(STRFLAG_DONTDELETE); // ref counting takes over now
		}

		if (m_bPreviousValueOfShouldBeLoaded && !wasLoaded && m_bFullyLoaded) // if it's supposed to be loaded, and it just became loaded
		{
			Dwd* dict = g_DwdStore.Get(strLocalIndex(m_dwdRequest.GetRequestId()));
			supertileAssertf(dict, "Invalid dict?!");

			char bkgName[32];
			formatf(bkgName, "supertile_back_%d_%d", m_SupertileColumn, m_SupertileRow);
			m_Background = dict->LookupLocal(bkgName);

			char seaName[32];
			formatf(seaName, "supertile_sea_%d_%d", m_SupertileColumn, m_SupertileRow);
			m_Sea = dict->LookupLocal(seaName);

			for(int i = 0; i < HEIGHT_OF_DWD_SUPERTILE; i++)
			{
				for(int j = 0; j < WIDTH_OF_DWD_SUPERTILE; j++)
				{
					char minitileName[64];
					formatf(minitileName, "supertile_fore_%d_%d_tile_%d_%d", m_SupertileColumn, m_SupertileRow, j, i);
					m_Foregrounds[i][j] = dict->LookupLocal(minitileName);
				}
			}
		}
	}

}

void CSupertile::HandleIsLoadedCallback(const GFxValue& /*gfxVal*/)
{
}


// ****************************** CSupertiles ******************************

void CSupertiles::Init(unsigned initMode)
{
	if(initMode == INIT_CORE)
	{
		for (u32 row = 0; row < NUMBER_OF_SUPERTILE_ROWS; row++)
		{
			for (u32 column = 0; column < NUMBER_OF_SUPERTILE_COLUMNS; column++)
			{
				m_Supertiles[row][column].Init(row, column);
			}
		}
	}
	else if (initMode == INIT_SESSION)
	{
		m_PreviousTopTile = -1;
		m_PreviousBottomTile = -1;
		m_PreviousLeftTile = -1;
		m_PreviousRightTile = -1;
	}
}

void CSupertiles::Shutdown(unsigned shutdownMode)
{
	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on UT
	{
		sfAssertf(0, "CSupertiles::Shutdown can only be called on the UpdateThread!");
		return;
	}

	if (shutdownMode == SHUTDOWN_SESSION)
	{
		CScaleformMgr::AutoLock lockFore(CMiniMap::GetMovieId(MINIMAP_MOVIE_FOREGROUND));
		CScaleformMgr::AutoLock lockBack(CMiniMap::GetMovieId(MINIMAP_MOVIE_BACKGROUND));

		for (u32 row = 0; row < NUMBER_OF_SUPERTILE_ROWS; row++)
		{
			for (u32 column = 0; column < NUMBER_OF_SUPERTILE_COLUMNS; column++)
			{
				m_Supertiles[row][column].Shutdown(SHUTDOWN_SESSION);
			}
		}
	}
}

bool CSupertiles::Update(const Vector2 &vCurrentTile, const Vector2 &vTilesAroundPlayer, GFxValue &TileLayerContainer, bool bBitmapOnly)
{
#define MAX_TILES_TO_ATTACH_PER_UPDATE (10)
#define MAX_TILES_TO_DETACH_PER_UPDATE (10)

	//	Get x,y of top left and bottom right supertiles for current
	s32 CurrentTopTile = rage::Max((s32)(floorf(vCurrentTile.y - vTilesAroundPlayer.y)), MINIMAP_VISIBLE_TILES_NORTH);
	s32 CurrentBottomTile = rage::Min((s32)(ceilf(vCurrentTile.y + vTilesAroundPlayer.y)), MINIMAP_VISIBLE_TILES_SOUTH);
	s32 CurrentLeftTile = rage::Max((s32)(floorf(vCurrentTile.x - vTilesAroundPlayer.x)), MINIMAP_VISIBLE_TILES_WEST);
	s32 CurrentRightTile = rage::Min((s32)(ceilf(vCurrentTile.x + vTilesAroundPlayer.x)), MINIMAP_VISIBLE_TILES_EAST);

	// once all tiles have been detached, store the current tile we now are starting to attach...
	m_PreviousTopTile = CurrentTopTile;
	m_PreviousBottomTile = CurrentBottomTile;
	m_PreviousLeftTile = CurrentLeftTile;
	m_PreviousRightTile = CurrentRightTile;


//	Deal with loading/unloading the supertiles
	{
		s32 CurrentTopSupertile = CurrentTopTile/HEIGHT_OF_SUPERTILE;
		s32 CurrentBottomSupertile = CurrentBottomTile/HEIGHT_OF_SUPERTILE;
		s32 CurrentLeftSupertile = CurrentLeftTile/WIDTH_OF_SUPERTILE;
		s32 CurrentRightSupertile = CurrentRightTile/WIDTH_OF_SUPERTILE;

		for (s32 supertile_row = 0; supertile_row < NUMBER_OF_SUPERTILE_ROWS; supertile_row++)
		{
			for (s32 supertile_column = 0; supertile_column < NUMBER_OF_SUPERTILE_COLUMNS; supertile_column++)
			{
				bool bIsInVisibleRange = false;

				if (!bBitmapOnly)
				{
					if ( (supertile_row >= CurrentTopSupertile) && (supertile_row <= CurrentBottomSupertile) )			//	Change to < maybe
					{
						if ( (supertile_column >= CurrentLeftSupertile) && (supertile_column <= CurrentRightSupertile) )	//	Change to < maybe
						{
							bIsInVisibleRange = true;
						}
					}
				}

				m_Supertiles[supertile_row][supertile_column].Update(bIsInVisibleRange, TileLayerContainer);
			}
		}
	}

	return true;
}

void CSupertiles::ProcessAtEndOfFrame(s32 iParentMovieId)
{
	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on UT
	{
		sfAssertf(0, "CSupertiles::ProcessAtEndOfFrame can only be called on the UpdateThread!");
		return;
	}

	for (u32 row = 0; row < NUMBER_OF_SUPERTILE_ROWS; row++)
	{
		for (u32 column = 0; column < NUMBER_OF_SUPERTILE_COLUMNS; column++)
		{
			m_Supertiles[row][column].ProcessAtEndOfFrame(iParentMovieId);
		}
	}
}

void CSupertiles::HandleIsLoadedCallback(s32 Row, s32 Column, const GFxValue &gfxVal)
{
	if (Verifyf((Row >= 0) && (Row < NUMBER_OF_SUPERTILE_ROWS), "CSupertiles::HandleIsLoadedCallback - Row %d is out of range", Row))
	{
		if (Verifyf((Column >= 0) && (Column < NUMBER_OF_SUPERTILE_COLUMNS), "CSupertiles::HandleIsLoadedCallback - Column %d is out of range", Column))
		{
			m_Supertiles[Row][Column].HandleIsLoadedCallback(gfxVal);
		}
	}
}

spdRect CSupertiles::GetMiniTileWorldSpaceBounds(s32 superTileRow, s32 superTileCol, s32 tileRow, s32 tileColumn)
{
	spdRect superBounds = GetSuperTileWorldSpaceBounds(superTileRow, superTileCol);

	Vec2V superMin = superBounds.GetMin();
	Vec2V superMax = superBounds.GetMax();

	Vec2V superSize = superMax - superMin;
	Vec2V miniSize = superSize / Vec2V((float)WIDTH_OF_DWD_SUPERTILE, (float)(HEIGHT_OF_DWD_SUPERTILE));

	Vec2V miniStart = AddScaled(superMin, miniSize, Vec2V((float)tileColumn, (float)(HEIGHT_OF_DWD_SUPERTILE - (tileRow))));
	Vec2V miniEnd = AddScaled(superMin, miniSize, Vec2V((float)(tileColumn+1), (float)(HEIGHT_OF_DWD_SUPERTILE - (tileRow+1))));

	return spdRect(miniStart, miniEnd);
}

spdRect CSupertiles::GetSuperTileWorldSpaceBounds(s32 row, s32 col)
{
	float superWidth = CMiniMap::sm_Tunables.Tiles.vMiniMapWorldSize.x / (float)(NUMBER_OF_SUPERTILE_COLUMNS);
	float superHeight = CMiniMap::sm_Tunables.Tiles.vMiniMapWorldSize.y / (float)(NUMBER_OF_SUPERTILE_ROWS);

	Vec2V superStart(CMiniMap::sm_Tunables.Tiles.vMiniMapWorldStart.x + superWidth * col, CMiniMap::sm_Tunables.Tiles.vMiniMapWorldStart.y - superHeight * row);
	Vec2V superEnd(CMiniMap::sm_Tunables.Tiles.vMiniMapWorldStart.x + superWidth * (col+1), CMiniMap::sm_Tunables.Tiles.vMiniMapWorldStart.y - superHeight * (row+1));

	return spdRect(Min(superStart, superEnd), Max(superStart, superEnd));
}


void CSupertiles::Render()
{
	for(int layer = 0; layer < 2; layer++)
	{
		// TODO - based on camera orientation, draw in one of 8 directions to do back-to-front rendering
		for(int row = 0; row < NUMBER_OF_SUPERTILE_ROWS; row++)
		{
			for(int col = 0; col < NUMBER_OF_SUPERTILE_COLUMNS; col++)
			{
				m_Supertiles[row][col].Render(layer);
			}
		}
	}
}

void CSupertiles::RenderFoW()
{
	for(int row = 0; row < NUMBER_OF_SUPERTILE_ROWS; row++)
	{
		for(int col = 0; col < NUMBER_OF_SUPERTILE_COLUMNS; col++)
		{
			m_Supertiles[row][col].RenderFoW();
		}
	}
}


#endif	//	__STREAMED_SUPERTILE_FILES

#if SUPPORT_MULTI_MONITOR
class MultiMonitorHudHelper
{
	sMiniMapRenderStateStruct *m_State;
	Vector2 m_vPosition;
	Vector2 m_vSize;
	Vector2 m_vMaskPosition;
	Vector2 m_vMaskSize;
	Vector2 m_vBlurPosition;
	Vector2 m_vBlurSize;
	
	MultiMonitorHudHelper(const MultiMonitorHudHelper&);
	void operator= (const MultiMonitorHudHelper&);
	
public:
	static void TransformPos(const GridMonitor& mon, Vector2 *v)
	{
		v->x = (mon.uLeft + mon.getWidth() * v->x) / GRCDEVICE.GetWidth();
		v->y = (mon.uTop + mon.getHeight() * v->y) / GRCDEVICE.GetHeight();
	}
	static void TransformSize(const GridMonitor& mon, Vector2 *v)
	{
		v->x = mon.getWidth() * v->x / GRCDEVICE.GetWidth();
		v->y = mon.getHeight() * v->y / GRCDEVICE.GetHeight();
	}

	MultiMonitorHudHelper(sMiniMapRenderStateStruct *state)
	: m_State(state)
	, m_vPosition(state->m_vCurrentMiniMapPosition)
	, m_vSize(state->m_vCurrentMiniMapSize)
	, m_vMaskPosition(state->m_vCurrentMiniMapMaskPosition)
	, m_vMaskSize(state->m_vCurrentMiniMapMaskSize)
	, m_vBlurPosition(state->m_vCurrentMiniMapBlurPosition)
	, m_vBlurSize(state->m_vCurrentMiniMapBlurSize)
	{
		const GridMonitor &mon = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor();
		TransformPos (mon, &state->m_vCurrentMiniMapPosition);
		TransformSize(mon, &state->m_vCurrentMiniMapSize);
		TransformPos (mon, &state->m_vCurrentMiniMapMaskPosition);
		TransformSize(mon, &state->m_vCurrentMiniMapMaskSize);
		TransformPos (mon, &state->m_vCurrentMiniMapBlurPosition);
		TransformSize(mon, &state->m_vCurrentMiniMapBlurSize);
	}

	~MultiMonitorHudHelper()
	{
		m_State->m_vCurrentMiniMapPosition = m_vPosition;
		m_State->m_vCurrentMiniMapSize = m_vSize;
		m_State->m_vCurrentMiniMapMaskPosition = m_vMaskPosition;
		m_State->m_vCurrentMiniMapMaskSize = m_vMaskSize;
		m_State->m_vCurrentMiniMapBlurPosition = m_vBlurPosition;
		m_State->m_vCurrentMiniMapBlurSize = m_vBlurSize;
	}
};
#endif //SUPPORT_MULTI_MONITOR

void CMiniMap_RenderThread::Init(unsigned initMode)
{
	if(initMode == INIT_CORE)
	{
		grcBlendStateDesc bsd;

		bsd.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALPHA;
		ms_ClearAlphaMiniMapRenderBlendStateHandle = grcStateBlock::CreateBlendState(bsd);
		
		// stateblock for standard minimap render
		bsd.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_RGB;
		bsd.BlendRTDesc[0].BlendEnable = true;
		bsd.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_ONE;
		bsd.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
		bsd.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
		ms_StandardMiniMapRenderBlendStateHandle = grcStateBlock::CreateBlendState(bsd);

		grcSamplerStateDesc ssd;
		ssd.AddressU = grcSSV::TADDRESS_CLAMP;
		ssd.AddressV = grcSSV::TADDRESS_CLAMP;
		ms_MinimapBitmapSamplerStateHandle = grcStateBlock::CreateSamplerState(ssd);

#if __DEV
		ssd.Filter = grcSSV::FILTER_MIN_MAG_MIP_POINT;
		ms_MinimapBitmapSamplerStateNearestHandle = grcStateBlock::CreateSamplerState(ssd);
#endif

		grcRasterizerStateDesc rsd;
		rsd.CullMode = grcRSV::CULL_NONE;
		ms_MinimapBitmapRasterizerStateHandle = grcStateBlock::CreateRasterizerState(rsd);

#if ENABLE_FOG_OF_WAR 
		bsd.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALPHA;
		bsd.BlendRTDesc[0].BlendEnable = true;
		bsd.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_ONE;
		bsd.BlendRTDesc[0].DestBlend = grcRSV::BLEND_ONE;
		bsd.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_MIN;
		bsd.BlendRTDesc[0].SrcBlendAlpha = grcRSV::BLEND_ONE;
		bsd.BlendRTDesc[0].DestBlendAlpha = grcRSV::BLEND_ONE;
		bsd.BlendRTDesc[0].BlendOpAlpha = grcRSV::BLENDOP_MIN;
		
		ms_MinimapFoWBlendStateHandle = grcStateBlock::CreateBlendState(bsd);
#endif // ENABLE_FOG_OF_WAR

		// stateblock for copying the fully composited minimap with AA to the UI buffer
		bsd.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_ALL;
		bsd.BlendRTDesc[0].BlendEnable = true;
		bsd.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_ONE;
		bsd.BlendRTDesc[0].DestBlend = grcRSV::BLEND_ZERO;
		bsd.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
		bsd.BlendRTDesc[0].SrcBlendAlpha = grcRSV::BLEND_ZERO;
		bsd.BlendRTDesc[0].DestBlendAlpha = grcRSV::BLEND_ONE;
		bsd.BlendRTDesc[0].BlendOpAlpha = grcRSV::BLENDOP_ADD;

		ms_CopyAAMiniMapRenderBlendStateHandle = grcStateBlock::CreateBlendState(bsd);

		for (s32 i = 0; i < MAX_MINIMAP_MOVIE_LAYERS; i++)
		{
			ms_iMovieId[i] = -1;
		}

#if !__FINAL
		if (PARAM_minimapRound.Get())
		{
			bMiniMapSquare = false;
			bMiniMap3D = false;
		}

		if (PARAM_minimap3d.Get())
		{
			bMiniMap3D = true;
		}

#endif // !__FINAL
	}
	else if(initMode == INIT_SESSION)
	{
		ms_bRunwayBlipsAreDisplaying = false;	//	Will the runway movie be hidden when a new session is started?

		ms_asInteriorMc.SetUndefined();
		ms_asInteriorMovie.SetUndefined();
		ms_asGolfCourseMc.SetUndefined();
		ms_asGolfCourseMovie.SetUndefined();
		ms_asGolfCourseHole.SetUndefined();

		ms_PreviousInterior.Clear();

		ms_iPreviousGolfCourse = GOLF_COURSE_OFF;

		ms_iInteriorMovieId = -1;
		ms_bInteriorMovieIdFullySetup = false;
		ms_bInteriorWasSetOnPerFrame = false;
		ms_iGolfCourseMovieId = -1;

#if ENABLE_FOG_OF_WAR
		FowLastFrameRead = 0;
#endif // ENABLE_FOG_OF_WAR
		
		// initialise all blips:
		for (s32 iCount = 0; iCount < MAX_NUM_BLIPS; iCount++)
		{
			pRenderedBlipObject[iCount] = NULL;
		}

		s32 txdSlot = CShaderLib::GetTxdId();
		CMiniMap_RenderThread::ms_MaskTextureSm		= g_TxdStore.Get(strLocalIndex(txdSlot))->Lookup(ATSTRINGHASH("RadarMaskSm", 0x6dd9a16));
		CMiniMap_RenderThread::ms_MaskTextureLg		= g_TxdStore.Get(strLocalIndex(txdSlot))->Lookup(ATSTRINGHASH("RadarMaskLg", 0x91a810f0));
 		//CMiniMap_RenderThread::ms_MaskTextureCnCSm	= g_TxdStore.Get(strLocalIndex(txdSlot))->Lookup(ATSTRINGHASH("RadarMaskCnCSm", 0xEC1F6E28));
		//AssertMsg(CMiniMap_RenderThread::ms_MaskTextureCnCSm, "RadarMaskCnCSm not found!");
		//CMiniMap_RenderThread::ms_MaskTextureCnCLg	= g_TxdStore.Get(strLocalIndex(txdSlot))->Lookup(ATSTRINGHASH("RadarMaskCnCLg", 0x1AD26689));
		//AssertMsg(CMiniMap_RenderThread::ms_MaskTextureCnCLg, "RadarMaskCnCLg not found!");

#if RSG_PC
		ms_gotQuads = false;

		for (int i = 0; i < MAX_FOW_TARGETS*(MAX_FOG_SCRIPT_REVEALS + 1); i++)
		{
			ms_quads[i].flags = 0x7;
		}
#endif
	}

#if __STREAMED_SUPERTILE_FILES
	ms_Supertiles.Init(initMode);
#endif	//	__STREAMED_SUPERTILE_FILES
}

void CMiniMap_RenderThread::Shutdown(unsigned shutdownMode)
{
	if (shutdownMode == SHUTDOWN_CORE)
	{
	}
	else if (shutdownMode == SHUTDOWN_SESSION)
	{
		CScaleformMgr::AutoLock lockFore(CMiniMap::GetMovieId(MINIMAP_MOVIE_FOREGROUND));
		CScaleformMgr::AutoLock lockBack(CMiniMap::GetMovieId(MINIMAP_MOVIE_BACKGROUND));

		// remove any additional minimap movies we have loaded in
		RemoveInterior();
		HandleInteriorAtEndOfFrame();  // actually request the removal of the movie - fixes 1617703

		RemoveGolfCourse(true);
		HandleGolfCourseAtEndOfFrame();  // actually request the removal of the movie - fixes 1617703

		for (int loop = 0; loop < ms_ConesAttachedToBlips.GetCount(); loop++)
		{
			RemoveConeFromBlipOnStage(ms_ConesAttachedToBlips[loop].m_iActualIdOfBlip);
		}
		ms_ConesAttachedToBlips.Reset();

		CMiniMap_RenderThread::ms_MaskTextureSm = NULL;
		CMiniMap_RenderThread::ms_MaskTextureLg = NULL;
		CMiniMap_RenderThread::ms_MaskTextureCnCSm = NULL;
		CMiniMap_RenderThread::ms_MaskTextureCnCLg = NULL;

	}

#if __STREAMED_SUPERTILE_FILES
	ms_Supertiles.Shutdown(shutdownMode);
#endif	//	__STREAMED_SUPERTILE_FILES

}



void CMiniMap_RenderThread::SetMiniMapRenderState(const sMiniMapRenderStateStruct &newRenderState)
{
	// copy over the structure
	sysMemCpy(&ms_MiniMapRenderState, &newRenderState, sizeof(sMiniMapRenderStateStruct));
}

void CMiniMap_RenderThread::PrepareForRendering()
{
	UpdateStatesWithActionScriptOnRT();
	if (ms_MiniMapRenderState.m_bShouldProcessMiniMap)  // if in pause map and not in map screen then don't render
	{
		UpdateWithActionScriptOnRT(((!ms_MiniMapRenderState.m_bShouldRenderMiniMap) || ms_MiniMapRenderState.m_CurrentGolfMap != GOLF_COURSE_OFF), false);
	}
	else
	{
		// continue to update the tiles even though its not rendered since we need them to appear as soon as possible
		// so if they can be streaming and attaching in the background whilst the minimap isnt getting rendered, all the better
		// otherwise they stream in as soon as the minimap pops on screen and you see the LOD for a second or so
		UpdateTiles(false);  
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::SetLocalStageSize()
// PURPOSE: gets the stage size and stores it locally in code
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::SetLocalStageSize()
{
	GFxMovieView* pView = CScaleformMgr::GetMovieView(ms_iMovieId[MINIMAP_MOVIE_FOREGROUND]);

	if (uiVerifyf(pView, "CMiniMap_RenderThread: no movieview to get stage size from at init"))
	{
		ms_vOriginalStageSize = Vector2(pView->GetMovieDef()->GetWidth(), pView->GetMovieDef()->GetHeight());  // store the stage size
		ms_vStageSize = ms_vOriginalStageSize;
	}
	else
	{
		ms_vStageSize = Vector2(0,0);  // it will be buggered!
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::CreateRoot()
// PURPOSE: sets up the main root layer
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::CreateRoot(s32 iId)
{
	switch (iId)
	{
		case MINIMAP_MOVIE_BACKGROUND:
		{
			GFxValue asMovieObject1 = CScaleformMgr::GetActionScriptObjectFromRoot(ms_iMovieId[MINIMAP_MOVIE_BACKGROUND]);
			asMovieObject1.CreateEmptyMovieClip(&ms_asRootContainer[MINIMAP_ROOT_MAP_TRANSPARENT], "asRootContainer", MOVIE_DEPTH_ROOT_CONTAINER);
			break;
		}

		case MINIMAP_MOVIE_FOREGROUND:
		{
			GFxValue asMovieObject2 = CScaleformMgr::GetActionScriptObjectFromRoot(ms_iMovieId[MINIMAP_MOVIE_FOREGROUND]);
			asMovieObject2.CreateEmptyMovieClip(&ms_asRootContainer[MINIMAP_ROOT_LAYER_MASKED], "asRootContainer", MOVIE_DEPTH_ROOT_CONTAINER);

			GFxValue asMovieObject3 = CScaleformMgr::GetActionScriptObjectFromRoot(ms_iMovieId[MINIMAP_MOVIE_FOREGROUND]);
			asMovieObject3.CreateEmptyMovieClip(&ms_asRootContainer[MINIMAP_ROOT_LAYER_UNMASKED], "asRootContainerBlips", MOVIE_DEPTH_ROOT_BLIP_CONTAINER);
			break;
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::RemoveRoot()
// PURPOSE: removes the main root layer
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::RemoveRoot(s32 iId)
{
	switch (iId)
	{
		case MINIMAP_MOVIE_BACKGROUND:
		{
			if (ms_asRootContainer[MINIMAP_ROOT_MAP_TRANSPARENT].IsDefined())
			{
				ms_asRootContainer[MINIMAP_ROOT_MAP_TRANSPARENT].Invoke("removeMovieClip");
				ms_asRootContainer[MINIMAP_ROOT_MAP_TRANSPARENT].SetUndefined();
			}
			break;
		}

		case MINIMAP_MOVIE_FOREGROUND:
		{
			if (ms_asRootContainer[MINIMAP_ROOT_LAYER_MASKED].IsDefined())
			{
				ms_asRootContainer[MINIMAP_ROOT_LAYER_MASKED].Invoke("removeMovieClip");
				ms_asRootContainer[MINIMAP_ROOT_LAYER_MASKED].SetUndefined();
			}

			if (ms_asRootContainer[MINIMAP_ROOT_LAYER_UNMASKED].IsDefined())
			{
				ms_asRootContainer[MINIMAP_ROOT_LAYER_UNMASKED].Invoke("removeMovieClip");
				ms_asRootContainer[MINIMAP_ROOT_LAYER_UNMASKED].SetUndefined();
			}
			break;
		}
	}
}




/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::CreateTerritoryContainer
// PURPOSE: creates a container for the territory and passes info to actionscript
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::CreateTerritoryContainer()
{
	GFxValue asTerritoryContainer;
	
	ms_asBaseOverlay3D[MINIMAP_LAYER_FOREGROUND].CreateEmptyMovieClip(&asTerritoryContainer, "asTerritory3D", MOVIE_DEPTH_TERRITORY);

	GFxValue::DisplayInfo info;
	info.SetScale(MINIMAP_INITIAL_SCALE, MINIMAP_INITIAL_SCALE);
	asTerritoryContainer.SetDisplayInfo(info);

	if (uiVerifyf(asTerritoryContainer.IsDisplayObject(), "MiniMap: 'asTerritoryContainer' is not a display object at init"))
	{
		if (CScaleformMgr::BeginMethod(ms_iMovieId[MINIMAP_MOVIE_FOREGROUND], SF_BASE_CLASS_MINIMAP, "REGISTER_TERRITORY"))
		{
			CScaleformMgr::AddParamGfxValue(asTerritoryContainer);
			CScaleformMgr::EndMethod(true);  // this is instant as we are initialising the minimap here
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::RemoveTerritoryContainer
// PURPOSE: removes the containers
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::RemoveTerritoryContainer()
{
	GFxValue asTerritoryContainer;
	if (ms_asBaseOverlay3D[MINIMAP_LAYER_FOREGROUND].GFxValue::GetMember("asTerritory3D", &asTerritoryContainer))
	{
		asTerritoryContainer.Invoke("removeMovieClip");
		asTerritoryContainer.SetUndefined();
	}
}




/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::CreateHealthArmourContainer
// PURPOSE: creates a container for the health and armour
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::CreateHealthArmourContainer()
{
	GFxValue asHealthContainer;
	ms_asRootContainer[MINIMAP_ROOT_LAYER_UNMASKED].CreateEmptyMovieClip(&asHealthContainer, "asHealthContainer", MOVIE_DEPTH_HEALTH);

	if (uiVerifyf(asHealthContainer.IsDisplayObject(), "MiniMap: 'asHealthContainer' is not a display object at init"))
	{
		if (CScaleformMgr::BeginMethod(ms_iMovieId[MINIMAP_MOVIE_FOREGROUND], SF_BASE_CLASS_MINIMAP, "REGISTER_HEALTH_ARMOUR"))
		{
			CScaleformMgr::AddParamGfxValue(asHealthContainer);
			CScaleformMgr::EndMethod(true);  // this is instant as we are initialising the minimap here
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::RemoveHealthArmourContainer
// PURPOSE: removes the containers
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::RemoveHealthArmourContainer()
{
	GFxValue asHealthContainer;
	if (ms_asRootContainer[MINIMAP_ROOT_LAYER_UNMASKED].GFxValue::GetMember("asHealthContainer", &asHealthContainer))
	{
		asHealthContainer.Invoke("removeMovieClip");  // remove the mask movie
		asHealthContainer.SetUndefined();
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::CreateAltimeter
// PURPOSE: creates altimeter and hides it (it gets set to visible later when needed)
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::CreateAltimeter()
{
	if(ms_asRootContainer[MINIMAP_ROOT_LAYER_MASKED].CreateEmptyMovieClip(&ms_Altimeter.m_asContainerMc, "altimeterOffset", MOVIE_DEPTH_ALTIMETER_DISPLAY ) )
	{
		int iDepth = 1;
		ms_Altimeter.m_asContainerMc.AttachMovie(&ms_Altimeter.m_asGaugeMc,	"altimeterVertical",	"asAltimeterVertical",		iDepth++);
		ms_Altimeter.m_asContainerMc.AttachMovie(&ms_Altimeter.m_asRollMc,	"altimeterHorizontal",	"asAltimeterHorizontal",	iDepth++);
	}

	GFxValue::DisplayInfo info;

	info.SetPosition((ms_vStageSize.x * 0.5f), (ms_vStageSize.y * 0.5f));// + MINIMAP_VEHICLE_OFFSET);
	info.SetVisible(false);

	ms_Altimeter.m_asContainerMc.SetDisplayInfo(info);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::RemoveAltimeter
// PURPOSE: removes anything the altimeter has set up in AS
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::RemoveAltimeter()
{
	if (ms_Altimeter.m_asGaugeMc.IsDefined())
	{
		ms_Altimeter.m_asGaugeMc.Invoke("removeMovieClip");
		ms_Altimeter.m_asGaugeMc.SetUndefined();
	}

	if (ms_Altimeter.m_asRollMc.IsDefined())
	{
		ms_Altimeter.m_asRollMc.Invoke("removeMovieClip");
		ms_Altimeter.m_asRollMc.SetUndefined();
	}

	if (ms_Altimeter.m_asContainerMc.IsDefined())
	{
		ms_Altimeter.m_asContainerMc.Invoke("removeMovieClip");
		ms_Altimeter.m_asContainerMc.SetUndefined();
	}
}

void CMiniMap_RenderThread::CreateFreeway()
{
	const char *pFreewayComponentsString = "roads_overlay";

	GFxValue asFreewayLayerContainer;
	if (ms_asBaseOverlay3D[MINIMAP_LAYER_BACKGROUND].CreateEmptyMovieClip(&asFreewayLayerContainer, "asFreewayLayer", MOVIE_DEPTH_FREEWAY))
	{
		if (asFreewayLayerContainer.GFxValue::AttachMovie(&ms_asFreewayMovieClip, pFreewayComponentsString, pFreewayComponentsString, 1))
		{
			GFxValue::DisplayInfo freewayDisplayInfo;
			freewayDisplayInfo.SetVisible(true);
			freewayDisplayInfo.SetPosition(0.0f, 0.0f);
			freewayDisplayInfo.SetScale(100.0f, 100.0f);	//	MINIMAP_INITIAL_SCALE / 100.0f, MINIMAP_INITIAL_SCALE / 100.0f);
			ms_asFreewayMovieClip.SetDisplayInfo(freewayDisplayInfo);
		}
	}
}

void CMiniMap_RenderThread::RemoveFreeway()
{
	if (ms_asFreewayMovieClip.IsDefined())
	{
		ms_asFreewayMovieClip.Invoke("removeMovieClip");
		ms_asFreewayMovieClip.SetUndefined();
	}

	GFxValue asFreewayLayerContainer;
	if (ms_asBaseOverlay3D[MINIMAP_LAYER_BACKGROUND].GFxValue::GetMember("asFreewayLayer", &asFreewayLayerContainer))
	{
		asFreewayLayerContainer.Invoke("removeMovieClip");
		asFreewayLayerContainer.SetUndefined();
	}
}

void CMiniMap_RenderThread::UpdateFreeway()
{
	GFxValue::DisplayInfo freewayDisplayInfo;
#if __BANK
	freewayDisplayInfo.SetPosition(CMiniMap::sm_Tunables.Overlay.vPos.x, CMiniMap::sm_Tunables.Overlay.vPos.y);
	freewayDisplayInfo.SetScale(CMiniMap::sm_Tunables.Overlay.vScale.x, CMiniMap::sm_Tunables.Overlay.vScale.y);
#endif // __BANK

	if (ms_MiniMapRenderState.m_vCentrePosition.z > CMiniMap::sm_Tunables.Overlay.fDisplayZ)
	{
		freewayDisplayInfo.SetVisible(true);
	}
	else
	{
		freewayDisplayInfo.SetVisible(false);
	}

	ms_asFreewayMovieClip.SetDisplayInfo(freewayDisplayInfo);
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::CreateCrosshair
// PURPOSE: creates a crosshair and hides it (it gets set to visible later when needed)
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::CreateCrosshair()
{
	GFxValue asCrosshairMc;
	ms_asRootContainer[MINIMAP_ROOT_LAYER_UNMASKED].GFxValue::AttachMovie(&asCrosshairMc, "crosshair", "asCrosshair", MOVIE_DEPTH_CROSSHAIR);

	GFxValue::DisplayInfo info;
	info.SetVisible(false);
	asCrosshairMc.SetDisplayInfo(info);
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::CreateMapContainers
// PURPOSE: creates a container for the vector map
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::CreateMapContainers()
{
	if (ms_asRootContainer[MINIMAP_ROOT_MAP_TRANSPARENT].CreateEmptyMovieClip(&ms_asBaseLayerContainer[MINIMAP_LAYER_BACKGROUND], "asMapContainer", MOVIE_DEPTH_MAP))
	{
		ms_asBaseLayerContainer[MINIMAP_LAYER_BACKGROUND].GFxValue::AttachMovie(&ms_asMapContainerMc, "mapContainer", "asMapContainer", 1);
	}

	if (ms_asRootContainer[MINIMAP_ROOT_LAYER_MASKED].CreateEmptyMovieClip(&ms_asBaseLayerContainer[MINIMAP_LAYER_FOREGROUND], "asBlipLayerContainer", MOVIE_DEPTH_BLIPS_3D))
	{
		ms_asBaseLayerContainer[MINIMAP_LAYER_FOREGROUND].GFxValue::AttachMovie(&ms_asBlipContainerMc, "blipContainer", "asBlipContainer", 1);
	}

	if (ms_asMapContainerMc.CreateEmptyMovieClip(&ms_asBaseOverlay3D[MINIMAP_LAYER_BACKGROUND], "asBackgroundOverlay3D", MOVIE_DEPTH_GPS)) // 
	{
		GFxValue asInteriorLayer, asGolfCourseLayer;
		ms_asBaseOverlay3D[MINIMAP_LAYER_BACKGROUND].CreateEmptyMovieClip(&asInteriorLayer, "asInteriorLayer", MOVIE_DEPTH_INTERIOR);
		ms_asBaseOverlay3D[MINIMAP_LAYER_BACKGROUND].CreateEmptyMovieClip(&asGolfCourseLayer, "asGolfCourseLayer", MOVIE_DEPTH_GOLF_COURSE);
	}

	if (ms_asBlipContainerMc.CreateEmptyMovieClip(&ms_asBaseOverlay3D[MINIMAP_LAYER_FOREGROUND], "asForegroundOverlay3D", MOVIE_DEPTH_GPS)) // 
	{
		for (s32 iLayerCount = 0; iLayerCount < GPS_NUM_SLOTS; iLayerCount++)
		{
			char cLayerName[20];
			formatf(cLayerName, "asGpsLayer%d", iLayerCount, NELEM(cLayerName));
			ms_asBaseOverlay3D[MINIMAP_LAYER_FOREGROUND].CreateEmptyMovieClip(&ms_asGpsLayer[iLayerCount], cLayerName, MOVIE_DEPTH_GPS+iLayerCount);

			// init the route layers as invisible:
			GFxValue::DisplayInfo info;
			info.SetVisible(false);
			ms_asGpsLayer[iLayerCount].SetDisplayInfo(info);
		}

		GFxValue asRadiusBlipLayer;
		ms_asBaseOverlay3D[MINIMAP_LAYER_FOREGROUND].CreateEmptyMovieClip(&asRadiusBlipLayer, "asRadiusBlipLayer", MOVIE_DEPTH_RADIUS);

		GFxValue asSonarBlipLayer;
		ms_asBaseOverlay3D[MINIMAP_LAYER_FOREGROUND].CreateEmptyMovieClip(&asSonarBlipLayer, "asSonarBlipLayer", MOVIE_DEPTH_STEALTH);
	}


	if (ms_asBaseLayerContainer[MINIMAP_LAYER_BACKGROUND].GFxValue::HasMember("asMapContainer") &&
		ms_asMapContainerMc.GFxValue::HasMember("map"))
	{
		ms_asMapContainerMc.GFxValue::GetMember("map", &ms_asMapObject);
		ms_asBlipContainerMc.GFxValue::GetMember("blipLayer", &ms_asBlipLayer3D);

		GFxValue::DisplayInfo info;

		info.SetScale(MINIMAP_INITIAL_SCALE, MINIMAP_INITIAL_SCALE);

		ms_asMapObject.SetDisplayInfo(info);

		if (ms_asMapContainerMc.GFxValue::HasMember("sea"))
		{
			GFxValue seaMap;
			ms_asMapContainerMc.GFxValue::GetMember("sea", &seaMap);

			info.SetVisible(false);
			seaMap.SetDisplayInfo(info);
		}
		g_RenderMinimapSea = true;
	}
	else
	{
		uiAssertf(0, "MiniMap: One or more required movieclips are not set up in ActionScript");
	}

	GFxValue::DisplayInfo info;

	info.SetPosition((ms_vStageSize.x * 0.5f), (ms_vStageSize.y * 0.5f));
	ms_asBaseLayerContainer[MINIMAP_LAYER_BACKGROUND].SetDisplayInfo(info);
	ms_asBaseLayerContainer[MINIMAP_LAYER_FOREGROUND].SetDisplayInfo(info);

	info.Clear();  // default the initial zoom value to exterior on-foot
	info.SetScale(CMiniMap::sm_Tunables.Camera.fExteriorFootZoom, CMiniMap::sm_Tunables.Camera.fExteriorFootZoom);
	ms_asMapContainerMc.SetDisplayInfo(info);
	ms_asBlipContainerMc.SetDisplayInfo(info);

	// blip 2d layer:
	GFxValue asBlipLayer2D;
	ms_asRootContainer[MINIMAP_ROOT_LAYER_UNMASKED].CreateEmptyMovieClip(&asBlipLayer2D, "asBlipLayer2D", MOVIE_DEPTH_BLIPS_2D);

	GFxValue asMapObjectBackground;
	ms_asMapContainerMc.GFxValue::GetMember("map", &asMapObjectBackground);

	if (uiVerifyf(asMapObjectBackground.GFxValue::HasMember("main_map"), "MiniMap: 'main_map' does not exist in 'map'"))
	{
		GFxValue mainMap;
		asMapObjectBackground.GFxValue::GetMember("main_map", &mainMap);

		GFxValue asMapLayerGfx;
		mainMap.GFxValue::GetMember("map_layer", &asMapLayerGfx);

		asMapLayerGfx.CreateEmptyMovieClip(&ms_asTileLayerContainer, "asTileLayer", MOVIE_DEPTH_TILES);
		asMapLayerGfx.CreateEmptyMovieClip(&ms_asBitmapLayerContainer, "asBitmapLayer", MOVIE_DEPTH_BITMAP);

		//
		// we need to scale the tiles down - not 100% sure why, it used to be done in Actionscript, triggered by MINIMAP_INITIALISED
		// which we no longer call, so I do it here myself.   It may have something to do with AA fixes Eddie may of done but he
		// isnt here right now to ask...
		//
		GFxValue::DisplayInfo CurrInfo;

		// For drawable minimap:
		CurrInfo.SetVisible(false);

		ms_asTileLayerContainer.SetDisplayInfo(CurrInfo);
		ms_asBitmapLayerContainer.SetDisplayInfo(CurrInfo);

		if (CScaleformMgr::BeginMethod(ms_iMovieId[MINIMAP_MOVIE_BACKGROUND], SF_BASE_CLASS_MINIMAP, "REGISTER_MAP_LAYER"))
		{
			CScaleformMgr::AddParamGfxValue(ms_asBitmapLayerContainer);
			CScaleformMgr::EndMethod(true);  // this is instant as we are initialising the minimap here
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::RemoveMapContainers
// PURPOSE: removes a container for the vector map
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::RemoveMapContainers()
{
	GFxValue asInteriorLayer, asGolfCourseLayer, asRadiusBlipLayer, asSonarBlipLayer, asBlipLayer2D;

	if (ms_asBaseOverlay3D[MINIMAP_LAYER_BACKGROUND].GFxValue::GetMember("asInteriorLayer", &asInteriorLayer))
	{
		asInteriorLayer.Invoke("removeMovieClip");
	}

	if (ms_asBaseOverlay3D[MINIMAP_LAYER_BACKGROUND].GFxValue::GetMember("asGolfCourseLayer", &asGolfCourseLayer))
	{
		asGolfCourseLayer.Invoke("removeMovieClip");
	}

	if (ms_asBaseOverlay3D[MINIMAP_LAYER_FOREGROUND].GFxValue::GetMember("asRadiusBlipLayer", &asRadiusBlipLayer))
	{
		asRadiusBlipLayer.Invoke("removeMovieClip");
	}

	if (ms_asBaseOverlay3D[MINIMAP_LAYER_FOREGROUND].GFxValue::GetMember("asSonarBlipLayer", &asSonarBlipLayer))
	{
		asSonarBlipLayer.Invoke("removeMovieClip");
	}

	if (ms_asRootContainer[MINIMAP_ROOT_LAYER_UNMASKED].GFxValue::GetMember("asBlipLayer2D", &asBlipLayer2D))
	{
		asBlipLayer2D.Invoke("removeMovieClip");
	}

	for (s32 iLayerCount = 0; iLayerCount < GPS_NUM_SLOTS; iLayerCount++)
	{
		if (ms_asGpsLayer[iLayerCount].IsDefined())
		{
			ms_asGpsLayer[iLayerCount].Invoke("removeMovieClip");
			ms_asGpsLayer[iLayerCount].SetUndefined();
		}
	}

	if (ms_asMapObject.IsDefined())
	{
		ms_asMapObject.Invoke("removeMovieClip");
		ms_asMapObject.SetUndefined();
	}

	if (ms_asBlipLayer3D.IsDefined())
	{
		ms_asBlipLayer3D.Invoke("removeMovieClip");
		ms_asBlipLayer3D.SetUndefined();
	}

	if (ms_asBlipContainerMc.IsDefined())
	{
		ms_asBlipContainerMc.Invoke("removeMovieClip");
		ms_asBlipContainerMc.SetUndefined();
	}

	if (ms_asMapContainerMc.IsDefined())
	{
		ms_asMapContainerMc.Invoke("removeMovieClip");
		ms_asMapContainerMc.SetUndefined();
	}

	if (ms_asTileLayerContainer.IsDefined())
	{
		ms_asTileLayerContainer.Invoke("removeMovieClip");
		ms_asTileLayerContainer.SetUndefined();
	}

	if (ms_asBitmapLayerContainer.IsDefined())
	{
		ms_asBitmapLayerContainer.Invoke("removeMovieClip");
		ms_asBitmapLayerContainer.SetUndefined();
	}

	if (ms_asBaseOverlay3D[MINIMAP_LAYER_FOREGROUND].IsDefined())
	{
		ms_asBaseOverlay3D[MINIMAP_LAYER_FOREGROUND].Invoke("removeMovieClip");
		ms_asBaseOverlay3D[MINIMAP_LAYER_FOREGROUND].SetUndefined();
	}

	if (ms_asBaseOverlay3D[MINIMAP_LAYER_BACKGROUND].IsDefined())
	{
		ms_asBaseOverlay3D[MINIMAP_LAYER_BACKGROUND].Invoke("removeMovieClip");
		ms_asBaseOverlay3D[MINIMAP_LAYER_BACKGROUND].SetUndefined();
	}

	if (ms_asBaseLayerContainer[MINIMAP_LAYER_BACKGROUND].IsDefined())
	{
		ms_asBaseLayerContainer[MINIMAP_LAYER_BACKGROUND].Invoke("removeMovieClip");
		ms_asBaseLayerContainer[MINIMAP_LAYER_BACKGROUND].SetUndefined();
	}

	if (ms_asBaseLayerContainer[MINIMAP_LAYER_FOREGROUND].IsDefined())
	{
		ms_asBaseLayerContainer[MINIMAP_LAYER_FOREGROUND].Invoke("removeMovieClip");
		ms_asBaseLayerContainer[MINIMAP_LAYER_FOREGROUND].SetUndefined();
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::SetMaskOnLayer
// PURPOSE: sets or unsets the mask on a layer
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::SetMaskOnLayer(bool bSet, s32 iLayer)
{
	GFxValue args[1];

	// either way, delete what's here
	GFxValue asMask;

	if (ms_asRootContainer[iLayer].HasMember("asMask"))
	{
		ms_asRootContainer[iLayer].GetMember("asMask", &asMask);

		args[0].SetNull();  // set argument to AS as NULL
		ms_asRootContainer[iLayer].Invoke("setMask", NULL, args, 1);
		asMask.Invoke("removeMovieClip");  // remove the mask movie
		asMask.SetUndefined();
	}

	if (bSet)
	{
		// create the mask:
		GFxValue asMaskMc;

		ms_asRootContainer[iLayer].CreateEmptyMovieClip(&asMask, "asMask", MOVIE_DEPTH_MASK);

		GFxValue::DisplayInfo dispInfo;

		// we're drawing the mask directly in SF, to make it tons easier to tune.
		// the way this works, in theory, is:
		// 1. Determine the aspect ratio the minimap is in natively
		// 2. adjust away the offsets of the on-screen map (ie, the tunings are also in screen space)
		// 3. scale the dimensions from 'screen space' to 'map space' (using the ratio from step 1)
		// 4. draw the mask and set it on the appropriate layers
		
		// because the map is drawn stretched and scaled from its original size, this series of convolutions is required to get it look right
		// at all resolutions

		Vector2 maskPos( ms_MiniMapRenderState.m_vCurrentMiniMapMaskPosition );
		Vector2 maskSize( ms_MiniMapRenderState.m_vCurrentMiniMapMaskSize );
		float fMapRatio = ms_vOriginalStageSize.x / ms_vOriginalStageSize.y;
		const float fScaler = fMapRatio/CHudTools::GetAspectRatio();

		maskPos.x -= ms_MiniMapRenderState.m_vCurrentMiniMapPosition.x; maskPos.y -= ms_MiniMapRenderState.m_vCurrentMiniMapPosition.y;
		maskSize.x /= ms_MiniMapRenderState.m_vCurrentMiniMapSize.x; maskSize.y /= ms_MiniMapRenderState.m_vCurrentMiniMapSize.y;
		maskPos.x  *= ms_vOriginalStageSize.x/fScaler;	maskPos.y  *= ms_vOriginalStageSize.y;
		maskSize.x *= ms_vOriginalStageSize.x/fScaler;	maskSize.y *= ms_vOriginalStageSize.y;

		GFxValue args[2];

		u8 alpha = BANK_ONLY( sm_bDebugMaskDisplay ? sm_uDebugMaskAlpha : ) 100;

#define INVOKE(func, val1, val2); { args[0].SetNumber(val1); args[1].SetNumber(val2); asMask.Invoke(func, NULL, args,2); }
		INVOKE("beginFill", 0x00FFFFFF, alpha); // white, full alpha
		INVOKE("moveTo", maskPos.x,				maskPos.y);
		INVOKE("lineTo", maskPos.x+maskSize.x,	maskPos.y);
		INVOKE("lineTo", maskPos.x+maskSize.x,	maskPos.y+maskSize.y);
		INVOKE("lineTo", maskPos.x,				maskPos.y+maskSize.y);
		INVOKE("lineTo", maskPos.x,				maskPos.y);
		asMask.Invoke("endFill");

#if __BANK
		if( sm_bDebugMaskDisplay )
		{
			static float thickness = 1;
			static int color = 0x00FF00FF; // a nice purple
			static int alpha = 50;
			GFxValue args3[3]; 
			args3[0].SetNumber(thickness);
			args3[1].SetNumber(color);
			args3[2].SetNumber(alpha); // alpha
			asMask.Invoke("lineStyle", NULL, args3, 3);
			INVOKE("moveTo", maskPos.x,				maskPos.y);
			INVOKE("lineTo", maskPos.x+maskSize.x,	maskPos.y);
			INVOKE("lineTo", maskPos.x+maskSize.x,	maskPos.y+maskSize.y);
			INVOKE("lineTo", maskPos.x,				maskPos.y+maskSize.y);
			INVOKE("lineTo", maskPos.x,				maskPos.y);
		}
		else
#endif
		{
			args[0] = asMask;  // set the arguments to AS at the mask
			ms_asRootContainer[iLayer].Invoke("setMask", NULL, args, 1);
		}
	}
	else
	{
#if !__FINAL
		if(CMiniMap_Common::OutputDebugTransitions())
		{
			uiDisplayf("MINIMAP_TRANSITION: CMiniMap_RenderThread::SetMaskOnLayer.  turned off mask(RT)");
		}
#endif
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::SetMask
// PURPOSE: create and enable or disable a mask on the minimap
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::SetMask(bool bSet)
{
	for (s32 i = 0; i < MAX_MINIMAP_MOVIE_LAYERS; i++)
	{
		if (ms_iMovieId[i] == -1)
			continue;

		//s32 iLayer;

		if (i == MINIMAP_MOVIE_FOREGROUND)
		{
			SetMaskOnLayer(bSet, MINIMAP_ROOT_LAYER_MASKED);

			if (bSet)
			{
				if (ms_MiniMapRenderState.m_bIsInPauseMap && !CPauseMenu::IsNavigatingContent())
 				{
 					SetMaskOnLayer(true, MINIMAP_ROOT_LAYER_UNMASKED);
 				}
				else
				{
					SetMaskOnLayer(false, MINIMAP_ROOT_LAYER_UNMASKED);
				}
			}
			else
			{
				SetMaskOnLayer(false, MINIMAP_ROOT_LAYER_UNMASKED);
			}
		}
		else
		{
			SetMaskOnLayer(bSet, MINIMAP_ROOT_MAP_TRANSPARENT);
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::SetupContainers()
// PURPOSE: sets up each ActionScript container
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::SetupContainers()
{
	if (ms_iMovieId[MINIMAP_MOVIE_BACKGROUND] == -1 || ms_iMovieId[MINIMAP_MOVIE_FOREGROUND] == -1)
	{
		uiAssertf(0, "MiniMap: Minimap.gfx is not active yet - Minimap is unlikely to continue!");
		return;
	}
	
	SetLocalStageSize();
	CreateRoot(MINIMAP_MOVIE_BACKGROUND);
	CreateRoot(MINIMAP_MOVIE_FOREGROUND);
	CreateHealthArmourContainer();
	CreateMapContainers();
	CreateAltimeter();
	CreateCrosshair();
//	CreateRunway();
	CreateFreeway();
	CreateTerritoryContainer();

#if __DEV
	if (!VerifyComponents())
	{
		uiAssertf(0, "MiniMap: Minimap.gfx is missing some vital components.  Minimap is likely to be unstable!");
	}
#endif // _DEV

	SetMask(true);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::RemoveContainers()
// PURPOSE: removes the containers setup inside SetupContainers
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::RemoveContainers()
{
	if (ms_iMovieId[MINIMAP_MOVIE_BACKGROUND] == -1 || ms_iMovieId[MINIMAP_MOVIE_FOREGROUND] == -1)
	{
		uiAssertf(0, "MiniMap: Minimap.gfx is not active - Minimap is unlikely to continue!");
		return;
	}

	// remove in reverse order of that we created them
	SetMask(false);

	RemoveTerritoryContainer();

	RemoveFreeway();
	RemoveAltimeter();

	RemoveMapContainers();

	RemoveHealthArmourContainer();

	RemoveRoot(MINIMAP_MOVIE_BACKGROUND);
	RemoveRoot(MINIMAP_MOVIE_FOREGROUND);
}



#if __DEV
/////////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::VerifyComponents()
// PURPOSE:	this method checks some components on the movie at initialisation that we
//			assume are there from now on to avoid having to check every time.  This
//			will only fail if there is a problem on the Actionscript side.
/////////////////////////////////////////////////////////////////////////////////////////
bool CMiniMap_RenderThread::VerifyComponents()
{
	GFxValue asMovieObject = CScaleformMgr::GetActionScriptObjectFromRoot(ms_iMovieId[MINIMAP_MOVIE_FOREGROUND]);

	if ( ms_asMapObject.HasMember("main_map") )
	{
		return true;
	}

	return false;
}
#endif // __DEV




/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::AddSonarBlipToStage
// PURPOSE: creates a self-removing "sonar" stealth blip.   We do not need to keep
//			track of these as these blips self-remove via ActionScript
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::AddSonarBlipToStage(sSonarBlipStructRT *pSonarBlip)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		sfAssertf(0, "CMiniMap_RenderThread::AddSonarBlipToStage can only be called on the RenderThread!");
		return;
	}

	// if sonar layer is currently invisible, do not add any new sonar blips
	GFxValue asSonarBlipLayer;
	if (!ms_asBaseOverlay3D[MINIMAP_LAYER_FOREGROUND].GetMember("asSonarBlipLayer", &asSonarBlipLayer))
	{
		return;
	}

	GFxValue::DisplayInfo SonarLayerDisplayInfo;
	asSonarBlipLayer.GetDisplayInfo(&SonarLayerDisplayInfo);
	if (!SonarLayerDisplayInfo.GetVisible())
	{
		return;
	}

	char cRefName[MAX_BLIP_NAME_SIZE+10];
	formatf(cRefName, "stealth_blip_%d", pSonarBlip->iId, NELEM(cRefName));

	const char* cSonarBlipName = "stealth_blip_sonar_fill";

	GFxValue asSonarBlipMc;
	asSonarBlipLayer.GetMember(cRefName, &asSonarBlipMc);


	if (!asSonarBlipMc.IsUndefined())  // sonar blip exists, so just set the frame back to 1
	{
#define __BIGGEST_VISUAL_FRAME_NUM (1)
			asSonarBlipMc.GotoAndPlay(__BIGGEST_VISUAL_FRAME_NUM);
	}
	else    // sonar blip doesnt exist at all, so add it
	{
		if (pSonarBlip->iFramesAlive == 0)
		{
			asSonarBlipLayer.GFxValue::AttachMovie(&asSonarBlipMc, cSonarBlipName, cRefName, pSonarBlip->iId);

			uiDisplayf("MiniMap: SONAR_BLIP - Attaching new sonar blip %s %s %d", cSonarBlipName, cRefName, pSonarBlip->iId);
		}
	}

	// set it to the correct (latest) properties
	if (!asSonarBlipMc.IsUndefined())  // sonar blip doesnt exist at all, so add it
	{
		GFxValue::DisplayInfo info;
		info.SetPosition((pSonarBlip->vPos.x), -(pSonarBlip->vPos.y));

		if (pSonarBlip->iFramesAlive == 0)
		{
			info.SetScale(pSonarBlip->fSize, pSonarBlip->fSize);

			CRGBA blipColour = CHudColour::GetRGBA(pSonarBlip->iHudColour);
			asSonarBlipMc.SetColorTransform(blipColour);

			GFxValue::DisplayInfo dispInfo;
			dispInfo.SetAlpha((float)blipColour.GetAlphaf() * 100.0f);
			asSonarBlipMc.SetDisplayInfo(dispInfo);

			uiDisplayf("MiniMap: SONAR_BLIP - Setting as new blip - %s %s %d - pos:%0.2f,%0.2f - size:%0.2f - alpha:%0.2f", cSonarBlipName, cRefName, pSonarBlip->iId, pSonarBlip->vPos.x, pSonarBlip->vPos.y, pSonarBlip->fSize, (float)blipColour.GetAlphaf() * 100.0f);
		}

		asSonarBlipMc.SetDisplayInfo(info);
	}
}



void CMiniMap_RenderThread::SetupGolfCourseLayout()
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		sfAssertf(0, "CMiniMap_RenderThread::SetupGolfCourseLayout can only be called on the RenderThread!");
		return;
	}

	GFxValue asHealthContainer;
	if (uiVerifyf(ms_asRootContainer[MINIMAP_ROOT_LAYER_UNMASKED].GetMember("asHealthContainer", &asHealthContainer), "CMiniMap_RenderThread: 'asHealthContainer' cannot be found when switching to golfcoursemap"))
	{
		if (uiVerifyf(asHealthContainer.IsDisplayObject(), "CMiniMap_RenderThread: 'asHealthContainer' is not a display object when switching to golfcourse"))
		{
			GFxValue::DisplayInfo healthContainerDisplayInfo;
			healthContainerDisplayInfo.SetVisible(false);
			asHealthContainer.SetDisplayInfo(healthContainerDisplayInfo);
		}
	}

	UpdateSea(false);

	GFxValue::DisplayInfo info;
	info.SetVisible(false);
	ms_asMapObject.SetDisplayInfo(info);

	g_RenderMinimap = false;

	SetMask(false);

	for (s32 i = 0; i < MAX_MINIMAP_MOVIE_LAYERS; i++)
	{
		CScaleformMgr::ChangeMovieParams(ms_iMovieId[i], ms_MiniMapRenderState.m_vCurrentMiniMapPosition, ms_MiniMapRenderState.m_vCurrentMiniMapSize, GFxMovieView::SM_ExactFit);
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::UpdateGPSDisplay
// PURPOSE:	updates the minimap with any gps display
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::UpdateGPSDisplay()
{
	if (ms_iMovieId[MINIMAP_MOVIE_FOREGROUND] == -1)
		return;

	CGps::UpdateGpsOnMiniMapOnRT();
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::UpdateWithActionScriptOnRT
// PURPOSE: updates on the RenderThread as we alter ActionScript on the movie itself
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::UpdateWithActionScriptOnRT(bool bTransition, bool bClearTiles)
{
	if (!ms_MiniMapRenderState.m_bIsActive)
		return;

	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		sfAssertf(0, "CMiniMap_RenderThread::UpdateWithActionScriptOnRT can only be called on the RenderThread!");
		return;
	}

	UpdateInterior();

	UpdateMapPositionAndScale(bTransition);

	UpdateMapVisibility();

	UpdateAltimeter();

	UpdateSonarSweep();

	UpdateFreeway();

	UpdateTiles(bClearTiles);

	UpdateGolfCourse();

	UpdateGPSDisplay();

	GFxValue asCrosshairMc;
	if (ms_asRootContainer[MINIMAP_ROOT_LAYER_UNMASKED].GetMember("asCrosshair", &asCrosshairMc))
	{
		GFxValue::DisplayInfo newDisplayInfo;
		newDisplayInfo.SetVisible(ms_MiniMapRenderState.bDrawCrosshair);
		asCrosshairMc.SetDisplayInfo(newDisplayInfo);
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::UpdateStatesWithActionScriptOnRT
// PURPOSE: switches minimap state (on the RT)
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::UpdateStatesWithActionScriptOnRT()
{
	if (!ms_MiniMapRenderState.m_bIsActive)
		return;

	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		sfAssertf(0, "CMiniMap_RenderThread::UpdateStatesWithActionScriptOnRT can only be called on the RenderThread!");
		return;
	}

	switch (ms_MiniMapRenderState.m_MinimapModeState)
	{
	case MINIMAP_MODE_STATE_SETUP_FOR_MINIMAP:
		{
			//	MinimapModeState = MINIMAP_MODE_STATE_NONE;	//	Graeme - Can't clear this on the RenderThread. I'm relying on it being cleared at the end of CMiniMap::UpdateStatesOnUT()

#if !__FINAL
			if(CMiniMap_Common::OutputDebugTransitions())
				uiDisplayf("MINIMAP_TRANSITION: CMiniMap_RenderThread::UpdateStatesWithActionScriptOnRT.  Received MINIMAP_MODE_STATE_SETUP_FOR_MINIMAP(%d) before processed (RT)", ms_MiniMapRenderState.m_MinimapModeState);
#endif

			SetupForMiniMap();
			ms_MiniMapRenderState.m_MinimapModeState = MINIMAP_MODE_STATE_SETUP_FOR_MINIMAP_HAS_BEEN_PROCESSED_BY_RENDER_THREAD;
			break;
		}

	case MINIMAP_MODE_STATE_SETUP_FOR_BIGMAP:
		{
			//	MinimapModeState = MINIMAP_MODE_STATE_NONE;	//	Graeme - Can't clear this on the RenderThread. I'm relying on it being cleared at the end of CMiniMap::UpdateStatesOnUT()
#if !__FINAL
			if(CMiniMap_Common::OutputDebugTransitions())
				uiDisplayf("MINIMAP_TRANSITION: CMiniMap_RenderThread::UpdateStatesWithActionScriptOnRT.  Received MINIMAP_MODE_STATE_SETUP_FOR_BIGMAP(%d) before processed (RT)", ms_MiniMapRenderState.m_MinimapModeState);
#endif

			SetupForBigMap();
			ms_MiniMapRenderState.m_MinimapModeState = MINIMAP_MODE_STATE_SETUP_FOR_BIGMAP_HAS_BEEN_PROCESSED_BY_RENDER_THREAD;
			break;
		}

	case MINIMAP_MODE_STATE_SETUP_FOR_PAUSEMAP:
		{
			//	MinimapModeState = MINIMAP_MODE_STATE_NONE;	//	Graeme - Can't clear this on the RenderThread. I'm relying on it being cleared at the end of CMiniMap::UpdateStatesOnUT()
#if !__FINAL
			if(CMiniMap_Common::OutputDebugTransitions())
				uiDisplayf("MINIMAP_TRANSITION: CMiniMap_RenderThread::UpdateStatesWithActionScriptOnRT.  Received MINIMAP_MODE_STATE_SETUP_FOR_PAUSEMAP(%d) before processed (RT)", ms_MiniMapRenderState.m_MinimapModeState);
#endif

			SetupForPauseMenu(true);
			ms_MiniMapRenderState.m_MinimapModeState = MINIMAP_MODE_STATE_SETUP_FOR_PAUSEMAP_HAS_BEEN_PROCESSED_BY_RENDER_THREAD;
			break;
		}
	case MINIMAP_MODE_STATE_SETUP_FOR_CUSTOMMAP:
		{
#if !__FINAL
			if(CMiniMap_Common::OutputDebugTransitions())
				uiDisplayf("MINIMAP_TRANSITION: CMiniMap_RenderThread::UpdateStatesWithActionScriptOnRT.  Received MINIMAP_MODE_STATE_SETUP_FOR_CUSTOMMAP(%d) before processed (RT)", ms_MiniMapRenderState.m_MinimapModeState);
#endif
			//	MinimapModeState = MINIMAP_MODE_STATE_NONE;	//	Graeme - Can't clear this on the RenderThread. I'm relying on it being cleared at the end of CMiniMap::UpdateStatesOnUT()

			SetupForPauseMenu(false);
			ms_MiniMapRenderState.m_MinimapModeState = MINIMAP_MODE_STATE_SETUP_FOR_CUSTOMMAP_HAS_BEEN_PROCESSED_BY_RENDER_THREAD;
			break;
		}
	case MINIMAP_MODE_STATE_SETUP_FOR_GOLFMAP:
		{
#if !__FINAL
			if(CMiniMap_Common::OutputDebugTransitions())
				uiDisplayf("MINIMAP_TRANSITION: CMiniMap_RenderThread::UpdateStatesWithActionScriptOnRT.  Received MINIMAP_MODE_STATE_SETUP_FOR_GOLFMAP(%d) before processed (RT)", ms_MiniMapRenderState.m_MinimapModeState);
#endif
			//	MinimapModeState = MINIMAP_MODE_STATE_NONE;	//	Graeme - Can't clear this on the RenderThread. I'm relying on it being cleared at the end of CMiniMap::UpdateStatesOnUT()

			SetupForMiniMap();
			ms_MiniMapRenderState.m_MinimapModeState = MINIMAP_MODE_STATE_SETUP_FOR_GOLFMAP_HAS_BEEN_PROCESSED_BY_RENDER_THREAD;
			break;
		}
	default:
		{
			// nothing
			break;
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::ClearMatrix
// PURPOSE: clears the matrix3d when going from 3D to 2D view
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::ClearMatrix()
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		sfAssertf(0, "CMiniMap_RenderThread::ClearMatrix can only be called on the RenderThread!");
		return;
	}

	GFxValue nullValue;
	nullValue.SetNull();
	ms_asBaseLayerContainer[MINIMAP_LAYER_BACKGROUND].SetMember("_matrix3d", nullValue);
	ms_asBaseLayerContainer[MINIMAP_LAYER_FOREGROUND].SetMember("_matrix3d", nullValue);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::UpdateMapPositionAndScale
// PURPOSE: update position and scale of the map
/////////////////////////////////////////////////////////////////////////////////////
bool CMiniMap_RenderThread::UpdateMapPositionAndScale(bool bForceInstantRangeChange)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		sfAssertf(0, "CMiniMap_RenderThread::UpdateMapPositionAndScale can only be called on the RenderThread!");
		return true;
	}

	bool bValuesChanged = false;
	bool bInstantChanges = bForceInstantRangeChange;

	if (ms_MiniMapRenderState.m_bIsInBigMap || ms_MiniMapRenderState.m_bInsideReappearance)
	{
		bInstantChanges = true;  // always instant changes if in bigmap
	}

	const float fTimeAdjustment = fwTimer::GetSystemTimeStep();

	//
	// Tilt
	//

	GFxValue::DisplayInfo infoTilt;
	bValuesChanged = false;

	ms_asBaseLayerContainer[MINIMAP_LAYER_FOREGROUND].GetDisplayInfo(&infoTilt);

	float fNewPosY = (ms_vStageSize.y * 0.5f) + ms_MiniMapRenderState.m_fOffset;
	float fCurrentAngle = (float)infoTilt.GetXRotation();

	if (infoTilt.GetY() != fNewPosY)
	{
		infoTilt.SetY(fNewPosY);
		bValuesChanged = true;
	}

	if (ms_MiniMapRenderState.m_bIsInPauseMap || bInstantChanges)
	{
		if (infoTilt.GetXRotation() != ms_MiniMapRenderState.m_fAngle)
		{
			infoTilt.SetXRotation(ms_MiniMapRenderState.m_fAngle);

			bValuesChanged = true;
		}
	}
	else
	{
#define ANGLE_ADJUSTER (6.0f*30.0f)  // 6 degrees every update * 30 to compensate for legacy frame-based tunings

		float fCurrAngle = fCurrentAngle;

		Approach(fCurrAngle, ms_MiniMapRenderState.m_fAngle, ANGLE_ADJUSTER, fTimeAdjustment);
		infoTilt.SetXRotation(fCurrAngle);
	}

	//if (bValuesChanged)
	{
		ms_asBaseLayerContainer[MINIMAP_LAYER_BACKGROUND].SetDisplayInfo(infoTilt);
		ms_asBaseLayerContainer[MINIMAP_LAYER_FOREGROUND].SetDisplayInfo(infoTilt);

		if (infoTilt.GetXRotation() == 0.0f && fCurrentAngle != 0.0f)  // if new angle is 0.0f and old was not 0.0f then clear matrix
		{
			ClearMatrix();
		}
	}

	//
	// end of Tilt
	//

	//
	// Position
	// 
	GFxValue::DisplayInfo CurrInfo;
	ms_asMapObject.GetDisplayInfo(&CurrInfo);
	if ((float)CurrInfo.GetX() != -ms_MiniMapRenderState.m_vMiniMapPosition.x || (float)CurrInfo.GetY() != ms_MiniMapRenderState.m_vMiniMapPosition.y)
	{
		GFxValue::DisplayInfo infoPosition;

		infoPosition.SetPosition(-ms_MiniMapRenderState.m_vMiniMapPosition.x, ms_MiniMapRenderState.m_vMiniMapPosition.y);

		ms_asMapObject.SetDisplayInfo(infoPosition);
		ms_asBlipLayer3D.SetDisplayInfo(infoPosition);
		ms_asBaseOverlay3D[MINIMAP_LAYER_BACKGROUND].SetDisplayInfo(infoPosition);
		ms_asBaseOverlay3D[MINIMAP_LAYER_FOREGROUND].SetDisplayInfo(infoPosition);
	}
	//
	// End of Position
	// 


	//
	// Range and Rotation
	//
	GFxValue::DisplayInfo infoRangeRotation;

	ms_asMapContainerMc.GetDisplayInfo(&CurrInfo);

	float fCurrRange = (float)CurrInfo.GetXScale();

	bValuesChanged = false;
	if ( (fCurrRange != ms_MiniMapRenderState.m_fMiniMapRange) )
	{
		float fThisRange = ms_MiniMapRenderState.m_fMiniMapRange;

		if (( ms_MiniMapRenderState.m_bIsInPauseMap || (ms_MiniMapRenderState.m_bIsInCustomMap && !ms_MiniMapRenderState.m_bLockedToDistanceZoom)) || bInstantChanges)
		{
			// if in pausemap and we zoom, redraw the gps so we can scale it accordingly
			CGps::ForceUpdateOfGpsOnMiniMap();

			infoRangeRotation.SetScale(fThisRange, fThisRange);

			bValuesChanged = true;
		}
		else
		{
	#define RANGE_SCALER (0.07f*30.0f) // *30 to compensate for legacy frame-based tunings
			if( fCurrRange != fThisRange )
			{
				float fRangeAdjuster = (RANGE_SCALER * fCurrRange);
				Approach(fCurrRange, fThisRange, fRangeAdjuster, fTimeAdjustment);
				infoRangeRotation.SetScale(fCurrRange, fCurrRange);
				bValuesChanged = true;
			}
		}
	}

	if ((s32)CurrInfo.GetRotation() != ms_MiniMapRenderState.m_iRotation)
	{
		infoRangeRotation.SetRotation(ms_MiniMapRenderState.m_iRotation);
		bValuesChanged = true;
	}

	if (bValuesChanged)
	{
		ms_asMapContainerMc.SetDisplayInfo(infoRangeRotation);
		ms_asBlipContainerMc.SetDisplayInfo(infoRangeRotation);
	}

	//
	// end of Range and Rotation
	//

	GFxMovieView* pView = CScaleformMgr::GetMovieView(ms_iMovieId[MINIMAP_MOVIE_FOREGROUND]);
	if (pView)
	{
		pView->FindLocalToScreenMatrix(BLIP_LAYER_PATH, ms_LocalToScreen.GetRenderBuf());
	}

	ms_fBlipMapRange = fCurrRange;
	ms_fBlipMapAngle = ms_MiniMapRenderState.m_fAngle;

	return (bValuesChanged);
}


// bool CMiniMap_RenderThread::IsFlagSet(CBlipComplex *pBlip, const s32 iQueryFlag)
// {
// 	if (uiVerifyf(pBlip, "CMiniMap_RenderThread::IsFlagSet - Blip not valid!"))
// 	{
// 		return ((pBlip->iFlags & iQueryFlag) ? true : false);
// 	}
// 
// 	return false;
// }


Vector3 CMiniMap_RenderThread::GetBlipPositionValue(const CBlipComplex *pBlip)
{
	return pBlip->vPosition;
}

const Vector3& CMiniMap_RenderThread::GetBlipPositionConstRef(const CBlipComplex *pBlip)
{
	return pBlip->vPosition;
}

//	This seems to be the same as GetBlipPositionValue
Vector3 CMiniMap_RenderThread::GetBlipPositionValueOnStage(const CBlipComplex *pBlip)
{
	if (uiVerifyf(pBlip, "Blip doesnt exist!"))
	{
		return pBlip->vPosition;
	}

	return ORIGIN;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::CheckForHeight()
// PURPOSE:	Returns whether the blip is higher or lower than the player
//			(expressed as -1, 0, 1)
/////////////////////////////////////////////////////////////////////////////////////
s32 CMiniMap_RenderThread::CheckForHeight(CBlipComplex *pBlip)
{
#define METRES_TO_CHECK_ABOVE_BELOW_BLIP_EXTENDED	(10.0f)
#define METRES_TO_CHECK_ABOVE_BELOW_BLIP_STANDARD	(2.0f)
#define METRES_TO_CHECK_ABOVE_BELOW_BLIP_SHORT		(1.0f)

	if (pBlip)
	{
		float fMetresToCheck = METRES_TO_CHECK_ABOVE_BELOW_BLIP_STANDARD;

		if (CMiniMap::IsFlagSet(pBlip, BLIP_FLAG_USE_EXTENDED_HEIGHT_THRESHOLD) || ms_MiniMapRenderState.bInPlaneOrHeli)
		{
			fMetresToCheck = METRES_TO_CHECK_ABOVE_BELOW_BLIP_EXTENDED;
		}
		else if (CMiniMap::IsFlagSet(pBlip, BLIP_FLAG_USE_SHORT_HEIGHT_THRESHOLD))
		{
			fMetresToCheck = METRES_TO_CHECK_ABOVE_BELOW_BLIP_SHORT;
		}

		float fBlipHeight = GetBlipPositionValue(pBlip).z;
		float fPlayerHeight = ms_MiniMapRenderState.m_vCentrePosition.z;

		if (fPlayerHeight < fBlipHeight-fMetresToCheck)
		{
			return -1;  // lower
		}
		else if (fPlayerHeight > fBlipHeight+fMetresToCheck)
		{
			return 1;  // higher
		}
	}

	return 0;  // level
}

void CMiniMap_RenderThread::GetMiniMapCoordinatesForCone(float inputX, float inputY, float fMapRange, float &outputX, float &outputY)
{
	if ( (ms_MiniMapRenderState.m_fPlayerVehicleSpeed == 0.0f) && (fMapRange < MAP_BLIP_ROUND_THRESHOLD) )
	{
		outputX = floor(inputX+0.5f);
		outputY = floor(-inputY+0.5f);
	}
	else
	{
		outputX = inputX;
		outputY = -inputY;
	}
}


void DrawCone(GFxValue &coneValue, float fStartAzimuthAngle, float fEndAzimuthAngle, float fFocusRange)
{
	GFxValue args[2];

	Vector3 line(0.0f, fFocusRange, 0.0f);

	line.RotateZ(fStartAzimuthAngle);

	//	Draw line from ped
	args[0].SetNumber(0.0f);
	args[1].SetNumber(0.0f);
	coneValue.Invoke("moveTo", NULL, args, 2);

	args[0].SetNumber(line.x);
	args[1].SetNumber(-line.y);
	coneValue.Invoke("lineTo", NULL, args, 2);
	//	End of draw line from ped

	//	Draw arc
	float fAngle = fStartAzimuthAngle;
	static float ANGLE_RENDER_DIFF = PI/32.0f;
	while( fAngle < fEndAzimuthAngle )
	{
		float fNextAngle = Min(fAngle + ANGLE_RENDER_DIFF, fEndAzimuthAngle);

		Vector3 line2(0.0f, fFocusRange, 0.0f);
		line2.RotateZ(fNextAngle);

		args[0].SetNumber(line2.x);
		args[1].SetNumber(-line2.y);
		coneValue.Invoke("lineTo", NULL, args, 2);

		fAngle = fNextAngle;
	}

	//	Draw line back from end of cone to origin
	args[0].SetNumber(0.0f);
	args[1].SetNumber(0.0f);
	coneValue.Invoke("lineTo", NULL, args, 2);
}

void CMiniMap_RenderThread::DrawConeForBlip(GFxValue &coneValue, CBlipCone &blipCone)
{
	GFxValue::DisplayInfo infoForNewlyCreatedCone;

	GFxValue args[5];

	float aAsimuthAngles[MAX_CONE_ANGLES] = { blipCone.GetVisualFieldMinAzimuthAngle(),	-blipCone.GetGazeAngle(), 
		blipCone.GetGazeAngle(), blipCone.GetVisualFieldMaxAzimuthAngle() };

	// Calculate the peripheral and focus ranges to render
	//	float fPeripheralRange = blipCone.GetPeripheralRange();
	float fFocusRange = blipCone.GetFocusRange();


	args[0].SetNumber(0);	//	thickness - 0=thinnest, 255=thickest

	args[3].SetBoolean(false);	//	pixelHinting
	args[4].SetString("normal");		//	noScale - "normal" is the default, try "none"


#if __BANK
	if (CMiniMap::GetDrawActualRangeCone())
	{
//	Set colour and alpha of debug cone
		args[1].SetNumber(0xff0000);
		args[2].SetNumber(100);

		coneValue.Invoke("lineStyle", NULL, args, 5);

//	Draw debug cone. This shows the actual perception range i.e. before FocusRangeMultiplier is applied
		DrawCone(coneValue, aAsimuthAngles[1], aAsimuthAngles[2], fFocusRange);
	}
#endif	//	__BANK


//	Set colour and alpha of real cone
	CRGBA color = CHudColour::GetRGBA(blipCone.GetColor());
	s32 fillColour = color.GetRed() << 16;
	fillColour |= color.GetGreen() << 8;
	fillColour |= color.GetBlue();

	s32 coneAlpha = CMiniMap_Common::ALPHA_OF_POLICE_PERCEPTION_CONES;

#if __BANK
	if (CMiniMap::GetOverrideConeColour())
	{	//	This isn't really thread-safe. Hopefully, it doesn't matter too much since it's debug code.
		fillColour = CMiniMap::GetConeColourRed() << 16;
		fillColour |= CMiniMap::GetConeColourGreen() << 8;
		fillColour |= CMiniMap::GetConeColourBlue();

		coneAlpha = CMiniMap::GetConeColourAlpha();
	}
#endif	//	__BANK

	args[1].SetNumber(fillColour);
	args[2].SetNumber(coneAlpha);

	coneValue.Invoke("lineStyle", NULL, args, 5);

#if __BANK
	fFocusRange *= CMiniMap::GetConeFocusRangeMultiplier();
#else
	fFocusRange *= 1.05f;
#endif	//	__BANK

//	Graeme - I'm not drawing the peripheral cones here - maybe I'll need to add them back in later.
//	In the past when I was drawing them, they were so small that I couldn't actually see them on the radar.
	args[0].SetNumber(fillColour);
	args[1].SetNumber(coneAlpha);
	coneValue.Invoke("beginFill", NULL, args, 2);

	DrawCone(coneValue, aAsimuthAngles[1], aAsimuthAngles[2], fFocusRange);

//	float fStartAzimuthAngle = aAsimuthAngles[1];
//	float fEndAzimuthAngle = aAsimuthAngles[2];

	coneValue.Invoke("endFill", NULL, args, 0);

	infoForNewlyCreatedCone.SetVisible(true);
	coneValue.SetDisplayInfo(infoForNewlyCreatedCone);
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::AddOrUpdateConeToBlipOnStage
// PURPOSE: adds a directional code to the blip on the stage
//			idea is to mimic what CPedPerception::DrawVisualField() does here
//			but in Scaleform
/////////////////////////////////////////////////////////////////////////////////////
bool CMiniMap_RenderThread::AddOrUpdateConeToBlipOnStage(CBlipCone &blipCone)
{
	if (ms_iMovieId[MINIMAP_MOVIE_FOREGROUND] == -1)
		return false;

	GFxValue::DisplayInfo currentDisplayInfoMc;
	ms_asMapContainerMc.GetDisplayInfo(&currentDisplayInfoMc);
	float fMapRange = (float)currentDisplayInfoMc.GetXScale();

	GFxValue coneValue;
	bool bConeNeedsToBeDrawn = false;

	char cRefName[MAX_BLIP_NAME_SIZE];
	s32 iActualId = blipCone.GetActualId();
	formatf(cRefName, "cone_%d", iActualId, NELEM(cRefName));

	bool bHasConeAttached = ms_asBlipLayer3D.GFxValue::HasMember(cRefName);

	if (!bHasConeAttached)
	{
		ms_asBlipLayer3D.CreateEmptyMovieClip(&coneValue, cRefName, iActualId);

		if (!coneValue.IsUndefined())
		{
			sConesAttachedToBlips dataForNewCone;
			dataForNewCone.m_iActualIdOfBlip = iActualId;
			dataForNewCone.m_bConeShouldBeRemovedFromStage = false;
			ms_ConesAttachedToBlips.PushAndGrow(dataForNewCone);

			bConeNeedsToBeDrawn = true;

			uiDebugf2("[PEDCONES] Creating cone for Blip Actual %i", iActualId);
		}
		else
		{
			uiErrorf("[PEDCONES] Failed to create cone for Blip Actual %i (no empty movie clip)", iActualId);
		}
	}
	else
	{
		ms_asBlipLayer3D.GFxValue::GetMember(cRefName, &coneValue);

		const s32 arraySize = ms_ConesAttachedToBlips.GetCount();
		s32 coneLoop = 0;
		bool bFound = false;
		while ( (coneLoop < arraySize) && !bFound)
		{
			if (ms_ConesAttachedToBlips[coneLoop].m_iActualIdOfBlip == iActualId)
			{
				ms_ConesAttachedToBlips[coneLoop].m_bConeShouldBeRemovedFromStage = false;
				bFound = true;
			}
			coneLoop++;
		}
		uiAssertf(bFound, "CMiniMap_RenderThread::AddOrUpdateConeToBlipOnStage - didn't find cone with ActualId of %d in the ms_ConesAttachedToBlips array", iActualId);

		if (blipCone.GetForceRedraw())
		{
			if (uiVerifyf(coneValue.IsDisplayObject(), "CMiniMap_RenderThread::AddOrUpdateConeToBlipOnStage - cone is not a display object"))
			{
				coneValue.Invoke("clear");
			}
			bConeNeedsToBeDrawn = true;
		}
	}

	if (!coneValue.IsUndefined())
	{
		if (bConeNeedsToBeDrawn)
		{
			DrawConeForBlip(coneValue, blipCone);
			uiDebugf2("[PEDCONES] Drawing a ped cone for Blip %i with Alpha %i, Focus Range %f, Gaze Angle %f, Pos <<%.2f, %.2f>>"
				, iActualId, blipCone.GetAlpha(), blipCone.GetFocusRange(), blipCone.GetGazeAngle(), blipCone.GetPosX(), blipCone.GetPosY());
		}

		GFxValue::DisplayInfo info;

		// ensure the cone is positioned at the coords of the blip but on the 3d layer

		float fPositionOnMiniMapX = 0.0f;
		float fPositionOnMiniMapY = 0.0f;
		GetMiniMapCoordinatesForCone(blipCone.GetPosX(), blipCone.GetPosY(), fMapRange, fPositionOnMiniMapX, fPositionOnMiniMapY);

		info.SetPosition(fPositionOnMiniMapX, fPositionOnMiniMapY);

		float fBlipRotation = -(blipCone.GetRotation() * RtoD);  // get blip rotation

		info.SetRotation(fBlipRotation);
		info.SetAlpha(100.0f * (float)blipCone.GetAlpha()/255.0f);

		coneValue.SetDisplayInfo(info);

		return true;
	}

	return false;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::UpdateVisualDataSettings
// PURPOSE: Load visual settings tune data.
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::UpdateVisualDataSettings()
{
	Assertf(g_visualSettings.GetIsLoaded(), "visual settings aren't loaded");
	ms_MiniMapAlpha = g_visualSettings.Get("UI.minimap.alpha", 1.0f);
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::RemoveConeFromBlipOnStage
// PURPOSE: removes the cone
/////////////////////////////////////////////////////////////////////////////////////
bool CMiniMap_RenderThread::RemoveConeFromBlipOnStage(s32 iActualIdOfBlip)
{
	if (ms_iMovieId[MINIMAP_MOVIE_FOREGROUND] == -1)
		return false;

	char cRefName[MAX_BLIP_NAME_SIZE+10];
	formatf(cRefName, "cone_%d", iActualIdOfBlip, NELEM(cRefName));

	bool bHasConeAttached = ms_asBlipLayer3D.GFxValue::HasMember(cRefName);

	if (bHasConeAttached)  // already got higher blip attached, so remove this 1st
	{
		GFxValue pCone;
		ms_asBlipLayer3D.GFxValue::GetMember(cRefName, &pCone);

		if (pCone.IsDisplayObject())
		{
			pCone.Invoke("removeMovieClip");

			return true;
		}
	}

	return false;
}


void CMiniMap_RenderThread::ResetBlipConeFlags()
{
	const s32 arraySize = ms_ConesAttachedToBlips.GetCount();
	for (s32 coneLoop = 0; coneLoop < arraySize; coneLoop++)
	{
		ms_ConesAttachedToBlips[coneLoop].m_bConeShouldBeRemovedFromStage = true;
	}
}

void CMiniMap_RenderThread::RemoveUnusedBlipConesFromStage()
{
	const s32 sizeOfArray = ms_ConesAttachedToBlips.GetCount();
	s32 coneLoop = sizeOfArray - 1;
	while (coneLoop >= 0)
	{
		if (ms_ConesAttachedToBlips[coneLoop].m_bConeShouldBeRemovedFromStage)
		{
			uiDebugf2("[PEDCONES] Removing unused cone from Blip Actual %i", ms_ConesAttachedToBlips[coneLoop].m_iActualIdOfBlip);
			RemoveConeFromBlipOnStage(ms_ConesAttachedToBlips[coneLoop].m_iActualIdOfBlip);
			ms_ConesAttachedToBlips.Delete(coneLoop);
		}

		coneLoop--;
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::ShouldBlipHeightIndicatorFlash
// PURPOSE: decides whether this blip height indicator should flash - bug 948610
/////////////////////////////////////////////////////////////////////////////////////
bool CMiniMap_RenderThread::ShouldBlipHeightIndicatorFlash(CBlipComplex *pBlip)
{
	if (!pBlip)
		return true;

	switch(GetBlipObjectNameId(pBlip))
	{
		// always flash health or armour (1665263)
		case RADAR_TRACE_WEAPON_HEALTH:
		case RADAR_TRACE_WEAPON_ARMOUR:
		// exceptional list continues
		// should PROBABLY be based on if the blip is a 'circle', and if it uses the override AI_BLIP instead of the normal higher/lower
		case RADAR_TRACE_HOT_PROPERTY:
		case RADAR_TRACE_KING_OF_THE_CASTLE:
		case RADAR_TRACE_DEAD_DROP:
		case RADAR_TRACE_SM_CARGO:
		case RADAR_TRACE_SM_HANGAR:
		case RADAR_TRACE_NHP_BAG:
		case RADAR_TRACE_BAT_CARGO:
		case RADAR_TRACE_TEMP_1:
		case RADAR_TRACE_BAT_ASSASSINATE:
			return true;
		default:
			break;
	}

	eBLIP_COLOURS blipColour = GetBlipColourValue(pBlip);

	bool bReturnValue = false;

	switch (blipColour)
	{
		case BLIP_COLOUR_BLUE:
		case BLIP_COLOUR_GREEN:
		case BLIP_COLOUR_YELLOW:
		case BLIP_COLOUR_RED:
		case BLIP_COLOUR_FRIENDLY:
		{
			//uiDisplayf("BLIP '%s' WILL NOT FLASH", CMiniMap::GetBlipNameValue(pBlip));

			bReturnValue = false;
			break;
		}

		default:
		{
			//uiDisplayf("BLIP '%s' WILL FLASH", CMiniMap::GetBlipNameValue(pBlip));

			bReturnValue = true;
		}
	}

	return (bReturnValue);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::ShouldBlipUseWeaponTypeArrows
// PURPOSE: should the blip be overridden with a specific type of up/down arrow
/////////////////////////////////////////////////////////////////////////////////////
bool CMiniMap_RenderThread::ShouldBlipUseWeaponTypeArrows(CBlipComplex const *pBlip)
{
	if (pBlip)
	{
		BlipLinkage const c_blipId = GetBlipObjectNameId(pBlip);

		return ((!strncmp(WEAPON_BLIP_NAME, GetBlipObjectName(pBlip), NELEM(WEAPON_BLIP_NAME)-1)) ||
				c_blipId == RADAR_TRACE_TESTOSTERONE ||
				c_blipId == RADAR_TRACE_PICKUP_BEAST ||
				c_blipId == RADAR_TRACE_PICKUP_ZONED ||
				c_blipId == RADAR_TRACE_PICKUP_RANDOM ||
				c_blipId == RADAR_TRACE_PICKUP_SLOW_TIME ||
				c_blipId == RADAR_TRACE_PICKUP_SWAP ||
				c_blipId == RADAR_TRACE_PICKUP_THERMAL ||
				c_blipId == RADAR_TRACE_PICKUP_WEED ||
				c_blipId == RADAR_TRACE_WEAPON_RAILGUN ||
				c_blipId == RADAR_TRACE_BALL ||
				c_blipId == RADAR_TRACE_HIDDEN ||
				c_blipId == RADAR_TRACE_ROCKETS ||
				c_blipId == RADAR_TRACE_BOOST ||
				c_blipId == RADAR_TRACE_PICKUP_GHOST ||
				c_blipId == RADAR_TRACE_PICKUP_ARMOURED ||
				c_blipId == RADAR_TRACE_PICKUP_ACCELERATOR ||
				c_blipId == RADAR_TRACE_PICKUP_SWAP ||
				c_blipId == RADAR_TRACE_PICKUP_DETONATOR ||
				c_blipId == RADAR_TRACE_PICKUP_BOMB ||
				c_blipId == RADAR_TRACE_PICKUP_JUMP ||
				c_blipId == RADAR_TRACE_PICKUP_DEADLINE ||
				c_blipId == RADAR_TRACE_PICKUP_REPAIR ||
				c_blipId == RADAR_TRACE_PICKUP_ROCKET_BOOST ||
				c_blipId == RADAR_TRACE_PICKUP_HOMING_ROCKET ||
				c_blipId == RADAR_TRACE_PICKUP_MACHINEGUN ||
				c_blipId == RADAR_TRACE_PICKUP_PARACHUTE ||				
				c_blipId == RADAR_TRACE_PICKUP_TIME_5 ||
				c_blipId == RADAR_TRACE_PICKUP_TIME_10 ||
				c_blipId == RADAR_TRACE_PICKUP_TIME_15 ||
				c_blipId == RADAR_TRACE_PICKUP_TIME_20 ||
				c_blipId == RADAR_TRACE_PICKUP_TIME_30 ||
				c_blipId == RADAR_TRACE_CONTRABAND	||
				c_blipId == RADAR_TRACE_PICKUP_DTB_HEALTH ||
				c_blipId == RADAR_TRACE_PICKUP_DTB_BOMB_INCREASE ||
				c_blipId == RADAR_TRACE_PICKUP_DTB_BOMB_DECREASE ||
				c_blipId == RADAR_TRACE_PICKUP_DTB_BLAST_INCREASE ||
				c_blipId == RADAR_TRACE_PICKUP_DTB_BLAST_DECREASE);
	}

	return false;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::ShouldBlipUseAiTypeArrows
// PURPOSE: should the blip be overridden with a specific type of up/down arrow
/////////////////////////////////////////////////////////////////////////////////////
bool CMiniMap_RenderThread::ShouldBlipUseAiTypeArrows(CBlipComplex const *pBlip)
{
	if (pBlip)
	{
		BlipLinkage const c_blipId = GetBlipObjectNameId(pBlip);

		return (c_blipId == RADAR_TRACE_CAPTURE_THE_FLAG ||
				c_blipId == RADAR_TRACE_CAPTURE_THE_USAFLAG ||
				c_blipId == RADAR_TRACE_LEVEL_INSIDE ||
				c_blipId == RADAR_TRACE_PLAYER_KING ||
				c_blipId == RADAR_TRACE_BOUNTY_HIT_INSIDE);  // fix for 1723248 & 1772239
	}

	return false;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::AddHigherLowerBlip
// PURPOSE: adds the higher/lower blip onto the blip that is on the stage
/////////////////////////////////////////////////////////////////////////////////////
bool CMiniMap_RenderThread::AddHigherLowerBlip(GFxValue *pBlipGfx, CBlipComplex *pBlip, s32 iHighLowBlipId)
{
	if (ms_iMovieId[MINIMAP_MOVIE_FOREGROUND] == -1)
		return false;

	if (!pBlipGfx)
		return false;

	if (!pBlipGfx->IsDisplayObject())
		return false;

	if (!pBlip)
		return false;

	GFxValue higherBlip;
	GFxValue lowerBlip;

	pBlipGfx->GFxValue::GetMember("higher_blip",&higherBlip);
	pBlipGfx->GFxValue::GetMember("lower_blip",&lowerBlip);
	GFxValue* pRecolourThis = nullptr;

	if (iHighLowBlipId == -1)  // lower
	{
		RemoveHigherLowerBlip(pBlipGfx, 1); // remove the higher one

		if (!lowerBlip.IsDisplayObject())
		{
			const char* pHigherLowerName = NULL;

			if (GetBlipObjectNameId(pBlip) == RADAR_TRACE_BOUNTY_HIT)  // new stuff doesnt require these higher/lower blips to be in the enum but means we need to use their names here
			{
				pHigherLowerName = "radar_bounty_hit_higher";
			}
			else
			{
				s32 iLowerBlipNameId = BLIP_LEVEL+iHighLowBlipId;

				if (ShouldBlipUseWeaponTypeArrows(pBlip))
 				{
 					iLowerBlipNameId = BLIP_WEAPON_HIGHER;
 				}
				else if (ShouldBlipUseAiTypeArrows(pBlip))
				{
					iLowerBlipNameId = BLIP_AI_HIGHER;
				}
				else if (!strcmp(HOOP_BLIP_NAME, GetBlipObjectName(pBlip)))
				{
					iLowerBlipNameId = BLIP_AI_HIGHER;
				}

				pHigherLowerName = CMiniMap_Common::GetBlipName(iLowerBlipNameId);
			}

			pBlipGfx->GFxValue::AttachMovie(&lowerBlip, pHigherLowerName, "lower_blip", BLIP_DEPTH_HIGHER_LOWER);
			pRecolourThis = &lowerBlip;
		}
		else if( CMiniMap::IsFlagSet(pBlip,BLIP_FLAG_VALUE_CHANGED_COLOUR) )
		{
			pRecolourThis = &lowerBlip;
		}
	}
	else if (iHighLowBlipId == 1)  // higher
	{
		RemoveHigherLowerBlip(pBlipGfx, -1); // remove the lower one

		if (!higherBlip.IsDisplayObject())
		{
			const char* pHigherLowerName = NULL;

			if (GetBlipObjectNameId(pBlip) == RADAR_TRACE_BOUNTY_HIT)  // new stuff doesnt require these higher/lower blips to be in the enum but means we need to use their names here
			{
				pHigherLowerName = "radar_bounty_hit_lower";
			}
			else
			{
				s32 iHigherBlipNameId = BLIP_LEVEL+iHighLowBlipId;

				if (ShouldBlipUseWeaponTypeArrows(pBlip))
				{
					iHigherBlipNameId = BLIP_WEAPON_LOWER;
				}
				else if (ShouldBlipUseAiTypeArrows(pBlip))
				{
					iHigherBlipNameId = BLIP_AI_LOWER;
				}
				else if (!strcmp(HOOP_BLIP_NAME, GetBlipObjectName(pBlip)))
				{
					iHigherBlipNameId = BLIP_AI_LOWER;
				}

				pHigherLowerName = CMiniMap_Common::GetBlipName(iHigherBlipNameId);
			}

			pBlipGfx->GFxValue::AttachMovie(&higherBlip, pHigherLowerName, "higher_blip", BLIP_DEPTH_HIGHER_LOWER);
			pRecolourThis = &higherBlip;
		}
		else if( CMiniMap::IsFlagSet(pBlip,BLIP_FLAG_VALUE_CHANGED_COLOUR) )
		{
			pRecolourThis = &higherBlip;
		}
	}
	else
	{
		uiAssertf(0, "CMiniMap_RenderThread: Invalid height value");
		return false;
	}

	if( pRecolourThis != nullptr )
	{
		// apply colour onto the blip whether we created it this time or we got it from before
		if (pRecolourThis->IsDisplayObject())
		{
			// since this is based on color, we should check it here too
			if (!ShouldBlipHeightIndicatorFlash(pBlip))
			{
				pRecolourThis->GotoAndStop(1);  // stop on frame 1 as we don't want to flash these (bug 948610)
			}

			CRGBA blipColour;

			if (GetBlipObjectNameId(pBlip) == RADAR_TRACE_WEAPON_HEALTH || GetBlipObjectNameId(pBlip) == RADAR_TRACE_WEAPON_ARMOUR)
			{
				// health/armour blip height indicator should be white
				blipColour = CHudColour::GetRGBA(HUD_COLOUR_WHITE);
			}
			else
			{
				// other blips should be same colour as the main blip
				blipColour = CMiniMap_Common::GetColourFromBlipSettings(GetBlipColourValue(pBlip), CMiniMap::IsFlagSet(pBlip,BLIP_FLAG_BRIGHTNESS));
			}

			pRecolourThis->SetColorTransform(blipColour);

			GFxValue::DisplayInfo dispInfo;
			s32 iMainBlipAlpha = GetBlipAlphaValue(pBlip);
			dispInfo.SetAlpha((float)((iMainBlipAlpha / 255.0f) * 100.0f));
			pRecolourThis->SetDisplayInfo(dispInfo);
		}
	}

	return true;
}




/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::RemoveHigherLowerBlip
// PURPOSE: removes the higher/lower blip
/////////////////////////////////////////////////////////////////////////////////////
bool CMiniMap_RenderThread::RemoveHigherLowerBlip(GFxValue *pBlipGfx, int iHigherOrLower /* = 0 */)
{
	if (ms_iMovieId[MINIMAP_MOVIE_FOREGROUND] == -1)
		return false;

	if (!pBlipGfx)
		return false;

	if (!pBlipGfx->IsDisplayObject())
		return false;

	// if iHigherOrLower is -1, we wanna remove ONLY the lower blip
	// if it's 1, remove ONLY the higher blip
	// otherwise, remove any (but for some reason not both...?)

	GFxValue pHeightBlip;
	if ( (iHigherOrLower == 0 || iHigherOrLower == 1) && pBlipGfx->GFxValue::GetMember("higher_blip", &pHeightBlip))
	{
		if (pHeightBlip.IsDisplayObject())
		{
			pHeightBlip.Invoke("removeMovieClip");
		}

		// some of the height blips may selectively have hidden the main blip
		// so we'll unhide it just in case
		GFxValue mainBlip;
		if( pBlipGfx->GetMember("main_blip", &mainBlip) )
		{
			GFxValue::DisplayInfo di;
			di.SetVisible(true);
			mainBlip.SetDisplayInfo(di);
		}

		return true;
	}

	if ((iHigherOrLower == 0 || iHigherOrLower == -1) && pBlipGfx->GFxValue::GetMember("lower_blip", &pHeightBlip))
	{
		if (pHeightBlip.IsDisplayObject())
		{
			pHeightBlip.Invoke("removeMovieClip");
		}

		// some of the height blips may selectively have hidden the main blip
		// so we'll unhide it just in case
		GFxValue mainBlip;
		if( pBlipGfx->GetMember("main_blip", &mainBlip) )
		{
			GFxValue::DisplayInfo di;
			di.SetVisible(true);
			mainBlip.SetDisplayInfo(di);
		}

		return true;
	}

	return false;
}

/*void CMiniMap_RenderThread::SetFlag(CBlipComplex *pBlip, const int flag)
{
	pBlip->iFlags |= flag;
}

void CMiniMap_RenderThread::UnsetFlag(CBlipComplex *pBlip, const int flag)
{
	if(IsFlagSet(pBlip, flag))
	{
		pBlip->iFlags &= ~flag;
	}
}*/



bool CMiniMap_RenderThread::GetIsInsideInterior()
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		sfAssertf(0, "CMiniMap_RenderThread::GetIsInsideInterior can only be called on the RenderThread!");
		return false;
	}

	return ms_MiniMapRenderState.bInsideInterior;
}

s32 CMiniMap_RenderThread::DetermineBlipStageDepth(CBlipComplex* pBlip)
{	
	eBLIP_PRIORITY priority = GetBlipPriorityValue(pBlip);
	if( CMiniMap::IsFlagSet(pBlip, BLIP_FLAG_HOVERED_ON_PAUSEMAP) )
		priority = BLIP_PRIORITY_ONTOP_OF_EVERYTHING; // so they will always float to the top, as they DO have the player hovering directly over them, after all

	return ( s32(priority) * MAX_NUM_BLIPS) + pBlip->m_iActualId; // fix for 1564035 (and possibly some priority issues too)
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::AddBlipToStage
// PURPOSE: creates the GFxValue object as the blip is now active on the stage
/////////////////////////////////////////////////////////////////////////////////////
bool CMiniMap_RenderThread::AddBlipToStage(CBlipComplex *pBlip)
{
	PF_FUNC(AddBlipToStage);

	if (ms_iMovieId[MINIMAP_MOVIE_FOREGROUND] == -1)
		return false;

	if (!pBlip)
		return false;

	s32 iActualId = pBlip->m_iActualId;
	if (iActualId == -1)
	{
		uiAssertf(0, "CMiniMap_RenderThread::AddBlipToStage - iActualId of blip is -1");
		return false;
	}

	const char* pBlipName = GetBlipObjectName(pBlip);

	
	s32 iStageDepth = DetermineBlipStageDepth(pBlip);

	uiAssertf(pRenderedBlipObject[iActualId] == NULL, "CMiniMap_RenderThread::AddBlipToStage - pRenderedBlipObject in slot %d should be NULL at the beginning of this function", iActualId);

	// create the actual blip:
	pRenderedBlipObject[iActualId] = rage_new GFxValue;

	GFxValue *pGfxValue = pRenderedBlipObject[iActualId];

	GFxValue asLayerForBlip;

	if(GetIsPlacedOn3dMap(pBlip))
	{
		// override area blips' 3D linkage
		// on RDR we do this for RADIUS blips too, but don't want to rock the boat here
		if( GetBlipTypeValue(pBlip) == BLIP_TYPE_AREA )
			pBlipName = CMiniMap_Common::GetBlipName(BLIP_AREA);

		if (!ms_asBaseOverlay3D[MINIMAP_LAYER_FOREGROUND].GetMember("asRadiusBlipLayer", &asLayerForBlip))
		{
			uiAssertf(0, "CMiniMap_RenderThread::AddBlipToStage - cannot find 'asRadiusBlipLayer'");
			return false;
		}
	}
	else
	{
		if( GetBlipTypeValue(pBlip) == BLIP_TYPE_AREA && CMiniMap::IsFlagSet(pBlip, BLIP_FLAG_SHORTRANGE) )
			pBlipName = CMiniMap_Common::GetBlipName(RADAR_TRACE_EDGE_POINTER);

		if (!ms_asRootContainer[MINIMAP_ROOT_LAYER_UNMASKED].GetMember("asBlipLayer2D", &asLayerForBlip))
		{
			uiAssertf(0, "CMiniMap_RenderThread::AddBlipToStage - cannot find 'asBlipLayer2D'");
			return false;
		}
	}

	if (!asLayerForBlip.IsUndefined())
	{
		char cRefName[MAX_BLIP_NAME_SIZE+10];
		formatf(cRefName, "%s_%d", pBlipName, iActualId, NELEM(cRefName));

		if (asLayerForBlip.CreateEmptyMovieClip(pGfxValue, cRefName, iStageDepth))
		{
			GFxValue asTheBlip;
			if (pGfxValue->AttachMovie(&asTheBlip, pBlipName, "main_blip", BLIP_DEPTH_MAIN_BLIP))
			{
				// re-init various flags:
				CMiniMap::SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_COLOUR);
				CMiniMap::SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_TICK);
				CMiniMap::SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_GOLD_TICK);
				CMiniMap::SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_FOR_SALE);
				CMiniMap::SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_PULSE);
				CMiniMap::SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_OUTLINE_INDICATOR);
				CMiniMap::SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_FRIEND_INDICATOR);
				CMiniMap::SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_CREW_INDICATOR);

				// re-init the number (if any)
				CMiniMap::SetFlag(pBlip,BLIP_FLAG_VALUE_SET_NUMBER);
				
				// re-init the flash:
				if (CMiniMap::IsFlagSet(pBlip,BLIP_FLAG_FLASHING))
				{
					CMiniMap::UnsetFlag(pBlip, BLIP_FLAG_FLASHING);  // unset so when we setup the flash again it starts it flashing
					CMiniMap::SetFlag(pBlip, BLIP_FLAG_VALUE_CHANGED_FLASH);
				}

		#if __BANK
				if (ms_MiniMapRenderState.m_bDisplayAllBlipNames)
				{
					CMiniMap::SetFlag(pBlip, BLIP_FLAG_VALUE_SET_LABEL);
				}
				else
				{
					if (CMiniMap::GetBlipDebugNumberValue(pBlip) != -1)
					{
						CMiniMap::SetFlag(pBlip, BLIP_FLAG_VALUE_SET_LABEL);
					}
				}
		#endif // __BANK

				uiDebugf1("CMiniMap_RenderThread: Added blip %d %s with priority %d, pos %0.2f,%0.2f onto stage at depth %d", pBlip->m_iUniqueId, GetBlipObjectName(pBlip), (s32)GetBlipPriorityValue(pBlip), GetBlipPositionValueOnStage(pBlip).x, GetBlipPositionValueOnStage(pBlip).y, iStageDepth);
				return true; 
			}
		}
	}

	return false;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::GetBlipNameValue
// PURPOSE: gets the text name value to be shown on screen
/////////////////////////////////////////////////////////////////////////////////////
const char* CMiniMap_RenderThread::GetBlipNameValue(const CBlipComplex *pBlip)
{
	return pBlip->cLocName.GetStr();
}

#if __BANK
/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::LabelBlipOnStage
// PURPOSE: calls invoke on the ActionScript object to colour a blip
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::LabelBlipOnStage(GFxValue *pBlipGfx, CBlipComplex *pBlip, bool bOnOff)
{
	uiAssertf(pBlip && pBlipGfx, "blip doesnt exist");

	if (pBlip && pBlipGfx && pBlipGfx->IsDisplayObject())
	{
		if (GetBlipObjectNameId(pBlip) != BLIP_NORTH)
		{
			s8 iBlipNumber = -1;

			iBlipNumber = CMiniMap::GetBlipDebugNumberValue(pBlip);

			if (bOnOff)
			{
				if (CScaleformMgr::BeginMethod(ms_iMovieId[MINIMAP_MOVIE_FOREGROUND], SF_BASE_CLASS_MINIMAP, "SET_BLIP_LABEL"))
				{
					CScaleformMgr::AddParamGfxValue(*pBlipGfx);

					if (iBlipNumber == -1)
					{
						CScaleformMgr::AddParamString(GetBlipNameValue(pBlip));
					}
					else
					{
						char cBlipString[5];
						formatf(cBlipString, "%d", CMiniMap::GetBlipDebugNumberValue(pBlip), NELEM(cBlipString));
						CScaleformMgr::AddParamString(cBlipString);
					}

					// Pass Scaleform the correct counter-scale for the text label it is about to create
					GFxValue::DisplayInfo blipDisplayInfo;
					pBlipGfx->GetDisplayInfo(&blipDisplayInfo);
					CScaleformMgr::AddParamFloat(GetBlipScalerValue(pBlip) / blipDisplayInfo.GetXScale() * 100);

					CScaleformMgr::EndMethod();
				}
			}
			else
			{
				if (CScaleformMgr::BeginMethod(ms_iMovieId[MINIMAP_MOVIE_FOREGROUND], SF_BASE_CLASS_MINIMAP, "REMOVE_BLIP_LABEL"))
				{
					CScaleformMgr::AddParamGfxValue(*pBlipGfx);
					CScaleformMgr::EndMethod();
				}
			}
		}
	}
	else
	{
		uiAssertf(0, "CMiniMap_RenderThread: Tried to label a blip on the stage that is no longer is a display object!");
	}
}


#endif // __BANK


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::NumberBlipOnStage
// PURPOSE: displays a number ontop of a blip on the stage
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::NumberBlipOnStage(GFxValue *pBlipGfx, CBlipComplex *pBlip)
{
	if( uiVerifyf(pBlip, "Tried to display a number on a blip using a null blip pointer.") &&
		uiVerifyf(pBlipGfx, "Tried to display a number on a blip that doesn't have a main_blip component.") && 
		uiVerifyf(pBlipGfx->IsDisplayObject(), "Tried to display a number on a blip that is no longer is a display object!"))
	{
		s8 iNumberToDisplay = CMiniMap::GetBlipNumberValue(pBlip);

		if (iNumberToDisplay == -1)
		{
			if (pBlipGfx->GFxValue::HasMember("blipTextField"))
			{
				GFxValue pNumberClip;
				pBlipGfx->GFxValue::GetMember("blipTextField", &pNumberClip);

				if (pNumberClip.IsDisplayObject())
				{
					pNumberClip.Invoke("removeMovieClip");
				}
			}

			return;
		}

		if (iNumberToDisplay >= 0)
		{
			GFxValue gfxBlipTextFieldMc;
			pBlipGfx->GFxValue::AttachMovie(&gfxBlipTextFieldMc, "blipTextField", "blipTextField", BLIP_DEPTH_NUMBER);

			if (uiVerifyf(!gfxBlipTextFieldMc.IsUndefined(), "CMiniMap_RenderThread::NumberBlipOnStage - 'blipTextField' doesnt exist in Actionscript or is undefined"))
			{
				GFxValue gfxBlipTextField;
				if (uiVerifyf(gfxBlipTextFieldMc.GFxValue::GetMember("numberTF", &gfxBlipTextField), "CMiniMap_RenderThread::NumberBlipOnStage - 'numberTF' doesnt exist in Actionscript"))
				{
					char cTextFieldString[5];
					formatf(cTextFieldString, "%d", iNumberToDisplay, NELEM(cTextFieldString));
					gfxBlipTextField.SetText(cTextFieldString);
				}
			}
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::PulseBlipOnStage
// PURPOSE: pulses a blip on the stage
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::PulseBlipOnStage(GFxValue *pBlipGfx, CBlipComplex *pBlip)
{
#define __PULSE_FRAME_NUM (2)

	if (pBlip && pBlipGfx && pBlipGfx->IsDisplayObject())
	{
		//uiDebugf1("CMiniMap_RenderThread: Pulsing blip %d %s", pBlip->m_iUniqueId, GetBlipObjectName(pBlip));

		s32 iCurrentFrame = 1;

		GFxValue currentFrameGfx;

		if (pBlipGfx->GFxValue::HasMember("_currentframe"))
		{
			pBlipGfx->GFxValue::GetMember("_currentframe", &currentFrameGfx);

			if (currentFrameGfx.IsNumber())
			{
				iCurrentFrame = (s32)currentFrameGfx.GetNumber();
			}
		}
		else
		{
			uiAssertf(0, "CMiniMap_RenderThread: _currentframe is not a member of blip %s", GetBlipObjectName(pBlip));
		}

		if (iCurrentFrame < __PULSE_FRAME_NUM)
		{
			if (!pBlipGfx->GotoAndPlay(__PULSE_FRAME_NUM))
			{
				uiAssertf(0, "CMiniMap_RenderThread: Pulse blip didnt work on blip %s", GetBlipObjectName(pBlip));
			}
		}
	}
	else
	{
		uiAssertf(0, "CMiniMap_RenderThread: Tried to pulse a blip on the stage that is no longer is a display object!");
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::UpdateTickBlipOnStage
// PURPOSE: adds or removes a "tick" icon on a blip on the stage
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::UpdateTickBlipOnStage(GFxValue *pBlipGfx, CBlipComplex *pBlip)
{
	UpdateBlipSecondaryIconOnStage(pBlipGfx, pBlip, BLIP_FLAG_SHOW_TICK, "radar_completed", "tick_blip", BLIP_DEPTH_TICK);
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::UpdateGoldTickBlipOnStage
// PURPOSE: adds or removes a golden "tick" icon on a blip on the stage
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::UpdateGoldTickBlipOnStage(GFxValue *pBlipGfx, CBlipComplex *pBlip)
{
	UpdateBlipSecondaryIconOnStage(pBlipGfx, pBlip, BLIP_FLAG_SHOW_GOLD_TICK, "radar_completed", "tick_blip", BLIP_DEPTH_TICK);
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::UpdateSaleBlipOnStage
// PURPOSE: adds or removes a "for sale" icon on a blip on the stage
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::UpdateForSaleBlipOnStage(GFxValue *pBlipGfx, CBlipComplex *pBlip)
{
	UpdateBlipSecondaryIconOnStage(pBlipGfx, pBlip, BLIP_FLAG_SHOW_FOR_SALE, "radar_for_sale", "sale_blip", BLIP_DEPTH_SALE_ICON);
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::UpdateBlipSecondaryIconOnStage
// PURPOSE: adds or removes a secondary icon on a blip on the stage
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::UpdateBlipSecondaryIconOnStage(GFxValue *pBlipGfx, CBlipComplex *pBlip, eBLIP_PROPERTY_FLAGS iconFlag, const char* symbolName, const char* instanceName, int displayDepth)
{
	if (pBlip && pBlipGfx && pBlipGfx->IsDisplayObject())
	{
		bool bHasIconAttached = pBlipGfx->GFxValue::HasMember(instanceName);

		if (!CMiniMap::IsFlagSet(pBlip, iconFlag))
		{
			const bool c_checkmarkIcon = displayDepth == BLIP_DEPTH_TICK ;
			const bool c_shouldRemove = c_checkmarkIcon ? !CMiniMap::IsFlagSet(pBlip, BLIP_FLAG_SHOW_TICK) && !CMiniMap::IsFlagSet(pBlip, BLIP_FLAG_SHOW_GOLD_TICK) : true;

			// remove the icon if it is there
			if (bHasIconAttached && c_shouldRemove)  // already got higher blip attached, so remove this 1st
			{
				GFxValue pIcon;
				pBlipGfx->GFxValue::GetMember(instanceName, &pIcon);

				if (pIcon.IsDisplayObject())
				{
					pIcon.Invoke("removeMovieClip");
				}
			}
		}
		else
		{
			// add a tick blip if its not there already
			if (!bHasIconAttached)
			{
				GFxValue tempValue;

				if(uiVerifyf(pBlipGfx->GFxValue::AttachMovie(&tempValue, symbolName, instanceName, displayDepth), "Failed to attach secondary blip icon movieclip (symbolName = %s)", symbolName))
				{
					CRGBA blipColour = iconFlag == BLIP_FLAG_SHOW_GOLD_TICK ? CHudColour::GetRGBA(HUD_COLOUR_GOLD) : CHudColour::GetRGBA(HUD_COLOUR_GREEN);
					tempValue.SetColorTransform(blipColour);

					GFxValue::DisplayInfo dispInfo;
					dispInfo.SetAlpha((float)blipColour.GetAlphaf() * 100.0f);
					tempValue.SetDisplayInfo(dispInfo);
				}
			}
		}
	}
	else
	{
		uiAssertf(0, "CMiniMap_RenderThread: Tried to update %s on a blip on the stage that is no longer is a display object!", instanceName);
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::UpdateHeadingIndicatorOnBlipOnStage
// PURPOSE: adds or removes and rotates a heading indicator onto the blip
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::UpdateHeadingIndicatorOnBlipOnStage(GFxValue *pBlipGfx, CBlipComplex *pBlip, s32 iMapRotation)
{
	if (pBlip && pBlipGfx && pBlipGfx->IsDisplayObject())
	{
		GFxValue pAttachmentBlip;

		bool bHasAttachmentBlip = pBlipGfx->GFxValue::GetMember("direction_blip", &pAttachmentBlip);

		if (!CMiniMap::IsFlagSet(pBlip,BLIP_FLAG_SHOW_HEADING_INDICATOR))
		{
			//
			// remove the direction blip if it is there
			//
			if (bHasAttachmentBlip)  // already got higher blip attached, so remove this 1st
			{
				if (pAttachmentBlip.IsDisplayObject())
				{
					pAttachmentBlip.Invoke("removeMovieClip");
				}

				bHasAttachmentBlip = false;
			}
		}
		else
		{
			//
			// add the direction blip if its not there already
			//
			if (!bHasAttachmentBlip)
			{
				pBlipGfx->GFxValue::AttachMovie(&pAttachmentBlip, "radar_pointer", "direction_blip", BLIP_DEPTH_DIRECTION_INDICATOR);

				bHasAttachmentBlip = true;
			}
		}

		if (bHasAttachmentBlip)
		{
			if (pAttachmentBlip.IsDisplayObject())
			{
				// then update its rotation based on entity heading
				float fBlipRotation = -CMiniMap::GetBlipDirectionValue(pBlip);

				fBlipRotation -= iMapRotation;

				// ensure the value is between 0 and 359 degrees
				fBlipRotation = CMiniMap_Common::WrapBlipRotation(fBlipRotation);

				GFxValue::DisplayInfo rotationInfo;
				rotationInfo.SetRotation(fBlipRotation);

				// copy the alpha from the main blip:
				s32 const c_mainBlipAlpha = GetBlipAlphaValue(pBlip);
				rotationInfo.SetAlpha((float)((c_mainBlipAlpha / 255.0f) * 100.0f));

				pAttachmentBlip.SetDisplayInfo(rotationInfo);  // apply the rotation to the "direction" only
			}
		}
	}
	else
	{
		uiAssertf(0, "CMiniMap_RenderThread: Tried to update a headingIndicator on a blip on the stage that is no longer is a display object!");
	}
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::UpdateOutlineIndicatorOnBlipOnStage
// PURPOSE: adds or removes an outline indicator blip
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::UpdateOutlineIndicatorOnBlipOnStage(GFxValue *pBlipGfx, CBlipComplex *pBlip)
{
	if (pBlip && pBlipGfx && pBlipGfx->IsDisplayObject())
	{
		GFxValue pAttachmentBlip;

		bool bHasAttachmentBlip = pBlipGfx->GFxValue::GetMember("outline_blip", &pAttachmentBlip);

		if (!CMiniMap::IsFlagSet(pBlip,BLIP_FLAG_SHOW_OUTLINE_INDICATOR))
		{
			//
			// remove the outline blip if it is there
			//
			if (bHasAttachmentBlip)  // already got higher blip attached, so remove this 1st
			{
				if (pAttachmentBlip.IsDisplayObject())
				{
					pAttachmentBlip.Invoke("removeMovieClip");
				}
			}
		}
		else
		{
			//
			// add the outline blip if its not there already
			//
			if (!bHasAttachmentBlip)
			{
				const char* pAttachmentBlipName = NULL;
				if (GetBlipObjectNameId(pBlip) == RADAR_TRACE_CAPTURE_THE_FLAG)
				{
					// suitcase outline:
					pAttachmentBlipName = "radar_capture_the_flag_outline";
				}
				else if (GetBlipObjectNameId(pBlip) == RADAR_TRACE_CAPTURE_THE_USAFLAG)
				{
					// usa flag outline:
					pAttachmentBlipName = "radar_capture_the_useflag_outline";
				}
				else
				{
					// standard circle outline:
					pAttachmentBlipName = "RADAR_MP_FRIEND";  // not "really" friend anymore ;-)  confusing I know, last minute requests etc etc
				}

				pBlipGfx->GFxValue::AttachMovie(&pAttachmentBlip, pAttachmentBlipName, "outline_blip", BLIP_DEPTH_OUTLINE_INDICATOR);
			}

			// apply colour onto the blip whether we created it this time or we got it from before
			if (pAttachmentBlip.IsDisplayObject())
			{
				CRGBA blipColour = GetBlipSecondaryColourValue(pBlip);
				pAttachmentBlip.SetColorTransform(blipColour);

				GFxValue::DisplayInfo dispInfo;
				s32 iMainBlipAlpha = GetBlipAlphaValue(pBlip);  // fix for 1584520 - use alpha of the main blip (as the friend indicator needs to fade out when they fade the blip)
				dispInfo.SetAlpha((float)((iMainBlipAlpha / 255.0f) * 100.0f));
				pAttachmentBlip.SetDisplayInfo(dispInfo);
			}
		}
	}
	else
	{
		uiAssertf(0, "CMiniMap_RenderThread: Tried to update an outline blip on a blip on the stage that is no longer is a display object!");
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::UpdateFriendIndicatorOnBlipOnStage
// PURPOSE: adds or removes an outline indicator blip
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::UpdateFriendIndicatorOnBlipOnStage(GFxValue *pBlipGfx, CBlipComplex *pBlip)
{
	if (pBlip && pBlipGfx && pBlipGfx->IsDisplayObject())
	{
		GFxValue pAttachmentBlip;

		bool bHasAttachmentBlip = pBlipGfx->GFxValue::GetMember("friend_blip", &pAttachmentBlip);

		if (!CMiniMap::IsFlagSet(pBlip,BLIP_FLAG_SHOW_FRIEND_INDICATOR))
		{
			//
			// remove the direction blip if it is there
			//
			if (bHasAttachmentBlip)  // already got higher blip attached, so remove this 1st
			{
				if (pAttachmentBlip.IsDisplayObject())
				{
					pAttachmentBlip.Invoke("removeMovieClip");
				}
			}
		}
		else
		{
			//
			// add the direction blip if its not there already
			//
			if (!bHasAttachmentBlip)
			{
				pBlipGfx->GFxValue::AttachMovie(&pAttachmentBlip, "radar_mp_friendlies", "friend_blip", BLIP_DEPTH_FRIEND_INDICATOR);
			}

			// apply colour onto the blip whether we created it this time or we got it from before
			if (pAttachmentBlip.IsDisplayObject())
			{
				CRGBA blipColour = CHudColour::GetRGBA(HUD_COLOUR_FRIENDLY);
				pAttachmentBlip.SetColorTransform(blipColour);

				GFxValue::DisplayInfo dispInfo;
				s32 iMainBlipAlpha = GetBlipAlphaValue(pBlip);  // fix for 1584520 - use alpha of the main blip (as the friend indicator needs to fade out when they fade the blip)
				dispInfo.SetAlpha((float)((iMainBlipAlpha / 255.0f) * 100.0f));
				pAttachmentBlip.SetDisplayInfo(dispInfo);
			}
		}
	}
	else
	{
		uiAssertf(0, "CMiniMap_RenderThread: Tried to update a friend indicator on a blip on the stage that is no longer is a display object!");
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::UpdateCrewIndicatorOnBlipOnStage
// PURPOSE: adds or removes an outline indicator blip
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::UpdateCrewIndicatorOnBlipOnStage(GFxValue *pBlipGfx, CBlipComplex *pBlip)
{
	if (pBlip && pBlipGfx && pBlipGfx->IsDisplayObject())
	{
		GFxValue pAttachmentBlip;

		bool bHasAttachmentBlip = pBlipGfx->GFxValue::GetMember("crew_blip", &pAttachmentBlip);

		if (!CMiniMap::IsFlagSet(pBlip,BLIP_FLAG_SHOW_CREW_INDICATOR))
		{
			//
			// remove the direction blip if it is there
			//
			if (bHasAttachmentBlip)  // already got higher blip attached, so remove this 1st
			{
				if (pAttachmentBlip.IsDisplayObject())
				{
					pAttachmentBlip.Invoke("removeMovieClip");
				}
			}
		}
		else
		{
			//
			// add the direction blip if its not there already
			//
			if (!bHasAttachmentBlip)
			{
				pBlipGfx->GFxValue::AttachMovie(&pAttachmentBlip, "radar_mp_crew", "crew_blip", BLIP_DEPTH_CREW_INDICATOR);
			}

			// apply colour onto the blip whether we created it this time or we got it from before
			if (pAttachmentBlip.IsDisplayObject())
			{
				CRGBA blipColour = GetBlipSecondaryColourValue(pBlip);
				pAttachmentBlip.SetColorTransform(blipColour);

				GFxValue::DisplayInfo dispInfo;
				s32 iMainBlipAlpha = GetBlipAlphaValue(pBlip);  // fix for 1584520 - use alpha of the main blip (as the friend indicator needs to fade out when they fade the blip)
				dispInfo.SetAlpha((float)((iMainBlipAlpha / 255.0f) * 100.0f));
				pAttachmentBlip.SetDisplayInfo(dispInfo);
			}
		}
	}
	else
	{
		uiAssertf(0, "CMiniMap_RenderThread: Tried to update a crew indicator on a blip on the stage that is no longer is a display object!");
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::FadeOutDistantBlipOnStage
// PURPOSE: fades out a blip as it gets further out into the distance
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::FadeOutDistantBlipOnStage(GFxValue *pBlipGfx, s32 iDisplayAlpha)
{
	if (ms_iMovieId[MINIMAP_MOVIE_FOREGROUND] == -1)
		return;

	if (pBlipGfx && pBlipGfx->IsDisplayObject())
	{
		GFxValue::DisplayInfo currentAlphaDisplayInfo;
		pBlipGfx->GetDisplayInfo(&currentAlphaDisplayInfo);

		if (currentAlphaDisplayInfo.GetAlpha() != iDisplayAlpha)
		{
			currentAlphaDisplayInfo.SetAlpha(iDisplayAlpha);
			pBlipGfx->SetDisplayInfo(currentAlphaDisplayInfo);
		}
	}
	else
	{
		uiAssertf(0, "CMiniMap_RenderThread: Tried to set alpha on the stage that is no longer is a display object!");
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::ColourBlipOnStage
// PURPOSE: calls invoke on the ActionScript object to colour a blip
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::ColourBlipOnStage(GFxValue *pBlipGfx, CBlipComplex *pBlip)
{
	if (ms_iMovieId[MINIMAP_MOVIE_FOREGROUND] == -1)
		return;

	if (pBlip && pBlipGfx && pBlipGfx->IsDisplayObject())
	{
		GFxValue pMainBlip;
		if (pBlipGfx->GFxValue::GetMember("main_blip", &pMainBlip))
		{
			if (pMainBlip.IsDisplayObject())
			{
				CRGBA blipColour;

				if (GetBlipColourValue(pBlip) != BLIP_COLOUR_USE_COLOUR32)
				{
					blipColour = CMiniMap_Common::GetColourFromBlipSettings(GetBlipColourValue(pBlip), CMiniMap::IsFlagSet(pBlip,BLIP_FLAG_BRIGHTNESS));

					if (GetBlipAlphaValue(pBlip) != 255)
					{
						blipColour.SetAlpha(GetBlipAlphaValue(pBlip));  // use individual alpha settings
					}
				}
				else
				{
					blipColour = GetBlipSecondaryColourValue(pBlip);  // use actual RGBA value
				}

				// with the alpha, we set it on the blip container movieclip so it sets the alpha for all its children aswell
				GFxValue::DisplayInfo currentDisplayInfo;
				float fAlpha = blipColour.GetAlphaf() * 100;

				Assertf(CMiniMap::IsFlagSet(pBlip,BLIP_FLAG_UPDATE_ALPHA_ONLY) || CMiniMap::IsFlagSet(pBlip,BLIP_FLAG_VALUE_CHANGED_COLOUR), "CMiniMap_RenderThread::ColourBlipOnStage - expected either the CHANGED_COLOUR or the UPDATE_ALPHA_ONLY flag to be set");
				currentDisplayInfo.SetAlpha(fAlpha);
				pBlipGfx->SetDisplayInfo(currentDisplayInfo);

				if (CMiniMap::IsFlagSet(pBlip,BLIP_FLAG_VALUE_CHANGED_COLOUR))
				{
					// we set the colour on the "main_blip" only - as we only want to colour the blip iself (and any children) and not its siblings
					pMainBlip.SetColorTransform(blipColour);
				}

				if (CMiniMap::IsFlagSet(pBlip,BLIP_FLAG_VALUE_CHANGED_COLOUR) || CMiniMap::IsFlagSet(pBlip,BLIP_FLAG_UPDATE_ALPHA_ONLY))
				{
					GFxValue::DisplayInfo dispInfo;
					dispInfo.SetAlpha((float)blipColour.GetAlphaf() * 100.0f);
					pMainBlip.SetDisplayInfo(dispInfo);
				}

				// need to copy the alpha value of the blip onto the higher/lower blip
				bool bHasHigherBlipAttached = pBlipGfx->GFxValue::HasMember("higher_blip");
				bool bHasLowerBlipAttached = pBlipGfx->GFxValue::HasMember("lower_blip");

				GFxValue pHeightBlip;

				if (bHasHigherBlipAttached)
				{
					pBlipGfx->GFxValue::GetMember("higher_blip", &pHeightBlip);
				}
				else if (bHasLowerBlipAttached)
				{
					pBlipGfx->GFxValue::GetMember("lower_blip", &pHeightBlip);
				}

				if (pHeightBlip.IsDisplayObject())
				{
					// copy the alpha from the main blip:
					GFxValue::DisplayInfo heightBlipDisplayInfo;
					heightBlipDisplayInfo.SetAlpha(fAlpha);
					pHeightBlip.SetDisplayInfo(heightBlipDisplayInfo);
				}
			}
		}
	}
	else
	{
		uiAssertf(0, "CMiniMap_RenderThread: Tried to colour a blip on the stage that is no longer is a display object!");
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::FlashBlipOnStage
// PURPOSE: calls invoke on the ActionScript object to flash/stop flashing a blip
/////////////////////////////////////////////////////////////////////////////////////
bool CMiniMap_RenderThread::FlashBlipOnStage(GFxValue *pBlipGfx, CBlipComplex *pBlip)
{
	if (pBlip && pBlipGfx && pBlipGfx->IsDisplayObject())
	{
		if (!CMiniMap::IsFlagSet(pBlip, BLIP_FLAG_FLASHING))
		{
			float fTimeInSeconds = ( (float)GetBlipFlashInterval(pBlip) / 1000.0f );  // convert ms into seconds to pass to ActionScript

			if (CScaleformMgr::BeginMethod(ms_iMovieId[MINIMAP_MOVIE_FOREGROUND], SF_BASE_CLASS_MINIMAP, "START_BLIP_FLASHING"))
			{
				CScaleformMgr::AddParamGfxValue(*pBlipGfx);
				CScaleformMgr::AddParamFloat(fTimeInSeconds);
				CScaleformMgr::EndMethod();
			}
		}
		else
		{
			if (CScaleformMgr::BeginMethod(ms_iMovieId[MINIMAP_MOVIE_FOREGROUND], SF_BASE_CLASS_MINIMAP, "STOP_BLIP_FLASHING"))
			{
				CScaleformMgr::AddParamGfxValue(*pBlipGfx);
				CScaleformMgr::EndMethod();
			}
		}

		return true;
	}
	else
	{
		uiAssertf(0, "CMiniMap_RenderThread: Tried to flash a blip on the stage that is no longer is a display object!");
	}

	return false;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::GetBlipGfxValue
// PURPOSE: returns the gfxvalue used for this slot (in the render)
/////////////////////////////////////////////////////////////////////////////////////
GFxValue* CMiniMap_RenderThread::GetBlipGfxValue(const CBlipComplex *pBlip)
{
	if (pBlip)
	{
		if (pBlip->m_iActualId != -1)
		{
			uiAssertf((pBlip->m_iActualId >= 0) && (pBlip->m_iActualId < MAX_NUM_BLIPS), "CMiniMap_RenderThread::GetBlipGfxValue()...Accessing bad blip!");
			return pRenderedBlipObject[pBlip->m_iActualId];
		}
	}

	return NULL;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::SetBlipGfxValue
// PURPOSE: sets the gfx value of the blip
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::SetBlipGfxValue(const CBlipComplex *pBlip, GFxValue *pParam)
{
	if (pBlip)
	{
		if (pBlip->m_iActualId != -1)
		{
			pRenderedBlipObject[pBlip->m_iActualId] = pParam;
		}
	}
}

bool CMiniMap_RenderThread::GetIsPlacedOn3dMap(const CBlipComplex* pBlip)
{
	if (pBlip && pBlip->m_iActualId != -1)
	{
		uiAssertf((pBlip->m_iActualId >= 0) && (pBlip->m_iActualId < MAX_NUM_BLIPS), "CMiniMap_RenderThread::GetIsPlacedOn3dMap()...Accessing bad blip!");
		return bBlipIsOn3dLayer.IsSet(pBlip->m_iActualId);
	}

	return false;
}

void CMiniMap_RenderThread::SetIsPlacedOn3dMap(const CBlipComplex* pBlip, bool bOnMap)
{
	if (pBlip && pBlip->m_iActualId != -1)
	{
		bBlipIsOn3dLayer.Set(pBlip->m_iActualId, bOnMap);
	}
}

#define BLIP_SIZE_MINIMAP_GOLF_COURSE	(45.0f)
#define BLIP_SIZE_MINIMAP				(100.0f)
#define BLIP_SIZE_PAUSEMENU_MAP			(24.0f)
#define BLIP_SIZE_PAUSEMENU_MAP_HOVERED	(30.0f)
#define BLIP_SIZE_BIGMAP				(45.0f)
#define BLIP_SIZE_GALLERYMAP			(45.0f)

float CMiniMap_RenderThread::GetBlipScalerValue(const CBlipComplex* pBlip, bool bCareAboutType /* = true */)
{
	float fBlipScaler = BLIP_SIZE_MINIMAP;

	if ( bCareAboutType && (GetBlipTypeValue(pBlip) == BLIP_TYPE_RADIUS || GetBlipTypeValue(pBlip) == BLIP_TYPE_AREA))
	{
		fBlipScaler = 2.0f;
	}
	else if (ms_MiniMapRenderState.m_bIsInPauseMap)
	{
		fBlipScaler = BLIP_SIZE_PAUSEMENU_MAP;

		if( CMiniMap::IsFlagSet(pBlip, BLIP_FLAG_HOVERED_ON_PAUSEMAP) )
			fBlipScaler = BLIP_SIZE_PAUSEMENU_MAP_HOVERED;
	}
	else if (ms_MiniMapRenderState.m_bIsInBigMap)
	{
		fBlipScaler = BLIP_SIZE_BIGMAP;
	}
	else if (ms_MiniMapRenderState.m_bIsInCustomMap)
	{
		fBlipScaler = BLIP_SIZE_GALLERYMAP;
	}
	else if (ms_MiniMapRenderState.m_CurrentGolfMap != GOLF_COURSE_OFF)
	{
		fBlipScaler = BLIP_SIZE_MINIMAP_GOLF_COURSE;
	}

	
	return fBlipScaler;
}

bool CMiniMap_RenderThread::DoesRenderedBlipExist(s32 BlipIndex)
{
	if (uiVerifyf((BlipIndex >= 0) && (BlipIndex < MAX_NUM_BLIPS), "CMiniMap_RenderThread::DoesRenderedBlipExist - blip index is out of range"))
	{
		return (pRenderedBlipObject[BlipIndex] != NULL);
	}

	return false;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::RemoveBlipFromStage
// PURPOSE: removes the GFxValue object if the blip leaves the stage
/////////////////////////////////////////////////////////////////////////////////////
bool CMiniMap_RenderThread::RemoveBlipFromStage(CBlipComplex *pBlip, bool bFromUpdateThreadInShutdownSession)
{
	PF_FUNC(RemoveBlipFromStage);

	if (bFromUpdateThreadInShutdownSession)
	{
		if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on UT
		{
			sfAssertf(0, "CMiniMap_RenderThread::RemoveBlipFromStage has been called with the special-case flag bFromUpdateThreadInShutdownSession so it's expected to be called on the UpdateThread");
			return false;
		}
	}
	else
	{
		if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
		{
			sfAssertf(0, "CMiniMap_RenderThread::RemoveBlipFromStage can only be called on the RenderThread!");
			return false;
		}
	}

	if (ms_iMovieId[MINIMAP_MOVIE_FOREGROUND] == -1)
		return false;

	if (!pBlip)
		return false;

	uiDebugf1("CMiniMap_RenderThread: Removed blip %d %s from stage", pBlip->m_iUniqueId, GetBlipObjectName(pBlip));

	GFxValue *pGfxValue = GetBlipGfxValue(pBlip);

	if (pGfxValue)
	{
		if (pGfxValue->IsDisplayObject())
		{
			//
			// at this point, we need to ensure we call some "stop" methods on actionscript so it call cleans up nicely in the tweener:
			//
			if (CMiniMap::IsFlagSet(pBlip, BLIP_FLAG_FLASHING))
			{
				if (CScaleformMgr::BeginMethod(ms_iMovieId[MINIMAP_MOVIE_FOREGROUND], SF_BASE_CLASS_MINIMAP, "STOP_BLIP_FLASHING"))
				{
					CScaleformMgr::AddParamGfxValue(*pGfxValue);
					CScaleformMgr::EndMethod(bFromUpdateThreadInShutdownSession);
				}
			}

			if(CMiniMap::IsFlagSet(pBlip, BLIP_FLAG_SHOW_HEIGHT))
			{
				RemoveHigherLowerBlip(pGfxValue, 0);
			}

			//
			// finally, remove the blip:
			//
			pGfxValue->Invoke("removeMovieClip");
			pGfxValue->SetUndefined();

			delete (pGfxValue);

			CScaleformMgr::ForceCollectGarbage(ms_iMovieId[MINIMAP_MOVIE_FOREGROUND]);  // perform garbage collection once the blip has been removed
		}
		else
		{
			uiAssertf(0, "MiniMap: Tried to remove a blip from the stage that is no longer is a display object!");
		}

		SetBlipGfxValue(pBlip, NULL);
	}

	return true;
}


void CMiniMap_RenderThread::RenderToFoW(CBlipComplex * FOG_OF_WAR_ONLY(pBlip))
{
#if ENABLE_FOG_OF_WAR
	Vector2 coords = WorldToFowCoord(pBlip->vPosition.x, pBlip->vPosition.y);

	float fowWidth = (float)CMiniMap_Common::GetFogOfWarRT()->GetWidth();
	float fowHeight = (float)CMiniMap_Common::GetFogOfWarRT()->GetHeight();

	const float aspectRatio = CMiniMap::AreFowCustomWorldExtentsEnabled()?
								(CMiniMap::GetFowCustomWorldH() / CMiniMap::GetFowCustomWorldW())				:
								(CMiniMap::sm_Tunables.FogOfWar.fWorldH / CMiniMap::sm_Tunables.FogOfWar.fWorldW);

	float xDelta = (fowBlipColorSizeX/fowWidth)*aspectRatio;
	float yDelta = (fowBlipColorSizeY/fowHeight);

	grcBindTexture(CShaderLib::LookupTexture("FoW"));
	
	for(int i=0;i<bleepCount;i++)
	{
		grcDrawSingleQuadf(coords.x-xDelta,coords.y-yDelta,coords.x+xDelta,coords.y+yDelta, 0.0f, 0.0f , 0.0f , 1.0f , 1.0f , fowBleepColor);
	}
#endif // ENABLE_FOG_OF_WAR
}

#define SCALEFORM_SCALE_VALUE (100.0f)  // flash uses 100 as a base scale value instead of the sane 1.0.
#define ADJUST_SCALEFORM_SCALAR(scalar) (SCALEFORM_SCALE_VALUE/scalar) // a scalar of 50 means 2x 'zoom'
#define SCALEFORM_SCALE_TO_PERCENT(scalar) (scalar/SCALEFORM_SCALE_VALUE) // a scalar of 50 means 50% (.5)

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::UpdateIndividualBlip
// PURPOSE:	updates an individual blip
/////////////////////////////////////////////////////////////////////////////////////
bool  CMiniMap_RenderThread::UpdateIndividualBlip(CBlipComplex *pBlip, s32 iMapRotation, bool bRangeCheckOnly)
{
	PF_FUNC(UpdateIndividualBlip);

	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		sfAssertf(0, "CMiniMap_RenderThread::UpdateIndividualBlip can only be called on the RenderThread!");
		return false;
	}

	if (ms_iMovieId[MINIMAP_MOVIE_FOREGROUND] == -1)
		return false;

	if (!pBlip)
		return false;

	iMapRotation = -ms_MiniMapRenderState.m_iRotation;

	Vector2 vMapCentrePosition(ms_MiniMapRenderState.m_vCentrePosition.x, ms_MiniMapRenderState.m_vCentrePosition.y);

	CMiniMap::BlipFlagBitset flags(false);
	CMiniMap::GetAllFlags(flags, pBlip);

	if (!bRangeCheckOnly)
	{
		if (flags.IsSet(BLIP_FLAG_VALUE_REMOVE_FROM_STAGE))
		{
			RemoveBlipFromStage(pBlip, false);
			return false;
		}

		if (flags.IsSet(BLIP_FLAG_VALUE_REINIT_STAGE))
		{
			ReInitBlipOnStage(pBlip);
			CMiniMap::GetAllFlags(flags, pBlip); // Flags may have changed, get new ones
		}
	}

	//
	// update this individual blip
	//
	Vec3f vBlipPos(RCC_VEC3F(GetBlipPositionConstRef(pBlip))); // Stay out of the vector registers
	Vector2 vNewBlipPos = Vector2(vBlipPos.GetX(), -vBlipPos.GetY());
	const eBLIP_DISPLAY_TYPE c_blipDisplayType = GetBlipDisplayValue(pBlip);

	bool bOnEdgeOfRadar = false;
	bool bOnRadar = true;

	bool bOn3dMap = GetBlipTypeValue(pBlip) == BLIP_TYPE_AREA || GetBlipTypeValue(pBlip) == BLIP_TYPE_RADIUS;
	//
	// depending on the display value of this blip, we need to decide whether to have this displayed on
	// the current map (either in the pausemap or on the ingame minimap)
	//

	// no POI when in golf (bug 1319054) or creator (1702753)
	if (GetBlipObjectNameId(pBlip) == RADAR_TRACE_POI)
	{
		if ( (ms_MiniMapRenderState.m_CurrentGolfMap != GOLF_COURSE_OFF) ||
			(ms_MiniMapRenderState.bInCreator) )
		{
			bOnRadar = false;
		}
	}

	// fix for 1690613 - no mission creator blips should be shown when script are hiding the exterior map
	// except sometimes
	if ((c_blipDisplayType != BLIP_DISPLAY_BOTH) &&
		(flags.IsSet(BLIP_FLAG_MISSION_CREATOR)) && 
		((ms_MiniMapRenderState.m_bIsInPauseMap && ms_MiniMapRenderState.bInsideInterior) || 
		((!ms_MiniMapRenderState.m_bShowMainMap) && (!ms_MiniMapRenderState.m_bShowPrologueMap) && (!ms_MiniMapRenderState.m_bShowIslandMap))))
	{
		bOnRadar = false;
	}

	// probably dont want to do this now this has been changed to high and low detail - but design is sketchy, leaving out to begin with
	/*if (bOnRadar && ((!CMiniMap::GetBlipVisibilityHighDetail()) && (flags.IsSet(BLIP_FLAG_HIGH_DETAIL))) )  // 1190959 wants legend blips hidden on minimap too
	{
		bOnRadar = false;
	}*/


	// if minimap is not being rendered, then lets not add any blips to the stage, rather lets remove them (optimisation to help fix 1708020)
	if ( (!ms_MiniMapRenderState.m_bShouldRenderMiniMap) &&  // minimap isnt getting rendered
		 (!ms_MiniMapRenderState.m_bIsInPauseMap) &&  // and we are not in the pausemap
		 (!ms_MiniMapRenderState.m_bIsInCustomMap)) // and we are not in a custom map
	{
		bOnRadar = false;
	}

	if (bOnRadar)
	{
		//
		// if in-game minimap:
		//
		if (!ms_MiniMapRenderState.m_bIsInPauseMap && !ms_MiniMapRenderState.m_bIsInCustomMap)
		{
			if (ms_MiniMapRenderState.m_bIsInCustomMap)
			{
				if (c_blipDisplayType != BLIP_DISPLAY_CUSTOM_MAP_ONLY)
				{
					bOnRadar = false;
				}
			}
			else
			{
				if (c_blipDisplayType == BLIP_DISPLAY_PAUSEMAP ||
					c_blipDisplayType == BLIP_DISPLAY_MARKERONLY ||
					c_blipDisplayType == BLIP_DISPLAY_NEITHER)
				{
					bOnRadar = false;
				}

				if (ms_MiniMapRenderState.m_bIsInBigMap)
				{
					if (c_blipDisplayType == BLIP_DISPLAY_MINIMAP)
					{
						bOnRadar = false;
					}

					if (!ms_MiniMapRenderState.m_bBigMapFullZoom)
					{
						if (c_blipDisplayType == BLIP_DISPLAY_BIGMAP_FULL_ONLY)
						{
							bOnRadar = false;
						}
					}
				}
				else
				{
					if (c_blipDisplayType == BLIP_DISPLAY_BIGMAP_FULL_ONLY)
					{
						bOnRadar = false;
					}
				}
			}
		}
		else if (ms_MiniMapRenderState.m_bIsInCustomMap)
		{
			if (c_blipDisplayType != BLIP_DISPLAY_CUSTOM_MAP_ONLY && c_blipDisplayType != BLIP_DISPLAY_BLIPONLY)
				bOnRadar = false;
			else
				bOnRadar = true;
		}
		else
		//
		// frontend map:
		//
		{
			if (((CMiniMap::GetBlipVisibilityHighDetail()) && (!flags.IsSet(BLIP_FLAG_HIGH_DETAIL))) ||
				c_blipDisplayType == BLIP_DISPLAY_MINIMAP ||
				c_blipDisplayType == BLIP_DISPLAY_MINIMAP_OR_BIGMAP ||
				c_blipDisplayType == BLIP_DISPLAY_MARKERONLY ||
				c_blipDisplayType == BLIP_DISPLAY_BIGMAP_FULL_ONLY ||
				c_blipDisplayType == BLIP_DISPLAY_NEITHER)
			{
				bOnRadar = false;
			}

			if ( (bOnRadar && ms_MiniMapRenderState.bInsideInterior) && (!ms_MiniMapRenderState.m_Interior.vBoundMin.IsZero()) && (!ms_MiniMapRenderState.m_Interior.vBoundMax.IsZero()) )
			{
				if (GetBlipTypeValue(pBlip) == BLIP_TYPE_COORDS)
				{
					if (vBlipPos.GetX() < ms_MiniMapRenderState.m_Interior.vBoundMin.x ||
						vBlipPos.GetX() > ms_MiniMapRenderState.m_Interior.vBoundMax.x ||
						vBlipPos.GetY() < ms_MiniMapRenderState.m_Interior.vBoundMin.y ||
						vBlipPos.GetY() > ms_MiniMapRenderState.m_Interior.vBoundMax.y)
					{
						bOnRadar = false;
					}
				}
			}
		}
	}
	
	Vector2 vNewBlipPosInMiniMap(0,0);
	bool bBlipWithin15mOfPlayer = true;
	s32 iDisplayAlpha = 100;
	int iQuadrant = -1; // used solely for edgey blips

	if (bOnRadar)
	{
/*		if (asBlipTestObject.IsDisplayObject())
		{
			GFxValue::DisplayInfo displayInfo;
			if ( (!ms_MiniMapRenderState.m_bIsInPauseMap) && (pLocalPlayer && (!pLocalPlayer->GetVehiclePedInside())) && (GetRange() < MAP_BLIP_ROUND_THRESHOLD) )
			{
				displayInfo.SetPosition(floor(vNewBlipPos.x+0.5f), floor(vNewBlipPos.y+0.5f));
			}
			else
			{
				displayInfo.SetPosition(vNewBlipPos.x, vNewBlipPos.y);
			}

			asBlipTestObject.SetDisplayInfo(displayInfo);
		}*/
	
		//
		// check the position of the blip in "2D space"
		//
		GPointF blipPoint, screenPoint;

		//
		// because of the way scaleform returns the screen position of the 3D blip, we need to limit the range so we only ever check coords
		// that are close to the edge of the minimap ring.   We first work out the height of the blip onscreen in 3D space not relative to the
		// current rotation of the map, then from that we scale the distance we check for.  This means the range that is checked against is
		// further away if you can see further away into the distance, but the range is less if the blip is at the bottom of the minimap and
		// there isnt a lot of map visible.
		//

		bool bAdjustPosForDistance = true;

		if( GetBlipTypeValue(pBlip) == BLIP_TYPE_AREA )
		{
			// Extended area checks necessary for Square blips (and radius blips, if we did this [which we don't for V])
			// Only take into account distance adjustments if the area is outside the bounds of the map
			// If we run these calculations for area blips within map bounds, the area position glitches about 
			// when the center extends outside of the map
			bAdjustPosForDistance = IsAreaBlipOutsideMapBounds(*pBlip);
		}

		if ( (!ms_MiniMapRenderState.m_bIsInPauseMap && !ms_MiniMapRenderState.m_bIsInCustomMap) && (ms_MiniMapRenderState.m_CurrentGolfMap == GOLF_COURSE_OFF) && 
				bAdjustPosForDistance)
		{
			if (GetBlipTypeValue(pBlip) != BLIP_TYPE_RADIUS /*&& GetBlipTypeValue(pBlip) != BLIP_TYPE_AREA*/)
			{
				Vector2 vTestBlipForDistance(Vector2(vBlipPos.GetX(), vBlipPos.GetY())-vMapCentrePosition);
				float fMaxDistance = 0.0f;

				// do we need to re-adjust the range based on a 3D view....?
				if (ms_fBlipMapAngle != 0.0f)  // if we have an angle
				{
					// work out the angle of the blip relative to the centre blip:
					float fAngle = atan2(vMapCentrePosition.y - vBlipPos.GetY(), vMapCentrePosition.x - vBlipPos.GetX());

					if (fAngle < 0)
						fAngle += (2*PI);

					fAngle = ((fAngle * RtoD) - 270.0f) + (float)iMapRotation;

					// ensure its between 0 to 360 degrees
					fAngle = rage::Wrap(fAngle, 0.0f, 360.0f);

					// work out the max distance we want to check for based on how much range is visible (may also want to add 3D angle in here too)
					fMaxDistance = MINIMAP_DISTANCE_SCALER_3D * (MIN_MINIMAP_RANGE/ms_fBlipMapRange);  // more zoomed out you are, the less we need to restrict the check

					if (fAngle > 180.0f)
						fAngle = 180.0f-(fAngle-180.0f);  // get a 0 to 180 degree range across the top to bottom of the minimap, regardless of which side of the minimap the blip is on

					float fHeightScaler = Max(0.15f, 1.0f-(fAngle / 180.0f));  // convert that to 0-1 space

					if (fHeightScaler > 0.5f)  // if the blip is high up on the minimap then its in the area that is more visible, so gradually give it more range
					{
						fHeightScaler *= (1.0f+((fHeightScaler-0.5f)*MINIMAP_HEIGHT_SCALER));
					}

					fMaxDistance *= fHeightScaler;  // apply the scaler to the max distance
				}
				else  // ... or a 2D view?
				{
					fMaxDistance = MINIMAP_DISTANCE_SCALER_2D * (MIN_MINIMAP_RANGE/ms_fBlipMapRange);
				}

				float fDistance = vTestBlipForDistance.Mag();  // get the distance

				if (fDistance > 15.0f)  // check to see if its 15m
				{
					bBlipWithin15mOfPlayer = false;
				}

				if (fDistance > fMaxDistance)
				{
					float fScale = fMaxDistance / fDistance;

					vTestBlipForDistance.x *= fScale;
					vTestBlipForDistance.y *= fScale;

					vNewBlipPos = Vector2(vTestBlipForDistance.x + vMapCentrePosition.x, -(vTestBlipForDistance.y + vMapCentrePosition.y));
				}
			}
		}

		blipPoint.x = vNewBlipPos.x;
		blipPoint.y = vNewBlipPos.y;

		Vector2 vPosInMiniMap(0,0);
		ms_LocalToScreen.GetRenderBuf().Transform(&screenPoint, blipPoint, true);

#if SUPPORT_MULTI_MONITOR
		// ms_LocalToScreen is built from the scaleform viewport, which is already adjusted for multihead
		// other values here are not aware yet, hence we transform it back into the original space
		const GridMonitor &mon = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor();
		screenPoint.x = SCREEN_WIDTH * (screenPoint.x - mon.uLeft) / mon.getWidth();
		screenPoint.y = SCREEN_HEIGHT * (screenPoint.y - mon.uTop) / mon.getHeight();
#endif //SUPPORT_MULTI_MONITOR

#if 0 // Sanity check the new transform code
		{
			GFxMovieView* pView = CScaleformMgr::GetMovieView(ms_iMovieId);
			GPointF oldScreenPoint;
			if (pView && pView->TranslateLocalToScreen(BLIP_LAYER_PATH, blipPoint, &oldScreenPoint))
			{
				Assertf(oldScreenPoint.DistanceSquared(screenPoint) < 0.001f, "FindLocalToScreen didn't work!");
			}
		}
#endif
		// if(...) was removed
		{
			// convert "screen space" to "minimap space":
			vPosInMiniMap = Vector2(screenPoint.x / SCREEN_WIDTH, screenPoint.y / SCREEN_HEIGHT);

/*#if __BANK
			if (bDebug3DBlips)
			{
				if (CMiniMap::GetUniqueBlipUsed(pBlip) == CMiniMap::GetUniqueCentreBlipId())
	  			{
	  				vGameScreenPos = vPosInMiniMap;
  				}
			}
#endif // __BANK*/

			if ( (!ms_MiniMapRenderState.m_bIsInPauseMap) && (ms_MiniMapRenderState.m_CurrentGolfMap != GOLF_COURSE_OFF) )
			{
				vPosInMiniMap.x -= ms_MiniMapRenderState.m_vCurrentMiniMapPosition.x;
 				vPosInMiniMap.y -= ms_MiniMapRenderState.m_vCurrentMiniMapPosition.y;

				// work out if the blip is in the range of the visible map on the screen (the stage):
				if (vPosInMiniMap.x >= 0.0f && vPosInMiniMap.y >= 0.0f &&
					vPosInMiniMap.x <= ms_MiniMapRenderState.m_vCurrentMiniMapSize.x && vPosInMiniMap.y <= ms_MiniMapRenderState.m_vCurrentMiniMapSize.y)
				{
					vPosInMiniMap.x = (vPosInMiniMap.x / ms_MiniMapRenderState.m_vCurrentMiniMapSize.x) * ms_vStageSize.x;  // reposition into "stage space"
					vPosInMiniMap.y = (vPosInMiniMap.y / ms_MiniMapRenderState.m_vCurrentMiniMapSize.y) * ms_vStageSize.y;
				}
				else
				{
					bOnRadar = false;
				}

			}
			else
			{
				vPosInMiniMap.x -= ms_MiniMapRenderState.m_vCurrentMiniMapPosition.x;
				vPosInMiniMap.y -= ms_MiniMapRenderState.m_vCurrentMiniMapPosition.y;

				vPosInMiniMap.x /= ms_MiniMapRenderState.m_vCurrentMiniMapSize.x;
				vPosInMiniMap.y /= ms_MiniMapRenderState.m_vCurrentMiniMapSize.y;
			}

			vNewBlipPosInMiniMap = Vector2(vPosInMiniMap.x - 0.5f, vPosInMiniMap.y - 0.5f);
			
/*#if __BANK  // cant access GetActiveWaypointId() anympore from RT
			if (bDebug3DBlips)
			{
				if (CMiniMap::GetUniqueBlipUsed(pBlip) == CMiniMap::GetUniqueCentreBlipId())
				{
					Displayf("Debug blip pos - %d (%d) - %0.2f,%0.2f    vorig %0.2f,%0.2f", CMiniMap::GetUniqueBlipUsed(pBlip), CMiniMap::GetActualBlipUsed(pBlip), vNewBlipPosInMiniMap.x, vNewBlipPosInMiniMap.y, vBlipPos.x, vBlipPos.y);
				}
			}
#endif // __BANK*/


			// BIG NASTY SORTA HACK
			// Counter-scale area blips so they don't appear too big on the screen
			// because script is currently using these to spoof a line
			// if we ever need them to be an ACTUAL area, this won't work
			if( GetBlipTypeValue(pBlip) == BLIP_TYPE_AREA && pBlip->vScale.y <= 5.0f )// assumption, if it's kinda small, always counterscale
			{
				GFxValue::DisplayInfo infoRangeRotation;

				ms_asMapContainerMc.GetDisplayInfo(&infoRangeRotation);

				float fCurrRange = ADJUST_SCALEFORM_SCALAR((float)infoRangeRotation.GetXScale());
				pBlip->vScale.y = Min(pBlip->vScale.y, fCurrRange * pBlip->vScale.y); 
				// we could scale the values entirely, but it looks really weird very zoomed out, so we'll just prevent it from getting too fat
			}


			// we now have the 2D blip position in the minimap space

			// check the range so we can lock blips to the edge of the radar in the 2D space:
			if ( (ms_MiniMapRenderState.m_bIsInPauseMap) || (ms_MiniMapRenderState.m_CurrentGolfMap == GOLF_COURSE_OFF) )
			{
#define MAX_2D_RADAR_DISTANCE_RADIUS_CIRCLE (0.39f)
#define MAX_2D_RADAR_DISTANCE_RADIUS_SQUARE (0.8f)

				float fMaxDistance = MAX_2D_RADAR_DISTANCE_RADIUS_CIRCLE;

				if (bMiniMapSquare)
				{
					fMaxDistance = MAX_2D_RADAR_DISTANCE_RADIUS_SQUARE;
				}

				float f2DDistance = vNewBlipPosInMiniMap.Mag();

				if (GetBlipTypeValue(pBlip) != BLIP_TYPE_RADIUS && GetBlipTypeValue(pBlip) != BLIP_TYPE_AREA)
				{
					if (f2DDistance > fMaxDistance)
					{
						// if these are short range blips and isnt flashing then dont lock to the edge (they dissapear off the radar)
						if (flags.IsSet(BLIP_FLAG_SHORTRANGE) && ( (flags.IsClear(BLIP_FLAG_FLASHING)) && (flags.IsClear(BLIP_FLAG_VALUE_CHANGED_FLASH)) ))
						{
							if (!bMiniMapSquare)
							{
								bOnRadar = false;
							}
						}

						float fScale = fMaxDistance / f2DDistance;

						vNewBlipPosInMiniMap.x *= fScale;
						vNewBlipPosInMiniMap.y *= fScale;

						if (!bMiniMapSquare)
						{
							bOnEdgeOfRadar = true;
						}
					}
				}

				if (bMiniMapSquare)
				{
					float fCentreOffsetX = 0.0f;
					Vector2 vMax2dRadarDistanceSquare(0.459f, 0.42f);
					
					if (ms_MiniMapRenderState.m_bIsInPauseMap)
					{
						vMax2dRadarDistanceSquare = Vector2(0.5f, 0.5f);
					}
					else if (ms_MiniMapRenderState.m_bIsInCustomMap)
					{
						float x = 0.225000009f;
						float y = 0.5972222f;

						vMax2dRadarDistanceSquare = Vector2(x, y);
					}
					else if (ms_MiniMapRenderState.m_bIsInBigMap)
					{
						fCentreOffsetX = __BIGMAP_INTERNAL_ADJUSTMENT_BLIP_RANGE;
						vMax2dRadarDistanceSquare = Vector2(0.305f, 0.448f);
					}

					if( GetBlipTypeValue(pBlip) != BLIP_TYPE_RADIUS && (GetBlipTypeValue(pBlip) != BLIP_TYPE_AREA || flags.IsClear(BLIP_FLAG_SHORTRANGE)) )
					{
						if (vNewBlipPosInMiniMap.x > vMax2dRadarDistanceSquare.x-fCentreOffsetX)
						{
							iQuadrant = 0;
							vNewBlipPosInMiniMap.x = vMax2dRadarDistanceSquare.x-fCentreOffsetX;
							bOnEdgeOfRadar = true;
						}
						if (vNewBlipPosInMiniMap.y > vMax2dRadarDistanceSquare.y)
						{
							iQuadrant = 1;
							vNewBlipPosInMiniMap.y = vMax2dRadarDistanceSquare.y;
							bOnEdgeOfRadar = true;
						}
						if (vNewBlipPosInMiniMap.x < -(vMax2dRadarDistanceSquare.x+fCentreOffsetX))
						{
							iQuadrant = 2;
							vNewBlipPosInMiniMap.x = -(vMax2dRadarDistanceSquare.x+fCentreOffsetX);
							bOnEdgeOfRadar = true;
						}
						if (vNewBlipPosInMiniMap.y < -vMax2dRadarDistanceSquare.y)
						{
							iQuadrant = 3;
							vNewBlipPosInMiniMap.y = -vMax2dRadarDistanceSquare.y;
							bOnEdgeOfRadar = true;
						}

						if( GetBlipTypeValue(pBlip) == BLIP_TYPE_AREA )
						{
							// Extended area checks necessary for Square blips (and radius blips, if we did this [which we don't for V])
							bOnEdgeOfRadar = IsAreaBlipOutsideMapBounds(*pBlip);
						}


						// if these are short range blips and isnt flashing then dont lock to the edge (they dissapear off the radar)
						if (bOnEdgeOfRadar)
						{
							if (ms_MiniMapRenderState.m_bIsInPauseMap || ms_MiniMapRenderState.m_bIsInCustomMap)
							{
								bOnRadar = false;
							}
							else
							{
								if (flags.IsSet(BLIP_FLAG_SHORTRANGE) && (flags.IsClear(BLIP_FLAG_FLASHING)) && (flags.IsClear(BLIP_FLAG_VALUE_CHANGED_FLASH)))
								{
									bOnRadar = false;
								}
							}

							//blips on the edge cannot be drawn in the 3d format because they override vNewBlipPosInMiniMap
							bOn3dMap = false;
						}


						// fade out short range blips when in 3D mode and on standard minimap: (fixes todo 1166607)
						if ( (!ms_MiniMapRenderState.m_bIsInPauseMap) && (!ms_MiniMapRenderState.m_bIsInBigMap) && (!ms_MiniMapRenderState.m_bIsInCustomMap) && (ms_MiniMapRenderState.m_fAngle != 0.0f) )
						{
							if ( (bOnRadar) && (flags.IsSet(BLIP_FLAG_SHORTRANGE)) && (flags.IsClear(BLIP_FLAG_FLASHING)) && (flags.IsClear(BLIP_FLAG_ROUTE)) && (flags.IsClear(BLIP_FLAG_VALUE_CHANGED_FLASH)) )
							{
								if (vNewBlipPosInMiniMap.y < -0.4f)
								{
									bOnRadar = false;
								}
								else if (vNewBlipPosInMiniMap.y < -0.3f)
								{
									iDisplayAlpha = (s32)floor((0.4f-(-vNewBlipPosInMiniMap.y)) * 1000.0f);
								}
							}
						}
					}
				}

				// set the 2D blip final position:
				vNewBlipPosInMiniMap.x *= ms_vStageSize.x;
				vNewBlipPosInMiniMap.y *= ms_vStageSize.y;

				vNewBlipPosInMiniMap.x += (ms_vStageSize.x * 0.5f);
				vNewBlipPosInMiniMap.y += (ms_vStageSize.y * 0.5f);
			}
		}
	}

	//
	// now we know if we are visible or not on the minimap, we can return
	//
	if (bRangeCheckOnly)
	{
		return (bOnEdgeOfRadar);
	}

	const bool bLayerChanged = bOn3dMap!=GetIsPlacedOn3dMap(pBlip);

	//layer changed? refresh stage
	if( bLayerChanged )
	{
		SetIsPlacedOn3dMap(pBlip, bOn3dMap);
		ReInitBlipOnStage(pBlip);
		CMiniMap::GetAllFlags(flags, pBlip); // Flags may have changed, get new ones
	}


	GFxValue *pGfxValue = GetBlipGfxValue(pBlip);

	if (bOnRadar)
	{
		if (!pGfxValue)
		{
			if (AddBlipToStage(pBlip))  // add object to stage
			{
				pGfxValue = GetBlipGfxValue(pBlip);
				CMiniMap::GetAllFlags(flags, pBlip); // Flags may have changed, get the new ones
			}
		}
	}
	else
	{
		if (pGfxValue)
		{
			if (RemoveBlipFromStage(pBlip, false))  // remove object from stage
			{
				return false;  // we have now removed the blip so exit out early
			}
		}
	}

	if (bOnRadar && pGfxValue)
	{
		//
		// update the blip if it is on the stage:
		//
		uiAssertf(pGfxValue->IsDisplayObject(), "CMiniMap_RenderThread: Blip is not display object!");

		float fBlipRotation = -(GetBlipDirectionValue(pBlip));  // get blip rotation

		if (fBlipRotation == -DEFAULT_BLIP_ROTATION)  // if blip has no rotation...
		{
			if (bOn3dMap)
			{
				fBlipRotation = static_cast<float>(iMapRotation);  // ... then counter rotate by the map rotation to keep it upright
			}
		}
		// 3D blips don't get rotated
		else if(!bOn3dMap)
		{
			fBlipRotation -= iMapRotation;
		}

		// ensure the value is between 0 and 359 degrees (Note:  This actually wraps between 0 and 360)
		fBlipRotation = rage::Wrap(fBlipRotation, 0.0f, 360.0f);

		// 3D, edge-shrunked blips disregard their rotation, and instead put it on a quadrant
		if( bOnEdgeOfRadar && GetBlipTypeValue(pBlip) == BLIP_TYPE_AREA && flags.IsClear(BLIP_FLAG_SHORTRANGE))
		{
			fBlipRotation = 90.0f * (iQuadrant+1);
		}


		/*
		if (CMiniMap::GetUniqueBlipUsed(pBlip) == CMiniMap::GetUniqueCentreBlipId())
		{
			Displayf("new centre blip rot: %d    (map %d)    blip orig %d", iBlipRotation, iMapRotation, GetBlipDirectionValue(pBlip));
		}
		*/

		GFxValue pMainBlip;
		if (pGfxValue->GFxValue::GetMember("main_blip", &pMainBlip))
		{
			// 3D blips don't rotate their sub blip, they need to rotate the parent object
			// otherwise 2D shapes get weird (this may come up for other blips too)
			if(!GetIsPlacedOn3dMap(pBlip) && flags.IsClear(BLIP_FLAG_SHOW_HEADING_INDICATOR))
			{
				if (pMainBlip.IsDisplayObject())
				{
					GFxValue::DisplayInfo rotationInfo;
					rotationInfo.SetRotation(fBlipRotation);

					pMainBlip.SetDisplayInfo(rotationInfo);  // apply the rotation to the "main_blip" only
				}
			}
		}

		GFxValue::DisplayInfo info;
		if (GetIsPlacedOn3dMap(pBlip))
		{
			// need to rotate 3D blips' main blip instead of the subblip above
			info.SetRotation(fBlipRotation);

			// fMapRange was computed in the first "if (bOnRadar)" block
			if (ms_fBlipMapRange < MAP_BLIP_ROUND_THRESHOLD)
			{
				info.SetPosition(floor(vNewBlipPos.x+0.5f), floor(vNewBlipPos.y+0.5f));
			}
			else
			{
				info.SetPosition(vNewBlipPos.x, vNewBlipPos.y);
			}
		}
		else
		{	
			info.SetPosition(vNewBlipPosInMiniMap.x, vNewBlipPosInMiniMap.y);
		}

		Vector2 vBlipFinalSize;
		float fBlipScaler = GetBlipScalerValue(pBlip);

		if(GetBlipTypeValue(pBlip) == BLIP_TYPE_AREA )
		{
			if (bOnEdgeOfRadar)
			{
				//minimized radius edge blips shouldn't inherit alpha
				// and this is OKAY to do, because all the blips are currently copied to be rendered
				// so the original's data is untouched
				pBlip->iAlpha = 0xFF;
				fBlipScaler = GetBlipScalerValue(pBlip, false);
				vBlipFinalSize.Set( fBlipScaler, fBlipScaler); // if on the edge, shrink it to match what others look like
			}
			else
			{
				vBlipFinalSize.Set( GetBlipScaleValue(pBlip) );
			}
		}
		else if (GetBlipTypeValue(pBlip) == BLIP_TYPE_RADIUS)
		{
			vBlipFinalSize.Set( GetBlipScaleValue(pBlip) * 2.0f );  // radius needs to be diameter for SetScale
		}
		else
		{
			vBlipFinalSize.Set( GetBlipScaleValue(pBlip) * fBlipScaler );

			// if blip is a player blip then reduce to 50% on edge of minimap
			// if blip is considered "dead" then 50% size
			if ((flags.IsSet(BLIP_FLAG_DEAD)) ||
				(bOnEdgeOfRadar && flags.IsSet(BLIP_FLAG_MINIMISE_ON_EDGE)))
			{
				vBlipFinalSize *= 0.5f;
			}
		}

		if (GetBlipObjectNameId(pBlip) == BLIP_LEVEL && !bOn3dMap)
		{
			vBlipFinalSize *= 0.8f;  // these blips are always 80% of the original size
		}

		info.SetScale(vBlipFinalSize.x, vBlipFinalSize.y);

		if(pGfxValue->GFxValue::HasMember("textLabel"))
		{
			GFxValue gfxTextLabel;
			if (pGfxValue->GFxValue::GetMember("textLabel", &gfxTextLabel))
			{
				// Counter-scale text label so the size remains consistent
				GFxValue::DisplayInfo textLabelInfo;

				float fTextLabelScale = fBlipScaler / vBlipFinalSize.x * 100;
				textLabelInfo.SetScale(fTextLabelScale, fTextLabelScale);

				gfxTextLabel.SetDisplayInfo(textLabelInfo);
			}
		}
		
		pGfxValue->SetDisplayInfo(info);  // apply the changes

		if (flags.IsSet(BLIP_FLAG_UPDATE_STAGE_DEPTH))
		{
			GFxValue newDepth;
			newDepth.SetNumber( DetermineBlipStageDepth(pBlip) );
			pGfxValue->Invoke("swapDepths", nullptr, &newDepth, 1);
		}

		//
		// now check for anything that requires an invoke (a flag must be checked before invoking):
		//
		if (flags.IsSet(BLIP_FLAG_VALUE_CHANGED_COLOUR) || flags.IsSet(BLIP_FLAG_UPDATE_ALPHA_ONLY) )
		{
			ColourBlipOnStage(pGfxValue, pBlip);
		}
		//if (flags.IsSet(BLIP_FLAG_SHOW_HEADING_INDICATOR))
		{
			UpdateHeadingIndicatorOnBlipOnStage(pGfxValue, pBlip, iMapRotation);
		}
		if (flags.IsSet(BLIP_FLAG_VALUE_CHANGED_OUTLINE_INDICATOR))
		{
			UpdateOutlineIndicatorOnBlipOnStage(pGfxValue, pBlip);
		}
		if (flags.IsSet(BLIP_FLAG_VALUE_CHANGED_FRIEND_INDICATOR))
		{
			UpdateFriendIndicatorOnBlipOnStage(pGfxValue, pBlip);
		}
		if (flags.IsSet(BLIP_FLAG_VALUE_CHANGED_CREW_INDICATOR))
		{
			UpdateCrewIndicatorOnBlipOnStage(pGfxValue, pBlip);
		}
		if (flags.IsSet(BLIP_FLAG_VALUE_CHANGED_TICK))
		{
			UpdateTickBlipOnStage(pGfxValue, pBlip);
		}
		if( flags.IsSet(BLIP_FLAG_VALUE_CHANGED_GOLD_TICK))
		{
			UpdateGoldTickBlipOnStage(pGfxValue, pBlip);
		}
		if (flags.IsSet(BLIP_FLAG_VALUE_CHANGED_FOR_SALE))
		{
			UpdateForSaleBlipOnStage(pGfxValue, pBlip);
		}
		if (flags.IsSet(BLIP_FLAG_VALUE_CHANGED_PULSE))
		{
			PulseBlipOnStage(&pMainBlip, pBlip);
		}
		if (flags.IsSet(BLIP_FLAG_VALUE_CHANGED_FLASH))
		{
			FlashBlipOnStage(pGfxValue, pBlip);
		}
#if __BANK
		if (flags.IsSet(BLIP_FLAG_VALUE_SET_LABEL))
		{
			LabelBlipOnStage(pGfxValue, pBlip, true);
		}
		if (flags.IsSet(BLIP_FLAG_VALUE_REMOVE_LABEL))
		{
			LabelBlipOnStage(pGfxValue, pBlip, false);
		}
#endif // __BANK
		if (flags.IsSet(BLIP_FLAG_VALUE_SET_NUMBER))
		{
			NumberBlipOnStage(&pMainBlip, pBlip);
		}

		FadeOutDistantBlipOnStage(pGfxValue, iDisplayAlpha);

		if (GetBlipCategoryValue(pBlip) == BLIP_CATEGORY_PLAYER)  // only blips that are in the player category
		{
			ProcessPlayerNameDisplayOnBlip(pGfxValue, pBlip, bOnEdgeOfRadar);
		}

		//
		// check for changing the height display of a blip:
		//
		if (flags.IsSet(BLIP_FLAG_SHOW_HEIGHT))
		{
			bool bShowHeight = false;
			s32 iBlipHeight = 0;

			if (GetBlipTypeValue(pBlip) != BLIP_TYPE_RADIUS && GetBlipTypeValue(pBlip) != BLIP_TYPE_AREA)
			{
				if ( (bOnRadar) && (!ms_MiniMapRenderState.m_bIsInPauseMap)/* && (!ms_MiniMapRenderState.m_bIsInBigMap)*/)  // bug 623508 requests no height shown in pausemenu map
				{
					if (CMiniMap::IsFlagSet(pBlip, BLIP_FLAG_USE_HEIGHT_ON_EDGE))
					{
						bShowHeight = true;
					}
					else
					{
						// we only show the height indicator if the blip is inside the radar area or the player is in a vehicle that has a large change of height (ie a plane), or inside an interior
						if ((!bOnEdgeOfRadar) || (ms_MiniMapRenderState.bInsideInterior))
						{
							// always show height if inside the radar area
							bShowHeight = true;
						}
						else
						{
							// check if in a plane/heli
							if (ms_MiniMapRenderState.bInPlaneOrHeli || ms_MiniMapRenderState.bIsInParachute)  // fix for 1430446
							{
								bShowHeight = true;
							}
						}
					}
				}
			}

			// if its a weapon blip then only show it if its within 15m (for bug 980784)
			if (bShowHeight)  // only if we are already wanting to show height 
			{
				if (!strncmp(WEAPON_BLIP_NAME, GetBlipObjectName(pBlip), NELEM(WEAPON_BLIP_NAME)-1))
				{
					bShowHeight = bBlipWithin15mOfPlayer;
				}
			}

			if (bShowHeight)  // height indicator only happens when the blip is inside the radar ring
			{
				iBlipHeight = CheckForHeight(pBlip);  // whether this blip is above, below, level to the player
			}

			if (iBlipHeight != 0)
			{
				AddHigherLowerBlip(pGfxValue, pBlip, iBlipHeight);
			}
			else
			{
				RemoveHigherLowerBlip(pGfxValue);
			}
		}
		else if (flags.IsSet(BLIP_FLAG_VALUE_TURNED_OFF_SHOW_HEIGHT))
		{
			RemoveHigherLowerBlip(pGfxValue);
			flags.Set(BLIP_FLAG_VALUE_TURNED_OFF_SHOW_HEIGHT, false);
		}
	}

	return true;
}

// Returns whether the area blip is outside of minimap bounds
bool CMiniMap_RenderThread::IsAreaBlipOutsideMapBounds(const CBlipComplex& rBlip)
{
	// NOTE: Unlike on RDR, all the world positions had to be inverted
	// Not sure why, but... didn't work otherwise.

	// construct box also in world space, which is easy
	ScalarV boxRotate( ScalarVFromF32(GetBlipDirectionValue(&rBlip) * DtoR) );
	Mat34V boxLocalRotate;
	// translation inverted so the box can transform stuff into its local space
	// Removed the - from the positions....I don't know why they were in there but it didn't work with them....B*2173220
	Mat34VFromZAxisAngle( boxLocalRotate, boxRotate, Vec3V(-GetBlipPositionConstRef(&rBlip).x, -GetBlipPositionConstRef(&rBlip).y, 0.0f) );

	const float boxHeight = 0.0f;
	const float fRadarSizeScalarForEdgeBlips = 0.95f; // give it a LITTLE bit of gimme room, so we don't have tiny pixels hanging out off screen

	const float diameterScalar =  0.5f * fRadarSizeScalarForEdgeBlips;

	const Vec3V vBoxHalfDims(GetBlipScaleValue(&rBlip).x*diameterScalar, GetBlipScaleValue(&rBlip).y*diameterScalar, boxHeight);
	const spdAABB aabb(-vBoxHalfDims, vBoxHalfDims);
	const spdOrientedBB oriRect( aabb, boxLocalRotate );

	ScalarV mapRotate( ScalarVFromF32(ms_MiniMapRenderState.m_iRotation * DtoR) );

#if SUPPORT_MULTI_MONITOR
	MultiMonitorHudHelper helper(&ms_MiniMapRenderState);
#endif // SUPPORT_MULTI_MONITOR

	Vec3V mapPosition(ms_MiniMapRenderState.m_vMiniMapPosition.x, ms_MiniMapRenderState.m_vMiniMapPosition.y, 0.0f);
	Vec3V offsetInWorld(0.0f, ADJUST_SCALEFORM_SCALAR(ms_MiniMapRenderState.m_fMiniMapRange) * ms_MiniMapRenderState.m_fOffset, 0.0f);
	Vec3V offsetRotated( RotateAboutZAxis( offsetInWorld, mapRotate ) );
	mapPosition += offsetRotated;
	Mat34V mapLocalRotate;
	Mat34VFromZAxisAngle( mapLocalRotate, mapRotate, -mapPosition );

	GFxValue curXTilt;
	if( ms_asBaseLayerContainer[MINIMAP_LAYER_FOREGROUND].GetMember("_xrotation", &curXTilt) )
	{
		Mat34VRotateLocalX(mapLocalRotate, ScalarVFromF32( float(-curXTilt.GetNumber()) * DtoR ) );
	}

	// We need this in 'world' units. The map is 1:1 pixels to meters, so if we found out how big the map is in pixels, we're all set!
	// 

	// lifted from SetMaskOnLayer, because we need how it clips this to match
	Vector2 maskSize( ms_MiniMapRenderState.m_vCurrentMiniMapMaskSize );
	float fMapRatio = ms_vOriginalStageSize.x / ms_vOriginalStageSize.y;
	const float fScaler = fMapRatio/CHudTools::GetAspectRatio();

	maskSize.x /= ms_MiniMapRenderState.m_vCurrentMiniMapSize.x; maskSize.y /= ms_MiniMapRenderState.m_vCurrentMiniMapSize.y;
	maskSize.x *= ms_vOriginalStageSize.x/fScaler;	maskSize.y *= ms_vOriginalStageSize.y;

	// then, now that we have it in 'map meters', we just adjust it by the scalar to see how big the area is in world meters!
	const Vec3V vMapHalfDims(  ADJUST_SCALEFORM_SCALAR(ms_MiniMapRenderState.m_fMiniMapRange) * maskSize.x * 0.5f
		, ADJUST_SCALEFORM_SCALAR(ms_MiniMapRenderState.m_fMiniMapRange) * maskSize.y * 0.5f
		, 1000.0f ); // fake a frustum by picking a rather large (?) value
	const spdAABB mapAABB(-vMapHalfDims, vMapHalfDims);
	const spdOrientedBB mapRect( mapAABB, mapLocalRotate );

	bool bOnEdgeOfRadar = !oriRect.IntersectsOrientedBB(mapRect);

#if DEBUG_DRAW && __BANK
	// if this the first one, push the minimap's settings
	if( CMiniMap_RenderThread::sm_bDrawCollisionVolumes )
	{
		if( s_DebugDrawVolumes.GetRenderBuf().empty() )
			s_DebugDrawVolumes.GetRenderBuf().PushAndGrow(debugVolume(mapRect, Color_WhiteSmoke), 4);

		Color32 blipColor = CMiniMap_Common::GetColourFromBlipSettings(GetBlipColourValue(&rBlip), CMiniMap::IsFlagSet(&rBlip,BLIP_FLAG_BRIGHTNESS));
		s_DebugDrawVolumes.GetRenderBuf().PushAndGrow(debugVolume(oriRect, blipColor, !bOnEdgeOfRadar), 4);
	}
#endif

	return bOnEdgeOfRadar;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::ProcessPlayerNameDisplayOnBlip
// PURPOSE: whether to attach or remove the player name display on a blip
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::ProcessPlayerNameDisplayOnBlip(GFxValue *pBlipGfx, CBlipComplex *pBlip, bool bOnEdgeOfRadar)
{
	if (!pBlip)  // safeguard against invalid blips
	{
		return;
	}

	//
	// check to see if we need to display the blip name against the blip (ie player blips)
	//
	bool bSetBlipLabelThisFrame = false;
	bool bRemoveBlipLabelThisFrame = false;

	bool bHasNameBeenAttachedPreviously = false;
		
	if (pBlipGfx)
	{
		bHasNameBeenAttachedPreviously = pBlipGfx->GFxValue::HasMember("textLabel");
	}

	if ( (ms_MiniMapRenderState.m_bDisplayPlayerNames) && (!ms_MiniMapRenderState.m_bIsInPauseMap) && (ms_MiniMapRenderState.m_bIsInBigMap) )
	{
		if (bHasNameBeenAttachedPreviously)
		{
			if (!bOnEdgeOfRadar)
			{
				if (!IsBlipNameTagWithinBounds(pBlipGfx, pBlip))
				{
					bRemoveBlipLabelThisFrame = true;
				}
			}
			else
			{
				bRemoveBlipLabelThisFrame = true;
			}
		}
		else
		{
			if (!bOnEdgeOfRadar)  // if within the zone, not already attached then flag to add
			{
				bSetBlipLabelThisFrame = true;
			}
		}

	}
	else
	{
		bRemoveBlipLabelThisFrame = bHasNameBeenAttachedPreviously;  // if no longer needing names at all, then flag to remove it it has been previously added
	}

	if (pBlipGfx)
	{
		// if flagged to be added, then invoke scaleform
		if (bSetBlipLabelThisFrame)
		{
			if (CScaleformMgr::BeginMethod(ms_iMovieId[MINIMAP_MOVIE_FOREGROUND], SF_BASE_CLASS_MINIMAP, "SET_BLIP_LABEL"))
			{
				CScaleformMgr::AddParamGfxValue(*pBlipGfx);
				CScaleformMgr::AddParamString(GetBlipNameValue(pBlip));
				GFxValue::DisplayInfo blipDisplayInfo;
				pBlipGfx->GetDisplayInfo(&blipDisplayInfo);
				CScaleformMgr::AddParamFloat(GetBlipScalerValue(pBlip) / blipDisplayInfo.GetXScale() * 100);
				CScaleformMgr::EndMethod();
			}

			if (!IsBlipNameTagWithinBounds(pBlipGfx, pBlip))
			{
				if (CScaleformMgr::BeginMethod(ms_iMovieId[MINIMAP_MOVIE_FOREGROUND], SF_BASE_CLASS_MINIMAP, "REMOVE_BLIP_LABEL"))
				{
					CScaleformMgr::AddParamGfxValue(*pBlipGfx);
					CScaleformMgr::EndMethod();
				}
			}
		}

		// if flagged to be removed, then invoke scaleform
		if (bRemoveBlipLabelThisFrame)
		{
			if (CScaleformMgr::BeginMethod(ms_iMovieId[MINIMAP_MOVIE_FOREGROUND], SF_BASE_CLASS_MINIMAP, "REMOVE_BLIP_LABEL"))
			{
				CScaleformMgr::AddParamGfxValue(*pBlipGfx);
				CScaleformMgr::EndMethod();
			}
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::IsBlipNameTagWithinBounds
// PURPOSE: is the blip name tag within the bounds of the minimap zone
/////////////////////////////////////////////////////////////////////////////////////
bool CMiniMap_RenderThread::IsBlipNameTagWithinBounds(GFxValue *pBlipGfx, CBlipComplex *pBlip)
{
	if ( (!pBlip) || (!pBlipGfx) )
		return false;

	GFxValue::DisplayInfo displayInfo;
	pBlipGfx->GetDisplayInfo(&displayInfo);

	Vector2 vPosOnStage = Vector2((float)displayInfo.GetX(), (float)displayInfo.GetY());

	GFxValue gfxTextLabel;
	if (pBlipGfx->GFxValue::GetMember("textLabel", &gfxTextLabel))
	{
		GFxValue gfxTextLabelBG;

		if (gfxTextLabel.GetMember("bg", &gfxTextLabelBG))
		{
			float fNameLength = 0.0f;
			GFxValue nameTagWidth;
			gfxTextLabelBG.GetMember("_width", &nameTagWidth);

			if(nameTagWidth.IsDefined())
			{
#define GAP_BETWEEN_BLIP_AND_BORDER (20.0f)
				// need to convert from "actionscript space" to the minimap scaled space:
				fNameLength = static_cast<float>(( (nameTagWidth.GetNumber() + GAP_BETWEEN_BLIP_AND_BORDER) / (float)ACTIONSCRIPT_STAGE_SIZE_X) * MINIMAP_INITIAL_SCALE);

				// deal with adjusting this length to the correct scaled side:
				fNameLength *= GetBlipScaleValue(pBlip).x;  // scale for blip scaling

				if (GetBlipObjectNameId(pBlip) == BLIP_LEVEL)
				{
					fNameLength *= 0.8f;  // these blips are always 80% of the original size
				}
			}

			// these values match the position in the AS stage where the name shouldnt display
			if ((vPosOnStage.x-fNameLength) < 8.0f ||  // check if within bounds
				vPosOnStage.y < 17.0f ||
				vPosOnStage.x > 112.0f ||
				vPosOnStage.y > 119.0f)
			{
				return false;
			}
			else
			{
				return true;  // within bounds
			}
		}
	}

	return false;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::ReInitBlipOnStage
// PURPOSE: re-initialises the blips that are currently on the stage
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::ReInitBlipOnStage(CBlipComplex *pBlip)
{
 	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
 	{
 		sfAssertf(0, "CMiniMap_RenderThread::ReInitBlipOnStage can only be called on the RenderThread!");
 		return;
 	}

	if (pBlip)
	{
		if (GetBlipGfxValue(pBlip))
		{
			if (RemoveBlipFromStage(pBlip, false))
			{
				AddBlipToStage(pBlip); 
			}
			else
			{
				uiAssertf(0, "CMiniMap_RenderThread: Reinit blip on stage couldnt complete!");
			}
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::UpdateAltimeter
// PURPOSE:	updates the altimeter
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::UpdateAltimeter()
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		sfAssertf(0, "CMiniMap_RenderThread::UpdateAltimeter can only be called on the RenderThread!");
		return;
	}

	if (ms_iMovieId[MINIMAP_MOVIE_FOREGROUND] == -1)
		return;

//	bool bSwitchOff = true;
	bool bDrawAltimeter = (ms_MiniMapRenderState.m_AltimeterMode != ALTIMETER_OFF);

	if( bDrawAltimeter )
	{
		// remember that -Y is UP, so the TOP is actually the lower value
		// whereas the bottom is a higher value
		const float fHeight = (ms_MiniMapRenderState.m_vCentrePosition.z + ALTIMETER_GROUND_LEVEL_FUDGE_FACTOR);

		//
		// Background
		//
		
		const float fFloorLevel		= ms_MiniMapRenderState.m_fMinAltimeterHeight;
		const float fCeilingLevel	= ms_MiniMapRenderState.m_fMaxAltimeterHeight;

		const float fPercent = ms_MiniMapRenderState.m_AltimeterMode == ALTIMETER_SWIMMING ? rage::Range(fHeight, fCeilingLevel, fFloorLevel) : rage::Range(fHeight, fFloorLevel, fCeilingLevel);

		GFxValue::DisplayInfo infoGauge;
		const float fPercentPixels = (fPercent - 0.5f) * ALTIMETER_TICK_PIXEL_HEIGHT;// 0.5f to adjust for the registration point being centered
		infoGauge.SetY( fPercentPixels );
		ms_Altimeter.m_asGaugeMc.SetDisplayInfo(infoGauge);

		GFxValue altimeter_colour_mc;
		if (ms_Altimeter.m_asGaugeMc.GFxValue::GetMember("altimeter_colour", &altimeter_colour_mc))
		{
			GFxValue::DisplayInfo visibilityDisplayInfo;
			visibilityDisplayInfo.SetVisible(ms_MiniMapRenderState.m_bColourAltimeter);

			altimeter_colour_mc.SetDisplayInfo(visibilityDisplayInfo);

			GFxValue ColorArg[1];
			GFxValue result;
			ColorArg[0].SetNumber((int)(ms_MiniMapRenderState.m_AltimeterColor));
			altimeter_colour_mc.Invoke("Recolor", &result, ColorArg, COUNTOF(ColorArg));
		}

		//
		// asAltimeterRoll:
		//
		GFxValue::DisplayInfo infoRoll;
		infoRoll.SetVisible( ms_MiniMapRenderState.m_AltimeterMode != ALTIMETER_OFF );
		infoRoll.SetRotation(ms_MiniMapRenderState.fRoll * RtoD);
		ms_Altimeter.m_asRollMc.SetDisplayInfo(infoRoll);
	}

	// determine visibility for the whole shebang
	{
		GFxValue::DisplayInfo drawInfo;
		drawInfo.SetVisible(bDrawAltimeter);
		drawInfo.SetPosition((ms_vStageSize.x * 0.5f), (ms_vStageSize.y * 0.5f) + ms_MiniMapRenderState.m_fOffset);
		ms_Altimeter.m_asContainerMc.SetDisplayInfo(drawInfo);
	}

	//	Show/Hide all four runways
	if( ms_MiniMapRenderState.bInPlaneOrHeli != ms_bRunwayBlipsAreDisplaying )
	{	
		ms_bRunwayBlipsAreDisplaying = ms_MiniMapRenderState.bInPlaneOrHeli;
		
		if (CScaleformMgr::BeginMethod(ms_iMovieId[MINIMAP_MOVIE_BACKGROUND], SF_BASE_CLASS_MINIMAP, "SHOW_AIRCRAFT_COMPONENTS"))
		{
			CScaleformMgr::AddParamBool(ms_bRunwayBlipsAreDisplaying);
			CScaleformMgr::EndMethod();
		}
	}
}

void CMiniMap_RenderThread::UpdateSonarSweep()
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		sfAssertf(0, "CMiniMap_RenderThread::UpdateSonarSweep can only be called on the RenderThread!");
		return;
	}

	if (ms_iMovieId[MINIMAP_MOVIE_FOREGROUND] == -1)
		return;

	if(ms_MiniMapRenderState.m_bShowSonarSweep)
	{
		const bool bHideSonarSweep = ms_MiniMapRenderState.m_bIsInBigMap;
		const bool bShowSonarSweep = ms_MiniMapRenderState.m_bShowSonarSweep && !bHideSonarSweep;
		if (CScaleformMgr::BeginMethod(ms_iMovieId[MINIMAP_MOVIE_FOREGROUND], SF_BASE_CLASS_MINIMAP, "SHOW_SONAR_SWEEP"))
		{
			CScaleformMgr::AddParamBool(bShowSonarSweep);
			CScaleformMgr::EndMethod();
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::UpdateMapVisibility
// PURPOSE:	makes sure the prologue map is displayed instead of the standard one
//			when script set it, or the main map is turned off
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::UpdateMapVisibility()
{
	if (ms_iMovieId[MINIMAP_MOVIE_FOREGROUND] == -1)
		return;

	GFxValue mainMap;
	GFxValue::DisplayInfo currentMainMapDisplayInfo;

	ms_asMapObject.GFxValue::GetMember("main_map", &mainMap);
	mainMap.GetDisplayInfo(&currentMainMapDisplayInfo);

	// show/hide the prologue map:
	if (ms_MiniMapRenderState.m_bShowPrologueMap)
	{
#if COMPANION_APP
		CCompanionData::GetInstance()->SetMapDisplay(eMapDisplay::MAP_DISPLAY_PROLOGUE);
#endif	//	COMPANION_APP
	}

	// show/hide the island map:
	if (ms_MiniMapRenderState.m_bShowIslandMap)
	{
#if COMPANION_APP
		CCompanionData::GetInstance()->SetMapDisplay(eMapDisplay::MAP_DISPLAY_ISLAND);
#endif	//	COMPANION_APP
	}


	//
	// show/hide the main map:
	//
	if (ms_MiniMapRenderState.m_bShowMainMap)
	{
#if COMPANION_APP
		CCompanionData::GetInstance()->SetMapDisplay(eMapDisplay::MAP_DISPLAY_NORMAL);
#endif	//	COMPANION_APP
		if (!currentMainMapDisplayInfo.GetVisible())
		{
			GFxValue::DisplayInfo newDisplayInfo;
			newDisplayInfo.SetVisible(true);
			mainMap.SetDisplayInfo(newDisplayInfo);
			g_RenderMinimap = true;

			if ( (!ms_MiniMapRenderState.m_bIsInPauseMap) && (!ms_MiniMapRenderState.m_bIsInCustomMap) )
			{
				g_RenderMinimapSea = true;
			}
		}
	}
	else
	{
		if (currentMainMapDisplayInfo.GetVisible())
		{
			GFxValue::DisplayInfo newDisplayInfo;
			newDisplayInfo.SetVisible(false);
			mainMap.SetDisplayInfo(newDisplayInfo);
			g_RenderMinimap = false;
			g_RenderMinimapSea = false;
		}
	}

}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::CheckIncomingFunctions
// PURPOSE:	checks for incoming functions from ActionScript
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::CheckIncomingFunctions(atHashWithStringBank methodName, const GFxValue* args)
{
	// INTERIOR_IS_LOADED
	if (methodName == ATSTRINGHASH("INTERIOR_IS_LOADED",0xad2c4c8))
	{
		if (ms_asInteriorMc.IsDisplayObject())  // ignore this until its been invoked with LoadMovie - where ms_asInteriorMc is attached
		{
			ms_asInteriorMovie = args[1];
		}
		return;
	}

	// GOLF_COURSE_IS_LOADED
	if (methodName == ATSTRINGHASH("GOLF_COURSE_IS_LOADED",0x9a61c818))
	{
		if (ms_asGolfCourseMc.IsDisplayObject())  // ignore this until its been invoked with LoadMovie - where ms_asGolfCourseMc is attached
		{
			ms_asGolfCourseMovie = args[1];
		}
		return;
	}

#if __STREAMED_SUPERTILE_FILES
	if (methodName == ATSTRINGHASH("SUPERTILE_IS_LOADED",0xc72454db))
	{
		ms_Supertiles.HandleIsLoadedCallback((s32)args[1].GetNumber(), (s32)args[2].GetNumber(), args[3]);
		return;
	}
#endif	//	__STREAMED_SUPERTILE_FILES
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::RemoveInterior
// PURPOSE: creates the GFxValue object as the interior is now active on the stage
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::RemoveInterior()
{
	if (ms_iMovieId[MINIMAP_MOVIE_FOREGROUND] == -1)
		return;

	ms_bInteriorMovieIdFullySetup = false;
	ms_bInteriorWasSetOnPerFrame = false;

	if (ms_asInteriorMc.IsDisplayObject())
	{
		ms_asInteriorMc.Invoke("removeMovieClip");
	}

	ms_asInteriorMc.SetUndefined();
	ms_asInteriorMovie.SetUndefined();

	ms_PreviousInterior.iLevel = sMiniMapInterior::INVALID_LEVEL;

	if (ms_iInteriorMovieId != -1)
	{
		ms_bRemoveInteriorMovie = true;
	}

	uiDebugf1("MiniMap: Interior removed from stage");
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::UpdateInterior
// PURPOSE:	updates the minimap with any information interiors/exteriors
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::UpdateInterior()
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		uiAssertf(0, "CMiniMap_RenderThread::UpdateInterior can only be called on the RenderThread!");
		return;
	}

	if (ms_iMovieId[MINIMAP_MOVIE_FOREGROUND] == -1)
		return;

	if (ms_PreviousInterior.ResolveDifferentHash(ms_MiniMapRenderState.m_Interior))
	{
		if (ms_MiniMapRenderState.m_Interior.uHash != 0)
		{
#if __BANK &&  __DEV
			if (bDisplayInteriorInfoToLog)
			{
				Displayf("Interior info for minimap is: %u (%d)", ms_MiniMapRenderState.m_Interior.uHash, ms_MiniMapRenderState.m_Interior.uHash);
			}
#endif  // __BANK && __DEV

			RemoveInterior();

			ms_bStreamInteriorMovie = true;
		}
		else
		{
			RemoveInterior();
		}
	}

	if (ms_bStreamInteriorMovie || ms_bRemoveInteriorMovie)
		return;

	GFxValue asInteriorLayer;
	if (!ms_asBaseOverlay3D[MINIMAP_LAYER_BACKGROUND].GetMember("asInteriorLayer", &asInteriorLayer))
	{
		uiAssertf(0, "CMiniMap_RenderThread::UpdateInterior cannot find member 'asInteriorLayer'");
		return;
	}

	//
	// no interior map should be shown when not in an interior:
	//
	GFxValue::DisplayInfo displayInfo;
	asInteriorLayer.GetDisplayInfo(&displayInfo);

	if (ms_MiniMapRenderState.m_Interior.uHash == 0 || ms_MiniMapRenderState.m_bHideInteriorMap)
	{
		// hide
		if (displayInfo.GetVisible())
		{
			GFxValue::DisplayInfo newInteriorDisplayInfo;
			newInteriorDisplayInfo.SetVisible(false);
			asInteriorLayer.SetDisplayInfo(newInteriorDisplayInfo);
		}
	}
	else
	{
		// show
		if (!displayInfo.GetVisible())
		{
			GFxValue::DisplayInfo newInteriorDisplayInfo;
			newInteriorDisplayInfo.SetVisible(true);
			asInteriorLayer.SetDisplayInfo(newInteriorDisplayInfo);
		}
	}

	if (CScaleformMgr::IsMovieActive(ms_iInteriorMovieId))
	{
		if ((!ms_asInteriorMovie.IsDisplayObject()) || (ms_asInteriorMovie.IsDisplayObject() && (!ms_asInteriorMovie.HasMember("interior"))))
		{
			char cHashName[20];
			formatf(cHashName, "INT%u", ms_MiniMapRenderState.m_Interior.uHash, NELEM(cHashName));

			if (ms_asInteriorMc.IsUndefined())
			{
				// invoke the load here
				GFxValue result, params[1]; 
				params[0].SetString(cHashName); 

				asInteriorLayer.CreateEmptyMovieClip(&ms_asInteriorMc, "INTERIOR_LINK", MOVIE_DEPTH_INTERIOR);

				ms_asInteriorMc.Invoke("loadMovie", &result, params, 1);
			}

			if (ms_asInteriorMc.IsDisplayObject())
			{
				if (ms_asInteriorMovie.IsDisplayObject())
				{
					if (ms_asInteriorMovie.GotoAndStop(cHashName))
					{
						if (ms_asInteriorMovie.HasMember("interior"))
						{
							Vector3 vPos(0,0,0);
							float fRot = 0.0f;

#if __BANK
							if (!ms_MiniMapRenderState.m_bHasDebugInterior)
#endif // __BANK
							{
								// get current interior rotation and position:
								vPos = ms_MiniMapRenderState.m_Interior.vPos;
								fRot = ms_MiniMapRenderState.m_Interior.fRot;
							}

							// set the position:
							GFxValue::DisplayInfo currentDisplayInfo;
							currentDisplayInfo.SetPosition((vPos.x), -(vPos.y));
							currentDisplayInfo.SetRotation(fRot);

							if (ms_MiniMapRenderState.m_bShowPrologueMap)  // fix for 986642
							{
								currentDisplayInfo.SetScale(400.0f, 400.0f);
							}
							else if (ms_MiniMapRenderState.m_bShowIslandMap)
							{
								currentDisplayInfo.SetScale(100.0f, 100.0f);
							}
							else
							{
								currentDisplayInfo.SetScale(100.0f, 100.0f);
							}

							ms_asInteriorMovie.SetDisplayInfo(currentDisplayInfo);

							ms_bInteriorMovieIdFullySetup = true;
							uiDebugf1("CMiniMap_RenderThread: Interior %s added to stage", cHashName);
						}
						else
						{
							uiDebugf1("CMiniMap_RenderThread: Interior data for %s is invalid - unable to set up Interior on stage (probably movieclip is missing member 'interior')", cHashName);

							RemoveInterior();  // remove anything that was setup

							return;
						}
					}
					else
					{
						uiDebugf1("CMiniMap_RenderThread: Interior %s is invalid - probably doesnt exist (gotoandstop(%s) failed)", cHashName, cHashName);
					}
				}
			}
		}

		//
		// make current level in the interior visible and hide the previous interior level:
		//
		if (ms_asInteriorMovie.IsDisplayObject() && ms_asInteriorMovie.HasMember("interior"))
		{
			GFxValue theinterior;
			ms_asInteriorMovie.GetMember("interior", &theinterior);

			s32 iCurrentInteriorLevel = ms_MiniMapRenderState.m_Interior.iLevel;
			s32 iPreviousInteriorLevel = ms_PreviousInterior.iLevel;

			if(ms_PreviousInterior.ResolveDifferentOrientation(ms_MiniMapRenderState.m_Interior))
			{
				GFxValue::DisplayInfo currentDisplayInfo;
				currentDisplayInfo.SetPosition(ms_MiniMapRenderState.m_Interior.vPos.x, -(ms_MiniMapRenderState.m_Interior.vPos.y));
				currentDisplayInfo.SetRotation(ms_MiniMapRenderState.m_Interior.fRot);
				ms_asInteriorMovie.SetDisplayInfo(currentDisplayInfo);
			}

			if( ms_PreviousInterior.ResolveDifferentLevels(ms_MiniMapRenderState.m_Interior) )
			{
				char cCurrentLevelString[50];

				if (iCurrentInteriorLevel >= 0)
					formatf(cCurrentLevelString, "level_%d", iCurrentInteriorLevel, NELEM(cCurrentLevelString));
				else
					formatf(cCurrentLevelString, "level_u%d", -iCurrentInteriorLevel, NELEM(cCurrentLevelString));

				if (ms_PreviousInterior.iLevel != sMiniMapInterior::INVALID_LEVEL)  // if we have a previous level
				{
					char cPreviousLevelString[50];
					if (iPreviousInteriorLevel >= 0)
						formatf(cPreviousLevelString, "level_%d", iPreviousInteriorLevel, NELEM(cPreviousLevelString));
					else
						formatf(cPreviousLevelString, "level_u%d", -iPreviousInteriorLevel, NELEM(cPreviousLevelString));

					if (theinterior.HasMember(cPreviousLevelString))
					{
						GFxValue thisLevel;
						theinterior.GFxValue::GetMember(cPreviousLevelString, &thisLevel);

						GFxValue::DisplayInfo info;
						info.SetAlpha(0);
						thisLevel.SetDisplayInfo(info);

						uiDebugf1("CMiniMap_RenderThread: Removed interior '%s'", cPreviousLevelString);
					}
#if __DEV
					else
					{
						uiDebugf1("CMiniMap_RenderThread: Tried to remove interior '%s' but it doesnt exist", cPreviousLevelString);
					}
#endif // __DEV
				}

				if (theinterior.HasMember(cCurrentLevelString))
				{
					GFxValue thisLevel;
					theinterior.GFxValue::GetMember(cCurrentLevelString, &thisLevel);

					GFxValue::DisplayInfo info;
					info.SetAlpha(100);
					thisLevel.SetDisplayInfo(info);

					uiDebugf1("CMiniMap_RenderThread: Activated interior '%s'", cCurrentLevelString);
				}
#if __DEV
				else
				{
					uiDebugf1("CMiniMap_RenderThread: Tried to activate interior '%s' but it doesnt exist", cCurrentLevelString);
				}
#endif // __DEV

			}
		}
	}
}

void CMiniMap_RenderThread::HandleInteriorAtEndOfFrame()
{
	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on UT
	{
		sfAssertf(0, "CMiniMap_RenderThread::HandleInteriorsAtEndOfFrame can only be called on the UpdateThread!");
		return;
	}

	// 
	// removal/stream of interior movie
	//
	if (ms_bRemoveInteriorMovie || ms_bStreamInteriorMovie)
	{
		if (ms_iInteriorMovieId != -1)
		{
			if (CScaleformMgr::IsMovieActive(ms_iInteriorMovieId))
			{
				CScaleformMgr::RequestRemoveMovie(ms_iInteriorMovieId);
			}

			ms_iInteriorMovieId = -1;
		}

		ms_bRemoveInteriorMovie = false;
	}

	if (ms_bStreamInteriorMovie)
	{
		char cInteriorFilename[64];
		formatf(cInteriorFilename, "INT%u", ms_MiniMapRenderState.m_Interior.uHash, NELEM(cInteriorFilename));

		if (CScaleformMgr::DoesMovieExistInImage(cInteriorFilename))
		{
			ms_bInteriorWasSetOnPerFrame = CScriptHud::FakeInterior.bActive;
			ms_iInteriorMovieId = CScaleformMgr::CreateMovie(cInteriorFilename, Vector2(0.0f, 0.0f), Vector2(1.0f, 1.0f), true, -1, -1, false);
		}
		else
		{
			uiDebugf1("CMiniMap: Interior %s is invalid - movie doesnt exist in RPF", cInteriorFilename);
		}

		ms_bStreamInteriorMovie = false;
	}

	CMiniMap_Common::SetInteriorMovieActive(ms_iInteriorMovieId != -1 && ms_bInteriorMovieIdFullySetup);  // flag the interior movie as on/off
}


void CMiniMap_RenderThread::ProcessAtEndOfFrame()
{
#if __STREAMED_SUPERTILE_FILES
	ms_Supertiles.ProcessAtEndOfFrame(ms_iMovieId[MINIMAP_MOVIE_FOREGROUND]);
#endif	//	__STREAMED_SUPERTILE_FILES

	HandleInteriorAtEndOfFrame();

	HandleGolfCourseAtEndOfFrame();
}

bool CMiniMap_RenderThread::HasRenderThreadProcessedThisMinimapModeState(eSETUP_STATE StateToCheck)
{
	switch (StateToCheck)
	{
	case MINIMAP_MODE_STATE_NONE :
		return false;

	case MINIMAP_MODE_STATE_SETUP_FOR_MINIMAP :
		if (MINIMAP_MODE_STATE_SETUP_FOR_MINIMAP_HAS_BEEN_PROCESSED_BY_RENDER_THREAD == ms_MiniMapRenderState.m_MinimapModeState)
		{
#if !__FINAL
			if(CMiniMap_Common::OutputDebugTransitions())
				uiDisplayf("MINIMAP_TRANSITION: CMiniMap_RenderThread::HasRenderThreadProcessedThisMinimapModeState.  Processed MINIMAP_MODE_STATE_SETUP_FOR_MINIMAP(%d) this frame (RT)", StateToCheck);
#endif

			return true;
		}
		break;
	case MINIMAP_MODE_STATE_SETUP_FOR_PAUSEMAP :
		if (MINIMAP_MODE_STATE_SETUP_FOR_PAUSEMAP_HAS_BEEN_PROCESSED_BY_RENDER_THREAD == ms_MiniMapRenderState.m_MinimapModeState)
		{
#if !__FINAL
			if(CMiniMap_Common::OutputDebugTransitions())
				uiDisplayf("MINIMAP_TRANSITION: CMiniMap_RenderThread::HasRenderThreadProcessedThisMinimapModeState.  Processed MINIMAP_MODE_STATE_SETUP_FOR_PAUSEMAP(%d) this frame (RT)", StateToCheck);
#endif

			return true;
		}
		break;
	case MINIMAP_MODE_STATE_SETUP_FOR_BIGMAP :
		if (MINIMAP_MODE_STATE_SETUP_FOR_BIGMAP_HAS_BEEN_PROCESSED_BY_RENDER_THREAD == ms_MiniMapRenderState.m_MinimapModeState)
		{
#if !__FINAL
			if(CMiniMap_Common::OutputDebugTransitions())
				uiDisplayf("MINIMAP_TRANSITION: CMiniMap_RenderThread::HasRenderThreadProcessedThisMinimapModeState.  Processed MINIMAP_MODE_STATE_SETUP_FOR_BIGMAP(%d) this frame (RT)", StateToCheck);
#endif

			return true;
		}
		break;
	case MINIMAP_MODE_STATE_SETUP_FOR_CUSTOMMAP :
		if (MINIMAP_MODE_STATE_SETUP_FOR_CUSTOMMAP_HAS_BEEN_PROCESSED_BY_RENDER_THREAD == ms_MiniMapRenderState.m_MinimapModeState)
		{
#if !__FINAL
			if(CMiniMap_Common::OutputDebugTransitions())
				uiDisplayf("MINIMAP_TRANSITION: CMiniMap_RenderThread::HasRenderThreadProcessedThisMinimapModeState.  Processed MINIMAP_MODE_STATE_SETUP_FOR_CUSTOMMAP(%d) this frame (RT)", StateToCheck);
#endif

			return true;
		}
		break;
	case MINIMAP_MODE_STATE_SETUP_FOR_GOLFMAP :
		if (MINIMAP_MODE_STATE_SETUP_FOR_GOLFMAP_HAS_BEEN_PROCESSED_BY_RENDER_THREAD == ms_MiniMapRenderState.m_MinimapModeState)
		{
#if !__FINAL
			if(CMiniMap_Common::OutputDebugTransitions())
				uiDisplayf("MINIMAP_TRANSITION: CMiniMap_RenderThread::HasRenderThreadProcessedThisMinimapModeState.  Processed MINIMAP_MODE_STATE_SETUP_FOR_GOLFMAP(%d) this frame (RT)", StateToCheck);
#endif

			return true;
		}
		break;
	default :
		Assertf(0, "CMiniMap_RenderThread::HasRenderThreadProcessedThisMinimapModeState - setup state %d not handled by this function", StateToCheck);
		break;
	}

	return false;
}

bool CMiniMap_RenderThread::IsInGameMiniMapDisplaying()
{
	return ms_MiniMapRenderState.m_bShouldRenderMiniMap &&
		   ( ms_MiniMapRenderState.m_bIsInBigMap ||
		   (!ms_MiniMapRenderState.m_bIsInCustomMap && !ms_MiniMapRenderState.m_bIsInPauseMap) );
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::UpdateTiles
// PURPOSE:	switches on/off tiles based on current player position
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::UpdateTiles(bool bClearAll)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		sfAssertf(0, "CMiniMap_RenderThread::UpdateTiles can only be called on the RenderThread!");
		return;
	}

	Vector2 vTilesAroundPlayer;
	bool bBitmapOnly = false;

	if (bClearAll)
	{
		bBitmapOnly = true;
		vTilesAroundPlayer = Vector2(0.0f, 0.0f);
	}
	else
	{
		bBitmapOnly = ms_MiniMapRenderState.m_bBitmapOnly;
		vTilesAroundPlayer = ms_MiniMapRenderState.m_vTilesAroundPlayer;
	}

	Vector2 vCurrentTile = ms_MiniMapRenderState.m_vCentreTileForPlayer;


	bool bAllTilesAreVisible = ms_Supertiles.Update(vCurrentTile, vTilesAroundPlayer, ms_asTileLayerContainer, bBitmapOnly);
	bool bBitmapVisible = false;

#if __STREAMED_SUPERTILE_FILES
	if ( (ms_MiniMapRenderState.m_bIsInPauseMap) && (!ms_MiniMapRenderState.bInsideInterior) )
	{
		if (!bAllTilesAreVisible)
		{
			bBitmapVisible = (ms_MiniMapRenderState.m_fMiniMapRange < 22.0f);  // only have bitmap if we are zoomed out enough
		}
		else
		{
			bBitmapVisible = (ms_MiniMapRenderState.m_fMiniMapRange < 8.0f);
		}
	}
	else
	{
		bBitmapVisible = true;
	}

	// turn bitmap on/off as desired
	static bool bPreviouslyVisible = !bBitmapVisible;
	if (bPreviouslyVisible != bBitmapVisible)
	{
		bPreviouslyVisible = bBitmapVisible;

		GFxValue txd_mc;
		if (uiVerifyf(ms_asBitmapLayerContainer.GFxValue::GetMember("TXD_MC", &txd_mc), "cannot find 'TXD_MC' member"))
		{
			GFxValue::DisplayInfo bitmapDisplayInfo;

			if (bBitmapVisible)
				bitmapDisplayInfo.SetVisible(true);
			else
				bitmapDisplayInfo.SetVisible(false);

			txd_mc.SetDisplayInfo(bitmapDisplayInfo);
		}
	}
#else	//	__STREAMED_SUPERTILE_FILES

	static Vector2 vPreviousTile(-1.0f,-1.0f);
	static Vector2 vPreviousTilesAroundPlayer = vTilesAroundPlayer;

	// if tile player is on hasnt changed and the amount of tiles to display hasnt changed, then no point in processing any further
	if ( (!bClearAll) && (vPreviousTile == vCurrentTile && vPreviousTilesAroundPlayer == vTilesAroundPlayer) )
	{
		return;
	}

	char cLinkageName[30];

	Vector2 vTile;

#if __DEV
	s32 iDebugTileSwap = 0;
#endif  // __DEV

	if ( (vPreviousTile.x != -1.0f && vPreviousTile.y != -1.0f) || (vPreviousTilesAroundPlayer != vTilesAroundPlayer) )
	{
		for (vTile.x = vPreviousTile.x - vPreviousTilesAroundPlayer.x; vTile.x <= vPreviousTile.x + vPreviousTilesAroundPlayer.x; vTile.x++)
		{
			if (vTile.x < 0.0f || vTile.x > MINIMAP_WORLD_TILE_SIZE_X)  // ignore tiles out of the map bounds
				continue;

			for (vTile.y = vPreviousTile.y - vPreviousTilesAroundPlayer.y; vTile.y <= vPreviousTile.y + vPreviousTilesAroundPlayer.y; vTile.y++)
			{
				if (vTile.y < 0.0f || vTile.y > MINIMAP_WORLD_TILE_SIZE_Y)  // ignore tiles out of the map bounds
					continue;

				formatf(cLinkageName, "MAP_high_r%d_col%d", (s32)vTile.y, (s32)vTile.x, NELEM(cLinkageName));

				if (vPreviousTile != vCurrentTile && vTile == vPreviousTile)
				{
					//					Displayf("REMOVING TILE (centre) %s", cLinkageName); 
				}
				else
				{
					// no need to turn off tiles that we still want to display this time round
					if ((!bBitmapOnly) &&
						vTile.x >= vCurrentTile.x - vTilesAroundPlayer.x &&
						vTile.x <= vCurrentTile.x + vTilesAroundPlayer.x &&
						vTile.y >= vCurrentTile.y - vTilesAroundPlayer.y &&
						vTile.y <= vCurrentTile.y + vTilesAroundPlayer.y)
					{
						continue;
					}

					//					Displayf("REMOVING TILE %s", cLinkageName); 
				}

				if (ms_asTileLayerContainer.HasMember(cLinkageName))
				{
					GFxValue tile;
					ms_asTileLayerContainer.GFxValue::GetMember(cLinkageName, &tile);

					if (tile.IsDisplayObject())
					{
						tile.Invoke("removeMovieClip");

#if __DEV
						//						uiDebugf1("REMOVING TILE %s %d", cLinkageName, iDebugTileSwap); 
						iDebugTileSwap++;
#endif // __DEV
					}
				}
			}
		}
	}

	if (!bBitmapOnly)
	{
		for (vTile.x = vCurrentTile.x - vTilesAroundPlayer.x; vTile.x <= vCurrentTile.x + vTilesAroundPlayer.x; vTile.x++)
		{
			if (vTile.x < 0.0f || vTile.x > MINIMAP_WORLD_TILE_SIZE_X)  // ignore tiles out of the map bounds
				continue;

			for (vTile.y = vCurrentTile.y - vTilesAroundPlayer.y; vTile.y <= vCurrentTile.y + vTilesAroundPlayer.y; vTile.y++)
			{
				if (vTile.y < 0.0f || vTile.y > MINIMAP_WORLD_TILE_SIZE_Y)  // ignore tiles out of the map bounds
					continue;

				formatf(cLinkageName, "MAP_high_r%d_col%d", (s32)vTile.y, (s32)vTile.x, NELEM(cLinkageName));

				if (vTile != vCurrentTile)
				{
					// no need to turn on tiles that we already know are active from last time
					if (vTile != vPreviousTile &&
						vTile.x >= vPreviousTile.x - vPreviousTilesAroundPlayer.x &&
						vTile.x <= vPreviousTile.x + vPreviousTilesAroundPlayer.x &&
						vTile.y >= vPreviousTile.y - vPreviousTilesAroundPlayer.y &&
						vTile.y <= vPreviousTile.y + vPreviousTilesAroundPlayer.y)
					{
						continue;
					}
				}

				if (!ms_asTileLayerContainer.HasMember(cLinkageName))
				{
					GFxValue tile;

					static s32 iTileDepth = 1;

					if (iTileDepth > (MINIMAP_WORLD_TILE_SIZE_X * MINIMAP_WORLD_TILE_SIZE_Y) + 1 )
					{
						iTileDepth = 1;  // wrap around back to depth 1
					}
					else
					{
						iTileDepth++;
					}
					// #if __DEV
					// 					uiDebugf1("TRYING TO ADD TILE %s (%0.0f %0.0f)", cLinkageName, vTile.x, vTile.y); 
					// #endif // __DEV
					if (vTile.x >= MINIMAP_VISIBLE_TILES_WEST && vTile.y >= MINIMAP_VISIBLE_TILES_NORTH && vTile.x <= MINIMAP_VISIBLE_TILES_EAST && vTile.y <= MINIMAP_VISIBLE_TILES_SOUTH)
					{
						if (ms_asTileLayerContainer.GFxValue::AttachMovie(&tile, cLinkageName, cLinkageName, iTileDepth))
						{
							Vector2 vWorldTilePosition((vTile.x * MINIMAP_TILE_WIDTH) - (MINIMAP_WORLD_SIZE_X * 0.5f), ((MINIMAP_WORLD_TILE_SIZE_Y-vTile.y) * MINIMAP_TILE_HEIGHT) - (MINIMAP_WORLD_SIZE_Y * 0.5f));
							GFxValue::DisplayInfo tileDisplayInfo;
							tileDisplayInfo.SetPosition(vWorldTilePosition.x, -vWorldTilePosition.y);
							tile.SetDisplayInfo(tileDisplayInfo);
#if __DEV
							//							uiDebugf1("ADDED TILE %s (%0.2f,%0.2f) %d %d", cLinkageName, vWorldTilePosition.x,vWorldTilePosition.y, iDebugTileSwap, iTileDepth); 
							iDebugTileSwap++;
#endif // __DEV
						}
					}
				}
			}
		}
	}

#if __DEV
	//	uiDebugf1("Had to change %d tiles this frame", iDebugTileSwap);
#endif // __DEV

	vPreviousTile = vCurrentTile;

	vPreviousTilesAroundPlayer = vTilesAroundPlayer;

#endif	//	__STREAMED_SUPERTILE_FILES
}




/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::RemoveGolfCourse
// PURPOSE: 
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::RemoveGolfCourse(bool bFromUpdateThreadInShutdownSession)
{
	if (ms_iMovieId[MINIMAP_MOVIE_FOREGROUND] == -1)
		return;

	if (ms_asGolfCourseMc.IsDisplayObject())
	{
		ms_asGolfCourseMc.Invoke("removeMovieClip");
	}

	ms_asGolfCourseMc.SetUndefined();
	ms_asGolfCourseMovie.SetUndefined();
	ms_asGolfCourseHole.SetUndefined();

	// remove the movie gfx
	if (ms_iGolfCourseMovieId != -1)
	{
		ms_bRemoveGolfCourseMovie = true;
	}

	if (!bFromUpdateThreadInShutdownSession && !ms_MiniMapRenderState.m_bIsInPauseMap && !ms_MiniMapRenderState.m_bIsInCustomMap)
	{
		//	Graeme - I'm not sure if this will work. MinimapModeState on the Update Thread is cleared every frame
		//	Can I call SetupForMiniMap() directly here?
		//	ms_MiniMapRenderState.m_MinimapModeState = MINIMAP_MODE_STATE_SETUP_FOR_MINIMAP;
		SetupForMiniMap();
	}

	uiDebugf1("MiniMap: golf course removed from stage");
}



void CMiniMap_RenderThread::UpdateSea(bool bOn)
{
	// turn the sea on/off
	if (ms_asMapContainerMc.GFxValue::HasMember("sea"))
	{
		GFxValue seaMap;
		ms_asMapContainerMc.GFxValue::GetMember("sea", &seaMap);

		GFxValue::DisplayInfo info;
		info.SetVisible(false);

		seaMap.SetDisplayInfo(info);
	}
	g_RenderMinimapSea = bOn;
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::UpdateGolfCourse
// PURPOSE:	updates the minimap with any information golf courses
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::UpdateGolfCourse()
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		sfAssertf(0, "CMiniMap_RenderThread::UpdateGolfCourse can only be called on the RenderThread!");
		return;
	}

	if (ms_iMovieId[MINIMAP_MOVIE_FOREGROUND] == -1)
		return;

	eGOLF_COURSE_HOLES iCurrentGolfCourse = ms_MiniMapRenderState.m_CurrentGolfMap;

	if (ms_iPreviousGolfCourse != iCurrentGolfCourse)
	{
		UpdateSea(iCurrentGolfCourse == GOLF_COURSE_OFF && !ms_MiniMapRenderState.m_bIsInPauseMap && !ms_MiniMapRenderState.m_bIsInCustomMap);

		if (iCurrentGolfCourse != GOLF_COURSE_OFF)
		{
			if (ms_iPreviousGolfCourse != GOLF_COURSE_OFF)
			{
				ms_iPreviousGolfCourse = iCurrentGolfCourse;

				RemoveGolfCourse(false);
			}

			ms_iPreviousGolfCourse = iCurrentGolfCourse;

			ms_bStreamGolfCourseMovie = true;
		}
		else
		{
			ms_iPreviousGolfCourse = iCurrentGolfCourse;

			RemoveGolfCourse(false);
		}
	}

	if (ms_bStreamGolfCourseMovie || ms_bRemoveGolfCourseMovie)
		return;

	if (ms_iGolfCourseMovieId != -1)
	{
		if (CScaleformMgr::IsMovieActive(ms_iGolfCourseMovieId))
		{
			if ((!ms_asGolfCourseMovie.IsDisplayObject()) || (ms_asGolfCourseMovie.IsDisplayObject() && (ms_asGolfCourseHole.IsUndefined())))
			{
				if (ms_asGolfCourseMc.IsUndefined())
				{
					// invoke the load here
					GFxValue result, asGolfCourseLayer, params[1]; 
					params[0].SetString("golf_courses"); 

					if (uiVerifyf(ms_asBaseOverlay3D[MINIMAP_LAYER_BACKGROUND].GetMember("asGolfCourseLayer", &asGolfCourseLayer), "cannot find 'asGolfCourseLayer' member"))
					{
						asGolfCourseLayer.CreateEmptyMovieClip(&ms_asGolfCourseMc, "GOLF_LINK", MOVIE_DEPTH_INTERIOR);

						ms_asGolfCourseMc.Invoke("loadMovie", &result, params, 1);

						ms_asGolfCourseHole.SetUndefined();
					}
				}

				if (ms_asGolfCourseMc.IsDisplayObject())
				{
					if (ms_asGolfCourseMovie.IsDisplayObject())
					{
						eGOLF_COURSE_HOLES iCurrentGolfCourse = ms_MiniMapRenderState.m_CurrentGolfMap;

						char cHashName[20];

						if (iCurrentGolfCourse == GOLF_COURSE_HOLE_ALL)
						{
							formatf(cHashName, "hole_all");
						}
						else
						{
							formatf(cHashName, "hole_%u", iCurrentGolfCourse);
						}

						if (ms_asGolfCourseMovie.HasMember("golf_course"))
						{
							ms_asGolfCourseMovie.GetMember("golf_course", &ms_asGolfCourseHole);

							ms_asGolfCourseHole.GotoAndStop(cHashName);

							Vector2 vPos(-1170.0f, 50.0f);

							// set the position:
							GFxValue::DisplayInfo currentDisplayInfo;
							currentDisplayInfo.SetPosition((vPos.x), -(vPos.y));
							ms_asGolfCourseMovie.SetDisplayInfo(currentDisplayInfo);

							if(!ms_MiniMapRenderState.m_bIsInPauseMap && !ms_MiniMapRenderState.m_bIsInCustomMap)
								SetupGolfCourseLayout();

							uiDebugf1("CMiniMap_RenderThread: golf course %s added to stage", cHashName);
						}
						else
						{
							uiDebugf1("CMiniMap_RenderThread: golf course data for %s is invalid - unable to set up golf course on stage", cHashName);

							RemoveGolfCourse(false);  // remove anything that was setup
							ms_iPreviousGolfCourse = GOLF_COURSE_OFF;
						}
					}
					else
					{
						uiDebugf1("CMiniMap_RenderThread: golf course movie is invalid!");
					}
				}
			}
		}
	}
}


void CMiniMap_RenderThread::HandleGolfCourseAtEndOfFrame()
{
	if (CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on UT
	{
		sfAssertf(0, "CMiniMap_RenderThread::HandleGolfCourseAtEndOfFrame can only be called on the UpdateThread!");
		return;
	}

	// 
	// removal/stream of golf course movie
	//
	if (ms_bRemoveGolfCourseMovie || ms_bStreamGolfCourseMovie)
	{
		if (ms_iGolfCourseMovieId != -1)
		{
			if (CScaleformMgr::IsMovieActive(ms_iGolfCourseMovieId))
			{
				CScaleformMgr::RequestRemoveMovie(ms_iGolfCourseMovieId);
			}
			ms_iGolfCourseMovieId = -1;
		}

		ms_bRemoveGolfCourseMovie = false;
	}

	if (ms_bStreamGolfCourseMovie)
	{
		ms_iGolfCourseMovieId = CScaleformMgr::CreateMovie(ms_MiniMapRenderState.m_CurrentMiniMapFilename, Vector2(0.0f, 0.0f), Vector2(1.0f, 1.0f), true, -1, -1, false);

		ms_bStreamGolfCourseMovie = false;
	}
}






/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::RenderNavPath
// PURPOSE: renders the nav path onto the minimap.  This only needs to be called once
//			as it adds the route on but doesnt need to be set every frame
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::RenderNavPath(s32 iRouteNum, s32 iNumPoints, Vector3 *vPoints, rage::Color32 colour, float fOffsetX, float fOffsetY, bool bClipFirstLine, float fClipX, float fClipY, float fRoadWidthMiniMap, float fRoadWidthPauseMap)
{
	// DMP
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		sfAssertf(0, "CMiniMap_RenderThread::RenderNavPath can only be called on the RenderThread!");
		return;
	}

	if (ms_iMovieId[MINIMAP_MOVIE_FOREGROUND] == -1)
		return;

	static float PAUSE_MAP_INITIAL_SCALER = 0.01f;

	// only render the route once its invisible:
	GFxValue::DisplayInfo info;

	ms_asGpsLayer[iRouteNum].GetDisplayInfo(&info);

	if (info.GetVisible())
	{
		ms_asGpsLayer[iRouteNum].Invoke("clear");
		info.SetVisible(false);
		ms_asGpsLayer[iRouteNum].SetDisplayInfo(info);
	}

	uiDebugf1("CMiniMap_RenderThread: Redrawing GPS route %d", iRouteNum);

	GFxValue args[3];

	if (!ms_MiniMapRenderState.m_bIsInPauseMap && !ms_MiniMapRenderState.m_bIsInCustomMap)  // different road widths (due to scale) when in minimap or pausemap
	{
		args[0].SetNumber(fRoadWidthMiniMap);
	}
	else
	{
		args[0].SetNumber((fRoadWidthPauseMap * PAUSE_MAP_INITIAL_SCALER) * (CMiniMap_Common::GetScaleFromZoomLevel(ZOOM_LEVEL_4)-rage::Min(30.0f,ms_MiniMapRenderState.m_fPauseMapScale)));
	}

	args[1].SetNumber(colour.GetColor());
	args[2].SetNumber(colour.GetAlphaf()*100.0f);

	//uiDebugf1("lineStyle %0.2f, %0.2f, %0.2f", args[0].GetNumber(), args[1].GetNumber(), args[2].GetNumber());
	ms_asGpsLayer[iRouteNum].Invoke("lineStyle", NULL, args, 3);

	int iLastValidNode = 0;

	if (iNumPoints > 0)
	{
		for (s32 i = 1; i < iNumPoints; i++)
		{
			if ((*(int*)&vPoints[i].w) == GNI_IGNORE_FOR_NAV)
				continue;

			Vector2 vMain = Vector2(vPoints[i].x+fOffsetX, vPoints[i].y+fOffsetY);

			if (iLastValidNode == 0)
			{
				iLastValidNode = i;
				Vector2 vLink(vPoints[0].x+fOffsetX, vPoints[0].y+fOffsetY);

				if (bClipFirstLine)
				{
					Vector2 diff = vLink - vMain;
					float	length = diff.Mag();
					Vector2 diffClip = Vector2(fClipX, fClipY) - vMain;
					float	lengthAlongLine = (diffClip.x * diff.x + diffClip.y * diff.y) / length;
					lengthAlongLine = rage::Max(0.0f, lengthAlongLine);

					if (lengthAlongLine < length)
					{	
						// reduced line segment.
						vLink = vMain + diff * (lengthAlongLine / length);
					}
				}

				if ((*(int*)&vPoints[0].w) == GNI_PLAYER_TRAIL_POS)	// Adding extra point
				{
					args[0].SetNumber(fClipX+fOffsetX);
					args[1].SetNumber(-(fClipY+fOffsetY));

					ms_asGpsLayer[iRouteNum].Invoke("moveTo", NULL, args, 2);

					args[0].SetNumber(vLink.x);
					args[1].SetNumber(-vLink.y);

					//uiDebugf1("0 moveTo %0.2f, %0.2f", vLink.x, -vLink.y);
					ms_asGpsLayer[iRouteNum].Invoke("lineTo", NULL, args, 2);
				}
				else
				{
					args[0].SetNumber(vLink.x);
					args[1].SetNumber(-vLink.y);

					//uiDebugf1("0 moveTo %0.2f, %0.2f", vLink.x, -vLink.y);
					ms_asGpsLayer[iRouteNum].Invoke("moveTo", NULL, args, 2);
				}

				args[0].SetNumber(vMain.x);
				args[1].SetNumber(-vMain.y);

				//uiDebugf1("1 lineTo %0.2f, %0.2f", vMain.x, -vMain.y);
				ms_asGpsLayer[iRouteNum].Invoke("lineTo", NULL, args, 2);
			}
			else
			{
				args[0].SetNumber(vMain.x);
				args[1].SetNumber(-vMain.y);

				ms_asGpsLayer[iRouteNum].Invoke("lineTo", NULL, args, 2);

				if ((*(int*)&vPoints[i].w) == GNI_PLAYER_TRAIL_POS)	// Adding extra point
				{
					args[0].SetNumber(vPoints[i+1].x+fOffsetX);
					args[1].SetNumber(-(vPoints[i+1].y+fOffsetY));

					ms_asGpsLayer[iRouteNum].Invoke("lineTo", NULL, args, 2);
				}

				//uiDebugf1("%d lineTo %0.2f, %0.2f", i, vMain.x, -vMain.y);
			}
		}
	}

	// make the layer visible again now the route has been drawn
	info.SetVisible(true);
	ms_asGpsLayer[iRouteNum].SetDisplayInfo(info);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::ClearNavPath
// PURPOSE: clears any nav path from the minimap
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::ClearNavPath(s32 iRouteNum)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		sfAssertf(0, "CMiniMap_RenderThread::ClearNavPath can only be called on the RenderThread!");
		return;
	}

	if (ms_iMovieId[MINIMAP_MOVIE_FOREGROUND] == -1)
		return;

	// only invoke "clear" when the route is visible:
	GFxValue::DisplayInfo info;

	ms_asGpsLayer[iRouteNum].GetDisplayInfo(&info);

	if (info.GetVisible())
	{
		// set to invisible and clear and dont set visible again (render will do this if a new route is set)
		info.SetVisible(false);
		ms_asGpsLayer[iRouteNum].SetDisplayInfo(info);

		uiDebugf1("CMiniMap_RenderThread: Clearing GPS route %d", iRouteNum);

		ms_asGpsLayer[iRouteNum].Invoke("clear");
	}
}



Mat34V ToMat34V(GMatrix3D mtx)
{
	return Mat34V(V_COL_MAJOR,
		mtx[0], mtx[1], mtx[2],
		mtx[4], mtx[5], mtx[6],
		mtx[8], mtx[9], mtx[10],
		mtx[12], mtx[13], mtx[14]
	);
}

Mat44V ToMat44V(GMatrix3D mtx)
{
	return Mat44V(V_COL_MAJOR,
		mtx[0], mtx[1], mtx[2], mtx[3],
		mtx[4], mtx[5], mtx[6], mtx[7],
		mtx[8], mtx[9], mtx[10], mtx[11],
		mtx[12], mtx[13], mtx[14], mtx[15]
	);
}

void MakeOrthoMtx(Mat44V_InOut outMtx, float left, float right, float bottom, float top, float znear, float zfar)
{
	// Stole

	float oorl = 1.0f / (right - left);
	float ootb = 1.0f / (top - bottom);
	float oolr = 1.0f / (left - right);
	float oobt = 1.0f / (bottom - top);

	float cz, dz;

	// D3DXMatrixOrthoOffCenterRH
	cz = 1 / (znear-zfar);
	dz = znear / (znear - zfar);

	//
	// taken from the D3D documentation 
	//
	//					2/(r-l)      			0            			0           0
	//					0            			2/(t-b)      			0           0
	//					0            			0            			1/(zn-zf)   0
	//					(l+r)/(l-r)  			(t+b)/(b-t)  			zn/(zn-zf)  l
	//
	// shearing sets it off in the x direction and / or in the y direction
	// scaling makes it bigger in the x or y direction
	//
	Vector2 scale = grcViewport::GetCurrent()->GetWidthHeightScale();
	Vector2 shear = grcViewport::GetCurrent()->GetPerspectiveShear();

	Mat44V projection;
	projection.SetCol0f(2 * oorl * scale.x,	0.0f,					0.0f,		0.0f);
	projection.SetCol1f(0.0f,					2 * ootb * scale.y,	0.0f,		0.0f);
	projection.SetCol2f(shear.x,				shear.y,				cz,			0.0f);
	projection.SetCol3f((left + right) * oolr,(top + bottom) * oobt,	dz,			1.0f);

	outMtx = projection;
}

void CMiniMap_RenderThread::SetupStateForSupertileRender(Vector2 vBackgroundPos, Vector2 vBackgroundSize)
{
	const float Drawable_zFar = 13.0f;
	const float Drawable_zNear = -13.0f;
	const bool Scissor = true;

	GFxMovieView* pView = CScaleformMgr::GetMovieView(ms_iMovieId[MINIMAP_MOVIE_BACKGROUND]);
	if (pView)
	{
		int clipX = 0; 
		int clipY = 0;
		int clipW = VideoResManager::GetUIWidth();
		int clipH = VideoResManager::GetUIHeight();

		int totalWidth = clipW;
		int totalHeight = clipH;
		
		float mapWidth	= (float)totalWidth;
		float mapHeight	= (float)totalHeight;

		int offsetX = 0;
		int offsetY = 0;

		bool doMiniMapAlpha = ( (!ms_MiniMapRenderState.m_bIsInPauseMap) && (!ms_MiniMapRenderState.m_bIsInCustomMap) && (ms_MiniMapAlpha < 1.0f) && (ms_MiniMapRenderState.m_CurrentGolfMap == GOLF_COURSE_OFF) );
		if (doMiniMapAlpha)
		{
			Vector2 maskPos,maskSize;
			if( ms_bUseTexturedAlphaAllMovies || ms_bUseTextureAlphaBaseMovie )
			{
				maskPos = ms_MiniMapRenderState.m_vCurrentMiniMapBlurPosition;
				maskSize = ms_MiniMapRenderState.m_vCurrentMiniMapBlurSize;
			}
            else
            {
				maskPos = ms_MiniMapRenderState.m_vCurrentMiniMapMaskPosition;
				maskSize = ms_MiniMapRenderState.m_vCurrentMiniMapMaskSize;
			}

			clipX = Clamp(rage::round((maskPos.x)*mapWidth) - 1 + offsetX, 0, totalWidth);
			clipY = Clamp(rage::round((maskPos.y)*mapHeight) - 1 + offsetY, 0, totalHeight);
			clipW = Clamp(rage::round((maskSize.x)*mapWidth) + 1, 0, totalWidth - clipX);
			clipH = Clamp(rage::round((maskSize.y)*mapHeight) + 1, 0, totalHeight - clipY);
		}
		else if (vBackgroundSize.x > 0.0f && vBackgroundSize.y > 0.0f)
		{
			// the in-game map seems to be already adjusted, fixing the pause map here
#if SUPPORT_MULTI_MONITOR
			if(ms_MiniMapRenderState.m_bIsInPauseMap )
			{
				const GridMonitor &mon = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor();
				mapWidth = (float)mon.getWidth();
				mapHeight = (float)mon.getHeight();
				offsetX = mon.uLeft;
				offsetY = mon.uTop;
			}
#endif //SUPPORT_MULTI_MONITOR

			if (Scissor)
			{
				clipX = Clamp(rage::round(mapWidth * vBackgroundPos.x) + offsetX, 0, totalWidth);
				clipY = Clamp(rage::round(mapHeight * vBackgroundPos.y) + offsetY, 0, totalHeight);
				clipW = Clamp(rage::round(mapWidth * vBackgroundSize.x), 0, totalWidth - clipX);
				clipH = Clamp(rage::round(mapHeight * vBackgroundSize.y), 0, totalHeight - clipY);
			}
		}

		grcBindTexture(NULL);
		grcWorldIdentity();

		GRCDEVICE.SetDefaultEffect(false,false); 
		GRCDEVICE.SetScissor(clipX, clipY, clipW, clipH);
		if (g_RenderMinimapSea)
		{
			// bug in repeated ClearRenderTargetView calls so use ClearRect
			GRCDEVICE.ClearRect(clipX, clipY, clipW, clipH, true, g_MinimapSeaColor, false, 0.0f, false, 0, true);  // fix for 1763187
		}

		Mat34V camera(V_IDENTITY);
		grcViewport::GetCurrent()->SetCameraMtx(camera);

		// Set the projection matrix to the scaleform world-to-screen projection matrix, concatenated with 
		// the rage ortho screen matrix.

		GMatrix3D worldToScreen = ms_LocalToScreen.GetRenderBuf();

		{
			GMatrix3D flipY;
			flipY.Scaling(1.0f, -1.0f, 1.0f);
			worldToScreen.Prepend(flipY);
		}

		Mat44V worldToScreenRage = ToMat44V(worldToScreen);

		Mat44V orthoScreen;
		MakeOrthoMtx(orthoScreen, 0.0, (float)totalWidth, (float)totalHeight, 0.0, Drawable_zNear, Drawable_zFar);

#if SUPPORT_MULTI_MONITOR && 0
		// this is already taken care of
		if (GRCDEVICE.GetMonitorConfig().isMultihead())
		{
			const GridMonitor &mon = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor();
			// computing the center difference of the preferred monitor versus the whole screen
			const float ox = (mon.uLeft + mon.uRight)/mapWidth - 1.f;
			const float oy = 1.f - (mon.uTop + mon.uBottom)/mapHeight;
			Mat44V monMx(
				mon.getWidth() / mapWidth, 0.0, 0.0, 0.0,
				0.0, mon.getHeight() / mapHeight, 0.0, 0.0,
				0.0, 0.0, 1.0, 0.0,
				ox, oy, 0.0, 1.0
				);
			Mat44V old = orthoScreen;
			Multiply(orthoScreen, monMx, old);
		}
#endif //SUPPORT_MULTI_MONITOR

		// Now one additional transform to move from screen space to render target space

		Mat44V proj;
		Multiply(proj, orthoScreen, worldToScreenRage);

		grcViewport::GetCurrent()->SetNearFarClip(Drawable_zNear, Drawable_zFar);
		grcViewport::GetCurrent()->SetProjection(proj);

		CShaderLib::SetGlobalToneMapScalers(Vector4(1.0, 1.0, 1.0, 1.0));
		CShaderLib::SetGlobalAlpha(1.0f);
		
		grcLightState::SetEnabled(false);
		grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
		grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);
		grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);

	}
}

namespace rage
{
	extern grcEffect *g_DefaultEffect;
	extern grcEffectVar g_DefaultSampler;
}

void CMiniMap_RenderThread::RenderBitmaps()
{
#if 0 // Causes asserts since this is on the render thread 
	TUNE_GROUP_FLOAT(MINIMAP, AdjustBitmapX, 0.0f, -100.0f, 100.0f, 0.01f);
	TUNE_GROUP_FLOAT(MINIMAP, AdjustBitmapY, 0.0f, -100.0f, 100.0f, 0.01f);
	TUNE_GROUP_FLOAT(MINIMAP, AdjustBitmapWidth, 1.0f, 0.0f, 2.0f, 0.0001f);
	TUNE_GROUP_FLOAT(MINIMAP, AdjustBitmapHeight, 1.0f, 0.0f, 2.0f, 0.0001f);
	TUNE_GROUP_BOOL(MINIMAP, PointSampler, false);
#else
#if __DEV
	const bool PointSampler = false;
#endif
	// I found these values experimentally, but what are they? Maybe related to texel size (and zooming up by 1/1024)?
	const float AdjustBitmapX = 0.0f;
	const float AdjustBitmapY = 0.0f;
	const float AdjustBitmapWidth = 1.0f;
	const float AdjustBitmapHeight = 1.0f;
#endif

	grcSamplerStateHandle oldSampler = g_DefaultEffect->GetSamplerState(g_DefaultSampler);
#if __DEV
	if (PointSampler)
	{
		g_DefaultEffect->SetSamplerState(g_DefaultSampler, ms_MinimapBitmapSamplerStateNearestHandle);
	}
	else
#endif
	{
		g_DefaultEffect->SetSamplerState(g_DefaultSampler, ms_MinimapBitmapSamplerStateHandle);
	}

	grcStateBlock::SetRasterizerState(ms_MinimapBitmapRasterizerStateHandle);


	strLocalIndex iLodTxdId = strLocalIndex(CMiniMap::ms_BitmapSuperLowRes.iDrawIdx.GetReadBuf());
	if (iLodTxdId.Get() >= 0)
	{
		Vector2 bitmapPos = CMiniMap::sm_Tunables.Bitmap.vBitmapStart;
		fwTxd* txd = g_TxdStore.Get(iLodTxdId);
		if (uiVerifyf(txd, "Couldn't find a txd but we're in an active state. %d", iLodTxdId.Get()))
		{
			// grab the first texture in the dict (cause we don't know the name)
			grcTexture* tex = txd->GetEntry(0);
			if (uiVerifyf(tex, "Empty texture dictionary? %d", iLodTxdId.Get()))
			{
				grcBindTexture(tex);
				CSprite2d::Draw(
					bitmapPos + Vector2(0.0f, -CMiniMap::sm_Tunables.Bitmap.vBitmapTileSize.y * CMiniMap::sm_Tunables.Bitmap.iBitmapTilesY),
					bitmapPos + Vector2(0.0f, 0.0f),
					bitmapPos + Vector2(CMiniMap::sm_Tunables.Bitmap.vBitmapTileSize.x * CMiniMap::sm_Tunables.Bitmap.iBitmapTilesX, -CMiniMap::sm_Tunables.Bitmap.vBitmapTileSize.y * CMiniMap::sm_Tunables.Bitmap.iBitmapTilesY),
					bitmapPos + Vector2(CMiniMap::sm_Tunables.Bitmap.vBitmapTileSize.x * CMiniMap::sm_Tunables.Bitmap.iBitmapTilesX, 0.0f),
					Color_white);
			}
		}
	}

	for (s32 a = 0; a < MM_BITMAP_VERSION_NUM_ENUMS; a++)
	{
		float BitmapTileSizeX, BitmapTileSizeY, BitmapStartX, BitmapStartY;

		BitmapTileSizeX = CMiniMap::sm_Tunables.Bitmap.vBitmapTileSize.x * AdjustBitmapWidth;
		BitmapTileSizeY = CMiniMap::sm_Tunables.Bitmap.vBitmapTileSize.y * AdjustBitmapHeight;
		BitmapStartX = CMiniMap::sm_Tunables.Bitmap.vBitmapStart.x + AdjustBitmapX;
		BitmapStartY = CMiniMap::sm_Tunables.Bitmap.vBitmapStart.y + AdjustBitmapY;

		Vector2 corners[4];
		corners[0].Set(0.0f, -BitmapTileSizeY);
		corners[1].Set(0.0f, 0.0f);
		corners[2].Set(BitmapTileSizeX, -BitmapTileSizeY);
		corners[3].Set(BitmapTileSizeX, 0.0f);


		for (s32 i = 0; i < CMiniMap::sm_Tunables.Bitmap.iBitmapTilesX; i++)
		{
			for (s32 j = 0; j < CMiniMap::sm_Tunables.Bitmap.iBitmapTilesY; j++)
			{
				strLocalIndex iTxdId = strLocalIndex(CMiniMap::ms_BitmapBackground(a,i,j).iDrawIdx.GetReadBuf());
				if (iTxdId.Get() >= 0)
				{
					Vector2 bitmapPos(BitmapStartX + i * BitmapTileSizeX, BitmapStartY - j * BitmapTileSizeY);
					fwTxd* txd = g_TxdStore.Get(iTxdId);
					if (uiVerifyf(txd, "Couldn't find a txd but we're in an active state. %d", iTxdId.Get()))
					{
						// grab the first texture in the dict (cause we don't know the name)
						grcTexture* tex = txd->GetEntry(0);
						if (uiVerifyf(tex, "Empty texture dictionary? %d", iTxdId.Get()))
						{
							grcBindTexture(tex);
							CSprite2d::Draw(
								bitmapPos + corners[0],
								bitmapPos + corners[1],
								bitmapPos + corners[2],
								bitmapPos + corners[3],
								Color_white
								);
						}
					}
				}
			}
		}
	}

	g_DefaultEffect->SetSamplerState(g_DefaultSampler, oldSampler);
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::Render
// PURPOSE:	renders the minimap
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::RenderMiniMap(s32 iId, Vector2 vBackgroundPos, Vector2 vBackgroundSize, float fAlpha, float fSizeScalar)
{
	if (!ms_MiniMapRenderState.m_bIsActive)
		return;

	if (ms_iMovieId[iId] == -1)
		return;

	PF_AUTO_PUSH_TIMEBAR(iId == MINIMAP_MOVIE_BACKGROUND ? "RenderMiniMap - background" : "RenderMiniMap - foreground");

#if RSG_PC
	if (GRCDEVICE.IsStereoEnabled() && GRCDEVICE.CanUseStereo())
	{
		//float fVal = 0.0f;
		//if (iId == MINIMAP_MOVIE_BACKGROUND)
		//	fVal = 1.0f;
		
		GRCDEVICE.SetStereoScissor(false);
		CShaderLib::SetStereoParams(Vector4(1.0f,0.0f,0.0f,0.0f));
	}
#endif

	if (BANK_ONLY(CMiniMap::ms_bRenderMiniMapBackground &&) iId == MINIMAP_MOVIE_BACKGROUND)
	{
		if ( (g_RenderMinimap) &&
			( (ms_MiniMapRenderState.m_CurrentGolfMap == GOLF_COURSE_OFF) ||
			  (ms_MiniMapRenderState.m_bIsInPauseMap || ms_MiniMapRenderState.m_bIsInCustomMap) ) )
		{
			grcViewport oldVP = *grcViewport::GetCurrent();
			SetupStateForSupertileRender(vBackgroundPos, vBackgroundSize);
			RenderBitmaps();
			ms_Supertiles.Render();

#if ENABLE_FOG_OF_WAR
			if( ms_MiniMapRenderState.m_bShowFow && !CTheScripts::GetIsInDirectorMode())
			{
				CSprite2d fowRender;

				grcRenderTarget *fow = CMiniMap_Common::GetFogOfWarCopyRT();
				
				grcStateBlock::SetBlendState(ms_MinimapFoWBlendStateHandle);
				fowRender.SetGeneralParams(Vector4(1.0f, 0.0f, 0.0f, 0.0f), Vector4(CMiniMap::sm_Tunables.FogOfWar.fBaseAlpha, CMiniMap::sm_Tunables.FogOfWar.fTopAlpha, 0.0, 0.0));
				fowRender.SetTexelSize(Vector2(1.0f/((float)fow->GetWidth()),1.0f/((float)fow->GetHeight())));
				fowRender.BeginCustomList(CSprite2d::CS_BLIT_TO_ALPHA_BLUR, fow);
				
				ms_Supertiles.RenderFoW();
				
				fowRender.EndCustomList();
				grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
			}
#endif // ENABLE_FOG_OF_WAR
			*grcViewport::GetCurrent() = oldVP;
		}
	}
	
#if __BANK
	if ((CMiniMap::ms_bRenderMiniMapBackground && iId == MINIMAP_MOVIE_BACKGROUND) || (CMiniMap::ms_bRenderMiniMapForeground && iId == MINIMAP_MOVIE_FOREGROUND))
#endif
	{
		if( iId == MINIMAP_MOVIE_BACKGROUND ) // Disable AA only for the map itself
		{
			CScaleformMgr::GetMovieMgr()->SetEdgeAAEnable(false);
		}

		const float fTimeStep = CLiveManager::IsSystemUiShowing() ? -1.0f : fwTimer::GetTimeStep_NonPausedNonScaledClipped();

		if (ms_MiniMapRenderState.m_bIsInPauseMap/* || ms_MiniMapRenderState.m_bIsInCustomMap*/)  // fix for 1503078
		{
			CScaleformMgr::AffectRenderSizeOnly(ms_iMovieId[iId], fSizeScalar);
		}

		CScaleformMgr::RenderMovie(ms_iMovieId[iId], fTimeStep, true, false, fAlpha);

		if( iId == MINIMAP_MOVIE_BACKGROUND )
		{
			CScaleformMgr::GetMovieMgr()->SetEdgeAAEnable(true);
		}

#if ENABLE_FOG_OF_WAR
		// NOTES: only enable this when rendering the island minimap
		if( iId == MINIMAP_MOVIE_BACKGROUND && ms_MiniMapRenderState.m_bShowFow && !CTheScripts::GetIsInDirectorMode())
		{
			grcViewport oldVP = *grcViewport::GetCurrent();
			SetupStateForSupertileRender(vBackgroundPos, vBackgroundSize);
			{
				CSprite2d fowRender;

				grcRenderTarget *fow = CMiniMap_Common::GetFogOfWarCopyRT();

				grcStateBlock::SetBlendState(ms_MinimapFoWBlendStateHandle);
				fowRender.SetGeneralParams(Vector4(1.0f, 0.0f, 0.0f, 0.0f), Vector4(CMiniMap::sm_Tunables.FogOfWar.fBaseAlpha, CMiniMap::sm_Tunables.FogOfWar.fTopAlpha, 0.0, 0.0));
				fowRender.SetTexelSize(Vector2(1.0f/((float)fow->GetWidth()),1.0f/((float)fow->GetHeight())));
				fowRender.BeginCustomList(CSprite2d::CS_BLIT_TO_ALPHA_BLUR, fow);

					CSupertile::RenderIslandFoW();	// 1 tile over whole Island map

				fowRender.EndCustomList();
				grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
			}
			*grcViewport::GetCurrent() = oldVP;
		}
#endif // ENABLE_FOG_OF_WAR

	}
#if RSG_PC
	if (GRCDEVICE.IsStereoEnabled() && GRCDEVICE.CanUseStereo())
	{
		//float fVal = 0.0f;
		//if (iId == MINIMAP_MOVIE_BACKGROUND)
		//	fVal = 1.0f;
		
		GRCDEVICE.SetStereoScissor(true);
		CShaderLib::SetStereoParams(Vector4(0.0f,0.0f,0.0f,0.0f));
	}
#endif
}




/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::CanDisplayGpsLine
// PURPOSE: returns whether the gps line can be displayed based on current minimap
//			settings (zoom etc)
/////////////////////////////////////////////////////////////////////////////////////
bool CMiniMap_RenderThread::CanDisplayGpsLine(bool bHideWhenZoomed)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		sfAssertf(0, "CMiniMap_RenderThread::CanDisplayGpsLine can only be called on the RenderThread!");
		return true;
	}

	if (ms_MiniMapRenderState.m_iScriptZoomValue != 0 || ms_MiniMapRenderState.m_bLockedToDistanceZoom)  // always display GPS line regardless of zoom level if script are setting it as its assumed they are in control of the value they are passing
		return true;

	if ( (ms_MiniMapRenderState.m_fAngle == 0.0f) && (!ms_MiniMapRenderState.m_bIsInBigMap) && (!ms_MiniMapRenderState.m_bIsInPauseMap) && (!ms_MiniMapRenderState.m_bIsInCustomMap) )  // no gps when at no angle and not in pausemap
		return false;

	if (ms_asMapContainerMc.IsDisplayObject())
	{
		if(bHideWhenZoomed)
		{
			GFxValue::DisplayInfo CurrInfo;
			ms_asMapContainerMc.GetDisplayInfo(&CurrInfo);

			if ((float)CurrInfo.GetXScale() <= MINIMAP_GPS_DISPLAY_LIMIT)
			{
				return true;
			}
		}
		else
		{
			return true;
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::SetupHealthArmourOnStage
// PURPOSE:	sets up the health and armour containers if required
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::SetupHealthArmourOnStage(bool bBigMapProperties)
{
	GFxValue asHealthContainer;
	if (uiVerifyf(ms_asRootContainer[MINIMAP_ROOT_LAYER_UNMASKED].GetMember("asHealthContainer", &asHealthContainer), "CMiniMap_RenderThread: 'asHealthContainer' cannot be found when switching to minimap"))
	{
		if (uiVerifyf(asHealthContainer.IsDisplayObject(), "CMiniMap_RenderThread: 'asHealthContainer' is not a display object when switching to minimap"))
		{
			GFxValue::DisplayInfo healthContainerDisplayInfo;
			healthContainerDisplayInfo.SetVisible(true);
			asHealthContainer.SetDisplayInfo(healthContainerDisplayInfo);

			if (CScaleformMgr::BeginMethod(ms_iMovieId[MINIMAP_MOVIE_FOREGROUND], SF_BASE_CLASS_MINIMAP, "REGISTER_HEALTH_ARMOUR"))
			{
				CScaleformMgr::AddParamGfxValue(asHealthContainer);
				CScaleformMgr::EndMethod(true);
			}

			if (CScaleformMgr::BeginMethod(ms_iMovieId[MINIMAP_MOVIE_FOREGROUND], SF_BASE_CLASS_MINIMAP, "SETUP_HEALTH_ARMOUR"))
			{
				if (bBigMapProperties)
				{
					CScaleformMgr::AddParamInt(2);
				}
				else
				{
					CScaleformMgr::AddParamInt(1);
				}
				CScaleformMgr::EndMethod(true);
			}

			CNewHud::ForceUpdateHealthInfoWithExistingValuesOnRT();  // fixes 1829470 (need to update health/armour bars straight away, instead of next frame)

#if !__FINAL
			if(CMiniMap_Common::OutputDebugTransitions())
			{
				if (bBigMapProperties)
					uiDisplayf("MINIMAP_TRANSITION: CMiniMap_RenderThread::SetupHealthArmourOnStage.  Setup actionscript for BIGMAP properties(RT)");
				else
					uiDisplayf("MINIMAP_TRANSITION: CMiniMap_RenderThread::SetupHealthArmourOnStage.  Setup actionscript for MINIMAP properties(RT)");
			}
#endif
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::SetupForGalleryMap
// PURPOSE:	sets the map to be in "gallery" mode
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::SetupForGalleryMap()
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		sfAssertf(0, "CMiniMap_RenderThread::SetupForGalleryMap can only be called on the RenderThread!");
		return;
	}

#if __DEV
	iTimeTaken = fwTimer::GetSystemTimeInMilliseconds();
#endif // __DEV

	//	Graeme - we can't call SetCurrentGolfMap() on the RenderThread so instead MiniMapUpdateState.m_CurrentGolfMap is set to GOLF_COURSE_OFF in CMiniMap::UpdateStatesOnUT() if IsInBigMap()
	// turn off the golf course if it has been set:
	// 	if (ms_MiniMapRenderState.m_CurrentGolfMap != GOLF_COURSE_OFF)  // ensure golf course is turned off if bigmap becomes active
	// 	{
	// 		SetCurrentGolfMap(GOLF_COURSE_OFF);
	// 	}

	SetupHealthArmourOnStage(true);

	// default the intial zoom value to starting pausemap scale
	GFxValue::DisplayInfo info;

	info.SetScale(ms_MiniMapRenderState.m_fPauseMapScale, ms_MiniMapRenderState.m_fPauseMapScale);
	ms_asMapContainerMc.SetDisplayInfo(info);
	ms_asBlipContainerMc.SetDisplayInfo(info);

	SetMask(true);

	for (s32 i = 0; i < MAX_MINIMAP_MOVIE_LAYERS; i++)
	{
		CScaleformMgr::ChangeMovieParams(ms_iMovieId[i], ms_MiniMapRenderState.m_vCurrentMiniMapPosition, ms_MiniMapRenderState.m_vCurrentMiniMapSize, GFxMovieView::SM_ExactFit);
	}

	// re-init the nav (so it changes for pausemap vs radar)
	CGps::ForceUpdateOfGpsOnMiniMap();

	UpdateSea(false);

	UpdateWithActionScriptOnRT(true, false);
}





/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::SetupForPauseMenu
// PURPOSE:	sets the map to be in "pausemap" mode
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::SetupForPauseMenu(bool bFullScreen)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		sfAssertf(0, "CMiniMap_RenderThread::SetupForPauseMenu can only be called on the RenderThread!");
		return;
	}

	AdjustVisualStageSpaceWithinMovie(0.0f);

#if __DEV
	iTimeTaken = fwTimer::GetSystemTimeInMilliseconds();
#endif // __DEV

	ms_vStageSize = ms_vOriginalStageSize;
	
	float fScaler = 100.0f;

	if (bFullScreen)
	{
		float fMapRatio = ms_vOriginalStageSize.x / ms_vOriginalStageSize.y;
		fScaler *= fMapRatio/CHudTools::GetAspectRatio();
		ms_vStageSize.x /= (fScaler/100.0f);

		GFxValue asCrosshairMc;
		if( ms_asRootContainer[MINIMAP_ROOT_LAYER_UNMASKED].GetMember("asCrosshair", &asCrosshairMc) )
		{
			GFxValue::DisplayInfo dispInfo;
			dispInfo.SetPosition(ms_vStageSize.x*0.5f, ms_vStageSize.y*0.5f);
			asCrosshairMc.SetDisplayInfo(dispInfo);
		}
	}


	for (s32 i = 0; i < MAX_MINIMAP_ROOT_LAYERS; i++)
	{
		GFxValue::DisplayInfo dispInfo;
		dispInfo.SetScale(fScaler, 100.0f);
		ms_asRootContainer[i].SetDisplayInfo(dispInfo);
	}

	GFxValue::DisplayInfo info;
	info.SetPosition((ms_vStageSize.x * 0.5f), (ms_vStageSize.y * 0.5f));
	ms_asBaseLayerContainer[MINIMAP_LAYER_BACKGROUND].SetDisplayInfo(info);
	ms_asBaseLayerContainer[MINIMAP_LAYER_FOREGROUND].SetDisplayInfo(info);

	CScaleformMgr::ResetMeshCache();

	// default the intial zoom value to starting pausemap scale
	GFxValue::DisplayInfo info2;
	info2.SetScale(ms_MiniMapRenderState.m_fPauseMapScale, ms_MiniMapRenderState.m_fPauseMapScale);
	ms_asMapContainerMc.SetDisplayInfo(info2);
	ms_asBlipContainerMc.SetDisplayInfo(info2);

	GFxValue asHealthContainer;
	if (uiVerifyf(ms_asRootContainer[MINIMAP_ROOT_LAYER_UNMASKED].GetMember("asHealthContainer", &asHealthContainer), "CMiniMap_RenderThread: 'asHealthContainer' cannot be found when switching to pausemap"))
	{
		if (uiVerifyf(asHealthContainer.IsDisplayObject(), "CMiniMap_RenderThread: 'asHealthContainer' is not a display object when switching to pausemenu"))
 		{
 			GFxValue::DisplayInfo healthContainerDisplayInfo;
 			healthContainerDisplayInfo.SetVisible(false);
 			asHealthContainer.SetDisplayInfo(healthContainerDisplayInfo);
 		}
	}

	GFxValue mainMap;
	ms_asMapObject.GFxValue::GetMember("main_map", &mainMap);
	GFxValue::DisplayInfo mapDisplayInfo;
	mapDisplayInfo.SetVisible(true);
	mainMap.SetDisplayInfo(mapDisplayInfo);

	g_RenderMinimap = true;

	GFxValue asSonarBlipLayer;
	if (ms_asBaseOverlay3D[MINIMAP_LAYER_FOREGROUND].GetMember("asSonarBlipLayer", &asSonarBlipLayer))
	{
		GFxValue::DisplayInfo SonarLayerDisplayInfo;
		SonarLayerDisplayInfo.SetVisible(false);
		asSonarBlipLayer.SetDisplayInfo(SonarLayerDisplayInfo);
	}
	
	SetMask(true);

	for (s32 i = 0; i < MAX_MINIMAP_MOVIE_LAYERS; i++)
	{
		CScaleformMgr::ChangeMovieParams(ms_iMovieId[i], ms_MiniMapRenderState.m_vCurrentMiniMapPosition, ms_MiniMapRenderState.m_vCurrentMiniMapSize, GFxMovieView::SM_ExactFit);  // make it fill the screen in 4:3
	}

	// re-init the nav (so it changes for pausemap vs radar)
	CGps::ForceUpdateOfGpsOnMiniMap();

	UpdateSea(false);

	UpdateWithActionScriptOnRT(true, false);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::AdjustVisualStageSpaceWithinMovie
// PURPOSE:	adjusts the contents of the movie by X amount of pixels in order for
//			it to appear in the correct place onscreen even though it has a huge
//			mask
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::AdjustVisualStageSpaceWithinMovie(float fAmount)
{
	// move the map across
	GFxValue::DisplayInfo rootContainerDisplayInfo6;
	rootContainerDisplayInfo6.SetX(-fAmount);
	ms_asRootContainer[MINIMAP_ROOT_LAYER_MASKED].SetDisplayInfo(rootContainerDisplayInfo6);

	GFxValue::DisplayInfo rootContainerDisplayInfo5;
	rootContainerDisplayInfo5.SetX(-fAmount);
	ms_asRootContainer[MINIMAP_ROOT_MAP_TRANSPARENT].SetDisplayInfo(rootContainerDisplayInfo5);

	// move the health container (frame) across
	GFxValue asHealthContainer;
	if (uiVerifyf(ms_asRootContainer[MINIMAP_ROOT_LAYER_UNMASKED].GetMember("asHealthContainer", &asHealthContainer), "CMiniMap_RenderThread: 'asHealthContainer' cannot be found when adjustingVisualSpace"))
	{
		if (uiVerifyf(asHealthContainer.IsDisplayObject(), "CMiniMap_RenderThread: 'asHealthContainer' is not a display object when adjustingVisualSpace"))
		{
			GFxValue::DisplayInfo healthContainerDisplayInfo;
			healthContainerDisplayInfo.SetX(-fAmount);
			asHealthContainer.SetDisplayInfo(healthContainerDisplayInfo);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::SetupForMiniMap
// PURPOSE:	sets the map to be in "minimap" mode
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::SetupForMiniMap()
{
	SetupMiniMapProperties(false);  // no bigmap
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::SetupForBigMap
// PURPOSE:	sets the map to be in "bigmap" mode
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::SetupForBigMap()
{
	SetupMiniMapProperties(true);  // minimap with bigmap style
}


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::SetupMiniMapProperties
// PURPOSE:	sets the map properties
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::SetupMiniMapProperties(bool bBigMapProperties)
{
	if (!CSystem::IsThisThreadId(SYS_THREAD_RENDER))  // only on RT
	{
		sfAssertf(0, "CMiniMap_RenderThread::SetupForMiniMap can only be called on the RenderThread!");
		return;
	}

	if (bBigMapProperties)
	{
#if !__FINAL
		if(CMiniMap_Common::OutputDebugTransitions())
			uiDisplayf("MINIMAP_TRANSITION: CMiniMap_RenderThread::SetupMiniMapProperties.  bBigMapProperties = TRUE (RT)");
#endif

		AdjustVisualStageSpaceWithinMovie(__BIGMAP_INTERNAL_ADJUSTMENT_CONTAINERS);  // we need to move the bigmap 34 pixels to avoid putting the movie offscreen whilst making it appear to be the same position of the minimap onscreen
	}
	else
	{
#if !__FINAL
		if(CMiniMap_Common::OutputDebugTransitions())
			uiDisplayf("MINIMAP_TRANSITION: CMiniMap_RenderThread::SetupMiniMapProperties.  bBigMapProperties = FALSE (RT)");
#endif

		AdjustVisualStageSpaceWithinMovie(0.0f);
	}

#if __DEV
	iTimeTaken = fwTimer::GetSystemTimeInMilliseconds();
#endif // __DEV

	ms_vStageSize = ms_vOriginalStageSize;

	for (s32 i = 0; i < MAX_MINIMAP_ROOT_LAYERS; i++)
	{
		GFxValue::DisplayInfo dispInfo;
		dispInfo.SetScale(100.0f, 100.0f);

		ms_asRootContainer[i].SetDisplayInfo(dispInfo);
	}

	GFxValue::DisplayInfo info;
	info.SetPosition((ms_vStageSize.x * 0.5f), (ms_vStageSize.y * 0.5f));
	ms_asBaseLayerContainer[MINIMAP_LAYER_BACKGROUND].SetDisplayInfo(info);
	ms_asBaseLayerContainer[MINIMAP_LAYER_FOREGROUND].SetDisplayInfo(info);

	CScaleformMgr::ResetMeshCache();

	SetupHealthArmourOnStage(bBigMapProperties);


	GFxValue mainMap;
	ms_asMapObject.GFxValue::GetMember("main_map", &mainMap);

	GFxValue::DisplayInfo mapDisplayInfo;
	mapDisplayInfo.SetVisible(true);
	mainMap.SetDisplayInfo(mapDisplayInfo);

	g_RenderMinimap = true;

	GFxValue asSonarBlipLayer;
	if (ms_asBaseOverlay3D[MINIMAP_LAYER_FOREGROUND].GetMember("asSonarBlipLayer", &asSonarBlipLayer))
	{
		GFxValue::DisplayInfo SonarLayerDisplayInfo;
		SonarLayerDisplayInfo.SetVisible(true);
		asSonarBlipLayer.SetDisplayInfo(SonarLayerDisplayInfo);
	}

	SetMask(true);

	for (s32 i = 0; i < MAX_MINIMAP_MOVIE_LAYERS; i++)
	{
		CScaleformMgr::ChangeMovieParams(ms_iMovieId[i], ms_MiniMapRenderState.m_vCurrentMiniMapPosition, ms_MiniMapRenderState.m_vCurrentMiniMapSize, GFxMovieView::SM_ExactFit);
	}

	uiDebugf3("CMiniMap_RenderThread::SetupMiniMapProperties -- Minimap Position = %.2f, %.2f", ms_MiniMapRenderState.m_vCurrentMiniMapPosition.x, ms_MiniMapRenderState.m_vCurrentMiniMapPosition.y);
	uiDebugf3("CMiniMap_RenderThread::SetupMiniMapProperties -- Minimap Size = %.2f, %.2f", ms_MiniMapRenderState.m_vCurrentMiniMapSize.x, ms_MiniMapRenderState.m_vCurrentMiniMapSize.y);

	// re-init the nav (so it changes for pausemap vs radar)
	CGps::ForceUpdateOfGpsOnMiniMap();

	//
	// set the crosshair as invisible for the minimap:
	//
	GFxValue asCrosshairMc;
	if (ms_asRootContainer[MINIMAP_ROOT_LAYER_UNMASKED].GetMember("asCrosshair", &asCrosshairMc))
	{
		GFxValue::DisplayInfo newDisplayInfo;
		newDisplayInfo.SetVisible(false);
		asCrosshairMc.SetDisplayInfo(newDisplayInfo);
	}

	UpdateSea(true);

	if (!bBigMapProperties)
	{
		if (ms_MiniMapRenderState.m_CurrentGolfMap != GOLF_COURSE_OFF)
		{
			SetupGolfCourseLayout();
		}
	}

	UpdateWithActionScriptOnRT(true, false);
}



//
// new for 1881980 - render a code "wanted overlay"
//
void CMiniMap_RenderThread::RenderWantedOverlay()
{
	static u32 iRangeTimer = fwTimer::GetTimeInMilliseconds();
	static bool bOn = false;

	if (ms_MiniMapRenderState.m_bFlashWantedOverlay)
	{
		if (fwTimer::GetTimeInMilliseconds()-iRangeTimer > CMiniMap::sm_Tunables.Display.WantedOverlayTime)
		{
			bOn = !bOn;
			iRangeTimer = fwTimer::GetTimeInMilliseconds();
		}

		CRGBA overlayCol;

		if (bOn)
		{
			overlayCol = CHudColour::GetRGBA(HUD_COLOUR_RED);
		}
		else
		{
			overlayCol = CHudColour::GetRGBA(HUD_COLOUR_BLUE);
		}

		grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, overlayCol);
	}
	else
	{
		iRangeTimer = fwTimer::GetTimeInMilliseconds();
		bOn = false;
	}
}

#if SUPPORT_MULTI_MONITOR
void CMiniMap_RenderThread::DrawFeatheredRect(Vector2 vBottomLeft, Vector2 vTopRight, Color32 middleColor)
{

#define NORM_TO_SCREEN(val) RemapRange(val, 0.0f, 1.0f, vBottomLeft.x, vTopRight.x)
#define SCREEN_TO_NORM(val) RemapRange(val, vBottomLeft.x, vTopRight.x, 0.0f, 1.0f)
	fwRect middleScreen = GRCDEVICE.GetMonitorConfig().getLandscapeMonitor().getArea();

	const float fLeftEdge = NORM_TO_SCREEN(middleScreen.GetLeft());
	const float fRightEdge= NORM_TO_SCREEN(middleScreen.GetRight());

	const float fFeatherPct = CMiniMap_RenderThread::sm_fMultiheadEdgeFeatherPct;
	const float fFeatherZ = 0.0f;

	// defines the bottom left and top right coordinate for each "tridrant"
	Vector4 vPos[3] = { Vector4(fLeftEdge,				vBottomLeft.y,  fLeftEdge+fFeatherPct,	vTopRight.y),
                        Vector4(fLeftEdge+fFeatherPct,	vBottomLeft.y,  fRightEdge-fFeatherPct,	vTopRight.y),
	                    Vector4(fRightEdge-fFeatherPct,	vBottomLeft.y,  fRightEdge,				vTopRight.y) };

	Vector4 uv[3] = { Vector4(SCREEN_TO_NORM(vPos[0].x), 0.0f,			SCREEN_TO_NORM(vPos[0].z), 1.0f),
	                  Vector4(SCREEN_TO_NORM(vPos[1].x), 0.0f,			SCREEN_TO_NORM(vPos[1].z), 1.0f),
	                  Vector4(SCREEN_TO_NORM(vPos[2].x), 0.0f,			SCREEN_TO_NORM(vPos[2].z), 1.0f) };

	rage::Color32 edgeColor(middleColor);
	edgeColor.SetAlpha(0);

	grcBegin(drawTriStrip,8);

	grcVertex(vPos[0].x, vPos[0].y, fFeatherZ, 0,0,0, edgeColor,	uv[0].x,uv[0].y);
	grcVertex(vPos[0].x, vPos[0].w, fFeatherZ, 0,0,0, edgeColor,	uv[0].x,uv[0].w);

	grcVertex(vPos[0].z, vPos[0].y, fFeatherZ, 0,0,0, middleColor,  uv[0].z,uv[0].y);
	grcVertex(vPos[0].z, vPos[0].w, fFeatherZ, 0,0,0, middleColor,  uv[0].z,uv[0].w);

	grcVertex(vPos[1].z, vPos[1].y, fFeatherZ, 0,0,0, middleColor,  uv[1].z,uv[1].y);
	grcVertex(vPos[1].z, vPos[1].w, fFeatherZ, 0,0,0, middleColor,  uv[1].z,uv[1].w);

	grcVertex(vPos[2].z, vPos[2].y, fFeatherZ, 0,0,0, edgeColor,	uv[2].z,uv[2].y);
	grcVertex(vPos[2].z, vPos[2].w, fFeatherZ, 0,0,0, edgeColor,	uv[2].z,uv[2].w);

	grcEnd();

}
#endif // SUPPORT_MULTI_MONITOR

void CMiniMap_RenderThread::RenderPauseMap(Vector2 vBackgroundPos, Vector2 vBackgroundSize, float fAlphaFader, float fBlipFader, float fSizeScalar)
{
#if RSG_PC
	if (ms_iMultiGPUCount > 0)
	{
		UploadFoWTextureData(true);
		ms_iMultiGPUCount--;
	}
#endif

	Vector2 origPos(vBackgroundPos);
	Vector2 origSize(vBackgroundSize);

	CScaleformMgr::ScalePosAndSize(vBackgroundPos, vBackgroundSize, fSizeScalar);

	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);

	grcRenderTarget *fbRT		= CRenderTargets::GetOffscreenBuffer2();
	grcRenderTarget *depthRT	= CRenderTargets::GetUIDepthBuffer();

	CRenderTargets::UnlockUIRenderTargets();

	MULTI_MONITOR_ONLY( bool bDrawBlended = ms_MiniMapRenderState.bDrawCrosshair && GRCDEVICE.GetMonitorConfig().isMultihead() );
	
#if ENABLE_FOG_OF_WAR
	// Prepare FoW map
	{
		grcTextureFactory::GetInstance().LockRenderTarget(0, CMiniMap_Common::GetFogOfWarCopyRT(), NULL);
		grcStateBlock::SetBlendState(grcStateBlock::BS_Default);

		CSprite2d fowCopy;
		grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
		fowCopy.SetGeneralParams(Vector4(1.0f, 1.0f, 1.0f, 1.0f), Vector4(0.0f, 0.0, 0.0, 0.0));
		fowCopy.BeginCustomList(CSprite2d::CS_BLIT, CMiniMap_Common::GetFogOfWarRT());

		grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, Color32(0xffffffff));

		fowCopy.EndCustomList();

		// Render Blips
		grcStateBlock::SetBlendState(grcStateBlock::BS_Add);
#if __PS3
		// We need to use signed blend func here, so we force it.
		cellGcmSetBlendFunc(GCM_CONTEXT, CELL_GCM_ONE, CELL_GCM_ONE, CELL_GCM_ONE, CELL_GCM_ONE);
		cellGcmSetBlendEquation(GCM_CONTEXT, CELL_GCM_FUNC_ADD_SIGNED, CELL_GCM_FUNC_ADD_SIGNED);
		cellGcmSetBlendEnable(GCM_CONTEXT, CELL_GCM_TRUE);
#endif // __PS3

		// May want to filter to only blips that are on the island, but that should hit the side alone, so we don't really care
		CMiniMap_Common::RenderBlipToFow();

		grcTextureFactory::GetInstance().UnlockRenderTarget(0);
	
#if __PS3
		// Reset to what everybody else think it actually is.
		cellGcmSetBlendFunc(GCM_CONTEXT, CELL_GCM_ONE, CELL_GCM_ZERO, CELL_GCM_ONE, CELL_GCM_ZERO);
		cellGcmSetBlendEquation(GCM_CONTEXT, CELL_GCM_FUNC_ADD, CELL_GCM_FUNC_ADD);
		cellGcmSetBlendEnable(GCM_CONTEXT, CELL_GCM_FALSE);
		
		grcStateBlock::MakeDirty();
#endif // __PS3

		grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
		grcBindTexture(NULL);
	}
#endif // ENABLE_FOG_OF_WAR	

	grcTextureFactory::GetInstance().LockRenderTarget(0, fbRT, depthRT);

	int mapDeviceWidth = GRCDEVICE.GetWidth();
	int mapDeviceHeight = GRCDEVICE.GetHeight();
	float mapWidth	= (float)VideoResManager::GetUIWidth();
	float mapHeight	= (float)VideoResManager::GetUIHeight();
	int mapX = Clamp((int)(ms_MiniMapRenderState.m_vCurrentMiniMapPosition.x*mapWidth) - 2,0,mapDeviceWidth);
	int mapY = Clamp((int)(ms_MiniMapRenderState.m_vCurrentMiniMapPosition.y*mapHeight) - 2,0,mapDeviceHeight);
	int mapW = Clamp((int)(ms_MiniMapRenderState.m_vCurrentMiniMapSize.x*mapWidth) + 4,0,Min((int)mapWidth - mapX,mapDeviceWidth));
	int mapH = Clamp((int)(ms_MiniMapRenderState.m_vCurrentMiniMapSize.y*mapHeight) + 10,0,Min((int)mapHeight - mapY,mapDeviceHeight));

	if (mapH == mapDeviceHeight && mapW == mapDeviceWidth)
	{
		GRCDEVICE.Clear(true, Color32(0,0,0,0), false, 1.0f, true, 0);
	}else
	{
		GRCDEVICE.ClearRect(mapX, mapY, mapW, mapH, true, Color32(0,0,0,0), false, 1.0f, true, 0, true);
	}

	RenderMiniMap(MINIMAP_MOVIE_BACKGROUND, origPos, origSize, 1.0f, 1.0f);

	grcTextureFactory::GetInstance().UnlockRenderTarget(0);
#if DEVICE_MSAA
	CRenderTargets::ResolveOffscreenBuffer2();
	fbRT = CRenderTargets::GetOffscreenBuffer2_Resolved();
#endif	//DEVICE_MSAA
	CRenderTargets::LockUIRenderTargets();

	// Blend minimap in
	int screenDeviceWidth = GRCDEVICE.GetWidth();
	int screenDeviceHeight = GRCDEVICE.GetHeight();
	float screenWidth	= (float)VideoResManager::GetUIWidth();
	float screenHeight	= (float)VideoResManager::GetUIHeight();
	int screenX = Clamp((int)(ms_MiniMapRenderState.m_vCurrentMiniMapPosition.x*screenWidth) - 1,0,screenDeviceWidth);
	int screenY = Clamp((int)(ms_MiniMapRenderState.m_vCurrentMiniMapPosition.y*screenHeight) - 1,0,screenDeviceHeight);
	int screenW = Clamp((int)(ms_MiniMapRenderState.m_vCurrentMiniMapSize.x*screenWidth) + 1,0,Min((int)screenWidth - screenX,screenDeviceWidth));
	int screenH = Clamp((int)(ms_MiniMapRenderState.m_vCurrentMiniMapSize.y*screenHeight) + 1,0,Min((int)screenHeight - screenY,screenDeviceHeight));
				
	GRCDEVICE.SetScissor(screenX, screenY, screenW, screenH);
	grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);
				
	CSprite2d minimapBlend;

	float fDimmerColor = 1.0f;
	if (ms_MiniMapRenderState.m_bIsInPauseMap || ms_MiniMapRenderState.m_bIsInCustomMap)  // 1354721
	{
		// alternative shade of tint if navigating content (fixes 1481452)
		eHUD_COLOURS whichTinter = HUD_COLOUR_PAUSEMAP_TINT;
		if (CPauseMenu::IsNavigatingContent())
			whichTinter = HUD_COLOUR_PAUSEMAP_TINT_HALF;
		
		fDimmerColor = CHudColour::GetRGBA(whichTinter).GetAlphaf();
	}

	minimapBlend.SetGeneralParams(Vector4(fDimmerColor, fDimmerColor, fDimmerColor, fAlphaFader), Vector4(0.0f, 0.0, 0.0, 0.0));

	const Vector2 TexSize(1.0f/float(fbRT->GetWidth()), 1.0f/float(fbRT->GetHeight()));
	minimapBlend.SetTexelSize(TexSize);
	int scissorX = rage::round(vBackgroundPos.x*screenWidth);
	int scissorY = rage::round(vBackgroundPos.y*screenHeight);
	int scissorW = rage::round(vBackgroundSize.x*screenWidth);
	int scissorH = rage::round(vBackgroundSize.y*screenHeight);

	if (scissorW < screenWidth || scissorH < screenHeight)
	{
		GRCDEVICE.SetScissor( scissorX, scissorY, scissorW, scissorH );
	}


	if(ms_MiniMapRenderState.m_bIsInPauseMap || ms_MiniMapRenderState.m_bIsInCustomMap)  // 1354721
	{
#if SUPPORT_MULTI_MONITOR
		if( bDrawBlended )
		{
			
			eHUD_COLOURS hudColour = HUD_COLOUR_PAUSEMAP_TINT;
			Color32 adjColor(CHudColour::GetRGBA(hudColour));

			// the color is stored 'inverted', so reverse it
			adjColor.SetAlpha( Clamp( int(float(255 - adjColor.GetAlpha()) * fAlphaFader), 0, 255) );

			// (ab)using lambda closures because we can now (yay C++11!)
			CSprite2d::DrawCustomShape( [&]() { DrawFeatheredRect(vBackgroundPos, vBackgroundPos+vBackgroundSize, adjColor); } );
		}
		else
#endif
		{
			CMapMenu::RenderPauseMapTint(vBackgroundPos, vBackgroundSize, fAlphaFader, false);
		}
	}

	minimapBlend.BeginCustomList(CSprite2d::CS_BLIT, fbRT);


	

#if SUPPORT_MULTI_MONITOR
	if( bDrawBlended )
	{
		DrawFeatheredRect(Vector2(-fSizeScalar, fSizeScalar), Vector2(fSizeScalar, -fSizeScalar), Color32(0xFFFFFFFF));
	}
	else
#endif
	{
		Vector4 vPos(-fSizeScalar,fSizeScalar,fSizeScalar,-fSizeScalar);
		grcDrawSingleQuadf(vPos.x,vPos.y,vPos.z,vPos.w, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, Color32(0xffffffff));
	}

	minimapBlend.EndCustomList();

	RenderMiniMap(MINIMAP_MOVIE_FOREGROUND, vBackgroundPos, vBackgroundSize, fBlipFader, fSizeScalar);

	if (scissorW < screenWidth || scissorH < screenHeight)
	{
		GRCDEVICE.DisableScissor();
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::Render
// PURPOSE: renders the minimap
/////////////////////////////////////////////////////////////////////////////////////
#if ENABLE_FOG_OF_WAR
void CMiniMap_RenderThread::FOWRenderQuadLocation(Vec4f *quads, int numQuads)
{
	CRenderTargets::UnlockUIRenderTargets();

	grcTextureFactory::GetInstance().LockRenderTarget(0, CMiniMap_Common::GetFogOfWarRT(), NULL);
	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);

	grcStateBlock::SetBlendState(grcStateBlock::BS_Add);

	grcBindTexture(CShaderLib::LookupTexture("FoW"));

	for (long index = 0; index < numQuads; index++)
	{
		grcDrawSingleQuadf(quads[index].GetX(),quads[index].GetY(),quads[index].GetZ(),quads[index].GetW(), 0.0f, 0.0f , 0.0f , 1.0f , 1.0f , fowColor);
	}

	grcTextureFactory::GetInstance().UnlockRenderTarget(0);
	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcBindTexture(NULL);

	CRenderTargets::LockUIRenderTargets();

}

#if RSG_PC
void CMiniMap_RenderThread::FOWRenderCatchupQuads()
{
	CRenderTargets::UnlockUIRenderTargets();

	grcTextureFactory::GetInstance().LockRenderTarget(0, CMiniMap_Common::GetFogOfWarRT(), NULL);
	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);

	grcStateBlock::SetBlendState(grcStateBlock::BS_Add);

	grcBindTexture(CShaderLib::LookupTexture("FoW"));

	unsigned int currentRTFlag = (1 << CMiniMap_Common::GetCurrentExportRTIndex());
	ms_gotQuads = false;

	for (int i = 0; i < MAX_FOW_TARGETS*(MAX_FOG_SCRIPT_REVEALS + 1); i++)
	{
		if ( (ms_quads[i].flags & currentRTFlag) == 0)
		{
			grcDrawSingleQuadf(ms_quads[i].pos.GetX(),ms_quads[i].pos.GetY(),ms_quads[i].pos.GetZ(),ms_quads[i].pos.GetW(), 0.0f, 0.0f , 0.0f , 1.0f , 1.0f , fowColor);
			ms_quads[i].flags |= currentRTFlag;
		}		
		ms_gotQuads |= (ms_quads[i].flags != 0x7);
	}

	grcTextureFactory::GetInstance().UnlockRenderTarget(0);
	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcBindTexture(NULL);

	CRenderTargets::LockUIRenderTargets();

}
#endif
void CMiniMap_RenderThread::FOWClear()
{
	FoWPrevX = 0.0f;
	FoWPrevY = 0.0f;

	CRenderTargets::UnlockUIRenderTargets();

	// Update Fog of War
	grcTextureFactory::GetInstance().LockRenderTarget(0, CMiniMap_Common::GetFogOfWarRT(), NULL);

	GRCDEVICE.Clear(true, Color32(0,0,0,0),false,0.0f,false,0);

	grcTextureFactory::GetInstance().UnlockRenderTarget(0);

#if RSG_DURANGO
	// Clear the reveal ratio texture to prevent spurious Sightseer Achievement awards
	grcTextureFactory::GetInstance().LockRenderTarget(0, CMiniMap_Common::GetFogOfWarRevealRatioRT(), NULL);

	GRCDEVICE.Clear(true, Color32(0,0,0,0),false,0.0f,false,0);

	grcTextureFactory::GetInstance().UnlockRenderTarget(0);
#endif
	CRenderTargets::LockUIRenderTargets();
}

void CMiniMap_RenderThread::FOWReveal()
{
	FoWPrevX = 0.0f;
	FoWPrevY = 0.0f;

	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);

	CRenderTargets::UnlockUIRenderTargets();

	// Update Fog of War
#if RSG_PC
	for(size_t i = 0; i < MAX_FOW_TARGETS; ++i)
	{
#endif
	grcTextureFactory::GetInstance().LockRenderTarget(0, CMiniMap_Common::GetFogOfWarRT(), NULL);

	GRCDEVICE.Clear(true, Color32(0xffffffff),false,0.0f,false,0);

	grcTextureFactory::GetInstance().UnlockRenderTarget(0);
#if RSG_PC
	CMiniMap_Common::UpdateFogOfWarRBIndex();
	}
#endif

	CRenderTargets::LockUIRenderTargets();

	grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);
}

void CMiniMap_RenderThread::FOWDownSample()
{
	CRenderTargets::UnlockUIRenderTargets();

	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);

	CSprite2d fowReveal;
	Vector4 TexSize;

	fowReveal.SetGeneralParams(Vector4(1.0f, 1.0f, 1.0f, 1.0f), Vector4(0.0f, 0.0, 0.0, 0.0));

	grcTextureFactory::GetInstance().LockRenderTarget(0,PostFX::FogOfWarRT0, NULL);

	TexSize = Vector4(1.0f/float(CMiniMap_Common::GetFogOfWarRT()->GetWidth()),1.0f/float(CMiniMap_Common::GetFogOfWarRT()->GetHeight()), 1.0f/float(PostFX::FogOfWarRT0->GetWidth()), 1.0f/float(PostFX::FogOfWarRT0->GetHeight()));
	fowReveal.SetTexelSize(TexSize);

	fowReveal.BeginCustomList(CSprite2d::CS_FOWCOUNT, CMiniMap_Common::GetFogOfWarRT());
	grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f,0.1f,0.0f,0.0f,1.0f,1.0f,Color32(0xffffffff));
	fowReveal.EndCustomList();

	// Add Height map
	fowReveal.BeginCustomList(CSprite2d::CS_FOWFILL, NULL);
	for(int x=0;x<fowTileSize;x++)
	{
		for( int y=0;y<fowTileSize;y++)
		{
			float fowX1 = ((float)x)/((float)fowTileSize);
			float fowY1 = ((float)y)/((float)fowTileSize);
			float fowX2 = fowX1 + 1.0f/((float)fowTileSize);
			float fowY2 = fowY1 + 1.0f/((float)fowTileSize);

			Vector2 world1 = FowToWorldCoord(fowX1,fowY1);
			Vector2 world2 = FowToWorldCoord(fowX2,fowY2);
			float xMin = Min(world1.x,world2.x);
			float yMin = Min(world1.y,world2.y);
			float xMax = Max(world1.x,world2.x);
			float yMax = Max(world1.y,world2.y);				

			float height = CGameWorldHeightMap::GetMaxHeightFromWorldHeightMap(xMin, yMin, xMax, yMax);
			if( height < CMiniMap::sm_Tunables.FogOfWar.fFowWaterHeight )
			{
				grcDrawSingleQuadf(fowX1,fowY1,fowX2,fowY2, 0.0f, 0.0f , 0.0f , 1.0f , 1.0f , Color32(0xffffffff));
			}
		}
	}
	fowReveal.EndCustomList();

	grcTextureFactory::GetInstance().UnlockRenderTarget(0);


	grcTextureFactory::GetInstance().LockRenderTarget(0,PostFX::FogOfWarRT1, NULL);

	TexSize = Vector4(1.0f/float(PostFX::FogOfWarRT0->GetWidth()),1.0f/float(PostFX::FogOfWarRT0->GetHeight()),1.0f/float(PostFX::FogOfWarRT1->GetWidth()),1.0f/float(PostFX::FogOfWarRT1->GetHeight()));
	fowReveal.SetTexelSize(TexSize);

	fowReveal.BeginCustomList(CSprite2d::CS_FOWAVERAGE, PostFX::FogOfWarRT0);
	grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f,0.1f,0.0f,0.0f,1.0f,1.0f,Color32(0xffffffff));
	fowReveal.EndCustomList();

	grcTextureFactory::GetInstance().UnlockRenderTarget(0);


	grcTextureFactory::GetInstance().LockRenderTarget(0,PostFX::FogOfWarRT2, NULL);

	TexSize = Vector4(1.0f/float(PostFX::FogOfWarRT1->GetWidth()),1.0f/float(PostFX::FogOfWarRT1->GetHeight()),1.0f/float(PostFX::FogOfWarRT2->GetWidth()),1.0f/float(PostFX::FogOfWarRT2->GetHeight()));
	fowReveal.SetTexelSize(TexSize);

	fowReveal.BeginCustomList(CSprite2d::CS_FOWAVERAGE, PostFX::FogOfWarRT1);
	grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f,0.1f,0.0f,0.0f,1.0f,1.0f,Color32(0xffffffff));
	fowReveal.EndCustomList();

	grcTextureFactory::GetInstance().UnlockRenderTarget(0);


	grcTextureFactory::GetInstance().LockRenderTarget(0,CMiniMap_Common::GetFogOfWarRevealRatioRT(), NULL);

	TexSize = Vector4(1.0f/float(PostFX::FogOfWarRT2->GetWidth()),1.0f/float(PostFX::FogOfWarRT2->GetHeight()),1.0f/float(CMiniMap_Common::GetFogOfWarRevealRatioRT()->GetWidth()),1.0f/float(CMiniMap_Common::GetFogOfWarRevealRatioRT()->GetHeight()));
	fowReveal.SetTexelSize(TexSize);

	fowReveal.BeginCustomList(CSprite2d::CS_FOWAVERAGE, PostFX::FogOfWarRT2);
	grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f,0.1f,0.0f,0.0f,1.0f,1.0f,Color32(0xffffffff));
	fowReveal.EndCustomList();

	grcTextureFactory::GetInstance().UnlockRenderTarget(0);

	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);

	CRenderTargets::LockUIRenderTargets();
}

#endif //ENABLE_FOG_OF_WAR

void CMiniMap_RenderThread::Render()
{
#if DISABLE_HUD_FOR_CONTENT_CONTROLLED_BUILD
	return; 

#else

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);

#if RSG_PC
	if (GRCDEVICE.IsStereoEnabled() && GRCDEVICE.CanUseStereo())
	{	
		CShaderLib::SetStereoParams(Vector4(0.0f,0.0f,0.0f,0.0f));
	}
#endif

#if ENABLE_FOG_OF_WAR
#if __D3D11
	if( ms_MiniMapRenderState.m_bUploadFoWTextureData )
	{
		UploadFoWTextureData();
	}
	static bool fogOfWarRatioRTValid = false;
	if (fogOfWarRatioRTValid)
		ReadBackFoWFromStaging();
#endif
	bool fogOfWarUpdated = false;

#if RSG_PC
	if( ms_gotQuads )
	{
		FOWRenderCatchupQuads();
	}

	if (g_curFOWClearGPUCount > 0)
	{
		FOWClear();
		g_curFOWClearGPUCount--;
	}

	if (g_curFOWRevealGPUCount > 0)
	{
		FOWReveal();
		g_curFOWRevealGPUCount--;
	}
#endif

	if( ms_MiniMapRenderState.m_bUpdateFoW )
	{
		// Update Fog of War
		Vector2 coords = WorldToFowCoord(ms_MiniMapRenderState.m_vPlayerPos.x, ms_MiniMapRenderState.m_vPlayerPos.y);

		float fowWidth = (float)CMiniMap_Common::GetFogOfWarRT()->GetWidth();
		float fowHeight = (float)CMiniMap_Common::GetFogOfWarRT()->GetHeight();

		bool needPedRender = ms_MiniMapRenderState.m_bUpdatePlayerFoW && (abs(FoWPrevX - coords.x) > moveStep/fowWidth || abs(FoWPrevY - coords.y) > moveStep/fowHeight);
		bool needScriptRender = ms_MiniMapRenderState.m_numScriptRevealRequested > 0;
		
		if( needPedRender || needScriptRender )
		{
			const float aspectRatio = CMiniMap::AreFowCustomWorldExtentsEnabled()?
										(CMiniMap::GetFowCustomWorldH() / CMiniMap::GetFowCustomWorldW())				:
										(CMiniMap::sm_Tunables.FogOfWar.fWorldH / CMiniMap::sm_Tunables.FogOfWar.fWorldW);

			float xDelta = (fowColorSizeX/fowWidth)*aspectRatio;
			float yDelta = (fowColorSizeY/fowHeight)*aspectRatio;

			Vec4f quads[MAX_FOG_SCRIPT_REVEALS + 1];
			int numOfQuads = 0;

			if( needPedRender )
			{
				FoWPrevX = coords.x;
				FoWPrevY = coords.y;
			
				quads[numOfQuads++] = Vec4f(coords.x-xDelta,coords.y-yDelta,coords.x+xDelta,coords.y+yDelta);
			}
			
			if( needScriptRender )
			{
				for(int i=0;i<ms_MiniMapRenderState.m_numScriptRevealRequested;i++)
				{
					Vector2 coords = WorldToFowCoord(ms_MiniMapRenderState.m_ScriptReveal[i].x, ms_MiniMapRenderState.m_ScriptReveal[i].y);
					quads[numOfQuads++] = Vec4f(coords.x-xDelta,coords.y-yDelta,coords.x+xDelta,coords.y+yDelta);
				}
			}

			if (numOfQuads )
			{
				FOWRenderQuadLocation(quads, numOfQuads);
#if RSG_PC
				int quadIdx = 0;
				unsigned int currentRTFlag = (1 << CMiniMap_Common::GetCurrentExportRTIndex());
				for (int i = 0; i < MAX_FOW_TARGETS*(MAX_FOG_SCRIPT_REVEALS + 1); i++)
				{
					if( ms_quads[i].flags == 0x7 )
					{
						ms_quads[i].pos = quads[quadIdx];
						ms_quads[i].flags = currentRTFlag;
						ms_gotQuads = true;

						quadIdx++;
						if( quadIdx == numOfQuads )
							break;
					}
				}
#endif
			}

			fogOfWarUpdated = true;
		}
	}

	if( ms_MiniMapRenderState.m_bClearFoW )
	{
		FOWClear();
#if RSG_PC
		g_curFOWClearGPUCount = GRCDEVICE.GetGPUCount(true)-1;
#endif
		fogOfWarUpdated = true;
	}

	if( ms_MiniMapRenderState.m_bRevealFoW)
	{
		FOWReveal();

#if RSG_PC
		g_curFOWRevealGPUCount = GRCDEVICE.GetGPUCount(true)-1;
#endif

		fogOfWarUpdated = true;
	}

	// Downsample FoW.
	FowLastFrameRead++;

	if( FowLastFrameRead > fowFrameCount )
	{
		FowLastFrameRead = 0;
		
		FOWDownSample();

#if __D3D11
		//	copy to current frame to staging buffer and increment frame number
		CopyFoWForCPURead();

		fogOfWarRatioRTValid = true;
#endif
	}

#endif // ENABLE_FOG_OF_WAR


#if	!__FINAL
	XPARAM(nominimap);
	if (PARAM_nominimap.Get())
	{
		if (!ms_MiniMapRenderState.m_bIsInPauseMap)
			return;
	}
#endif // !__FINAL

	if ( (ms_MiniMapRenderState.m_bShouldProcessMiniMap) && (ms_MiniMapRenderState.m_bShouldRenderMiniMap) )  // if in pause map and not in map screen then dont render
	{
#define __TEST_SCALEFORM_3D_ISSUE (0)
#if __TEST_SCALEFORM_3D_ISSUE
		ms_MiniMapRenderState.m_vCurrentMiniMapPosition = Vector2(0.3f,0.8f);
		ms_MiniMapRenderState.m_vCurrentMiniMapSize = Vector2(0.5f, 0.5f);

		CScaleformMgr::ChangeMovieParams(ms_iMovieId[MINIMAP_MOVIE_BACKGROUND], ms_MiniMapRenderState.m_vCurrentMiniMapPosition, ms_MiniMapRenderState.m_vCurrentMiniMapSize, GFxMovieView::SM_ExactFit);
		CScaleformMgr::ChangeMovieParams(ms_iMovieId[MINIMAP_MOVIE_FOREGROUND], ms_MiniMapRenderState.m_vCurrentMiniMapPosition, ms_MiniMapRenderState.m_vCurrentMiniMapSize, GFxMovieView::SM_ExactFit);
#endif // __TEST_SCALEFORM_3D_ISSUE

		// I'd love to skip the whole rigmarole of doing the scissoring and clear and copy, but it would appear
		// the frontend menu relies on this behaviour to not spew map all over the screen...
		bool doMiniMapAlpha = ( (!ms_MiniMapRenderState.m_bIsInPauseMap) && (!ms_MiniMapRenderState.m_bIsInCustomMap) && (ms_MiniMapAlpha < 1.0f) && (ms_MiniMapRenderState.m_CurrentGolfMap == GOLF_COURSE_OFF) );
		float miniMapAlpha = doMiniMapAlpha ? ms_MiniMapAlpha : 1.0f;
		if( ms_bUseTexturedAlphaAllMovies || ms_bUseTextureAlphaBaseMovie )
			miniMapAlpha = 1.0f;

		PF_PUSH_TIMEBAR("Scaleform Movie Render (MiniMap)");

		bool useOffscreenBuffer = false;
		if(doMiniMapAlpha)
			useOffscreenBuffer = true;

		Vector2 zero(0.0f, 0.0f);
		Vector2 maskPos,maskSize;
		int mapMaskX = 0;
		int mapMaskY = 0;
		int mapMaskW = 0;
		int mapMaskH = 0;
		int mapX = 0;
		int mapY = 0;
		int mapW = 0;
		int mapH = 0;

		if(useOffscreenBuffer)
		{
			int mapDeviceWidth = GRCDEVICE.GetWidth();
			int mapDeviceHeight = GRCDEVICE.GetHeight();
			float mapWidth	= (float)VideoResManager::GetUIWidth();
			float mapHeight	= (float)VideoResManager::GetUIHeight();

#if SUPPORT_MULTI_MONITOR
			MultiMonitorHudHelper helper(&ms_MiniMapRenderState);
#endif // SUPPORT_MULTI_MONITOR

			if( ms_bUseTexturedAlphaAllMovies || ms_bUseTextureAlphaBaseMovie )
			{
				maskPos = ms_MiniMapRenderState.m_vCurrentMiniMapBlurPosition;
				maskSize = ms_MiniMapRenderState.m_vCurrentMiniMapBlurSize;
			}
			else
			{
				maskPos = ms_MiniMapRenderState.m_vCurrentMiniMapMaskPosition;
				maskSize = ms_MiniMapRenderState.m_vCurrentMiniMapMaskSize;
			}

			int mapMaskDeviceWidth = GRCDEVICE.GetWidth();
			int mapMaskDeviceHeight = GRCDEVICE.GetHeight();
			mapMaskX = Clamp((int)((maskPos.x)*mapWidth) - 1,0,mapMaskDeviceWidth);
			mapMaskY = Clamp((int)((maskPos.y)*mapHeight) - 1,0,mapMaskDeviceHeight);
			mapMaskW = Clamp((int)((maskSize.x)*mapWidth) + 1,0,Min((int)mapWidth - mapMaskX,mapMaskDeviceWidth));
			mapMaskH = Clamp((int)((maskSize.y)*mapHeight) + 1,0,Min((int)mapHeight - mapMaskY,mapMaskDeviceHeight));

			if (!ms_MiniMapRenderState.m_bBackgroundMapShouldBeHidden)
			{
				// Render the minimap to a secondary buffer and composite it to the final render
				// That way we can apply a constant alpha blend to the whole thing, but let artists alpha blends elements together.
				// Clever Shit.

				// Render to offscreen target
				grcRenderTarget *fbRT		= CRenderTargets::GetOffscreenBuffer2();
				grcRenderTarget *depthRT	= CRenderTargets::GetUIDepthBuffer();

				CRenderTargets::UnlockUIRenderTargets();
				grcTextureFactory::GetInstance().LockRenderTarget(0, fbRT, depthRT);

				if (GRCDEVICE.GetMSAA() || RSG_PC)
				{
					GRCDEVICE.Clear(true,Color32(0),false,1.f,false,0);
				}

				Vector2 vMapPos, vMapSize;
				if( ms_bUseTexturedAlphaAllMovies || ms_bUseTextureAlphaBaseMovie )
				{
					vMapPos = ms_MiniMapRenderState.m_vCurrentMiniMapBlurPosition;
					vMapSize = ms_MiniMapRenderState.m_vCurrentMiniMapBlurSize;
				}
				else
				{
					vMapPos = ms_MiniMapRenderState.m_vCurrentMiniMapPosition;
					vMapSize = ms_MiniMapRenderState.m_vCurrentMiniMapSize;
				}

				mapX = Clamp((int)(vMapPos.x*mapWidth) - 1,0,mapDeviceWidth);
				mapY = Clamp((int)(vMapPos.y*mapHeight) - 1,0,mapDeviceHeight);
				mapW = Clamp((int)(vMapSize.x*mapWidth) + 1,0,Min((int)mapWidth - mapX,mapDeviceWidth));
				mapH = Clamp((int)(vMapSize.y*mapHeight) + 1,0,Min((int)mapHeight - mapY,mapDeviceHeight));

				GRCDEVICE.ClearRect(mapX, mapY, mapW, mapH, true, Color32(0,0,0,0),true,1.0f,true,0 ORBIS_ONLY(,true) DURANGO_ONLY(,true) WIN32PC_ONLY(,true));

				RenderMiniMap(MINIMAP_MOVIE_BACKGROUND, zero, zero, 1.0f, 1.0f);
		        if( ms_bUseTexturedAlphaAllMovies )
				{
					RenderMiniMap(MINIMAP_MOVIE_FOREGROUND, zero, zero, 1.0f, 1.0f);
				}

				u32 now = fwTimer::GetTimeInMilliseconds();
				float flashAlpha = Clamp(1.0f - (now-ms_MiniMapRenderState.m_uFlashStartTime)/float(CMiniMap::sm_Tunables.Display.ScriptOverlayTime), 0.0f, 1.0f);
				if( flashAlpha > 0.0f )
				{
					CSprite2d minimapFlashOverlay;
					CRGBA overlayCol = CHudColour::GetRGBF(ms_MiniMapRenderState.m_eFlashColour, flashAlpha);
					minimapFlashOverlay.SetGeneralParams(Vector4(1.0f, 1.0f, 1.0f, 1.0f), Vector4(0.0f, 0.0, 0.0, 0.0));
					minimapFlashOverlay.BeginCustomList(CSprite2d::CS_BLIT, grcTexture::NoneWhite);
					grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, overlayCol);
					minimapFlashOverlay.EndCustomList();
				}
				
				if(doMiniMapAlpha)
				{
					if( ms_bUseTexturedAlphaAllMovies || ms_bUseTextureAlphaBaseMovie )
					{
					    CSprite2d minimapAlphaClear;

						grcTexture *texture;
						if(false && NetworkInterface::IsInCopsAndCrooks())
						{	// use different radar mask textures for C&C (BS#6508644 - C&C - Radar Mask - Code set up our new Radar Mask Texture)
							texture = ms_MiniMapRenderState.m_bIsInBigMap ? ms_MaskTextureCnCLg : ms_MaskTextureCnCSm;
						}
						else
						{
							texture = ms_MiniMapRenderState.m_bIsInBigMap ? ms_MaskTextureLg : ms_MaskTextureSm;
						}
						
						grcStateBlock::SetBlendState(ms_ClearAlphaMiniMapRenderBlendStateHandle);

					    // clear the full alpha background, because Scaleform leaks outside the rect...
						minimapAlphaClear.SetGeneralParams(Vector4(0.0f, 0.0f, 0.0f, 0.0f), Vector4(0.0f, 0.0, 0.0, 0.0));
					    minimapAlphaClear.BeginCustomList(CSprite2d::CS_BLIT, grcTexture::NoneWhite);
					    grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f,0.1f,0.0f,0.0f,1.0f,1.0f,Color32(0xffffffff));
					    minimapAlphaClear.EndCustomList();

						// Render the textured blob....
						float alphaOut = Lerp(flashAlpha, CMiniMap::sm_Tunables.Display.MaskNormalAlpha, CMiniMap::sm_Tunables.Display.MaskFlashAlpha );
					    minimapAlphaClear.SetGeneralParams(Vector4(1.0f, 0.0f, 0.0f, 0.0f), Vector4(0.0f, alphaOut, 0.0f, 0.0f));
					    minimapAlphaClear.SetTexelSize(Vector2(1.0f/((float)texture->GetWidth()),1.0f/((float)texture->GetHeight())));
					    minimapAlphaClear.BeginCustomList(CSprite2d::CS_BLIT_TO_ALPHA_BLUR, texture);

						float ooScreenWidth	= 1.0f/(float)VideoResManager::GetUIWidth();
						float ooScreenHeight = 1.0f/(float)VideoResManager::GetUIHeight();

					    grcDrawSingleQuadf(	maskPos.x-ooScreenWidth,
										    maskPos.y-ooScreenHeight,
										    maskPos.x + maskSize.x+ooScreenWidth,
										    maskPos.y + maskSize.y+ooScreenHeight,
										    0.0f,
										    0.0f,0.0f,1.0f,1.0f,
										    Color32(0xffffffff));
    
					    minimapAlphaClear.EndCustomList();
					}
					else
					{   
					    GRCDEVICE.SetScissor(mapMaskX, mapMaskY, mapMaskW, mapMaskH);
    
					    grcStateBlock::SetBlendState(ms_ClearAlphaMiniMapRenderBlendStateHandle);
    
					    CSprite2d minimapAlphaClear;
					    minimapAlphaClear.SetGeneralParams(Vector4(1.0f, 1.0f, 1.0f, 1.0f), Vector4(0.0f, 0.0, 0.0, 0.0));
					    minimapAlphaClear.BeginCustomList(CSprite2d::CS_BLIT, grcTexture::NoneWhite);
    
					    grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f,0.1f,0.0f,0.0f,1.0f,1.0f,Color32(0xffffffff));
    
					    minimapAlphaClear.EndCustomList();
					}
				}

				grcTextureFactory::GetInstance().UnlockRenderTarget(0);
				{
#if DEVICE_EQAA
					eqaa::ResolveArea area(mapX, mapY, mapW, mapH);
#endif // DEVICE_EQAA
					CRenderTargets::ResolveOffscreenBuffer2();
					fbRT = CRenderTargets::GetOffscreenBuffer2_Resolved();
				}
				CRenderTargets::LockUIRenderTargets();

				// Blend minimap in
				GRCDEVICE.SetScissor(mapMaskX, mapMaskY, mapMaskW, mapMaskH);

				grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);

				CSprite2d minimapBlend;
				const Vector2 TexSize(1.0f/float(fbRT->GetWidth()),1.0f/float(fbRT->GetHeight()));
				minimapBlend.SetTexelSize(TexSize);

				minimapBlend.SetGeneralParams(Vector4(miniMapAlpha, miniMapAlpha, miniMapAlpha, miniMapAlpha), Vector4(0.0f, 0.0, 0.0, 0.0));

				minimapBlend.BeginCustomList(CSprite2d::CS_BLIT, fbRT);
		
				grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, Color32(0xffffffff));

				// if we attempt to flash the radar while wanted, we just get a brighter red radar. And that's no good
				if( flashAlpha <= 0.0f )
					RenderWantedOverlay();

				minimapBlend.EndCustomList();

				GRCDEVICE.DisableScissor();
			}
		    
			if( ms_bUseTexturedAlphaAllMovies == false )  // we always want to show foreground (so we can render "blips only" mode) - fix for 1883759
		    {
				if (!(RSG_PC && GRCDEVICE.GetMSAA()))
				{
					RenderMiniMap(MINIMAP_MOVIE_FOREGROUND, zero, zero, 1.0f, 1.0f);
				}
				else
				{
					grcRenderTarget *uiRT		= CRenderTargets::GetUIBackBuffer();
					grcRenderTarget *fbRT		= CRenderTargets::GetOffscreenBuffer2();
					grcRenderTarget *depthRT	= CRenderTargets::GetUIDepthBuffer();

					CRenderTargets::UnlockUIRenderTargets();
					
					// Copy minimap back to OffscreenBuffer2 and render foreground layer with AA
					grcTextureFactory::GetInstance().LockRenderTarget(0, fbRT, depthRT);
					{
						grcStateBlock::SetBlendState(ms_CopyAAMiniMapRenderBlendStateHandle);
						GRCDEVICE.SetScissor(mapMaskX, mapMaskY, mapMaskW, mapMaskH);

						CSprite2d minimapBlend;
						const Vector2 TexSize(1.0f/float(fbRT->GetWidth()),1.0f/float(fbRT->GetHeight()));
						minimapBlend.SetTexelSize(TexSize);
						minimapBlend.SetGeneralParams(Vector4(miniMapAlpha, miniMapAlpha, miniMapAlpha, miniMapAlpha), Vector4(0.0f, 0.0, 0.0, 0.0));

						minimapBlend.BeginCustomList(CSprite2d::CS_BLIT, uiRT);
						{
							grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, Color32(0xffffffff));
						}
						minimapBlend.EndCustomList();


						RenderMiniMap(MINIMAP_MOVIE_FOREGROUND, zero, zero, 1.0f, 1.0f);
						GRCDEVICE.DisableScissor();
					}
					grcTextureFactory::GetInstance().UnlockRenderTarget(0);

#if DEVICE_EQAA
					eqaa::ResolveArea area(mapX, mapY, mapW, mapH);
#endif
					CRenderTargets::ResolveOffscreenBuffer2();
					fbRT = CRenderTargets::GetOffscreenBuffer2_Resolved();
				
					CRenderTargets::LockUIRenderTargets();

					// Copy composited minimap to UI Back Buffer
					grcStateBlock::SetBlendState(ms_CopyAAMiniMapRenderBlendStateHandle);
					GRCDEVICE.SetScissor(mapMaskX, mapMaskY, mapMaskW, mapMaskH);

					CSprite2d minimapBlend;
					const Vector2 TexSize(1.0f/float(fbRT->GetWidth()),1.0f/float(fbRT->GetHeight()));
					minimapBlend.SetTexelSize(TexSize);
					minimapBlend.SetGeneralParams(Vector4(miniMapAlpha, miniMapAlpha, miniMapAlpha, miniMapAlpha), Vector4(0.0f, 0.0, 0.0, 0.0));

					minimapBlend.BeginCustomList(CSprite2d::CS_BLIT, fbRT);
					{
						grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, Color32(0xffffffff));
					}
					minimapBlend.EndCustomList();

					GRCDEVICE.DisableScissor();
				}
		    }
		}
		else
		{
			if (!ms_MiniMapRenderState.m_bBackgroundMapShouldBeHidden)
			{
				RenderMiniMap(MINIMAP_MOVIE_BACKGROUND, zero, zero, 1.0f, 1.0f);
			}

			RenderMiniMap(MINIMAP_MOVIE_FOREGROUND, zero, zero, 1.0f, 1.0f);
		}

		
		PF_POP_TIMEBAR();

#if __BANK
		if (bDebug3DBlips && vGameScreenPos.x != 0 && vGameScreenPos.y != 0)
		{
			CSprite2d::DrawRect( fwRect(vGameScreenPos.x, vGameScreenPos.y, vGameScreenPos.x+0.003f, vGameScreenPos.y+0.005f), CRGBA(0,255,0,255) );

			vGameScreenPos = Vector2(0,0);  // reset it for next frame
		}
#endif // __BANK
	}
#endif

#if __DEV
	if (iTimeTaken != 0)
	{
		uiDebugf1("Time taken to switch between MiniMap and PauseMap: %d", fwTimer::GetSystemTimeInMilliseconds() - iTimeTaken);
		iTimeTaken = 0;
	}
#endif // __DEV

#if __BANK && ENABLE_FOG_OF_WAR
	if( DebugDrawFOW )
	{
		CSprite2d::SetRenderState(CMiniMap_Common::GetFogOfWarRT());
		CSprite2d::SetGeneralParams(Vector4(1.0f,1.0f,1.0f,1.0f), Vector4(0.0f, 0.0, 0.0, 0.0));
		CSprite2d::DrawRect(Vector2(DebugFOWX1,DebugFOWY1),
							Vector2(DebugFOWX2,DebugFOWY1),
							Vector2(DebugFOWX1,DebugFOWY2),
							Vector2(DebugFOWX2,DebugFOWY2),
							0.0f,
							Vector2(0.0f,0.0f),
							Vector2(1.0f,0.0f),
							Vector2(0.0f,1.0f),
							Vector2(1.0f,1.0f),
							Color32(0xffffffff));
		CSprite2d::ClearRenderState();
	}
#endif // __BANK && ENABLE_FOG_OF_WAR

#if RSG_PC
	if (GRCDEVICE.IsStereoEnabled() && GRCDEVICE.CanUseStereo())
	{	
		CShaderLib::SetStereoParams(Vector4(0.0f,0.0f,0.0f,0.0f));
	}
#endif
}



#if __D3D11 && ENABLE_FOG_OF_WAR
#if RSG_PC
bool CMiniMap_RenderThread::IsRenderMultiGPU()
{
	return (ms_gotQuads || (g_curFOWClearGPUCount > 0) || (g_curFOWRevealGPUCount > 0));
}
#endif

void CMiniMap_RenderThread::CopyFoWForCPURead()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	grcTextureD3D11* fogOfWarTex11 = (grcTextureD3D11*)CMiniMap_Common::GetFogOfWarTex();
	fogOfWarTex11->CopyFromGPUToStagingBuffer();

	grcTextureD3D11* fogOfWarRatioTex11 = (grcTextureD3D11*)CMiniMap_Common::GetFogOfWarRevealRatioTex();
	fogOfWarRatioTex11->CopyFromGPUToStagingBuffer();

#if RSG_PC
	CMiniMap_Common::UpdateFogOfWarRBIndex();
#endif

	ms_bUpdateFogOfWarData = true;
}

void CMiniMap_RenderThread::ReadBackFoWFromStaging()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	
	if(ms_bUpdateFogOfWarData)
	{
		PF_PUSH_TIMEBAR_BUDGETED("CMiniMap_Common::ms_MiniMapFogOfWarTex Read Back", 0.4f);

		bool fogOfWarDataUpdated = false;
#if RSG_PC
		grcTextureD3D11* fogOfWarTex11 = (grcTextureD3D11*)CMiniMap_Common::GetPreviousFogOfWarTex();
#else
		grcTextureD3D11* fogOfWarTex11 = (grcTextureD3D11*)CMiniMap_Common::GetFogOfWarTex();
#endif
		u8* fogOfWarData = CMiniMap_Common::GetFogOfWarData();
		if(fogOfWarTex11->MapStagingBufferToBackingStore(true))
		{
			grcTextureLock lock;
			if (fogOfWarTex11->LockRect( 0, 0, lock, grcsRead ))
			{
				u8* pBits = (u8*)lock.Base;
				for (int col=0; col<lock.Height; col++) 
				{
					sysMemCpy( fogOfWarData, pBits, sizeof(u8)*FOG_OF_WAR_RT_SIZE);
					fogOfWarData += FOG_OF_WAR_RT_SIZE;
					pBits += lock.Pitch / sizeof(u8);
				}
				fogOfWarTex11->UnlockRect(lock);
				fogOfWarDataUpdated = true;
			}
		}
		PF_POP_TIMEBAR();

		PF_PUSH_TIMEBAR_BUDGETED("CMiniMap_Common::ms_MiniMapFogofWarRevealRatioTex Read Back", 0.4f);
		bool fogOfWarRatioDataUpdated = false;
#if RSG_PC
		grcTextureD3D11* fogOfWarRatioTex11 = (grcTextureD3D11*)CMiniMap_Common::GetPreviousFogOfWarRevealRatioTex();
#else
		grcTextureD3D11* fogOfWarRatioTex11 = (grcTextureD3D11*)CMiniMap_Common::GetFogOfWarRevealRatioTex();
#endif
		if(fogOfWarRatioTex11->MapStagingBufferToBackingStore(true))
		{
			grcTextureLock lock;
			if (fogOfWarRatioTex11->LockRect( 0, 0, lock, grcsRead ))
			{
				*CMiniMap_Common::GetFogOfWarRevealRatioLockPtr() = *(float*)lock.Base;
				fogOfWarRatioDataUpdated = true;
				fogOfWarRatioTex11->UnlockRect(lock);
			}
		}
		PF_POP_TIMEBAR();

		ms_bUpdateFogOfWarData = !(fogOfWarDataUpdated && fogOfWarRatioDataUpdated);
	}
}

void CMiniMap_RenderThread::UploadFoWTextureData(WIN32PC_ONLY(bool bMultiGPUSync))
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

#if RSG_PC
	//	copy everything across the different textures
	for(size_t i = 0; i < MAX_FOW_TARGETS; ++i)
	{
		((grcTextureD3D11*)CMiniMap_Common::GetFogOfWarTex(i))->UpdateGPUCopy(0, 0, CMiniMap_Common::GetFogOfWarLockPtr());
#if RSG_PC
		if (!bMultiGPUSync)
#endif
		((grcTextureD3D11*)CMiniMap_Common::GetFogOfWarTex(i))->CopyFromGPUToStagingBuffer();
	}
#else
	// UpdateGPU Copy is a lie...
	grcTextureLock texLock;
	if(CMiniMap_Common::GetFogOfWarTex()->LockRect(0, 0, texLock))
	{
		u8 *fogOfWarData = CMiniMap_Common::GetFogOfWarLockPtr();
		u8* pBits = (u8*)texLock.Base;
		for (int col=0; col<texLock.Height; col++) 
		{
			sysMemCpy(pBits , fogOfWarData, sizeof(u8)*FOG_OF_WAR_RT_SIZE);
			fogOfWarData += FOG_OF_WAR_RT_SIZE;
			pBits += texLock.Pitch / sizeof(u8);
		}

		CMiniMap_Common::GetFogOfWarTex()->UnlockRect(texLock);
	}
#endif
}

#endif	//	__D3D11



#if __RENDER_PED_VISUAL_FIELD


/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::RenderPedVisualField
// PURPOSE: renders the ped visual field to the minimap (needs moving over to Scaleform)
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::RenderPedVisualField()
{
	if (!GetInStealthMode())  // if not in stealth mode, then dont create any stealth blips
		return;

	// Render AI perception cones and dead peds
	CEntityIterator entityIterator(IteratePeds);
	CPed* pPed = static_cast<CPed*>(entityIterator.GetNext());
	while(pPed)
	{
		if( pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DrawRadarVisualField ) )
		{
			if( pPed->IsInjured() )
			{
				DrawDeadPed(pPed);
			}
			else
			{
#if __DEV
				if (CTaskInvestigate::ms_bToggleRenderInvestigationPosition)
				{
					CTask* pTask = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_INVESTIGATE);
					if (pTask && pTask->GetTaskType() == CTaskTypes::TASK_INVESTIGATE)
					{
						DrawLine(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()),static_cast<CTaskInvestigate*>(pTask)->GetInvestigationPos(),Color_orange);
					}

					pTask = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SEARCH);
					if (pTask && pTask->GetTaskType() == CTaskTypes::TASK_SEARCH)
					{
						DrawLine(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()),static_cast<CTaskInvestigate*>(pTask)->GetInvestigationPos(),Color_orange);
					}
				}
#endif // __DEV
				DrawPedVisualField(pPed);
			}
		}

		pPed = static_cast<CPed*>(entityIterator.GetNext());
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::DrawPedVisualField
// PURPOSE: (needs moving over to Scaleform)
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::DrawPedVisualField( CPed* pPed )
{
	if (pPed)
	{
		// Get global positions for the peds visual field
		Vector3 avPerceptionCone[CPedPerception::VF_Max];
		pPed->GetPedIntelligence()->GetPedPerception().GetVisualFieldLines(avPerceptionCone);//, pPed->GetPedIntelligence()->GetPedPerception().GetIdentificationRange());
		Vector3 vHead = avPerceptionCone[CPedPerception::VF_HeadPos];

		// Work out the line and fill colours
		Color32 lineColour = pPed->GetPedIntelligence()->GetAlertnessState() == CPedIntelligence::AS_NotAlert ? Color_DarkGreen : Color_DarkOrange;
		if( pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMBAT))
		{
			lineColour = Color_red;
		}
		Color32 fillColour = lineColour;
		fillColour.SetAlpha(60);

		// Convert the targets into screen coords
		Vector3 vLeftEnd = avPerceptionCone[CPedPerception::VF_LeftFocus];
		Vector3 vRightEnd = avPerceptionCone[CPedPerception::VF_RightFocus];

		// Draw the field onto the hud
		if( DrawPoly( vHead, vLeftEnd, vRightEnd, fillColour ) )
		{
			DrawLine(vHead, vLeftEnd, lineColour);
			DrawLine(vHead, vRightEnd, lineColour);
		}
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::DrawDeadPed
// PURPOSE: (needs moving over to Scaleform)
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::DrawDeadPed( CPed* pPed )
{
#define DEAD_PED_SIZE (1.0f)

	Vector3 vCentre = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
	Vector3 vLineStart1 = Vector3(vCentre.x - DEAD_PED_SIZE, vCentre.y - DEAD_PED_SIZE, vCentre.z);
	Vector3 vLineEnd1	= Vector3(vCentre.x + DEAD_PED_SIZE, vCentre.y + DEAD_PED_SIZE, vCentre.z);
	Vector3 vLineStart2 = Vector3(vCentre.x + DEAD_PED_SIZE, vCentre.y - DEAD_PED_SIZE, vCentre.z);
	Vector3 vLineEnd2	= Vector3(vCentre.x - DEAD_PED_SIZE, vCentre.y + DEAD_PED_SIZE, vCentre.z);

	DrawLine(vLineStart1, vLineEnd1, Color_red);
	DrawLine(vLineStart2, vLineEnd2, Color_red);
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::ConvertToScreenCoords()
// PURPOSE:
/////////////////////////////////////////////////////////////////////////////////////
bool CMiniMap_RenderThread::ConvertToScreenCoords(Vector2& screenCoordsOut, const Vector3& vWorldCoords)
{
	if(ms_iMovieId[MINIMAP_MOVIE_FOREGROUND] >= 0)
	{
		GPointF blipPoint, screenPoint;
		blipPoint.x = vWorldCoords.x;
		blipPoint.y = -vWorldCoords.y;

		GFxMovieView* pView = CScaleformMgr::GetMovieView(ms_iMovieId[MINIMAP_MOVIE_FOREGROUND]);

		if(pView && pView->TranslateLocalToScreen(BLIP_LAYER_PATH, blipPoint, &screenPoint))
		{
			screenCoordsOut.Set(screenPoint.x/SCREEN_WIDTH, screenPoint.y/SCREEN_HEIGHT);
			return true;
		}
	}
	return false;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::DrawLine
// PURPOSE: (needs moving over to Scaleform)
/////////////////////////////////////////////////////////////////////////////////////
void CMiniMap_RenderThread::DrawLine( const Vector3 &vFrom, const Vector3 &vTo, const Color32 DEBUG_DRAW_ONLY(colour) )
{
	Vector2 p1, p2;

	ConvertToScreenCoords(p1, vFrom);
	ConvertToScreenCoords(p2, vTo);

	if( ClampInsideLimits(p1, p2) )
	{
#if DEBUG_DRAW
		grcDebugDraw::Line(p1, p2, colour, colour);
#endif
	}
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::DrawPoly
// PURPOSE: (needs moving over to Scaleform)
/////////////////////////////////////////////////////////////////////////////////////
#if DEBUG_DRAW
bool CMiniMap_RenderThread::DrawPoly( const Vector3 &p1, const Vector3 &p2, const Vector3 &p3, const Color32 colour)
#else
bool CMiniMap_RenderThread::DrawPoly( const Vector3 &p1, const Vector3 &p2, const Vector3 &p3, const Color32 UNUSED_PARAM(colour))
#endif
{
	s32 iNumValidLines = 0;

	Vector2 vEdge1A, vEdge1B, vEdge2B;
	ConvertToScreenCoords(vEdge1A, p1);
	ConvertToScreenCoords(vEdge1B, p2);
	Vector2 vEdge2A = vEdge1B;
	ConvertToScreenCoords(vEdge2B, p3);
	Vector2 vEdge3A = vEdge2B;
	Vector2 vEdge3B = vEdge1A;

	if( ClampInsideLimits(vEdge1A, vEdge1B) )
		++iNumValidLines;

	if( ClampInsideLimits(vEdge2A, vEdge2B) )
		++iNumValidLines;

	if( ClampInsideLimits(vEdge3A, vEdge3B) )
		++iNumValidLines;

	if( iNumValidLines == 3 )
	{
#if DEBUG_DRAW
		grcDebugDraw::Poly(vEdge3B, vEdge1A, vEdge3A, colour, colour, colour);
		grcDebugDraw::Poly(vEdge1A, vEdge1B, vEdge3A, colour, colour, colour);
		grcDebugDraw::Poly(vEdge1B, vEdge2A, vEdge3A, colour, colour, colour);
		grcDebugDraw::Poly(vEdge2A, vEdge2B, vEdge3A, colour, colour, colour);
#endif
		return true;
	}
	return false;
}



/////////////////////////////////////////////////////////////////////////////////////
// NAME:	CMiniMap_RenderThread::ClampInsideLimits
// PURPOSE: (needs moving over to Scaleform)
/////////////////////////////////////////////////////////////////////////////////////
bool CMiniMap_RenderThread::ClampInsideLimits(Vector2 &vFrom, Vector2 &vTo)
{
	// Transform to "minimap coordinates", ranged -0.5...0.5.
	const float mapPosX = ms_MiniMapRenderState.m_vCurrentMiniMapPosition.x;
	const float mapPosY = ms_MiniMapRenderState.m_vCurrentMiniMapPosition.y;
	const float mapSizeX = ms_MiniMapRenderState.m_vCurrentMiniMapSize.x;
	const float mapSizeY = ms_MiniMapRenderState.m_vCurrentMiniMapSize.y;
	const float x1 = (vFrom.x - mapPosX)/mapSizeX - 0.5f;
	const float y1 = (vFrom.y - mapPosY)/mapSizeY - 0.5f;
	const float x2 = (vTo.x - mapPosX)/mapSizeX - 0.5f;
	const float y2 = (vTo.y - mapPosY)/mapSizeY - 0.5f;

	// Compute the direction of the line segment.
	const float dirX = x2 - x1;
	const float dirY = y2 - y1;

	// Compute the square of the clip radius and the distance to the starting point.
	static float s_ClipRadius = 0.43f;	// MAGIC! This is MAX_2D_RADAR_DISTANCE in CMiniMap_RenderThread::UpdateIndividualBlip().
	const float radiusSq = square(s_ClipRadius);
	const float distSq = square(x1) + square(y1);

	// Convert to a quadratic equation a*t^2 + b*t + c = 0.
	const float a = square(dirX) + square(dirY);
	const float b = 2.0f*(dirX*x1 + dirY*y1);
	const float c = distSq - radiusSq;

	// Make sure we don't divide by zero if it's a degenerate segment.
	if(a != 0.0f)
	{
		// Transform to t^2 + p*t + q = 0 form.
		const float p = b/a;
		const float q = c/a;

		// Check if there is a solution.
		const float r2 = p*p - 4*q;
		if(r2 > 0.0f)
		{
			// Yes, the infinite line intersects the circle.

			// Compute the roots.
			const float r = sqrtf(r2);
			const float tt1 = 0.5f*(-p - r);
			const float tt2 = 0.5f*(-p + r);

			// Check if the relevant line segment part of the infinite line intersects.
			if(tt2 >= 0.0f && tt1 <= 1.0f)
			{
				// Clamp the T values to not go outside the segment.
				const float t1 = Clamp(tt1, 0.0f, 1.0f);
				const float t2 = Clamp(tt2, 0.0f, 1.0f);

				// Compute the clipped positions.
				const float clippedx1 = x1 + dirX*t1;
				const float clippedy1 = y1 + dirY*t1;
				const float clippedx2 = x1 + dirX*t2;
				const float clippedy2 = y1 + dirY*t2;

				// Transform back to screen coordinates.
				vFrom.x = (clippedx1 + 0.5f)*mapSizeX + mapPosX;
				vFrom.y = (clippedy1 + 0.5f)*mapSizeY + mapPosY;
				vTo.x = (clippedx2 + 0.5f)*mapSizeX + mapPosX;
				vTo.y = (clippedy2 + 0.5f)*mapSizeY + mapPosY;

				return true;
			}
		}
	}
	else if(c < 0.0f)
	{
		// Both points are in the same spot, but they are inside the circle.
		return true;
	}

	// In this case, no part of the line segment intersects the disc.
	return false;
}


#endif	//	__RENDER_PED_VISUAL_FIELD

