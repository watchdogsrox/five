#ifndef SAVEGAME_MIGRATION_DATA_CODE_H
#define SAVEGAME_MIGRATION_DATA_CODE_H

// Game headers
#include "SaveLoad/savegame_defines.h"

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES

// Forward declarations
namespace rage {
 	class parRTStructure;
	class parTree;
	class psoBuilder;
	class psoBuilderInstance;
 	class psoFile;
 	class psoStruct;
}

class CSimpleVariablesSaveStructure;
class CMiniMapSaveStructure;
class CPlayerPedSaveStructure;
class CPlayerPedSaveStructure_Migration;
class CExtraContentSaveStructure;
class CStatsSaveStructure;
class CStatsSaveStructure_Migration;
class CStuntJumpSaveStructure;
class CRadioStationSaveStructure;
class CSaveGarages;
class CSaveGarages_Migration;
class CSaveDoors;
class CCameraSaveStructure;
class CCameraSaveStructure_Migration;


class CSaveGameMigrationData_Code
{
public:
	CSaveGameMigrationData_Code() { Initialize(); }
	~CSaveGameMigrationData_Code() { Delete(); }

	void Initialize();
	void Create();
	void Delete();

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	void ReadDataFromPsoFile(psoFile* psofile);

	parRTStructure *ConstructRTSToSave();
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
	bool ReadDataFromParTree(parTree *pParTree);

	psoBuilderInstance *ConstructPsoDataToSave(psoBuilder *pPsoBuilder);
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

	s32 GetHashOfNameOfLastMissionPassed();
	float GetSinglePlayerCompletionPercentage();

private:

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	void HandleTranslationsFromLoadedPsoToSavedXmlData();

	template<typename _Type> void LoadSingleObject(_Type *pObj, const char* name, psoStruct& root);	
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES

#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
	void HandleTranslationsFromLoadedXmlToSavedPsoData();

	template<typename _Type> void AddChildObject(psoBuilder& builder, psoBuilderInstance& parent, const _Type* var, const char* name);
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES

private:
	CSimpleVariablesSaveStructure *m_pSimpleVariablesSaveStructure;
	CMiniMapSaveStructure *m_pMiniMapSaveStructure;
	CPlayerPedSaveStructure *m_pPlayerPedSaveStructure_Pso;
	CPlayerPedSaveStructure_Migration *m_pPlayerPedSaveStructure_Xml;
	CExtraContentSaveStructure *m_pExtraContentSaveStructure;
	CStatsSaveStructure *m_pStatsSaveStructure_Pso;
	CStatsSaveStructure_Migration *m_pStatsSaveStructure_Xml;
	CStuntJumpSaveStructure *m_pStuntJumpSaveStructure;
	CRadioStationSaveStructure *m_pRadioStationSaveStructure;
	CSaveGarages *m_pGaragesSaveStructure_Pso;
	CSaveGarages_Migration *m_pGaragesSaveStructure_Xml;
	CSaveDoors *m_pDoorsSaveStructure;
	CCameraSaveStructure *m_pCameraSaveStructure_Pso;
	CCameraSaveStructure_Migration *m_pCameraSaveStructure_Xml;

#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
	parRTStructure *m_pRTSToSave;
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES
};


#if __ALLOW_EXPORT_OF_SP_SAVEGAMES
// Based on CSaveGameBuffer::LoadSingleObject
template<typename _Type> void CSaveGameMigrationData_Code::LoadSingleObject(_Type *pObj, const char* name, psoStruct& root)
{
	psoMember mem = root.GetMemberByName(atLiteralHashValue(name));
	if (!mem.IsValid())
	{
		return;
	}

	psoStruct str = mem.GetSubStructure();
	if (!str.IsValid())
	{
		return;
	}

	//	_Type obj;
	psoLoadObject(str, *pObj);
	//	obj.PostLoad();	//	I don't want to overwrite the game data with the save data that I've loaded for exporting
}
#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES


#if __ALLOW_IMPORT_OF_SP_SAVEGAMES
// Based on CRTStructureDataToBeSaved::AddChildObject
template<typename _Type> void CSaveGameMigrationData_Code::AddChildObject(psoBuilder& builder, psoBuilderInstance& parent, const _Type* var, const char* name)
{
	// Add the object to the PSO file we're constructing
	psoBuilderInstance* childInst = psoAddParsableObject(builder, var);

	// Get the parent object's instance data, and add a new fixup so that the parent member will point to the child object
	parent.AddFixup(0, name, childInst);
}
#endif // __ALLOW_IMPORT_OF_SP_SAVEGAMES


#endif // __ALLOW_EXPORT_OF_SP_SAVEGAMES || __ALLOW_IMPORT_OF_SP_SAVEGAMES

#endif // SAVEGAME_MIGRATION_DATA_CODE_H
