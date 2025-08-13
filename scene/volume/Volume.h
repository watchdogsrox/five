// Title	:	"scene/Volume.h"
// Author	:	Jason Jurecka
// Started	:	2/5/11
//

#ifndef _VOLUME_H_
#define _VOLUME_H_

#define VOLUME_SUPPORT 0

#if VOLUME_SUPPORT
#define VOLUME_SUPPORT_ONLY(x)	x

#include "entity\extensiblebase.h"
#include "math\random.h"
#include "vector\color32.h"

class CVolumeAggregate;

typedef	fwRegdRef<CVolume>			RegdVolume;
typedef	fwRegdRef<const CVolume>	RegdConstVolume;

class CVolume : public fwExtensibleBase
{
	DECLARE_RTTI_DERIVED_CLASS(CVolume, fwExtensibleBase);
public:
            CVolume() : m_bIsEnabled(true), m_Owner(NULL) {}
    virtual ~CVolume();

	inline void SetEnabled(bool bEnabled)	{ m_bIsEnabled = bEnabled; }
	inline bool GetEnabled() const			{ return m_bIsEnabled; }

    inline void                     SetOwner(CVolumeAggregate* pOwner) { Assertf(!m_Owner || !pOwner,"Volume %x already has an owner", this); m_Owner = pOwner; }
    inline CVolumeAggregate*        GetOwner()                         { return m_Owner; }
    inline const CVolumeAggregate*  GetOwner() const                   { return m_Owner; }

    virtual bool	IsAggregateVolume           ()const { return false;}
	virtual void	Scale						(float fScale) = 0;
	virtual void	GetBounds					(Vec3V_InOut rMax, Vec3V_InOut rMin) const = 0;
	virtual bool	GetPosition					(Vec3V_InOut position) const = 0;
	virtual bool    SetPosition					(Vec3V_In UNUSED_PARAM(rPoint)) { return false; }
	virtual bool	GetScale					(Vec3V_InOut UNUSED_PARAM(scale)) const { return false; }
	virtual bool    SetScale					(Vec3V_In UNUSED_PARAM(rScale)) { return false; }
	virtual bool    SetOrientation				(Mat33V_In UNUSED_PARAM(rOrient)) { return false; }
	virtual bool    GetOrientation				(Mat33V_InOut UNUSED_PARAM(rOrient)) const  { return false; }
    virtual bool	GetTransform                (Mat34V_InOut mtrxOut) const = 0;
	virtual bool	IsPointInside				(Vec3V_In ) const = 0;
    virtual bool	DoesRayIntersect			(Vec3V_In /*startPoint*/, Vec3V_In /*endPoint*/, Vec3V_InOut /*hitPoint*/) const = 0;
	virtual void	GenerateRandomPointInVolume	(Vec3V_InOut point, mthRandom &randGen) const = 0;
	virtual float	ComputeVolume				() const = 0;
	virtual bool	FindClosestPointInVolume    (Vec3V_In closestToPoint, Vec3V_InOut pointInVolumeOut) const = 0;

#if __BANK
	enum RenderMode
	{
		RENDER_MODE_WIREFRAME,
		RENDER_MODE_SOLID
	};
	virtual void RenderDebug(Color32 color, CVolume::RenderMode renderMode) = 0;
    virtual void ExportToFile       () const=0;
    virtual void CopyToClipboard    () const=0;

    //Editor Interface
    virtual void Translate          (Vec3V_In )=0;
    virtual void Scale              (Vec3V_In )=0;
    virtual void Rotate             (Mat33V_In rRotation)=0;
    virtual void RotateAboutPoint   (Mat33V_In rRotation, Vec3V_In rPoint)=0;
    //Editor Interface

    
#endif

private:

	bool m_bIsEnabled;
    CVolumeAggregate* m_Owner;
};

#else

#define VOLUME_SUPPORT_ONLY(x)

#endif // VOLUME_SUPPORT

#endif //_VOLUME_H_