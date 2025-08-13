//
// spurs_pm_entry:
//
//
//
#if __SPU
#include <cell/spurs/policy_module.h>
#include <spu_intrinsics.h>
#include <spu_printf.h>

#include "SpuPmMgr_pm.h"

// Description:
// Declaration for the policy module entry point. This function is not expected to return.
// Arguments:
// pKernelContext - The SPURS kernel context to be used when talking to SPURS.
// ea_simple_pm - Effective address of the policy module arguments in main memory.
void pm_main(uintptr_t pKernelContext, uint64_t ea_simple_pm, CellSpursModulePollStatus status)
			    __attribute__((__noreturn__));

// Description:
// The SPURS module entry point.
// Argument:
// _pKernelContext : The SPURS kernel context to be used when talking to SPURS.
// _argp : The pointer to the policy module arguments in main memory.
void cellSpursModuleEntryStatus(uintptr_t _pKernelContext, uint64_t _argp,
								CellSpursModulePollStatus status)
{
    static uintptr_t pKernelContext=0;
    static uint64_t  argp=0;

    pKernelContext = _pKernelContext;
    argp           = _argp;

	// Set stack pointer: adress & size (for stack checks)
	register vec_uint4 sp0 __asm__("$sp") = {PM_SPU_MEMORY_MAP_STACK_BASE,PM_SPU_MEMORY_MAP_STACK_SIZE,PM_SPU_MEMORY_MAP_STACK_SIZE,PM_SPU_MEMORY_MAP_STACK_SIZE};
	qword* sp = (qword*)si_to_ptr((qword)sp0);
	sp[0] = si_from_ptr(&sp[2]);
	sp[1] = (qword)(0);
	sp[2] = (qword)(0);

	pm_main(pKernelContext, argp, status);
}

#endif //__SPU...

