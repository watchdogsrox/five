// Title	:	"scene/volume/VolumeCylinder.h"
// Author	:	Jason Jurecka
// Started	:	2/5/11
//

#include "scene\volume\VolumeCylinder.h"

#if VOLUME_SUPPORT

// rage includes
#include "math\amath.h"
#include "vector\geometry.h"
#include "vector\color32.h"
#include "grcore\debugdraw.h"
#include "grcore\debugdraw.h"
#include "grcore\im.h"
#include "file\stream.h"
#include "bank\bkmgr.h"

// framework includes

// game includes

#include "scene\volume\VolumeManager.h"
#include "scene\volume\VolumeTool.h"

FW_INSTANTIATE_CLASS_POOL(CVolumeCylinder, 256, atHashString("VolumeCylinder",0x9d200c95));
INSTANTIATE_RTTI_CLASS(CVolumeCylinder,0xA1D7A95F)

void CVolumeCylinder::GetBoundSphere(Vec3V_InOut rCentreOut, ScalarV_InOut rRadiusOut) const
{
    rCentreOut = m_Transform.GetCol3();
	
    // fTmp here used to be m_Scale.FlatMag(), but I guess we want the largest
	// component like this, similar to what we do for eSphere. /FF
	ScalarV temp = Max(m_Scale.GetX(), m_Scale.GetZ());
	rRadiusOut = Sqrt((temp*temp) + (m_Scale.GetY()*m_Scale.GetY()));
}

void CVolumeCylinder::GetBounds(Vec3V_InOut rMax, Vec3V_InOut rMin) const
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
	float radius;
	
    // Compute the bounding box around the sphere. This is optimal
	// for perfect (uniformly scaled) spheres. /FF
	Vec3V tmp(radius, radius, radius);
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

bool CVolumeCylinder::IsPointInside(Vec3V_In rPoint) const
{
	if (!GetEnabled())
		return false;

	Vec3V local = Transform(m_MatrixInverse, rPoint);
    ScalarV one(V_ONE);
    Vec3V squared = local * local;
    
	if(IsGreaterThanAll((squared.GetX() + squared.GetZ()), one))
		return false;

	// Although the Y axis test is quicker than the radius check above, it
	// probably makes sense to do the radius check first, since in most cases
	// that's the one that's going to reject most points, assuming that most
	// cylinder triggers are not rotated and we have a somewhat flat world.
	if(
        IsLessThanAll(local.GetY(), ScalarV(V_NEGONE)) ||
        IsGreaterThanAll(local.GetY(), one)
      )
		return false;

	return true;
}

bool CVolumeCylinder::DoesRayIntersect(Vec3V_In startPoint, Vec3V_In endPoint, Vec3V_InOut hitPoint) const
{
    if (!GetEnabled())
        return false;

    if (IsPointInside(startPoint) && IsPointInside(endPoint))
        return false;

    Vec3V localStart = Transform(m_MatrixInverse, startPoint);
    Vec3V localEnd = Transform(m_MatrixInverse, endPoint);
    Vec3V dir = localEnd - localStart;
    
    float segmentT1, segmentT2, cylinderT1, cylinderT2;
    //The cylinder is considered to extend from y=-length/2 to y=+length/2
    // for us that is -1.0 to 1.0.
    if (geomSegments::SegmentToUprightCylIsects(
            VEC3V_TO_VECTOR3(localStart),
            VEC3V_TO_VECTOR3(dir),
            2.0f, 
            1.0f,
            &segmentT1,
            &segmentT2,
            &cylinderT1,
            &cylinderT2)
        )
    {

        //Are we within the limits of the cylinders height
        //	- The cylinder t values are from 0 (meaning y=-length/2) to 1 (meaning y=+length/2).  They indicate the
        //	t value of the point on the cylinder's axis closest to the intersection point.
        if(cylinderT1 >= 0.0f && cylinderT1 <= 1.0f)
        {
            Vec3V hitLocal = localStart + (dir * ScalarV(segmentT1));

            Mat34V mtrx;
            ComputeNonInvertedMatrix(mtrx);

            hitPoint = Transform(mtrx, hitLocal);
            return true;
        }

        if (cylinderT2 >= 0.0f && cylinderT2 <= 1.0f)
        {
            Vec3V hitLocal = localStart + (dir * ScalarV(segmentT2));

            Mat34V mtrx;
            ComputeNonInvertedMatrix(mtrx);

            hitPoint = Transform(mtrx, hitLocal);
            return true;
        }
    }
 
    return false;
}

void CVolumeCylinder::GenerateRandomPointInVolume(Vec3V_InOut point, mthRandom &randGen) const
{
    float localX, localY, localZ;

    // Repeat until we get a point within a cylinder along the Y axis with radius 1.
    // IIRC, while some people may worry about the theoretically unlimited loop here,
    // this is the recommended way to generate a point within a circle according to
    // Numerical Recipes in C. /FF
    do
    {
        localX = randGen.GetFloat()*2.0f - 1.0f;
        localZ = randGen.GetFloat()*2.0f - 1.0f;
    } while(square(localX) + square(localZ) >= 1.0f);

    // Randomize the Y coordinate. /FF
    localY = randGen.GetFloat()*2.0f - 1.0f;

    Vec3V local(localX, localY, localZ);

    Mat34V mtrx;
    ComputeNonInvertedMatrix(mtrx);

    point = Transform(mtrx, local);
}

float CVolumeCylinder::ComputeVolume() const
{
    ScalarV temp = m_Scale.GetX() * m_Scale.GetY() * m_Scale.GetZ() * ScalarV(V_TWO) * ScalarV(V_PI);
    return temp.Getf();
}

bool CVolumeCylinder::FindClosestPointInVolume(Vec3V_In closestToPoint, Vec3V_InOut pointInVolumeOut) const
{
    // Start off by computing local coordinates. /FF
    Vec3V preuvw = UnTransformFull(m_Transform, closestToPoint);

    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    float sx = m_Scale.GetXf();
    float sy = m_Scale.GetYf();
    float sz = m_Scale.GetZf();

    // In the local XZ plane, find the closest point on the ellipse corresponding
    // to the transformed cylinder. /FF

    if(CVolumeManager::FindClosestPointInEllipse(sx, sz, preuvw.GetXf(), preuvw.GetZf(), x, z))
    {
        // For the local Y coordinate, we can just clamp by the length of the cylinder. /FF
        y = Clamp(preuvw.GetYf(), -sy, sy);

        // Transform back to world space. /FF
        pointInVolumeOut = Transform(m_Transform, Vec3V(x,y,z));

        return true;
    }
    return false;
}

#if __BANK
/// Note: there is no solid rendering of cylinders at this time, so the renderMode is simply ignored for now
void CVolumeCylinder::RenderDebug(Color32 color, CVolume::RenderMode UNUSED_PARAM(renderMode))
{
    Mat34V mtrx;
    ComputeNonInvertedMatrix(mtrx);
    // Used to be grcDrawCylinder(1.0f, 1.0f, mtrx, 12, true), but
    // grcDrawCylinder expects the full cylinder length, and for us that's
    // from -1.0 to 1.0. /FF
	grcDebugDraw::Cylinder(ScalarV(V_TWO), ScalarV(V_ONE), mtrx, color, true, 12);
	g_VolumeManager.DrawVolumeAxis(mtrx);
}

void CVolumeCylinder::ExportToFile () const
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

    fprintf(CVolumeTool::ms_ExportToFile, "VOLUME scrVol%d = CREATE_VOLUME_CYLINDER (<< %f, %f, %f >>, << %f, %f, %f >>, << %f, %f, %f >>)\n", 
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

void CVolumeCylinder::CopyToClipboard () const
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

    formatf(temp, "VOLUME scrVol%d = CREATE_VOLUME_CYLINDER (<< %f, %f, %f >>, << %f, %f, %f >>, << %f, %f, %f >>)\n", 
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