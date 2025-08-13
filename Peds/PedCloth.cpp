
#include "PedCloth.h"
#include "fwpheffects/clothmanager.h"
#include "bank/bank.h"
#include "crskeleton/skeletondata.h"
#include "physics/physics.h"


#include "pedcloth_parser.h"

AI_OPTIMISATIONS()

#define		MAX_PED_CLOTH_COLLISIONS		32

const char* CPedClothCollision::sm_tuneFilePath = "common:/data/cloth/";
CPedClothCollision gPedClothCollisions[MAX_PED_CLOTH_COLLISIONS];


// CCollisionBound

void CCollisionBound::AttachBounds(const crSkeleton* skel, atArray<CClothCollisionData>& collisionDataArrayRef)
{
	Assert( skel );
	const int numBounds = collisionDataArrayRef.GetCount();
	if( numBounds )
	{
		const Mat34V* pParentMat = skel->GetParentMtx();
		Assert( pParentMat );

		phBoundComposite*	pCompositeBound = rage_new phBoundComposite();
		Assert( pCompositeBound );

		pCompositeBound->Init(numBounds + 1, true);				// add one extra bound for the weapon
		pCompositeBound->SetNumBounds(numBounds + 1);

		m_pCompositeBound = pCompositeBound;
		m_pParentMatrix = pParentMat;

		int i;
		for( i = 0; i < numBounds; ++i )
		{
			CClothCollisionData& rCollisionData = collisionDataArrayRef[i];
// NOTE: related to B*2042296. when main player is changed into an animal, the bound is not valid anymore
			if( rCollisionData.m_Enabled )
			{
				Assert( rCollisionData.m_BoneIndex > -1 );

	// NOTE: create and attach bound
				phBoundCapsule*	pBound = rage_new phBoundCapsule();
				Assert( pBound );
				pBound->SetCapsuleSize( rCollisionData.m_CapsuleRadius, rCollisionData.m_CapsuleLen );	

				pCompositeBound->SetBound( i, pBound );
				pCompositeBound->SetCurrentMatrix( i, skel->GetObjectMtx(rCollisionData.m_BoneIndex) );

				Assert( pBound->GetRefCount() == 2 );
				pBound->Release();
			}
		}

// NOTE: create and attach weapon bound
		Assert( !m_pWeaponBound );
		m_pWeaponBound = rage_new phBoundCapsule();
		Assert( m_pWeaponBound );

		m_pWeaponBound->SetCapsuleSize( 0.01f, 0.01f );

		pCompositeBound->SetBound( i, m_pWeaponBound );
		pCompositeBound->SetCurrentMatrix( i, Mat34V(V_IDENTITY) );

		Assert( m_pWeaponBound->GetRefCount() == 2 );
		m_pWeaponBound->Release();
	}
}

void CCollisionBound::UpdateWeaponBound(float capsuleLen, float capsuleRad, Mat34V_In weaponBoundMat)
{
	Assert( m_pCompositeBound );
	Assert( m_pWeaponBound );
	m_pWeaponBound->SetCapsuleSize( capsuleRad, capsuleLen );
	m_pCompositeBound->SetCurrentMatrix( m_pCompositeBound->GetNumBounds()-1, weaponBoundMat );
};


// CPedClothCollision

void CPedClothCollision::Init()
{
	for(int i = 0; i < MAX_PED_CLOTH_COLLISIONS; ++i )
	{
		gPedClothCollisions[i].Load();
	}
}

CPedClothCollision* CPedClothCollision::GetNextPedClothCollision(const crSkeleton* skel)
{
	Assert( skel );
	for(int i = 0; i < MAX_PED_CLOTH_COLLISIONS; ++i )
	{
		if( gPedClothCollisions[i].IsAvailable() )
		{
			clothDebugf1("[CPedClothCollision::GetNextPedClothCollision] Searched through %d slots to find available ped collision.", i);

			gPedClothCollisions[i].AttachSkeleton(skel);
			return &gPedClothCollisions[i];
		}
	}	
	AssertMsg( false, "[CPedClothCollision::GetNextPedClothCollision] Out of ped collision slots" );
	return NULL;
}

CPedClothCollision::CPedClothCollision() 
 : m_Name("customClothCollision")
 , m_Skeleton(NULL)
 , m_DynamicRadiusBoneIndex(-1)
 , m_DynamicRadiusDataIndex(-1)
 , m_oldPos(Vec3V(V_ZERO))
{
}



void CPedClothCollision::Load()
{
	Assert( CPedClothCollision::sm_tuneFilePath );
	Assert( m_Name );

	ASSET.PushFolder( CPedClothCollision::sm_tuneFilePath );
	Assertf( ASSET.Exists(m_Name, "xml"), "Expected file %s/%s.xml does not exist", CPedClothCollision::sm_tuneFilePath, m_Name);

	char buff[128];
	formatf( buff, "%s%s", sm_tuneFilePath, m_Name );
	parSettings s = parSettings::sm_StrictSettings; 
	s.SetFlag(parSettings::USE_TEMPORARY_HEAP, true); 
	PARSER.LoadObject( buff, "xml", *this, &s);	

	ASSET.PopFolder();
}


void CPedClothCollision::Save()
{
#if !__FINAL
	char buff[128];
	formatf( buff, "%s%s", sm_tuneFilePath, m_Name );
	AssertVerify(PARSER.SaveObject( buff, "xml", this, parManager::XML));
#endif
}

void CPedClothCollision::AttachToCloth(phVerletCloth* pCloth)
{
	Assert( pCloth );

	phBoundComposite* pCompositeBound = m_CollisionBound.GetCompositeBound();

	if(		pCloth->m_PedBound0 == pCompositeBound
		||	pCloth->m_PedBound1 == pCompositeBound
		)
		return;

	if( !pCloth->m_PedBound0 )
	{
		clothDebugf1("[CPedClothCollision::AttachToCloth] Attach bound at m_PedBound0");

		Assert( pCompositeBound );
		pCloth->m_PedBound0 = pCompositeBound;

		Assert( m_CollisionBound.GetParentMatrix() );
#if __PS3
		Assert( !pCloth->m_PedBoundMatrix0 );
		pCloth->m_PedBoundMatrix0 = (u32)const_cast<Mat34V*>(m_CollisionBound.GetParentMatrix());
#else
		Assert( !pCloth->m_PedBoundMatrix0.ptr );
		pCloth->m_PedBoundMatrix0 = const_cast<Mat34V*>(m_CollisionBound.GetParentMatrix());
#endif		
		pCloth->SetFlag(phVerletCloth::FLAG_COLLIDE_EDGES, true);
		pCloth->m_nIterations = DEFAULT_VERLET_ITERS_W_COLLISION;
	}
	else if( !pCloth->m_PedBound1 )
	{
		clothDebugf1("[CPedClothCollision::AttachToCloth] Attach bound at m_PedBound1");

		Assert( pCompositeBound );
		pCloth->m_PedBound1 = pCompositeBound;

		Assert( m_CollisionBound.GetParentMatrix() );
#if __PS3
		Assert( !pCloth->m_PedBoundMatrix1 );
		pCloth->m_PedBoundMatrix1 = (u32)const_cast<Mat34V*>(m_CollisionBound.GetParentMatrix());
#else
		Assert( !pCloth->m_PedBoundMatrix1.ptr );
		pCloth->m_PedBoundMatrix1 = const_cast<Mat34V*>(m_CollisionBound.GetParentMatrix());
#endif		
		pCloth->SetFlag(phVerletCloth::FLAG_COLLIDE_EDGES, true);
		pCloth->m_nIterations = DEFAULT_VERLET_ITERS_W_COLLISION;
	}
}

void CPedClothCollision::Update()
{
	Assert( m_Skeleton );

	clothManager* pClothManagher = CPhysics::GetClothManager();
	Assert( pClothManagher );
	Assert( m_Skeleton->GetParentMtx() );
#if USE_AABB_VS_AABB_PED_VS_CLOTH
	Vec3V pos = m_Skeleton->GetParentMtx()->GetCol3();
#else
	Vec3V pos = Add( m_Skeleton->GetParentMtx()->GetCol3(), Vec3V(V_UP_AXIS_WZERO) );
#endif

	pClothManagher->FindPedInClothInstance(pos, m_CollisionBound.GetCompositeBound(), clothManager::Environment );

	pClothManagher->FindClosestClothInstance(pos, clothManager::Environment, MakeFunctor(*this, &CPedClothCollision::AttachToCloth) );

	QuatV qZero(V_ZERO);
	for( int i = 0; i < m_CollisionData.GetCount(); ++i )
	{
		const CClothCollisionData& rCollisionData = m_CollisionData[i];
		if( !rCollisionData.m_Enabled )
			continue;

		const int boneIndex = rCollisionData.m_BoneIndex;
		Assert( boneIndex > -1 && boneIndex < m_Skeleton->GetBoneCount() );
		Mat34V boneMat = m_Skeleton->GetObjectMtx( boneIndex );

		QuatV q =  SelectFT( IsEqual(rCollisionData.m_Rotation, qZero), Normalize( rCollisionData.m_Rotation ), qZero );
		Mat34V rotMat;
		Mat34VFromQuatV( rotMat, q );
		Transform( boneMat, boneMat, rotMat );	

		m_CollisionBound.Update(i, boneMat);
	}

// NOTE: update the dynamic radius if index is set
	if( m_DynamicRadiusDataIndex > -1 )
	{
		const CClothCollisionData& rCollisionData = m_CollisionData[m_DynamicRadiusDataIndex];
		if( rCollisionData.m_Enabled )
		{			

			Vec3V newPos = m_CollisionBound.GetPosition( m_DynamicRadiusDataIndex );
			Vec3V tempV = SelectFT( IsEqual(m_oldPos, Vec3V(V_ZERO)), Subtract(newPos, m_oldPos), m_oldPos );				

			ScalarV mag2 = Dot( tempV, tempV );
			m_oldPos = newPos;

			static float toprange = 0.06f; 
			ScalarV maxMag2V = ScalarVFromF32(toprange);
			mag2 = SelectFT( IsGreaterThan(mag2, maxMag2V), mag2, maxMag2V );
				
			static float maxRadiusDelta = 0.4f;
			float radiusDelta = maxRadiusDelta * (mag2.Getf()*(1.0f/toprange));

			m_CollisionBound.UpdateCapsuleRadius( m_DynamicRadiusDataIndex, rCollisionData.m_CapsuleRadius + radiusDelta );
		}
	}

}

void CPedClothCollision::UpdateWeaponBound(float capsuleLen, float capsuleRad, Mat34V_In weaponBoundMat)
{
	m_CollisionBound.UpdateWeaponBound( capsuleLen, capsuleRad, weaponBoundMat );
}


void CPedClothCollision::AttachSkeleton(const crSkeleton* skel)
{
	if( m_Skeleton )
		return;

	Assert( skel );
	m_Skeleton = skel;
	const crSkeletonData& skelData = skel->GetSkeletonData();
	for( int i = 0; i < m_CollisionData.GetCount(); ++i )
	{
		CClothCollisionData& rCollisionData = m_CollisionData[i];			
		const char* pBoneName = rCollisionData.m_BoneName.c_str();
		Assert( pBoneName );
		const crBoneData* boneData = skelData.FindBoneData( pBoneName );
		// Turning this assert into a warning since we can have player models without all the usual ped bones, e.g. fish (see B*2002303).
#if __ASSERT
		if(!boneData)
		{
			Warningf("Ped skeleton doesn't have bone with name: %s  . Collision bound attached to this bone will be turned off.", rCollisionData.m_BoneName.c_str());
		}
#endif // __ASSERT
		if( boneData )
		{
			rCollisionData.m_BoneIndex = boneData->GetIndex();
			Assert( rCollisionData.m_BoneIndex < skel->GetBoneCount() );
			rCollisionData.m_Enabled = true;

			if( stricmp(pBoneName, "SKEL_Spine3") == 0 )
			{
				Assert( m_DynamicRadiusBoneIndex == -1 );
				m_DynamicRadiusBoneIndex = rCollisionData.m_BoneIndex;
				Assert( m_DynamicRadiusDataIndex == -1 );
				m_DynamicRadiusDataIndex = i;
			}
		}
		else
		{
			rCollisionData.m_BoneIndex = 0;			// use default value to avoid any crashes later 
			rCollisionData.m_Enabled = false;
		}
	}

	m_CollisionBound.AttachBounds(skel, m_CollisionData);
}

void CPedClothCollision::Shutdown()
{
	clothManager* pClothManagher = CPhysics::GetClothManager();
	Assert( pClothManagher );

// NOTE: pedbound is pgref-ed , the following line may be is not needed, clear cloths from ped bounds for just in case
	pClothManagher->FindPedInClothInstanceShutdown( m_CollisionBound.GetCompositeBound(), clothManager::Environment );

	m_Skeleton = NULL;
	m_DynamicRadiusBoneIndex = -1;
	m_DynamicRadiusDataIndex = -1;
	m_oldPos = Vec3V(V_ZERO);

	m_CollisionBound.Shutdown();
}
