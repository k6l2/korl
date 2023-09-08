/** 
 * This is a very non-standard C header (notice how it does not contain any 
 * preprocessor directives to guard against being included by multiple files 
 * within the same translation unit!) whose sole purpose is to help KORL 
 * platform API be easier to modify, without having to go modify many different 
 * files & lines just to add/remove a single API entry.  
 * By only having to maintain this API list, users can instead just define a 
 * specific macro operation which can be run across each API string, allowing 
 * them to perform actions such as declaring all APIs in a particular way, or 
 * setting all API function pointers to be a specific value.
 * 
 * # How to add a new KORL platform API
 * 
 *   1) Add an API entry to this file with an appropriately named API postfix.  
 *      Note that this postfix will be the string used to identify the API in all 
 *      code modules which use the platform layer!  By convention, I prefix each 
 *      API entry with `korl_` to hopefully prevent the platform layer's symbols 
 *      from colliding with other code symbols, and if the API shouldn't be called 
 *      directly by the user (but still requires to be present in the translation 
 *      unit), I use the `_korl_` prefix to indicate that it is more of a 
 *      "private" API.
 *      Example: 
 *          _KORL_PLATFORM_API_MACRO_OPERATION(korl_newApi)
 *   2) Define a function signature for the API in a macro somewhere, and make 
 *      sure it is included in `korl-interface-platform.h` so that all code that 
 *      uses the platform layer can see it.  Note that it is important to end the 
 *      macro with the API postfix composed in step (1)!
 *      Use the following example as a standard for nomenclature:
 *          #define KORL_FUNCTION_korl_newApi(name) void name(void)
 *   3) NOTE: no longer need to declare the function symbol, as `korl-interface-platform.h` handles this automatically
 *      Declare & define the function symbol itself in whatever KORL module it 
 *      belongs to.  Make sure to match the function's symbol name to the _same_ 
 *      string as the function signature's macro name postfix!
 *      Examples:
 *      - Declaration:
 *          korl_internal KORL_FUNCTION_korl_newApi(korl_newApi);
 *      - Definition:
 *          korl_internal KORL_FUNCTION_korl_newApi(korl_newApi)
 *          {
 *              // code goes here :)
 *          }
 * 
 * # How to remove a KORL platform API
 * 
 *   1) Delete the API entry from this file.
 *   2) Remove the reference to the API's function signature macro from `korl-interface-platform.h`
 *   3) Delete the actual function declaration & definition.
 *      NOTE: no longer need to remove declaration, as `korl-interface-platform.h` handles this automatically
 */
#ifndef _KORL_PLATFORM_API_MACRO_OPERATION
#   define _KORL_PLATFORM_API_MACRO_OPERATION(x)
#endif
_KORL_PLATFORM_API_MACRO_OPERATION(korl_algorithm_sort_quick)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_algorithm_sort_quick_context)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_assetCache_get)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_bluetooth_query)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_bluetooth_connect)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_bluetooth_disconnect)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_bluetooth_read)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_clipboard_set)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_clipboard_get)
_KORL_PLATFORM_API_MACRO_OPERATION(_korl_command_register)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_command_invoke)
_KORL_PLATFORM_API_MACRO_OPERATION(_korl_crash_assertConditionFailed)
_KORL_PLATFORM_API_MACRO_OPERATION(_korl_functionDynamo_register)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_functionDynamo_get)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_useCamera)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_camera_getCurrent)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_cameraOrthoGetSize)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_camera_windowToWorld)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_camera_worldToWindow)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_getSurfaceSize)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_setClearColor)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_draw)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_setDrawState)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_stagingAllocate)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_stagingReallocate)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_drawStagingAllocation)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_drawVertexBuffer)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_getBuiltInShaderVertex)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_getBuiltInShaderFragment)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_light_use)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_getBlankTexture)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gui_setFontAsset)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gui_windowBegin)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gui_windowEnd)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gui_setNextWidgetSize)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gui_setNextWidgetAnchor)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gui_setNextWidgetParentAnchor)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gui_setNextWidgetParentOffset)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gui_setLoopIndex)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gui_realignY)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gui_widgetTextFormat)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gui_widgetText)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gui_widgetButtonFormat)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gui_widgetScrollAreaBegin)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gui_widgetScrollAreaEnd)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gui_widgetInputText)
_KORL_PLATFORM_API_MACRO_OPERATION(_korl_log_variadic)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_log_getBuffer)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_math_round_f32_to_u8)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_math_round_f32_to_u32)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_math_round_f32_to_i32)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_math_round_f64_to_i64)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_math_f32_modulus)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_math_sine)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_math_arcSine)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_math_cosine)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_math_arcCosine)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_math_tangent)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_math_arcTangent)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_math_f32_floor)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_math_f64_floor)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_math_f32_ceiling)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_math_f32_squareRoot)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_math_f32_exponential)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_math_f64_exponential)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_math_f32_power)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_math_f64_power)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_math_loadExponent)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_math_f32_isNan)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_math_exDecay)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_memory_zero)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_memory_copy)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_memory_move)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_memory_compare)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_memory_allocator_create)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_memory_allocator_allocate)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_memory_allocator_reallocate)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_memory_allocator_free)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_memory_allocator_empty)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_memory_allocator_isFragmented)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_memory_allocator_defragment)
// _KORL_PLATFORM_API_MACRO_OPERATION(korl_memory_allocator_enumerateAllocators)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_memory_allocator_enumerateAllocations)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_memory_allocator_enumerateHeaps)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_network_openUdp)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_network_close)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_network_send)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_network_receive)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_network_resolveAddress)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_descriptor_register)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_fromFile)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_create)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_getDescriptorStruct)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_getDescriptorContextStruct)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_resize)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_shift)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_destroy)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_update)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_getUpdateBuffer)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_getByteSize)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_isLoaded)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_forEach)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_font_getMetrics)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_font_getResources)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_font_getUtf8Metrics)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_font_generateUtf8)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_font_getUtf16Metrics)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_font_generateUtf16)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_font_textGraphemePosition)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_scene3d_setDefaultResource)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_scene3d_getMaterialCount)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_scene3d_getMaterial)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_scene3d_getMeshIndex)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_scene3d_getMeshName)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_scene3d_getMeshPrimitiveCount)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_scene3d_getMeshPrimitive)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_scene3d_newSkin)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_scene3d_skin_getBoneParentIndices)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_scene3d_skin_getBoneTopologicalOrder)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_scene3d_skin_getBoneInverseBindMatrices)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_scene3d_skin_applyAnimation)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_scene3d_getAnimationIndex)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_scene3d_animation_seconds)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_texture_getSize)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_texture_getRowByteStride)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_sfx_playResource)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_sfx_setVolume)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_sfx_category_set)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_sfx_setListener)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_sfx_tape_stop)
_KORL_PLATFORM_API_MACRO_OPERATION(_korl_stb_ds_reallocate)
_KORL_PLATFORM_API_MACRO_OPERATION(_korl_stb_ds_free)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_string_utf8_to_f32)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_string_formatVaListUtf8)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_string_formatVaListUtf16)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_string_formatBufferVaListUtf16)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_string_unicode_toUpper)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_time_secondsSinceTimeStamp)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_timeStamp)
