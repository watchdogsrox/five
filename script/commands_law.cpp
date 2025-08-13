#include "commands_law.h"
#include "game/Wanted.h"
#include "script/wrapper.h"

namespace law_commands
{

//-----------------------------------------------------------------------------

bool CommandGetLawWitnessRequired()
{
	return CCrimeWitnesses::GetWitnessesNeeded();
}


void CommandSetLawWitnessRequired(bool b)
{
	CCrimeWitnesses::SetWitnessesNeeded(b);
}


scrVector CommandGetLawWitnessReportPosition()
{
	return scrVector(CCrimeWitnesses::GetWitnessReportPosition());
}


void CommandSetLawWitnessReportPosition(const scrVector & pos)
{
	const Vec3V posV = pos;
	CCrimeWitnesses::SetWitnessReportPosition(posV);
}


void SetupScriptCommands()
{
	SCR_REGISTER_UNUSED(GET_LAW_WITNESS_REQUIRED,0x3d9dafd22ef2b25f,	CommandGetLawWitnessRequired);
 	SCR_REGISTER_UNUSED(SET_LAW_WITNESS_REQUIRED,0xef963098f0dcef7a,	CommandSetLawWitnessRequired);
 	SCR_REGISTER_UNUSED(GET_LAW_WITNESS_REPORT_POSITION,0x0bfef7fab55bff28,	CommandGetLawWitnessReportPosition);
 	SCR_REGISTER_UNUSED(SET_LAW_WITNESS_REPORT_POSITION,0x5f5730160d6a0beb,	CommandSetLawWitnessReportPosition);
}

//-----------------------------------------------------------------------------

}	// namespace law_commands

// End of file 'script/commands_law.cpp'.
