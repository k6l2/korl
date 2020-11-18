/**
 * Until I decide to put actual documentation here about how to use the KCPP PTU 
 * funcitonality, just refer to kgtActor for sample code demonstrating usage.
 */
#pragma once
#define KCPP_POLYMORPHIC_TAGGED_UNION
#define KCPP_POLYMORPHIC_TAGGED_UNION_EXTENDS(superStruct)
#define KCPP_POLYMORPHIC_TAGGED_UNION_PURE_VIRTUAL
/* we DO need to specify `superFunctionId` in the override macro because it is 
	definitely possible for multiple pure virtual functions to have the exact 
	same function signature! */
#define KCPP_POLYMORPHIC_TAGGED_UNION_PURE_VIRTUAL_OVERRIDE(superFunctionId)