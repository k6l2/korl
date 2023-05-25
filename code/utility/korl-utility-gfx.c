#include "utility/korl-utility-gfx.h"
#include "utility/korl-checkCast.h"
#include "korl-interface-platform.h"
korl_internal void korl_gfx_drawable_mesh_initialize(Korl_Gfx_Drawable*const context, Korl_Resource_Handle resourceHandleScene3d, acu8 utf8MeshName)
{
    korl_memory_zero(context, sizeof(*context));
    context->type            = KORL_GFX_DRAWABLE_TYPE_MESH;
    context->_model.rotation = KORL_MATH_QUATERNION_IDENTITY;
    context->_model.scale    = KORL_MATH_V3F32_ONE;
    context->subType.mesh.resourceHandleScene3d = resourceHandleScene3d;
    korl_assert(utf8MeshName.size < korl_arraySize(context->subType.mesh.rawUtf8Scene3dMeshName));
    korl_memory_copy(context->subType.mesh.rawUtf8Scene3dMeshName, utf8MeshName.data, utf8MeshName.size);
    context->subType.mesh.rawUtf8Scene3dMeshName[utf8MeshName.size] = '\0';
    context->subType.mesh.rawUtf8Scene3dMeshNameSize = korl_checkCast_u$_to_u8(utf8MeshName.size);
}
