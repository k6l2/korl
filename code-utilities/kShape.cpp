#include "kShape.h"
#include "kVertex.h"
internal GJK_SUPPORT_FUNCTION(shapeGjkSupport)
{
	const ShapeGjkSupportData& data = 
		*reinterpret_cast<ShapeGjkSupportData*>(userData);
	const v3f32 supportA = data.positionA + 
		shapeSupport(data.shapeA, data.orientationA,  supportDirection);
	const v3f32 supportB = data.positionB + 
		shapeSupport(data.shapeB, data.orientationB, -supportDirection);
	return supportA - supportB;
}
v3f32 shapeSupport(
	const Shape& shape, const kQuaternion& orientation, 
	const v3f32& supportDirection)
{
	switch(shape.type)
	{
		case ShapeType::BOX:
			return kmath::supportBox(
				shape.box.lengths, orientation, supportDirection);
		case ShapeType::SPHERE:
			return kmath::supportSphere(
				shape.sphere.radius, supportDirection);
	}
	KLOG(ERROR, "shape.type {%i} is invalid!", i32(shape.type));
	return {};
}
void shapeDrawBox(
	const v3f32& worldPosition, const kQuaternion& orientation, 
	const Shape& shape, bool wireframe, const KrbApi* krb)
{
	krb->setModelXform(worldPosition, orientation, {1,1,1});
	Vertex generatedBox[36];
	kmath::generateMeshBox(
		shape.box.lengths, generatedBox, sizeof(generatedBox), 
		sizeof(generatedBox[0]), offsetof(Vertex,position), 
		offsetof(Vertex,textureNormal));
	const KrbVertexAttributeOffsets vertexAttribOffsets = wireframe
		? VERTEX_ATTRIBS_POSITION_ONLY
		: VERTEX_ATTRIBS_NO_COLOR;
	DRAW_TRIS(generatedBox, vertexAttribOffsets);
}
void shapeDrawSphere(
	const v3f32& worldPosition, const kQuaternion& orientation, 
	const Shape& shape, bool wireframe, const KrbApi* krb, 
	KAllocatorHandle hKal)
{
	/* generate sphere mesh */
	const size_t generatedSphereMeshVertexCount = 
		kmath::generateMeshCircleSphereVertexCount(16, 16);
	const size_t generatedSphereMeshBytes = 
		generatedSphereMeshVertexCount * sizeof(Vertex);
	Vertex*const generatedSphereMesh = reinterpret_cast<Vertex*>(
		kAllocAlloc(hKal, generatedSphereMeshBytes));
	kmath::generateMeshCircleSphere(
		1.f, 16, 16, generatedSphereMesh, generatedSphereMeshBytes, 
		sizeof(Vertex), offsetof(Vertex,position), 
		offsetof(Vertex,textureNormal));
	/* draw the sphere */
	const v3f32 sphereScale = 
		{ shape.sphere.radius, shape.sphere.radius, shape.sphere.radius };
	krb->setModelXform(worldPosition, orientation, sphereScale);
	const KrbVertexAttributeOffsets vertexAttribOffsets = wireframe
		? VERTEX_ATTRIBS_POSITION_ONLY
		: VERTEX_ATTRIBS_NO_COLOR;
	krb->drawTris(
		generatedSphereMesh, generatedSphereMeshVertexCount, 
		sizeof(generatedSphereMesh[0]), vertexAttribOffsets);
	kAllocFree(hKal, generatedSphereMesh);
}
void shapeDraw(
	const v3f32& worldPosition, const kQuaternion& orientation, 
	const Shape& shape, bool wireframe, const KrbApi* krb, 
	KAllocatorHandle hKal)
{
	switch(shape.type)
	{
		case ShapeType::BOX:
			shapeDrawBox(worldPosition, orientation, shape, wireframe, krb);
			break;
		case ShapeType::SPHERE:
			shapeDrawSphere(
				worldPosition, orientation, shape, wireframe, krb, hKal);
			break;
		default:
			KLOG(ERROR, "Invalid shape type! (%i)", shape.type);
			break;
	}
}
