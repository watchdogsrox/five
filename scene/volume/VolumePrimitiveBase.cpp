#include "VolumePrimitiveBase.h"

#if VOLUME_SUPPORT

#include "math\amath.h"
#include "vector\geometry.h"
#include "vector\color32.h"
#include "grcore\im.h"

INSTANTIATE_RTTI_CLASS(CVolumePrimitiveBase,0x510B809D)

bool CVolumePrimitiveBase::SetScale (Vec3V_In rScale)
{	
    m_Scale = rScale; 
    UpdateVolume();  
	return true;
}

bool CVolumePrimitiveBase::SetTransform(Mat34V_In rMatrix)
{
    m_Transform = rMatrix;
    UpdateVolume();
    return true;
}

bool CVolumePrimitiveBase::GetTransform(Mat34V_InOut rMatrix) const
{
    rMatrix = m_Transform;
    return true;
}

void CVolumePrimitiveBase::Scale(float fScale)
{
    const float fkMinScale = 0.001f;
    Assertf(fScale > fkMinScale, "New scale for Volume %f > %f", fScale, fkMinScale);
    if (fScale > fkMinScale)
    {
        ScalarV tScale = ScalarVFromF32(fScale);
        m_Scale *= tScale;
        UpdateVolume();
    }
}

#if __BANK
void CVolumePrimitiveBase::Translate(Vec3V_In rTrans)
{
    Vec3V pos(V_ZERO);
    GetPosition(pos);
    SetPosition(pos + rTrans);
}

void CVolumePrimitiveBase::Scale (Vec3V_In rScale)
{
	Vec3V scale(V_ZERO);
	GetScale(scale);
    SetScale(scale + rScale);
}

void CVolumePrimitiveBase::Rotate (Mat33V_In rRotation)
{
    Mat33V rot(V_IDENTITY);
    GetOrientation(rot);
    Mat33V final(V_IDENTITY);
    Multiply(final, rot, rRotation);
    SetOrientation(final);
}

void CVolumePrimitiveBase::RotateAboutPoint (Mat33V_In rRotation, Vec3V_In rPoint)
{
    Vec3V pos(V_ZERO);
    GetPosition(pos);
    Vec3V offset = pos - rPoint;
    
    //rotate dir
    Vec3V newdir = Multiply(offset, rRotation);
    SetPosition(rPoint + newdir);
}

#endif

bool CVolumePrimitiveBase::SetPosition(Vec3V_In rPoint)
{
    m_Transform.SetCol3(rPoint);
    UpdateVolume();
    return true;
}

bool CVolumePrimitiveBase::SetOrientation(Mat33V_In rOrient)
{
    m_Transform.Set3x3(rOrient);
    UpdateVolume();
    return true;
}

bool CVolumePrimitiveBase::GetPosition(Vec3V_InOut rPoint) const
{
    rPoint = m_Transform.GetCol3();
    return true;
}

bool CVolumePrimitiveBase::GetScale(Vec3V_InOut rScale) const
{
	rScale = m_Scale;
	return true;
}

bool CVolumePrimitiveBase::GetOrientation(Mat33V_InOut rOrient) const
{
    rOrient = m_Transform.GetMat33();
    return true;
}

void CVolumePrimitiveBase::Init(Vec3V_In rPosition, Mat33V_In rOrientation, Vec3V_In rScale)
{
    m_Transform.Set3x3(rOrientation);
    m_Transform.Setd(rPosition);
    m_Scale = rScale;

    UpdateVolume();
	SetEnabled( true );
}

void CVolumePrimitiveBase::UpdateVolume()
{
	Mat34V mtrx;
	ComputeNonInvertedMatrix(mtrx);

    InvertTransformFull(m_MatrixInverse, mtrx);
}

void CVolumePrimitiveBase::ComputeNonInvertedMatrix(Mat34V_InOut mtrxOut) const
{
    Mat34V scaleMat;
    Mat34VFromScale(scaleMat, m_Scale);
    Mat33V orientMat = m_Transform.GetMat33();
    Mat34V oMat;
    oMat.Set3x3(orientMat);
    Transform(mtrxOut, oMat, scaleMat);
    mtrxOut.SetCol3(m_Transform.GetCol3());
}

void CVolumePrimitiveBase::ArePointsInside(bool* results, const Vec3V* pPoints, int numPoints) const
{
    if (!GetEnabled())
        return;

    while (numPoints--)
    {
        *results++ |= IsPointInside(*pPoints++);
    }
}

#endif // VOLUME_SUPPORT