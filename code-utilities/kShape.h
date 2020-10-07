#pragma once
#include "kmath.h"
enum class ShapeType : u8
	{ BOX
	, SPHERE };
union Shape
{
	ShapeType type;
	struct 
	{
		ShapeType type;
		v3f32 lengths;
	} box;
	struct 
	{
		ShapeType type;
		f32 radius;
	} sphere;
};
struct ShapeGjkSupportData
{
	const Shape& shapeA;
	const v3f32& positionA;
	const kQuaternion& orientationA;
	const Shape& shapeB;
	const v3f32& positionB;
	const kQuaternion& orientationB;
};
internal GJK_SUPPORT_FUNCTION(shapeGjkSupport);
v3f32 shapeSupport(
	const Shape& shape, const kQuaternion& orientation, 
	const v3f32& supportDirection);
/**
 * @param hKal A temporary frame allocator would work best, as any memory needs 
 *             are entirely transient.
 */
void shapeDraw(
	const v3f32& worldPosition, const kQuaternion& orientation, 
	const Shape& shape, bool wireframe, const KrbApi* krb, 
	KAllocatorHandle hKal);
