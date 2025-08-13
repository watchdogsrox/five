// Class header
#include "Task/Movement/Climbing/ClimbHandHold.h"

#include "ai/task/taskchannel.h"
#include "grcore/debugdraw.h"

// Game headers
#include "Camera/CamInterface.h"
#include "Camera/Helpers/Frame.h"

#include "Scene/Physical.h"

#include "Vfx/VehicleGlass/VehicleGlassManager.h"

#include "vehicles/wheel.h"
#include "fragment/instance.h"
#include "renderer/HierarchyIds.h"
#include "vehicles/vehicle.h" 

AI_OPTIMISATIONS()
AI_MOVEMENT_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CClimbHandHold
//////////////////////////////////////////////////////////////////////////

CClimbHandHold::CClimbHandHold()
: m_vPoint(Vector3::ZeroType)
, m_vNormal(Vector3::ZeroType)
, m_pEntity(NULL)
{
}

CClimbHandHold::CClimbHandHold(const Vector3& vPoint, const Vector3& vNormal, const CEntity* pEntity)
{
	Set(vPoint, vNormal, pEntity);
}

CClimbHandHold::CClimbHandHold(const CClimbHandHold& other)
: m_vPoint(other.m_vPoint)
, m_vNormal(other.m_vNormal)
, m_pEntity(other.m_pEntity)
{
}

void CClimbHandHold::Set(const Vector3& vPoint, const Vector3& vNormal, const CEntity* pEntity)
{
	m_vPoint = vPoint;
	m_vNormal = vNormal;
	m_pEntity = pEntity;
}

#if __BANK
void CClimbHandHold::Render(const Color32& UNUSED_PARAM(colour)) const
{
}
#endif


////////////////////////////////////////////////////////////////////////////////
// 	CClimbHandHoldDetected
////////////////////////////////////////////////////////////////////////////////

FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CClimbHandHoldDetected, CONFIGURED_FROM_FILE, 0.66f, atHashString("CClimbHandHoldDetected",0x293949c9));

CClimbHandHoldDetected::CClimbHandHoldDetected()
: m_fHeight(0.0f)
, m_fDepth(0.0f)
, m_fDrop(0.0f)
, m_fHorizontalClearance(0.0f)
, m_fVerticalClearance(0.0f)
, m_pPhysical(NULL)
, m_pTopSurfaceMaterial(NULL)
, m_pFrontSurfaceMaterial(NULL)
, m_FrontSurfaceMaterialId(0)
, m_vPhysicalHandHoldOffset(Vector3::ZeroType)
, m_vPhysicalHandHoldNormal(Vector3::ZeroType)
, m_vGroundPositionAtDrop(Vector3::ZeroType)
, m_vMaxGroundPosition(Vector3::ZeroType)
, m_vGroundNormalAVG(ZAXIS)
, m_vAlignDirection(YAXIS)
, m_fClimbAngle(0.0f)
, m_fHandHoldGroundAngle(0.0f)
, m_bIsMaxDrop(false)
, m_bIsMaxHorizontalClearance(false)
, m_fCloneTestHeading(0.0f)
, m_bCloneUseTestDirection(false)
, m_uPhysicalComponent(0)
, m_uCloneMinNumDepthTests(0)
, m_vCloneIntersection(Vector3::ZeroType)
{
}

void CClimbHandHoldDetected::Reset()
{
	m_fHeight = 0.0f;
	m_fDepth = 0.0f;
	m_fDrop = 0.0f;
	m_fHorizontalClearance = 0.0f;
	m_fVerticalClearance = 0.0f;
	m_pPhysical = NULL;
	m_pTopSurfaceMaterial = NULL;
	m_pFrontSurfaceMaterial = NULL;
	m_FrontSurfaceMaterialId = 0;
	m_vPhysicalHandHoldOffset = Vector3::ZeroType;
	m_vPhysicalHandHoldNormal = Vector3::ZeroType;
	m_vGroundPositionAtDrop = Vector3::ZeroType;
	m_vMaxGroundPosition = Vector3::ZeroType;
	m_vGroundNormalAVG = ZAXIS;
	m_vAlignDirection = YAXIS;
	m_fClimbAngle = 0.0f;
	m_fHandHoldGroundAngle = 0.0f;
	m_bIsMaxDrop = false;
	m_bIsMaxHorizontalClearance = false;

	m_fCloneTestHeading = 0.0f;
	m_bCloneUseTestDirection = false;
	m_vCloneIntersection = Vector3::ZeroType;

	m_uPhysicalComponent = 0;
	m_uCloneMinNumDepthTests = 0;
}

s16 CClimbHandHoldDetected::GetBoneFromHitComponent() const
{
	return GetBoneFromHitComponent(m_pPhysical, m_uPhysicalComponent);
}

const CWheel * GetCollidedWheelFromCollisionComponent(const CVehicle * pVehicle, const int iHitComponent)
{
	static const int nNumWheels = VEH_WHEEL_LAST_WHEEL-VEH_WHEEL_FIRST_WHEEL;
	int d;
	int iWheelComponentIds[nNumWheels];

	for(d=0; d<nNumWheels; d++)
	{
		iWheelComponentIds[d] = pVehicle->GetFragInst()->GetComponentFromBoneIndex(pVehicle->GetBoneIndex( (eHierarchyId)(VEH_WHEEL_FIRST_WHEEL+d) ));
	}

	for(d=0; d<nNumWheels; d++)
	{
		if(iHitComponent==iWheelComponentIds[d])
		{
			const CWheel * pWheel = pVehicle->GetWheelFromId( (eHierarchyId)(VEH_WHEEL_FIRST_WHEEL+d));
			return pWheel;
		}
	}

	return NULL;
}

s16 CClimbHandHoldDetected::GetBoneFromHitComponent(const CPhysical* pPhysical, u16 uPhysicalComponent)
{
	s16 iHitBone = -1;

	TUNE_GROUP_BOOL(VAULTING,bAttachToChildren,true);
	if(pPhysical && pPhysical->GetIsTypeVehicle() && bAttachToChildren)
	{
		fragInst* pFragInst = pPhysical->GetFragInst();
		if(pFragInst && pFragInst->GetCached())
		{
			Assert(uPhysicalComponent < pFragInst->GetTypePhysics()->GetNumChildren());

			//! Don't climb on wheel bones. 
			const CVehicle *pVehicle = static_cast<const CVehicle*>(pPhysical);
			if(GetCollidedWheelFromCollisionComponent(pVehicle, uPhysicalComponent))
			{
				iHitBone = -1;
			}
			else
			{
				fragTypeChild* pChild = pFragInst->GetTypePhysics()->GetAllChildren()[uPhysicalComponent];
				iHitBone = (s16) pFragInst->GetType()->GetBoneIndexFromID((s16)pChild->GetBoneID());

				// If we're hitting vehicle glass we want to attach to the parent bone, otherwise we will lose the attachment
				// as soon as the window cracks since the bone matrix gets zeroed out
				// NOTE: We probably want to do this on non-vehicles too
				if(CVehicleGlassManager::IsComponentSmashableGlass(static_cast<const CPhysical*>(pPhysical),uPhysicalComponent))
				{
					s16 parentBone = (s16)pFragInst->GetType()->GetSkeletonData().GetParentIndex(iHitBone);
					if(parentBone >= 0)
					{
						iHitBone = parentBone;
					}
				}
			}
		}
	}

	return iHitBone;
}

void CClimbHandHoldDetected::GetPhysicalMatrix(Matrix34 &physicalMatrix) const
{
	GetPhysicalMatrix(physicalMatrix, m_pPhysical, m_uPhysicalComponent);
}

void CClimbHandHoldDetected::GetPhysicalMatrix(Matrix34 &physicalMatrix, const CPhysical* pPhysical, u16 uPhysicalComponent)
{
	if(pPhysical)
	{
		s16 iHitBone = GetBoneFromHitComponent(pPhysical, uPhysicalComponent);

		if(pPhysical->GetSkeleton() && (iHitBone >= 0))
		{
			pPhysical->GetGlobalMtx(iHitBone, physicalMatrix);
		}
		else
		{
			physicalMatrix.Set(MAT34V_TO_MATRIX34(pPhysical->GetMatrix()));
		}
	}
}

Vector3 CClimbHandHoldDetected::GetPositionRelativeToPhysical(const Vector3 &vPosition) const 
{ 
	Vector3 vTemp = vPosition;
	if(m_pPhysical)
	{
		Matrix34 m;
		GetPhysicalMatrix(m);
		m.Transform(vTemp);
	}

	return vTemp;
}

Vector3 CClimbHandHoldDetected::GetDirectionRelativeToPhysical(const Vector3 &vDirection) const 
{ 
	Vector3 vTemp = vDirection;
	if(m_pPhysical)
	{
		Matrix34 m;
		GetPhysicalMatrix(m);
		m.Transform3x3(vTemp);
	}

	return vTemp;
}

void CClimbHandHoldDetected::CalcPhysicalHandHoldOffset()
{
	if(m_pPhysical)
	{
		Matrix34 m;
		GetPhysicalMatrix(m);

		//! Quick fix up if matrix is invalid. For some reason, we get invalid rotations in our transforms. E.g. when 
		//! climbing on a Modded car.
		if(!m.IsOrthonormal(0.05f))
		{
			m.Set(MAT34V_TO_MATRIX34(m_pPhysical->GetMatrix()));
			m_uPhysicalComponent=0;
		}

		m_vPhysicalHandHoldOffset = GetHandHold().GetPoint();
		m_vPhysicalHandHoldNormal = GetHandHold().GetNormal();

		m.UnTransform(m_vPhysicalHandHoldOffset);
		m.UnTransform3x3(m_vPhysicalHandHoldNormal);
		m.UnTransform3x3(m_vAlignDirection);
	}
}

void CClimbHandHoldDetected::SetGroundPositionAtDrop(const Vector3 &vPosition)
{
	m_vGroundPositionAtDrop = vPosition;

	if(m_pPhysical)
	{
		Matrix34 m;
		GetPhysicalMatrix(m);
		m.UnTransform(m_vGroundPositionAtDrop);
	}
}

void CClimbHandHoldDetected::SetMaxGroundPosition(const Vector3 &vPosition)
{
	m_vMaxGroundPosition = vPosition;

	if(m_pPhysical)
	{
		Matrix34 m;
		GetPhysicalMatrix(m);
		m.UnTransform(m_vMaxGroundPosition);
	}
}

Vector3 CClimbHandHoldDetected::GetHandholdPosition() const 
{ 
	return m_pPhysical ? GetPositionRelativeToPhysical(m_vPhysicalHandHoldOffset) : GetHandHold().GetPoint();
}

Vector3 CClimbHandHoldDetected::GetHandholdNormal() const 
{ 
	return m_pPhysical ? GetDirectionRelativeToPhysical(m_vPhysicalHandHoldNormal) : GetHandHold().GetNormal();
}

Vector3 CClimbHandHoldDetected::GetAlignDirection() const
{ 
	return m_pPhysical ? GetDirectionRelativeToPhysical(m_vAlignDirection) : m_vAlignDirection;
}

Vector3 CClimbHandHoldDetected::GetGroundPositionAtDrop() const 
{ 
	return GetPositionRelativeToPhysical(m_vGroundPositionAtDrop);
}

Vector3 CClimbHandHoldDetected::GetMaxGroundPosition() const 
{ 
	return GetPositionRelativeToPhysical(m_vMaxGroundPosition);
}

#if __BANK
void CClimbHandHoldDetected::Render(const Color32& colour) const
{
	m_handHold.Render(colour);

	const Vector3& vPoint = m_handHold.GetPoint();
	const Vector3& vNormal = m_handHold.GetNormal();
	char buff[16];

	// Height
	Vector3 vHeightStart(vPoint);
	vHeightStart += vNormal * 0.05f;
	Vector3 vHeightEnd(vHeightStart);
	vHeightEnd.z -= m_fHeight;

	grcDebugDraw::Sphere(vHeightStart, 0.05f, colour);
	grcDebugDraw::Sphere(vHeightEnd, 0.05f, colour);
	grcDebugDraw::Line(vHeightStart, vHeightEnd, colour);
	sprintf(buff, "%.2f", m_fHeight);
	grcDebugDraw::Text((vHeightStart + vHeightEnd) * 0.5f, colour, buff);

	// Depth
	Vector3 vDepthStart(vPoint);
	vDepthStart.z += 0.05f;
	Vector3 vDepthEnd(vDepthStart);
	vDepthEnd.x += -vNormal.x * m_fDepth;
	vDepthEnd.y += -vNormal.y * m_fDepth;

	grcDebugDraw::Sphere(vDepthStart, 0.05f, colour);
	grcDebugDraw::Sphere(vDepthEnd, 0.05f, colour);
	grcDebugDraw::Line(vDepthStart, vDepthEnd, colour);
	sprintf(buff, "%.2f", m_fDepth);
	grcDebugDraw::Text((vDepthStart + vDepthEnd) * 0.5f, colour, buff);

	// Drop
	Vector3 vDropStart(vDepthEnd);
	Vector3 vDropEnd(vDropStart);
	vDropEnd.z -= m_fDrop;

	grcDebugDraw::Sphere(vDropStart, 0.05f, colour);
	grcDebugDraw::Sphere(vDropEnd, 0.05f, colour);
	grcDebugDraw::Line(vDropStart, vDropEnd, colour);
	sprintf(buff, "%.2f", m_fDrop);
	grcDebugDraw::Text((vDropStart + vDropEnd) * 0.5f, colour, buff);

	//! handhold position.
	grcDebugDraw::Sphere(GetHandholdPosition(), 0.05f, colour);

	//! position for drop.
	grcDebugDraw::Sphere(GetGroundPositionAtDrop(), 0.05f, colour);

	//! max position for drop.
	grcDebugDraw::Sphere(GetMaxGroundPosition(), 0.05f, colour);
}

void CClimbHandHoldDetected::DumpToLog()
{
	taskDisplayf("Position = %f:%f:%f", m_handHold.GetPoint().x, m_handHold.GetPoint().y, m_handHold.GetPoint().z);
	taskDisplayf("Normal = %f:%f:%f", m_handHold.GetNormal().x, m_handHold.GetNormal().y, m_handHold.GetNormal().z);
	taskDisplayf("PhysicalHandHoldOffset = %f:%f:%f", m_vPhysicalHandHoldOffset.x, m_vPhysicalHandHoldOffset.y, m_vPhysicalHandHoldOffset.z);
	taskDisplayf("m_fHeight = %f", m_fHeight);
	taskDisplayf("m_fDepth = %f", m_fDepth);
	taskDisplayf("m_fDrop = %f", m_fDrop);
	taskDisplayf("m_fHorizontalClearance = %f", m_fHorizontalClearance);
	taskDisplayf("m_fVerticalClearance = %f", m_fVerticalClearance);
	taskDisplayf("m_fHighestZ = %f", m_fHighestZ);
	taskDisplayf("m_fLowestZ = %f", m_fLowestZ);
	taskDisplayf("m_fClimbAngle = %f", m_fClimbAngle);
	taskDisplayf("m_fHandHoldGroundAngle = %f", m_fHandHoldGroundAngle);
}
#endif // __BANK
