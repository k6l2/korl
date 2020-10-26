/*
 * User code must define a global KrbApi* called `g_krb` to use this module!
 */
#pragma once
#include "kmath.h"
enum class KgtShapeType : u8
	{ SPHERE
	, BOX };
union KgtShape
{
	KgtShapeType type;
	struct 
	{
		KgtShapeType type;
		v3f32 lengths;
	} box;
	struct 
	{
		KgtShapeType type;
		f32 radius;
	} sphere;
} FORCE_SYMBOL_EXPORT;
struct KgtShapeGjkSupportData
{
	const KgtShape& shapeA;
	const v3f32& positionA;
	const q32& orientA;
	const KgtShape& shapeB;
	const v3f32& positionB;
	const q32& orientB;
};
internal GJK_SUPPORT_FUNCTION(kgtShapeGjkSupport);
internal v3f32 
	kgtShapeSupport(
		const KgtShape& shape, const q32& orient, 
		const v3f32& supportDirection);
/**
 * @param hKal A temporary frame allocator would work best, as any memory needs 
 *             are entirely transient.
 */
internal void 
	kgtShapeDraw(
		const KgtShape& shape, const v3f32& worldPosition, 
		const q32& orient, bool wireframe, KgtAllocatorHandle hKal);
/** 
 * @return NAN32 if the ray doesn't intersect with the shape 
 */
internal f32 
	kgtShapeTestRay(
		const KgtShape& shape, const v3f32& position, 
		const q32& orient, const v3f32& rayOrigin, const v3f32& rayNormal);
internal void 
	kgtShapeCalculateAabb(
		const KgtShape& shape, v3f32* o_aabbMin, v3f32* o_aabbMax, 
		const v3f32& position, const q32& orient);