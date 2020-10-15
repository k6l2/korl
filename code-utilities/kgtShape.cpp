#include "kgtShape.h"
#include "kgtVertex.h"
internal GJK_SUPPORT_FUNCTION(kgtShapeGjkSupport)
{
	const KgtShapeGjkSupportData& data = 
		*reinterpret_cast<KgtShapeGjkSupportData*>(userData);
	const v3f32 supportA = data.positionA + 
		kgtShapeSupport(data.shapeA, data.orientA,  supportDirection);
	const v3f32 supportB = data.positionB + 
		kgtShapeSupport(data.shapeB, data.orientB, -supportDirection);
	return supportA - supportB;
}
v3f32 kgtShapeSupport(
	const KgtShape& shape, const q32& orient, const v3f32& supportDirection)
{
	switch(shape.type)
	{
		case KgtShapeType::BOX:
			return kmath::supportBox(
				shape.box.lengths, orient, supportDirection);
		case KgtShapeType::SPHERE:
			return kmath::supportSphere(shape.sphere.radius, supportDirection);
	}
	KLOG(ERROR, "shape.type {%i} is invalid!", i32(shape.type));
	return {};
}
void kgtShapeDrawBox(
	const v3f32& worldPosition, const q32& orient, const KgtShape& shape, 
	bool wireframe)
{
	g_krb->setModelXform(worldPosition, orient, {1,1,1});
	KgtVertex generatedBox[36];
	kmath::generateMeshBox(
		shape.box.lengths, generatedBox, sizeof(generatedBox), 
		sizeof(generatedBox[0]), offsetof(KgtVertex,position), 
		offsetof(KgtVertex,textureNormal));
	const KrbVertexAttributeOffsets vertexAttribs = wireframe
		? KGT_VERTEX_ATTRIBS_POSITION_ONLY
		: KGT_VERTEX_ATTRIBS_NO_COLOR;
	DRAW_TRIS(generatedBox, vertexAttribs);
}
void kgtShapeDrawSphere(
	const v3f32& worldPosition, const q32& orient, const KgtShape& shape, 
	bool wireframe, KgtAllocatorHandle hKal)
{
	/* generate sphere mesh */
	const size_t generatedSphereMeshVertexCount = 
		kmath::generateMeshCircleSphereVertexCount(16, 16);
	const size_t generatedSphereMeshBytes = 
		generatedSphereMeshVertexCount * sizeof(KgtVertex);
	KgtVertex*const generatedSphereMesh = reinterpret_cast<KgtVertex*>(
		kgtAllocAlloc(hKal, generatedSphereMeshBytes));
	kmath::generateMeshCircleSphere(
		1.f, 16, 16, generatedSphereMesh, generatedSphereMeshBytes, 
		sizeof(KgtVertex), offsetof(KgtVertex,position), 
		offsetof(KgtVertex,textureNormal));
	/* draw the sphere */
	const v3f32 sphereScale = 
		{ shape.sphere.radius, shape.sphere.radius, shape.sphere.radius };
	g_krb->setModelXform(worldPosition, orient, sphereScale);
	const KrbVertexAttributeOffsets vertexAttribOffsets = wireframe
		? KGT_VERTEX_ATTRIBS_POSITION_ONLY
		: KGT_VERTEX_ATTRIBS_NO_COLOR;
	g_krb->drawTris(
		generatedSphereMesh, generatedSphereMeshVertexCount, 
		sizeof(generatedSphereMesh[0]), vertexAttribOffsets);
	kgtAllocFree(hKal, generatedSphereMesh);
}
void kgtShapeDraw(
	const KgtShape& shape, const v3f32& worldPosition, const q32& orient, 
	bool wireframe, KgtAllocatorHandle hKal)
{
	switch(shape.type)
	{
		case KgtShapeType::BOX:
			kgtShapeDrawBox(worldPosition, orient, shape, wireframe);
			break;
		case KgtShapeType::SPHERE:
			kgtShapeDrawSphere(worldPosition, orient, shape, wireframe, hKal);
			break;
		default:
			KLOG(ERROR, "Invalid shape type! (%i)", shape.type);
			break;
	}
}
internal f32 kgtShapeTestRay(
	const KgtShape& shape, const v3f32& position, const q32& orient, 
	const v3f32& rayOrigin, const v3f32& rayNormal)
{
	switch(shape.type)
	{
		case KgtShapeType::BOX:
			return kmath::collideRayBox(
				rayOrigin, rayNormal, position, orient, shape.box.lengths);
		case KgtShapeType::SPHERE:
			return kmath::collideRaySphere(
				rayOrigin, rayNormal, position, shape.sphere.radius);
	}
	KLOG(ERROR, "shape.type {%i} is invalid!", i32(shape.type));
	return NAN32;
}
