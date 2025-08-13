#ifndef JUNCTION_EDITOR_H
#define JUNCTION_EDITOR_H

#include "vehicleAi/junctions.h"

#if __JUNCTION_EDITOR

#include "atl\array.h"
#include "bank\bank.h"

class CJunctionEditor
{
public:
	CJunctionEditor();
	~CJunctionEditor();

	enum
	{
		Mode_Normal,
		Mode_SelectJunctionNode,
		Mode_SelectTrafficLightLocation
		//Mode_SelectEntranceNode
	};
	struct EntranceBankVars
	{
		bkGroup * m_pGroup;
		bkSlider * m_pPhaseSlider;
		bkSlider * m_pStoppingLineSlider;
		bkSlider * m_pOrientationSlider;
		bkSlider * m_pLeftFilterPhaseSlider;
		bkToggle * m_pLeftLaneAheadOnlyToggle;
		bkToggle * m_pCanTurnRightToggle;
		bkToggle * m_pRightLaneIsRightOnlyToggle;
	};
	struct PhaseVars
	{
		bkGroup * m_pGroup;
		bkSlider * m_pTime;
	};

	static void Init();
	static void ToggleEditor();
	static void InitWidgets(bool bFirstTime);
	static void SetInitialNodesDebug();
	static bkBank * GetBank();

	static void NextJunction();
	static void PrevJunction();
	static void InitJunctionWidgets();

	static void NewJunction();
	static void DeleteJunction();

	static void StartSelectJunctionNode();
	//static void StartSelectEntranceNode();
	static void SelectAndRebindJunctionAndEntrances();
	static void OnSelectJunctionNode(const CNodeAddress & iNodeAddress);
	static void UpdateSelectNode();
	static void UpdateSelectTrafficLightLocation();
	static void SetCurrentTrafficLightText();
	static void OnUseJunctionSlider();
	static void OnToggleIsRailwayCrossing();
	static void OnToggleDisableSkipPedPhase();
	static void OnChangePhaseOffset();
	static void OnSetNumPhases();
	static void AddNewPhase();
	static void FindOrCreateAutoAdjustment();
	static void RemoveAutoAdjustment();
	static void OnUpdateAutoAdjustment();

	static void OnChangeCurrentTrafficLight();
	static void OnPickTrafficLightLocation();
	static void OnClearTrafficLight();

	static void WarpToCurrentJunctionTemplate();

	static void Update();

	static bool GetNumLanesForEntranceNode(const CPathNode * pEntranceNode, int & iNumLanesToJunction, int & iNumLanesFromJunction);

	static void Render();
	static void RenderJunctionTemplate(const Vector3 & vOrigin, const s32 iTemplate);
	static void RenderJunctionNodes(const Vector3 & vOrigin);

	static void RenderTextHUD();

	static bool m_bSelectAnyNodeAsJunction;

protected:

	static bool m_bInitialised;
	static bool m_bActivated;
	static bool m_bJustActivated;
	static bool m_bShowEditor;
	static bool m_bToggleActivation;

	static bkWidget * m_pActivateButton;
	static bkWidget * m_pMainGroup;
	static bkWidget * m_pCurrentJunctionGroup;
	static bkSlider * m_pCurrentJunctionSlider;
	static bkSlider * m_pCurrentEntranceSlider;
	static bkSlider * m_pNumPhasesSlider;
	static bkGroup * m_pEntrancesGroup;
	static bkGroup * m_pPhasesGroup;

	static bkGroup * m_pTrafficLightLocationsGroup;
	static bkSlider * m_pTrafficLightSearchDistanceSlider;
	static bkSlider * m_pTrafficLightIndexSlider;
	static bkText * m_pNumTrafficLightsText;
	static bkText * m_pTrafficLightText;
	static bkButton * m_pTrafficLightPickButton;
	static bkButton * m_pTrafficLightClearButton;

	static EntranceBankVars m_EntranceBankVars[MAX_ROADS_INTO_JUNCTION];
	static PhaseVars m_PhaseVars[MAX_ROADS_INTO_JUNCTION];


	//-------------------------------------------------

	static s32 m_iMode;
	static bool m_bRebindEntrances;
	static bool m_bMustReinitialiseWidgets;

	static s32 m_iNumJunctions;
	static s32 m_iCurrentJunction;
	static s32 m_iSecondaryJunction;
	static s32 m_iCurrentTrafficLight;
	static char m_NumTrafficLightsText[64];
	static char m_CurrentTrafficLightText[256];

	static bool m_iCurrentJunctionIsRailwayCrossing;
	static bool m_iCurrentJunctionCanSkipPedPhase;
	static float m_fCurrentJunctionPhaseOffset;

	static s32 m_iDesiredJunction;
	static s32 m_iDesiredEntrance;

	static s32 m_iNumPhases;

	static CAutoJunctionAdjustment m_CurrentAutoJuncAdj;
};

#endif // __JUNCTION_EDITOR

#endif // JUNCTION_EDITOR_H
