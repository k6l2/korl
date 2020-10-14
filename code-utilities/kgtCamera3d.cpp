#include "KgtCamera3d.h"
internal v3f32 kgtCam3dWorldForward(const KgtCamera3d* cam)
{
	return (kQuaternion(v3f32::Z*-1, cam->radiansYaw) * 
	        kQuaternion(v3f32::Y*-1, cam->radiansPitch))
	            .transform(v3f32::X);
}
internal v3f32 kgtCam3dWorldRight(const KgtCamera3d* cam)
{
	return (kQuaternion(v3f32::Z*-1, cam->radiansYaw) * 
	        kQuaternion(v3f32::Y*-1, cam->radiansPitch))
	            .transform(v3f32::Y*-1);
}
internal v3f32 kgtCam3dWorldUp(const KgtCamera3d* cam)
{
	return (kQuaternion(v3f32::Z*-1, cam->radiansYaw) * 
	        kQuaternion(v3f32::Y*-1, cam->radiansPitch))
	            .transform(v3f32::Z);
}
internal void kgtCam3dStep(
	KgtCamera3d* cam, bool moveForward, bool moveBack, bool moveRight, 
	bool moveLeft, bool moveUp, bool moveDown, f32 deltaSeconds)
{
	local_persist const f32 CAMERA_SPEED_MAX = 25;
	local_persist const f32 CAMERA_ACCELERATION = 10;
	local_persist const f32 CAMERA_DECELERATION = 50;
	const v3f32 cameraWorldForward = kgtCam3dWorldForward(cam);
	const v3f32 cameraWorldRight   = kgtCam3dWorldRight(cam);
	const v3f32 cameraWorldUp      = kgtCam3dWorldUp(cam);
	v3f32 camControl = v3f32::ZERO;
	/* move the camera world position */
	if(moveForward)
		camControl += cameraWorldForward;
	if(moveBack)
		camControl -= cameraWorldForward;
	if(moveRight)
		camControl += cameraWorldRight;
	if(moveLeft)
		camControl -= cameraWorldRight;
	if(moveUp)
		camControl += cameraWorldUp;
	if(moveDown)
		camControl -= cameraWorldUp;
	f32 camControlMag = camControl.normalize();
	if(kmath::isNearlyZero(camControlMag))
	{
		const v3f32 camDecelDir = kmath::normal(-cam->velocity);
		cam->velocity += 
			deltaSeconds*CAMERA_DECELERATION*camDecelDir;
		if(cam->velocity.dot(camDecelDir) > 0)
			cam->velocity = v3f32::ZERO;
	}
	else
	{
		/* split camera velocity into the `control direction` direction & 
		 * the component perpendicular to this direction */
		v3f32 camControlComp = 
			cam->velocity.projectOnto(camControl, true);
		v3f32 camControlPerpComp = 
			cam->velocity - camControlComp;
		/* decelerate along the perpendicular component */
		const v3f32 camControlPerpDecelDir = 
			kmath::normal(-camControlPerpComp);
		camControlPerpComp += 
			deltaSeconds*CAMERA_DECELERATION*camControlPerpDecelDir;
		if(camControlPerpComp.dot(camControlPerpDecelDir) > 0)
			camControlPerpComp = v3f32::ZERO;
		/* halt if we're moving away from camControl */
		if(camControlComp.dot(camControl) < 0)
			camControlComp = v3f32::ZERO;
		/* rejoin the components and accelerate towards camControl */
		cam->velocity = camControlComp + camControlPerpComp + 
			deltaSeconds*CAMERA_ACCELERATION*camControl;
	}
	f32 camSpeed = cam->velocity.normalize();
	if(camSpeed > CAMERA_SPEED_MAX)
		camSpeed = CAMERA_SPEED_MAX;
	cam->velocity *= camSpeed;
	cam->position += deltaSeconds * cam->velocity;
}
internal void kgtCam3dLook(KgtCamera3d* cam, const v2i32& deltaYawPitch)
{
	local_persist const f32 CAMERA_LOOK_SENSITIVITY = 0.0025f;
	local_persist const f32 MAX_PITCH_MAGNITUDE = PI32/2 - 0.001f;
	/* move the camera yaw & pitch based on relative mouse inputs */
	cam->radiansYaw += CAMERA_LOOK_SENSITIVITY * deltaYawPitch.x;
	cam->radiansYaw  = fmodf(cam->radiansYaw, 2*PI32);
	cam->radiansPitch -= CAMERA_LOOK_SENSITIVITY*deltaYawPitch.y;
	if(cam->radiansPitch < -MAX_PITCH_MAGNITUDE)
		cam->radiansPitch = -MAX_PITCH_MAGNITUDE;
	if(cam->radiansPitch > MAX_PITCH_MAGNITUDE)
		cam->radiansPitch = MAX_PITCH_MAGNITUDE;
}
internal void kgtCam3dApplyViewProjection(
	const KgtCamera3d* cam, const KrbApi* krb, const v2u32& windowDimensions)
{
	const v3f32 cameraWorldForward = kgtCam3dWorldForward(cam);
	if(cam->orthographicView)
		krb->setProjectionOrthoFixedHeight(
			windowDimensions.x, windowDimensions.y, 100, 1000);
	else
		krb->setProjectionFov(50.f, windowDimensions.elements, 0.001f, 1000);
	krb->lookAt(cam->position.elements, 
	            (cam->position + cameraWorldForward).elements, 
	            v3f32::Z.elements);
}
