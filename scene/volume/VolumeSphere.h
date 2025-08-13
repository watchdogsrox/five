// Title	:	"scene/volume/VolumeSphere.h"
// Author	:	Jason Jurecka
// Started	:	2/5/11
//
#ifndef _VOLUME_SPHERE_H_
#define _VOLUME_SPHERE_H_

#include "scene/volume/volume.h"

#if VOLUME_SUPPORT

#include "VolumePrimitiveBase.h"

class CVolumeSphere : public CVolumePrimitiveBase 
{
public:
	FW_REGISTER_CLASS_POOL(CVolumeSphere);
	DECLARE_RTTI_DERIVED_CLASS(CVolumeSphere, CVolumePrimitiveBase);
public:

    CVolumeSphere   (){}
    virtual ~CVolumeSphere  (){}

    //////////////////////////////////////////////////////////////////////////
    // CVolume
    bool    IsPointInside              (Vec3V_In rPoint) const;
    bool	DoesRayIntersect		   (Vec3V_In startPoint, Vec3V_In endPoint, Vec3V_InOut hitPoint) const;
    void    GenerateRandomPointInVolume(Vec3V_InOut point, mthRandom &randGen) const;
    float   ComputeVolume              () const;
    bool    FindClosestPointInVolume   (Vec3V_In closestToPoint, Vec3V_InOut pointInVolumeOut) const;
    void    GetBounds                  (Vec3V_InOut rMax, Vec3V_InOut rMin) const;
#if __BANK
    void RenderDebug        (Color32 color, CVolume::RenderMode renderMode);
    void ExportToFile       () const;
    void CopyToClipboard    () const;
#endif
    //////////////////////////////////////////////////////////////////////////

private:

    //////////////////////////////////////////////////////////////////////////
    // CVolumePrimitiveBase
    void GetBoundSphere (Vec3V_InOut rCentreOut, ScalarV_InOut rRadiusOut) const;
    //////////////////////////////////////////////////////////////////////////
};

#endif // VOLUME_SUPPORT

#endif // _VOLUME_SPHERE_H_