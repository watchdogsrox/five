//
// game/renderer/HorizonObjects.cpp
// 
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

#include "HorizonObjects.h"

#include "grmodel/shader.h"
#include "grcore/allocscope.h"
#include "grcore/light.h"
#include "grcore/debugdraw.h"
#include "fwsys/gameskeleton.h"
#include "fwmaths/vectorutil.h"
#include "file/asset.h"
#include "streaming/streaming.h"
#include "vectormath/classfreefuncsv.h"
#include "renderer/GtaDrawable.h"
#include "renderer/Water.h"
#include "shaders/ShaderLib.h"

#if __BANK
#include "bank/widget.h"
#include "bank/bkmgr.h"
#include "debug/TiledScreenCapture.h"
#endif

RENDER_OPTIMISATIONS()
//OPTIMISATIONS_OFF()
CHorizonObjects g_HorizonObjects;

static const strStreamingObjectName HORIZON_OBJECTS_FILENAME("platform:/models/farlods",0xA1FA0AB3);

CHorizonObjects::CHorizonObjects() : m_bEnable(true), m_bCullEverything(false), m_pSilhoutteShader(NULL), m_pHorizonObjectsDictionary(NULL), m_fScaleFactor(10.0f), m_fDepthRangePercentage(0.032f), m_fFarPlane(25000.0f)
	BANK_ONLY(, m_bDrawBounds(false), m_bSkipAABBTest(false), m_bDebugShader(false), m_iTotalObjects(0), m_iTotalVisibleObjects(0), m_iObjectSelection(-1), m_MemUsage(0), m_bSkipFarDistanceSearch(false),
	m_bSortMapObjects(true), m_bSkipMapObjects(false), m_bSkipWaterObjects(false)
	PS3_ONLY(, m_bDisableEdgeCulling(false)))
{

}

CHorizonObjects::~CHorizonObjects()
{

}

// Sort function for the models - sorts based on height (z component)
int CompareFunc(gtaDrawable* const* a, gtaDrawable* const* b)
{
	Vector3 boxMin;
	Vector3 boxMax;

	float heightA;
	(*a)->GetLodGroup().GetBoundingBox(boxMin, boxMax);
	heightA = boxMax.z;

	float heightB;
	(*b)->GetLodGroup().GetBoundingBox(boxMin, boxMax);
	heightB = boxMax.z;

	return heightA > heightB ? 1 : (heightA < heightB ? -1 : 0);
}

void CHorizonObjects::Init(unsigned initMode)
{
#if !__FINAL
	XPARAM(level);
	const char* levelName = NULL;
	if (PARAM_level.Get(levelName))
	{
		m_bEnable = false;

		if (atStringHash(levelName) == atStringHash("gta5") || atStringHash(levelName) == atStringHash("gta5_liberty"))
		{
			m_bEnable = true;
		}
	}
#endif
	if (initMode == INIT_CORE)
	{
		// Create model and setup shaders
		ASSET.PushFolder("common:/shaders");
		m_pSilhoutteShader = grmShaderFactory::GetInstance().Create();
		m_pSilhoutteShader->Load("silhouettelayer");
		ASSET.PopFolder();
		m_SilhoutteDrawTech = m_pSilhoutteShader->LookupTechnique("draw", true);
		m_SilhoutteDrawWaterReflectionTech = m_pSilhoutteShader->LookupTechnique("waterreflection_draw", true);

		// Prepare the depth stencil state
		grcDepthStencilStateDesc DSDesc;
		DSDesc.StencilEnable = TRUE;
		DSDesc.FrontFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;
		DSDesc.BackFace.StencilPassOp = grcRSV::STENCILOP_REPLACE;
		DSDesc.FrontFace.StencilFunc = grcRSV::CMP_ALWAYS;
		DSDesc.BackFace.StencilFunc = grcRSV::CMP_ALWAYS;
		DSDesc.DepthWriteMask = true;
		DSDesc.DepthFunc = grcRSV::CMP_LESSEQUAL;
		m_DSS_Water = grcStateBlock::CreateDepthStencilState(DSDesc, "WaterHorizonObjectsDepthStencil");
		DSDesc.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
		m_DSS_Scene = grcStateBlock::CreateDepthStencilState(DSDesc, "SceneHorizonObjectsDepthStencil");

		// Load the models
		BANK_ONLY(m_MemUsage = 0;)
		strLocalIndex modelSlot = g_DwdStore.FindSlot(HORIZON_OBJECTS_FILENAME);
		if(modelSlot.Get() >= 0)
		{
			bool modelLoaded = CStreaming::LoadObject(modelSlot, g_DwdStore.GetStreamingModuleId());
			if (modelLoaded)
			{
#if __BANK
				strIndex streamIndex = strStreamingEngine::GetInfo().GetModuleMgr().GetModule(g_DwdStore.GetStreamingModuleId())->GetStreamingIndex(modelSlot);
				strStreamingInfo& info = *strStreamingEngine::GetInfo().GetStreamingInfo(streamIndex);
				m_MemUsage = (info.ComputePhysicalSize(streamIndex, true) + info.ComputeVirtualSize(streamIndex, true))/1024;
#endif // __BANK

				g_DwdStore.AddRef(modelSlot, REF_OTHER);
				m_pHorizonObjectsDictionary = g_DwdStore.Get(modelSlot);
				Assert(m_pHorizonObjectsDictionary);

				m_arrDrawables.Reserve(m_pHorizonObjectsDictionary->GetCount());

				for(int iCurModel = 0; iCurModel < m_pHorizonObjectsDictionary->GetCount(); iCurModel++)
				{
					gtaDrawable* pDrawable = static_cast<gtaDrawable*>(m_pHorizonObjectsDictionary->GetEntry(iCurModel));
					if(pDrawable)
					{
						m_arrDrawables.Append() = pDrawable;
					}
				}

				BANK_ONLY(m_iTotalObjects = m_arrDrawables.GetCount());

				Assertf(m_arrDrawables.GetCount() == m_pHorizonObjectsDictionary->GetCount(), "Horizon Objects ouldn't load all the models");
				m_arrVisible.Reserve(m_arrDrawables.GetCount());

				// Sort the models based on their height to avoid water getting rendered on top of terrain
				m_arrDrawables.QSort(0, -1, CompareFunc);
			}
		}
	}
}

void CHorizonObjects::Shutdown(unsigned shutdownMode)
{
	if (shutdownMode == INIT_CORE)
	{
		if(m_pSilhoutteShader)
		{
			// Delete shader
			delete m_pSilhoutteShader;
			m_pSilhoutteShader = NULL;

			// Delete drawables
			for(int iCurDrawable = 0; iCurDrawable < m_arrDrawables.GetCount(); iCurDrawable++)
			{
				if (m_arrDrawables[iCurDrawable] != NULL) 
				{
					delete m_arrDrawables[iCurDrawable];
				}
			}
			m_arrDrawables.clear();

			// Delete ref and dictionary
			if(m_pHorizonObjectsDictionary)
			{
				g_DwdStore.RemoveRef(strLocalIndex(g_DwdStore.FindSlot(HORIZON_OBJECTS_FILENAME)), REF_OTHER);
				m_pHorizonObjectsDictionary = NULL;
			}
		}
	}
}

void CHorizonObjects::Render()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));	

	g_HorizonObjects.RenderInternal(g_HorizonObjects.m_fDepthRangePercentage, true, false, -1, -1, -1, -1);
}

void CHorizonObjects::RenderEx(float depthRange, bool renderWater, bool waterReflections, int scissorX, int scissorY, int scissorW, int scissorH)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));	

	g_HorizonObjects.RenderInternal(depthRange, renderWater, waterReflections, scissorX, scissorY, scissorW, scissorH);
}

void CHorizonObjects::CullEverything(bool bEnabled)
{
	g_HorizonObjects.m_bCullEverything = bEnabled;
}

void CHorizonObjects::RenderInternal(float depthRange, bool renderWater, bool waterReflections, int scissorX, int scissorY, int scissorW, int scissorH)
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	// First figure which models are visible
	m_arrVisible.clear();

	if(!m_bEnable || m_bCullEverything BANK_ONLY(|| TiledScreenCapture::IsEnabled()))
	{
		return;
	}

#if __BANK && __PS3
	// This is just for testing - we never want to disable EDGE
	bool edgeCullPrevEnabled;
	if(m_bDisableEdgeCulling)
	{
		edgeCullPrevEnabled = grcEffect::SetEdgeViewportCullEnable(false);
	}
#endif // __BANK && __PS3

	// Prepare a scale matrix for the models
	Mat34V worldMtx = Mat34V(V_IDENTITY);
	Vec3V vScale = Vec3V(ScalarV(m_fScaleFactor));
	Scale(worldMtx, vScale, worldMtx);

	// Move the near plane just a little in front of where the far plane is during GBuffer stage
	// This will cull all the triangles that are inside the GBuffer visible range
	grcViewport vp = *grcViewport::GetCurrent();
	float fPrevNear = vp.GetNearClip();
	float fPrevFar = vp.GetFarClip();
	const float fNewNearPlane = Max(fPrevNear, fPrevFar - fPrevFar * depthRange);
	const float fNewFarPlane = m_fFarPlane;
	vp.SetNearFarClip(fNewNearPlane, fNewFarPlane);

	float fClosestObjFarPlane = 0.0;
	Vec3V boxMin;
	Vec3V boxMax;
	for(int iCurDrawable = 0; iCurDrawable < m_arrDrawables.GetCount(); iCurDrawable++)
	{
		// Get the bounding box and scale it
		m_arrDrawables[iCurDrawable]->GetLodGroup().GetBoundingBox(RC_VECTOR3(boxMin), RC_VECTOR3(boxMax));
		boxMin *= vScale;
		boxMax *= vScale;

#if __BANK
		if(m_bDrawBounds)
		{
			grcDebugDraw::BoxAxisAligned(boxMin, boxMax, Color_red, false);
		}

		if(m_bSkipAABBTest)
		{
			m_arrVisible.Append() = iCurDrawable;
		}
		else
#endif // __BANK
		{
			if (grcViewport::IsAABBVisible(boxMin.GetIntrin128ConstRef(), boxMax.GetIntrin128ConstRef(), vp.GetFrustumNFNF()))
			{
				m_arrVisible.Append() = iCurDrawable;

				BANK_ONLY(if(!m_bSkipFarDistanceSearch))
				{
					// TODO -- this is terribly inefficient code ..
					Vector4 nearclip = VEC4V_TO_VECTOR4(vp.GetFrustumClipPlane(grcViewport::CLIP_PLANE_NEAR));
					Vector3 vBoxMin = RCC_VECTOR3(boxMin);
					Vector3 vBoxMax = RCC_VECTOR3(boxMax);
					Vector3 arrCorners[8] = {
						Vector3(vBoxMin.x, vBoxMin.y, vBoxMin.z),
						Vector3(vBoxMax.x, vBoxMin.y, vBoxMin.z),
						Vector3(vBoxMin.x, vBoxMax.y, vBoxMin.z),
						Vector3(vBoxMin.x, vBoxMin.y, vBoxMax.z),
						Vector3(vBoxMax.x, vBoxMax.y, vBoxMin.z),
						Vector3(vBoxMin.x, vBoxMax.y, vBoxMax.z),
						Vector3(vBoxMax.x, vBoxMin.y, vBoxMax.z),
						Vector3(vBoxMax.x, vBoxMax.y, vBoxMax.z)
					};
					float fFurthestDist = 0.0f;
					for(int i = 0; i < 8; i++)
					{
						float dist = nearclip.Dot3(arrCorners[i]) + nearclip.w;
						fFurthestDist = Max(dist, fFurthestDist);
					}
					fFurthestDist += vp.GetNearClip();

					fClosestObjFarPlane = Max(fClosestObjFarPlane, fFurthestDist);
				}
			}
		}
	}

	BANK_ONLY(m_iTotalVisibleObjects = m_arrVisible.GetCount());

	// Don't bother if nothing is visible
	grcViewport *prevVP= grcViewport::GetCurrent();
	grcViewport curVP = *prevVP;
	grcViewport* pCurVP = &curVP;
	grcViewport::SetCurrent(pCurVP);

#if USE_INVERTED_PROJECTION_ONLY
	pCurVP->SetInvertZInProjectionMatrix(!waterReflections);
#endif // USE_INVERTED_PROJECTION_ONLY

	pCurVP->SetNearFarClip(fNewNearPlane, fNewFarPlane); // Set the far plane to a very high distance to avoid culling
	grcWindow winPrev = pCurVP->GetWindow();

	// Set initial state
	grcBindTexture(NULL);
	grcLightState::SetEnabled(false);

	// Set the states
	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetDepthStencilState(waterReflections ? m_DSS_Water : m_DSS_Scene, DEFERRED_MATERIAL_DEFAULT);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_Default);

	// This is the last depth value before the far plane
	// Unfortunately the PS3 is the problematic boy as usual
#if USE_INVERTED_PROJECTION_ONLY
	static float fZMax = waterReflections ? 0.99999994f : 1.0f - 0.99999994f;
#else
	static float fZMax = 0.99999994f;
#endif
	if(BANK_ONLY(!m_bSkipWaterObjects &&) renderWater)
	{
		// Clamp the water depth value to the last value before the far plane
		pCurVP->SetWindow(winPrev.GetX(), winPrev.GetY(), winPrev.GetWidth(), winPrev.GetHeight(), fZMax, fZMax);

		// Render the water
		Water::RenderWaterFLOD();
	}

	if(m_arrVisible.GetCount() > 0)
	{
		BANK_ONLY(if(m_bSortMapObjects))
		{
			m_arrVisible.QSort(0,m_arrVisible.GetCount(),CHorizonObjects::Compare);
		}
		
		// Check if we want to use the closest far plane value
		if(fClosestObjFarPlane >= fNewFarPlane BANK_ONLY( || m_bSkipFarDistanceSearch ))
		{
			// Use the fixed far plane value
			pCurVP->SetNearFarClip(fNewNearPlane, fNewFarPlane);
		}
		else
		{
			// USe the closest far plane value
			pCurVP->SetNearFarClip(fNewNearPlane, fClosestObjFarPlane);
		}

		// Use the window depth range to force the depths into the end of the range
#if USE_INVERTED_PROJECTION_ONLY
		static float fHorizonZMin = waterReflections ? 0.99999976f : fZMax;
		static float fHorizonZMax = waterReflections ? fZMax : 1.0f - 0.99999976f;
#else
		static float fHorizonZMin = 0.99999976f;
		static float fHorizonZMax = fZMax;
#endif
		pCurVP->SetWindow(winPrev.GetX(), winPrev.GetY(), winPrev.GetWidth(), winPrev.GetHeight(), fHorizonZMin, fHorizonZMax);
		
		if (scissorX > -1)
		{
			GRCDEVICE.SetScissor(scissorX, scissorY, scissorW, scissorH);
		}

		BANK_ONLY(if(!m_bSkipMapObjects))
		if (m_pSilhoutteShader->BeginDraw(grmShader::RMC_DRAW, true, waterReflections ? m_SilhoutteDrawWaterReflectionTech : m_SilhoutteDrawTech))
		{
			m_pSilhoutteShader->Bind(BANK_SWITCH(m_bDebugShader ? 1 : 0, 0));

			for(int iCurDrawable = 0; iCurDrawable < m_arrVisible.GetCount(); iCurDrawable++)
			{
#if __BANK
				if(m_iObjectSelection > -1 && m_iObjectSelection != iCurDrawable)
				{
					continue;
				}
#endif // __BANK
				m_arrDrawables[m_arrVisible[iCurDrawable]]->DrawNoShaders(RCC_MATRIX34(worldMtx), CRenderer::GenerateBucketMask(CRenderer::RB_OPAQUE), 0);
			}

			m_pSilhoutteShader->UnBind();
			m_pSilhoutteShader->EndDraw();
		}
	}

	// Restore the states
	grcStateBlock::SetDepthStencilState(CShaderLib::DSS_Default_Invert);

	// Restore viewport
	grcViewport::SetCurrent(prevVP);

#if __BANK && __PS3
	// Restore EDGE as needed
	if(m_bDisableEdgeCulling)
	{
		grcEffect::SetEdgeViewportCullEnable(edgeCullPrevEnabled);
	}
#endif // __BANK && __PS3
}

s32 CHorizonObjects::Compare(const int* a, const int* b)
{
	const gtaDrawable *dblA = g_HorizonObjects.m_arrDrawables[*a];
	const Vec3V ctrA = dblA->GetLodGroup().GetCullSphereV().GetCenter();
	const gtaDrawable *dblB = g_HorizonObjects.m_arrDrawables[*b];
	const Vec3V ctrB = dblB->GetLodGroup().GetCullSphereV().GetCenter();
	
	const Vec3V camPos = grcViewport::GetCurrent()->GetCameraMtx().d();
	
	const ScalarV distA = MagSquared(camPos - ctrA);
	const ScalarV distB = MagSquared(camPos - ctrB);
	
	return (IsGreaterThan(distA,distB).Getb() ? -1 : 1);
}

#if __BANK

void CHorizonObjects::ClampSelection()
{
	m_iObjectSelection = Clamp(m_iObjectSelection, -1, m_arrDrawables.GetCount()-1);
}

void CHorizonObjects::InitWidgets(bkBank* pBank)
{
	pBank->PushGroup("Horizon Objects");
	pBank->AddSlider("Total Horizon Objects", &m_iTotalObjects, 0, 1000, -1);
	pBank->AddSlider("Total Visible", &m_iTotalVisibleObjects, 0, 1000, -1);
	pBank->AddSlider("Total Memory Used (in Kb)", &m_MemUsage, 0, 5000, 1);
	pBank->AddToggle("Enable", &m_bEnable);
	pBank->AddToggle("Cull Everything", &m_bCullEverything);
	pBank->AddToggle("Sort Map Objects", &m_bSortMapObjects);
	pBank->AddToggle("Skip Map Objects", &m_bSkipMapObjects);
	pBank->AddToggle("Skip Water Objects", &m_bSkipWaterObjects);
	pBank->AddToggle("Skip AABB Test", &m_bSkipAABBTest);
	pBank->AddSlider("Object selection", &m_iObjectSelection, -1, 50, 1, datCallback(MFA2(CHorizonObjects::ClampSelection), (datBase*)this));
	pBank->AddToggle("Draw Bounds", &m_bDrawBounds);
	pBank->AddToggle("Use Debug Shader", &m_bDebugShader);
	pBank->AddSlider("Model Scale Factor", &m_fScaleFactor, 0.1f, 15.0f, 0.1f);
	pBank->AddSlider("Depth Range Used", &m_fDepthRangePercentage, 0.001f, 1.0f, 0.001f);
	pBank->AddSlider("Far Plane Distance", &m_fFarPlane, 5000.0, 30000.0f, 1.0f);
	pBank->AddToggle("Skip Far Distance Search", &m_bSkipFarDistanceSearch);
#if __PS3
	pBank->AddToggle("Disable EDGE Culling", &m_bDisableEdgeCulling);
#endif // __PS3
	pBank->PopGroup();
}
#endif // __BANK
