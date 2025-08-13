/////////////////////////////////////////////////////////////////////////////////
// FILE :		VolumeTool.h
// PURPOSE :	A tool for volume manipulation
// AUTHOR :		Jason Jurecka.
// CREATED :	10/21/2011
/////////////////////////////////////////////////////////////////////////////////
#ifndef INC_VOLUMETOOL_H_
#define INC_VOLUMETOOL_H_

#include "scene/volume/Volume.h"

#if __BANK && VOLUME_SUPPORT

// rage includes
#include "file/limits.h"

// framework includes
#include "fwscript/scripthandler.h"

// game includes
#include "Scene/Volume/VolumeEditor.h"

class CVolumeTool
{
public:
    CVolumeTool  ();
    virtual	~CVolumeTool (){}

    enum eRenderStyle
    {
        ersNormal,
        ersHighlight,
        ersSelected,
        ersInActive,
    };

    void Init    ();
    void Shutdown();
    void Update  ();
    void Render  ();
    bool IsOpen  () const { return m_isOpen; }

    CVolumeTool::eRenderStyle GetRenderStyle() const                        { return m_RenderStyle; }
    void                      SetRenderStyle(CVolumeTool::eRenderStyle rs)  { m_RenderStyle = rs; }

    CVolumeEditor&       GetEditor()        { return m_Editor; }
    const CVolumeEditor& GetEditor() const  { return m_Editor; }

    void CreateWidgets                  (bkBank* pBank);
    void EnterEditMode                  ();
    void ExitEditMode                   ();
    void ExportSelectedScript           ();
    void ExportSelectedScriptToClipboard();
    void ExportSelected                 ();
    void CopySelectedToClipboard        ();

    static bool RenderCB                    (const scriptResource* resource, void* data);
    static bool ExportScriptVolumeCB        (const scriptResource* resource, void* data);
    static bool CopyScriptVolumeClipboardCB (const scriptResource* resource, void* data);

    static void StartEditMode           ();
    static void EndEditMode             ();
    static void ExportScript            ();
    static void CopyScriptToClipboard   ();
    static void ExportSel               ();
    static void CopySelToClipboard      ();

    //Used when writing out ... 
    static fiStream* ms_ExportToFile;
    static unsigned  ms_ExportCurVolIndex;

private:
    char m_ExportFileName[RAGE_MAX_PATH];
    bkBank* m_ParentBank;
    bool m_isOpen;
    unsigned m_SelectedFilter;
    CVolumeTool::eRenderStyle m_RenderStyle;
    CVolumeEditor m_Editor;
};

///////////////////////////////////////////////////////////////////////////////
//	EXTERNS
///////////////////////////////////////////////////////////////////////////////

extern CVolumeTool g_VolumeTool;
#endif // __BANK && VOLUME_SUPPORT

#endif // INC_VOLUMETOOL_H_
