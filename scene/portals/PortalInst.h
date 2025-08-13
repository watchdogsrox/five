// Title	:	PortalInst.h
// Author	:	John Whyte
// Started	:	5/12/2005

#ifndef _PORTAL_INST_H_
#define _PORTAL_INST_H_

// Framework headers

// Game headers
#include "scene/building.h"

class CInteriorInst;

//
// name:		CPortal
// description:	Describes the interface between rooms containing objects
class CPortalInst : public CBuilding
{
protected:

	virtual void Add();
	virtual void Remove();

public:
	CPortalInst(const eEntityOwnedBy ownedBy, const fwPortalCorners &corners, CInteriorInst* pInterior, u32 portalIdx);
	~CPortalInst();

	FW_REGISTER_CLASS_POOL(CPortalInst); 

	virtual Vector3	GetBoundCentre() const;
	virtual void GetBoundCentre(Vector3& centre) const;	// quicker version
	virtual float GetBoundCentreAndRadius(Vector3& centre) const; // get both in only 1 virtual function and help to simplify SPU code
	virtual void GetBoundSphere(spdSphere& sphere) const;
	virtual float GetBoundRadius() const;
	virtual const Vector3& GetBoundingBoxMin() const;
	virtual const Vector3& GetBoundingBoxMax() const;
	virtual FASTRETURNCHECK(const spdAABB &) GetBoundBox(spdAABB& box) const;

	void SetIsOneWayPortal(bool b){ m_bIsOneWayPortal = b;}
	bool GetIsOneWayPortal(void) {return(m_bIsOneWayPortal);}
	void SetIsLinkPortal(bool b){ m_bIsLinkPortal = b;}
	bool IsLinkPortal(void){ return(m_bIsLinkPortal);}
	bool HasBeenLinked(void){ return(m_pMLO2!=NULL);}

	void SetIgnoreModifier(bool b){ m_bIgnoreModifier = b;}
	bool GetIgnoreModifier(void) {return(m_bIgnoreModifier);}

	void SetUseLightBleed(bool b){ m_bUseLightBleed = b;}
	bool GetUseLightBleed(void) {return(m_bUseLightBleed);}


//	CPortalInst* GetLinkedToPortal(void) {return(m_pLinkPortal);}
//	void	SetLinkedToPortal(CPortalInst* pPortalInst) {Assert(this!=pPortalInst); m_pLinkPortal = pPortalInst;}

	bool IsOnVisibleSide(const Vector3& pos);

	static	spdSphere FindIntersectingPortals(Vec3V_In testPosition, const fwIsSphereIntersecting &testSphere, fwPtrListSingleLink&	scanList, bool bFindLinkPortals = true, bool bFindNonLinkPortals = true);

#if __DEV
	virtual void DebugRender();
	static void		ShowPortalInstances(void);
#endif //__DEV

	bool	PromoteLinkedInterior(void);
	void	ClearLinkedInterior(void) { m_pMLO2 = NULL; m_portalIdx2 = -1;}
	void	SetLinkedInterior(CInteriorInst* pIntInst, s32 portalIdx);
	CInteriorInst* GetPrimaryInterior(void) const {return(m_pMLO1);}
	CInteriorInst* GetLinkedInterior(void) const {return(m_pMLO2);}
	s32 GetPrimaryPortalIndex() const { return m_portalIdx1; }
	s32 GetLinkedPortalIndex() const { return m_portalIdx2; }

	void GetOwnerData(CInteriorInst*& pIntInst, s32& portalIdx) { pIntInst = m_pMLO1; portalIdx = m_portalIdx1;}
	void GetDestinationData(const CInteriorInst* pSrcInt, CInteriorInst*& pDestInt, s32& portalIdx);

	void	SetDetailDistance(float dist) { m_interiorDetailDistance = dist;}

private:
	float				m_boundSphereRadius;

	// bounding sphere info
	// [SPHERE-OPTIMISE] should be spdSphere
	Vector3				m_boundSphereCentre;

	// bounding box info
	spdAABB				m_boundBox;

public:
	u32	m_bScannedAsVisible;
	float	m_dist;

private:
	// might not be needed
//	s32					m_portalIdx;
//	CInteriorInst*		m_pInterior;				// interior which instanced out this portal
//	CPortalInst*		m_pLinkPortal;				// ptr to portal inst from another MLO which has been linked to

	// new info for shared portals...
	CInteriorInst*		m_pMLO1;					// this is the creating or owner interior for this portal inst
	s32					m_portalIdx1;

	CInteriorInst*		m_pMLO2;					// this is the interior which has been linked into the existing portal inst
	s32					m_portalIdx2;

	float				m_interiorDetailDistance;	// if less than this distance from portal, then render into it.

	bool				m_bIsOneWayPortal : 1;			// entry /exit portal which you can only see out of
	bool				m_bIsLinkPortal : 1;			// portal which links to a different MLO

	bool				m_bIgnoreModifier : 1;			// important now that interior modifiers now influence exteriors
	bool				m_bUseLightBleed : 1;

	static CPortalInst*	ms_pLinkingPortal;			// stores result of the search CB for a linking portal
};

#endif
