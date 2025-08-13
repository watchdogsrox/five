///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	FogVolume.h
//	BY	: 	Anoop Ravi Thomas
//	FOR	:	Rockstar Games
//	ON	:	25 October 2013
//	WHAT:	Fog Volumes
//
///////////////////////////////////////////////////////////////////////////////

#ifndef FOGVOLUME_H
#define FOGVOLUME_H

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////		

// rage
#include "vector/color32.h"
#include "grmodel/shader.h"
#include "vectormath/vec3v.h"
#include "fwutil/xmacro.h"

//framework
#include "fwscene/world/InteriorLocation.h"


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////													

#define FOGVOLUME_MAX_STORED						(128)
#define FOGVOLUME_MAX_STORED_NUM_BUFFERS			(2)


///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////	



///////////////////////////////////////////////////////////////////////////////
//  ENUMS
///////////////////////////////////////////////////////////////////////////////	

enum FogVolumeShaderVar_e
{
	FOGVOLUME_SHADERVAR_COLOR = 0,
	FOGVOLUME_SHADERVAR_POSITION,
	FOGVOLUME_SHADERVAR_PARAMS,
	FOGVOLUME_SHADERVAR_TRANSFORM,
	FOGVOLUME_SHADERVAR_INV_TRANSFORM,
	FOGVOLUME_SHADERVAR_SCREENSIZE,
	FOGVOLUME_SHADERVAR_PROJECTION_PARAMS,						
	FOGVOLUME_SHADERVAR_PERSPECTIVESHEAR_PARAMS0,
	FOGVOLUME_SHADERVAR_PERSPECTIVESHEAR_PARAMS1,
	FOGVOLUME_SHADERVAR_PERSPECTIVESHEAR_PARAMS2,
	FOGVOLUME_SHADERVAR_DEPTHBUFFER,
	FOGVOLUME_SHADERVAR_COUNT
};

enum FogVolumeShaderTechniques_e
{
	FOGVOLUME_SHADERTECHNIQUE_DEFAULT = 0,
	FOGVOLUME_SHADERTECHNIQUE_ELLIPSOIDAL,
	FOGVOLUME_SHADERTECHNIQUE_COUNT
};

enum FogVolumeShaderPasses_e
{
	FOGVOLUME_SHADERPASS_OUTSIDE = 0,
	FOGVOLUME_SHADERPASS_INSIDE,
	FOGVOLUME_SHADERPASS_COUNT
};

enum FogVolumeLighting_e
{
	FOGVOLUME_LIGHTING_FOGHDR = 0,
	FOGVOLUME_LIGHTING_DIRECTIONAL,
	FOGVOLUME_LIGHTING_NONE,
	FOGVOLUME_LIGHTING_NUM
};

enum FogVolumeMeshLOD
{
	FOGVOLUME_MESHLOD_INSIDE = 0,
	FOGVOLUME_MESHLOD_OUTSIDE_HIGH,
	FOGVOLUME_MESHLOD_OUTSIDE_MED,
	FOGVOLUME_MESHLOD_OUTSIDE_LOW,

	FOGVOLUME_MESHLOD_TOTAL
};

enum FogVolumeVertexBufferType
{
	FOGVOLUME_VB_LOWDETAIL = 0,
	FOGVOLUME_VB_MIDDETAIL,
	FOGVOLUME_VB_HIDETAIL,

	FOGVOLUME_VB_TOTAL
};
///////////////////////////////////////////////////////////////////////////////
//  STRUCTURES
///////////////////////////////////////////////////////////////////////////////	

struct FogVolume_t
{
	Vec3V pos;
	Vec3V scale;
	Vec3V rotation;
	Color32 col;
	float range;
	float density;
	float fallOff;
	float hdrMult;
	float fogMult;
	u64 interiorHash;
	FogVolumeLighting_e lightingType;
	bool useGroundFogColour;
	BANK_ONLY(bool debugRender;)

	FogVolume_t()
	: pos(V_ZERO)
	, scale(V_ONE)
	, rotation(V_ZERO)
	, col(0,0,0,0)
	, range(0.0f)
	, density(0.0f)
	, fallOff(1.0f)
	, hdrMult(1.0f)
	, fogMult(1.0f)
	, lightingType(FOGVOLUME_LIGHTING_FOGHDR)
	, useGroundFogColour(false)
	, interiorHash(0)
	BANK_ONLY(, debugRender(true))
	{
	}
};


///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////	

class CFogVolumeMgr
{
public:
	CFogVolumeMgr();
	void Init(unsigned initMode);
	static void InitDLCCommands();
	void InitShaders();
#if RSG_PC
	void DeleteShaders();
	static void ResetShaders();
#endif
	void SetupRenderthreadFrameInfo();
	void ClearRenderthreadFrameInfo();
	void PreUpdate();
	void UpdateAfterRender();
	void Shutdown(unsigned shutdownMode);
	void RenderImmediate(const FogVolume_t& fogVolumeParams);
	void RenderFogVolume(const FogVolume_t& fogVolumeParams BANK_PARAM(bool bImmediateMode));
	void Register(const FogVolume_t& fogVolumeParams);
	void RenderRegistered();


	FogVolume_t* GetFogVolumesUpdateBuffer()	{ return &(m_FogVolumeRegister[0]); }

	u32 GetNumFogVolumesRegisteredUpdate();
	void SetNumFogVolumesRegisteredUpdate(u32 numFogVolumes);
	u32 GetNumFogVolumesRegisteredRender();
	void SetNumFogVolumesRegisteredRender(u32 numFogVolumes);

	void SetDisableFogVolumeRender(bool b) { m_DisableFogVolumeRender = b; }

#if __BANK
	void InitWidgets();
	static void RenderTestFogVolume();
	static void RenderTestFogVolumeInFrontOfCam();
	static void RemoveTestFogVolume();
	static void PrintTestFogVolumeParams();
#endif


	static FogVolume_t*				m_FogVolumeRenderBuffer;

private:

	//fogVolume shader
	grmShader*			m_fogVolumeShader;
	grcEffectTechnique	m_fogVolumeShaderTechniques[FOGVOLUME_SHADERTECHNIQUE_COUNT];
	grcEffectVar		m_fogVolumeShaderVars[FOGVOLUME_SHADERVAR_COUNT];

	//fogVolume registration
	FogVolume_t			m_FogVolumeRegister		[FOGVOLUME_MAX_STORED];
	u32					m_numRegisteredFogVolumes	[FOGVOLUME_MAX_STORED_NUM_BUFFERS];


	//helper functions
	grcEffectTechnique CalculateTechnique(const FogVolume_t& fogVolumeParams);
	__forceinline float CalculateRadius(const FogVolume_t& fogVolumeParams);
	FogVolumeShaderPasses_e CalculatePass(const FogVolume_t& fogVolumeParams);
	FogVolumeMeshLOD CalculateMeshLOD(const FogVolume_t& fogVolumeParams, FogVolumeShaderPasses_e pass);
	void RenderVolumeMesh(const FogVolume_t& fogVolumeParams, FogVolumeShaderPasses_e pass);

	bool		m_DisableFogVolumeRender;

#if __BANK
	float		m_TestDistanceFromCam;
	Vec3V		m_TestPosition;
	Vec3V		m_TestScale;
	Vec3V		m_TestRotation;
	Color32		m_TestColor;
	float		m_TestDensity;
	float		m_TestRange;
	float		m_TestFallOff;
	float		m_TestHDRMult;
	float		m_TestFadeStart;
	float		m_TestFadeEnd;
	int			m_TestLightingType;
	u64			m_TestInteriorHash;
	bool		m_TestUseGroundFogColour;
	bool		m_TestRender;
	bool		m_TestApplyFade;
	bool		m_TestGenerateInFrontOfCamera;

	float		m_DebugExtendedRadiusPercent;
	bool		m_DebugFogVolumeRender;
	float		m_DebugDensityScale;

	float		m_DebugFogVolumeLowLODDistFactor;
	float		m_DebugFogVolumeMedLODDistFactor;
	bool		m_DisableOcclusionTest;

	int			m_ForceDetailMeshIndex;

#endif

};




///////////////////////////////////////////////////////////////////////////////
//  EXTERNS
///////////////////////////////////////////////////////////////////////////////

extern CFogVolumeMgr g_fogVolumeMgr;














#endif //FOGVOLUME_H