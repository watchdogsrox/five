
#ifndef PED_CLOTH_H
#define PED_CLOTH_H

#include "atl/dlist.h"
#include "crskeleton/skeleton.h"
#include "parser/manager.h"
#include "phbound/boundcapsule.h"
#include "phbound/boundcomposite.h"
#include "pheffects/cloth_verlet.h"


struct CClothCollisionData
{	
	QuatV		m_Rotation;
	float		m_CapsuleRadius;
	float		m_CapsuleLen;
	int			m_BoneIndex;	
	atString	m_BoneName;	
	bool		m_Enabled;

	PAR_SIMPLE_PARSABLE;
};


class CCollisionBound
{
public:

	CCollisionBound()
		: m_pCompositeBound(NULL)
		, m_pWeaponBound(NULL)
		, m_pParentMatrix(NULL)
	{}

	~CCollisionBound( )
	{
		Shutdown();
	}

	void AttachBounds(const crSkeleton* skel, atArray<CClothCollisionData>& collisionDataArrayRef);
	void UpdateWeaponBound(float capsuleLen, float capsuleRad, Mat34V_In weaponBoundMat);

	void Update( int boundIndex, Mat34V_In boneMat )
	{
		Assert( m_pCompositeBound );
		m_pCompositeBound->SetCurrentMatrix( boundIndex, boneMat );
	}

	void UpdateCapsuleRadius( int boundIndex, float newCapsuleRadius )
	{		
		Assert( m_pCompositeBound );
		phBoundCapsule* pBound = (phBoundCapsule*)(m_pCompositeBound->GetBound( boundIndex ));
		Assert( pBound );
		pBound->SetCapsuleRadius( newCapsuleRadius );
	}

	Vec3V_Out GetPosition( int boundIndex )
	{
		Assert( m_pCompositeBound );
		phBoundCapsule* pCapsuleBound = (phBoundCapsule*)(m_pCompositeBound->GetBound( boundIndex ));
		Assert( pCapsuleBound );
		Assert( m_pParentMatrix );
		return pCapsuleBound->GetWorldCentroid( *((Mat34V*)m_pParentMatrix) );
	}

	void Shutdown()
	{
		Assert( m_pCompositeBound );
		m_pCompositeBound->Release();
		m_pCompositeBound = NULL;
		m_pWeaponBound = NULL;
		m_pParentMatrix = NULL;
	}

	phBoundComposite* GetCompositeBound() const { return m_pCompositeBound; }
	const Mat34V* GetParentMatrix() const { return m_pParentMatrix; }

private:
	phBoundComposite*	m_pCompositeBound;
	phBoundCapsule*		m_pWeaponBound;
	const Mat34V*		m_pParentMatrix;
};


class CPedClothCollision : datBase
{		
public:
	CPedClothCollision();	
	~CPedClothCollision() {}

	void AttachToCloth(phVerletCloth* cloth);
	
	void Update();	
	void UpdateWeaponBound(float capsuleLen, float capsuleRad, Mat34V_In weaponBoundMat);
	void Shutdown();
	
	void Load();
	void Save();

	bool IsAvailable() const { return m_Skeleton? false: true; }

	static const char* sm_tuneFilePath;
	static CPedClothCollision* GetNextPedClothCollision(const crSkeleton* skel);
	static void Init();

protected:
	void AttachSkeleton(const crSkeleton* skel);

private:
	
	const char*			m_Name;
	const crSkeleton*	m_Skeleton;
	int m_DynamicRadiusBoneIndex;
	int m_DynamicRadiusDataIndex;
	Vec3V m_oldPos;

	atArray<CClothCollisionData> m_CollisionData;

	CCollisionBound m_CollisionBound;

	PAR_SIMPLE_PARSABLE;
};



#endif // PED_CLOTH_H
