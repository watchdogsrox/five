// Title	:	"scene/volume/VolumeAggregate.h"
// Author	:	Jason Jurecka
// Started	:	2/5/11
//
#ifndef _VOLUME_AGGREGATE_H_
#define _VOLUME_AGGREGATE_H_

#include "Volume.h"

#if VOLUME_SUPPORT

class CVolumeAggregate : public CVolume 
{
	DECLARE_RTTI_DERIVED_CLASS(CVolumeAggregate, CVolume);
public:
    FW_REGISTER_CLASS_POOL(CVolumeAggregate);

            CVolumeAggregate () : m_Head(NULL), m_OwnedCount(0){}
    virtual ~CVolumeAggregate();// NOTE: DELETES ANY VOLUMES STILL IN THE LINKED LIST

    void Add		(CVolume* pVolume);
    void Remove		(CVolume* pVolume); // NOTE: DOES NOT DELETE THE VOLUME ONLY REMOVES FROM LINKED LIST
	void RemoveAll	(); // NOTE: DOES NOT DELETE THE VOLUMES ONLY REMOVES ALL FROM LINKED LIST

    //////////////////////////////////////////////////////////////////////////
    // CVolume
    bool    IsAggregateVolume          () const { return true; }
    void	Scale					   (float fScale);
    bool	GetPosition				   (Vec3V_InOut ) const;
    bool    GetTransform               (Mat34V_InOut rMatrix) const;
    bool    IsPointInside              (Vec3V_In rPoint) const;
    bool	DoesRayIntersect		   (Vec3V_In /*startPoint*/, Vec3V_In /*endPoint*/, Vec3V_InOut /*hitPoint*/) const { return false; }
    void    GenerateRandomPointInVolume(Vec3V_InOut point, mthRandom &randGen) const;
    float   ComputeVolume              () const;
    bool    FindClosestPointInVolume   (Vec3V_In closestToPoint, Vec3V_InOut pointInVolumeOut) const;
    void    GetBounds                  (Vec3V_InOut rMax, Vec3V_InOut rMin) const;

#if __BANK
    void RenderDebug        (Color32 color, CVolume::RenderMode renderMode);
    void ExportToFile       () const;
    void CopyToClipboard    () const;
    void Translate          (Vec3V_In rTrans);
    void Scale              (Vec3V_In rScale);
    void Rotate             (Mat33V_In rRotation);
    void RotateAboutPoint   (Mat33V_In rRotation, Vec3V_In rPoint);
    int  GetOwnedCount      () const { return m_OwnedCount; }
#endif
    //////////////////////////////////////////////////////////////////////////

private:
    struct LinkedListNode
    {
        CVolume*        m_Data;
        LinkedListNode* m_Next;
    };

    LinkedListNode* m_Head;
    int m_OwnedCount;
};

#endif // VOLUME_SUPPORT

#endif // _VOLUME_AGGREGATE_H_