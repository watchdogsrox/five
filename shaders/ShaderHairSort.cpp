//
// Filename:	ShaderHairSort.cpp
// Description:	special skinned hair renderer able to sort geometries in a drawable using the hair shader
// Written by:	Andrzej
//
//	17/01/2006	-	John:		- initial;
//	04/09/2007	-	Andrzej:	- code update & revival;
//	12/09/2007	-	Andrzej:	- CShaderHairSort::ModelDrawSkinned(): renderstate is correctly restored;
//	06/07/2008	-	Andrzej:	- CShaderHairSort::ShaderDrawSkinned(): 1st pre-pass removed as it was unnecessary anymore
//									(materialID is stored in stencil now (and not GBuffer.col0.a));
//	22/01/2009	-	Andrzej:	- fur shader;
//	16/03/2009	-	Andrzej:	- long hair shader;
//
//
//
//
//
#include "shaders/ShaderHairSort.h"

// Rage headers
#include "system/xtl.h"
#include "shaderlib/rage_constants.h"	// MTX_IN_VB
#include "crskeleton/skeletondata.h"
#include "crskeleton/skeleton.h"
#include "rmcore/drawable.h"
#include "grmodel/geometry.h"
#include "grmodel/matrixset.h"
#include "grmodel/shader.h"
#include "grmodel/shaderfx.h"

// Framework
#include "fwsys/gameskeleton.h"

// Game headers:
#include "debug/debug.h"
#include "renderer/DrawLists/DrawListMgr.h"
#include "shaders/ShaderLib.h"
#include "renderer/Deferred/GBuffer.h"
#include "renderer/PostProcessFx.h"		// PostFX::g_UseSubSampledAlpha
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"

//
//
//
//
CShaderHairSort::CShaderHairSort(void)
{
	// do nothing
}

CShaderHairSort::~CShaderHairSort(void)
{
	// do nothing
}



static grcDepthStencilStateHandle	hLongHairDSS_pass1						= grcStateBlock::DSS_Invalid;
static grcBlendStateHandle			hLongHairBS_pass1						= grcStateBlock::BS_Invalid;
static grcDepthStencilStateHandle	hLongHairDSS_pass2						= grcStateBlock::DSS_Invalid;
static grcBlendStateHandle			hLongHairBS_pass2						= grcStateBlock::BS_Invalid;
static grcBlendStateHandle			hLongHairBS_pass2_end					= grcStateBlock::BS_Invalid;
static grcDepthStencilStateHandle	hLongHairDSS_pass2_end					= grcStateBlock::DSS_Invalid;
static grcDepthStencilStateHandle	hPedHairDSS								= grcStateBlock::DSS_Invalid;
static grcRasterizerStateHandle		hPedHairHOrderMinusOneRS_pass1			= grcStateBlock::RS_Invalid;
static grcBlendStateHandle			hPedHairHOrderMinusOneBS_pass1			= grcStateBlock::BS_Invalid;
static grcRasterizerStateHandle		hPedHairHOrderMinusOneRS_pass1_end		= grcStateBlock::RS_Invalid;
static grcBlendStateHandle			hHairFurBS								= grcStateBlock::BS_Invalid;
static grcBlendStateHandle			hHairOrderedBS_order0					= grcStateBlock::BS_Invalid;
static grcBlendStateHandle			hHairOrderedBS_orderN					= grcStateBlock::BS_Invalid;
static grcBlendStateHandle			hHairSpikedBS_order0					= grcStateBlock::BS_Invalid;
static grcBlendStateHandle			hHairSpikedBS_order0_ssa				= grcStateBlock::BS_Invalid;
static grcBlendStateHandle			hHairSpikedBS_order0_singlepass_ssa		= grcStateBlock::BS_Invalid;
static grcBlendStateHandle			hHairSpikedBS_order0_singlepass_ssa_0rgb= grcStateBlock::BS_Invalid;	// for stippling
static grcDepthStencilStateHandle	hHairSpikedDSS_order0					= grcStateBlock::DSS_Invalid;
static grcDepthStencilStateHandle	hHairSpikedDSS_order1					= grcStateBlock::DSS_Invalid;
static grcBlendStateHandle			hHairSpikedBS_order1_ssa				= grcStateBlock::BS_Invalid;
static grcBlendStateHandle			hHairSpikedBS_order1					= grcStateBlock::BS_Invalid;
static grcDepthStencilStateHandle	hHairSpikedDSS_order1_end				= grcStateBlock::DSS_Invalid;
//static grcBlendStateHandle			hHairSpikedBS_forward					= grcStateBlock::BS_Invalid;
static grcBlendStateHandle			hHairCutoutBS_order0					= grcStateBlock::BS_Invalid;
static grcBlendStateHandle			hHairCutoutBS_order0_ssa				= grcStateBlock::BS_Invalid;
static grcBlendStateHandle			hHairCutoutBS_order0_singlepass_ssa		= grcStateBlock::BS_Invalid;
static grcBlendStateHandle			hHairCutoutBS_order0_singlepass_ssa_0rgb= grcStateBlock::BS_Invalid;	// for stippling


static DECLARE_MTR_THREAD s32	gPreviousTechniqueGroupId			= - 1;
static atMap<s32, s32>		gHairTintTechniqueGroupOverrideTable;


void CShaderHairSort::PushHairTintTechnique()
{
	Assert(gPreviousTechniqueGroupId == -1);
	int currentTech = grmShaderFx::GetForcedTechniqueGroupId();

	s32* pTechniqueGroupOverrideId = gHairTintTechniqueGroupOverrideTable.Access(currentTech);
	if (pTechniqueGroupOverrideId != NULL)
	{		
		gPreviousTechniqueGroupId = currentTech;
		Assert(currentTech != -1 );
		grmShaderFx::SetForcedTechniqueGroupId(*pTechniqueGroupOverrideId);
	}
}

void CShaderHairSort::PopHairTintTechnique()
{
	if (gPreviousTechniqueGroupId != -1)
	{
		grmShaderFx::SetForcedTechniqueGroupId(gPreviousTechniqueGroupId);
		gPreviousTechniqueGroupId = -1;
	}
}

//
//
//
//
static void InitHairTintTechniqueGroupOverrideTable()
{
	s32 techniqueGroupOverrideFrom = grmShaderFx::FindTechniqueGroupId("deferredsubsamplealpha");
	s32 techniqueGroupOverrideTo = grmShaderFx::FindTechniqueGroupId("deferredsubsamplealphatint");
	if (Verifyf(techniqueGroupOverrideFrom != -1 && techniqueGroupOverrideTo != -1, "failed to find technique group"))
	{
		gHairTintTechniqueGroupOverrideTable[techniqueGroupOverrideFrom] = techniqueGroupOverrideTo;
	}

	techniqueGroupOverrideFrom = grmShaderFx::FindTechniqueGroupId("deferredalphaclip");
	techniqueGroupOverrideTo = grmShaderFx::FindTechniqueGroupId("deferredalphacliptint");
	if (Verifyf(techniqueGroupOverrideFrom != -1 && techniqueGroupOverrideTo != -1, "failed to find technique group"))
	{
		gHairTintTechniqueGroupOverrideTable[techniqueGroupOverrideFrom] = techniqueGroupOverrideTo;
	}

	techniqueGroupOverrideFrom = grmShaderFx::FindTechniqueGroupId("lightweight0CutOut");
	techniqueGroupOverrideTo = grmShaderFx::FindTechniqueGroupId("lightweight0CutOutTint");
	if (Verifyf(techniqueGroupOverrideFrom != -1 && techniqueGroupOverrideTo != -1, "failed to find technique group"))
	{
		gHairTintTechniqueGroupOverrideTable[techniqueGroupOverrideFrom] = techniqueGroupOverrideTo;
	}

	techniqueGroupOverrideFrom = grmShaderFx::FindTechniqueGroupId("lightweight4CutOut");
	techniqueGroupOverrideTo = grmShaderFx::FindTechniqueGroupId("lightweight4CutOutTint");
	if (Verifyf(techniqueGroupOverrideFrom != -1 && techniqueGroupOverrideTo != -1, "failed to find technique group"))
	{
		gHairTintTechniqueGroupOverrideTable[techniqueGroupOverrideFrom] = techniqueGroupOverrideTo;
	}

	techniqueGroupOverrideFrom = grmShaderFx::FindTechniqueGroupId("lightweight8CutOut");
	techniqueGroupOverrideTo = grmShaderFx::FindTechniqueGroupId("lightweight8CutOutTint");
	if (Verifyf(techniqueGroupOverrideFrom != -1 && techniqueGroupOverrideTo != -1, "failed to find technique group"))
	{
		gHairTintTechniqueGroupOverrideTable[techniqueGroupOverrideFrom] = techniqueGroupOverrideTo;
	}

	techniqueGroupOverrideFrom = grmShaderFx::FindTechniqueGroupId("waterreflectionalphaclip");
	techniqueGroupOverrideTo = grmShaderFx::FindTechniqueGroupId("waterreflectionalphacliptint");
	if (Verifyf(techniqueGroupOverrideFrom != -1 && techniqueGroupOverrideTo != -1, "failed to find technique group"))
	{
		gHairTintTechniqueGroupOverrideTable[techniqueGroupOverrideFrom] = techniqueGroupOverrideTo;
	}
}

void CShaderHairSort::Init(unsigned initMode)
{
    if(initMode != INIT_CORE)
		return;

	InitHairTintTechniqueGroupOverrideTable();
	
	if(hLongHairDSS_pass1 == grcStateBlock::DSS_Invalid)
	{
		grcDepthStencilStateDesc dsDesc;
		dsDesc.DepthEnable					= TRUE;
		dsDesc.DepthFunc					= rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
		dsDesc.DepthWriteMask				= TRUE;

		// enable stencil:
		dsDesc.StencilEnable				= TRUE;
		dsDesc.StencilReadMask				= 0xff;
		dsDesc.StencilWriteMask				= 0xff;
		dsDesc.FrontFace.StencilFunc		= 
		dsDesc.BackFace.StencilFunc			= grcRSV::CMP_ALWAYS;
		dsDesc.FrontFace.StencilPassOp		=
		dsDesc.BackFace.StencilPassOp		= grcRSV::STENCILOP_REPLACE;
		dsDesc.FrontFace.StencilFailOp		=
		dsDesc.BackFace.StencilFailOp		= grcRSV::STENCILOP_KEEP;
		dsDesc.FrontFace.StencilDepthFailOp	=
		dsDesc.BackFace.StencilDepthFailOp	= grcRSV::STENCILOP_KEEP;

		hLongHairDSS_pass1		= grcStateBlock::CreateDepthStencilState(dsDesc);
		Assert(hLongHairDSS_pass1 != grcStateBlock::DSS_Invalid);
	}

	if(hLongHairBS_pass1 == grcStateBlock::BS_Invalid)
	{
	static dev_bool debugEnableAlphaToMask1		= true;
	static dev_bool debugUseSolidAlphaToMask1	= false;
		grcBlendStateDesc bsDesc;
		bsDesc.IndependentBlendEnable				= FALSE;
		bsDesc.BlendRTDesc[0].BlendEnable			= FALSE;
		bsDesc.BlendRTDesc[0].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_RGB;

		bsDesc.AlphaToCoverageEnable				= debugEnableAlphaToMask1;
		bsDesc.AlphaToMaskOffsets					= debugUseSolidAlphaToMask1 ? grcRSV::ALPHATOMASKOFFSETS_SOLID : grcRSV::ALPHATOMASKOFFSETS_DITHERED;
		
		hLongHairBS_pass1 = grcStateBlock::CreateBlendState(bsDesc);
		Assert(hLongHairBS_pass1 != grcStateBlock::BS_Invalid);
	}

	if(hLongHairDSS_pass2 == grcStateBlock::DSS_Invalid)
	{
		grcDepthStencilStateDesc dsDesc;
		dsDesc.DepthEnable					= TRUE;
		dsDesc.DepthFunc					= rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
		dsDesc.DepthWriteMask				= FALSE;

		// enable stencil:
		dsDesc.StencilEnable				= TRUE;
		dsDesc.StencilReadMask				= 0x17;								// read only bits 4+2..0 		
		dsDesc.StencilWriteMask				= DEFERRED_MATERIAL_SPECIALBIT;		// write to bit 4 only
		dsDesc.FrontFace.StencilFunc		= 
		dsDesc.BackFace.StencilFunc			= grcRSV::CMP_EQUAL;
		dsDesc.FrontFace.StencilPassOp		=
		dsDesc.BackFace.StencilPassOp		= grcRSV::STENCILOP_ZERO;			// clear special bit
		dsDesc.FrontFace.StencilFailOp		=
		dsDesc.BackFace.StencilFailOp		= grcRSV::STENCILOP_KEEP;
		dsDesc.FrontFace.StencilDepthFailOp	=
		dsDesc.BackFace.StencilDepthFailOp	= grcRSV::STENCILOP_KEEP;

		hLongHairDSS_pass2	= grcStateBlock::CreateDepthStencilState(dsDesc);
		Assert(hLongHairDSS_pass2 != grcStateBlock::DSS_Invalid);
	}

	if(hLongHairBS_pass2 == grcStateBlock::BS_Invalid)
	{
		grcBlendStateDesc bsDesc;
		bsDesc.IndependentBlendEnable				= TRUE;
		bsDesc.BlendRTDesc[0].BlendEnable			=
		bsDesc.BlendRTDesc[1].BlendEnable			=
		bsDesc.BlendRTDesc[2].BlendEnable			=
		bsDesc.BlendRTDesc[3].BlendEnable			= FALSE;
		bsDesc.BlendRTDesc[0].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_NONE;
		bsDesc.BlendRTDesc[1].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_RGB;
		bsDesc.BlendRTDesc[2].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_RGB;
		bsDesc.BlendRTDesc[3].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_RGB;

		bsDesc.AlphaToCoverageEnable				= FALSE;

		hLongHairBS_pass2 = grcStateBlock::CreateBlendState(bsDesc);
		Assert(hLongHairBS_pass2 != grcStateBlock::BS_Invalid);
	}

	if(hLongHairBS_pass2_end == grcStateBlock::BS_Invalid)
	{
		grcBlendStateDesc bsDesc;
		bsDesc.IndependentBlendEnable				= FALSE;
		bsDesc.BlendRTDesc[0].BlendEnable			= FALSE;
		bsDesc.BlendRTDesc[0].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_RGB;
	
		hLongHairBS_pass2_end = grcStateBlock::CreateBlendState(bsDesc);
		Assert(hLongHairBS_pass2_end != grcStateBlock::BS_Invalid);
	}

	if(hLongHairDSS_pass2_end == grcStateBlock::DSS_Invalid)
	{
		grcDepthStencilStateDesc dsDesc;
		dsDesc.DepthEnable					= TRUE;
		dsDesc.DepthFunc					= rage::FixupDepthDirection(grcRSV::CMP_LESS);
		dsDesc.DepthWriteMask				= TRUE;

		// enable stencil:
		dsDesc.StencilEnable				= FALSE;
		dsDesc.StencilReadMask				= 0xFF;
		dsDesc.StencilWriteMask				= 0xFF;
		dsDesc.FrontFace.StencilFunc		= 
		dsDesc.BackFace.StencilFunc			= grcRSV::CMP_ALWAYS;
		dsDesc.FrontFace.StencilPassOp		=
		dsDesc.BackFace.StencilPassOp		= grcRSV::STENCILOP_KEEP;
		dsDesc.FrontFace.StencilFailOp		=
		dsDesc.BackFace.StencilFailOp		= grcRSV::STENCILOP_KEEP;
		dsDesc.FrontFace.StencilDepthFailOp	=
		dsDesc.BackFace.StencilDepthFailOp	= grcRSV::STENCILOP_KEEP;

		hLongHairDSS_pass2_end	= grcStateBlock::CreateDepthStencilState(dsDesc);
		Assert(hLongHairDSS_pass2_end != grcStateBlock::DSS_Invalid);
	}

	if(hPedHairDSS == grcStateBlock::DSS_Invalid)
	{
		grcDepthStencilStateDesc dsDesc;
		dsDesc.DepthFunc					= rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
	
		// enable stencil:
		dsDesc.StencilEnable				= TRUE;
		dsDesc.FrontFace.StencilFunc		= grcRSV::CMP_ALWAYS;
		dsDesc.FrontFace.StencilPassOp		= grcRSV::STENCILOP_REPLACE;
		dsDesc.BackFace = dsDesc.FrontFace;

		hPedHairDSS	= grcStateBlock::CreateDepthStencilState(dsDesc);
		Assert(hPedHairDSS != grcStateBlock::DSS_Invalid);
	}

	if(hPedHairHOrderMinusOneRS_pass1 == grcStateBlock::RS_Invalid)
	{
		grcRasterizerStateDesc rasDesc;
		rasDesc.CullMode	= grcRSV::CULL_FRONT;		// only back facing polygons
		
		hPedHairHOrderMinusOneRS_pass1 = grcStateBlock::CreateRasterizerState(rasDesc);
		Assert(hPedHairHOrderMinusOneRS_pass1 != grcStateBlock::RS_Invalid);
	}

	// force wanted renderstate:
	const bool rsAlphaToMask =
#if DEVICE_MSAA
		GRCDEVICE.GetMSAA() ||
#endif
		false;

	if(hPedHairHOrderMinusOneBS_pass1 == grcStateBlock::BS_Invalid)
	{
		grcBlendStateDesc bsDesc;
		bsDesc.IndependentBlendEnable				= FALSE;
		bsDesc.BlendRTDesc[0].BlendEnable			= FALSE;
		// Standard srcAlpha, 1-srcAlpha blend
		bsDesc.BlendRTDesc[0].BlendOp				= 
		bsDesc.BlendRTDesc[0].BlendOpAlpha			= grcRSV::BLENDOP_ADD;
		bsDesc.BlendRTDesc[0].SrcBlend				=
		bsDesc.BlendRTDesc[0].SrcBlendAlpha			= grcRSV::BLEND_SRCALPHA;
		bsDesc.BlendRTDesc[0].DestBlend				=
		bsDesc.BlendRTDesc[0].DestBlendAlpha		= grcRSV::BLEND_INVSRCALPHA;
		bsDesc.BlendRTDesc[0].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_RGB;

		bsDesc.AlphaToCoverageEnable				= rsAlphaToMask;

		hPedHairHOrderMinusOneBS_pass1 = grcStateBlock::CreateBlendState(bsDesc);
		Assert(hPedHairHOrderMinusOneBS_pass1 != grcStateBlock::BS_Invalid);
	}

	if(hPedHairHOrderMinusOneRS_pass1_end == grcStateBlock::RS_Invalid)
	{
		grcRasterizerStateDesc rasDesc;
		rasDesc.CullMode	= grcRSV::CULL_BACK;
		
		hPedHairHOrderMinusOneRS_pass1_end	= grcStateBlock::CreateRasterizerState(rasDesc);
		Assert(hPedHairHOrderMinusOneRS_pass1_end != grcStateBlock::RS_Invalid);
	}

	if(hHairFurBS == grcStateBlock::BS_Invalid)
	{
		grcBlendStateDesc bsDesc;
		bsDesc.IndependentBlendEnable = true;
		bsDesc.BlendRTDesc[0].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_RGB;

		bsDesc.AlphaToCoverageEnable				= rsAlphaToMask;
		
		hHairFurBS = grcStateBlock::CreateBlendState(bsDesc);
		Assert(hHairFurBS != grcStateBlock::BS_Invalid);
	}

	if(hHairOrderedBS_order0 == grcStateBlock::BS_Invalid)
	{
		grcBlendStateDesc bsDesc;
		bsDesc.IndependentBlendEnable				= FALSE;
		bsDesc.BlendRTDesc[0].BlendEnable			= TRUE;
		// Standard srcAlpha, 1-srcAlpha blend
		bsDesc.BlendRTDesc[0].BlendOp				= 
		bsDesc.BlendRTDesc[0].BlendOpAlpha			= grcRSV::BLENDOP_ADD;
		bsDesc.BlendRTDesc[0].SrcBlend				=
		bsDesc.BlendRTDesc[0].SrcBlendAlpha			= grcRSV::BLEND_SRCALPHA;
		bsDesc.BlendRTDesc[0].DestBlend				=
		bsDesc.BlendRTDesc[0].DestBlendAlpha		= grcRSV::BLEND_INVSRCALPHA;
		bsDesc.BlendRTDesc[0].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_RGB;

		bsDesc.AlphaToCoverageEnable				= rsAlphaToMask;

		hHairOrderedBS_order0 = grcStateBlock::CreateBlendState(bsDesc);
		Assert(hHairOrderedBS_order0 != grcStateBlock::BS_Invalid);
	}

	if(hHairOrderedBS_orderN == grcStateBlock::BS_Invalid)
	{
		grcBlendStateDesc bsDesc;
		bsDesc.IndependentBlendEnable				= FALSE;
		bsDesc.BlendRTDesc[0].BlendEnable			= TRUE;
		// Standard srcAlpha, 1-srcAlpha blend
		bsDesc.BlendRTDesc[0].BlendOp				= 
		bsDesc.BlendRTDesc[0].BlendOpAlpha			= grcRSV::BLENDOP_ADD;
		bsDesc.BlendRTDesc[0].SrcBlend				=
		bsDesc.BlendRTDesc[0].SrcBlendAlpha			= grcRSV::BLEND_SRCALPHA;
		bsDesc.BlendRTDesc[0].DestBlend				=
		bsDesc.BlendRTDesc[0].DestBlendAlpha		= grcRSV::BLEND_INVSRCALPHA;
		bsDesc.BlendRTDesc[0].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_RGB;

		bsDesc.AlphaToCoverageEnable				= rsAlphaToMask;
		
		hHairOrderedBS_orderN = grcStateBlock::CreateBlendState(bsDesc);
		Assert(hHairOrderedBS_orderN != grcStateBlock::BS_Invalid);
	}

	if(hHairSpikedBS_order0 == grcStateBlock::BS_Invalid)
	{
		grcBlendStateDesc bsDesc;
		bsDesc.IndependentBlendEnable				= FALSE;
		bsDesc.BlendRTDesc[0].BlendEnable			= FALSE;
		bsDesc.BlendRTDesc[0].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALL;

		bsDesc.AlphaToCoverageEnable				= rsAlphaToMask;
		
		hHairSpikedBS_order0 = grcStateBlock::CreateBlendState(bsDesc);
		Assert(hHairSpikedBS_order0 != grcStateBlock::BS_Invalid);
	}

	if(hHairSpikedBS_order0_ssa == grcStateBlock::BS_Invalid)
	{
		grcBlendStateDesc bsDesc;
		bsDesc.IndependentBlendEnable				= TRUE;
		bsDesc.BlendRTDesc[0].BlendEnable			= FALSE;
		bsDesc.BlendRTDesc[0].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_RGB;
		bsDesc.BlendRTDesc[1].BlendEnable			= FALSE;
		bsDesc.BlendRTDesc[1].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_RGB;
		bsDesc.BlendRTDesc[2].BlendEnable			= FALSE;
		bsDesc.BlendRTDesc[2].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALL;
		bsDesc.BlendRTDesc[3].BlendEnable			= FALSE;
		bsDesc.BlendRTDesc[3].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALL;

		bsDesc.AlphaToCoverageEnable				= rsAlphaToMask;
		
		hHairSpikedBS_order0_ssa = grcStateBlock::CreateBlendState(bsDesc);
		Assert(hHairSpikedBS_order0_ssa != grcStateBlock::BS_Invalid);
	}

	if(hHairSpikedBS_order0_singlepass_ssa == grcStateBlock::BS_Invalid)
	{
		grcBlendStateDesc bsDesc;
		bsDesc.IndependentBlendEnable				= TRUE;
		bsDesc.BlendRTDesc[0].BlendEnable			= FALSE;
		bsDesc.BlendRTDesc[0].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALL;
		bsDesc.BlendRTDesc[1].BlendEnable			= FALSE;
		bsDesc.BlendRTDesc[1].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_RGB;
		bsDesc.BlendRTDesc[2].BlendEnable			= FALSE;
		bsDesc.BlendRTDesc[2].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALL;
		bsDesc.BlendRTDesc[3].BlendEnable			= FALSE;
		bsDesc.BlendRTDesc[3].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALL;

		bsDesc.AlphaToCoverageEnable				= rsAlphaToMask;
		
		hHairSpikedBS_order0_singlepass_ssa = grcStateBlock::CreateBlendState(bsDesc);
		Assert(hHairSpikedBS_order0_singlepass_ssa != grcStateBlock::BS_Invalid);
	}

	if(hHairSpikedBS_order0_singlepass_ssa_0rgb == grcStateBlock::BS_Invalid)
	{
		grcBlendStateDesc bsDesc;
		bsDesc.IndependentBlendEnable				= TRUE;
		bsDesc.BlendRTDesc[0].BlendEnable			= FALSE;
		bsDesc.BlendRTDesc[0].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_RGB;	// do not write alpha to avoid SSA resolve (and to ruin stippling)
		bsDesc.BlendRTDesc[1].BlendEnable			= FALSE;
		bsDesc.BlendRTDesc[1].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_RGB;
		bsDesc.BlendRTDesc[2].BlendEnable			= FALSE;
		bsDesc.BlendRTDesc[2].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALL;
		bsDesc.BlendRTDesc[3].BlendEnable			= FALSE;
		bsDesc.BlendRTDesc[3].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALL;

		bsDesc.AlphaToCoverageEnable				= rsAlphaToMask;
		
		hHairSpikedBS_order0_singlepass_ssa_0rgb = grcStateBlock::CreateBlendState(bsDesc);
		Assert(hHairSpikedBS_order0_singlepass_ssa_0rgb != grcStateBlock::BS_Invalid);
	}

	if(hHairSpikedDSS_order0 == grcStateBlock::DSS_Invalid)
	{
		grcDepthStencilStateDesc dsDesc;

		dsDesc.DepthEnable					= TRUE;
		dsDesc.DepthFunc					= rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
		dsDesc.DepthWriteMask				= TRUE;

		// enable stencil:
		dsDesc.StencilEnable				= TRUE;
		dsDesc.StencilReadMask				= 0xff;
		dsDesc.StencilWriteMask				= 0xff;
		dsDesc.FrontFace.StencilFunc		= 
		dsDesc.BackFace.StencilFunc			= grcRSV::CMP_ALWAYS;
		dsDesc.FrontFace.StencilPassOp		=
		dsDesc.BackFace.StencilPassOp		= grcRSV::STENCILOP_REPLACE;
		dsDesc.FrontFace.StencilFailOp		=
		dsDesc.BackFace.StencilFailOp		= grcRSV::STENCILOP_KEEP;
		dsDesc.FrontFace.StencilDepthFailOp	=
		dsDesc.BackFace.StencilDepthFailOp	= grcRSV::STENCILOP_KEEP;

		hHairSpikedDSS_order0		= grcStateBlock::CreateDepthStencilState(dsDesc);
		Assert(hHairSpikedDSS_order0 != grcStateBlock::DSS_Invalid);
	}

	if(hHairSpikedDSS_order1 == grcStateBlock::DSS_Invalid)
	{
		grcDepthStencilStateDesc dsDesc;

		dsDesc.DepthEnable					= TRUE;
		dsDesc.DepthFunc					= rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
		dsDesc.DepthWriteMask				= FALSE;

		// enable stencil:
		dsDesc.StencilEnable				= TRUE;
		dsDesc.StencilReadMask				= 0x17;							// read only bits 4+2..0
		dsDesc.StencilWriteMask				= DEFERRED_MATERIAL_SPECIALBIT;	// write to bit 4 only
		dsDesc.FrontFace.StencilFunc		= 
		dsDesc.BackFace.StencilFunc			= grcRSV::CMP_EQUAL;
		dsDesc.FrontFace.StencilPassOp		=
		dsDesc.BackFace.StencilPassOp		= grcRSV::STENCILOP_ZERO;		// clear special bit
		dsDesc.FrontFace.StencilFailOp		=
		dsDesc.BackFace.StencilFailOp		= grcRSV::STENCILOP_KEEP;
		dsDesc.FrontFace.StencilDepthFailOp	=
		dsDesc.BackFace.StencilDepthFailOp	= grcRSV::STENCILOP_KEEP;

		hHairSpikedDSS_order1		= grcStateBlock::CreateDepthStencilState(dsDesc);
		Assert(hHairSpikedDSS_order1 != grcStateBlock::DSS_Invalid);
	}

	if(hHairSpikedBS_order1_ssa == grcStateBlock::BS_Invalid)
	{
		grcBlendStateDesc bsDesc;
		bsDesc.IndependentBlendEnable				= FALSE;
		bsDesc.BlendRTDesc[0].BlendEnable			= FALSE;

		bsDesc.AlphaToCoverageEnable				= rsAlphaToMask;
		
		hHairSpikedBS_order1_ssa = grcStateBlock::CreateBlendState(bsDesc);
		Assert(hHairSpikedBS_order1_ssa != grcStateBlock::BS_Invalid);
	}

	if(hHairSpikedBS_order1 == grcStateBlock::BS_Invalid)
	{
		grcBlendStateDesc bsDesc;
		bsDesc.IndependentBlendEnable				= TRUE;
		bsDesc.BlendRTDesc[0].BlendEnable			=
		bsDesc.BlendRTDesc[1].BlendEnable			=
		bsDesc.BlendRTDesc[2].BlendEnable			=
		bsDesc.BlendRTDesc[3].BlendEnable			= FALSE;
		bsDesc.BlendRTDesc[0].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_NONE;
		bsDesc.BlendRTDesc[1].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_RGB;
		bsDesc.BlendRTDesc[2].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_NONE;
		bsDesc.BlendRTDesc[3].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_RED+grcRSV::COLORWRITEENABLE_GREEN;	// overwrite AO

		bsDesc.AlphaToCoverageEnable				= rsAlphaToMask;
		
		hHairSpikedBS_order1 = grcStateBlock::CreateBlendState(bsDesc);
		Assert(hHairSpikedBS_order1 != grcStateBlock::BS_Invalid);
	}

	if(hHairSpikedDSS_order1_end == grcStateBlock::DSS_Invalid)
	{
		grcDepthStencilStateDesc dsDesc;

		dsDesc.DepthFunc					= rage::FixupDepthDirection(grcRSV::CMP_LESS);
		// enable stencil:
		dsDesc.StencilEnable				= FALSE;
		dsDesc.StencilReadMask				= 0xFF;
		dsDesc.StencilWriteMask				= 0xFF;
		dsDesc.FrontFace.StencilFunc		= 
		dsDesc.BackFace.StencilFunc			= grcRSV::CMP_ALWAYS;
		dsDesc.FrontFace.StencilPassOp		=
		dsDesc.BackFace.StencilPassOp		= grcRSV::STENCILOP_KEEP;
		dsDesc.FrontFace.StencilFailOp		=
		dsDesc.BackFace.StencilFailOp		= grcRSV::STENCILOP_KEEP;
		dsDesc.FrontFace.StencilDepthFailOp	=
		dsDesc.BackFace.StencilDepthFailOp	= grcRSV::STENCILOP_KEEP;

		hHairSpikedDSS_order1_end		= grcStateBlock::CreateDepthStencilState(dsDesc);
		Assert(hHairSpikedDSS_order1_end != grcStateBlock::DSS_Invalid);
	}

/*	if(hHairSpikedBS_forward == grcStateBlock::BS_Invalid)
	{
		grcBlendStateDesc bsDesc;
		bsDesc.IndependentBlendEnable				= FALSE;
		bsDesc.BlendRTDesc[0].BlendEnable			= FALSE;
		bsDesc.BlendRTDesc[0].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALL;

		bsDesc.AlphaToCoverageEnable				= rsAlphaToMask;
		
		hHairSpikedBS_forward = grcStateBlock::CreateBlendState(bsDesc);
		Assert(hHairSpikedBS_forward != grcStateBlock::BS_Invalid);
	}
*/

	const bool rsAlphaToMaskCutout = false;

	if(hHairCutoutBS_order0 == grcStateBlock::BS_Invalid)
	{
		grcBlendStateDesc bsDesc;
		bsDesc.IndependentBlendEnable				= FALSE;
		bsDesc.BlendRTDesc[0].BlendEnable			= FALSE;
		bsDesc.BlendRTDesc[0].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALL;

		bsDesc.AlphaToCoverageEnable				= rsAlphaToMaskCutout;
		
		hHairCutoutBS_order0 = grcStateBlock::CreateBlendState(bsDesc);
		Assert(hHairCutoutBS_order0 != grcStateBlock::BS_Invalid);
	}

	if(hHairCutoutBS_order0_ssa == grcStateBlock::BS_Invalid)
	{
		grcBlendStateDesc bsDesc;
		bsDesc.IndependentBlendEnable				= FALSE;
		bsDesc.BlendRTDesc[0].BlendEnable			= FALSE;
		bsDesc.BlendRTDesc[0].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALL;
		bsDesc.AlphaToCoverageEnable				= rsAlphaToMaskCutout;
		
		hHairCutoutBS_order0_ssa = grcStateBlock::CreateBlendState(bsDesc);
		Assert(hHairCutoutBS_order0_ssa != grcStateBlock::BS_Invalid);
	}

	if(hHairCutoutBS_order0_singlepass_ssa == grcStateBlock::BS_Invalid)
	{
		grcBlendStateDesc bsDesc;
		bsDesc.IndependentBlendEnable				= FALSE;
		bsDesc.BlendRTDesc[0].BlendEnable			= FALSE;
		bsDesc.BlendRTDesc[0].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALL;
		bsDesc.AlphaToCoverageEnable				= rsAlphaToMaskCutout;
		
		hHairCutoutBS_order0_singlepass_ssa = grcStateBlock::CreateBlendState(bsDesc);
		Assert(hHairCutoutBS_order0_singlepass_ssa != grcStateBlock::BS_Invalid);
	}

	if(hHairCutoutBS_order0_singlepass_ssa_0rgb == grcStateBlock::BS_Invalid)
	{
		grcBlendStateDesc bsDesc;
		bsDesc.IndependentBlendEnable				= TRUE;
		bsDesc.BlendRTDesc[0].BlendEnable			= FALSE;
		bsDesc.BlendRTDesc[0].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_RGB;	// do not write alpha to avoid SSA resolve (and to ruin stippling)
		bsDesc.BlendRTDesc[1].BlendEnable			= FALSE;
		bsDesc.BlendRTDesc[1].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALL;
		bsDesc.BlendRTDesc[2].BlendEnable			= FALSE;
		bsDesc.BlendRTDesc[2].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALL;
		bsDesc.BlendRTDesc[3].BlendEnable			= FALSE;
		bsDesc.BlendRTDesc[3].RenderTargetWriteMask	= grcRSV::COLORWRITEENABLE_ALL;

		bsDesc.AlphaToCoverageEnable				= rsAlphaToMaskCutout;
		
		hHairCutoutBS_order0_singlepass_ssa_0rgb = grcStateBlock::CreateBlendState(bsDesc);
		Assert(hHairCutoutBS_order0_singlepass_ssa_0rgb != grcStateBlock::BS_Invalid);
	}
}// end of Init()...


#define DEFAULT_FUR_LOD_DIST	(10.0f)
#define DEFAULT_FUR_LOD_FOVY	(50.0f)
#define DEFAULT_FUR_SHADOW_MIN  (0.2f)
#define DEFAULT_FUR_MIN_LAYERS  (2)
#define DEFAULT_FUR_MAX_LAYERS  (20)
#define DEFAULT_FUR_LENGTH		(0.05f)
#define DEFAULT_FUR_STIFFNESS   (0.6f)
#define DEFAULT_FUR_GRAVITYWIND (0.05f)
#define DEFAULT_FUR_ATTEN_SQ	(1.21f)
#define DEFAULT_FUR_ATTEN_LIN	(-0.22f)
#define DEFAULT_FUR_ATTEN_CONST (0.0f)
#define DEFAULT_FUR_UV_SCALE	(2.0f)
#define DEFAULT_FUR_AO_BLEND	(1.0f)


// fur data
#if (__BANK)
bool	CShaderHairSort::furOverride			= false;
float	CShaderHairSort::furLayerShadowMin		= DEFAULT_FUR_SHADOW_MIN;
u32		CShaderHairSort::furMinLayers			= DEFAULT_FUR_MIN_LAYERS;
u32		CShaderHairSort::furMaxLayers			= DEFAULT_FUR_MAX_LAYERS;
float	CShaderHairSort::furLength				= DEFAULT_FUR_LENGTH;
float	CShaderHairSort::furStiffness			= DEFAULT_FUR_STIFFNESS;
float	CShaderHairSort::furLodDist				= DEFAULT_FUR_LOD_DIST;	// disable detailed fur after 10 meters
float	CShaderHairSort::furDefaultFOVY			= DEFAULT_FUR_LOD_FOVY; // default fovy for fur LOD
float	CShaderHairSort::furGravityToWindRatio	= DEFAULT_FUR_GRAVITYWIND;
Vec3V	CShaderHairSort::furAttenuationCoeff    = Vec3V(DEFAULT_FUR_ATTEN_SQ, DEFAULT_FUR_ATTEN_LIN, DEFAULT_FUR_ATTEN_CONST);
float	CShaderHairSort::furNoiseUVScale		= DEFAULT_FUR_UV_SCALE;
float   CShaderHairSort::furAOBlend				= DEFAULT_FUR_AO_BLEND;

void CShaderHairSort::InitWidgets(bkBank& bank)
{
	bank.PushGroup("Fur Controls");
		bank.AddToggle(	"Fur: Enable Override", &furOverride);
		bank.AddSlider("Fur: Self Shadow", &furLayerShadowMin, 0.0f, 1.0f, 0.05f);
		bank.AddSlider("Fur: Min Layers", &furMinLayers, 1, 10, 1);
		bank.AddSlider("Fur: Max Layers", &furMaxLayers, 1, 50, 1);
		bank.AddSlider("Fur: Length", &furLength, 0.0f, 10.0f, 0.01f);
		bank.AddSlider("Fur: Stiffness", &furStiffness, 0.0f, 1.0f, 0.05f);
		bank.AddSlider("Fur: LOD MaxDist", &furLodDist, 0.0f, 200.0f, 1.0f);
		bank.AddSlider("Fur: LOD FOVY Base", &furDefaultFOVY, 1.0f, 179.0f, 1.0f);
		bank.AddSlider("Fur: Gravity/Wind Blend", &furGravityToWindRatio, 0.0f, 1.0f, 0.1f);
		bank.AddSlider("Fur: Noise UV Scale", &furNoiseUVScale, 0.1f, 50.0f, 0.1f);
		bank.AddSlider("Fur: AO Blend", &furAOBlend, 0.0f, 1.0f, 0.05f);
		bank.AddVector("Fur: rounding,taper,bias", &furAttenuationCoeff, -2.0f, 4.0f, 0.01f);
	bank.PopGroup();
}
#endif

//
// called from RenderPed(); funcs (stream and pack versions)
//
// 1. stolen from rmcDrawable::DrawSkinned()
//
void CShaderHairSort::DrawableDrawSkinned(const rmcDrawable *pDrawable, const Matrix34 &rootMatrix,const grmMatrixSet &ms,int bucketMask,int lod, eHairSortMode hairMode, Vector3 *windVector)
{
	Assert(pDrawable);

#if __BANK
	const bool bIsShadowPass = DRAWLISTMGR->IsExecutingShadowDrawList();
	if (bIsShadowPass && !CRenderPhaseCascadeShadowsInterface::GetRenderHairEnabled())
	{
		return;
	}
#endif // __BANK

	if(hairMode==HAIRSORT_CUTOUT)
	{
		// ped_hair_cutout_alpha may be mixed up with geoms using bucket=0,1, etc. so skip early bucket check;
		CShaderHairSort::LodGroupDrawMulti(pDrawable->GetLodGroup(), pDrawable->GetShaderGroup(),rootMatrix,ms,bucketMask,lod,0,hairMode, windVector);
	}
	else
	{
		// ped_hair_spiked, ped_hair_spiked_enveff and ped_fur use drawbucket=3:
		if(CRenderer::IsBaseBucket(bucketMask,CRenderer::RB_CUTOUT))
		{
			CShaderHairSort::LodGroupDrawMulti(pDrawable->GetLodGroup(), pDrawable->GetShaderGroup(),rootMatrix,ms,bucketMask,lod,0,hairMode, windVector);
		}

		// ped_hair_spiked_noalpha uses bucket=0:
		if(hairMode==HAIRSORT_SPIKED)
		{
			if(CRenderer::IsBaseBucket(bucketMask,CRenderer::RB_OPAQUE))
			{
				CShaderHairSort::LodGroupDrawMulti(pDrawable->GetLodGroup(), pDrawable->GetShaderGroup(),rootMatrix,ms,bucketMask,lod,0,hairMode, windVector);
			}
		}
	}	
}


//
//
//
// 2. stolen from rmcLodGroup::DrawMulti()
//
void CShaderHairSort::LodGroupDrawMulti(const rmcLodGroup& lodGroup, const grmShaderGroup &group,const Matrix34 &rootMatrix,const grmMatrixSet &ms,int bucketMask,int lodIdx,const atBitSet *enables, eHairSortMode hairMode, Vector3 *windVector)
{
	const rmcLod &lod = lodGroup.GetLod(lodIdx);

	float fFurLOD = 1.0f;
	Vector3 bendVec(0.0f, 0.0f, 0.0f);
	
	if(hairMode==HAIRSORT_FUR)
	{
		float lodDist = DEFAULT_FUR_LOD_DIST;
		float lodFOVY = DEFAULT_FUR_LOD_FOVY;
		float furGravityWindBlend = DEFAULT_FUR_GRAVITYWIND;
#if __BANK
		if( furOverride )
		{
			lodDist = furLodDist;
			lodFOVY = furDefaultFOVY;
			furGravityWindBlend = furGravityToWindRatio;
		}
#endif

		// compute the Fur's LOD percentage: 0 is low detail, 1 is full detail
		// we take the distance from the camera in proportion to sm_furLodDist and the field of view in proportion to sm_furDefaultFOVY 
		const grcViewport *pViewport = grcViewport::GetCurrent();	// RT version
		Assert(pViewport);
		const Vector3 camPos = VEC3V_TO_VECTOR3(pViewport->GetCameraPosition());
		const Vector3 objPos = rootMatrix.d;
		Vector3 d = camPos - objPos;

		fFurLOD = Clamp(1.0f - ((d.Mag() - pViewport->GetNearClip())/lodDist) * (pViewport->GetFOVY()/lodFOVY), 0.0f, 1.0f);

		// also, transform the bend vector right now
		Matrix34 invRoot = rootMatrix;
		invRoot.FastInverse();

		if(windVector)
		{
			Vector3 comboVec = Lerp(furGravityWindBlend, Vector3(0.0f,0.0f,-1.0f), *windVector);
			comboVec.Normalize();
			invRoot.Transform3x3(comboVec, bendVec);
		}
	}

	int lastMtx = -2;
	for (int i=0; i<lod.GetCount(); i++)
	{
		if (lod.GetModel(i))
		{
			grmModel &model = *lod.GetModel(i);
			int idx = model.GetMatrixIndex();
			Assertf(idx>=0&&idx<ms.GetMatrixCount(),"Not enough matrices in model.  You probably tried to draw an articulated drawable without a skeleton.");
			if (!enables || enables->IsSet(idx))
			{
				if (model.GetSkinFlag())
				{
					if (lastMtx != -1)
					{
						grcViewport::SetCurrentWorldMtx(RCC_MAT34V(rootMatrix));
						lastMtx = -1;
					}
				
					CShaderHairSort::ModelDrawSkinned(model, group,ms,bucketMask,lodIdx,hairMode, fFurLOD, &bendVec);
				}
				else
				{
					if (idx != lastMtx)
					{
						// This is a bit slower than I'd like because it has to untranspose the matrix and re-add the offset back in again.
						Matrix34 tmp;
						ms.ComputeMatrix(tmp,lastMtx = idx);
						tmp.Dot(rootMatrix);
						grcViewport::SetCurrentWorldMtx(RCC_MAT34V(tmp));
					}
					// non skinned models not supported (yet):
					// model.Draw(group,bucket,lodIdx);
					Assert(FALSE);
				}
			}
		}
	}
}// end of LodGroupDrawMulti()...

static DECLARE_MTR_THREAD bool g_bRestoreStateBlocks = false;

//
//
// 3. stolen from grmModel::DrawSkinned()
//
void CShaderHairSort::ModelDrawSkinned(const grmModel &model, const grmShaderGroup &group,const grmMatrixSet &ms,int bucketMask,int lod, 
										eHairSortMode hairMode, float fFurLOD, Vector3 *windVector )
{
	// NOTE: there is not vis check here (as above), due to changing shape of skinned objects...
#if MTX_IN_VB
	GRCDEVICE.GetCurrent()->SetStreamSource(1,ms.GetVertexBuffer(),0,MTX_IN_VB_HALF? 24 : 48);
#endif

	u32 forceBucketMask = 0xFFFFFFFF;
	grmShader *pForceShader = grmModel::GetForceShader();
	if (pForceShader && pForceShader->InheritsBucketId() == false ) 
		forceBucketMask = pForceShader->GetDrawBucketMask();

	g_bRestoreStateBlocks		= false;
	grcRasterizerStateHandle	RS_prev	= grcStateBlock::RS_Active;
	grcDepthStencilStateHandle	DSS_prev= grcStateBlock::DSS_Active;
	grcBlendStateHandle			BS_prev = grcStateBlock::BS_Active;
	const u32 prevStencilRef	= grcStateBlock::ActiveStencilRef;
	const u32 prevBlendFactors	= grcStateBlock::ActiveBlendFactors;
	const u64 prevSampleMask	= grcStateBlock::ActiveSampleMask;


	const bool bIsShadowPass = DRAWLISTMGR->IsExecutingShadowDrawList();
	const bool bIsGbufPass	 = DRAWLISTMGR->IsExecutingGBufDrawList();
	const bool bIsHudPass    = DRAWLISTMGR->IsExecutingHudDrawList();
	const bool bIsForwardPass= DRAWLISTMGR->IsExecutingMirrorReflectionDrawList() || DRAWLISTMGR->IsExecutingWaterReflectionDrawList() || DRAWLISTMGR->IsExecutingDrawSceneDrawList();

	if(hairMode == HAIRSORT_FUR)
	{
		const s32 countGeometry = model.GetGeometryCount();

		// init from static
#if !__BANK
		u32 minLayers = DEFAULT_FUR_MIN_LAYERS;
		u32 maxLayers = DEFAULT_FUR_MAX_LAYERS;
		float maxLength = DEFAULT_FUR_LENGTH;
		float shadowMin = DEFAULT_FUR_SHADOW_MIN;
		float stiffness = DEFAULT_FUR_STIFFNESS;
		Vector3 atten = Vector3(DEFAULT_FUR_ATTEN_SQ, DEFAULT_FUR_ATTEN_LIN, DEFAULT_FUR_ATTEN_CONST);
#else // !__BANK
		u32 minLayers = furMinLayers;
		u32 maxLayers = furMaxLayers;
		float maxLength = furLength;
		float shadowMin = furLayerShadowMin;
		float stiffness = furStiffness;
		Vector3 atten = VEC3V_TO_VECTOR3(furAttenuationCoeff);

		if( !furOverride ) // if bank stuff is on, leave it alone
#endif // !__BANK
		{
			// grab artist's fur params from shader: 
			for(s32 i=0; i<countGeometry; i++)
			{
				const grmShaderFx *shader = (grmShaderFx*)((pForceShader) ? pForceShader : &group[model.GetShaderIndex(i)]);
				grcEffectVar idVarFurMinLayers = shader->LookupVar("furMinLayers0", FALSE);
				if(idVarFurMinLayers)
				{
					float tempVal=0.0f;
					shader->GetVar(idVarFurMinLayers, tempVal);
					minLayers = (u32)tempVal;
				}

				grcEffectVar idVarFurMaxLayers = shader->LookupVar("furMaxLayers0", FALSE);
				if(idVarFurMaxLayers)
				{
					float tempVal=0.0f;
					shader->GetVar(idVarFurMaxLayers, tempVal);
					maxLayers = (u32)tempVal;
				}

				grcEffectVar idVarFurLength = shader->LookupVar("furLength0", FALSE);
				if(idVarFurLength)
				{
					float tempVal=0.0f;
					shader->GetVar(idVarFurLength, tempVal);
					maxLength = tempVal;
				}
				grcEffectVar idVarFurShadowMin = shader->LookupVar("furSelfShadowMin0", FALSE);
				if(idVarFurShadowMin)
				{
					float tempVal=0.0f;
					shader->GetVar(idVarFurShadowMin, tempVal);
					shadowMin = tempVal;
				}
				grcEffectVar idVarFurStiffness = shader->LookupVar("furStiffness0", FALSE);
				if(idVarFurStiffness)
				{
					float tempVal=0.0f;
					shader->GetVar(idVarFurStiffness, tempVal);
					stiffness = tempVal; 
				}
				grcEffectVar idVarFurAtten = shader->LookupVar("furAttenCoef0", FALSE);
				if(idVarFurAtten)
				{
					Vector3 tempVal;
					shader->GetVar(idVarFurAtten, tempVal);
					atten = tempVal; 
				}
			}
		}
#if __BANK
		// if we are in rag override mode, set the noise UV scale and AO blend here
		if( furOverride )
		{
			for(s32 i=0; i<countGeometry; i++)
			{
				const grmShaderFx *shader = (grmShaderFx*)((pForceShader) ? pForceShader : &group[model.GetShaderIndex(i)]);
				grcEffectVar idVarFurNoiseUV = shader->LookupVar("furNoiseUVScale0", FALSE);
				if(idVarFurNoiseUV)
				{
					((grmShader*)shader)->SetVar(idVarFurNoiseUV, furNoiseUVScale);
				}

				grcEffectVar idVarFurAOBlend = shader->LookupVar("furAOBlend0", FALSE);
				if(idVarFurAOBlend)
				{
					((grmShader*)shader)->SetVar(idVarFurAOBlend, furAOBlend);
				}
			}
		}
#endif //__BANK

		// adjust the number of layers for smooth LOD
		float floatLayer = Lerp(fFurLOD, (float)minLayers, (float)maxLayers);
		u32 numLayers = u32(floatLayer);
		float fracLayer = (floatLayer - numLayers);

		// 1. draw all the hair shells now in given order:
		for(s32 furLayer=0; furLayer<(numLayers+1); furLayer++)
		{
			for(s32 i=0; i<countGeometry; i++)
			{
				const grmShaderFx *shader = (grmShaderFx*)((pForceShader) ? pForceShader : &group[model.GetShaderIndex(i)]);
				int shaderBucketMask = (forceBucketMask==~0U) ? group[model.GetShaderIndex(i)].GetDrawBucketMask() : forceBucketMask;
				if (BUCKETMASK_MATCH(shaderBucketMask,bucketMask))
				{
					// clipping uses current layer's opacity
					float pct = float(Max(0.0f,furLayer+fracLayer-1.0f))/floatLayer;

					// shadowing uses the next layer's percentage
					float furShadow = Lerp(Saturate((furLayer+fracLayer+1.0f)/floatLayer), shadowMin, 1.0f);
					float furOffset = pct * maxLength;
					// attenuation is quadratic
					float furAlphaClip = pct*pct*atten.x + pct * atten.y + atten.z;

					grcEffectVar idVarFurParams0 = shader->LookupVar("furGlobalParams0", FALSE);
					if(idVarFurParams0)
					{
						Vector3 furParams;
						furParams.x = furOffset;	// x=fur offset for geom
						furParams.y = furShadow;	// y=fur layer for shadowing
						furParams.z = furAlphaClip;	// w=fur alpha clip level
						((grmShader*)shader)->SetVar(idVarFurParams0, furParams);
					}
					grcEffectVar idVarFurParams1 = shader->LookupVar("furGlobalParams1", FALSE);
					if(idVarFurParams1)
					{
						// set the bend vector from gravity & wind
						Assertf(windVector, "Fur wind vector can't be NULL!");

						Vector4 params1(*windVector);
						params1.w = pct*pct*(1.0f-stiffness);
						((grmShader*)shader)->SetVar(idVarFurParams1, params1);
					}

					//bool nextIsSame = pForceShader || (i+1<countGeometry && shader->GetTemplateIndex() == group[model.GetShaderIndex(i+1)].GetTemplateIndex());
					const bool restoreShaderState = false;	//(i+1==countGeometry || !nextIsSame);
					CShaderHairSort::ShaderDrawSkinned(*shader,model,i,ms,lod,restoreShaderState,PASS_ALL,furLayer,HAIRSORT_FUR,bIsShadowPass,bIsForwardPass);
				}
			}// for(int i=0; i<countGeometry; i++)...
		}// for(s32 hOrder=0; hOrder<MAX_HAIR_ORDER; hOrder++)...

//////////////////////////////////////////////////////////////////////////////////////////////////////
	}//if(hairMode == HAIRSORT_FUR)...
	else if(hairMode == HAIRSORT_ORDERED)
	{
//////////////////////////////////////////////////////////////////////////////////////////////////////
		s32 countGeometry0 = model.GetGeometryCount();
		Assert(countGeometry0 <= MAX_HAIR_ORDER); // do not allow too many geometries in model
		if(countGeometry0 > MAX_HAIR_ORDER)
			countGeometry0 = MAX_HAIR_ORDER;

		const s32 countGeometry = countGeometry0;

		// fill in handy lookup table with hair order values:
		s32 tabCurrDrawOrder[MAX_HAIR_ORDER];	// stores hair order value for given's geometry shader

		s32 lastDrawOrder			= -1;	// last layer order
		s32 lastDrawOrderGeomIdx	= 0;
		for(s32 i=0; i<MAX_HAIR_ORDER; i++)
			tabCurrDrawOrder[i]=-1;

		for(s32 i=0; i<countGeometry; i++)
		{
			const grmShaderFx *shader = (grmShaderFx*)((pForceShader) ? pForceShader : &group[model.GetShaderIndex(i)]);
			grcEffectVar idVarDrawOrder = shader->LookupVar("OrderNumber", FALSE);
			if(idVarDrawOrder)
			{
				float fDrawOrder=0.0f;
				shader->GetVar(idVarDrawOrder, fDrawOrder);
				tabCurrDrawOrder[i] = (s32)fDrawOrder;
				if(tabCurrDrawOrder[i] > lastDrawOrder)
				{
					lastDrawOrder		= tabCurrDrawOrder[i];
					lastDrawOrderGeomIdx= i;
				}
			}
		}

		// 0. fix for problems with mixing of hair outer shell with outside world:
		// draw biggest (=last) layer of hair geometry in backface mode to mark outer space between head and outer world
		// (so effectively it masks out blending of hair front faces with world behind the head):
		if(/*TRUE*/lastDrawOrder != -1)
		{
			Assert(lastDrawOrder != -1);
			const s32 i=lastDrawOrderGeomIdx;
			{
				const grmShaderFx *shader = (grmShaderFx*)((pForceShader) ? pForceShader : &group[model.GetShaderIndex(i)]);
				int shaderBucketMask = (forceBucketMask==~0U) ? group[model.GetShaderIndex(i)].GetDrawBucketMask() : forceBucketMask;

				if (BUCKETMASK_MATCH(shaderBucketMask,bucketMask))
				{
					const bool restoreShaderState = false;
					CShaderHairSort::ShaderDrawSkinned(*shader,model,i,ms,lod,restoreShaderState,PASS_ALL,/*hOrder*/-1, hairMode,bIsShadowPass,bIsForwardPass);
				}
			}// for(int i=0; i<countGeometry; i++)...
		}// end of if(bEnableOuterLayer)...


		// 1. draw all the hair shells now in given order:
		for(s32 hOrder=0; hOrder<MAX_HAIR_ORDER; hOrder++)
		{
			for(s32 i=0; i<countGeometry; i++)
			{
				const grmShaderFx *shader = (grmShaderFx*)((pForceShader) ? pForceShader : &group[model.GetShaderIndex(i)]);
				int shaderBucketMask = (forceBucketMask==~0U) ? group[model.GetShaderIndex(i)].GetDrawBucketMask() : forceBucketMask;

				const s32 currDrawOrder = tabCurrDrawOrder[i];
				if(currDrawOrder == hOrder)
				{
					if (BUCKETMASK_MATCH(shaderBucketMask,bucketMask))				
					{
						//bool nextIsSame = pForceShader || (i+1<countGeometry && shader->GetTemplateIndex() == group[model.GetShaderIndex(i+1)].GetTemplateIndex());
						const bool restoreShaderState = false;	//(i+1==countGeometry || !nextIsSame);
						CShaderHairSort::ShaderDrawSkinned(*shader,model,i,ms,lod,restoreShaderState,PASS_ALL,hOrder,hairMode,bIsShadowPass,bIsForwardPass);
					}
				}
			}// for(int i=0; i<countGeometry; i++)...
		}// for(s32 hOrder=0; hOrder<MAX_HAIR_ORDER; hOrder++)...

//////////////////////////////////////////////////////////////////////////////////////////////////////
	}//if(hairMode == HAIRSORT_ORDERED)...
	else if(hairMode == HAIRSORT_SPIKED)
	{
//////////////////////////////////////////////////////////////////////////////////////////////////////
		s32 countGeometry0 = model.GetGeometryCount();
		Assertf((countGeometry0 <= MAX_HAIR_ORDER), "countGeometry0 %d must be less than MAX_HAIR_ORDER %d", countGeometry0, MAX_HAIR_ORDER); // do not allow too many geometries in model
		if(countGeometry0 > MAX_HAIR_ORDER)
			countGeometry0 = MAX_HAIR_ORDER;

		const s32 countGeometry = countGeometry0;

		// fill in handy lookup table with hair order values:
		s32 tabCurrDrawOrder[MAX_HAIR_ORDER];	// stores hair order value for given's geometry shader

		s32 lastDrawOrder			= -1;	// last layer order
		//s32 lastDrawOrderGeomIdx	= 0;
		for(s32 i=0; i<MAX_HAIR_ORDER; i++)
			tabCurrDrawOrder[i]=-1;

		for(s32 i=0; i<countGeometry; i++)
		{
			const grmShaderFx *shader = (grmShaderFx*)((pForceShader) ? pForceShader : &group[model.GetShaderIndex(i)]);
			grcEffectVar idVarDrawOrder = shader->LookupVar("OrderNumber", FALSE);
			if(idVarDrawOrder)
			{
				float fDrawOrder=0.0f;
				shader->GetVar(idVarDrawOrder, fDrawOrder);
				tabCurrDrawOrder[i] = (s32)fDrawOrder;
				if(tabCurrDrawOrder[i] > lastDrawOrder)
				{
					lastDrawOrder		= tabCurrDrawOrder[i];
					//lastDrawOrderGeomIdx= i;
				}
			}
		}

		// draw all the hair shells now in given order:
		if(bIsShadowPass)
		{
			// shadow pass: draw layer8 (or layer0 if 8 is not present in model):
			const s32 hOrder		= (lastDrawOrder==8) ? 8 : 0;
			const s32 forcedPass	= 0;
			for(s32 i=0; i<countGeometry; i++)
			{
				const s32 currDrawOrder = tabCurrDrawOrder[i];
				if(currDrawOrder == hOrder)
				{
					const grmShaderFx *shader = (grmShaderFx*)((pForceShader) ? pForceShader : &group[model.GetShaderIndex(i)]);
					const u32 shaderBucketMask = (forceBucketMask==~0U) ? group[model.GetShaderIndex(i)].GetDrawBucketMask() : forceBucketMask;

					if (BUCKETMASK_MATCH(shaderBucketMask,bucketMask))
					{
						//bool nextIsSame = pForceShader || (i+1<countGeometry && shader->GetTemplateIndex() == group[model.GetShaderIndex(i+1)].GetTemplateIndex());
						const bool restoreShaderState = false;	//(i+1==countGeometry || !nextIsSame);
						CShaderHairSort::ShaderDrawSkinned(*shader,model,i,ms,lod,restoreShaderState,forcedPass,hOrder,hairMode,bIsShadowPass,bIsForwardPass);
					}
				}
			}// for(int i=0; i<countGeometry; i++)...
		}
		else //if(bIsShadowPass)...
		{
			// draw layer0 - main cutout hair:
			{
				const s32 hOrder		= 0;
				const s32 forcedPass	= 0;
				for(s32 i=0; i<countGeometry; i++)
				{
					const s32 currDrawOrder = tabCurrDrawOrder[i];
					if(currDrawOrder == hOrder)
					{
						const grmShaderFx *shader = (grmShaderFx*)((pForceShader) ? pForceShader : &group[model.GetShaderIndex(i)]);
						const u32 shaderBucketMask = (forceBucketMask==~0U) ? group[model.GetShaderIndex(i)].GetDrawBucketMask() : forceBucketMask;

						if (BUCKETMASK_MATCH(shaderBucketMask,bucketMask))
						{
							//bool nextIsSame = pForceShader || (i+1<countGeometry && shader->GetTemplateIndex() == group[model.GetShaderIndex(i+1)].GetTemplateIndex());
							const bool restoreShaderState = false;	//(i+1==countGeometry || !nextIsSame);
							CShaderHairSort::ShaderDrawSkinned(*shader,model,i,ms,lod,restoreShaderState,forcedPass,hOrder,hairMode,bIsShadowPass,bIsForwardPass);
						}
					}
				}// for(int i=0; i<countGeometry; i++)...
			}

			bool bDrawLayer1 = bIsGbufPass || bIsHudPass;
			// for sub sample alpha marking, shadows or NOT GBuffer we don't need to do this as it's only for normal cap
			if(PostFX::GetMarkingSubSampledAlphaSamples() || bIsShadowPass)
			{
				bDrawLayer1 = FALSE;
			}

			// draw layer1 - external normal layer:
			if(bDrawLayer1)
			{
				const s32 hOrder		= 1;
				const s32 forcedPass	= 1;
				for(s32 i=0; i<countGeometry; i++)
				{
					const s32 currDrawOrder = tabCurrDrawOrder[i];
					if(currDrawOrder == hOrder)
					{
						const grmShaderFx *shader = (grmShaderFx*)((pForceShader) ? pForceShader : &group[model.GetShaderIndex(i)]);
						const u32 shaderBucketMask = (forceBucketMask==~0U) ? group[model.GetShaderIndex(i)].GetDrawBucketMask() : forceBucketMask;

						if (BUCKETMASK_MATCH(shaderBucketMask,bucketMask))
						{
							//bool nextIsSame = pForceShader || (i+1<countGeometry && shader->GetTemplateIndex() == group[model.GetShaderIndex(i+1)].GetTemplateIndex());
							const bool restoreShaderState = false;	//(i+1==countGeometry || !nextIsSame);
							CShaderHairSort::ShaderDrawSkinned(*shader,model,i,ms,lod,restoreShaderState,forcedPass,hOrder,hairMode,bIsShadowPass,bIsForwardPass);
						}
					}
				}// for(int i=0; i<countGeometry; i++)...
			}
		}
//////////////////////////////////////////////////////////////////////////////////////////////////////
	}//if(hairMode == HAIRSORT_SPIKED)...
	else if(hairMode == HAIRSORT_CUTOUT)
	{
		const s32 countGeometry = model.GetGeometryCount();

		for(s32 i=0; i<countGeometry; i++)
		{
			const grmShaderFx *shader = (grmShaderFx*)((pForceShader) ? pForceShader : &group[model.GetShaderIndex(i)]);
			int shaderBucketMask = (forceBucketMask==~0U) ? group[model.GetShaderIndex(i)].GetDrawBucketMask() : forceBucketMask;

			if (BUCKETMASK_MATCH(shaderBucketMask,bucketMask))
			{
				const bool restoreShaderState = false;
				CShaderHairSort::ShaderDrawSkinned(*shader,model,i,ms,lod,restoreShaderState,PASS_ALL,0,hairMode,bIsShadowPass,bIsForwardPass);
			}
		}// for(int i=0; i<countGeometry; i++)...
	}//if(hairMode == HAIRSORT_CUTOUT)...
	else if(hairMode == HAIRSORT_LONGHAIR)
	{
//////////////////////////////////////////////////////////////////////////////////////////////////////
		// long hair (PS3 only):
		// - 1st pass: draw into MRT0 and stencil mask w/alphaToMask enabled;
		// - 2nd pass: draw into MRT1+MRT2 (normals+specular) with above stencil mask:
		//
		// 360: draws single pass only with alphaToMask enabled straight away;


		const s32 countGeometry = model.GetGeometryCount();

		// draw all the hair shells now in given order:
		s32 forcedShaderPass=0;		// ps3: force shader pass#0 w/ non-clipped alpha!
		for(s32 i=0; i<countGeometry; i++)
		{
			const grmShaderFx *shader = (grmShaderFx*)((pForceShader) ? pForceShader : &group[model.GetShaderIndex(i)]);
			int shaderBucketMask = (forceBucketMask==~0U) ? group[model.GetShaderIndex(i)].GetDrawBucketMask() : forceBucketMask;

			if (BUCKETMASK_MATCH(shaderBucketMask,bucketMask))
			{
				const bool restoreShaderState = false;
				CShaderHairSort::ShaderDrawSkinned_LongHair(*shader,model,i,ms,lod,restoreShaderState,forcedShaderPass,hairMode);
			}
		}// for(int i=0; i<countGeometry; i++)...
		
//////////////////////////////////////////////////////////////////////////////////////////////////////
	}//if(hairMode == HAIRSORT_LONGHAIR)...


	if(g_bRestoreStateBlocks)
	{
		// restore previous stateblocks
		SetRasterizerState(RS_prev);
		SetDepthStencilState(DSS_prev,prevStencilRef);
		SetBlendState(BS_prev, prevBlendFactors, prevSampleMask);
	}

#if MTX_IN_VB
	GRCDEVICE.GetCurrent()->SetStreamSource(1,NULL,0,0);
#endif

}// end of ModelDrawSkinned()....


//
//
// 4. stolen from grmShader::DrawSkinned():
//
void CShaderHairSort::ShaderDrawSkinned(const grmShader &shader, const grmModel &model,int geom,const grmMatrixSet &ms,int /*lod*/,bool restoreState, int forcedPass, int hOrder, eHairSortMode hairMode, bool bIsShadowPass, bool bIsForwardPass)
{
	rage::grmShader::eDrawType drawMode = grmShader::RMC_DRAWSKINNED;

	if(hOrder==-1)
	{	// special outer backfaced hair layer:
		const s32 numPasses = shader.BeginDraw(drawMode, restoreState);
		const s32 pass1=0;	// we want only 1st pass
		for(int pass=0; pass<numPasses; pass++)
		{
			if(pass==pass1)
			{
				shader.Bind(pass);
				{
					grcStateBlock::SetRasterizerState(hPedHairHOrderMinusOneRS_pass1);				
					grcStateBlock::SetDepthStencilState(hPedHairDSS);
					grcStateBlock::SetBlendState(hPedHairHOrderMinusOneBS_pass1, ~0U, grcStateBlock::ActiveSampleMask);
				
					model.DrawSkinned(geom,ms);
				}
				shader.UnBind();
			}// if(pass==pass1)...
		}// for(int pass=0; pass<numPasses; pass++)...
		shader.EndDraw();
			
		grcStateBlock::SetRasterizerState(hPedHairHOrderMinusOneRS_pass1_end);
	}
	else //if(hOrder==-1)
	{
		const s32 numPasses = shader.BeginDraw(drawMode, restoreState);
		for(int pass=0; pass<numPasses; pass++)
		{
			if(forcedPass != PASS_ALL)
			{
				if(pass != forcedPass)
					continue;
			}

			shader.Bind(pass);

			if(!bIsShadowPass)
			{
				g_bRestoreStateBlocks = true;

				if(hairMode == HAIRSORT_FUR)
				{
					if(!PostFX::GetMarkingSubSampledAlphaSamples())
					{
						u32 stencilRef =  CShaderLib::GetGlobalDeferredMaterial() | DEFERRED_MATERIAL_PED;
						grcStateBlock::SetDepthStencilState(hPedHairDSS, stencilRef);
						grcStateBlock::SetBlendState(hHairFurBS, ~0U, grcStateBlock::ActiveSampleMask);
					}
				} //HAIRSORT_FUR...
				else if(hairMode == HAIRSORT_ORDERED)
				{
					if(hOrder==0)
					{	// 1st layer:
						grcStateBlock::SetDepthStencilState(hPedHairDSS, CShaderLib::GetGlobalDeferredMaterial()+DEFERRED_MATERIAL_PED);
						grcStateBlock::SetBlendState(hHairOrderedBS_order0, ~0U, grcStateBlock::ActiveSampleMask);
					}//if(hOrder==0)...
					else
					{	// every other layer:
						grcStateBlock::SetDepthStencilState(hPedHairDSS, CShaderLib::GetGlobalDeferredMaterial()+DEFERRED_MATERIAL_PED);
						grcStateBlock::SetBlendState(hHairOrderedBS_orderN, ~0U, grcStateBlock::ActiveSampleMask);
					}
				}//HAIRSORT_ORDERED...
				else if(hairMode == HAIRSORT_SPIKED)
				{
					if(bIsForwardPass)
					{
						if(hOrder==0)
						{
							// NG: alphaTest doesn't exist on NG
							//const u32 rsAlphaRef = 100;
							//grcStateBlock::SetBlendState(hHairSpikedBS_forward, ~0U, ~0ULL, rsAlphaRef);
						}
					}
					else
					{
						const u32 DEFERRED_MATERIAL_PED_PD0x10 = CShaderLib::GetGlobalDeferredMaterial()+DEFERRED_MATERIAL_SPECIALBIT;	// pedMatID (with interior/mb/etc)+pass detect mask
						CompileTimeAssert(DEFERRED_MATERIAL_PED			==0x01);
						CompileTimeAssert(DEFERRED_MATERIAL_SPECIALBIT	==0x10);
						
						if(hOrder==0)
						{
							// 1st layer:
							if(PostFX::UseSubSampledAlpha())
							{
								if(PostFX::GetMarkingSubSampledAlphaSamples())
								{
									// do nothing
								}
								else
								{
									if(grcStateBlock::ActiveSampleMask != (~0ULL))
									{
										// BS#2060619: do not write SSA alpha when stippling:
										grcStateBlock::SetBlendState(PostFX::UseSinglePassSSA()? hHairSpikedBS_order0_singlepass_ssa_0rgb : hHairSpikedBS_order0_ssa, ~0U, grcStateBlock::ActiveSampleMask);
									}
									else
									{
									#if !RSG_ORBIS && !RSG_DURANGO
										if(CShaderLib::GetFadeMask16() != 0xFFFF)
										{
											// BS#2159489: do not write SSA alpha when stippling:
											grcStateBlock::SetBlendState(PostFX::UseSinglePassSSA()? hHairSpikedBS_order0_singlepass_ssa_0rgb : hHairSpikedBS_order0_ssa, ~0U, ~0ULL);
										}
										else
									#endif
										{
											grcStateBlock::SetBlendState(PostFX::UseSinglePassSSA()? hHairSpikedBS_order0_singlepass_ssa : hHairSpikedBS_order0_ssa, ~0U, ~0ULL);
										}
									}
								}
							}
							else
							{
								grcStateBlock::SetBlendState(hHairSpikedBS_order0, ~0U, grcStateBlock::ActiveSampleMask);
							}

							if (PostFX::GetMarkingSubSampledAlphaSamples())
								grcStateBlock::SetDepthStencilState(CShaderLib::DSS_TestOnly_Invert);
							else
								grcStateBlock::SetDepthStencilState(hHairSpikedDSS_order0, DEFERRED_MATERIAL_PED_PD0x10);
						}//if(hOrder==0)...
						else if(hOrder==1)
						{	// 1st layer: only normals:
							grcStateBlock::SetDepthStencilState(hHairSpikedDSS_order1, DEFERRED_MATERIAL_PED_PD0x10&0x17);

							if(PostFX::GetMarkingSubSampledAlphaSamples())
							{
								grcStateBlock::SetBlendState(hHairSpikedBS_order1_ssa, ~0U, grcStateBlock::ActiveSampleMask);
							}
							else
							{
								grcStateBlock::SetBlendState(hHairSpikedBS_order1, ~0U, grcStateBlock::ActiveSampleMask);
							}
						}
					}
				}//HAIRSORT_SPIKED...
				else if(hairMode == HAIRSORT_CUTOUT)
				{
					if(bIsForwardPass)
					{
						// NG: alphaTest doesn't exist on NG
						//const u32 rsAlphaRef = 100;
						//grcStateBlock::SetBlendState(hHairSpikedBS_forward, ~0U, ~0ULL, rsAlphaRef);
					}
					else
					{
						if(PostFX::UseSubSampledAlpha())
						{
							if(PostFX::GetMarkingSubSampledAlphaSamples())
							{
								// do nothing
							}
							else
							{
								if(grcStateBlock::ActiveSampleMask != (~0ULL))
								{
									// BS#2060619: do not write SSA alpha when stippling:
									grcStateBlock::SetBlendState(PostFX::UseSinglePassSSA()? hHairCutoutBS_order0_singlepass_ssa_0rgb : hHairCutoutBS_order0_ssa, ~0U, grcStateBlock::ActiveSampleMask);
								}
								else
								{
								#if !RSG_ORBIS && !RSG_DURANGO
									if(CShaderLib::GetFadeMask16() != 0xFFFF)
									{
										// BS#2159489: do not write SSA alpha when stippling:
										grcStateBlock::SetBlendState(PostFX::UseSinglePassSSA()? hHairCutoutBS_order0_singlepass_ssa_0rgb : hHairCutoutBS_order0_ssa, ~0U, ~0ULL);
									}
									else
								#endif
									{
										grcStateBlock::SetBlendState(PostFX::UseSinglePassSSA()? hHairCutoutBS_order0_singlepass_ssa : hHairCutoutBS_order0_ssa, ~0U, ~0ULL);
									}
								}
							}
						}
						else
						{
							grcStateBlock::SetBlendState(hHairCutoutBS_order0, ~0U, grcStateBlock::ActiveSampleMask);
						}
					}
				}//HAIRSORT_CUTOUT...
			}

			model.DrawSkinned(geom,ms);

			if(!bIsShadowPass)
			{
				if(hairMode == HAIRSORT_SPIKED)
				{
					if(bIsForwardPass)
					{
						// do nothing
					}
					else
					{
						if(hOrder==1)
						{	// cleanup:
							grcStateBlock::SetDepthStencilState(hHairSpikedDSS_order1_end, CShaderLib::GetGlobalDeferredMaterial());
						}
					}
				}
			}

			shader.UnBind();
		}// for(int pass=0; pass<numPasses; pass++)...
		shader.EndDraw();

	}//	if(hOrder==-1)....


}// end of ShaderDrawSkinned()...


//
//
// 4. stolen from grmShader::DrawSkinned():
//
void CShaderHairSort::ShaderDrawSkinned_LongHair(const grmShaderFx &shader, const grmModel &model,int geom,const grmMatrixSet &ms,int /*lod*/,bool restoreState, int forcedPass, eHairSortMode ASSERT_ONLY(hairMode))
{
	rage::grmShader::eDrawType drawMode = grmShader::RMC_DRAWSKINNED;

		Assert(hairMode == HAIRSORT_LONGHAIR);

		const s32 numPasses = shader.BeginDraw(drawMode, restoreState);
		if(numPasses)
		{
			Assertf(forcedPass < numPasses, "ForcedPass=%d, maxNumPasses=%d, drawlistType=%d", forcedPass, numPasses, (u32)DRAWLISTMGR->GetCurrExecDLInfo()->m_type);

			const s32 pass = forcedPass;	// force shader pass
			
			shader.Bind(pass);
				model.DrawSkinned(geom,ms);
			shader.UnBind();
		}
		shader.EndDraw();

}// end of ShaderDrawSkinned_LongHair()...








