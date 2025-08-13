#ifndef SAVEGAME_RT_STRUCTURE_H
#define SAVEGAME_RT_STRUCTURE_H

// Rage headers
#include "parser/psobuilder.h"
#include "parser/rtstructure.h"

// Game headers
#include "SaveLoad/savegame_defines.h"

class CRTStructureDataToBeSaved
{
public:
	CRTStructureDataToBeSaved() {}
	virtual ~CRTStructureDataToBeSaved() {}

	virtual void DeleteAllRTStructureData() = 0;

#if SAVEGAME_USES_XML
	virtual parRTStructure *CreateRTStructure() = 0;
#endif

#if SAVEGAME_USES_PSO
	virtual psoBuilderInstance* CreatePsoStructure(psoBuilder& builder) = 0;
#endif

#if SAVEGAME_USES_PSO
	template<typename _Type> void AddChildObject(psoBuilder& builder, psoBuilderInstance& parent, _Type*& var, const char* name);
#endif // SAVEGAME_USES_PSO
};


#if SAVEGAME_USES_PSO
template<typename _Type> void CRTStructureDataToBeSaved::AddChildObject(psoBuilder& builder, psoBuilderInstance& parent, _Type*& var, const char* name)
{
	// Create the new save data object, and call PreSave to populate it from global data
	var = rage_new _Type;
	var->PreSave();

	// Now add the new object to the PSO file we're constructing
	psoBuilderInstance* childInst = psoAddParsableObject(builder, var);

	// Get the parent object's instance data, and add a new fixup so that the parent member will point to the child object
	// we just created
	parent.AddFixup(0, name, childInst);
}
#endif // SAVEGAME_USES_PSO


#endif // SAVEGAME_RT_STRUCTURE_H


