//
// Filename:	TerrainGrid.h
// Description:	Helper class to render a grid with height information.
//				The implementation uses a projection box to render the grid
//				over the geometry.

#ifndef __VFX_TERRAIN_GRID_H__
#define __VFX_TERRAIN_GRID_H__

#include "grcore/stateblock.h"
#include "grcore/effect_typedefs.h"
#include "vector/color32.h"

namespace rage 
{
class bkBank;
class grmShader;
class grcTexture;
};

//////////////////////////////////////////////////////////////////////////
//
class TerrainGrid
{

public:

	void Activate(bool bEnable);
	void SetBoxParams(Vector3::Vector3Param dimsWHD, Vector3::Vector3Param pos, Vector3::Vector3Param dir, Vector4::Vector4Param miscParams);
	void SetColours(Color32 lowHeight, Color32 midHeight, Color32 highHeight);

	void Update();
	void Render();

#if __BANK
	void AddWidgets(bkBank* pBank);
#endif

	static void InitClass();
	static void ShutdownClass();

	static TerrainGrid&	GetInstance() { return *sm_pInstance; }

	void InitShaders();

private:

	void			Init();
	void			InitRenderData();

	void			SetRenderState();
	void			SetShaderConstants(const grcTexture *depth);
	grcTexture*		GetDepthTexture();
	void			BindShader();
	void			UnbindShader();
	void			RenderGrid();
	void			SetupGrid();

#if __BANK
	void			RenderDebug();
#endif

	// render data
	grcBlendStateHandle			m_defaultBS;
	grcDepthStencilStateHandle	m_defaultDSS;
	grcRasterizerStateHandle	m_defaultRS;

	grcSamplerStateHandle		m_diffuseMapSampler;

	grcEffectVar	m_shaderVarIdDiffTexture;
	grcEffectGlobalVar	m_shaderVarIdDepthTexture;
#if __XENON || __D3D11 || RSG_ORBIS
	grcEffectGlobalVar	m_shaderVarIdStencilTexture;
#endif
	grcEffectVar	m_shaderVarIdScreenSize;
	grcEffectVar	m_shaderVarIdProjParams;
	grcEffectVar	m_shaderVarIdTerrainGridProjDir;
	grcEffectVar	m_shaderVarIdTerrainGridCentrePos;
	grcEffectVar	m_shaderVarIdTerrainGridForward;
	grcEffectVar	m_shaderVarIdTerrainGridRight;
	grcEffectVar	m_shaderVarIdTerrainGridParams;
	grcEffectVar	m_shaderVarIdTerrainGridParams2;
	grcEffectVar	m_shaderVarIdLowHeightCol;
	grcEffectVar	m_shaderVarIdMidHeightCol;
	grcEffectVar	m_shaderVarIdHighHeightCol;

	grcEffectTechnique	m_technique;
	grmShader*			m_pShader;
	grcTexture*			m_pGridTexture;

	Vec3V				m_vProjectionDir;
	Vec3V				m_vCentrePosition;
	Vec4V				m_vDimensions;
	Vec4V				m_vForward;
	Vec4V				m_vRight;
	Vec4V				m_vGridMiscParams;

	Vec3V				m_vVerts[8];

	Vector3				m_UserGridCentrePosition;
	Vector3				m_UserForwardDir;
	Vector4				m_UserGridMiscParams;
	Vector4				m_UserDimensions;

	Color32				m_gridBaseColor;
	Color32				m_lowHeightColor;
	Color32				m_midHeightColor;
	Color32				m_highHeightColor;

	bool				m_bEnabled;
	bool				m_bRender;
	bool				m_bFollowCamera;

#if __BANK
	bool				m_useHeightRefFromPicker;
	bool				m_bOverrideSettings;
	bool				m_bShowProjectionBox;
	bool				m_bApplyTweaksToPosAndDir;
	ScalarV				m_vOffsetAlongForward;
	ScalarV				m_vOffsetAlongRight;
	ScalarV				m_vPitchAdjustment;
#endif

	// global instance
	static TerrainGrid* sm_pInstance;

};

#define GOLFGREENGRID TerrainGrid::GetInstance()

#endif // __VFX_TERRAIN_GRID_H__
