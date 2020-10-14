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
	kgtCam3dWorldForward(const KgtCamera3d* cam);
internal v3f32 
	kgtCam3dWorldRight(const KgtCamera3d* cam);
internal v3f32 
	kgtCam3dWorldUp(const KgtCamera3d* cam);
internal void 
	kgtCam3dStep(
		KgtCamera3d* cam, bool moveForward, bool moveBack, bool moveRight, 
		bool moveLeft, bool moveUp, bool moveDown, f32 deltaSeconds);
internal void 
	kgtCam3dLook(KgtCamera3d* cam, const v2i32& deltaYawPitch);
internal void 
	kgtCam3dApplyViewProjection(
		const KgtCamera3d* cam, const KrbApi* krb, 
		const v2u32& windowDimensions);
