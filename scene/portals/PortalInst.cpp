// Title	:	CPortalInst.cpp
// Author	:	John Whyte
// Started	:	5/12/2005

//rage
#include "grcore/debugdraw.h"
#include "spatialdata/sphere.h"

#include "fwscene/search/SearchVolumes.h"

//game
#include "scene/portals/InteriorInst.h"
#include "scene/portals/portal.h"
#include "scene/portals/portalInst.h"
#include "scene/portals/PortalTracker.h"
#include "scene/world/gameWorld.h"

#if __DEV
//OPTIMISATIONS_OFF()
#endif

#if __BANK
extern bool bSafeZoneSphereBoundingSphereMethod;
#endif

FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CPortalInst, CONFIGURED_FROM_FILE, 0.34f, atHashString("PortalInst",0x272e7f43)); //JW - was 500

CPortalInst* CPortalInst::ms_pLinkingPortal;		// used to hold result of link portal search call back

//
// name:		CPortal::CPortal
// description:	Constructor for a portal instance
CPortalInst::CPortalInst(const eEntityOwnedBy ownedBy, const fwPortalCorners &corners, CInteriorInst* pInterior, u32 portalIdx)
	: CBuilding( ownedBy )
{
	SetTypePortal();
	DisableCollision();
	m_bIsOneWayPortal = false;
	m_bIsLinkPortal = false;
	m_bIgnoreModifier = false;
	m_bUseLightBleed = false;

	Vector3 radiusVec = (corners.GetCorner(0) - corners.GetCorner(2))/2.0f;
	m_boundSphereCentre = corners.GetCorner(0) - radiusVec;		//mid point of both points
	m_boundSphereRadius = radiusVec.Mag();

	Mat34V m;
	Mat34VFromTranslation(m, RC_VEC3V(m_boundSphereCentre));
#if ENABLE_MATRIX_MEMBER	
	SetTransform(m);
	SetTransformScale(1.0f, 1.0f);
#else
	SetTransform(rage_new fwMatrixTransform(m));
#endif

	m_boundBox.Invalidate();
	m_boundBox.GrowPoint( VECTOR3_TO_VEC3V(corners.GetCorner(0)) );
	m_boundBox.GrowPoint( VECTOR3_TO_VEC3V(corners.GetCorner(1)) );
	m_boundBox.GrowPoint( VECTOR3_TO_VEC3V(corners.GetCorner(2)) );
	m_boundBox.GrowPoint( VECTOR3_TO_VEC3V(corners.GetCorner(3)) );

	m_bIsLinkPortal = false;

	m_bScannedAsVisible = 0;
	m_dist = 0.0f;

	// set owning (primary) interior inst and idx
	m_pMLO1 = pInterior;
	m_portalIdx1 = portalIdx;

	// clear linked interior and idx
	m_pMLO2 = NULL; 
	m_portalIdx2 = -1;

	m_interiorDetailDistance = 0.0f;
}

CPortalInst::~CPortalInst()
{
	if (m_pMLO1){
		m_pMLO1->RemovePortalInstanceRef(this);
	}
}


// add & remove from world is overloaded to handle linking / unlinking of link portals between MLOs
void CPortalInst::Add(){

	CEntity::Add();
}

// add & remove from world is overloaded to handle linking / unlinking of link portals between MLOs
void CPortalInst::Remove(){

	CEntity::Remove();
}

Vector3	CPortalInst::GetBoundCentre() const
{
	return m_boundSphereCentre;
}

void CPortalInst::GetBoundCentre(Vector3& ret) const
{
	ret = m_boundSphereCentre;
}

float CPortalInst::GetBoundRadius() const
{
	return m_boundSphereRadius;
}

float CPortalInst::GetBoundCentreAndRadius(Vector3& centre) const
{
	centre = m_boundSphereCentre;
	return m_boundSphereRadius;
}

void CPortalInst::GetBoundSphere(spdSphere& sphere) const
{
	sphere.Set(RCC_VEC3V(m_boundSphereCentre), ScalarV(m_boundSphereRadius));
}

const Vector3& CPortalInst::GetBoundingBoxMin() const
{
	return(m_boundBox.GetMinVector3());
}

const Vector3& CPortalInst::GetBoundingBoxMax() const
{
	return(m_boundBox.GetMaxVector3());
}

FASTRETURNCHECK(const spdAABB &) CPortalInst::GetBoundBox(spdAABB& /*box*/) const
{
	return m_boundBox;
}

// determine if the given position is on the visible side of this exterior portal or not
bool CPortalInst::IsOnVisibleSide(const Vector3& pos){
	fwPortalCorners outPortal;
	m_pMLO1->GetPortalInRoom(0, m_portalIdx1, outPortal);		// room 0 = exterior, remember...

	// if the current camera position is on the correct side for the portal
	return(outPortal.IsOnPositiveSide(pos)); 
}
	
// promote the linked to interior to the owner of this portal inst (e.g. when original creator interior is deleted)
bool	 CPortalInst::PromoteLinkedInterior(void) 
{ 
	if (!m_pMLO2){
		// no linked interior to promote to owner
		return(false);
	}
	
	m_pMLO1 = m_pMLO2; 
	m_portalIdx1 = m_portalIdx2;

	m_pMLO2 = NULL;
	m_portalIdx2 = -1;

	// linked interior is now the owner of this portal inst
	return(true);
}

void	 CPortalInst::SetLinkedInterior(CInteriorInst* pIntInst, s32 portalIdx)
{
	Assert(m_pMLO2 == NULL);
	Assertf(m_pMLO1 != m_pMLO2, "Interior found with link portals linking back to self");

	m_pMLO2 = pIntInst;
	m_portalIdx2 = portalIdx;
}

// for link portals, get the data of the interior on the other side of the portal from the current one
void CPortalInst::GetDestinationData(const CInteriorInst* pSrcInt, CInteriorInst*& pDestInt, s32& portalIdx) 
{
	Assert(m_bIsLinkPortal);
	Assert(pSrcInt);

	if (pSrcInt == m_pMLO2){
		pDestInt = m_pMLO1; 
		portalIdx = m_portalIdx1;
	} 
	else if (pSrcInt == m_pMLO1) {
		pDestInt = m_pMLO2;
		portalIdx = m_portalIdx2;
	} 
	else {
		Assertf(false, "Portal instance doesn't point back to owning interior\n");
	}
}

// go through the pool and resturn the list of intersections
spdSphere CPortalInst::FindIntersectingPortals(Vec3V_In testPosition, const fwIsSphereIntersecting &testSphere, fwPtrListSingleLink& scanList, bool bFindLinkPortals, bool bFindNonLinkPortals)
{

	CPortalInst::Pool* pPortalInstPool = CPortalInst::GetPool();
	CPortalInst* pPortalInst = NULL;
	CPortalInst* pPortalInstNext = NULL;
	s32 poolSize=pPortalInstPool->GetSize();
	ScalarV minSafeDistance( V_FLT_MAX );
	ScalarV safeDistancePullBack( V_FLT_SMALL_1 );
	bool safeDistanceSet = false;

	pPortalInstNext = pPortalInstPool->GetSlot(0);

	s32 i = 0;
	while(i<poolSize)
	{
		pPortalInst = pPortalInstNext;

		// spin to next valid entry
		while((++i < poolSize) && (pPortalInstPool->GetSlot(i) == NULL));

		if (i != poolSize)
			pPortalInstNext = pPortalInstPool->GetSlot(i);

		if(	!pPortalInst ||  
			(!bFindLinkPortals && pPortalInst->IsLinkPortal()) ||
			(!bFindNonLinkPortals && !pPortalInst->IsLinkPortal()) )
		{
			continue;
		}

		//sanity check
		Assert(pPortalInst->GetPrimaryInterior());
		Assert(pPortalInst->GetPrimaryInterior() == pPortalInst->GetPrimaryInterior()->GetProxy()->GetInteriorInst());

		// Update the "safe zone" sphere.
		if(BANK_SWITCH(bSafeZoneSphereBoundingSphereMethod, true))
		{
			spdSphere sphere;
			pPortalInst->GetBoundSphere( sphere );

			ScalarV safeDistance = MagFast( sphere.GetCenter() - testPosition ) - sphere.GetRadius();
			minSafeDistance = Min( minSafeDistance, safeDistance );
			safeDistanceSet = true;
		}
#if __BANK
		else
		{
			fwPortalCorners outPortal;
			pPortalInst->GetPrimaryInterior()->GetPortalInRoom(0, pPortalInst->GetPrimaryPortalIndex(), outPortal);
			ScalarV currentPortalSafeDistance = outPortal.CalcDistanceToPoint(testPosition) - safeDistancePullBack;
			minSafeDistance = Min( minSafeDistance, currentPortalSafeDistance );
			safeDistanceSet = true;
		}
#endif

		// If portal intersects, add to list of portals.
		spdAABB tempBox;
		spdSphere sphere;
		const spdAABB &box = pPortalInst->GetBoundBox( tempBox );
		pPortalInst->GetBoundSphere( sphere );
		if (testSphere( sphere, box )){
			scanList.Add(pPortalInst);
		}
	}

	if (safeDistanceSet)
	{
		minSafeDistance = Max( minSafeDistance, ScalarV(V_ZERO) );
	}
	else
	{
		minSafeDistance = ScalarV(1000.0f);
	}
	return spdSphere( testPosition, minSafeDistance );
}

// render the bounding spheres for all the portal instances
#if __DEV
extern bool bShowPortalInstances;
void		CPortalInst::ShowPortalInstances(){

	if (!bShowPortalInstances){
		return;
	}

	CPortalInst::Pool* pPool = CPortalInst::GetPool();
	u32 numPortals = pPool->GetSize();

	for(u32 i=0; i<numPortals; i++){
		CPortalInst* pPortalInst = pPool->GetSlot(i);
		
		if (pPortalInst){
//			Vector3 pos = pPortalInst->GetBoundCentre();
			Color32 colour(0,0,100,100);

			// one-way
			if (pPortalInst->m_bIsOneWayPortal){
				colour.Set(150,0,0,100);
			}

			// can be linked
			if (pPortalInst->m_bIsLinkPortal) {
				colour.Set(250, 250, 250, 200);
			}

			// is successfully linked
			if (pPortalInst->m_pMLO2) {
				colour.Set(100, 100, 100, 80);
			}

			if (pPortalInst == CPortalTracker::GetPlayerProximityPortal()){
				colour.Set(0, 255, 0, 255);
			}

//			grcDebugDraw::Sphere(pos, pPortalInst->GetBoundRadius(),colour);
			
			float radius = pPortalInst->GetBoundRadius();
			Vector3	p1 = pPortalInst->GetBoundCentre() - Vector3(0.0f,0.0f,radius);
			Vector3	p2 = pPortalInst->GetBoundCentre() + Vector3(0.0f,0.0f,radius);
			grcDebugDraw::Line(p1, p2, colour);
			p1 = pPortalInst->GetBoundCentre() - Vector3(0.0f,radius, 0.0f);
			p2 = pPortalInst->GetBoundCentre() + Vector3(0.0f,radius, 0.0f);
			grcDebugDraw::Line(p1, p2, colour);
			p1 = pPortalInst->GetBoundCentre() - Vector3(radius, 0.0f, 0.0f);
			p2 = pPortalInst->GetBoundCentre() + Vector3(radius, 0.0f, 0.0f);
			grcDebugDraw::Line(p1, p2, colour);
		}
	}
}

// only used for debug visualization
void CPortalInst::DebugRender(){
		Vector3 pos = GetBoundCentre();
		Color32 colour(0,100,100,100);

		if (m_bIsOneWayPortal){
			colour.Set(150,0,0,100);
		}

		if (m_bIsLinkPortal) {
			colour.Set(250, 250, 250, 200);
		}

		if (m_pMLO2) {
			colour.Set(100, 100, 100, 80);
		}

		grcDebugDraw::Sphere(pos, GetBoundRadius(),colour);

		fwRect rect = GetBoundRect();
		Vector3 pos1(rect.left,rect.top, pos.z);
		Vector3 pos2(rect.left,rect.bottom, pos.z);
		Vector3 pos3(rect.right,rect.top, pos.z);
		Vector3 pos4(rect.right,rect.bottom, pos.z);

		grcDebugDraw::Poly(RCC_VEC3V(pos1), RCC_VEC3V(pos2), RCC_VEC3V(pos3), colour);
		grcDebugDraw::Poly(RCC_VEC3V(pos4), RCC_VEC3V(pos3), RCC_VEC3V(pos2), colour);
}
#endif //__DEV
