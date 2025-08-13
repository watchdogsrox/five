//
//
//    Filename: CompEntityModelInfo.h
//     Creator: John Whyte
//     $Author: $
//       $Date: $
//   $Revision: $
// Description: Class describing composite entity model - a model which sequences multiple simple models (e.g. rayfire object)
//
//
#ifndef INC_COMP_ENTITY_MODELINFO_H_
#define INC_COMP_ENTITY_MODELINFO_H_

// Game headers
#include "modelinfo\BaseModelInfo.h"

#include "scene/loader/mapTypes.h"

// object composed of multiple entities (in a sequence)
class CCompEntityModelInfo : public CBaseModelInfo
{
public:
	CCompEntityModelInfo() {m_type = MI_TYPE_COMPOSITE;}

	void SetStartModelHash(u32 startModelHash) { m_startModelHash = startModelHash; }
	void SetEndModelHash(u32 endModelHash) {m_endModelHash = endModelHash; }

	u32 GetStartModelHash() { return(m_startModelHash); }
	u32 GetEndModelHash() {return(m_endModelHash); }

	void SetStartImapFile(const atHashString &startFile) { m_startImapFile = startFile; }
	void SetEndImapFile(const atHashString &endFile) { m_endImapFile = endFile; }

	atHashString& GetStartImapFile(void) { return(m_startImapFile); }
	atHashString& GetEndImapFile(void)	{ return(m_endImapFile); }

	void SetPtFxAssetName(const atHashString &ptfxAssetName) { m_ptfxAssetName = ptfxAssetName; }
	atHashString& GetPtFxAssetName() { return m_ptfxAssetName; }

	u32 GetNumOfEffects(u32 animIdx) { return(m_anims[animIdx].GetEffectsData().GetCount()); }
	CCompEntityEffectsData& GetEffect(u32 animIdx, u32 idx) { return((m_anims[animIdx].GetEffectsData())[idx]);}

	// new stuff
	u32 GetNumOfAnims() { return(m_anims.GetCount()); }
	CCompEntityAnims&	GetAnims(u32 idx) { return(m_anims[idx]); }
	void CopyAnimsData(atArray<CCompEntityAnims> &animsData);

	void SetUseImapForStartAndEnd(bool bUse)	{ m_bUseImapForStartAndEnd = bUse; }
	bool GetUseImapForStartAndEnd(void)		{ return(m_bUseImapForStartAndEnd); }

protected:

	atArray <CCompEntityAnims>				m_anims;

	u32		m_startModelHash;
	u32		m_endModelHash;

	bool	m_bUseImapForStartAndEnd;

	atHashString	m_startImapFile;
	atHashString	m_endImapFile;

	atHashString	m_ptfxAssetName;
};


#endif // INC_COMP_ENTITY_MODELINFO_H_