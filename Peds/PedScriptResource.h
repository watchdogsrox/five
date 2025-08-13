#ifndef PED_SCRIPT_RESOURCE_H
#define PED_SCRIPT_RESOURCE_H

// Headers
#include "atl/hashstring.h"
#include "fwanimation/animdefines.h"
#include "fwscript/scripthandler.h"
#include "streaming/streamingrequest.h"

// Game headers
#include "ModelInfo/PedModelInfo.h"

////////////////////////////////////////////////////////////////////////////////

class CMovementModeScriptResource
{
public:

	CMovementModeScriptResource(atHashWithStringNotFinal MovementModeHash, CPedModelInfo::PersonalityMovementModes::MovementModes Type, ScriptResourceRef Reference);
	~CMovementModeScriptResource();

	bool GetIsStreamedIn() { return m_assetRequester.HaveAllLoaded(); }
	u32 GetHash() const { return m_MovementModeHash.GetHash(); }
	CPedModelInfo::PersonalityMovementModes::MovementModes GetType() const { return m_Type; }
	ScriptResourceRef GetReference() const { return m_Reference; }

private:

	void RequestAnimsFromClipSet(const fwMvClipSetId& clipSetId);
	void RequestAnims();

	atHashWithStringNotFinal m_MovementModeHash;
	CPedModelInfo::PersonalityMovementModes::MovementModes m_Type;
	ScriptResourceRef m_Reference;

	// Streaming request helper
	static const s32 MAX_REQUESTS = 24;
	strRequestArray<MAX_REQUESTS> m_assetRequester;
};

////////////////////////////////////////////////////////////////////////////////

class CMovementModeScriptResourceManager
{
public:

	// Initialise
	static void Init();

	// Shutdown
	static void Shutdown();

	// Add a resource
	static s32 RegisterResource(atHashWithStringNotFinal MovementModeHash, CPedModelInfo::PersonalityMovementModes::MovementModes Type);

	// Remove a resource
	static void UnregisterResource(ScriptResourceRef Reference);

	// Access a resource
	static CMovementModeScriptResource* GetResource(ScriptResourceRef Reference);

	// Get the appropriate reference
	static ScriptResourceRef GetReference(atHashWithStringNotFinal MovementModeHash, CPedModelInfo::PersonalityMovementModes::MovementModes Type);

private:

	// Get index
	static ScriptResourceRef GetFreeReference();

	// Find a resource index
	static s32 GetIndex(ScriptResourceRef Reference);

	typedef atArray<CMovementModeScriptResource*> Resources;
	static Resources ms_Resources;
};

////////////////////////////////////////////////////////////////////////////////

#endif // PED_SCRIPT_RESOURCE_H
