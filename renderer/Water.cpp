//
// Title	:	Water.cpp
// Author	:	Obbe
//
//	16/03/2007	-	Andrzej:	- PSN: Water::UpdateDynamicWater() ported fully to SPU;
//	20/03/2007	-	Andrzej:	- Critical section support added around accesses to m_WaterTaskHandle (MikeD's idea);
//

// rage headers
#include "grcore/allocscope.h"
#include "grcore/quads.h"
#include "grcore/effect_mrt_config.h"
#include "grcore/image.h"
#include "grcore/indexbuffer.h"
#include "grcore/texturedefault.h"
#include "profile/timebars.h"
#include "spatialdata/aabb.h"
#include "spatialdata/kdop2d.h"
#if __XENON
#include "grcore/texturexenon.h"
#endif

#if RSG_ORBIS
#include "grcore/texture_gnm.h"
#include "grcore/wrapper_gnm.h"
#include "grcore/gfxcontext_gnm.h"
#endif

#include "fwgeovis/geovis.h"
#include "fwutil/xmacro.h"
#include "fwmaths/vectorutil.h"
#include "fwscene/scan/Scan.h"
#include "fwsys/timer.h"

// game headers
#include "water.h"

#include "audio/northaudioengine.h"
#include "camera/CamInterface.h"
#include "camera/helpers/frame.h"
#include "control/replay/replay.h"
#include "Core/Game.h"
#include "debug/Rendering/DebugDeferred.h"
#include "debug/TiledScreenCapture.h"
#include "Game/Weather.h"
#include "streaming/streaming.h"
#include "vehicles/Boat.h"
#include "Vehicles/AmphibiousAutomobile.h"
#include "pathserver/ExportCollision.h"
#include "renderer/Deferred/DeferredLighting.h"
#include "renderer/Entities/EntityDrawHandler.h"
#include "renderer/lights/lights.h"
#include "renderer/lights/TiledLighting.h"
#include "renderer/Mirrors.h"
#include "renderer/occlusion.h"
#include "renderer/PostProcessFX.h"
#include "renderer/PostScan.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/RenderPhases/RenderPhaseDebugOverlay.h"
#include "renderer/RenderPhases/RenderPhaseHeightMap.h"
#include "renderer/RenderPhases/RenderPhaseMirrorReflection.h"
#include "renderer/RenderPhases/RenderPhaseReflection.h"
#include "renderer/RenderPhases/RenderPhaseStd.h"
#include "renderer/rendertargets.h"
#include "renderer/RenderListBuilder.h"
#include "renderer/River.h"
#include "renderer/Util/ShaderUtil.h"
#include "renderer/SpuPm/SpuPmMgr.h"
#include "renderer/SSAO.h"
#include "scene/Physical.h"
#include "scene/world/GameWorldHeightMap.h"
#include "scene/world/GameWorldWaterHeight.h"
#include "system/SettingsManager.h"
#include "scene/FocusEntity.h"
#include "../shader_source/Lighting/Shadows/cascadeshadows_common.fxh"
#include "vehicleAi/task/TaskVehicleAnimation.h"

#include "debug/vectormap.h"
#include "TimeCycle/TimeCycle.h"
#include "vfx/Systems/VfxWater.h"
#include "vfx/misc/Puddles.h"
#include "vfx/VfxSettings.h"
#include "vfx/sky/Sky.h"
#include "vfx/VfxHelper.h"
#include "vfx/vfxutil.h"
#include "vfx/particles/PtFxManager.h"

#if GTA_REPLAY
#include "control/replay/Misc/WaterPacket.h"
#endif

#if __PPU
#include "WaterSPU.h"
RELOAD_TASK_DECLARE(WaterSPU);
#endif

#if __XENON
#include "system/xtl.h"
#include "system/d3d9.h"
#define DBG 0 // yuck
#include <xgraphics.h>
#endif //__XENON

#include "profile/element.h"
#include "profile/group.h"
#include "profile/page.h"
#include "WaterUpdateDynamicCommon.h"

#if __BANK
#include "tools/ObjectProfiler.h"
#include "fwscene/scan/ScreenQuad.h"
#include "fwscene/scan/ScanNodesDebug.h"
#endif // __BANK

enum { MID_TECHNICAL2	= 0x4662BCBB };	// "technical2"
enum { MID_TORNADO4		= 0x86CF7CDD };	// "tornado4"
enum { MID_APC			= 0x2189D250 }; // "apc"
enum { MID_TULA			= 0x3E2E4F8A }; // "tula"
enum { MID_SEABREEZE	= 0xE8983F9F }; // "seabreeze"

RENDER_OPTIMISATIONS()
PHYSICS_OPTIMISATIONS()

PF_PAGE(WaterProfile_Page, "Water Update");
PF_GROUP(WaterProfile_Group);
PF_LINK(WaterProfile_Page, WaterProfile_Group);

PF_COUNTER_FLOAT(WaterUpdateTime_Counter, WaterProfile_Group);
PF_COUNTER_FLOAT(Water_ScanCutBlocks, WaterProfile_Group);

#if RSG_PC
#define GPUINDEX		GRCDEVICE.GPUIndex()
#define GPUINDEXMT		GRCDEVICE.GPUIndexMT()
#else
#define GPUINDEX		0
#define GPUINDEXMT		0
#endif

#define WATER_TILED_LIGHTING	(1)

#define USE_WATER_THREAD (RSG_PC)

#define WATER_PIXELCOUNT_DEFAULT_THRESHOLD (64) //reduced to 64 for more bugs BS#1369782

//PARAM(loadwaterdat, "Use old water data");
PARAM(noWaterUpdate, "Update the water on a per-frame basis");

namespace rage { extern u32 g_AllowVertexBufferVramLocks; };

extern bool g_cheapmodeHogEnabled;

#if USE_WATER_THREAD
static BankBool	gbOutputTimingsCPU		= FALSE;
#endif // USE_WATER_THREAD

#define BATCH_WATER_RENDERING		(1 && __D3D11)

#if BATCH_WATER_RENDERING
BankBool sBatchWaterRendering = true;
BankBool m_EnableBatchWaterRendering = sBatchWaterRendering;
static const u32 waterBatchSize = 1024;
static u32 currentBatchOffset[NUMBER_OF_RENDER_THREADS] = {NUMBER_OF_RENDER_THREADS_INITIALIZER_LIST(0)};
static bool waterBatchStarted[NUMBER_OF_RENDER_THREADS] = {NUMBER_OF_RENDER_THREADS_INITIALIZER_LIST(false)};
static Vec4V m_CurrentFogParams(V_NEGONE);
#endif //BATCH_WATER_RENDERING

class CWaterQuad
{
public:
	enum{
		eHasBeenAddedToRenderList = 1<<0,
		eLimitedDepth = 1<<1,
		eShouldPlaySeaSounds = 1<<2,
		eInvisible = 1<<3
	};

	float	GetZ()const							{ return z; }
	void	SetZ(float setVal)					{ z = setVal; }

	bool	GetAddedToRenderList()const			{ return bHasBeenAddedToRenderList; }
	void	SetAddedToRenderList(bool setVal)	{ bHasBeenAddedToRenderList = setVal; }
	bool	GetHasLimitedDepth()const			{ return bLimitedDepth; }
	void	SetHasLimitedDepth(bool setVal)		{ bLimitedDepth = setVal; }
	bool	GetPlaySounds()const				{ return bShouldPlaySeaSounds; }
	void	SetPlaySounds(bool setVal)			{ bShouldPlaySeaSounds = setVal; }
	bool	GetInvisible()const					{ return bInvisible; }
	void	SetInvisible(bool setVal)			{ bInvisible = setVal; }

	u8		GetType()const						{ return m_Type; }
	void	SetType(u8 inType)					{ m_Type = inType; }
	bool	GetNoStencil()const					{ return bNoStencil; }
	void	SetNoStencil(bool setVal)			{ bNoStencil = setVal; }

	float	GetSlope()const						{ return slope; }
	float	GetOffset()const					{ return offset; }
	void	GetSlopeAndOffset(float& outSlope, float& outOffset)const
	{
		outSlope	= slope;
		outOffset	= offset;
	}

	s16	minX, minY, maxX, maxY;
	u8	a1, a2, a3, a4;

	float slope;
	float offset;

protected:
	float z;

	u8	bHasBeenAddedToRenderList:1;
	u8	bLimitedDepth:1;
	u8	bShouldPlaySeaSounds:1;
	u8  bInvisible:1;
	u8	bNoStencil:1;
	u8	m_Type;
};

//PARGEN STUFF - extra member vars in parsable structs leaves a bad taste in my mouth...hence these custom temp classes...
class CWaterQuadParsable : public CWaterQuad
{
public:
	CWaterQuadParsable(){}
	CWaterQuadParsable(CWaterQuad quad)
	{
		minX = quad.minX; minY = quad.minY;
		maxX = quad.maxX; maxY = quad.maxY;
		a1	= quad.a1;
		a2	= quad.a2;
		a3	= quad.a3;
		a4	= quad.a4;

		z = quad.GetZ();
		m_Type				= quad.GetType();
		m_IsInvisible		= quad.GetInvisible();
		m_HasLimitedDepth	= quad.GetHasLimitedDepth();
		m_NoStencil			= quad.GetNoStencil();
	}
	bool m_IsInvisible;
	bool m_HasLimitedDepth;
	bool m_NoStencil;
	PAR_SIMPLE_PARSABLE;
};

class CCalmingQuadParsable : public CCalmingQuad
{
public:
	CCalmingQuadParsable(){}
	CCalmingQuadParsable(CCalmingQuad  quad)
	{
		minX = quad.minX; minY = quad.minY;
		maxX = quad.maxX; maxY = quad.maxY;
		m_fDampening = quad.m_fDampening;
	}
	PAR_SIMPLE_PARSABLE;
};

class CWaveQuadParsable : public CWaveQuad
{
public:
	CWaveQuadParsable(){}
	CWaveQuadParsable(CWaveQuad  quad)
	{
		minX = quad.minX; minY = quad.minY;
		maxX = quad.maxX; maxY = quad.maxY;
		m_Amplitude		= quad.GetAmplitude();
		m_XDirection	= quad.GetXDirection();
		m_YDirection	= quad.GetYDirection();
	}
	float m_Amplitude;
	float m_XDirection;
	float m_YDirection;
	PAR_SIMPLE_PARSABLE;
};

class CDeepWaveQuadParsable
{
public:
	s16	minX, minY, maxX, maxY;
	float m_Amplitude;
	PAR_SIMPLE_PARSABLE;
};

class CFogQuadParsable
{
public:
	CFogQuadParsable(){}
	s16	minX, minY, maxX, maxY;
	Color32 c0;
	Color32 c1;
	Color32 c2;
	Color32 c3;
	PAR_SIMPLE_PARSABLE;
};

class CWaterData : public datBase
{
public:
	atArray<CWaterQuadParsable,		128, u16> m_WaterQuads;
	atArray<CCalmingQuadParsable,	128, u16> m_CalmingQuads;
	atArray<CWaveQuadParsable,		128, u16> m_WaveQuads;
	PAR_PARSABLE;
};

#define MAXEXTRACALMINGQUADS	8

atRangeArray<CCalmingQuad,MAXEXTRACALMINGQUADS> sm_ExtraCalmingQuads;
int sm_ExtraQuadsCount = 0;

float sm_ScriptDeepOceanScaler = 1.0f;
float sm_ScriptCalmedWaveHeightScaler = 1.0f;

#if __BANK
static bool		sm_DebugDrawExtraCalmingQuads = false;
static float	sm_bkDeepOceanMult = 1.5f;
static bool		sm_bkDeepOceanExp2 = true;
static bool		sm_bkDeepOceanExp4 = true;
#endif  // __BANK

CMainWaterTune m_MainWaterTunings;
CSecondaryWaterTune m_SecondaryWaterTunings;
float m_RippleWind = 0.0f;

#if GTA_REPLAY
Vector3 m_ReplayWaterPos;
float m_ReplayWaterRand;
float *m_pReplayWaterBufferForRecording = NULL;

Vector3 m_ReplayWaterPos_Previous;
float *m_pReplayWaterBufferForRecording_Previous = NULL;
#endif // GTA_REPLAY

static void LoadWaterTunings(const char *filename=NULL);

#include "Water_parser.h"

bool CMainWaterTune::m_OverrideTimeCycle = false;
float CMainWaterTune::m_SpecularIntensity = DEFAULT_SPECULARINTENSITY;

float CMainWaterTune::GetSpecularIntensity(const tcKeyframeUncompressed& currKeyframe) const
{
	(void)currKeyframe;
#if __BANK
	if(m_OverrideTimeCycle)
		return m_SpecularIntensity;
	else
#endif //__BANK
		return currKeyframe.GetVar(TCVAR_WATER_SPECULAR_INTENSITY);
}

void CSecondaryWaterTune::interpolate(const CSecondaryWaterTune &src, const CSecondaryWaterTune &dst, float interp)
{
	#define SWTINTERP(a) a = src.a + ((dst.a-src.a)*interp)

	SWTINTERP(m_RippleBumpiness);
	SWTINTERP(m_RippleMinBumpiness);
	SWTINTERP(m_RippleMaxBumpiness);
	SWTINTERP(m_RippleBumpinessWindScale);
	SWTINTERP(m_RippleSpeed);
	SWTINTERP(m_RippleDisturb);
	SWTINTERP(m_RippleVelocityTransfer);
	SWTINTERP(m_OceanBumpiness);
	SWTINTERP(m_DeepOceanScale);
	SWTINTERP(m_OceanNoiseMinAmplitude);
	SWTINTERP(m_OceanWaveAmplitude);
	SWTINTERP(m_ShoreWaveAmplitude);
	SWTINTERP(m_OceanWaveWindScale);
	SWTINTERP(m_ShoreWaveWindScale);
	SWTINTERP(m_OceanWaveMinAmplitude);
	SWTINTERP(m_ShoreWaveMinAmplitude);
	SWTINTERP(m_OceanWaveMaxAmplitude);
	SWTINTERP(m_ShoreWaveMaxAmplitude);
	SWTINTERP(m_OceanFoamIntensity);
}

#if __BANK
void CSecondaryWaterTune::InitWidgets(bkBank *bank)
{
	bank->PushGroup("Water");
	
  bank->AddSlider("Ripple Bumpiness",				&(m_RippleBumpiness), 0.0f, 2.0f, 0.1f);
	bank->AddSlider("Ripple Min Bumpiness",			&(m_RippleMinBumpiness), 0.0f, 2.0f, 0.1f);
	bank->AddSlider("Ripple Max Bumpiness",			&(m_RippleMaxBumpiness), 0.0f, 2.0f, 0.1f);
	bank->AddSlider("Ripple Bumpiness Wind Scale",	&(m_RippleBumpinessWindScale), 0.0f, 1.0f, 0.1f);
	bank->AddSlider("Ripple Speed",					&(m_RippleSpeed), 0.0f, 50.0f, 0.01f);
	bank->AddSlider("Ripple Velocity Transfer",		&(m_RippleVelocityTransfer), 0.0f, 2.0f, 0.01f);
	bank->AddSlider("Ripple Disturb",				&(m_RippleDisturb), 0.0f, 2.0f, 0.01f);
	bank->AddSlider("Ocean Bumpiness",				&(m_OceanBumpiness), 0.0f, 2.0f, 0.05f );
	bank->AddSlider("Deep Ocean Scale",				&(m_DeepOceanScale), 0.0f, 10.0f, 0.5f);
	bank->AddSlider("Ocean Noise Min Amplitude",	&(m_OceanNoiseMinAmplitude),	0.0f, 50.0f, 1.00f);
	bank->AddSlider("Ocean Wave Amplitude",			&(m_OceanWaveAmplitude),		0.0f, 10.0f, 0.25f);
	bank->AddSlider("Shore Wave Amplitude",			&(m_ShoreWaveAmplitude),		0.0f, 20.0f, 0.25f);
	bank->AddSlider("Ocean Wave Wind Scale",		&(m_OceanWaveWindScale),		0.0f, 1.0f, 0.1f);
	bank->AddSlider("Shore Wave Wind Scale",		&(m_ShoreWaveWindScale),		0.0f, 1.0f, 0.1f);
	bank->AddSlider("Ocean Wave Min Amplitude",		&(m_OceanWaveMinAmplitude),		0.0f, 20.0f, 0.25f);
	bank->AddSlider("Shore Wave Min Amplitude",		&(m_ShoreWaveMinAmplitude),		0.0f, 40.0f, 0.25f);
	bank->AddSlider("Ocean Wave Max Amplitude",		&(m_OceanWaveMaxAmplitude),		0.0f, 20.0f, 0.25f);
	bank->AddSlider("Shore Wave Max Amplitude",		&(m_ShoreWaveMaxAmplitude),		0.0f, 40.0f, 0.25f);
	bank->AddSlider("Ocean Foam Intensity",			&(m_OceanFoamIntensity),		0.0f, 1.0f, 0.01f);
	bank->PopGroup();
}
#endif	

#if USE_WATER_REGIONS

class CWaterRegion
{
public:
	inline CWaterRegion() {}
	inline CWaterRegion(const CWaterQuad& quad) : m_bounds(Vec2V((float)quad.minX, (float)quad.minY), Vec2V((float)quad.maxX, (float)quad.maxY)), m_z(quad.GetZ())
	{
#if __DEV
		m_count = 1;
#endif // __DEV
	}

	inline void Grow(const CWaterQuad& quad)
	{
		m_bounds.GrowPoint(Vec2V((float)quad.minX, (float)quad.minY));
		m_bounds.GrowPoint(Vec2V((float)quad.maxX, (float)quad.maxY));
#if __DEV
		m_count++;
#endif // __DEV
	}

	spdRect m_bounds;
	ScalarV m_z;
#if __DEV
	int m_count;
#endif // __DEV
};

static atArray<CWaterRegion> g_waterRegions;
#if __DEV
static CWaterRegion g_waterRegion0;
#endif // __DEV

static void BuildWaterRegions(const atArray<CWaterQuad,128>& quads, bool DEV_ONLY(bReport))
{
	atMap<int,int> zmap;

#if __DEV
	g_waterRegion0.m_bounds.Invalidate();
	g_waterRegion0.m_z = ScalarV(V_ZERO);
	g_waterRegion0.m_count = 0;
#endif // __DEV

	for (int i = 0; i < quads.GetCount(); i++)
	{
		const CWaterQuad& quad = quads[i];
		const float quadZ = quad.GetZ();

		if (quadZ != 0.0f) // ignore z=0 quads, these are ocean and they surround the whole map
		{
			const int quadZi = *(const int*)&quadZ;

			int* pIndex = zmap.Access(quadZi);

			if (pIndex == NULL)
			{
				g_waterRegions.PushAndGrow(CWaterRegion(quad));
				zmap[quadZi] = g_waterRegions.GetCount() - 1;
			}
			else
			{
				g_waterRegions[*pIndex].Grow(quad);
			}
		}
		else // z=0
		{
#if __DEV
			g_waterRegion0.Grow(quad);
#endif // __DEV
		}
	}

#if __DEV
	if (bReport)
	{
		// z=0 region (encompasses entire map)
		{
			Displayf(
				"%d water quads using z=%f, x=[%f..%f], y=[%f..%f]",
				g_waterRegion0.m_count,
				g_waterRegion0.m_z.Getf(),
				g_waterRegion0.m_bounds.GetMin().GetXf(),
				g_waterRegion0.m_bounds.GetMax().GetXf(),
				g_waterRegion0.m_bounds.GetMin().GetYf(),
				g_waterRegion0.m_bounds.GetMax().GetYf()
			);
		}

		for (int i = 0; i < g_waterRegions.GetCount(); i++)
		{
			Displayf(
				"%d water quads using z=%f, x=[%f..%f], y=[%f..%f]",
				g_waterRegions[i].m_count,
				g_waterRegions[i].m_z.Getf(),
				g_waterRegions[i].m_bounds.GetMin().GetXf(),
				g_waterRegions[i].m_bounds.GetMax().GetXf(),
				g_waterRegions[i].m_bounds.GetMin().GetYf(),
				g_waterRegions[i].m_bounds.GetMax().GetYf()
			);
		}
	}
#endif // __DEV
}

ScalarV_Out GetWaterRegionZ(const spdRect& bounds)
{
	for (int i = 0; i < g_waterRegions.GetCount(); i++)
	{
		if (g_waterRegions[i].m_bounds.IntersectsRect(bounds))
		{
			return g_waterRegions[i].m_z;
		}
	}

	return ScalarV(V_ZERO);
}

ScalarV_Out GetWaterRegionMaxZ(const spdRect& bounds)
{
	ScalarV maxZ(V_ZERO);

	for (int i = 0; i < g_waterRegions.GetCount(); i++)
	{
		if (g_waterRegions[i].m_bounds.IntersectsRect(bounds))
		{
			maxZ = Max(g_waterRegions[i].m_z, maxZ);
		}
	}

	return maxZ;
}

#endif // USE_WATER_REGIONS

//////////////////////////////////////////////////////////////////////////
// WORLD EXTENTS
#define WORLD_V_BORDERXMIN					(-4000)		// world bounds for V
#define WORLD_V_BORDERYMIN					(-4000)
#define WORLD_V_BORDERXMAX					( 4500)
#define WORLD_V_BORDERYMAX					( 8000)
#define WORLD_ISLANDHEIST_BORDERXMIN		( 2700)		// world bounds for Island Heist DLC
#define WORLD_ISLANDHEIST_BORDERYMIN		(-7150)
#define WORLD_ISLANDHEIST_BORDERXMAX		( 6700)
#define WORLD_ISLANDHEIST_BORDERYMAX		(-3150)


static s32 sm_WorldBorderXMin				= WORLD_V_BORDERXMIN;
static s32 sm_WorldBorderYMin				= WORLD_V_BORDERYMIN;
static s32 sm_WorldBorderXMax				= WORLD_V_BORDERXMAX;
static s32 sm_WorldBorderYMax				= WORLD_V_BORDERYMAX;


static u32	sm_GlobalWaterXmlFile			= Water::WATERXML_V_DEFAULT;		// 0=V, 1=Island Heist DLC
static u32  sm_RequestedGlobalWaterXml		= u32(Water::WATERXML_INVALID);
static bool	sm_bReloadGlobalWaterXml		= false;


static void ResetWater();

//
//
//
//
void Water::LoadGlobalWaterXmlFile(u32 waterXml)
{
	Assertf((waterXml==WATERXML_V_DEFAULT) || (waterXml==WATERXML_ISLANDHEIST), "Invalid waterXml id (%d)!", waterXml);

	if(	!((waterXml==WATERXML_V_DEFAULT) || (waterXml==WATERXML_ISLANDHEIST)) )
	{
		return;
	}

	sm_GlobalWaterXmlFile = waterXml;

	if(sm_GlobalWaterXmlFile == WATERXML_V_DEFAULT)
	{	// V: water.xml
		sm_WorldBorderXMin	= WORLD_V_BORDERXMIN;
		sm_WorldBorderYMin	= WORLD_V_BORDERYMIN;
		sm_WorldBorderXMax	= WORLD_V_BORDERXMAX;
		sm_WorldBorderYMax	= WORLD_V_BORDERYMAX;

		LoadWaterTunings();
		InitFogTextureTxdSlots();
		ResetWater();
		LoadWater();

	}
	else if (sm_GlobalWaterXmlFile == WATERXML_ISLANDHEIST)
	{	// Island Heist DLC: water_HeistIsland.xml
		sm_WorldBorderXMin	= WORLD_ISLANDHEIST_BORDERXMIN;
		sm_WorldBorderYMin	= WORLD_ISLANDHEIST_BORDERYMIN;
		sm_WorldBorderXMax	= WORLD_ISLANDHEIST_BORDERXMAX;
		sm_WorldBorderYMax	= WORLD_ISLANDHEIST_BORDERYMAX;
		
		LoadWaterTunings("common:/data/watertune_HeistIsland.xml");
		InitFogTextureTxdSlots();

		const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::WATER_FILE);			
		while(DATAFILEMGR.IsValid(pData))
		{
			if(strstr(pData->m_filename,"water_HeistIsland.xml"))
			{
				ResetWater();
				LoadWater(pData->m_filename);
				break;
			}

			pData = DATAFILEMGR.GetNextFile(pData);
		}
	}
}

//
//
//
//
void Water::RequestGlobalWaterXmlFile(u32 reqWaterXml)
{
	// water reload already requested earlier this frame?
	if(sm_bReloadGlobalWaterXml)
	{	// then cancel any previous request(s):
		sm_RequestedGlobalWaterXml	= u32(WATERXML_INVALID);
		sm_bReloadGlobalWaterXml	= false;
	}

	if(sm_GlobalWaterXmlFile == reqWaterXml)
		return;	// do nothing - requested water.xml already active

	Assertf((reqWaterXml==WATERXML_V_DEFAULT) || (reqWaterXml==WATERXML_ISLANDHEIST), "Invalid waterXml id (%d)!", reqWaterXml);
	sm_RequestedGlobalWaterXml		= reqWaterXml;
	sm_bReloadGlobalWaterXml		= true;
}


//
//
//
//
u32	Water::GetGlobalWaterXmlFile()
{
	return sm_GlobalWaterXmlFile;
}


#define WATERBLOCKWIDTH						(500)
#define FOGTEXTURERESOLUTION				(256)
#define FOGTEXELSIZE						(WATERBLOCKWIDTH/((float)FOGTEXTURERESOLUTION))
//////////////////////////////////////////////////////////////////////////
#define WORLD_WIDTH							(sm_WorldBorderXMax - sm_WorldBorderXMin)
#define WORLD_HEIGHT						(sm_WorldBorderYMax - sm_WorldBorderYMin)


#define WATER_SHIFT_HEIGHT_FAR				(70.0f)
#define DYN_WATER_FADEDIST					(10)
#define DYN_WATER_FADEDIST_INVF				(0.1f)
#define DETAILEDWATERDIST					(DYNAMICGRIDELEMENTS)
#define WATERLEVEL_DEFAULTHEIGHT 			(0.0f)		// Water level outside the map


#define WATERNUMBLOCKSX 					((WORLD_WIDTH	) / WATERBLOCKWIDTH)
#define WATERNUMBLOCKSY 					((WORLD_HEIGHT	) / WATERBLOCKWIDTH)


#define WORLD_WIDTH_MAX						(WORLD_V_BORDERXMAX - WORLD_V_BORDERXMIN)
#define WORLD_HEIGHT_MAX					(WORLD_V_BORDERYMAX - WORLD_V_BORDERYMIN)
#define WATERNUMBLOCKSX_MAX					((WORLD_WIDTH_MAX)	/ WATERBLOCKWIDTH)		// max allowed dimensions used by arrays
#define WATERNUMBLOCKSY_MAX 				((WORLD_HEIGHT_MAX) / WATERBLOCKWIDTH)


//Helpers
#define WATERWORLDTOBLOCKX(c)				((((c) + WATERBLOCKWIDTH*100000) - sm_WorldBorderXMin) / WATERBLOCKWIDTH - 100000)
#define WATERWORLDTOBLOCKY(c)				((((c) + WATERBLOCKWIDTH*100000) - sm_WorldBorderYMin) / WATERBLOCKWIDTH - 100000)
static s32	GetBlockFromWorldX(float c)		{ return WATERWORLDTOBLOCKX((s32)floor(c)); }
static s32	GetBlockFromWorldY(float c)		{ return WATERWORLDTOBLOCKY((s32)floor(c)); }
#define WATERBLOCKTOWORLDX(x) 				(((x) * WATERBLOCKWIDTH) + sm_WorldBorderXMin)
#define WATERBLOCKTOWORLDY(y) 				(((y) * WATERBLOCKWIDTH) + sm_WorldBorderYMin)
inline s32 _WaterBlockIndexToBlockX(s32 i, const s32 waterNumBlocksY)		{ return(i/waterNumBlocksY); }
inline s32 _WaterBlockIndexToBlockY(s32 i, const s32 waterNumBlocksY)		{ return(i%waterNumBlocksY); }
inline s32 WaterBlockIndexToWorldX(s32 i,  const s32 waterNumBlocksY)		{ return(WATERBLOCKTOWORLDX(_WaterBlockIndexToBlockX(i,waterNumBlocksY))); }
inline s32 WaterBlockIndexToWorldY(s32 i,  const s32 waterNumBlocksY)		{ return(WATERBLOCKTOWORLDY(_WaterBlockIndexToBlockY(i,waterNumBlocksY))); }

inline s32 WaterBlockXYToIndex(s32 x, s32 y, const s32 waterNumBlocksY)		{ return (x*waterNumBlocksY) + y;}


#define	WATERGRIDSIZE 						(2)
#define WATERGRIDSIZE_INVF					(1.0f/((float)WATERGRIDSIZE))
#define MAXVERTICESINSTRIP					(DYNAMICGRIDELEMENTS)

#define QUADTRIS_SHIFT 					(14)
#define QUADTRIS_MASK  					(0x3fff)
#define WATER_STENCIL_ID				(DEFERRED_MATERIAL_SPECIALBIT)
#define WATER_STENCIL_MASKID			(DEFERRED_MATERIAL_SPAREMASK) // Lighting sets OR0 or OR1 but not both, so I can check both, it is cleared on the 360

#define WATERFLAGS_VISIBLE				(1<<0)
#define WATERFLAGS_LIMITEDDEPTH			(1<<1)
#define WATERFLAGS_STANDARDPOLY			(1<<2)
#define WATERFLAGS_CALMINGPOLY			(1<<3)

#define FOGTEXTUREBLOCKSPAN				(1)
#define FOGTEXTUREBLOCKWIDTH			(1 + FOGTEXTUREBLOCKSPAN*2)

#define WATEROPACITY_UNDERWATER_VFX	(0.1f)

u32				Water::ms_bufIdx	= 0;
static s32		m_WaterFogIndex[2];
static s32		m_WaterHeightMapIndex;
static bank_s32	m_WaterHeightMapUpdateIndexOffset	= 1;

static bool		waterDataLoaded		= false;
static s32		m_WaterFogTxdSlots[WATERNUMBLOCKSX_MAX * WATERNUMBLOCKSY_MAX];
#if RSG_PC
	static s32	m_WaterFogTxdSlotsDX10[WATERNUMBLOCKSX_MAX * WATERNUMBLOCKSY_MAX];
	#if __BANK
		static bool	m_UseAlternateFogStreamingTxd = false;
	#endif // __BANK
#endif // RSG_PC
static bool		m_bResetSim		= false;

#if RSG_PC
bool	Water::m_bDepthUpdated					= false;
#endif

#if __BANK
bool	Water::m_bForceWaterPixelsRenderedON	= false;
bool	Water::m_bForceWaterPixelsRenderedOFF	= false;
bool	Water::m_bForceWaterReflectionON		= false;
bool	Water::m_bForceWaterReflectionOFF		= false;
bool	Water::m_bForceMirrorReflectionOn		= false;
bool	Water::m_bForceMirrorReflectionOff		= false;
bool	Water::m_bUnderwaterPlanarReflection	= true;
int		Water::m_debugReflectionSource			= Water::WATER_REFLECTION_SOURCE_DEFAULT;
bool	Water::m_bRunScanCutBlocksEarly			= true;
#if WATER_SHADER_DEBUG
float	Water::m_debugAlphaBias					= 0.0f;
float	Water::m_debugRefractionOverride		= 0.0f;
float	Water::m_debugReflectionOverride		= 0.0f;
float	Water::m_debugReflectionOverrideParab	= 0.0f;
float	Water::m_debugReflectionBrightnessScale = 1.0f;
bool	Water::m_debugWaterFogBiasEnabled		= true;
float	Water::m_debugWaterFogDepthBias			= 0.0f;
float	Water::m_debugWaterFogRefractBias		= 0.0f;
float	Water::m_debugWaterFogColourBias		= 0.0f;
float	Water::m_debugWaterFogShadowBias		= 0.0f;
bool	Water::m_debugDistortionEnabled			= true;
float	Water::m_debugDistortionScale			= 1.0f;
float	Water::m_debugHighlightAmount			= 0.0f;
#endif // WATER_SHADER_DEBUG
bool	g_show_water_clip						= false;
bool	m_HighDetailDrawn						= false;
bool	m_EnableTestBuoys						= false;
bool	m_DrawShoreWaves						= false;
bool	m_bkReloadWaterData						= false;
s32		m_NumWaterTris							= 0;
s32		m_NumQuadsDrawn							= -1;
s32		m_NumBlocksDrawn						= -1;
s32		m_16x16Drawn							= -1;
s32		m_NumPixelsRendered						= -1;
s32		m_QueryFrameDelay						= -1;
float	m_UpdateTime							= -1.0f;
float	m_ReflectionTime						= -1.0f;
float	m_PreRenderTime							= -1.0f;
float	m_RenderTime							= -1.0f;
s32		m_ReflectionRenderRange					= -1;
s32		m_DisturbCount							= 0;
s32		m_DisturbCountRead						= 0;
bool	m_DisableUnderwater						= false;

bool		m_frustumHasBeenLocked		= false;
bool		m_frustumStored				= false;
bool		m_drawFrustum				= false;
Mat44V		m_lockedFrustum;

#if __BANK
s32		m_TotalDrawCalls							= 0;
s32		m_HighDetailDrawCalls						= 0;
s32		m_FlatWaterQuadDrawCalls					= 0;
s32		m_WaterFLODDrawCalls						= 0;

s32		m_TotalDrawCallsDisplay						= 0;
s32		m_HighDetailDrawCallsDisplay				= 0;
s32		m_FlatWaterQuadDrawCallsDisplay				= 0;
s32		m_WaterFLODDrawCallsDisplay					= 0;
#endif //__BANK

atArray<grcTexChangeData>	m_TexChangeDataBuffer;
struct WaterTexChangeTexture
{
	grcTexture* m_Texture;
	bool m_MarkForDelete;
};
atArray<WaterTexChangeTexture>	m_TexChangeTextureBuffer[2];
struct WaterBuoy
{
	Vector2 pos;
	Color32 color;
};
atRangeArray <WaterBuoy, 64> m_WaterBuoyBuffer;

void UpdateTestBuoys()
{
	for(s32 i = 0; i < 64; i++)
	{
		Vector2 flow;
		River::GetRiverFlowFromPosition(Vec3V(m_WaterBuoyBuffer[i].pos.x, m_WaterBuoyBuffer[i].pos.y, 0.0f), flow);
		m_WaterBuoyBuffer[i].pos += flow;
		m_WaterBuoyBuffer[i].color = Color32(flow.x/River::GetRiverPushForce()*.5f+.5f, flow.y/River::GetRiverPushForce()*.5f+.5f, 0.0f, 0.0f);
	}
}

void DrawTestBuoys()
{
	if(!m_EnableTestBuoys)
		return;
	// I'm assuming that depth test should be on normally here.
	grcStateBlock::SetDepthStencilState(CShaderLib::DSS_Default_Invert);
	float h = Water::GetWaterLevel()+2.0f;
	grcWorldIdentity();
	for(int i = 0; i < 64; i++)
	{
		grcColor(m_WaterBuoyBuffer[i].color);
		grcDrawSphere(0.5f, Vector3(m_WaterBuoyBuffer[i].pos.x, m_WaterBuoyBuffer[i].pos.y, h), 8, false, true);
	}
}

void SetShaderEditVars()
{
	s32 b = Water::GetWaterRenderIndex();
	s32 count = m_TexChangeTextureBuffer[b].GetCount();
	for(s32 i = 0; i < count; i++)
	{
		grcTexture* texture = m_TexChangeTextureBuffer[b][i].m_Texture;
		if(texture)
		{
			grcEffect &effect = m_TexChangeDataBuffer[i].m_instance->GetBasis();
			effect.SetVar(*(m_TexChangeDataBuffer[i].m_instance), effect.GetVarByIndex(m_TexChangeDataBuffer[i].m_varNum), texture);
		}
	}
}
#else
const bool m_bRunScanCutBlocksEarly = true;
#endif // __BANK

struct WaterFogTextureData
{
	strLocalIndex txdSlot;
	bool loaded;
};


class CWaterBlockData
{
public:
	float minHeight;
	float maxHeight;
	s16 numQuads;
	s16 quadIndex;
};
atFixedArray<CWaterBlockData,		WATERNUMBLOCKSX_MAX*WATERNUMBLOCKSY_MAX>		m_WaterBlockData;
atFixedArray<u16,					WATERNUMBLOCKSX_MAX*WATERNUMBLOCKSY_MAX>		m_WaterBlockDrawList[2];
#if RSG_PC
atFixedArray<u16,					WATERNUMBLOCKSX_MAX*WATERNUMBLOCKSY_MAX>		m_WaterBlockDrawListTemp;
#endif

#if __XENON || __PS3
#define MAXCALMINGQUADSTORENDER	(16)
#define MAXWAVEQUADSTORENDER	(16)
#else
#define MAXCALMINGQUADSTORENDER	(64)
#define MAXWAVEQUADSTORENDER	(64)
#endif

#if __PS3
atFixedArray<CCalmingQuad,	MAXCALMINGQUADSTORENDER>	m_CalmingQuadsToRender	ALIGNED(128);
atFixedArray<CWaveQuad,		MAXWAVEQUADSTORENDER>		m_WaveQuadsToRender		ALIGNED(128);
#endif // __PS3

atFixedArray<WaterFogTextureData,	FOGTEXTUREBLOCKWIDTH*FOGTEXTUREBLOCKWIDTH>	m_FogTextureData[3];

s32		m_QuadsToRendered[2]		= {0, 0};
s32		m_IsCameraDiving[2]			= {0, 0};
s32		m_IsCameraCutting[2]		= {0, 0};
float	m_CameraWaterDepth[2]		= {0.0f, 0.0f};
float	m_CameraFlatWaterHeight[2]	= {0.0f, 0.0f};
bool	m_WaterHeightDefault[2]		= {true, true};
float	m_CameraWaterHeight[2]		= {0.0f, 0.0f};
float	m_WaterOpacityUnderwaterVfx	= WATEROPACITY_UNDERWATER_VFX;
float	m_AverageSimHeight			= 0.0f;
bool	m_IsCameraUnderWater[2]		= {false, false};
bool	m_UseFogPrepass[2]			= {false, false};
bool	m_UseHQWaterRendering[2]	= {false, false};
bool	m_UsePlanarReflection		= true;
bool	m_BuffersFlipped			= false;
bool	m_bLocalPlayerInFirstPersonMode = false;

float	Water::m_waterSurfaceTextureSimParams[8];

static atFixedArray<WaterDisturb,	MAXDISTURB> m_WaterDisturbBuffer[2];
static atFixedArray<u8,				MAXDISTURB> m_WaterDisturbStack[2];
static atFixedArray<WaterFoam,		MAXFOAM>	m_WaterFoamBuffer[2];

//GPU Update Stuff ===============================================
#if __GPUUPDATE
#define	CPU_WATER_MINIMUM_CORES_COUNT	4
#define CPU_WATER_MINIMUM_CLOCK_SPEED	2.6f

bool m_GPUUpdate;
bank_bool m_GPUUpdateEnabled  = false;
bank_bool m_CPUUpdateEnabled  = false;
bank_bool m_HeightGPUReadBack = true;

#if __BANK
static bank_s32 m_PackedDisturbCount;
float m_GPUUpdateTime			= -1.0f;
s32 m_NumCalmingQuadsDrawn		= -1;
s32 m_NumWaveQuadsDrawn			= -1;
#endif //__BANK
#endif //!__GPUUPDATE =============================================

bool m_BorderQuadsVisible		= false;

#if __BANK
bank_bool m_DrawCalmingQuads	= false;
#endif // __BANK
bank_bool m_DrawWaterQuads		= true;
bank_bool m_DrawDepthOccluders	= true;
bank_bool m_DisableTessellatedQuads = false;
bank_bool m_EnableFogStreaming	= true;
bank_bool m_IgnoreTCFogStreaming = false;
bool m_EnableTCFogStreaming		= true;
bool m_EnableWaterLighting	= true;
#if WATER_TILED_LIGHTING
bank_bool m_EnableFogPrepassHiZ	 = false;
bank_bool m_UseTiledFogComposite = false;
#endif //WATER_TILED_LIGHTING
#if __BANK
static Vector2 m_FogCompositeTexOffset(0.f, 0.f);
#endif
bank_bool m_EnableVisibility	= true;
#if __USEVERTEXSTREAMRENDERTARGETS
bank_bool m_UseVSRT				= true;
bank_bool m_VSRTRenderAll		= false;
#endif //__USEVERTEXSTREAMRENDERTARGETS

static __forceinline bool fogStreamingEnabled()
{
	return m_EnableFogStreaming && (m_IgnoreTCFogStreaming || m_EnableTCFogStreaming);
}

/*#define MAXFOAM 256
struct WaterFoam
{
	s16 x, y;
	float time;
};
static atFixedArray<WaterFoam, MAXFOAM> WaterFoamBuffer;
sysCriticalSectionToken m_CriticalSection;
*/

bank_bool m_TrianglesAsQuads=false;
#if __BANK
bool m_TrianglesAsQuadsForOcclusion=false;
bool m_RasterizeWaterQuadsIntoStencil=true;
bool m_RasterizeRiverBoxesIntoStencil=false;
bool m_RasterizeRiverBoxesIntoStencilDebugDraw=false;
#endif // __BANK
bank_float m_WaveHeightRange = 6.0f;

static bank_bool m_bUpdateDynamicWater		= true;
static bank_bool m_bWaterHeightMapRender	= true;

#if RSG_PC
static bank_bool m_bEnableExtraQuads	 = true;
static Water::WaterUpdateShaderParameters* s_GPUWaterShaderParams   = NULL;
static Water::WaterBumpShaderParameters* s_GPUWaterBumpShaderParams = NULL;
static Water::FoamShaderParameters* s_GPUFoamShaderParams = NULL;
#endif

#if __BANK
static float m_waterTestOffset				= 0.0f;
static bool m_bWireFrame					= false;
#endif // __BANK
static bank_bool m_bUseCamPos				= false;
static bank_bool m_bFreezeCameraUpdate		= false;
static bank_u32 m_WaterPixelCountThreshold	= WATER_PIXELCOUNT_DEFAULT_THRESHOLD;

static bank_float m_DisturbScale			= 3.0f;

static bank_float m_WaveLengthScale	= 0.5f;
static bank_s32 m_WaveTimePeriod	= 4000;		// Don't change this lightly as recordings will look crap when changed.
static bank_float m_TimeStepScale	= 0.75f;

static float(*m_WaterHeightBuffer)      [DYNAMICGRIDELEMENTS];
static float(*m_WaterHeightBufferCPU[2])[DYNAMICGRIDELEMENTS] = {NULL, NULL};
static float(*m_WaterVelocityBuffer)    [DYNAMICGRIDELEMENTS];

static atArray<CWaterQuad,		128>	m_WaterQuadsBuffer;
static atArray<CCalmingQuad,	128>	m_CalmingQuadsBuffer;
static atArray<CWaveQuad,		128>	m_WaveQuadsBuffer;
static Vector3	m_DynamicWater_Coords_RT[2] ALIGNED(128);// Water height as an offset to the height as defined by the artists (2 values for double buffering)

static bool m_doReplayWaterSimulate = false;

Mat44V	m_ReflectionMatrix[2];

static s32	RenderCameraRelX, RenderCameraRelY;
static s32	RenderCameraRangeMinX, RenderCameraRangeMaxX, RenderCameraRangeMinY, RenderCameraRangeMaxY;
static s32	RenderCameraBlockX, RenderCameraBlockY;
static s32	RenderCameraBlockMinX, RenderCameraBlockMaxX, RenderCameraBlockMinY, RenderCameraBlockMaxY;
static float	MinBorder;
static float	MaxBorder;

static s32  m_ReflectionRenderBlockSize = 32;
Vector4		m_GlobalFogUVParams;
Vec4V		gWaterDirectionalColor;
Vec4V		gWaterAmbientColor;


const CRenderPhaseScanned* m_WaterRenderPhase = NULL;

static s32	m_ElementsOnQuadsAndTrianglesList;
static float	m_CurrentDefaultHeight = WATERLEVEL_DEFAULTHEIGHT;
static float	m_RefractionIndex = DEFAULT_REFRACTIONINDEX;

#if !RSG_ORBIS
static sysTaskHandle	m_WaterTaskHandle=NULL;
#endif

static Mat44V	m_GBufferRenderPhaseFrustum;
#if __BANK
static bool m_bUseFrustumCull = true;
#endif // __BANK


//================ Textures and Render Targets =====================
static bank_bool m_UseMultiBuffering = RSG_PC;
#if RSG_DURANGO || RSG_ORBIS

#if GTA_REPLAY && REPLAY_WATER_USES_TEXTURE_MEM_DIRECTLY
// We need more textures/buffers when Replay is active.
#define WATERHEIGHTBUFFERING   (5)
#else // GTA_REPLAY && REPLAY_WATER_USES_TEXTURE_MEM_DIRECTLY
#define WATERHEIGHTBUFFERING   (3)
#endif // GTA_REPLAY && REPLAY_WATER_USES_TEXTURE_MEM_DIRECTLY

#define WATERVELOCITYBUFFERING (2)
const char* m_WaterVelocityMapNames[WATERVELOCITYBUFFERING] = {"Velocity Map 0", "Velocity Map 1"};
const char* m_WaterHeightMapNames[WATERHEIGHTBUFFERING]     = {"Height Map 0",   "Height Map 1",   "Height Map 2"};
#else //RSG_DURANGO || RSG_ORBIS
#define WATERHEIGHTBUFFERING   (3)
#define WATERVELOCITYBUFFERING (3)
const char* m_WaterVelocityMapNames[WATERVELOCITYBUFFERING] = {"Velocity Map 0", "Velocity Map 1", "Velocity Map 2"};
const char* m_WaterHeightMapNames[WATERHEIGHTBUFFERING]     = {"Height Map 0",   "Height Map 1",   "Height Map 2"};
#endif //RSG_DURANGO || RSG_ORBIS


static grcTexture*			m_HeightMap        [WATERHEIGHTBUFFERING];
static grcTexture*			m_VelocityMap      [WATERVELOCITYBUFFERING];

static grcRenderTarget*		m_HeightMapRT      [WATERHEIGHTBUFFERING];
static grcRenderTarget*		m_VelocityMapRT    [WATERVELOCITYBUFFERING];

static grcRenderTarget*		m_HeightTempRT;
static grcRenderTarget*     m_VelocityTempRT;

static grcRenderTarget*		m_WorldBaseRT;
static float*				m_WorldBaseRTX;
static float*				m_WorldBaseRTY;
//Used to optimise FindDynamicWaterHeight_RT by caching the cast s32 for the above world base arrays.
static s32					m_tWorldBaseRTX;
static s32					m_tWorldBaseRTY;

#if __D3D11
grcTexture*					m_WorldBaseTex;
static float				m_WorldBaseRTXVal;
static float				m_WorldBaseRTYVal;
#if RSG_PC
static s32					m_HeightMapBaseX[WATERHEIGHTBUFFERING];
static s32                  m_HeightMapBaseY[WATERHEIGHTBUFFERING];
#endif
#endif

#if GTA_REPLAY
static Vector3				m_ReplayCoordsBuffer[WATERHEIGHTBUFFERING];
#endif

grcTexture*					Water::m_noiseTexture	= NULL;
grcTexture*					Water::m_CausticTexture	= NULL;

grcTexture*							m_OceanWavesTexture		= NULL;
grcTexture*							m_ShoreWavesTexture		= NULL;

#define BUFFERED_WET_MAP_SOLUTION (RSG_ORBIS || __D3D11)
#if BUFFERED_WET_MAP_SOLUTION
grcTexture*							m_WetMap[2]				= {NULL, NULL};
#if RSG_ORBIS
grcTexture*							m_FoamAccumulation		= NULL;
#endif
#else
grcTexture*							m_WetMap				= NULL;
#endif

static grcTexture*					m_CurrentFogTexture		= NULL;
static grcTexture*					m_FogTexture_V			= NULL;
static grcTexture*					m_FogTexture_H4			= NULL;

grcTexture*							m_BumpTexture			= NULL; //Static bump texture
grcTexture*							m_BumpTexture2			= NULL; //Low pixel bump texture
grcTexture*							m_CurrentBumpTexture	= NULL;
grcTexture*							m_CurrentBump2Texture	= NULL;
grcTexture*							m_FoamTexture			= NULL;
grcTexture*							m_CloudTexture			= NULL;

grcRenderTarget*					Water::sm_pWaterSurface	= NULL;
grcRenderTarget*					m_BumpRT				= NULL;
grcRenderTarget*					m_Bump4RT				= NULL;
grcRenderTarget*					m_BumpHeightRT[2]		= {NULL, NULL};
grcRenderTarget*					m_BumpHeight4RT			= NULL;
grcRenderTarget*					m_BumpRainRT			= NULL;
#if __PS3
grcRenderTarget*					m_BumpTempRT;
grcRenderTarget*					m_BumpTemp4RT;
#endif

grcRenderTarget*					m_DownSampleRT;
grcRenderTarget*                    m_RefractionRT;

grcRenderTarget*					m_RefractionSource;

grcRenderTarget*					m_LightingRT;
grcRenderTarget*					m_Lighting4RT;
grcRenderTarget*					m_Lighting16RT;
grcRenderTarget*					m_RefractionMaskRT;
grcRenderTarget*					m_FogDepthRT;
grcRenderTarget*					m_BlendRT;

grcRenderTarget*					m_Lighting16TempRT;

//grcRenderTarget*					m_CausticsRT;
//grcRenderTarget*					m_CausticsBumpRT;

grcRenderTarget*					m_ReflectionRT;
#if RSG_PC
grcRenderTarget*					m_MSAAReflectionRT;
#endif
#if BUFFERED_WET_MAP_SOLUTION
grcRenderTarget*					m_WetMapRT[2];
#if RSG_ORBIS
grcRenderTarget*					m_FoamAccumulationRT;
#endif
#else
grcRenderTarget*					m_WetMapRT;
grcRenderTarget*					m_WetMapRTCopy;
#endif
grcRenderTarget*					m_FoamRT;

#if WATER_TILED_LIGHTING
grcRenderTarget*					m_ClassificationRT;
#endif //WATER_TILED_LIGHTING

#if __USEVERTEXSTREAMRENDERTARGETS
grcRenderTarget*					m_WaterTessellationFogRT;
grcRenderTarget*					m_WaterVertexParamsRT;
grcRenderTarget*					m_WaterVertexHeightRT;
#endif //__USEVERTEXSTREAMRENDERTARGETS

//=================================================================



//======= Water Shader Vars =========
//Techniques
static grmShader*		m_waterShader = NULL;
static					grcEffectTechnique	m_WaterTechnique;
#if __USEVERTEXSTREAMRENDERTARGETS
static					grcEffectTechnique	m_VertexStreamTechnique;
#endif //__USEVERTEXSTREAMRENDERTARGETS
PS3_ONLY(static			grcEffectGlobalVar  m_depthSampler);
#if __BANK
static					grcEffectTechnique	m_WaterDebugOverlayTechnique;
static					grcEffectVar		m_WaterDebugOverlayDepthBufferVar;
#endif // __BANK

//Variables
static grcEffectVar				m_WaterLocalParamsVar;
static grcEffectVar				QuadAlpha_ID;
#if __USEVERTEXSTREAMRENDERTARGETS
static grcEffectVar				m_CameraPositionVar;
#endif //__USEVERTEXSTREAMRENDERTARGETS
static grcEffectGlobalVar		m_WaterWorldBaseVSVar;
static grcEffectGlobalVar		m_WaterParams0Var;
static grcEffectGlobalVar		m_WaterParams1Var;
static grcEffectGlobalVar		m_WaterParams2Var;
static grcEffectGlobalVar		m_WaterAmbientColorVar;
static grcEffectGlobalVar		m_WaterDirectionalColorVar;
static grcEffectGlobalVar		m_WaterHeightTextureVar;
static grcEffectGlobalVar		m_StaticBumpTextureVar;
static grcEffectGlobalVar		m_WaterBumpTextureVar;
static grcEffectGlobalVar		m_WaterBumpTexture2Var;
static grcEffectGlobalVar		m_WaterReflectionMatrixVar;
static grcEffectGlobalVar		m_WaterRefractionMatrixVar;
static grcEffectGlobalVar		m_WaterLightingTextureVar;
static grcEffectGlobalVar		m_WaterBlendTextureVar;
static grcEffectGlobalVar		m_WaterStaticFoamTextureVar;
static grcEffectGlobalVar		m_WaterWetTextureVar;

static grcEffectGlobalVar		m_WaterDepthTextureVar;
static grcEffectGlobalVar		m_WaterParamsTextureVar;
static grcEffectGlobalVar       m_WaterVehicleProjParamsVar;
static grcEffectVar				m_WaterFogParamsVar;
static grcEffectVar				m_WaterFogTextureVar;
static grcEffectVar				m_HeightTextureVar;
static grcEffectVar             m_VelocityTextureVar;
static grcEffectVar				m_NoiseTextureVar;
static grcEffectVar				m_BumpHeightTextureVar;
//==================================


//======= Surface Shader Vars ======
static grmShader*				m_WaterTextureShader = NULL; 

static grcEffectTechnique		m_BumpTechnique;
//static grcEffectTechnique		m_CausticsTechnique;
static grcEffectTechnique		m_WetUpdateTechnique;
#if __GPUUPDATE
static grcEffectTechnique		m_WaterUpdateTechnique;
#endif //__GPUUPDATE
static grcEffectTechnique		m_DownSampleTechnique;

static grcEffectVar				m_ProjParamsVar;
static grcEffectVar				UpdateOffset_ID;
static grcEffectVar				UpdateParams0_ID;
#if __GPUUPDATE
static grcEffectVar				UpdateParams1_ID;
static grcEffectVar				UpdateParams2_ID;
#endif //__GPUUPDATE

//Variables
static grcEffectVar				m_DepthTextureVar;
static grcEffectVar				m_PointTextureVar;
static grcEffectVar				m_PointTexture2Var;
static grcEffectVar				m_LinearTextureVar;
static grcEffectVar				m_LinearTexture2Var;
static grcEffectVar				m_LinearWrapTextureVar;
static grcEffectVar				m_LinearWrapTexture2Var;
static grcEffectVar				m_LinearWrapTexture3Var;
static grcEffectVar				m_BumpTextureVar;
static grcEffectVar				m_FullTextureVar;
static grcEffectVar				WaterSurfaceSimParams_ID;
static grcEffectVar				m_WaterCompositeParamsVar;
static grcEffectVar				m_WaterCompositeAmbientColorVar;
static grcEffectVar				m_WaterCompositeDirectionalColorVar;
static grcEffectVar				m_WaterCompositeTexOffsetVar;
//==================================

//======= Renderstate Descs =======
static grcDepthStencilStateHandle	m_WaterDepthStencilState;
static grcDepthStencilStateHandle	m_WaterOccluderDepthStencilState;
static grcDepthStencilStateHandle	m_WaterRefractionAlphaDepthStencilState;
static grcDepthStencilStateHandle	m_BoatDepthStencilState;
static grcBlendStateHandle			m_AddFoamBlendState;
static grcBlendStateHandle			m_WaterRefractionAlphaBlendState;
static grcBlendStateHandle          m_UpscaleShadowMaskBlendState;
static grcRasterizerStateHandle		m_UnderwaterRasterizerState;
#if __BANK
static grcDepthStencilStateHandle	m_WaterTiledScreenCaptureDepthStencilState;
#endif // __BANK
//=================================

//============== Water rendering globals ====================
s32 EnvTechniqueGroup;
s32 UnderwaterLowTechniqueGroup;
s32 UnderwaterHighTechniqueGroup;
s32 UnderwaterEnvLowTechniqueGroup;
s32 UnderwaterEnvHighTechniqueGroup;
s32 WaterDepthTechniqueGroup;
s32 FoamTechniqueGroup;
s32 SinglePassTechniqueGroup;
s32 SinglePassEnvTechniqueGroup;
s32 WaterHeightTechniqueGroup;
s32 InvalidTechniqueGroup;
#if __USEVERTEXSTREAMRENDERTARGETS
s32 VertexParamsTechniqueGroup;
#endif //__USEVERTEXSTREAMRENDERTARGETS

static DECLARE_MTR_THREAD s32	m_CurrentTrisOrientation;
static DECLARE_MTR_THREAD s32	GridSize             = DYNAMICGRIDSIZE;
static DECLARE_MTR_THREAD bool	RenderHighDetail     = false;
static DECLARE_MTR_THREAD bool	TessellateHighDetail = true;
static bool	FillDrawList			= false;
//===========================================================


// Storage for the 'to be rendered' lists.
struct sDBVars {
	s32	m_CameraRelX;
	s32	m_CameraRelY;
	s32	m_CameraRangeMinX, m_CameraRangeMaxX, m_CameraRangeMinY, m_CameraRangeMaxY;
#if GTA_REPLAY

	float *m_pReplayWaterHeightPlayback; // Source for the next frame (set by game thread).
	float *m_pReplayWaterHeightResultsToRT; // Destination for next frames read back results (set by game thread).
	Vector3 m_CentreCoordsResultsFromRT; // Last frames coords read back from GPU (set by RT).
	float *m_pReplayWaterHeightResultsFromRT; // Last frames height buffer read back from GPU (filled in by RT).
#endif // GTA_REPLAY
};

struct WaterVertex{
	s16 x;
	s16 y;
};
struct WaterVertexLow{
	s16 x;
	s16 y;
	float z;
};

grcVertexDeclaration*			m_WaterVertexDeclaration;
grcVertexDeclaration*			m_WaterLowVertexDeclaration;
#if __USEVERTEXSTREAMRENDERTARGETS
grcVertexDeclaration*			m_WaterVSRTDeclaration;
#endif //__USEVERTEXSTREAMRENDERTARGETS
PS3_ONLY(grcVertexDeclaration*	m_WaterCausticsDeclaration);

//============== Water Tessellation Defines ====================
#define VERTSWIDTH			(DYNAMICGRIDELEMENTS + 1)
#define QUADSWIDTH			(VERTSWIDTH - 1)
#define BLOCKQUADSWIDTH		(16)
#define BLOCKVERTSWIDTH		(BLOCKQUADSWIDTH + 1)
#define BLOCKSWIDTH			(QUADSWIDTH/BLOCKQUADSWIDTH)
#define VERTSPERBLOCK		(BLOCKVERTSWIDTH*2*BLOCKQUADSWIDTH+BLOCKQUADSWIDTH*2)
//Defines for water tri-strip vertex buffer
#define NUMVERTICES			(VERTSPERBLOCK*(BLOCKSWIDTH*BLOCKSWIDTH))
//Defines for water vertex + tri-strip index buffer
//#define NUMVERTICES			(VERTSWIDTH*VERTSWIDTH)
//#define NUMINDICES			(VERTSPERBLOCK*(BLOCKSWIDTH*BLOCKSWIDTH))

grcVertexBuffer*			m_WaterVertexBuffer;
//grcIndexBuffer*				m_WaterIndexBuffer;
#if WATER_TILED_LIGHTING
grcVertexBuffer*			m_WaterFogCompositeVertexBuffer;
#endif //WATER_TILED_LIGHTING
#if __USEVERTEXSTREAMRENDERTARGETS
grcVertexBuffer*			m_WaterHeightVertexBuffer;
grcVertexBuffer*			m_WaterParamsVertexBuffer;

struct CVertexBlockDrawList
{
	bool	m_Draw;
	float	m_Height;
};

atRangeArray<CVertexBlockDrawList, BLOCKSWIDTH*BLOCKSWIDTH> m_VertexBlockDrawList;
#endif //__USEVERTEXSTREAMRENDERTARGETS


//=============================================================
//TODO: Implement single block version for NG(256x256 exceeds 65536 indices)
bank_bool m_Use16x16Block = __D3D11;  //vertexBuffer offsets don't work on Orbis...

//Vertex stream technique enums
enum
{
	pass_vsrtparams,
	PS3_ONLY(pass_vsrtheight)
};

enum{
	passdownsample,
	passdownsample_underwater,
	passdownsample_underwaterdepth,
	passdownsample_lighting
};
//Height Map Shader Pass Enums
enum{
	pass_updatevelocity,
	pass_updateheight,
	pass_updatedisturb,
	pass_updatecalming,
	pass_updatewave,
	pass_updateworldbase
};

//Bump Map Shader Pass Enums
enum{
	pass_updatefull,
	pass_copyrain,
#if __PS3
	pass_updatetemp,
	pass_copyheight,
	pass_copybump,
#else
	pass_updatemrt,
#endif //__PS3
};

//Bump Map Shader Pass Enums
enum{
	pass_caustics,
	pass_causticsprepass,
#if __PS3
	pass_causticsverts,
#endif //__XENON
};

//Downsample pass enums
enum
{
	pass_waterfogcomposite,
	pass_waterfogcompositetiled,
	pass_underwaterdownsampledepth,
	pass_underwaterdownsamplesoft,
	pass_downsample,
	pass_downsampletile,
	pass_blur,
	pass_waterblend,
	pass_watermask,
	pass_hizsetup,
	pass_upscaleshadowmask,
};

enum
{
	pass_updatewet,
	pass_addfoam,
	pass_foamintensity,
	pass_copywet,
#if RSG_ORBIS
	pass_combinefoamandwetmap,
#endif
};

static sDBVars	ms_DBVars[2];

static __forceinline sDBVars*	GetReadBuf(void)	{ return(&ms_DBVars[Water::GetWaterRenderIndex()]); }
static __forceinline sDBVars*	GetWriteBuf(void)	{ return(&ms_DBVars[Water::GetWaterUpdateIndex()]); }

//====================== Water Occlusion Queries =========================
#if RSG_ORBIS
#define WATEROCCLUSIONQUERYBUFFERING 4
#elif RSG_DURANGO
#define WATEROCCLUSIONQUERYBUFFERING 4
#else
#define WATEROCCLUSIONQUERYBUFFERING 5
#endif

#define WATEROCCLUSIONQUERY_MAX 0x10000000
u32 m_WaterPixelsRendered			= WATEROCCLUSIONQUERY_MAX;
bool m_WaterOcclusionQueriesCreated = false;

class CWaterOcclusionQuery
{
public:
	grcOcclusionQuery m_Queries[WATEROCCLUSIONQUERYBUFFERING];
	bool m_QuerySubmitted[WATEROCCLUSIONQUERYBUFFERING];
	u32 m_PixelsRenderedWrite;
	u32 m_PixelsRenderedRead;
	bool m_CollectQuery;
	u32 m_CurrentQueryIndex;

	void BeginQuery()
	{
		m_CurrentQueryIndex = (m_CurrentQueryIndex + 1) % WATEROCCLUSIONQUERYBUFFERING;
		GRCDEVICE.BeginOcclusionQuery(m_Queries[m_CurrentQueryIndex]);
		m_CollectQuery = true;
 		m_QuerySubmitted[m_CurrentQueryIndex] = true;
	}
	void EndQuery()
	{
		GRCDEVICE.EndOcclusionQuery(m_Queries[m_CurrentQueryIndex]);
	}
	void CollectQuery()
	{
		u32 queryResult		= WATEROCCLUSIONQUERY_MAX;
		bool queryAvailable = false;

		BANK_ONLY(m_QueryFrameDelay = INT_MAX);

		u32 queryIndex		= m_CurrentQueryIndex;

		for(int j = 0; j < WATEROCCLUSIONQUERYBUFFERING - 1; j++)
		{
			if(	m_Queries[queryIndex] && 
				m_CollectQuery &&
				m_QuerySubmitted[queryIndex] &&
				(GRCDEVICE.GetOcclusionQueryData(m_Queries[queryIndex], queryResult) == true))
			{
				queryAvailable = true;
				BANK_ONLY(m_QueryFrameDelay = j);
				break;
			}

			queryIndex = (queryIndex - 1 + WATEROCCLUSIONQUERYBUFFERING) % WATEROCCLUSIONQUERYBUFFERING;
		}

		if(queryAvailable)
		{
			m_PixelsRenderedWrite = queryResult;
			m_CollectQuery = false;
		}
		else
		{
			m_PixelsRenderedWrite = WATEROCCLUSIONQUERY_MAX;	
		}
	}
};

atRangeArray<CWaterOcclusionQuery, Water::query_count> m_WaterOcclusionQueries[MAX_GPUS];

void CreateWaterOcclusionQueries()
{
	for (int gpuindex = 0; gpuindex < MAX_GPUS; gpuindex++)
	{
		s32 count = m_WaterOcclusionQueries[gpuindex].GetMaxCount();
		for(s32 i = 0; i < count; i++)
		{
			CWaterOcclusionQuery& query = m_WaterOcclusionQueries[gpuindex][i];
			for(s32 j = 0; j < WATEROCCLUSIONQUERYBUFFERING; j++)
			{
				query.m_Queries[j]			= GRCDEVICE.CreateOcclusionQuery();
				query.m_QuerySubmitted[j]	= false;
			}
			query.m_PixelsRenderedWrite			= WATEROCCLUSIONQUERY_MAX;
			query.m_PixelsRenderedRead			= WATEROCCLUSIONQUERY_MAX;
			query.m_CurrentQueryIndex			= 0;
			query.m_CollectQuery				= false;
		}
	}

	m_WaterOcclusionQueriesCreated = true;
}

void DestroyWaterOcclusionQueries()
{
	for (int gpuindex = 0; gpuindex < MAX_GPUS; gpuindex++)
	{
		s32 count = m_WaterOcclusionQueries[gpuindex].GetMaxCount();
		for(s32 i = 0; i < count; i++)
		{
			CWaterOcclusionQuery& query = m_WaterOcclusionQueries[gpuindex][i];
			for(s32 j = 0; j < WATEROCCLUSIONQUERYBUFFERING; j++)
				GRCDEVICE.DestroyOcclusionQuery(query.m_Queries[j]);
		}
	}
	m_WaterOcclusionQueriesCreated = false;
}

atFixedArray<float, Water::query_oceanend - Water::query_oceanstart + 1> m_OceanQueryHeights;
#define OCEAN_HEIGHT_TOLERANCE (0.01f)

//======================================================================

#if __BANK
static bool	m_bDisplayDebugImages = false;
#endif // __BANK

using namespace Water;

static void SetShouldPlaySeaSoundsFlag()
{
	// First pass: All quads above a certain size set to true.
	s32 count = m_WaterQuadsBuffer.GetCount();
	for (s32 q = 0; q < count; q++)
	{ 
		m_WaterQuadsBuffer[q].SetPlaySounds(m_WaterQuadsBuffer[q].maxX - m_WaterQuadsBuffer[q].minX >= 150 ||
											m_WaterQuadsBuffer[q].maxY - m_WaterQuadsBuffer[q].minY >= 150);
	}

	bool bChange = true;
	while (bChange)
	{
		bChange = false;

		for (s32 q = 0; q < count; q++)
		{
			if (m_WaterQuadsBuffer[q].GetPlaySounds() == false)
			{
				s32 mainMinX = m_WaterQuadsBuffer[q].minX;
				s32 mainMaxX = m_WaterQuadsBuffer[q].maxX;
				s32 mainMinY = m_WaterQuadsBuffer[q].minY;
				s32 mainMaxY = m_WaterQuadsBuffer[q].maxY;

				for (s32 qq = 0; qq < count; qq++)
				{
					if (qq != q && m_WaterQuadsBuffer[qq].GetPlaySounds())
					{
						s32 minX = m_WaterQuadsBuffer[qq].minX;
						s32 maxX = m_WaterQuadsBuffer[qq].maxX;
						s32 minY = m_WaterQuadsBuffer[qq].minY;
						s32 maxY = m_WaterQuadsBuffer[qq].maxY;

						if (mainMaxX >= minX && mainMinX <= maxX && mainMaxY >= minY && mainMinY <= maxY)
						{
							m_WaterQuadsBuffer[q].SetPlaySounds(true);
							bChange = true;
						}
					}
				}
			}
		}
	}
}

#if __DEV
PARAM(dumpwaterquadstoepsfile, "dump water quads to eps file"); // useful for visualising water boundaries, use gsview etc.
#endif // __DEV

static int WaterQuadGetPoints_(const CWaterQuad& quad, Vec3V points[5]);

// FillQuadsAndTrianglesList
// Fills the list that tell the renderer which quads and triangles to render
static void FillQuadsAndTrianglesList()
{
	renderAssertf(WATERNUMBLOCKSX <= WATERNUMBLOCKSX_MAX, "WATERNUMBLOCKSX is too big! WATERNUMBLOCKSX_MAX needs adjustment!");
	renderAssertf(WATERNUMBLOCKSY <= WATERNUMBLOCKSY_MAX, "WATERNUMBLOCKSY is too big! WATERNUMBLOCKSY_MAX needs adjustment!");

	m_ElementsOnQuadsAndTrianglesList	= 0;
	m_WaterBlockData.Reset();

	atFixedArray<CWaterQuad, MAXNUMWATERQUADS> waterCutQuadsBuffer;
	waterCutQuadsBuffer.Reset();

	const s32 count = m_WaterQuadsBuffer.GetCount();
	s32 numQuadsRemoved = 0;

	bool bReport = false;
	(void)bReport; // don't complain compiler

#if __DEV
	if (PARAM_dumpwaterquadstoepsfile.Get() && m_WaterQuadsBuffer.GetCount() > 100)
	{
		int numPolys[3] = {0,0,0};

		for (int i = 0; i < m_WaterQuadsBuffer.GetCount(); i++)
		{
			const CWaterQuad& quad = m_WaterQuadsBuffer[i];
			Vec3V points[5];
			const int numPoints = WaterQuadGetPoints_(quad, points);

			numPolys[numPoints - 3]++;
		}

		const int numPolys3 = numPolys[0]*1 + numPolys[1]*2 + numPolys[2]*3;
		const int numPolys4 = numPolys[0]*1 + numPolys[1]*1 + numPolys[2]*2;

		Displayf("before cutting, water quads require %d verts and %d indices for %d triangles", numPolys3*3, numPolys3*(3-2)*3, numPolys3);
		Displayf("before cutting, water quads require %d verts and %d indices for %d quads"    , numPolys4*4, numPolys4*(4-2)*3, numPolys4);
		Displayf("before cutting, water quads require %d verts total", numPolys[0]*3 + numPolys[1]*4 + numPolys[2]*5);

		RenderDebugToEPS("waterquads1"); // before cutting
		bReport = true;
	}
#endif // __DEV

#if USE_WATER_REGIONS
	BuildWaterRegions(m_WaterQuadsBuffer, bReport);
#endif // USE_WATER_REGIONS

	m_OceanQueryHeights.Append() = 0.0f; // query_ocean0 is always height 0

const s32 waterNumBlocksX = WATERNUMBLOCKSX;
const s32 waterNumBlocksY = WATERNUMBLOCKSY;

	for (s32 BlockX = 0; BlockX < waterNumBlocksX; BlockX++)
	{
		for (s32 BlockY = 0; BlockY < waterNumBlocksY; BlockY++)
		{
			float blockMinHeight = 9999.0f;
			float blockMaxHeight = 0.0f;

			s16 CoorsXMin = (s16)WATERBLOCKTOWORLDX(BlockX);
			s16 CoorsYMin = (s16)WATERBLOCKTOWORLDY(BlockY);
			s16 CoorsXMax = CoorsXMin + WATERBLOCKWIDTH;
			s16 CoorsYMax = CoorsYMin + WATERBLOCKWIDTH;

			atArray<CWaterQuad> quadsToAdd;

			// Find the quads that go through this block.
			for (s32 Quad = 0; Quad < count; Quad++)
			{
				CWaterQuad waterQuad = m_WaterQuadsBuffer[Quad];
				if (waterQuad.maxX > CoorsXMin &&
					waterQuad.minX < CoorsXMax &&
					waterQuad.maxY > CoorsYMin &&
					waterQuad.minY < CoorsYMax)
				{
					//Lazy way of cutting triangles for now
					float slope		= 0.0f;
					float offset	= 0.0f;
					if(waterQuad.GetType() != 0)
					{
						float minX		= waterQuad.minX;
						float maxX		= waterQuad.maxX;
						float minY		= waterQuad.minY;
						float maxY		= waterQuad.maxY;

						float slopeDir	= (waterQuad.GetType()%2==1)?-1.0f : 1.0f;

						slope =	(maxY-minY) / (slopeDir*(maxX-minX));

						if(slope < 0.0f)
							offset = maxY - slope * minX;
						else
							offset = minY - slope * minX;

						m_CurrentTrisOrientation = (waterQuad.GetType() > 2) ? 1:0;
					}
					CWaterQuad quad = waterQuad;
					quad.minX	= Max(CoorsXMin, quad.minX);
					quad.maxX	= Min(CoorsXMax, quad.maxX);
					quad.minY	= Max(CoorsYMin, quad.minY);
					quad.maxY	= Min(CoorsYMax, quad.maxY);

					if(slope!=0&&!m_TrianglesAsQuads)//cull triangle
					{
						if(m_CurrentTrisOrientation)
							if(slope<0.0f)
							{
								if(quad.maxX*slope + offset > (float)quad.maxY)//C
									continue;
							}
							else
							{
								if(quad.maxX*slope + offset+0.001f < (float)quad.minY)//D
									continue;
							}
						else
							if(slope<0.0f)
							{
								if(quad.minX*slope + offset+0.001f  < (float)quad.minY)//A
									continue;
							}
							else
							{
								if(quad.minX*slope+offset > (float)quad.maxY)//B
									continue;
							}
					}

					quad.offset	= offset;
					quad.slope	= slope;

					quadsToAdd.PushAndGrow(quad);
				}
			}

			if (1) // convert cut quads to simple quads (type=0) where possible
			{
				for (int i = 0; i < quadsToAdd.GetCount(); i++)
				{
					CWaterQuad& quad = quadsToAdd[i];

					if (quad.GetType() != 0)
					{
						bool bIsSimpleQuad = false;

						if (quad.GetType() <= 2)
						{
							if (quad.slope < 0.0f)
							{
								bIsSimpleQuad = (quad.maxX*quad.slope + quad.offset >= (float)quad.maxY);
							}
							else
							{
								bIsSimpleQuad = (quad.maxX*quad.slope + quad.offset <= (float)quad.minY);
							}
						}
						else
						{
							if (quad.slope < 0.0f)
							{
								bIsSimpleQuad = (quad.minX*quad.slope + quad.offset <= (float)quad.minY);
							}
							else
							{
								bIsSimpleQuad = (quad.minX*quad.slope + quad.offset >= (float)quad.maxY);
							}
						}

						if (bIsSimpleQuad)
						{
							quad.SetType(0); // it's really just a simple quad
							quad.slope = 0.0f;
							quad.offset = 0.0f;
						}
					}
					else
					{
						Assert(quad.slope == 0.0f);
						Assert(quad.offset == 0.0f);
					}
				}
			}

			if (1) // optimise by combining adjacent quads
			{
				bool bDone = false;

				while (!bDone)
				{
					bDone = true;

					for (int hv = 0; hv < 2; hv++) // 0=horiz, 1=vert
					{
						for (int i = 0; i < quadsToAdd.GetCount(); i++)
						{
							for (int j = 0; j < quadsToAdd.GetCount(); j++)
							{
								CWaterQuad& quad1 = quadsToAdd[i];
								CWaterQuad& quad2 = quadsToAdd[j];

								if (i != j && quad1.GetType() == 0 && quad2.GetType() == 0)// && quad1.GetZ() == quad2.GetZ())
								{
									if (hv == 0)
									{
										if (quad1.minY == quad2.minY && quad1.maxY == quad2.maxY)
										{
											if (quad1.maxX == quad2.minX)
											{
												quad1.maxX = quad2.maxX;
												quad2.SetType(5); // deleted
												bDone = false;
												numQuadsRemoved++;
											}
										}
									}
									else // hv == 1
									{
										if (quad1.minX == quad2.minX && quad1.maxX == quad2.maxX)
										{
											if (quad1.maxY == quad2.minY)
											{
												quad1.maxY = quad2.maxY;
												quad2.SetType(5); // deleted
												bDone = false;
												numQuadsRemoved++;
											}
										}
									}
								}
							}
						}
					}
				}
			}

			if (1) // optimise by fixing quad bounds
			{
				for (int i = 0; i < quadsToAdd.GetCount(); i++)
				{
					CWaterQuad& quad = quadsToAdd[i];
					Vec3V points[5];
					const int numPoints = WaterQuadGetPoints_(quad, points);

					Vec3V pmin = points[0];
					Vec3V pmax = pmin;

					for (int j = 1; j < numPoints; j++)
					{
						pmin = Min(points[j], pmin);
						pmax = Max(points[j], pmax);
					}

					quad.minX = (s16)floorf(pmin.GetXf());
					quad.minY = (s16)floorf(pmin.GetYf());
					quad.maxX = (s16)ceilf(pmax.GetXf());
					quad.maxY = (s16)ceilf(pmax.GetYf());

					// align to grid
					quad.minX -= (quad.minX + sm_WorldBorderXMin)%DYNAMICGRIDSIZE;
					quad.minY -= (quad.minY + sm_WorldBorderYMin)%DYNAMICGRIDSIZE;
					quad.maxX += (sm_WorldBorderXMin - quad.maxX)%DYNAMICGRIDSIZE;
					quad.maxY += (sm_WorldBorderYMin - quad.maxY)%DYNAMICGRIDSIZE;
				}
			}

			s32 numQuadsInSector	= 0;
			s32 quadIndex			= waterCutQuadsBuffer.GetCount();

			for (int i = 0; i < quadsToAdd.GetCount(); i++)
			{
				const CWaterQuad& quad = quadsToAdd[i];

				if (quad.GetType() == 5) // don't add deleted quads
					continue;

				if((quad.minX == quad.maxX) || (quad.minY == quad.maxY))	// don't add degenerate quads
					continue;

				Assertf(quad.maxX > quad.minX, "Invalid cut quad #%d X values Min: %d Max: %d", i, quad.minX, quad.maxX);
				Assertf(quad.maxY > quad.minY, "Invalid cut quad #%d Y values Min: %d Max: %d", i, quad.minY, quad.maxY);
				Assertf(quad.minX%DYNAMICGRIDSIZE == 0, "Invalid cut quad #%d MinX: %d is no longer %d Grid-aligned", i, quad.minX, DYNAMICGRIDSIZE);
				Assertf(quad.minY%DYNAMICGRIDSIZE == 0, "Invalid cut quad #%d MinY: %d is no longer %d Grid-aligned", i, quad.minY, DYNAMICGRIDSIZE);
				Assertf(quad.maxX%DYNAMICGRIDSIZE == 0, "Invalid cut quad #%d MaxX: %d is no longer %d Grid-aligned", i, quad.maxX, DYNAMICGRIDSIZE);
				Assertf(quad.maxY%DYNAMICGRIDSIZE == 0, "Invalid cut quad #%d MaxY: %d is no longer %d Grid-aligned", i, quad.maxY, DYNAMICGRIDSIZE);

				if(Verifyf(!waterCutQuadsBuffer.IsFull(), "Water quads buffer is full after cutting!"))
				{
					numQuadsInSector++;
					if(blockMinHeight > quad.GetZ())
						blockMinHeight = quad.GetZ();
					if(blockMaxHeight < quad.GetZ())
						blockMaxHeight = quad.GetZ();

					waterCutQuadsBuffer.Append()	= quad;
				}
			}

			CWaterBlockData blockData;
			blockData.minHeight	= blockMinHeight;
			blockData.maxHeight	= blockMaxHeight;
			blockData.numQuads	= (s16)numQuadsInSector;
			blockData.quadIndex	= (s16)quadIndex;
			m_WaterBlockData.Append() = blockData;

			if(numQuadsInSector)
			{
				//min height is the same as max height on V in all cases, planning on axing the whole ocean tech in the future...
				bool addToHeightGroup = true;
				for(s32 i = 0; i < m_OceanQueryHeights.GetCount(); i++)
				{
					if(Abs<float>(blockMinHeight - m_OceanQueryHeights[i]) <= OCEAN_HEIGHT_TOLERANCE)
					{
						addToHeightGroup = false;
						break;
					}
				}

				if(addToHeightGroup && Verifyf(!m_OceanQueryHeights.IsFull(), "OceanQueryHeights is full, no room for height of %f", blockMinHeight))
				{
					Displayf("Adding Water Height %f", blockMinHeight);
					m_OceanQueryHeights.Append() = blockMinHeight;
				}
			}
		}
	}

	m_WaterQuadsBuffer.Reset();
	m_WaterQuadsBuffer.Reserve(waterCutQuadsBuffer.GetCount());
	m_WaterQuadsBuffer.CopyFrom(waterCutQuadsBuffer.GetElements(), (unsigned short)waterCutQuadsBuffer.GetCount());
#if __BANK
	Displayf("Total Quads are now %d (from %d), removed %d during optimisation", m_WaterQuadsBuffer.GetCount(), count, numQuadsRemoved);
#endif // __BANK

	m_nNumOfWaterQuads = m_WaterQuadsBuffer.GetCount();

#if __DEV
	if (PARAM_dumpwaterquadstoepsfile.Get() && m_WaterQuadsBuffer.GetCount() > 100)
	{
		int numPolys[3] = {0,0,0};

		for (int i = 0; i < m_WaterQuadsBuffer.GetCount(); i++)
		{
			const CWaterQuad& quad = m_WaterQuadsBuffer[i];
			Vec3V points[5];
			const int numPoints = WaterQuadGetPoints_(quad, points);

			numPolys[numPoints - 3]++;

			Vec3V qmin((float)quad.minX, (float)quad.minY, quad.GetZ());
			Vec3V qmax((float)quad.maxX, (float)quad.maxY, quad.GetZ());
			Vec3V pmin = points[0];
			Vec3V pmax = pmin;
			float emin = MaxElement(Abs(points[numPoints - 1] - points[0])).Getf();
			float emax = emin;

			for (int j = 1; j < numPoints; j++)
			{
				pmin = Min(points[j], pmin);
				pmax = Max(points[j], pmax);
				emin = Min(MaxElement(Abs(points[j] - points[j - 1])).Getf(), emin);
				emax = Max(MaxElement(Abs(points[j] - points[j - 1])).Getf(), emax);
			}

			const float border = MaxElement(Max(Abs(pmin - qmin), Abs(pmax - qmax))).Getf();

			if (border > (float)DYNAMICGRIDSIZE + 0.01f)
			{
				Assertf(0, "quad %d (type=%d) border is %f", i, quad.GetType(), border);
			}

			if (emin/emax < 0.01f)
			{
				Displayf("quad %d (type=%d) edge ratio is %f/%f = %f", i, quad.GetType(), emin, emax, emin/emax);
			}
		}

		const int numPolys3 = numPolys[0]*1 + numPolys[1]*2 + numPolys[2]*3;
		const int numPolys4 = numPolys[0]*1 + numPolys[1]*1 + numPolys[2]*2;
		const int numPolys5 = numPolys[0]*1 + numPolys[1]*1 + numPolys[2]*1;

		Displayf("after cutting, water quads require %d verts and %d indices for %d triangles", numPolys3*3, numPolys3*(3-2)*3, numPolys3);
		Displayf("after cutting, water quads require %d verts and %d indices for %d quads"    , numPolys4*4, numPolys4*(4-2)*3, numPolys4);
		Displayf("after cutting, water quads require %d verts and %d indices for %d pentagons", numPolys5*5, numPolys5*(5-2)*3, numPolys5);
		Displayf("after cutting, water quads require %d verts total", numPolys[0]*3 + numPolys[1]*4 + numPolys[2]*5);

		RenderDebugToEPS("waterquads2"); // after cutting
	}
#endif // __DEV

	waterDataLoaded = true;
}

static void LoadWaterTunings(const char *tuneFilename)
{
	if(!tuneFilename)
	{
		tuneFilename = 	"common:/data/watertune.xml";
	}

	Displayf("loading water tunings from %s", tuneFilename);
	if(!Verifyf(PARSER.InitAndLoadObject(tuneFilename, "", m_MainWaterTunings), "Water tuning file not found!"))
	{
		m_MainWaterTunings.m_WaterColor					= Color32(DEFAULT_WATERCOLOR);
		m_MainWaterTunings.m_RippleScale				= DEFAULT_RIPPLESCALE;
		m_MainWaterTunings.m_OceanFoamScale				= DEFAULT_OCEANFOAMSCALE;
		m_MainWaterTunings.m_SpecularFalloff			= DEFAULT_SPECULARFALLOFF;
		m_MainWaterTunings.m_FogPierceIntensity			= DEFAULT_FOGPIERCEINTENSITY;
		m_MainWaterTunings.m_WaterCycleDepth			= DEFAULT_WATERCYCLEDEPTH;
		m_MainWaterTunings.m_WaterCycleFade				= DEFAULT_WATERCYCLEFADE;
		m_MainWaterTunings.m_WaterLightningDepth		= DEFAULT_WATERCYCLEDEPTH;
		m_MainWaterTunings.m_WaterLightningFade			= DEFAULT_WATERCYCLEFADE;
		m_MainWaterTunings.m_DeepWaterModDepth			= DEFAULT_DEEPWATERMODDEPTH;
		m_MainWaterTunings.m_DeepWaterModFade			= DEFAULT_DEEPWATERMODFADE;
		m_MainWaterTunings.m_GodRaysLerpStart			= DEFAULT_GODRAYSLERPSTART;
		m_MainWaterTunings.m_GodRaysLerpEnd				= DEFAULT_GODRAYSLERPEND;
	}
}

void LoadWaterData(const char* fileName)
{
#if __DEV
	sysTimer timer;
#endif

	CWaterData waterData;

	if (fileName)
	{
		Displayf("loading water data from %s", fileName);

		AssertVerify(PARSER.LoadObject(fileName, "", waterData));
	}
	else
	{
		Displayf("loading NULL water data");
	}


	s32 count = waterData.m_WaterQuads.GetCount();
	m_WaterQuadsBuffer.Reserve(count);

	for(s32 i = 0; i < count; i++)
	{
		CWaterQuad quad;
		CWaterQuadParsable pQuad = waterData.m_WaterQuads[i];
		quad.minX	= pQuad.minX;
		quad.minY	= pQuad.minY;
		quad.maxX	= pQuad.maxX;
		quad.maxY	= pQuad.maxY;

		if(pQuad.a1 == 0)
		{
			u8 defaultAlpha = GetMainWaterTunings().m_WaterColor.GetAlpha();
			quad.a1 = defaultAlpha;
			quad.a2 = defaultAlpha;
			quad.a3 = defaultAlpha;
			quad.a4 = defaultAlpha;
		}
		else
		{
			quad.a1		= pQuad.a1;
			quad.a2		= pQuad.a2;
			quad.a3		= pQuad.a3;
			quad.a4		= pQuad.a4;
		}

		if(!Verifyf(quad.minX < quad.maxX, "[LoadWaterData] Water Quad #%d MinX:%d >= MaxX:%d", i, quad.minX, quad.maxX))
			continue;
		if(!Verifyf(quad.minY < quad.maxY, "[LoadWaterData] Water Quad #%d MinY:%d >= MaxY:%d", i, quad.minY, quad.maxY))
			continue;

		quad.SetZ(pQuad.GetZ());
		quad.SetType(pQuad.GetType());
		quad.SetInvisible(pQuad.m_IsInvisible);
		quad.SetHasLimitedDepth(pQuad.m_HasLimitedDepth);
		quad.SetNoStencil(pQuad.m_NoStencil);
		quad.slope = 0.0f;
		quad.offset = 0.0f;
#if __BANK
		if(!pQuad.m_IsInvisible && audNorthAudioEngine::GetGtaEnvironment()->IsProcessinWorldAudioSectors())
		{
			audNorthAudioEngine::GetGtaEnvironment()->AddWaterQuad(Vector3(quad.minX,quad.minY,pQuad.GetZ()),Vector3(quad.minX,quad.maxY,pQuad.GetZ()),Vector3(quad.maxX,quad.minY,pQuad.GetZ()),Vector3(quad.maxX,quad.maxY,pQuad.GetZ()));
		}
#endif // __BANK
		m_WaterQuadsBuffer.Append() = quad;
	}

	count = waterData.m_CalmingQuads.GetCount();
	m_CalmingQuadsBuffer.Reserve(count);

	for(s32 i = 0; i < count; i++)
	{
		CCalmingQuad quad;
		CCalmingQuadParsable pQuad = waterData.m_CalmingQuads[i];
		quad.minX = pQuad.minX;
		quad.minY = pQuad.minY;
		quad.maxX = pQuad.maxX;
		quad.maxY = pQuad.maxY;
		quad.m_fDampening = pQuad.m_fDampening;

		if(!Verifyf(quad.minX < quad.maxX, "[LoadWaterData] Calming Quad #%d MinX:%d >= MaxX:%d", i, quad.minX, quad.minY))
			continue;
		if(!Verifyf(quad.minY < quad.maxY, "[LoadWaterData] Calming Quad #%d MinY:%d >= MaxY:%d", i, quad.minY, quad.maxY))
			continue;

		if(!Verifyf(quad.m_fDampening < 1.0f, "[LoadWaterData] Calming Quad #%d x=[%d..%d],y=[%d..%d] has no dampening effect (%f)", i, quad.minX, quad.maxX, quad.minY, quad.maxY, quad.m_fDampening))
			continue;

		m_CalmingQuadsBuffer.Append() = quad;
	}

	m_nNumOfWaterCalmingQuads = m_CalmingQuadsBuffer.GetCount();


	count = waterData.m_WaveQuads.GetCount();
	m_WaveQuadsBuffer.Reserve(count);

#if __BANK
	m_nNumOfWaveQuads = count;
#endif// __BANK

	for(s32 i = 0; i < count; i++)
	{
		CWaveQuad quad;
		CWaveQuadParsable pQuad = waterData.m_WaveQuads[i];
		quad.minX = pQuad.minX;
		quad.minY = pQuad.minY;
		quad.maxX = pQuad.maxX;
		quad.maxY = pQuad.maxY;
		quad.SetAmplitude(pQuad.m_Amplitude);
		quad.SetXDirection(pQuad.m_XDirection);
		quad.SetYDirection(pQuad.m_YDirection);

		if(!Verifyf(quad.minX < quad.maxX, "[LoadWaterData] Wave Quad #%d MinX:%d >= MaxX:%d", i, quad.minX, quad.minY))
			continue;
		if(!Verifyf(quad.minY < quad.maxY, "[LoadWaterData] Wave Quad #%d MinY:%d >= MaxY:%d", i, quad.minY, quad.maxY))
			continue;

		m_WaveQuadsBuffer.Append() = quad;
	}

	SetShouldPlaySeaSoundsFlag();

	FillQuadsAndTrianglesList();

#if __DEV
	Displayf("Load water.xml took %f ms", timer.GetMsTime());
#endif //__DEV
}

static bool m_WaterDataLoaded = false;

void ResetWater()
{
	m_WaterQuadsBuffer.Reset();
	m_CalmingQuadsBuffer.Reset();
	m_WaveQuadsBuffer.Reset();

	m_nNumOfWaterQuads					= 0;
	m_nNumOfWaveQuads					= 0;
	m_nNumOfWaterCalmingQuads			= 0;
	m_ElementsOnQuadsAndTrianglesList	= 0;
	m_WaterDataLoaded					= false;

	m_WaterBlockDrawList[0].Reset();
	m_WaterBlockDrawList[1].Reset();

	m_OceanQueryHeights.Reset();
}

void Water::LoadWater(const char* path)
{
	if (Verifyf(!m_WaterDataLoaded, "Water::LoadWater called twice"))
		m_WaterDataLoaded = true;
	else
		return;

	bool bValidWaterFiles = false;

	if (path)
	{
		LoadWaterData(path);
		bValidWaterFiles = true;
	}
	else
	{
		const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetLastFile(CDataFileMgr::WATER_FILE);	// load main water.xml
		if(DATAFILEMGR.IsValid(pData))
		{
			LoadWaterData(pData->m_filename);
			bValidWaterFiles = true;
		}
	}

	if (!bValidWaterFiles)
	{
		LoadWaterData(NULL); // init water with no quads, so it won't crash later
	}
}

#if __GPUUPDATE
PARAM(gpuWater, "Force GPU Water Update");
PARAM(cpuWater, "Force CPU Water Update");
#endif // __GPUUPDATE


#if RSG_PC
bool waterInitialized = false;
#endif

void Water::CreateScreenResolutionBasedRenderTargets()
{
#if RSG_PC
	waterInitialized = true;
#endif
	grcTextureFactory::CreateParams createParams;
	createParams.UseFloat	= false;
	createParams.HasParent	= true;
	createParams.StereoRTMode = grcDevice::STEREO;

//Changing the resolution of these targets breaks the fog shadows.
//Not sure if we want them at lower resolutions so disabling for now.
#if RSG_PC
	const Settings& settings = CSettingsManager::GetInstance().GetSettings();

	m_EnableWaterLighting = settings.m_graphics.m_WaterQuality != (CSettings::eSettingsLevel) (0);
#endif

	u32 resWidth			= VideoResManager::GetSceneWidth() / 2;
	u32 resHeight			= VideoResManager::GetSceneHeight()/ 2;

	if (m_EnableWaterLighting)
	{
		m_LightingRT			= grcTextureFactory::GetInstance().CreateRenderTarget("Water Fog+Shadow",           grcrtPermanent, resWidth,   resHeight,   32, &createParams);
		m_Lighting4RT			= grcTextureFactory::GetInstance().CreateRenderTarget("Water Fog+Shadow 1/4",       grcrtPermanent, resWidth/2, resHeight/2, 32, &createParams);
		m_Lighting16TempRT		= grcTextureFactory::GetInstance().CreateRenderTarget("Water Fog+Shadow 1/16 Temp", grcrtPermanent, resWidth/4, resHeight/4, 32, &createParams);
		m_Lighting16RT			= grcTextureFactory::GetInstance().CreateRenderTarget("Water Fog+Shadow 1/16",      grcrtPermanent, resWidth/4, resHeight/4, 32, &createParams);
		m_DownSampleRT			= grcTextureFactory::GetInstance().CreateRenderTarget("Water Params",               grcrtPermanent, resWidth,   resHeight,   32, &createParams);
		createParams.Format		= grctfL8;
		m_RefractionMaskRT		= grcTextureFactory::GetInstance().CreateRenderTarget("Water Refraction Mask",      grcrtPermanent, resWidth/2, resHeight/2, 32, &createParams);

		createParams.Format		= grctfD32F;
		m_FogDepthRT			= grcTextureFactory::GetInstance().CreateRenderTarget("Water Refraction Depth",      grcrtDepthBuffer,	resWidth, resHeight, 32, &createParams);
	}
	else
	{
		m_UseFogPrepass[0] = false;
		m_UseFogPrepass[1] = false;
	}

	createParams.Format = grctfR11G11B10F;
	m_RefractionRT = grcTextureFactory::GetInstance().CreateRenderTarget("Water Refraction", grcrtPermanent, resWidth, resHeight, 32, &createParams);

#if WATER_TILED_LIGHTING
	grcTextureFactory::CreateParams classificationParams;
	classificationParams.Format = grctfA8R8G8B8;

	m_ClassificationRT = grcTextureFactory::GetInstance().CreateRenderTarget("Water Classification", grcrtPermanent, 
		CTiledLighting::GetNumTilesX(), CTiledLighting::GetNumTilesY(), 32, &classificationParams);
#endif //WATER_TILED_LIGHTING
}

void Water::DeleteScreenResolutionBasedRenderTargets()
{
	if(m_DownSampleRT) delete m_DownSampleRT; m_DownSampleRT = NULL;
	if (m_LightingRT) delete m_LightingRT; m_LightingRT = NULL;
	if (m_Lighting4RT) delete m_Lighting4RT; m_Lighting4RT = NULL;
	if (m_Lighting16TempRT) delete m_Lighting16TempRT; m_Lighting16TempRT = NULL;
	if (m_Lighting16RT) delete m_Lighting16RT; m_Lighting16RT = NULL;
	if (m_RefractionMaskRT) delete m_RefractionMaskRT; m_RefractionMaskRT = NULL;
	if (m_FogDepthRT) delete m_FogDepthRT; m_FogDepthRT = NULL;
	if (m_RefractionRT) delete m_RefractionRT; m_RefractionRT = NULL;
#if WATER_TILED_LIGHTING
	if (m_ClassificationRT) delete m_ClassificationRT; m_ClassificationRT = NULL;
#endif
}

#if RSG_PC
void Water::DeviceLost()
{
	if (waterInitialized)
	{
		DeleteScreenResolutionBasedRenderTargets();
	}
}

void Water::DeviceReset()
{
	if (waterInitialized)
	{
		CreateScreenResolutionBasedRenderTargets();
	}
}
#endif

void Water::Init(unsigned initMode)
{

	USE_MEMBUCKET(MEMBUCKET_RENDER);
	//@@: location WATER_INIT_GET_WATER_SURFACE
	if(initMode == INIT_CORE)
	{
		Vector3	ScanStart;
		Displayf("Init Water\n");

		//========================= Load Textures ===============================
		//What type of update are we running.
#if __GPUUPDATE

		m_GPUUpdate = true;

#if RSG_PC
		if (GRCDEVICE.GetGPUCount() >= 2)
		{
			m_GPUUpdate = false;
		}
#endif // RSG_PC

#if NAVMESH_EXPORT
		if(CNavMeshDataExporter::WillExportCollision())	// GPU water crashes the exporer apps
			m_GPUUpdate = false;
#endif //NAVMESH_EXPORT

#if __BANK
		m_GPUUpdateEnabled = m_GPUUpdate;
		m_CPUUpdateEnabled = !m_GPUUpdate;

		if(PARAM_gpuWater.Get())
		{
			m_GPUUpdate         = true;
			m_GPUUpdateEnabled  = true;
		}
		if(PARAM_cpuWater.Get())
		{
			m_GPUUpdate         = false;
			m_CPUUpdateEnabled  = true;
		}

#if RSG_PC
		Displayf("Water Update %d %d (%d %d)",m_GPUUpdate, GRCDEVICE.GetGPUCount(), m_GPUUpdateEnabled, m_CPUUpdateEnabled);
#endif

#elif RSG_PC
		Displayf("Water Update %d %d",m_GPUUpdate, GRCDEVICE.GetGPUCount());
#endif

#endif // __GPUUPDATE


		//========================= Load Textures ===============================
		//general noise texture
		m_noiseTexture = CShaderLib::LookupTexture("waternoise");
		Assert(m_noiseTexture);
		if(m_noiseTexture)
			m_noiseTexture->AddRef();
		//wave texture for ocean waves
		m_OceanWavesTexture = CShaderLib::LookupTexture("oceanwaves");
		Assert(m_OceanWavesTexture);
		if(m_OceanWavesTexture)
			m_OceanWavesTexture->AddRef();
		//wave texture for shore waves
		m_ShoreWavesTexture = CShaderLib::LookupTexture("shorewaves");
		Assert(m_ShoreWavesTexture);
		if(m_ShoreWavesTexture)
			m_ShoreWavesTexture->AddRef();
		//caustic texture for underwater lighting
		m_CausticTexture = CShaderLib::LookupTexture("caustic");
		Assert(m_CausticTexture);
		if(m_CausticTexture)
			m_CausticTexture->AddRef();

		//global water fog texture
		m_FogTexture_V = CShaderLib::LookupTexture("waterfog");	// main waterfog texture for V
		Assert(m_FogTexture_V);
		if(m_FogTexture_V)
			m_FogTexture_V->AddRef();

		m_CurrentFogTexture = m_FogTexture_V;

		m_FogTexture_H4 = CShaderLib::LookupTexture("waterfog_h4");	// waterfog texture for Heist Island DLC
		Assert(m_FogTexture_H4);
		if(m_FogTexture_H4)
			m_FogTexture_H4->AddRef();


		m_BumpTexture = CShaderLib::LookupTexture("waterbump");
		Assert(m_BumpTexture);
		if(m_BumpTexture)
			m_BumpTexture->AddRef();
		m_BumpTexture2 = CShaderLib::LookupTexture("waterbump2");
		Assert(m_BumpTexture2);
		if(m_BumpTexture2)
			m_BumpTexture2->AddRef();
		m_FoamTexture = CShaderLib::LookupTexture("waterfoam");
		Assert(m_FoamTexture);
		if(m_FoamTexture)
			m_FoamTexture->AddRef();
		m_CloudTexture = CShaderLib::LookupTexture("cloudnoise");
		Assert(m_CloudTexture);
		if(m_CloudTexture)
			m_CloudTexture->AddRef();
		//========================================================================

		//========================= Setup Renderstates =============================
		grcDepthStencilStateDesc depthStencilStateDesc;
		depthStencilStateDesc.DepthFunc						= rage::FixupDepthDirection(grcRSV::CMP_LESS);
		depthStencilStateDesc.StencilEnable					= true;
		depthStencilStateDesc.StencilReadMask				= WATER_STENCIL_ID | WATER_STENCIL_MASKID | DEFERRED_MATERIAL_CLEAR;
		depthStencilStateDesc.StencilWriteMask				= WATER_STENCIL_ID | DEFERRED_MATERIAL_CLEAR;
		grcDepthStencilStateDesc::grcStencilOpDesc stencilDesc;
		stencilDesc.StencilFunc								= grcRSV::CMP_NOTEQUAL;
		stencilDesc.StencilPassOp							= grcRSV::STENCILOP_REPLACE;
		depthStencilStateDesc.FrontFace						= stencilDesc;
		depthStencilStateDesc.BackFace						= stencilDesc;
		m_WaterDepthStencilState							= grcStateBlock::CreateDepthStencilState(depthStencilStateDesc);

		stencilDesc.StencilFunc								= grcRSV::CMP_ALWAYS;
		stencilDesc.StencilPassOp							= grcRSV::STENCILOP_REPLACE;
		depthStencilStateDesc.FrontFace						= stencilDesc;
		depthStencilStateDesc.BackFace						= stencilDesc;
		depthStencilStateDesc.StencilWriteMask				= 0xff;
		depthStencilStateDesc.StencilReadMask				= 0xff;
		m_WaterOccluderDepthStencilState					= grcStateBlock::CreateDepthStencilState(depthStencilStateDesc);


		depthStencilStateDesc.DepthWriteMask				= false;
		m_BoatDepthStencilState								= grcStateBlock::CreateDepthStencilState(depthStencilStateDesc);

		depthStencilStateDesc.StencilEnable					= false;
		depthStencilStateDesc.DepthFunc						= rage::FixupDepthDirection(grcRSV::CMP_LESS);
		depthStencilStateDesc.DepthWriteMask				= true;
		depthStencilStateDesc.StencilWriteMask				= 0x0;
		depthStencilStateDesc.StencilReadMask				= 0x0;
		m_WaterRefractionAlphaDepthStencilState				= grcStateBlock::CreateDepthStencilState(depthStencilStateDesc);

#if __BANK
		{
			grcDepthStencilStateDesc desc;

			desc.StencilEnable = true;
			desc.StencilWriteMask = 0x07;
			desc.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESS);
			desc.FrontFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;
			desc.BackFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;

			m_WaterTiledScreenCaptureDepthStencilState = grcStateBlock::CreateDepthStencilState(desc);
		}
#endif // __BANK

		grcBlendStateDesc addFoamBlendStateDesc;
		addFoamBlendStateDesc.BlendRTDesc[0].BlendEnable	= 1;
		addFoamBlendStateDesc.BlendRTDesc[0].SrcBlend		= grcRSV::BLEND_ONE;
		addFoamBlendStateDesc.BlendRTDesc[0].DestBlend		= grcRSV::BLEND_ONE;
		addFoamBlendStateDesc.BlendRTDesc[0].SrcBlendAlpha	= grcRSV::BLEND_ONE;
		addFoamBlendStateDesc.BlendRTDesc[0].DestBlendAlpha	= grcRSV::BLEND_ONE;
		m_AddFoamBlendState									= grcStateBlock::CreateBlendState(addFoamBlendStateDesc);


		grcBlendStateDesc waterRefractionAlphaBlendStateDesc;
		waterRefractionAlphaBlendStateDesc.BlendRTDesc[0].BlendEnable	= 1;
		waterRefractionAlphaBlendStateDesc.BlendRTDesc[0].SrcBlend		= grcRSV::BLEND_SRCALPHA;
		waterRefractionAlphaBlendStateDesc.BlendRTDesc[0].DestBlend		= grcRSV::BLEND_INVSRCALPHA;
		waterRefractionAlphaBlendStateDesc.BlendRTDesc[0].SrcBlendAlpha	= grcRSV::BLEND_ONE;
		waterRefractionAlphaBlendStateDesc.BlendRTDesc[0].DestBlendAlpha	= grcRSV::BLEND_ONE;
		waterRefractionAlphaBlendStateDesc.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_RGB;
		m_WaterRefractionAlphaBlendState					= grcStateBlock::CreateBlendState(waterRefractionAlphaBlendStateDesc);

		grcRasterizerStateDesc rasterizerStateDesc;
		rasterizerStateDesc.CullMode						= grcRSV::CULL_FRONT;
		m_UnderwaterRasterizerState							= grcStateBlock::CreateRasterizerState(rasterizerStateDesc);

		grcBlendStateDesc upscaleShadowMaskStateDesc;
		upscaleShadowMaskStateDesc.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_BLUE | grcRSV::COLORWRITEENABLE_ALPHA;
		m_UpscaleShadowMaskBlendState = grcStateBlock::CreateBlendState(upscaleShadowMaskStateDesc);
		//==========================================================================


		//========================= Setup High Res Water Vertex Buffer ===============================
		grcVertexElement elements[] =
		{
			grcVertexElement(0, grcVertexElement::grcvetTexture, 0, 4, grcFvf::grcdsShort2)
		};

		m_WaterVertexDeclaration = GRCDEVICE.CreateVertexDeclaration(elements, sizeof(elements)/sizeof(grcVertexElement));

		grcFvf fvf0;
		fvf0.SetTextureChannel(0, true, grcFvf::grcdsShort2);
#if RSG_PC
		WaterVertex waterVertData[NUMVERTICES];
#else // RSG_PC &&__D3D11
		bool bReadWrite = true;
		bool bDynamic = false;
		m_WaterVertexBuffer = grcVertexBuffer::Create(NUMVERTICES, fvf0, bReadWrite, bDynamic, NULL);
#endif // RSG_PC &&__D3D11

		s32 i = 0;

		// Scoped to cause vertex buffer to be unlocked before locking index buffer

		//vertex buffer only variant
		{
#if RSG_PC
			WaterVertex* verts = waterVertData;
#else //RSG_PC
			grcVertexBuffer::LockHelper vlock(m_WaterVertexBuffer);
			WaterVertex* verts = (WaterVertex*) vlock.GetLockPtr();
#endif //RSG_PC
			for(s16 block = 0; block < BLOCKSWIDTH*BLOCKSWIDTH; block++)
			{
				s16 blockOffsetX = BLOCKQUADSWIDTH*(block%BLOCKSWIDTH);
				s16 blockOffsetY = BLOCKQUADSWIDTH*(block/BLOCKSWIDTH);

				for(s16 y = blockOffsetY; y < blockOffsetY + BLOCKQUADSWIDTH; y++)
				{
					s16 x = blockOffsetX;
					verts[i].x = x*DYNAMICGRIDSIZE - (s16)DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE/2;
					verts[i].y = y*DYNAMICGRIDSIZE - (s16)DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE/2;
					i++;
					for(; x < blockOffsetX + BLOCKVERTSWIDTH; x++)
					{
						verts[i    ].x = (x    )*DYNAMICGRIDSIZE - (s16)DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE/2;
						verts[i    ].y = (y    )*DYNAMICGRIDSIZE - (s16)DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE/2;
						verts[i + 1].x = (x    )*DYNAMICGRIDSIZE - (s16)DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE/2;
						verts[i + 1].y = (y + 1)*DYNAMICGRIDSIZE - (s16)DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE/2;
						i += 2;
					}
					verts[i].x = (x - 1)*DYNAMICGRIDSIZE - (s16)DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE/2;
					verts[i].y = (y + 1)*DYNAMICGRIDSIZE - (s16)DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE/2;
					i++;
				}
			}
#if RSG_PC
			m_WaterVertexBuffer = grcVertexBuffer::CreateWithData(NUMVERTICES, fvf0, grcsBufferCreate_NeitherReadNorWrite, verts, false );
#endif //RSG_PC
		}

/*
		{
#if RSG_PC
			WaterVertex* verts = waterVertData;
#else //RSG_PC
			grcVertexBuffer::LockHelper vlock(m_WaterVertexBuffer);
			WaterVertex* verts = (WaterVertex*) vlock.GetLockPtr();
#endif //RSG_PC
			//fill vertex buffer
			for(s16 y = 0; y < VERTSWIDTH; y++)
			{
				for(s16 x = 0; x < VERTSWIDTH; x++)
				{
					verts[i].x	= ((x  )*DYNAMICGRIDSIZE - (s16)DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE/2);
					verts[i].y	= ((y  )*DYNAMICGRIDSIZE - (s16)DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE/2);
					i++;
				}
			}

#if RSG_PC
			m_WaterVertexBuffer = grcVertexBuffer::CreateWithData(NUMVERTICES, fvf0, grcsBufferCreate_NeitherReadNorWrite, verts, false );
#endif //RSG_PC
		}

		//fill index buffer
		m_WaterIndexBuffer		= grcIndexBuffer::Create(NUMINDICES DURANGO_ONLY(, false));
		u16* indices = m_WaterIndexBuffer->LockRW();
		i = 0;
		for(s16 block = 0; block < BLOCKSWIDTH*BLOCKSWIDTH; block++)
		{
			s16 blockOffsetX = BLOCKQUADSWIDTH*(block%BLOCKSWIDTH);
			s16 blockOffsetY = BLOCKQUADSWIDTH*(block/BLOCKSWIDTH);

			for(s16 y = blockOffsetY; y < blockOffsetY + BLOCKQUADSWIDTH; y++)
			{
				s16 x = blockOffsetX;
				indices[i]		= (x  ) + (y  )*VERTSWIDTH;
				i++;
				for(; x < blockOffsetX + BLOCKVERTSWIDTH; x++)
				{
					indices[i]		= (x  ) + (y  )*VERTSWIDTH;
					indices[i+1]	= (x  ) + (y+1)*VERTSWIDTH;
					i += 2;
				}
				indices[i]		= (x-1) + (y+1)*VERTSWIDTH;
				i++;
			}
		}

		m_WaterIndexBuffer->UnlockRW();
		*/

#if !__D3D11
		m_WaterVertexBuffer->MakeReadOnly();
#endif

		//============================================================================================


		//================================= Setup low res Water =================================
		//this is used for environment map render
		grcVertexElement lowElements[] =
		{
			grcVertexElement(0, grcVertexElement::grcvetTexture, 0, 4, grcFvf::grcdsShort2),
			grcVertexElement(0, grcVertexElement::grcvetTexture, 1, 4, grcFvf::grcdsFloat)
		};
		m_WaterLowVertexDeclaration  = GRCDEVICE.CreateVertexDeclaration(lowElements, sizeof(lowElements)/sizeof(grcVertexElement));
		//=======================================================================================

		UnderwaterLowTechniqueGroup		= grmShader::FindTechniqueGroupId("alt0");
		UnderwaterHighTechniqueGroup	= grmShader::FindTechniqueGroupId("alt1");
		UnderwaterEnvLowTechniqueGroup	= grmShader::FindTechniqueGroupId("alt2");
		UnderwaterEnvHighTechniqueGroup	= grmShader::FindTechniqueGroupId("alt3");
		EnvTechniqueGroup				= grmShader::FindTechniqueGroupId("alt4");
		SinglePassTechniqueGroup		= grmShader::FindTechniqueGroupId("alt5");
		SinglePassEnvTechniqueGroup		= grmShader::FindTechniqueGroupId("alt6");
		FoamTechniqueGroup				= grmShader::FindTechniqueGroupId("alt7");
		WaterHeightTechniqueGroup		= grmShader::FindTechniqueGroupId("alt8");
		WaterDepthTechniqueGroup		= grmShader::FindTechniqueGroupId("clouddepth");

		InvalidTechniqueGroup			= grmShader::FindTechniqueGroupId("ui");

#if __USEVERTEXSTREAMRENDERTARGETS
		VertexParamsTechniqueGroup		= grmShader::FindTechniqueGroupId("imposterdeferred");
#endif //__USEVERTEXSTREAMRENDERTARGETS

		//defaults
		m_waterSurfaceTextureSimParams[0] = 1.00f;	//acc scale - adjacent transfer
		m_waterSurfaceTextureSimParams[1] = 0.18f;	//acc scale - restore to rest position 
		m_waterSurfaceTextureSimParams[2] = 0.01f;	//acc scale - wind
		m_waterSurfaceTextureSimParams[3] = 1.00f;	//vel scale - acc transfer
		m_waterSurfaceTextureSimParams[4] = 0.45f;	//pos scale - vel transfer
		m_waterSurfaceTextureSimParams[5] = 0.16f;	//disturbance scale
		m_waterSurfaceTextureSimParams[6] = 1.00f;	
		m_waterSurfaceTextureSimParams[7] = 0.75f;	

		CreateScreenResolutionBasedRenderTargets();

		//==================================================================
		
		sm_pWaterSurface = (grcRenderTarget*)grcTexture::None;

		const u32 alignment = 128;

		//======== Create Height Map and Render Targets ==========
		struct WaterBuffer
		{
			float buffer[DYNAMICGRIDELEMENTS][DYNAMICGRIDELEMENTS];
		};
		WaterBuffer* heightBuffer1	= rage_aligned_new(alignment) WaterBuffer;
		sysMemSet(heightBuffer1, 0, sizeof(WaterBuffer));
		m_WaterHeightBufferCPU[0]	= heightBuffer1->buffer;
		if(!m_GPUUpdate || m_CPUUpdateEnabled)
		{
			WaterBuffer* heightBuffer2 = rage_aligned_new(alignment) WaterBuffer;
			sysMemSet(heightBuffer2, 0, sizeof(WaterBuffer));
			m_WaterHeightBufferCPU[1]  = heightBuffer2->buffer;
		}

		WaterBuffer* velocityBuffer = rage_aligned_new(alignment) WaterBuffer;
		sysMemSet(velocityBuffer, 0, sizeof(WaterBuffer));
		m_WaterVelocityBuffer		= velocityBuffer->buffer;

		struct WaveBuffer
		{
			u8 buffer[WAVETEXTURERES][WAVETEXTURERES];
		};
		WaveBuffer* waveBuffer      = rage_aligned_new(alignment) WaveBuffer;
		m_WaveBuffer				= waveBuffer->buffer;

		m_WaterHeightBuffer         = m_WaterHeightBufferCPU[0];
		m_DynamicWater_Height		= NULL;
		m_DynamicWater_dHeight		= NULL;
		//=========================================================

		grcTextureFactory::CreateParams params;

		u16 poolID = kRTPoolIDInvalid;
		params.Multisample				= 0;
		params.HasParent				= true;
		params.Parent					= NULL;
		params.Format					= grctfR32F;
		params.PoolID					= poolID;
		params.AllocateFromPoolOnCreate	= true;


		u32 lockType  = grcsWrite DURANGO_ONLY( | grcsDiscard);
		grcTextureFactory::TextureCreateParams::Memory_t memoryType	= grcTextureFactory::TextureCreateParams::VIDEO;
		grcTextureFactory::TextureCreateParams::Type_t textureType	= grcTextureFactory::TextureCreateParams::NORMAL;
		if(m_GPUUpdate || m_GPUUpdateEnabled)
		{
			lockType |= grcsRead;
			memoryType	= grcTextureFactory::TextureCreateParams::SYSTEM;
			textureType	= grcTextureFactory::TextureCreateParams::RENDERTARGET;
		}
		
#if RSG_PC
		if (GRCDEVICE.GetGPUCount() > 1)
		{
			s_GPUWaterShaderParams      = rage_new WaterUpdateShaderParameters[GRCDEVICE.GetGPUCount()];
			s_GPUWaterBumpShaderParams  = rage_new WaterBumpShaderParameters[GRCDEVICE.GetGPUCount()];
			s_GPUFoamShaderParams       = rage_new FoamShaderParameters[GRCDEVICE.GetGPUCount()];
		}
#endif

		grcTextureFactory::TextureCreateParams TextureParams(memoryType, 
			grcTextureFactory::TextureCreateParams::LINEAR,	lockType, NULL, textureType, 
			grcTextureFactory::TextureCreateParams::MSAA_NONE);

		for(int i = 0; i < WATERHEIGHTBUFFERING; i++)
		{
			BANK_ONLY(grcTexture::SetCustomLoadName(m_WaterHeightMapNames[i]);)
			m_HeightMap[i]   = grcTextureFactory::GetInstance().Create(DYNAMICGRIDELEMENTS, DYNAMICGRIDELEMENTS, grctfR32F, NULL, 1U, &TextureParams );
			if(m_GPUUpdate || m_GPUUpdateEnabled)
				m_HeightMapRT[i] = grcTextureFactory::GetInstance().CreateRenderTarget(m_WaterHeightMapNames[i], m_HeightMap[i]->GetTexturePtr());
			else
				m_HeightMapRT[i] = NULL;

			grcTextureLock lock;
			m_HeightMap[i]->LockRect(0, 0, lock, grcsWrite);
#if GTA_REPLAY 
#if REPLAY_WATER_USES_TEXTURE_MEM_DIRECTLY
			if((i >= 2) && (i <= 4))
				CPacketCubicPatchWaterFrame::SetWaterHeightMapTextureAndLockBasePtr(i-2, lock.Base, m_HeightMap[i], m_HeightMapRT[i]);
#endif // REPLAY_WATER_USES_TEXTURE_MEM_DIRECTLY
#endif // GTA_REPLAY
			sysMemSet(lock.Base, 0, DYNAMICGRIDELEMENTS*DYNAMICGRIDELEMENTS*sizeof(float));
			m_HeightMap[i]->UnlockRect(lock);
		}
		
		for(int i = 0; i < WATERVELOCITYBUFFERING; i++)
		{
			BANK_ONLY(grcTexture::SetCustomLoadName(m_WaterVelocityMapNames[i]);)
			m_VelocityMap[i]   = grcTextureFactory::GetInstance().Create(DYNAMICGRIDELEMENTS, DYNAMICGRIDELEMENTS, grctfR32F, NULL, 1U, &TextureParams );
			if(m_GPUUpdate || m_GPUUpdateEnabled)
				m_VelocityMapRT[i] = grcTextureFactory::GetInstance().CreateRenderTarget(m_WaterVelocityMapNames[i], m_VelocityMap[i]->GetTexturePtr());
			else
				m_VelocityMap[i] = NULL;
		}

#if RSG_DURANGO || RSG_ORBIS
		m_HeightTempRT   = m_HeightMapRT[1];
		m_VelocityTempRT = m_VelocityMapRT[1];
#else
		m_HeightTempRT   = grcTextureFactory::GetInstance().CreateRenderTarget("Height Temp RT",       grcrtPermanent, DYNAMICGRIDELEMENTS, DYNAMICGRIDELEMENTS, 32, &params);
		m_VelocityTempRT = grcTextureFactory::GetInstance().CreateRenderTarget("Velocity Map Temp RT", grcrtPermanent, DYNAMICGRIDELEMENTS, DYNAMICGRIDELEMENTS, 32, &params);
#endif

#if RSG_ORBIS
		if(m_GPUUpdate || m_GPUUpdateEnabled)
		{
			m_WaterVelocityBuffer	= (float(*)[DYNAMICGRIDELEMENTS])((sce::Gnm::Texture *) m_VelocityMap[0]->GetTexturePtr())->getBaseAddress();
			m_WaterHeightBuffer		= (float(*)[DYNAMICGRIDELEMENTS])((sce::Gnm::Texture *) m_HeightMap[0]->GetTexturePtr())->getBaseAddress();
		}
#endif

		params.PoolID = kRTPoolIDInvalid;
		params.Format = grctfG32R32F;

#if __D3D11
		if (m_GPUUpdate || m_GPUUpdateEnabled)
		{
			BANK_ONLY(grcTexture::SetCustomLoadName("Water World Base Tex");)
			m_WorldBaseTex = grcTextureFactory::GetInstance().Create( 1, 1, params.Format, NULL, 1U /*numMips*/,  &TextureParams );
			m_WorldBaseRT  = grcTextureFactory::GetInstance().CreateRenderTarget("Water World Base", m_WorldBaseTex->GetTexturePtr());		
		}
		else
		{
			m_WorldBaseTex = m_WorldBaseRT = NULL;
		}
#else // __D3D11
		m_WorldBaseRT	= grcTextureFactory::GetInstance().CreateRenderTarget("Water World Base", grcrtPermanent, 1, 1, 32, &params);
#endif

		BANK_ONLY(grcTexture::SetCustomLoadName(NULL);)

#if __XENON || RSG_ORBIS//Not supported on PC yet, PC likely to use triple buffering instead
		grcTextureLock lock;
		m_WorldBaseRT->LockRect(0, 0, lock, grcsRead | grcsAllowVRAMLock); //VRAM is not actually accessed here
		m_WorldBaseRTX	= (float*)lock.Base;
		m_WorldBaseRTY	= (float*)lock.Base + 1;
#elif __D3D11
		m_WorldBaseRTXVal = 0.0f;
		m_WorldBaseRTYVal = 0.0f;
		m_WorldBaseRTX     = &m_WorldBaseRTXVal;
		m_WorldBaseRTY     = &m_WorldBaseRTYVal;
#else
		m_WorldBaseRTX  = NULL;
		m_WorldBaseRTY  = NULL;
#endif //__XENON

#if __GPUUPDATE
		//can only do this on 360 for now
		static dev_bool	useGPUWorldBase = __XENON || __D3D11 || RSG_ORBIS;
		if(useGPUWorldBase && m_GPUUpdate)
		{
			m_tWorldBaseRTX = (s32)(WATERGRIDSIZE * rage::Floorf(*m_WorldBaseRTX*WATERGRIDSIZE_INVF));
			m_tWorldBaseRTY = (s32)(WATERGRIDSIZE * rage::Floorf(*m_WorldBaseRTY*WATERGRIDSIZE_INVF));
		}
#endif	//__GPUUPDATE

		//===============================================================================================================


		m_WaterDisturbBuffer[0].Reset();
		m_WaterDisturbBuffer[1].Reset();
		m_WaterDisturbStack[0].Reset();
		m_WaterDisturbStack[1].Reset();
		m_WaterFoamBuffer[0].Reset();
		m_WaterFoamBuffer[1].Reset();

		//===================================================
#if __D3D11 // TODO Oscar - Do we need to do this?
		//		CREATE a Temporary backing store texture to copy things into
		GRCDEVICE.LockContext();
		grcTextureD3D11* waveTexture = (grcTextureD3D11*)m_ShoreWavesTexture;
#if RSG_PC && __D3D11
		if (!waveTexture->HasStagingTexture())
			waveTexture->InitializeTempStagingTexture();
		waveTexture->UpdateCPUCopy();
#endif // RSG_PC && __D3D11
		grcTextureLock lock;
		waveTexture->LockRect( 0, 0, lock, grcsRead );
		u8* pBits = (u8*)lock.Base;
		for (int col=0; col<lock.Height; col++) 
		{
			sysMemCpy( &m_WaveBuffer[col][0], pBits, sizeof(u8)*m_ShoreWavesTexture->GetWidth());
			pBits += lock.Pitch / sizeof(u8);
		}
		waveTexture->UnlockRect(lock);
#if RSG_PC && __D3D11
		if (waveTexture->HasStagingTexture())
			waveTexture->ReleaseTempStagingTexture();
#endif // RSG_PC && __D3D11

		grcTextureD3D11* noiseTexture = (grcTextureD3D11*)m_OceanWavesTexture;
#if RSG_PC && __D3D11
		if (!noiseTexture->HasStagingTexture())
			noiseTexture->InitializeTempStagingTexture();
		noiseTexture->UpdateCPUCopy();
#endif // RSG_PC && __D3D11
		noiseTexture->LockRect( 0, 0, lock, grcsRead );
		pBits = (u8*)lock.Base;
		for (int col=0; col<lock.Height; col++) 
		{
			sysMemCpy( &m_WaveNoiseBuffer[col][0], pBits, sizeof(u8)*m_OceanWavesTexture->GetWidth());
			pBits += lock.Pitch / sizeof(u8);
		}
		noiseTexture->UnlockRect(lock);
#if RSG_PC && __D3D11
		if (noiseTexture->HasStagingTexture())
			noiseTexture->ReleaseTempStagingTexture();
#endif // RSG_PC && __D3D11
		GRCDEVICE.UnlockContext();
#elif RSG_ORBIS
		grcTextureGNM* waveTexture = (grcTextureGNM*)m_ShoreWavesTexture;

		const sce::Gnm::Texture *gnmTexture = reinterpret_cast<const sce::Gnm::Texture*>(waveTexture->GetTexturePtr());

		sce::Gnm::DataFormat format = gnmTexture->getDataFormat();

		uint64_t offset;
		uint64_t size;
#if SCE_ORBIS_SDK_VERSION < (0x00930020u)
		sce::GpuAddress::computeSurfaceOffsetAndSize(
#else
		sce::GpuAddress::computeTextureSurfaceOffsetAndSize(
#endif
			&offset,&size,gnmTexture,0,0);

		sce::GpuAddress::TilingParameters tp;
		tp.initFromTexture(gnmTexture, 0, 0);

		sce::GpuAddress::detileSurface(m_WaveBuffer, ((char*)gnmTexture->getBaseAddress() + offset),&tp);

			
		grcTextureGNM* noiseTexture = (grcTextureGNM*)m_OceanWavesTexture;

		gnmTexture = reinterpret_cast<const sce::Gnm::Texture*>(noiseTexture->GetTexturePtr());

		format = gnmTexture->getDataFormat();

#if SCE_ORBIS_SDK_VERSION < (0x00930020u)
		sce::GpuAddress::computeSurfaceOffsetAndSize(
#else
		sce::GpuAddress::computeTextureSurfaceOffsetAndSize(
#endif
			&offset,&size,gnmTexture,0,0);
		tp.initFromTexture(gnmTexture, 0, 0);

		sce::GpuAddress::detileSurface(m_WaveNoiseBuffer, ((char*)gnmTexture->getBaseAddress() + offset),&tp);
#endif

		//======== Create Wet Map =========
		params.Format	= grctfG16R16;
		params.Parent	= CRenderTargets::GetBackBuffer();
		params.PoolID	= kRTPoolIDInvalid;
#if BUFFERED_WET_MAP_SOLUTION
		m_WetMapRT[0]	= grcTextureFactory::GetInstance().CreateRenderTarget("Wet Map 0", grcrtPermanent, DYNAMICGRIDELEMENTS, DYNAMICGRIDELEMENTS, 32, &params);
		m_WetMapRT[1]	= grcTextureFactory::GetInstance().CreateRenderTarget("Wet Map 1", grcrtPermanent, DYNAMICGRIDELEMENTS, DYNAMICGRIDELEMENTS, 32, &params);
		m_WetMap[0]		= m_WetMapRT[0];
		m_WetMap[1]		= m_WetMapRT[1];

#if RSG_ORBIS
		params.Format	= grctfA8R8G8B8;
		m_FoamAccumulationRT = grcTextureFactory::GetInstance().CreateRenderTarget("Foam Accumulation", grcrtPermanent, DYNAMICGRIDELEMENTS, DYNAMICGRIDELEMENTS, 32, &params);
		m_FoamAccumulation = m_FoamAccumulationRT;
#endif //RSG_ORBIS

#else
		m_WetMapRT		= grcTextureFactory::GetInstance().CreateRenderTarget("Wet Map", grcrtPermanent, DYNAMICGRIDELEMENTS, DYNAMICGRIDELEMENTS, 32, &params);

#if __XENON
		m_WetMapRTCopy	= m_WetMapRT;
		m_WetMap		= m_WetMapRT;
#elif __PS3
		m_WetMapRTCopy	= CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P512_TILED_LOCAL_COMPMODE_DISABLED,	"Wet Map Temp", grcrtPermanent, DYNAMICGRIDELEMENTS, DYNAMICGRIDELEMENTS, 32, &params, 1);
		m_WetMapRTCopy->AllocateMemoryFromPool();
		m_WetMap		= m_WetMapRT;
#else
		m_WetMapRTCopy	= grcTextureFactory::GetInstance().CreateRenderTarget("Wet Map Copy", grcrtPermanent, DYNAMICGRIDELEMENTS, DYNAMICGRIDELEMENTS, 32, &params);
		m_WetMap		= m_WetMapRT;
#endif
#endif //BUFFERED_WET_MAP_SOLUTION


		//================================



		//========= Create Bump Map Targets =========
		params.Format		= grctfG16R16;
		m_BumpHeightRT[0]	= CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Water Bump Height RT 0", grcrtPermanent, BUMPRTSIZE, BUMPRTSIZE, 32, &params);
		m_BumpHeightRT[1]	= CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Water Bump Height RT 1", grcrtPermanent, BUMPRTSIZE, BUMPRTSIZE, 32, &params);

#if __XENON
		params.Parent		= m_BumpHeightRT;
#endif //__XENON

		const s32 bumpMips	= Log2Floor((u32)BUMPRTSIZE) - 3;
		params.MipLevels	= bumpMips;
#if __PS3
		m_BumpRT			= CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P2560_TILED_LOCAL_COMPMODE_DISABLED, "Water Bump RT", grcrtPermanent, BUMPRTSIZE, BUMPRTSIZE, 16, &params, 0);
		m_BumpRT->AllocateMemoryFromPool();
		params.MipLevels	= Max(bumpMips - 1, 1);
		m_Bump4RT			= CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P2560_TILED_LOCAL_COMPMODE_DISABLED, "Water Bump 1/4 RT", grcrtPermanent, BUMPRTSIZE/2, BUMPRTSIZE/2, 16, &params, 18);
		m_Bump4RT->AllocateMemoryFromPool();
#elif __XENON
		m_BumpRT			= CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_GENERAL1, "Water Bump RT", grcrtPermanent, BUMPRTSIZE, BUMPRTSIZE, 16, &params, kMainWaterHeap);
		m_BumpRT->AllocateMemoryFromPool();
		params.MipLevels	= Max(bumpMips - 1, 1);
		m_Bump4RT			= CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_GENERAL1, "Water Bump 1/4 RT", grcrtPermanent, BUMPRTSIZE/2, BUMPRTSIZE/2, 16, &params, kMainWaterHeap);
		m_Bump4RT->AllocateMemoryFromPool();
#else
		m_BumpRT			= CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Water Bump RT", grcrtPermanent, BUMPRTSIZE, BUMPRTSIZE, 32, &params);
#endif

		params.MipLevels	= bumpMips;
#if __PS3
		params.Format		= grctfA8R8G8B8;
		m_BumpRainRT		= CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P2560_TILED_LOCAL_COMPMODE_DISABLED, "Water Bump Rain RT", grcrtPermanent, BUMPRTSIZE, BUMPRTSIZE, 32, &params, 11);
		m_BumpRainRT->AllocateMemoryFromPool();
#elif __XENON
		m_BumpRainRT		= CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_GENERAL1, "Water Bump Rain RT", grcrtPermanent, BUMPRTSIZE, BUMPRTSIZE, 32, &params, kMainWaterHeap);
		m_BumpRainRT->AllocateMemoryFromPool();
#else
		m_BumpRainRT		= CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Water Bump Rain RT", grcrtPermanent, BUMPRTSIZE, BUMPRTSIZE, 32, &params);
#endif


#if __PS3
		params.MipLevels	= 1;
		m_BumpTempRT		= CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P1024_TILED_LOCAL_COMPMODE_C32_2X1, "Water Bump Temp RT", grcrtPermanent, BUMPRTSIZE, BUMPRTSIZE, 32, &params, 1);
		m_BumpTempRT->AllocateMemoryFromPool();
		m_BumpTemp4RT		= CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P1024_TILED_LOCAL_COMPMODE_C32_2X1, "Water Bump Temp 1/4 RT", grcrtPermanent, BUMPRTSIZE/2, BUMPRTSIZE/2, 32, &params, 4);
		m_BumpTemp4RT->AllocateMemoryFromPool();
#endif //__PS3
		//============================================

		//================================ Create Smoothstep Texture ====================================
		params.Parent					= CRenderTargets::GetBackBuffer();
		params.Format					= grctfG16R16;
		params.MipLevels				= 1;
		m_BlendRT						= CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Water Blend RT", grcrtPermanent, 128, 1, 32, &params);

		//===============================================================================================

		//================================ Create Foam RT ======================================
		params.Format		= grctfL8;
#if __XENON
		m_FoamRT		= CRenderTargets::CreateRenderTarget(XENON_RTMEMPOOL_GENERAL1,	"Foam RT", grcrtPermanent, DYNAMICGRIDELEMENTS, DYNAMICGRIDELEMENTS, 32, &params, kMainWaterHeap);
		m_FoamRT->AllocateMemoryFromPool();
#elif __PS3
		m_FoamRT		= CRenderTargets::CreateRenderTarget(PSN_RTMEMPOOL_P512_TILED_LOCAL_COMPMODE_DISABLED,	"Foam RT", grcrtPermanent, DYNAMICGRIDELEMENTS, DYNAMICGRIDELEMENTS, 32, &params, 2);
		m_FoamRT->AllocateMemoryFromPool();
#else
		m_FoamRT		= CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE,	"Foam RT", grcrtPermanent, DYNAMICGRIDELEMENTS, DYNAMICGRIDELEMENTS, 32, &params, 2);
#endif
		//======================================================================================

#if __USEVERTEXSTREAMRENDERTARGETS
		//================================= Setup Vertex Stream Stuff =================================

		grcTextureFactory::CreateParams VSRTParams;
		VSRTParams.EnableCompression	= false;
		VSRTParams.Format				= grctfA8R8G8B8;

		m_WaterTessellationFogRT		= CRenderTargets::CreateRenderTarget(	RTMEMPOOL_NONE,	"Water Tessellation Fog",grcrtPermanent,
			DYNAMICGRIDELEMENTS, DYNAMICGRIDELEMENTS, 32, &VSRTParams);

		m_WaterVertexParamsRT		= CRenderTargets::CreateRenderTarget(	RTMEMPOOL_NONE,	"Water Vertex Params",	grcrtPermanent,
			DYNAMICGRIDELEMENTS, DYNAMICGRIDELEMENTS, 32, &VSRTParams);

		VSRTParams.Format			= __PS3? grctfA8R8G8B8 : grctfR32F; //PS3 MRTs need to be the same format, so use PS level coversion back to float
		m_WaterVertexHeightRT		= CRenderTargets::CreateRenderTarget(	RTMEMPOOL_NONE,	"Water Vertex Height",	grcrtPermanent,
			DYNAMICGRIDELEMENTS, DYNAMICGRIDELEMENTS, 32, &VSRTParams);
		
		grcVertexElement VSRTElements[] =
		{
			grcVertexElement(0, grcVertexElement::grcvetTexture, 0, 4, grcFvf::grcdsShort2),//Position XY
			grcVertexElement(1, grcVertexElement::grcvetTexture, 1, 4, grcFvf::grcdsFloat),//Height Z
			grcVertexElement(2, grcVertexElement::grcvetTexture, 2, 4, grcFvf::grcdsColor)//Bump XY, Opacity, WetMask
		};

		m_WaterVSRTDeclaration = GRCDEVICE.CreateVertexDeclaration(VSRTElements, sizeof(VSRTElements)/sizeof(grcVertexElement));

		grcFvf waterHeightFVF;
		waterHeightFVF.SetTextureChannel(1, true, grcFvf::grcdsFloat);
		m_WaterVertexHeightRT->LockRect(0, 0, lock, grcsRead | grcsAllowVRAMLock);
		bReadWrite = false;
		bDynamic = false;
		m_WaterHeightVertexBuffer		= grcVertexBuffer::Create(NUMVERTICES, waterHeightFVF, bReadWrite, bDynamic, lock.Base);
		m_WaterVertexHeightRT->UnlockRect(lock);

		grcFvf waterParamsFVF;
		waterParamsFVF.SetTextureChannel(2, true, grcFvf::grcdsColor);
		m_WaterVertexParamsRT->LockRect(0, 0, lock, grcsRead | grcsAllowVRAMLock);
		bReadWrite = false;
		bDynamic = false;
		m_WaterParamsVertexBuffer		= grcVertexBuffer::Create(NUMVERTICES, waterParamsFVF, bReadWrite, bDynamic, lock.Base);
		m_WaterVertexParamsRT->UnlockRect(lock);

		//=============================================================================================
#endif //__USEVERTEXSTREAMRENDERTARGETS		
		
/*
		//========= Create Caustics Targets ==========
		params.MipLevels	= 1;
#if __PS3
		params.Format		= grctfA8R8G8B8;
#else
		params.Format		= grctfL8;
#endif //__PS3
		m_CausticsRT		= CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Water Caustics RT", grcrtPermanent, 512, 512, 8, &params);
		m_CausticTexture	= m_CausticsRT;

		params.Format		= grctfA8R8G8B8;
		params.EnableCompression = false;
		m_CausticsBumpRT	= CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "Water Caustics Verts RT", grcrtPermanent, 128, 128, 32, &params);
#if __PS3
		m_CausticsBumpRT->LockRect(0, 0, lock, grcsRead | grcsAllowVRAMLock); //VRAM is not actually accessed here
		grcFvf fvf1;
		fvf1.SetTextureChannel(1, true, grcFvf::grcdsColor);
		bReadWrite = false;
		bDynamic = false;
		m_WaterCausticsVertexBuffer	= grcVertexBuffer::Create(NUMVERTICES, fvf1, bReadWrite, bDynamic, lock.Base);
#endif //__PS3
		//============================================
*/

#if __XENON
		SSAO::GetSSAORT()->AllocateMemoryFromPool();
		CRenderTargets::GetDepthBufferQuarter()->AllocateMemoryFromPool();
		grcRenderTargetXenon* fakeStencilTexture = rage_new grcRenderTargetXenon("Depth Quarter Alias");
		fakeStencilTexture->SetupAsFakeStencilTexture(CRenderTargets::GetDepthBufferQuarter());
		CRenderTargets::SetDepthBufferQuarterAsColor(fakeStencilTexture);
#endif

#if __XENON || __PS3
		CRenderTargets::GetDepthBufferQuarterLinear()->AllocateMemoryFromPool();
#endif //__XENON || __PS3

		//======================== Load and Set Water Shader Vars =============================
		ASSET.PushFolder("common:/shaders");
			m_waterShader = grmShaderFactory::GetInstance().Create();
			m_waterShader->Load("water");
			m_WaterTextureShader = grmShaderFactory::GetInstance().Create();
			m_WaterTextureShader->Load("waterTex");
		ASSET.PopFolder();

		//Setup Water Shader
		m_WaterTechnique						= m_waterShader->LookupTechnique("water");
#if __USEVERTEXSTREAMRENDERTARGETS
		m_VertexStreamTechnique					= m_waterShader->LookupTechnique("vertexstream");
#endif //__USEVERTEXSTREAMRENDERTARGETS
#if __BANK
		m_WaterDebugOverlayTechnique			= m_waterShader->LookupTechnique("debugoverlay_water_ocean");

		m_WaterDebugOverlayDepthBufferVar		= m_waterShader->LookupVar("depthBuffer");
#endif // __BANK
		m_WaterLocalParamsVar					= m_waterShader->LookupVar("OceanLocalParams0");
		QuadAlpha_ID							= m_waterShader->LookupVar("QuadAlpha");
#if __USEVERTEXSTREAMRENDERTARGETS
		m_CameraPositionVar						= m_waterShader->LookupVar("CameraPosition");
#endif //__USEVERTEXSTREAMRENDERTARGETS
		m_WaterWorldBaseVSVar					= grcEffect::LookupGlobalVar("WorldBaseVS");
		m_WaterParams0Var						= grcEffect::LookupGlobalVar("OceanParams0");
		m_WaterParams1Var						= grcEffect::LookupGlobalVar("OceanParams1");
		m_WaterParams2Var						= grcEffect::LookupGlobalVar("OceanParams2");
		m_WaterAmbientColorVar					= grcEffect::LookupGlobalVar("WaterAmbientColor");
		m_WaterDirectionalColorVar				= grcEffect::LookupGlobalVar("WaterDirectionalColor");
		m_WaterHeightTextureVar					= grcEffect::LookupGlobalVar("HeightTexture");
		m_StaticBumpTextureVar					= grcEffect::LookupGlobalVar("StaticBumpTexture");
		m_WaterBumpTextureVar					= grcEffect::LookupGlobalVar("WaterBumpTexture");
		m_WaterBumpTexture2Var					= grcEffect::LookupGlobalVar("WaterBumpTexture2");
		m_WaterLightingTextureVar				= grcEffect::LookupGlobalVar("LightingTexture");
		m_WaterReflectionMatrixVar				= grcEffect::LookupGlobalVar("ReflectionMatrix");
		m_WaterRefractionMatrixVar				= grcEffect::LookupGlobalVar("RefractionMatrix");
		m_WaterBlendTextureVar					= grcEffect::LookupGlobalVar("BlendTexture");
		PS3_ONLY(m_depthSampler					= grcEffect::LookupGlobalVar("RefractionDepthTexture");)
		m_WaterStaticFoamTextureVar				= grcEffect::LookupGlobalVar("StaticFoamTexture");
		m_WaterWetTextureVar					= grcEffect::LookupGlobalVar("WetTexture");
		m_WaterDepthTextureVar					= grcEffect::LookupGlobalVar("WaterDepthTexture");
		m_WaterParamsTextureVar					= grcEffect::LookupGlobalVar("WaterParamsTexture");
		m_WaterVehicleProjParamsVar             = grcEffect::LookupGlobalVar("gVehicleWaterProjParams");

		m_WaterFogParamsVar						= m_waterShader->LookupVar("FogParams");
		m_WaterFogTextureVar					= m_waterShader->LookupVar("FogTexture");

		//Setup Surface Shader
		m_BumpTechnique								= m_WaterTextureShader->LookupTechnique("bump");
		//m_CausticsTechnique						= m_WaterTextureShader->LookupTechnique("caustics");
		m_DownSampleTechnique						= m_WaterTextureShader->LookupTechnique("downsample");
		m_WetUpdateTechnique						= m_WaterTextureShader->LookupTechnique("wetupdate");
#if __GPUUPDATE
		m_WaterUpdateTechnique						= m_WaterTextureShader->LookupTechnique("waterupdate");
#endif //GPUUPDATE

		WaterSurfaceSimParams_ID					= m_WaterTextureShader->LookupVar("WaterBumpParams");
		m_WaterCompositeParamsVar					= m_WaterTextureShader->LookupVar("FogCompositeParams");
		m_WaterCompositeAmbientColorVar				= m_WaterTextureShader->LookupVar("FogCompositeAmbientColor");
		m_WaterCompositeDirectionalColorVar			= m_WaterTextureShader->LookupVar("FogCompositeDirectionalColor");
		m_WaterCompositeTexOffsetVar				= m_WaterTextureShader->LookupVar("FogCompositeTexOffset", false);
		m_ProjParamsVar								= m_WaterTextureShader->LookupVar("ProjParams");
		UpdateOffset_ID								= m_WaterTextureShader->LookupVar("UpdateOffset");
		UpdateParams0_ID							= m_WaterTextureShader->LookupVar("UpdateParams0");
#if __GPUUPDATE
		UpdateParams1_ID							= m_WaterTextureShader->LookupVar("UpdateParams1");
		UpdateParams2_ID							= m_WaterTextureShader->LookupVar("UpdateParams2");	
		m_VelocityTextureVar                        = m_WaterTextureShader->LookupVar("VelocityMapTexture");
#endif //__GPUUPDATE
		m_DepthTextureVar							= m_WaterTextureShader->LookupVar("DepthTexture");
		m_PointTextureVar							= m_WaterTextureShader->LookupVar("PointTexture");
		m_PointTexture2Var							= m_WaterTextureShader->LookupVar("PointTexture2");
		m_LinearTextureVar							= m_WaterTextureShader->LookupVar("LinearTexture");
		m_LinearTexture2Var							= m_WaterTextureShader->LookupVar("LinearTexture2");
		m_LinearWrapTextureVar						= m_WaterTextureShader->LookupVar("LinearWrapTexture");
		m_LinearWrapTexture2Var						= m_WaterTextureShader->LookupVar("LinearWrapTexture2");
		m_LinearWrapTexture3Var						= m_WaterTextureShader->LookupVar("LinearWrapTexture3");
		m_HeightTextureVar							= m_WaterTextureShader->LookupVar("HeightMapTexture");

		m_NoiseTextureVar							= m_WaterTextureShader->LookupVar("NoiseTexture");
		m_WaterTextureShader->SetVar(m_NoiseTextureVar, m_noiseTexture);

		m_BumpTextureVar							= m_WaterTextureShader->LookupVar("BumpTexture");
		 

		m_BumpHeightTextureVar						= m_WaterTextureShader->LookupVar("HeightTexture");

		m_FullTextureVar							= m_WaterTextureShader->LookupVar("FullTexture");

		//===================================================

	#if GTA_REPLAY
		for(int i=0; i<2; i++)
		{
			ms_DBVars[i].m_pReplayWaterHeightPlayback = NULL;
			ms_DBVars[i].m_pReplayWaterHeightResultsFromRT = NULL;
			ms_DBVars[i].m_pReplayWaterHeightResultsToRT = NULL;
		}
	#endif // GTA_REPLAY

		//======================================================================================
	}
	else if(initMode == INIT_BEFORE_MAP_LOADED)
	{
		ResetWater();
    }
	else if(initMode == INIT_AFTER_MAP_LOADED)
	{
		LoadWaterTunings();
		LoadWater();
		for(int i=0;i<MAXEXTRACALMINGQUADS;i++)
		{
			sm_ExtraCalmingQuads[i].m_fDampening = 1.0f;
		}
	}

#if __BANK
	if (PARAM_noWaterUpdate.Get())
		m_bUpdateDynamicWater = false;
#endif
}
void Water::InitDLCCommands()
{
	DLC_REGISTER_EXTERNAL(dlCmdWaterRender);
}

void Water::InitFogTextureTxdSlots()
{
	const int indexMax = WATERNUMBLOCKSX * WATERNUMBLOCKSY;

	sysMemSet(m_WaterFogTxdSlots, 0xFF, sizeof(m_WaterFogTxdSlots));	// STRLOCALINDEX_INVALID = 0xFFFFFFFF
#if RSG_PC
	sysMemSet(m_WaterFogTxdSlotsDX10, 0xFF, sizeof(m_WaterFogTxdSlotsDX10));
#endif
	
char fogName[32];

	//Cache txd slots
	if(sm_GlobalWaterXmlFile == WATERXML_V_DEFAULT)
	{	// V:
		m_CurrentFogTexture = m_FogTexture_V;
		Assert(m_CurrentFogTexture);

		for(int index=0; index < indexMax; index++)
		{
			formatf(fogName, "waterfog-%d", index);
			m_WaterFogTxdSlots[index] = g_TxdStore.FindSlot(fogName).Get();
		#if RSG_PC
			formatf(fogName, "waterfog-%d-dx", index);
			m_WaterFogTxdSlotsDX10[index] = g_TxdStore.FindSlot(fogName).Get();
		#endif // RSG_PC
		}
	}
	else if(sm_GlobalWaterXmlFile == WATERXML_ISLANDHEIST)
	{	// Island Heist DLC:
		m_CurrentFogTexture = m_FogTexture_H4;
		Assert(m_CurrentFogTexture);

		for(int index=0; index < indexMax; index++)
		{
			formatf(fogName, "waterfog_h4-%d", index);
			m_WaterFogTxdSlots[index] = g_TxdStore.FindSlot(fogName).Get();
		#if RSG_PC
			formatf(fogName, "waterfog_h4-%d-dx", index);
			m_WaterFogTxdSlotsDX10[index] = g_TxdStore.FindSlot(fogName).Get();
		#endif // RSG_PC
		}
	}

}

// name:		Shutdown
// description:	shutdown water module (delete wavy atomics/wakes)
void Water::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_CORE)
    {
	    m_waterShader = NULL;
	    m_WaterTextureShader = NULL;

	    if (m_noiseTexture) 
	    {
		    m_noiseTexture->Release();
		    m_noiseTexture=NULL;
	    }

		DestroyWaterOcclusionQueries();
    }
    else if(shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
    {
        m_WaterQuadsBuffer.Reset();
        m_CalmingQuadsBuffer.Reset();
		m_WaveQuadsBuffer.Reset();
    }

#if RSG_PC
	if (s_GPUWaterShaderParams)
	{
		delete [] s_GPUWaterShaderParams;
		s_GPUWaterShaderParams = NULL;
	}

	if (s_GPUWaterBumpShaderParams)
	{
		delete [] s_GPUWaterBumpShaderParams;
		s_GPUWaterBumpShaderParams = NULL;
	}

	if (s_GPUFoamShaderParams)
	{
		delete [] s_GPUFoamShaderParams;
		s_GPUFoamShaderParams = NULL;
	}
#endif
}


static float FindDynamicWaterHeight_RT(s32 WorldX, s32 WorldY, Vector3 *pNormal, float * Velocity)
{
#if __GPUUPDATE
	Vector3	CenterCoors = m_DynamicWater_Coords_RT[0];
#else
	Vector3	CenterCoors = m_DynamicWater_Coords_RT[GetWaterRenderIndex()];
#endif //!__GPUUPDATE

	s32	t_WorldBaseX;
	s32	t_WorldBaseY;

#if __GPUUPDATE
	//can only do this on 360 for now
	static dev_bool	useGPUWorldBase = __XENON || __D3D11 || RSG_ORBIS;
	if(useGPUWorldBase && m_GPUUpdate REPLAY_ONLY(&& CReplayMgr::IsReplayInControlOfWorld() == false))
	{
#if RSG_ORBIS
		Water::UpdateCachedWorldBaseCoords();
#endif // RSG_ORBIS
		t_WorldBaseX = m_tWorldBaseRTX;
		t_WorldBaseY = m_tWorldBaseRTY;
	}
	else
#endif	//__GPUUPDATE
	{
		t_WorldBaseX = m_WorldBaseX;
		t_WorldBaseY = m_WorldBaseY;
	}
	
	s32 DistToBorder1 = WorldX - t_WorldBaseX;
	if (DistToBorder1 <= 0) return 0.0f;
	s32 DistToBorder2 = t_WorldBaseX + DYNAMICGRIDELEMENTS * DYNAMICGRIDSIZE - WorldX;
	if (DistToBorder2 <= 0) return 0.0f;
	s32 DistToBorder3 = WorldY - t_WorldBaseY;
	if (DistToBorder3 <= 0) return 0.0f;
	s32 DistToBorder4 = t_WorldBaseY + DYNAMICGRIDELEMENTS * DYNAMICGRIDSIZE - WorldY;
	if (DistToBorder4 <= 0) return 0.0f;

	// To make the transition from dynamic water to the area around it smooth we calculate
	// a multiplier value.
	s32 DistToBorder = MIN(MIN(DistToBorder1, DistToBorder2), MIN(DistToBorder3, DistToBorder4));
	float Multiplier = (DistToBorder < DYN_WATER_FADEDIST) ? float(DistToBorder+1) * DYN_WATER_FADEDIST_INVF : 1.0f;

	s32	GridX = FindGridXFromWorldX(WorldX, t_WorldBaseX);
	s32	GridY = FindGridYFromWorldY(WorldY, t_WorldBaseY);

	if (pNormal)
	{	
		s32 X0=(GridX+DYNAMICGRIDELEMENTS-1)&(DYNAMICGRIDELEMENTS-1);
		s32 X1=(GridX+DYNAMICGRIDELEMENTS+1)&(DYNAMICGRIDELEMENTS-1);

		s32 Y0=(GridY+DYNAMICGRIDELEMENTS-1)&(DYNAMICGRIDELEMENTS-1);
		s32 Y1=(GridY+DYNAMICGRIDELEMENTS+1)&(DYNAMICGRIDELEMENTS-1);

		float deltax;
		float deltay;

		deltax = m_WaterHeightBuffer[GridY][X0] - m_WaterHeightBuffer[GridY][X1];
		deltay = m_WaterHeightBuffer[Y0][GridX] - m_WaterHeightBuffer[Y1][GridX];

		pNormal->x	+= deltax*Multiplier;
		pNormal->y	+= deltay*Multiplier;
	}

#if __XENON
	if(Velocity)
	{
		u32 offset = XGAddress2DTiledOffset(GridX, GridY, DYNAMICGRIDELEMENTS, 4);
		Assert(offset < DYNAMICGRIDELEMENTS*DYNAMICGRIDELEMENTS);
		*Velocity = (*m_WaterVelocityBuffer)[offset]*Multiplier;
	}
#else
	if(Velocity)
		*Velocity = m_WaterVelocityBuffer[GridY][GridX]*Multiplier;
#endif //__XENON

#if GTA_REPLAY	
	//when playing back in replay look at the provided buffers to get the correct height
	if(CReplayMgr::IsReplayInControlOfWorld() && ms_DBVars[GetWaterRenderIndex()].m_pReplayWaterHeightPlayback)
	{
		return ms_DBVars[GetWaterRenderIndex()].m_pReplayWaterHeightPlayback[(GridY * DYNAMICGRIDELEMENTS) + GridX] * Multiplier;
	}
#endif

	return m_WaterHeightBuffer[GridY][GridX]*Multiplier;
}

// Calculates everything having to with the waves at a specific coordinate
static inline void CalculateWavesOnlyForCoordinate(s32 X, s32 Y,float *pResultHeight, float *pResultSpeedUp)
{
	*pResultHeight += FindDynamicWaterHeight_RT(X, Y, NULL, pResultSpeedUp);
}

// Given the coordinates of a point on the surface of the sea this function works
//			  out the wavyness and adds it to the water height.
void Water::AddWaveToResult(float x, float y, float *pWaterZ, Vector3 *pNormal, float *pSpeedUp, bool bHighDetail)
{
	float	Val1 = 0.0f, Val2 = 0.0f, Val3 = 0.0f;
	float	SpeedVal1 = 0.0f, SpeedVal2 = 0.0f, SpeedVal3 = 0.0f;

	s32	BaseX = (s32)(WATERGRIDSIZE * rage::Floorf(x*WATERGRIDSIZE_INVF));
	s32	BaseY = (s32)(WATERGRIDSIZE * rage::Floorf(y*WATERGRIDSIZE_INVF));

	TUNE_BOOL(ADD_WAVE_TO_RESULT_HD, false);
	TUNE_BOOL(DISABLE_HIGH_DETAIL_WATER, false);
	if((bHighDetail || ADD_WAVE_TO_RESULT_HD) && !DISABLE_HIGH_DETAIL_WATER)
	{
		Vec2V vTestPoint(x, y);

		// The vertex positions for the quadrilateral currently being tested (defined anticlockwise from top-left).
		Vec2V v0, v1, v2, v3;

		// When we exit this loop, v0-->v3 will contain the transformed positions of the cell vertices containing our point and BaseX
		// and BaseY will have been updated to match.
		bool bFoundQuad = false;
		int nTimes = 0;
		while(!bFoundQuad)
		{
			// In some extreme cases, the deformed quadrilaterals can fail to be convex which makes it impossible to correctly
			// identify the appropriate grid cell to compute the wave height. If we get stuck searching, break out of the loop.
			/// TODO: In these cases we should consider the individual triangles as these should remain convex.
			nTimes++;
			//Assert(nTimes < 10);
			if(nTimes > 10) break;

			float fBaseX = (float)BaseX;
			float fBaseY = (float)BaseY;

			SetWaterCellVertexPositions(fBaseX, fBaseY, v0, v1, v2, v3);

#if __BANK
			TUNE_BOOL(DEBUG_DRAW_WATER_VERTICES, false);
			if(DEBUG_DRAW_WATER_VERTICES)
			{
				float z0, z1, z2, z3;
				z0 = z1 = z2 = z3 = 0.0f;
				CalculateWavesOnlyForCoordinate(BaseX, BaseY+WATERGRIDSIZE, &z0, &SpeedVal1);
				CalculateWavesOnlyForCoordinate(BaseX, BaseY, &z1, &SpeedVal1);
				CalculateWavesOnlyForCoordinate(BaseX+WATERGRIDSIZE, BaseY, &z2, &SpeedVal1);
				CalculateWavesOnlyForCoordinate(BaseX+WATERGRIDSIZE, BaseY+WATERGRIDSIZE, &z3, &SpeedVal1);

				Vec3V V0(v0.GetXf(), v0.GetYf(), z0);
				Vec3V V1(v1.GetXf(), v1.GetYf(), z1);
				Vec3V V2(v2.GetXf(), v2.GetYf(), z2);
				Vec3V V3(v3.GetXf(), v3.GetYf(), z3);

				grcDebugDraw::Line(V0, V1, Color_green);
				grcDebugDraw::Line(V1, V2, Color_green);
			}
#endif // __BANK

			TransformWaterVertexXY(v0);
			TransformWaterVertexXY(v1);
			TransformWaterVertexXY(v2);
			TransformWaterVertexXY(v3);

#if __BANK
			if(DEBUG_DRAW_WATER_VERTICES)
			{
				float z0, z1, z2, z3;
				z0 = z1 = z2 = z3 = 0.0f;
				CalculateWavesOnlyForCoordinate(BaseX, BaseY+WATERGRIDSIZE, &z0, &SpeedVal1);
				CalculateWavesOnlyForCoordinate(BaseX, BaseY, &z1, &SpeedVal1);
				CalculateWavesOnlyForCoordinate(BaseX+WATERGRIDSIZE, BaseY, &z2, &SpeedVal2);
				CalculateWavesOnlyForCoordinate(BaseX+WATERGRIDSIZE, BaseY+WATERGRIDSIZE, &z3, &SpeedVal1);

				Vec3V V0(v0.GetXf(), v0.GetYf(), z0);
				Vec3V V1(v1.GetXf(), v1.GetYf(), z1);
				Vec3V V2(v2.GetXf(), v2.GetYf(), z2);
				Vec3V V3(v3.GetXf(), v3.GetYf(), z3);

				grcDebugDraw::Line(V0, V1, Color_yellow);
				grcDebugDraw::Line(V1, V2, Color_yellow);
			}
#endif // __BANK

			bFoundQuad = true;
			if(!IsPointInteriorToLineSegmentXY(vTestPoint, v0, v1))
			{
				bFoundQuad = false;
				BaseX -= WATERGRIDSIZE;
				continue;
			}
			if(!IsPointInteriorToLineSegmentXY(vTestPoint, v1, v2))
			{
				bFoundQuad = false;
				BaseY -= WATERGRIDSIZE;
				continue;
			}
			if(!IsPointInteriorToLineSegmentXY(vTestPoint, v2, v3))
			{
				bFoundQuad = false;
				BaseX += WATERGRIDSIZE;
				continue;
			}
			if(!IsPointInteriorToLineSegmentXY(vTestPoint, v3, v0))
			{
				bFoundQuad = false;
				BaseY += WATERGRIDSIZE;
				continue;
			}
		}

		// Interpolation is done based on and "upper" and "lower" triangle as below:
		// v0 |---| v3
		//    |\  |
		//    | \ |
		// v1 |__\| v2
		//
		// Use the normal of the edge v0->v2 to decide which triangle the test point is in.

		Vec2V vNorm02 = ComputeRightNormalToLineSegmentXY(v0, v2);
		Vec2V r = vTestPoint - v0;
		if(Dot(r, vNorm02).Getf() > 0)
		{
			// Triangle 012:
			*pWaterZ += InterpTriangle(BaseX, BaseY+WATERGRIDSIZE, BaseX, BaseY, BaseX+WATERGRIDSIZE, BaseY, v0, v1, v2, r, pNormal, pSpeedUp);
		}
		else
		{
			// Triangle 023:
			*pWaterZ += InterpTriangle(BaseX, BaseY+WATERGRIDSIZE, BaseX+WATERGRIDSIZE, BaseY, BaseX+WATERGRIDSIZE, BaseY+WATERGRIDSIZE, v0, v2, v3, r, pNormal, pSpeedUp);
		}
	}
	else
	{
		float AlongX = (x*WATERGRIDSIZE_INVF) - rage::Floorf(x*WATERGRIDSIZE_INVF);
		float AlongY = (y*WATERGRIDSIZE_INVF) - rage::Floorf(y*WATERGRIDSIZE_INVF);

		if (AlongX + AlongY < 1.0f)
		{
			CalculateWavesOnlyForCoordinate(BaseX, BaseY, &Val1, &SpeedVal1);
			CalculateWavesOnlyForCoordinate(BaseX+WATERGRIDSIZE, BaseY, &Val2, &SpeedVal2);
			CalculateWavesOnlyForCoordinate(BaseX, BaseY+WATERGRIDSIZE, &Val3, &SpeedVal3);
			*pWaterZ += Val1 + (Val2 - Val1) * AlongX + (Val3 - Val1) * AlongY;

			if(pSpeedUp)
			{
				*pSpeedUp = SpeedVal1 + (SpeedVal2 - SpeedVal1) * AlongX + (SpeedVal3 - SpeedVal1) * AlongY;
			}

			if(pNormal)
			{
				pNormal->x=(Val1-Val2)*0.5f;
				pNormal->y=(Val1-Val3)*0.5f;
				pNormal->z=1.0f;
			}
		}
		else
		{
			CalculateWavesOnlyForCoordinate(BaseX+WATERGRIDSIZE, BaseY+WATERGRIDSIZE, &Val1, &SpeedVal1);
			CalculateWavesOnlyForCoordinate(BaseX, BaseY+WATERGRIDSIZE, &Val2, &SpeedVal2);
			CalculateWavesOnlyForCoordinate(BaseX+WATERGRIDSIZE, BaseY, &Val3, &SpeedVal3);
			*pWaterZ += Val1 + (Val2 - Val1) * (1.0f - AlongX) + (Val3 - Val1) * (1.0f - AlongY);

			if(pSpeedUp)
			{
				*pSpeedUp = SpeedVal1 + (SpeedVal2 - SpeedVal1) * (1.0f - AlongX) + (SpeedVal3 - SpeedVal1) * (1.0f - AlongY);
			}

			if(pNormal)
			{
				float num=0.5f;
				pNormal->x=(Val2-Val1)*num;
				pNormal->y=(Val3-Val1)*num;
				pNormal->z=1.0f;
			}
		}
	}
}

// Helper function used in AddWaveToResult_().
void Water::SetWaterCellVertexPositions(const float fBaseX, const float fBaseY, Vec2V_InOut v0, Vec2V_InOut v1, Vec2V_InOut v2, Vec2V_InOut v3)
{
	// Taking fBaseX and fBaseY as the bottom left corner of the rectilinear grid cell currently under consideration, set v0, ... v3
	// accordingly. Labelling is anticlockwise starting with v0 as the top-left vertex.

	v0.SetXf(fBaseX);
	v0.SetYf(fBaseY+WATERGRIDSIZE);
	
	v1.SetXf(fBaseX);
	v1.SetYf(fBaseY);

	v2.SetXf(fBaseX+WATERGRIDSIZE);
	v2.SetYf(fBaseY);

	v3.SetXf(fBaseX+WATERGRIDSIZE);
	v3.SetYf(fBaseY+WATERGRIDSIZE);
}

// Helper function used in AddWaveToResult_(). Shifts a vertex in the X-Y plane based on the height differences to neighbouring grid
// points (see VSWaterCommon in water.fx).
void Water::TransformWaterVertexXY(Vec2V_InOut vVert)
{
	const int hOffsets[2] = { -1, 1 };
	float fHeights[4]; // TODO: Make this a Vec4V.

	s32	t_WorldBaseX;
	s32	t_WorldBaseY;

#if __GPUUPDATE
	//can only do this on 360 for now
	static dev_bool	useGPUWorldBase = __XENON || __D3D11 || RSG_ORBIS;
	if(useGPUWorldBase && m_GPUUpdate)
	{
#if RSG_ORBIS
		Water::UpdateCachedWorldBaseCoords();
#endif // RSG_ORBIS
		t_WorldBaseX = m_tWorldBaseRTX;
		t_WorldBaseY = m_tWorldBaseRTY;
	}
	else
#endif	//__GPUUPDATE
	{
		t_WorldBaseX = m_WorldBaseX;
		t_WorldBaseY = m_WorldBaseY;
	}

	s32 GridX = FindGridXFromWorldX((s32)vVert.GetXf(), t_WorldBaseX);
	s32 GridY = FindGridYFromWorldY((s32)vVert.GetYf(), t_WorldBaseY);

	fHeights[0] = m_WaterHeightBuffer[GridY][GridX+hOffsets[0]];
	fHeights[1] = m_WaterHeightBuffer[GridY][GridX+hOffsets[1]];
	fHeights[2] = m_WaterHeightBuffer[GridY+hOffsets[0]][GridX];
	fHeights[3] = m_WaterHeightBuffer[GridY+hOffsets[1]][GridX];

	float fHeightScale = 1.0f;
	float fHeight = m_WaterHeightBuffer[GridY][GridX] * fHeightScale;

	for(int ii = 0; ii < 4; ++ii)
	{
		fHeights[ii] *= fHeightScale;
		fHeights[ii] += fHeight;
	}
	Vec3V vBump(fHeights[0] - fHeights[1], fHeights[2] - fHeights[3], 1.0f);

	Vec3V tNorm = Normalize(vBump);

	Vec2V tDir(-2.0f*tNorm[0], -2.0f*tNorm[1]);
	Vec2V vSmall(0.00001f,0.00001f);
	tDir = -Normalize(Vec2V(tNorm.GetXY()) + vSmall) * (Sqrt(ScalarV(4.0f)*Mag(tNorm.GetXY()+vSmall)  + ScalarV(1.0f)) - ScalarV(1.0f));

	vVert += tDir;
}

// Helper function used in AddWaveToResult_().
// NB - This isn't actually a normal since we don't care about the length. It just defines the vector direction of the right normal.
Vec2V_Out Water::ComputeRightNormalToLineSegmentXY(Vec2V_ConstRef v0, Vec2V_ConstRef v1)
{
	Vec2V vNormal(v1.GetY()-v0.GetY(), v0.GetX()-v1.GetX());

	return vNormal;
}

// Helper function used in AddWaveToResult_(). Given a 2D point and two vertices this function tests whether the point is on the
// "inside" or "outside" of the line based on the right-normal. The normal is deemed to point towards the "outside".
bool Water::IsPointInteriorToLineSegmentXY(Vec2V_In vPoint, Vec2V_In v0, Vec2V_In v1)
{
	Vec2V r = vPoint - v0;

	Vec2V vNorm = ComputeRightNormalToLineSegmentXY(v0, v1);

	float d = Dot(r, vNorm).Getf();
	if(d > SMALL_FLOAT)
		return false;
	else
		return true;
}

// Helper function used in AddWaveToResult_(). Look up the heights of the vertices of the triangle (necessary to use the untransformed
// vertices to find the correct array indices into the height map) and find the height of the test point by interpolating over the plane
// defined by the transformed vertices of the triangle.
// NB - The test point is given in local space relative to vertex 0. This is the vector "r".
float Water::InterpTriangle(s32 Base0X, s32 Base0Y, s32 Base1X, s32 Base1Y, s32 Base2X, s32 Base2Y,
							Vec2V_In vTransformed0, Vec2V_In vTransformed1, Vec2V_In vTransformed2, Vec2V_In r,
							Vector3* pNormal, float* pVelZ)
{
	Vec2V vBasis0 = vTransformed1 - vTransformed0;
	Vec2V vBasis1 = vTransformed2 - vTransformed0;

	float z0, z1, z2;
	float velZ0, velZ1, velZ2;
	z0 = z1 = z2 = 0.0f;
	velZ0 = velZ1 = velZ2 = 0.0f;
	CalculateWavesOnlyForCoordinate(Base0X, Base0Y, &z0, &velZ0);
	CalculateWavesOnlyForCoordinate(Base1X, Base1Y, &z1, &velZ1);
	CalculateWavesOnlyForCoordinate(Base2X, Base2Y, &z2, &velZ2);

	// Determinant of matrix(vBasis0, vBasis1):
	float fDetBases = vBasis0.GetXf()*vBasis1.GetYf() - vBasis0.GetYf()*vBasis1.GetXf();
	fDetBases += SMALL_FLOAT;

	// Compute the coordinates of the test point in this triangle's basis set.
	float alpha = (r.GetXf()*vBasis1.GetYf() - r.GetYf()*vBasis1.GetXf()) / fDetBases;
	float beta = (vBasis0.GetXf()*r.GetYf() - vBasis0.GetYf()*r.GetXf()) / fDetBases;

	float fWaterZ = z0 + (z1 - z0)*alpha + (z2 - z0)*beta;
	// Try to correct for lag introduced from double/triple buffering of the water mesh update on GPU.
	float fVelZ = velZ0 + (velZ1 - velZ0)*alpha + (velZ2 - velZ0)*beta;
	TUNE_FLOAT(WATER_LAG_TUNE, 0.0f, 0.0f, 5.0f, 1.0f);
	TUNE_BOOL(USE_WATER_LAG_TUNE_ORBIS, false);
	float fWaterLag = WATER_LAG_TUNE;

#if RSG_ORBIS && FPS_MODE_SUPPORTED
	// Use lag tune value of 2 if in FPS mode on PS4. Helps ensure the water level values we produce are accurate.
	if (m_bLocalPlayerInFirstPersonMode && !USE_WATER_LAG_TUNE_ORBIS)
	{
		fWaterLag = 2.0f;
	}
#endif	// RSG_ORBIS && FPS_MODE_SUPPORTED

	fWaterZ += fVelZ * fwTimer::GetTimeStep()*fWaterLag;

	if(pVelZ)
	{
		*pVelZ = fVelZ;
	}

	if(pNormal)
	{
		pNormal->x = 0.5f*(z0 - z1);
		pNormal->y = 0.5f*(z0 - z2);
		pNormal->z = 1.0f;
		pNormal->Normalize();
	}

	return fWaterZ;
}

// Internal version with no normal lookup, 
// for some reason the compilers get confused between Water::AddWaveToResult(n) and static void AddWaveToResult(n-2)
static void AddWaveToResult_(const float x, const float y, float *pWaterZ, bool bHighDetail=false)
{
	float	Val1 = 0.0f, Val2 = 0.0f, Val3 = 0.0f;
	float	SpeedVal1 = 0.0f, SpeedVal2 = 0.0f, SpeedVal3 = 0.0f;

	s32	BaseX = (s32)(WATERGRIDSIZE * rage::Floorf(x*WATERGRIDSIZE_INVF));
	s32	BaseY = (s32)(WATERGRIDSIZE * rage::Floorf(y*WATERGRIDSIZE_INVF));

	TUNE_BOOL(ADD_WAVE_TO_RESULT_HD_, false);
	TUNE_BOOL(DISABLE_HIGH_DETAIL_WATER_, false);
	if((bHighDetail || ADD_WAVE_TO_RESULT_HD_) && !DISABLE_HIGH_DETAIL_WATER_)
	{
		Vec2V vTestPoint(x, y);

		// The vertex positions for the quadrilateral currently being tested (defined anticlockwise from top-left).
		Vec2V v0, v1, v2, v3;

		// When we exit this loop, v0-->v3 will contain the transformed positions of the cell vertices containing our point and BaseX
		// and BaseY will have been updated to match.
		bool bFoundQuad = false;
		int nTimes = 0;
		while(!bFoundQuad)
		{
			// In some extreme cases, the deformed quadrilaterals can fail to be convex which makes it impossible to correctly
			// identify the appropriate grid cell to compute the wave height. If we get stuck searching, break out of the loop.
			/// TODO: In these cases we should consider the individual triangles as these should remain convex.
			nTimes++;
			//Assert(nTimes < 10);
			if(nTimes > 10) break;

			float fBaseX = (float)BaseX;
			float fBaseY = (float)BaseY;

			SetWaterCellVertexPositions(fBaseX, fBaseY, v0, v1, v2, v3);

#if __BANK
			TUNE_BOOL(DEBUG_DRAW_WATER_VERTICES, false);
			if(DEBUG_DRAW_WATER_VERTICES)
			{
				float z0, z1, z2, z3;
				z0 = z1 = z2 = z3 = 0.0f;
				CalculateWavesOnlyForCoordinate(BaseX, BaseY+WATERGRIDSIZE, &z0, &SpeedVal1);
				CalculateWavesOnlyForCoordinate(BaseX, BaseY, &z1, &SpeedVal1);
				CalculateWavesOnlyForCoordinate(BaseX+WATERGRIDSIZE, BaseY, &z2, &SpeedVal1);
				CalculateWavesOnlyForCoordinate(BaseX+WATERGRIDSIZE, BaseY+WATERGRIDSIZE, &z3, &SpeedVal1);

				Vec3V V0(v0.GetXf(), v0.GetYf(), z0);
				Vec3V V1(v1.GetXf(), v1.GetYf(), z1);
				Vec3V V2(v2.GetXf(), v2.GetYf(), z2);
				Vec3V V3(v3.GetXf(), v3.GetYf(), z3);

				grcDebugDraw::Line(V0, V1, Color_green);
				grcDebugDraw::Line(V1, V2, Color_green);
			}
#endif // __BANK

			TransformWaterVertexXY(v0);
			TransformWaterVertexXY(v1);
			TransformWaterVertexXY(v2);
			TransformWaterVertexXY(v3);

#if __BANK
			if(DEBUG_DRAW_WATER_VERTICES)
			{
				float z0, z1, z2, z3;
				z0 = z1 = z2 = z3 = 0.0f;
				CalculateWavesOnlyForCoordinate(BaseX, BaseY+WATERGRIDSIZE, &z0, &SpeedVal1);
				CalculateWavesOnlyForCoordinate(BaseX, BaseY, &z1, &SpeedVal1);
				CalculateWavesOnlyForCoordinate(BaseX+WATERGRIDSIZE, BaseY, &z2, &SpeedVal2);
				CalculateWavesOnlyForCoordinate(BaseX+WATERGRIDSIZE, BaseY+WATERGRIDSIZE, &z3, &SpeedVal1);

				Vec3V V0(v0.GetXf(), v0.GetYf(), z0);
				Vec3V V1(v1.GetXf(), v1.GetYf(), z1);
				Vec3V V2(v2.GetXf(), v2.GetYf(), z2);
				Vec3V V3(v3.GetXf(), v3.GetYf(), z3);

				grcDebugDraw::Line(V0, V1, Color_yellow);
				grcDebugDraw::Line(V1, V2, Color_yellow);
			}
#endif // __BANK

			bFoundQuad = true;
			if(!IsPointInteriorToLineSegmentXY(vTestPoint, v0, v1))
			{
				bFoundQuad = false;
				BaseX -= WATERGRIDSIZE;
				continue;
			}
			if(!IsPointInteriorToLineSegmentXY(vTestPoint, v1, v2))
			{
				bFoundQuad = false;
				BaseY -= WATERGRIDSIZE;
				continue;
			}
			if(!IsPointInteriorToLineSegmentXY(vTestPoint, v2, v3))
			{
				bFoundQuad = false;
				BaseX += WATERGRIDSIZE;
				continue;
			}
			if(!IsPointInteriorToLineSegmentXY(vTestPoint, v3, v0))
			{
				bFoundQuad = false;
				BaseY += WATERGRIDSIZE;
				continue;
			}
		}

		// Interpolation is done based on and "upper" and "lower" triangle as below:
		// v0 |---| v3
		//    |\  |
		//    | \ |
		// v1 |__\| v2
		//
		// Use the normal of the edge v0->v2 to decide which triangle the test point is in.

		Vec2V vNorm02 = ComputeRightNormalToLineSegmentXY(v0, v2);
		Vec2V r = vTestPoint - v0;
		if(Dot(r, vNorm02).Getf() > 0)
		{
			// Triangle 012:
			*pWaterZ += InterpTriangle(BaseX, BaseY+WATERGRIDSIZE, BaseX, BaseY, BaseX+WATERGRIDSIZE, BaseY, v0, v1, v2, r);
		}
		else
		{
			// Triangle 023:
			*pWaterZ += InterpTriangle(BaseX, BaseY+WATERGRIDSIZE, BaseX+WATERGRIDSIZE, BaseY, BaseX+WATERGRIDSIZE, BaseY+WATERGRIDSIZE, v0, v2, v3, r);
		}
	}
	else
	{
		float AlongX = (x*WATERGRIDSIZE_INVF) - rage::Floorf(x*WATERGRIDSIZE_INVF);
		float AlongY = (y*WATERGRIDSIZE_INVF) - rage::Floorf(y*WATERGRIDSIZE_INVF);

		if (AlongX + AlongY < 1.0f)
		{
			CalculateWavesOnlyForCoordinate(BaseX, BaseY, &Val1, &SpeedVal1);
			CalculateWavesOnlyForCoordinate(BaseX+WATERGRIDSIZE, BaseY, &Val2, &SpeedVal2);
			CalculateWavesOnlyForCoordinate(BaseX, BaseY+WATERGRIDSIZE, &Val3, &SpeedVal3);
			*pWaterZ += Val1 + (Val2 - Val1) * AlongX + (Val3 - Val1) * AlongY;
		}
		else
		{
			CalculateWavesOnlyForCoordinate(BaseX+WATERGRIDSIZE, BaseY+WATERGRIDSIZE, &Val1, &SpeedVal1);
			CalculateWavesOnlyForCoordinate(BaseX, BaseY+WATERGRIDSIZE, &Val2, &SpeedVal2);
			CalculateWavesOnlyForCoordinate(BaseX+WATERGRIDSIZE, BaseY, &Val3, &SpeedVal3);
			*pWaterZ += Val1 + (Val2 - Val1) * (1.0f - AlongX) + (Val3 - Val1) * (1.0f - AlongY);
		}
	}
}

float Water::GetWaterLevel()
{
	return m_CameraFlatWaterHeight[GetWaterCurrentIndex()];
}

bool Water::IsWaterHeightDefault()
{
	return m_WaterHeightDefault[GetWaterCurrentIndex()];
}

float Water::GetAverageWaterSimHeight()
{
	return m_AverageSimHeight;
}

float Water::GetCurrentDefaultWaterHeight()
{
	return m_CurrentDefaultHeight;
}

// Works out what the level of the water is at this location.
bool Water::GetWaterLevel(const Vector3& vPos, float* pWaterZ, bool UNUSED_PARAM(bForceResult), float PoolDepth, 
						  float RejectionAboveWater, bool *pShouldPlaySeaSounds, const CPhysical* pContextEntity/*=NULL*/, bool bHighDetail)
{
	if(sm_bReloadGlobalWaterXml)
		return false;

#if __BANK
	if(m_bkReloadWaterData)
		return false;
#endif //__BANK

	if (!GetWaterLevelNoWaves(vPos, pWaterZ, PoolDepth, RejectionAboveWater, pShouldPlaySeaSounds, pContextEntity))
	{
		return false;
	}

	AddWaveToResult_((const float)vPos.x, (const float)vPos.y, (float*)pWaterZ, bHighDetail);


	return true;
}


// Looks at one particular quad and works out whether the specified point falls within it.
static bool TestQuadToGetWaterLevel(CWaterQuad *pQuad, float x, float y, float z, float *pWaterZ, float PoolDepth, float RejectionAboveWater, bool *pShouldPlaySeaSounds)
{
	float minX = pQuad->minX;
	float maxX = pQuad->maxX;
	float minY = pQuad->minY;
	float maxY = pQuad->maxY;
	if (x >= minX && x <= maxX)
	{			
		if (y >= minY && y <= maxY)
		{			
			// We're in the right rectangular area.

			float waterZ = pQuad->GetZ();
			*pWaterZ = waterZ;
			if(pQuad->GetType())//triangle test
			{
				float slope; float offset;
				pQuad->GetSlopeAndOffset(slope, offset);

				if(slope!=0 && !m_TrianglesAsQuads)//do triangle render
				{
					if(pQuad->GetType() > 2)
						if(slope<0.0f)
						{
							if( x*slope+offset > (float)y)//C
								return false;
						}
						else
						{
							if( x*slope+offset+0.001f < (float)y)//D
								return false;
						}
					else
						if(slope<0.0f)
						{
							if( x*slope+offset+0.001f  < (float)y)//A
								return false;
						}
						else
						{
							if( x*slope+offset > (float)y)//B
								return false;
						}
				}
			}
			// It is possible we are too far below a pool to care.
			if (z < waterZ - PoolDepth)
				if (pQuad->GetHasLimitedDepth()) 
					return false;

			// It is possible we are too far above water to care.
			if (z > waterZ + RejectionAboveWater)
				return false;

			if (pShouldPlaySeaSounds)
				*pShouldPlaySeaSounds = pQuad->GetPlaySounds();

			return true;
		}
	}
	return false;
}


// Works out what the level of the water is at this location. Now using
//			  the water map (with 32x32meter squares rather than the list (faster)
//			  This version doesn't bother with the wavey effect.
bool Water::GetWaterLevelNoWaves(const Vector3& vPos, float *pWaterZ, float PoolDepth, float RejectionAboveWater, bool *pShouldPlaySeaSounds, const CPhysical* pContextEntity/*=NULL*/)
{
	// if supplying an entity to check the context of, then see if there is a local water level in that entity's room.
	CPortalTracker* pPT = const_cast<CPortalTracker*>(pContextEntity ? pContextEntity->GetPortalTracker() : NULL);
	if (pContextEntity && pPT && pPT->IsInsideInterior()){
		const CPortalTracker* pPortalTracker = pContextEntity->GetPortalTracker();
		Assert(pPortalTracker);

		float localWaterLevel = 0.0f;
		CInteriorInst* pIntInst = pPT->GetInteriorInst();
		if ( pIntInst->GetLocalWaterLevel(pPortalTracker->m_roomIdx, localWaterLevel)){
			// there _is_ water in the room which this entity is in
			*pWaterZ = localWaterLevel;
			if (pShouldPlaySeaSounds)
			{
				*pShouldPlaySeaSounds = true;
			}
			return true;
		}
	}

	// Rather than going through all the water polys in the world we only go through the
	// ones in each 500x500m block.
	const s32 BlockX = GetBlockFromWorldX(vPos.x);
	const s32 BlockY = GetBlockFromWorldY(vPos.y);
	const s32 waterNumBlocksX = WATERNUMBLOCKSX;
	const s32 waterNumBlocksY = WATERNUMBLOCKSY;

	// Go through all the water polys and find the ones we're within.
	if (BlockX < 0 || BlockX >= waterNumBlocksX ||
		BlockY < 0 || BlockY >= waterNumBlocksY)
	{
		*pWaterZ = m_CurrentDefaultHeight;
		if (pShouldPlaySeaSounds)
		{
			*pShouldPlaySeaSounds = true;
		}
		return true;
	}
	
	CWaterBlockData waterBlockData	= m_WaterBlockData[WaterBlockXYToIndex(BlockX, BlockY, waterNumBlocksY)];
	s32 startIndex					= waterBlockData.quadIndex;
	s32 endIndex					= startIndex + waterBlockData.numQuads;

	for(int i = startIndex; i < endIndex; i++)
	{
		if (TestQuadToGetWaterLevel(&m_WaterQuadsBuffer[i], vPos.x, vPos.y, vPos.z, pWaterZ, PoolDepth, RejectionAboveWater, pShouldPlaySeaSounds))
			return true;
	}

	return false;
}


// Works out whether a line goes through the water surface and where.
bool TestLineAgainstWaterQuad(const Vector3& StartCoors, const Vector3& EndCoors, const Vector3& MinCoors, const Vector3& MaxCoors, 
							  Vector3 *pPenetrationPoint,
							  s16 detailMinX, s16 detailMinY, s16 detailMaxX, s16 detailMaxY, 
							  const CWaterQuad& waterQuad)
{
	float waterHeight = waterQuad.GetZ();// All water polys are horizontal. z-co of one vert is z-co of poly.

	s16 clampedMinX = Max(detailMinX, waterQuad.minX);
	s16 clampedMinY = Max(detailMinY, waterQuad.minY);
	s16 clampedMaxX = Min(detailMaxX, waterQuad.maxX);
	s16 clampedMaxY = Min(detailMaxY, waterQuad.maxY);


	float slope; float trisOffset;
	s32 currentTrisOrientation;
	if(waterQuad.GetType() > 0)
	{
		currentTrisOrientation = (waterQuad.GetType() > 2) ? 1:0;
		waterQuad.GetSlopeAndOffset(slope, trisOffset);
	}
	else
	{
		currentTrisOrientation		= 0;
		slope						= 0.0f;
		trisOffset					= 0.0f;
	}

	//compute for dynamic water height
	if(clampedMinX < clampedMaxX && clampedMinY < clampedMaxY)
	{
		static dev_s32 taps = 64;
		float inc = 1.0f/taps;
		Vector3 heightOffset = Vector3(0.0f, 0.0f, waterHeight);
		s16 offsetX = (clampedMaxX + clampedMinX)/2;
		s16 offsetY = (clampedMaxY + clampedMinY)/2;
		Vector3 offset = Vector3((float)offsetX, (float)offsetY, waterHeight);

		Vector3 centeredStart = StartCoors - offset;
		Vector3 rayVector = EndCoors - StartCoors;
		float startT;
		float endT;

		//Displayf("testing [%f, %f, %f] [%f, %f, %f] cHeight:%f", StartCoors.x, StartCoors.y, StartCoors.z, EndCoors.x, EndCoors.y, EndCoors.z, waterHeight);
		//Displayf("box range [%d %d]-[%d %d], offsetRay: [%f, %f, %f]", clampedMinX, clampedMinY, clampedMaxX, clampedMaxY,
		//	centeredStart.x, centeredStart.y, centeredStart.z);

		//clamp line segment to box
		bool hit = geomBoxes::TestSegmentToCenteredAlignedBox(centeredStart, rayVector, 
			Vector3((clampedMaxX - clampedMinX)/2.0f, (clampedMaxY - clampedMinY)/2.0f, m_WaveHeightRange),
			&startT, &endT);

		//if(hit)
		//	Displayf("hit startT: %f", startT);

		bool inBox =	abs(centeredStart.x) < DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE*1.0f &&
			abs(centeredStart.y) < DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE*1.0f &&
			abs(centeredStart.z) < m_WaveHeightRange;

		//if(inBox)
		//	Displayf("in box", inBox);

		if(hit && (inBox || (startT >= 0.0f && startT <= 1.0f)))
		{
			endT	= Clamp(endT,	0.0f, 1.0f);
			startT	= Clamp(startT,	0.0f, endT);

			Vector3 start	= StartCoors + rayVector*startT	- heightOffset;
			Vector3 end		= StartCoors + rayVector*endT	- heightOffset;

			//clamp line segment to triangle
			bool inTriangle = true;
			if(waterQuad.GetType() > 0)
			{
				Vector2 trisStart;
				Vector2 trisEnd;
				if(waterQuad.GetType() & 1)
				{
					trisStart.Set(waterQuad.minX, waterQuad.maxY);
					trisEnd.Set(waterQuad.maxX, waterQuad.minY);
				}
				else
				{
					trisStart.Set(waterQuad.minX, waterQuad.minY);
					trisEnd.Set(waterQuad.maxX, waterQuad.maxY);
				}


				float trisT1;
				float trisT2;
				bool trisHit = 	geom2D::Test2DSegVsSeg(	trisT1, trisT2, trisStart, trisEnd, 
					Vector2(start.x, start.y), Vector2(end.x, end.y), true);

				bool startInTriangle = true;
				if(currentTrisOrientation)
					if(slope<0.0f)
					{
						if( start.x*slope + trisOffset > start.y)//C
							startInTriangle = false;
					}
					else
					{
						if( start.x*slope+trisOffset+0.001f < start.y)//D
							startInTriangle = false;
					}
				else
					if(slope<0.0f)
					{
						if( start.x*slope+trisOffset+0.001f  < start.y)//A
							startInTriangle = false;
					}
					else
					{
						if( start.x*slope+trisOffset > start.y)//B
							startInTriangle = false;
					}

					if(trisHit)
					{
						if(startInTriangle)
							end = StartCoors + rayVector*trisT2 - heightOffset;
						else
							start = StartCoors + rayVector*trisT2 - heightOffset;

					//	Displayf("trisHit! %f %f", trisT1, trisT2);
					}
					else
					{
						inTriangle = startInTriangle;
					//	Displayf("sideclip");
					}
			}

			//Displayf("in box [%f, %f, %f] [%f, %f, %f] se:%f %f", start.x, start.y, start.z, end.x, end.y, end.z, startT, endT);
			s32 pPosX = -9999;
			s32 pPosY = -9999;
			float pHeight = 0;
			if(inTriangle)
				for(float i = 0; i < 1.0f; i += inc)
				{
					Vector3 pos = start*(1.0f-i) + end*i;
					s32 posX = (s32)pos.x;
					s32 posY = (s32)pos.y;
					float height;

					//use cached value if sampling the same point
					if(pPosX != posX || pPosY != posY)
					{
						height = FindDynamicWaterHeight_RT((s32)pos.x, (s32)pos.y, NULL, NULL);
						pPosX = posX;
						pPosY = posY;
						pHeight = height;
					}
					else
						height = pHeight;

					if(height > pos.z)
					{
						if(i == 0)
							break;
						*pPenetrationPoint = pos + heightOffset;
						//Displayf("hit at pos [%f %f %f] %f", pPenetrationPoint->x, pPenetrationPoint->y, pPenetrationPoint->z, i);
						return true;
					}
				}
		}
	}

	float Diff1 = StartCoors.z - waterHeight;
	float Diff2 = EndCoors.z - waterHeight;
	if (Diff1 * Diff2 < 0.0f)
	{
		Vector3 penetrationPoint = StartCoors + (rage::Abs(Diff1) / (MaxCoors.z - MinCoors.z)) * (EndCoors - StartCoors); 
		*pPenetrationPoint = penetrationPoint;
		if (penetrationPoint.x >= waterQuad.minX &&
			penetrationPoint.x <= waterQuad.maxX &&
			penetrationPoint.y >= waterQuad.minY &&
			penetrationPoint.y <= waterQuad.maxY)
		{
			///additional check for triangles
			if(waterQuad.GetType() > 0)
			{
				if(currentTrisOrientation)
					if(slope<0.0f)
					{
						if( penetrationPoint.x*slope + trisOffset > penetrationPoint.y)//C
							return false;
					}
					else
					{
						if( penetrationPoint.x*slope+trisOffset+0.001f < penetrationPoint.y)//D
							return false;
					}
				else
					if(slope<0.0f)
					{
						if( penetrationPoint.x*slope+trisOffset+0.001f  < penetrationPoint.y)//A
							return false;
					}
					else
					{
						if( penetrationPoint.x*slope+trisOffset > penetrationPoint.y)//B
							return false;
					}
			}
			return true;
		}
	}
	return false;
}

bool Water::TestLineAgainstWater(const Vector3& StartCoors, const Vector3& EndCoors, Vector3 *pPenetrationPoint)
{
	Vector3	MinCoors = Vector3(rage::Min(StartCoors.x, EndCoors.x), rage::Min(StartCoors.y, EndCoors.y), rage::Min(StartCoors.z, EndCoors.z));
	Vector3	MaxCoors = Vector3(rage::Max(StartCoors.x, EndCoors.x), rage::Max(StartCoors.y, EndCoors.y), rage::Max(StartCoors.z, EndCoors.z));

	s16 detailMinX = (s16)(GetWriteBuf()->m_CameraRangeMinX);
	s16 detailMinY = (s16)(GetWriteBuf()->m_CameraRangeMinY);
	s16 detailMaxX = (s16)(GetWriteBuf()->m_CameraRangeMaxX);
	s16 detailMaxY = (s16)(GetWriteBuf()->m_CameraRangeMaxY);

	// Rather than going through all the water polys in the world we only go through the
	// ones in each 500x500m block.
	const s32 BlockMinX = GetBlockFromWorldX(MinCoors.x);
	const s32 BlockMinY = GetBlockFromWorldY(MinCoors.y);
	const s32 BlockMaxX = GetBlockFromWorldX(MaxCoors.x);
	const s32 BlockMaxY = GetBlockFromWorldY(MaxCoors.y);
	const s32 waterNumBlocksX = WATERNUMBLOCKSX;
	const s32 waterNumBlocksY = WATERNUMBLOCKSY;

	for (s32 BlockX = BlockMinX; BlockX <= BlockMaxX; BlockX++)
	{
		for (s32 BlockY = BlockMinY; BlockY <= BlockMaxY; BlockY++)
		{
			if (BlockX < 0 || BlockX >= waterNumBlocksX ||
				BlockY < 0 || BlockY >= waterNumBlocksY)
			{
				if (MinCoors.z < m_CurrentDefaultHeight && MaxCoors.z > m_CurrentDefaultHeight)
				{
					*pPenetrationPoint = StartCoors + (rage::Abs(StartCoors.z - m_CurrentDefaultHeight) / (MaxCoors.z - MinCoors.z)) * (EndCoors - StartCoors);
				
					if (BlockX == GetBlockFromWorldX(pPenetrationPoint->x) &&
						BlockY == GetBlockFromWorldY(pPenetrationPoint->y))
					{	// We've got a winner
						return true;
					}
				}			
			}
			else
			{
				CWaterBlockData waterBlockData	= m_WaterBlockData[WaterBlockXYToIndex(BlockX, BlockY, waterNumBlocksY)];
				s32 startIndex					= waterBlockData.quadIndex;
				s32 endIndex					= startIndex + waterBlockData.numQuads;

				for(int i = startIndex; i < endIndex; i++)
				{
					CWaterQuad& waterQuad = m_WaterQuadsBuffer[i];
					if(TestLineAgainstWaterQuad(	StartCoors, EndCoors, MinCoors, MaxCoors,
													pPenetrationPoint, detailMinX, detailMinY, detailMaxX, detailMaxY, waterQuad))
						return true;
				}
			}
		}
	}
	return false;
}

// Gets called once per frame to update the water.
#if __PPU
static void SynchronizeWaterVarsSPU()
{
	// copy back required class variables from SPU update:
#if DMA_HEIGHT_BUFFER_WITH_JOB
	m_GridBaseX	 = s_pWaterBuffer0->spuWaterParams.m_OutGridBaseX;
	m_GridBaseY	 = s_pWaterBuffer0->spuWaterParams.m_OutGridBaseY;
	m_WorldBaseX = s_pWaterBuffer0->spuWaterParams.m_OutWorldBaseX;
	m_WorldBaseY = s_pWaterBuffer0->spuWaterParams.m_OutWorldBaseY;
#else
	m_GridBaseX		= s_outWaterStruct.m_GridBaseX;
	m_GridBaseY		= s_outWaterStruct.m_GridBaseY;
	m_WorldBaseX	= s_outWaterStruct.m_WorldBaseX;
	m_WorldBaseY	= s_outWaterStruct.m_WorldBaseY;
#endif
}
#endif //__PPU...


void Water::BlockTillUpdateComplete()
{
#if __GPUUPDATE
	if(m_GPUUpdate)
		return;
#endif //__GPUUPDATE
#if __PPU
	if(gbSpuUseSpu)
	{
		if(m_WaterTaskHandle)
		{
			BANK_ONLY(volatile const float cpuTimeLast = fwTimer::GetFrameTimer().GetMsTime());

			rage::sysTaskManager::Wait(m_WaterTaskHandle); //make sure processing is finished
			m_WaterTaskHandle=NULL;
			SynchronizeWaterVarsSPU();
			BANK_ONLY(m_SpuWaitTime=fwTimer::GetFrameTimer().GetMsTime() - cpuTimeLast);
		}
	}
#endif //__PPU...
#if USE_WATER_THREAD
	if(m_WaterTaskHandle)
	{
		BANK_ONLY(volatile const float cpuTimeLast = fwTimer::GetFrameTimer().GetMsTime();)

		rage::sysTaskManager::Wait(m_WaterTaskHandle); //rage doesnt free up the task unless you perform a wait, so if we didnt wait because the water wasnt rendered then we need to wait now
		m_WaterTaskHandle = NULL;
#if __BANK
		if(gbOutputTimingsCPU)
		{
			OUTPUT_ONLY(const float time= fwTimer::GetFrameTimer().GetMsTime() - cpuTimeLast;)
			Displayf("\nCPU waiting for thread processing water %.3f microsecs.",time);
		}
#endif // __BANK
	}
#endif // USE_WATER_THREAD
}


#if USE_WATER_THREAD
static void UpdateDynamicWaterTask(rage::sysTaskParameters& p)
{
	PF_AUTO_PUSH_TIMEBAR_BUDGETED("Water::UpdateDynamicWaterTask", 2.0f);

#if __BANK
	volatile const float cpuTimeLast = fwTimer::GetFrameTimer().GetMsTime();
#endif //__BANK

	CWaterUpdateDynamicParams *params = (CWaterUpdateDynamicParams*)p.Input.Data;
	UpdateDynamicWater(params);

	m_WaterHeightBuffer = m_WaterHeightBufferCPU[GetWaterUpdateIndex()];

#if __BANK
	if(gbOutputTimingsCPU)
	{
		OUTPUT_ONLY(const float time = fwTimer::GetFrameTimer().GetMsTime() - cpuTimeLast;)
		Displayf("\n---> code in task took: %.3f microsecs.",time);
	}
#endif // __BANK
}
#endif // USE_WATER_THREAD

// Calculates the points at which the water triangle have to be rendered in
//			  high detail.
static void SetCameraRange(const Vector3 &centerCoors)
{
	s32 b							= GetWaterCurrentIndex();
	ms_DBVars[b].m_CameraRelX		= ((s32)floor(centerCoors.x/WATERGRIDSIZE))*WATERGRIDSIZE;
	ms_DBVars[b].m_CameraRelY		= ((s32)floor(centerCoors.y/WATERGRIDSIZE))*WATERGRIDSIZE;
	ms_DBVars[b].m_CameraRangeMinX	= ms_DBVars[b].m_CameraRelX - DETAILEDWATERDIST;
	ms_DBVars[b].m_CameraRangeMaxX	= ms_DBVars[b].m_CameraRelX + DETAILEDWATERDIST;
	ms_DBVars[b].m_CameraRangeMinY	= ms_DBVars[b].m_CameraRelY - DETAILEDWATERDIST;
	ms_DBVars[b].m_CameraRangeMaxY	= ms_DBVars[b].m_CameraRelY + DETAILEDWATERDIST;
}

// copies double buffered vars from ReadBuf() into RenderCameraXXX vars (for quicker access):
void Water::GetCameraRangeForRender()
{
	s32 b							= GetWaterRenderIndex();
	RenderCameraRelX				= ms_DBVars[b].m_CameraRelX;
	RenderCameraRelY				= ms_DBVars[b].m_CameraRelY;
	RenderCameraRangeMinX			= ms_DBVars[b].m_CameraRangeMinX;
	RenderCameraRangeMaxX			= ms_DBVars[b].m_CameraRangeMaxX;
	RenderCameraRangeMinY			= ms_DBVars[b].m_CameraRangeMinY;
	RenderCameraRangeMaxY			= ms_DBVars[b].m_CameraRangeMaxY;
	RenderCameraBlockX				= WATERWORLDTOBLOCKX(RenderCameraRelX);
	RenderCameraBlockY				= WATERWORLDTOBLOCKY(RenderCameraRelY);
	RenderCameraBlockMinX			= WATERBLOCKTOWORLDX(RenderCameraBlockX - FOGTEXTUREBLOCKSPAN);
	RenderCameraBlockMinY			= WATERBLOCKTOWORLDY(RenderCameraBlockY - FOGTEXTUREBLOCKSPAN);
	RenderCameraBlockMaxX			= WATERBLOCKTOWORLDX(RenderCameraBlockX + FOGTEXTUREBLOCKSPAN) + WATERBLOCKWIDTH;
	RenderCameraBlockMaxY			= WATERBLOCKTOWORLDY(RenderCameraBlockY + FOGTEXTUREBLOCKSPAN) + WATERBLOCKWIDTH;

	Assertf(RenderCameraBlockMinX <= RenderCameraRangeMinX, "%d <= %d", RenderCameraBlockMinX, RenderCameraRangeMinX);
	Assertf(RenderCameraBlockMinY <= RenderCameraRangeMinY, "%d <= %d", RenderCameraBlockMinY, RenderCameraRangeMinY);
	Assertf(RenderCameraBlockMaxX >= RenderCameraRangeMaxX, "%d >= %d", RenderCameraBlockMaxX, RenderCameraRangeMaxX);
	Assertf(RenderCameraBlockMaxY >= RenderCameraRangeMaxY, "%d >= %d", RenderCameraBlockMaxY, RenderCameraRangeMaxY);
}

#if RSG_PC
void Water::DepthUpdated(bool flag)
{
	m_bDepthUpdated = flag;
}
#endif

void ClearDynamicWaterRT()
{
	sysMemSet(m_WaterHeightBuffer, 0, sizeof(float)*DYNAMICGRIDELEMENTS*DYNAMICGRIDELEMENTS);
	sysMemSet(m_WaterVelocityBuffer, 0, sizeof(float)*DYNAMICGRIDELEMENTS*DYNAMICGRIDELEMENTS);
}

void Water::FlipBuffers()
{ 
	FastAssert(ms_bufIdx < 2);

	if (fwTimer::IsGamePaused() REPLAY_ONLY(&& ShouldUpdateDuringReplay() == false))
		return;

#if __GPUUPDATE && RSG_ORBIS
	if(m_GPUUpdate)
	{
		m_WorldBaseRTX = (float*)((sce::Gnm::Texture *) m_WorldBaseRT->GetTexturePtr())->getBaseAddress();
		m_WorldBaseRTY = (float*)((sce::Gnm::Texture *) m_WorldBaseRT->GetTexturePtr())->getBaseAddress() + 1;
	}
#endif // __GPUUPDATE && RSG_ORBIS
	
	if(sm_bReloadGlobalWaterXml)
	{
		LoadGlobalWaterXmlFile(sm_RequestedGlobalWaterXml);

		sm_RequestedGlobalWaterXml	= u32(WATERXML_INVALID);
		sm_bReloadGlobalWaterXml	= false;
	}


#if __BANK//safe place to reload the water quads file
	ResetBankVars();
	m_HighDetailDrawn	= false;
	m_NumQuadsDrawn		= 0;
	m_16x16Drawn		= 0;

	if(m_bkReloadWaterData)
	{
		if(sm_GlobalWaterXmlFile == WATERXML_V_DEFAULT)			// V: water.xml
		{
			ResetWater();
			LoadWater();
		}
		else if (sm_GlobalWaterXmlFile == WATERXML_ISLANDHEIST)	// Island Heist DLC: water_HeistIsland.xml
		{
			const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::WATER_FILE);			
			while(DATAFILEMGR.IsValid(pData))
			{
				if(strstr(pData->m_filename,"water_HeistIsland.xml"))
				{
					ResetWater();
					LoadWater(pData->m_filename);
					break;
				}
				pData = DATAFILEMGR.GetNextFile(pData);
			}
		}

		m_bkReloadWaterData = false;
	}
#endif //__BANK

	GetWaterOcclusionQueryData();
	
	//I need the data from the render thread ASAP

	s32 count				= m_WaterOcclusionQueries[GPUINDEXMT].GetMaxCount();
	u32 reflectionPixels	= 0;
	u32 totalPixels			= 0;
	for(s32 i = 0; i < count; i++)
	{
		CWaterOcclusionQuery& query	= m_WaterOcclusionQueries[GPUINDEXMT][i];

		u32 pixelsRendered			= query.m_PixelsRenderedWrite == WATEROCCLUSIONQUERY_MAX ? 0 : query.m_PixelsRenderedWrite;

#if __BANK
		if (m_bForceWaterPixelsRenderedON)
			pixelsRendered = WATEROCCLUSIONQUERY_MAX;
		if (m_bForceWaterPixelsRenderedOFF)
			pixelsRendered = 0;
#endif // __BANK

		query.m_PixelsRenderedRead = pixelsRendered;

		totalPixels	+= pixelsRendered;
		if(i < query_riverenv)
			reflectionPixels += pixelsRendered;
	}

	s32 b = ms_bufIdx;

	m_WaterPixelsRendered		= totalPixels;
	m_UseHQWaterRendering[b]	= totalPixels		> m_WaterPixelCountThreshold || m_IsCameraCutting[b] || m_IsCameraDiving[b];
	m_UsePlanarReflection		= reflectionPixels	> m_WaterPixelCountThreshold || m_IsCameraCutting[b] || m_IsCameraDiving[b];
	m_UseFogPrepass[b]			= m_EnableWaterLighting && m_UseHQWaterRendering[b] && !m_IsCameraUnderWater[b];

	BANK_ONLY(m_NumPixelsRendered = totalPixels);

	b = 1 - b;
	m_BuffersFlipped = true;

	m_WaterDisturbBuffer[b].Reset();
	m_WaterDisturbStack[b].Reset();
	m_WaterFoamBuffer[b].Reset();

	if(!fwTimer::IsGamePaused() REPLAY_ONLY(|| ShouldUpdateDuringReplay()))
	{
#if RSG_PC
		if(m_GPUUpdate && m_UseMultiBuffering)
			m_WaterHeightMapIndex = (m_WaterHeightMapIndex + 1)%WATERHEIGHTBUFFERING;
#else //   RSG_PC
		m_WaterHeightMapIndex = (m_WaterHeightMapIndex + 1)%3;
#endif
			
	}

	ms_bufIdx = b;
}

__forceinline int GetFogIndexFromBlockXY(s32 blockX, s32 blockY, const s32 waterNumBlocksX, const s32 waterNumBlocksY)
{
	s32 fogY = waterNumBlocksY - blockY - 1;
	s32 fogX = blockX;
	return (fogX + fogY*waterNumBlocksX);
}

void UpdateFogTextures(s32 centerBlockX, s32 centerBlockY, s32 b)
{
	const tcKeyframeUncompressed& currKeyframe = g_timeCycle.GetCurrUpdateKeyframe();
	const float fogStreaming = currKeyframe.GetVar(TCVAR_WATER_ENABLEFOGSTREAMING);
	m_EnableTCFogStreaming = fogStreaming > 0.5f;

	s32 fb				= (m_WaterFogIndex[1-b] + 1)%3;
	m_WaterFogIndex[b]	= fb;
	b = fb;
	s32 count = m_FogTextureData[b].GetCount();
	for(s32 i = 0; i < count; i++)
	{
		if(m_FogTextureData[b][i].loaded)
			g_TxdStore.RemoveRef(m_FogTextureData[b][i].txdSlot, REF_OTHER);
	}

	m_FogTextureData[b].Reset();

	bool waterEnabled	= WaterEnabled();

	s32 *txdIdx = m_WaterFogTxdSlots;
#if RSG_PC
	if( BANK_ONLY(m_UseAlternateFogStreamingTxd || ) GRCDEVICE.GetDxFeatureLevel() < 1100 )
	{
		txdIdx = m_WaterFogTxdSlotsDX10;
	}
#endif // RSG_PC

const s32 waterNumBlocksX = WATERNUMBLOCKSX;
const s32 waterNumBlocksY = WATERNUMBLOCKSY;


	for(s32 x = centerBlockX - FOGTEXTUREBLOCKSPAN; x <= centerBlockX + FOGTEXTUREBLOCKSPAN; x++)
	{
		for(s32 y = centerBlockY - FOGTEXTUREBLOCKSPAN; y <= centerBlockY + FOGTEXTUREBLOCKSPAN; y++)
		{
			WaterFogTextureData textureData;
			textureData.txdSlot		= -1;
			textureData.loaded		= false;

			if( !fogStreamingEnabled() ||
				!waterEnabled ||
				x < 0 || x >= waterNumBlocksX ||
				y < 0 || y >= waterNumBlocksY ||
				m_WaterBlockData[WaterBlockXYToIndex(x, y, waterNumBlocksY)].numQuads == 0)
			{
				m_FogTextureData[b].Append() = textureData;
				continue;
			}

			int index = GetFogIndexFromBlockXY(x, y, waterNumBlocksX, waterNumBlocksY);
			strLocalIndex txdSlot = strLocalIndex(txdIdx[index]);

			textureData.txdSlot		= txdSlot;

			if(txdSlot.Get() >= 0)
			{
				if(g_TxdStore.HasObjectLoaded(txdSlot))
				{
					g_TxdStore.AddRef(txdSlot, REF_OTHER);
					textureData.loaded	= true;
				}
				else
					CStreaming::RequestObject(txdSlot, g_TxdStore.GetStreamingModuleId(), 0);
			}
			m_FogTextureData[b].Append() = textureData;
		}
	}
}

void ApplyWaterDisturb()
{
	atFixedArray<WaterDisturb, MAXDISTURB>* disturbBuffer	= &m_WaterDisturbBuffer[GetWaterRenderIndex()];
	s32 count												= disturbBuffer->GetCount();

	for(s32 i = 0; i < count; i++)
	{
		WaterDisturb disturb = (*disturbBuffer)[i];
		s32 worldX = disturb.m_x;
		s32 worldY = disturb.m_y;

		if(worldX < m_WorldBaseX || worldX >= m_WorldBaseX + DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE)
			continue;
		if(worldY < m_WorldBaseY || worldY >= m_WorldBaseY + DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE)
			continue;

		u32	GridX = FindGridXFromWorldX(worldX);
		u32	GridY = FindGridYFromWorldY(worldY);

		for(s32 j = 0; j < 2; j++)
		{
			float newSpeed			= disturb.m_amount[j];
			float changePercentage	= disturb.m_change[j];

			if (rage::Abs(m_WaterHeightBuffer[GridX][GridY]) < 4.0f)		// Don't so this if the water is already disturbed.
			{
				// Limit the change to stop tsunamis from happening.
				static float maxSpeedAllowed = 5.0;
#if __XENON
				u32 offset			= XGAddress2DTiledOffset(GridX, GridY, DYNAMICGRIDELEMENTS, 4);
				Assert(offset < DYNAMICGRIDELEMENTS*DYNAMICGRIDELEMENTS);
				float oldDHeight	= (*m_WaterVelocityBuffer)[offset];
#else
				float oldDHeight	= m_WaterVelocityBuffer[GridX][GridY];
#endif //__XENON
				float newDHeight	= (newSpeed * changePercentage) + (oldDHeight * (1.0f - changePercentage));

				if (newDHeight > maxSpeedAllowed)
				{
					newDHeight		= rage::Max(maxSpeedAllowed, oldDHeight);
				}
				if (newDHeight < -maxSpeedAllowed)
				{
					newDHeight		= rage::Min( (-maxSpeedAllowed), oldDHeight );
				}

				newDHeight	= rage::Max(newDHeight, -20.0f);
				newDHeight	= rage::Min(newDHeight, 20.0f);
#if __XENON
				(*m_WaterVelocityBuffer)[offset]	= newDHeight;
#else
				m_WaterVelocityBuffer[GridX][GridY]	= newDHeight;
#endif //__XENON
			}
		}
	}
}

bool gHeightSimulationActive = true;
void Water::Update()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));

	CGameWorldWaterHeight::Update();

#if __BANK

	m_DisturbCountRead=m_DisturbCount;
	m_DisturbCount=0;
	sysTimer timer;

	if(m_EnableTestBuoys)
		UpdateTestBuoys();
		
	if( sm_DebugDrawExtraCalmingQuads )
	{
		for(int i=0;i<MAXEXTRACALMINGQUADS;i++)
		{
			if( sm_ExtraCalmingQuads[i].m_fDampening < 1.0f )
			{
				Vector3 v1,v2,v3,v4;
				v1 = Vector3((float)sm_ExtraCalmingQuads[i].minX, (float)sm_ExtraCalmingQuads[i].minY, 2.0f);
				v2 = Vector3((float)sm_ExtraCalmingQuads[i].maxX, (float)sm_ExtraCalmingQuads[i].minY, 2.0f);
				v3 = Vector3((float)sm_ExtraCalmingQuads[i].maxX, (float)sm_ExtraCalmingQuads[i].maxY, 2.0f);
				v4 = Vector3((float)sm_ExtraCalmingQuads[i].minX, (float)sm_ExtraCalmingQuads[i].maxY, 2.0f);
				
				grcDebugDraw::Line(v1, v2, Color32(0xffffffff));
				grcDebugDraw::Line(v2, v3, Color32(0xffffffff));
				grcDebugDraw::Line(v3, v4, Color32(0xffffffff));
				grcDebugDraw::Line(v4, v1, Color32(0xffffffff));
			}
		}
	}
#endif //__BANK

	s32 b					= GetWaterUpdateIndex();

	m_CurrentDefaultHeight	= g_vfxSettings.GetWaterLevel();

	River::ResetRiverDrawList();

	//we need to process this while paused when playback the replay, otherwise when you place a marker	
	//with a camera the underwater camera states don't get update properly
	if (fwTimer::IsGamePaused() REPLAY_ONLY(&& ShouldUpdateDuringReplay() == false))
		return;

	River::Update();

	//Update water at camera info
	float cameraFlatWaterHeight		= 0.0f;
	float cameraWaterHeight			= 0.0f;
	float cameraWaterDepth			= 0.0f;
	bool  isDefault					= true;

	m_bLocalPlayerInFirstPersonMode = false;
	CPed* pPlayer =  CPedFactory::GetFactory()->GetLocalPlayer();
	if (pPlayer && pPlayer->IsFirstPersonShooterModeEnabledForPlayer(false))
	{
		m_bLocalPlayerInFirstPersonMode = true;
	}

	if(River::CameraOnRiver())
	{
		cameraWaterHeight		= River::GetRiverHeightAtCamera();
		cameraFlatWaterHeight	= cameraWaterHeight;
		cameraWaterDepth		= Max(0.0f, River::GetRiverHeightAtCamera() - camInterface::GetPos().GetZ());
		isDefault				= false;
	}
	else
	{
			if (GetWaterLevelNoWaves(camInterface::GetPos(), &cameraFlatWaterHeight, POOL_DEPTH, REJECTIONABOVEWATER, NULL))
			{
				cameraWaterHeight = cameraFlatWaterHeight;

				bool bUseHighDetail = false;
#if FPS_MODE_SUPPORTED
				// Use high detail water level value if player is in FPS mode.
				if (m_bLocalPlayerInFirstPersonMode)
				{
					bUseHighDetail = true;
				}
#endif	//FPS_MODE_SUPPORTED

				AddWaveToResult_((const float)camInterface::GetPos().x, (const float)camInterface::GetPos().y, &cameraWaterHeight, bUseHighDetail);
				cameraWaterDepth		= Max(0.0f, cameraWaterHeight - camInterface::GetPos().GetZ());
				isDefault				= false;
			}
		}

	m_CameraFlatWaterHeight[b]	= cameraFlatWaterHeight;
	m_CameraWaterHeight[b]		= cameraWaterHeight;
	m_WaterHeightDefault[b]		= isDefault;

	m_CameraWaterDepth[b]		= cameraWaterDepth;

	m_IsCameraUnderWater[b]	= cameraWaterDepth > 0.0f;	

	m_IsCameraDiving[b]		= m_IsCameraDiving[1 - b];
	m_IsCameraCutting[b]	= m_IsCameraCutting[1 - b];

	static dev_s32 soakFrames = WATEROCCLUSIONQUERYBUFFERING;
	if(m_IsCameraUnderWater[b] != m_IsCameraUnderWater[1-b])
		m_IsCameraDiving[b] = soakFrames;
	else
		if(m_IsCameraDiving[b] > 0)
			m_IsCameraDiving[b]--;

	if((camInterface::GetFrame().GetFlags() & (camFrame::Flag_HasCutPosition | camFrame::Flag_HasCutOrientation)) > 0)
		m_IsCameraCutting[b] = soakFrames;
	else
		if(m_IsCameraCutting[b] > 0)
			m_IsCameraCutting[b]--;

#if __BANK
	m_UpdateTime=timer.GetMsTime();
	PF_INCREMENTBY(WaterUpdateTime_Counter, m_UpdateTime);
#endif  // __BANK
}// end of Water::Update()

#if __GPUUPDATE
void UpdateAverageHeight()
{
	if(gHeightSimulationActive)
	{
		static dev_s32 sampleWidth	= 5;
		static dev_s32 sampleStride	= 2;
		s32 start	= DYNAMICGRIDELEMENTS/2 - sampleWidth*sampleStride/2;
		s32 end		= DYNAMICGRIDELEMENTS/2 + sampleWidth*sampleStride/2;
		float totalHeight = 0.0f;

		for(s32 x = start; x < end; x += sampleStride)
			for(s32 y = start; y < end; y += sampleStride)
				totalHeight += m_WaterHeightBuffer[x][y];

		dev_float blend = 0.1f;
		m_AverageSimHeight	= Lerp(blend, m_AverageSimHeight, totalHeight/(sampleWidth*sampleWidth));
	}
	else
		m_AverageSimHeight	= 0.0f;
}
#else
void UpdateAverageHeight()
{
	if(gHeightSimulationActive)
	{
		static dev_s32 sampleWidth	= 5;
		static dev_s32 sampleStride	= 2;
		s32 b		= GetWaterUpdateIndex();
		s32 startX	= ms_DBVars[b].m_CameraRelX - sampleWidth*sampleStride/2;
		s32 endX	= ms_DBVars[b].m_CameraRelX + sampleWidth*sampleStride/2;
		s32 startY	= ms_DBVars[b].m_CameraRelY - sampleWidth*sampleStride/2;
		s32 endY	= ms_DBVars[b].m_CameraRelY + sampleWidth*sampleStride/2;
		float totalHeight = 0.0f;
		for(s32 x = startX; x < endX; x += sampleStride)
		{
				u32 gridX = FindGridXFromWorldX(x);
				for(s32 y = startY; y < endY; y += sampleStride)
				{
					u32 gridY	= FindGridYFromWorldY(y);
					totalHeight += m_WaterHeightBuffer[gridX][gridY];
				}
		}
		dev_float blend = 0.1f;
		m_AverageSimHeight	= Lerp(blend, m_AverageSimHeight, totalHeight/(sampleWidth*sampleWidth));
	}
	else
		m_AverageSimHeight	= 0.0f;
}
#endif //__GPUUDPATE

void UpdateHeightSimulationActive(s32 b)
{
	const s32 waterNumBlocksX = WATERNUMBLOCKSX;
	const s32 waterNumBlocksY = WATERNUMBLOCKSY;
	const s32 BlockMinX = WATERWORLDTOBLOCKX(ms_DBVars[b].m_CameraRangeMinX);
	const s32 BlockMinY = WATERWORLDTOBLOCKY(ms_DBVars[b].m_CameraRangeMinY);
	const s32 BlockMaxX = WATERWORLDTOBLOCKX(ms_DBVars[b].m_CameraRangeMaxX);
	const s32 BlockMaxY = WATERWORLDTOBLOCKY(ms_DBVars[b].m_CameraRangeMaxY);

	if(BlockMinX < 0 || BlockMinY < 0 || BlockMaxX >= waterNumBlocksX || BlockMaxY >= waterNumBlocksY)
	{
		gHeightSimulationActive = true;
		return;
	}

	for (s32 BlockX = BlockMinX; BlockX <= BlockMaxX; BlockX++)
	{
		for (s32 BlockY = BlockMinY; BlockY <= BlockMaxY; BlockY++)
		{
			CWaterBlockData& blockData = m_WaterBlockData[BlockX*waterNumBlocksY + BlockY];
			s32 startIndex	= blockData.quadIndex;
			s32 endIndex	= startIndex + blockData.numQuads;

			for(int i = startIndex; i < endIndex; i++)
			{
				CWaterQuad& waterQuad = m_WaterQuadsBuffer[i];

				if (	waterQuad.minX < RenderCameraRangeMaxX && waterQuad.maxX > RenderCameraRangeMinX
					 &&	waterQuad.minY < RenderCameraRangeMaxY && waterQuad.maxY > RenderCameraRangeMinY)
				{
					gHeightSimulationActive = true;
					return;
				}


			}
		}
	}
	gHeightSimulationActive = false;
}

void Water::UpdateHeightSimulation()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));

	s32 bufferIndex = GetWaterUpdateIndex();

#if GTA_REPLAY
	// Assume no Replay playback or collection of height data.
	ms_DBVars[bufferIndex].m_pReplayWaterHeightPlayback = NULL;
	ms_DBVars[bufferIndex].m_pReplayWaterHeightResultsToRT = NULL;
#endif // GTA_REPLAY

	//don't pause during replay update, otherwise it wont update all the states correctly
	//when cameras are moved and jumps are performed
	if (fwTimer::IsGamePaused() REPLAY_ONLY(&& ShouldUpdateDuringReplay() == false))
	{
	#if GTA_REPLAY
		if(CReplayMgr::IsEditModeActive())
	{
			// Set Replay playback for next frame.
			SetCameraRange(m_ReplayWaterPos);
			m_DynamicWater_Coords_RT[bufferIndex] = m_ReplayWaterPos;
			SetReplayPlayBackForNextFrame(bufferIndex);

			//force the other side of this buffer to update while we're pause, otherwise the render side will be stuck rendering
			//either above or below the water when the camera position has been changed due to the buffer not being flipped while pause
			m_IsCameraUnderWater[1 - bufferIndex] = m_IsCameraUnderWater[bufferIndex];
		}
	#endif // GTA_REPLAY
		return;
	}
	

	Vector3 centerCoors;

#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		centerCoors = m_ReplayWaterPos;
	}
	else
#endif
	{
		if(m_bUseCamPos)
		{
			centerCoors = camInterface::GetPos();
		}
		else
		{
			centerCoors = CFocusEntityMgr::GetMgr().GetPos();
#if REPLAY_WATER_ENABLE_DEBUG
			//force the water to follow the player, useful for debugging replay water while in free cam
			CPed* pPlayer =  CPedFactory::GetFactory()->GetLocalPlayer();
			if(pPlayer)
				centerCoors = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
#endif // WATER_ENABLE_DEBUG
		}

#if GTA_REPLAY
		if(m_GPUUpdate)
		{
		#if !REPLAY_WATER_USES_TEXTURE_MEM_DIRECTLY
			// Collect coords and height data from GPU for Replay.
			m_ReplayWaterPos = ms_DBVars[bufferIndex].m_CentreCoordsResultsFromRT;
			m_pReplayWaterBufferForRecording = ms_DBVars[bufferIndex].m_pReplayWaterHeightResultsFromRT;
			ms_DBVars[bufferIndex].m_pReplayWaterHeightResultsToRT = CPacketCubicPatchWaterFrame::GetWaterHeightBuffer(GetWaterUpdateIndex());
		#else // !REPLAY_WATER_USES_TEXTURE_MEM_DIRECTLY

			m_pReplayWaterBufferForRecording = CPacketCubicPatchWaterFrame::GetWaterHeightTempWorkBuffer();

			// Pass frame "-2"  to Replay for recording.
			if(m_pReplayWaterBufferForRecording_Previous)
			{
				m_ReplayWaterPos = m_ReplayWaterPos_Previous;

			#if RSG_DURANGO
				// We can copy straight from GPU memory on Durango.
				sysMemCpy(m_pReplayWaterBufferForRecording, m_pReplayWaterBufferForRecording_Previous, DYNAMICGRIDELEMENTS*DYNAMICGRIDELEMENTS*sizeof(float));
			#endif // RSG_DURANGO
			#if RSG_ORBIS
				sysMemCpy(m_pReplayWaterBufferForRecording, m_pReplayWaterBufferForRecording_Previous, DYNAMICGRIDELEMENTS*DYNAMICGRIDELEMENTS*sizeof(float));
				/*
				// Get the texture corresponding to our lock base.
				grcTexture *pSource = CPacketCubicPatchWaterFrame::GetWaterHeightMapTexture(m_pReplayWaterBufferForRecording_Previous);
				// De-tile out the contents for Replay to compress.
				const sce::Gnm::Texture* srcGnmTexture = reinterpret_cast<const sce::Gnm::Texture*>(pSource->GetTexturePtr());
				uint64_t offset;
				uint64_t size;
				sce::GpuAddress::computeTextureSurfaceOffsetAndSize(&offset, &size, srcGnmTexture, 0, 0);
				sce::GpuAddress::TilingParameters tp;
				tp.initFromTexture(srcGnmTexture, 0, 0);
				sce::GpuAddress::detileSurface(m_pReplayWaterBufferForRecording, ((char*)srcGnmTexture->getBaseAddress() + offset), &tp);
				*/
			#endif //RSG_ORBIS
			}

			// Collect frame "-1" (previous one) created by the renderthread/GPU last frame.
			m_ReplayWaterPos_Previous = ms_DBVars[bufferIndex].m_CentreCoordsResultsFromRT;
			m_pReplayWaterBufferForRecording_Previous = ms_DBVars[bufferIndex].m_pReplayWaterHeightResultsFromRT;

			// Say where to place next frames data.
			ms_DBVars[bufferIndex].m_pReplayWaterHeightResultsToRT = CPacketCubicPatchWaterFrame::GetWaterHeightMapTextureLockBase(CPacketCubicPatchWaterFrame::GetNextBufferIndex());
		#endif // !REPLAY_WATER_USES_TEXTURE_MEM_DIRECTLY
		}
		else
		{
			// Cache centre coords etc for Replay.
			m_ReplayWaterPos = centerCoors;
			m_pReplayWaterBufferForRecording = (float *)m_WaterHeightBufferCPU[GetWaterUpdateIndex()];
			ms_DBVars[bufferIndex].m_pReplayWaterHeightResultsToRT = NULL;
		}
#endif // GTA_REPLAY
	}

	if(!m_bFreezeCameraUpdate)
	{
		//set the co-ords that will be used for rendering on the render thread.
		SetCameraRange(centerCoors);
		m_DynamicWater_Coords_RT[bufferIndex] = centerCoors;
	#if GTA_REPLAY
		if(CReplayMgr::IsEditModeActive())
		{
			// Set Replay playback for next frame.
			SetReplayPlayBackForNextFrame(bufferIndex);
		}
	#endif // GTA_REPLAY
	}

	UpdateHeightSimulationActive(bufferIndex);
	UpdateAverageHeight();

	//Handle fog texture streaming
	s32 centerBlockX = GetBlockFromWorldX(centerCoors.x);
	s32 centerBlockY = GetBlockFromWorldY(centerCoors.y);
	UpdateFogTextures(centerBlockX, centerBlockY, bufferIndex);

	if (!WaterEnabled())
		return;

#if __GPUUPDATE
	if(m_GPUUpdate)
	{
#if RSG_ORBIS
		m_WaterHeightBuffer	= (float(*)[DYNAMICGRIDELEMENTS])((sce::Gnm::Texture *) m_HeightMap[0]->GetTexturePtr())->getBaseAddress();
#else
		m_WaterHeightBuffer = m_WaterHeightBufferCPU[0];
#endif
		return;
	}
#endif //__GPUUPDATE

	if(!m_bUpdateDynamicWater)
		return;

	bool heightSimActiveLastFrame = gHeightSimulationActive;


	if(!gHeightSimulationActive)
		return;
	 
	if(!heightSimActiveLastFrame || m_bResetSim)
	{
		ClearDynamicWaterRT();
		//BANK_ONLY(m_bResetSim = false;)
		m_bResetSim = false;
	}

	ApplyWaterDisturb();

	sysTaskParameters p;
	memset(&p,0,sizeof(p));

	const CSecondaryWaterTune &secTunings = GetSecondaryWaterTunings();

	static CWaterUpdateDynamicParams	waterUpdateParams ALIGNED(128);
	waterUpdateParams.m_CenterCoors		= centerCoors;
	waterUpdateParams.m_timestep		= fwTimer::GetTimeStep()*m_TimeStepScale;
	waterUpdateParams.m_timeInMilliseconds = NetworkInterface::GetSyncedTimeInMilliseconds();
	Assert(waterUpdateParams.m_timestep >= 0.0f);

	waterUpdateParams.m_waveLengthScale = m_WaveLengthScale;

	float oceanWaveMult	= Lerp(secTunings.m_OceanWaveWindScale, secTunings.m_OceanWaveAmplitude, secTunings.m_OceanWaveAmplitude*g_weather.GetWind());
	oceanWaveMult		= Clamp(oceanWaveMult, secTunings.m_OceanWaveMinAmplitude, secTunings.m_OceanWaveMaxAmplitude);
	float shoreWaveMult	= Lerp(secTunings.m_ShoreWaveWindScale, secTunings.m_ShoreWaveAmplitude, secTunings.m_ShoreWaveAmplitude*g_weather.GetWind());
	shoreWaveMult		= Clamp(shoreWaveMult, secTunings.m_ShoreWaveMinAmplitude, secTunings.m_ShoreWaveMaxAmplitude);
	const s32 xBorder	= 500;
	const s32 yBorder	= 1000;
	float deepScale		= 2.0f*Max(	Min(abs(centerCoors.x - (sm_WorldBorderXMin + sm_WorldBorderXMax)/2), (WORLD_WIDTH	+ xBorder)/2.0f)/(WORLD_WIDTH	+ xBorder),
									Min(abs(centerCoors.y - (sm_WorldBorderYMin + sm_WorldBorderYMax)/2), (WORLD_HEIGHT	+ yBorder)/2.0f)/(WORLD_HEIGHT	+ yBorder));
	deepScale			= deepScale*deepScale;
	deepScale			= deepScale*deepScale;

	oceanWaveMult		= Lerp(deepScale, oceanWaveMult, oceanWaveMult*secTunings.m_DeepOceanScale*sm_ScriptDeepOceanScaler);


	waterUpdateParams.m_waveMult		= oceanWaveMult;
	waterUpdateParams.m_waveTimePeriod	= m_WaveTimePeriod;
	waterUpdateParams.m_ShoreWavesMult	= shoreWaveMult;

	float disturbScale					= Max(m_DisturbScale*oceanWaveMult, secTunings.m_OceanNoiseMinAmplitude);

	waterUpdateParams.m_disturbAmount	= disturbScale;	
	// dest address of double-buffered height buffer for Render():
	waterUpdateParams.m_WaterHeightBuffer = m_WaterHeightBufferCPU[GetWaterRenderIndex()];

	m_DynamicWater_Height	= m_WaterHeightBufferCPU[GetWaterUpdateIndex()];
	m_DynamicWater_dHeight	= &m_WaterVelocityBuffer[0];
	m_aWaveQuads = (CWaveQuad*)m_WaveQuadsBuffer.GetElements();
	m_aCalmingQuads = (CCalmingQuad*)m_CalmingQuadsBuffer.GetElements();
	m_aExtraCalmingQuads = (sm_ExtraQuadsCount > 0) ? (CCalmingQuad*)sm_ExtraCalmingQuads.GetElements() : NULL;
	m_nNumOfExtraWaterCalmingQuads = MAXEXTRACALMINGQUADS;

#if USE_WATER_THREAD
	// the block has been placed at a safe point in the end of game update _only_.
	Assert(m_WaterTaskHandle==NULL);	// assume BlockTillUpdateComplete() was called before

	p.Input.Data	= &waterUpdateParams;
	p.Input.Size	= sizeof(waterUpdateParams);
	m_WaterTaskHandle=rage::sysTaskManager::Create(TASK_INTERFACE(UpdateDynamicWaterTask), p);
	Assertf(m_WaterTaskHandle,"Create Dynamic Water Task failed");
#else

	UpdateDynamicWater(&waterUpdateParams);

#if RSG_ORBIS || RSG_DURANGO
	u32 heightMapIndex = (m_WaterHeightMapIndex + 1) % 3;
	grcTextureLock lock;
	m_HeightMap[heightMapIndex]->LockRect(0, 0, lock, grcsWrite);
	float* textureBuffer = (float*)lock.Base;
	m_WaterHeightBuffer = m_WaterHeightBufferCPU[GetWaterUpdateIndex()];
	memcpy(textureBuffer, m_WaterHeightBuffer, DYNAMICGRIDELEMENTS * DYNAMICGRIDELEMENTS * sizeof(float));
	m_HeightMap[heightMapIndex]->UnlockRect(lock);
#endif

#endif //USE_WATER_THREAD
}

#if GTA_REPLAY
void Water::SetReplayPlayBackForNextFrame(s32 bufferIndex)
{
#if !REPLAY_WATER_USES_TEXTURE_MEM_DIRECTLY
	// Say where the Replay water data will be next frame.
	ms_DBVars[bufferIndex].m_pReplayWaterHeightPlayback = CPacketCubicPatchWaterFrame::GetWaterHeightBuffer(GetWaterUpdateIndex());
	CReplayMgr::ExtractCachedWaterFrame(ms_DBVars[bufferIndex].m_pReplayWaterHeightPlayback, RSG_ORBIS ? NULL : &m_WaterHeightBuffer[0][0]);

#else // !REPLAY_WATER_USES_TEXTURE_MEM_DIRECTLY

	// Say where the Replay water data will be next frame (triple buffer as per recording).
	ms_DBVars[bufferIndex].m_pReplayWaterHeightPlayback = CPacketCubicPatchWaterFrame::GetWaterHeightMapTextureLockBase(CPacketCubicPatchWaterFrame::GetNextBufferIndex());

#if RSG_DURANGO
	// Write directly to texture memory on Durango.
	CReplayMgr::ExtractCachedWaterFrame(ms_DBVars[bufferIndex].m_pReplayWaterHeightPlayback, RSG_ORBIS ? NULL : &m_WaterHeightBuffer[0][0]);
#endif // RSG_DURANGO
#if RSG_ORBIS
	CReplayMgr::ExtractCachedWaterFrame(ms_DBVars[bufferIndex].m_pReplayWaterHeightPlayback, RSG_ORBIS ? NULL : &m_WaterHeightBuffer[0][0]);
	/*
	// Extract to a temp working buffer.
	CReplayMgr::ExtractCachedWaterFrame(CPacketCubicPatchWaterFrame::GetWaterHeightTempWorkBuffer(), RSG_ORBIS ? NULL : &m_WaterHeightBuffer[0][0]);
	// Tile into the texture.
	grcTexture *pDest = CPacketCubicPatchWaterFrame::GetWaterHeightMapTexture(ms_DBVars[bufferIndex].m_pReplayWaterHeightPlayback);
	// De-tile out the contents for Replay to compress.
	const sce::Gnm::Texture* srcGnmTexture = reinterpret_cast<const sce::Gnm::Texture*>(pDest->GetTexturePtr());
	uint64_t offset;
	uint64_t size;
	sce::GpuAddress::computeTextureSurfaceOffsetAndSize(&offset, &size, srcGnmTexture, 0, 0);
	sce::GpuAddress::TilingParameters tp;
	tp.initFromTexture(srcGnmTexture, 0, 0);
	sce::GpuAddress::tileSurface(((char*)srcGnmTexture->getBaseAddress() + offset), CPacketCubicPatchWaterFrame::GetWaterHeightTempWorkBuffer(), &tp);
	*/
#endif // RSG_ORBIS
#endif // !REPLAY_WATER_USES_TEXTURE_MEM_DIRECTLY
}

bool Water::ShouldUpdateDuringReplay()
{
	return CReplayMgr::IsReplayInControlOfWorld() && CReplayMgr::IsSettingUp() == false;
}

#endif // GTA_REPLAY

bool Water::IsHeightSimulationActive(){ return gHeightSimulationActive; }

void RenderBorderQuads(s32 pass);
#if __USEVERTEXSTREAMRENDERTARGETS
void UpdateVertexStreamRenderTargets();
#endif //__USEVERTEXSTREAMRENDERTARGETS


static s32 GetTrisVertsA(float minX, float maxX, float minY, float maxY, float a1, float a2, float a3, float a4, float slope, float offset, Vector3 *points);
static s32 GetTrisVertsB(float minX, float maxX, float minY, float maxY, float a1, float a2, float a3, float a4, float slope, float offset, Vector3 *points);
static s32 GetTrisVertsC(float minX, float maxX, float minY, float maxY, float a1, float a2, float a3, float a4, float slope, float offset, Vector3 *points);
static s32 GetTrisVertsD(float minX, float maxX, float minY, float maxY, float a1, float a2, float a3, float a4, float slope, float offset, Vector3 *points);

template <typename T> static int WaterQuadGetPoints(const T& quad, Vec3V points[5], float a[5], bool UNUSED_PARAM(bTrianglesAsQuads), float UNUSED_PARAM(zOffset))
{
	const float minX = (float)quad.minX;
	const float maxX = (float)quad.maxX;
	const float minY = (float)quad.minY;
	const float maxY = (float)quad.maxY;

	points[0] = Vec3V(minX, minY, 0.0f);
	points[1] = Vec3V(maxX, minY, 0.0f);
	points[2] = Vec3V(maxX, maxY, 0.0f);
	points[3] = Vec3V(minX, maxY, 0.0f);
	a[0] = 1.0f;
	a[1] = 1.0f;
	a[2] = 1.0f;
	a[3] = 1.0f;

	return 4;
}

// code similar to RenderFlatWaterRectangle
template <> int WaterQuadGetPoints<CWaterQuad>(const CWaterQuad& quad, Vec3V points[5], float a[5], bool bTrianglesAsQuads, float zOffset)
{
	const float minX = (float)quad.minX;
	const float maxX = (float)quad.maxX;
	const float minY = (float)quad.minY;
	const float maxY = (float)quad.maxY;
	const float a1   = (float)quad.a1/255.0f;
	const float a2   = (float)quad.a2/255.0f;
	const float a3   = (float)quad.a3/255.0f;
	const float a4   = (float)quad.a4/255.0f;
	const float z    = quad.GetZ() + zOffset;

	int numPoints = 0;

	if (quad.GetType() != 0 && !bTrianglesAsQuads)
	{
		const float slope  = quad.slope;
		const float offset = quad.offset;

		if (slope == 0.0f) // this is necessary to reconstruct water quad helpers before cutting
		{
			switch (quad.GetType())
			{
			case 1:
				// +
				// |\  A (type=1)
				// o-+
				points[numPoints++] = Vec3V(minX, minY, z);
				points[numPoints++] = Vec3V(maxX, minY, z);
				points[numPoints++] = Vec3V(minX, maxY, z);
				break;
			case 2:
				// +-+
				// |/  B (type=2)
				// o
				points[numPoints++] = Vec3V(minX, minY, z);
				points[numPoints++] = Vec3V(maxX, maxY, z);
				points[numPoints++] = Vec3V(minX, maxY, z);
				break;
			case 3:
				// +-+
				//  \| C (type=3)
				// o +
				points[numPoints++] = Vec3V(maxX, minY, z);
				points[numPoints++] = Vec3V(maxX, maxY, z);
				points[numPoints++] = Vec3V(minX, maxY, z);
				break;
			case 4:
				//   +
				//  /| D (type=4)
				// o-+
				points[numPoints++] = Vec3V(minX, minY, z);
				points[numPoints++] = Vec3V(maxX, minY, z);
				points[numPoints++] = Vec3V(maxX, maxY, z);
				break;
			}
		}
		else if (quad.GetType() <= 2)
		{
			if (slope < 0.0f)
			{
				// +
				// |\  A (type=1)
				// o-+
				numPoints = GetTrisVertsA(minX, maxX, minY, maxY, a1, a2, a3, a4, slope, offset, (Vector3*)points);
			}
			else
			{
				// +-+
				// |/  B (type=2)
				// o
				numPoints = GetTrisVertsB(minX, maxX, minY, maxY, a1, a2, a3, a4, slope, offset, (Vector3*)points);
			}
		}
		else
		{
			if (slope < 0.0f)
			{
				// +-+
				//  \| C (type=3)
				// o +
				numPoints = GetTrisVertsC(minX, maxX, minY, maxY, a1, a2, a3, a4, slope, offset, (Vector3*)points);
			}
			else
			{
				//   +
				//  /| D (type=4)
				// o-+
				numPoints = GetTrisVertsD(minX, maxX, minY, maxY, a1, a2, a3, a4, slope, offset, (Vector3*)points);
			}
		}

		if (numPoints > 0) // GetTriVerts* stored a (alpha?) here, we need to set z before rendering
		{
			for (int i = 0; i < numPoints; i++)
			{
				a[i] = points[i].GetZf();
				points[i].SetZ(ScalarV(z));
			}
		}
	}

	if (numPoints == 0) // quad
	{
		points[0] = Vec3V(minX, minY, z);
		points[1] = Vec3V(maxX, minY, z);
		points[2] = Vec3V(maxX, maxY, z);
		points[3] = Vec3V(minX, maxY, z);
		a[0] = a1;
		a[1] = a2;
		a[2] = a3;
		a[3] = a4;

		numPoints = 4;
	}

	return numPoints;
}

static int WaterQuadGetPoints_(const CWaterQuad& quad, Vec3V points[5])
{
	float a[5]; // not used
	return WaterQuadGetPoints(quad, points, a, false, 0.0f);
}

#if __BANK || (__WIN32PC && !__FINAL)

void Water::GetWaterPolys(atArray<CWaterPoly>& polys)
{
	for (int i = 0; i < m_WaterQuadsBuffer.GetCount(); i++)
	{
		CWaterPoly poly;
		float a[5];
		poly.m_numPoints = WaterQuadGetPoints(m_WaterQuadsBuffer[i], poly.m_points, a, false, 0.0f);
		polys.PushAndGrow(poly);
	}
}

#endif // __BANK || (__WIN32PC && !__FINAL)

static void ScanCutBlocks(const grcViewport &grcVP)
{
#if __BANK
	sysTimer timer;
#endif //__BANK

#if __ASSERT
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));

	// don't call this multiple times per frame
	static u32 framePrev = 0;
	const u32 frameCurrent = fwTimer::GetSystemFrameCount();
	Assert(framePrev == 0 || framePrev != frameCurrent);
	framePrev = frameCurrent;
#endif // __ASSERT

	s32 b = GetWaterUpdateIndex();
	m_WaterBlockDrawList[b].Reset();

	s32 quadsToRender = 0;

	bool bRegisterWaterQuadsForHiZ = COcclusion::bRasterizeWaterQuadsInHiZ && !IsCameraUnderwater();
	float fWaterQuadHiZBias = COcclusion::GetWaterQuadHiZBias();
	float fWaterQuadStencilBias = COcclusion::GetWaterQuadStencilBias();

#define DEBUG_BLOCK_OCCLUSION_STATS (0 && __DEV)

#if DEBUG_BLOCK_OCCLUSION_STATS
	int numWaterBlocksVisible = 0;
	int numWaterBlocksOccluded = 0;
	int numBlocksVisibleButOccluded = 0;
	int numBlocksNotVisibleOrOccluded = 0;
#endif // DEBUG_BLOCK_OCCLUSION_STATS

const s32 waterNumBlocksX = WATERNUMBLOCKSX;
const s32 waterNumBlocksY = WATERNUMBLOCKSY;

	for(s32 x = 0; x < waterNumBlocksX; x++)
	{
		for(s32 y = 0; y < waterNumBlocksY; y++)
		{
			s32 blockIndex = y+x*waterNumBlocksY;
			const CWaterBlockData& blockData = m_WaterBlockData[blockIndex];

			if(blockData.numQuads == 0)
				continue;

			static dev_float heightScale	= 1.0f;
			Vector3 minV = Vector3(	(float)WATERBLOCKTOWORLDX(x),
									(float)WATERBLOCKTOWORLDY(y),
									blockData.minHeight - heightScale);
			Vector3 maxV = Vector3(	(float)(WATERBLOCKTOWORLDX(x) + WATERBLOCKWIDTH),
									(float)(WATERBLOCKTOWORLDY(y) + WATERBLOCKWIDTH),
									blockData.maxHeight + heightScale);



#if DEBUG_BLOCK_OCCLUSION_STATS

			static dev_s32	testThreshold = 0;


			const bool bIsNotOccluded = m_bRunScanCutBlocksEarly ? true : COcclusion::IsBoxVisibleFast(spdAABB((Vec3V)minV, (Vec3V)maxV));
			const bool bIsVisibleView = grcVP.IsAABBVisible(minV, maxV, m_frustumHasBeenLocked ? m_lockedFrustum : grcVP.GetCullFrustumLRTB()) ? true : false;

			if (bIsVisibleView)
			{
				numWaterBlocksVisible++;
			}

			if (!bIsNotOccluded)
			{
				numWaterBlocksOccluded++;
			}
			if (bIsVisibleView && !bIsNotOccluded)
			{
				numBlocksVisibleButOccluded++;
			}
			else if (!bIsVisibleView && bIsNotOccluded)
			{
				numBlocksNotVisibleOrOccluded++; // this shouldn't happen, but let's check
			}

			if(blockData.numQuads > testThreshold)
			{
				if (!bIsNotOccluded)
					continue;
			}
			else if (!bIsVisibleView)
				continue;
#else

#if __BANK
			const Mat44V &frustm = m_frustumHasBeenLocked ? m_lockedFrustum : grcVP.GetCullFrustumLRTB();
#else // __BANK
			const Mat44V &frustm = grcVP.GetCullFrustumLRTB();
#endif // __BANK

			if (bRegisterWaterQuadsForHiZ)
			{
				const s32 worldX = WaterBlockIndexToWorldX(blockIndex, waterNumBlocksY);
				const s32 worldY = WaterBlockIndexToWorldY(blockIndex, waterNumBlocksY);

				if ((worldX < RenderCameraBlockMaxX) &&
					(worldX + WATERBLOCKWIDTH > RenderCameraBlockMinX) &&
					(worldY < RenderCameraBlockMaxY) &&
					(worldY + WATERBLOCKWIDTH > RenderCameraBlockMinY) )
				{
					int startIndex = m_WaterBlockData[blockIndex].quadIndex;
					int numQuads = m_WaterBlockData[blockIndex].numQuads;
					for (int i=startIndex;i< (startIndex+numQuads);i++)
					{
						const CWaterQuad& quad = m_WaterQuadsBuffer[i];

#if __BANK
						if (m_TrianglesAsQuadsForOcclusion)
						{
							COcclusion::RegisterWaterQuadHiZ(quad.minX, quad.minY, quad.maxX, quad.maxY, quad.GetZ() + fWaterQuadHiZBias);
						}
						else
#endif // __BANK
						{
							Vec3V points[5];
							float a[5]; // unused
							const int numPoints = WaterQuadGetPoints(quad, points, a, false, fWaterQuadHiZBias);
							COcclusion::RegisterWaterPolyHiZ(points, numPoints);
						}
					}
				}
			}

			if((m_bRunScanCutBlocksEarly || !m_EnableVisibility) && !grcVP.IsAABBVisible(minV, maxV, frustm))
				continue;

			if(!m_bRunScanCutBlocksEarly && m_EnableVisibility)
			{
				if(!COcclusion::IsBoxVisibleFast(spdAABB((Vec3V)minV, (Vec3V)maxV)))
					continue;
			}
#endif

			if(!Verifyf(!m_WaterBlockDrawList[b].IsFull(), "Water Block Drawlist is full!"))
				break;

			m_WaterBlockDrawList[b].Append() = (u16)blockIndex;

			CWaterBlockData wb = m_WaterBlockData[blockIndex];

			if (BANK_SWITCH(m_RasterizeWaterQuadsIntoStencil, true))
			{
				for (int i=wb.quadIndex;i< (wb.quadIndex+wb.numQuads);i++)
				{
					const CWaterQuad& quad = m_WaterQuadsBuffer[i];

					if (!quad.GetNoStencil())
					{
#if __BANK
						if (m_TrianglesAsQuadsForOcclusion)
						{
							COcclusion::RegisterWaterQuadStencil(quad.minX, quad.minY, quad.maxX, quad.maxY, quad.GetZ() + fWaterQuadStencilBias);
						}
						else
#endif // __BANK
						{
							Vec3V points[5];
							float a[5]; // unused
							const int numPoints = WaterQuadGetPoints(quad, points, a, false, fWaterQuadStencilBias);
							COcclusion::RegisterWaterPolyStencil(points, numPoints);
						}
					}
				}
			}

			quadsToRender += wb.numQuads;
		}
	}
	RenderBorderQuads(pass_invalid);

#if DEBUG_BLOCK_OCCLUSION_STATS
	grcDebugDraw::AddDebugOutput("water blocks visible = %d", numWaterBlocksVisible);
	grcDebugDraw::AddDebugOutput("water blocks occluded = %d", numWaterBlocksOccluded);
	grcDebugDraw::AddDebugOutput("water blocks visible but occluded = %d, not occluded = %d", numBlocksVisibleButOccluded, numWaterBlocksVisible - numBlocksVisibleButOccluded);
	grcDebugDraw::AddDebugOutput("water blocks not visible or occluded = %d", numBlocksNotVisibleOrOccluded);
#endif // DEBUG_BLOCK_OCCLUSION_STATS

	if (BANK_SWITCH(m_RasterizeRiverBoxesIntoStencil, false)) // TEMPORARY HACK -- render river bounds into water stencil
	{
#if __BANK
		int numWaterGeoms = 0;
		int numVisWaterGeoms = 0;
#endif // __BANK

		for (int i = 0; i < River::GetRiverEntityCount(); i++)
		{
			const CEntity* pWaterEntity = River::GetRiverEntity(i);

			if (pWaterEntity)
			{
				const Mat34V matrix = pWaterEntity->GetMatrix();
				const Drawable* pDrawable = pWaterEntity->GetDrawable();

				if (pDrawable)
				{
					const rmcLodGroup& group = pDrawable->GetLodGroup();

					if (group.ContainsLod(LOD_HIGH))
					{
						const grmModel& model = group.GetLodModel0(LOD_HIGH);
						const int numGeoms = model.GetGeometryCount();

						const Vec3V camPosLocal = UnTransformOrtho(matrix, grcVP.GetCameraPosition());

						for (int geomIndex = 0; geomIndex < numGeoms; geomIndex++)
						{
							const int shaderId = model.GetShaderIndex(geomIndex);
							const grmShader& shader = pDrawable->GetShaderGroup().GetShader(shaderId);

							if (strstr(shader.GetName(), "water_"))
							{
								const spdAABB& aabbLocal = model.GetGeometryAABB(geomIndex);
								const Vec3V minV = aabbLocal.GetMin();
								const Vec3V maxV = aabbLocal.GetMax();

								spdAABB aabbWorld = aabbLocal;
								aabbWorld.Transform(matrix);

								if (grcVP.IsAABBVisible(aabbWorld.GetMin().GetIntrin128(), aabbWorld.GetMax().GetIntrin128(), grcVP.GetCullFrustumLRTB()))
								{
									const Vec3V corners[8] =
									{
										Transform(matrix, minV),
										Transform(matrix, GetFromTwo<Vec::X2,Vec::Y1,Vec::Z1>(minV, maxV)),
										Transform(matrix, GetFromTwo<Vec::X1,Vec::Y2,Vec::Z1>(minV, maxV)),
										Transform(matrix, GetFromTwo<Vec::X2,Vec::Y2,Vec::Z1>(minV, maxV)),
										Transform(matrix, GetFromTwo<Vec::X1,Vec::Y1,Vec::Z2>(minV, maxV)),
										Transform(matrix, GetFromTwo<Vec::X2,Vec::Y1,Vec::Z2>(minV, maxV)),
										Transform(matrix, GetFromTwo<Vec::X1,Vec::Y2,Vec::Z2>(minV, maxV)),
										Transform(matrix, maxV),
									};

									class RegisterWaterQuadForBox { public: static void func(const Vec3V corners[8], int idx0, int idx1, int idx2, int idx3, bool bProjectAndClip, const grcViewport& vp, ScalarV_In z0)
									{
										Vec3V temp[4];
										temp[0] = corners[idx0];
										temp[1] = corners[idx1];
										temp[2] = corners[idx2];
										temp[3] = corners[idx3];

										if (bProjectAndClip)
										{
											Vec3V clipped[NELEM(temp) + 6];
											const int numClipped = PolyClipToFrustum(clipped, temp, NELEM(temp), vp.GetCompositeMtx());

											if (numClipped >= 3)
											{
												const Vec3V camPos = +vp.GetCameraMtx().GetCol3();
												const Vec3V camDir = -vp.GetCameraMtx().GetCol2();

												for (int j = 0; j < numClipped; j++)
												{
													clipped[j] = camPos + (clipped[j] - camPos)*(z0/Dot(clipped[j] - camPos, camDir));
												}

												temp[2] = clipped[0];

												for (int j = 2; j < numClipped; j++)
												{
													temp[1] = clipped[j - 1];
													temp[0] = clipped[j];

													COcclusion::RegisterWaterPolyStencil(temp, 3);
												}
											}
										}
										else
										{
											COcclusion::RegisterWaterPolyStencil(temp, NELEM(temp));
										}
									}};

									const bool bCamInsideBox = aabbLocal.IntersectsAABB(spdAABB(camPosLocal - Vec3V(V_ONE), camPosLocal + Vec3V(V_ONE)));
									const ScalarV z0 = ScalarV(grcVP.GetNearClip() + 2.34f); // TODO -- why do we need to offset by 2.34f? if we don't, the polygons get clipped

									RegisterWaterQuadForBox::func(corners, 2,3,1,0, bCamInsideBox, grcVP, z0);
									RegisterWaterQuadForBox::func(corners, 4,5,7,6, bCamInsideBox, grcVP, z0);
									RegisterWaterQuadForBox::func(corners, 1,3,7,5, bCamInsideBox, grcVP, z0);
									RegisterWaterQuadForBox::func(corners, 4,6,2,0, bCamInsideBox, grcVP, z0);
									RegisterWaterQuadForBox::func(corners, 0,1,5,4, bCamInsideBox, grcVP, z0);
									RegisterWaterQuadForBox::func(corners, 6,7,3,2, bCamInsideBox, grcVP, z0);
#if __BANK
									if (m_RasterizeRiverBoxesIntoStencilDebugDraw)
									{
										grcDebugDraw::BoxOriented(minV, maxV, matrix, bCamInsideBox ? Color32(128,255,0,255) : Color32(0,128,255,255), false);

										numVisWaterGeoms++;
									}
#endif // __BANK
								}
#if __BANK
								if (m_RasterizeRiverBoxesIntoStencilDebugDraw)
								{
									numWaterGeoms++;
								}
#endif // __BANK
							}
						}
					}
				}
			}
		}

#if __BANK
		if (m_RasterizeRiverBoxesIntoStencilDebugDraw)
		{
			grcDebugDraw::AddDebugOutput("%d vis water geoms (%d total)", numVisWaterGeoms, numWaterGeoms);
		}
#endif // __BANK
	}

	m_QuadsToRendered[b] = quadsToRender;

	BANK_ONLY(m_NumBlocksDrawn = m_WaterBlockDrawList[b].GetCount());


	COcclusion::WaterOcclusionDataIsReady();
	
#if __BANK
	if (m_drawFrustum)
	{
		fwScanNodesDebug::DrawFrustumForScreenQuad(fwScreenQuad(SCREEN_QUAD_FULLSCREEN), m_lockedFrustum, Color_orange);
	}
#endif // __BANK
	PF_INCREMENTBY(Water_ScanCutBlocks, timer.GetMsTime());
}

void Water::ScanCutBlocksWrapper()
{
	if (m_bRunScanCutBlocksEarly)
	{
		const grcViewport* primaryVP = gVpMan.GetCurrentGameGrcViewport();
#if __BANK
		if (m_frustumHasBeenLocked)
		{
			if (!m_frustumStored)
			{
				m_lockedFrustum = primaryVP->GetCompositeMtx();
				m_frustumStored = true;
			}
		}
		else
		{
			m_frustumStored = false;
		}
#endif // __BANK
		ScanCutBlocks(*primaryVP);
	}
}

void Water::PreRender(const grcViewport &viewport)
{
	if (!waterDataLoaded)
		return;

	if(!WaterEnabled())
		return;

#if __BANK
	sysTimer timer;
#endif //__BANK

	if (!m_bRunScanCutBlocksEarly)
	{
#if __BANK
		if (m_frustumHasBeenLocked)
		{
			if (!m_frustumStored)
			{
				m_lockedFrustum = viewport.GetCompositeMtx();
				m_frustumStored = true;
			}
		}
		else
		{
			m_frustumStored = false;
		}
#endif //__BANK
		ScanCutBlocks(viewport);
	}
	m_GBufferRenderPhaseFrustum = viewport.GetCullFrustumLRTB();

	//Check Border Quads
	//render border quads
	Vec3V minV;
	Vec3V maxV;
	CalculateFrustumBounds(minV, maxV, viewport);
	//minV = RoundToNearestIntNegInf(minV); // floor -- don't need to round here, we're passing floats to IsBoxVisibleFast
	//maxV = RoundToNearestIntPosInf(maxV); // ceil
	const float minX = minV.GetXf();
	const float minY = minV.GetYf();
	const float maxX = maxV.GetXf();
	const float maxY = maxV.GetYf();

	float defaultHeight = m_CurrentDefaultHeight;
	bool borderQuadsVisible	= false;

	if(minY < (float)sm_WorldBorderYMin)
		borderQuadsVisible |= COcclusion::IsBoxVisibleFast(spdAABB(	Vec3V(minX,						minY,						defaultHeight),
																	Vec3V(maxX,						(float)sm_WorldBorderYMin,	defaultHeight)));

	if(minX < (float)sm_WorldBorderXMin)
		borderQuadsVisible |= COcclusion::IsBoxVisibleFast(spdAABB(	Vec3V(minX,						(float)sm_WorldBorderYMin,	defaultHeight),
																	Vec3V((float)sm_WorldBorderXMin,(float)sm_WorldBorderYMax,	defaultHeight)));

	if(maxY > (float)sm_WorldBorderYMax)
		borderQuadsVisible |= COcclusion::IsBoxVisibleFast(spdAABB(	Vec3V(minX,						(float)sm_WorldBorderYMax,	defaultHeight),
																	Vec3V(maxX,						maxY,						defaultHeight)));

	if(maxX > (float)sm_WorldBorderXMax)
		borderQuadsVisible |= COcclusion::IsBoxVisibleFast(spdAABB(	Vec3V((float)sm_WorldBorderXMax,(float)sm_WorldBorderYMin,	defaultHeight),
																	Vec3V(maxX,						(float)sm_WorldBorderYMax,	defaultHeight)));

#if RSG_PC
	if(m_bRunScanCutBlocksEarly)
	{
		//	do occlusion culling
		s32 b = GetWaterUpdateIndex();

		m_WaterBlockDrawListTemp = m_WaterBlockDrawList[b];
		m_WaterBlockDrawList[b].Reset();

		const s32 waterNumBlocksY = WATERNUMBLOCKSY;

		for(s32 i = 0; i < m_WaterBlockDrawListTemp.GetCount(); ++i)
		{
			s32 blockIndex	= m_WaterBlockDrawListTemp[i];

			const CWaterBlockData& blockData = m_WaterBlockData[blockIndex];

			const s32 x = WaterBlockIndexToWorldX(blockIndex, waterNumBlocksY);
			const s32 y = WaterBlockIndexToWorldY(blockIndex, waterNumBlocksY);

			static dev_float heightScale	= 1.0f;
			Vector3 minV = Vector3(	(float)x,
				(float)y,
				blockData.minHeight - heightScale);
			Vector3 maxV = Vector3(	(float)(x + WATERBLOCKWIDTH),
				(float)(y + WATERBLOCKWIDTH),
				blockData.maxHeight + heightScale);

			if(!COcclusion::IsBoxVisibleFast(spdAABB((Vec3V)minV, (Vec3V)maxV)))
			{
				continue;
			}
			m_WaterBlockDrawList[b].Append() = (u16)blockIndex;
		}
	}
#endif

	m_BorderQuadsVisible	= borderQuadsVisible;

}

void AddToDrawListBoatInteriorsNoWater(s32 pass) 
{
	if(pass==0)
		DLC( dlCmdSetDepthStencilStateEx, (m_BoatDepthStencilState,  WATER_STENCIL_ID | WATER_STENCIL_MASKID));
	else if(pass==1)		
		DLC( dlCmdSetDepthStencilStateEx, (m_BoatDepthStencilState, DEFERRED_MATERIAL_VEHICLE));
	else
	{
		Assertf(FALSE, "Invalid pass number!");
	}

	DLCPushMarker("Boat Interior Stencil");

	DLC( dlCmdSetBlendState,		(grcStateBlock::BS_Default_WriteMaskNone));
	DLC( dlCmdSetRasterizerState,	(grcStateBlock::RS_Default));

	DLC_Add(CRenderPhaseCascadeShadowsInterface::SetCloudShadowParams);
	DLC_Add(CRenderPhaseCascadeShadowsInterface::SetSharedShaderVars);
	DLC_Add(CShaderLib::SetFogRayTexture);

	const s32	numVehicles = (s32) CVehicle::GetPool()->GetSize();
	for(s32 i=0; i<numVehicles; i++)
	{		
		CVehicle *pVehicle = CVehicle::GetPool()->GetSlot(i);
		if(pVehicle)
		{
			bool bDrawNoWater = false;
			
			const u32 modelNameHash = pVehicle->GetBaseModelInfo()->GetModelNameHash();

			if(	(pVehicle->GetVehicleType()==VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE)	&&
				((modelNameHash==MID_TECHNICAL2) || (modelNameHash==MID_APC))		)
			{
				bDrawNoWater = true;

				CAmphibiousAutomobile *pAmpAutomobile = (CAmphibiousAutomobile*)pVehicle;
				for(int i=0; i<pAmpAutomobile->GetNumDoors(); i++) 
                { 
					CCarDoor* pDoor = pAmpAutomobile->GetDoor(i); 
					if(pDoor->GetHierarchyId() == VEH_DOOR_DSIDE_F || pDoor->GetHierarchyId() == VEH_DOOR_PSIDE_F) 
                    { 
						if((!pDoor->GetIsClosed() || !pDoor->GetIsIntact(pAmpAutomobile)) && pAmpAutomobile->IsPropellerSubmerged())
						{ 
							// Flood interior 
							bDrawNoWater = false;
							break;
						}
					} 
                }
			}
			else if(	(pVehicle->GetVehicleType()==VEHICLE_TYPE_PLANE)				&&
						((modelNameHash==MID_TULA) || (modelNameHash==MID_SEABREEZE))	)
			{
				bDrawNoWater = true;
			}
			else if((pVehicle->GetVehicleType()==VEHICLE_TYPE_BOAT)			||
					(pVehicle->GetVehicleType()==VEHICLE_TYPE_SUBMARINE)	)	
			{
				bDrawNoWater = true;
			}
			
			if(bDrawNoWater)
			{
				if(	(pVehicle->GetTransform().GetC().GetZf() < 0.0f	)										||		// boat is capsized?
					(pVehicle->m_Buoyancy.GetStatus() == FULLY_IN_WATER)									||		// boat is fully submerged
					(pVehicle->IsBeingRespotted())															||		// boat is being respotted
					(pVehicle->PopTypeIsMission() && pVehicle->IsBaseFlagSet(fwEntity::FORCED_ZERO_ALPHA))	||		// invisible boat in DM creation mode (when overlapping something)
					(CPostScan::GetAlphaOverride(pVehicle) <= 248)											||		// boat is half visible (for network racing)
					(!pVehicle->GetIsVisibleInSomeViewportThisFrame())										||		// boat is not visible
					(pVehicle->IsBaseFlagSet(fwEntity::FORCE_ALPHA))												// boat does bad thing to alpha.
					)
				{
					// do nothing
				}
				else
				{
					spdAABB aabb;
					pVehicle->GetAABB(aabb);
				#if __BANK
					int visible = m_bUseFrustumCull ? grcViewport::IsAABBVisible(aabb.GetMin().GetIntrin128(), aabb.GetMax().GetIntrin128(), m_GBufferRenderPhaseFrustum) : 1;
				#else // __BANK
					int visible = grcViewport::IsAABBVisible(aabb.GetMin().GetIntrin128(), aabb.GetMax().GetIntrin128(), m_GBufferRenderPhaseFrustum);
				#endif // __BANK
					if (visible)
					{
						// use rmStandard as vehicle damage variables must be set (see: CGtaDrawData::BeforeAddToDrawList())
						DLC ( dlCmdSetBucketsAndRenderMode, (CRenderer::RB_NOWATER, rmStandard) );
						fwDrawData* pDrawHandler = pVehicle->GetDrawHandlerPtr();
						if (pDrawHandler)
						{
							CDrawDataAddParams drawParams;
							drawParams.AlphaFade = 255;
							pDrawHandler->AddToDrawList(pVehicle, &drawParams);
						}
					}
				}
			}
		}
	}

	// Reset alphas stipple and global: the boat will set it to "stuff"
	DLC_Add( CShaderLib::ResetStippleAlpha );
	DLC_Add( CShaderLib::SetGlobalAlpha, 1.0f );

	DLC ( dlCmdSetBucketsAndRenderMode, (CRenderer::RB_OPAQUE, rmStandard) );

	DLCPopMarker();
}


void Water::AddToDrawListBoatInteriorsNoSplashes(int drawListType)
{
	bool bDuringDrawSceneRP = FALSE;

	// check in which renderphase we are now:
	if(drawListType==DL_RENDERPHASE_DRAWSCENE)
		bDuringDrawSceneRP = TRUE;

	// we want to execute this only during DrawSceneRP:
	if(!bDuringDrawSceneRP)
		return;


	// no boat splashes on screen? - then do nothing:
	if(!g_vfxWater.GetBoatBowPtFxActive())
		return;

	DLCPushMarker("Boat Interior Splash Stencil");

	DLC_Add(CRenderPhaseCascadeShadowsInterface::SetCloudShadowParams);
	DLC_Add(CRenderPhaseCascadeShadowsInterface::SetSharedShaderVars);
	DLC_Add(CShaderLib::SetFogRayTexture);

	// reset the flag:
	g_vfxWater.SetBoatBowPtFxActive(FALSE);

	// write only to depth buffer:
	DLC( dlCmdSetStates, (grcStateBlock::RS_Default, CShaderLib::DSS_Default_Invert, grcStateBlock::BS_Default_WriteMaskNone));

	const s32	numVehicles = (s32) CVehicle::GetPool()->GetSize();
	for(s32 i=0; i<numVehicles; i++)
	{		
		CVehicle *pVehicle = CVehicle::GetPool()->GetSlot(i);
		if(pVehicle)
		{
			if(((pVehicle->GetVehicleType()==VEHICLE_TYPE_BOAT) || (pVehicle->GetVehicleType()==VEHICLE_TYPE_SUBMARINE)) && (!pVehicle->IsBaseFlagSet(fwEntity::FORCE_ALPHA)))
			{	
				spdAABB aabb;
				pVehicle->GetAABB(aabb);
			#if __BANK
				int visible = m_bUseFrustumCull ? grcViewport::IsAABBVisible(aabb.GetMin().GetIntrin128(), aabb.GetMax().GetIntrin128(), m_GBufferRenderPhaseFrustum) : 1;
			#else // __BANK
				int visible = grcViewport::IsAABBVisible(aabb.GetMin().GetIntrin128(), aabb.GetMax().GetIntrin128(), m_GBufferRenderPhaseFrustum);
			#endif // __BANK
				if (visible)
				{
					DLC ( dlCmdSetBucketsAndRenderMode, (CRenderer::RB_NOSPLASH, rmStandard) );
					fwDrawData* pDrawHandler = pVehicle->GetDrawHandlerPtr();
					if (pDrawHandler)
					{
						CDrawDataAddParams drawParams;
						drawParams.AlphaFade = 255;
						pDrawHandler->AddToDrawList(pVehicle, &drawParams);
					}
				}
			}
		}
	}

	DLCPopMarker();
}


void Water::AddToDrawListCarInteriorsNoSplashes(int drawListType)
{
	bool bDuringDrawSceneRP = FALSE;

	// check in which renderphase we are now:
	if(drawListType==DL_RENDERPHASE_DRAWSCENE)
		bDuringDrawSceneRP = TRUE;

	// we want to execute this only during DrawSceneRP:
	if(!bDuringDrawSceneRP)
		return;


	DLCPushMarker("Car Interior Splash Stencil");

	DLC_Add(CRenderPhaseCascadeShadowsInterface::SetCloudShadowParams);
	DLC_Add(CRenderPhaseCascadeShadowsInterface::SetSharedShaderVars);
	DLC_Add(CShaderLib::SetFogRayTexture);

	// write only to depth buffer:
	DLC( dlCmdSetStates, (grcStateBlock::RS_Default, CShaderLib::DSS_Default_Invert, grcStateBlock::BS_Default_WriteMaskNone));

	const s32	numVehicles = (s32) CVehicle::GetPool()->GetSize();
	for(s32 i=0; i<numVehicles; i++)
	{		
		CVehicle *pVehicle = CVehicle::GetPool()->GetSlot(i);
		if(pVehicle)
		{
			bool bRenderNoSplash = false;

			const u32 modelNameHash = pVehicle->GetBaseModelInfo()->GetModelNameHash();
			const VehicleType vehType = pVehicle->GetVehicleType();

			if((vehType==VEHICLE_TYPE_CAR) && (!pVehicle->IsBaseFlagSet(fwEntity::FORCE_ALPHA)))
			{
				if(pVehicle->DoesVehicleHaveAConvertibleRoofAnimation())
				{	
					// convertible cars with lowered roof:	
					const s32 roofState = pVehicle->GetConvertibleRoofState();
					bRenderNoSplash =	(roofState==CTaskVehicleConvertibleRoof::STATE_LOWERED)			||
										(roofState==CTaskVehicleConvertibleRoof::STATE_LOWERING)		||
										(roofState==CTaskVehicleConvertibleRoof::STATE_ROOF_STUCK_LOWERED);
				}
				else if(pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_EXTRAS_CONVERTIBLE))
				{
					// cars with removable roof as an extra:
					bRenderNoSplash = !pVehicle->CarHasRoof(false);
				}
				else if(modelNameHash==MID_TORNADO4)
				{
					bRenderNoSplash = true;
				}
			}

			if(vehType==VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE)
			{
				if((modelNameHash==MID_TECHNICAL2) || (modelNameHash==MID_APC))
				{
					bRenderNoSplash = true;
				}
			}
			else if(vehType==VEHICLE_TYPE_PLANE)
			{
				if((modelNameHash==MID_TULA) || (modelNameHash==MID_SEABREEZE))
				{
					bRenderNoSplash = true;
				}
			}

			if(bRenderNoSplash)
			{	
				spdAABB aabb;
				pVehicle->GetAABB(aabb);
			#if __BANK
				int visible = m_bUseFrustumCull ? grcViewport::IsAABBVisible(aabb.GetMin().GetIntrin128(), aabb.GetMax().GetIntrin128(), m_GBufferRenderPhaseFrustum) : 1;
			#else // __BANK
				int visible = grcViewport::IsAABBVisible(aabb.GetMin().GetIntrin128(), aabb.GetMax().GetIntrin128(), m_GBufferRenderPhaseFrustum);
			#endif // __BANK
				if (visible)
				{
					DLC ( dlCmdSetBucketsAndRenderMode, (CRenderer::RB_NOSPLASH, rmStandard) );
					fwDrawData* pDrawHandler = pVehicle->GetDrawHandlerPtr();
					if (pDrawHandler)
					{
						CDrawDataAddParams drawParams;
						drawParams.AlphaFade = 255;
						pDrawHandler->AddToDrawList(pVehicle, &drawParams);
					}
				}
			}
		}
	}

	DLCPopMarker();
}

DECLARE_MTR_THREAD s32 PreviousForcedTechniqueGroupId = -1;

void Water::BeginWaterRender()
{
	if(Unlikely(!m_WaterOcclusionQueriesCreated))
		CreateWaterOcclusionQueries();
}

void Water::EndWaterRender()
{
#if __D3D11 && RSG_PC
	if(!GRCDEVICE.IsReadOnlyDepthAllowed())
		CRenderTargets::CopyDepthBuffer(CRenderTargets::GetDepthBuffer(), CRenderTargets::GetDepthBufferCopy());
#endif //__D3D11 && RSG_PC
};

void Water::BeginRiverRender(s32 techniqueGroup, s32 underwaterLowTechniqueGroup, s32 underwaterHighTechniqueGroup, s32 singlePassTechniqueGroup, s32 queryIndex)
{
	PreviousForcedTechniqueGroupId = grmShaderFx::GetForcedTechniqueGroupId();

	if(IsCameraUnderwater())
	{
		if(m_UseHQWaterRendering[GetWaterRenderIndex()])
			grmShaderFx::SetForcedTechniqueGroupId(underwaterHighTechniqueGroup);
		else
			grmShaderFx::SetForcedTechniqueGroupId(underwaterLowTechniqueGroup);
	}
	else
	{
		if(!UseFogPrepass())
			grmShaderFx::SetForcedTechniqueGroupId(singlePassTechniqueGroup);
		else
			grmShaderFx::SetForcedTechniqueGroupId(techniqueGroup);
	}

	if(queryIndex >= 0)
		m_WaterOcclusionQueries[GPUINDEX][queryIndex].BeginQuery();
}

void Water::EndRiverRender(s32 queryIndex)
{
	grmShaderFx::SetForcedTechniqueGroupId(PreviousForcedTechniqueGroupId);

	if(queryIndex >= 0)
		m_WaterOcclusionQueries[GPUINDEX][queryIndex].EndQuery();

}

void Water::GetWaterOcclusionQueryData()
{
	if(Unlikely(!m_WaterOcclusionQueriesCreated))
	{
		CreateWaterOcclusionQueries();
		return;
	}

	s32 count = m_WaterOcclusionQueries[GPUINDEXMT].GetMaxCount();
	for(s32 i = 0; i < count; i++)
	{
		CWaterOcclusionQuery& query = m_WaterOcclusionQueries[GPUINDEXMT][i];
		query.CollectQuery();
	}
}

void WaterScreenBlit(grmShader* shader, grcEffectTechnique technique, s32 pass)
{
	if(shader->BeginDraw(grmShader::RMC_DRAW, true, technique))
	{
		shader->Bind(pass);
		grcDrawSingleQuadf(-1.0, 1.0f, 1.0f, -1.0f, 0.0f, 0.0, 0.0, 1.0f, 1.0f, Color32(0,0,0,0));
		shader->UnBind();
	}
	shader->EndDraw();
}

void Water::ProcessDepthOccluderPass()
{
#if __BANK
	if (CRenderPhaseDebugOverlayInterface::GetWaterQuadWireframeMode(m_bWireFrame) != 0)
		return;
#endif

	//[HACK GTAV] tunnel entrance hack
	float tunnelCameraDistance	= camInterface::GetPos().Dist(Vector3(-1232.2f, -934.9f, 0.0f));
	bool nearTunnelEntrance		= tunnelCameraDistance < 15.0f; 
	
	if(WaterEnabled() && !IsCameraUnderwater() && GetWaterPixelsRendered() > 16384 && (!nearTunnelEntrance) BANK_ONLY(&& m_DrawDepthOccluders))
	{
		DLC_Add(Water::BeginRenderDepthOccluders, Water::GetWaterRenderPhase()->GetEntityListIndex());
		RENDERLIST_DUMP_PASS_INFO_BEGIN("CRenderPhaseDeferredLighting_SceneToGBuffer - water");
		CRenderListBuilder::AddToDrawList(Water::GetWaterRenderPhase()->GetEntityListIndex(), CRenderListBuilder::RENDER_WATER, CRenderListBuilder::USE_NO_LIGHTING);
		RENDERLIST_DUMP_PASS_INFO_END();
		DLC_Add(Water::EndRenderDepthOccluders);
	}

	River::FillRiverDrawList();
}

#if __BANK
void Water::ResetBankVars()
{
	m_TotalDrawCallsDisplay = m_TotalDrawCalls;	
	m_HighDetailDrawCallsDisplay = m_HighDetailDrawCalls;
	m_FlatWaterQuadDrawCallsDisplay = m_FlatWaterQuadDrawCalls;
	m_WaterFLODDrawCallsDisplay = m_WaterFLODDrawCalls;

	m_TotalDrawCalls = 0;
	m_HighDetailDrawCalls = 0;
	m_FlatWaterQuadDrawCalls = 0;
	m_WaterFLODDrawCalls = 0;

#if BATCH_WATER_RENDERING
	if( m_EnableBatchWaterRendering != sBatchWaterRendering)
		sBatchWaterRendering = m_EnableBatchWaterRendering;
#endif //BATCH_WATER_RENDERING
}
#endif

void GenerateRipplesForWater()
{	
	SplashData dummySplash;
	PuddlePassSingleton::InstanceRef().GenerateRipples(DeferredLighting::GetShader(DEFERRED_SHADER_LIGHTING), dummySplash);
}

#if USE_INVERTED_PROJECTION_ONLY
static void WaterSetInvertZInProjectionMatrix(bool value)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()
	Assert(grcViewport::GetCurrent());
	grcViewport::GetCurrent()->SetInvertZInProjectionMatrix(value);
}
#endif // USE_INVERTED_PROJECTION_ONLY

void Water::ProcessWaterPass()
{
	s32 b				= GetWaterUpdateIndex();

	bool waterVisible	=  (m_WaterBlockDrawList[b].GetCount() > 0) | (River::GetWaterBucketCount() > 0) | m_BorderQuadsVisible;

#if __BANK
	// don't render water if we're viewing a gbuffer (this would overwrite GBUFFER3 on 360, we don't want to do that)
	if (DebugDeferred::m_GBufferIndex > 0)
		return;
#endif // __BANK

#if RSG_PC
	DLC_Add(DepthUpdated, false);
#endif

	// temp fix for 740469: num river drawable doesn't take swimming pools into account.
	if (WaterEnabled() && waterVisible)//looks like a hack to not render water during prologue...
	{
		DLCPushGPUTimebar(GT_WATER,"Water Bucket");
		DLCPushMarker("Water");

#if RSG_PC
		DLC_Add(DepthUpdated, true);
#endif

		//This sets water bit to on and water mask bits to on
		AddToDrawListBoatInteriorsNoWater(0);

		XENON_ONLY(DLC_Add(PostFX::SetLDR8bitHDR10bit);)

		//=================== Water Fog Prepass ========================

		DLC_Add(GetCameraRangeForRender);

#if __USEVERTEXSTREAMRENDERTARGETS
		UpdateVertexStreamRenderTargets();
#endif //__USEVERTEXSTREAMRENDERTARGETS

		DLC_Add(SetupWaterParams);

		DLC_Add(CRenderPhaseReflection::SetReflectionMap);
#if USE_INVERTED_PROJECTION_ONLY
		DLC_Add(WaterSetInvertZInProjectionMatrix,(false));
#endif // USE_INVERTED_PROJECTION_ONLY

		if(m_UseFogPrepass[b] && m_EnableWaterLighting)
		{
			DLC_Add(Water::BeginFogPrepass, GetWaterPixelsRendered());
			RENDERLIST_DUMP_PASS_INFO_BEGIN("CRenderPhaseDrawScene - water fog");
			CRenderListBuilder::AddToDrawList(m_WaterRenderPhase->GetEntityListIndex(), CRenderListBuilder::RENDER_WATER, CRenderListBuilder::USE_DEFERRED_LIGHTING);
			RENDERLIST_DUMP_PASS_INFO_END();

			DLC_Add(Water::EndFogPrepass, TIME.GetElapsedTime()/40.0f);
			Water::RenderForwardIntoRefraction();
			DLC_Add(Water::RenderRefractionAndFoam);
		}
		else
			DLC_Add(Water::SetupRefractionTexture);

		if(GetWaterPixelsRendered() > 1024)
			DLC_Add(UpdateWaterBumpTexture, GetWaterPixelsRendered());
#if RSG_PC
		else
			{
			if (GRCDEVICE.UsingMultipleGPUs())
			{
					DLC_Add(UpdatePreviousWaterBumps, false);
			}
		}
#endif

		if (m_WaterRenderPhase->GetRenderFlags() & RENDER_SETTING_RENDER_SHADOWS)
			DLC_Add(CRenderPhaseCascadeShadowsInterface::SetSharedShaderVars);

#if __XENON
		DLC_Add(River::SetWaterPassGlobalVars, TIME.GetElapsedTime(), CRenderPhaseMirrorReflectionInterface::GetIsMirrorUsingWaterReflectionTechnique_Update());
#else
		DLC_Add(River::SetWaterPassGlobalVars, TIME.GetElapsedTime(), false);
#endif

		//=================== Main Water Pass =========================
		XENON_ONLY( DLC_Add( dlCmdEnableAutomaticHiZ, true ) );

		// Reset any custom shader effect.
		DLC(CCustomShaderEffectDC, (NULL));

#if USE_INVERTED_PROJECTION_ONLY
		DLC_Add(WaterSetInvertZInProjectionMatrix,(true));
#endif // USE_INVERTED_PROJECTION_ONLY

		DLC_Add(Water::BeginWaterRender);//Setup
		//Render Ocean
		DLC(dlCmdWaterRender, (GetWaterPixelsRendered(), m_WaterRenderPhase->GetRenderFlags()));
		//Render Rivers
		
		//water_shallow, water_poolenv
		DLC_Add(Water::BeginRiverRender,	EnvTechniqueGroup, UnderwaterEnvLowTechniqueGroup, 
											UnderwaterEnvHighTechniqueGroup, SinglePassEnvTechniqueGroup, (s32)query_riverenv);

		RENDERLIST_DUMP_PASS_INFO_BEGIN("CRenderPhaseDrawScene - river env");
		CRenderListBuilder::AddToDrawList(m_WaterRenderPhase->GetEntityListIndex(), CRenderListBuilder::RENDER_WATER, CRenderListBuilder::USE_DEFERRED_LIGHTING);
		RENDERLIST_DUMP_PASS_INFO_END();
		DLC_Add(Water::EndRiverRender, (s32)query_riverenv);

		//water_river, water_riverocean, water_rivershallow, water_fountain
		DLC_Add(Water::BeginRiverRender,	-1, UnderwaterLowTechniqueGroup, 
											UnderwaterHighTechniqueGroup, SinglePassTechniqueGroup, (s32)query_riverplanar);
		RENDERLIST_DUMP_PASS_INFO_BEGIN("CRenderPhaseDrawScene - river planar");
		CRenderListBuilder::AddToDrawList(m_WaterRenderPhase->GetEntityListIndex(), CRenderListBuilder::RENDER_WATER, CRenderListBuilder::USE_DEFERRED_LIGHTING);
		RENDERLIST_DUMP_PASS_INFO_END();
		DLC_Add(Water::EndRiverRender, (s32)query_riverplanar);

		//water_riverfoam
		BANK_ONLY(if (!TiledScreenCapture::IsEnabled()))
		{
			DLC_Add(Water::BeginRiverRender,	FoamTechniqueGroup, InvalidTechniqueGroup,
												InvalidTechniqueGroup, FoamTechniqueGroup, -1);
			RENDERLIST_DUMP_PASS_INFO_BEGIN("CRenderPhaseDrawScene - river foam");
			CRenderListBuilder::AddToDrawList(m_WaterRenderPhase->GetEntityListIndex(), CRenderListBuilder::RENDER_WATER, CRenderListBuilder::USE_DEFERRED_LIGHTING);
			RENDERLIST_DUMP_PASS_INFO_END();
			DLC_Add(Water::EndRiverRender, -1);
		}

		DLC_Add(Water::EndWaterRender); // This will also trigger a depth resolve (required by the DepthFX pass)

		XENON_ONLY(DLC_Add(PostFX::SetLDR10bitHDR10bit);)


		AddToDrawListBoatInteriorsNoWater(1);
		AddToDrawListBoatInteriorsNoSplashes(m_WaterRenderPhase->GetDrawListType());

#if __PS3
		if(GetWaterPixelsRendered() > 640*360 && puddlePassOn) //Water rendering fragments Hi-Z on PS3, so refresh it for the fog and sky pass if a lot of water is being rendered
		{
			dlCmdSetStates(grcStateBlock::RS_Default, grcStateBlock::DSS_TestOnly, grcStateBlock::BS_Default_WriteMaskNone);
			DLC_Add(RenderWaterFLODDisc, (s32)Water::pass_clear, CGameWorldWaterHeight::GetWaterHeight() - 50.0f);
		}

#endif

		DLCPopMarker();
		DLCPopGPUTimebar();

#if __BANK
		DLC_Add(CRenderPhaseDebugOverlayInterface::StencilMaskOverlayRender, 2);
#endif // __BANK
	}

	AddToDrawListCarInteriorsNoSplashes(m_WaterRenderPhase->GetDrawListType());
}

void Water::ResetSimulation()
{
	m_bResetSim = true;
}

#if __GPUUPDATE
void Water::UpdateDynamicGPU(bool heightSimulationActive)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	s32	b	= GetWaterRenderIndex();

	if (!m_bUpdateDynamicWater)
		return;

	if(!m_GPUUpdate)
	{
	#if GTA_REPLAY
		// This is playback for CPU water.
		if(ms_DBVars[b].m_pReplayWaterHeightPlayback)
		{
			u32 offsetIndex = (m_WaterHeightMapIndex)%3;
		#if RSG_PC && __D3D11
			((grcTextureD3D11*)m_HeightMap[offsetIndex])->UpdateGPUCopy(0, 0, ms_DBVars[b].m_pReplayWaterHeightPlayback);
		#endif // RSG_PC && __D3D11
		#if RSG_DURANGO
			((grcTextureD3D11*)m_HeightMap[offsetIndex])->UpdateGPUCopy(0, 0, ms_DBVars[b].m_pReplayWaterHeightPlayback);
		#endif // RSG_DURANGO
		#if RSG_ORBIS
			float *pDest = (float *)((sce::Gnm::Texture *)m_HeightMap[offsetIndex]->GetTexturePtr())->getBaseAddress();
			sysMemCpy(pDest, ms_DBVars[b].m_pReplayWaterHeightPlayback, sizeof(float)*DYNAMICGRIDELEMENTS*DYNAMICGRIDELEMENTS);
		#endif // RSG_ORBIS
			s32 minX = ms_DBVars[b].m_CameraRangeMinX;
			s32 minY = ms_DBVars[b].m_CameraRangeMinY;
			float fScaledBaseOffsetX = floorf(((float)minX - (float)m_WorldBaseX) / (float)DYNAMICGRIDSIZE) / (float)DYNAMICGRIDELEMENTS;
			float fScaledBaseOffsetY = floorf(((float)minY - (float)m_WorldBaseY) / (float)DYNAMICGRIDSIZE) / (float)DYNAMICGRIDELEMENTS;
			Vector4 vUpdateOffset((float)minX, (float)minY, fScaledBaseOffsetX, fScaledBaseOffsetY);
			m_WaterTextureShader->SetVar(UpdateOffset_ID, vUpdateOffset);
		}
		else
	#endif // GTA_REPLAY
		{
		#if RSG_PC && __D3D11
			u32 offsetIndex = (m_WaterHeightMapIndex)%3;
			((grcTextureD3D11*)m_HeightMap[offsetIndex])->UpdateGPUCopy(0, 0, m_WaterHeightBufferCPU[b]);
		#endif
 			s32 worldBaseX = m_WorldBaseX;
 			s32 worldBaseY = m_WorldBaseY;

			static s32 oldWorldBaseX = 0;
			static s32 oldWorldBaseY = 0;
			float fScaledBaseOffsetX = floorf(((float)(worldBaseX - DYNAMICGRIDSIZE) - (float)(oldWorldBaseX - DYNAMICGRIDSIZE)) / (float)DYNAMICGRIDSIZE) / (float)DYNAMICGRIDELEMENTS;
			float fScaledBaseOffsetY = floorf(((float)(worldBaseY - DYNAMICGRIDSIZE) - (float)worldBaseY) / (float)(oldWorldBaseY - DYNAMICGRIDSIZE)) / (float)DYNAMICGRIDELEMENTS;
			m_WaterTextureShader->SetVar(UpdateOffset_ID, Vector4((float)worldBaseX - DYNAMICGRIDSIZE, (float)worldBaseY - DYNAMICGRIDSIZE, fScaledBaseOffsetX, fScaledBaseOffsetY));
			oldWorldBaseX = worldBaseY;
			oldWorldBaseY = worldBaseY;
		}
		return;
	}

	bool isPaused = fwTimer::IsGamePaused();

#if __D3D11 || RSG_ORBIS
	if(m_HeightGPUReadBack && !isPaused REPLAY_ONLY(&& (ms_DBVars[b].m_pReplayWaterHeightPlayback == NULL)))
	{
		u32 readIndex;
		if(m_UseMultiBuffering)
			readIndex = m_WaterHeightMapIndex;
		else
			readIndex = 0;

	#if __D3D11
		// Perform the readback on D3D11 platforms (no need on Orbis - just uses texture base pointer).
		PF_PUSH_TIMEBAR_BUDGETED("Water - Height GPU Read Back", 0.4f);
		grcTextureD3D11* heightMap11 = (grcTextureD3D11*)m_HeightMap[readIndex];
		if(heightMap11->MapStagingBufferToBackingStore(!m_UseMultiBuffering))
		{
			grcTextureLock lock;
			heightMap11->LockRect( 0, 0, lock, grcsRead );
			float* pBits = (float*)lock.Base;
			for (int col=0; col<lock.Height; col++) 
			{
				sysMemCpy( &m_WaterHeightBuffer[col][0], pBits, sizeof(float)*DYNAMICGRIDELEMENTS);
				pBits += lock.Pitch / sizeof(float);
			}
			heightMap11->UnlockRect(lock);
		}
		PF_POP_TIMEBAR();
		
		PF_PUSH_TIMEBAR_BUDGETED("Water - Velocity GPU Read Back", 0.4f);
		grcTextureD3D11* velocityMapTex11 = (grcTextureD3D11*)m_VelocityMap[readIndex];
		if(velocityMapTex11->MapStagingBufferToBackingStore(!m_UseMultiBuffering))
		{
			grcTextureLock lock;
			velocityMapTex11->LockRect( 0, 0, lock, grcsRead );
			float* pBits = (float*)lock.Base;
			for (int col=0; col<lock.Height; col++) 
			{
				sysMemCpy( &m_WaterVelocityBuffer[col][0], pBits, sizeof(float)*DYNAMICGRIDELEMENTS);
				pBits += lock.Pitch / sizeof(float);
			}
			velocityMapTex11->UnlockRect(lock);
		}
		PF_POP_TIMEBAR();		
	#endif //__D3D11

	#if GTA_REPLAY && !REPLAY_WATER_USES_TEXTURE_MEM_DIRECTLY
		// Pass back coord and height data for Replay.
		if(ms_DBVars[b].m_pReplayWaterHeightResultsToRT != NULL)
		{
			ms_DBVars[b].m_CentreCoordsResultsFromRT = m_ReplayCoordsBuffer[readIndex];
			ms_DBVars[b].m_pReplayWaterHeightResultsFromRT = ms_DBVars[b].m_pReplayWaterHeightResultsToRT; // Could use same storage for this!

		#if RSG_ORBIS
			float *pSrc = (float *)((sce::Gnm::Texture *)m_HeightMap[readIndex]->GetTexturePtr())->getBaseAddress();
			sysMemCpy(ms_DBVars[b].m_pReplayWaterHeightResultsFromRT, pSrc, sizeof(float)*DYNAMICGRIDELEMENTS*DYNAMICGRIDELEMENTS);
		#endif // RSG_ORBIS
		#if __D3D11
			if(ms_DBVars[b].m_pReplayWaterHeightResultsFromRT != &m_WaterHeightBuffer[0][0])
				sysMemCpy(ms_DBVars[b].m_pReplayWaterHeightResultsFromRT, m_WaterHeightBuffer, sizeof(float)*DYNAMICGRIDELEMENTS*DYNAMICGRIDELEMENTS);
		#endif // __D3D11
		}
	#endif // GTA_REPLAY && !REPLAY_WATER_USES_TEXTURE_MEM_DIRECTLY

	#if __D3D11
	#if RSG_PC
		if(m_UseMultiBuffering)
		{
			m_WorldBaseRTXVal = (float)m_HeightMapBaseX[readIndex];
			m_WorldBaseRTYVal = (float)m_HeightMapBaseY[readIndex];
		}
		else
	#endif //RSG_PC
		{
			PF_PUSH_TIMEBAR_BUDGETED("Water - World Base GPU Read Back", 0.4f);
			grcTextureD3D11* worldBaseTex11 = (grcTextureD3D11*)m_WorldBaseTex;
			if(worldBaseTex11->MapStagingBufferToBackingStore(!m_UseMultiBuffering))
			{
				grcTextureLock lock;
				worldBaseTex11->LockRect( 0, 0, lock, grcsRead );
				float* pBits = (float*)lock.Base;
				m_WorldBaseRTXVal = *pBits;
				m_WorldBaseRTYVal = *(pBits+1);

				worldBaseTex11->UnlockRect(lock);
			}
			PF_POP_TIMEBAR();
		}

		UpdateCachedWorldBaseCoords();
	#endif // __D3D11
	}
#endif //__D3D11 || RSG_ORBIS

	BANK_ONLY(sysTimer timer;)

	//these needs to be set for the wetmap pass regardless of height sim
	s32 minX = ms_DBVars[b].m_CameraRangeMinX;
	s32 maxX = ms_DBVars[b].m_CameraRangeMaxX;
	s32 minY = ms_DBVars[b].m_CameraRangeMinY;
	s32 maxY = ms_DBVars[b].m_CameraRangeMaxY;
	float fScaledBaseOffsetX = floorf(((float)minX - (float)m_WorldBaseX) / (float)DYNAMICGRIDSIZE) / (float)DYNAMICGRIDELEMENTS;
	float fScaledBaseOffsetY = floorf(((float)minY - (float)m_WorldBaseY) / (float)DYNAMICGRIDSIZE) / (float)DYNAMICGRIDELEMENTS;
	Vector4 vUpdateOffset((float)minX, (float)minY, fScaledBaseOffsetX, fScaledBaseOffsetY);
	m_WaterTextureShader->SetVar(UpdateOffset_ID, vUpdateOffset);
	m_WorldBaseX	= minX;
	m_WorldBaseY	= minY;


	static bool heightSimActiveLastFrame = true;

	if(!heightSimulationActive)
	{
		heightSimActiveLastFrame = heightSimulationActive;
		return;
	}

	WaterUpdateShaderParameters currentFrameShaderParams;

	bool resetSimulation = !heightSimActiveLastFrame || m_bResetSim;

	currentFrameShaderParams._resetThisFrame	= resetSimulation;

#if RSG_PC
	if (GRCDEVICE.UsingMultipleGPUs())
	{
		for (u32 drawCount = 1; drawCount < GRCDEVICE.GetGPUCount(true); drawCount++)
		{
			u32 currentReadIndex = (GRCDEVICE.GPUIndex() + drawCount) % GRCDEVICE.GetGPUCount(true);
			resetSimulation |= s_GPUWaterShaderParams[currentReadIndex]._resetThisFrame;
		}
	}
#endif

	if(resetSimulation)
	{
		for( u32 i = 0; i < WATERVELOCITYBUFFERING; i++)
		{
			if(m_VelocityMapRT[i])
			{
				grcTextureFactory::GetInstance().LockRenderTarget(0, m_VelocityMapRT[i], NULL);
#if RSG_DURANGO //WORKAROUND: clear does not work with linear targets and will only clear every other line.
				CSprite2d tonemapSprite;
				tonemapSprite.SetGeneralParams(Vector4(0,0,0,0), Vector4(0,0,0,0));
				tonemapSprite.BeginCustomList(CSprite2d::CS_BLIT_COLOUR, NULL);
				grcDrawSingleQuadf(-1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, Color32());
				tonemapSprite.EndCustomList();
#else
				GRCDEVICE.Clear(true, Color32(0, 0, 0, 0), false, grcDepthFarthest, 0);
#endif //RSG_DURANGO
				grcTextureFactory::GetInstance().UnlockRenderTarget(0);
			}
		}

		for( u32 i = 0; i < WIN32PC_SWITCH(WATERHEIGHTBUFFERING, WATERHEIGHTBUFFERING - 1); i++) //one of the height targets is for CPU only
		{
			if(m_HeightMapRT[i])
			{
				grcTextureFactory::GetInstance().LockRenderTarget(0, m_HeightMapRT[i], NULL);
#if RSG_DURANGO //WORKAROUND: clear does not work with linear targets and will only clear every other line.
				CSprite2d tonemapSprite;
				tonemapSprite.SetGeneralParams(Vector4(0,0,0,0), Vector4(0,0,0,0));
				tonemapSprite.BeginCustomList(CSprite2d::CS_BLIT_COLOUR, NULL);
				grcDrawSingleQuadf(-1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, Color32());
				tonemapSprite.EndCustomList();
#else
				GRCDEVICE.Clear(true, Color32(0, 0, 0, 0), false, grcDepthFarthest, 0);
#endif //RSG_DURANGO
				grcTextureFactory::GetInstance().UnlockRenderTarget(0);
			}
		}

		//BANK_ONLY(m_bResetSim = false;)
		m_bResetSim = false;
	}
	
	heightSimActiveLastFrame = heightSimulationActive;
	
	PF_PUSH_TIMEBAR_DETAIL("Water Height GPU Update");
	
	const CSecondaryWaterTune &secTunings = GetSecondaryWaterTunings();

	Vector3 centerCoors	= m_DynamicWater_Coords_RT[b];
#if GTA_REPLAY
	// Maintain list of centre coords for Replay.
	m_ReplayCoordsBuffer[RSG_PC ? m_WaterHeightMapIndex : 0] = centerCoors;
#endif // GTA_REPLAY

#if REPLAY_WATER_ENABLE_DEBUG
	static char debugString[256];
	sprintf(debugString, "[GPUW] POS %f %f WB %i %i", centerCoors.GetX(), centerCoors.GetY(), m_WorldBaseX, m_WorldBaseY);
	grcDebugDraw::Text(Vec2V(0.01f, 0.5f), Color32(0.0f, 0.0f, 0.0f), debugString, true, 1.5f);
#endif // WATER_ENABLE_DEBUG

	float oceanWaveMult	= Lerp(secTunings.m_OceanWaveWindScale, secTunings.m_OceanWaveAmplitude, secTunings.m_OceanWaveAmplitude*g_weather.GetWind());
	oceanWaveMult		= Clamp(oceanWaveMult, secTunings.m_OceanWaveMinAmplitude, secTunings.m_OceanWaveMaxAmplitude);
	const s32 xBorder	= 500;
	const s32 yBorder	= 1000;


	float deepScale = 0.5f;

	if(sm_GlobalWaterXmlFile==WATERXML_V_DEFAULT)
	{
		float ds0 = abs(centerCoors.x - (sm_WorldBorderXMin + sm_WorldBorderXMax)/2);
		float ds1 = (WORLD_WIDTH	+ xBorder)/2.0f;
		float ds2 = abs(centerCoors.y - (sm_WorldBorderYMin + sm_WorldBorderYMax)/2);
		float ds3 = (WORLD_HEIGHT	+ yBorder)/2.0f;
		deepScale		= 2.0f*Max(	Min(ds0, ds1)/(WORLD_WIDTH	+ xBorder),
									Min(ds2, ds3)/(WORLD_HEIGHT	+ yBorder));

		deepScale			= deepScale*deepScale;
		deepScale			= deepScale*deepScale;
	}
	else if(sm_GlobalWaterXmlFile==WATERXML_ISLANDHEIST)
	{
		dev_float ds_mult = BANK_SWITCH(sm_bkDeepOceanMult, 1.5f);
		float ds0 = ds_mult * abs(centerCoors.x - (sm_WorldBorderXMin + sm_WorldBorderXMax)/2);
		float ds1 = (WORLD_WIDTH	+ xBorder)/2.0f;
		float ds2 = ds_mult * abs(centerCoors.y - (sm_WorldBorderYMin + sm_WorldBorderYMax)/2);
		float ds3 = (WORLD_HEIGHT	+ yBorder)/2.0f;
		deepScale		= 2.0f*Max(	Min(ds0, ds1)/(WORLD_WIDTH	+ xBorder),
									Min(ds2, ds3)/(WORLD_HEIGHT	+ yBorder));
		
		dev_bool ds_exp2 = BANK_SWITCH(sm_bkDeepOceanExp2, true);
		dev_bool ds_exp4 = BANK_SWITCH(sm_bkDeepOceanExp4, true);
		if(ds_exp2)
		{
			deepScale			= deepScale*deepScale;
		}
		if(ds_exp4)
		{
			deepScale			= deepScale*deepScale;
		}
	}


	oceanWaveMult		= Lerp(deepScale, oceanWaveMult, oceanWaveMult*secTunings.m_DeepOceanScale*sm_ScriptDeepOceanScaler);

	float shoreWaveMult	= Lerp(secTunings.m_ShoreWaveWindScale, secTunings.m_ShoreWaveAmplitude, secTunings.m_ShoreWaveAmplitude*g_weather.GetWind());
	shoreWaveMult		= Clamp(shoreWaveMult, secTunings.m_ShoreWaveMinAmplitude, secTunings.m_ShoreWaveMaxAmplitude);

	currentFrameShaderParams._minX				= (float)minX;
	currentFrameShaderParams._maxX				= (float)maxX;
	currentFrameShaderParams._minY				= (float)minY;
	currentFrameShaderParams._maxY				= (float)maxY;
	currentFrameShaderParams._centerCoors		= centerCoors;
	currentFrameShaderParams._timeStep			= fwTimer::IsGamePaused()? 0.0f : fwTimer::GetTimeStep()*m_TimeStepScale;
	currentFrameShaderParams._deepScale			= deepScale;
	currentFrameShaderParams._oceanWaveMult		= oceanWaveMult;
	currentFrameShaderParams._shoreWaveMult		= shoreWaveMult;
	currentFrameShaderParams._timeFactorScale	= ((2.0f*PI)/m_WaveTimePeriod)/64.0f;
	currentFrameShaderParams._timeFactorOffset	= (fwTimer::GetTimeInMilliseconds()%m_WaveTimePeriod)*(currentFrameShaderParams._timeFactorScale);
	currentFrameShaderParams._disturbScale	    = Max(m_DisturbScale*oceanWaveMult, secTunings.m_OceanNoiseMinAmplitude);
	currentFrameShaderParams._fTimeMS		    = (float)fwTimer::GetTimeInMilliseconds();
	currentFrameShaderParams._waterRandScaled	= g_waterRand.GetRanged(0.0f, 1.0f);
	currentFrameShaderParams._waveLengthScale	= m_WaveLengthScale;
	currentFrameShaderParams._vUpdateOffset		= vUpdateOffset;
	currentFrameShaderParams._isPaused          = isPaused;
		currentFrameShaderParams._WaterDisturbBuffer = m_WaterDisturbBuffer[b];

#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		currentFrameShaderParams._waterRandScaled = m_ReplayWaterRand;
		currentFrameShaderParams._isPaused = true;
	}
	else
	{
		m_ReplayWaterRand = currentFrameShaderParams._waterRandScaled;
	}
#endif

	//Set Renderstates/
	grcStateBlock::SetDepthStencilState	(grcStateBlock::DSS_IgnoreDepth);
	grcStateBlock::SetBlendState		(grcStateBlock::BS_Default);
	grcStateBlock::SetRasterizerState	(grcStateBlock::RS_Default);
	
	u32 index;
#if RSG_PC
	if(m_UseMultiBuffering)
	{
		m_HeightMapBaseX[m_WaterHeightMapIndex] = minX;
		m_HeightMapBaseY[m_WaterHeightMapIndex] = minY;
		index = m_WaterHeightMapIndex;
		u32 sourceIndex = (index + WATERHEIGHTBUFFERING - 1)%WATERHEIGHTBUFFERING;
		m_WaterTextureShader->SetVar(m_HeightTextureVar,   m_HeightMapRT  [sourceIndex]);
		m_WaterTextureShader->SetVar(m_VelocityTextureVar, m_VelocityMapRT[sourceIndex]);
	}
	else
#endif //RSG_PC
	{
		index = 0;
		m_WaterTextureShader->SetVar(m_HeightTextureVar,   m_HeightMapRT[0]);
		m_WaterTextureShader->SetVar(m_VelocityTextureVar, m_VelocityTempRT);
	}


#if RSG_PC
	//Update Velocity Simulation -- preceding frames
	if(GRCDEVICE.UsingMultipleGPUs())
	{
		for (u32 drawCount = 1; drawCount < GRCDEVICE.GetGPUCount(true); drawCount++)
		{
			u32 currentReadIndex = (GRCDEVICE.GPUIndex() + drawCount) % GRCDEVICE.GetGPUCount(true);
			UpdateGPUVelocityAndHeight(s_GPUWaterShaderParams[currentReadIndex], index); //Early returns if the simulation was paused during that frame
		}

		//Store the current frame's shader parameters for re-draw next frame
		s_GPUWaterShaderParams[GRCDEVICE.GPUIndex()] = currentFrameShaderParams;

		GetCameraRangeForRender();
	}
#endif

	//Update Velocity Simulation -- current frame, early returns if we are currently paused this frame
	UpdateGPUVelocityAndHeight(currentFrameShaderParams, index);

	//Update the worldbase pixel
	grcTextureFactory::GetInstance().LockRenderTarget(0, m_WorldBaseRT, NULL);
	AssertVerify(m_WaterTextureShader->BeginDraw(grmShader::RMC_DRAW, true, m_WaterUpdateTechnique));
	m_WaterTextureShader->Bind(pass_updateworldbase);
	grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f,0,0.0f,0.0f,1.0f,1.0f,Color32(1.0f,1.0f,1.0f,1.0f));
	m_WaterTextureShader->UnBind();
	m_WaterTextureShader->EndDraw();
	grcTextureFactory::GetInstance().UnlockRenderTarget(0);

#if __D3D11
	if(m_HeightGPUReadBack && !isPaused)
	{
		grcTextureD3D11* heightMap11      = (grcTextureD3D11*)m_HeightMap[index];
		heightMap11->CopyFromGPUToStagingBuffer();

		grcTextureD3D11* velocityMapTex11 = (grcTextureD3D11*)m_VelocityMap[index];
		velocityMapTex11->CopyFromGPUToStagingBuffer();

		if(!m_UseMultiBuffering)
		{
			grcTextureD3D11* worldBaseTex11   = (grcTextureD3D11*)m_WorldBaseTex;
			worldBaseTex11->CopyFromGPUToStagingBuffer();
		}
	}
#endif

#if GTA_REPLAY && REPLAY_WATER_USES_TEXTURE_MEM_DIRECTLY
	if(m_HeightGPUReadBack && !isPaused REPLAY_ONLY(&& (ms_DBVars[b].m_pReplayWaterHeightPlayback == NULL)))
	{
		// Pass back coord and height data for Replay.
		if(ms_DBVars[b].m_pReplayWaterHeightResultsToRT != NULL)
		{
			ms_DBVars[b].m_CentreCoordsResultsFromRT = centerCoors;
			ms_DBVars[b].m_pReplayWaterHeightResultsFromRT = ms_DBVars[b].m_pReplayWaterHeightResultsToRT; 
			// Convert the lock base into a texture pointer.
			grcTexture *pReplayWaterDestData = CPacketCubicPatchWaterFrame::GetWaterHeightMapTexture(ms_DBVars[b].m_pReplayWaterHeightResultsFromRT);

		#if RSG_DURANGO
			pReplayWaterDestData->Copy(m_HeightMap[index]);
		#endif // RSG_DURANGO
		#if RSG_ORBIS
			((grcTextureGNM *)pReplayWaterDestData)->Copy(m_HeightMap[index], 0);
		#endif // RSG_ORBIS
		}
	}
#endif // GTA_REPLAY && REPLAY_WATER_USES_TEXTURE_MEM_DIRECTLY

	PF_POP_TIMEBAR_DETAIL();
	BANK_ONLY(m_GPUUpdateTime = timer.GetMsTime();)
}

void Water::UpdateGPUVelocityAndHeight(WaterUpdateShaderParameters& params, u32 index)
{
#if GTA_REPLAY && (RSG_DURANGO || RSG_PC || RSG_ORBIS)
	// Write Replay water height data into the water height texture.
	if(ms_DBVars[GetWaterRenderIndex()].m_pReplayWaterHeightPlayback)
	{
	#if !REPLAY_WATER_USES_TEXTURE_MEM_DIRECTLY

	#if __D3D11 
		grcTextureLock vlock;
		m_VelocityMap[index]->LockRect(0, 0, vlock, grcsWrite);
		sysMemCpy((float*)vlock.Base, m_WaterVelocityBuffer, DYNAMICGRIDELEMENTS * DYNAMICGRIDELEMENTS * sizeof(float));
		m_VelocityMap[index]->UnlockRect(vlock);

		grcTextureLock hlock;
		m_HeightMap[index]->LockRect(0, 0, hlock, grcsWrite);
		sysMemCpy((float*)hlock.Base, ms_DBVars[GetWaterRenderIndex()].m_pReplayWaterHeightPlayback, DYNAMICGRIDELEMENTS * DYNAMICGRIDELEMENTS * sizeof(float));
		m_HeightMap[index]->UnlockRect(hlock);
	#endif //__D3D11
	#if RSG_ORBIS
		// Note this method doesn't seem to work correctly on Orbis.
		float *pDest = (float *)((sce::Gnm::Texture *)m_HeightMap[index]->GetTexturePtr())->getBaseAddress();
		sysMemCpy(pDest, ms_DBVars[GetWaterRenderIndex()].m_pReplayWaterHeightPlayback, sizeof(float)*DYNAMICGRIDELEMENTS*DYNAMICGRIDELEMENTS);
	#endif // RSG_ORBIS

	#endif // !REPLAY_WATER_USES_TEXTURE_MEM_DIRECTLY
	}
#endif // GTA_REPLAY && (RSG_DURANGO || RSG_PC || RSG_ORBIS)

	if (params._isPaused)
		return;

	m_WaterTextureShader->SetVar(UpdateParams0_ID, Vector4(params._timeStep, params._fTimeMS, params._timeFactorOffset, params._disturbScale));
	m_WaterTextureShader->SetVar(UpdateParams1_ID, Vector4(params._waveLengthScale, params._oceanWaveMult, params._shoreWaveMult, params._waterRandScaled));
	m_WaterTextureShader->SetVar(UpdateParams2_ID, Vector4(params._shoreWaveMult, 0.0f, 0.0f, 0.0f));
	m_WaterTextureShader->SetVar(m_LinearWrapTextureVar, m_OceanWavesTexture);
	m_WaterTextureShader->SetVar(UpdateOffset_ID, params._vUpdateOffset);
	
	//Update velocity
	grcTextureFactory::GetInstance().LockRenderTarget(0, m_VelocityMapRT[index], NULL);
	WaterScreenBlit(m_WaterTextureShader, m_WaterUpdateTechnique, pass_updatevelocity);
	grcTextureFactory::GetInstance().UnlockRenderTarget(0);

	m_VelocityTempRT->Copy(m_VelocityMapRT[index]);
	
	m_WaterTextureShader->SetVar(m_VelocityTextureVar, m_VelocityTempRT);

	//Dynamic Velocity Mods
	WIN32PC_ONLY(BANK_ONLY(if(m_bEnableExtraQuads)))
	{
		const grcViewport* prevViewport = grcViewport::GetCurrent();
		grcViewport viewport;
		viewport.Ortho(params._minX, params._maxX, params._maxY - 1.0f, params._minY - 1.0f, 0.0f, 1.0f);

		grcViewport::SetCurrent(&viewport);
		grcViewport::SetCurrentWorldIdentity();

		atFixedArray<WaterDisturb, MAXDISTURB>* disturbBuffer	= &params._WaterDisturbBuffer;
		s32 count												= disturbBuffer->GetCount();

#if __BANK
		m_PackedDisturbCount=count;
#endif // __BANK

		float offset = DYNAMICGRIDSIZE/2.0f;

		//disturb
		if (count)
		{
			grcTextureFactory::GetInstance().LockRenderTarget(0, m_VelocityMapRT[index], NULL);
			//by assigning the height map as the child of the velocity, a valid velocity map should be in the EDRAM without me having to blit it in again.
			AssertVerify(m_WaterTextureShader->BeginDraw(grmShader::RMC_DRAW, true, m_WaterUpdateTechnique));
			m_WaterTextureShader->Bind(pass_updatedisturb);
			grcBeginQuads(count);//draw disturb pixels
			for(s32 i=0; i<count; i++)
			{
				WaterDisturb disturb	= (*disturbBuffer)[i];
				grcDrawQuadf(	disturb.m_x - offset,
					disturb.m_y - offset,
					disturb.m_x + DYNAMICGRIDSIZE - offset,
					disturb.m_y + DYNAMICGRIDSIZE - offset,
					0.0f,
					disturb.m_amount[0], disturb.m_amount[1],
					disturb.m_amount[0], disturb.m_amount[1], 
					Color32(disturb.m_change[0], disturb.m_change[1], 0.0f, 0.0f));
			}
			grcEndQuads();
			m_WaterTextureShader->UnBind();
			m_WaterTextureShader->EndDraw();
			grcTextureFactory::GetInstance().UnlockRenderTarget(0);

			m_VelocityTempRT->Copy(m_VelocityMapRT[index]);
		}

		//============================== Draw Quads =======================================
		atFixedArray <CCalmingQuad, MAXCALMINGQUADSTORENDER> calmingQuads;

		if( sm_ExtraQuadsCount > 0 )
		{
			for(int i=0;i<MAXEXTRACALMINGQUADS;i++)
			{
				if( sm_ExtraCalmingQuads[i].m_fDampening < 1.0f )
				{
					CCalmingQuad quad = sm_ExtraCalmingQuads[i];
					if(quad.minX >= params._maxX || quad.maxX <= params._minX || quad.minY >= params._maxY || quad.maxY <= params._minY)
						continue;
					calmingQuads.Append() = quad;
				}
			}
		}

		count = m_CalmingQuadsBuffer.GetCount();
		for(s32 i = 0; i < count; i++)
		{
			CCalmingQuad quad = m_CalmingQuadsBuffer[i];
			if(quad.minX >= params._maxX || quad.maxX <= params._minX || quad.minY >= params._maxY || quad.maxY <= params._minY)
				continue;
			if(Verifyf(!calmingQuads.IsFull(), "Drawing more than %d calming quads!", calmingQuads.GetMaxCount()))
				calmingQuads.Append() = quad;
		}
		s32 calmingQuadsCount = calmingQuads.GetCount();
		BANK_ONLY(m_NumCalmingQuadsDrawn = calmingQuadsCount;)

		atFixedArray <CWaveQuad, MAXWAVEQUADSTORENDER> waveQuads;
		count = m_WaveQuadsBuffer.GetCount();
		for(s32 i = 0; i < count; i++)
		{
			CWaveQuad quad = m_WaveQuadsBuffer[i];
			if(quad.minX >= params._maxX || quad.maxX <= params._minX || quad.minY >= params._maxY || quad.maxY <= params._minY)
				continue;
			if(Verifyf(!waveQuads.IsFull(), "Drawing more than %d wave quads!", waveQuads.GetMaxCount()))
				waveQuads.Append() = quad;
		}
		s32 waveQuadsCount = waveQuads.GetCount();
		BANK_ONLY(m_NumWaveQuadsDrawn = waveQuadsCount;)

		//Draw Wave Quads
		if (waveQuadsCount)
		{
			// Damp the amplitude of any wave quads that overlap script added calming quads.
			if( sm_ScriptCalmedWaveHeightScaler < 1.0f )
			{
				for(s32 i = 0; i < waveQuadsCount; i++)
				{
					for(s32 j = 0; j < MAXEXTRACALMINGQUADS; j++)
					{
						if( sm_ExtraCalmingQuads[ j ].m_fDampening < 1.0f )
						{
							CWaveQuad&		quad		= waveQuads[ i ];
							CCalmingQuad&	calmingQuad	= sm_ExtraCalmingQuads[ j ];

							if( quad.minX < calmingQuad.maxX && 
								quad.maxX > calmingQuad.minX && 
								quad.minY < calmingQuad.maxY && 
								quad.maxY > calmingQuad.minY )
							{
								quad.amp = (u16)( (float)quad.amp * sm_ScriptCalmedWaveHeightScaler );
							}
						}
					}
				}
			}
			grcTextureFactory::GetInstance().LockRenderTarget(0, m_VelocityMapRT[index], NULL);

			m_WaterTextureShader->SetVar(m_LinearWrapTextureVar, m_ShoreWavesTexture);
			AssertVerify(m_WaterTextureShader->BeginDraw(grmShader::RMC_DRAW, true, m_WaterUpdateTechnique));
			m_WaterTextureShader->Bind(pass_updatewave);
			grcBeginQuads(waveQuadsCount);
			for(s32 i = 0; i < waveQuadsCount; i++)
			{
				CWaveQuad quad = waveQuads[i];
				grcDrawQuadf(	quad.minX - offset, quad.minY - offset, quad.maxX - offset, quad.maxY - offset, quad.GetAmplitude(),
					quad.GetXDirection(), quad.GetYDirection(), quad.GetXDirection(), quad.GetYDirection(), Color32());
			}
			grcEndQuads();
			m_WaterTextureShader->UnBind();
			m_WaterTextureShader->EndDraw();

			grcTextureFactory::GetInstance().UnlockRenderTarget(0);

			m_VelocityTempRT->Copy(m_VelocityMapRT[index]);
		}

		//Draw Calming Quads
		if (calmingQuadsCount)
		{
			grcTextureFactory::GetInstance().LockRenderTarget(0, m_VelocityMapRT[index], NULL);

			AssertVerify(m_WaterTextureShader->BeginDraw(grmShader::RMC_DRAW, true, m_WaterUpdateTechnique));
			m_WaterTextureShader->Bind(pass_updatecalming);
			grcBeginQuads(calmingQuadsCount);
			for(s32 i = 0; i < calmingQuadsCount; i++)
			{
				CCalmingQuad quad = calmingQuads[i];
				grcDrawQuadf(	quad.minX - offset, quad.minY - offset, quad.maxX - offset, quad.maxY - offset, 0.0f,
					quad.m_fDampening, quad.m_fDampening, quad.m_fDampening, quad.m_fDampening, Color32());
			}
			grcEndQuads();
			m_WaterTextureShader->UnBind();
			m_WaterTextureShader->EndDraw();

			grcTextureFactory::GetInstance().UnlockRenderTarget(0);

			m_VelocityTempRT->Copy(m_VelocityMapRT[index]);
		}

		grcViewport::SetCurrent(prevViewport);
	}

	//Update Height
	grcTextureFactory::GetInstance().LockRenderTarget(0, m_HeightTempRT, NULL);
	WaterScreenBlit(m_WaterTextureShader, m_WaterUpdateTechnique, pass_updateheight);
	grcTextureFactory::GetInstance().UnlockRenderTarget(0);
	m_HeightMapRT[index]->Copy(m_HeightTempRT);
}


#else
void Water::UpdateDynamicGPU(bool)
{
#if RSG_PC && __D3D11
	u32 offsetIndex = (m_WaterHeightMapIndex)%3;
	((grcTextureD3D11*)m_HeightMap[offsetIndex])->UpdateGPUCopy(0, 0, m_WaterHeightBuffer);
#endif

	s32 worldBaseX = m_WorldBaseX;
	s32 worldBaseY = m_WorldBaseY;

	static s32 oldWorldBaseX = 0;
	static s32 oldWorldBaseY = 0;
	float fScaledBaseOffsetX = floorf(((float)(worldBaseX - DYNAMICGRIDSIZE) - (float)(oldWorldBaseX - DYNAMICGRIDSIZE)) / (float)DYNAMICGRIDSIZE) / (float)DYNAMICGRIDELEMENTS;
	float fScaledBaseOffsetY = floorf(((float)(worldBaseY - DYNAMICGRIDSIZE) - (float)worldBaseY) / (float)(oldWorldBaseY - DYNAMICGRIDSIZE)) / (float)DYNAMICGRIDELEMENTS;
	m_WaterTextureShader->SetVar(UpdateOffset_ID, Vector4((float)worldBaseX - DYNAMICGRIDSIZE, (float)worldBaseY - DYNAMICGRIDSIZE, fScaledBaseOffsetX, fScaledBaseOffsetY));
	oldWorldBaseX = worldBaseX;
	oldWorldBaseY = worldBaseY;
}
#endif //__GPUUPDATE


void Water::UpdateWetMapRender(FoamShaderParameters &foamParameters)
{
	grcStateBlock::SetDepthStencilState	(grcStateBlock::DSS_IgnoreDepth);
	grcStateBlock::SetBlendState		(grcStateBlock::BS_Default);
	grcStateBlock::SetRasterizerState	(grcStateBlock::RS_Default);

#if __PS3 || (!BUFFERED_WET_MAP_SOLUTION && (__D3D11 || RSG_ORBIS))
	m_WaterTextureShader->SetVar(m_LinearWrapTextureVar, m_WetMapRT);
	grcTextureFactory::GetInstance().LockRenderTarget(0, m_WetMapRTCopy, NULL);
	AssertVerify(m_WaterTextureShader->BeginDraw(grmShader::RMC_DRAW, true, m_WetUpdateTechnique));
	m_WaterTextureShader->Bind(pass_copywet);
	grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f,0,0.0f,0.0f,1.0f,1.0f,Color32(1.0f,1.0f,1.0f,1.0f));
	m_WaterTextureShader->UnBind();
	m_WaterTextureShader->EndDraw();
	grcTextureFactory::GetInstance().UnlockRenderTarget(0);
#endif //__PS3 || (!BUFFERED_WET_MAP_SOLUTION && (__D3D11 || RSG_ORBIS))

#if __PS3 || __D3D11 || RSG_ORBIS
	m_WaterTextureShader->SetVar(UpdateOffset_ID, foamParameters._updateOffset);
#endif //__PS3

	m_WaterTextureShader->SetVar(UpdateParams0_ID, foamParameters._updateParams0);
	m_WaterTextureShader->SetVar(m_HeightTextureVar, foamParameters._heightMap);

#if BUFFERED_WET_MAP_SOLUTION
	int b = GetWaterRenderIndex();
#if RSG_PC
	if (GRCDEVICE.UsingMultipleGPUs())
	{
		s_GPUFoamShaderParams[GRCDEVICE.GPUIndex()].m_wetmapIndex = 1 - s_GPUFoamShaderParams[GRCDEVICE.GPUIndex()].m_wetmapIndex;
		b = s_GPUFoamShaderParams[GRCDEVICE.GPUIndex()].m_wetmapIndex;
	}
#endif
	grcRenderTarget* pShaderTex		= m_WetMapRT[1 - b];
	grcRenderTarget* pRenderTarget	= m_WetMapRT[b];
#else
	grcRenderTarget* pShaderTex		= m_WetMapRTCopy;
	grcRenderTarget* pRenderTarget	= m_WetMapRT;
#endif

	m_WaterTextureShader->SetVar(m_LinearWrapTextureVar, pShaderTex);

	grcTextureFactory::GetInstance().LockRenderTarget(0, pRenderTarget, NULL);

	AssertVerify(m_WaterTextureShader->BeginDraw(grmShader::RMC_DRAW, true, m_WetUpdateTechnique));
	m_WaterTextureShader->Bind(pass_updatewet);
	grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f,0,0.0f,0.0f,1.0f,1.0f,Color32(1.0f,1.0f,1.0f,1.0f));
	m_WaterTextureShader->UnBind();

	//disturb
	atFixedArray<WaterDisturb,	MAXDISTURB>* disturbBuffer	= &foamParameters.m_disturbBuffer;
	s32 disturbCount										= disturbBuffer->GetCount();
	atFixedArray<WaterFoam,			MAXFOAM>* foamBuffer	= &foamParameters.m_foamBuffer;
	s32 foamCount											= foamBuffer->GetCount();
	s32 totalCount = disturbCount + foamCount;

	if(totalCount)
	{
		s32 minX	= foamParameters.cameraMinX;
		s32 maxX	= foamParameters.cameraMaxX;
		s32 minY	= foamParameters.cameraMinY;
		s32 maxY	= foamParameters.cameraMaxY;

		Assertf(minX < maxX, "[UpdateWetMap] Invalid x range [%d - %d]", minX, maxX);
		Assertf(minY < maxY, "[UpdateWetMap] Invalid y range [%d - %d]", minY, maxY);

		float offset = 1.0f*DYNAMICGRIDSIZE;

		const grcViewport* prevViewport = grcViewport::GetCurrent();
		grcViewport viewport;
		viewport.Ortho(	(float)minX,	(float)maxX,
			(float)maxY,	(float)minY,
			0.0f,	1.0f);

		grcViewport::SetCurrent(&viewport);

#if RSG_ORBIS && BUFFERED_WET_MAP_SOLUTION
		grcTextureFactory::GetInstance().UnlockRenderTarget(0);
		grcTextureFactory::GetInstance().LockRenderTarget(0, m_FoamAccumulationRT, NULL);
		GRCDEVICE.Clear(true, Color_black, false, 0.0, 0);
#endif //RSG_ORBIS && BUFFERED_WET_MAP_SOLUTION
		grcStateBlock::SetBlendState(m_AddFoamBlendState);

		m_WaterTextureShader->Bind(pass_addfoam);
		grcBeginQuads(totalCount);//draw disturb pixels

		float disturbFoamScale = foamParameters.m_disturbFoamScale;//GetMainWaterTunings().m_DisturbFoamScale;

		for(s32 i = 0; i < disturbCount; i++)
		{
			WaterDisturb& disturb	= (*disturbBuffer)[i];

			grcDrawQuadf(	disturb.m_x,
				disturb.m_y,
				disturb.m_x + offset,
				disturb.m_y + offset,
				0.0f,
				disturb.m_amount[0], disturb.m_amount[1],
				disturb.m_amount[0], disturb.m_amount[1], 
				Color32(disturb.m_change[0]*disturbFoamScale, disturb.m_change[1]*disturbFoamScale, 0.0f, 0.0f));
		}

		for(s32 i = 0; i < foamCount; i++)
		{
			WaterFoam& foam		= (*foamBuffer)[i];

			grcDrawQuadf(	foam.m_x,
				foam.m_y,
				foam.m_x + offset,
				foam.m_y + offset,
				0.0f,
				foam.m_amount, 0.0f,
				foam.m_amount, 0.0f, 
				Color32(255, 255, 0, 0));
		}

		grcEndQuads();
		m_WaterTextureShader->UnBind();

		grcViewport::SetCurrent(prevViewport);

#if RSG_ORBIS && BUFFERED_WET_MAP_SOLUTION
		grcTextureFactory::GetInstance().UnlockRenderTarget(0);

		grcStateBlock::SetBlendState		(grcStateBlock::BS_Default);

		m_WaterTextureShader->SetVar(m_LinearWrapTextureVar, m_WetMapRT[b]);
		m_WaterTextureShader->SetVar(m_LinearWrapTexture2Var, m_FoamAccumulation);
		grcTextureFactory::GetInstance().LockRenderTarget(0, m_WetMapRT[b], NULL);
		m_WaterTextureShader->Bind(pass_combinefoamandwetmap);
		grcDrawSingleQuadf(-1.0f,1.0f,1.0f,-1.0f,0,0.0f,0.0f,1.0f,1.0f,Color32(1.0f,1.0f,1.0f,1.0f));
		m_WaterTextureShader->UnBind();
#endif
	}
	m_WaterTextureShader->EndDraw();

	grcTextureFactory::GetInstance().UnlockRenderTarget(0);
}

void Water::UpdateCachedWorldBaseCoords()
{
	m_tWorldBaseRTX = (s32)(WATERGRIDSIZE * rage::Floorf(*m_WorldBaseRTX*WATERGRIDSIZE_INVF));
	m_tWorldBaseRTY = (s32)(WATERGRIDSIZE * rage::Floorf(*m_WorldBaseRTY*WATERGRIDSIZE_INVF));
}

void Water::UpdateWetMap()
{
	PF_PUSH_TIMEBAR_DETAIL("Water Wet Map Update");

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	GetCameraRangeForRender();

	FoamShaderParameters localFoamParameters;
#if RSG_PC
	if (GRCDEVICE.UsingMultipleGPUs())
	{
		for (u32 drawCount = 1; drawCount < GRCDEVICE.GetGPUCount(true); drawCount++)
		{

			u32 currentReadIndex = (GRCDEVICE.GPUIndex() + drawCount) % GRCDEVICE.GetGPUCount(true);

			s_GPUFoamShaderParams[currentReadIndex]._heightMap = gHeightSimulationActive ? GetWaterHeightMap() : grcTexture::NoneBlack;
			UpdateWetMapRender(s_GPUFoamShaderParams[currentReadIndex]);
		}
	}
#endif //RSG_PC

#if !__XENON
	static s32 pRCRMinX = 0, pRCRMinY = 0;
	float fScaledBaseOffsetX = floorf(((float)RenderCameraRangeMinX - (float)pRCRMinX) / (float)DYNAMICGRIDSIZE) / (float)DYNAMICGRIDELEMENTS;
	float fScaledBaseOffsetY = floorf(((float)RenderCameraRangeMinY - (float)pRCRMinY) / (float)DYNAMICGRIDSIZE) / (float)DYNAMICGRIDELEMENTS;
	m_WaterTextureShader->SetVar(UpdateOffset_ID, Vector4((float)RenderCameraRangeMinX, (float)RenderCameraRangeMinY, (float)fScaledBaseOffsetX, (float)fScaledBaseOffsetY));
	pRCRMinX = RenderCameraRangeMinX;
	pRCRMinY = RenderCameraRangeMinY;
#endif // !__XENON

	float timeStep = fwTimer::GetTimeStep();
	const CSecondaryWaterTune &secTunings = GetSecondaryWaterTunings();

	const tcKeyframeUncompressed& currKeyframe = g_timeCycle.GetCurrRenderKeyframe();
	float foamIntensity = secTunings.m_OceanFoamIntensity * currKeyframe.GetVar(TCVAR_WATER_FOAM_INTENSITY_MULT);
	
	m_WaterTextureShader->SetVar(UpdateParams0_ID, Vector4(fwTimer::IsGamePaused()?0:timeStep, foamIntensity, 0.0f, 0.0f));
	m_WaterTextureShader->SetVar(m_HeightTextureVar, gHeightSimulationActive ? GetWaterHeightMap() : grcTexture::NoneBlack);

	localFoamParameters._updateOffset = Vector4((float)RenderCameraRangeMinX, (float)RenderCameraRangeMinY, (float)fScaledBaseOffsetX, (float)fScaledBaseOffsetY);
	localFoamParameters._updateParams0 = Vector4(fwTimer::IsGamePaused()?0:timeStep, foamIntensity, 0.0f, 0.0f);
	localFoamParameters._heightMap = gHeightSimulationActive ? GetWaterHeightMap() : grcTexture::NoneBlack;

	localFoamParameters.m_disturbBuffer = m_WaterDisturbBuffer[GetWaterRenderIndex()];
		localFoamParameters.m_foamBuffer = m_WaterFoamBuffer[GetWaterRenderIndex()];

	localFoamParameters.m_disturbFoamScale = GetMainWaterTunings().m_DisturbFoamScale;

	localFoamParameters.cameraMinX = RenderCameraRangeMinX;
	localFoamParameters.cameraMaxX = RenderCameraRangeMaxX;
	localFoamParameters.cameraMinY = RenderCameraRangeMinY;
	localFoamParameters.cameraMaxY = RenderCameraRangeMaxY;

#if RSG_PC
	if (GRCDEVICE.UsingMultipleGPUs())
	{
		s32 currentIndex = s_GPUFoamShaderParams[GRCDEVICE.GPUIndex()].m_wetmapIndex;
		s_GPUFoamShaderParams[GRCDEVICE.GPUIndex()] = localFoamParameters;
		s_GPUFoamShaderParams[GRCDEVICE.GPUIndex()].m_wetmapIndex = currentIndex;
	}
#endif // RSG_PC

	UpdateWetMapRender(localFoamParameters);

	PF_POP_TIMEBAR_DETAIL();
}

#if BATCH_WATER_RENDERING
void BeginWaterBatch()
{
	const unsigned rti = g_RenderThreadIndex;
	Assert( currentBatchOffset[rti] == 0 );
	grcBegin(drawTriStrip, waterBatchSize);
	waterBatchStarted[rti] = true;
}

void EndWaterBatch()
{
	const unsigned rti = g_RenderThreadIndex;
	grcEnd(currentBatchOffset[rti]);
	currentBatchOffset[rti] = 0;
	waterBatchStarted[rti] = false;
#if __BANK
	m_TotalDrawCalls++;
#endif //__BANK
}

void IncrementBatchOffset( u32 numVertices )
{
	const unsigned rti = g_RenderThreadIndex;
	if( currentBatchOffset[rti]+numVertices >= waterBatchSize)
	{
		EndWaterBatch();
		BeginWaterBatch();
	}
	currentBatchOffset[rti] += numVertices;
}
#endif //BATCH_WATER_RENDERING

//|_  Lerp to get intersection heights but I'm going just assign the first height for now
static s32 GetTrisVertsA(float minX, float maxX, float minY, float maxY, float a1, float a2, float a3, float a4, float slope, float offset, Vector3 *points)
{
	if( maxX*slope+offset >= maxY )//slope over range, draw quad
		return 0;

	if( minX*slope+offset <= minY+0.0001f )//slope under range, don't draw anything;
		return -1;

	points[0].Set(minX, minY, a1);
	s32 numPoints	= 1;

	//add intersection points
	float yTest	= maxX*slope + offset;
	if(yTest>minY && yTest<maxY)
	{
		points[numPoints++].Set(maxX, minY, a2);
		points[numPoints++].Set(maxX, yTest, Lerp((yTest-minY)/(maxY-minY), a2, a4));
	}
	else
	{
		float xTest=(minY-offset)/slope;
		points[numPoints++].Set(xTest, minY, Lerp((xTest-minX)/(maxX-minX), a1, a2));
	}

	yTest		= minX*slope + offset;
	if(yTest>=minY && yTest<=maxY)
		points[numPoints++].Set(minX, yTest, Lerp((yTest-minY)/(maxY-minY), a1, a3));
	else
	{
		float xTest	= (maxY-offset)/slope;
		points[numPoints++].Set(xTest, maxY, Lerp((xTest-minX)/(maxX-minX), a3, a4));
		points[numPoints++].Set(minX, maxY, a3);
	}

	return numPoints;
}
//|-
static s32 GetTrisVertsB(float minX, float maxX, float minY, float maxY, float a1, float a2, float a3, float a4, float slope, float offset, Vector3 *points)
{
	if( maxX*slope+offset <= minY )//slope over range, draw quad
		return 0;

	if( minX*slope+offset+0.0001f >= maxY )//slope under range, don't draw anything;
		return -1;

	points[0].Set(minX, maxY, a3);
	s32 numPoints	= 1;


	float yTest	= minX*slope + offset;
	if(yTest>=minY && yTest<=maxY)
		points[numPoints++].Set(minX, yTest, Lerp((yTest-minY)/(maxY-minY), a1, a3));
	else
	{
		float xTest=(minY-offset)/slope;
		points[numPoints++].Set(minX, minY, a1);
		points[numPoints++].Set(xTest, minY, Lerp((xTest-minX)/(maxX-minX), a1, a2));
	}

	//add intersection points
	yTest		=maxX*slope + offset;
	if(yTest>minY && yTest<maxY)
	{
		points[numPoints++].Set(maxX, yTest, Lerp((yTest-minY)/(maxY-minY), a2, a4));
		points[numPoints++].Set(maxX, maxY, a4);
	}
	else
	{
		float xTest=(maxY-offset)/slope;
		points[numPoints++].Set(xTest, maxY, Lerp((xTest-minX)/(maxX-minX), a3, a4));
	}

	return numPoints;
}
//-|
static s32 GetTrisVertsC(float minX, float maxX, float minY, float maxY, float a1, float a2, float a3, float a4, float slope, float offset, Vector3 *points)
{
	if( minX*slope+offset <= minY )//slope over range, draw quad
		return 0;

	if( maxX*slope+offset+0.0001f >= maxY )//slope under range, don't draw anything;
		return -1;

	points[0].Set(maxX, maxY, a4);
	s32 numPoints	= 1;

	float yTest	= minX*slope+offset;
	if(yTest>minY && yTest<maxY)
	{
		points[numPoints++].Set(minX, maxY, a3);
		points[numPoints++].Set(minX, yTest, Lerp((yTest-minY)/(maxY-minY), a1, a3));
	}
	else
	{
		float xTest	= (maxY-offset)/slope;
		points[numPoints++].Set(xTest, maxY, Lerp((xTest-minX)/(maxX-minX), a3, a4));
	}

	//add intersection points
	yTest		= maxX*slope + offset;
	if(yTest>=minY && yTest<=maxY)
		points[numPoints++].Set(maxX, yTest, Lerp((yTest-minY)/(maxY-minY), a2, a4));
	else
	{
		float xTest=(minY-offset)/slope;
		points[numPoints++].Set(xTest, minY, Lerp((xTest-minX)/(maxX-minX), a1, a2));
		points[numPoints++].Set(maxX, minY, a2);
	}
	return numPoints;
}
//_|
static s32 GetTrisVertsD(float minX, float maxX, float minY, float maxY, float a1, float a2, float a3, float a4, float slope, float offset, Vector3 *points)
{
	if( minX*slope+offset >= maxY )//slope over range, draw quad
		return 0;

	if( maxX*slope+offset <= minY+0.0001f )//slope under range, don't draw anything;
		return -1;

	points[0].Set(maxX, minY, a2);
	s32 numPoints	= 1;

	float yTest	= maxX*slope + offset;
	if(yTest>=minY && yTest<=maxY)
		points[numPoints++].Set(maxX, yTest, Lerp((yTest-minY)/(maxY-minY), a2, a4));
	else
	{
		float xTest	= (maxY-offset)/slope;
		points[numPoints++].Set(maxX, maxY, a4);
		points[numPoints++].Set(xTest, maxY, Lerp((xTest-minX)/(maxX-minX), a3, a4));
	}

	//add intersection points
	yTest		= minX*slope + offset;
	if(yTest>minY && yTest<maxY)
	{
		points[numPoints++].Set(minX, yTest, Lerp((yTest-minY)/(maxY-minY), a1, a3));
		points[numPoints++].Set(minX, minY, a1);
	}
	else
	{
		float xTest	= (minY-offset)/slope;
		points[numPoints++].Set(xTest, minY, Lerp((xTest-minX)/(maxX-minX), a1, a2));
	}
	return numPoints;
}
// Renders a water rectangle as one flat poly. To be used a bit further from the camera. 
static void RenderFlatWaterRectangle(s32 MinX, s32 MaxX, s32 MinY, s32 MaxY, float z, float a1, float a2, float a3, float a4, float slope, float offset)
{
#if __BANK
	m_NumQuadsDrawn++;
#endif // __BANK

#if BATCH_WATER_RENDERING
	const unsigned rti = g_RenderThreadIndex;
#endif // BATCH_WATER_RENDERING

	float minX = MinX - MinBorder;
	float minY = MinY - MinBorder;
	float maxX = MaxX + MaxBorder;
	float maxY = MaxY + MaxBorder;
	
	if(slope!=0&&!m_TrianglesAsQuads)//do triangle render
	{	
		Vector3 points[5];
		s32 numPoints;
		if(m_CurrentTrisOrientation)//might toss this in a function pointer since it's const per rectangle.
			if(slope<0.0f)
				numPoints	= GetTrisVertsC(minX, maxX, minY, maxY, a1, a2, a3, a4, slope,offset, points);
			else
				numPoints	= GetTrisVertsD(minX, maxX, minY, maxY, a1, a2, a3, a4, slope,offset, points);
		else
			if(slope<0.0f)
				numPoints	= GetTrisVertsA(minX, maxX, minY, maxY, a1, a2, a3, a4, slope,offset, points);
			else
				numPoints	= GetTrisVertsB(minX, maxX, minY, maxY, a1, a2, a3, a4, slope,offset, points);

		if(numPoints<0)
			return;

		if(numPoints>0)
		{
			for(s32 i=0; i<numPoints; i++)
			{
				points[i].x-=RenderCameraRelX;
				points[i].y-=RenderCameraRelY;
			}


			grcDrawMode drawMode = drawTriFan;

#if BATCH_WATER_RENDERING
			if( sBatchWaterRendering && waterBatchStarted[rti] )
			{
				IncrementBatchOffset(numPoints + 2);				
				drawMode = drawTriStrip;
			}
			else
#endif //BATCH_WATER_RENDERING
				grcBegin(drawMode,numPoints);


			for(int i=0; i<numPoints; i++)
			{
				int index = i;
#if BATCH_WATER_RENDERING
				if( sBatchWaterRendering && waterBatchStarted[rti] )
				{
					if( i % 2 != 0 )
						index = i / 2;
					else
						index = numPoints - 1 - i / 2;
				}
#endif //BATCH_WATER_RENDERING

				float a = points[index].z;
				grcVertex(points[index].x, points[index].y, z,0.0f,0.0f,1.0f, Color32(a, a, a, a),0.0f,0.0f);

#if BATCH_WATER_RENDERING
				if( sBatchWaterRendering && waterBatchStarted[rti] )
				{
					if( (i == 0) || i == numPoints - 1)
						grcVertex(points[index].x, points[index].y, z,0.0f,0.0f,1.0f, Color32(a, a, a, a),0.0f,0.0f);
				}
#endif //BATCH_WATER_RENDERING
			}
#if BATCH_WATER_RENDERING
			if( !sBatchWaterRendering || !waterBatchStarted[rti] )
#endif //BATCH_WATER_RENDERING
			{
				grcEnd();

#if __BANK
				m_TotalDrawCalls++;
				m_FlatWaterQuadDrawCalls++;
#endif //__BANK
			}

			return;
		}
	}

	minX	-= RenderCameraRelX;
	maxX	-= RenderCameraRelX;
	minY	-= RenderCameraRelY;
	maxY	-= RenderCameraRelY;

	//do a quad render
#if BATCH_WATER_RENDERING
	if( sBatchWaterRendering && waterBatchStarted[rti] )
	{
		IncrementBatchOffset(6);
		grcVertex((float)minX,(float)minY, z, 0.0f,0.0f,1.0f, Color32(a1, a1, a1, a1), 0.0f, 0.0f);
	}
	else
#endif //BATCH_WATER_RENDERING
		grcBegin(drawTriStrip,4);

	grcVertex((float)minX,(float)minY, z, 0.0f,0.0f,1.0f, Color32(a1, a1, a1, a1), 0.0f, 0.0f);
	grcVertex((float)maxX,(float)minY, z, 0.0f,0.0f,1.0f, Color32(a2, a2, a2, a2), 0.0f, 0.0f);
	grcVertex((float)minX,(float)maxY, z, 0.0f,0.0f,1.0f, Color32(a3, a3, a3, a3), 0.0f, 0.0f);
	grcVertex((float)maxX,(float)maxY, z, 0.0f,0.0f,1.0f, Color32(a4, a4, a4, a4), 0.0f, 0.0f);

#if BATCH_WATER_RENDERING
	if( sBatchWaterRendering && waterBatchStarted[rti] )
		grcVertex((float)maxX,(float)maxY, z, 0.0f,0.0f,1.0f, Color32(a4, a4, a4, a4), 0.0f, 0.0f);
	else
#endif //BATCH_WATER_RENDERING
	{
		grcEnd();
#if __BANK
		m_TotalDrawCalls++;
		m_FlatWaterQuadDrawCalls++;
#endif //__BANK
	}
}

// This keep us from going insane : they all seem to be calling each other...
static void RenderWaterRectangle(s32 minX, s32 maxX, s32 minY, s32 maxY, float z, float a1 = 1.0f, float a2 = 1.0f, float a3 = 1.0f, float a4 = 1.0f, float slope=0.0f, float offset=0.0f);

// Takes a water rectangle and splits it up along a line with constant X
static void SplitWaterRectangleAlongXLine(s32 splitX, s32 minX, s32 maxX, s32 minY, s32 maxY, float z, float a1, float a2, float a3, float a4, float slope=0.0f, float offset=0.0f)
{
	Assertf(minX < splitX && maxX > splitX, "SplitX: %d out of range [%d, %d]", splitX, minX, maxX);		// Otherwise there is no point

	float I = float(splitX - minX) / (maxX - minX);

	RenderWaterRectangle(minX, splitX, minY, maxY, z, a1, Lerp(I, a1, a2), a3, Lerp(I, a3, a4), slope, offset);
	RenderWaterRectangle(splitX, maxX, minY, maxY, z, Lerp(I, a1, a2), a2, Lerp(I, a3, a4), a4, slope, offset);
}


// Takes a water rectangle and splits it up along a line with constant Y
static void SplitWaterRectangleAlongYLine(s32 splitY, s32 minX, s32 maxX, s32 minY, s32 maxY, float z, float a1, float a2, float a3, float a4, float slope=0.0f, float offset=0.0f)
{
	Assertf((minY < splitY && maxY > splitY) || (minY > splitY && maxY < splitY), "SplitY: %d out of range [%d, %d]", splitY, minY, maxY);		// Otherwise there is no point

	float I = float(splitY - minY) / (maxY - minY);

	RenderWaterRectangle(minX, maxX, minY, splitY, z, a1, a2, Lerp(I, a1, a3), Lerp(I, a2, a4), slope, offset);
	RenderWaterRectangle(minX, maxX, splitY, maxY, z, Lerp(I, a1, a3), Lerp(I, a2, a4), a3, a4, slope, offset);
}

// Will render a water rectangle in high detail (split it up in 2x2 meter triangles
#if __USEVERTEXSTREAMRENDERTARGETS
static void RenderHighDetailWaterRectangle_OneLayer(s32 MinX, s32 MaxX, s32 MinY, s32 MaxY, float z, float slope, float offset)
#else
static void RenderHighDetailWaterRectangle_OneLayer(s32 MinX, s32 MaxX, s32 MinY, s32 MaxY, float slope, float offset)
#endif
{
	Assert(MinX <= MaxX);
	Assert(MinY <= MaxY);
	
	s16 gridSize		= (s16)GridSize;//the final gridsize used is customizable and not hardcoded to DYNAMICGRIDSIZE
	s16 xSizeInPolys	= (s16)(MaxX - MinX + gridSize - 1)/gridSize;
	s16 ySizeInPolys	= (s16)(MaxY - MinY + gridSize - 1)/gridSize;
	Assert(ySizeInPolys < MAXVERTICESINSTRIP-1);

	if(slope!=0&&!m_TrianglesAsQuads)//do triangle render
	{
		if(m_CurrentTrisOrientation)
			if(slope<0.0f)
			{
				if( MaxX*slope+offset > (float)MaxY)//C
					return;
			}
			else
			{
				if( MaxX*slope+offset+0.001f < (float)MinY)//D
					return;
			}
		else
			if(slope<0.0f)
			{
				if( MinX*slope+offset+0.001f  < (float)MinY)//A
					return;
			}
			else
			{
				if( MinX*slope+offset > (float)MaxY)//B
					return;
			}
	}

	s32 baseX = RenderCameraRangeMinX;
	s32 baseY = RenderCameraRangeMinY;
	s32 blockX = (MinX - baseX)/(BLOCKQUADSWIDTH*DYNAMICGRIDSIZE);
	s32 blockY = (MinY - baseY)/(BLOCKQUADSWIDTH*DYNAMICGRIDSIZE);

#if __USEVERTEXSTREAMRENDERTARGETS
	if(FillDrawList)
	{
		Assert(xSizeInPolys <= BLOCKQUADSWIDTH);
		Assert(ySizeInPolys <= BLOCKQUADSWIDTH);
		Assert(blockX == (MaxX - baseX - DYNAMICGRIDSIZE)/(BLOCKQUADSWIDTH*DYNAMICGRIDSIZE));
		Assert(blockY == (MaxY - baseY - DYNAMICGRIDSIZE)/(BLOCKQUADSWIDTH*DYNAMICGRIDSIZE));
		m_VertexBlockDrawList[blockX + blockY*BLOCKSWIDTH].m_Draw	= true;
		m_VertexBlockDrawList[blockX + blockY*BLOCKSWIDTH].m_Height	= z;
		return;
	}
#endif //__USEVERTEXSTREAMRENDERTARGETS

	if(	m_Use16x16Block &&
		GridSize == DYNAMICGRIDSIZE &&//not sure this check is necessary, when you modify the tessellation detail, the subsection should never be 32x32
		xSizeInPolys == ySizeInPolys && 
		xSizeInPolys == BLOCKQUADSWIDTH &&
		(MinX - baseX)%(BLOCKQUADSWIDTH*DYNAMICGRIDSIZE) == 0 &&
		(MinY - baseY)%(BLOCKQUADSWIDTH*DYNAMICGRIDSIZE) == 0) //Use Tristrip buffer to render
	{
		s32 blockIndex	= blockX + blockY*BLOCKSWIDTH;
		Assertf(blockIndex >=0 && blockIndex <BLOCKSWIDTH*BLOCKSWIDTH, "block index %d invalid MinX: %d MinY: %d RXY: %d %d", 
				blockIndex, MinX, MinY, RenderCameraRelX, RenderCameraRelY);
		s32 offset = blockIndex*VERTSPERBLOCK;

		//GRCDEVICE.SetIndices(*m_WaterIndexBuffer);
		GRCDEVICE.SetStreamSource(0, *m_WaterVertexBuffer, 0, m_WaterVertexBuffer->GetVertexStride());
		//GRCDEVICE.DrawIndexedPrimitive(drawTriStrip, offset, VERTSPERBLOCK);
		GRCDEVICE.DrawPrimitive(drawTriStrip, offset, VERTSPERBLOCK);
		GRCDEVICE.ClearStreamSource(0);
		BANK_ONLY(m_16x16Drawn++);
#if __BANK
		m_TotalDrawCalls++;
		m_HighDetailDrawCalls++;
#endif //__BANK

		return;
	}

	s16 rMinX		= (s16)(MinX - RenderCameraRelX);
	s16 rMinY		= (s16)(MinY - RenderCameraRelY);

	s32 numVerts	= (xSizeInPolys + 1)*(ySizeInPolys)*2 + (ySizeInPolys)*2;
	s16 xSize		= rMinX + (s16)xSizeInPolys*gridSize + gridSize;
	s16 ySize		= rMinY + (s16)ySizeInPolys*gridSize;
	s32 i			= 0;

	if(!numVerts) // avoid read violation inside dx11
		return;

	WaterVertex* waterVertices = (WaterVertex*)GRCDEVICE.BeginVertices(drawTriStrip, numVerts, sizeof(WaterVertex));

	for (s16 y = rMinY; y < ySize; y += gridSize)
	{
		s16 y1		= y + gridSize;
		s16 x		= rMinX;
			
		waterVertices[i].x		= x;
		waterVertices[i].y		= y;
		i++;

		for (; x < xSize; x += gridSize)
		{
			waterVertices[i].x		= x;
			waterVertices[i].y		= y;
			waterVertices[i+1].x	= x;
			waterVertices[i+1].y	= y1;
			i						+= 2;
		}

		waterVertices[i].x	= x - gridSize;
		waterVertices[i].y	= y1;
		i++;
	}

	GRCDEVICE.EndVertices();

#if __BANK
	m_TotalDrawCalls++;
	m_HighDetailDrawCalls++;
#endif
}


// Will render a water rectangle in high detail (split it up in 2x2 meter triangles
static void RenderHighDetailWaterRectangle(s32 minX, s32 maxX, s32 minY, s32 maxY, float z, 
										   float aMinX, float aMaxX, float aMinY, float aMaxY, 
										   float slope, float offset)
{
	Vector3 minV = Vector3(minX - 3.0f, minY - 3.0f, z - 3.0f);
	Vector3 maxV = Vector3(maxX + 3.0f, maxY + 3.0f, z + 3.0f);
	const grcViewport* vp = grcViewport::GetCurrent();

	if (!vp->IsAABBVisible(minV, maxV, vp->GetCullFrustumLRTB()))		
		return;

#if __BANK
	m_HighDetailDrawn=true;
#endif // __BANK

		// Calculate the number of polys we will need for this water triangle.
	s32 xSize = (maxX - minX);
	s32 ySize = (maxY - minY);
	
	s32 baseX	=  RenderCameraRangeMinX;
	s32 baseY	=  RenderCameraRangeMinY;
	s32 minXBlockX	= (minX - baseX)/(BLOCKQUADSWIDTH*DYNAMICGRIDSIZE);
	s32 minYBlockY	= (minY - baseY)/(BLOCKQUADSWIDTH*DYNAMICGRIDSIZE);


	bool splitX;
	bool splitY;
	
#if __USEVERTEXSTREAMRENDERTARGETS
	if(m_UseVSRT) //For VSRT, quads must be block aligned so they must be split if they're not regardless if they are smaller than a block
	{
		s32 maxXBlockX	= (maxX - baseX - DYNAMICGRIDSIZE)/(BLOCKQUADSWIDTH*DYNAMICGRIDSIZE);
		s32 maxYBlockY	= (maxY - baseY - DYNAMICGRIDSIZE)/(BLOCKQUADSWIDTH*DYNAMICGRIDSIZE);
		splitX = maxXBlockX > minXBlockX;
		splitY = maxYBlockY > minYBlockY;
	}
	else
#endif //__USEVERTEXSTREAMRENDERTARGETS
	{
		splitX = (xSize > BLOCKQUADSWIDTH*DYNAMICGRIDSIZE) && (xSize > ySize);
		splitY = (ySize > BLOCKQUADSWIDTH*DYNAMICGRIDSIZE);
	}

	if (splitX)
	{
		SplitWaterRectangleAlongXLine(baseX + (minXBlockX + 1)*BLOCKQUADSWIDTH*DYNAMICGRIDSIZE, minX, maxX, minY, maxY, z, aMinX, aMinY, aMaxX, aMaxY, slope, offset);
		return;
	}

	if (splitY)
	{
		SplitWaterRectangleAlongYLine(baseY + (minYBlockY + 1)*BLOCKQUADSWIDTH*DYNAMICGRIDSIZE, minX, maxX, minY, maxY, z, aMinX, aMinY, aMaxX, aMaxY, slope, offset);
		return;
	}

#if __USEVERTEXSTREAMRENDERTARGETS
	RenderHighDetailWaterRectangle_OneLayer(minX, maxX, minY, maxY, z, slope, offset);
#else
	RenderHighDetailWaterRectangle_OneLayer(minX, maxX, minY, maxY, slope, offset);
#endif
}

void Water::SetWorldBase()
{
#if RSG_ORBIS || (__WIN32PC && __D3D11) || RSG_DURANGO
#if __GPUUPDATE
	grcEffect::SetGlobalVar(m_WaterWorldBaseVSVar, Vec2V((float)RenderCameraRangeMinX, (float)RenderCameraRangeMinY));			
#else
	grcEffect::SetGlobalVar(m_WaterWorldBaseVSVar, Vec2V((float)m_WorldBaseX, (float)m_WorldBaseY));
#endif
#else
	grcEffect::SetGlobalVar(m_WaterWorldBaseVSVar, Vec2V((float)RenderCameraRangeMinX, (float)RenderCameraRangeMinY));
#endif
}

void Water::SetWaterRenderPhase(const CRenderPhaseScanned* renderPhase)
{
	m_WaterRenderPhase = renderPhase;
}
const CRenderPhaseScanned* Water::GetWaterRenderPhase()
{
	return m_WaterRenderPhase;
}

// Will render a water triangle in high detail or as a flat poly depending on the distance
//			  to the camera.
static void RenderWaterRectangle(s32 minX, s32 maxX, s32 minY, s32 maxY, float z, 
								 float aMinX, float aMaxX, float aMinY, float aMaxY, 
								 float slope, float offset)
{
	// Do some easy checks that would make it so that we don't have to split the square up.
	if ((minX >= RenderCameraRangeMaxX) || (maxX <= RenderCameraRangeMinX))
	{
		if(!RenderHighDetail)
			RenderFlatWaterRectangle(minX, maxX, minY, maxY, z, aMinX, aMaxX, aMinY, aMaxY, slope, offset);
		return;
	}


	if ((minY >= RenderCameraRangeMaxY) || (maxY <= RenderCameraRangeMinY))
	{
		if(!RenderHighDetail)
			RenderFlatWaterRectangle(minX, maxX, minY, maxY, z, aMinX, aMaxX, aMinY, aMaxY, slope, offset);
		return;
	}

	// Test whether we will have to split up the rectangle into smaller ones.
	// First look at the MaxX line
	if ((minX < RenderCameraRangeMaxX) && (maxX > RenderCameraRangeMaxX))
	{
		SplitWaterRectangleAlongXLine(RenderCameraRangeMaxX, minX, maxX, minY, maxY, z, aMinX, aMaxX, aMinY, aMaxY, slope, offset);
		return;
	}

	if ((minX < RenderCameraRangeMinX) && (maxX > RenderCameraRangeMinX))
	{
		SplitWaterRectangleAlongXLine(RenderCameraRangeMinX, minX, maxX, minY, maxY, z, aMinX, aMaxX, aMinY, aMaxY, slope, offset);
		return;
	}

	if ((minY < RenderCameraRangeMinY) && (maxY > RenderCameraRangeMinY))
	{
		SplitWaterRectangleAlongYLine(RenderCameraRangeMinY, minX, maxX, minY, maxY, z, aMinX, aMaxX, aMinY, aMaxY, slope, offset);
		return;
	}

	if ((minY < RenderCameraRangeMaxY) && (maxY > RenderCameraRangeMaxY))
	{
		SplitWaterRectangleAlongYLine(RenderCameraRangeMaxY, minX, maxX, minY, maxY, z, aMinX, aMaxX, aMinY, aMaxY, slope, offset);
		return;
	}

	// What is left should be rendered in high detail.
	if(RenderHighDetail)
	{
		if(TessellateHighDetail)
			RenderHighDetailWaterRectangle(minX, maxX, minY, maxY, z, aMinX, aMaxX, aMinY, aMaxY, slope, offset);
		else
			RenderFlatWaterRectangle(minX, maxX, minY, maxY, z, aMinX, aMaxX, aMinY, aMaxY, slope, offset);
	}
}

grcTexture*	Water::GetWaterHeightMap()
{
#if GTA_REPLAY && REPLAY_WATER_USES_TEXTURE_MEM_DIRECTLY
	if(ms_DBVars[GetWaterRenderIndex()].m_pReplayWaterHeightPlayback)
	{
		return CPacketCubicPatchWaterFrame::GetWaterHeightMapTexture(ms_DBVars[GetWaterRenderIndex()].m_pReplayWaterHeightPlayback);
	}
#endif // GTA_REPLAY && REPLAY_WATER_USES_TEXTURE_MEM_DIRECTLY

#if __GPUUPDATE
	if(m_GPUUpdate && !m_UseMultiBuffering)
		return m_HeightMap[0];
	else
#endif //__GPUUPDATE
		return m_HeightMap[m_WaterHeightMapIndex];
}

grcRenderTarget* Water::GetWaterBumpHeightRT()
{
	return m_BumpHeightRT[GetWaterRenderIndex()];
}

grcRenderTarget* Water::GetWaterFogTexture()
{
	return m_LightingRT;
}

grcTexture* Water::GetBumpTexture()
{
	return m_BumpTexture;
}

grcRenderTarget* Water::GetReflectionRT()
{
	return m_ReflectionRT;
}

#if RSG_PC
grcRenderTarget* Water::GetMSAAReflectionRT()
{
	return m_MSAAReflectionRT;
}
#endif

float Water::GetWaterOpacityUnderwaterVfx()
{
	return m_WaterOpacityUnderwaterVfx;
}
float Water::GetCameraWaterHeight()
{
	return m_CameraWaterHeight[GetWaterCurrentIndex()];
}

float Water::GetCameraWaterDepth()
{
	return m_CameraWaterDepth[GetWaterCurrentIndex()];
}

bool Water::IsCameraUnderwater()
{
	return m_IsCameraUnderWater[GetWaterCurrentIndex()] BANK_ONLY(&& !m_DisableUnderwater);
}

bool Water::WaterEnabled()
{
#if __BANK
	if(CObjectProfiler::IsActive())
		return false;		// Don't draw water while profiling objects.
	if(m_bForceWaterReflectionON)
		return true;
	if(m_bForceWaterReflectionOFF || PARAM_noWaterUpdate.Get())
		return false;
#endif	//BANK

	const tcKeyframeUncompressed& currKeyframe = g_timeCycle.GetCurrUpdateKeyframe();
	const float waterReflection = currKeyframe.GetVar(TCVAR_WATER_REFLECTION);
	return waterReflection > 0.01f;
}

bool Water::UseFogPrepass()
{
	return m_UseFogPrepass[GetWaterCurrentIndex()];
}

bool Water::UseHQWaterRendering()
{
	return m_UseHQWaterRendering[GetWaterCurrentIndex()];
}

bool Water::UsePlanarReflection()
{
	return m_UsePlanarReflection;
}

float* Water::GetHeightMapData()
{
	return m_WaterHeightBuffer[0];
}

float* Water::GetVelocityMapData()
{
	return m_WaterVelocityBuffer[0];
}

void Water::DoReplayWaterSimulate(bool b)
{
	m_doReplayWaterSimulate = b;
}

u32 Water::GetWaterPixelsRendered(int queryType, bool bIgnoreCameraCuts)
{
	Assertf(sysThreadType::IsUpdateThread(), "Grabbing water pixels rendered not on the update thread is not supported!");
	s32 b = GetWaterUpdateIndex();

#if __BANK
	if (m_bForceWaterPixelsRenderedON)
		return WATEROCCLUSIONQUERY_MAX;
	if (m_bForceWaterPixelsRenderedOFF)
		return 0;
#endif // __BANK

	if (Unlikely(m_IsCameraCutting[b] || m_IsCameraDiving[b]))
	{
		if (!bIgnoreCameraCuts)
			return WATEROCCLUSIONQUERY_MAX;
	}

	if (queryType >= 0)
	{
		Assert(queryType < m_WaterOcclusionQueries[GPUINDEXMT].GetMaxCount());
		return m_WaterOcclusionQueries[GPUINDEXMT][queryType].m_PixelsRenderedRead;
	}

	return m_WaterPixelsRendered;
}

u32 Water::GetWaterPixelsRenderedOcean()
{
	u32 pixels = 0;
	for(s32 i = query_oceanstart; i <= query_oceanend; i++)
		pixels += m_WaterOcclusionQueries[GPUINDEXMT][i].m_PixelsRenderedRead;
	return pixels;
}

u32 Water::GetWaterPixelsRenderedPlanarReflective(bool bIgnoreCameraCuts)
{
	return GetWaterPixelsRenderedOcean() + GetWaterPixelsRendered(query_riverplanar, bIgnoreCameraCuts);
}

float Water::GetOceanHeight(int queryType)
{
	if (queryType >= query_oceanstart && (queryType - query_oceanstart) < m_OceanQueryHeights.GetCount())
		return m_OceanQueryHeights[queryType - query_oceanstart];

	return 0.0f;
}

bool Water::ShouldDrawCaustics()
{
	return GetWaterPixelsRendered() >= m_WaterPixelCountThreshold && !IsCameraUnderwater();
}

void Water::SetReflectionRT(grcRenderTarget* rt){ m_ReflectionRT = rt; }
#if RSG_PC
void Water::SetMSAAReflectionRT(grcRenderTarget* rt){ m_MSAAReflectionRT = rt; }
#endif

void Water::SetUnderwaterRefractionIndex(float index){ m_RefractionIndex = index; }
float Water::GetUnderwaterRefractionIndex(){ return m_RefractionIndex; }
float Water::GetWaterTimeStepScale(){ return m_TimeStepScale; }

const grcTexture* Water::GetWetMap(){
#if BUFFERED_WET_MAP_SOLUTION
#if RSG_PC
	if (GRCDEVICE.UsingMultipleGPUs())
	{
		return m_WetMap[s_GPUFoamShaderParams[GRCDEVICE.GPUIndex()].m_wetmapIndex];
	}
#endif
	return m_WetMap[GetWaterRenderIndex()]; 
#else
	return m_WetMap; 
#endif
}

void Water::SetReflectionMatrix(const Mat44V& matrix){ m_ReflectionMatrix[GetWaterUpdateIndex()] = matrix; }

const CMainWaterTune& Water::GetMainWaterTunings(){ return m_MainWaterTunings; }
const CSecondaryWaterTune& Water::GetSecondaryWaterTunings(){ return m_SecondaryWaterTunings; }
void Water::SetRippleWindValue(float wind){ m_RippleWind = wind; }
#if GTA_REPLAY
const Vector3& Water::GetCurrentWaterPos() { return m_ReplayWaterPos; }
const float& Water::GetWaterRand() { return m_ReplayWaterRand; }
float *Water::GetWaterHeightBufferForRecording() { return m_pReplayWaterBufferForRecording; }

#endif
void Water::SetSecondaryWaterTunings(const CSecondaryWaterTune& newOne) {  m_SecondaryWaterTunings = newOne; }
#if GTA_REPLAY
void Water::SetCurrentWaterPos(const Vector3& pos) { m_ReplayWaterPos = pos; }
void Water::SetCurrentWaterRand(float rand) { m_ReplayWaterRand = rand; }
#endif
grcRenderTarget* Water::GetRefractionRT(){	return m_RefractionSource; }
#if __PS3
grcRenderTarget* Water::GetFogDepthRT(){ return m_FogDepthRT; }
#endif //__PS3

void DownsampleBackbuffer()
{
	grcTextureFactory::GetInstance().LockRenderTarget(0, m_RefractionRT, NULL);

	grcStateBlock::SetStates(grcStateBlock::RS_Default, grcStateBlock::DSS_Default, grcStateBlock::BS_Default);
	m_WaterTextureShader->SetVar(m_LinearTextureVar,
#if DEVICE_MSAA
		GRCDEVICE.GetMSAA()>1 ? CRenderTargets::GetBackBufferCopy(true) : CRenderTargets::GetBackBuffer()
#else
		CRenderTargets::GetBackBuffer()
#endif
		);

	WaterScreenBlit(m_WaterTextureShader, m_DownSampleTechnique, pass_underwaterdownsamplesoft);

	grcTextureFactory::GetInstance().UnlockRenderTarget(0);

}
void Water::SetupRefractionTexture()
{
	PF_PUSH_TIMEBAR_DETAIL("Water Refraction Setup");
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	if(m_UseHQWaterRendering[GetWaterRenderIndex()] && IsCameraUnderwater())
	{
		CRenderTargets::UnlockSceneRenderTargets();

		DownsampleBackbuffer();
		
		//If camera is below water then render all above water ptfx into 
		//refraction texture
		//TODO: Re-enable when fixing underwater particles
#if 0
		if(IsCameraUnderwater())
		{
			//downsample depth before rendering the particles
			PF_PUSH_TIMEBAR_DETAIL("PtFx Above Water Refraction");

			CRenderTargets::DownsampleDepth();

			grcTextureFactory::GetInstance().LockRenderTarget(0, m_DownSampleRT, CRenderTargets::GetDepthBufferQuarter());


			g_ptFxManager.Render(
				m_RefractionRT,
				m_RefractionRT,
				true);

			grcTextureFactory::GetInstance().UnlockRenderTarget(0);

			PF_POP_TIMEBAR_DETAIL();
		}
#endif

		CRenderTargets::LockSceneRenderTargets();

		m_RefractionSource = m_RefractionRT;

	}
	else
	{	
#if DEVICE_MSAA
		//EQAA FIXME
		if (0 && GRCDEVICE.GetMSAA())	//do we really need to resolve the light buffer now?
		{
			CRenderTargets::UnlockSceneRenderTargets();
			m_RefractionSource = CRenderTargets::GetBackBufferCopy(true);
			CRenderTargets::LockSceneRenderTargets();
		}else
		{
#if RSG_PC
			CRenderTargets::UnlockSceneRenderTargets();

			DownsampleBackbuffer();

			//========================== Render Foam RT ==============================
			m_WaterTextureShader->SetVar(m_PointTextureVar, GetWetMap());
			grcTextureFactory::GetInstance().LockRenderTarget(0, m_FoamRT, NULL);
			WaterScreenBlit(m_WaterTextureShader, m_WetUpdateTechnique, pass_foamintensity);
			grcTextureFactory::GetInstance().UnlockRenderTarget(0);
			//================================================================================

			CRenderTargets::LockSceneRenderTargets();
#endif

			m_RefractionSource = RSG_PC? m_RefractionRT : CRenderTargets::GetBackBuffer();
		}
#else
		m_RefractionSource = XENON_SWITCH(CRenderTargets::GetBackBuffer7e3toInt(false), CRenderTargets::GetBackBuffer());
#endif
	}

	PF_POP_TIMEBAR_DETAIL();
}


static bool SetFogTexture(s32 worldX, s32 worldY, s32 b)
{
	s32 blockX	= WATERWORLDTOBLOCKX(worldX);
	s32 blockY	= WATERWORLDTOBLOCKY(worldY);
	s32 gridX = blockX - RenderCameraBlockX + FOGTEXTUREBLOCKSPAN;
	s32 gridY = blockY - RenderCameraBlockY + FOGTEXTUREBLOCKSPAN;
	s32 index = gridY + gridX*FOGTEXTUREBLOCKWIDTH;
	
	b = m_WaterFogIndex[b];

	bool fogParamsChanged = false;

	if( (m_FogTextureData[b].GetCount() > 0)  && Verifyf(index >=0 && index < m_FogTextureData[b].GetCount(), "Water fog index out of range! [%d]", index))
	{
		if(m_FogTextureData[b][index].loaded)//Use High Detail Fog Texture
		{
			strLocalIndex fogTextureSlot = strLocalIndex(m_FogTextureData[b][index].txdSlot);
			Assert(g_TxdStore.HasObjectLoaded(fogTextureSlot));

#if BATCH_WATER_RENDERING
			Vec4V fogParams((float)fogTextureSlot.Get(), (float)worldX, (float)worldY, 1.0f);
			if( !IsEqualIntAll((m_CurrentFogParams == fogParams), VecBoolV(V_TRUE)))
			{
				m_CurrentFogParams = fogParams;
				fogParamsChanged = true;
			}
#endif //BATCH_WATER_RENDERING

			m_waterShader->SetVar(m_WaterFogTextureVar, g_TxdStore.Get(fogTextureSlot)->GetEntry(0));
			m_waterShader->SetVar(m_WaterFogParamsVar, Vector4((float)worldX - 4*FOGTEXELSIZE, (float)worldY - 4*FOGTEXELSIZE,
																1.0f/(WATERBLOCKWIDTH + 8*FOGTEXELSIZE), 1.0f/(WATERBLOCKWIDTH + 8*FOGTEXELSIZE)));
			BANK_ONLY(SetShaderEditVars());

			return fogParamsChanged;
		}
	}

#if BATCH_WATER_RENDERING
	Vec4V vecNegOne(V_NEGONE);
	if( !IsEqualIntAll((m_CurrentFogParams == vecNegOne), VecBoolV(V_TRUE)))
	{
		m_CurrentFogParams = Vec4V(V_NEGONE);
		fogParamsChanged = true;
	}
#endif //BATCH_WATER_RENDERING

	//Use Low Detail Fog Texture
	m_waterShader->SetVar(m_WaterFogTextureVar, m_CurrentFogTexture);
	m_waterShader->SetVar(m_WaterFogParamsVar, m_GlobalFogUVParams);
	BANK_ONLY(SetShaderEditVars());

	return fogParamsChanged;
}

  
void RenderBorderQuads(s32 pass)
{
	//render border quads
	Vec3V minV;
	Vec3V maxV;
	const grcViewport* vp =  ( pass != pass_invalid ) ? grcViewport::GetCurrent() : gVpMan.GetCurrentGameGrcViewport();
	CalculateFrustumBounds(minV, maxV, *vp);
	minV = RoundToNearestIntNegInf(minV); // floor
	maxV = RoundToNearestIntPosInf(maxV); // ceil
	const s32 minX = (s32)minV.GetXf();
	const s32 minY = (s32)minV.GetYf();
	const s32 maxX = (s32)maxV.GetXf();
	const s32 maxY = (s32)maxV.GetYf();

	float defaultHeight	= m_CurrentDefaultHeight;
	float defaultAlpha	= GetMainWaterTunings().m_WaterColor.GetAlphaf();

	if ( pass != pass_invalid ){

		RenderHighDetail	= false;

		PIXBegin(0, "Water Border Quads");

		m_waterShader->Bind(pass);

#if BATCH_WATER_RENDERING
		if( sBatchWaterRendering )
			BeginWaterBatch();	
#endif //BATCH_WATER_RENDERING

		if(minY < sm_WorldBorderYMin)
			RenderWaterRectangle(	minX,				maxX,				minY,				sm_WorldBorderYMin,	defaultHeight, defaultAlpha, defaultAlpha, defaultAlpha, defaultAlpha);

		if(minX < sm_WorldBorderXMin)
			RenderWaterRectangle(	minX,				sm_WorldBorderXMin,	sm_WorldBorderYMin,	sm_WorldBorderYMax,	defaultHeight, defaultAlpha, defaultAlpha, defaultAlpha, defaultAlpha);

		if(maxY > sm_WorldBorderYMax)
			RenderWaterRectangle(	minX,				maxX,				sm_WorldBorderYMax,	maxY,				defaultHeight, defaultAlpha, defaultAlpha, defaultAlpha, defaultAlpha);

		if(maxX > sm_WorldBorderXMax)
			RenderWaterRectangle(	sm_WorldBorderXMax,	maxX,				sm_WorldBorderYMin,	sm_WorldBorderYMax,	defaultHeight, defaultAlpha, defaultAlpha, defaultAlpha, defaultAlpha);

#if BATCH_WATER_RENDERING
		if( sBatchWaterRendering )
			EndWaterBatch();
#endif //BATCH_WATER_RENDERING

		m_waterShader->UnBind();

		PIXEnd();
	}
	else{
		if (!COcclusion::OcclusionEnabled() )
			return;	

		if(minY < sm_WorldBorderYMin)
			COcclusion::RegisterWaterQuadStencil(	(float)minX,				(float)minY,				(float)maxX,				(float)sm_WorldBorderYMin,	(float)defaultHeight);

		if(minX < sm_WorldBorderXMin)
			COcclusion::RegisterWaterQuadStencil(	(float)minX,				(float)sm_WorldBorderYMin,	(float)sm_WorldBorderXMin,	(float)sm_WorldBorderYMax,	(float)defaultHeight);

		if(maxY > sm_WorldBorderYMax)
			COcclusion::RegisterWaterQuadStencil(	(float)minX,				(float)sm_WorldBorderYMax,	(float)maxX,				(float)maxY,				(float)defaultHeight);

		if(maxX > sm_WorldBorderXMax)
			COcclusion::RegisterWaterQuadStencil(	(float)sm_WorldBorderXMax,	(float)sm_WorldBorderYMin,	(float)maxX,				(float)sm_WorldBorderYMax,	(float)defaultHeight);

	}
}


void RenderLowDetailFarQuads(s32 pass, s32 heightGroupIndex = -1)
{
	const s32 waterNumBlocksY = WATERNUMBLOCKSY;

	s32 b		= GetWaterRenderIndex();
	s32 count	= m_WaterBlockDrawList[b].GetCount();

	PIXBegin(0, "Water Low Detail Far Quads");

	m_waterShader->Bind(pass);

#if BATCH_WATER_RENDERING
	if( sBatchWaterRendering )
		BeginWaterBatch();
#endif //BATCH_WATER_RENDERING

	for(s32 i = 0; i < count; i++)
	{
		s32 blockIndex	= m_WaterBlockDrawList[b][i];

		const s32 x = WaterBlockIndexToWorldX(blockIndex, waterNumBlocksY);
		const s32 y = WaterBlockIndexToWorldY(blockIndex, waterNumBlocksY);

		CWaterBlockData& waterBlock = m_WaterBlockData[blockIndex];

		if(heightGroupIndex >= 0 && Abs<float>(m_OceanQueryHeights[heightGroupIndex] - waterBlock.minHeight) > OCEAN_HEIGHT_TOLERANCE)
			continue;

		if (!(	(x >= RenderCameraBlockMaxX) ||
				(x + WATERBLOCKWIDTH <= RenderCameraBlockMinX)||
				(y >= RenderCameraBlockMaxY) ||
				(y + WATERBLOCKWIDTH <= RenderCameraBlockMinY)))
			continue;
		
		s32 startIndex	= waterBlock.quadIndex;
		s32 endIndex	= startIndex + waterBlock.numQuads;

		for(s32 j = startIndex; j < endIndex; j++)
		{
			CWaterQuad waterQuad = m_WaterQuadsBuffer[j];
			float slope, offset;
			if(waterQuad.GetType() > 0)
			{
				m_CurrentTrisOrientation = (waterQuad.GetType() > 2) ? 1:0;
				waterQuad.GetSlopeAndOffset(slope, offset);
			}
			else
			{
				slope	= 0.0f;
				offset	= 0.0f;
			}

			RenderFlatWaterRectangle(	(s32)waterQuad.minX,
										(s32)waterQuad.maxX,
										(s32)waterQuad.minY,
										(s32)waterQuad.maxY,
										waterQuad.GetZ(), waterQuad.a1/255.0f,  waterQuad.a2/255.0f,  waterQuad.a2/255.0f,  waterQuad.a3/255.0f,
										slope, offset);

		}
	}
#if BATCH_WATER_RENDERING
	if( sBatchWaterRendering )
		EndWaterBatch();
#endif //BATCH_WATER_RENDERING


	m_waterShader->UnBind();

	PIXEnd();
}

void RenderLowDetailNearQuads(s32 pass, s32 heightGroupIndex = -1)
{
	const s32 waterNumBlocksY = WATERNUMBLOCKSY;

	s32 b		= GetWaterRenderIndex();
	s32 count	= m_WaterBlockDrawList[b].GetCount();

	RenderHighDetail	= false;

	PIXBegin(0, "Water Low Detail Near Quads");	

	if(pass >= pass_singlepasstess)
	{
		m_waterShader->Bind(pass);

#if BATCH_WATER_RENDERING
		if(sBatchWaterRendering)
			BeginWaterBatch();
#endif //BATCH_WATER_RENDERING
	}

	for(s32 i = 0; i < count; i++)
	{
		s32 blockIndex	= m_WaterBlockDrawList[b][i];
		CWaterBlockData& waterBlock = m_WaterBlockData[blockIndex];

		if(heightGroupIndex >= 0 && Abs<float>(m_OceanQueryHeights[heightGroupIndex] - waterBlock.minHeight) > OCEAN_HEIGHT_TOLERANCE)
			continue;

		const s32 x = WaterBlockIndexToWorldX(blockIndex, waterNumBlocksY);
		const s32 y = WaterBlockIndexToWorldY(blockIndex, waterNumBlocksY);

		if ((	(x >= RenderCameraBlockMaxX) ||
				(x + WATERBLOCKWIDTH <= RenderCameraBlockMinX)||
				(y >= RenderCameraBlockMaxY) ||
				(y + WATERBLOCKWIDTH <= RenderCameraBlockMinY)))
			continue;


		s32 startIndex	= waterBlock.quadIndex;
		s32 endIndex	= startIndex + waterBlock.numQuads;

		if(pass < pass_singlepasstess)
		{
			SetFogTexture(x, y, b);
			m_waterShader->Bind(pass);

#if BATCH_WATER_RENDERING
			if(sBatchWaterRendering)
				BeginWaterBatch();
#endif //BATCH_WATER_RENDERING
		}

		for(s32 j = startIndex; j < endIndex; j++)
		{
			CWaterQuad waterQuad = m_WaterQuadsBuffer[j];

			float slope, offset;
			if(waterQuad.GetType() > 0)
			{
				m_CurrentTrisOrientation = (waterQuad.GetType() > 2) ? 1:0;
				waterQuad.GetSlopeAndOffset(slope, offset);
			}
			else
			{
				slope	= 0.0f;
				offset	= 0.0f;
			}

			RenderWaterRectangle(	(s32)waterQuad.minX,
				(s32)waterQuad.maxX,
				(s32)waterQuad.minY,
				(s32)waterQuad.maxY,
				waterQuad.GetZ(), waterQuad.a1/255.0f,  waterQuad.a2/255.0f,  waterQuad.a3/255.0f,  waterQuad.a4/255.0f,
				slope, offset);
		}

		if(pass < pass_singlepasstess)
		{
#if BATCH_WATER_RENDERING
			if(sBatchWaterRendering)
				EndWaterBatch();	
#endif //BATCH_WATER_RENDERING
			m_waterShader->UnBind();
		}

	}

	if(pass >= pass_singlepasstess)
	{
#if BATCH_WATER_RENDERING
		if(sBatchWaterRendering)
			EndWaterBatch();	
#endif //BATCH_WATER_RENDERING

		m_waterShader->UnBind();
	}
	


	PIXEnd();
}

#if __USEVERTEXSTREAMRENDERTARGETS
void RenderVertexStreamTessellation(s32 pass, s32 heightGroupIndex)
{
	Matrix34 worldMat	= M34_IDENTITY;
	worldMat.d.x		= (float)RenderCameraRelX;
	worldMat.d.y		= (float)RenderCameraRelY;
	grcViewport::SetCurrentWorldMtx(RCC_MAT34V(worldMat));

	m_waterShader->SetVar(m_WaterFogTextureVar,	m_WaterTessellationFogRT);
	m_waterShader->SetVar(m_WaterFogParamsVar,	Vector4((float)RenderCameraRangeMinX, (float)RenderCameraRangeMinY,
												1.0f/(DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE), 1.0f/(DYNAMICGRIDELEMENTS*DYNAMICGRIDSIZE)));

	GRCDEVICE.SetVertexDeclaration(m_WaterVSRTDeclaration);

	m_waterShader->Bind(pass);
	{
		GRCDEVICE.SetIndices(*m_WaterIndexBuffer);
		GRCDEVICE.SetStreamSource(0, *m_WaterVertexBuffer,			0, m_WaterVertexBuffer->GetVertexStride());
		GRCDEVICE.SetStreamSource(1, *m_WaterHeightVertexBuffer,	0, m_WaterHeightVertexBuffer->GetVertexStride());
		GRCDEVICE.SetStreamSource(2, *m_WaterParamsVertexBuffer,	0, m_WaterParamsVertexBuffer->GetVertexStride());

		s32 count = m_VertexBlockDrawList.GetMaxCount();
		for(s32 i = 0; i < count; i++)
		{
			if(heightGroupIndex >=0 && Abs<float>(m_VertexBlockDrawList[i].m_Height - m_OceanQueryHeights[heightGroupIndex]) > OCEAN_HEIGHT_TOLERANCE)
				continue; 

			if(m_VertexBlockDrawList[i].m_Draw || m_VSRTRenderAll)
			{
				GRCDEVICE.DrawIndexedPrimitive(drawTriStrip, i*VERTSPERBLOCK, VERTSPERBLOCK);
				BANK_ONLY(m_16x16Drawn++);
			}
		}

		GRCDEVICE.ClearStreamSource(0);
		GRCDEVICE.ClearStreamSource(1);
		GRCDEVICE.ClearStreamSource(2);
	}
	m_waterShader->UnBind();
}
#endif //__USEVERTEXSTREAMRENDERTARGETS

void RenderTessellatedQuads(s32 pass, bool tessellateHighDetail, s32 heightGroupIndex = -1)
{
#if __USEVERTEXSTREAMRENDERTARGETS
	if(pass == pass_fogtessvsrt || pass == pass_tessvsrt || pass == pass_singlepasstessvsrt PS3_ONLY(|| pass == pass_underwatertess || pass == pass_underwatertesslow))
	{
		RenderVertexStreamTessellation(pass, heightGroupIndex);
		return;
	}
#endif //__USEVERTEXSTREAMRENDERTARGETS

#if __BANK
	if( m_DisableTessellatedQuads )
		return;
#endif

	PIXBegin(0, "Water Tesselated Quads");

	Matrix34 worldMat	= M34_IDENTITY;
	worldMat.d.x		= (float)RenderCameraRelX;
	worldMat.d.y		= (float)RenderCameraRelY;

	const s32 waterNumBlocksY = WATERNUMBLOCKSY;

	s32 b		= GetWaterRenderIndex();
	s32 count	= m_WaterBlockDrawList[b].GetCount();

	if(pass != pass_invalid)
		GRCDEVICE.SetVertexDeclaration(m_WaterVertexDeclaration);

	RenderHighDetail		= true;
	TessellateHighDetail	= tessellateHighDetail;

	for(s32 i = 0; i < count; i++)
	{
		s32 blockIndex	= m_WaterBlockDrawList[b][i];
		CWaterBlockData& waterBlock = m_WaterBlockData[blockIndex];

		if(heightGroupIndex >= 0 && Abs<float>(m_OceanQueryHeights[heightGroupIndex] - waterBlock.minHeight) > OCEAN_HEIGHT_TOLERANCE)
			continue;

		const s32 x = WaterBlockIndexToWorldX(blockIndex, waterNumBlocksY);
		const s32 y = WaterBlockIndexToWorldY(blockIndex, waterNumBlocksY);

		if ((	(x >= RenderCameraBlockMaxX) ||
				(x + WATERBLOCKWIDTH <= RenderCameraBlockMinX)||
				(y >= RenderCameraBlockMaxY) ||
				(y + WATERBLOCKWIDTH <= RenderCameraBlockMinY)))
			continue;

		s32 startIndex	= m_WaterBlockData[blockIndex].quadIndex;
		s32 endIndex	= startIndex + m_WaterBlockData[blockIndex].numQuads;

		if(pass < pass_underwaterflat)
			SetFogTexture(x, y, b);

		for(s32 j = startIndex; j < endIndex; j++)
		{
			CWaterQuad waterQuad = m_WaterQuadsBuffer[j];

			if ((waterQuad.minX >= RenderCameraRangeMaxX) || (waterQuad.maxX <= RenderCameraRangeMinX))
				continue;
			if ((waterQuad.minY >= RenderCameraRangeMaxY) || (waterQuad.maxY <= RenderCameraRangeMinY))
				continue;

			float slope, offset;
			if(waterQuad.GetType() > 0)
			{
				m_CurrentTrisOrientation = (waterQuad.GetType() > 2) ? 1:0;
				waterQuad.GetSlopeAndOffset(slope, offset);
			}
			else
			{
				slope	= 0.0f;
				offset	= 0.0f;
			}

			float a1 = (float)waterQuad.a1/255.0f;
			float a2 = (float)waterQuad.a2/255.0f;
			float a3 = (float)waterQuad.a3/255.0f;
			float a4 = (float)waterQuad.a4/255.0f;
			float z = waterQuad.GetZ();

			if(tessellateHighDetail && !FillDrawList)
			{
				worldMat.d.z = z;
				grcViewport::SetCurrentWorldMtx(RCC_MAT34V(worldMat));

				float minX = waterQuad.minX;
				float maxX = waterQuad.maxX;
				float minY = waterQuad.minY;
				float maxY = waterQuad.maxY;

				float xRange = maxX - minX;
				float yRange = maxY - minY;
				float lerpMinX = (RenderCameraRangeMinX - minX)/xRange;
				float lerpMaxX = (RenderCameraRangeMaxX - minX)/xRange;
				float lerpMinY = (RenderCameraRangeMinY - minY)/yRange;
				float lerpMaxY = (RenderCameraRangeMaxY - minY)/yRange;
				//3 4
				//1 2
				float minXMinYAlpha = Lerp(lerpMinX, a1, a2);
				float minXMaxYAlpha = Lerp(lerpMinX, a3, a4);
				float maxXMinYAlpha = Lerp(lerpMaxX, a1, a2);
				float maxXMaxYAlpha = Lerp(lerpMaxX, a3, a4);
				a1 = Lerp(lerpMinY, minXMinYAlpha, minXMaxYAlpha);
				a2 = Lerp(lerpMinY, maxXMinYAlpha, maxXMaxYAlpha);
				a3 = Lerp(lerpMaxY, minXMinYAlpha, minXMaxYAlpha);
				a4 = Lerp(lerpMaxY, maxXMinYAlpha, maxXMaxYAlpha);

				m_waterShader->SetVar(QuadAlpha_ID, Vector4(a1, a2, a3, a4));
			}

			if(pass != pass_invalid)
				m_waterShader->Bind(pass);

#if RSG_ORBIS
			if(pass == pass_fogtess)
			{
				gfxc.triggerEvent(sce::Gnm::kEventTypeDbCacheFlushAndInvalidate);
			}
#endif
			RenderWaterRectangle(	waterQuad.minX, waterQuad.maxX, waterQuad.minY, waterQuad.maxY,
									z, a1, a2, a3, a4,
									slope, offset);

			if(pass != pass_invalid)
				m_waterShader->UnBind();
		}
	}

	//Draw high detail water outside of water world
	float defaultAlpha = GetMainWaterTunings().m_WaterColor.GetAlphaf();
	if(tessellateHighDetail && !FillDrawList)
		m_waterShader->SetVar(QuadAlpha_ID, Vector4(defaultAlpha, defaultAlpha, defaultAlpha, defaultAlpha));

	if(pass != pass_invalid)
	{
		m_waterShader->SetVar(m_WaterFogTextureVar, m_CurrentFogTexture);
		BANK_ONLY(SetShaderEditVars());
		m_waterShader->SetVar(m_WaterFogParamsVar, m_GlobalFogUVParams);
	}

	float defaultHeight = m_CurrentDefaultHeight;

	worldMat.d.z = defaultHeight;
	grcViewport::SetCurrentWorldMtx(RCC_MAT34V(worldMat));

	if(RenderCameraRangeMinX < sm_WorldBorderXMin)
	{
		if(pass != pass_invalid)
			m_waterShader->Bind(pass);

			RenderWaterRectangle(	RenderCameraRangeMinX, sm_WorldBorderXMin,
			Clamp(RenderCameraRangeMinY, sm_WorldBorderYMin, sm_WorldBorderYMax), Clamp(RenderCameraRangeMaxY, sm_WorldBorderYMin, sm_WorldBorderYMax),
			defaultHeight, defaultAlpha, defaultAlpha, defaultAlpha, defaultAlpha);

		if(pass != pass_invalid)
			m_waterShader->UnBind();
	}
	if(RenderCameraRangeMaxX > sm_WorldBorderXMax)
	{
		if(pass != pass_invalid)
			m_waterShader->Bind(pass);

			RenderWaterRectangle(	sm_WorldBorderXMax, RenderCameraRangeMaxX,
			Clamp(RenderCameraRangeMinY, sm_WorldBorderYMin, sm_WorldBorderYMax), Clamp(RenderCameraRangeMaxY, sm_WorldBorderYMin, sm_WorldBorderYMax),
			defaultHeight, defaultAlpha, defaultAlpha, defaultAlpha, defaultAlpha);

		if(pass != pass_invalid)
			m_waterShader->UnBind();
	}
	if(RenderCameraRangeMinY < sm_WorldBorderYMin)
	{
		if(pass != pass_invalid)
			m_waterShader->Bind(pass);

			RenderWaterRectangle(	RenderCameraRangeMinX, RenderCameraRangeMaxX,
			RenderCameraRangeMinY, sm_WorldBorderYMin,
			defaultHeight, defaultAlpha, defaultAlpha, defaultAlpha, defaultAlpha);

		if(pass != pass_invalid)
			m_waterShader->UnBind();
	}
	if(RenderCameraRangeMaxY > sm_WorldBorderYMax)
	{
		if(pass != pass_invalid)
			m_waterShader->Bind(pass);

			RenderWaterRectangle(	RenderCameraRangeMinX, RenderCameraRangeMaxX,
			sm_WorldBorderYMax, RenderCameraRangeMaxY,
			defaultHeight, defaultAlpha, defaultAlpha, defaultAlpha, defaultAlpha);

		if(pass != pass_invalid)
				m_waterShader->UnBind();
	}

	PIXEnd();
}

grcViewport* gPrevViewport;
grcViewport gCurrentViewport;

#if __USEVERTEXSTREAMRENDERTARGETS
void UpdateVertexBlockDrawList()
{
	for(s32 i = 0; i < m_VertexBlockDrawList.GetMaxCount(); i++)
		m_VertexBlockDrawList[i].m_Draw = false;

	FillDrawList = true;
	RenderTessellatedQuads(pass_invalid, true);
	FillDrawList = false;
}

void BeginRenderVertexParams()
{
	PF_PUSH_TIMEBAR_DETAIL("Water Vertex Stream Update");

	UpdateVertexBlockDrawList();

	s32 minX				= RenderCameraRangeMinX;
	s32 minY				= RenderCameraRangeMinY;
	s32 maxX				= RenderCameraRangeMaxX - DYNAMICGRIDSIZE;
	s32 maxY				= RenderCameraRangeMaxY - DYNAMICGRIDSIZE;

	m_waterShader->SetVar(m_CameraPositionVar,			camInterface::GetPos());
	grcEffect::SetGlobalVar(m_WaterHeightTextureVar,	GetWaterHeightMap());
	grcEffect::SetGlobalVar(m_StaticBumpTextureVar,		m_BumpTexture);

	float offset		= 0.5f*DYNAMICGRIDSIZE;
	gPrevViewport		= grcViewport::GetCurrent();
	grcViewport& viewport = gCurrentViewport;
	viewport.Ortho(	minX + offset,	maxX + offset,
		maxY + offset,	minY + offset,
		1.0f,	-1000.0f);

	grcViewport::SetCurrent(&viewport);
	Matrix34 worldMat	= M34_IDENTITY;

	worldMat.d			= Vector3((float)RenderCameraRelX, (float)RenderCameraRelY, 0.0f);
	grcViewport::SetCurrentWorldMtx(RCC_MAT34V(worldMat));
	MinBorder			= 0.0f;
	MaxBorder			= (float)DYNAMICGRIDSIZE;

	grcStateBlock::SetDepthStencilState	(grcStateBlock::DSS_Default);
	grcStateBlock::SetBlendState		(grcStateBlock::BS_Default);
	grcStateBlock::SetRasterizerState	(grcStateBlock::RS_NoBackfaceCull);

	grcTextureFactory::GetInstance().LockRenderTarget(0, m_WaterVertexParamsRT, NULL);
	GRCDEVICE.Clear(true, Color32(0,0,0), false, 0, false, 0);
	grcTextureFactory::GetInstance().LockRenderTarget(1, m_WaterTessellationFogRT, NULL);
	grcTextureFactory::GetInstance().LockRenderTarget(2, m_WaterVertexHeightRT, NULL);

	AssertVerify(m_waterShader->BeginDraw(grmShader::RMC_DRAW, true, m_VertexStreamTechnique));
	RenderTessellatedQuads(pass_vsrtparams, false);
	m_waterShader->EndDraw();

	RenderCameraRangeMaxX	= maxX;
	RenderCameraRangeMaxY	= maxY;

	PreviousForcedTechniqueGroupId = grmShader::GetForcedTechniqueGroupId();
	
	grmShader::SetForcedTechniqueGroupId(VertexParamsTechniqueGroup);
}

void EndRenderVertexParams()
{
	grcTextureFactory::GetInstance().UnlockRenderTarget(2);
	grcTextureFactory::GetInstance().UnlockRenderTarget(1);
	grcTextureFactory::GetInstance().UnlockRenderTarget(0);

	grmShader::SetForcedTechniqueGroupId(PreviousForcedTechniqueGroupId);
	grcViewport::SetCurrent(gPrevViewport);
	PF_POP_TIMEBAR_DETAIL();
}

void UpdateVertexStreamRenderTargets()
{
	if(!m_UseVSRT)
		return;

	DLC_Add(BeginRenderVertexParams);
	RENDERLIST_DUMP_PASS_INFO_BEGIN("CRenderPhaseDeferredLighting_SceneToGBuffer - water VSRT");
	CRenderListBuilder::AddToDrawList(Water::GetWaterRenderPhase()->GetEntityListIndex(), CRenderListBuilder::RENDER_WATER, CRenderListBuilder::USE_NO_LIGHTING);
	RENDERLIST_DUMP_PASS_INFO_END();
	DLC_Add(EndRenderVertexParams);
}
#endif //__USEVERTEXSTREAMRENDERTARGETS

void Water::SetupWaterParams()
{
	CRenderTargets::UnlockSceneRenderTargets();

#if RSG_PC
	if (GRCDEVICE.IsStereoEnabled())
	{
		CShaderLib::SetStereoTexture();
	}
#endif

	//=================== Water Blend Pass ====================
	PF_PUSH_TIMEBAR("Water Blend");

	grcStateBlock::SetStates(grcStateBlock::RS_Default, grcStateBlock::DSS_Default, grcStateBlock::BS_Default);

	grcTextureFactory::GetInstance().LockRenderTarget(0, m_BlendRT, NULL);
	
	const tcKeyframeUncompressed& currKeyframe = g_timeCycle.GetCurrRenderKeyframe();
	float waterReflection = currKeyframe.GetVar(TCVAR_WATER_REFLECTION);

	const CMainWaterTune &mainTunings		= GetMainWaterTunings();
	const CSecondaryWaterTune &secTunings	= GetSecondaryWaterTunings();

	// TODO -- need to pass the result of CGameWorldWaterHeight::GetWaterHeight to renderthread
	m_WaterTextureShader->SetVar(UpdateParams0_ID, Vec4V(waterReflection, mainTunings.m_RefractionBlend, mainTunings.m_RefractionExponent, CGameWorldWaterHeight::GetWaterHeight()));

	AssertVerify(m_WaterTextureShader->BeginDraw(grmShader::RMC_DRAW, true, m_DownSampleTechnique));
	m_WaterTextureShader->Bind(pass_waterblend);
	grcDrawSingleQuadf(-1.0, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, Color32(0,0,0,0));
	m_WaterTextureShader->UnBind();
	m_WaterTextureShader->EndDraw();
	grcTextureFactory::GetInstance().UnlockRenderTarget(0);
	PF_POP_TIMEBAR();
	//========================================================

	CRenderTargets::LockSceneRenderTargets();
	
	River::SetFogPassGlobalVars();

	float foamIntensity = secTunings.m_OceanFoamIntensity * currKeyframe.GetVar(TCVAR_WATER_FOAM_INTENSITY_MULT);

	m_waterShader->SetVar(m_WaterLocalParamsVar, Vec4V(foamIntensity, mainTunings.m_OceanFoamScale, (float)RenderCameraRangeMinX, (float)RenderCameraRangeMinY));


	const CLightSource &dirLight	= Lights::GetRenderDirectionalLight();
	Vec4V directionalColor(dirLight.GetColorTimesIntensity());
	Vector4 directional				= RC_VECTOR4(directionalColor);
	Vector4 ambient					= Lights::m_lightingGroup.GetNaturalAmbientDown()		+ Lights::m_lightingGroup.GetNaturalAmbientBase();
	Vector4 interiorAmbient			= Lights::m_lightingGroup.GetArtificialIntAmbientDown()	+ Lights::m_lightingGroup.GetArtificialIntAmbientBase();
	float ambientHack               = 0.27f;
	interiorAmbient					= interiorAmbient*(ambientHack*ambientHack*2.0f);

	float waterInteriorLerp		= currKeyframe.GetVar(TCVAR_WATER_INTERIOR);
	directional	= directional*(1.0f - waterInteriorLerp);
	ambient.Lerp(waterInteriorLerp, interiorAmbient);
	ambient.w	= mainTunings.m_OceanFoamScale;

#if __WIN32PC && __BANK
	if (TiledScreenCapture::IsEnabled())
	{
		directional.w = 1.0f; // force all water to be pure blue in the shader
	}
	else
#endif // __WIN32PC && __BANK
	{
		directional.w = 0.0f;
	}

	gWaterDirectionalColor	= RC_VEC4V(directional);
	gWaterAmbientColor		= RC_VEC4V(ambient);
}

void RenderWaterHeight()
{
	GetCameraRangeForRender();
	Matrix34 worldMat = M34_IDENTITY;
	worldMat.d = Vector3((float)RenderCameraRelX, (float)RenderCameraRelY, 0.0f);
	grcViewport::SetCurrentWorldMtx(RCC_MAT34V(worldMat));

	m_waterShader->SetVar(m_WaterLocalParamsVar, Vec4V(0.0f, 0.0f, (float)RenderCameraRangeMinX, (float)RenderCameraRangeMinY));
	grcEffect::SetGlobalVar(m_WaterLightingTextureVar, GetWaterHeightMap());

	if(m_waterShader->BeginDraw(grmShader::RMC_DRAW, true, m_WaterTechnique))
		RenderTessellatedQuads(pass_height, false);
	m_waterShader->EndDraw();
}

void BeginHeightMapPass()
{
	PF_PUSH_TIMEBAR("Water Height Pass");

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	RenderWaterHeight();

	PreviousForcedTechniqueGroupId = grmShaderFx::GetForcedTechniqueGroupId();

	grmShaderFx::SetForcedTechniqueGroupId(WaterHeightTechniqueGroup);
}

void EndHeightMapPass()
{
	grmShaderFx::SetForcedTechniqueGroupId(PreviousForcedTechniqueGroupId);

	PF_POP_TIMEBAR();
}

void Water::ProcessHeightMapPass()
{
	DLC_Add(BeginHeightMapPass);
	RENDERLIST_DUMP_PASS_INFO_BEGIN("CRenderPhaseDeferredLighting_SceneToGBuffer - Water Height");
	CRenderListBuilder::AddToDrawList(Water::GetWaterRenderPhase()->GetEntityListIndex(), CRenderListBuilder::RENDER_WATER, CRenderListBuilder::USE_NO_LIGHTING);
	RENDERLIST_DUMP_PASS_INFO_END();
	DLC_Add(EndHeightMapPass);
}

#if WATER_TILED_LIGHTING
void SetupFogPrepassHiZ()
{
	grcStateBlock::SetStates(grcStateBlock::RS_Default, grcStateBlock::DSS_ForceDepth, grcStateBlock::BS_Default_WriteMaskNone);
	m_WaterTextureShader->SetVar(m_PointTextureVar,	CTiledLighting::GetClassificationTexture());

	WaterScreenBlit(m_WaterTextureShader, m_DownSampleTechnique, pass_hizsetup);
	GRCDEVICE.Clear(true, Color32(0, 0, 0, 0), false, 1.0f, false, 0);
}
#endif //WATER_TILED_LIGHTING

void Water::BeginFogPrepass(u32 XENON_ONLY(pixels))
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

#if __BANK
	// don't render water if we're viewing a gbuffer (this would overwrite GBUFFER3 on 360, we don't want to do that)
	if (DebugDeferred::m_GBufferIndex > 0)
		return;
#endif // __BANK


	s32 fogTessellationPass	= pass_fogtess;

#if __USEVERTEXSTREAMRENDERTARGETS
	if(m_UseVSRT)
		fogTessellationPass	= pass_fogtessvsrt;
#endif //__USEVERTEXSTREAMRENDERTARGETS

	PF_PUSH_TIMEBAR_DETAIL("Water Fog Prepass");

#if __XENON

	if(pixels > 32768)
		XENON_ONLY(GRCDEVICE.SetShaderGPRAllocation(128 - CGPRCounts::ms_WaterFogPrepassRegs, 112));
		
	MinBorder	= 0.5f;
	MaxBorder	= 0.5f;
#else
	MinBorder	= 0.0f;
	MaxBorder	= 0.0f;
#endif

	CRenderTargets::UnlockSceneRenderTargets();

	m_WaterTextureShader->SetVar(m_ProjParamsVar, ShaderUtil::CalculateProjectionParams());

	const grcRenderTarget* rendertargets[grcmrtColorCount] = {
		m_LightingRT,
		m_DownSampleRT,
		NULL,
		NULL
	};
	grcTextureFactory::GetInstance().LockMRT(rendertargets, m_FogDepthRT);

#if USE_INVERTED_PROJECTION_ONLY
	gPrevViewport		= grcViewport::GetCurrent();
	gCurrentViewport	= *(grcViewport::GetCurrent());
	gCurrentViewport.SetInvertZInProjectionMatrix(true);
	grcViewport::SetCurrent(&gCurrentViewport);
#endif

#if WATER_TILED_LIGHTING
	if(CTiledLighting::IsEnabled() && m_EnableFogPrepassHiZ)
		SetupFogPrepassHiZ();
	else
#endif //WATER_TILED_LIGHTING
	{
		float clearDepth = USE_INVERTED_PROJECTION_ONLY? 0.0f : 1.0f;
		GRCDEVICE.Clear(true, Color32(0, 0, 0, 0), true, clearDepth, false, 0);
	}

	//With batched water we add degenerate tris in which changes the winding order
	//so disable culling, cant imagine it benefits greatly from having it
	grcStateBlock::SetStates(BATCH_WATER_RENDERING? grcStateBlock::RS_NoBackfaceCull : grcStateBlock::RS_Default, m_WaterDepthStencilState, grcStateBlock::BS_Default);

	Matrix34 worldMat = M34_IDENTITY;
	worldMat.d = Vector3((float)RenderCameraRelX, (float)RenderCameraRelY, 0.0f);
	grcViewport::SetCurrentWorldMtx(RCC_MAT34V(worldMat));

	grcEffect::SetGlobalVar(m_StaticBumpTextureVar,		m_CloudTexture);
	grcEffect::SetGlobalVar(m_WaterHeightTextureVar,	GetWaterHeightMap());
	grcEffect::SetGlobalVar(m_WaterParams0Var,			Vec4V(m_DownSampleRT->GetWidth()/4.0f, m_DownSampleRT->GetHeight()/4.0f, 0.0f, 0.0f));
	// Set Water.fx - turns off bump derived vertex xy perturbation (the height map is quantized in Replay and gives rise to jerky vertex positions).
	grcEffect::SetGlobalVar(m_WaterParams2Var,			Vec4V(REPLAY_ONLY(CReplayMgr::IsReplayInControlOfWorld() ? 0.0f :) 1.0f, 0.0f, 0.0f, 0.0f));
	m_waterShader->SetVar(m_WaterFogTextureVar, m_CurrentFogTexture);

	if(m_DrawWaterQuads && waterDataLoaded)
	{
		AssertVerify(m_waterShader->BeginDraw(grmShader::RMC_DRAW, true, m_WaterTechnique));
			RenderBorderQuads(pass_fogflat);
			RenderLowDetailFarQuads(pass_fogflat);
			RenderLowDetailNearQuads(pass_fogflat);
		RenderTessellatedQuads(fogTessellationPass, true);
		m_waterShader->EndDraw();
	}

	River::BeginRenderRiverFog();
}

void Water::EndFogPrepass(float elapsedTime)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

#if __BANK
	// don't render water if we're viewing a gbuffer (this would overwrite GBUFFER3 on 360, we don't want to do that)
	if (DebugDeferred::m_GBufferIndex > 0)
	{
		return;
	}
#endif // __BANK

#if __D3D11 || RSG_ORBIS
	grcTextureFactory::GetInstance().UnlockMRT();
#else
	grcTextureFactory::GetInstance().UnlockRenderTarget(1);
	grcTextureFactory::GetInstance().UnlockRenderTarget(0);
#endif

	PF_POP_TIMEBAR_DETAIL();
	PF_PUSH_TIMEBAR("Water Fog Composite");

	grcStateBlock::SetDepthStencilState	(grcStateBlock::DSS_IgnoreDepth);
	grcStateBlock::SetBlendState		(grcStateBlock::BS_Default);
	grcStateBlock::SetRasterizerState	(grcStateBlock::RS_Default);

	m_WaterTextureShader->SetVar(m_LinearTextureVar, m_LightingRT);

	grcTextureFactory::GetInstance().LockRenderTarget(0, m_Lighting4RT, NULL);
	WaterScreenBlit(m_WaterTextureShader, m_DownSampleTechnique, pass_downsample);
	grcTextureFactory::GetInstance().UnlockRenderTarget(0);

	m_WaterTextureShader->SetVar(m_LinearTextureVar, m_Lighting4RT);
	grcTextureFactory::GetInstance().LockRenderTarget(0, m_Lighting16TempRT, NULL);

	WaterScreenBlit(m_WaterTextureShader, m_DownSampleTechnique, pass_downsample);
	grcTextureFactory::GetInstance().UnlockRenderTarget(0);

	m_WaterTextureShader->SetVar(m_LinearTextureVar, m_Lighting16TempRT);

	grcTextureFactory::GetInstance().LockRenderTarget(0, m_Lighting16RT, NULL);
	WaterScreenBlit(m_WaterTextureShader, m_DownSampleTechnique, pass_blur);
	grcTextureFactory::GetInstance().UnlockRenderTarget(0);
	
#if WATER_TILED_LIGHTING
	if(CTiledLighting::IsEnabled() && m_UseTiledFogComposite)
	{
		m_WaterTextureShader->SetVar(m_LinearTextureVar, m_Lighting16RT);
		grcTextureFactory::GetInstance().LockRenderTarget(0, m_ClassificationRT, NULL);
		WaterScreenBlit(m_WaterTextureShader, m_DownSampleTechnique, pass_downsampletile);
		grcTextureFactory::GetInstance().UnlockRenderTarget(0);
	}
#endif //WATER_TILED_LIGHTING

	//========================== Water Fog Composite ==============================
	m_WaterTextureShader->SetVar(m_PointTextureVar,			m_DownSampleRT);
	m_WaterTextureShader->SetVar(m_PointTexture2Var,		m_LightingRT);
	m_WaterTextureShader->SetVar(m_LinearTexture2Var,		m_Lighting16RT);
	m_WaterTextureShader->SetVar(m_LinearWrapTextureVar,	m_noiseTexture);
	m_WaterTextureShader->SetVar(m_LinearWrapTexture2Var,	m_CausticTexture);
	m_WaterTextureShader->SetVar(m_LinearTextureVar,		MSAA_ONLY(GRCDEVICE.GetMSAA()>1? CRenderTargets::GetBackBufferCopy(false) :) CRenderTargets::GetBackBuffer());	//resolved HDR buffer
	m_WaterTextureShader->SetVar(m_DepthTextureVar,			CRenderTargets::GetDepthBufferQuarterLinear());
	m_WaterTextureShader->SetVar(m_FullTextureVar,          m_FogDepthRT);

	m_WaterTextureShader->SetVar(m_WaterCompositeDirectionalColorVar, gWaterDirectionalColor);

	float fogLightIntensity			= g_timeCycle.GetCurrRenderKeyframe().GetVar(TCVAR_WATER_FOGLIGHT);
	float directionalScale			= -Lights::GetRenderDirectionalLight().GetDirection().z;
	Vec4V ambientColor				= gWaterAmbientColor*ScalarV(fogLightIntensity);
	m_WaterTextureShader->SetVar(m_WaterCompositeAmbientColorVar, ambientColor);

	float fogPierceIntensity		= GetMainWaterTunings().m_FogPierceIntensity;
	m_WaterTextureShader->SetVar(m_WaterCompositeParamsVar, Vec4V(fogPierceIntensity, fogLightIntensity*directionalScale, elapsedTime, 0.0f));

	
	grcTextureFactory::GetInstance().LockRenderTarget(0, m_RefractionRT, NULL);

	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);

#if DEVICE_GPU_WAIT //corruption is visible without a fence here
	GRCDEVICE.GpuWaitOnPreviousWrites();
#endif

#if WATER_TILED_LIGHTING
	if(CTiledLighting::IsEnabled() && m_UseTiledFogComposite)
	{
		m_WaterTextureShader->SetVar(m_LinearWrapTexture3Var,	m_ClassificationRT);
		CTiledLighting::RenderTiles(NULL, m_WaterTextureShader, m_DownSampleTechnique, pass_waterfogcompositetiled, 
									"Fog Composite Tiled", false, false, false);
	}
	else
#endif //WATER_TILED_LIGHTING
	{
		Vec4V texOffset(0.f, 0.f, 0.f, 0.f);
#if DEVICE_MSAA && RSG_PC
		//Note: using the sample[0] from MSAA back buffer seems like the solution here,
		// but it introduces a weird image corruption on AMD cards, hence the workaround below:

		float sampleX = 0.0f, sampleY = 0.0f;
		switch (GRCDEVICE.GetMSAA())
		{
		case 2:
			sampleX = 0.24f; sampleY = 0.24f;
			break;
		case 4:
			sampleX = -0.12f; sampleY = -0.36f;
			break;
		case 8:
			sampleX = 0.08f; sampleY = -0.2f;
			break;
		default: ;
		}
		
		// compensate the fact our back buffer was resolved differently from the depth buffer
		// back buffer resolve is the average, depth buffer resolve is sample[0]
		texOffset.SetX((sampleX BANK_ONLY(+ m_FogCompositeTexOffset.x)) / GRCDEVICE.GetWidth());
		texOffset.SetY((sampleY BANK_ONLY(+ m_FogCompositeTexOffset.y)) / GRCDEVICE.GetHeight());
#endif //DEVICE_MSAA
		
		m_WaterTextureShader->SetVar(m_WaterCompositeTexOffsetVar, texOffset);
		WaterScreenBlit(m_WaterTextureShader, m_DownSampleTechnique, pass_waterfogcomposite);
	}

#if DEVICE_GPU_WAIT  
	// B*6570119 :: Corruption is visible without a fence here too!
	// Maybe this is where the fence above was intended to be placed?
		GRCDEVICE.GpuWaitOnPreviousWrites();
#endif

	grcTextureFactory::GetInstance().UnlockRenderTarget(0);
	//====================================================================================

   	PF_POP_TIMEBAR();
}

Lights::LightweightTechniquePassType prevPassType = Lights::kLightPassNormal;
void SetupWaterRefractionAlphaState()
{
	PF_PUSH_TIMEBAR_DETAIL("Water Refraction Alpha");
	grcStateBlock::SetDepthStencilState(m_WaterRefractionAlphaDepthStencilState);
	grcStateBlock::SetBlendState(m_WaterRefractionAlphaBlendState);
	prevPassType = Lights::GetLightweightTechniquePassType();
	Lights::SetLightweightTechniquePassType(Lights::kLightPassWaterRefractionAlpha);
	grcEffect::SetGlobalVar(m_WaterDepthTextureVar, m_FogDepthRT);
	grcEffect::SetGlobalVar(m_WaterParamsTextureVar, m_DownSampleRT);
	grcEffect::SetGlobalVar(m_WaterVehicleProjParamsVar, VECTOR4_TO_VEC4V(ShaderUtil::CalculateProjectionParams()).GetZW());

	Lights::SetupDirectionalAndAmbientGlobals();
	CRenderPhaseReflection::SetReflectionMap();
	CShaderLib::SetFogRayTexture();
	CRenderPhaseCascadeShadowsInterface::SetDeferredLightingShaderVars();
	CRenderPhaseCascadeShadowsInterface::SetSharedShaderVars();
	CRenderPhaseCascadeShadowsInterface::SetCloudShadowParams();
}

void CleanupWaterRefractionAlphaState()
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP();

	Lights::SetLightweightTechniquePassType(prevPassType);
#if USE_INVERTED_PROJECTION_ONLY
	grcViewport::SetCurrent(gPrevViewport);
#endif //SUPPORT_INVERTED_VIEWPORT

	PF_POP_TIMEBAR_DETAIL();
}

void Water::RenderForwardIntoRefraction()
{
	DLC_Add(CRenderTargets::DownsampleDepth);
	DLC(dlCmdLockRenderTarget, (0, m_RefractionRT, CRenderTargets::GetDepthBufferQuarter()));

	DLC_Add(SetupWaterRefractionAlphaState);
	//If camera is above water then render all underwater ptfx in
	//Render all alpha stuff into water refraction
	
	//Adding forced alpha drawlist
	
	DLC_Add(Lights::BeginLightweightTechniques);
	CRenderListBuilder::AddToDrawList_ForcedAlpha(m_WaterRenderPhase->GetEntityListIndex(), CRenderListBuilder::RENDER_WATER, CRenderListBuilder::USE_FORWARD_LIGHTING, SUBPHASE_NONE);

	DLC_Add(Lights::EndLightweightTechniques);

	//Disabling this for now.
	//TODO: Re-enable this for next project
	//Render all visual fx into water refraction
	//CVisualEffects::RenderIntoWaterRefraction(RENDER_SETTING_RENDER_PARTICLES);

	DLC_Add(CleanupWaterRefractionAlphaState);
	DLC(dlCmdUnLockRenderTarget, (0));
}

void Water::RenderRefractionAndFoam()
{
	grcStateBlock::SetStates(grcStateBlock::RS_Default, grcStateBlock::DSS_Default, grcStateBlock::BS_Default);

	PF_PUSH_TIMEBAR("Water Refraction Mask");
	//========================== Render Refraction Mask ==============================
	m_WaterTextureShader->SetVar(m_LinearWrapTexture3Var, m_DownSampleRT);
	grcTextureFactory::GetInstance().LockRenderTarget(0, m_RefractionMaskRT, NULL);
	WaterScreenBlit(m_WaterTextureShader, m_DownSampleTechnique, pass_watermask);
	grcTextureFactory::GetInstance().UnlockRenderTarget(0);
	//================================================================================
	PF_POP_TIMEBAR();

	PF_PUSH_TIMEBAR("Water Refraction Mask Composite");
	//========================== Render Refraction Mask Composite ====================
	grcStateBlock::SetBlendState(m_UpscaleShadowMaskBlendState);
	m_WaterTextureShader->SetVar(m_LinearWrapTextureVar, m_RefractionMaskRT);
	m_WaterTextureShader->SetVar(m_LinearTextureVar,     m_Lighting16RT);
	grcTextureFactory::GetInstance().LockRenderTarget(0, m_DownSampleRT, NULL);
	WaterScreenBlit(m_WaterTextureShader, m_DownSampleTechnique, pass_upscaleshadowmask);
	grcTextureFactory::GetInstance().UnlockRenderTarget(0);
	//================================================================================
	PF_POP_TIMEBAR();

	PF_PUSH_TIMEBAR("Water Foam");
	//========================== Render Foam RT ==============================
	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	m_WaterTextureShader->SetVar(m_PointTextureVar, GetWetMap());
	grcTextureFactory::GetInstance().LockRenderTarget(0, m_FoamRT, NULL);
	WaterScreenBlit(m_WaterTextureShader, m_WetUpdateTechnique, pass_foamintensity);
	grcTextureFactory::GetInstance().UnlockRenderTarget(0);
	//========================================================================
	PF_POP_TIMEBAR();

	m_RefractionSource = m_RefractionRT;

	CRenderTargets::LockSceneRenderTargets();

	River::EndRenderRiverFog();


}

grcRenderTarget* Water::GetWaterRefractionColorTarget()
{
	return m_RefractionRT;
}

grcRenderTarget* Water::GetWaterRefractionDepthTarget()
{
	return CRenderTargets::GetDepthBufferQuarter();
}

void Water::Render(u32 pixelsRendered, u32 renderFlags)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));		// this command is forbidden outwith the render thread

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	const tcKeyframeUncompressed& currKeyframe = g_timeCycle.GetCurrRenderKeyframe();
	float waterReflection = currKeyframe.GetVar(TCVAR_WATER_REFLECTION);
	Assert(waterReflection > 0.01f); // otherwise we should not be in here

#if __BANK
	sysTimer timer;

	if (m_bForceWaterReflectionON)
		waterReflection = 1.0f;
#endif // __BANK

	if (!(renderFlags & RENDER_SETTING_RENDER_WATER))
		return;

	const CMainWaterTune &mainTunings = (const CMainWaterTune &)GetMainWaterTunings();
	const CSecondaryWaterTune &secTunings	= GetSecondaryWaterTunings();

	PF_PUSH_TIMEBAR_DETAIL("Water Render");
	
	grcEffect::SetGlobalVar(m_WaterBlendTextureVar,	m_BlendRT);
	if(pixelsRendered > 1024)
	{
		grcEffect::SetGlobalVar(m_WaterBumpTextureVar,	m_CurrentBumpTexture);
		grcEffect::SetGlobalVar(m_WaterBumpTexture2Var,	m_CurrentBump2Texture);
	}
	else
	{
		grcEffect::SetGlobalVar(m_WaterBumpTextureVar,	m_BumpTexture2);
		grcEffect::SetGlobalVar(m_WaterBumpTexture2Var,	m_BumpTexture2);
	}


	grcEffect::SetGlobalVar(m_WaterDirectionalColorVar,	gWaterDirectionalColor);
	grcEffect::SetGlobalVar(m_WaterAmbientColorVar,		gWaterAmbientColor);

	float bumpiness	= Lerp(secTunings.m_RippleBumpinessWindScale, secTunings.m_RippleBumpiness, secTunings.m_RippleBumpiness*m_RippleWind);
	bumpiness		= Clamp(bumpiness, secTunings.m_RippleMinBumpiness, secTunings.m_RippleMaxBumpiness);

	grcEffect::SetGlobalVar(m_WaterParams0Var,			
							Vec4V(bumpiness, mainTunings.m_RippleScale, mainTunings.GetSpecularIntensity(currKeyframe), mainTunings.m_SpecularFalloff));
	grcEffect::SetGlobalVar(m_WaterParams1Var,			
							Vec4V(waterReflection, mainTunings.m_FogPierceIntensity, 0.0f, secTunings.m_OceanBumpiness));
	grcEffect::SetGlobalVar(m_WaterParams2Var,
							Vec4V(REPLAY_ONLY(CReplayMgr::IsReplayInControlOfWorld() ? 0.0f :) 1.0f, 0.0f, 0.0f, 0.0f));

	grcEffect::SetGlobalVar(m_StaticBumpTextureVar,		m_BumpTexture);
	
	grcEffect::SetGlobalVar(m_WaterStaticFoamTextureVar, m_FoamTexture);

	const grcViewport *vp = grcViewport::GetCurrent();
	if(Verifyf(vp, "Could not get the current game viewport. Water refraction matrix will be incorrect!"))
	{
		Mat44V refractionMatrix;
		Multiply(refractionMatrix, vp->GetProjection(), vp->GetViewMtx());
		grcEffect::SetGlobalVar(m_WaterRefractionMatrixVar,	refractionMatrix);
	}

	grcEffect::SetGlobalVar(m_WaterReflectionMatrixVar,	m_ReflectionMatrix[GetWaterRenderIndex()]);
	grcEffect::SetGlobalVar(m_WaterHeightTextureVar,	GetWaterHeightMap());
	grcEffect::SetGlobalVar(m_WaterWetTextureVar,		m_FoamRT);

	//=============================== Set Water Rendering Render States ===============================
	grcStateBlock::SetDepthStencilState	(m_WaterDepthStencilState, WATER_STENCIL_MASKID | WATER_STENCIL_ID | DEFERRED_MATERIAL_DEFAULT);
	grcStateBlock::SetBlendState		(grcStateBlock::BS_Default);
#if BATCH_WATER_RENDERING
	//With batched water we add degenerate tris in which changes the winding order
	//so disable culling, cant imagine it benefits greatly from having it on.
	grcStateBlock::SetRasterizerState	(grcStateBlock::RS_NoBackfaceCull);
#else
	if(IsCameraUnderwater())
		grcStateBlock::SetRasterizerState	(m_UnderwaterRasterizerState);
	else
		grcStateBlock::SetRasterizerState	(grcStateBlock::RS_Default);
#endif
	//=================================================================================================

#if __BANK
	if (TiledScreenCapture::IsEnabled())
	{
		grcStateBlock::SetDepthStencilState(m_WaterTiledScreenCaptureDepthStencilState, DEFERRED_MATERIAL_WATER_DEBUG);
	}

	bool bWireframeEnabled = false;
	bool bWireframePrev = false;

	if (CRenderPhaseDebugOverlayInterface::GetWaterQuadWireframeMode(m_bWireFrame) == 1)
	{
		bWireframeEnabled = true;
		bWireframePrev = grcStateBlock::SetWireframeOverride(true);
	}
#endif // __BANK

	//Pick technique to use
	s32 waterTessellationPass;
	s32 waterPass;

	if(IsCameraUnderwater())
	{
		if(m_UseHQWaterRendering[GetWaterRenderIndex()] && m_EnableWaterLighting)
		{
			waterTessellationPass	= pass_underwatertess;
			waterPass				= pass_underwaterflat;
		}
		else
		{
			waterTessellationPass	= pass_underwatertesslow;
			waterPass				= pass_underwaterflatlow;
		}
	}
	else
	{
		if(UseFogPrepass())
		{
			waterPass					= pass_flat;
#if __USEVERTEXSTREAMRENDERTARGETS
			if(m_UseVSRT)
				waterTessellationPass	= pass_tessvsrt;
			else
#endif //__USEVERTEXSTREAMRENDERTARGETS
				waterTessellationPass	= pass_tess;

			grcEffect::SetGlobalVar(m_WaterLightingTextureVar, m_DownSampleRT);
		}
		else
		{
			waterPass					= pass_singlepassflat;
#if __USEVERTEXSTREAMRENDERTARGETS
			if(m_UseVSRT)
				waterTessellationPass	= pass_singlepasstessvsrt;
			else
#endif //__USEVERTEXSTREAMRENDERTARGETS
				waterTessellationPass	= pass_singlepasstess;
		}

	}

	Matrix34 worldMat = M34_IDENTITY;
	worldMat.d = Vector3((float)RenderCameraRelX, (float)RenderCameraRelY, 0.0f);
	grcViewport::SetCurrentWorldMtx(RCC_MAT34V(worldMat));

	Vector2 fogScale	= mainTunings.m_FogMax - mainTunings.m_FogMin;
	Vector2 fogOffset	= mainTunings.m_FogMin;
	m_GlobalFogUVParams	= Vector4(fogOffset.x, fogOffset.y, 1.0f/fogScale.x, 1.0f/fogScale.y);

	m_waterShader->SetVar(m_WaterFogTextureVar,	m_CurrentFogTexture);
	BANK_ONLY(SetShaderEditVars());
	m_waterShader->SetVar(m_WaterFogParamsVar,	m_GlobalFogUVParams);
	
	if(m_DrawWaterQuads && 	waterDataLoaded)
	{
		if(m_waterShader->BeginDraw(grmShader::RMC_DRAW, true, m_WaterTechnique))
		{
			m_WaterOcclusionQueries[GPUINDEX][query_oceanfar].BeginQuery();
			RenderBorderQuads(waterPass);
			m_WaterOcclusionQueries[GPUINDEX][query_oceanfar].EndQuery();

			for(s32 i = query_oceanstart; i <= query_oceanend; i++)
			{
				m_WaterOcclusionQueries[GPUINDEX][i].BeginQuery();

				if (i - query_oceanstart < m_OceanQueryHeights.GetCount())
				{
					RenderLowDetailFarQuads(waterPass, i);
					RenderLowDetailNearQuads(waterPass, i);
					RenderTessellatedQuads(waterTessellationPass, true, i);
				}

				m_WaterOcclusionQueries[GPUINDEX][i].EndQuery();
			}
		}
		m_waterShader->EndDraw();
	}

	m_waterShader->SetVar(m_WaterFogTextureVar,	m_CurrentFogTexture);

	PF_POP_TIMEBAR_DETAIL();

#if __BANK
	if (bWireframeEnabled)
		grcStateBlock::SetWireframeOverride(bWireframePrev);
#endif // __BANK

#if __BANK
	if(m_EnableTestBuoys)
		DrawTestBuoys();

	m_RenderTime=timer.GetMsTime();
#endif //__BANK
}

#if __BANK
void Water::RenderDebugOverlay()
{
	if(!m_DrawWaterQuads || !waterDataLoaded)
		return;

	GetCameraRangeForRender();

	Matrix34 worldMat = M34_IDENTITY;
	worldMat.d = Vector3((float)RenderCameraRelX, (float)RenderCameraRelY, 0.0f);
	grcViewport::SetCurrentWorldMtx(RCC_MAT34V(worldMat));

	m_waterShader->SetVar(m_WaterDebugOverlayDepthBufferVar, CRenderTargets::PS3_SWITCH(GetDepthBufferAsColor, GetDepthBuffer)());

	const int cull = IsCameraUnderwater() ? (int)grcRSV::CULL_CCW : (int)grcRSV::CULL_CW;;

	if (CRenderPhaseDebugOverlayInterface::InternalPassBegin(false, false, cull, false)) // fill
	{
		if (AssertVerify(m_waterShader->BeginDraw(grmShader::RMC_DRAW, true, m_WaterDebugOverlayTechnique)))
		{
			if ((CRenderPhaseDebugOverlayInterface::GetWaterQuadWireframeFlags() & BIT(4)) == 0) { RenderBorderQuads(0); }
			if ((CRenderPhaseDebugOverlayInterface::GetWaterQuadWireframeFlags() & BIT(5)) == 0) { RenderLowDetailFarQuads(0); }
			if ((CRenderPhaseDebugOverlayInterface::GetWaterQuadWireframeFlags() & BIT(6)) == 0) { RenderLowDetailNearQuads(0); }
			if ((CRenderPhaseDebugOverlayInterface::GetWaterQuadWireframeFlags() & BIT(7)) == 0) { RenderTessellatedQuads(1, true); }
		}
		m_waterShader->EndDraw();
		CRenderPhaseDebugOverlayInterface::InternalPassEnd(false);
	}

	if (CRenderPhaseDebugOverlayInterface::InternalPassBegin(true, false, cull, false)) // wire
	{
		if (AssertVerify(m_waterShader->BeginDraw(grmShader::RMC_DRAW, true, m_WaterDebugOverlayTechnique)))
		{
			if ((CRenderPhaseDebugOverlayInterface::GetWaterQuadWireframeFlags() & BIT(4)) == 0) { RenderBorderQuads(0); }
			if ((CRenderPhaseDebugOverlayInterface::GetWaterQuadWireframeFlags() & BIT(5)) == 0) { RenderLowDetailFarQuads(0); }
			if ((CRenderPhaseDebugOverlayInterface::GetWaterQuadWireframeFlags() & BIT(6)) == 0) { RenderLowDetailNearQuads(0); }
			if ((CRenderPhaseDebugOverlayInterface::GetWaterQuadWireframeFlags() & BIT(7)) == 0) { RenderTessellatedQuads(1, true); }
		}
		m_waterShader->EndDraw();
		CRenderPhaseDebugOverlayInterface::InternalPassEnd(true);
	}
}
#endif // __BANK


static grcRasterizerStateHandle g_prev_RS;
static grcDepthStencilStateHandle g_prev_DSS;
static grcBlendStateHandle g_prev_BS;

static u8 g_prev_StencilRef;
static u32 g_prev_BlendFactors;
static u64 g_prev_SampleMask;

void Water::BeginRenderDepthOccluders(s32)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	PF_PUSH_TIMEBAR("Water Depth Occluders");

	// Save previous states AND ref values
	g_prev_RS = grcStateBlock::RS_Active;
	g_prev_DSS = grcStateBlock::DSS_Active;
	g_prev_BS = grcStateBlock::BS_Active;

	g_prev_StencilRef = grcStateBlock::ActiveStencilRef;
	g_prev_BlendFactors = grcStateBlock::ActiveBlendFactors;
	g_prev_SampleMask = grcStateBlock::ActiveSampleMask;

	//With batched water we add degenerate tris in which changes the winding order
	//so disable culling, cant imagine it benefits greatly from having it on.
	grcStateBlock::SetStates(BATCH_WATER_RENDERING? grcStateBlock::RS_NoBackfaceCull : grcStateBlock::RS_Default, CShaderLib::DSS_Default_Invert, grcStateBlock::BS_Default);

	GetCameraRangeForRender();

	Matrix34 worldMat = M34_IDENTITY;
	worldMat.d = Vector3((float)RenderCameraRelX, (float)RenderCameraRelY, 0.0f);
	grcViewport::SetCurrentWorldMtx(RCC_MAT34V(worldMat));

	SetWorldBase();
	grcEffect::SetGlobalVar(m_WaterHeightTextureVar, GetWaterHeightMap());

	if(m_waterShader->BeginDraw(grmShader::RMC_DRAW, true, m_WaterTechnique))
	{
		RenderLowDetailNearQuads(pass_clear);//but they take a good portion of they time, but currently godrays will break if you do
		GridSize = DYNAMICGRIDSIZE*2;//use 1/4 the normal tessellation detail
		RenderTessellatedQuads(pass_cleartess, true);
		GridSize = DYNAMICGRIDSIZE;
	}
	m_waterShader->EndDraw();

	//Not drawing for border quads since it's most likely to be nothing under them anyways

	PreviousForcedTechniqueGroupId = grmShaderFx::GetForcedTechniqueGroupId();
	grmShaderFx::SetForcedTechniqueGroupId(WaterDepthTechniqueGroup);
}

void Water::EndRenderDepthOccluders()
{
	grmShaderFx::SetForcedTechniqueGroupId(PreviousForcedTechniqueGroupId);

	grcStateBlock::SetDepthStencilState	(g_prev_DSS,g_prev_StencilRef);
	grcStateBlock::SetBlendState		(g_prev_BS,g_prev_BlendFactors,g_prev_SampleMask);
	grcStateBlock::SetRasterizerState	(g_prev_RS);

	PF_POP_TIMEBAR();
}

void Water::CausticsPreRender()
{
	GetCameraRangeForRender();
	Matrix34 worldMat = M34_IDENTITY;
	worldMat.d = Vector3((float)RenderCameraRelX, (float)RenderCameraRelY, 0.0f);
	grcViewport::SetCurrentWorldMtx(RCC_MAT34V(worldMat));
}

void Water::RenderWaterCaustics(s32)
{
	RenderTessellatedQuads(pass_invalid, false);
}

// ========================================= Paraboloid Rendering Functions ================================================
void RenderWaterRectangleParaboloid(s32 minX, s32 maxX, s32 minY, s32 maxY, float z);

void RenderWaterRectangleParaboloid_OneLayer(s32 minX, s32 maxX, s32 minY, s32 maxY, float z)
{
	Assertf(minX < maxX, "[RenderWaterRectangleParaboloid] MinX:(%d) is greater than MaxX(%d)", minX, maxX);
	Assertf(minX < maxX, "[RenderWaterRectangleParaboloid] MinY:(%d) is greater than MaxY(%d)", minY, maxY);
	s32 blockSize = m_ReflectionRenderBlockSize/16;

	s32 quadsWidth	= ((maxX - minX + blockSize-1)/blockSize);
	s32 quadsHeight	= ((maxY - minY + blockSize-1)/blockSize);

	s32 i = 0;
	s32 xStart		= minX;
	s32 xEnd		= maxX + blockSize;
	s32 yStart		= minY;
	s32 yEnd		= maxY;
	s32 vertCount	= 0;

	s32 numVerts	= 2*(quadsWidth+1)*quadsHeight + 2*quadsHeight;

	Assertf(numVerts*sizeof(WaterVertexLow) < GPU_VERTICES_MAX_SIZE, 
			"Begin vertices limit exceeded [%d bytes][%d verts][%" SIZETFMT "d size of vert][%" SIZETFMT "d bytes requested]",
			GPU_VERTICES_MAX_SIZE, numVerts, sizeof(WaterVertexLow), numVerts*sizeof(WaterVertexLow));

	WaterVertexLow* waterVertices = (WaterVertexLow*)GRCDEVICE.BeginVertices(drawTriStrip, numVerts, sizeof(WaterVertexLow));


	for(s32 y = yStart; y < yEnd; y += blockSize)
	{
		s32 x = xStart;
		waterVertices[i].x	= (s16)x;
		waterVertices[i].y	= (s16)y;
		waterVertices[i].z	= z;
		i++;
		vertCount++;
		s16 y2 = (s16)(y + blockSize);
		for(; x < xEnd; x += blockSize)
		{
			waterVertices[i].x	= (s16)x;
			waterVertices[i].y	= (s16)y;
			waterVertices[i].z	= z;
			i++;
			vertCount++;
			waterVertices[i].x	= (s16)x;
			waterVertices[i].y	= y2;
			waterVertices[i].z	= z;
			i++;
			vertCount++;
		}
		waterVertices[i].x	= (s16)(x - blockSize);
		waterVertices[i].y	= y2;
		waterVertices[i].z	= z;
		i++;
		vertCount++;
	}

	Assertf(vertCount == numVerts, "%d verts does not match %d verts passed in", vertCount, numVerts);
	GRCDEVICE.EndVertices();
}

static void SplitWaterRectangleAlongXLineParaboloid(s32 splitX, s32 minX, s32 maxX, s32 minY, s32 maxY, float z)
{
	Assertf(minX < splitX && maxX > splitX, "SplitX: %d out of range [%d, %d]", splitX, minX, maxX);		// Otherwise there is no point

	RenderWaterRectangleParaboloid(minX, splitX, minY, maxY, z);
	RenderWaterRectangleParaboloid(splitX, maxX, minY, maxY, z);
}

// Takes a water rectangle and splits it up along a line with constant Y
static void SplitWaterRectangleAlongYLineParaboloid(s32 splitY, s32 minX, s32 maxX, s32 minY, s32 maxY, float z)
{
	Assertf((minY < splitY && maxY > splitY) || (minY > splitY && maxY < splitY), "SplitY: %d out of range [%d, %d]", splitY, minY, maxY);		// Otherwise there is no point

	RenderWaterRectangleParaboloid(minX, maxX, minY, splitY, z);
	RenderWaterRectangleParaboloid(minX, maxX, splitY, maxY, z);
}

void RenderWaterRectangleParaboloid(s32 minX, s32 maxX, s32 minY, s32 maxY, float z)
{
	if (!Verifyf(minX < maxX && minY < maxY, "Trying to render water of dimensions %dx%d (x: %d to %d, y: %d to %d)", (maxX-minX), (maxY-minY), minX, maxX, minY, maxY))
	{
		return;
	}

	s32 maxSize = m_ReflectionRenderBlockSize;

	s32 xSize = maxX - minX;
	s32 ySize = maxY - minY;

	if (xSize > ySize)
	{
		if (xSize > maxSize)
		{
			SplitWaterRectangleAlongXLineParaboloid(minX + maxSize, minX, maxX, minY, maxY, z);
			return;
		}
	}
	else
	{
		if (ySize > maxSize)
		{
			SplitWaterRectangleAlongYLineParaboloid(minY + maxSize, minX, maxX, minY, maxY, z);
			return;
		}
	}

	RenderWaterRectangleParaboloid_OneLayer(minX, maxX, minY, maxY, z);
}

__inline void RenderWaterLODQuad(WaterVertexLow* waterVertices, s16 minX, s16 maxX, s16 minY, s16 maxY, float z)
{
#if __D3D11
	waterVertices[0].x = minX;
	waterVertices[0].y = minY;
	waterVertices[0].z = z;
	waterVertices[1].x = maxX;
	waterVertices[1].y = minY;
	waterVertices[1].z = z;
	waterVertices[2].x = maxX;
	waterVertices[2].y = maxY;
	waterVertices[2].z = z;

	waterVertices[3].x = minX;
	waterVertices[3].y = minY;
	waterVertices[3].z = z;
	waterVertices[4].x = maxX;
	waterVertices[4].y = maxY;
	waterVertices[4].z = z;
	waterVertices[5].x = minX;
	waterVertices[5].y = maxY;
	waterVertices[5].z = z;
#else
	waterVertices[0].x = minX;
	waterVertices[0].y = minY;
	waterVertices[0].z = z;
	waterVertices[1].x = maxX;
	waterVertices[1].y = minY;
	waterVertices[1].z = z;
	waterVertices[2].x = maxX;
	waterVertices[2].y = maxY;
	waterVertices[2].z = z;
	waterVertices[3].x = minX;
	waterVertices[3].y = maxY;
	waterVertices[3].z = z;
#endif
}

void RenderWaterLODBorderQuads(WaterVertexLow* waterVertices, s16 minX, s16 maxX, s16 minY, s16 maxY, float z)
{
#if __D3D11
	const int vertIncrement = 6;
#else
	const int vertIncrement = 4;
#endif
	s32 vertsUsed = 0;
	if(minY < sm_WorldBorderYMin)
	{
		RenderWaterLODQuad(waterVertices, minX, maxX, minY, (s16)sm_WorldBorderYMin, z);
		waterVertices	+= vertIncrement;
		vertsUsed		+= vertIncrement;
	}

	if(minX < sm_WorldBorderXMin)
	{
		RenderWaterLODQuad(waterVertices, minX, (s16)sm_WorldBorderXMin, (s16)sm_WorldBorderYMin, (s16)sm_WorldBorderYMax, z);
		waterVertices	+= vertIncrement;
		vertsUsed		+= vertIncrement;
	}

	if(maxY > sm_WorldBorderYMax)
	{
		RenderWaterLODQuad(waterVertices, minX, maxX, (s16)sm_WorldBorderYMax, maxY, z);
		waterVertices	+= vertIncrement;
		vertsUsed		+= vertIncrement;
	}

	if(maxX > sm_WorldBorderXMax)
	{
		RenderWaterLODQuad(waterVertices, (s16)sm_WorldBorderXMax, maxX, (s16)sm_WorldBorderYMin, (s16)sm_WorldBorderYMax, z);
		waterVertices	+= vertIncrement;
		vertsUsed		+= vertIncrement;
	}

#if __D3D11
	memset(waterVertices, 0, sizeof(WaterVertexLow)*(24 - vertsUsed));
#else
	memset(waterVertices, 0, sizeof(WaterVertexLow)*(16 - vertsUsed));
#endif
}

#if GS_INSTANCED_CUBEMAP
void Water::RenderWaterCubeInst()
{
	PF_PUSH_TIMEBAR("Water Cube Inst Rendering");

	m_waterShader->SetVar(m_WaterFogTextureVar, m_CurrentFogTexture);


	const s32 waterNumBlocksX = WATERNUMBLOCKSX;
	const s32 waterNumBlocksY = WATERNUMBLOCKSY;

	// it's ok to get 0 index inst viewport since cubemap cameras have the same cam position
	const grcViewport* vp	= grcViewport::GetCurrent();
	Vec3V camPos			= vp->GetCameraPosition();
	s32 camBlockX			= GetBlockFromWorldX(camPos.GetXf());
	s32 camBlockY			= GetBlockFromWorldY(camPos.GetYf());
	static dev_s32 distance	= 1;//distance in blocks to render

	const s32 blockMinX		= Clamp(camBlockX - distance	, 0, waterNumBlocksX);
	const s32 blockMaxX		= Clamp(camBlockX + distance + 1, 0, waterNumBlocksX);
	const s32 blockMinY		= Clamp(camBlockY - distance	, 0, waterNumBlocksY);
	const s32 blockMaxY		= Clamp(camBlockY + distance + 1, 0, waterNumBlocksY);

	// not rendering sky facet
	grcViewport::SetNumInstVP(5);
	grcViewport::SetViewportInstancedRenderBit(0xff);
	grcViewport::RegenerateInstVPMatrices(Mat44V(V_IDENTITY));

	GRCDEVICE.SetVertexDeclaration(m_WaterLowVertexDeclaration);

	float worldMinY			= (float)WATERBLOCKTOWORLDY(blockMinY);

	const s32 vertsPerBatch	= 48;
	s32 batchVerts			= 48;
	WaterVertexLow* waterVertices = NULL;

	if(m_waterShader->BeginDraw(grmShader::RMC_DRAW, true, m_WaterTechnique))
	{
		m_waterShader->Bind(pass_cubeinst);

		//render world quads
		float x					= (float)WATERBLOCKTOWORLDX(blockMinX);
		for (s32 blockX = blockMinX; blockX < blockMaxX; blockX++, x += WATERBLOCKWIDTH)
		{
			float y				= worldMinY;
			for (s32 blockY = blockMinY; blockY < blockMaxY; blockY++, y += WATERBLOCKWIDTH)
			{
				s32 blockIndex				= blockY + blockX*waterNumBlocksY;
				const CWaterBlockData& blockData	= m_WaterBlockData[blockIndex];
				s32 numQuads				= blockData.numQuads;
				if(numQuads == 0)
					continue;

				s32 start	= blockData.quadIndex;
				s32 end		= start + numQuads;

				for(int i = start; i < end; i++)
				{
					CWaterQuad& waterQuad	= m_WaterQuadsBuffer[i];

					if(batchVerts >= vertsPerBatch)	
					{
						waterVertices	= (WaterVertexLow*)GRCDEVICE.BeginVertices(drawTris, vertsPerBatch, sizeof(WaterVertexLow));
						batchVerts		= 0;
					}

					Assert(waterVertices);


					RenderWaterLODQuad(waterVertices, waterQuad.minX, waterQuad.maxX, waterQuad.minY, waterQuad.maxY, waterQuad.GetZ());


					waterVertices	+= 6;
					batchVerts		+= 6;
					if(batchVerts >= vertsPerBatch)
						GRCDEVICE.EndInstancedVertices();				
				}
			}
		}

		Assert(batchVerts <= vertsPerBatch);
		//End verts if it's still open
		if(batchVerts < vertsPerBatch)
		{
			memset(waterVertices, 0, sizeof(WaterVertexLow)*(vertsPerBatch - batchVerts));
			GRCDEVICE.EndInstancedVertices();
		}

		// The following can't be instanced, but since it's inside instancing algorithm
		// we need to fake it here.
		for (int i = 0; i < 5; i++)
		{
			grcViewport::SetViewportInstancedRenderBit(BIT(i));
			grcViewport::RegenerateInstVPMatrices(Mat44V(V_IDENTITY));
			//Render border quads
			Vec3V minV;
			Vec3V maxV;
			grcViewport *vp = grcViewport::GetCurrentInstVP(i);
			CalculateFrustumBounds(minV, maxV, *vp);
			minV = RoundToNearestIntNegInf(minV); // floor
			maxV = RoundToNearestIntPosInf(maxV); // ceil
			const s16 minX = (s16)minV.GetXf();
			const s16 minY = (s16)minV.GetYf();
			const s16 maxX = (s16)maxV.GetXf();
			const s16 maxY = (s16)maxV.GetYf();

			grcViewport::SetNumInstVP(1);
			waterVertices	= (WaterVertexLow*)GRCDEVICE.BeginVertices(drawTris, 24, sizeof(WaterVertexLow));
			RenderWaterLODBorderQuads(waterVertices, minX, maxX, minY, maxY, m_CurrentDefaultHeight);
			GRCDEVICE.EndInstancedVertices();
			grcViewport::SetNumInstVP(5);
		}

		m_waterShader->UnBind();
		m_waterShader->EndDraw();
	}
	grcViewport::SetNumInstVP(6);

	PF_POP_TIMEBAR();
}
#endif

void Water::RenderWaterCube()
{
	PF_PUSH_TIMEBAR("Water Cube Rendering");

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	m_waterShader->SetVar(m_WaterFogTextureVar, m_CurrentFogTexture);

	const s32 waterNumBlocksX = WATERNUMBLOCKSX;
	const s32 waterNumBlocksY = WATERNUMBLOCKSY;

	const grcViewport* vp	= grcViewport::GetCurrent();
	Vec3V camPos			= vp->GetCurrentCameraPosition();
	s32 camBlockX			= GetBlockFromWorldX(camPos.GetXf());
	s32 camBlockY			= GetBlockFromWorldY(camPos.GetYf());
	static dev_s32 distance	= 1;//distance in blocks to render

	const s32 blockMinX		= Clamp(camBlockX - distance	, 0, waterNumBlocksX);
	const s32 blockMaxX		= Clamp(camBlockX + distance + 1, 0, waterNumBlocksX);
	const s32 blockMinY		= Clamp(camBlockY - distance	, 0, waterNumBlocksY);
	const s32 blockMaxY		= Clamp(camBlockY + distance + 1, 0, waterNumBlocksY);

	grcViewport::SetCurrentWorldIdentity();

	GRCDEVICE.SetVertexDeclaration(m_WaterLowVertexDeclaration);

	float worldMinY			= (float)WATERBLOCKTOWORLDY(blockMinY);

#if __D3D11
	const s32 vertsPerBatch	= 48;
	s32 batchVerts			= 48;
#else
	const s32 vertsPerBatch	= 32;
	s32 batchVerts			= 32;
#endif
	WaterVertexLow* waterVertices = NULL;

	if(m_waterShader->BeginDraw(grmShader::RMC_DRAW, true, m_WaterTechnique))
	{
		m_waterShader->Bind(pass_cube);

		//render world quads
		float x					= (float)WATERBLOCKTOWORLDX(blockMinX);
		for (s32 blockX = blockMinX; blockX < blockMaxX; blockX++, x += WATERBLOCKWIDTH)
		{
			float y				= worldMinY;
			for (s32 blockY = blockMinY; blockY < blockMaxY; blockY++, y += WATERBLOCKWIDTH)
			{
				s32 blockIndex				= blockY + blockX*waterNumBlocksY;
				const CWaterBlockData& blockData	= m_WaterBlockData[blockIndex];
				s32 numQuads				= blockData.numQuads;
				if(numQuads == 0)
					continue;

				static dev_float heightScale = 1.0f;
				Vector3 minV = Vector3(	x,
										y,
										blockData.minHeight - heightScale);
				Vector3 maxV = Vector3(	x + WATERBLOCKWIDTH,
										y + WATERBLOCKWIDTH,
										blockData.maxHeight + heightScale);

				if(!vp->IsAABBVisible(minV, maxV, vp->GetCullFrustumLRTB()))
					continue;

				
				s32 start	= blockData.quadIndex;
				s32 end		= start + numQuads;

				for(int i = start; i < end; i++)
				{
					CWaterQuad& waterQuad	= m_WaterQuadsBuffer[i];

					if(batchVerts >= vertsPerBatch)	
					{
#if __D3D11
						waterVertices	= (WaterVertexLow*)GRCDEVICE.BeginVertices(drawTris, vertsPerBatch, sizeof(WaterVertexLow));
#else
						waterVertices	= (WaterVertexLow*)GRCDEVICE.BeginVertices(drawQuads, vertsPerBatch, sizeof(WaterVertexLow));
#endif
						batchVerts		= 0;
					}

					Assert(waterVertices);


					RenderWaterLODQuad(waterVertices, waterQuad.minX, waterQuad.maxX, waterQuad.minY, waterQuad.maxY, waterQuad.GetZ());

#if __D3D11
					waterVertices	+= 6;
					batchVerts		+= 6;
#else
					waterVertices	+= 4;
					batchVerts		+= 4;
#endif
					if(batchVerts >= vertsPerBatch)
						GRCDEVICE.EndVertices();				
				}
		
			}
		}

		//Render border quads
		Vec3V minV;
		Vec3V maxV;
		CalculateFrustumBounds(minV, maxV, *vp);
		minV = RoundToNearestIntNegInf(minV); // floor
		maxV = RoundToNearestIntPosInf(maxV); // ceil
		const s16 minX = (s16)minV.GetXf();
		const s16 minY = (s16)minV.GetYf();
		const s16 maxX = (s16)maxV.GetXf();
		const s16 maxY = (s16)maxV.GetYf();

		Assert(batchVerts <= vertsPerBatch);
		//End verts if it's still open
		if(batchVerts < vertsPerBatch)
		{
			memset(waterVertices, 0, sizeof(WaterVertexLow)*(vertsPerBatch - batchVerts));
			GRCDEVICE.EndVertices();
		}

#if __D3D11
		waterVertices	= (WaterVertexLow*)GRCDEVICE.BeginVertices(drawTris, 24, sizeof(WaterVertexLow));
#else
		waterVertices	= (WaterVertexLow*)GRCDEVICE.BeginVertices(drawQuads, 16, sizeof(WaterVertexLow));
#endif
		RenderWaterLODBorderQuads(waterVertices, minX, maxX, minY, maxY, m_CurrentDefaultHeight);
		GRCDEVICE.EndVertices();

		m_waterShader->UnBind();
		m_waterShader->EndDraw();
	}

	PF_POP_TIMEBAR();
}

void Water::RenderWaterFLOD()
{
	PF_PUSH_TIMEBAR_DETAIL("Water FLOD");

	static dev_float dist	= 23000.0f;
	grcViewport::SetCurrentWorldIdentity();
	float x					= grcViewport::GetCurrent()->GetCameraPosition().GetXf();
	float y					= grcViewport::GetCurrent()->GetCameraPosition().GetYf();
	float z					= m_CurrentDefaultHeight;

#if __PS3 //PS3 has terrible hardware clipping...
	Vec3V quadPoints[4];
	quadPoints[0]		= Vec3V(x + dist, y - dist, z);
	quadPoints[1]		= Vec3V(x + dist, y + dist, z);
	quadPoints[2]		= Vec3V(x - dist, y + dist, z);
	quadPoints[3]		= Vec3V(x - dist, y - dist, z);
	Vec3V points[6];
	grcViewport* vp		= grcViewport::GetCurrent();
	float nearClip		= vp->GetNearClip();
	Vec4V nearClipPlane = BuildPlane(vp->GetCameraPosition() - vp->GetCurrentCameraMtx().GetCol2()*ScalarV(nearClip), -vp->GetCurrentCameraMtx().GetCol2());
	s32 numPoints		= PolyClip(points, 6, quadPoints, 4, nearClipPlane);
	Assert(numPoints <= 5);
#endif

	const CMainWaterTune &mainTunings = (const CMainWaterTune &)GetMainWaterTunings();
	const CSecondaryWaterTune &secTunings		= GetSecondaryWaterTunings();
	const tcKeyframeUncompressed& currKeyframe	= g_timeCycle.GetCurrRenderKeyframe();
	float waterReflection						= currKeyframe.GetVar(TCVAR_WATER_REFLECTION);

	float bumpiness	= Lerp(secTunings.m_RippleBumpinessWindScale, secTunings.m_RippleBumpiness, secTunings.m_RippleBumpiness*g_weather.GetWind());
	bumpiness		= Clamp(bumpiness, secTunings.m_RippleMinBumpiness, secTunings.m_RippleMaxBumpiness);

	grcEffect::SetGlobalVar(m_WaterParams0Var,			
		Vec4V(bumpiness, mainTunings.m_RippleScale, mainTunings.GetSpecularIntensity(currKeyframe), mainTunings.m_SpecularFalloff));
	grcEffect::SetGlobalVar(m_WaterParams1Var,			
		Vec4V(waterReflection, mainTunings.m_FogPierceIntensity, 0.0f, secTunings.m_OceanBumpiness));
	grcEffect::SetGlobalVar(m_WaterParams2Var,			
		Vec4V(REPLAY_ONLY(CReplayMgr::IsReplayInControlOfWorld() ? 0.0f :) 1.0f, 0.0f, 0.0f, 0.0f));

	const CLightSource &dirLight	= Lights::GetRenderDirectionalLight();
	float fogLightIntensity			= currKeyframe.GetVar(TCVAR_WATER_FOGLIGHT);
	grcEffect::SetGlobalVar(m_WaterAmbientColorVar,		
		((ScalarV(-dirLight.GetDirection().z)*gWaterDirectionalColor) + gWaterAmbientColor)*ScalarV(fogLightIntensity));
	
	grcEffect::SetGlobalVar(m_StaticBumpTextureVar,	m_BumpTexture);
	grcEffect::SetGlobalVar(m_WaterBlendTextureVar,	m_BlendRT);

	m_waterShader->SetVar(m_WaterFogTextureVar,	m_CurrentFogTexture);
	m_waterShader->SetVar(m_WaterFogParamsVar,	m_GlobalFogUVParams);

	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);

	XENON_ONLY(PostFX::SetLDR8bitHDR10bit();)

	AssertVerify(m_waterShader->BeginDraw(grmShader::RMC_DRAW, true, m_WaterTechnique));
	m_waterShader->Bind(pass_flod);
#if __PS3
	if(Likely(numPoints > 0))
	{
		grcBegin(drawTriFan, numPoints);
		for(int i = 0; i < numPoints; i++)
			grcVertex(points[i].GetXf(), points[i].GetYf(), z, 0.0f, 0.0f, 0.0f, Color32(), 0.0f,0.0f);
		grcEnd();
	}
#else
	grcBegin(drawTriStrip, 4);
	grcVertex(x - dist, y + dist, z, 0.0f, 0.0f, 1.0f, Color32(), 0.0f, 0.0f);
	grcVertex(x - dist, y - dist, z, 0.0f, 0.0f, 1.0f, Color32(), 0.0f, 1.0f);
	grcVertex(x + dist, y + dist, z, 0.0f, 0.0f, 1.0f, Color32(), 1.0f, 0.0f);
	grcVertex(x + dist, y - dist, z, 0.0f, 0.0f, 1.0f, Color32(), 1.0f, 1.0f);
	grcEnd();
#endif //__PS3
	m_waterShader->UnBind();
	m_waterShader->EndDraw();

	XENON_ONLY(PostFX::SetLDR10bitHDR10bit();)

	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);

#if __BANK
	m_TotalDrawCalls++;
	m_WaterFLODDrawCalls++;
#endif

	PF_POP_TIMEBAR_DETAIL();
}

void Water::RenderWaterFLODDisc(s32 pass, float z)
{	
	PF_PUSH_TIMEBAR("Water FLOD Disc");

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	static dev_float dist	= 23000.0f;
	grcViewport::GetCurrent()->SetWorldIdentity();

	float x	= grcViewport::GetCurrent()->GetCameraPosition().GetXf();
	float y	= grcViewport::GetCurrent()->GetCameraPosition().GetYf();

	if(m_waterShader->BeginDraw(grmShader::RMC_DRAW, true, m_WaterTechnique))
	{
		m_waterShader->Bind(pass);
		grcBegin(drawTriStrip, 4);
		grcVertex(x - dist, y + dist, z, 0.0f, 0.0f, 1.0f, Color32(), 0.0f, 0.0f);
		grcVertex(x - dist, y - dist, z, 0.0f, 0.0f, 1.0f, Color32(), 0.0f, 1.0f);
		grcVertex(x + dist, y + dist, z, 0.0f, 0.0f, 1.0f, Color32(), 1.0f, 0.0f);
		grcVertex(x + dist, y - dist, z, 0.0f, 0.0f, 1.0f, Color32(), 1.0f, 1.0f);
		grcEnd();
		m_waterShader->UnBind();
	}
	m_waterShader->EndDraw();

	PF_POP_TIMEBAR();
}

s32 Water::GetWaterUpdateIndex()
{
	if(m_BuffersFlipped)
		return 1 - ms_bufIdx;
	else
		return ms_bufIdx;
}
s32 Water::GetWaterRenderIndex(){ return 1 - ms_bufIdx; }
s32 Water::GetWaterCurrentIndex(){
	if(sysThreadType::IsRenderThread())
		return GetWaterRenderIndex();
	else
		return GetWaterUpdateIndex();
}

void Water::UnflipWaterUpdateIndex()
{
	m_BuffersFlipped = false;
}

bool Water::IsUsingMirrorWaterSurface()
{
#if __BANK
	if (m_bForceMirrorReflectionOn)  { return true; }
	if (m_bForceMirrorReflectionOff) { return false; }
#endif // __BANK

	return fwScan::GetScanResults().GetWaterSurfaceVisible() && CMirrors::GetIsMirrorVisible(true);
}

// Start recording and playing back a recording when this returns true.
//			  The playback will be in synch with the water.
bool Water::SynchRecordingWithWater()
{
	return ((fwTimer::GetTimeInMilliseconds() / m_WaveTimePeriod) != (fwTimer::GetPrevElapsedTimeInMilliseconds() / m_WaveTimePeriod));
}


static inline bool GetGroundLevel(const Vector3& pos, float *groundZ, float H)
{
	WorldProbe::CShapeTestFixedResults<> probeResult;
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetStartAndEnd(pos + Vector3(0.0f, 0.0f, H), pos);
	probeDesc.SetResultsStructure(&probeResult);
	probeDesc.SetExcludeEntity(NULL);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);

	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
	{
		physicsAssert(probeResult[0].GetHitDetected());
		(*groundZ) = probeResult[0].GetHitPosition().z;
		return true;
	}
		
	return false;
}


bool Water::GetWaterDepth(const Vector3& pos, float *pWaterDepth, float *pWaterZ, float *pGroundZ)
{

	float waterZ = 0.0f;
	float groundZ = 0.0f;

	if(GetWaterLevelNoWaves(pos, &waterZ, POOL_DEPTH, REJECTIONABOVEWATER, NULL))
	{
		// check for ground below water surface:
		if(!GetGroundLevel(pos, &groundZ,30))
		{
			// assume there's no bottom ground (water is VERY deep):
			groundZ = -100.0f;
		}
							
		// at least 3 meters of vertical water space:
		if(pWaterDepth)
			*pWaterDepth = waterZ - groundZ;
	
		if(pWaterZ)
			*pWaterZ = waterZ;

		if(pGroundZ)
			*pGroundZ = groundZ;
			
		return(true);
	}
	
	return(false);
}


void UpdateBumpMap(float x, float y, u32/* pixels*/)
{
	s32 b = GetWaterRenderIndex();

#if RSG_PC
	if (GRCDEVICE.GetGPUCount(true) > 1)
		b = s_GPUWaterBumpShaderParams[GRCDEVICE.GPUIndex()]._doubleBufferID;
#endif //RSG_PC

	grcRenderTarget* bumpHeightRT = m_BumpHeightRT[b];
	m_WaterTextureShader->SetVar(m_BumpHeightTextureVar, m_BumpHeightRT[1 - b]);

#if MULTIPLE_RENDER_THREADS
	m_WaterTextureShader->SetVar(m_NoiseTextureVar,			m_noiseTexture);
#endif

	//Update height velocity and bump
	const grcRenderTarget* rendertargets[grcmrtColorCount] = {bumpHeightRT,	m_BumpRT, NULL, NULL};
	grcTextureFactory::GetInstance().LockMRT(rendertargets, NULL);

	AssertVerify(m_WaterTextureShader->BeginDraw(grmShader::RMC_DRAW, true, m_BumpTechnique));
	m_WaterTextureShader->Bind(pass_updatemrt);
	grcDrawSingleQuadf(-1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, Color32(x, y, 0.0f, 0.0f));
	m_WaterTextureShader->UnBind();
	m_WaterTextureShader->EndDraw();

	grcTextureFactory::GetInstance().UnlockMRT();

#if RSG_ORBIS
	m_BumpRT->GenerateMipmaps();
#endif

	if(g_weather.GetRain() > 0.0f)
	{
		m_WaterTextureShader->SetVar(m_FullTextureVar,			m_BumpRT);
		m_WaterTextureShader->SetVar(m_PointTexture2Var,		PuddlePassSingleton::GetInstance().GetRippleTexture());
		m_WaterTextureShader->SetVar(m_WaterCompositeParamsVar,	Vec4V(g_weather.GetRain(), 0.0f, 0.0f, 0.0f));
		grcTextureFactory::GetInstance().LockRenderTarget(0, m_BumpRainRT, NULL);

		WaterScreenBlit(m_WaterTextureShader, m_BumpTechnique, pass_copyrain);

		grcResolveFlags* resolveFlags = NULL;
#if __D3D11
		grcResolveFlags resolveMips;
		resolveMips.MipMap = true;
		resolveFlags = &resolveMips;
#endif
		grcTextureFactory::GetInstance().UnlockRenderTarget(0, resolveFlags);

#if RSG_ORBIS
		m_BumpRainRT->GenerateMipmaps();
#endif

		m_CurrentBumpTexture	= m_BumpRainRT;
	}
	else
	{
		m_CurrentBumpTexture	= m_BumpRT;
	}
	
	m_CurrentBump2Texture	= m_BumpRT;

#if RSG_PC
	if (GRCDEVICE.GetGPUCount(true) > 1)
		s_GPUWaterBumpShaderParams[GRCDEVICE.GPUIndex()]._doubleBufferID ^= 1;
#endif //RSG_PC
}

#if RSG_PC
void Water::UpdatePreviousWaterBumps(bool currentFrameDrawn) {
	for (u32 drawCount = 1; drawCount < GRCDEVICE.GetGPUCount(true); drawCount++)
	{
		u32 currentReadIndex = (GRCDEVICE.GPUIndex() + drawCount) % GRCDEVICE.GetGPUCount(true);
		const WaterBumpShaderParameters& params = s_GPUWaterBumpShaderParams[currentReadIndex];

		if (params._inUse){
			m_WaterTextureShader->SetVar(WaterSurfaceSimParams_ID, params._params, 2);
			UpdateBumpMap(params._offx, params._offy, params._pixels);
		}
	}

	s_GPUWaterBumpShaderParams[GRCDEVICE.GPUIndex()]._inUse = currentFrameDrawn;
}
#endif

void Water::UpdateWaterBumpTexture(u32 pixels)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

#if __BANK
	if (TiledScreenCapture::IsEnabled())
		return;
#endif // __BANK
	PF_PUSH_TIMEBAR_DETAIL("Water Bump Map");

	static float g_fRandomOffX = 0.0f;
	static float g_fRandomOffY = 0.0f;
	static float g_fTimeStep = 0.0f;
	static float g_fElapsedTime = 0.0f;


	{
		g_fRandomOffX = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
		g_fRandomOffY = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);

#if __WIN32PC
//		float t = (GRCDEVICE.GetGPUCount() > 1) ? (rage::Floorf(fwTimer::GetTimeStep() * 1000.0f) / 1000.0f) : fwTimer::GetTimeStep();
		float t = fwTimer::GetTimeStep();
#else
		float t = fwTimer::GetTimeStep();
#endif
		g_fTimeStep = fwTimer::IsGamePaused() ? 0.0f : t;

#if GTA_REPLAY
		//clamp the maximum time step during replay, otherwise jumping about can produce weird graphical issues
		if(CReplayMgr::IsReplayInControlOfWorld())
		{
			g_fTimeStep = rage::Min(g_fTimeStep, 0.066f);
		}
#endif

		g_fElapsedTime = TIME.GetElapsedTime();
	}

	//normalized - passed in as colour
	float offx = g_fRandomOffX;
	float offy = g_fRandomOffY;

	grcStateBlock::SetStates(grcStateBlock::RS_Default, grcStateBlock::DSS_IgnoreDepth, grcStateBlock::BS_Default);

	Vector4 t_waterSurfaceTextureSimParams[2];

	t_waterSurfaceTextureSimParams[0] = Vector4( m_waterSurfaceTextureSimParams[0], m_waterSurfaceTextureSimParams[1], m_waterSurfaceTextureSimParams[2], m_waterSurfaceTextureSimParams[3]);
	t_waterSurfaceTextureSimParams[1] = Vector4( m_waterSurfaceTextureSimParams[4], m_waterSurfaceTextureSimParams[5], m_waterSurfaceTextureSimParams[6], m_waterSurfaceTextureSimParams[7]);

	// static dev_float ooNormalStep=1.0f/50.0f;

	float timeScale = g_fTimeStep;

	t_waterSurfaceTextureSimParams[1].z = g_fElapsedTime;
	t_waterSurfaceTextureSimParams[1].w = timeScale * GetSecondaryWaterTunings().m_RippleSpeed;
	t_waterSurfaceTextureSimParams[1].x = GetSecondaryWaterTunings().m_RippleVelocityTransfer;
	t_waterSurfaceTextureSimParams[1].y = GetSecondaryWaterTunings().m_RippleDisturb;

	CRenderTargets::UnlockSceneRenderTargets();

#if __WIN32PC && __D3D11
	if(GRCDEVICE.UsingMultipleGPUs())
	{
		UpdatePreviousWaterBumps(true);

		//Store the current frame's shader parameters for re-draw next frame
		s_GPUWaterBumpShaderParams[GRCDEVICE.GPUIndex()]._params[0] = t_waterSurfaceTextureSimParams[0];
		s_GPUWaterBumpShaderParams[GRCDEVICE.GPUIndex()]._params[1] = t_waterSurfaceTextureSimParams[1];
		s_GPUWaterBumpShaderParams[GRCDEVICE.GPUIndex()]._offx = offx;
		s_GPUWaterBumpShaderParams[GRCDEVICE.GPUIndex()]._offy = offy;
		s_GPUWaterBumpShaderParams[GRCDEVICE.GPUIndex()]._pixels = pixels;
	}

#endif

	m_WaterTextureShader->SetVar(WaterSurfaceSimParams_ID, t_waterSurfaceTextureSimParams, 2);


	UpdateBumpMap(offx, offy, pixels);

	CRenderTargets::LockSceneRenderTargets();
	
	PF_POP_TIMEBAR_DETAIL();
}

// Change the vertical water speed at these coordinates in world space.
//			  The old speed gets dragged towards the newspeed (m/s) to the amount of
//			  ChangePercentage (0.0f - 1.0f)
void	Water::ModifyDynamicWaterSpeed(float WorldX, float WorldY, float NewSpeed, float ChangePercentage)
{
	BANK_ONLY(m_DisturbCount++);

	int b	= GetWaterUpdateIndex();

	s32 gridX = rage::round(WorldX/DYNAMICGRIDSIZE)*DYNAMICGRIDSIZE;
	s32 gridY = rage::round(WorldY/DYNAMICGRIDSIZE)*DYNAMICGRIDSIZE;

	atFixedArray<WaterDisturb, MAXDISTURB>* disturbBuffer	= &m_WaterDisturbBuffer[b];
	atFixedArray<u8, MAXDISTURB>* disturbStack				= &m_WaterDisturbStack[b];
	s32 count=disturbBuffer->GetCount();
	if(disturbBuffer->IsFull())
		return;

	for(s32 i=0; i<count; i++)
	{
		if(gridX == (*disturbBuffer)[i].m_x)
			if(gridY == (*disturbBuffer)[i].m_y)
			{
				s32 stack	= (*disturbStack)[i];
				if(stack < 2)
				{
					(*disturbBuffer)[i].m_amount[stack]	= NewSpeed;
					(*disturbBuffer)[i].m_change[stack]	= ChangePercentage;
					(*disturbStack)[i]					= (u8)stack++;
				}
				return;
			}
	}

	

	WaterDisturb d;
	d.m_x					= (s16)gridX;
	d.m_y					= (s16)gridY;
	d.m_amount[0]			= NewSpeed;
	d.m_change[0]			= ChangePercentage;
	d.m_amount[1]			= 0.0f;
	d.m_change[1]			= 0.0f;
	disturbStack->Append()	= 1;
	disturbBuffer->Append()	= d;
}

int Water::AddExtraCalmingQuad(int minX, int minY, int maxX, int maxY, float dampening)
{
	if( sm_ExtraQuadsCount < MAXEXTRACALMINGQUADS )
	{
		for(int i=0;i<MAXEXTRACALMINGQUADS;i++)
		{
			if( sm_ExtraCalmingQuads[i].m_fDampening == 1.0f )
			{
				sm_ExtraCalmingQuads[i].minX = (s16)(minX & 0xFFFF);
				sm_ExtraCalmingQuads[i].minY = (s16)(minY & 0xFFFF);
				sm_ExtraCalmingQuads[i].maxX = (s16)(maxX & 0xFFFF);
				sm_ExtraCalmingQuads[i].maxY = (s16)(maxY & 0xFFFF);
				sm_ExtraCalmingQuads[i].m_fDampening = dampening;
				sm_ExtraQuadsCount++;
				return i;
			}
		}
	}
	
	return -1;
}

void Water::RemoveExtraCalmingQuad(int idx)
{
	if( idx < MAXEXTRACALMINGQUADS )
	{
		sm_ExtraCalmingQuads[idx].m_fDampening = 1.0f;
		sm_ExtraQuadsCount--;
	}
}

void Water::RemoveAllExtraCalmingQuads()
{
	if(sm_ExtraQuadsCount > 0)
	{
		for(u32 i=0; i<MAXEXTRACALMINGQUADS; i++)
		{
			if(sm_ExtraCalmingQuads[i].m_fDampening != 1.0f)
			{
				RemoveExtraCalmingQuad(i);
			}
		}
	}
	
	sm_ExtraQuadsCount = 0;
}

void Water::SetScriptDeepOceanScaler(float s)
{
	sm_ScriptDeepOceanScaler = s;
}

void Water::SetScriptCalmedWaveHeightScaler(float s)
{
	sm_ScriptCalmedWaveHeightScaler = s;
}


float Water::GetScriptDeepOceanScaler()
{
	return sm_ScriptDeepOceanScaler;
}

void	Water::AddFoam(float worldX, float worldY, float amount)
{
	int b	= GetWaterUpdateIndex();

	s32 gridX = rage::round(worldX/DYNAMICGRIDSIZE)*DYNAMICGRIDSIZE;
	s32 gridY = rage::round(worldY/DYNAMICGRIDSIZE)*DYNAMICGRIDSIZE;

	atFixedArray<WaterFoam, MAXFOAM>* foamBuffer	= &m_WaterFoamBuffer[b];

	if(foamBuffer->IsFull())
		return;

	WaterFoam foam;
	foam.m_x				= (s16)gridX;
	foam.m_y				= (s16)gridY;
	foam.m_amount			= amount;
	foamBuffer->Append()	= foam;

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordFx<CPacketWaterFoam>(CPacketWaterFoam(worldX, worldY,  amount));
	}
#endif
}

#if __BANK

// Goes through the water and finds the nearest height. This is used for distant light generation.
float Water::FindNearestWaterHeightFromQuads(float TestX, float TestY)
{
	float	NearestDistFound = 9999999.9f;
	float	NearestDistFoundIgnoringFlatWater = 9999999.9f;
	float	HeightOfNearestWaterIgnoringFlatWater = 0.0f;
	float	XDist, YDist, Distance;

	if (TestX <= sm_WorldBorderXMin || TestX >= sm_WorldBorderXMax ||
		TestY <= sm_WorldBorderYMin || TestY >= sm_WorldBorderYMax)
	{
		return 0.0f;
	}

	s32 count = m_WaterQuadsBuffer.GetCount();
	for (s32 Quads = 0; Quads < count; Quads++)
	{
		// Test the distance to this quad.
		if (TestX < m_WaterQuadsBuffer[Quads].minX)
			XDist = m_WaterQuadsBuffer[Quads].minX - TestX;
		else if (TestX > m_WaterQuadsBuffer[Quads].maxX)
			XDist = TestX - m_WaterQuadsBuffer[Quads].maxX;
		else
			XDist = 0.0f;

		if (TestY < m_WaterQuadsBuffer[Quads].minY)
			YDist = m_WaterQuadsBuffer[Quads].minY - TestY;
		else if (TestY > m_WaterQuadsBuffer[Quads].maxY)
			YDist = TestY - m_WaterQuadsBuffer[Quads].maxY;
		else
			YDist = 0.0f;

		Distance = rage::Sqrtf(XDist * XDist + YDist * YDist);

		if (Distance < NearestDistFoundIgnoringFlatWater)
		{
			NearestDistFoundIgnoringFlatWater = Distance;
			HeightOfNearestWaterIgnoringFlatWater = m_WaterQuadsBuffer[Quads].GetZ();
		}

		if (Distance < NearestDistFound)
			NearestDistFound = Distance;
	}

	return NearestDistFoundIgnoringFlatWater;	// return value only used to build the distant light file.
}

void ResetTestBuoys(){

	Vector3 camPos =  camInterface::GetPos();
	float xPos = camPos.x - 9.0f;
	for(s32 i = 0; i < 8; i++)
	{
		float yPos = camPos.y - 9.0f;
		for(s32 j = 0; j < 8; j++)
		{
			m_WaterBuoyBuffer[i*8+j].pos.x = xPos;
			m_WaterBuoyBuffer[i*8+j].pos.y = yPos;
			yPos += 2.0f;
		}
		xPos += 2.0f;
	}
}

void SaveWaterData()
{
	CWaterData waterData;
	waterData.m_WaterQuads.Reserve(		m_WaterQuadsBuffer.GetCount());
	waterData.m_CalmingQuads.Reserve(	m_CalmingQuadsBuffer.GetCount());
	waterData.m_WaveQuads.Reserve(		m_WaveQuadsBuffer.GetCount());

	s32 count = m_WaterQuadsBuffer.GetCount();
	for(s32 i = 0 ; i< count; i++)
		waterData.m_WaterQuads.Append() = CWaterQuadParsable(m_WaterQuadsBuffer[i]);

	count = m_CalmingQuadsBuffer.GetCount();
	for(s32 i = 0 ; i< count; i++)
		waterData.m_CalmingQuads.Append() = CCalmingQuadParsable(m_CalmingQuadsBuffer[i]);
	
	count = m_WaveQuadsBuffer.GetCount();
	for(s32 i = 0 ; i< count; i++)
		waterData.m_WaveQuads.Append() = CWaveQuadParsable(m_WaveQuadsBuffer[i]);
		
	Displayf("saving water data to %s", "common:/data/waterout");
	Assertf(PARSER.SaveObject("common:/data/waterout", "xml", &waterData), "Saving water data failed!");
}

void SaveWaterTunings()
{
	Displayf("saving water data to %s", "common:/data/watertune");
	Assertf(PARSER.SaveObject("common:/data/watertune", "xml", &m_MainWaterTunings), "Saving water tunings failed!");
}

void Water::UpdateShaderEditVars()
{
	s32 count = m_TexChangeDataBuffer.GetCount();
	s32 b = GetWaterUpdateIndex();
	for(s32 i = 0; i < count; i++)
	{
		if(m_TexChangeTextureBuffer[b][i].m_MarkForDelete)
			m_TexChangeTextureBuffer[b][i].m_Texture->Release();

		m_TexChangeTextureBuffer[b][i] = m_TexChangeTextureBuffer[1-b][i];

		if (m_TexChangeDataBuffer[i].m_ready)
		{
			if(m_TexChangeTextureBuffer[b][i].m_Texture)
				m_TexChangeTextureBuffer[1-b][i].m_MarkForDelete = true;

			grcTexture *texture = NULL;
			const char* ext = strrchr(m_TexChangeDataBuffer[i].m_filename, '.');

			if (ext && stricmp(ext + 1, grcTexture::GetTxdExtension()) == 0) // is it a TXD?
			{
				texture = grcTexture::RunCustomLoadFunc(m_TexChangeDataBuffer[i].m_filename);
			}
			else // no, it's probably just a DDS
			{
				texture = grcTextureFactory::GetInstance().Create(m_TexChangeDataBuffer[i].m_filename);
			}
			
			Assert(texture);
			m_TexChangeTextureBuffer[b][i].m_Texture = texture;
			// Reset
			m_TexChangeDataBuffer[i].m_ready = false;
		}
	}
}


void ReloadWaterSPUJob()
{
// Had trouble using the macro below so just implemented it myself
//	RELOAD_TASK_BINARY(WaterSPU);
#if __PS3 && __DEV
	if (rage::fiStream *elfFile=rage::ASSET.Open("c:\\spu_debug\\WaterSPU_job", "bin")) 
	{
		unsigned int fileSize = elfFile->Size();
		Displayf("Reloaded job binary c:\\spu_debug\\WaterSPU_job.bin, used %d bytes", fileSize); 
		void *elf=memalign(128, fileSize);			
		elfFile->Read(elf, fileSize);				
		elfFile->Close();							
		::reload_WaterSPU_start = (char*)elf;		
		::reload_WaterSPU_size = (char*)fileSize;	
	}												
	else											
	{												
		Warningf("Job binary c:\\spu_debug\\WaterSPU_job.bin was not found"); 
	}
#endif

}

void Water::MakeWaterFlat()
{
	m_MainWaterTunings.m_RippleScale        = 0.0f;

	g_weather.MakeWaterFlat();

	m_bResetSim                            = true;
}

void Water::InitOptimizationWidgets()
{
	bkBank* pOptimisationsBank = BANKMGR.FindBank("Optimization");
	if(pOptimisationsBank)
	{
		pOptimisationsBank->AddToggle("Draw Water Quads", &m_DrawWaterQuads);
		pOptimisationsBank->AddToggle("Draw Water Depth Occluders", &m_DrawDepthOccluders);
	}
}

static void CalmingQuadEnabled()
{
	sm_ExtraQuadsCount = 0;
	for(int i=0;i<MAXEXTRACALMINGQUADS;i++)
	{
		if( sm_ExtraCalmingQuads[i].m_fDampening < 1.0f  )
		{
			sm_ExtraQuadsCount++;
		}
	}
}

static void bkReloadWaterTunings()
{
	if(Water::GetGlobalWaterXmlFile() == 1)
	{
		LoadWaterTunings("common:/data/watertune_HeistIsland.xml");
	}
	else
	{
		LoadWaterTunings();
	}
}

void Water::InitWidgets()
{
	bkBank *pMainBank = BANKMGR.FindBank("Renderer");
	Assert(pMainBank);
	pMainBank->PushGroup("Water", false);
	bkBank& bank = *pMainBank;

	bank.AddButton("Make Water Flat", MakeWaterFlat);
	bank.AddSlider("Water test offset", &m_waterTestOffset, -10.0f, 10.0f, 1.0f/32.0f);
	bank.AddToggle("Water pixels rendered force ON", &m_bForceWaterPixelsRenderedON);
	bank.AddToggle("Water pixels rendered force OFF", &m_bForceWaterPixelsRenderedOFF);
	bank.AddToggle("Water reflection force ON", &m_bForceWaterReflectionON);
	bank.AddToggle("Water reflection force OFF", &m_bForceWaterReflectionOFF);
	bank.AddToggle("Water force mirror reflection for planar reflection ON", &m_bForceMirrorReflectionOn);
	bank.AddToggle("Water force mirror reflection for planar reflection OFF", &m_bForceMirrorReflectionOff);
	bank.AddToggle("Water use underwater planar reflection mode", &m_bUnderwaterPlanarReflection);
	bank.AddSeparator();
	const char* waterReflectionSourceStrings[] =
	{
		"Default",
		"Black", // use grcTexture::NoneBlack
		"White", // use grcTexture::NoneWhite
		"Sky Only",
	};
	CompileTimeAssert(NELEM(waterReflectionSourceStrings) == WATER_REFLECTION_SOURCE_COUNT);
	bank.AddCombo("Water reflection source",	&m_debugReflectionSource, NELEM(waterReflectionSourceStrings), waterReflectionSourceStrings);
#if WATER_SHADER_DEBUG
	bank.AddSlider("Water alpha bias",			&m_debugAlphaBias, -1.0f, 1.0f, 1.0f/32.0f);
	bank.AddSlider("Water refraction override",	&m_debugRefractionOverride, 0.0f, 1.0f, 1.0f/32.0f);
	bank.AddSlider("Water reflection override",	&m_debugReflectionOverride, 0.0f, 1.0f, 1.0f/32.0f);
	bank.AddSlider("Water refl. paraboloid",	&m_debugReflectionOverrideParab, 0.0f, 1.0f, 1.0f/32.0f);
	bank.AddSlider("Water refl. brightness",	&m_debugReflectionBrightnessScale, 0.0f, 16.0f, 1.0f/32.0f);
	bank.AddToggle("Water fog bias enabled",	&m_debugWaterFogBiasEnabled);
	bank.AddSlider("Water fog depth bias",		&m_debugWaterFogDepthBias, -10.0f, 10.0f, 1.0f/32.0f);
	bank.AddSlider("Water fog refract bias",	&m_debugWaterFogRefractBias, -1.0f, 1.0f, 1.0f/32.0f);
	bank.AddSlider("Water fog colour bias",		&m_debugWaterFogColourBias, -1.0f, 1.0f, 1.0f/32.0f);
	bank.AddSlider("Water fog shadow bias",		&m_debugWaterFogShadowBias, -1.0f, 1.0f, 1.0f/32.0f);
	bank.AddToggle("Water distortion enabled",	&m_debugDistortionEnabled);
	bank.AddSlider("Water distortion scale",	&m_debugDistortionScale, 0.0f, 8.0f, 1.0f/32.0f);
	bank.AddSlider("Water debug highlight",		&m_debugHighlightAmount, 0.0f, 1.0f, 1.0f/32.0f);
	bank.AddSeparator();
#endif // WATER_SHADER_DEBUG

#if __PPU
	bank.AddToggle("SPU: Use SPU",				&gbSpuUseSpu);
	bank.AddToggle("SPU: Wait for SPU",			&gbSpuWaitForSpu);
#endif //__PPU
	bank.AddSlider("Update Offset Index",		&m_WaterHeightMapUpdateIndexOffset, -2, 2, 1);

#if USE_WATER_THREAD
	bank.AddToggle("Output CPU timings",		&gbOutputTimingsCPU);
#endif // USE_WATER_THREAD

	bank.AddButton("ReloadWaterSPUJob", CFA(ReloadWaterSPUJob));
	bank.AddToggle("Use Frustum Test", &m_bUseFrustumCull);
	bank.AddToggle("Render water into rain heightmap", &m_bWaterHeightMapRender);
	bank.AddToggle("Reset surface sim", &m_bResetSim);
	bank.AddToggle("Wire Frame Water", &m_bWireFrame);
	bank.AddToggle("Triangles as Quads", &m_TrianglesAsQuads);
	bank.AddToggle("Triangles as Quads for Occlusion", &m_TrianglesAsQuadsForOcclusion);
	bank.AddToggle("Rasterize Water Quads into Stencil", &m_RasterizeWaterQuadsIntoStencil);
	bank.AddToggle("Rasterize River Boxes into Stencil", &m_RasterizeRiverBoxesIntoStencil);
	bank.AddToggle("Rasterize River Boxes into Stencil Debug Draw", &m_RasterizeRiverBoxesIntoStencilDebugDraw);
	bank.AddToggle("Use Camera Position", &m_bUseCamPos);
	bank.AddToggle("Freeze Camera Update", &m_bFreezeCameraUpdate);
	bank.AddToggle("Update dynamic water", &m_bUpdateDynamicWater);

#if __WIN32PC && __D3D11
	bank.AddToggle("Extra Quads", &m_bEnableExtraQuads);
#endif

	bank.AddToggle("Use 16x16 Block Render", &m_Use16x16Block);
	bank.AddSlider("Water Opacity for Underwater VFX", &m_WaterOpacityUnderwaterVfx, 0.0f, 1.0f, 0.001f);

#if __GPUUPDATE
	if(m_CPUUpdateEnabled && m_GPUUpdateEnabled)
		bank.AddToggle("GPU Update", &m_GPUUpdate);
	if(m_GPUUpdateEnabled)
	{
		bank.AddText("Packed Disturb Count", &m_PackedDisturbCount);
		bank.AddToggle("Height GPU Readback", &m_HeightGPUReadBack);
	}
#endif //__GPUUPDATE

	bank.AddSlider("Hit Detection Wave Height Range", &m_WaveHeightRange, 0.0f, 10.0f, 0.5f);
	bank.AddSlider("disturb scale", &m_DisturbScale,0.0f,100.0f, 1.0f);

	bank.AddSlider("wave length scale", &m_WaveLengthScale, 0.0f, 10.0f, 0.05f );
	bank.AddSlider("wave time period", &m_WaveTimePeriod, 0, 100000, 1000 );
	bank.AddSlider("time step scale", &m_TimeStepScale, 0.0f, 10.0f, 0.1f);

	bank.AddToggle("Display debug images", &m_bDisplayDebugImages);
	bank.AddSlider("acc scale - adjacent transfer", &m_waterSurfaceTextureSimParams[0], 0.0f, 1.0f, 0.01f );
	bank.AddSlider("acc scale - restore to rest position", &m_waterSurfaceTextureSimParams[1], 0.0f, 1.0f, 0.01f );
	bank.AddSlider("vel scale - acc transfer", &m_waterSurfaceTextureSimParams[3], 0.0f, 1.0f, 0.01f );
	bank.AddSlider("pos scale - vel transfer", &m_waterSurfaceTextureSimParams[4], 0.0f, 1.0f, 0.01f );
	bank.AddSlider("disturbance scale", &m_waterSurfaceTextureSimParams[5], 0.0f, 1.0f, 0.01f );
	bank.AddSlider("temp 0", &m_waterSurfaceTextureSimParams[6], 0.0f, 100.0f, 0.01f );
	bank.AddSlider("temp 1", &m_waterSurfaceTextureSimParams[7], 0.0f, 5.0f, 0.01f );
	bank.AddSlider("Refraction Index", &m_RefractionIndex, 0.0f, 2.0f, 0.1f);

	bank.AddTitle("Debug Info:");
	bank.AddText("Num Quads Drawn", &m_NumQuadsDrawn);
	bank.AddText("Num Blocks Drawn", &m_NumBlocksDrawn);
	bank.AddText("16x16 Quads Drawn", &m_16x16Drawn);
	bank.AddText("High Detail Drawn", &m_HighDetailDrawn);
	bank.AddText("Disturb Count", &m_DisturbCountRead);
	bank.AddText("Num Water Quads", &m_nNumOfWaterQuads);
	bank.AddText("Num Calming Quads", &m_nNumOfWaterCalmingQuads);
	bank.AddText("Num Wave Quads", &m_nNumOfWaveQuads);
	bank.AddText("Num Water Tris", &m_NumWaterTris);
	bank.AddText("Camera Flat Height", &m_CameraFlatWaterHeight[0]);
	bank.AddText("AverageSimHeight", &m_AverageSimHeight);
	bank.AddText("Reflection Render Range", &m_ReflectionRenderRange);
	bank.AddText("Reflection Render Block Size", &m_ReflectionRenderBlockSize);
	bank.AddText("Update Time", &m_UpdateTime);
#if __PS3
	bank.AddText("SPU Wait Time", &m_SpuWaitTime);
#endif //__PS3

#if __GPUUPDATE
	bank.AddText("GPU Update RT Time", &m_GPUUpdateTime);
	bank.AddText("Calming Quads Drawn", &m_NumCalmingQuadsDrawn);
	bank.AddText("Wave Quads Drawn", &m_NumWaveQuadsDrawn);
#endif //__GPUUPDATE

	bank.AddTitle("Draw Call Info:");
	bank.AddText("Total Draw Calls", &m_TotalDrawCallsDisplay);
	bank.AddText("High Detail Quad Draw Calls", &m_HighDetailDrawCallsDisplay);
	bank.AddText("Flat water Quad Draw Calls", &m_FlatWaterQuadDrawCallsDisplay);
	bank.AddText("Water FLOD Draw Calls", &m_WaterFLODDrawCallsDisplay);

#if BATCH_WATER_RENDERING
	bank.AddToggle("Batch water rendering", &m_EnableBatchWaterRendering);
#endif //BATCH_WATER_RENDERING	

	bank.AddToggle("Debug Draw Calming Quads", &m_DrawCalmingQuads);
	bank.AddToggle("Draw Water Quads", &m_DrawWaterQuads);
	bank.AddToggle("Draw Water Depth Occluders", &m_DrawDepthOccluders);
	bank.AddToggle("Disable Tessellated Quads", &m_DisableTessellatedQuads);
#if __USEVERTEXSTREAMRENDERTARGETS
	bank.AddToggle("Use VSRT Tessellation", &m_UseVSRT);
	bank.AddToggle("VSRT Render All Blocks", &m_VSRTRenderAll);
#endif //__USEVERTEXSTREAMRENDERTARGETS
	bank.AddToggle("Enable Fog Streaming",	&m_EnableFogStreaming);
	bank.AddToggle("Ignore TC Fog Streaming", &m_IgnoreTCFogStreaming);
#if RSG_PC
	bank.AddToggle("Enabled DX10 water fog", &m_UseAlternateFogStreamingTxd);
#endif // RSG_PC
	bank.AddToggle("Enable Fog Prepass",	&m_EnableWaterLighting);
#if WATER_TILED_LIGHTING
	bank.AddToggle("FogPrepass Use HiZ", &m_EnableFogPrepassHiZ);
	bank.AddToggle("Use Tiled Fog Composite", &m_UseTiledFogComposite);
#endif
	bank.AddSlider("Composite refraction tex X offset", &m_FogCompositeTexOffset.x, -1.0f, 1.0f, 0.1f);
	bank.AddSlider("Composite refraction tex Y offset", &m_FogCompositeTexOffset.y, -1.0f, 1.0f, 0.1f);

	bank.AddToggle("Enable Visibility",		&m_EnableVisibility);

	bank.AddText("Reflection Render Time", &m_ReflectionTime);
	bank.AddText("PreRender Time", &m_PreRenderTime);
	bank.AddText("Render Time", &m_RenderTime);

	char string[256];
	for(s32 i = 0; i < query_count; i++)
	{
		formatf(string, "Query %d", i);
		bank.AddText(string, (s32*)&m_WaterOcclusionQueries[GPUINDEXMT][i].m_PixelsRenderedRead);
	}

	bank.AddText("Total Pixels", &m_NumPixelsRendered);
	bank.AddText("Query Frame Delay", &m_QueryFrameDelay);
	bank.AddSlider("Query Pixels Threshold", &m_WaterPixelCountThreshold, 0, 921600, 1);

	bank.AddSlider("Script Deep Ocean Scaler", &sm_ScriptDeepOceanScaler, 0.0f, 4.0f, 0.1f);
	
	River::InitWidgets(bank);
	bank.AddToggle("Enable Test Buoys", &m_EnableTestBuoys);
	bank.AddToggle("Disable Underwater Effects", &m_DisableUnderwater);
	bank.AddButton("Reset Test Buoys", ResetTestBuoys);
	bank.AddTitle("Ocean Pixel Shader Global Tunings");
	bank.AddToggle("Override TimeCycle",			&CMainWaterTune::m_OverrideTimeCycle);
	bank.AddColor("Fog Color",						&(m_MainWaterTunings.m_WaterColor));
	bank.AddSlider("Ripple Scale",					&(m_MainWaterTunings.m_RippleScale), 0.001f, 1.0f, 0.001f);
	bank.AddSlider("Ocean Foam Scale",				&(m_MainWaterTunings.m_OceanFoamScale),			0.0f, 1.0f, 0.05f);
	bank.AddSlider("Specular Intensity (TC)",		&(CMainWaterTune::m_SpecularIntensity), 0.0f, 1000.0f, 0.1f);
	bank.AddSlider("Specular Falloff",				&(m_MainWaterTunings.m_SpecularFalloff), 0.0f, 100000.0f, 10.0f);
	bank.AddSlider("Fog Pierce Intensity",			&(m_MainWaterTunings.m_FogPierceIntensity), 0.0f, 50.0f, 1.0f);
	bank.AddSlider("Refraction Blend",				&(m_MainWaterTunings.m_RefractionBlend), 0.0f, 1.0f, 0.1f);
	bank.AddSlider("Refraction Exponent",			&(m_MainWaterTunings.m_RefractionExponent), 0.0f, 4.0f, 0.25f);
	bank.AddSlider("Water Cycle Depth",				&(m_MainWaterTunings.m_WaterCycleDepth), 0.0f, 2000.0f, 10.0f);
	bank.AddSlider("Water Cycle Fade",				&(m_MainWaterTunings.m_WaterCycleFade), 0.0f, 2000.0f, 10.0f);
	bank.AddSlider("Water Lightning Depth",			&(m_MainWaterTunings.m_WaterLightningDepth), 0.0f, 2000.0f, 10.0f);
	bank.AddSlider("Water Lightning Fade",			&(m_MainWaterTunings.m_WaterLightningFade), 0.0f, 2000.0f, 10.0f);
	bank.AddSlider("Deep Water Mod Depth",			&(m_MainWaterTunings.m_DeepWaterModDepth), 0.0f, 2000.0f, 10.0f);
	bank.AddSlider("Deep Water Mod Fade",			&(m_MainWaterTunings.m_DeepWaterModFade), 0.0f, 2000.0f, 10.0f);
	bank.AddSlider("God Rays Lerp Start",			&(m_MainWaterTunings.m_GodRaysLerpStart), 0.0f, 2000.0f, 10.0f);
	bank.AddSlider("God Rays Lerp End", 			&(m_MainWaterTunings.m_GodRaysLerpEnd), 0.0f, 2000.0f, 10.0f);
	bank.AddSlider("Disturb Foam Scale", 			&(m_MainWaterTunings.m_DisturbFoamScale), 0.0f, 1.0f, 0.01f);
	bank.AddVector("Fog Min",						&(m_MainWaterTunings.m_FogMin), -10000.0f, 10000.0f, 10.0f);
	bank.AddVector("Fog Max",						&(m_MainWaterTunings.m_FogMax), -10000.0f, 10000.0f, 10.0f);

	bank.PushGroup("Island Heist DLC: Deep Ocean blend controls");
		bank.AddSlider("Dist Multiplier",	&sm_bkDeepOceanMult, 0.1f, 20.0f, 0.1f);
		bank.AddToggle("Curve Exp2",		&sm_bkDeepOceanExp2);
		bank.AddToggle("Curve Exp4",		&sm_bkDeepOceanExp4);
	bank.PopGroup();


	bank.AddButton("Save Water.dat to XML",			&SaveWaterData);
	bank.AddButton("Reload Water Tunings",			&bkReloadWaterTunings);
	bank.AddButton("Save Water Tunings",			&SaveWaterTunings);
	bank.AddToggle("Reload Water Data",				&m_bkReloadWaterData);

	bkGroup *pScanCutBlocksGroup = bank.PushGroup("Scan Cut Blocks", false);

	pScanCutBlocksGroup->AddToggle("Run ScanCutBlocks Early", &m_bRunScanCutBlocksEarly);
	pScanCutBlocksGroup->AddToggle("Lock Frustum", &m_frustumHasBeenLocked);
	pScanCutBlocksGroup->AddToggle("Draw Frustum", &m_drawFrustum);

	bank.PopGroup();
	bank.PushGroup("Extra Calming Quads");
	bank.AddToggle("Draw Extra Calming Quads", &sm_DebugDrawExtraCalmingQuads);
	for(int i=0;i<MAXEXTRACALMINGQUADS;i++)
	{
		bank.PushGroup("Calming Quad");
		bank.AddSlider("MinX",&sm_ExtraCalmingQuads[i].minX,-5000,5000,1);
		bank.AddSlider("MinY",&sm_ExtraCalmingQuads[i].minY,-5000,5000,1);
		bank.AddSlider("MaxX",&sm_ExtraCalmingQuads[i].maxX,-5000,5000,1);
		bank.AddSlider("MaxY",&sm_ExtraCalmingQuads[i].maxY,-5000,5000,1);
		bank.AddSlider("Dampening",&sm_ExtraCalmingQuads[i].m_fDampening,0.0f,1.0f,0.1f,CalmingQuadEnabled);
		bank.PopGroup();
	}
	bank.PopGroup();
	

	//Shader Edit Stuff
	m_waterShader->GetInstanceData().AddWidgets_WithLoad(*pMainBank, m_TexChangeDataBuffer, INDEX_NONE);
	s32 count = m_TexChangeDataBuffer.GetCount();
	m_TexChangeTextureBuffer[0].Reserve(count);
	m_TexChangeTextureBuffer[1].Reserve(count);
	WaterTexChangeTexture texture;
	texture.m_Texture = NULL;
	texture.m_MarkForDelete = false;
	for(s32 i = 0; i < count; i++)
	{
		m_TexChangeTextureBuffer[0].Append() = texture;
		m_TexChangeTextureBuffer[1].Append() = texture;
	}

	pMainBank->PopGroup();
}

bool& Water::GetRasterizeWaterQuadsIntoStencilRef()
{
	return m_RasterizeWaterQuadsIntoStencil;
}

float Water::GetWaterTestOffset()
{
	return m_waterTestOffset;
}

template <typename T> static bool WaterQuadDoesNotPlaySounds(const T&) { return false; }
template <> bool WaterQuadDoesNotPlaySounds<CWaterQuad>(const CWaterQuad& quad) { return !quad.GetPlaySounds(); }

template <typename T> static void WaterQuadRenderDebug(const T& quad, Color32 color, bool bRenderSolid, bool bRenderCutTriangles, bool bRenderVectorMap)
{
	Vec3V points[5];
	float a[5];
	const int numPoints = WaterQuadGetPoints(quad, points, a, !bRenderCutTriangles, 0.0f);

	if (numPoints >= 3)
	{
		Color32 c = color;

		if (WaterQuadDoesNotPlaySounds(quad))
		{
			c.SetRed(255);
			c.SetGreen(0);
		}

		if (bRenderVectorMap)
		{
			for (int i = 0; i < numPoints; i++)
			{
				CVectorMap::DrawLine(RCC_VECTOR3(points[i]), RCC_VECTOR3(points[(i + 1)%numPoints]), c, c);
			}
		}
		else if (bRenderSolid)
		{
			for (int i = 2; i < numPoints; i++)
			{
				grcDebugDraw::Poly(points[0], points[i - 1], points[i], c, true, true);
			}
		}
		else
		{
			for (int i = 0; i < numPoints; i++)
			{
				grcDebugDraw::Line(points[i], points[(i + 1)%numPoints], c);
			}
		}
	}
}

void Water::RenderDebug(Color32 waterColor, Color32 calmingColor, Color32 waveColor, bool bRenderSolid, bool bRenderCutTriangles, bool bRenderVectorMap)
{
	if (waterColor.GetAlpha() > 0)
	{
		for (int i = 0; i < m_WaterQuadsBuffer.GetCount(); i++)
		{
			WaterQuadRenderDebug(m_WaterQuadsBuffer[i], waterColor, bRenderSolid, bRenderCutTriangles, bRenderVectorMap);
		}
	}

	if (calmingColor.GetAlpha() > 0)
	{
		for (int i = 0; i < m_CalmingQuadsBuffer.GetCount(); i++)
		{
			WaterQuadRenderDebug(m_CalmingQuadsBuffer[i], calmingColor, bRenderSolid, bRenderCutTriangles, bRenderVectorMap);
		}
	}

	if (waveColor.GetAlpha() > 0)
	{
		for (int i = 0; i < m_WaveQuadsBuffer.GetCount(); i++)
		{
			WaterQuadRenderDebug(m_WaveQuadsBuffer[i], waveColor, bRenderSolid, bRenderCutTriangles, bRenderVectorMap);
		}
	}
}

void Water::RenderDebugToEPS(const char* name)
{
	ASSET.PushFolder("assets:");
	fiStream* eps = ASSET.Create(name, "eps");
	ASSET.PopFolder();

	if (!AssertVerify(eps))
	{
		return;
	}

	fprintf(eps, "%%!PS-Adobe EPSF-3.0\n");
	fprintf(eps, "%%%%HiResBoundingBox: 0 0 700 700\n");
	fprintf(eps, "%%%%BoundingBox: 0 0 700 700\n");
	fprintf(eps, "\n");
	fprintf(eps, "/Times-Roman findfont\n");
	fprintf(eps, "12 scalefont\n");
	fprintf(eps, "setfont\n");
	fprintf(eps, "25 12 moveto\n");
	fprintf(eps, "0 0 0 setrgbcolor\n");
	fprintf(eps, "(%d water quads) show\n", m_WaterQuadsBuffer.GetCount());
	fprintf(eps, "\n");

	const float scale = 512.0f/(float)(gv::WORLD_BOUNDS_MAX_X - gv::WORLD_BOUNDS_MIN_X);
	const float offset = 40.0f;

	for (int pass = 0; pass < 2; pass++)
	{
		fprintf(eps, "%s setrgbcolor\n", pass == 0 ? "0.9 0.9 0.9" : "0.0 0.0 0.0");
		fprintf(eps, "0.2 setlinewidth\n");
		fprintf(eps, "\n");

		for (int i = 0; i < m_WaterQuadsBuffer.GetCount(); i++)
		{
			Vec3V points[5];
			float a[5];
			const int numPoints = WaterQuadGetPoints(m_WaterQuadsBuffer[i], points, a, false, 0.0f);

			if (numPoints >= 3)
			{
				float x = (points[numPoints - 1].GetXf() - (float)gv::WORLD_BOUNDS_MIN_X)*scale + offset;
				float y = (points[numPoints - 1].GetYf() - (float)gv::WORLD_BOUNDS_MIN_Y)*scale + offset;

				fprintf(eps, "%f %f moveto ", x, y);

				for (int j = 0; j < numPoints; j++)
				{
					x = (points[j].GetXf() - (float)gv::WORLD_BOUNDS_MIN_X)*scale + offset;
					y = (points[j].GetYf() - (float)gv::WORLD_BOUNDS_MIN_Y)*scale + offset;

					fprintf(eps, "%f %f lineto ", x, y);
				}

				fprintf(eps, "%s\n", pass == 0 ? "fill" : "stroke");
			}
		}

		fprintf(eps, "\n");
	}

	fprintf(eps, "showpage\n");
	eps->Close();
}

void Water::PreRenderVisibility_Debug(spdkDOP2D_8& bounds, const grcViewport& viewport, bool bUseOcclusion, bool BANK_ONLY(bDrawPoints))
{
	bounds.SetEmpty();

	const s32 waterNumBlocksX = WATERNUMBLOCKSX;
	const s32 waterNumBlocksY = WATERNUMBLOCKSY;
	const Mat34V localToWorld = viewport.GetCameraMtx();
	const Vec2V invTanFOV = Vec2V(1.0f/viewport.GetTanHFOV(), -1.0f/viewport.GetTanVFOV());

	for (int x = 0; x < waterNumBlocksX; x++)
	{
		for (int y = 0; y < waterNumBlocksY; y++)
		{
			const int blockIndex = y + x*waterNumBlocksY;
			const CWaterBlockData& blockData = m_WaterBlockData[blockIndex];

			if (blockData.numQuads == 0)
				continue;

			const s16 minX = (s16)WATERBLOCKTOWORLDX(x);
			const s16 minY = (s16)WATERBLOCKTOWORLDY(y);
			const s16 maxX = minX + WATERBLOCKWIDTH;
			const s16 maxY = minY + WATERBLOCKWIDTH;

			const Vec3V minV = Vec3V((float)minX, (float)minY, blockData.minHeight);
			const Vec3V maxV = Vec3V((float)maxX, (float)maxY, blockData.maxHeight);

			if (bUseOcclusion)
			{
				if (!COcclusion::IsBoxVisible(spdAABB(minV, maxV)))
					continue;
			}
			else
			{
				if (!grcViewport::IsAABBVisible(minV.GetIntrin128(), maxV.GetIntrin128(), viewport.GetCullFrustumLRTB()))
					continue;
			}

			const int i0 = m_WaterBlockData[blockIndex].quadIndex;
			const int i1 = m_WaterBlockData[blockIndex].numQuads + i0;

			for (int i = i0; i < i1; i++)
			{
				// TODO -- optimise this (i.e. we only need to clip against the near plane i think)
				Vec3V points[5];
				Vec3V clipped[5 + 6];
				float a[5]; // not used
				const int numPoints = WaterQuadGetPoints(m_WaterQuadsBuffer[i], points, a, false, 0.0f);
				const int numClipped = PolyClipToFrustum(clipped, points, numPoints, viewport.GetCompositeMtx());

				if (numClipped >= 3)
				{
					for (int j = 0; j < numClipped; j++)
					{
						const Vec3V v = UnTransformOrtho(localToWorld, clipped[j]);
						const Vec2V p = -invTanFOV*v.GetXY()/v.GetZ();

						const Vec2V point = Vec2V(V_HALF) + Vec2V(V_HALF)*p; // [-1..1] -> [0..1]

						bounds.AddPoint(point);
//#if __BANK
						if (bDrawPoints)
						{
							grcDebugDraw::Line(point - Vec2V(0.01f,+0.01f), point + Vec2V(0.01f,+0.01f), Color32(255,0,0,255));
							grcDebugDraw::Line(point - Vec2V(0.01f,-0.01f), point + Vec2V(0.01f,-0.01f), Color32(255,0,0,255));
						}
//#endif // __BANK
					}
				}
			}
		}
	}

	spdkDOP2D_8 ident;
	ident.SetIdentity();
	bounds.Clip(ident);
}

#endif // __BANK
