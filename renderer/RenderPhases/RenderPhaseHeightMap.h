//
// filename:	RenderPhaseHeight.h
// description: renderphase classes
//

#ifndef INC_RENDERPHASEHEIGHTMAP_H_
#define INC_RENDERPHASEHEIGHTMAP_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "grcore/effect.h"
#include "gpuptfx/ptxgpubase.h"
// Game headers
#include "renderer/DrawLists/DrawListMgr.h"
#include "Renderer/RenderPhases/RenderPhase.h"

// --- Defines ------------------------------------------------------------------
#define HGHT_TARGET_SIZE			128
#define NOISE_SIZE					32
#define EXTRA_EMITTER_BOTTOM_HEIGHT (8.0f)
#define EXTRA_BOTTOM_HEIGHT			(4.0f)

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------

#if __BANK
class CFrustumDebug;
#endif // __BANK

//
// name:		CRenderPhaseHeight
// description:	Height render phase
class CRenderPhaseHeight : public CRenderPhaseScanned
{
public:
	struct Params
	{
		bool		useCaching;
		const char* name;
		eDrawListType	type;

		Params( const char* n, bool c , eDrawListType t)
			: useCaching( c), name( n ), type( t )
		{	
			Assertf( n , " Required Name for looking up initialization params" );
		}

		Params()
			:	useCaching( true ), 
				name( "heightReflect" ),
				type( DL_RENDERPHASE_HEIGHT_MAP )
		{}
	};

	CRenderPhaseHeight(CViewport* pGameViewport, const Params& p= Params());
	~CRenderPhaseHeight();

	virtual void BuildRenderList();
	
	virtual void UpdateViewport();
	
	virtual void BuildDrawList(); 

	virtual u32 GetCullFlags() const;

#if RAIN_UPDATE_CPU_HEIGHTMAP
	static void CopyDepthToStaging(void *rp);
#endif

#if __BANK
	virtual const char* GetName() const {return "HeightMap";}
#endif

#if __BANK
	virtual void AddWidgets(bkGroup &group);
#endif // __BANK

	inline void SetRenderTarget(grcRenderTarget *target)			{ m_sRenderTarget = target; }
	
	void SwapRenderTargets() {}


	// for storing current pointer of front target during RenderThread:
	static void SetRenderTargetRT(void *rp, void *t);
	inline const grcRenderTarget* GetRenderTargetRT() const { FastAssert(CSystem::IsThisThreadId(SYS_THREAD_RENDER)); return m_sRenderTargetRT;}


	inline bool GetIsRenderingHeightMap() const { return m_sRenderingHeight; }
	inline void SetIsRenderingHeightMap(const bool state) { m_sRenderingHeight = state; }

	Matrix44 GetHeightTransformMtx()	
	{
		Matrix34  mtx = Matrix34( Matrix34::ZeroType );

		mtx.MakeScale( m_Scale[gRenderThreadInterface.GetRenderBuffer()]);
		mtx.MakeTranslate( m_Offset[gRenderThreadInterface.GetRenderBuffer()] );

		Matrix44 res;
		res.FromMatrix34( mtx);
		return res;
	}

	static CRenderPhaseHeight*	GetPtFxGPUCollisionMap();
	static void SetPtFxGPUCollisionMap(CRenderPhaseHeight* hgt);

private:
	
	static const float m_fNearPlane;

	// This is a hack to overcome sampling issues with the GPU particles (B*1458033)
	// For some reason I can't explain we need a full pixel offset on the X and Y to get the collision to work right
	static float	m_offsetShiftX;
	static float	m_offsetShiftY;

	Vector3			m_lastCell;
	Vector3			m_Scale[2]; //double buffered for render thread pleasure
	Vector3			m_Offset[2];

	Params			m_params;

	// General
	grcRenderTarget *m_sRenderTarget;
	grcRenderTarget *m_sRenderTargetRT;

#if RAIN_UPDATE_CPU_HEIGHTMAP
public:	// TODO - HACK remove me
	grcTexture		*m_stagingRenderTex[MAX_GPUS];
	grcRenderTarget	*m_stagingRenderTargets[MAX_GPUS];
	int				m_numStagingRenderTargets;
	int				m_currentStagingRenderTarget;
private:
#endif

	bool m_sRenderingHeight;
	bool m_renderFrame;

#if __BANK
	bool m_quantiseToGrid;
	bool m_renderVehiclesInHeightMap;
	bool m_renderObjectsInHeightMap;
	bool m_renderAnimatedBuildingsInHeightMap;
	bool m_renderWaterInHeightmap;
	bool m_useShadowTechnique;
	bool m_VisualizeHeightmapFrustum;
	bool m_FreezeHeightmapFrustum;
	bool m_bSkipHeightWhenExteriorNotVisible;
	float m_fExtraEmitterBottomHeight;
	float m_fExtraBottomHeight;
	CFrustumDebug* m_pVisualiseCamFrustum;
#if !__PS3
	bool m_useShadowShader;
#endif // !__PS3
#endif // __BANK
};

#endif // !INC_RENDERPHASEHEIGHTMAP_H_
