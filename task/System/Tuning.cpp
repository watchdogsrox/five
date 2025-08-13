//
// task/System/Tuning.cpp
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#include "task/System/Tuning.h"

#include "system/param.h"
#include "bank/bkmgr.h"
#include "fwsys/fileExts.h"
#include "fwsys/gameskeleton.h"
#include "parser/visitorutils.h"

#include "task/System/Tuning_parser.h"

#include "../game/scene/ExtraMetadataDefs.h"
#include "task/Physics/TaskNMFlinch.h"
#include "task/Physics/TaskNMShot.h"
#include "task/Physics/TaskNMJumpRollFromRoadVehicle.h"
#include "task/Physics/GrabHelper.h"

#if __BANK
PARAM(tuningWidgets, "Add Tuning Manager widgets to the bank");
#endif

AI_OPTIMISATIONS()

//-----------------------------------------------------------------------------

CTuning::~CTuning()
{
	// Unregister this object, if we've got a manager. If the manager got
	// destroyed before us, there shouldn't be any need to do anything.
	if(CTuningManager::sm_Instance)
	{
		CTuningManager::sm_Instance->UnregisterTuning(*this);
	}
}

void CTuning::RegisterTuning(const char* pGroupName, const char* BANK_ONLY(pBankName), const bool nonFinalOnly, bool usePso)
{
	// Instantiate the manager if needed, and register this object.
	// Note: in general, when dealing with these objects that tend to be
	// statically constructed, there is some risk of running into issues
	// with mixing allocations from the stock allocator and the game heap.
	// I think it should be OK now at least as long as all CTuning objects are
	// static, but if it becomes a problem, we could just put them all in
	// linked list (not using memory allocation) until the manager can be
	// initialized.
	if(!CTuningManager::sm_Instance)
	{
		CTuningManager::sm_Instance = rage_new CTuningManager;
	}
	CTuningManager::sm_Instance->RegisterTuning(*this, pGroupName
#if __BANK
		, pBankName
#else
		, NULL
#endif // __BANK	
		, nonFinalOnly, usePso);
}

//-----------------------------------------------------------------------------

CTunablesGroup::CTunablesGroup(const char* groupName)
		: m_Name(groupName)
		, m_UsePso(false)
#if __BANK
		, m_Bank(NULL)
		, m_BankGroup(NULL)
#endif
{
}


CTunablesGroup::~CTunablesGroup()
{
#if __BANK
	// Destroy the widgets.
	// Note: this would be a bit funky if multiple groups used the same bank,
	// and then not all groups were destroyed at the same time. Normally
	// they would all just get destroyed together with the manager, though.
	bkBank* pBank = BANKMGR.FindBank(m_BankName.c_str());
	if(pBank)
	{
		BANKMGR.DestroyBank(*pBank);
	}
#endif	// __BANK
}


CTuningFile::~CTuningFile()
{
	for(int i = 0; i < m_Tunables.GetCount(); i++)
	{
		delete m_Tunables[i];
	}
	m_Tunables.Reset();
}


void CTunablesGroup::Load(bool BANK_ONLY(loadFromAssetsDir))
{
	// Temporarily disable registration of CTuning objects, so that the ones contained
	// in the file we are about to load don't get registered.
	CTuningManager& mgr = *CTuningManager::sm_Instance;
	Assert(mgr.m_RegistrationActive);
	mgr.m_RegistrationActive = false;

	// Always initialize up-front in case the data can't be loaded or there are missing entries in the loaded data
	for (int i = 0; i < m_Tunables.GetCount(); i++)
	{
		atHashString name = m_Tunables[i]->GetNameHashString();
		PARSER.InitObject(*m_Tunables[i]);
		m_Tunables[i]->SetNameHashString(name);
	}

	char filename[256];

	if(m_UsePso BANK_ONLY(&& !loadFromAssetsDir))
	{
		// Generate the file name for PSO loading.
		GenerateFilename(filename, sizeof(filename), kPlatformCrcPso);

		// Load the PSO file.
		fwPsoStoreLoader loader = fwPsoStoreLoader::MakeSimpleFileLoader<CTuningFile>();
		loader.GetFlagsRef().Set(fwPsoStoreLoader::RUN_POSTLOAD_CALLBACKS);
		fwPsoStoreLoadInstance inst;
		loader.Load(filename, inst);

		if(inst.IsLoaded())
		{
			// The set of tuning objects in the CTuningFile isn't necessarily complete, and we need to
			// copy the data over the existing static tuning data members in memory.
			CTuningFile* pTuning = reinterpret_cast<CTuningFile*>(inst.GetInstance());
			for(int j = 0; j < pTuning->m_Tunables.GetCount(); j++)
			{
				for (int i = 0; i < m_Tunables.GetCount(); i++)
				{
					if(m_Tunables[i] && pTuning->m_Tunables[j] && m_Tunables[i]->GetHash() == pTuning->m_Tunables[j]->GetHash())
					{
						parDeepCopy(*pTuning->m_Tunables[j], false, NULL, m_Tunables[i]);
						break;
					}
				}
			}

			loader.Unload(inst);
		}
		else
		{
			Assertf(0, "Didn't find PSO tuning file '%s'.", filename);
		}
	}
	else
	{
		FileType fileType = kCommonCrcXml;
#if __BANK
		if(loadFromAssetsDir)
		{
			fileType = kAssetXml;
		}
#endif	// __BANK
		GenerateFilename(filename, sizeof(filename), fileType);

		parTree* pTree = PARSER.LoadTree(filename, "");
		if (pTree != NULL)
		{
			if (pTree->GetRoot() != NULL)
			{
				parTreeNode* pTuningNode = pTree->GetRoot()->FindChildWithName("Tunables");
				if (Verifyf(pTuningNode != NULL, "CTunablesGroup::Load: Missing 'Tunables' node in tuning file %s", filename))
				{
					for (int i = 0; i < m_Tunables.GetCount(); i++)
					{
						parStructure* pTuningStructure = PARSER.GetStructure(m_Tunables[i]);
						if (Verifyf(pTuningStructure != NULL, "CTunablesGroup::Load: Missing structure for Tunable %s in tuning file %s", m_Tunables[i]->GetName(), filename))
						{
							for (parTreeNode::ChildNodeIterator it = pTuningNode->BeginChildren(); it != pTuningNode->EndChildren(); ++it)
							{
								parTreeNode* pNameNode = (*it)->FindChildWithName("Name");
								if (pNameNode != NULL && m_Tunables[i]->GetHash() == atStringHash(pNameNode->GetData()))
								{
									parAttribute* pTypeAttribute = (*it)->GetElement().FindAttribute("type");
									if (Verifyf(pTypeAttribute != NULL, "CTunablesGroup::Load: Missing type attribute for Tunable %s in tuning file %s", pNameNode->GetData(), filename))
									{
										parStructure* pFileStructure = PARSER.FindStructure(pTypeAttribute->GetStringValue());
										if (Verifyf(pFileStructure != NULL && pTuningStructure->IsSubclassOf(pFileStructure), "CTunablesGroup::Load: Invalid structure %s for Tunable %s in tuning file %s", pTypeAttribute->GetStringValue(), pNameNode->GetData(), filename))
										{
											pTuningStructure->ReadMembersFromTree((*it), m_Tunables[i]->parser_GetPointer());
										}
									}

									break;
								}
							}
						}
					}
				}
			}

			delete pTree;
		}
	}

	for(int i = 0; i < m_Tunables.GetCount(); i++)
	{
		if(m_Tunables[i])
		{
			m_Tunables[i]->Finalize();
		}
	}

	mgr.m_RegistrationActive = true;
}

#if !__FINAL

void CTunablesGroup::Save()
{
	char filename[256];
	GenerateFilename(filename, sizeof(filename), m_UsePso ? kAssetXml : kCommonXml);

	parTree* pTree = rage_new parTree();
	if (Verifyf(pTree != NULL, "CTunablesGroup::Save: Invalid tree in tuning file %s", filename))
	{
		parTreeNode* pRootNode = rage_new parTreeNode("CTuningFile");
		if (Verifyf(pRootNode != NULL, "CTunablesGroup::Save: Invalid root node in tuning file %s", filename))
		{
			pTree->SetRoot(pRootNode);

			parTreeNode* pTuningNode = rage_new parTreeNode("Tunables");
			if (Verifyf(pTuningNode != NULL, "CTunablesGroup::Save: Invalid tuning node in tuning file %s", filename))
			{
				pTuningNode->AppendAsChildOf(pRootNode);
				for (int i = 0; i < m_Tunables.GetCount(); i++)
				{
					parTreeNode* pTreeNode = PARSER.BuildTreeNode(m_Tunables[i], "Item");
					if (Verifyf(pTreeNode != NULL, "CTunablesGroup::Save: Invalid node for Tunable %s in tuning file %s", m_Tunables[i]->GetName(), filename))
					{
						pTreeNode->AppendAsChildOf(pTuningNode);
					}
				}
			}
		}

		if(PARSER.SaveTree(filename, "", pTree))
		{
			// On error, it's probably enough to just rely on SaveTree() to print/assert.
			Displayf("Successfully saved to '%s'.", filename);
		}

		delete pTree;
	}
}

#endif	// !__FINAL

void CTunablesGroup::FinishInitialization()
{
	Load();
}

#if __BANK

void CTunablesGroup::AddWidgets()
{
	bkBank* pBank = BANKMGR.FindBank(m_BankName.c_str());
	if(pBank == NULL)
	{
		pBank = &BANKMGR.CreateBank(m_BankName.c_str());
	}

	char filename[256];
	GenerateFilename(filename, sizeof(filename), m_UsePso ? kAssetXml : kCommonXml);
	m_BankDisplayFilename = filename;

	m_Bank = pBank;
	m_BankGroup = pBank->PushGroup(m_Name.c_str(), false);
	const char* pFileName = m_BankDisplayFilename.c_str();
	pBank->AddText("File name:", (char*)pFileName, ustrlen(pFileName) + 1, true);
	if(m_UsePso)
	{
		pBank->AddButton("Reload from PSO", datCallback(CFA1(CTunablesGroup::LoadCB), this));
		pBank->AddButton("Reload from assets dir", datCallback(CFA1(CTunablesGroup::LoadFromAssetsCB), this));
	}
	else
	{
		pBank->AddButton("Reload", datCallback(CFA1(CTunablesGroup::LoadCB), this));
	}
	pBank->AddButton("Save", datCallback(CFA1(CTunablesGroup::SaveCB), this));

	for (int i = 0; i < m_Tunables.GetCount(); i++)
    {
		pBank->PushGroup(m_Tunables[i]->GetName(), false);

		parBuildWidgetsVisitor widgets;
		widgets.m_CurrBank = pBank;
		widgets.VisitToplevelStructure(m_Tunables[i]->parser_GetPointer(), *m_Tunables[i]->parser_GetStructure());

		pBank->PopGroup();
    }

	pBank->PopGroup();
}

void CTunablesGroup::RefreshBank()
{
	if(m_Bank && m_BankGroup)
	{
		m_Bank->DeleteGroup(*m_BankGroup);
	}
	m_Bank = NULL;
	m_BankGroup = NULL;
	AddWidgets();
}
#endif //__BANK

void CTunablesGroup::GenerateFilename(char* buffOut, int buffSz, FileType fileType) const
{
	// TODO: Should probably get this from 'gameconfig.xml' or something.

	const char* pDevice = NULL;
	const char* pPath = NULL;
	const char* pExtension = NULL;
	switch(fileType)
	{
		case kAssetXml:
			Assert(!m_NonFinal);
			pDevice = CFileMgr::GetAssetsFolder();
			pPath = "export/data/tune";
			pExtension = "pso.meta";
			break;
		case kCommonCrcXml:
		case kCommonXml:
			pDevice = (fileType == kCommonCrcXml) ? "commoncrc:/" : "common:/";
#if !__FINAL
			pPath = m_NonFinal ? "non_final/tune" : "data/tune";
#else // !__FINAL
			pPath = "data/tune";
#endif // !__FINAL
			pExtension = "meta";
			break;
		case kPlatformCrcPso:
			Assert(!m_NonFinal);
			pDevice = "platformcrc:/";
			pPath = "data/tune";
			pExtension = META_FILE_EXT;
			break;
		default:
			Assertf(0, "Unsupported file type %d.", (int)fileType);
			break;
	}

#if __DEV
	if (!strcmp(m_Name, "Physics Tasks"))
	{
		extern ::rage::sysParam PARAM_nmfolder;
		PARAM_nmfolder.Get(pPath);
	}
#endif //__DEV

	// Build the base file name.
	formatf(buffOut, buffSz, "%s%s/%s.%s", pDevice, pPath, m_Name.c_str(), pExtension);

	// Switch to all lowercase, and strip any spaces. This is done
	// so the group names in the widgets can be nicely formatted,
	// while keeping the filenames simple.
	const char* inPtr = buffOut;
	char* outPtr = buffOut;
	while(1)
	{
		char c = *inPtr++;
		if(c == '\0')
		{
			break;
		}
		if(c != ' ')
		{
			if(c >= 'A' && c <= 'Z')
			{
				c = c - 'A' + 'a';
			}
			*outPtr = c;
			outPtr++;
		}
	}
	*outPtr = '\0';
}

#if __BANK

void CTunablesGroup::LoadAndUpdateWidgets(bool loadFromAssetsDir)
{
	if(m_Bank && m_BankGroup)
	{
		m_Bank->DeleteGroup(*m_BankGroup);
	}
	m_Bank = NULL;
	m_BankGroup = NULL;

	Load(loadFromAssetsDir);
	AddWidgets();
}

#endif	// __BANK

//-----------------------------------------------------------------------------

CTuningManager* CTuningManager::sm_Instance;

#if __BANK
	bkButton*	CTuningManager::ms_pCreateButton = NULL;
#endif

CTuningManager::CTuningManager()
		: m_RegistrationActive(true)
{
}


CTuningManager::~CTuningManager()
{
	// Delete all the groups.
	const int numGroups = m_TunableGroups.GetCount();
	for(int i = 0; i < numGroups; i++)
	{
		delete m_TunableGroups[i];
	}
	m_TunableGroups.Reset();
}


void CTuningManager::FinishInitialization()
{
	// Load data and add widgets.
	const int numGroups = m_TunableGroups.GetCount();
	for(int i = 0; i < numGroups; i++)
	{
		m_TunableGroups[i]->FinishInitialization();
	}

#if __BANK
	if(PARAM_tuningWidgets.Get())
	{
		CreateWidgetsInternal();
	}
#endif
}

#if __BANK

void CTuningManager::CreateWidgets()
{
	if(sm_Instance)
	{
		if(ms_pCreateButton)
		{
			ms_pCreateButton->Destroy();
			ms_pCreateButton = NULL;
		}
		else
		{
			//bank must already be setup as the create button doesn't exist so just return.
			return;
		}

		sm_Instance->CreateWidgetsInternal();
	}
}

void CTuningManager::CreateWidgetsInternal()
{
	// Load data and add widgets.
	const int numGroups = sm_Instance->m_TunableGroups.GetCount();
	for(int i = 0; i < numGroups; i++)
	{
		sm_Instance->m_TunableGroups[i]->AddWidgets();
	}
}

#endif

void CTuningManager::RegisterTuning(CTuning& tunables
		, const char* pGroupName, const char* BANK_ONLY(pBankName), const bool NOTFINAL_ONLY(bNonFinal), const bool bUsePso)
{
	if(!m_RegistrationActive)
	{
		return;
	}

	const int numGroups = m_TunableGroups.GetCount();
	CTunablesGroup* pGrp = NULL;
	for(int i = 0; i < numGroups; i++)
	{
		if(strcmp(m_TunableGroups[i]->GetName(), pGroupName) == 0)
		{
			pGrp = m_TunableGroups[i];

			// This is a bit funky, we get the bank name passed in for each
			// CTuning object, but then put them in groups and store the bank
			// name only for the group, assuming that all CTuning objects in
			// each group use the same bank name. If that's not the case, the
			// assert below will fail.
#if __BANK
			Assertf(strcmp(pGrp->GetBankName(), pBankName) == 0,
					"Tunable group '%s' bank name mismatch: '%s' and '%s'.",
					pGroupName, pBankName, pGrp->GetBankName());
#endif
			break;
		}
	}
	if(!pGrp)
	{
		pGrp = rage_new CTunablesGroup(pGroupName);
		pGrp->SetUsePso(bUsePso);
#if __BANK
		pGrp->SetBankName(pBankName);
#endif
#if !__FINAL
		pGrp->SetNonFinal(bNonFinal);
#endif // !__FINAL
		m_TunableGroups.PushAndGrow(pGrp);
	}

    pGrp->Add(tunables);
}


void CTuningManager::UnregisterTuning(CTuning& tunables)
{
	if(!m_RegistrationActive)
	{
		return;
	}

	const int numGroups = m_TunableGroups.GetCount();
	for(int i = 0; i < numGroups; i++)
	{
		m_TunableGroups[i]->DeleteIfPresent(tunables);
	}
}


void CTuningManager::Init(unsigned int initMode)
{
	if(initMode == INIT_CORE)
	{
		if(!CTuningManager::sm_Instance)
		{
			CTuningManager::sm_Instance = rage_new CTuningManager;
		}

		CTuningManager::sm_Instance->FinishInitialization();

#if __BANK
		if( bkBank *pBank = &BANKMGR.CreateBank("Tuning Manager") )
		{
			pBank->AddButton("Dump tuning list", datCallback(DebugOutputTuningList));

			if(!PARAM_tuningWidgets.Get())
			{
				ms_pCreateButton = pBank->AddButton("Create Tuning widgets", &CTuningManager::CreateWidgets);
			}
		}
#endif
	}
}


void CTuningManager::Shutdown(unsigned int shutdownMode)
{
	if(shutdownMode == SHUTDOWN_CORE)
	{
		// Not sure exactly what to do here. Deleting the CTuningManager
		// object and try to free up everything it allocated may not be
		// the best option, because the manager and some of its memory
		// may have been allocated before CTuningManager::Init(), when
		// registering the static CTuning objects.
	}
}

void CTuningManager::GetTuningIndexesFromHash(u32 uGroupHash, u32 uTuningHash, int &outGroupIdx, int &outItemIdx)
{
	for(int i = 0; i < CTuningManager::sm_Instance->m_TunableGroups.GetCount(); ++i)
	{
		const CTunablesGroup* pTunableGroup = CTuningManager::sm_Instance->m_TunableGroups[i];
		if( uGroupHash == atStringHash(pTunableGroup->GetName()) )
		{
			for(int j = 0; j < pTunableGroup->GetNumTunables(); ++j)
			{
				const CTuning* pTuning = pTunableGroup->GetTuning(j);
				if( uTuningHash == pTuning->GetHash() )
				{
					outGroupIdx = i;
					outItemIdx = j;
					return;
				}
			}
		}
	}

	outGroupIdx = outItemIdx = -1;
}

const CTuning* CTuningManager::GetTuningFromIndexes(int iGroupIdx, int iItemIdx)
{
	if( (u32)iGroupIdx < (u32)CTuningManager::sm_Instance->m_TunableGroups.GetCount() )
	{
		// GetTuning already does a range check internally
		return CTuningManager::sm_Instance->m_TunableGroups[iGroupIdx]->GetTuning(iItemIdx);
	}

	return NULL;
}

bool CTuningManager::RevertPhysicsTasks(const char *fname)
{
	NMExtraTunables* tunables = rage_new NMExtraTunables;
	if(PARSER.LoadObject(fname,NULL,*tunables))
	{
		for(int i=0;i<tunables->m_actionFlinchTunables.GetCount();++i)
		{
			CTaskNMFlinch::RevertActionSet(tunables->m_actionFlinchTunables[i].m_key);
		}
		for(int i=0;i<tunables->m_weaponFlinchTunables.GetCount();++i)
		{
			CTaskNMFlinch::RevertWeaponSet(tunables->m_weaponFlinchTunables[i].m_key);
		}
		for(int i=0;i<tunables->m_weaponShotTunables.GetCount();++i)
		{
			CTaskNMShot::RevertWeaponSets(tunables->m_weaponShotTunables[i].m_key);
		}
		for(int i=0;i<tunables->m_grabHelperTunables.GetCount();++i)
		{
			CGrabHelper::RevertTuningSet(tunables->m_grabHelperTunables[i].m_key);
		}
		for(int i=0;i<tunables->m_jumpRollTunables.GetCount();++i)
		{
			CTaskNMJumpRollFromRoadVehicle::RevertEntryPointSets(tunables->m_weaponShotTunables[i].m_key);
		}
		for(int i=0;i<tunables->m_taskSimpleTunables.GetCount();++i)
		{
			CTaskNMSimple::RevertSimpleTask(tunables->m_taskSimpleTunables[i].m_key);
		}
		for(int i=0;i<tunables->m_vehicleTypeTunables.GetCount();++i)
		{
			CTaskNMBrace::RevertVehicleOverrides(tunables->m_vehicleTypeTunables[i]);
		}
#if __BANK
		CTunablesGroup* physicsTunables = NULL;
		for(int index=0; index<CTuningManager::sm_Instance->m_TunableGroups.GetCount();index++)
		{
			if(atLiteralHashValue(CTuningManager::sm_Instance->m_TunableGroups[index]->GetName())==atLiteralHashValue("Physics Tasks"))
			{
				physicsTunables = CTuningManager::sm_Instance->m_TunableGroups[index];
				break;
			}
		}
		if(physicsTunables != NULL)
		{
			physicsTunables->RefreshBank();
		}
#endif	
		delete tunables;	
		return true;
	}
	delete tunables;	
	return false;
}

bool CTuningManager::AppendPhysicsTasks(const char *fname)
{	
	NMExtraTunables* tunables = rage_new NMExtraTunables;
	if(PARSER.LoadObject(fname,NULL,*tunables))
	{
		for(int i=0;i<tunables->m_actionFlinchTunables.GetCount();++i)
		{
			CTaskNMFlinch::AppendActionSet(&(tunables->m_actionFlinchTunables[i].m_tuningSet), tunables->m_actionFlinchTunables[i].m_key);
		}
		for(int i=0;i<tunables->m_weaponFlinchTunables.GetCount();++i)
		{
			CTaskNMFlinch::AppendWeaponSet(&(tunables->m_weaponFlinchTunables[i].m_tuningSet), tunables->m_weaponFlinchTunables[i].m_key);
		}
		for(int i=0;i<tunables->m_weaponShotTunables.GetCount();++i)
		{
			CTaskNMShot::AppendWeaponSets(&(tunables->m_weaponShotTunables[i].m_tuningSet), tunables->m_weaponShotTunables[i].m_key);
		}
		for(int i=0;i<tunables->m_grabHelperTunables.GetCount();++i)
		{
			CGrabHelper::AppendTuningSet(&(tunables->m_grabHelperTunables[i].m_tuningSet), tunables->m_grabHelperTunables[i].m_key);
		}
		for(int i=0;i<tunables->m_jumpRollTunables.GetCount();++i)
		{
			CTaskNMJumpRollFromRoadVehicle::AppendEntryPointSets(&(tunables->m_jumpRollTunables[i].m_tuningSet), tunables->m_weaponShotTunables[i].m_key);
		}
		for(int i=0;i<tunables->m_taskSimpleTunables.GetCount();++i)
		{
			CTaskNMSimple::AppendSimpleTask(&(tunables->m_taskSimpleTunables[i].m_taskSimpleEntry),tunables->m_taskSimpleTunables[i].m_key);
		}
		for(int i=0;i<tunables->m_vehicleTypeTunables.GetCount();++i)
		{
			CTaskNMBrace::AppendVehicleOverrides(tunables->m_vehicleTypeTunables[i]);
		}
#if __BANK
		CTunablesGroup* physicsTunables = NULL;
		for(int index=0; index<CTuningManager::sm_Instance->m_TunableGroups.GetCount();index++)
		{
			if(atLiteralHashValue(CTuningManager::sm_Instance->m_TunableGroups[index]->GetName())==atLiteralHashValue("Physics Tasks"))
			{
				physicsTunables = CTuningManager::sm_Instance->m_TunableGroups[index];
				break;
			}
		}
		if(physicsTunables != NULL)
		{
			physicsTunables->RefreshBank();
		}
#endif	
		delete tunables;	
		return true;
	}
	delete tunables;	
	return false;
}

#if __BANK
void CTuningManager::DebugOutputTuningList()
{
	Displayf("===============================");
	Displayf("======== Tunables List ========");
	Displayf("===============================");
#if !__NO_OUTPUT
	Displayf("\n>>>>>> [This is a tunable group]");
	Displayf(">> [This is a tunable instance]\n\n");
	for(int i = 0; i < sm_Instance->m_TunableGroups.GetCount(); ++i)
	{
		const CTunablesGroup* pTunableGroup = sm_Instance->m_TunableGroups[i];
		Displayf("\n>>>>>> '%s'", pTunableGroup->GetName());
		for(int j = 0; j < pTunableGroup->GetNumTunables(); ++j)
		{
			Displayf(">> '%s'", pTunableGroup->GetTuning(j)->GetName());
		}
	}
#else
	Displayf("CAN'T DISPLAY LIST IN A NO_OUTPUT BUILD");
#endif
	Displayf("===============================");
}
#endif

//-----------------------------------------------------------------------------

// End of file task/System/Tuning.cpp
