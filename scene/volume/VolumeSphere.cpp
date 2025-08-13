// Title	:	"scene/volume/VolumeSphere.h"
// Author	:	Jason Jurecka
// Started	:	2/5/11
//

#include "scene\volume\VolumeSphere.h"
#if VOLUME_SUPPORT

//rage includes
#include "math\amath.h"
#include "vector\geometry.h"
#include "vector\color32.h"
#include "grcore\debugdraw.h"
#include "grcore\im.h"
#include "file\stream.h"
#include "bank\bkmgr.h"

//framework includes

//game includes
#include "scene\volume\VolumeManager.h"
#include "scene\volume\VolumeTool.h"

FW_INSTANTIATE_CLASS_POOL(CVolumeSphere, 256, atHashString("VolumeSphere",0xa84af05c));
INSTANTIATE_RTTI_CLASS(CVolumeSphere,0xD52CDBA5)

void CVolumeSphere::GetBoundSphere(Vec3V_InOut rCentreOut, ScalarV_InOut rRadiusOut) const
{
    rCentreOut = m_Transform.GetCol3();

    // Used to be
	//	rRadiusOut = (m_Scale.GetX() > m_Scale.GetY()) ? (m_Scale.GetY() > transform->GetScaleZ() ? m_Scale.GetY() : transform->GetScaleZ()): (m_Scale.GetX() > transform->GetScaleZ() ? m_Scale.GetX() : transform->GetScaleZ());
	// that didn't actually work to get the largest component - the x/y check was reversed.
	// Max() is also nice for using fsel() instead of floating point compares. /FF
	rRadiusOut = MaxElement(m_Scale);
}

void CVolumeSphere::GetBounds(Vec3V_InOut rMax, Vec3V_InOut rMin) const
{
	// First, compute the forward transformation matrix.
	// Note: Alternatives here could be to either pay the cost of storing this,
	// or to invert m_MatrixInverse (probably slower than what we do now, but I
	// haven't checked). /FF
	Mat34V mtrx;
	ComputeNonInvertedMatrix(mtrx);

	// Project the axis of this matrix onto the axis of the world, to
	// find the side lengths of an axis-aligned box.
	Vec3V aAbs, bAbs, cAbs;
	aAbs = Abs(mtrx.GetCol0());
	bAbs = Abs(mtrx.GetCol1());
	cAbs = Abs(mtrx.GetCol2());
	Vec3V boxFromOobbSize;
	boxFromOobbSize = aAbs + bAbs;
	boxFromOobbSize += cAbs;

	// Compute an actual axis-aligned box, large enough to fully contain an
	// object-oriented box surrounding the volume. For box volumes, this is
	// an optimal AABB. /FF
	Vec3V center = mtrx.GetCol3();
	Vec3V boxFromOobbMin, boxFromOobbMax;
	boxFromOobbMax = center + boxFromOobbSize;
	boxFromOobbMin = center - boxFromOobbSize;

	// For cylinders and spheres, the above isn't generally optimal.
	// We can also compute an AABB using the bounding sphere produced
	// by the existing GetBoundSphere() function - in fact, that's
	// how this whole GetBounds() function used to work. /FF
	Vec3V boxFromSphereMax, boxFromSphereMin;
	ScalarV radius;
	GetBoundSphere(boxFromSphereMax, radius);

	// Compute the bounding box around the sphere. This is optimal
	// for perfect (uniformly scaled) spheres. /FF
	Vec3V tmp(radius);
	boxFromSphereMin = boxFromSphereMax - tmp;
	boxFromSphereMax += tmp;

	// Now, we've got two valid axis-aligned bounding boxes, and we
	// effectively use those to compute a box (as an intersection between
	// the two) that's potentially tighter than either one of them
	// by itself. /FF
	rMin = Max(boxFromOobbMin, boxFromSphereMin);
	rMax = Min(boxFromOobbMax, boxFromSphereMax);

	// Note: The box that we've now computed is not necessarily optimal
	// for cylinders and spheres, I think.
	// TODO: Consider working out all the math to compute fully optimal
	// bounding boxes for cylinders and spheres with arbitrary orientation/scale. /FF
}

bool CVolumeSphere::IsPointInside(Vec3V_In rPoint) const
{
	if (!GetEnabled())
		return false;

	Vec3V local = Transform(m_MatrixInverse, rPoint);
    if(IsGreaterThanAll(MagSquared(local), ScalarV(V_ONE)))
		return false;
	return true;
}

bool CVolumeSphere::DoesRayIntersect(Vec3V_In startPoint, Vec3V_In endPoint, Vec3V_InOut hitPoint) const
{
    if (!GetEnabled())
        return false;

    if (IsPointInside(startPoint) && IsPointInside(endPoint))
        return false;

    Vec3V center;
    ScalarV radius;
    GetBoundSphere(center, radius);
    ScalarV segmentT1;
    ScalarV unusedT2;

    if (geomSegments::SegmentToSphereIntersections(center, startPoint, endPoint, radius, segmentT1, unusedT2))
    {
        hitPoint = startPoint + ((endPoint - startPoint) * segmentT1);
        return true;
    }

    return false;
}

void CVolumeSphere::GenerateRandomPointInVolume(Vec3V_InOut point, mthRandom &randGen) const
{
    float localX, localY, localZ;
    // Keep generating random points within a cube until we get one within the
    // sphere. This shouldn't take many iterations on average, and ensures a
    // uniform distribution. /FF
    do
    {
        localX = randGen.GetFloat()*2.0f - 1.0f;
        localY = randGen.GetFloat()*2.0f - 1.0f;
        localZ = randGen.GetFloat()*2.0f - 1.0f;
    } while(square(localX) + square(localY) + square(localZ) >= 1.0f);

    Vec3V local(localX, localY, localZ);

    Mat34V mtrx;
    ComputeNonInvertedMatrix(mtrx);

    point = Transform(mtrx, local);
}

float CVolumeSphere::ComputeVolume() const
{
    ScalarV fourOVERthree = ScalarVFromF32(4.0f/3.0f);
    ScalarV temp = m_Scale.GetX() * m_Scale.GetY() * m_Scale.GetZ() * ScalarV(V_PI) * fourOVERthree;
    return temp.Getf();
}

bool CVolumeSphere::FindClosestPointInVolume(Vec3V_In closestToPoint, Vec3V_InOut pointInVolumeOut) const
{
    // Start off by computing local coordinates. Note that this is untransformed
    // by the orthonormal matrix, so it doesn't transform the ellipsoid into
    // a sphere (doing that and finding the closest point on the sphere would
    // have been simple, but when transformed back it wouldn't in general be
    // the closest point on the ellipsoid). /FF
    Vec3V preuvw = UnTransformFull(m_Transform, closestToPoint);

    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    if(CVolumeManager::FindClosestPointInEllipsoid (m_Scale.GetXf(), m_Scale.GetYf(), m_Scale.GetZf(),
                                                    preuvw.GetXf(), preuvw.GetYf(), preuvw.GetZf(), x, y, z))
    {
        pointInVolumeOut = Transform(m_Transform, Vec3V(x, y, z));
        return true;
    }
    return false;
}

#if __BANK
void CVolumeSphere::RenderDebug(Color32 color, CVolume::RenderMode renderMode)
{
    Mat34V mtrx;
    ComputeNonInvertedMatrix(mtrx);
	ScalarV one = ScalarV(V_ONE);
	bool solid = (renderMode == CVolume::RENDER_MODE_SOLID);
	grcDebugDraw::Sphere(one, mtrx, color, solid, 1, 12);
    g_VolumeManager.DrawVolumeAxis(mtrx);
}

void CVolumeSphere::ExportToFile () const
{
    //CVolumeTool::ms_ExportToFile is assumed to be a valid open fistream pointer at this point
    //CVolumeTool::ms_ExportCurVolIndex is used to keep variable names unique as we export
    Assert(CVolumeTool::ms_ExportToFile);

    Vec3V pos(V_ZERO);
    GetPosition(pos);
	Vec3V scale(V_ZERO);
	GetScale(scale);
    Mat33V orient(V_IDENTITY);
    GetOrientation(orient);
    Vec3V euler = Mat33VToEulersXYZ(orient);
    
    fprintf(CVolumeTool::ms_ExportToFile, "VOLUME scrVol%d = CREATE_VOLUME_SPHERE (<< %f, %f, %f >>, << %f, %f, %f >>, << %f, %f, %f >>)\n", 
                CVolumeTool::ms_ExportCurVolIndex,

                pos.GetXf(),
                pos.GetYf(),
                pos.GetZf(),

                //Orientation in Euler values
                euler.GetXf()*RtoD,
                euler.GetYf()*RtoD,
                euler.GetZf()*RtoD,

                //Scale
                scale.GetXf(),
                scale.GetYf(),
                scale.GetZf()
            );
    CVolumeTool::ms_ExportCurVolIndex++;
}

void CVolumeSphere::CopyToClipboard () const
{
    //CVolumeTool::ms_ExportCurVolIndex is used to keep variable names unique as we export
    char temp[RAGE_MAX_PATH];

    Vec3V pos(V_ZERO);
    GetPosition(pos);
	Vec3V scale(V_ZERO);
	GetScale(scale);
    Mat33V orient(V_IDENTITY);
    GetOrientation(orient);
    Vec3V euler = Mat33VToEulersXYZ(orient);

    formatf(temp, "VOLUME scrVol%d = CREATE_VOLUME_SPHERE (<< %f, %f, %f >>, << %f, %f, %f >>, << %f, %f, %f >>)\n", 
        CVolumeTool::ms_ExportCurVolIndex,

        pos.GetXf(),
        pos.GetYf(),
        pos.GetZf(),

        //Orientation in Euler values
        euler.GetXf()*RtoD,
        euler.GetYf()*RtoD,
        euler.GetZf()*RtoD,

        //Scale
        scale.GetXf(),
        scale.GetYf(),
        scale.GetZf()
        );

    BANKMGR.CopyTextToClipboard(temp);
    CVolumeTool::ms_ExportCurVolIndex++;
}

#endif

#endif // VOLUME_SUPPORT