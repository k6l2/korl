/*
 * User code must define a global KrbApi* called `g_krb` to use this module!
 */
#pragma once
struct KgtCamera3d
{
	v3f32 position;
	v3f32 velocity;
	f32 radiansYaw;
	f32 radiansPitch;
	bool orthographicView;
};
internal v3f32 
	kgtCamera3dWorldForward(const KgtCamera3d* cam);
internal v3f32 
	kgtCamera3dWorldRight(const KgtCamera3d* cam);
internal v3f32 
	kgtCamera3dWorldUp(const KgtCamera3d* cam);
internal void 
	kgtCamera3dStep(
		KgtCamera3d* cam, bool moveForward, bool moveBack, bool moveRight, 
		bool moveLeft, bool moveUp, bool moveDown, f32 deltaSeconds);
internal void 
	kgtCamera3dLook(KgtCamera3d* cam, const v2i32& deltaYawPitch);
internal void 
	kgtCamera3dApplyViewProjection(
		const KgtCamera3d* cam, const v2u32& windowDimensions);
