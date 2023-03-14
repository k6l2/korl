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
 *   3) Declare & define the function symbol itself in whatever KORL module it 
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
 * 
 * # Code samples
 * 
 *   ## Declaration of all KORL platform APIs
 *   
 *     ```
 *     #define _KORL_PLATFORM_API_MACRO_OPERATION(x) fnSig_##x *x;
 *         #include "korl-interface-platform-api.h"
 * KORL-ISSUE-000-000-120: interface-platform: remove KORL_DEFINED_INTERFACE_PLATFORM_API; this it just messy imo; if there is clear separation of code that should only exist in the platform layer, then we should probably just physically separate it out into separate source file(s)
 *         #define KORL_DEFINED_INTERFACE_PLATFORM_API// prevent certain KORL modules which define symbols that are required by the game module, but whose codes are confined to the platform layer, from re-defining them since we just declared the API
 *     #undef _KORL_PLATFORM_API_MACRO_OPERATION
 *     ```
 *   
 *   ## Function to get the KORL platform API from the platform layer
 *   
 *     This sample assumes you declared the KORL platform API exactly like the above 
 *     sample.
 *     
 *     ```
 *     korl_internal void _getInterfacePlatformApi(KorlPlatformApi korlApi)
 *     {
 *         #define _KORL_PLATFORM_API_MACRO_OPERATION(x) (x) = korlApi.x;
 *         #include "korl-interface-platform-api.h"
 *         #undef _KORL_PLATFORM_API_MACRO_OPERATION
 *     }
 *     ```
 */
#ifndef _KORL_PLATFORM_API_MACRO_OPERATION
#   define _KORL_PLATFORM_API_MACRO_OPERATION(x)
#endif
_KORL_PLATFORM_API_MACRO_OPERATION(_korl_crash_assertConditionFailed)
_KORL_PLATFORM_API_MACRO_OPERATION(_korl_log_variadic)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_log_getBuffer)
_KORL_PLATFORM_API_MACRO_OPERATION(_korl_command_register)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_command_invoke)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_clipboard_set)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_clipboard_get)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_timeStamp)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_time_secondsSinceTimeStamp)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_memory_zero)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_memory_copy)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_memory_move)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_memory_compare)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_memory_arrayU8Compare)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_memory_arrayU16Compare)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_memory_acu16_hash)
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
_KORL_PLATFORM_API_MACRO_OPERATION(_korl_stb_ds_reallocate)
_KORL_PLATFORM_API_MACRO_OPERATION(_korl_stb_ds_free)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_assetCache_get)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_fromFile)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_resource_texture_getSize)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_font_getMetrics)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_createCameraFov)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_createCameraOrtho)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_createCameraOrthoFixedHeight)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_cameraFov_rotateAroundTarget)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_useCamera)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_camera_getCurrent)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_cameraSetScissor)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_cameraSetScissorPercent)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_cameraOrthoSetOriginAnchor)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_cameraOrthoGetSize)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_camera_windowToWorld)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_camera_worldToWindow)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_batch)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_createBatchRectangleTextured)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_createBatchRectangleColored)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_createBatchCircle)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_createBatchCircleSector)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_createBatchTriangles)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_createBatchLines)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_createBatchText)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_batchSetBlendState)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_batchSetPosition)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_batchSetPosition2d)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_batchSetPosition2dV2f32)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_batchSetScale)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_batchSetQuaternion)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_batchSetRotation)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_batchSetVertexColor)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_batchAddLine)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_batchSetLine)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_batchTextGetAabb)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_batchTextSetPositionAnchor)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_batchRectangleSetSize)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_batchRectangleSetColor)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_batch_rectangle_setUv)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_gfx_batchCircleSetColor)
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
_KORL_PLATFORM_API_MACRO_OPERATION(korl_bluetooth_query)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_bluetooth_connect)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_bluetooth_disconnect)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_bluetooth_read)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_string_formatVaListUtf8)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_string_formatVaListUtf16)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_string_formatBufferVaListUtf16)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_sfx_playResource)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_sfx_setVolume)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_sfx_category_set)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_sfx_setListener)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_sfx_tape_stop)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_algorithm_sort_quick)
_KORL_PLATFORM_API_MACRO_OPERATION(korl_algorithm_sort_quick_context)
