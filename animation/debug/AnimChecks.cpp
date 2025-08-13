// 
// animation/debug/AnimChecks.cpp 
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved. 
// 

#if __DEV

// Game includes
#include "fwanimation/animmanager.h"
#include "animation/debug/AnimChecks.h"
#include "animation/debug/AnimDebug.h"
#include "Peds/Ped.h"
#include "streaming/streaming.h"			// For CStreaming::LoadAllRequestedObjects(), etc.
#include "Task/Combat/TaskCombatMelee.h"

// Rage includes
#include "crmetadata/properties.h"
#include "crmetadata/propertyattributes.h"
#include "crmetadata/tagiterators.h"

ANIM_OPTIMISATIONS()

CAnimChecks::CAnimChecks()
{

}
CAnimChecks::~CAnimChecks()
{

}

// Compares the bone positions in two anims at the specified phases, and check if all bones are within given distance
bool
CAnimChecks::CompareBonePositions(eAnimBoneTag iBoneTag, const crClip *pClip1, const crClip *pClip2, float fPhase1, float fPhase2, float fMaxBonePosDiff)
{
	CPed * pPed = FindPlayerPed();
	const crSkeletonData & skelData = pPed->GetSkeletonData();

	Matrix34 mBoneMat1, mBoneMat2;

	fwAnimManager::GetObjectMatrix(&skelData, pClip1, fPhase1, (u16)iBoneTag, mBoneMat1);
	fwAnimManager::GetObjectMatrix(&skelData, pClip2, fPhase2, (u16)iBoneTag, mBoneMat2);

	Vector3 vDiff = mBoneMat1.d - mBoneMat2.d;
	float fDist = vDiff.Mag();

	return (fDist < fMaxBonePosDiff);
}

// Compares the bone positions in two anims at the specified phases, and check if all bones are within given distance
bool
CAnimChecks::ComparePoses(const crClip *pClip1, const crClip *pClip2, float fPhase1, float fPhase2, float fMaxBonePosDiff)
{
	// Just using a selection of the bones.  Might want to add to this, or have specific ones for other tests.
	eAnimBoneTag iBonesToCompare[] =
	{
		BONETAG_L_THIGH,
		BONETAG_L_CALF,
		BONETAG_L_FOOT,

		BONETAG_R_THIGH,
		BONETAG_R_CALF,
		BONETAG_R_FOOT,

		BONETAG_L_CLAVICLE,	
		BONETAG_L_UPPERARM,
		BONETAG_L_FOREARM,
		BONETAG_L_HAND,

		BONETAG_R_CLAVICLE,	
		BONETAG_R_UPPERARM,
		BONETAG_R_FOREARM,
		BONETAG_R_HAND,

		BONETAG_PELVIS,
		BONETAG_SPINE0,
		BONETAG_SPINE1,
		BONETAG_SPINE2,
		BONETAG_SPINE3,
		BONETAG_NECK,
		BONETAG_HEAD,

		(eAnimBoneTag)-1
	};

	int b=0;
	while(iBonesToCompare[b] != -1)
	{
		bool bOk = CompareBonePositions(iBonesToCompare[b++], pClip1, pClip2, fPhase1, fPhase2, fMaxBonePosDiff);

		if(!bOk)
			return false;
	}

	return true;
}


int
CAnimChecks::WhichFootIsForwards(const crClip *pClip, float fPhase, float fRequiredDiff)
{
	CPed * pPed = FindPlayerPed();
	const crSkeletonData & skelData = pPed->GetSkeletonData();

	Matrix34 mLeftFootMat, mRightFootMat;
	fwAnimManager::GetObjectMatrix(&skelData, pClip, fPhase, (u16)BONETAG_L_FOOT, mLeftFootMat);
	fwAnimManager::GetObjectMatrix(&skelData, pClip, fPhase, (u16)BONETAG_R_FOOT, mRightFootMat);

	//**********************************************************************************************
	// These anims are authored orientated along the -Y axis.  Therefore the leading foot's
	// Y-position should be smaller than the trailing foot's Y.
	//**********************************************************************************************

	if(mLeftFootMat.d.y < mRightFootMat.d.y - fRequiredDiff)
	{
		return 1;
	}
	else if(mRightFootMat.d.y < mLeftFootMat.d.y - fRequiredDiff)
	{
		return -1;
	}
	else
	{
		return 0;
	}
}


// Counts how many complete walk-cycles there are in this anim
int
CAnimChecks::CountNumWalkLoops(const crClip *pClip)
{
	static const float fFootSeparation = 0.125f;
	static const float fTestIntervalInSecs = 0.0125f;	// Has to be pretty low to work on v fast anims like sprints

	float fNumTests = pClip->GetDuration() / fTestIntervalInSecs;
	float fPhaseInc = 1.0f / fNumTests;

	float fPhase = 0.0f;

	int iNumStrides = 0;
	int iCurrentFootForwards = WhichFootIsForwards(pClip, fPhase, fFootSeparation);

	animAssertf(iCurrentFootForwards != 0, "Couldn't determine which foot is forwards at start of the anim!");
	animAssertf(iCurrentFootForwards == -1, "The right foot is forwards at start of the anim!");

	while(fPhase < 1.0f)
	{
		fPhase += fPhaseInc;
		if(fPhase >= 1.0f)
			break;
		int iNextFootForwards = WhichFootIsForwards(pClip, fPhase, fFootSeparation);

		if(iNextFootForwards != 0 && iCurrentFootForwards != 0 && iNextFootForwards != iCurrentFootForwards)
		{
			iCurrentFootForwards = iNextFootForwards;
			iNumStrides++;
		}
	}

	int iNumCycles = iNumStrides/2;
	return iNumCycles;
}

float
CAnimChecks::GetTotalTurnAmount(const crClip *pClip)
{
	// Get the total Z rotation of the anim
	Quaternion rot;
	Vector3 rotEulers;

	rot.Identity();
	if(pClip)
	{
		rot = fwAnimHelpers::GetMoverTrackRotation(*pClip, 0.f);
	}
	rot.ToEulers(rotEulers);
	float fStart = rotEulers.z;

	rot.Identity();
	if(pClip)
	{
		rot = fwAnimHelpers::GetMoverTrackRotation(*pClip, 1.f);
	}
	rot.ToEulers(rotEulers);
	float fEnd = rotEulers.z;

	float fTurnAmount = fEnd - fStart;
	return fTurnAmount;
}

#define SEPARATOR			'/'
#define SEPARATOR_STRING	"/"
#define OTHER_SEPARATOR	'\\'

void RemoveExtensionFromPath(char *dest,int destSize,const char *src) {
	if (dest != src)
		safecpy(dest,src,destSize);
	int sl = StringLength(dest);
	while (sl && dest[sl-1] != SEPARATOR && dest[sl-1] != OTHER_SEPARATOR) {
		--sl;
		if (dest[sl] == '-') {
			dest[sl] = 0;
			break;
		}
	}
}

// Test all anims
bool CAnimChecks::TestAllAnims()
{
	static bool bAnimDictIndexAndHashKey = false;
	if (bAnimDictIndexAndHashKey)
	{
		animDebugf1("dictionary name,  anim name, dictionary streaming index, anim hash key \n");
	}

	static bool bMoverTest = false;
	if (bMoverTest)
	{
		animDebugf1("dictionary name,  anim name, track type, displacement\n");
	}

	//s32 aCompressionCount = 0;
	//s32 defaultCompressionCount = 0;
	//s32 otherCompressionCount = 0;
	s32 totalClipCount = 0;
	int invalidClipCount = 0;
	int invalidAnimCount = 0;
	//int invalidClipFileCount = 0;
	//int invalidAnimFileCount = 0;
	//int animGreaterThan64Count = 0;

	int clipDictionaryCount = fwAnimManager::GetSize();
	for (int i = 0; i<clipDictionaryCount; i++)
	{
		// Is the current slot valid
		if (fwAnimManager::IsValidSlot(strLocalIndex(i)))
		{
			if ( CStreaming::IsObjectInImage(strLocalIndex(i), fwAnimManager::GetStreamingModuleId()) )
			{
				fwAnimManager::StreamingRequest(strLocalIndex(i), STRFLAG_FORCE_LOAD);
				CStreaming::LoadAllRequestedObjects();

				if ( CStreaming::HasObjectLoaded(strLocalIndex(i), fwAnimManager::GetStreamingModuleId()) )
				{
					animDebugf1("slot [%d] = %s, \n", i, fwAnimManager::GetName(strLocalIndex(i)));

					int clipCount = fwAnimManager::CountAnimsInDict(i);
					for (int j = 0; j<clipCount; j++)
					{
						const crClip* pClip = fwAnimManager::GetClipInDict(i, j);
						if (pClip)
						{			
							if (bAnimDictIndexAndHashKey)
							{
								animDebugf1("%s,  %s, %d, %d \n", fwAnimManager::GetName(strLocalIndex(i)), pClip->GetName(), i, fwAnimManager::GetAnimHashInDict(i, j));
							}

							totalClipCount++;
							bool bSaveClip = false;

							crTags* pTags = const_cast<crTags*>(pClip->GetTags());
							if(pTags)
							{
								int removeCount = 0;	
								crTagIterator it(*pTags, 0.0f, 100.0f);
								while(*it)
								{
									const crTag* pTag = *it;
									if (pTag->GetStart() > 1.0f 
										&& pTag->GetMid() > 1.0f
										&& pTag->GetEnd() > 1.0f )
									{
										++removeCount;
									}

									++it;
								}
								
								for (int j=0; j<removeCount; j++)
								{
									crTagIterator it(*pTags, 0.0f, 100.0f);
									while(*it)
									{
										crTag* pTag = const_cast<crTag*>(*it);
										if (pTag->GetStart() > 1.0f 
											&& pTag->GetMid() > 1.0f
											&& pTag->GetEnd() > 1.0f )
										{
											bSaveClip = true;

											if (pTags->RemoveTag(*pTag))
											{
													animDebugf1("REMOVE '%s', '%s', %4.2f, %4.2f, %4.2f, %d\n", 
													fwAnimManager::GetName(strLocalIndex(i)), 
													pClip->GetName(),
													pTag->GetStart(),
													pTag->GetMid(),
													pTag->GetEnd(),
													0);
											}
										}
										++it;
									}
								}
								
								{
								crTagIterator it(*pTags, 0.0f, 100.0f);
								while(*it)
								{
									const crTag* pTag = *it;
									if (pTag->GetStart()==pTag->GetMid()
										&& pTag->GetStart()==pTag->GetEnd())
									{
									}
									else
									{
										animDebugf1("BAD (%d) '%s', '%s', %4.2f, %4.2f, %4.2f, %d\n", 
											pTag->GetKey(),
											fwAnimManager::GetName(strLocalIndex(i)), 
											pClip->GetName(),
											pTag->GetStart(),
											pTag->GetMid(),
											pTag->GetEnd(),
											0);
									}
									++it;
								}
								}

								{
								crTagIterator it(*pTags, 0.0f, 100.0f);
								while(*it)
								{	
									crTag* pTag = const_cast<crTag*>(*it);
											{
												bSaveClip = true;
												pTag->SetStartEnd(pTag->GetStart(), pTag->GetEnd());
											}
									++it;
								}
								}

								if (bSaveClip)
								{
									const char *pLocalPath = CDebugClipDictionary::FindLocalDirectoryForDictionary( CDebugClipDictionary::ms_AssetFolders[CDebugClipDictionary::ms_AssetFolderIndex], fwAnimManager::GetName(strLocalIndex(i)));												
									animAssert(pLocalPath);
									
									if(pLocalPath && !stricmp(pLocalPath,""))
									{
										const char* p_fullyQualifiedName = pClip->GetName();

										// Remove the path name leaving the filename and the extension
										const char* fileNameAndExtension = p_fullyQualifiedName;
										fileNameAndExtension = ASSET.FileName(p_fullyQualifiedName);

										// Remove the extension leaving just the filename
										char fileName[256];
										ASSET.RemoveExtensionFromPath(fileName, 256, fileNameAndExtension);

										char clipFileNameAndPath[512];
										sprintf(clipFileNameAndPath, "%s\\%s.clip", pLocalPath, fileName);

										crClip * pLoadedClip = crClip::AllocateAndLoad(clipFileNameAndPath, NULL);
										crAnimation* pAnim = NULL;
										if (pLoadedClip)
										{
											//create a clone of the in memory clip, then set the anim and save that out - saves us messing up our in memory copy
											crClip* pNewClip = pClip->Clone();

											//get the animation to add to the clip
											switch(pNewClip->GetType())
											{
											case crClip::kClipTypeAnimation:
											case crClip::kClipTypeAnimationExpression:
												{
													crClipAnimation* pClipAnim = static_cast<crClipAnimation*>(pNewClip);

													atString animFileNameAndPath(clipFileNameAndPath);
													animFileNameAndPath.Replace(".clip", ".anim");
													pAnim =  crAnimation::AllocateAndLoad(animFileNameAndPath);						
													if (pAnim)
													{
														pClipAnim->SetAnimation(*pAnim);
													}
													else
													{
														Displayf("Unsuccessfully loaded anim %s\n", animFileNameAndPath.c_str());
													}
												}
												break;

											default:
												break;
											}

											static bool bSaveMe = true;
											if (bSaveMe)
											{
												//bSaveMe = false;
												if (pNewClip->Save(clipFileNameAndPath))
												{
													animDebugf1("Successfully saved %s\n", clipFileNameAndPath);
												}
												else
												{
													animDebugf1("%s failed to save! Is it checked out of perforce", clipFileNameAndPath);
												}
											}

											delete pLoadedClip;
											delete pNewClip;
											if (pAnim)
											{
												pAnim->Shutdown();
												pAnim = NULL;
											}
										}
										else
										{
											animDebugf1("Failed to load clip %s\n", clipFileNameAndPath);
										}
									} //if(pLocalPath && pLocalPath != "")
								}
							} //if(pTags)

							/*
							if (bMoverTest)
							{
								crFrameDofVector3 moverTransDof;
								moverTransDof.SetTrack(kTrackMoverTranslation);
								moverTransDof.SetId(0);
								pClip->Composite(moverTransDof, 1.0f*pClip->GetDuration(), 0.0f);
								Vector3 moverTranslation = moverTransDof.GetVector3();
								float moverDisplacement = moverTranslation.Mag();
								animDebugf1("%s,  %s, TRACK_MOVER_TRANSLATION,  %f\n", fwAnimManager::GetName(i), pClip->GetName(), moverDisplacement);

								crFrameDofVector3 rootTransDof;
								rootTransDof.SetTrack(kTrackBoneTranslation);
								rootTransDof.SetId(0);
								pClip->Composite(rootTransDof, 1.0f*pClip->GetDuration(), 0.0f);
								Vector3 rootTranslation = rootTransDof.GetVector3();
								float rootDisplacement = rootTranslation.Mag();
								animDebugf1("%s, %s, TRACK_BONE_TRANSLATION, %f\n", fwAnimManager::GetName(i), pClip->GetName(), rootDisplacement);
							}

							// 
							const crClipAnimation* pClipAnim = static_cast<const crClipAnimation*>(pClip);
							if (pClipAnim && pClipAnim->GetAnimation())
							{
								//animDebugf1("The following clip has an valid anim %s %s = %6.4f s\n", fwAnimManager::GetName(i), pClip->GetName(), pClip->GetDuration());
							}
							else
							{
								invalidAnimCount++;
								animDebugf1("The following clip has an invalid anim %s %s = %6.4f s\n", fwAnimManager::GetName(i), pClip->GetName(), pClip->GetDuration());
							}
*/
						}
						else
						{
							animDebugf1("Invalid clip -> %d, %d\n", i, j);
							invalidClipCount++;
						}
					}
				}
			}
		}
	}

	animDebugf1("clipDictionaryCount = %d \n", clipDictionaryCount);
	animDebugf1("totalClipCount = %d \n", totalClipCount);
	
	//animDebugf1("defaultCompressionCount = %d \\ %d \n", defaultCompressionCount, totalClipCount);
	//animDebugf1("aCompressionCount = %d \\ %d \n", aCompressionCount, totalClipCount);
	//animDebugf1("otherCompressionCount = %d \\ %d \n", otherCompressionCount, totalClipCount);

	animDebugf1("invalidClipCount = %d \\ %d \n", invalidClipCount, totalClipCount);
	//animDebugf1("invalidClipFileCount = %d \\ %d \n", invalidClipFileCount, totalClipCount);
	animDebugf1("invalidAnimCount = %d \\ %d \n", invalidAnimCount, totalClipCount);
	//animDebugf1("invalidAnimFileCount = %d \\ %d \n", invalidAnimFileCount, totalClipCount);
	//animDebugf1("animGreaterThan64Count = %d \\ %d \n", animGreaterThan64Count, totalClipCount);

	return true;
}

#endif	// __DEV
