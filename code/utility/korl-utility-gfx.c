#include "utility/korl-utility-gfx.h"
korl_internal void korl_gfx_drawable_scene3d_initialize(Korl_Gfx_Drawable*const context, Korl_Resource_Handle resourceHandleScene3d)
{
    korl_memory_zero(context, sizeof(*context));
    context->type            = KORL_GFX_DRAWABLE_TYPE_SCENE3D;
    context->_model.rotation = KORL_MATH_QUATERNION_IDENTITY;
    context->_model.scale    = KORL_MATH_V3F32_ONE;
    context->subType.scene3d.resourceHandle = resourceHandleScene3d;
    #if 0
    context->subType.scene3d.resourceHandleShaderVertex   = ;
    context->subType.scene3d.resourceHandleShaderFragment = ;
    #endif
}
