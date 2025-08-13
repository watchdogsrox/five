// Title	:	"scene/volume/VolumePrimitiveBase.h"
// Author	:	Jason Jurecka
// Started	:	2/5/11
//

#ifndef _VOLUME_SIMPLE_H_
#define _VOLUME_SIMPLE_H_

#include "Volume.h"
#if VOLUME_SUPPORT

class CVolumePrimitiveBase : public CVolume 
{
    DECLARE_RTTI_DERIVED_CLASS(CVolumePrimitiveBase, CVolume);
public:

	CVolumePrimitiveBase   (){}
    virtual ~CVolumePrimitiveBase  (){}

    void Init           (Vec3V_In rPosition, Mat33V_In rOrientation, Vec3V_In rScale);
    void UpdateVolume   ();
    void ArePointsInside(bool* results, const Vec3V* pPoints, int numPoints) const;

    //////////////////////////////////////////////////////////////////////////
    // CVolume
    void	Scale			(float fScale);
    bool	GetPosition		(Vec3V_InOut rPoint) const;
	bool    SetPosition		(Vec3V_In rPoint);
	bool	GetScale		(Vec3V_InOut scale) const;
	bool    SetScale		(Vec3V_In rScale);
	bool    SetOrientation	(Mat33V_In rOrient);
	bool    GetOrientation	(Mat33V_InOut rOrient) const;
    bool	GetTransform	(Mat34V_InOut rMatrix) const;
    bool	SetTransform	(Mat34V_In  rMatrix);

#if __BANK
    void Translate          (Vec3V_In rTrans);
    void Scale              (Vec3V_In rScale);
    void Rotate             (Mat33V_In rRotation);
    void RotateAboutPoint   (Mat33V_In rRotation, Vec3V_In rPoint);
#endif
    //////////////////////////////////////////////////////////////////////////

    virtual void GetBoundSphere (Vec3V_InOut rCentreOut, ScalarV_InOut rRadiusOut) const = 0;

protected:

    // PURPOSE:	Compute the full transform matrix between the "unit volume"
    //			and the actual transformed volume in world space. This is
    //			the matrix which m_MatrixInverse is the inverse of.
    // PARAMS:	mtrxOut		- Output matrix.
    void ComputeNonInvertedMatrix(Mat34V_InOut mtrxOut) const;
    
    inline  Mat34V_Out    GetInverseMatrix   () const                { return m_MatrixInverse;}

    Mat34V m_MatrixInverse;
    Mat34V m_Transform;
    Vec3V  m_Scale;
};

#endif // VOLUME_SUPPORT

#endif // _VOLUME_SIMPLE_H_