#pragma once
struct Camera3d
{
	v3f32 position;
	v3f32 velocity;
	f32 radiansYaw;
	f32 radiansPitch;
	bool orthographicView;
};
internal v3f32 cam3dWorldForward(const Camera3d* cam);
internal v3f32 cam3dWorldRight(const Camera3d* cam);
internal v3f32 cam3dWorldUp(const Camera3d* cam);
internal void cam3dStep(
	Camera3d* cam, bool moveForward, bool moveBack, bool moveRight, 
	bool moveLeft, bool moveUp, bool moveDown, f32 deltaSeconds);
internal void cam3dLook(Camera3d* cam, const v2i32& deltaYawPitch);
internal void cam3dApplyViewProjection(
	const Camera3d* cam, const KrbApi* krb, const v2u32& windowDimensions);
