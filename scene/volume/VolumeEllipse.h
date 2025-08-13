// Title	:	"scene/volume/VolumeEllipse.h"
// Author	:	Jason Jurecka
// Started	:	2/5/11
//
#ifndef _VOLUME_ELLIPSE_H_
#define _VOLUME_ELLIPSE_H_

#include "scene/volume/Volume.h"

#if  VOLUME_SUPPORT

#include "VolumePrimitiveBase.h"

class CVolumeEllipse : public CVolumePrimitiveBase 
{
public:

    FW_REGISTER_CLASS_POOL(CVolumeEllipse);

            CVolumeEllipse   (){}
    virtual ~CVolumeEllipse  (){}

    //////////////////////////////////////////////////////////////////////////
    // CVolume
    bool    IsPointInside              (Vec3V_In rPoint) const;
    bool	DoesRayIntersect		   (Vec3V_In startPoint, Vec3V_In endPoint, Vec3V_InOut hitPoint) const;
    void    GenerateRandomPointInVolume(Vec3V_InOut point, mthRandom &randGen) const;
    float   ComputeVolume              () const;
    bool    FindClosestPointInVolume   (Vec3V_In closestToPoint, Vec3V_InOut pointInVolumeOut) const;
    void    GetBounds                  (Vec3V_InOut rMax, Vec3V_InOut rMin) const;
#if __BANK
    void RenderDebug        ();
    void ExportToFile       () const;
    void CopyToClipboard    () const;
#endif
    //////////////////////////////////////////////////////////////////////////

#if __BANK
    static  bool RenderDebugCB  (CVolumeEllipse* volume, void* data);  
#endif

private:

    //////////////////////////////////////////////////////////////////////////
    // CVolumePrimitiveBase
    void GetBoundSphere (Vec3V_InOut rCentreOut, ScalarV_InOut rRadiusOut) const;
    //////////////////////////////////////////////////////////////////////////
};

#endif // VOLUME_SUPPORT

#endif // _VOLUME_ELLIPSE_H_