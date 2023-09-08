#include "utility/korl-utility-resource.h"
#include "korl-interface-platform.h"
korl_internal void korl_resource_scene3d_skin_destroy(Korl_Resource_Scene3d_Skin* context)
{
    korl_free(context->allocator, context);
}
