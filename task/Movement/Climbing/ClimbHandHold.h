#ifndef CLIMB_HANDHOLD_H
#define CLIMB_HANDHOLD_H

// Rage headers
#include "math/simpleMath.h"

// Framework headers
#include "fwmaths/Angle.h"
#include "fwmaths/geomutil.h"

// Game headers
#include "Scene/RegdRefTypes.h"
#include "physics/gtaMaterialManager.h"

// Forward declarations
namespace rage { class Color32; }
namespace rage { struct CollisionMaterialSettings; }
class CEntity;

//////////////////////////////////////////////////////////////////////////
// CClimbHandHold
//////////////////////////////////////////////////////////////////////////

class CClimbHandHold
{
public:

	CClimbHandHold();
	CClimbHandHold(const Vector3& vPoint, const Vector3& vNormal, const CEntity* pEntity = NULL);
	CClimbHandHold(const CClimbHandHold& other);

	// Member settors
	void Set(const Vector3& vPoint, const Vector3& vNormal, const CEntity* pEntity = NULL);

	// Set the hand hold point
	void SetPoint(const Vector3& vPoint) { m_vPoint = vPoint; }

	//
	// Accessors
	//

	// Member accessors
	const Vector3& GetPoint() const { return m_vPoint; }
	const Vector3& GetNormal() const { return m_vNormal; }
	const CEntity* GetEntity() const { return m_pEntity; }

	// Get the heading a ped would orientate to when hanging on this hand hold
	float GetHeading() const { return fwAngle::LimitRadianAngle(rage::Atan2f(m_vNormal.x, -m_vNormal.y)); }

#if __BANK
	// Render
	void Render(const Color32& colour) const;
#endif // __BANK

	// Operators
	inline bool operator==(const CClimbHandHold& other) const;
	inline bool operator!=(const CClimbHandHold& other) const;

private:

	// Line defining the ledge
	Vector3 m_vPoint;

	// The direction that we hang on from
	Vector3 m_vNormal;

	// The CEntity that the hand hold belongs to
	const CEntity* m_pEntity;
};

inline bool CClimbHandHold::operator==(const CClimbHandHold& other) const
{
	if(m_pEntity != other.m_pEntity)
	{
		return false;
	}

	Vector3 vStartDiff(m_vPoint - other.m_vPoint);
	if(!IsNearZero(vStartDiff.x) || !IsNearZero(vStartDiff.y) || !IsNearZero(vStartDiff.z))
	{
		return false;
	}

	Vector3 vNormalDiff(m_vNormal - other.m_vNormal);
	if(!IsNearZero(vNormalDiff.x) || !IsNearZero(vNormalDiff.y) || !IsNearZero(vNormalDiff.z))
	{
		return false;
	}

	return true;
}

inline bool CClimbHandHold::operator!=(const CClimbHandHold& other) const
{
	return !(*this == other);
}

////////////////////////////////////////////////////////////////////////////////
// CClimbHandHoldDetected
////////////////////////////////////////////////////////////////////////////////

class CClimbHandHoldDetected
{
public:
	FW_REGISTER_CLASS_POOL(CClimbHandHoldDetected);

	CClimbHandHoldDetected();

	// Reset the members of the hand hold
	void Reset();

	//
	// Accessors
	//

	// Get the hand hold
	const CClimbHandHold& GetHandHold() const { return m_handHold; }

	// Get the height of the hand hold
	float GetHeight() const { return m_fHeight; }

	// Get the depth of the hand hold
	float GetDepth() const { return m_fDepth; }

	// Get the drop of the hand hold
	float GetDrop() const { return m_fDrop; }

	// Get the minimum depth across the hand hold
	float GetHorizontalClearance() const { return m_fHorizontalClearance; }

	// Get the height above the hand hold
	float GetVerticalClearance() const { return m_fVerticalClearance; }

	// Get the highest Z recorded during handhold test.
	float GetHighestZ() const { return m_fHighestZ; }

	// Get the lowest Z recorded during handhold test.
	float GetLowestZ() const { return m_fLowestZ; }

	// Get the physical
	CPhysical* GetPhysical() const { return m_pPhysical; }

	// Get the top surface of the thing we are climbing on
	CollisionMaterialSettings* GetTopSurfaceMaterial() const { return m_pTopSurfaceMaterial; }

	// Get the front surface of the thing we are climbing on
	CollisionMaterialSettings* GetFrontSurfaceMaterial() const { return m_pFrontSurfaceMaterial; }
	phMaterialMgr::Id GetFrontSurfaceMaterialId() const { return m_FrontSurfaceMaterialId; }

	// Get local offset of object we are climbing on.
	const Vector3& GetPhysicalHandHoldOffset() const { return m_vPhysicalHandHoldOffset; }

	// Get local offset of object we are climbing on.
	const Vector3& GetPhysicalHandHoldNormal() const { return m_vPhysicalHandHoldNormal; }

	// Get the angle of the climb point.
	float GetClimbAngle() { return m_fClimbAngle; }

	// Is Drop infinitely big? I.e. bigger that we are able to detect.
	bool IsMaxDrop() const { return m_bIsMaxDrop; }

	// Is clearance infinitely big? I.e. bigger that we are able to detect.
	bool IsMaxHorizontalClearance() const { return m_bIsMaxHorizontalClearance; }

	// Get angle of ground at handhold point.
	float GetHandHoldGroundAngle() const { return m_fHandHoldGroundAngle; }

	// Get the handhold position.
	Vector3 GetHandholdPosition() const;

	// Get the handhold position.
	Vector3 GetHandholdNormal() const;

	// Get the position just before the drop was detected.
	Vector3 GetGroundPositionAtDrop() const;

	// Get the max ground position along climb direction.
	Vector3 GetMaxGroundPosition() const;

	// Get the average normal of the ground at handhold position.
	const Vector3& GetGroundNormalAVG() const { return m_vGroundNormalAVG; }

	// Get direction to align ped.
	Vector3 GetAlignDirection() const;

	// Get clone position and heading.
	float	GetCloneTestHeading() const { return m_fCloneTestHeading; }
	bool	GetCloneUseTestheading() const { return m_bCloneUseTestDirection; }
	Vector3 GetCloneIntersection() const {return m_vCloneIntersection;}

	u16 GetPhysicalComponent() const { return m_uPhysicalComponent; }
	u8	GetMinNumDepthTests() const { return m_uCloneMinNumDepthTests; }
	
	Vector3 GetPositionRelativeToPhysical(const Vector3 &vPosition) const;
	Vector3 GetDirectionRelativeToPhysical(const Vector3 &vPosition) const;
	
	//
	// Settors
	//

	// Set the hand hold
	void SetHandHold(const CClimbHandHold& handHold) { m_handHold = handHold; }

	// Set the hand hold point
	void SetPoint(const Vector3& vHandHoldPoint) { m_handHold.SetPoint(vHandHoldPoint); }

	// Set the height
	void SetHeight(const float fHeight) { m_fHeight = fHeight; }

	// Set the depth
	void SetDepth(const float fDepth) { m_fDepth = fDepth; }

	// Set the drop
	void SetDrop(const float fDrop) { m_fDrop = fDrop; }

	// Set the minimum depth across the hand hold
	void SetHorizontalClearance(const float fHorizontalClearance) { m_fHorizontalClearance = fHorizontalClearance; }

	// Set the height above the hand hold
	void SetVerticalClearance(const float fVerticalClearance) { m_fVerticalClearance = fVerticalClearance; }

	// Set the highest Z recorded during handhold test.
	void SetHighestZ(float fHighestZ) { m_fHighestZ = fHighestZ; }

	// Set the lowest Z recorded during handhold test.
	void SetLowestZ(float fLowestZ) { m_fLowestZ = fLowestZ; }

	// Set the physical
	void SetPhysical(CPhysical* pPhysical, u16 uComponent) { m_pPhysical = pPhysical; m_uPhysicalComponent = uComponent; } 

	// Set the top surface of the thing we are climbing on
	void SetTopSurfaceMaterial(CollisionMaterialSettings* pMaterial) { m_pTopSurfaceMaterial = pMaterial; }

	// Set the front surface of the thing we are climbing on
	void SetFrontSurfaceMaterial(phMaterialMgr::Id materialId, CollisionMaterialSettings* pMaterial) {  m_FrontSurfaceMaterialId = materialId; m_pFrontSurfaceMaterial = pMaterial; }

	// Set physical offset (useful for clone peds to directly set it from owner).
	void SetPhysicalHandHoldOffset(const Vector3& vHandHoldOffset) { m_vPhysicalHandHoldOffset = vHandHoldOffset; }

	//! Calc Local offset of handhold point, relative to physical.
	void CalcPhysicalHandHoldOffset();

	// Set the angle of the climb point.
	void SetClimbAngle(float fAngle) { m_fClimbAngle = fAngle; }

	// Set max drop flag.
	void SetMaxDrop(bool bMaxDrop) { m_bIsMaxDrop = bMaxDrop; }

	// Set max hor clearance flag.
	void SetMaxHorizontalClearance(bool bMaxClearance) { m_bIsMaxHorizontalClearance = bMaxClearance; }

	// Set angle of ground at handhold point.
	void SetHandHoldGroundAngle(float fAngle) { m_fHandHoldGroundAngle = fAngle; }

	// Set the position just before the drop was detected.
	void SetGroundPositionAtDrop(const Vector3 &vPosition);

	// Set max ground position for climb.
	void SetMaxGroundPosition(const Vector3 &vPosition);

	// Set the average normal of the ground at handhold position.
	void SetGroundNormalAVG(const Vector3 &vNormal) { m_vGroundNormalAVG = vNormal; }

	// Set direction to align ped.
	void SetAlignDirection(const Vector3 &vAlignDirection) { m_vAlignDirection = vAlignDirection; }
 
	//Clone 
	void SetCloneTestHeading( float fCloneTestheading) { m_fCloneTestHeading = fCloneTestheading; }
	void SetCloneUseTestheading(bool bCloneUseTestDirection) {m_bCloneUseTestDirection =  bCloneUseTestDirection; }
	void SetCloneIntersection(const Vector3& vCloneIntersection) {m_vCloneIntersection =  vCloneIntersection; }
	void SetCloneMinNumDepthTests(u8 uMinNumDepthTests) {  m_uCloneMinNumDepthTests = uMinNumDepthTests; } 

#if __BANK
	// Render
	void Render(const Color32& colour) const;

	void DumpToLog();
#endif // __BANK

	void GetPhysicalMatrix(Matrix34 &physicalMatrix) const;
	s16 GetBoneFromHitComponent() const;
		
	static void GetPhysicalMatrix(Matrix34 &physicalMatrix, const CPhysical* pPhysical, u16 uPhysicalComponent);
	static s16 GetBoneFromHitComponent(const CPhysical* pPhysical, u16 uPhysicalComponent);

private:

	//
	// Members
	//

	// The hand hold
	CClimbHandHold m_handHold;

	//! Offset of handhold relative to physical.
	Vector3 m_vPhysicalHandHoldOffset;
	Vector3 m_vPhysicalHandHoldNormal;

	//! Position just before drop. If physical is set, this is in object space.
	Vector3 m_vGroundPositionAtDrop;

	//! Max ground position for climb.
	Vector3 m_vMaxGroundPosition;

	//! AVG ground normal at climb point (sampled up to 2m ahead of handhold).
	Vector3 m_vGroundNormalAVG;

	//! Align direction.
	Vector3 m_vAlignDirection;

	//clone
	Vector3 m_vCloneIntersection;

	// The height of the hand hold
	float m_fHeight;

	// The depth of the hand hold
	float m_fDepth;

	// The drop of the hand hold
	float m_fDrop;

	// The minimum depth across the hand hold
	float m_fHorizontalClearance;

	// The height above the hand hold
	float m_fVerticalClearance;

	// Highest recorded Z during handhold test.
	float m_fHighestZ;

	// lowest recorded Z during handhold test.
	float m_fLowestZ;

	// the angle of the climb surface (i.e x degree slope).
	float m_fClimbAngle;

	// the angle of the ground surface at the handhold point.
	float m_fHandHoldGroundAngle;

	//! Save the test direction that succeeded for syncing and setting the same heading on remote clone.
	float m_fCloneTestHeading;

	// The physical we are climbing on
	RegdPhy m_pPhysical;

	// The top surface of the thing we are climbing on
	CollisionMaterialSettings* m_pTopSurfaceMaterial;

	// The front surface of the thing we are climbing on
	CollisionMaterialSettings* m_pFrontSurfaceMaterial;

	u16 m_uPhysicalComponent;
	u8	m_uCloneMinNumDepthTests;

	//! If this drop as big as we can detect?
	bool m_bIsMaxDrop : 1;
	bool m_bIsMaxHorizontalClearance : 1;
	bool m_bCloneUseTestDirection : 1;

	phMaterialMgr::Id m_FrontSurfaceMaterialId;
};

#endif // CLIMB_HANDHOLD_H
