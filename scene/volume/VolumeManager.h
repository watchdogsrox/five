/////////////////////////////////////////////////////////////////////////////////
// FILE :		VolumeManager.h
// PURPOSE :	The factory class for volumes
// AUTHOR :		Jason Jurecka.
// CREATED :	3/5/2011
/////////////////////////////////////////////////////////////////////////////////
#ifndef INC_VOLUMEMANAGER_H_
#define INC_VOLUMEMANAGER_H_

// rage includes
#include "vector/color32.h"

// game includes
#include "Scene/Volume/Volume.h"

#if VOLUME_SUPPORT

class CVolumeManager
{
public:
            CVolumeManager  ();
    virtual	~CVolumeManager (){}

    // SKELETON INTERFACE
    static void Init    (unsigned initMode);
    static void Shutdown(unsigned shutdownMode);
    static void Update  ();
    // SKELETON INTERFACE

    CVolume* CreateVolumeBox        (Vec3V_In rPosition, Mat33V_In rOrientation, Vec3V_In rScale);
    CVolume* CreateVolumeCylinder   (Vec3V_In rPosition, Mat33V_In rOrientation, Vec3V_In rScale);
    CVolume* CreateVolumeSphere     (Vec3V_In rPosition, Mat33V_In rOrientation, Vec3V_In rScale);
    CVolume* CreateVolumeAggregate  ();
    void     DeleteVolume           (CVolume* toDelete);

#if __BANK
    static void OpenVolumeTool ();
    static void CloseVolumeTool();
    static bool RenderDebugCB  (void* volume, void* data);

    void DrawVolumeAxis     (Mat34V_In mtrx) const;
    void InitWidgets        ();
    void OpenTool           ();
    void CloseTool          ();
    void RenderDebug        ();

    Color32 GetDrawColor        (const CVolume* volume) const;
    bool    ShouldRenderAsSolid () const { return m_renderSolidObjects; }
#endif

    static bool FindClosestPointInEllipse   (float prea, float preb,float preu, float prev, float &xout, float &yout);
    static bool FindClosestPointInEllipsoid (float prea, float preb, float prec,
                                             float preu, float prev, float prew, float &xout, float &yout, float &zout);
private:

#if __BANK
    bool m_renderAllDebug;
    bool m_renderBoxDebug;
    bool m_renderCylinderDebug;
    bool m_renderSphereDebug;
    bool m_renderAggregateDebug;
    bool m_renderSolidObjects;
    bool m_renderAxis;
    bool m_UseHighLightColor;

    Color32 m_DrawColor;
    Color32 m_AggregateColor;
    Color32 m_SelectedColor;
    Color32 m_HighlightColor;
    Color32 m_InActiveColor;

    float m_AxisSize;
#endif
};

///////////////////////////////////////////////////////////////////////////////
//	EXTERNS
///////////////////////////////////////////////////////////////////////////////

extern CVolumeManager g_VolumeManager;

#endif // VOLUME_SUPPORT

#endif // INC_VOLUMEMANAGER_H_
