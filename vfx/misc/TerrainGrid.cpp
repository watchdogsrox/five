#include "TerrainGrid.h"

#include "bank/bank.h"
#include "grcore/effect.h"
#include "grmodel/shader.h"
#include "grcore/texture.h"
#include "grcore/debugdraw.h"

#include "physics/gtaArchetype.h"
#include "fwscene/stores/txdstore.h"
#include "renderer/rendertargets.h"
#include "renderer/Util/ShaderUtil.h"
#include "camera/CamInterface.h"
#include "shaders/shaderlib.h"
#include "physics/WorldProbe/shapetestmgr.h"
#include "physics/WorldProbe/WorldProbe.h"

#include "fwdebug/picker.h"

//OPTIMISATIONS_OFF()

//////////////////////////////////////////////////////////////////////////
//
TerrainGrid* TerrainGrid::sm_pInstance = NULL;


//////////////////////////////////////////////////////////////////////////
//
void TerrainGrid::InitClass()
{
	Assert(sm_pInstance == NULL);

	sm_pInstance = rage_new TerrainGrid();

	if (Verifyf(sm_pInstance != NULL, "TerrainGrid::InitClass: couldn't allocate instance"))
	{
		GOLFGREENGRID.Init();
	}
}

//////////////////////////////////////////////////////////////////////////
//
void TerrainGrid::ShutdownClass()
{
	delete sm_pInstance;
	sm_pInstance = NULL;
}


//////////////////////////////////////////////////////////////////////////
//
void TerrainGrid::Init()
{
#if __BANK
	m_useHeightRefFromPicker = false;
	m_bOverrideSettings = false;
	m_bShowProjectionBox = false;
	m_bApplyTweaksToPosAndDir = false;
	m_vOffsetAlongForward = ScalarV(V_ZERO);
	m_vOffsetAlongRight = ScalarV(V_ZERO);
	m_vPitchAdjustment = ScalarV(V_ZERO);
	m_gridBaseColor.Set(0,0,0,0);
#endif
	m_pShader = NULL;
	m_bFollowCamera = false;
	m_vCentrePosition.ZeroComponents();
	m_vProjectionDir.ZeroComponents();
	m_vGridMiscParams.ZeroComponents();

	// grid resolution
	m_vGridMiscParams.SetX(ScalarVFromF32(20.0f));
	// grid colour multiplier
	m_vGridMiscParams.SetY(ScalarV(V_FIVE));
	// grid min. height
	m_vGridMiscParams.SetZ(ScalarV(V_ONE));
	// grid max. height
	m_vGridMiscParams.SetW(ScalarV(V_TWO));

	m_vDimensions = Vec4V(V_ONE);

	// grid box width
	m_vDimensions.SetX(ScalarV(V_TEN));

	// grid box height
	m_vDimensions.SetY(ScalarVFromF32(20.0f));

	m_vForward = Vec4V(V_Y_AXIS_WZERO);
	m_vRight = Vec4V(V_X_AXIS_WZERO);

	// data set via public interface
	m_UserGridCentrePosition.Zero();
	m_UserForwardDir.Zero();
	m_UserGridMiscParams.x = 20.0f;
	m_UserGridMiscParams.y = 5.0f;
	m_UserGridMiscParams.z = 1.0f;
	m_UserGridMiscParams.w = 2.0f;

	m_lowHeightColor = Color32(255, 0, 0, 255);
	m_midHeightColor = Color32(255, 255, 255, 0);
	m_highHeightColor = Color32(255, 255, 0, 255);

	m_bEnabled = false;
	m_bRender = false;
	InitRenderData();
}

void TerrainGrid::InitShaders()
{
	if(m_pShader)
	{
		delete m_pShader;
		m_pShader = NULL;
	}

	const char* pName = MSAA_ONLY(GRCDEVICE.GetMSAA() > 1 ? "vfx_decaldynMS": ) "vfx_decaldyn";

	// load shader
	m_pShader = grmShaderFactory::GetInstance().Create();
	Assertf(m_pShader, "TerrainGrid::InitRenderData: cannot create '%s' shader", pName);

	if (!m_pShader->Load(pName))
	{
		Assertf(0, "TerrainGrid::InitRenderData: error loading '%s' shader", pName);
		return;
	}

	// look up technique
	m_technique = m_pShader->LookupTechnique("terrain_grid", true);
	Assertf(m_technique != grcetNONE, "TerrainGrid::InitRenderData: could not find technique");

	// look up shader vars
	m_shaderVarIdDiffTexture			= m_pShader->LookupVar("decalTexture");
	m_shaderVarIdDepthTexture			= grcEffect::LookupGlobalVar("gbufferTextureDepthGlobal");
	m_shaderVarIdScreenSize				= m_pShader->LookupVar("deferredLightScreenSize");
	m_shaderVarIdProjParams				= m_pShader->LookupVar("deferredProjectionParams");
	m_shaderVarIdTerrainGridProjDir		= m_pShader->LookupVar("TerrainGridProjDir");
	m_shaderVarIdTerrainGridCentrePos	= m_pShader->LookupVar("TerrainGridCentrePos");
	m_shaderVarIdTerrainGridForward		= m_pShader->LookupVar("TerrainGridForward");
	m_shaderVarIdTerrainGridRight		= m_pShader->LookupVar("TerrainGridRight");
	m_shaderVarIdTerrainGridParams		= m_pShader->LookupVar("TerrainGridParams");
	m_shaderVarIdTerrainGridParams2		= m_pShader->LookupVar("TerrainGridParams2");
	m_shaderVarIdLowHeightCol			= m_pShader->LookupVar("TerrainGridLowHeightCol");
	m_shaderVarIdMidHeightCol			= m_pShader->LookupVar("TerrainGridMidHeightCol");
	m_shaderVarIdHighHeightCol			= m_pShader->LookupVar("TerrainGridHighHeightCol");
#if __XENON || __D3D11 || RSG_ORBIS
	m_shaderVarIdStencilTexture			= grcEffect::LookupGlobalVar("gbufferStencilTextureGlobal");
#endif
}

//////////////////////////////////////////////////////////////////////////
//
void TerrainGrid::InitRenderData()
{
	InitShaders();

	// setup state blocks
	m_defaultBS	= grcStateBlock::BS_Normal;
	m_defaultRS	= grcStateBlock::RS_NoBackfaceCull;

	grcDepthStencilStateDesc dsdTest;
	dsdTest.DepthEnable = TRUE;
	dsdTest.DepthWriteMask = FALSE;
	dsdTest.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
	dsdTest.StencilEnable = FALSE;
	m_defaultDSS = grcStateBlock::CreateDepthStencilState(dsdTest);


	strLocalIndex txdSlot = strLocalIndex(CShaderLib::GetTxdId());
	m_pGridTexture = g_TxdStore.Get(txdSlot)->Lookup(ATSTRINGHASH("terrain_grid", 0x02f58f02e));

	grcSamplerStateDesc desc;
	desc.AddressU = grcSSV::TADDRESS_MIRROR;
	desc.AddressV = grcSSV::TADDRESS_MIRROR;
	desc.Filter = grcSSV::FILTER_ANISOTROPIC;
	desc.MaxAnisotropy = 6;
	m_diffuseMapSampler = grcStateBlock::CreateSamplerState(desc);

}

//////////////////////////////////////////////////////////////////////////
//
void TerrainGrid::Activate(bool bEnable) 
{
	m_bEnabled = bEnable;
}

//////////////////////////////////////////////////////////////////////////
//
void TerrainGrid::SetBoxParams(Vector3::Vector3Param dimsWHD, Vector3::Vector3Param pos, Vector3::Vector3Param dir, Vector4::Vector4Param miscParams) 
{
	if (m_bEnabled == false BANK_ONLY(|| m_bOverrideSettings == true))
	{
		return;
	}

	m_UserGridCentrePosition	= pos;
	m_UserForwardDir			= dir;
	m_UserGridMiscParams		= miscParams;
	m_UserDimensions			= dimsWHD;
}

//////////////////////////////////////////////////////////////////////////
//
void TerrainGrid::SetColours(Color32 lowHeight, Color32 midHeight, Color32 highHeight)
{
	m_lowHeightColor = lowHeight;
	m_midHeightColor = midHeight;
	m_highHeightColor = highHeight;
}

//////////////////////////////////////////////////////////////////////////
//
void TerrainGrid::Update()
{

	if (m_bEnabled == false BANK_ONLY(&& m_bOverrideSettings == false))
	{
		m_bRender = false;
		return;
	}

#if __BANK
	if (m_bFollowCamera)
	{
		// If the "Follow Camera" mode is enabled, the projection box will be adjusted to match the camera orientation;
		// its position will be adjusted to appear in front of the camera. Three collision probes positioned at the front, 
		// middle and back positions of the box will further refine the forward direction of the box, as well as its
		// position (i.e.: a rough fit to match the geometry that the box will be projected onto).
		// The minimum and maximum height values used the grid colouring are also derived from the probe results.

		// default to camera position and direction
		m_vForward.SetXYZ(VECTOR3_TO_VEC3V(camInterface::GetFront()));
		m_vForward.SetZ(ScalarV(V_ZERO));
		m_vCentrePosition = VECTOR3_TO_VEC3V(camInterface::GetPos());
		m_vCentrePosition = m_vCentrePosition + m_vForward.GetXYZ()*m_vDimensions.GetY()*ScalarV(V_HALF);

		Vec3V vAdjustedPos0 = m_vCentrePosition;
		Vec3V vAdjustedPos1 = m_vCentrePosition;
		Vec3V vAdjustedPos2 = m_vCentrePosition;

		// setup probe data
		Vec3V vProbeDepth = Vec3V(Vec2V(V_ZERO), ScalarVFromF32(-25.0f));		
		Vec3V vStartPos0 = m_vCentrePosition;
		Vec3V vStartPos1 = m_vCentrePosition - m_vForward.GetXYZ()*m_vDimensions.GetY()*ScalarV(V_HALF);
		Vec3V vStartPos2 = m_vCentrePosition + m_vForward.GetXYZ()*m_vDimensions.GetY()*ScalarV(V_HALF);
		Vec3V vEndPos0 =  vStartPos0 + vProbeDepth;
		Vec3V vEndPos1 =  vStartPos1 + vProbeDepth;
		Vec3V vEndPos2 =  vStartPos2 + vProbeDepth;

		u32 probeFlags = ArchetypeFlags::GTA_MAP_TYPE_WEAPON;
		WorldProbe::CShapeTestProbeDesc probeDesc;
		WorldProbe::CShapeTestHitPoint probeResult;
		WorldProbe::CShapeTestResults probeResults(probeResult);
		probeDesc.SetResultsStructure(&probeResults);

		// test first position
		probeDesc.SetStartAndEnd(RCC_VECTOR3(vStartPos0), RCC_VECTOR3(vEndPos0));
		probeDesc.SetIncludeFlags(probeFlags);
		if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
		{
			vAdjustedPos0 = RCC_VEC3V(probeResults[0].GetHitPosition());
		}

		// test second position
		probeDesc.SetStartAndEnd(RCC_VECTOR3(vStartPos1), RCC_VECTOR3(vEndPos1));
		probeDesc.SetIncludeFlags(probeFlags);
		if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
		{
			vAdjustedPos1 = RCC_VEC3V(probeResults[0].GetHitPosition());
		}

		// test third position
		probeDesc.SetStartAndEnd(RCC_VECTOR3(vStartPos2), RCC_VECTOR3(vEndPos2));
		probeDesc.SetIncludeFlags(probeFlags);
		if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
		{
			vAdjustedPos2 = RCC_VEC3V(probeResults[0].GetHitPosition());
			Vec3V vTmp = m_vForward.GetXYZ();
			m_vForward.SetXYZ(NormalizeSafe(vAdjustedPos2-vAdjustedPos0, vTmp));
		}

		m_vCentrePosition = (vAdjustedPos0+vAdjustedPos1+vAdjustedPos2)*ScalarV(V_THIRD);

		// adjust reference height to be the average height
		m_vGridMiscParams.SetZ(m_vCentrePosition.GetZ());
		ScalarV vDelta = Scale(Max(Max(vAdjustedPos0.GetZ(), vAdjustedPos1.GetZ()), vAdjustedPos2.GetZ())-Min(Min(vAdjustedPos0.GetZ(), vAdjustedPos1.GetZ()), vAdjustedPos2.GetZ()), ScalarV(V_HALF));
		m_vGridMiscParams.SetW(vDelta);
	}
	else if (m_bOverrideSettings == false)
#endif
	{
		m_vCentrePosition = VECTOR3_TO_VEC3V(m_UserGridCentrePosition);
		m_vForward.SetXYZ(VECTOR3_TO_VEC3V(m_UserForwardDir));
		m_vGridMiscParams = VECTOR4_TO_VEC4V(m_UserGridMiscParams);
		m_vDimensions = VECTOR3_TO_VEC4V(m_UserDimensions);
	}

#if __BANK
	if (m_useHeightRefFromPicker)
	{
		s32 numEntities = g_PickerManager.GetNumberOfEntities();
		if (numEntities == 1)
		{
			const fwEntity* pEntity = g_PickerManager.GetEntity(0);
			if (pEntity)
			{
				//const Vector3 position = pEntity->GetPreviousPosition();
				spdAABB aabb;
				pEntity->GetAABB(aabb);
				Vec3V vMin = aabb.GetMin();
				m_vGridMiscParams.SetZf(vMin.GetZf());
				
				if (m_bOverrideSettings == false)
				{
					m_vGridMiscParams.SetYf(10.0f);
					m_vGridMiscParams.SetWf(0.25f);
				}
			}
		}
	}
#endif

	m_bRender = true;
}

//////////////////////////////////////////////////////////////////////////
//
void TerrainGrid::Render()
{
	if (m_bRender == false)
	{
		return;
	}

	const grcTexture *depthSource = CRenderTargets::GetDepthBuffer();
#if RSG_PC
	depthSource = CRenderTargets::LockReadOnlyDepth();
#endif

	SetupGrid();
	SetRenderState();
	SetShaderConstants(depthSource);

	BindShader();
	RenderGrid();
	UnbindShader();

	grcStateBlock::SetStates(grcStateBlock::RS_Default, grcStateBlock::DSS_Default, grcStateBlock::BS_Default);

#if RSG_PC
	CRenderTargets::UnlockReadOnlyDepth();
#endif
#if __BANK
	RenderDebug();
#endif

}

//////////////////////////////////////////////////////////////////////////
//
void TerrainGrid::SetupGrid()
{
	Vec3V vPos		= m_vCentrePosition;
	Vec3V vForward	= m_vForward.GetXYZ();
	Vec3V vUp		= Vec3V(V_Z_AXIS_WZERO);
	Vec3V vRight	= Normalize(Cross(vUp, vForward));
	vUp 			= Cross(vForward, vRight);
	vUp 			= Normalize(vUp);

#if __BANK
	vPos = vPos + vForward*m_vOffsetAlongForward;
	vPos = vPos + vRight*m_vOffsetAlongRight; 

	if (m_bFollowCamera)
	{
		Mat33V vRotMat;
		Mat33VFromAxisAngle(vRotMat, vRight, m_vPitchAdjustment*ScalarV(V_TO_RADIANS));

		Mat34V vTransformMat(vRotMat, Vec3V(V_ZERO));
		vForward	= Normalize(Transform(vTransformMat, vForward));
		vUp			= Normalize(Transform(vTransformMat, vUp));
		vRight		= Normalize(Cross(vUp, vForward));
	}

	if (m_bApplyTweaksToPosAndDir)
	{
		m_vCentrePosition = m_vCentrePosition + m_vForward.GetXYZ()*m_vOffsetAlongForward;
		m_vCentrePosition = m_vCentrePosition + m_vForward.GetXYZ()*m_vOffsetAlongForward;
		m_vForward.SetXYZ(vForward);

		m_bApplyTweaksToPosAndDir = false;
		m_vOffsetAlongForward = ScalarV(V_ZERO);
		m_vOffsetAlongRight = ScalarV(V_ZERO);
		m_vPitchAdjustment = ScalarV(V_ZERO);
	}
#endif

	m_vProjectionDir			= Normalize(Cross(vForward, vRight));

	const ScalarV vHalfWidth	= Scale(m_vDimensions.GetX(), ScalarV(V_HALF));
	const ScalarV vHalfHeight	= Scale(m_vDimensions.GetY(), ScalarV(V_HALF));
	const Vec3V vDepthOffset	= Vec3V(Vec2V(V_ZERO), m_vDimensions.GetZ());

	const Vec3V vForwardScaled	= Scale(vForward, vHalfHeight);
	const Vec3V vRightScaled	= Scale(vRight, vHalfWidth);

	m_vVerts[0] = vPos + vForwardScaled - vRightScaled;
	m_vVerts[1] = vPos + vForwardScaled + vRightScaled;
	m_vVerts[2] = vPos - vForwardScaled + vRightScaled;
	m_vVerts[3] = vPos - vForwardScaled - vRightScaled;	

	m_vVerts[4] = m_vVerts[0] - vDepthOffset;
	m_vVerts[5] = m_vVerts[1] - vDepthOffset;
	m_vVerts[6] = m_vVerts[2] - vDepthOffset;
	m_vVerts[7] = m_vVerts[3] - vDepthOffset;	

	m_vForward.SetXYZ(vForward);
	m_vForward.SetW(InvMag(vForwardScaled));
	m_vRight.SetXYZ(vRight);
	m_vRight.SetW(InvMag(vRightScaled));

}

//////////////////////////////////////////////////////////////////////////
//
void TerrainGrid::RenderGrid()
{
	grcWorldIdentity();

	grcColor(m_gridBaseColor);
	grcTexCoord2f(0.0f, 0.0f);

	grcBegin(drawTris,12 * 3);
	grcNormal3f( 0.0f,  0.0f, -1.0f);
	grcVertex3f(m_vVerts[3]);
	grcVertex3f(m_vVerts[2]);
	grcVertex3f(m_vVerts[1]);

	grcVertex3f(m_vVerts[3]);
	grcVertex3f(m_vVerts[1]);
	grcVertex3f(m_vVerts[0]);

	grcNormal3f( 0.0f,  0.0f,  1.0f);
	grcVertex3f(m_vVerts[4]);
	grcVertex3f(m_vVerts[5]);
	grcVertex3f(m_vVerts[6]);

	grcVertex3f(m_vVerts[4]);
	grcVertex3f(m_vVerts[6]);
	grcVertex3f(m_vVerts[7]);

	grcNormal3f( 1.0f,  0.0f, 0.0f);
	grcVertex3f(m_vVerts[1]);
	grcVertex3f(m_vVerts[2]);
	grcVertex3f(m_vVerts[6]);

	grcVertex3f(m_vVerts[1]);
	grcVertex3f(m_vVerts[6]);
	grcVertex3f(m_vVerts[5]);

	grcNormal3f( 0.0f,  1.0f, 0.0f);
	grcVertex3f(m_vVerts[2]);
	grcVertex3f(m_vVerts[3]);
	grcVertex3f(m_vVerts[7]);

	grcVertex3f(m_vVerts[2]);
	grcVertex3f(m_vVerts[7]);
	grcVertex3f(m_vVerts[6]);

	grcNormal3f(-1.0f,  0.0f, 0.0f);
	grcVertex3f(m_vVerts[0]);
	grcVertex3f(m_vVerts[4]);
	grcVertex3f(m_vVerts[7]);

	grcVertex3f(m_vVerts[0]);
	grcVertex3f(m_vVerts[7]);
	grcVertex3f(m_vVerts[3]);

	grcNormal3f( 0.0f, -1.0f, 0.0f);
	grcVertex3f(m_vVerts[0]);
	grcVertex3f(m_vVerts[1]);
	grcVertex3f(m_vVerts[5]);

	grcVertex3f(m_vVerts[0]);
	grcVertex3f(m_vVerts[5]);
	grcVertex3f(m_vVerts[4]);
	grcEnd();
}

//////////////////////////////////////////////////////////////////////////
//
grcTexture* TerrainGrid::GetDepthTexture()
{
#if __PPU
	return CRenderTargets::GetDepthBufferAsColor();
#else
	return CRenderTargets::GetDepthBuffer();
#endif
}

//////////////////////////////////////////////////////////////////////////
//
void TerrainGrid::SetRenderState()
{
	grcStateBlock::SetDepthStencilState(m_defaultDSS);//, DEFERRED_MATERIAL_TERRAIN);
	grcStateBlock::SetBlendState(m_defaultBS);
	grcStateBlock::SetRasterizerState(m_defaultRS);
}

//////////////////////////////////////////////////////////////////////////
//
void TerrainGrid::SetShaderConstants(const grcTexture *depth)
{
	grcEffect::SetGlobalVar(m_shaderVarIdDepthTexture, depth);

#if __XENON 
	grcEffect::SetGlobalVar(m_shaderVarIdStencilTexture, GBuffer::GetDepthTargetAsColor());
#elif __D3D11 || RSG_ORBIS
	grcEffect::SetGlobalVar(m_shaderVarIdStencilTexture, CRenderTargets::GetDepthBuffer_Stencil());
#endif

	// calc and set the one over screen size shader var
	const grcViewport *vp = grcViewport::GetCurrent();
	const float width = static_cast<float>(vp->GetWidth());
	const float height = static_cast<float>(vp->GetHeight());

	Vec4V vScreenSize = Vec4V( width, height, 1.0f/width, 1.0f/height);
	m_pShader->SetVar(m_shaderVarIdScreenSize, RCC_VECTOR4(vScreenSize));

	// calc and set the projection paramater shader var
	Vector4 projParams = ShaderUtil::CalculateProjectionParams();
	m_pShader->SetVar(m_shaderVarIdProjParams, projParams);

	m_pShader->SetVar(m_shaderVarIdTerrainGridProjDir, m_vProjectionDir);
	m_pShader->SetVar(m_shaderVarIdTerrainGridCentrePos, m_vCentrePosition);
	m_pShader->SetVar(m_shaderVarIdTerrainGridForward, m_vForward);
	m_pShader->SetVar(m_shaderVarIdTerrainGridRight, m_vRight);

	m_pShader->SetVar(m_shaderVarIdTerrainGridParams, m_vGridMiscParams);
	m_pShader->SetVar(m_shaderVarIdDiffTexture, m_pGridTexture);


	Vec4V vGridMiscParams = m_vGridMiscParams;
	Vec4V vGridMiscParams2;

	// min height: refHeight - delta height
	vGridMiscParams2.SetX((vGridMiscParams.GetZ()-vGridMiscParams.GetW())-ScalarV(V_FLT_SMALL_2));
	// max height: refHeight + delta height
	vGridMiscParams2.SetY((vGridMiscParams.GetZ()+vGridMiscParams.GetW())+ScalarV(V_FLT_SMALL_2));
	// 1/(refHeight-minHeight)
	vGridMiscParams2.SetZ(Invert(vGridMiscParams.GetZ()-vGridMiscParams2.GetX()));
	// 1/(maxHeight-refHeight)
	vGridMiscParams2.SetW(Invert(vGridMiscParams2.GetY()-vGridMiscParams.GetZ()));

	m_pShader->SetVar(m_shaderVarIdTerrainGridParams2, vGridMiscParams2);
	m_pShader->SetVar(m_shaderVarIdLowHeightCol, m_lowHeightColor);
	m_pShader->SetVar(m_shaderVarIdMidHeightCol, m_midHeightColor);
	m_pShader->SetVar(m_shaderVarIdHighHeightCol, m_highHeightColor);

}

//////////////////////////////////////////////////////////////////////////
//
void TerrainGrid::BindShader()
{
	AssertVerify(m_pShader->BeginDraw(grmShader::RMC_DRAW, true, m_technique));
	m_pShader->PushSamplerState(m_shaderVarIdDiffTexture, m_diffuseMapSampler);
	m_pShader->Bind((int)0);
}

//////////////////////////////////////////////////////////////////////////
//
void TerrainGrid::UnbindShader()
{
	const grcTexture* pNullTexture = NULL;
	m_pShader->UnBind();
	m_pShader->EndDraw();
	m_pShader->SetVar(m_shaderVarIdDiffTexture, pNullTexture);
	grcEffect::SetGlobalVar(m_shaderVarIdDepthTexture, pNullTexture);
#if __XENON
	grcEffect::SetGlobalVar(m_shaderVarIdStencilTexture, pNullTexture);
#endif

	m_pShader->PopSamplerState(m_shaderVarIdDiffTexture);
}

//////////////////////////////////////////////////////////////////////////
//
#if __BANK
void TerrainGrid::RenderDebug()
{
	if (m_bShowProjectionBox)
	{
		Color32 col(255, 128, 0, 255);
		grcDebugDraw::Line(m_vVerts[0], m_vVerts[1], col);
		grcDebugDraw::Line(m_vVerts[1], m_vVerts[2], col);
		grcDebugDraw::Line(m_vVerts[2], m_vVerts[3], col);
		grcDebugDraw::Line(m_vVerts[3], m_vVerts[0], col);

		grcDebugDraw::Line(m_vVerts[0+4], m_vVerts[1+4], col);
		grcDebugDraw::Line(m_vVerts[1+4], m_vVerts[2+4], col);
		grcDebugDraw::Line(m_vVerts[2+4], m_vVerts[3+4], col);
		grcDebugDraw::Line(m_vVerts[3+4], m_vVerts[0+4], col);

		grcDebugDraw::Line(m_vVerts[0], m_vVerts[4], col);
		grcDebugDraw::Line(m_vVerts[1], m_vVerts[5], col);
		grcDebugDraw::Line(m_vVerts[2], m_vVerts[6], col);
		grcDebugDraw::Line(m_vVerts[3], m_vVerts[7], col);
	}
}
#endif

//////////////////////////////////////////////////////////////////////////
//
#if __BANK
void TerrainGrid::AddWidgets(bkBank* pBank)
{
	pBank->PushGroup("Terrain Grid");

		pBank->AddToggle("Enable Override", &m_bOverrideSettings);

		pBank->PushGroup("Edit");
			pBank->AddToggle("Show Grid Projection Box", &m_bShowProjectionBox);
			pBank->AddVector("Grid Box Dims. (X: Width, Y: Height Z: Depth)", &m_vDimensions, -8000.0f, 8000.0f, 0.01f);
			pBank->AddVector("Grid Box Centre Position", &m_vCentrePosition, -8000.0f, 8000.0f, 0.01f);
			pBank->AddSeparator();

			pBank->AddToggle("Use Picker Selection For Height Reference Value", &m_useHeightRefFromPicker);
			pBank->AddToggle("Follow Camera", &m_bFollowCamera);
			pBank->AddSlider("Adjust Pitch", &m_vPitchAdjustment, -180.5f, 180.5f, 0.01f);
			pBank->AddToggle("Apply Position/Dir Tweaks", &m_bApplyTweaksToPosAndDir);
			pBank->AddSeparator();

			pBank->AddColor("Grid Base Colour", &m_gridBaseColor);
			pBank->AddColor("Low Height Colour", &m_lowHeightColor);
			pBank->AddColor("Mid Height Colour", &m_midHeightColor);
			pBank->AddColor("High Height Colour", &m_highHeightColor);

			pBank->AddVector("Grid Misc. (X: Grid Res. Y: Col. Mult. Z: Ref. Height W: Height Delta)", &m_vGridMiscParams, -8000.0f, 8000.0f, 0.01f);
		pBank->PopGroup();

		pBank->PushGroup("Reference Data For Script");
			pBank->AddVector("Grid Box Dims. (X: Width, Y: Height Z: Depth)", &m_vDimensions, -8000.0f, 8000.0f, 0.0f);
			pBank->AddVector("Grid Box Centre Position", &m_vCentrePosition, -8000.0f, 8000.0f, 0.0f);
			pBank->AddVector("Grid Box Forward Vector", &m_vForward, -8000.0f, 8000.0f, 0.0f);
			pBank->AddVector("Grid Misc. (X: Grid Res. Y: Col. Mult. Z: Ref. Height W: Height Delta)", &m_vGridMiscParams, -8000.0f, 8000.0f, 0.0f);
		pBank->PopGroup();

	pBank->PopGroup();
}
#endif