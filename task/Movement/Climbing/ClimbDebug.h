#ifndef CLIMB_DEBUG_H
#define CLIMB_DEBUG_H

// Forward declarations
namespace rage { class bkBank; }
class CPed;

//////////////////////////////////////////////////////////////////////////
// CClimbDebug
//////////////////////////////////////////////////////////////////////////

#if __BANK

class CClimbDebug
{
public:

	// Setup the RAG widgets for climbing
	static void SetupWidgets(bkBank& bank);

	// Debug function for shared systems
	static void RenderDebug();

private:

	//
	// Members
	//
};

#endif // __BANK

#endif // CLIMB_DEBUG_H
