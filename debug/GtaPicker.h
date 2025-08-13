/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    GtaPicker.h
// PURPOSE : wrapper for our three picker types - world probe, intersecting entities, entity render
// AUTHOR :  
// CREATED : 15/04/11
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _DEBUG_GTAPICKER_H_
#define _DEBUG_GTAPICKER_H_

#if __BANK

// Rage headers
#include "atl/array.h"


// Framework headers
#include "fwdebug/picker.h"
#include "fwdrawlist/drawlistmgr.h"
#include "entity/archetypemanager.h"

// Game headers
#include "modelinfo/BaseModelInfo.h"


// Forward declarations
namespace rage{
class bkSlider;
}

class CUiGadgetText;
class CUiGadgetSimpleListAndWindow;
class CUiGadgetInspector;


// Forward declarations
class CEntity;

namespace rage {
class strIndex;
}

// Typedefs
typedef void (*EntityDetailFunc)(CEntity*, atString&);


enum ePickerType
{
	INTERSECTING_ENTITY_PICKER = 1,	//	Might as well start this at 1 so it's easier to differentiate from an uninitialised fwBasePicker::m_PickerType
	WORLD_PROBE_PICKER,
	ENTITY_RENDER_PICKER,
	NUMBER_OF_PICKER_TYPES
};


/////////////////////////////////////////////////////////////////////////////////
//
// EntityDetailStruct - contains a name and a pointer to a function to display
//							one row of the detailed entity view
//
/////////////////////////////////////////////////////////////////////////////////
class EntityDetailStruct
{
public:
	EntityDetailStruct()
		: m_pTitle(NULL), m_pGetDetailFunction(NULL)
	{
	}

	EntityDetailStruct(const char *pTitle, EntityDetailFunc pGetDetailFunction)
		: m_pTitle(pTitle), m_pGetDetailFunction(pGetDetailFunction)
	{
	}

	const char *GetTitle() { return m_pTitle; }
	void GetValue(CEntity *pEntity, atString &ReturnString) { if (m_pGetDetailFunction) { (*m_pGetDetailFunction)(pEntity, ReturnString); } }

private:
	const char *m_pTitle;
	EntityDetailFunc m_pGetDetailFunction;
};

#define MAX_INSPECTOR_PARENT_TXD_CHAIN_LENGTH (5)
#define MAX_INSPECTOR_ROWS	(34 + MAX_INSPECTOR_PARENT_TXD_CHAIN_LENGTH)
#define MAX_MEMORY_INSPECTOR_ROWS (23)
typedef atFixedArray<EntityDetailStruct, MAX_INSPECTOR_ROWS> EntityDetailsArray;

/////////////////////////////////////////////////////////////////////////////////
//
// CGtaPickerInterfaceSettings - 
//
/////////////////////////////////////////////////////////////////////////////////
class CGtaPickerInterfaceSettings : public fwPickerInterfaceSettings
{
public :
	CGtaPickerInterfaceSettings(bool bShowOnlySelectedEntity = false, bool bDisplayBoundsOfSelectedEntity = true, bool bDisplayBoundsOfEntitiesInList = false, bool bDisplayBoundsOfHoverEntity = false);

	virtual ~CGtaPickerInterfaceSettings() {}

	bool m_bShowHideEnabled;
	ePickerShowHideMode m_ShowHideMode;
	bool m_bDisplayWireframe;
	bool m_bDisplayBoundsOfSelectedEntity;
	bool m_bDisplayBoundsOfEntitiesInList;
	bool m_bDisplayBoundsOfHoverEntity;
	bool m_bDisplayBoundsInWorldspace;
	bool m_bDisplayBoundsOfGeometries;
	bool m_bDisplayVisibilityBoundsForSelectedEntity;
	bool m_bDisplayVisibilitySphereForSelectedEntity;

	bool m_bDisplayPhysicalBoundsForNodeOfSelectedEntity;
	bool m_bDisplayStreamingBoundsForNodeOfSelectedEntity;


	bool m_bDisplayVisibilitykDOPTest;
	bool m_bDisplayScreenQuadForSelectedEntity;
	bool m_bDisplayOcclusionScreenQuadForSelectedEntity;
	u8 m_BoundingBoxOpacity;
};

/////////////////////////////////////////////////////////////////////////////////
//
// CGtaPickerInterface
//
/////////////////////////////////////////////////////////////////////////////////
class CGtaPickerInterface : public fwPickerInterface
{
public:
	CGtaPickerInterface();
	virtual ~CGtaPickerInterface() {}

	virtual void Init(unsigned initMode);
	virtual void Shutdown(unsigned shutdownMode);

	virtual void AddGeneralWidgets(bkBank* pWidgetBank);
	virtual void AddOptionsWidgets(bkBank *pWidgetBank);

	static bool IsEntitySelectRequired();
	virtual void OnWidgetChange();
	virtual void OnDisplayResultsWindow();

	virtual bool DoesUiHaveMouseFocus();

	virtual bool AllowMultipleSelection();

	virtual void Update();

	virtual void OutputResults();

	virtual void PickerHasNewController();

	virtual ePickerShowHideMode GetShowHideMode();

	//	When a system wants to take control of the picker, it can call this 
	//	to set the widget settings to suit its needs.
	//	This is optional.
	virtual void SetPickerSettings(fwPickerInterfaceSettings *pNewSettings);

	virtual bool IsValidEntityPtr(fwEntity* pToTest);

	u8 &GetReferenceToBoundingBoxOpacity() { return m_Settings.m_BoundingBoxOpacity; }
	bool &GetReferenceToShowOnlySelectedEntityFlag() { return m_Settings.m_bShowHideEnabled; }

	void AttachInspectorChild(CUiGadgetInspector* child);
	void DetachInspectorChild(CUiGadgetInspector* child);
	CUiGadgetInspector* GetCurrentInspector();
	CUiGadgetSimpleListAndWindow* GetMainWindow();

	static void GetObjectAndDependenciesSplitSizes(CEntity* ent, fwModelId modelId, const strIndex* ignoreList, s32 numIgnores,
												   u32& virtGeom, u32& physGeom, u32& virtTex, u32& physTex, u32& virtClip, u32& physClip,
												   u32& virtFrag, u32& physFrag, u32& virtTotal, u32& physTotal);


	CGtaPickerInterfaceSettings& GetSettings() { return m_Settings; }

	void ToggleWindowVisibility();
	void ResetWindowPosition();

private:
	static void SelectedEntityResultsCB(CUiGadgetText* pResult, u32 row, u32 col, void * extraCallbackData );
	static void PopulateMemoryInspectorCB(CUiGadgetText* pResult, u32 row, u32 col );

	static void DetailedEntityResultsCB(CUiGadgetText* pResult, u32 row, u32 col );

	static char *GetEntityDetailsString(const fwEntity *pEntity);

	void SetDefaultPickerSettings(bool bEnablePicker);
	void EnablePickerWithDefaultSettings();
#if DEBUG_ENTITY_GPU_COST
	void EnablePickEntityForGPUCost();
	void OnChangeGpuCostMode();
#endif

	void ShowUIGadgets();
	void HideUIGadgets();

	void InitDetailedView();
	void PushDetailedView(const char *pTitle, EntityDetailFunc pGetDetailFunction);

	void OnDisplayWireframeChange();

	void LodSettingsChanged();
	void UpdateLodSettings();

// Private data
	static EntityDetailsArray EntityDetails;	//	Had to make this static so that it can be used within the static
												//	DetailedEntityResultsCB function. There should only be one instance
												//	of CGtaPickerInterface in the game anyway.

	CGtaPickerInterfaceSettings m_Settings;

	atArray<CUiGadgetInspector*> m_inspectorList;
	s32 m_activeInspector;
	bool m_bHideWindowEvent;
	bool m_bShowWindowEvent;
	bool m_bResetWindowPos;

	//	This can be removed from here once Ian's UI code has moved to the framework
	CUiGadgetSimpleListAndWindow *m_pResultsWindow;
	CUiGadgetInspector *m_pInspectorWindow;
	CUiGadgetInspector *m_pMemoryWindow;


	float		m_highLod;
	float		m_medLod;
	float		m_lowLod;
	float		m_vlowLod;
	bkSlider*	m_sliderHighLod;
	bkSlider*	m_sliderMedLod;
	bkSlider*	m_sliderLowLod;
	bkSlider*	m_sliderVLowLod;
	fwEntity*	m_lastSelectedEntity;

#if DEBUG_ENTITY_GPU_COST
	bool		m_EnableGPUCostPicker;
	eDebugEntityGpuCostMode m_GPUCostMode;
	fwEntity*	m_pSelectedEntityGPUCostPicker;
#endif
};

strIndex EntityDetails_StreamingIndex(CBaseModelInfo* pBaseModelInfo);
void EntityDetails_GetModelName(CEntity *pEntity, atString &ReturnString);
void EntityDetails_GetPackfile(CEntity* pEntity, atString &ReturnString);
void EntityDetails_GetMountPoint(CEntity* pEntity, atString &ReturnString);

#endif //__BANK

#endif //_DEBUG_GTAPICKER_H_

