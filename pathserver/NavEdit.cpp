#include "navedit.h"

#if __NAVEDITORS

#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "bank/combo.h"
#include "bank/slider.h"

#include "ai/navmesh/navmeshoptimisations.h"

#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/helpers/Frame.h"
#include "camera/viewports/Viewport.h"
#include "Debug/DebugScene.h"
#include "pathserver/PathServer.h"
#include "Peds/Ped.h"
#include "Peds/PlayerInfo.h"
#include "task/Combat/Cover/cover.h"

NAVMESH_OPTIMISATIONS()

namespace rage
{

bool CNavResAreaEditor::m_bEditorInitialised = false;
bool CNavResAreaEditor::m_bActive = false;
CNavResAreaEditor::EditorState CNavResAreaEditor::m_iState = CNavResAreaEditor::STATE_NONE;
int CNavResAreaEditor::m_iCurrentArea = -1;
int CNavResAreaEditor::m_iCurrentResMultCombo = 1;
bool CNavResAreaEditor::m_bCurrentAreaFlag_DoNotOptimise = false;
bool CNavResAreaEditor::m_bCurrentAreaFlag_NoCover = false;
bool CNavResAreaEditor::m_bCurrentAreaFlag_NoDrops = false;
bool CNavResAreaEditor::m_bCurrentAreaFlag_NoClimbs = false;
bool CNavResAreaEditor::m_bCurrentAreaFlag_NoCullSmallAreas = false;
CNavResArea CNavResAreaEditor::m_CurrentArea;
atArray<CNavResArea*> CNavResAreaEditor::m_ResAreas;
float CNavResAreaEditor::m_fSizeStep = 1.0f;
bool CNavResAreaEditor::m_bSizeFromCenter = false;
int CNavResAreaEditor::m_iWarpNavMeshX = 0;
int CNavResAreaEditor::m_iWarpNavMeshY = 0;

bkSlider * CNavResAreaEditor::m_pCurrentAreaSlider = NULL;

const Vector3 CNavResAreaEditor::ms_vDefaultAreaSize(4.0f, 4.0f, 2.0f);
const float CNavResAreaEditor::ms_fDefaultResolution = 1.0f;
const int CNavResAreaEditor::ms_iXmlVersion = 1;

// CNavDataCoverpointEditor statics:
// -- public --
const int CNavDataCoverpointEditor::ms_iXmlVersion = 1;
// -- private --
bool CNavDataCoverpointEditor::ms_bEditorInitialized = false;
bool CNavDataCoverpointEditor::ms_bActive = false;
bool CNavDataCoverpointEditor::ms_bDisplayExistingMapPointProximity = false;
int CNavDataCoverpointEditor::ms_iSelectedDataCoverpointIndex = -1;
atArray<CNavDataCoverpoint*> CNavDataCoverpointEditor::ms_aDataCoverpoints;
u8 CNavDataCoverpointEditor::ms_uCurrentCoverpointDirection = 0;
u8 CNavDataCoverpointEditor::ms_uCurrentCoverpointType = 0;
bkSlider* CNavDataCoverpointEditor::ms_pCurrentCoverpointIndexSlider = NULL;
bkSlider* CNavDataCoverpointEditor::ms_pCurrentCoverpointDirectionSlider = NULL;
int CNavDataCoverpointEditor::ms_iStatusCheckDataCoverpointIndex = -1;
s32 CNavDataCoverpointEditor::ms_iTempCoverPointIndex = -1;
CCoverPointStatusHelper CNavDataCoverpointEditor::ms_CoverPointStatusHelper;
// --

CNavResAreaEditor::CNavResAreaEditor()
{

}

CNavResAreaEditor::~CNavResAreaEditor()
{

}

void CNavResAreaEditor::Init()
{
	bkBank * pBank = BANKMGR.FindBank("Navigation");
	if(!pBank)
	{
		Assertf(false, "Couldn't locate \"Navigation\" bank.");
		return;
	}

	pBank->AddButton("Initialise Nav Res Editor", OnInitEditor);
}

void CNavResAreaEditor::InitWidgets()
{
	bkBank * pBank = BANKMGR.FindBank("Navigation");
	if(!pBank)
	{
		Assertf(false, "Couldn't locate \"Navigation\" bank.");
		return;
	}

	static const char * pResMultipliers[] = { "1x", "2x", "4x", "8x" };

	pBank->PushGroup("ResArea Editor");

		pBank->AddToggle("Editor Active", &m_bActive);
		pBank->AddSeparator();

		pBank->AddButton("New Area", OnNewArea);
		pBank->AddButton("New Area (doorway default)", OnNewAreaDoorway);
		pBank->AddButton("Delete Area", OnDeleteArea);
		pBank->AddButton("Duplicate Area", OnDuplicateArea);
		
		m_pCurrentAreaSlider = pBank->AddSlider("Current Area", &m_iCurrentArea, -1, m_ResAreas.GetCount()-1, 1, OnSelectArea);
		pBank->AddButton("Warp Here", OnWarpToArea);
		pBank->AddSeparator("Area Details..");
		pBank->AddCombo("Resolution Multiplier:", &m_iCurrentResMultCombo, 4, pResMultipliers, OnSelectResolutionMultiplier, "Selects resolution multiplier for current selected area");
		pBank->PushGroup("Flags", false);
			pBank->AddToggle("DO NOT OPTIMISE", &m_bCurrentAreaFlag_DoNotOptimise, OnToggleFlag_DoNotOptimise);
			pBank->AddToggle("NO COVER", &m_bCurrentAreaFlag_NoCover, OnToggleFlag_NoCover);
			pBank->AddToggle("NO DROPS", &m_bCurrentAreaFlag_NoDrops, OnToggleFlag_NoDrops);
			pBank->AddToggle("NO CLIMBS", &m_bCurrentAreaFlag_NoClimbs, OnToggleFlag_NoClimbs);
			pBank->AddToggle("NO CULL SMALL AREAS", &m_bCurrentAreaFlag_NoCullSmallAreas, OnToggleFlag_NoCullSmallAreas);
		pBank->PopGroup();
		pBank->AddSeparator("");

		pBank->PushGroup("Position");
			pBank->AddButton("Nudge +X", OnNudgeAreaPosX);
			pBank->AddButton("Nudge -X", OnNudgeAreaNegX);
			pBank->AddButton("Nudge +Y", OnNudgeAreaPosY);
			pBank->AddButton("Nudge -Y", OnNudgeAreaNegY);
			pBank->AddButton("Nudge +Z", OnNudgeAreaPosZ);
			pBank->AddButton("Nudge -Z", OnNudgeAreaNegZ);
		pBank->PopGroup();

		pBank->PushGroup("Size");
			//pBank->AddSlider("Size Step", &m_fSizeStep, 1.0f, 10.0f, 1.0f);
			pBank->AddToggle("Size from center", &m_bSizeFromCenter);
			pBank->AddButton("Increase Size X (Width)", OnIncreaseWidth);
			pBank->AddButton("Increase Size Y (Depth)", OnIncreaseDepth);
			pBank->AddButton("Increase Size Z (Height)", OnIncreaseHeight);
			pBank->AddButton("Decrease Size X (Width)", OnDecreaseWidth);
			pBank->AddButton("Decrease Size Y (Depth)", OnDecreaseDepth);
			pBank->AddButton("Decrease Size Z (Height)", OnDecreaseHeight);
		pBank->PopGroup();

		pBank->PushGroup("Save/Load");
			pBank->AddButton("Save..", OnSave);
			pBank->AddButton("Load..", OnLoad);
		pBank->PopGroup();

		pBank->PushGroup("Warp to navmesh coords");
		pBank->AddSlider("X: ", &m_iWarpNavMeshX, 0, CPathServerExtents::GetWorldWidthInSectors(), CPathServerExtents::m_iNumSectorsPerNavMesh);
		pBank->AddSlider("Y: ", &m_iWarpNavMeshY, 0, CPathServerExtents::GetWorldDepthInSectors(), CPathServerExtents::m_iNumSectorsPerNavMesh);
		pBank->AddButton("Warp", OnWarpToNavmeshCoords);

	pBank->PopGroup();
}

void CNavResAreaEditor::Update()
{
	if(!m_bEditorInitialised || !m_bActive)
		return;

	ProcessInput();
}

void CNavResAreaEditor::Render()
{
	if(!m_bEditorInitialised || !m_bActive)
		return;

	camDebugDirector & debugDirector = camInterface::GetDebugDirector();
	Vector3 vOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;

	Matrix34 matAxis;
	matAxis.Identity();

	char text[16];
	
	const float fMaxRange = 200.0f;
	const float fGridRange = 50.0f;

	for(int r=0; r<m_ResAreas.GetCount(); r++)
	{
		CNavResArea * pArea = m_ResAreas[r];

		const Vector3 vMid = (pArea->m_vMin + pArea->m_vMax) * 0.5f;
		const float fDistSqr = (vMid - vOrigin).Mag2();
		if(fDistSqr < fMaxRange*fMaxRange)
		{
			const float fDist = sqrtf(fDistSqr);
			const float fBoxAlpha = Clamp(1.0f - (fDist / fMaxRange), 0.0f, 1.0f);
			const float fGridAlpha = Clamp(1.0f - (fDist / fGridRange), 0.0f, 1.0f);

			Color32 iBoxCol = (r == m_iCurrentArea) ? Color_green : Color_DimGray;
			Color32 iGridCol = (r == m_iCurrentArea) ? Color_cyan3 : Color_grey49;
			Color32 iTextCol = Color_red;

			iBoxCol.SetAlpha((int)(fBoxAlpha*255.0f));
			iGridCol.SetAlpha((int)(fGridAlpha*255.0f));
			iTextCol.SetAlpha((int)(fGridAlpha*255.0f));

			grcDebugDraw::BoxAxisAligned(RCC_VEC3V(pArea->m_vMin), RCC_VEC3V(pArea->m_vMax), iBoxCol, false);

			if(fGridAlpha > 0.0f)
			{
				DrawSamplingGrid(pArea, iGridCol);

				matAxis.d = vMid;
				grcDebugDraw::Axis(matAxis, 1.0f, true);
				sprintf(text, "(%i) x%i", r, 1<<(pArea->m_iResMult+1) );
				grcDebugDraw::Text(vMid, iTextCol, 0, 0, text);

				int iY = grcDebugDraw::GetScreenSpaceTextHeight();
				if((pArea->m_iFlags & CNavResArea::FLAG_DO_NOT_OPTIMISE)!=0)
					grcDebugDraw::Text(vMid, iTextCol, 0, iY+=grcDebugDraw::GetScreenSpaceTextHeight(), "DO NOT OPTIMISE");
				if((pArea->m_iFlags & CNavResArea::FLAG_NO_COVER)!=0)
					grcDebugDraw::Text(vMid, iTextCol, 0, iY+=grcDebugDraw::GetScreenSpaceTextHeight(), "NO COVER");				
				if((pArea->m_iFlags & CNavResArea::FLAG_NO_DROPS)!=0)
					grcDebugDraw::Text(vMid, iTextCol, 0, iY+=grcDebugDraw::GetScreenSpaceTextHeight(), "NO DROPS");				
				if((pArea->m_iFlags & CNavResArea::FLAG_NO_CLIMBS)!=0)
					grcDebugDraw::Text(vMid, iTextCol, 0, iY+=grcDebugDraw::GetScreenSpaceTextHeight(), "NO CLIMBS");				
				if((pArea->m_iFlags & CNavResArea::FLAG_NO_CULL_SMALL_AREAS)!=0)
					grcDebugDraw::Text(vMid, iTextCol, 0, iY+=grcDebugDraw::GetScreenSpaceTextHeight(), "NO CULL SMALL AREAS");				
			}
		}
	}
}

void CNavResAreaEditor::DrawSamplingGrid(CNavResArea * pArea, const Color32 iCol)
{
	if(pArea->m_iResMult==-1)
		return;

	const float fStep = pArea->GetStep();

	for(float y=pArea->m_vMin.y; y<=pArea->m_vMax.y; y+=fStep)
	{
		for(float x=pArea->m_vMin.x; x<=pArea->m_vMax.x; x+=fStep)
		{
			Vector3 vStart(x, y, pArea->m_vMin.z);
			Vector3 vEnd(x, y, pArea->m_vMax.z);
			grcDebugDraw::Line(vStart, vEnd, iCol);
		}
	}
}

void CNavResAreaEditor::LoadMetadata()
{
	// switch to debug allocator

	// switch back to game allocator
}

void CNavResAreaEditor::ProcessInput()
{
	camDebugDirector & debugDirector = camInterface::GetDebugDirector();
	Vector3 vOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;

	bool bLeftButtonDown = (ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT)? true : false;
	bool bRightButtonDown = (ioMouse::GetPressedButtons() & ioMouse::MOUSE_RIGHT)? true : false;

	switch(m_iState)
	{
		case STATE_NONE:
		{
			if(bLeftButtonDown)
			{
				Vector3 vPickPos;
				if(CDebugScene::GetWorldPositionUnderMouse(vPickPos))
				{
					int iArea = FindResAreaClosestToPos(vPickPos, 200.0f);
					if(iArea != -1)
					{
						m_iCurrentArea = iArea;
						OnSelectArea();
					}
				}
			}
			else if(bRightButtonDown)
			{
				if(m_iCurrentArea >= 0 && m_iCurrentArea < m_ResAreas.GetCount())
				{
					Vector3 vPickPos;
					if(CDebugScene::GetWorldPositionUnderMouse(vPickPos))
					{
						RoundToNearest(vPickPos);
						// TODO: we should really also ensure vPickPos components are multiples of ms_fDefaultResolution

						Vector3 vMid = (m_CurrentArea.m_vMin + m_CurrentArea.m_vMax) * 0.5f;
						Vector3 vDelta = vPickPos - vMid;
						m_CurrentArea.m_vMin += vDelta;
						m_CurrentArea.m_vMax += vDelta;

						RoundToNearest(m_CurrentArea.m_vMin);
						RoundToNearest(m_CurrentArea.m_vMax);

						if(m_CurrentArea.m_vMax.x == m_CurrentArea.m_vMin.x) m_CurrentArea.m_vMax.x += 1.0f;
						if(m_CurrentArea.m_vMax.y == m_CurrentArea.m_vMin.y) m_CurrentArea.m_vMax.y += 1.0f;
						if(m_CurrentArea.m_vMax.z == m_CurrentArea.m_vMin.z) m_CurrentArea.m_vMax.z += 1.0f;

						*m_ResAreas[m_iCurrentArea] = m_CurrentArea;
						m_iCurrentResMultCombo = m_CurrentArea.m_iResMult+1;
					}
				}
			}
			break;
		}
		default:
			break;
	}
}

int CNavResAreaEditor::FindResAreaClosestToPos(const Vector3 & vPos, const float fMaxDist)
{
	float fClosestDistSqr = FLT_MAX;
	int iClosest = -1;
	for(int r=0; r<m_ResAreas.GetCount(); r++)
	{
		CNavResArea * pArea = m_ResAreas[r];
		Vector3 vCenter = (pArea->m_vMin + pArea->m_vMax) * 0.5f;
		const float fDistSqr = (vPos - vCenter).Mag2();
		if(fDistSqr < fMaxDist && fDistSqr < fClosestDistSqr)
		{
			fClosestDistSqr = fDistSqr;
			iClosest = r;
		}
	}
	return iClosest;
}


//---------------------------------------------------------------------------------

void CNavResAreaEditor::OnInitEditor()
{
	if(!m_bEditorInitialised || !m_bActive)
	{
		InitWidgets();
		m_bEditorInitialised = true;
		m_bActive = true;
	}
}

void CNavResAreaEditor::OnSelectArea()
{
	if(m_iCurrentArea >= 0 && m_iCurrentArea < m_ResAreas.GetCount())
	{
		m_CurrentArea = *m_ResAreas[m_iCurrentArea];
		m_iCurrentResMultCombo = m_CurrentArea.m_iResMult+1;

		m_bCurrentAreaFlag_DoNotOptimise = ((m_CurrentArea.m_iFlags & (CNavResArea::FLAG_DO_NOT_OPTIMISE)) !=0);
		m_bCurrentAreaFlag_NoCover = ((m_CurrentArea.m_iFlags & (CNavResArea::FLAG_NO_COVER)) !=0);
		m_bCurrentAreaFlag_NoDrops = ((m_CurrentArea.m_iFlags & (CNavResArea::FLAG_NO_DROPS)) !=0);
		m_bCurrentAreaFlag_NoClimbs = ((m_CurrentArea.m_iFlags & (CNavResArea::FLAG_NO_CLIMBS)) !=0);
		m_bCurrentAreaFlag_NoCullSmallAreas = ((m_CurrentArea.m_iFlags & (CNavResArea::FLAG_NO_CULL_SMALL_AREAS)) !=0);
	}
}
void CNavResAreaEditor::OnNewArea()
{
	camDebugDirector & debugDirector = camInterface::GetDebugDirector();
	Vector3 vOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;

	AddNewArea(vOrigin, ms_vDefaultAreaSize.x, ms_vDefaultAreaSize.y, ms_vDefaultAreaSize.z);
}
void CNavResAreaEditor::OnNewAreaDoorway()
{
	camDebugDirector & debugDirector = camInterface::GetDebugDirector();
	Vector3 vOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;

	AddNewArea(vOrigin, 1.0f, 1.0f, 1.0f);
}
void CNavResAreaEditor::AddNewArea(const Vector3 & vOriginIn, const float fWidth, const float fDepth, const float fHeight)
{
	USE_DEBUG_MEMORY();	// Ensure that we alloc from any free memory devkit may have

	CNavResArea * pNewArea = rage_new CNavResArea();

	Vector3 vOrigin = vOriginIn;
	RoundToNearest(vOrigin);

	pNewArea->m_vMin = vOrigin - (Vector3(fWidth, fDepth, fHeight) * 0.5f);
	pNewArea->m_vMax = vOrigin + (Vector3(fWidth, fDepth, fHeight) * 0.5f);
	pNewArea->m_iResMult = 0;

	m_ResAreas.PushAndGrow(pNewArea);
	m_iCurrentArea = m_ResAreas.GetCount()-1;

	m_pCurrentAreaSlider->SetRange((float)-1, (float)m_ResAreas.GetCount()-1);

	m_CurrentArea = *pNewArea;
	m_iCurrentResMultCombo = m_CurrentArea.m_iResMult+1;

	m_bCurrentAreaFlag_DoNotOptimise = false;
	m_bCurrentAreaFlag_NoCover = false;
	m_bCurrentAreaFlag_NoDrops = false;
	m_bCurrentAreaFlag_NoClimbs = false;
	m_bCurrentAreaFlag_NoCullSmallAreas = false;
}

void CNavResAreaEditor::OnDeleteArea()
{
	if(m_iCurrentArea >= 0 && m_iCurrentArea < m_ResAreas.GetCount())
	{
		CNavResArea * pArea = m_ResAreas[m_iCurrentArea];
		delete pArea;
		m_ResAreas.Delete(m_iCurrentArea);

		m_pCurrentAreaSlider->SetRange((float)-1, (float)m_ResAreas.GetCount()-1);

		if(m_iCurrentArea >= m_ResAreas.GetCount())
			m_iCurrentArea--;

		if(m_iCurrentArea >= 0)
		{
			m_CurrentArea = *m_ResAreas[m_iCurrentArea];
			m_iCurrentResMultCombo = m_CurrentArea.m_iResMult+1;

			m_bCurrentAreaFlag_DoNotOptimise = (m_CurrentArea.m_iFlags & (CNavResArea::FLAG_DO_NOT_OPTIMISE));
			m_bCurrentAreaFlag_NoCover = ((m_CurrentArea.m_iFlags & (CNavResArea::FLAG_NO_COVER)) !=0);
			m_bCurrentAreaFlag_NoDrops = ((m_CurrentArea.m_iFlags & (CNavResArea::FLAG_NO_DROPS)) !=0);
			m_bCurrentAreaFlag_NoClimbs = ((m_CurrentArea.m_iFlags & (CNavResArea::FLAG_NO_CLIMBS)) !=0);
			m_bCurrentAreaFlag_NoCullSmallAreas = ((m_CurrentArea.m_iFlags & (CNavResArea::FLAG_NO_CULL_SMALL_AREAS)) !=0);
		}
		else
		{
			m_CurrentArea.Clear();
		}
	}
}
void CNavResAreaEditor::OnDuplicateArea()
{
	if(m_iCurrentArea >= 0 && m_iCurrentArea < m_ResAreas.GetCount())
	{
		const int iOriginalArea = m_iCurrentArea;
		const Vector3 vSize = m_CurrentArea.m_vMax - m_CurrentArea.m_vMin;

		camDebugDirector & debugDirector = camInterface::GetDebugDirector();
		Vector3 vOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;

		AddNewArea(vOrigin, vSize.x, vSize.y, vSize.z);
		m_ResAreas[m_iCurrentArea]->m_iResMult = m_ResAreas[iOriginalArea]->m_iResMult;
		m_ResAreas[m_iCurrentArea]->m_iFlags = m_ResAreas[iOriginalArea]->m_iFlags;
		m_CurrentArea = *m_ResAreas[m_iCurrentArea];
		m_iCurrentResMultCombo = m_CurrentArea.m_iResMult+1;

		m_bCurrentAreaFlag_DoNotOptimise = (m_CurrentArea.m_iFlags & (CNavResArea::FLAG_DO_NOT_OPTIMISE));
		m_bCurrentAreaFlag_NoCover = ((m_CurrentArea.m_iFlags & (CNavResArea::FLAG_NO_COVER)) !=0);
		m_bCurrentAreaFlag_NoDrops = ((m_CurrentArea.m_iFlags & (CNavResArea::FLAG_NO_DROPS)) !=0);
		m_bCurrentAreaFlag_NoClimbs = ((m_CurrentArea.m_iFlags & (CNavResArea::FLAG_NO_CLIMBS)) !=0);
		m_bCurrentAreaFlag_NoCullSmallAreas = ((m_CurrentArea.m_iFlags & (CNavResArea::FLAG_NO_CULL_SMALL_AREAS)) !=0);
	}
}
void CNavResAreaEditor::OnWarpToArea()
{
	if(m_iCurrentArea >= 0 && m_iCurrentArea < m_ResAreas.GetCount())
	{
		Vector3 vMid = (m_CurrentArea.m_vMin + m_CurrentArea.m_vMax) * 0.5f;
		CPed * pPlayer = FindPlayerPed();
		if(pPlayer)
		{
			pPlayer->Teleport(vMid, pPlayer->GetCurrentHeading(), false);
		}
	}
}
void CNavResAreaEditor::OnWarpToNavmeshCoords()
{
	TNavMeshIndex iNavMesh = CPathServerGta::GetNavMeshIndexFromSector(m_iWarpNavMeshX, m_iWarpNavMeshY, kNavDomainRegular);
	if(iNavMesh != NAVMESH_NAVMESH_INDEX_NONE)
	{
		Vector2 vMin,vMax;
		CPathServerGta::GetNavMeshExtentsFromIndex(iNavMesh, vMin, vMax, kNavDomainRegular);

		CPed * pPlayer = FindPlayerPed();
		if(pPlayer)
		{
			Vector3 vPos((vMin.x + vMax.x)*0.5f, (vMin.y + vMax.y)*0.5f, pPlayer->GetTransform().GetPosition().GetZf());
			pPlayer->Teleport(vPos, pPlayer->GetCurrentHeading(), false);
		}
	}
}
void CNavResAreaEditor::OnSelectResolutionMultiplier()
{
	if(m_iCurrentArea >= 0 && m_iCurrentArea < m_ResAreas.GetCount())
	{
		m_CurrentArea.m_iResMult = m_iCurrentResMultCombo-1;
		*m_ResAreas[m_iCurrentArea] = m_CurrentArea;
	}
}

void CNavResAreaEditor::OnToggleFlag_DoNotOptimise()
{
	if(m_bCurrentAreaFlag_DoNotOptimise)
		m_CurrentArea.m_iFlags |= CNavResArea::FLAG_DO_NOT_OPTIMISE;
	else
		m_CurrentArea.m_iFlags &= ~CNavResArea::FLAG_DO_NOT_OPTIMISE;

	*m_ResAreas[m_iCurrentArea] = m_CurrentArea;
}
void CNavResAreaEditor::OnToggleFlag_NoCover()
{
	if(m_bCurrentAreaFlag_NoCover)
		m_CurrentArea.m_iFlags |= CNavResArea::FLAG_NO_COVER;
	else
		m_CurrentArea.m_iFlags &= ~CNavResArea::FLAG_NO_COVER;

	*m_ResAreas[m_iCurrentArea] = m_CurrentArea;
}
void CNavResAreaEditor::OnToggleFlag_NoDrops()
{
	if(m_bCurrentAreaFlag_NoDrops)
		m_CurrentArea.m_iFlags |= CNavResArea::FLAG_NO_DROPS;
	else
		m_CurrentArea.m_iFlags &= ~CNavResArea::FLAG_NO_DROPS;

	*m_ResAreas[m_iCurrentArea] = m_CurrentArea;
}
void CNavResAreaEditor::OnToggleFlag_NoClimbs()
{
	if(m_bCurrentAreaFlag_NoClimbs)
		m_CurrentArea.m_iFlags |= CNavResArea::FLAG_NO_CLIMBS;
	else
		m_CurrentArea.m_iFlags &= ~CNavResArea::FLAG_NO_CLIMBS;

	*m_ResAreas[m_iCurrentArea] = m_CurrentArea;
}
void CNavResAreaEditor::OnToggleFlag_NoCullSmallAreas()
{
	if(m_bCurrentAreaFlag_NoCullSmallAreas)
		m_CurrentArea.m_iFlags |= CNavResArea::FLAG_NO_CULL_SMALL_AREAS;
	else
		m_CurrentArea.m_iFlags &= ~CNavResArea::FLAG_NO_CULL_SMALL_AREAS;

	*m_ResAreas[m_iCurrentArea] = m_CurrentArea;
}

void CNavResAreaEditor::OnNudgeAreaPosX()
{
	if(m_iCurrentArea >= 0 && m_iCurrentArea < m_ResAreas.GetCount())
	{
		m_CurrentArea.m_vMin.x += ms_fDefaultResolution;
		m_CurrentArea.m_vMax.x += ms_fDefaultResolution;
		*m_ResAreas[m_iCurrentArea] = m_CurrentArea;
	}
}
void CNavResAreaEditor::OnNudgeAreaNegX()
{
	if(m_iCurrentArea >= 0 && m_iCurrentArea < m_ResAreas.GetCount())
	{
		m_CurrentArea.m_vMin.x -= ms_fDefaultResolution;
		m_CurrentArea.m_vMax.x -= ms_fDefaultResolution;
		*m_ResAreas[m_iCurrentArea] = m_CurrentArea;
	}
}
void CNavResAreaEditor::OnNudgeAreaPosY()
{
	if(m_iCurrentArea >= 0 && m_iCurrentArea < m_ResAreas.GetCount())
	{
		m_CurrentArea.m_vMin.y += ms_fDefaultResolution;
		m_CurrentArea.m_vMax.y += ms_fDefaultResolution;
		*m_ResAreas[m_iCurrentArea] = m_CurrentArea;
	}
}
void CNavResAreaEditor::OnNudgeAreaNegY()
{
	if(m_iCurrentArea >= 0 && m_iCurrentArea < m_ResAreas.GetCount())
	{
		m_CurrentArea.m_vMin.y -= ms_fDefaultResolution;
		m_CurrentArea.m_vMax.y -= ms_fDefaultResolution;
		*m_ResAreas[m_iCurrentArea] = m_CurrentArea;
	}
}
void CNavResAreaEditor::OnNudgeAreaPosZ()
{
	if(m_iCurrentArea >= 0 && m_iCurrentArea < m_ResAreas.GetCount())
	{
		m_CurrentArea.m_vMin.z += ms_fDefaultResolution;
		m_CurrentArea.m_vMax.z += ms_fDefaultResolution;
		*m_ResAreas[m_iCurrentArea] = m_CurrentArea;
	}
}
void CNavResAreaEditor::OnNudgeAreaNegZ()
{
	if(m_iCurrentArea >= 0 && m_iCurrentArea < m_ResAreas.GetCount())
	{
		m_CurrentArea.m_vMin.z -= ms_fDefaultResolution;
		m_CurrentArea.m_vMax.z -= ms_fDefaultResolution;
		*m_ResAreas[m_iCurrentArea] = m_CurrentArea;
	}
}
void CNavResAreaEditor::OnIncreaseWidth()
{
	if(m_iCurrentArea >= 0 && m_iCurrentArea < m_ResAreas.GetCount())
	{
		m_CurrentArea.m_vMax.x += m_fSizeStep;
		if(m_bSizeFromCenter)
			m_CurrentArea.m_vMin.x -= m_fSizeStep;
		*m_ResAreas[m_iCurrentArea] = m_CurrentArea;
	}
}
void CNavResAreaEditor::OnDecreaseWidth()
{
	if(m_iCurrentArea >= 0 && m_iCurrentArea < m_ResAreas.GetCount())
	{
		m_CurrentArea.m_vMax.x -= m_fSizeStep;
		if(m_bSizeFromCenter)
			m_CurrentArea.m_vMin.x += m_fSizeStep;
		if(m_CurrentArea.m_vMax.x - m_CurrentArea.m_vMin.x < ms_fDefaultResolution)
			m_CurrentArea.m_vMax.x = m_CurrentArea.m_vMin.x + ms_fDefaultResolution;
		*m_ResAreas[m_iCurrentArea] = m_CurrentArea;
	}
}
void CNavResAreaEditor::OnIncreaseDepth()
{
	if(m_iCurrentArea >= 0 && m_iCurrentArea < m_ResAreas.GetCount())
	{
		m_CurrentArea.m_vMax.y += m_fSizeStep;
		if(m_bSizeFromCenter)
			m_CurrentArea.m_vMin.y -= m_fSizeStep;
		*m_ResAreas[m_iCurrentArea] = m_CurrentArea;
	}
}
void CNavResAreaEditor::OnDecreaseDepth()
{
	if(m_iCurrentArea >= 0 && m_iCurrentArea < m_ResAreas.GetCount())
	{
		m_CurrentArea.m_vMax.y -= m_fSizeStep;
		if(m_bSizeFromCenter)
			m_CurrentArea.m_vMin.y += m_fSizeStep;
		if(m_CurrentArea.m_vMax.y - m_CurrentArea.m_vMin.y < ms_fDefaultResolution)
			m_CurrentArea.m_vMax.y = m_CurrentArea.m_vMin.y + ms_fDefaultResolution;
		*m_ResAreas[m_iCurrentArea] = m_CurrentArea;
	}
}
void CNavResAreaEditor::OnIncreaseHeight()
{
	if(m_iCurrentArea >= 0 && m_iCurrentArea < m_ResAreas.GetCount())
	{
		m_CurrentArea.m_vMax.z += m_fSizeStep;
		if(m_bSizeFromCenter)
			m_CurrentArea.m_vMin.z -= m_fSizeStep;
		*m_ResAreas[m_iCurrentArea] = m_CurrentArea;
	}
}
void CNavResAreaEditor::OnDecreaseHeight()
{
	if(m_iCurrentArea >= 0 && m_iCurrentArea < m_ResAreas.GetCount())
	{
		m_CurrentArea.m_vMax.z -= m_fSizeStep;
		if(m_bSizeFromCenter)
			m_CurrentArea.m_vMin.z += m_fSizeStep;
		if(m_CurrentArea.m_vMax.z - m_CurrentArea.m_vMin.z < ms_fDefaultResolution)
			m_CurrentArea.m_vMax.z = m_CurrentArea.m_vMin.z + ms_fDefaultResolution;
		*m_ResAreas[m_iCurrentArea] = m_CurrentArea;
	}
}

void CNavResAreaEditor::RemoveAllAreas()
{
	for(int r=0; r<m_ResAreas.GetCount(); r++)
	{
		CNavResArea * pArea = m_ResAreas[r];
		delete pArea;
	}
	m_ResAreas.clear();

	m_iCurrentResMultCombo = 0;
	m_iCurrentArea = -1;
	m_CurrentArea.Clear();
	m_bCurrentAreaFlag_DoNotOptimise = false;
	m_bCurrentAreaFlag_NoCover = false;
	m_bCurrentAreaFlag_NoDrops = false;
	m_bCurrentAreaFlag_NoClimbs = false;
	m_bCurrentAreaFlag_NoCullSmallAreas = false;
}

void CNavResAreaEditor::OnSave()
{
	USE_DEBUG_MEMORY();

	char filename[512] = { 0 };
	bool bOk = BANKMGR.OpenFile(filename, 512, "*.xml", true, "Navmesh Resolution Areas XML");
	if(!bOk)
		return;

	parTree *pTree = rage_new parTree();
	Assert(pTree);

	parTreeNode* pRootNode = pTree->CreateRoot();
	Assert(pRootNode);

	parElement& rootElm = pRootNode->GetElement();
	rootElm.SetName("NavResAreas");
	rootElm.AddAttribute("Version", ms_iXmlVersion, false);

	pTree->SetRoot(pRootNode);

	parTreeNode * pAreasListXmlNode = rage_new parTreeNode();
	Assert(pAreasListXmlNode);

	pAreasListXmlNode->GetElement().SetName("Areas");
	pAreasListXmlNode->AppendAsChildOf(pRootNode);

	for(int r=0; r<m_ResAreas.GetCount(); r++)
	{
		CNavResArea * pResArea = m_ResAreas[r];

		parTreeNode * pAreaXmlNode = rage_new parTreeNode();
		Assert(pAreaXmlNode);

		pAreaXmlNode->GetElement().SetName("Area");

		pAreaXmlNode->GetElement().AddAttribute("MinX", pResArea->m_vMin.x, false);
		pAreaXmlNode->GetElement().AddAttribute("MinY", pResArea->m_vMin.y, false);
		pAreaXmlNode->GetElement().AddAttribute("MinZ", pResArea->m_vMin.z, false);
		pAreaXmlNode->GetElement().AddAttribute("MaxX", pResArea->m_vMax.x, false);
		pAreaXmlNode->GetElement().AddAttribute("MaxY", pResArea->m_vMax.y, false);
		pAreaXmlNode->GetElement().AddAttribute("MaxZ", pResArea->m_vMax.z, false);
		pAreaXmlNode->GetElement().AddAttribute("ResMult", pResArea->m_iResMult, false);
		pAreaXmlNode->GetElement().AddAttribute("Flags", pResArea->m_iFlags, false);

		pAreaXmlNode->AppendAsChildOf(pAreasListXmlNode);
	}

	const bool bSaveRes = PARSER.SaveTree(filename, "", pTree, parManager::XML);
	if(!bSaveRes)
	{
		Assertf(false, "Failed to save Navmesh Resolution Areas XML '%s'.", filename);
		delete pTree;
		return;
	}

	delete pTree;
}

void CNavResAreaEditor::OnLoad()
{
	USE_DEBUG_MEMORY();

	char filename[512] = { 0 };
	bool bOk = BANKMGR.OpenFile(filename, 512, "*.xml", false, "Navmesh Resolution Areas XML");
	if(!bOk)
		return;

	RemoveAllAreas();

	parTree * pTree = PARSER.LoadTree(filename, "");
	Assert(pTree);

	const parTreeNode* pRootNode = pTree->GetRoot();
	Assert(pRootNode);
	Assert(stricmp(pRootNode->GetElement().GetName(), "NavResAreas") == 0);

	// Make sure we have the correct version.
	ASSERT_ONLY(int version = pRootNode->GetElement().FindAttributeIntValue("Version", 0));
	Assert(version >= ms_iXmlVersion);

	//ASSERT_ONLY(int iNumAreas = pRootNode->GetElement().FindAttributeIntValue("NumAreas", 0));

	parTreeNode::ChildNodeIterator i = pRootNode->BeginChildren();
	for(; i != pRootNode->EndChildren(); ++i)
	{
		if(stricmp((*i)->GetElement().GetName(), "Areas") == 0)
		{
			// Go over any and all the child DOM trees and DOM nodes off of the list.
			int areaIndex = 0;
			parTreeNode::ChildNodeIterator j = (*i)->BeginChildren();
			for(; j != (*i)->EndChildren(); ++j)
			{
				Assert(stricmp((*j)->GetElement().GetName(), "Area") == 0);
				//Assertf(areaIndex < iNumAreas, "More areas than expected encountered.");

				CNavResArea * pArea = rage_new CNavResArea();
				m_ResAreas.PushAndGrow(pArea);

				pArea->m_vMin.x = (*j)->GetElement().FindAttributeFloatValue("MinX", 0.0f);
				pArea->m_vMin.y = (*j)->GetElement().FindAttributeFloatValue("MinY", 0.0f);
				pArea->m_vMin.z = (*j)->GetElement().FindAttributeFloatValue("MinZ", 0.0f);
				pArea->m_vMax.x = (*j)->GetElement().FindAttributeFloatValue("MaxX", 0.0f);
				pArea->m_vMax.y = (*j)->GetElement().FindAttributeFloatValue("MaxY", 0.0f);
				pArea->m_vMax.z = (*j)->GetElement().FindAttributeFloatValue("MaxZ", 0.0f);
				pArea->m_iResMult = (*j)->GetElement().FindAttributeIntValue("ResMult", 0);
				pArea->m_iFlags = (*j)->GetElement().FindAttributeIntValue("Flags", 0);

				Vector3 vMinRounded = pArea->m_vMin;
				RoundToNearest(vMinRounded);
				Vector3 vMaxRounded = pArea->m_vMax;
				RoundToNearest(vMaxRounded);

				if(vMinRounded.x > pArea->m_vMin.x)
					vMinRounded.x -= 1.0f;
				if(vMinRounded.y > pArea->m_vMin.y)
					vMinRounded.y -= 1.0f;
				if(vMinRounded.z > pArea->m_vMin.z)
					vMinRounded.z -= 1.0f;

				pArea->m_vMin = vMinRounded;

				if(vMaxRounded.x < pArea->m_vMax.x)
					vMaxRounded.x += 1.0f;
				if(vMaxRounded.y < pArea->m_vMax.y)
					vMaxRounded.y += 1.0f;
				if(vMaxRounded.z < pArea->m_vMax.z)
					vMaxRounded.z += 1.0f;

				if(vMaxRounded.x <= pArea->m_vMin.x)
					vMaxRounded.x += 1.0f;
				if(vMaxRounded.y <= pArea->m_vMin.y)
					vMaxRounded.y += 1.0f;
				if(vMaxRounded.z <= pArea->m_vMin.z)
					vMaxRounded.z += 1.0f;

				pArea->m_vMax = vMaxRounded;

				areaIndex++;
			}
		}
	}

	delete pTree;

	m_iCurrentArea = Min(0, m_ResAreas.GetCount()-1);
	m_pCurrentAreaSlider->SetRange((float)-1, (float)m_ResAreas.GetCount()-1);

	OnSelectArea();
}

//------------------------------------------------------------------

void CNavResAreaEditor::RoundToNearest(float & f)
{
	f = (float) round(f);
	//f = (float)((int)(f+0.5f));
}
void CNavResAreaEditor::RoundToNearest(Vector3 & v)
{
	RoundToNearest(v.x);
	RoundToNearest(v.y);
	RoundToNearest(v.z);
}

//------------------------------------------------------------------

CNavDataCoverpointEditor::CNavDataCoverpointEditor()
{

}

CNavDataCoverpointEditor::~CNavDataCoverpointEditor()
{

}

//------------------------------------------------------------------

void CNavDataCoverpointEditor::Init()
{
	bkBank * pBank = BANKMGR.FindBank("Navigation");
	if(!pBank)
	{
		Assertf(false, "Couldn't locate \"Navigation\" bank.");
		return;
	}

	pBank->AddButton("Initialise Nav Coverpoint Editor", OnInitEditor);
}

//------------------------------------------------------------------

void CNavDataCoverpointEditor::Update()
{
	if(!ms_bEditorInitialized || !ms_bActive)
		return;

	ProcessInput();

	ProcessAIObstructionChecks();
}

//------------------------------------------------------------------

void CNavDataCoverpointEditor::Render()
{
	if(!ms_bEditorInitialized || !ms_bActive)
		return;

	camDebugDirector & debugDirector = camInterface::GetDebugDirector();
	Vector3 vOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;

	const float fMaxRange = 200.0f;

	for(int r=0; r<ms_aDataCoverpoints.GetCount(); r++)
	{
		CNavDataCoverpoint* pDataCoverpoint = ms_aDataCoverpoints[r];

		const Vector3 vCoverpointPos = Vector3(pDataCoverpoint->m_CoordsX, pDataCoverpoint->m_CoordsY, pDataCoverpoint->m_CoordsZ);
		const float fDistSqr = (vCoverpointPos - vOrigin).Mag2();
		if(fDistSqr < fMaxRange*fMaxRange)
		{
			const float fDist = sqrtf(fDistSqr);
			const float fAlpha = Clamp(1.0f - (fDist / fMaxRange), 0.0f, 1.0f);
			if( fAlpha <= 0.0f )
			{
				continue;
			}

			float fDrawHeight = 1.3f;// default for tall cover
			if( pDataCoverpoint->m_Type == NAVMESH_COVERPOINT_LOW_WALL || pDataCoverpoint->m_Type == NAVMESH_COVERPOINT_LOW_WALL_TO_LEFT || pDataCoverpoint->m_Type == NAVMESH_COVERPOINT_LOW_WALL_TO_RIGHT )
			{
				fDrawHeight = 0.8f;// height for low cover
			}

			Color32 renderColor = Color_blue;
			renderColor.SetAlpha((int)(fAlpha*255));

			// Draw vertical line at cover position
			grcDebugDraw::Line(vCoverpointPos, vCoverpointPos + Vector3(0.0f,0.0f,fDrawHeight), renderColor, renderColor);

			Vector3 vCoverDir;
			Vector3FromCoverDir(vCoverDir, pDataCoverpoint->m_Direction);

			// Draw horizontal line in cover direction
			renderColor = Color_white;
			renderColor.SetAlpha((int)(fAlpha*255));
			grcDebugDraw::Line(vCoverpointPos + Vector3(0.0f,0.0f,fDrawHeight), vCoverpointPos + Vector3(0.0f,0.0f,fDrawHeight) + vCoverDir, renderColor, renderColor);

			// If coverpoint is wall to left
			if( pDataCoverpoint->m_Type == NAVMESH_COVERPOINT_LOW_WALL_TO_LEFT || pDataCoverpoint->m_Type == NAVMESH_COVERPOINT_WALL_TO_LEFT )
			{
				Vector3 vRight;
				vRight.Cross( vCoverDir, Vector3( 0.0f, 0.0f, 1.0f ) );
				vRight.NormalizeFast();

				renderColor = Color_green;
				renderColor.SetAlpha((int)(fAlpha*255));

				// Draw horizontal line indicating vantage side
				Vector3 vVantageStart = vCoverpointPos + Vector3(0.0f,0.0f,fDrawHeight);
				Vector3 vVantageEnd = vVantageStart + vRight;
				grcDebugDraw::Line(vVantageStart, vVantageEnd, renderColor);
			}
			// If coverpoint is wall to right
			else if( pDataCoverpoint->m_Type == NAVMESH_COVERPOINT_LOW_WALL_TO_RIGHT || pDataCoverpoint->m_Type == NAVMESH_COVERPOINT_WALL_TO_RIGHT )
			{
				Vector3 vRight;
				vRight.Cross( vCoverDir, Vector3( 0.0f, 0.0f, 1.0f ) );
				vRight.NormalizeFast();

				renderColor = Color_green;
				renderColor.SetAlpha((int)(fAlpha*255));

				// Draw horizontal line indicating vantage side
				Vector3 vVantageStart = vCoverpointPos + Vector3(0.0f,0.0f,fDrawHeight);
				Vector3 vVantageEnd = vVantageStart - vRight;
				grcDebugDraw::Line(vVantageStart, vVantageEnd, renderColor);
			}
			
			// Indicate selected coverpoint
			if( ms_iSelectedDataCoverpointIndex == r )
			{
				renderColor = Color_white;
				renderColor.SetAlpha((int)(fAlpha*255));
				grcDebugDraw::Sphere(vCoverpointPos + Vector3(0.0f,0.0f, fDrawHeight+0.1f), 0.1f, renderColor);
			}

			Vector3 vDrawPosition = vCoverpointPos + Vector3(0.0f,0.0f,0.5f);

			int iNumTexts = 0;

			// Render ID
			char idString[256];
			formatf(idString, "%u", pDataCoverpoint->m_ID );
			grcDebugDraw::Text(vDrawPosition, Color_grey, 0, iNumTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), idString);

			// Render type text
			renderColor = Color_green;
			renderColor.SetAlpha((int)(fAlpha*255));
			switch(pDataCoverpoint->m_Type)
			{
				case NAVMESH_COVERPOINT_LOW_WALL:
					grcDebugDraw::Text(vDrawPosition, renderColor, 0, iNumTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), "LOW_WALL_TO_NEITHER");
					break;
				case NAVMESH_COVERPOINT_LOW_WALL_TO_LEFT:
					grcDebugDraw::Text(vDrawPosition, renderColor, 0, iNumTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), "LOW_WALL_TO_LEFT");
					break;
				case NAVMESH_COVERPOINT_LOW_WALL_TO_RIGHT:
					grcDebugDraw::Text(vDrawPosition, renderColor, 0, iNumTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), "LOW_WALL_TO_RIGHT");
					break;
				case NAVMESH_COVERPOINT_WALL_TO_LEFT:
					grcDebugDraw::Text(vDrawPosition, renderColor, 0, iNumTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), "HIGH_WALL_TO_LEFT");
					break;
				case NAVMESH_COVERPOINT_WALL_TO_RIGHT:
					grcDebugDraw::Text(vDrawPosition, renderColor, 0, iNumTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), "HIGH_WALL_TO_RIGHT");
					break;
				case NAVMESH_COVERPOINT_WALL_TO_NEITHER:
					grcDebugDraw::Text(vDrawPosition, renderColor, 0, iNumTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), "HIGH_WALL_TO_NEITHER");
					break;
				default:
					grcDebugDraw::Text(vDrawPosition, Color_red, 0, iNumTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), "UNKNOWN!");
			}

			// Render AI obstruction status text
			switch(pDataCoverpoint->m_iObstructionCheckStatus)
			{
				case CNavDataCoverpoint::AISTATUS_VALID:
					renderColor = Color_green;
					renderColor.SetAlpha((int)(fAlpha*255));
					grcDebugDraw::Text(vDrawPosition, renderColor, 0, iNumTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), "AI Clear");
					break;
				case CNavDataCoverpoint::AISTATUS_INVALID:
					renderColor = Color_red;
					renderColor.SetAlpha((int)(fAlpha*255));
					grcDebugDraw::Text(vDrawPosition, renderColor, 0, iNumTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), "AI Obstructed");
					break;
				case CNavDataCoverpoint::AISTATUS_PENDING:
					renderColor = Color_yellow;
					renderColor.SetAlpha((int)(fAlpha*255));
					grcDebugDraw::Text(vDrawPosition, renderColor, 0, iNumTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), "AI Pending");
					break;
				case CNavDataCoverpoint::AISTATUS_EXISTING:
					renderColor = Color_orange;
					renderColor.SetAlpha((int)(fAlpha*255));
					grcDebugDraw::Text(vDrawPosition, renderColor, 0, iNumTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), "Existing Point");
					break;
				case CNavDataCoverpoint::AISTATUS_UNCHECKED:
					renderColor = Color_white;
					renderColor.SetAlpha((int)(fAlpha*255));
					grcDebugDraw::Text(vDrawPosition, renderColor, 0, iNumTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), "AI UNCHECKED");
					break;
				default:
					grcDebugDraw::Text(vDrawPosition, Color_red, 0, iNumTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), "Error, unhandled status!");
			}

			// Render too close to other data coverpoint
			if( pDataCoverpoint->m_bTooCloseToOtherDataPoint )
			{
				renderColor = Color_red;
				renderColor.SetAlpha((int)(fAlpha*255));
				grcDebugDraw::Text(vDrawPosition, renderColor, 0, iNumTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), "Too close to data coverpoint");
			}

			// Render too close to existing map coverpoint
			if( ms_bDisplayExistingMapPointProximity && pDataCoverpoint->m_bTooCloseToExistingMapCoverPoint )
			{
				renderColor = Color_red;
				renderColor.SetAlpha((int)(fAlpha*255));
				grcDebugDraw::Text(vDrawPosition, renderColor, 0, iNumTexts++*grcDebugDraw::GetScreenSpaceTextHeight(), "Too close to existing map coverpoint");
			}
		}
	}
}

//------------------------------------------------------------------

void CNavDataCoverpointEditor::OnInitEditor()
{
	if(!ms_bEditorInitialized || !ms_bActive)
	{
		InitWidgets();
		ms_bEditorInitialized = true;
		ms_bActive = true;
	}
}

//------------------------------------------------------------------

void CNavDataCoverpointEditor::InitWidgets()
{
	bkBank * pBank = BANKMGR.FindBank("Navigation");
	if(!pBank)
	{
		Assertf(false, "Couldn't locate \"Navigation\" bank.");
		return;
	}

	static const char * pTypeStrings[] = { "LOW_WALL_TO_NEITHER", "LOW_WALL_TO_LEFT", "LOW_WALL_TO_RIGHT", "HIGH_WALL_TO_LEFT", "HIGH_WALL_TO_RIGHT", "HIGH_WALL_TO_NEITHER" };

	pBank->PushGroup("Coverpoint Editor");

		pBank->AddToggle("Editor Active", &ms_bActive);
		pBank->AddToggle("Show Existing Map Cover Proximity (requires data re-Load and player in range)", &ms_bDisplayExistingMapPointProximity);
	
		pBank->AddSeparator();

		pBank->AddButton("New Coverpoint", OnNewCoverpoint);
		pBank->AddButton("Delete Coverpoint", OnDeleteCoverpoint);
		pBank->AddButton("Select None", OnSelectNone);

		ms_pCurrentCoverpointIndexSlider = pBank->AddSlider("Current Coverpoint Index", &ms_iSelectedDataCoverpointIndex, -1, ms_aDataCoverpoints.GetCount()-1, 1, OnSelectCoverpoint);
		ms_pCurrentCoverpointDirectionSlider = pBank->AddSlider("Direction", &ms_uCurrentCoverpointDirection, 0, 255, 1, OnModifyDirection);
		pBank->AddCombo("Type:", &ms_uCurrentCoverpointType, 6, pTypeStrings, OnModifyType);
	
		pBank->AddSeparator("");
	
		pBank->PushGroup("Save/Load");
		pBank->AddButton("Save..", OnSave);
		pBank->AddButton("Load..", OnLoad);
		pBank->PopGroup();

	pBank->PopGroup(); // "Coverpoint Editor"
}

//------------------------------------------------------------------

void CNavDataCoverpointEditor::ProcessInput()
{
	bool bLeftButtonDown = (ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT)? true : false;
	bool bRightButtonDown = (ioMouse::GetPressedButtons() & ioMouse::MOUSE_RIGHT)? true : false;

	// Left mouse click
	if(bLeftButtonDown)
	{
		// select closest
		Vector3 vPickPos;
		if(CDebugScene::GetWorldPositionUnderMouse(vPickPos))
		{
			int iCoverpointIndex = FindDataCoverpointIndexClosestToPos(vPickPos, 200.0f);
			if(iCoverpointIndex != -1)
			{
				ms_iSelectedDataCoverpointIndex = iCoverpointIndex;
				OnSelectCoverpoint();
			}
		}
	}
	// right mouse click
	else if(bRightButtonDown)
	{
		// reposition currently selected unless it is being checked
		if(ms_iSelectedDataCoverpointIndex >= 0 && ms_iSelectedDataCoverpointIndex < ms_aDataCoverpoints.GetCount())
		{
			Vector3 vPickPos;
			if(CDebugScene::GetWorldPositionUnderMouse(vPickPos))
			{
				// update position
				ms_aDataCoverpoints[ms_iSelectedDataCoverpointIndex]->m_CoordsX = vPickPos.x;
				ms_aDataCoverpoints[ms_iSelectedDataCoverpointIndex]->m_CoordsY = vPickPos.y;
				ms_aDataCoverpoints[ms_iSelectedDataCoverpointIndex]->m_CoordsZ = vPickPos.z;

				// mark for obstruction check needed
				ms_aDataCoverpoints[ms_iSelectedDataCoverpointIndex]->m_iObstructionCheckStatus = CNavDataCoverpoint::AISTATUS_UNCHECKED;

				// check updated position for proximity to other data points
				ms_aDataCoverpoints[ms_iSelectedDataCoverpointIndex]->m_bTooCloseToOtherDataPoint = IsViolatingMinDistToOtherDataPoint(ms_iSelectedDataCoverpointIndex);

				// check position for proximity to existing cover points
				ms_aDataCoverpoints[ms_iSelectedDataCoverpointIndex]->m_bTooCloseToExistingMapCoverPoint = IsViolatingMinDistToExistingCoverPoint(ms_iSelectedDataCoverpointIndex);
			}
		}
	}
}

//------------------------------------------------------------------

void CNavDataCoverpointEditor::ProcessAIObstructionChecks()
{
	ProcessStatusHelperStart();

	ms_CoverPointStatusHelper.Update();

	ProcessStatusHelperFinished();
}

//------------------------------------------------------------------

void CNavDataCoverpointEditor::OnSelectCoverpoint()
{
	if(ms_iSelectedDataCoverpointIndex >= 0 && ms_iSelectedDataCoverpointIndex < ms_aDataCoverpoints.GetCount())
	{
		ms_uCurrentCoverpointDirection = ms_aDataCoverpoints[ms_iSelectedDataCoverpointIndex]->m_Direction;
		ms_uCurrentCoverpointType = ms_aDataCoverpoints[ms_iSelectedDataCoverpointIndex]->m_Type;
	}
	else
	{
		ms_uCurrentCoverpointDirection = 0;
		ms_uCurrentCoverpointType = 0;
	}
}

//------------------------------------------------------------------

void CNavDataCoverpointEditor::OnModifyDirection()
{
	if(ms_iSelectedDataCoverpointIndex >= 0 && ms_iSelectedDataCoverpointIndex < ms_aDataCoverpoints.GetCount())
	{
		ms_aDataCoverpoints[ms_iSelectedDataCoverpointIndex]->m_Direction = ms_uCurrentCoverpointDirection;
		ms_aDataCoverpoints[ms_iSelectedDataCoverpointIndex]->m_iObstructionCheckStatus = CNavDataCoverpoint::AISTATUS_UNCHECKED;
	}
}

//------------------------------------------------------------------

void CNavDataCoverpointEditor::OnModifyType()
{
	if(ms_iSelectedDataCoverpointIndex >= 0 && ms_iSelectedDataCoverpointIndex < ms_aDataCoverpoints.GetCount())
	{
		ms_aDataCoverpoints[ms_iSelectedDataCoverpointIndex]->m_Type = ms_uCurrentCoverpointType;
		ms_aDataCoverpoints[ms_iSelectedDataCoverpointIndex]->m_iObstructionCheckStatus = CNavDataCoverpoint::AISTATUS_UNCHECKED;
	}
}

//------------------------------------------------------------------

void CNavDataCoverpointEditor::OnNewCoverpoint()
{
	camDebugDirector & debugDirector = camInterface::GetDebugDirector();
	Vector3 vSpawnPosition = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;
	vSpawnPosition += camInterface::GetFront();

	AddNewCoverpoint(vSpawnPosition);
}

//------------------------------------------------------------------

void CNavDataCoverpointEditor::OnDeleteCoverpoint()
{
	if(ms_iSelectedDataCoverpointIndex >= 0 && ms_iSelectedDataCoverpointIndex < ms_aDataCoverpoints.GetCount())
	{
		// First check if the point being deleted is being checked
		if( ms_iStatusCheckDataCoverpointIndex == ms_iSelectedDataCoverpointIndex )
		{
			ms_CoverPointStatusHelper.Reset();
			ms_iStatusCheckDataCoverpointIndex = -1;
		}

		// Delete the coverpoint from the list
		CNavDataCoverpoint * pCoverpoint = ms_aDataCoverpoints[ms_iSelectedDataCoverpointIndex];
		delete pCoverpoint;
		ms_aDataCoverpoints.Delete(ms_iSelectedDataCoverpointIndex);

		// Update the index slider range
		ms_pCurrentCoverpointIndexSlider->SetRange((float)-1, (float)ms_aDataCoverpoints.GetCount()-1);

		// If index is out of range
		if( ms_iSelectedDataCoverpointIndex >= ms_aDataCoverpoints.GetCount() )
		{
			// decrement it, possibly resulting in -1 index intentionally
			ms_iSelectedDataCoverpointIndex = ms_aDataCoverpoints.GetCount() - 1;
		}

		// Select current index point
		OnSelectCoverpoint();
	}
}

//------------------------------------------------------------------

void CNavDataCoverpointEditor::OnSelectNone()
{
	ms_iSelectedDataCoverpointIndex = -1;
	OnSelectCoverpoint();
}

//------------------------------------------------------------------

void CNavDataCoverpointEditor::OnSave()
{
	USE_DEBUG_MEMORY();

	char filename[512] = { 0 };
	bool bOk = BANKMGR.OpenFile(filename, 512, "*.xml", true, "Custom Coverpoints XML");
	if(!bOk)
		return;

	parTree *pTree = rage_new parTree();
	Assert(pTree);

	parTreeNode* pRootNode = pTree->CreateRoot();
	Assert(pRootNode);

	parElement& rootElm = pRootNode->GetElement();
	rootElm.SetName("NavCustomCoverpoints");
	rootElm.AddAttribute("Version", ms_iXmlVersion, false);

	pTree->SetRoot(pRootNode);

	parTreeNode * pCoverpointsListXmlNode = rage_new parTreeNode();
	Assert(pCoverpointsListXmlNode);

	pCoverpointsListXmlNode->GetElement().SetName("CustomCoverpoints");
	pCoverpointsListXmlNode->AppendAsChildOf(pRootNode);

	for(int r=0; r<ms_aDataCoverpoints.GetCount(); r++)
	{
		CNavDataCoverpoint * pDataCoverpoint = ms_aDataCoverpoints[r];

		parTreeNode * pCoverpointXmlNode = rage_new parTreeNode();
		Assert(pCoverpointXmlNode);

		pCoverpointXmlNode->GetElement().SetName("Coverpoint");

		pCoverpointXmlNode->GetElement().AddAttribute("ID", r, false); // ensure no possibility of duplicate IDs
		pCoverpointXmlNode->GetElement().AddAttribute("Type", (int)pDataCoverpoint->m_Type, false);
		pCoverpointXmlNode->GetElement().AddAttribute("Direction", (int)pDataCoverpoint->m_Direction, false);
		pCoverpointXmlNode->GetElement().AddAttribute("CoordsX", pDataCoverpoint->m_CoordsX, false);
		pCoverpointXmlNode->GetElement().AddAttribute("CoordsY", pDataCoverpoint->m_CoordsY, false);
		pCoverpointXmlNode->GetElement().AddAttribute("CoordsZ", pDataCoverpoint->m_CoordsZ, false);

		pCoverpointXmlNode->AppendAsChildOf(pCoverpointsListXmlNode);
	}

	const bool bSaveRes = PARSER.SaveTree(filename, "", pTree, parManager::XML);
	if(!bSaveRes)
	{
		Assertf(false, "Failed to save Custom Coverpoints XML '%s'.", filename);
		delete pTree;
		return;
	}

	delete pTree;
}

//------------------------------------------------------------------

void CNavDataCoverpointEditor::OnLoad()
{
	USE_DEBUG_MEMORY();

	char filename[512] = { 0 };
	bool bOk = BANKMGR.OpenFile(filename, 512, "*.xml", false, "Custom Coverpoints XML");
	if(!bOk)
		return;

	Reset();

	parTree * pTree = PARSER.LoadTree(filename, "");
	Assert(pTree);

	const parTreeNode* pRootNode = pTree->GetRoot();
	Assert(pRootNode);
	Assert(stricmp(pRootNode->GetElement().GetName(), "NavCustomCoverpoints") == 0);

	// Make sure we have the correct version.
	ASSERT_ONLY(int version = pRootNode->GetElement().FindAttributeIntValue("Version", 0));
	Assert(version >= ms_iXmlVersion);

	parTreeNode::ChildNodeIterator i = pRootNode->BeginChildren();
	for(; i != pRootNode->EndChildren(); ++i)
	{
		if(stricmp((*i)->GetElement().GetName(), "CustomCoverpoints") == 0)
		{
			int loadIndex = 0;
			// Go over any and all the child DOM trees and DOM nodes off of the list.
			parTreeNode::ChildNodeIterator j = (*i)->BeginChildren();
			for(; j != (*i)->EndChildren(); ++j)
			{
				Assert(stricmp((*j)->GetElement().GetName(), "Coverpoint") == 0);

				CNavDataCoverpoint * pDataCoverpoint = rage_new CNavDataCoverpoint();
				ms_aDataCoverpoints.PushAndGrow(pDataCoverpoint);

				pDataCoverpoint->m_ID = (u32)loadIndex; // ensure no possibility of duplicate IDs
				pDataCoverpoint->m_Type = (u8)(*j)->GetElement().FindAttributeIntValue("Type", 0);
				pDataCoverpoint->m_Direction = (u8)(*j)->GetElement().FindAttributeIntValue("Direction", 0);
				pDataCoverpoint->m_CoordsX = (*j)->GetElement().FindAttributeFloatValue("CoordsX", 0.0f);
				pDataCoverpoint->m_CoordsY = (*j)->GetElement().FindAttributeFloatValue("CoordsY", 0.0f);
				pDataCoverpoint->m_CoordsZ = (*j)->GetElement().FindAttributeFloatValue("CoordsZ", 0.0f);

				++loadIndex;
			}
		}
	}

	delete pTree;

	ms_pCurrentCoverpointIndexSlider->SetRange((float)-1, (float)ms_aDataCoverpoints.GetCount()-1);

	CheckAllPointsForViolatingMinDist();
}

//------------------------------------------------------------------

void CNavDataCoverpointEditor::Reset()
{
	for(int r=0; r<ms_aDataCoverpoints.GetCount(); r++)
	{
		CNavDataCoverpoint * pDataCoverpoint = ms_aDataCoverpoints[r];
		delete pDataCoverpoint;
	}
	ms_aDataCoverpoints.clear();

	ms_iSelectedDataCoverpointIndex = -1;
	OnSelectCoverpoint();
}

//------------------------------------------------------------------

int CNavDataCoverpointEditor::FindDataCoverpointIndexClosestToPos(const Vector3 & vPos, const float fMaxDist)
{
	float fClosestDistSq = FLT_MAX;
	int iClosestIndex = -1;
	for(int r=0; r < ms_aDataCoverpoints.GetCount(); r++)
	{
		CNavDataCoverpoint* pCoverpoint = ms_aDataCoverpoints[r];
		Vector3 vCoverpointPos(pCoverpoint->m_CoordsX, pCoverpoint->m_CoordsY, pCoverpoint->m_CoordsZ);
		float fDistSq = vPos.Dist2(vCoverpointPos);
		if(fDistSq < fClosestDistSq && fDistSq <= fMaxDist)
		{
			fClosestDistSq = fDistSq;
			iClosestIndex = r;
		}
	}
	return iClosestIndex;
}

//------------------------------------------------------------------

void CNavDataCoverpointEditor::AddNewCoverpoint(const Vector3& vSpawnPosition)
{
	USE_DEBUG_MEMORY();	// Ensure that we alloc from any free memory devkit may have

	// Setup the new coverpoint at position with default values
	CNavDataCoverpoint* pNewCoverpoint = rage_new CNavDataCoverpoint();
	pNewCoverpoint->m_CoordsX = vSpawnPosition.x;
	pNewCoverpoint->m_CoordsY = vSpawnPosition.y;
	pNewCoverpoint->m_CoordsZ = vSpawnPosition.z;
	
	// Append to the list
	ms_aDataCoverpoints.PushAndGrow(pNewCoverpoint);
	pNewCoverpoint->m_ID = ms_aDataCoverpoints.GetCount()-1;

	// Update the slider range of indices
	ms_pCurrentCoverpointIndexSlider->SetRange((float)-1, (float)(ms_aDataCoverpoints.GetCount()-1));

	// Select the new coverpoint
	ms_iSelectedDataCoverpointIndex = ms_aDataCoverpoints.GetCount()-1;
	OnSelectCoverpoint();
}

//------------------------------------------------------------------

void CNavDataCoverpointEditor::ProcessStatusHelperStart()
{
	// if status check index is negative
	if( ms_iStatusCheckDataCoverpointIndex < 0 )
	{
		// Find the next index corresponding to a data coverpoint in camera range
		const float fMaxRange = 200.0f;
		camDebugDirector & debugDirector = camInterface::GetDebugDirector();
		Vector3 vOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;
		for(int r=0; r<ms_aDataCoverpoints.GetCount(); r++)
		{
			CNavDataCoverpoint* pDataCoverpoint = ms_aDataCoverpoints[r];

			// skip unless unchecked status
			if( pDataCoverpoint->m_iObstructionCheckStatus != CNavDataCoverpoint::AISTATUS_UNCHECKED )
			{
				continue;
			}

			// check if within range
			const Vector3 vCoverpointPos = Vector3(pDataCoverpoint->m_CoordsX, pDataCoverpoint->m_CoordsY, pDataCoverpoint->m_CoordsZ);
			const float fDistSqr = (vCoverpointPos - vOrigin).Mag2();
			if(fDistSqr < fMaxRange*fMaxRange)
			{
				// set next index to check!
				ms_iStatusCheckDataCoverpointIndex = r;
				break;
			}
		}

		// If a qualifying coverpoint was found
		if( ms_iStatusCheckDataCoverpointIndex >= 0 )
		{
			// Get the qualifying data coverpoint
			CNavDataCoverpoint* pDataCoverpointToCheck = ms_aDataCoverpoints[ms_iStatusCheckDataCoverpointIndex];

			Vector3 vDataCoverpointPos(pDataCoverpointToCheck->m_CoordsX,pDataCoverpointToCheck->m_CoordsY,pDataCoverpointToCheck->m_CoordsZ);

			// Add a temporary coverpoint to test the status
			CCoverPoint::eCoverUsage coverUsage = CCoverPoint::COVUSE_WALLTOBOTH;
			CCoverPoint::eCoverHeight coverHeight = CCoverPoint::COVHEIGHT_LOW;

			// Convert the older style cover usage type into the new cover usage and cover height
			if(pDataCoverpointToCheck->m_Type == NAVMESH_COVERPOINT_LOW_WALL)
			{
				coverUsage = CCoverPoint::COVUSE_WALLTOBOTH;
				coverHeight = CCoverPoint::COVHEIGHT_LOW;
			}
			else if(pDataCoverpointToCheck->m_Type == NAVMESH_COVERPOINT_LOW_WALL_TO_LEFT)
			{
				coverUsage = CCoverPoint::COVUSE_WALLTOLEFT;
				coverHeight = CCoverPoint::COVHEIGHT_LOW;
			}
			else if(pDataCoverpointToCheck->m_Type == NAVMESH_COVERPOINT_LOW_WALL_TO_RIGHT)
			{
				coverUsage = CCoverPoint::COVUSE_WALLTORIGHT;
				coverHeight = CCoverPoint::COVHEIGHT_LOW;
			}
			else if(pDataCoverpointToCheck->m_Type == NAVMESH_COVERPOINT_WALL_TO_LEFT)
			{
				coverUsage = CCoverPoint::COVUSE_WALLTOLEFT;
				coverHeight = CCoverPoint::COVHEIGHT_TOOHIGH;
			}
			else if(pDataCoverpointToCheck->m_Type == NAVMESH_COVERPOINT_WALL_TO_RIGHT)
			{
				coverUsage = CCoverPoint::COVUSE_WALLTORIGHT;
				coverHeight = CCoverPoint::COVHEIGHT_TOOHIGH;
			}
			else if(pDataCoverpointToCheck->m_Type == NAVMESH_COVERPOINT_WALL_TO_NEITHER)
			{
				coverUsage = CCoverPoint::COVUSE_WALLTONEITHER;
				coverHeight = CCoverPoint::COVHEIGHT_TOOHIGH;
			}
			else
			{
				Assert(0);
			}

			// In case there is an existing coverpoint here
			s16 iExistingCoverPointIndex = INVALID_COVER_POINT_INDEX;

			// Try to add coverpoint the same way we load from map data
			ms_iTempCoverPointIndex = CCover::AddCoverPoint(
				CCover::CP_NavmeshAndDynamicPoints,
				CCoverPoint::COVTYPE_POINTONMAP,
				NULL,
				&vDataCoverpointPos,
				coverHeight,
				coverUsage,
				pDataCoverpointToCheck->m_Direction,
				CCoverPoint::COVARC_90,
				false,
				VEH_INVALID_ID,
				&iExistingCoverPointIndex
				);

			// If we are trying to add a coverpoint to test, and there is an existing one
			if( ms_iTempCoverPointIndex == INVALID_COVER_POINT_INDEX && iExistingCoverPointIndex != INVALID_COVER_POINT_INDEX )
			{
				// mark the status as existing
				pDataCoverpointToCheck->m_iObstructionCheckStatus = CNavDataCoverpoint::AISTATUS_EXISTING;

				// reset the status index to try again
				ms_iStatusCheckDataCoverpointIndex = -1;

				return;
			}
			// If we tried to add but just couldn't for some other reason
			else if( ms_iTempCoverPointIndex == INVALID_COVER_POINT_INDEX )
			{
				// reset the status index to try again
				ms_iStatusCheckDataCoverpointIndex = -1;

				return;
			}
			// we added a point for test successfully
			else // ms_iTempCoverPointIndex != INVALID_COVER_POINT_INDEX
			{
				CCoverPoint* pCoverPointToCheck = CCover::FindCoverPointWithIndex(ms_iTempCoverPointIndex);
				if(pCoverPointToCheck)
				{
					// Start the helper checks
					ms_CoverPointStatusHelper.Start(pCoverPointToCheck);

					// Get the qualifying data coverpoint and mark status pending
					ms_aDataCoverpoints[ms_iStatusCheckDataCoverpointIndex]->m_iObstructionCheckStatus = CNavDataCoverpoint::AISTATUS_PENDING;
						
					return;
				}

				// something went wrong along the way here
				// reset the status index to try again
				ms_iStatusCheckDataCoverpointIndex = -1;
			}
		}
	}
}

//------------------------------------------------------------------

void CNavDataCoverpointEditor::ProcessStatusHelperFinished()
{
	if( ms_CoverPointStatusHelper.IsFinished() )
	{
		// Get the data coverpoint being checked
		CNavDataCoverpoint* pDataCoverpointToCheck = ms_aDataCoverpoints[ms_iStatusCheckDataCoverpointIndex];

		// Check that nothing has been modified while the check was pending
		// NOTE: Otherwise it will be changed back to AISTATUS_UNCHECKED and we'll check it again
		if( pDataCoverpointToCheck->m_iObstructionCheckStatus == CNavDataCoverpoint::AISTATUS_PENDING )
		{
			// mark the data coverpoint status according to the result
			u8 statusResult = ms_CoverPointStatusHelper.GetStatus();
			if( statusResult == CCoverPoint::COVSTATUS_Clear )
			{
				pDataCoverpointToCheck->m_iObstructionCheckStatus = CNavDataCoverpoint::AISTATUS_VALID;
			}
			else if( statusResult == CCoverPoint::COVSTATUS_DirectionBlocked || statusResult == CCoverPoint::COVSTATUS_PositionBlocked )
			{
				pDataCoverpointToCheck->m_iObstructionCheckStatus = CNavDataCoverpoint::AISTATUS_INVALID;
			}
			// not sure why but sometimes status helper finishes on statusResult invalid, which means unchecked
			else if( statusResult == CCoverPoint::COVSTATUS_Invalid )
			{
				pDataCoverpointToCheck->m_iObstructionCheckStatus = CNavDataCoverpoint::AISTATUS_UNCHECKED;
			}
			else // unhandled coverpoint status!
			{
				Assertf(0, "unhandled coverpoint status!");
				pDataCoverpointToCheck->m_iObstructionCheckStatus = CNavDataCoverpoint::AISTATUS_UNCHECKED;
			}
		}

		// reset the helper
		ms_CoverPointStatusHelper.Reset();
		
		// remove the coverpoint from the real cover pool
		CCover::RemoveCoverPointWithIndex(ms_iTempCoverPointIndex);
		ms_iTempCoverPointIndex = INVALID_COVER_POINT_INDEX;

		// set the status check index back to -1 to repeat the process
		ms_iStatusCheckDataCoverpointIndex = -1;
	}
}

//------------------------------------------------------------------

void CNavDataCoverpointEditor::CheckAllPointsForViolatingMinDist()
{
	for(int r=0; r < ms_aDataCoverpoints.GetCount(); r++)
	{
		ms_aDataCoverpoints[r]->m_bTooCloseToOtherDataPoint = IsViolatingMinDistToOtherDataPoint(r);
		ms_aDataCoverpoints[r]->m_bTooCloseToExistingMapCoverPoint = IsViolatingMinDistToExistingCoverPoint(r);
	}
}

//------------------------------------------------------------------

bool CNavDataCoverpointEditor::IsViolatingMinDistToOtherDataPoint(int queryDataCoverpointIndex)
{
	if( queryDataCoverpointIndex >= 0 && queryDataCoverpointIndex < ms_aDataCoverpoints.GetCount() )
	{
		const CNavDataCoverpoint* pQueryDataCoverpoint = ms_aDataCoverpoints[queryDataCoverpointIndex];
		const Vector3 vQueryPosition(pQueryDataCoverpoint->m_CoordsX,pQueryDataCoverpoint->m_CoordsY,pQueryDataCoverpoint->m_CoordsZ);
		for(int r=0; r < ms_aDataCoverpoints.GetCount(); r++)
		{
			if(r == queryDataCoverpointIndex)
			{
				// skip self
				continue;
			}

			const CNavDataCoverpoint* pOtherDataCoverpoint = ms_aDataCoverpoints[r];

			const Vector3 vOtherPosition(pOtherDataCoverpoint->m_CoordsX, pOtherDataCoverpoint->m_CoordsY, pOtherDataCoverpoint->m_CoordsZ);
			const float fDistSq = vOtherPosition.Dist2(vQueryPosition);
			// check against cover constant limit - this is what run-time will use to reject inside AddCoverPoint
			if( fDistSq <= CCover::ms_fMinDistSqrBetweenCoverPoints )
			{
				return true;
			}
		}
	}
	
	return false;
}

//------------------------------------------------------------------

bool CNavDataCoverpointEditor::IsViolatingMinDistToExistingCoverPoint(int queryDataCoverpointIndex)
{
	if( queryDataCoverpointIndex >= 0 && queryDataCoverpointIndex < ms_aDataCoverpoints.GetCount() )
	{
		const CNavDataCoverpoint* pQueryDataCoverpoint = ms_aDataCoverpoints[queryDataCoverpointIndex];
		const Vector3 vQueryPosition(pQueryDataCoverpoint->m_CoordsX,pQueryDataCoverpoint->m_CoordsY,pQueryDataCoverpoint->m_CoordsZ);

		CCoverPoint* pOverlappingCoverPoint = NULL;
		if( NULL == CCoverPointsContainer::CheckIfNoOverlapAndGetGridCell(vQueryPosition, CCoverPoint::COVTYPE_POINTONMAP, NULL, pOverlappingCoverPoint) )
		{
			return true;
		}
	}

	return false;
}

//------------------------------------------------------------------

} // namespace rage


#endif // __NAVEDITORS

