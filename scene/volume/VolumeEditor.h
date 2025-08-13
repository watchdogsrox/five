/////////////////////////////////////////////////////////////////////////////////
// FILE :		VolumeEditor.h
// PURPOSE :	An Editor for volume manipulation
// AUTHOR :		Jason Jurecka.
// CREATED :	10/21/2011
/////////////////////////////////////////////////////////////////////////////////
#ifndef INC_VOLUMEEDITOR_H_
#define INC_VOLUMEEDITOR_H_

#include "scene/volume/Volume.h"

#if VOLUME_SUPPORT

#if __BANK

// rage includes
#include "vectormath/vec3v.h"
#include "atl/delegate.h"
#include "system/criticalsection.h"

// framework includes
#include "fwscript/scripthandler.h"

// game includes
#include "Scene/Volume/Volume.h"
#include "tools/EditorWidget.h"

/////////////////////////////////////////////////////////////////////////////////
class CGameScriptHandler;

class CVolumeEditor
{
public:
    CVolumeEditor  ();
    virtual	~CVolumeEditor (){}

    void Init                           (CGameScriptHandler* handler);
    void Shutdown                       ();
    void Update                         ();
    void Render                         ();
    bool IsActive                       () const { return m_isActive; }
    void CreateWidgets                  (bkBank* pBank);
    void DeleteSelected                 ();
    void ClearSelected                  ();
    void AddSelectedToAggregate         (CVolume* aggregate=NULL); //Null will create a new aggregate and put all selected in it.
    void RemoveSelectedFromAggregates   ();
    void AddSelectedToSelectedAggregate ();
    void AddEditable                    (CVolume* volume, bool toolCreated = false);
    void GetSelectedList                (atMap<unsigned, const CVolume*>& toFill) const;

    //WIDGET statics
    static void CreateBox();
    static void CreateSphere();
    static void CreateCylinder();
	static void CreateAggregate();
    static void DeleteSelectedObjects();
    static void CreateAggregateWithSelected();
    static void ClearSelectedFromAggregates();
    static void MergeSelectedIntoAggregate();
    //WIDGET statics

    static bool AddScriptResourceToEditorCB (const scriptResource* resource, void* data);
	static void OnVolumeSnappingChangedCB(void *obj);
private:
	Vec3V_Out GetSnappedWorldPosition(Vec3V_In worldPos, ScalarV_In probeOffset);
	void OnVolumeSnappingChanged();
    void OnEditorWidgetEvent (const CEditorWidget::EditorWidgetEvent& _event);

    struct EditorObject
    {
        EditorObject()
        : m_Volume(NULL)
        , m_Next(NULL)
        , m_Prev(NULL)
        {}

        CVolume* m_Volume;
        EditorObject* m_Next;
        EditorObject* m_Prev;
    };

    EditorObject* m_EditorObjectsHead;
    EditorObject* m_HoveredObject;
    CGameScriptHandler* m_ScriptHandler;
    bool m_isActive;
    bool m_LocalRotations;
    bool m_TransWorldAligned;
    bool m_isHoveredOnText;
    bool m_ShowNameOfHovered;
	bool m_SnapVolumes;
    int m_prevMouseX;
    int m_prevMouseY;
    float m_scaleStep;
    float m_transStep;
    float m_rotateStep; //degrees
    atMap<unsigned, EditorObject*> m_SelectedObjects;
    CEditorWidget m_EditorWidget;
    CEditorWidget::Delegate m_EditorWidgetDelegate;
	sysCriticalSectionToken m_criticalSection;
};

///////////////////////////////////////////////////////////////////////////////
//	EXTERNS
///////////////////////////////////////////////////////////////////////////////

#endif // __BANK
#endif // INC_VOLUMEEDITOR_H_

#endif // VOLUME_SUPPORT