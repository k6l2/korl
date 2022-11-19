#pragma once
#include "korl-globalDefines.h"
#include <stdarg.h>//for va_list
#include "korl-interface-platform-assetCache.h"
#include "korl-interface-platform-resource.h"
#include "korl-interface-platform-time.h"
#include "korl-interface-platform-input.h"
#include "korl-interface-platform-gui.h"
#include "korl-interface-platform-gfx.h"
#include "korl-interface-platform-memory.h"
#include "korl-interface-platform-bluetooth.h"
/* korl-log interface *********************************************************/
typedef enum KorlEnumLogLevel
    { KORL_LOG_LEVEL_ASSERT
    , KORL_LOG_LEVEL_ERROR
    , KORL_LOG_LEVEL_WARNING
    , KORL_LOG_LEVEL_INFO
    , KORL_LOG_LEVEL_VERBOSE
} KorlEnumLogLevel;
/** Do not call this function directly; use the \c korl_log macro instead! */
#define KORL_PLATFORM_LOG(name) void name(unsigned variadicArgumentCount, enum KorlEnumLogLevel logLevel\
                                         ,const wchar_t* cStringFileName, const wchar_t* cStringFunctionName\
                                         ,int lineNumber, const wchar_t* format, ...)
#define KORL_PLATFORM_LOG_GET_BUFFER(name) acu16 name(u$* out_loggedBytes)
/* korl-crash interface *******************************************************/
/** Do not call this function directly; use the \c korl_assert macro instead! */
#define KORL_PLATFORM_ASSERT_FAILURE(name) void name(const wchar_t* conditionString, const wchar_t* cStringFileName\
                                                    ,const wchar_t* cStringFunctionName, int lineNumber)
/* korl-stb-ds interface ******************************************************/
#define KORL_PLATFORM_STB_DS_REALLOCATE(name) void* name(void* context, void* allocation, u$ bytes, const wchar_t*const file, int line)
#define KORL_PLATFORM_STB_DS_FREE(name)       void  name(void* context, void* allocation)
/* FUNCTION TYPEDEFS **********************************************************/
typedef KORL_PLATFORM_ASSERT_FAILURE                      (fnSig_korlPlatformAssertFailure);
typedef KORL_PLATFORM_LOG                                 (fnSig_korlPlatformLog);
typedef KORL_PLATFORM_LOG_GET_BUFFER                      (fnSig_korl_log_getBuffer);
typedef KORL_PLATFORM_GET_TIMESTAMP                       (fnSig_korl_timeStamp);
typedef KORL_PLATFORM_SECONDS_SINCE_TIMESTAMP             (fnSig_korl_time_secondsSinceTimeStamp);
typedef KORL_PLATFORM_MEMORY_ZERO                         (fnSig_korl_memory_zero);
typedef KORL_PLATFORM_MEMORY_COPY                         (fnSig_korl_memory_copy);
typedef KORL_PLATFORM_MEMORY_MOVE                         (fnSig_korl_memory_move);
typedef KORL_PLATFORM_MEMORY_COMPARE                      (fnSig_korl_memory_compare);
typedef KORL_PLATFORM_ARRAY_U8_COMPARE                    (fnSig_korl_memory_arrayU8Compare);
typedef KORL_PLATFORM_ARRAY_U16_COMPARE                   (fnSig_korl_memory_arrayU16Compare);
typedef KORL_PLATFORM_ARRAY_CONST_U16_HASH                (fnSig_korl_memory_acu16_hash);
typedef KORL_PLATFORM_STRING_COMPARE                      (fnSig_korl_memory_stringCompare);
typedef KORL_PLATFORM_STRING_COMPARE_UTF8                 (fnSig_korl_memory_stringCompareUtf8);
typedef KORL_PLATFORM_STRING_SIZE                         (fnSig_korl_memory_stringSize);
typedef KORL_PLATFORM_STRING_SIZE_UTF8                    (fnSig_korl_memory_stringSizeUtf8);
typedef KORL_PLATFORM_STRING_COPY                         (fnSig_korl_memory_stringCopy);
typedef KORL_PLATFORM_STRING_FORMAT_VALIST                (fnSig_korl_memory_stringFormatVaList);
typedef KORL_PLATFORM_STRING_FORMAT_VALIST_UTF8           (fnSig_korl_memory_stringFormatVaListUtf8);
typedef KORL_PLATFORM_STRING_FORMAT_BUFFER                (fnSig_korl_memory_stringFormatBuffer);
typedef KORL_PLATFORM_MEMORY_CREATE_ALLOCATOR             (fnSig_korl_memory_allocator_create);
typedef KORL_PLATFORM_MEMORY_ALLOCATOR_ALLOCATE           (fnSig_korl_memory_allocator_allocate);
typedef KORL_PLATFORM_MEMORY_ALLOCATOR_REALLOCATE         (fnSig_korl_memory_allocator_reallocate);
typedef KORL_PLATFORM_MEMORY_ALLOCATOR_FREE               (fnSig_korl_memory_allocator_free);
typedef KORL_PLATFORM_MEMORY_ALLOCATOR_EMPTY              (fnSig_korl_memory_allocator_empty);
typedef KORL_PLATFORM_STB_DS_REALLOCATE                   (fnSig_korl_stb_ds_reallocate);
typedef KORL_PLATFORM_STB_DS_FREE                         (fnSig_korl_stb_ds_free);
typedef KORL_PLATFORM_ASSETCACHE_GET                      (fnSig_korl_assetCache_get);
typedef KORL_PLATFORM_RESOURCE_FROM_FILE                  (fnSig_korl_resource_fromFile);
typedef KORL_PLATFORM_GFX_CREATE_CAMERA_FOV               (fnSig_korl_gfx_createCameraFov);
typedef KORL_PLATFORM_GFX_CREATE_CAMERA_ORTHO             (fnSig_korl_gfx_createCameraOrtho);
typedef KORL_PLATFORM_GFX_CREATE_CAMERA_ORTHO_FIXED_HEIGHT(fnSig_korl_gfx_createCameraOrthoFixedHeight);
typedef KORL_PLATFORM_GFX_CAMERA_FOV_ROTATE_AROUND_TARGET (fnSig_korl_gfx_cameraFov_rotateAroundTarget);
typedef KORL_PLATFORM_GFX_USE_CAMERA                      (fnSig_korl_gfx_useCamera);
typedef KORL_PLATFORM_GFX_CAMERA_SET_SCISSOR              (fnSig_korl_gfx_cameraSetScissor);
typedef KORL_PLATFORM_GFX_CAMERA_SET_SCISSOR_PERCENT      (fnSig_korl_gfx_cameraSetScissorPercent);
typedef KORL_PLATFORM_GFX_CAMERA_ORTHO_SET_ORIGIN_ANCHOR  (fnSig_korl_gfx_cameraOrthoSetOriginAnchor);
typedef KORL_PLATFORM_GFX_BATCH                           (fnSig_korl_gfx_batch);
typedef KORL_PLATFORM_GFX_CREATE_BATCH_RECTANGLE_TEXTURED (fnSig_korl_gfx_createBatchRectangleTextured);
typedef KORL_PLATFORM_GFX_CREATE_BATCH_RECTANGLE_COLORED  (fnSig_korl_gfx_createBatchRectangleColored);
typedef KORL_PLATFORM_GFX_CREATE_BATCH_CIRCLE             (fnSig_korl_gfx_createBatchCircle);
typedef KORL_PLATFORM_GFX_CREATE_BATCH_TRIANGLES          (fnSig_korl_gfx_createBatchTriangles);
typedef KORL_PLATFORM_GFX_CREATE_BATCH_LINES              (fnSig_korl_gfx_createBatchLines);
typedef KORL_PLATFORM_GFX_CREATE_BATCH_TEXT               (fnSig_korl_gfx_createBatchText);
typedef KORL_PLATFORM_GFX_BATCH_SET_BLEND_STATE           (fnSig_korl_gfx_batchSetBlendState);
typedef KORL_PLATFORM_GFX_BATCH_SET_POSITION              (fnSig_korl_gfx_batchSetPosition);
typedef KORL_PLATFORM_GFX_BATCH_SET_POSITION_2D           (fnSig_korl_gfx_batchSetPosition2d);
typedef KORL_PLATFORM_GFX_BATCH_SET_POSITION_2D_V2F32     (fnSig_korl_gfx_batchSetPosition2dV2f32);
typedef KORL_PLATFORM_GFX_BATCH_SET_SCALE                 (fnSig_korl_gfx_batchSetScale);
typedef KORL_PLATFORM_GFX_BATCH_SET_QUATERNION            (fnSig_korl_gfx_batchSetQuaternion);
typedef KORL_PLATFORM_GFX_BATCH_SET_ROTATION              (fnSig_korl_gfx_batchSetRotation);
typedef KORL_PLATFORM_GFX_BATCH_SET_VERTEX_COLOR          (fnSig_korl_gfx_batchSetVertexColor);
typedef KORL_PLATFORM_GFX_BATCH_ADD_LINE                  (fnSig_korl_gfx_batchAddLine);
typedef KORL_PLATFORM_GFX_BATCH_SET_LINE                  (fnSig_korl_gfx_batchSetLine);
typedef KORL_PLATFORM_GFX_BATCH_TEXT_GET_AABB             (fnSig_korl_gfx_batchTextGetAabb);
typedef KORL_PLATFORM_GFX_BATCH_TEXT_SET_POSITION_ANCHOR  (fnSig_korl_gfx_batchTextSetPositionAnchor);
typedef KORL_PLATFORM_GFX_BATCH_RECTANGLE_SET_SIZE        (fnSig_korl_gfx_batchRectangleSetSize);
typedef KORL_PLATFORM_GFX_BATCH_RECTANGLE_SET_COLOR       (fnSig_korl_gfx_batchRectangleSetColor);
typedef KORL_PLATFORM_GFX_BATCH_CIRCLE_SET_COLOR          (fnSig_korl_gfx_batchCircleSetColor);
typedef KORL_PLATFORM_GUI_SET_FONT_ASSET                  (fnSig_korl_gui_setFontAsset);
typedef KORL_PLATFORM_GUI_WINDOW_BEGIN                    (fnSig_korl_gui_windowBegin);
typedef KORL_PLATFORM_GUI_WINDOW_END                      (fnSig_korl_gui_windowEnd);
typedef KORL_PLATFORM_GUI_WINDOW_SET_POSITION             (fnSig_korl_gui_windowSetPosition);
typedef KORL_PLATFORM_GUI_WINDOW_SET_SIZE                 (fnSig_korl_gui_windowSetSize);
typedef KORL_PLATFORM_GUI_WINDOW_SET_LOOP_INDEX           (fnSig_korl_gui_setLoopIndex);
typedef KORL_PLATFORM_GUI_WIDGET_TEXT_FORMAT              (fnSig_korl_gui_widgetTextFormat);
typedef KORL_PLATFORM_GUI_WIDGET_TEXT                     (fnSig_korl_gui_widgetText);
typedef KORL_PLATFORM_GUI_WIDGET_BUTTON_FORMAT            (fnSig_korl_gui_widgetButtonFormat);
typedef KORL_PLATFORM_BLUETOOTH_QUERY                     (fnSig_korl_bluetooth_query);
typedef KORL_PLATFORM_BLUETOOTH_CONNECT                   (fnSig_korl_bluetooth_connect);
typedef KORL_PLATFORM_BLUETOOTH_DISCONNECT                (fnSig_korl_bluetooth_disconnect);
typedef KORL_PLATFORM_BLUETOOTH_READ                      (fnSig_korl_bluetooth_read);
/* KORL INTERFACE API DECLARATION CONVENIENCE MACROS **************************/
#define KORL_INTERFACE_PLATFORM_API_DECLARE \
    fnSig_korlPlatformAssertFailure             * _korl_crash_assertConditionFailed;\
    fnSig_korlPlatformLog                       * _korl_log_variadic;\
    fnSig_korl_log_getBuffer                    * korl_log_getBuffer;\
    fnSig_korl_timeStamp                        * korl_timeStamp;\
    fnSig_korl_time_secondsSinceTimeStamp       * korl_time_secondsSinceTimeStamp;\
    fnSig_korl_memory_zero                      * korl_memory_zero;\
    fnSig_korl_memory_copy                      * korl_memory_copy;\
    fnSig_korl_memory_move                      * korl_memory_move;\
    fnSig_korl_memory_compare                   * korl_memory_compare;\
    fnSig_korl_memory_arrayU8Compare            * korl_memory_arrayU8Compare;\
    fnSig_korl_memory_arrayU16Compare           * korl_memory_arrayU16Compare;\
    fnSig_korl_memory_acu16_hash                * korl_memory_acu16_hash;\
    fnSig_korl_memory_stringCompare             * korl_memory_stringCompare;\
    fnSig_korl_memory_stringCompareUtf8         * korl_memory_stringCompareUtf8;\
    fnSig_korl_memory_stringSize                * korl_memory_stringSize;\
    fnSig_korl_memory_stringSizeUtf8            * korl_memory_stringSizeUtf8;\
    fnSig_korl_memory_stringCopy                * korl_memory_stringCopy;\
    fnSig_korl_memory_stringFormatVaList        * korl_memory_stringFormatVaList;\
    fnSig_korl_memory_stringFormatVaListUtf8    * korl_memory_stringFormatVaListUtf8;\
    fnSig_korl_memory_stringFormatBuffer        * korl_memory_stringFormatBuffer;\
    fnSig_korl_memory_allocator_create          * korl_memory_allocator_create;\
    fnSig_korl_memory_allocator_allocate        * korl_memory_allocator_allocate;\
    fnSig_korl_memory_allocator_reallocate      * korl_memory_allocator_reallocate;\
    fnSig_korl_memory_allocator_free            * korl_memory_allocator_free;\
    fnSig_korl_memory_allocator_empty           * korl_memory_allocator_empty;\
    fnSig_korl_stb_ds_reallocate                * _korl_stb_ds_reallocate;\
    fnSig_korl_stb_ds_free                      * _korl_stb_ds_free;\
    fnSig_korl_assetCache_get                   * korl_assetCache_get;\
    fnSig_korl_resource_fromFile                * korl_resource_fromFile;\
    fnSig_korl_gfx_createCameraFov              * korl_gfx_createCameraFov;\
    fnSig_korl_gfx_createCameraOrtho            * korl_gfx_createCameraOrtho;\
    fnSig_korl_gfx_createCameraOrthoFixedHeight * korl_gfx_createCameraOrthoFixedHeight;\
    fnSig_korl_gfx_cameraFov_rotateAroundTarget * korl_gfx_cameraFov_rotateAroundTarget;\
    fnSig_korl_gfx_useCamera                    * korl_gfx_useCamera;\
    fnSig_korl_gfx_cameraSetScissor             * korl_gfx_cameraSetScissor;\
    fnSig_korl_gfx_cameraSetScissorPercent      * korl_gfx_cameraSetScissorPercent;\
    fnSig_korl_gfx_cameraOrthoSetOriginAnchor   * korl_gfx_cameraOrthoSetOriginAnchor;\
    fnSig_korl_gfx_batch                        * korl_gfx_batch;\
    fnSig_korl_gfx_createBatchRectangleTextured * korl_gfx_createBatchRectangleTextured;\
    fnSig_korl_gfx_createBatchRectangleColored  * korl_gfx_createBatchRectangleColored;\
    fnSig_korl_gfx_createBatchCircle            * korl_gfx_createBatchCircle;\
    fnSig_korl_gfx_createBatchTriangles         * korl_gfx_createBatchTriangles;\
    fnSig_korl_gfx_createBatchLines             * korl_gfx_createBatchLines;\
    fnSig_korl_gfx_createBatchText              * korl_gfx_createBatchText;\
    fnSig_korl_gfx_batchSetBlendState           * korl_gfx_batchSetBlendState;\
    fnSig_korl_gfx_batchSetPosition             * korl_gfx_batchSetPosition;\
    fnSig_korl_gfx_batchSetPosition2d           * korl_gfx_batchSetPosition2d;\
    fnSig_korl_gfx_batchSetPosition2dV2f32      * korl_gfx_batchSetPosition2dV2f32;\
    fnSig_korl_gfx_batchSetScale                * korl_gfx_batchSetScale;\
    fnSig_korl_gfx_batchSetQuaternion           * korl_gfx_batchSetQuaternion;\
    fnSig_korl_gfx_batchSetRotation             * korl_gfx_batchSetRotation;\
    fnSig_korl_gfx_batchSetVertexColor          * korl_gfx_batchSetVertexColor;\
    fnSig_korl_gfx_batchAddLine                 * korl_gfx_batchAddLine;\
    fnSig_korl_gfx_batchSetLine                 * korl_gfx_batchSetLine;\
    fnSig_korl_gfx_batchTextGetAabb             * korl_gfx_batchTextGetAabb;\
    fnSig_korl_gfx_batchTextSetPositionAnchor   * korl_gfx_batchTextSetPositionAnchor;\
    fnSig_korl_gfx_batchRectangleSetSize        * korl_gfx_batchRectangleSetSize;\
    fnSig_korl_gfx_batchRectangleSetColor       * korl_gfx_batchRectangleSetColor;\
    fnSig_korl_gfx_batchCircleSetColor          * korl_gfx_batchCircleSetColor;\
    fnSig_korl_gui_setFontAsset                 * korl_gui_setFontAsset;\
    fnSig_korl_gui_windowBegin                  * korl_gui_windowBegin;\
    fnSig_korl_gui_windowEnd                    * korl_gui_windowEnd;\
    fnSig_korl_gui_windowSetPosition            * korl_gui_windowSetPosition;\
    fnSig_korl_gui_windowSetSize                * korl_gui_windowSetSize;\
    fnSig_korl_gui_setLoopIndex                 * korl_gui_setLoopIndex;\
    fnSig_korl_gui_widgetTextFormat             * korl_gui_widgetTextFormat;\
    fnSig_korl_gui_widgetText                   * korl_gui_widgetText;\
    fnSig_korl_gui_widgetButtonFormat           * korl_gui_widgetButtonFormat;\
    fnSig_korl_bluetooth_query                  * korl_bluetooth_query;\
    fnSig_korl_bluetooth_connect                * korl_bluetooth_connect;\
    fnSig_korl_bluetooth_disconnect             * korl_bluetooth_disconnect;\
    fnSig_korl_bluetooth_read                   * korl_bluetooth_read;
#define KORL_INTERFACE_PLATFORM_API_SET(apiVariableName) \
    (apiVariableName)._korl_crash_assertConditionFailed     = _korl_crash_assertConditionFailed;\
    (apiVariableName)._korl_log_variadic                    = _korl_log_variadic;\
    (apiVariableName).korl_log_getBuffer                    = korl_log_getBuffer;\
    (apiVariableName).korl_timeStamp                        = korl_timeStamp;\
    (apiVariableName).korl_time_secondsSinceTimeStamp       = korl_time_secondsSinceTimeStamp;\
    (apiVariableName).korl_memory_zero                      = korl_memory_zero;\
    (apiVariableName).korl_memory_copy                      = korl_memory_copy;\
    (apiVariableName).korl_memory_move                      = korl_memory_move;\
    (apiVariableName).korl_memory_compare                   = korl_memory_compare;\
    (apiVariableName).korl_memory_arrayU8Compare            = korl_memory_arrayU8Compare;\
    (apiVariableName).korl_memory_arrayU16Compare           = korl_memory_arrayU16Compare;\
    (apiVariableName).korl_memory_acu16_hash                = korl_memory_acu16_hash;\
    (apiVariableName).korl_memory_stringCompare             = korl_memory_stringCompare;\
    (apiVariableName).korl_memory_stringCompareUtf8         = korl_memory_stringCompareUtf8;\
    (apiVariableName).korl_memory_stringSize                = korl_memory_stringSize;\
    (apiVariableName).korl_memory_stringSizeUtf8            = korl_memory_stringSizeUtf8;\
    (apiVariableName).korl_memory_stringCopy                = korl_memory_stringCopy;\
    (apiVariableName).korl_memory_stringFormatVaList        = korl_memory_stringFormatVaList;\
    (apiVariableName).korl_memory_stringFormatVaListUtf8    = korl_memory_stringFormatVaListUtf8;\
    (apiVariableName).korl_memory_stringFormatBuffer        = korl_memory_stringFormatBuffer;\
    (apiVariableName).korl_memory_allocator_create          = korl_memory_allocator_create;\
    (apiVariableName).korl_memory_allocator_allocate        = korl_memory_allocator_allocate;\
    (apiVariableName).korl_memory_allocator_reallocate      = korl_memory_allocator_reallocate;\
    (apiVariableName).korl_memory_allocator_free            = korl_memory_allocator_free;\
    (apiVariableName).korl_memory_allocator_empty           = korl_memory_allocator_empty;\
    (apiVariableName)._korl_stb_ds_reallocate               = _korl_stb_ds_reallocate;\
    (apiVariableName)._korl_stb_ds_free                     = _korl_stb_ds_free;\
    (apiVariableName).korl_assetCache_get                   = korl_assetCache_get;\
    (apiVariableName).korl_resource_fromFile                = korl_resource_fromFile;\
    (apiVariableName).korl_gfx_createCameraFov              = korl_gfx_createCameraFov;\
    (apiVariableName).korl_gfx_createCameraOrtho            = korl_gfx_createCameraOrtho;\
    (apiVariableName).korl_gfx_createCameraOrthoFixedHeight = korl_gfx_createCameraOrthoFixedHeight;\
    (apiVariableName).korl_gfx_cameraFov_rotateAroundTarget = korl_gfx_cameraFov_rotateAroundTarget;\
    (apiVariableName).korl_gfx_useCamera                    = korl_gfx_useCamera;\
    (apiVariableName).korl_gfx_cameraSetScissor             = korl_gfx_cameraSetScissor;\
    (apiVariableName).korl_gfx_cameraSetScissorPercent      = korl_gfx_cameraSetScissorPercent;\
    (apiVariableName).korl_gfx_cameraOrthoSetOriginAnchor   = korl_gfx_cameraOrthoSetOriginAnchor;\
    (apiVariableName).korl_gfx_batch                        = korl_gfx_batch;\
    (apiVariableName).korl_gfx_createBatchRectangleTextured = korl_gfx_createBatchRectangleTextured;\
    (apiVariableName).korl_gfx_createBatchRectangleColored  = korl_gfx_createBatchRectangleColored;\
    (apiVariableName).korl_gfx_createBatchCircle            = korl_gfx_createBatchCircle;\
    (apiVariableName).korl_gfx_createBatchTriangles         = korl_gfx_createBatchTriangles;\
    (apiVariableName).korl_gfx_createBatchLines             = korl_gfx_createBatchLines;\
    (apiVariableName).korl_gfx_createBatchText              = korl_gfx_createBatchText;\
    (apiVariableName).korl_gfx_batchSetBlendState           = korl_gfx_batchSetBlendState;\
    (apiVariableName).korl_gfx_batchSetPosition             = korl_gfx_batchSetPosition;\
    (apiVariableName).korl_gfx_batchSetPosition2d           = korl_gfx_batchSetPosition2d;\
    (apiVariableName).korl_gfx_batchSetPosition2dV2f32      = korl_gfx_batchSetPosition2dV2f32;\
    (apiVariableName).korl_gfx_batchSetScale                = korl_gfx_batchSetScale;\
    (apiVariableName).korl_gfx_batchSetQuaternion           = korl_gfx_batchSetQuaternion;\
    (apiVariableName).korl_gfx_batchSetRotation             = korl_gfx_batchSetRotation;\
    (apiVariableName).korl_gfx_batchSetVertexColor          = korl_gfx_batchSetVertexColor;\
    (apiVariableName).korl_gfx_batchAddLine                 = korl_gfx_batchAddLine;\
    (apiVariableName).korl_gfx_batchSetLine                 = korl_gfx_batchSetLine;\
    (apiVariableName).korl_gfx_batchTextGetAabb             = korl_gfx_batchTextGetAabb;\
    (apiVariableName).korl_gfx_batchTextSetPositionAnchor   = korl_gfx_batchTextSetPositionAnchor;\
    (apiVariableName).korl_gfx_batchRectangleSetSize        = korl_gfx_batchRectangleSetSize;\
    (apiVariableName).korl_gfx_batchRectangleSetColor       = korl_gfx_batchRectangleSetColor;\
    (apiVariableName).korl_gfx_batchCircleSetColor          = korl_gfx_batchCircleSetColor;\
    (apiVariableName).korl_gui_setFontAsset                 = korl_gui_setFontAsset;\
    (apiVariableName).korl_gui_windowBegin                  = korl_gui_windowBegin;\
    (apiVariableName).korl_gui_windowEnd                    = korl_gui_windowEnd;\
    (apiVariableName).korl_gui_windowSetPosition            = korl_gui_windowSetPosition;\
    (apiVariableName).korl_gui_windowSetSize                = korl_gui_windowSetSize;\
    (apiVariableName).korl_gui_setLoopIndex                 = korl_gui_setLoopIndex;\
    (apiVariableName).korl_gui_widgetTextFormat             = korl_gui_widgetTextFormat;\
    (apiVariableName).korl_gui_widgetText                   = korl_gui_widgetText;\
    (apiVariableName).korl_gui_widgetButtonFormat           = korl_gui_widgetButtonFormat;\
    (apiVariableName).korl_bluetooth_query                  = korl_bluetooth_query;\
    (apiVariableName).korl_bluetooth_connect                = korl_bluetooth_connect;\
    (apiVariableName).korl_bluetooth_disconnect             = korl_bluetooth_disconnect;\
    (apiVariableName).korl_bluetooth_read                   = korl_bluetooth_read;
#define KORL_INTERFACE_PLATFORM_API_GET(apiVariableName) \
    _korl_crash_assertConditionFailed     = (apiVariableName)._korl_crash_assertConditionFailed;\
    _korl_log_variadic                    = (apiVariableName)._korl_log_variadic;\
    korl_log_getBuffer                    = (apiVariableName).korl_log_getBuffer;\
    korl_timeStamp                        = (apiVariableName).korl_timeStamp;\
    korl_time_secondsSinceTimeStamp       = (apiVariableName).korl_time_secondsSinceTimeStamp;\
    korl_memory_zero                      = (apiVariableName).korl_memory_zero;\
    korl_memory_copy                      = (apiVariableName).korl_memory_copy;\
    korl_memory_move                      = (apiVariableName).korl_memory_move;\
    korl_memory_compare                   = (apiVariableName).korl_memory_compare;\
    korl_memory_arrayU8Compare            = (apiVariableName).korl_memory_arrayU8Compare;\
    korl_memory_arrayU16Compare           = (apiVariableName).korl_memory_arrayU16Compare;\
    korl_memory_acu16_hash                = (apiVariableName).korl_memory_acu16_hash;\
    korl_memory_stringCompare             = (apiVariableName).korl_memory_stringCompare;\
    korl_memory_stringCompareUtf8         = (apiVariableName).korl_memory_stringCompareUtf8;\
    korl_memory_stringSize                = (apiVariableName).korl_memory_stringSize;\
    korl_memory_stringSizeUtf8            = (apiVariableName).korl_memory_stringSizeUtf8;\
    korl_memory_stringCopy                = (apiVariableName).korl_memory_stringCopy;\
    korl_memory_stringFormatVaList        = (apiVariableName).korl_memory_stringFormatVaList;\
    korl_memory_stringFormatVaListUtf8    = (apiVariableName).korl_memory_stringFormatVaListUtf8;\
    korl_memory_stringFormatBuffer        = (apiVariableName).korl_memory_stringFormatBuffer;\
    korl_memory_allocator_create          = (apiVariableName).korl_memory_allocator_create;\
    korl_memory_allocator_allocate        = (apiVariableName).korl_memory_allocator_allocate;\
    korl_memory_allocator_reallocate      = (apiVariableName).korl_memory_allocator_reallocate;\
    korl_memory_allocator_free            = (apiVariableName).korl_memory_allocator_free;\
    korl_memory_allocator_empty           = (apiVariableName).korl_memory_allocator_empty;\
    _korl_stb_ds_reallocate               = (apiVariableName)._korl_stb_ds_reallocate;\
    _korl_stb_ds_free                     = (apiVariableName)._korl_stb_ds_free;\
    korl_assetCache_get                   = (apiVariableName).korl_assetCache_get;\
    korl_resource_fromFile                = (apiVariableName).korl_resource_fromFile;\
    korl_gfx_createCameraFov              = (apiVariableName).korl_gfx_createCameraFov;\
    korl_gfx_createCameraOrtho            = (apiVariableName).korl_gfx_createCameraOrtho;\
    korl_gfx_createCameraOrthoFixedHeight = (apiVariableName).korl_gfx_createCameraOrthoFixedHeight;\
    korl_gfx_cameraFov_rotateAroundTarget = (apiVariableName).korl_gfx_cameraFov_rotateAroundTarget;\
    korl_gfx_useCamera                    = (apiVariableName).korl_gfx_useCamera;\
    korl_gfx_cameraSetScissor             = (apiVariableName).korl_gfx_cameraSetScissor;\
    korl_gfx_cameraSetScissorPercent      = (apiVariableName).korl_gfx_cameraSetScissorPercent;\
    korl_gfx_cameraOrthoSetOriginAnchor   = (apiVariableName).korl_gfx_cameraOrthoSetOriginAnchor;\
    korl_gfx_batch                        = (apiVariableName).korl_gfx_batch;\
    korl_gfx_createBatchRectangleTextured = (apiVariableName).korl_gfx_createBatchRectangleTextured;\
    korl_gfx_createBatchRectangleColored  = (apiVariableName).korl_gfx_createBatchRectangleColored;\
    korl_gfx_createBatchCircle            = (apiVariableName).korl_gfx_createBatchCircle;\
    korl_gfx_createBatchTriangles         = (apiVariableName).korl_gfx_createBatchTriangles;\
    korl_gfx_createBatchLines             = (apiVariableName).korl_gfx_createBatchLines;\
    korl_gfx_createBatchText              = (apiVariableName).korl_gfx_createBatchText;\
    korl_gfx_batchSetBlendState           = (apiVariableName).korl_gfx_batchSetBlendState;\
    korl_gfx_batchSetPosition             = (apiVariableName).korl_gfx_batchSetPosition;\
    korl_gfx_batchSetPosition2d           = (apiVariableName).korl_gfx_batchSetPosition2d;\
    korl_gfx_batchSetPosition2dV2f32      = (apiVariableName).korl_gfx_batchSetPosition2dV2f32;\
    korl_gfx_batchSetScale                = (apiVariableName).korl_gfx_batchSetScale;\
    korl_gfx_batchSetQuaternion           = (apiVariableName).korl_gfx_batchSetQuaternion;\
    korl_gfx_batchSetRotation             = (apiVariableName).korl_gfx_batchSetRotation;\
    korl_gfx_batchSetVertexColor          = (apiVariableName).korl_gfx_batchSetVertexColor;\
    korl_gfx_batchAddLine                 = (apiVariableName).korl_gfx_batchAddLine;\
    korl_gfx_batchSetLine                 = (apiVariableName).korl_gfx_batchSetLine;\
    korl_gfx_batchTextGetAabb             = (apiVariableName).korl_gfx_batchTextGetAabb;\
    korl_gfx_batchTextSetPositionAnchor   = (apiVariableName).korl_gfx_batchTextSetPositionAnchor;\
    korl_gfx_batchRectangleSetSize        = (apiVariableName).korl_gfx_batchRectangleSetSize;\
    korl_gfx_batchRectangleSetColor       = (apiVariableName).korl_gfx_batchRectangleSetColor;\
    korl_gfx_batchCircleSetColor          = (apiVariableName).korl_gfx_batchCircleSetColor;\
    korl_gui_setFontAsset                 = (apiVariableName).korl_gui_setFontAsset;\
    korl_gui_windowBegin                  = (apiVariableName).korl_gui_windowBegin;\
    korl_gui_windowEnd                    = (apiVariableName).korl_gui_windowEnd;\
    korl_gui_windowSetPosition            = (apiVariableName).korl_gui_windowSetPosition;\
    korl_gui_windowSetSize                = (apiVariableName).korl_gui_windowSetSize;\
    korl_gui_setLoopIndex                 = (apiVariableName).korl_gui_setLoopIndex;\
    korl_gui_widgetTextFormat             = (apiVariableName).korl_gui_widgetTextFormat;\
    korl_gui_widgetText                   = (apiVariableName).korl_gui_widgetText;\
    korl_gui_widgetButtonFormat           = (apiVariableName).korl_gui_widgetButtonFormat;\
    korl_bluetooth_query                  = (apiVariableName).korl_bluetooth_query;\
    korl_bluetooth_connect                = (apiVariableName).korl_bluetooth_connect;\
    korl_bluetooth_disconnect             = (apiVariableName).korl_bluetooth_disconnect;\
    korl_bluetooth_read                   = (apiVariableName).korl_bluetooth_read;
typedef struct KorlPlatformApi
{
    KORL_INTERFACE_PLATFORM_API_DECLARE
} KorlPlatformApi;
