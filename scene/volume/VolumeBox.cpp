// Title	:	"scene/volume/VolumeBox.h"
// Author	:	Jason Jurecka
// Started	:	2/5/11
//

#include "scene/volume/Volume.h"

#if VOLUME_SUPPORT
//rage includes
#include "math\amath.h"
#include "vector\geometry.h"
#include "vector\color32.h"
#include "grcore\im.h"
#include "grcore\debugdraw.h"
#include "file\stream.h"
#include "bank\bkmgr.h"

//framework includes 

//game includes
#include "scene\volume\VolumeBox.h"
#include "scene\volume\VolumeManager.h"
#include "scene\volume\VolumeTool.h"

FW_INSTANTIATE_CLASS_POOL(CVolumeBox, 256, atHashString("VolumeBox",0xfec2408d));
INSTANTIATE_RTTI_CLASS(CVolumeBox,0x9C31C5B5)

void CVolumeBox::GetBoundSphere(Vec3V_InOut rCentreOut, ScalarV_InOut rRadiusOut) const
{
    rCentreOut = m_Transform.GetCol3();

    // This used to not have the 0.5 factor, but m_Scale is the box' full
	// side lengths, so we need to divide by two to get the distance from
	// the center to a corner. /FF
    rRadiusOut = Mag(m_Scale) * ScalarV(V_HALF);
}

void CVolumeBox::GetBounds(Vec3V_InOut rMax, Vec3V_InOut rMin) const
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

	// Our unit cube has a side length of 1, while a cube surrounding the unit
	// cylinder or sphere would have a side length of 2, which is why we need
	// to scale the box sides down. /FF
	boxFromOobbSize *= ScalarV(V_HALF);
	
	// Compute an actual axis-aligned box, large enough to fully contain an
	// object-oriented box surrounding the volume. For box volumes, this is
	// an optimal AABB. /FF
	Vec3V center = mtrx.GetCol3();
	Vec3V boxFromOobbMin, boxFromOobbMax;
	boxFromOobbMax = center + boxFromOobbSize;
	boxFromOobbMin = center - boxFromOobbSize;

	// In this case, just use the bounding box we've got. For box volumes
	// this is optimal. /FF
	rMin = boxFromOobbMin;
	rMax = boxFromOobbMax;
}

bool CVolumeBox::IsPointInside(Vec3V_In rPoint) const
{
	if (!GetEnabled())
		return false;

	Vec3V local = Transform(m_MatrixInverse, rPoint);
    ScalarV half(V_HALF);
    ScalarV neghalf(V_NEGHALF);
	
    // The sides are 1m in length (as opposed to going from -1 to 1)
	if(
        IsLessThanAll(local.GetX(), neghalf) || 
        IsGreaterThanAll(local.GetX(), half)
      )
		return false;
	if(
        IsLessThanAll(local.GetZ(), neghalf) || 
        IsGreaterThanAll(local.GetZ(), half)
      )
		return false;
	if(
        IsLessThanAll(local.GetY(), neghalf) || 
        IsGreaterThanAll(local.GetY(), half)
      )
		return false;
	return true;
}

bool CVolumeBox::DoesRayIntersect(Vec3V_In startPoint, Vec3V_In endPoint, Vec3V_InOut hitPoint) const
{
    if (!GetEnabled())
        return false;

    if (IsPointInside(startPoint) && IsPointInside(endPoint))
        return false;

    Vec3V localStart = Transform(m_MatrixInverse, startPoint);
    Vec3V localEnd = Transform(m_MatrixInverse, endPoint);
    Vec3V dir = localEnd - localStart;

    //Because box is of unit size half length is just half
    Vec3V boxHalfSize(V_HALF);
    
    ScalarV segmentT1;
    if (geomBoxes::TestSegmentToBox(localStart, dir, boxHalfSize, &segmentT1))
    {
        Vec3V hitLocal = localStart + (dir * segmentT1);

        Mat34V mtrx;
        ComputeNonInvertedMatrix(mtrx);

        hitPoint = Transform(mtrx, hitLocal);
        return true;
    }
    
    return false;
}

void CVolumeBox::GenerateRandomPointInVolume(Vec3V_InOut point, mthRandom &randGen) const
{
    float localX, localY, localZ;
    // Box coordinates are in [-0.5, 0.5] range, which we simply generate here. /FF
    localX = randGen.GetFloat() - 0.5f;
    localY = randGen.GetFloat() - 0.5f;
    localZ = randGen.GetFloat() - 0.5f;

    Vec3V local(localX, localY, localZ);

    Mat34V mtrx;
    ComputeNonInvertedMatrix(mtrx);

    point = Transform(mtrx, local);
}

float CVolumeBox::ComputeVolume() const
{
    ScalarV temp = m_Scale.GetX() * m_Scale.GetY() * m_Scale.GetZ();
    return temp.Getf();
}

bool CVolumeBox::FindClosestPointInVolume(Vec3V_In closestToPoint, Vec3V_InOut pointInVolumeOut) const
{
    // Start off by computing local coordinates. /FF
    Vec3V preuvw = UnTransformFull(m_Transform, closestToPoint);

    // To find the closest point, we can just clamp in each dimension. /FF
    Vec3V vHalfTransform = m_Scale * ScalarV(V_HALF);
    Vec3V clampedUVW = Clamp(preuvw, vHalfTransform * ScalarV(V_NEGONE), vHalfTransform);

    // Transform back to world space. /FF
    pointInVolumeOut = Transform(m_Transform, clampedUVW);
    return true;
}

#if __BANK
void CVolumeBox::RenderDebug(Color32 color, CVolume::RenderMode renderMode)
{
    Mat34V mtrx;
    ComputeNonInvertedMatrix(mtrx);
	bool solid = (renderMode == CVolume::RENDER_MODE_SOLID);
	Vec3V localMin = Vec3V(V_NEGHALF);
	Vec3V localMax = Vec3V(V_HALF);
	grcDebugDraw::BoxOriented(localMin, localMax, mtrx, color, solid);
    g_VolumeManager.DrawVolumeAxis(mtrx);
}

void CVolumeBox::ExportToFile () const
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

    fprintf(CVolumeTool::ms_ExportToFile, "VOLUME scrVol%d = CREATE_VOLUME_BOX (<< %f, %f, %f >>, << %f, %f, %f >>, << %f, %f, %f >>)\n", 
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

void CVolumeBox::CopyToClipboard () const
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

    formatf(temp, "VOLUME scrVol%d = CREATE_VOLUME_BOX (<< %f, %f, %f >>, << %f, %f, %f >>, << %f, %f, %f >>)\n", 
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