// Class header
#include "Weapons/ThrownWeaponInfo.h"

// Rage headers
#include "parser/manager.h"

// Game headers
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "Objects/Object.h"
#include "Peds/Ped.h"
#include "Scene/DataFileMgr.h"
#include "scene/world/GameWorld.h"
#include "Weapons/WeaponDebug.h"

// Macro to disable optimisations if set
WEAPON_OPTIMISATIONS()

// Storage for CThrownWeaponInfo data
atArray<CThrownWeaponInfo> gThrownWeaponInfoStore;
extern atArray<CThrownWeaponInfo> gThrownWeaponInfoStore;

//////////////////////////////////////////////////////////////////////////
// CThrownWeaponInfo
//////////////////////////////////////////////////////////////////////////

bank_float CThrownWeaponInfo::ms_fDefaultProjectileMass(20.0f);

#if __BANK
int CThrownWeaponInfo::ms_iSelectedObjectIndex(0);
RegdObj CThrownWeaponInfo::ms_pSelectedObject(NULL);
Vector3 CThrownWeaponInfo::ms_vObjectPosOffset(VEC3_ZERO);
Vector3 CThrownWeaponInfo::ms_vObjectRotOffset(VEC3_ZERO);
#endif

void CThrownWeaponInfo::Init()
{
	// Ensure we are shutdown properly
	Shutdown();

	LoadThrownWeaponInfo();
}

void CThrownWeaponInfo::Shutdown()
{
	// Cleanup the array
	gThrownWeaponInfoStore.Reset();

#if __BANK
	DestroySelectedObject();
#endif
}

void CThrownWeaponInfo::LoadThrownWeaponInfo()
{
	// Iterate through the files backwards, so episodic data can override any old data
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetLastFile(CDataFileMgr::THROWNWEAPONINFO_FILE);
	while(DATAFILEMGR.IsValid(pData))
	{
		// Read in this particular file
		LoadThrownWeaponInfoXMLFile(pData->m_filename);

		// Get next filew
		pData = DATAFILEMGR.GetPrevFile(pData);
	}

	gThrownWeaponInfoStore.QSort(0, gThrownWeaponInfoStore.GetCount(), PairSort);

#if __BANK
	// Re-init the weapon widgets, 
	// which in turn initialises our widgets due to them being on the same bank
	CWeaponDebug::InitBank();
#endif
}

#if __BANK
void CThrownWeaponInfo::InitWidgets()
{
	if(gThrownWeaponInfoStore.GetCount() > 0)
	{
		bkBank* pBank = BANKMGR.FindBank("Weapons");
		if(pBank)
		{
			pBank->PushGroup("Thrown Weapons", false);
			{
				// Construct an array of object names
				atArray<const char*> aWeaponNames;
				aWeaponNames.Grow() = "None";
				for(int i = 0; i < gThrownWeaponInfoStore.GetCount(); i++)
				{
					aWeaponNames.Grow() = gThrownWeaponInfoStore[i].m_name.GetCStr();
				}

				pBank->AddCombo("Object", &ms_iSelectedObjectIndex, gThrownWeaponInfoStore.GetCount() + 1, &aWeaponNames[0], GiveThrownWeaponObject);
				pBank->AddSlider("Pos", &ms_vObjectPosOffset, -1.0f, 1.0f, 0.01f, UpdateObjectPosOffset);
				pBank->AddSlider("Rot", &ms_vObjectRotOffset, -180.0f, 180.0f, 1.0f, UpdateObjectRotOffset);
				pBank->AddSlider("Mass", &ms_fDefaultProjectileMass, 0.1f, 50.0f, 0.1f);
				pBank->AddButton("Reload File", Init);
			}
			pBank->PopGroup();
		}
	}
}
#endif // __BANK

CThrownWeaponInfo* CThrownWeaponInfo::GetThrownWeaponInfo(u32 uNameHash)
{
	CThrownWeaponInfo tempInfo;
	tempInfo.m_uNameHash = uNameHash;

	int iIndex = gThrownWeaponInfoStore.BinarySearch(tempInfo);
	if(iIndex != -1)
	{
		return &gThrownWeaponInfoStore[iIndex];
	}

	return NULL;
}

CThrownWeaponInfo* CThrownWeaponInfo::GetThrownWeaponInfoFromModelIndex(strLocalIndex iModelIndex)
{
	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(iModelIndex));
	if(pModelInfo)
	{
		return GetThrownWeaponInfo(pModelInfo->GetHashKey());
	}

	return NULL;
}

CThrownWeaponInfo::CThrownWeaponInfo()
: m_uNameHash(0)
, m_matOffset(M34_IDENTITY)
, m_bIsDisabled(false)
{
}

void CThrownWeaponInfo::GetOffsetMatrix(const CObject* pObj, Matrix34& rv) const
{
	Vector3 vModelBoundsMin(pObj->GetBoundingBoxMin());
	Vector3 vModelBoundsMax(pObj->GetBoundingBoxMax());

	Vector3 vGripOffset(vModelBoundsMin);
	vGripOffset += vModelBoundsMax;
	vGripOffset *= -0.5f;

	rv = m_matOffset;
	rv.Transform3x3(vGripOffset);
	rv.d += vGripOffset;
}

void CThrownWeaponInfo::LoadThrownWeaponInfoXMLFile(const char* szFileName)
{
	parTree* pTree = PARSER.LoadTree(szFileName, "xml");
	if(pTree)
	{
		parTreeNode* pRootNode = pTree->GetRoot();
		if(pRootNode)
		{
			parTreeNode::ChildNodeIterator i = pRootNode->BeginChildren();
			for(; i != pRootNode->EndChildren(); ++i)
			{
				if(stricmp((*i)->GetElement().GetName(), "object") == 0)
				{
					parAttribute* pAttr = (*i)->GetElement().FindAttributeAnyCase("name");
					if(pAttr)
					{
						// Convert the name into a hash key
						u32 uNameHash = atStringHash(pAttr->GetStringValue());

						// If we already have an entry with this name, re-use it
						CThrownWeaponInfo* pThrownWeaponInfo = GetThrownWeaponInfo(uNameHash);
						if(!pThrownWeaponInfo)
						{
							// Otherwise allocate a new entry
							pThrownWeaponInfo = &gThrownWeaponInfoStore.Grow();
						}

						// Store a copy of the name for debugging
						BANK_ONLY(pThrownWeaponInfo->m_name.SetFromString(pAttr->GetStringValue()));

						pThrownWeaponInfo->m_uNameHash   = uNameHash;
						pThrownWeaponInfo->m_bIsDisabled = (*i)->GetElement().FindAttributeBoolValue("disablepickup", false);

						enum ObjectHashes
						{
							OH_Offset		= 0xA13C79EF,		// "offset"
							OH_RotOffset	= 0x36F9E805,		// "rotoffset"
						};

						// Iterate over the children and fill in the info
						parTreeNode::ChildNodeIterator j = (*i)->BeginChildren();
						for(; j != (*i)->EndChildren(); ++j)
						{
							u32 nHash = atStringHash((*j)->GetElement().GetName());
							switch(nHash)
							{
							case OH_Offset:
							case OH_RotOffset:
								{
									Vector3 vAttr;
									vAttr.x = (*j)->GetElement().FindAttributeFloatValue("x", 0.0f);
									vAttr.y = (*j)->GetElement().FindAttributeFloatValue("y", 0.0f);
									vAttr.z = (*j)->GetElement().FindAttributeFloatValue("z", 0.0f);

									if(nHash == OH_Offset)
									{
										pThrownWeaponInfo->m_matOffset.d = vAttr;
									}
									else
									{
										// Input angles are in degrees, so convert them
										vAttr.x = ( DtoR * vAttr.x);
										vAttr.y = ( DtoR * vAttr.y);
										vAttr.z = ( DtoR * vAttr.z);

										// Convert the Euler angles into a matrix
										pThrownWeaponInfo->m_matOffset.FromEulersXYZ(vAttr);
									}
								}
								break;

							default:
								Assertf(0, "Unknown ThrownWeaponInfo:Object identifier [%s]:[0x%X]",(*j)->GetElement().GetName(), nHash);
								break;
							}
						}
					}
				}
				else
				{
					Assertf(0, "%s:Unknown ThrownWeaponInfo identifier", (*i)->GetElement().GetName());
				}
			}
		}

		delete pTree;
	}
}

#if __BANK
void CThrownWeaponInfo::GiveThrownWeaponObject()
{
	DestroySelectedObject();

	int iIndex = ms_iSelectedObjectIndex - 1;
	if(iIndex >= 0 && iIndex < gThrownWeaponInfoStore.GetCount())
	{
		fwModelId modelId;
		CModelInfo::GetBaseModelInfoFromHashKey(gThrownWeaponInfoStore[iIndex].m_uNameHash, &modelId);

		if(modelId.IsValid())
		{
			const CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(modelId);
			if(pModelInfo)
			{
				ms_pSelectedObject = CObjectPopulation::CreateObject(modelId, ENTITY_OWNEDBY_RANDOM, true);
				if(ms_pSelectedObject)
				{
					CPed* pPed = CGameWorld::FindLocalPlayer();

					//
					// Calculate a position to create the object
					//

					Matrix34 matObj;
					matObj.Identity();

					Vector3 vCreateOffset = camInterface::GetFront();
					vCreateOffset.z = 0.0f;
					vCreateOffset  *= 1.0f + pModelInfo->GetBoundingSphereRadius();

					if(camInterface::GetDebugDirector().IsFreeCamActive())
					{
						matObj.d = camInterface::GetPos() + vCreateOffset;
						matObj.Set3x3(camInterface::GetMat());
					}
					else if(pPed)
					{
						matObj.d = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) + vCreateOffset;
						matObj.Set3x3(MAT34V_TO_MATRIX34(pPed->GetMatrix()));
					}

					ms_pSelectedObject->SetMatrix(matObj);
					
					fwInteriorLocation	interiorLocation;

					if(!camInterface::GetDebugDirector().IsFreeCamActive() && pPed)
					{
						// Try to copy the interior data from the ped
						interiorLocation = pPed->GetInteriorLocation();
					}
					else
						interiorLocation.MakeInvalid();

					CGameWorld::Add(ms_pSelectedObject, interiorLocation);

					// Set the debug offset vars to the current vars
					ms_vObjectPosOffset   = gThrownWeaponInfoStore[iIndex].m_matOffset.d;
					ms_vObjectRotOffset   = gThrownWeaponInfoStore[iIndex].m_matOffset.GetEulers();
					ms_vObjectRotOffset.x = fwAngle::LimitDegreeAngle( RtoD * ms_vObjectRotOffset.x);
					ms_vObjectRotOffset.y = fwAngle::LimitDegreeAngle( RtoD * ms_vObjectRotOffset.y);
					ms_vObjectRotOffset.z = fwAngle::LimitDegreeAngle( RtoD * ms_vObjectRotOffset.z);
				}
			}
		}
	}
}

void CThrownWeaponInfo::UpdateObjectPosOffset()
{
	int iIndex = ms_iSelectedObjectIndex - 1;
	if(iIndex >= 0 && iIndex < gThrownWeaponInfoStore.GetCount())
	{
		gThrownWeaponInfoStore[iIndex].m_matOffset.d = ms_vObjectPosOffset;
	}
}

void CThrownWeaponInfo::UpdateObjectRotOffset()
{
	int iIndex = ms_iSelectedObjectIndex - 1;
	if(iIndex >= 0 && iIndex < gThrownWeaponInfoStore.GetCount())
	{
		Vector3 vRotOffset(ms_vObjectRotOffset);
		vRotOffset.x = ( DtoR * vRotOffset.x);
		vRotOffset.y = ( DtoR * vRotOffset.y);
		vRotOffset.z = ( DtoR * vRotOffset.z);

		// Convert the Euler angles into a matrix
		gThrownWeaponInfoStore[iIndex].m_matOffset.FromEulersXYZ(vRotOffset);
	}
}

void CThrownWeaponInfo::DestroySelectedObject()
{
	if(ms_pSelectedObject)
	{
		CGameWorld::Remove(ms_pSelectedObject);
		CObjectPopulation::DestroyObject(ms_pSelectedObject);
		ms_pSelectedObject = NULL;
	}
}
#endif
