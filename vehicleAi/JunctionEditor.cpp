#include "vehicleAi/JunctionEditor.h"

#if __JUNCTION_EDITOR

#include "bank\bkmgr.h"
#include "bank\slider.h"
#include "camera\CamInterface.h"
#include "camera\debug\DebugDirector.h"
#include "camera\helpers\Frame.h"
#include "debug\DebugScene.h"
#include "fwdebug\debugdraw.h"
#include "peds\PlayerInfo.h"
#include "peds/Ped.h"
#include "text\text.h"
#include "text\textformat.h"
#include "text\textconversion.h"
#include "vehicleAi\pathfind.h"
#include "fwvehicleai\pathfindtypes.h"
#include "fwmaths\angle.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

s32 CJunctionEditor::m_iMode = CJunctionEditor::Mode_Normal;
bool CJunctionEditor::m_bRebindEntrances = false;
bool CJunctionEditor::m_bMustReinitialiseWidgets = false;

bool CJunctionEditor::m_bInitialised = false;
bool CJunctionEditor::m_bActivated = false;
bool CJunctionEditor::m_bJustActivated = false;
bool CJunctionEditor::m_bShowEditor = true;
bool CJunctionEditor::m_bToggleActivation = false;
bkWidget * CJunctionEditor::m_pActivateButton = NULL;
bkWidget * CJunctionEditor::m_pMainGroup = NULL;
bkWidget * CJunctionEditor::m_pCurrentJunctionGroup = NULL;
bkSlider * CJunctionEditor::m_pCurrentJunctionSlider = NULL;
bkSlider * CJunctionEditor::m_pCurrentEntranceSlider = NULL;
bkSlider * CJunctionEditor::m_pNumPhasesSlider = NULL;
bkGroup * CJunctionEditor::m_pEntrancesGroup = NULL;
bkGroup * CJunctionEditor::m_pPhasesGroup = NULL;

bkGroup * CJunctionEditor::m_pTrafficLightLocationsGroup = NULL;
bkSlider * CJunctionEditor::m_pTrafficLightSearchDistanceSlider = NULL;
bkSlider * CJunctionEditor::m_pTrafficLightIndexSlider = NULL;
bkText * CJunctionEditor::m_pTrafficLightText = NULL;
bkText * CJunctionEditor::m_pNumTrafficLightsText = NULL;
bkButton * CJunctionEditor::m_pTrafficLightPickButton = NULL;
bkButton * CJunctionEditor::m_pTrafficLightClearButton = NULL;

CJunctionEditor::EntranceBankVars CJunctionEditor::m_EntranceBankVars[MAX_ROADS_INTO_JUNCTION];
CJunctionEditor::PhaseVars CJunctionEditor::m_PhaseVars[MAX_ROADS_INTO_JUNCTION];

bool CJunctionEditor::m_bSelectAnyNodeAsJunction = false;

s32 CJunctionEditor::m_iNumJunctions = 0;
s32 CJunctionEditor::m_iCurrentJunction = -1;
s32 CJunctionEditor::m_iSecondaryJunction = -1;
s32 CJunctionEditor::m_iCurrentTrafficLight = 0;
char CJunctionEditor::m_CurrentTrafficLightText[256] = { 0 };
char CJunctionEditor::m_NumTrafficLightsText[64] = { 0 };
s32 CJunctionEditor::m_iDesiredJunction = -1;

bool CJunctionEditor::m_iCurrentJunctionIsRailwayCrossing = false;
bool CJunctionEditor::m_iCurrentJunctionCanSkipPedPhase = false;
float CJunctionEditor::m_fCurrentJunctionPhaseOffset = 0.0f;
CAutoJunctionAdjustment CJunctionEditor::m_CurrentAutoJuncAdj;

//s32 CJunctionEditor::m_iCurrentEntrance = -1;
//s32 CJunctionEditor::m_iDesiredEntrance = -1;
s32 CJunctionEditor::m_iNumPhases = 0;

void GetJunctionsNodesLinkedToJunctionNode(const CPathNode * pNode, const CPathNode ** ppLinkedNodes, const s32 iMaxLinkedNodes, s32 & iNumLinkedNodes);
void GetEntrancesLinkedToJunctions(const CPathNode ** ppJunctionNodes, const s32 iNumJunctionNodes, const CPathNode ** ppEntranceNodes, const s32 iMaxNumEntranceNodes, s32 & iNumEntranceNodes);

const float fMaxDistSqr = 100.0f * 100.0f;
const Vector3 vRaiseJunction(0.0f,0.0f,5.0f);
const Vector3 vRaiseLink(0.0f,0.0f,3.0f);
const Vector3 vRaiseJoin(0.0f,0.0f,1.0f);

const Color32 iJunctionCol = Color_green2;
const Color32 iEntranceCol = Color_yellow2;
const Color32 iEntranceColSlipLane = Color_DarkGoldenrod;
const Color32 iLinkCol = Color_green4;

const Vector3 g_vInvalidJunctionNodePosition(64000.0f, 64000.0f, 64000.0f);

CJunctionEditor::CJunctionEditor()
{
}
CJunctionEditor::~CJunctionEditor()
{

}
void CJunctionEditor::Init()
{
	memset(m_EntranceBankVars, 0, sizeof(EntranceBankVars)*MAX_ROADS_INTO_JUNCTION);
	memset(m_PhaseVars, 0, sizeof(PhaseVars)*MAX_ROADS_INTO_JUNCTION);

	m_iNumJunctions = CJunctions::GetNumJunctionTemplates();

	InitWidgets(true);
}
void CJunctionEditor::ToggleEditor()
{
	m_bActivated = !m_bActivated;
	m_bToggleActivation = true;
}

bkBank * CJunctionEditor::GetBank()
{
	const char * pBankName = "Vehicle AI and Nodes";
	bkBank * pBank = BANKMGR.FindBank(pBankName);
	Assertf(pBank, "Ok, who moved the \"%s\" bank?", pBankName);
	return pBank;
}


void CJunctionEditor::InitWidgets(bool bFirstTime)
{
	bkBank * pBank = GetBank();
	
	if(!pBank)
	{
		return; 
	}

	if(bFirstTime)
	{
		m_pMainGroup = pBank->PushGroup("Junction Editor");
		m_pActivateButton = (bkWidget*)pBank->AddButton("Activate Junction Editor", ToggleEditor);
		pBank->PopGroup();
	}

	if(m_bActivated)
	{
		SetInitialNodesDebug();

		pBank->Remove(*m_pActivateButton);
		pBank->Remove(*m_pMainGroup);

		m_pMainGroup = pBank->PushGroup("Junction Editor");

		pBank->AddToggle("Show Editor", &m_bShowEditor);
		pBank->AddToggle("Debug Junctions", &CJunctions::m_bDebug);
		pBank->AddToggle("Debug Wait For Traffic", &CJunctions::m_bDebugWaitForTraffic);
		pBank->AddToggle("Debug Junction Text", &CJunctions::m_bDebugText);

		// >> Save / Load
		m_pCurrentJunctionGroup = pBank->PushGroup("Save / Load");
		pBank->AddButton("Save All Templates", CJunctions::EditorSaveJunctionTemplates);
		pBank->AddButton("Load All Templates", CJunctions::EditorLoadJunctionTemplates);

		safecpy(CJunctions::ms_JunctionEditorXmlFilename, "common:/data/levels/gta5/junctions.xml");
		pBank->AddText("XML Filename", CJunctions::ms_JunctionEditorXmlFilename, RAGE_MAX_PATH);
		pBank->AddButton("Save All Templates (XML)", CJunctions::EditorSaveJunctionTemplatesXml);
		pBank->AddButton("Load All Templates (XML)", CJunctions::EditorLoadJunctionTemplatesXml);

		pBank->PopGroup(); // < Save / Load

		pBank->AddButton("Refresh all junctions", CJunctions::RefreshAllJunctions);
		
		// > Junction Editor
		{
			pBank->AddText("Num Junctions", &m_iNumJunctions);
			m_pCurrentJunctionSlider = pBank->AddSlider("Current Junction", &m_iDesiredJunction, -1, m_iNumJunctions-1, 1);
			pBank->AddSlider("Secondary Junction", &m_iSecondaryJunction, -1, m_iNumJunctions-1, 1);

			pBank->AddButton("Warp here!", WarpToCurrentJunctionTemplate);

			pBank->AddButton("Add New Junction", NewJunction);
			pBank->AddButton("Delete Current Junction", DeleteJunction);

			pBank->AddToggle("Allow any node as junction", &m_bSelectAnyNodeAsJunction);

			m_pCurrentJunctionGroup = pBank->PushGroup("Current Junction");
		
			// > Current Junction
			{
				pBank->AddButton("Select Junction Node", StartSelectJunctionNode);
				pBank->AddButton("Reposition junction and entrances", SelectAndRebindJunctionAndEntrances);
				pBank->AddButton("Add new phase", AddNewPhase);

				pBank->AddToggle("Railway Crossing", &m_iCurrentJunctionIsRailwayCrossing, OnToggleIsRailwayCrossing);
				pBank->AddToggle("Disable Skip Ped Phase", &m_iCurrentJunctionCanSkipPedPhase, OnToggleDisableSkipPedPhase);
				pBank->AddSlider("Phase Offset", &m_fCurrentJunctionPhaseOffset, 0.0f, 100.0f, 0.1f, OnChangePhaseOffset);
			
				m_pNumPhasesSlider = pBank->AddSlider("Num Phases", &m_iNumPhases, 0, 0, 1, OnSetNumPhases);
				m_pEntrancesGroup = pBank->PushGroup("Entrances");

				// > Entrances
				{
					pBank->PopGroup(); // < Entrances
				}

				m_pPhasesGroup = pBank->PushGroup("Phases");

				// > Phases
				{
					pBank->PopGroup(); // < Phases
				}

				m_pTrafficLightLocationsGroup = pBank->PushGroup("Traffic Lights");

				m_pNumTrafficLightsText = pBank->AddText("Num Lights Defined : ", m_NumTrafficLightsText, 256 );
				m_pTrafficLightIndexSlider = pBank->AddSlider("Lights", &m_iCurrentTrafficLight, 0, MAX_TRAFFIC_LIGHT_LOCATIONS-1, 1, OnChangeCurrentTrafficLight);
				m_pTrafficLightText = pBank->AddText("Position:", m_CurrentTrafficLightText, 256 );
				m_pTrafficLightPickButton = pBank->AddButton("Pick Location", OnPickTrafficLightLocation);
				m_pTrafficLightClearButton = pBank->AddButton("Clear", OnClearTrafficLight);

				// > Traffic Light Locations
				{
					pBank->PopGroup(); // < Traffic Light Locations
				}

				pBank->PopGroup(); // < Current Junction
			}

			pBank->PushGroup("AutoJunction Adjustments");
			{
				pBank->AddButton("Find or create adjustment for nearest junction", FindOrCreateAutoAdjustment);
				pBank->AddButton("Remove Adjustment from nearest junction", RemoveAutoAdjustment);
				pBank->AddVector("Junction Pos", &m_CurrentAutoJuncAdj.m_vLocation, -10000.0f, 10000.0f, 0.0f);
				pBank->AddSlider("Cycle Offset", &m_CurrentAutoJuncAdj.m_fCycleOffset, 0.0f, 1000.0f, 0.1f, OnUpdateAutoAdjustment);
				pBank->AddSlider("Cycle Duration", &m_CurrentAutoJuncAdj.m_fCycleDuration, 0.0f, 1000.0f, 0.1f, OnUpdateAutoAdjustment);
				pBank->PopGroup(); // < AutoJunction Adjustments
			}
		
			pBank->PopGroup(); // < Junction Editor
		}

		m_bJustActivated = true;
	}
}
void CJunctionEditor::Update()
{
	if(!m_bShowEditor)
		return;

	if(!m_bInitialised)
	{
		Init();
		m_bInitialised = true;
	}
	if(m_bMustReinitialiseWidgets)
	{
		InitJunctionWidgets();
		m_bMustReinitialiseWidgets = false;
	}
	if(m_bToggleActivation)
	{
		m_bToggleActivation = false;

		InitWidgets(false);
		return;
	}
	if(!m_bActivated)
		return;

	if(m_bJustActivated)
	{
		// Make sure 'm_pMainGroup' is visible & selected as current
		//bkBank * pBank = GetBank();
		//if(pBank)
		//	pBank->SetCurrentGroup( *((bkGroup*)m_pMainGroup) );

		m_bJustActivated = false;
	}

	if(m_iDesiredJunction != -1 && m_iDesiredJunction != m_iCurrentJunction)
	{
		m_iCurrentJunction = m_iDesiredJunction;
		OnUseJunctionSlider();
	}

	switch(m_iMode)
	{
		case Mode_SelectJunctionNode:
		{
			UpdateSelectNode();
			break;
		}
		case Mode_SelectTrafficLightLocation:
		{
			UpdateSelectTrafficLightLocation();
			break;
		}
		/*
		case Mode_SelectEntranceNode:
		{
			UpdateSelectNode();
			break;
		}
		*/
		default:
			break;
	}

	CJunctions::m_bDebugLights = true;

	// Ensure that all pathnodes's junction nodes are marked as "m_qualifiesAsJunction"

	for(int j=0; j<CJunctions::GetNumJunctionTemplates(); j++)
	{
		CJunctionTemplate & temp = CJunctions::GetJunctionTemplate(j);
		if(temp.m_iFlags & CJunctionTemplate::Flag_NonEmpty)
		{
			//----------------------------------------------------------------------------------------------------
			// For all junction nodes in templated junctions, ensure that the "m_qualifiesAsJunction" flag is set
			for(int n=0; n<temp.m_iNumJunctionNodes; n++)
			{
				const Vector3 & vNodePos = temp.m_vJunctionNodePositions[n];
				CNodeAddress node = ThePaths.FindNodeClosestToCoors(vNodePos, 4.0f);
				if(!node.IsEmpty())
				{
					CPathNode * pJunctionNode = ThePaths.FindNodePointerSafe(node);
					if(pJunctionNode)
					{
						pJunctionNode->m_2.m_qualifiesAsJunction = true;
					}
				}
			}
		}
	}
}

void CJunctionEditor::OnUseJunctionSlider()
{
	m_bMustReinitialiseWidgets = true;
	//InitJunctionWidgets();
}

void CJunctionEditor::OnToggleDisableSkipPedPhase()
{
	if(m_iCurrentJunction != -1)
	{
		CJunctionTemplate & temp = CJunctions::GetJunctionTemplate(m_iCurrentJunction);
		if(m_iCurrentJunctionCanSkipPedPhase)
			temp.m_iFlags &= ~CJunctionTemplate::Flag_DisableSkipPedLightPhase;
		else
			temp.m_iFlags |= CJunctionTemplate::Flag_DisableSkipPedLightPhase;
	}
}

void CJunctionEditor::OnToggleIsRailwayCrossing()
{
	if(m_iCurrentJunction != -1)
	{
		CJunctionTemplate & temp = CJunctions::GetJunctionTemplate(m_iCurrentJunction);
		if(m_iCurrentJunctionIsRailwayCrossing)
			temp.m_iFlags |= CJunctionTemplate::Flag_RailwayCrossing;
		else
			temp.m_iFlags &= ~CJunctionTemplate::Flag_RailwayCrossing;
	}
}

void CJunctionEditor::OnChangePhaseOffset()
{
	if (m_iCurrentJunction != -1)
	{
		CJunctionTemplate& temp = CJunctions::GetJunctionTemplate(m_iCurrentJunction);
		temp.m_fPhaseOffset = m_fCurrentJunctionPhaseOffset;
	}
}

void CJunctionEditor::OnChangeCurrentTrafficLight()
{
	Assert(m_iCurrentTrafficLight >= 0 && m_iCurrentTrafficLight < MAX_TRAFFIC_LIGHT_LOCATIONS);
	m_iCurrentTrafficLight = Clamp(m_iCurrentTrafficLight, 0, MAX_TRAFFIC_LIGHT_LOCATIONS-1);

	CJunctionTemplate & temp = CJunctions::GetJunctionTemplate(m_iCurrentJunction);
	atRangeArray<CJunctionTemplate::CTrafficLightLocation,MAX_TRAFFIC_LIGHT_LOCATIONS> & lights = temp.m_TrafficLightLocations;

	// Never allow the user to advance the index beyond the last invalid entry
	// This ensures that we always have a packed zero-based array with valid values
	if(m_iCurrentTrafficLight > 0)
	{
		while(m_iCurrentTrafficLight > 0 && lights[m_iCurrentTrafficLight-1].m_iPosX == TRAFFIC_LIGHT_POS_INVALID)
			m_iCurrentTrafficLight--;
	}

	SetCurrentTrafficLightText();
}

void CJunctionEditor::OnPickTrafficLightLocation()
{
	if(m_iCurrentJunction != -1)
	{
		m_iMode = Mode_SelectTrafficLightLocation;
	}
}

void CJunctionEditor::OnClearTrafficLight()
{
	CJunctionTemplate & temp = CJunctions::GetJunctionTemplate(m_iCurrentJunction);
	atRangeArray<CJunctionTemplate::CTrafficLightLocation,MAX_TRAFFIC_LIGHT_LOCATIONS> & lights = temp.m_TrafficLightLocations;

	if(lights[m_iCurrentTrafficLight].m_iPosX == TRAFFIC_LIGHT_POS_INVALID)
		return;

	int i;
	for(i=m_iCurrentTrafficLight; i<MAX_TRAFFIC_LIGHT_LOCATIONS-1; i++)
	{
		lights[i] = lights[i+1];
	}
	lights[MAX_TRAFFIC_LIGHT_LOCATIONS-1].m_iPosX = TRAFFIC_LIGHT_POS_INVALID;
	lights[MAX_TRAFFIC_LIGHT_LOCATIONS-1].m_iPosY = TRAFFIC_LIGHT_POS_INVALID;
	lights[MAX_TRAFFIC_LIGHT_LOCATIONS-1].m_iPosZ = TRAFFIC_LIGHT_POS_INVALID;

	temp.m_iNumTrafficLightLocations--;
	Assert(temp.m_iNumTrafficLightLocations >= 0);
	temp.m_iNumTrafficLightLocations = Max(temp.m_iNumTrafficLightLocations, 0);

	SetCurrentTrafficLightText();
}

void CJunctionEditor::AddNewPhase()
{
	if(m_iCurrentJunction != -1 && m_iCurrentJunction < m_iNumJunctions)
	{
		if(m_iNumPhases < MAX_ROADS_INTO_JUNCTION)
		{
			m_iNumPhases++;
			CJunctionTemplate & temp = CJunctions::GetJunctionTemplate(m_iCurrentJunction);
			temp.m_iNumPhases = m_iNumPhases;
			//InitJunctionWidgets();
			m_bMustReinitialiseWidgets = true;
		}
	}
}

void CJunctionEditor::OnSetNumPhases()
{
	if(m_iCurrentJunction != -1 && m_iCurrentJunction < m_iNumJunctions)
	{
		if(m_iNumPhases == 0)
		{
			m_iNumPhases = 1;

		}
		CJunctionTemplate & temp = CJunctions::GetJunctionTemplate(m_iCurrentJunction);
		temp.m_iNumPhases = m_iNumPhases;
		m_bMustReinitialiseWidgets = true;
	}
}

void CJunctionEditor::WarpToCurrentJunctionTemplate()
{
	if(m_iCurrentJunction != -1)
	{
		CJunctionTemplate & temp = CJunctions::GetJunctionTemplate(m_iCurrentJunction);
		if(temp.m_iNumJunctionNodes && (temp.m_iFlags & CJunctionTemplate::Flag_NonEmpty)!=0)
		{
			CPed * pPlayer = FindPlayerPed();
			if(pPlayer)
			{
				Vector3 vHeightAboveJunction(0.0f, 0.0f, 50.0f);
				Vector3 vFront(0.0f, 0.0f, -1.0f);
				Vector3 vUp(0.0f, 1.0f, 0.0f);

				camDebugDirector & debugDirector = camInterface::GetDebugDirector();
				debugDirector.ActivateFreeCam();
				debugDirector.GetFreeCamFrameNonConst().SetPosition(temp.m_vJunctionNodePositions[0] + vHeightAboveJunction);
				debugDirector.GetFreeCamFrameNonConst().SetWorldMatrixFromFrontAndUp( vFront, vUp );

				pPlayer->Teleport(temp.m_vJunctionNodePositions[0], pPlayer->GetCurrentHeading());
			}
		}
	}
}

void CJunctionEditor::NextJunction()
{
	if(m_iNumJunctions == 0)
	{
		m_iCurrentJunction = -1;
	}
	else
	{
		m_iCurrentJunction++;
		if(m_iCurrentJunction >= m_iNumJunctions)
			m_iCurrentJunction = 0;
	}

	//InitJunctionWidgets();
	m_bMustReinitialiseWidgets = true;
}

void CJunctionEditor::PrevJunction()
{
	if(m_iNumJunctions == 0)
	{
		m_iCurrentJunction = -1;
	}
	else
	{
		m_iCurrentJunction--;
		if(m_iCurrentJunction < 0)
			m_iCurrentJunction = m_iNumJunctions-1;
	}

	//InitJunctionWidgets();
	m_bMustReinitialiseWidgets = true;
}

void CJunctionEditor::InitJunctionWidgets()
{
	const char * pBankName = "Vehicle AI and Nodes";
	bkBank * pBank = BANKMGR.FindBank(pBankName);
	Assertf(pBank, "Ok, who moved the \"%s\" bank?", pBankName);
	if(!pBank)
		return;

	//----------------------------------------------
	// Wipe out all the existing 'entrances' groups

	pBank->SetCurrentGroup(*m_pEntrancesGroup);

	s32 e;
	for(e=0; e<MAX_ROADS_INTO_JUNCTION; e++)
	{
		if(m_EntranceBankVars[e].m_pPhaseSlider)
			pBank->Remove(*m_EntranceBankVars[e].m_pPhaseSlider);

		if(m_EntranceBankVars[e].m_pStoppingLineSlider)
			pBank->Remove(*m_EntranceBankVars[e].m_pStoppingLineSlider);

		if(m_EntranceBankVars[e].m_pOrientationSlider)
			pBank->Remove(*m_EntranceBankVars[e].m_pOrientationSlider);

		if(m_EntranceBankVars[e].m_pLeftFilterPhaseSlider)
			pBank->Remove(*m_EntranceBankVars[e].m_pLeftFilterPhaseSlider);

		if(m_EntranceBankVars[e].m_pLeftLaneAheadOnlyToggle)
			pBank->Remove(*m_EntranceBankVars[e].m_pLeftLaneAheadOnlyToggle);

		if(m_EntranceBankVars[e].m_pCanTurnRightToggle)
			pBank->Remove(*m_EntranceBankVars[e].m_pCanTurnRightToggle);

		if (m_EntranceBankVars[e].m_pRightLaneIsRightOnlyToggle)
			pBank->Remove(*m_EntranceBankVars[e].m_pRightLaneIsRightOnlyToggle);
		
		if(m_EntranceBankVars[e].m_pGroup)
			pBank->DeleteGroup(*m_EntranceBankVars[e].m_pGroup);

		m_EntranceBankVars[e].m_pPhaseSlider = NULL;
		m_EntranceBankVars[e].m_pStoppingLineSlider = NULL;
		m_EntranceBankVars[e].m_pOrientationSlider = NULL;
		m_EntranceBankVars[e].m_pLeftFilterPhaseSlider = NULL;
		m_EntranceBankVars[e].m_pLeftLaneAheadOnlyToggle = NULL;
		m_EntranceBankVars[e].m_pCanTurnRightToggle = NULL;
		m_EntranceBankVars[e].m_pRightLaneIsRightOnlyToggle = NULL;
		m_EntranceBankVars[e].m_pGroup = NULL;
	}

	pBank->UnSetCurrentGroup(*m_pEntrancesGroup);

	//-------------------------------------
	// Likewise for all the phase groups

	pBank->SetCurrentGroup(*m_pPhasesGroup);

	s32 p;
	for(p=0; p<MAX_ROADS_INTO_JUNCTION; p++)
	{
		if(m_PhaseVars[p].m_pTime)
			pBank->Remove(*m_PhaseVars[p].m_pTime);
		if(m_PhaseVars[p].m_pGroup)
			pBank->DeleteGroup(*m_PhaseVars[p].m_pGroup);

		m_PhaseVars[p].m_pTime = NULL;
		m_PhaseVars[p].m_pGroup = NULL;
	}

	pBank->UnSetCurrentGroup(*m_pPhasesGroup);

	//-----------------------------------
	// Delete all the traffic-light info

	pBank->SetCurrentGroup(*m_pTrafficLightLocationsGroup);

	if(m_pNumTrafficLightsText)
		pBank->Remove(*m_pNumTrafficLightsText);
	if(m_pTrafficLightSearchDistanceSlider)
		pBank->Remove(*m_pTrafficLightSearchDistanceSlider);
	if(m_pTrafficLightIndexSlider)
		pBank->Remove(*m_pTrafficLightIndexSlider);
	if(m_pTrafficLightText)
		pBank->Remove(*m_pTrafficLightText);
	if(m_pTrafficLightPickButton)
		pBank->Remove(*m_pTrafficLightPickButton);
	if(m_pTrafficLightClearButton)
		pBank->Remove(*m_pTrafficLightClearButton);

	m_pNumTrafficLightsText = NULL;
	m_pTrafficLightSearchDistanceSlider = NULL;
	m_pTrafficLightIndexSlider = NULL;
	m_pTrafficLightText = NULL;
	m_pTrafficLightPickButton = NULL;
	m_pTrafficLightClearButton = NULL;

	pBank->UnSetCurrentGroup(*m_pTrafficLightLocationsGroup);

	//------------------------------------------------

	if(m_iCurrentJunction == -1 || m_iCurrentJunction >= m_iNumJunctions)
	{
		return;
	}
	CJunctionTemplate & temp = CJunctions::GetJunctionTemplate(m_iCurrentJunction);
	if(!(temp.m_iFlags & CJunctionTemplate::Flag_NonEmpty))
	{
		return;
	}

	m_iCurrentJunctionIsRailwayCrossing = ((temp.m_iFlags & CJunctionTemplate::Flag_RailwayCrossing)!=0);
	m_iCurrentJunctionCanSkipPedPhase = ((temp.m_iFlags & CJunctionTemplate::Flag_DisableSkipPedLightPhase)==0);
	m_fCurrentJunctionPhaseOffset = temp.m_fPhaseOffset;

	//-----------------------
	// Set the phases slider

	if( temp.m_iNumPhases == 0 )
	{
		temp.m_iNumPhases = 1;
		temp.m_PhaseTimings[0].m_fStartTime = 0.0f;
		temp.m_PhaseTimings[0].m_fDuration = 10.0f;
	}


	m_iNumPhases = temp.m_iNumPhases;
	m_pNumPhasesSlider->SetRange(0, float(temp.m_iNumEntrances));

	//---------------------------------
	// Create the new entrances groups

	char text[256];

	pBank->SetCurrentGroup(*m_pEntrancesGroup);

	for(e=0; e<temp.m_iNumEntrances; e++)
	{
		CJunctionTemplate::CEntrance & entrance = temp.m_Entrances[e];

		entrance.m_iPhase = Clamp(entrance.m_iPhase, 0, temp.m_iNumPhases-1);
		entrance.m_iLeftFilterLanePhase = Clamp(entrance.m_iLeftFilterLanePhase, -1, temp.m_iNumPhases-1);

		sprintf(text, "Entrance #%i", e);	
		m_EntranceBankVars[e].m_pGroup = pBank->PushGroup(text);
		m_EntranceBankVars[e].m_pPhaseSlider = pBank->AddSlider("Phase", &entrance.m_iPhase, 0, temp.m_iNumPhases, 1);
		m_EntranceBankVars[e].m_pStoppingLineSlider = pBank->AddSlider("Stopping Line", &entrance.m_fStoppingDistance, -20.0f, 20.0f, 0.1f);
		m_EntranceBankVars[e].m_pOrientationSlider = pBank->AddSlider("Orientation", &entrance.m_fOrientation, -PI, PI, 0.01f);
		m_EntranceBankVars[e].m_pLeftFilterPhaseSlider = pBank->AddSlider("Left Filter Phase", &entrance.m_iLeftFilterLanePhase, -1, temp.m_iNumPhases, 1);
		m_EntranceBankVars[e].m_pLeftLaneAheadOnlyToggle = pBank->AddToggle("Left Lane is Ahead Only", &entrance.m_bLeftLaneIsAheadOnly);
		m_EntranceBankVars[e].m_pCanTurnRightToggle = pBank->AddToggle("Can Turn Right On Red Light", &entrance.m_bCanTurnRightOnRedLight);
		m_EntranceBankVars[e].m_pRightLaneIsRightOnlyToggle = pBank->AddToggle("Right Lane Goes Right Only", &entrance.m_bRightLaneIsRightOnly);
		pBank->PopGroup(); 
	}

	pBank->UnSetCurrentGroup(*m_pEntrancesGroup);

	//------------------------------
	// Create the new phases groups

	pBank->SetCurrentGroup(*m_pPhasesGroup);

	for(p=0; p<temp.m_iNumPhases; p++)
	{
		sprintf(text, "Phase #%i", p);
		m_PhaseVars[p].m_pGroup = pBank->PushGroup(text);
		m_PhaseVars[p].m_pTime = pBank->AddSlider("Lights Time", &temp.m_PhaseTimings[p].m_fDuration, 0.0f, 120.0f, 1.0f);
		pBank->PopGroup();
	}

	pBank->UnSetCurrentGroup(*m_pPhasesGroup);

	//--------------------------------------
	// Create the new traffic lights groups

	// Invalidate all entries beyond the number which are defined for this template
	// This is because zero seems to be used for a default where parGen doesn't find
	// an entry in the xml, and this can't be overridden per data member
	for(int t=temp.m_iNumTrafficLightLocations; t<MAX_TRAFFIC_LIGHT_LOCATIONS; t++)
	{
		temp.m_TrafficLightLocations[t].m_iPosX = TRAFFIC_LIGHT_POS_INVALID;
		temp.m_TrafficLightLocations[t].m_iPosY = TRAFFIC_LIGHT_POS_INVALID;
		temp.m_TrafficLightLocations[t].m_iPosZ = TRAFFIC_LIGHT_POS_INVALID;
	}

	m_iCurrentTrafficLight = 0;
	SetCurrentTrafficLightText();

	pBank->SetCurrentGroup(*m_pTrafficLightLocationsGroup);
	m_pTrafficLightSearchDistanceSlider = pBank->AddSlider("Traffic Light Search Distance", &temp.m_fSearchDistance,0.0f,200.0f,1.0f);
	m_pNumTrafficLightsText = pBank->AddText("Num Lights Defined : ", m_NumTrafficLightsText, 256 );
	m_pTrafficLightIndexSlider = pBank->AddSlider("Lights", &m_iCurrentTrafficLight, 0, MAX_TRAFFIC_LIGHT_LOCATIONS-1, 1, OnChangeCurrentTrafficLight);
	m_pTrafficLightText = pBank->AddText("Position:", m_CurrentTrafficLightText, 256);
	m_pTrafficLightPickButton = pBank->AddButton("Pick Location", OnPickTrafficLightLocation);
	m_pTrafficLightClearButton = pBank->AddButton("Clear", OnClearTrafficLight);

	pBank->UnSetCurrentGroup(*m_pTrafficLightLocationsGroup);
}


void CJunctionEditor::NewJunction()
{
	if(m_iNumJunctions < CJunctions::GetMaxNumJunctionTemplates())
	{
		CJunctionTemplate & temp = CJunctions::GetJunctionTemplate(m_iNumJunctions);
		temp.m_iFlags = CJunctionTemplate::Flag_NonEmpty;

		m_iCurrentJunction = m_iNumJunctions;
		m_iDesiredJunction = m_iNumJunctions;

//		m_iCurrentEntrance = -1;
		m_iNumJunctions++;
	}

	m_pCurrentJunctionSlider->SetRange(-1.0f, m_iNumJunctions-1.0f);

	//InitJunctionWidgets();
	m_bMustReinitialiseWidgets = true;
}

void CJunctionEditor::DeleteJunction()
{
	if(m_iCurrentJunction >= 0 && m_iCurrentJunction < m_iNumJunctions)
	{
		CJunctionTemplate & temp = CJunctions::GetJunctionTemplate(m_iCurrentJunction);

		temp.m_iFlags = 0;
		temp.m_iNumEntrances = 0;
		temp.m_iNumJunctionNodes = 0;
		temp.m_iNumTrafficLightLocations = 0;
		temp.m_iNumPhases = 0;

		for(s32 j=0; j<MAX_JUNCTION_NODES_PER_JUNCTION; j++)
		{
			temp.m_vJunctionNodePositions[j] = g_vInvalidJunctionNodePosition;
		}

		m_iCurrentJunction = -1;
	}

	m_pCurrentJunctionSlider->SetRange(-1.0f, m_iNumJunctions-1.0f);

	//InitJunctionWidgets();
	m_bMustReinitialiseWidgets = true;
}

void CJunctionEditor::StartSelectJunctionNode()
{
	if(m_iCurrentJunction != -1)
	{
		m_iMode = Mode_SelectJunctionNode;
		m_bRebindEntrances = false;
	}
}
/*
void CJunctionEditor::StartSelectEntranceNode()
{
	if(m_iCurrentJunction != -1)
	{
		m_iMode = Mode_SelectEntranceNode;
		m_bRebindEntrances = false;
	}
}
*/
void CJunctionEditor::SelectAndRebindJunctionAndEntrances()
{
	if(m_iCurrentJunction != -1)
	{
		m_iMode = Mode_SelectJunctionNode;
		m_bRebindEntrances = true;
	}
}



void CJunctionEditor::OnSelectJunctionNode(const CNodeAddress & iNodeAddress)
{
	Assert(m_iCurrentJunction >= 0 && m_iCurrentJunction < m_iNumJunctions);
	Assert(!iNodeAddress.IsEmpty());

	//----------------------------------------------------------------------------
	// If we have a junction node selected, then set this in the current junction

	const CPathNode * pNode = ThePaths.FindNodePointerSafe(iNodeAddress);
	Assert(pNode);

	Vector3 vCoords;
	pNode->GetCoors(vCoords);

	//----------------------------------------------
	// Ensure we don't already have a template here

	CJunctionTemplate * pTemp = CJunctions::GetJunctionTemplateAtPosition(vCoords);
	if(!pTemp || m_bRebindEntrances)
	{
		CJunctionTemplate & currentJunction = CJunctions::GetJunctionTemplate(m_iCurrentJunction);

		// Flag as in-use
		currentJunction.m_iFlags |= CJunctionTemplate::Flag_NonEmpty;

		currentJunction.m_vJunctionMin = vCoords;
		currentJunction.m_vJunctionMin.z -= 2.0f;
		currentJunction.m_vJunctionMax = vCoords;
		currentJunction.m_vJunctionMax.z += 8.0f;

		s32 iNumJunctionNodes = 0;
		const CPathNode * ppJunctionNodes[MAX_JUNCTION_NODES_PER_JUNCTION];
		GetJunctionsNodesLinkedToJunctionNode(pNode, ppJunctionNodes, MAX_JUNCTION_NODES_PER_JUNCTION, iNumJunctionNodes);

		currentJunction.m_iNumJunctionNodes = iNumJunctionNodes;
		for(s32 n=0; n<iNumJunctionNodes; n++)
		{
			ppJunctionNodes[n]->GetCoors(vCoords);
			currentJunction.m_vJunctionNodePositions[n] = vCoords;
		}

		s32 iNumEntrances = 0;
		const CPathNode * ppEntranceNodes[MAX_ROADS_INTO_JUNCTION];
		GetEntrancesLinkedToJunctions(ppJunctionNodes, iNumJunctionNodes, ppEntranceNodes, MAX_ROADS_INTO_JUNCTION, iNumEntrances);

		currentJunction.m_iNumEntrances = iNumEntrances;

		s32 e;
		for(e=0; e<iNumEntrances; e++)
		{
			Vector3 vEntrancePos;
			ppEntranceNodes[e]->GetCoors(vEntrancePos);

			currentJunction.m_vJunctionMin.x = Min(currentJunction.m_vJunctionMin.x, vEntrancePos.x);
			currentJunction.m_vJunctionMin.y = Min(currentJunction.m_vJunctionMin.y, vEntrancePos.y);
			currentJunction.m_vJunctionMin.z = Min(currentJunction.m_vJunctionMin.z, vEntrancePos.z);
			currentJunction.m_vJunctionMax.x = Max(currentJunction.m_vJunctionMax.x, vEntrancePos.x);
			currentJunction.m_vJunctionMax.y = Max(currentJunction.m_vJunctionMax.y, vEntrancePos.y);
			currentJunction.m_vJunctionMax.z = Max(currentJunction.m_vJunctionMax.z, vEntrancePos.z);

			currentJunction.m_Entrances[e].m_vNodePosition = vEntrancePos;
			currentJunction.m_Entrances[e].m_fStoppingDistance = 0.0f;

			// Try to approximate a junction orientation
			CNodeAddress iEntranceNode = ThePaths.FindNodeClosestToCoors(vEntrancePos, PathfindFindNonJunctionNodeCB, NULL, 2.0f);
			Assert(!iEntranceNode.IsEmpty());
			const CPathNode * pEntranceNode = ThePaths.FindNodePointerSafe(iEntranceNode);
			Assert(pEntranceNode && !pEntranceNode->IsJunctionNode());

			if(pEntranceNode)
			{
				Vector3 vLinkedNodePos;
				pEntranceNode->GetCoors(vLinkedNodePos);
				Vector3 vPrevNodePos = vEntrancePos;
				if(pEntranceNode->NumLinks()==2)
				{
					const CPathNode * pLinkedNode1 = ThePaths.GetNodesLinkedNode(pEntranceNode, 0);
					const CPathNode * pLinkedNode2 = ThePaths.GetNodesLinkedNode(pEntranceNode, 1);
					const CPathNode * pPrevNode = (pLinkedNode1 && pLinkedNode2) ?
						(pLinkedNode1->IsJunctionNode() ? pLinkedNode2 : pLinkedNode1) : NULL;
					if(pPrevNode)
						pPrevNode->GetCoors(vPrevNodePos);
				}

				Vector3 vEntranceDir = vPrevNodePos - vLinkedNodePos;
				currentJunction.m_Entrances[e].m_fOrientation = rage::Atan2f(-vEntranceDir.x, vEntranceDir.y);
				currentJunction.m_Entrances[e].m_fOrientation = fwAngle::LimitRadianAngleSafe(currentJunction.m_Entrances[e].m_fOrientation);
			}
			else
			{
				currentJunction.m_Entrances[e].m_fOrientation = 0.0f;
			}

			// Get the angle used for sorting in clockwise order
			currentJunction.m_Entrances[e].m_fAngleFromCenter = fwAngle::GetRadianAngleBetweenPoints(vEntrancePos.x, vEntrancePos.y, vCoords.x, vCoords.y);

			if(currentJunction.m_Entrances[e].m_fAngleFromCenter < 0.0f)
				currentJunction.m_Entrances[e].m_fAngleFromCenter += TWO_PI;
			if(currentJunction.m_Entrances[e].m_fAngleFromCenter >= TWO_PI)
				currentJunction.m_Entrances[e].m_fAngleFromCenter -= TWO_PI;
		}

//		m_iCurrentEntrance = 0;
//		m_pCurrentEntranceSlider->SetRange(-1.0f, iNumEntrances-1.0f);

		currentJunction.m_iNumPhases = iNumEntrances;

		for(s32 p=0; p<currentJunction.m_iNumPhases; p++)
		{
			currentJunction.m_PhaseTimings[p].m_fDuration = 10.0f;
		}

		//------------------------------------------------------------
		// Now sort all the entrances so they are in clockwise order

		bool bChange = true;
		while(bChange)
		{
			bChange = false;
			for(e = 0; e < iNumEntrances-1; e++)
			{
				if(currentJunction.m_Entrances[e].m_fAngleFromCenter > currentJunction.m_Entrances[e+1].m_fAngleFromCenter)
				{
					CJunctionTemplate::CEntrance temp = currentJunction.m_Entrances[e];
					currentJunction.m_Entrances[e] = currentJunction.m_Entrances[e+1];
					currentJunction.m_Entrances[e+1] = temp;
					bChange = true;
				}
			}
		}

		for(e=0; e<iNumEntrances; e++)
		{
			currentJunction.m_Entrances[e].m_iPhase = e;
		}
	}

	// Init widgets
	//InitJunctionWidgets();
	m_bMustReinitialiseWidgets = true;
}

void GetJunctionsNodesLinkedToJunctionNode(const CPathNode * pNode, const CPathNode ** ppLinkedNodes, const s32 iMaxLinkedNodes, s32 & iNumLinkedNodes)
{
	Assert(pNode->m_2.m_qualifiesAsJunction || CJunctionEditor::m_bSelectAnyNodeAsJunction);

	if(iNumLinkedNodes >= iMaxLinkedNodes)
		return;

	ppLinkedNodes[iNumLinkedNodes++] = pNode;

	for(s32 l=0; l<pNode->NumLinks(); l++)
	{
		const CPathNodeLink & link = ThePaths.GetNodesLink(pNode, l);
		if(link.m_1.m_bShortCut)
			continue;

		const CPathNode * pLinkedNode = ThePaths.GetNodesLinkedNode(pNode, l);
		if(pLinkedNode && pLinkedNode->m_2.m_qualifiesAsJunction)
		{
			// Is this node already in the list?..
			s32 m;
			for(m=0; m<iNumLinkedNodes; m++)
			{
				if(ppLinkedNodes[m]==pLinkedNode)
					break;
			}
			// ..if not then visit it now
			if(m==iNumLinkedNodes)
			{
				GetJunctionsNodesLinkedToJunctionNode(pLinkedNode, ppLinkedNodes, iMaxLinkedNodes, iNumLinkedNodes);
			}
		}
	}
}

void GetEntrancesLinkedToJunctions(const CPathNode ** ppJunctionNodes, const s32 iNumJunctionNodes, const CPathNode ** ppEntranceNodes, const s32 iMaxNumEntranceNodes, s32 & iNumEntranceNodes)
{
	for(s32 j=0; j<iNumJunctionNodes; j++)
	{
		const CPathNode * pJunctionNode = ppJunctionNodes[j];

		for(s32 l=0; l<pJunctionNode->NumLinks(); l++)
		{
			const CPathNodeLink & link = ThePaths.GetNodesLink(pJunctionNode, l);

			// Don't include shortcuts as entrances
			if(link.m_1.m_bShortCut)
				continue;
			// Ignore links which are exit-only
			if(link.m_1.m_LanesFromOtherNode == 0)
				continue;

			const CPathNode * pLinkedNode = ThePaths.GetNodesLinkedNode(pJunctionNode, l);
			if(pLinkedNode && !pLinkedNode->m_2.m_qualifiesAsJunction)
			{
				s32 e;
				for(e=0; e<iNumEntranceNodes; e++)
				{
					if(ppEntranceNodes[e]==pLinkedNode)
						break;
				}
				if(e==iNumEntranceNodes)
				{
					ppEntranceNodes[iNumEntranceNodes++] = pLinkedNode;
					if(iNumEntranceNodes >= iMaxNumEntranceNodes)
						return;
				}
			}
		}
	}
}

void CJunctionEditor::UpdateSelectTrafficLightLocation()
{
	if(CDebugScene::GetMouseRightPressed())
	{
		m_iMode = Mode_Normal;
		return;
	}

	//-------------------------------------------------------------------
	// Draw the intersection pos with geometry of ray cast through mouse

	Vector3 vWorldPos;
	const bool bHit = CDebugScene::GetWorldPositionUnderMouse(vWorldPos);

	if(bHit)
	{
		s16 posX = (s16)(vWorldPos.x+0.5f);
		s16 posY = (s16)(vWorldPos.y+0.5f);
		s16 posZ = (s16)(vWorldPos.z+0.5f);

		grcDebugDraw::Sphere(Vector3((float)posX,(float)posY,(float)posZ), 6.0f, Color_SpringGreen2, false);

		if(CDebugScene::GetMouseLeftPressed())
		{
			CJunctionTemplate & temp = CJunctions::GetJunctionTemplate(m_iCurrentJunction);
			CJunctionTemplate::CTrafficLightLocation & lightLoc = temp.m_TrafficLightLocations[m_iCurrentTrafficLight];

			bool bNewEntry = lightLoc.m_iPosX == TRAFFIC_LIGHT_POS_INVALID;

			lightLoc.m_iPosX = posX;
			lightLoc.m_iPosY = posY;
			lightLoc.m_iPosZ = posZ;

			// only increment the num entries if we've just filled in a new slot
			if(bNewEntry)
				temp.m_iNumTrafficLightLocations++;
			Assert(temp.m_iNumTrafficLightLocations <= MAX_TRAFFIC_LIGHT_LOCATIONS);

			m_iCurrentTrafficLight++;
			m_iCurrentTrafficLight = Min(m_iCurrentTrafficLight, temp.m_iNumTrafficLightLocations);
			m_iCurrentTrafficLight = Min(m_iCurrentTrafficLight, MAX_TRAFFIC_LIGHT_LOCATIONS-1);

			SetCurrentTrafficLightText();

			m_iMode = Mode_Normal;
		}
	}
}

void CJunctionEditor::SetCurrentTrafficLightText()
{
	CJunctionTemplate & temp = CJunctions::GetJunctionTemplate(m_iCurrentJunction);
	CJunctionTemplate::CTrafficLightLocation & lightLoc = temp.m_TrafficLightLocations[m_iCurrentTrafficLight];

	if(lightLoc.m_iPosX == TRAFFIC_LIGHT_POS_INVALID)
	{
		strcpy(m_CurrentTrafficLightText, "[none]");
	}
	else
	{
		sprintf(m_CurrentTrafficLightText, "[%i, %i, %i]", lightLoc.m_iPosX, lightLoc.m_iPosY, lightLoc.m_iPosZ);
	}

	if(m_pTrafficLightText)
		m_pTrafficLightText->SetString(m_CurrentTrafficLightText);

	sprintf(m_NumTrafficLightsText, "%i", temp.m_iNumTrafficLightLocations);
	if(m_pNumTrafficLightsText)
		m_pNumTrafficLightsText->SetString(m_NumTrafficLightsText);
}

void CJunctionEditor::FindOrCreateAutoAdjustment()
{
	CJunction* junction = CJunctions::Debug_GetClosestJunction();
	if (junction )
	{
		if (junction->HasTrafficLightNodes())
		{
			m_CurrentAutoJuncAdj = CJunctions::FindOrCreateAutoJunctionAdjustment(*junction);
		}
		else
		{
			Warningf("No traffic lights at this junction");
		}
	}
	else
	{
		m_CurrentAutoJuncAdj.m_vLocation.ZeroComponents();
		m_CurrentAutoJuncAdj.m_fCycleDuration = 0.0f;
		m_CurrentAutoJuncAdj.m_fCycleOffset = 0.0f;
	}
}

void CJunctionEditor::RemoveAutoAdjustment()
{
	CJunction* junction = CJunctions::Debug_GetClosestJunction();
	if (junction)
	{
		CJunctions::DeleteAutoJunctionAdjustment(*junction);
	}
}

void CJunctionEditor::OnUpdateAutoAdjustment()
{
	CAutoJunctionAdjustment* adj = CJunctions::FindAutoJunctionAdjustmentData(m_CurrentAutoJuncAdj.m_vLocation);
	*adj = m_CurrentAutoJuncAdj;
}

bool JunctionEditorJunctionNodeCB(CPathNode * pNode, void * UNUSED_PARAM(pData))
{
	if(pNode->IsWaterNode() || pNode->IsPedNode() || pNode->IsParkingNode() || pNode->IsOpenSpaceNode())
		return false;

	if(!pNode->IsJunctionNode() && !CJunctionEditor::m_bSelectAnyNodeAsJunction)
		return false;

	return true;
}
bool JunctionEditorFindNonJunctionNodeCB(CPathNode * pNode, void * UNUSED_PARAM(pData))
{
	if(pNode->IsWaterNode() || pNode->IsPedNode() || pNode->IsParkingNode() || pNode->IsOpenSpaceNode())
		return false;

	if(pNode->IsJunctionNode())
		return false;

	return true;
}

void CJunctionEditor::UpdateSelectNode()
{
	if(CDebugScene::GetMouseRightPressed())
	{
		m_iMode = Mode_Normal;
		return;
	}

	const Vector3 vRaiseJunction(0.0f,0.0f,5.0f);

	//-------------------------------------------------------------------
	// Draw the intersection pos with geometry of ray cast through mouse

	Vector3 vWorldPos;
	const bool bHit = CDebugScene::GetWorldPositionUnderMouse(vWorldPos);

	if(bHit)
	{
		grcDebugDraw::Sphere(vWorldPos, 2.0f, Color_orange, false);

		if(m_iMode == Mode_SelectJunctionNode)
		{
			//-----------------------------------------------------------------------
			// Draw highlighted junction node if there is one close to intersect pos

			CNodeAddress iCloseNode = ThePaths.FindNodeClosestToCoors(vWorldPos, JunctionEditorJunctionNodeCB, NULL, 2.0f);
			const CPathNode * pCloseNode = ThePaths.FindNodePointerSafe(iCloseNode);

			if(pCloseNode &&
				(pCloseNode->m_2.m_qualifiesAsJunction || m_bSelectAnyNodeAsJunction))
			{
				s32 iNumLinkedNodes = 0;
				const CPathNode * ppLinkedNodes[MAX_JUNCTION_NODES_PER_JUNCTION];
				GetJunctionsNodesLinkedToJunctionNode(pCloseNode, ppLinkedNodes, MAX_JUNCTION_NODES_PER_JUNCTION, iNumLinkedNodes);

				for(s32 n=0; n<iNumLinkedNodes; n++)
				{
					pCloseNode = ppLinkedNodes[n];
					Vector3 vJunctionCoords;
					pCloseNode->GetCoors(vJunctionCoords);

					grcDebugDraw::Cylinder(RCC_VEC3V(vJunctionCoords), VECTOR3_TO_VEC3V(vJunctionCoords+vRaiseJunction), 1.5f, Color_purple);
				}

				if(CDebugScene::GetMouseLeftPressed())
				{
					OnSelectJunctionNode(iCloseNode);
					m_iMode = Mode_Normal;
				}
			}
		}
		/*
		else if(m_iMode == Mode_SelectEntranceNode)
		{
			//-----------------------------------------------------------------------
			// Draw highlighted entrance node if there is one close to intersect pos

			Assert(m_iCurrentJunction != -1);
			CJunctionTemplate & temp = CJunctions::GetJunctionTemplate(m_iCurrentJunction);

			float fClosest = FLT_MAX;
			int iClosestEntrance = -1;
			for(int e=0; e<temp.m_iNumEntrances; e++)
			{
				CJunctionTemplate::CEntrance & entance = temp.m_Entrances[e];
				if((entance.m_vNodePosition - vWorldPos).XYMag() < fClosest)
				{
					fClosest = (entance.m_vNodePosition - vWorldPos).XYMag();
					iClosestEntrance = e;
				}
			}
			if(iClosestEntrance != -1)
			{
				CJunctionTemplate::CEntrance & entance = temp.m_Entrances[iClosestEntrance];
				grcDebugDraw::Cylinder(entance.m_vNodePosition, entance.m_vNodePosition+vRaiseLink, 1.5f, Color_purple, false, false);

				if(CDebugScene::GetMouseLeftPressed())
				{
					m_iCurrentEntrance = iClosestEntrance;
					m_iMode = Mode_Normal;
				}
			}
		}
		*/
	}
}

void CJunctionEditor::SetInitialNodesDebug()
{
	//-------------------------------------
	// Set pathfind node/links debugging

	ThePaths.bDisplayPathsDebug_Allow = true;

	ThePaths.bDisplayPathsDebug_Nodes_AllowDebugDraw = true;
	ThePaths.bDisplayPathsDebug_Nodes_StandardInfo = true; //false;
	ThePaths.bDisplayPathsDebug_Nodes_Pilons = true;
	ThePaths.bDisplayPathsDebug_Nodes_ToCollisionDiff = false;
	ThePaths.bDisplayPathsDebug_Nodes_StreetNames = false;
	ThePaths.bDisplayPathsDebug_Nodes_DistanceHash = false;

	ThePaths.bDisplayPathsDebug_Links_AllowDebugDraw = true;
	ThePaths.bDisplayPathsDebug_Links_DrawLine = true;
	ThePaths.bDisplayPathsDebug_Links_DrawLaneDirectionArrows = true;
	ThePaths.bDisplayPathsDebug_Links_DrawTrafficLigths = true;
	ThePaths.bDisplayPathsDebug_Links_TextInfo = false;
	ThePaths.bDisplayPathsDebug_Links_LaneCenters = false;
	ThePaths.bDisplayPathsDebug_Links_RoadExtremes = false;
	ThePaths.bDisplayPathsDebug_Links_Tilt = false;
	ThePaths.bDisplayPathsDebug_Links_RegionAndIndex = false;
}


void CJunctionEditor::Render()
{
	if(!m_bActivated || !m_bShowEditor)
		return;

	//------------------------------
	// Display HUD text

	RenderTextHUD();

	camDebugDirector & debugDirector = camInterface::GetDebugDirector();
	const Vector3 vOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;

	RenderJunctionNodes(vOrigin);

	for(int t=0; t<m_iNumJunctions; t++)
	{
		CJunctionTemplate & temp = CJunctions::GetJunctionTemplate(t);
		if((temp.m_iFlags & CJunctionTemplate::Flag_NonEmpty)==0)
			continue;

		Color32 iBoxCol = (t == m_iCurrentJunction) ? Color_turquoise : Color_OrangeRed;

		// If we had an error binding this junction, then flash it red as a warning
		bool bErrorBindingJunction = false;
		if(temp.m_iNumJunctionNodes > 0)
		{
			s32 iJunction = CJunctions::GetJunctionAtPosition(temp.m_vJunctionNodePositions[0]);
			if(iJunction != -1)
			{
				CJunction * pJunction = CJunctions::GetJunctionByIndex(iJunction);
				if(pJunction && pJunction->GetErrorBindingJunction() && (fwTimer::GetFrameCount() & 1))
				{
					iBoxCol = Color_red;
					bErrorBindingJunction = true;
				}
			}
		}

		grcDebugDraw::BoxAxisAligned(RCC_VEC3V(temp.m_vJunctionMin), RCC_VEC3V(temp.m_vJunctionMax), iBoxCol, false);

		char tmp[128];

		if(bErrorBindingJunction)
		{			
			sprintf(tmp, "ERROR BINDING JUNCTION TEMPLATE : %i", t);
			grcDebugDraw::Text((temp.m_vJunctionMin+temp.m_vJunctionMax)*0.5f, iBoxCol, tmp);
		}
		else
		{
			sprintf(tmp, "Junction Template: %i", t);
			grcDebugDraw::Text((temp.m_vJunctionMin+temp.m_vJunctionMax)*0.5f, iBoxCol, tmp);
		}
	}

	if(m_iCurrentJunction >= 0 && m_iCurrentJunction < m_iNumJunctions)
	{
		RenderJunctionTemplate(vOrigin, m_iCurrentJunction);
	}
	if(m_iSecondaryJunction >= 0 && m_iSecondaryJunction < m_iNumJunctions)
	{
		RenderJunctionTemplate(vOrigin, m_iSecondaryJunction);
	}
}



void CJunctionEditor::RenderJunctionTemplate(const Vector3 & vOrigin, const s32 iTemplate)
{
	CJunctionTemplate & temp = CJunctions::GetJunctionTemplate(iTemplate);

	if(!(temp.m_iFlags & CJunctionTemplate::Flag_NonEmpty))
		return;

	CNodeAddress iCentralNode = ThePaths.FindNodeClosestToCoors(temp.m_vJunctionNodePositions[0], PathfindFindJunctionNodeCB, NULL, 2.0f);
	if(iCentralNode.IsEmpty())
		return;
	const CPathNode * pJunctionNode = ThePaths.FindNodePointerSafe(iCentralNode);
	if(!pJunctionNode)
		return;
	Assert(pJunctionNode->m_2.m_qualifiesAsJunction);

	Vector3 vJunctionPos, vLinkedNodePos;
	pJunctionNode->GetCoors(vJunctionPos);

	const float fMaxDistSqr = 100.0f * 100.0f;
	const float fDist2 = (vJunctionPos - vOrigin).Mag2();
	if(fDist2 > fMaxDistSqr)
		return;

	const Vector3 vRaiseJunction(0.0f,0.0f,5.0f);
	const Vector3 vRaiseLink(0.0f,0.0f,3.0f);
	const Vector3 vRaiseStopLine(0.0f,0.0f,1.0f);
	const Vector3 vTextRaise(0.0f,0.0f,1.0f); //2.0f);

	const s32 iTextHeight = grcDebugDraw::GetScreenSpaceTextHeight();
	char text[256];

	int iScrY = 0;

	sprintf(text, "Template (%i)", iTemplate);
	grcDebugDraw::Text(vJunctionPos+vTextRaise, Color_red, 0, 0, text);
	iScrY += grcDebugDraw::GetScreenSpaceTextHeight();

	// Print out some info about this junction if it is active
	int iJunctionIndex = CJunctions::GetJunctionUsingTemplate(iTemplate);
	if(iJunctionIndex != -1)
	{
		CJunction * pJunction = CJunctions::GetJunctionByIndex(iJunctionIndex);
		int iCurrPhase = pJunction->GetLightPhase();
		sprintf(text, "Current Light Phase : %i / %i", iCurrPhase, pJunction->GetNumLightPhases() );
		grcDebugDraw::Text(vJunctionPos+vTextRaise, Color_red, 0, iScrY, text);
		iScrY += grcDebugDraw::GetScreenSpaceTextHeight();

		sprintf(text, "Phase Time : %.1f / %.1f", pJunction->GetLightPhaseDuration(iCurrPhase)-pJunction->GetLightTimeRemaining(), pJunction->GetLightPhaseDuration(iCurrPhase));
		grcDebugDraw::Text(vJunctionPos+vTextRaise, Color_red, 0, iScrY, text);
		iScrY += grcDebugDraw::GetScreenSpaceTextHeight();

		sprintf(text, "Active entrances:");
		grcDebugDraw::Text(vJunctionPos+vTextRaise, Color_red, 0, iScrY, text);
		iScrY += grcDebugDraw::GetScreenSpaceTextHeight();

		int iActiveEntrances = 0;
		
		// Display all the currently active entrances
		for(int e=0; e<pJunction->GetNumEntrances(); e++)
		{
			CJunctionEntrance & entrance = pJunction->GetEntrance(e);
			if(entrance.m_iPhase == iCurrPhase && entrance.m_iLeftFilterPhase != iCurrPhase)
			{
				iActiveEntrances++;
				sprintf(text, "%i", e);
				grcDebugDraw::Text(vJunctionPos+vTextRaise, Color_red, 16, iScrY, text);
				iScrY += grcDebugDraw::GetScreenSpaceTextHeight();
			}

			if(entrance.m_iLeftFilterPhase == iCurrPhase)
			{
				iActiveEntrances++;
				sprintf(text, "%i (filter left)", e);
				grcDebugDraw::Text(vJunctionPos+vTextRaise, Color_red, 16, iScrY, text);
				iScrY += grcDebugDraw::GetScreenSpaceTextHeight();
			}
		}

		if(iActiveEntrances == 0)
		{
			sprintf(text, "none (peds will cross)");
			grcDebugDraw::Text(vJunctionPos+vTextRaise, Color_red, 16, iScrY, text);
			iScrY += grcDebugDraw::GetScreenSpaceTextHeight();
		}

		// Determine whether this junction has any light phases w/o any entrances assigned
		// This is the prerequisite for peds to cross

		int l,e;
		for(l=0; l<pJunction->GetNumLightPhases(); l++)
		{
			// Display all the currently active entrances
			for(e=0; e<pJunction->GetNumEntrances(); e++)
			{
				CJunctionEntrance & entrance = pJunction->GetEntrance(e);
				if(entrance.m_iPhase == l || entrance.m_iLeftFilterPhase == l)
					break;
			}
			if(e == pJunction->GetNumEntrances())	// Light phase 'l has no entrances assigned & will be used for peds
				break;
		}
		if(l==pJunction->GetNumLightPhases())
		{
			sprintf(text, "All light phases assigned - no phase for peds to cross!");
			grcDebugDraw::Text(vJunctionPos+vTextRaise, Color_red, 16, iScrY, text);
			iScrY += grcDebugDraw::GetScreenSpaceTextHeight();
		}

		if(pJunction->GetIsRailwayCrossing())
		{
			if(pJunction->GetRailwayBarriersShouldBeDown())
			{
				sprintf(text, "Railway crossing : train approaching!");
				grcDebugDraw::Text(vJunctionPos+vTextRaise, Color_red, 16, iScrY, text);
				iScrY += grcDebugDraw::GetScreenSpaceTextHeight();
			}
			else
			{
				sprintf(text, "Railway crossing : idle");
				grcDebugDraw::Text(vJunctionPos+vTextRaise, Color_red, 16, iScrY, text);
				iScrY += grcDebugDraw::GetScreenSpaceTextHeight();
			}
		}
	}
	else
	{
		sprintf(text, "Junction not created");
		grcDebugDraw::Text(vJunctionPos+vTextRaise, Color_red, 0, grcDebugDraw::GetScreenSpaceTextHeight(), text);
	}

	s32 iNumJunctionNodes = 0;
	const CPathNode * ppJunctionNodes[MAX_JUNCTION_NODES_PER_JUNCTION];
	GetJunctionsNodesLinkedToJunctionNode(pJunctionNode, ppJunctionNodes, MAX_JUNCTION_NODES_PER_JUNCTION, iNumJunctionNodes);

	for(s32 j=0; j<iNumJunctionNodes; j++)
	{
		ppJunctionNodes[j]->GetCoors(vJunctionPos);
		grcDebugDraw::Cylinder(RCC_VEC3V(vJunctionPos), VECTOR3_TO_VEC3V(vJunctionPos+vRaiseJunction), 1.0f, Color_cyan);
	}

	for(s32 e=0; e<temp.m_iNumEntrances; e++)
	{
		CJunctionTemplate::CEntrance & entrance = temp.m_Entrances[e];

		CNodeAddress iEntranceNode = ThePaths.FindNodeClosestToCoors(entrance.m_vNodePosition, PathfindFindNonJunctionNodeCB, NULL, 2.0f);
		Assert(!iEntranceNode.IsEmpty());

		const CPathNode * pEntranceNode = ThePaths.FindNodePointerSafe(iEntranceNode);
		Assert(pEntranceNode && !pEntranceNode->IsJunctionNode());

		if(pEntranceNode)
		{
			//-----------------------------------------------------------------------------------
			// Draw some info about the road node - num lanes, etc
			// This is so we can differentiate between nodes which might look almost on top of
			// each other due to the way the slip-lanes have to run coincident..

			int iNumLanesToJunction, iNumLanesFromJunction;
			GetNumLanesForEntranceNode(pEntranceNode, iNumLanesToJunction, iNumLanesFromJunction);

			Color32 iCylCol = (iNumLanesToJunction == 1 && iNumLanesFromJunction == 0) ? Color_cyan4 : Color_cyan;


			pEntranceNode->GetCoors(vLinkedNodePos);
			grcDebugDraw::Cylinder(RCC_VEC3V(entrance.m_vNodePosition), VECTOR3_TO_VEC3V(entrance.m_vNodePosition+vRaiseLink), 0.75f, iCylCol);

			sprintf(text, "Entrance (%i)", e);
			grcDebugDraw::Text(vLinkedNodePos+vTextRaise, Color_orange, 0, 0, text);


			sprintf(text, "NumLanesToJunction: %i", iNumLanesToJunction);
			grcDebugDraw::Text(vLinkedNodePos+vTextRaise, Color_orange2, 0, iTextHeight, text);

			sprintf(text, "NumLanesFromJunction: %i", iNumLanesFromJunction);
			grcDebugDraw::Text(vLinkedNodePos+vTextRaise, Color_orange2, 0, iTextHeight*2, text);


			sprintf(text, "Phase : %i", entrance.m_iPhase);
			grcDebugDraw::Text(vLinkedNodePos+vTextRaise, Color_orange, 0, iTextHeight*3, text);

			sprintf(text, "Filter Left on Phase: %i", entrance.m_iLeftFilterLanePhase);
			grcDebugDraw::Text(vLinkedNodePos+vTextRaise, Color_orange, 0, iTextHeight*4, text);

			sprintf(text, "Filter Right : %s", entrance.m_bCanTurnRightOnRedLight ? "true":"false");
			grcDebugDraw::Text(vLinkedNodePos+vTextRaise, Color_orange, 0, iTextHeight*5, text);

			sprintf(text, "Left Lane Ahead Only : %s", entrance.m_bLeftLaneIsAheadOnly ? "true":"false");
			grcDebugDraw::Text(vLinkedNodePos+vTextRaise, Color_orange, 0, iTextHeight*6, text);

			sprintf(text, "Right Lane Right Only : %s", entrance.m_bRightLaneIsRightOnly ? "true" : "false");
			grcDebugDraw::Text(vLinkedNodePos+vTextRaise, Color_orange, 0, iTextHeight*7, text);

			//----------------------------------------------------------------------
			// Draw the stopping lines
			// This is complicated because we want to use the orientation from the
			// previous node to each entrance node, approaching the junction.

			Vector3 vEntranceDir;
			vEntranceDir.x = -rage::Sinf(entrance.m_fOrientation);
			vEntranceDir.y = rage::Cosf(entrance.m_fOrientation);
			vEntranceDir.z = 0.0f;

			vEntranceDir.Normalize();
			Vector3 vRight;
			vRight.Cross(vEntranceDir, ZAXIS);
			vRight.Normalize();
			vRight *= 5.0f;
			const Vector3 vStopMid = vLinkedNodePos + (vEntranceDir * entrance.m_fStoppingDistance) + vRaiseStopLine;

			grcDebugDraw::Line(RCC_VEC3V(vStopMid), RCC_VEC3V(vLinkedNodePos), Color_orange2, Color_orange4);
			grcDebugDraw::Line(VECTOR3_TO_VEC3V(vStopMid - vRight), VECTOR3_TO_VEC3V(vStopMid + vRight), Color_orange2, Color_orange2);

			grcDebugDraw::Arrow(RCC_VEC3V(vStopMid), VECTOR3_TO_VEC3V(vStopMid + vEntranceDir), 0.25f, Color_orange);

		}
		else
		{
			grcDebugDraw::Cylinder(RCC_VEC3V(entrance.m_vNodePosition), VECTOR3_TO_VEC3V(entrance.m_vNodePosition+vRaiseLink), 0.75f, Color_red);
		}
	}

	pJunctionNode->GetCoors(vJunctionPos);
	grcDebugDraw::Sphere(vJunctionPos,temp.m_fSearchDistance,Color32(255,0,0,64),false);
	
	for(s32 l=0; l<temp.m_iNumTrafficLightLocations; l++)
	{
		Vector3 vPos;
		temp.m_TrafficLightLocations[l].GetAsVec3(vPos);
		Color32 col = (m_iCurrentTrafficLight == l) ? Color_LightSeaGreen : Color_SeaGreen4;
		grcDebugDraw::Sphere(vPos, 6.0f, col, false);
	}
}

//-----------------------------------------------------------------
// RenderJunctionNodes
// Draws nodes in the world which have the Junction flag set.
// These are the nodes which may be selected to form the central
// point of a hand-edited junction.

void CJunctionEditor::RenderJunctionNodes(const Vector3 & vOrigin)
{
	if(!m_bActivated)
		return;

	s32 r,n,l;
	Vector3 vNodePos, vLinkedNodePos;

	for(r=0; r<PATHFINDREGIONS; r++)
	{
		if(ThePaths.IsRegionLoaded(r))
		{
			const s32 iNumNodes = ThePaths.apRegions[r]->NumNodes;

			for(n=0; n<iNumNodes; n++)
			{
				CPathNode & node = ThePaths.apRegions[r]->aNodes[n];
				if(node.m_2.m_qualifiesAsJunction || m_bSelectAnyNodeAsJunction)
				{
					node.GetCoors(vNodePos);

					const float fDist2 = (vNodePos - vOrigin).Mag2();
					if(fDist2 < fMaxDistSqr)
					{
						// Is this the current junction?
						
						CJunctionTemplate * pCurrentTemplate = NULL;
						CJunctionTemplate * pTemplate = CJunctions::GetJunctionTemplateAtPosition(vNodePos);
						if(m_iCurrentJunction >= 0)
						{
							pCurrentTemplate = &CJunctions::GetJunctionTemplate(m_iCurrentJunction);
						}
						bool bCurrentJunction = pCurrentTemplate && (pCurrentTemplate==pTemplate);

						Color32 iCol = iJunctionCol;
						if(!bCurrentJunction)
							iCol.SetAlpha(128);
						
						grcDebugDraw::Cylinder(RCC_VEC3V(vNodePos), VECTOR3_TO_VEC3V(vNodePos+vRaiseJunction), 1.0f, iCol, false, false);

						for(l=0; l<node.NumLinks(); l++)
						{
							const CPathNode * pLinkedNode = ThePaths.GetNodesLinkedNode(&node, l);
							if(pLinkedNode)
							{
								pLinkedNode->GetCoors(vLinkedNodePos);

								if(!pLinkedNode->m_2.m_qualifiesAsJunction)
								{
									// Is this a slip lane?
									// Identifiable by having one lane to the junction, and none from
									// If so we'll colour it differently to differentiate
									iCol = iEntranceCol;

									const CPathNodeLink & link = ThePaths.GetNodesLink(&node, l);
									if(link.m_1.m_LanesToOtherNode == 1 && link.m_1.m_LanesFromOtherNode == 0)
									{
										iCol = iEntranceColSlipLane;
									}

									if(!bCurrentJunction)
										iCol.SetAlpha(128);

									grcDebugDraw::Cylinder(RCC_VEC3V(vLinkedNodePos), VECTOR3_TO_VEC3V(vLinkedNodePos+vRaiseLink), 0.75f, iCol, false, false);
								}

								iCol = iLinkCol;
								int iLinkAlpha = bCurrentJunction ? 64 : 32;
								iCol.SetAlpha(iLinkAlpha);

								grcDebugDraw::Cylinder(VECTOR3_TO_VEC3V(vNodePos+vRaiseJoin), VECTOR3_TO_VEC3V(vLinkedNodePos+vRaiseJoin), 0.25f, iCol, false, false, 6);
							}
						}
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------------
// Draw some info about the road node - num lanes, etc
// This is so we can differentiate between nodes which might look almost on top of
// each other due to the way the slip-lanes have to run coincident..

bool CJunctionEditor::GetNumLanesForEntranceNode(const CPathNode * pEntranceNode, int & iNumLanesToJunction, int & iNumLanesFromJunction)
{
	iNumLanesToJunction = 0;
	iNumLanesFromJunction = 0;

	if(pEntranceNode)
	{
		for(int l=0; l<pEntranceNode->NumLinks(); l++)
		{
			CPathNodeLink link = ThePaths.GetNodesLink(pEntranceNode, l);
			const CPathNode * pAdjNode = ThePaths.GetNodesLinkedNode(pEntranceNode, l);

			if(pAdjNode && pAdjNode->IsJunctionNode())
			{
				iNumLanesToJunction = link.m_1.m_LanesToOtherNode;
				iNumLanesFromJunction = link.m_1.m_LanesFromOtherNode;
				return true;
			}
		}
	}
	return false;
}

void CJunctionEditor::RenderTextHUD()
{
	char text[128];

	grcDebugDraw::Text( Vector2(0.05f,0.05f), Color_yellow, "Junction Editor", false, 4, 4);

	// Display which junction template is currently selected
	if(m_iCurrentJunction == -1)
	{
		sprintf(text, "Current template : none");
		return;
	}

	CJunctionTemplate & temp = CJunctions::GetJunctionTemplate(m_iCurrentJunction);

	if( (temp.m_iFlags & CJunctionTemplate::Flag_NonEmpty) == 0)
	{
		sprintf(text, "Current template : %i [EMPTY]", m_iCurrentJunction);
	}
	else if( temp.m_iNumJunctionNodes==0 || temp.m_vJunctionNodePositions[0].IsClose(g_vInvalidJunctionNodePosition, 0.1f) )
	{
		sprintf(text, "Current template : %i [JUNCTION NODE NOT YET SELECTED]", m_iCurrentJunction);
	}
	else
	{
		sprintf(text, "Current template : %i", m_iCurrentJunction);
	}

	grcDebugDraw::Text( Vector2(0.05f,0.1f), Color_yellow, text, false, 2, 2);

	float fScale = 2.0f;
	float fYPos = 0.15f;
	//float fTextHeight = (float)grcDebugDraw::GetScreenSpaceTextHeight() / (float)grcDevice::GetHeight();
	//float fYInc = fTextHeight * fScale;

	// Info specific to editing a junction
	if(m_iCurrentJunction != -1)
	{
		// If we are waiting to select a junction node
		if(m_iMode == Mode_SelectJunctionNode)
		{
			sprintf(text, "Select junction node (right-click to cancel)");
			grcDebugDraw::Text( Vector2(0.05f,fYPos), Color_green, text, false, fScale, fScale);
		}
		/*
		else if(m_iMode == Mode_SelectEntranceNode)
		{
			sprintf(text, "Select entrance node (right-click to cancel)");
			grcDebugDraw::Text( Vector2(0.05f,fYPos), Color_green, text, false, fScale, fScale);
		}
		*/

		if(m_iMode == Mode_Normal)
		{
			/*
			if(m_iCurrentEntrance != -1 && m_iCurrentEntrance < temp.m_iNumEntrances)
			{
				CJunctionTemplate::CEntrance & entrance = temp.m_Entrances[m_iCurrentEntrance];

				CNodeAddress iEntranceNode = ThePaths.FindNodeClosestToCoors(entrance.m_vNodePosition, PathfindFindNonJunctionNodeCB, NULL, 2.0f);
				Assert(!iEntranceNode.IsEmpty());

				CPathNode * pEntranceNode = ThePaths.FindNodePointerSafe(iEntranceNode);

				int iNumLanesToJunction, iNumLanesFromJunction;
				GetNumLanesForEntranceNode(pEntranceNode, iNumLanesToJunction, iNumLanesFromJunction);

				sprintf(text, "Current entrance : %i", m_iCurrentEntrance);
				grcDebugDraw::Text( Vector2(0.05f,fYPos), Color_green, text, false, fScale, fScale);
				fYPos += fYInc;


				sprintf(text, "NumLanesToJunction: %i", iNumLanesToJunction);
				grcDebugDraw::Text( Vector2(0.05f,fYPos), Color_green, text, false, fScale, fScale);
				fYPos += fYInc;

				sprintf(text, "NumLanesFromJunction: %i", iNumLanesFromJunction);
				grcDebugDraw::Text( Vector2(0.05f,fYPos), Color_green, text, false, fScale, fScale);
				fYPos += fYInc;

				fYPos += fYInc/2.0f;

				sprintf(text, "Main phase : %i", entrance.m_iPhase);
				grcDebugDraw::Text( Vector2(0.05f,fYPos), Color_green, text, false, fScale, fScale);
				fYPos += fYInc;

				sprintf(text, "Filter-left phase : %i", entrance.m_iLeftFilterLanePhase);
				grcDebugDraw::Text( Vector2(0.05f,fYPos), Color_green, text, false, fScale, fScale);
				fYPos += fYInc;

				sprintf(text, "Filter-right always : %s", entrance.m_bCanTurnRightOnRedLight ? "true" : "false");
				grcDebugDraw::Text( Vector2(0.05f,fYPos), Color_green, text, false, fScale, fScale);
				fYPos += fYInc;

				sprintf(text, "Left land ahead only : %s", entrance.m_bLeftLaneIsAheadOnly ? "true" : "false");
				grcDebugDraw::Text( Vector2(0.05f,fYPos), Color_green, text, false, fScale, fScale);
				fYPos += fYInc;
			}
			else
			{
				sprintf(text, "Current entrance : none");
				grcDebugDraw::Text( Vector2(0.05f,fYPos), Color_green, text, false, fScale, fScale);
				fYPos += fYInc;
			}
			*/
			
		}
	}

}

#endif	// __JUNCTION_EDITOR

