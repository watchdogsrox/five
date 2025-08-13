// Class header.
#include "physics/Deformable.h"

// Framework headers:

// Game headers:
#include "debug/DebugScene.h"
#include "Objects/object.h"
#include "physics/physics_channel.h"
#include "scene/Entity.h"
#include "scene/datafilemgr.h"

// Parser headers:
#include "physics/Deformable_parser.h"

// RAGE headers:
#include "bank/bank.h"
#include "crskeleton/jointdata.h"
#include "crskeleton/skeleton.h"
#include "crskeleton/skeletondata.h"
#include "parser/manager.h"
#include "vectormath/classfreefuncsv.h"

PHYSICS_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CDeformableBoneData methods:
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CDeformableBoneData::OnPostLoad()
{
	// Convert the bone name string to a hash.
	m_nBoneId = crSkeletonData::ConvertBoneNameToId(m_boneName.c_str());
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CDeformableObjectEntry methods:
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CDeformableObjectEntry::OnPostLoad()
{

}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// CDeformableObjectManager methods:
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Static manager singleton object instance.
CDeformableObjectManager CDeformableObjectManager::m_instance;

// RAG widget definitions ///////////////////////////////////////////
float CDeformableObjectManager::m_fDamageScaleCurveConst = 10.0f;
float CDeformableObjectManager::m_fMaxDamage = 50.0f; // The maximum damage will occur when the relative velocity exceeds this number (in m/s).
float CDeformableObjectManager::m_fDamageSaturationLimit = 0.9f;
float CDeformableObjectManager::m_fBoneDistanceScalingCurveConst = 1.2f;
#if __BANK
bool CDeformableObjectManager::m_bEnableDebugDraw = false;
bool CDeformableObjectManager::m_bVisualiseLimitsTrans = false;
bool CDeformableObjectManager::m_bVisualiseDeformableBones = false;
bool CDeformableObjectManager::m_bDisplayBoneText = false;
bool CDeformableObjectManager::m_bVisualiseBoneRadius = false;
bool CDeformableObjectManager::m_bDisplayImpactPos = false;
bool CDeformableObjectManager::m_bDisplayImpactInfo = false;
#endif //__BANK
/////////////////////////////////////////////////////////////////////

void CDeformableObjectManager::Load(const char* fileName)
{
	if(Verifyf(!m_instance.m_DeformableObjects.GetCount(), "Deformable object data have already been loaded."))
	{
		// Reset / delete the existing data before loading.
		m_instance.m_DeformableObjects.Reset();

		PARSER.LoadObject(fileName, "meta", m_instance);
	}
}

#if __BANK
static bool _GetListOfObjectsToFixUpCB(void* pItem, void* pData)
{
	physicsAssert(pItem);
	physicsAssert(pData);

	atArray<CObject*>& refObjectList = *static_cast<atArray<CObject*>*>(pData);

	CObject* pObject = static_cast<CObject*>(pItem);
	if(pObject->GetDeformableData())
	{
		refObjectList.PushAndGrow(pObject);
	}

	return true;
}
void CDeformableObjectManager::Reload()
{
	// Need to make sure that any objects with pointers into the deformable data structure
	// have these references updated. So store the current pointers if non-NULL and use that
	// to work out which new pointer to assign.

	//store hashes for all existing deformable objects
	atArray<atHashWithStringNotFinal> old_keys;
	for(int i = 0; i < m_instance.m_DeformableObjects.GetCount(); ++i)
	{
		old_keys.PushAndGrow(m_instance.m_DeformableObjects.GetKey(i)->GetHash());
	}

	// Reset / delete the existing data before loading.
	m_instance.m_DeformableObjects.Reset();

	// Load new metadata.
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::DEFORMABLE_OBJECTS_FILE);
	CDeformableObjectManager::Load(pData->m_filename);

	// Assign new pointers to all objects in the world which have deformable data references.
	atArray<CObject*> objectsToFixUp;
	CObject::GetPool()->ForAll(_GetListOfObjectsToFixUpCB, &objectsToFixUp);
	for(int nObjIdx = 0; nObjIdx < objectsToFixUp.GetCount(); ++nObjIdx)
	{
		//find the key for the object to fixup
		int nOldIndex = old_keys.Find(objectsToFixUp[nObjIdx]->GetBaseModelInfo()->GetModelNameHash());

		//get the new data
		CDeformableObjectEntry* pEntry = m_instance.m_DeformableObjects.SafeGet(old_keys[nOldIndex]);
		Assertf(pEntry, "failed to find deformableobject");
		
		//assign the new data to the object
		objectsToFixUp[nObjIdx]->SetDeformableData(pEntry);
	}
}
#endif // __BANK

void CDeformableObjectManager::OnPostLoad()
{

}

bool CDeformableObjectManager::IsModelDeformable(atHashWithStringNotFinal hName, CDeformableObjectEntry** ppData)
{
	CDeformableObjectEntry* pEntry = m_DeformableObjects.SafeGet(hName);

	if(pEntry)
	{
		if(ppData) *ppData = pEntry; 
	}

	return pEntry != NULL;
}

void CDeformableObjectManager::ProcessDeformableBones(phInst* pThisInst, const Vec3V& vImpulse, const Vec3V& vImpactPos, CEntity* pOtherEntity, const int &iThisComponent, const int &iOtherComponent)
{
	physicsAssert(pThisInst);

	if(pThisInst->GetUserData() && static_cast<CEntity*>(pThisInst->GetUserData())->GetIsTypeObject()
		&& static_cast<CObject*>(pThisInst->GetUserData())->HasDeformableData())
	{
		CObject* pObject = static_cast<CObject*>(pThisInst->GetUserData());
		const CDeformableObjectEntry* pDeformableData = pObject->GetDeformableData();

		// If this object is a frag, make sure it is inserted in the fragment cache. It won't have a skeleton otherwise.
		fragInst* pEntityFragInst = pObject->GetFragInst();
		if(pEntityFragInst)
		{
			if(!pEntityFragInst->GetCached())
			{
				pEntityFragInst->PutIntoCache();
			}
		}

		// No point being in here if we have no skeleton.
		crSkeleton* pSkel = pObject->GetSkeleton();
		rmcDrawable* pDrawable = pObject->GetDrawable();
		// Disable this assert for now as it appears an object can exist without a skeleton due to them being lazily initialised.
		//Assertf(pSkel || !pObject->GetDrawable(), "Deformable object '%s' has no skeleton pointer when it probably should have.", pObject->GetModelName());
		if(!pSkel || !pDrawable)
		{
			return;
		}		

#if __BANK && DEBUG_DRAW
		// Supply impact information for this frame to the debug draw functions if required.
		if(m_bDisplayImpactPos)
		{
			m_aImpactPositions.PushAndGrow(vImpactPos);
			m_aImpactDirVecs.PushAndGrow(vImpulse);
		}
		// Display textual info next to each impact sphere.
		if(m_bDisplayImpactInfo && m_bDisplayImpactPos)
		{
			m_aImpactImpulse.PushAndGrow(Mag(vImpulse).Getf());
			m_aImpactRelVel.PushAndGrow(Mag(vImpulse).Getf() * pThisInst->GetArchetype()->GetInvMass());
		}
#endif // __BANK && DEBUG_DRAW

		for(int i = 0; i < pDeformableData->GetNumDeformableBones(); ++i)
		{
			// Compute the distance to this bone from the impact position and use this to scale the
			// damage we apply to the bone.
			u16 nBoneId = pDeformableData->GetBoneId(i);
			int nBoneIdx = -1;
			pObject->GetSkeletonData().ConvertBoneIdToIndex(nBoneId, nBoneIdx);
			if(!Verifyf(nBoneIdx!=-1, "Couldn't find matching index for bone with ID=%d on object %s.", nBoneId, pObject->GetModelName()))
			{
				continue;
			}

			// B*1919898: Allow the frag inst to modify the impulse magnitude based on per-entity-type scales defined in the frag tuning.
			Vec3V vAdjustedImpulse = vImpulse;		
			if(pEntityFragInst && pOtherEntity && iThisComponent != -1 && iOtherComponent != -1)
			{
				ScalarV adjustedImpulseMag = pEntityFragInst->ModifyImpulseMag(iThisComponent, iOtherComponent, 1, MagSquared(vImpulse), pOtherEntity->GetCurrentPhysicsInst());
				if(adjustedImpulseMag.Getf() != -1)
				{
					vAdjustedImpulse = Normalize(vImpulse);
					vAdjustedImpulse *= adjustedImpulseMag;
				}				
			}

			Mat34V boneMatrix;
			pSkel->GetGlobalMtx(nBoneIdx, boneMatrix);
			Vec3V vPos = boneMatrix.d();
			ScalarV svDistanceSq;
			svDistanceSq = DistSquared(vPos, vImpactPos);

			ScalarV svInputDamageMag = Mag(vAdjustedImpulse) * ScalarV(pThisInst->GetArchetype()->GetInvMass());

			// Scale the applied damage for more fine grained control using a curve controlled by the constant "k" which
			// can be modified live using a RAG widget.
			ScalarV k(m_fDamageScaleCurveConst);
			ScalarV svDamageSatLimit(m_fDamageSaturationLimit);
			ScalarV svMaxDamage(m_fMaxDamage);
			ScalarV svOutputDamageMag = IsLessThanAll(svInputDamageMag, svDamageSatLimit) ?
				Pow(svInputDamageMag, k) * svMaxDamage/Pow(svDamageSatLimit, k) : svMaxDamage;

			// Now actually deform the bone if it meets certain criteria.
			ScalarV svDamageThreshold = ScalarV(pDeformableData->GetBoneDamageVelThreshold(i));
			if(IsLessThanAll(svDistanceSq, ScalarV(pDeformableData->GetBoneRadius(i))) && IsGreaterThanAll(svInputDamageMag, svDamageThreshold))
			{
				// Move this bone - the distance depends on the magnitude of the applied damage.
				Mat34V& localBoneMatrix = pSkel->GetLocalMtx(nBoneIdx);
				Vec3V vPos = localBoneMatrix.d();

				// Work out the bone displacement direction.
				Vec3V vBoneOffsetDir = vAdjustedImpulse;

				// Transform impulse direction into model space.
				vBoneOffsetDir = UnTransform3x3Ortho(pObject->GetTransform().GetMatrix(), vBoneOffsetDir);
				vBoneOffsetDir = Normalize(vBoneOffsetDir);

				// The applied damage is further scaled depending on the separation distance between the bone and the impact.
				ScalarV svAppliedDamageMag = svOutputDamageMag * Expt(-ScalarV(m_fBoneDistanceScalingCurveConst)*svDistanceSq);

				// Finally compute the amount we want to move this bone (without considering any constraints).
				physicsAssertf(pDeformableData->GetObjectStrength()*pDeformableData->GetBoneStrength(i)>0.0f, "Either object/bone strength is zero!");
				ScalarV svBoneDisplacement = svAppliedDamageMag / ScalarV(pDeformableData->GetObjectStrength()*pDeformableData->GetBoneStrength(i));

				// In case there are no translation limits set for this bone, don't let it go any further than the
				// centre line of the object.
				svBoneDisplacement = Clamp(svBoneDisplacement, ScalarV(V_ZERO), MagFast(localBoneMatrix.d()));
				Vec3V vBoneOffset = Scale(vBoneOffsetDir, svBoneDisplacement);
				vPos += vBoneOffset;

				const crBoneData* pBoneData = pObject->GetSkeletonData().GetBoneData(nBoneIdx);
				if(AssertVerify(pBoneData))
				{
					// Yes, this isn't pretty. But it means there's the minimum number of possible changes below
					bool bTransLimitX = (pBoneData->GetDofs()&(crBoneData::HAS_TRANSLATE_LIMITS)) != 0;
					bool bTransLimitY = (pBoneData->GetDofs()&(crBoneData::HAS_TRANSLATE_LIMITS)) != 0;
					bool bTransLimitZ = (pBoneData->GetDofs()&(crBoneData::HAS_TRANSLATE_LIMITS)) != 0;

					const crJointData* pJointData = NULL;

					Vec3V offsetMax, offsetMin;
					if(bTransLimitX || bTransLimitY || bTransLimitZ)
					{
						Vec3V tmin, tmax;
						pJointData = pDrawable->GetJointData();
						if(animVerifyf(pJointData, "Object(%s) has translation limits but no joint data.", pObject->GetModelName()))
						{
							pJointData->GetTranslationLimits(*pBoneData, tmin, tmax);
						}

						offsetMax = pBoneData->GetDefaultTranslation() + tmax;
						offsetMin = pBoneData->GetDefaultTranslation() + tmin;
					}

					// Does this bone have any joint limits? Don't allow them to be violated then.
					if(bTransLimitX)
					{
						ScalarV offsetMaxX = offsetMax.GetX();
						ScalarV offsetMinX = offsetMin.GetX();

						if(IsGreaterThanAll(vPos.GetX(), offsetMaxX))
							vPos.SetX(offsetMaxX);
						else if(IsLessThanAll(vPos.GetX(), offsetMinX))
							vPos.SetX(offsetMinX);
					}
					if(bTransLimitY)
					{
						ScalarV offsetMaxY = offsetMax.GetY();
						ScalarV offsetMinY = offsetMin.GetY();

						if(IsGreaterThanAll(vPos.GetY(), offsetMaxY))
							vPos.SetY(offsetMaxY);
						else if(IsLessThanAll(vPos.GetY(), offsetMinY))
							vPos.SetY(offsetMinY);
					}
					if(bTransLimitZ)
					{
						ScalarV offsetMaxZ = offsetMax.GetZ();
						ScalarV offsetMinZ = offsetMin.GetZ();
						if(IsGreaterThanAll(vPos.GetZ(), offsetMaxZ))
							vPos.SetZ(offsetMaxZ);
						else if(IsLessThanAll(vPos.GetZ(), offsetMinZ))
							vPos.SetZ(offsetMinZ);
					}

					if(pJointData)
					{
						localBoneMatrix.Setd(vPos);
					}
				}
			}
		}

		// Just update this object's skeleton on the main thread; this only gets called while the object is processing
		// collisions anyway and this is preferable to giving all of these objects anim directors.
		pSkel->Update();
	}
}

#if __BANK
void CDeformableObjectManager::AddWidgetsToBank(bkBank& bank)
{
	bank.AddToggle("Enable debug draw", &m_bEnableDebugDraw);
	bank.AddToggle("Show deformable bones", &m_bVisualiseDeformableBones);
	bank.AddToggle("Visualise translation limits", &m_bVisualiseLimitsTrans);
	bank.AddToggle("Show bone debug text", &m_bDisplayBoneText);
	bank.AddToggle("Show spheres of influence", &m_bVisualiseBoneRadius);
	bank.AddToggle("Display impact positions", &m_bDisplayImpactPos);
	bank.AddToggle("Display impact info", &m_bDisplayImpactInfo);

	bank.AddSlider("Damage scaling curve 'k'", &m_fDamageScaleCurveConst, 0.0f, 100.0f, 0.1f);
	bank.AddSlider("Max applied damage", &m_fMaxDamage, 0.0f, 100000.0f, 0.01f);
	bank.AddSlider("Applied damage saturation limit (as fraction of max damage)", &m_fDamageSaturationLimit, 0.0f, 1.0f, 0.001f);

	bank.AddSlider("Applied damage scaling vs. impact to bone distance curve 'k'", &m_fBoneDistanceScalingCurveConst, 0.0f, 100.0f, 0.1f);

	bank.AddButton("Reload deformable metadata", Reload);
}
#endif //__BANK

#if __BANK
void CDeformableObjectManager::RenderDebug()
{
	if(!m_bEnableDebugDraw)
		return;

	StoreFocusEntAddress();

	PerBoneDebugDraw();
}

void CDeformableObjectManager::StoreFocusEntAddress()
{
	// Null the pointer in case there is no focus ped.
	m_pFocusEnt = RegdEnt(NULL);

	CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(0);
	// Early out if no entity is selected.
	if(!pFocusEntity)
		return;
	if(!pFocusEntity->GetIsTypeObject())
		return;

	// We must have a selected object by this stage:
	m_pFocusEnt = RegdEnt(pFocusEntity);
}

void CDeformableObjectManager::PerBoneDebugDraw()
{
	if(!m_bEnableDebugDraw)
		return;
	if(!m_pFocusEnt.Get())
		return;

	// Create this once to be used by all text drawing routines.
	char zStringBuffer[100];

	// Obtain the deformable data for the selected entity.
	if(!static_cast<CObject*>(m_pFocusEnt.Get())->HasDeformableData())
		return;

	CObject* pObject = static_cast<CObject*>(m_pFocusEnt.Get());
	const CDeformableObjectEntry* pDeformableData = pObject->GetDeformableData();

	// If this object is a frag, make sure it is inserted in the fragment cache. It won't have a skeleton otherwise.
	if(fragInst* pEntityFragInst = pObject->GetFragInst())
	{
		if(!pEntityFragInst->GetCached())
		{
			pEntityFragInst->PutIntoCache();
		}
	}

	// No point being in here if we have no skeleton.
	crSkeleton* pSkel = pObject->GetSkeleton();
	rmcDrawable* pDrawable = pObject->GetDrawable();
	// Disable this assert for now as it appears an object can exist without a skeleton due to them being lazily initialised.
	//Assertf(pSkel || !pObject->GetDrawable(), "Deformable object '%s' has no skeleton pointer when it probably should have.", pObject->GetModelName());
	if(!pSkel || !pDrawable)
	{
		return;
	}

	// Loop over all deformable bones in this object and render the translational limits.
	for(int i = 0; i < pDeformableData->GetNumDeformableBones(); ++i)
	{
		u16 nBoneId = pDeformableData->GetBoneId(i);
		int nBoneIdx = -1;
		pObject->GetSkeletonData().ConvertBoneIdToIndex(nBoneId, nBoneIdx);
		if(!Verifyf(nBoneIdx!=-1, "Couldn't find matching index for bone with ID=%d on object %s.", nBoneId, pObject->GetModelName()))
		{
			continue;
		}
		Mat34V boneMatrix;
		pSkel->GetGlobalMtx(nBoneIdx, boneMatrix);

		const crBoneData* pBoneData = m_pFocusEnt->GetSkeletonData().GetBoneData(nBoneIdx);
		if(AssertVerify(pBoneData))
		{
			// Highlight the position of the deformable bones with a little solid sphere and
			// show the displacement from the default position with a debug line.
			if(m_bVisualiseDeformableBones)
			{
				Vec3V vDefaultPos = Transform(m_pFocusEnt->GetCurrentPhysicsInst()->GetMatrix(), pBoneData->GetDefaultTranslation());
				grcDebugDraw::Line(vDefaultPos, boneMatrix.d(), Color_purple);
				grcDebugDraw::Sphere(vDefaultPos, 0.05f, Color_purple, false);
				grcDebugDraw::Sphere(boneMatrix.d(), 0.05f, Color_yellow, true);
			}

			// Display the per-bone data next to each bone if required.
			if(m_bDisplayBoneText)
			{
				sprintf(zStringBuffer, "%d: boneID:%d, strength:%5.3f", i, nBoneId, pDeformableData->GetBoneStrength(i));
				Vec3V vTextPos = boneMatrix.d();
				grcDebugDraw::Text(vTextPos, Color32(0xff, 0xff, 0xff, 0xff), 0, 0, zStringBuffer, true);
			}

			// Display the sphere of influence of each bone.
			if(m_bVisualiseBoneRadius)
			{
				grcDebugDraw::Sphere(boneMatrix.d(), pDeformableData->GetBoneRadius(i), Color_yellow, false);
			}

			// Render the last impacts to be processed if there were any.
			if(m_aImpactPositions.GetCount())
			{
				// Mark each impact position with a sphere.
				if(m_bDisplayImpactPos)
				{
					for(int i = 0; i < m_aImpactPositions.GetCount(); ++i)
					{
						grcDebugDraw::Sphere(m_aImpactPositions[i], 0.03f, Color_red, true);

						// Draw an arrow showing the direction of the impulse at each impact.
						Vec3V vArrowStart = m_aImpactPositions[i];
						Vec3V vArrowEnd = m_aImpactDirVecs[i];
						vArrowEnd = vArrowStart + Scale(vArrowEnd, ScalarV(0.1f));
						grcDebugDraw::Arrow(vArrowStart, vArrowEnd, 0.4f, Color_red);
					}
				}
				// Display textual info next to each impact sphere.
				if(m_bDisplayImpactInfo && m_bDisplayImpactPos)
				{
					char zStringBuffer[100];
					for(int i = 0; i < m_aImpactPositions.GetCount(); ++i)
					{
						sprintf(zStringBuffer, "impulse:%5.3f, relVel:%5.3f", m_aImpactImpulse[i], m_aImpactRelVel[i]);
						Vec3V vTextPos = m_aImpactPositions[i];
						grcDebugDraw::Text(vTextPos, Color32(0xff, 0xff, 0xff, 0xff), 0, 0, zStringBuffer, true);
					}
				}
			}

			// Draw boxes to visualise translation limits on deformable bones if they exist. If a
			// bone doesn't have translational limits on X,Y and Z, just use arrows to indicate the
			// allowed motion range on the constrained axes.
			if(m_bVisualiseLimitsTrans)
			{
				Vec3V vOffsetMax, vOffsetMin;

				const crJointData* pJointData = m_pFocusEnt->GetDrawable()->GetJointData();
				Assert(pJointData);

				bool bTransLimitX = (pBoneData->GetDofs()&(crBoneData::HAS_TRANSLATE_LIMITS)) != 0;
				bool bTransLimitY = (pBoneData->GetDofs()&(crBoneData::HAS_TRANSLATE_LIMITS)) != 0;
				bool bTransLimitZ = (pBoneData->GetDofs()&(crBoneData::HAS_TRANSLATE_LIMITS)) != 0;
				if(bTransLimitX && bTransLimitY && bTransLimitZ && pJointData)
				{
					Vec3V tmin(V_ZERO), tmax(V_ZERO);
					pJointData->GetTranslationLimits(*pBoneData, tmin, tmax);
					vOffsetMax = pBoneData->GetDefaultTranslation() + tmax;
					vOffsetMin = pBoneData->GetDefaultTranslation() + tmin;

#if DEBUG_DRAW
					grcDebugDraw::BoxOriented(vOffsetMin, vOffsetMax, m_pFocusEnt->GetCurrentPhysicsInst()->GetMatrix(), Color_blue, false);
#endif // DEBUG_DRAW
				}
				else if(pJointData)
				{
					Vec3V vTransMin(V_ZERO), vTransMax(V_ZERO);
					pJointData->GetTranslationLimits(*pBoneData, vTransMin, vTransMax);
					if(bTransLimitX)
					{
						Vec3V vTransMaxX = vTransMax;
						vTransMaxX.SetY(ScalarV(V_ZERO));
						vTransMaxX.SetZ(ScalarV(V_ZERO));

						Vec3V vTransMinX = vTransMin;
						vTransMinX.SetY(ScalarV(V_ZERO));
						vTransMinX.SetZ(ScalarV(V_ZERO));

						vOffsetMax = pBoneData->GetDefaultTranslation() + vTransMaxX;
						vOffsetMin = pBoneData->GetDefaultTranslation() + vTransMinX;

						vOffsetMax = Transform(m_pFocusEnt->GetCurrentPhysicsInst()->GetMatrix(), vOffsetMax);
						vOffsetMin = Transform(m_pFocusEnt->GetCurrentPhysicsInst()->GetMatrix(), vOffsetMin);

#if DEBUG_DRAW
						grcDebugDraw::Arrow(vOffsetMax, vOffsetMin, 0.2f, Color_red);
						grcDebugDraw::Arrow(vOffsetMin, vOffsetMax, 0.2f, Color_red);
#endif // DEBUG_DRAW
					}
					if(bTransLimitY)
					{
						Vec3V vTransMaxY = vTransMax;
						vTransMaxY.SetX(ScalarV(V_ZERO));
						vTransMaxY.SetZ(ScalarV(V_ZERO));

						Vec3V vTransMinY = vTransMin;
						vTransMinY.SetX(ScalarV(V_ZERO));
						vTransMinY.SetZ(ScalarV(V_ZERO));

						vOffsetMax = pBoneData->GetDefaultTranslation() + vTransMaxY;
						vOffsetMin = pBoneData->GetDefaultTranslation() + vTransMinY;

						vOffsetMax = Transform(m_pFocusEnt->GetCurrentPhysicsInst()->GetMatrix(), vOffsetMax);
						vOffsetMin = Transform(m_pFocusEnt->GetCurrentPhysicsInst()->GetMatrix(), vOffsetMin);

#if DEBUG_DRAW
						grcDebugDraw::Arrow(vOffsetMax, vOffsetMin, 0.2f, Color_green);
						grcDebugDraw::Arrow(vOffsetMin, vOffsetMax, 0.2f, Color_green);
#endif // DEBUG_DRAW
					}
					if(bTransLimitZ)
					{
						Vec3V vTransMaxZ = vTransMax;
						vTransMaxZ.SetX(ScalarV(V_ZERO));
						vTransMaxZ.SetY(ScalarV(V_ZERO));

						Vec3V vTransMinZ = vTransMin;
						vTransMinZ.SetX(ScalarV(V_ZERO));
						vTransMinZ.SetY(ScalarV(V_ZERO));

						vOffsetMax = pBoneData->GetDefaultTranslation() + vTransMaxZ;
						vOffsetMin = pBoneData->GetDefaultTranslation() + vTransMinZ;

						vOffsetMax = Transform(m_pFocusEnt->GetCurrentPhysicsInst()->GetMatrix(), vOffsetMax);
						vOffsetMin = Transform(m_pFocusEnt->GetCurrentPhysicsInst()->GetMatrix(), vOffsetMin);

#if DEBUG_DRAW
						grcDebugDraw::Arrow(vOffsetMax, vOffsetMin, 0.2f, Color_blue);
						grcDebugDraw::Arrow(vOffsetMin, vOffsetMax, 0.2f, Color_blue);
#endif // DEBUG_DRAW
					}
				}
			}
		} // Visualise translational limits.


	} // Loop over all deformable bones.
}
#endif // __BANK
