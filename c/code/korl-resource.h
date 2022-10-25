/**
 * What is a "Resource"?  A "Resource" consists of the following components:
 * - (optional) raw file
 * - (optional) encoded asset data 
 * - decoded asset data _or_ code that generates asset data
 * - encoded multimedia asset data
 * What we want is a system which takes in an asset file name _or_ raw asset 
 * data generated by code, and maintains a link between this data & the usable 
 * multimedia asset that derives from this data.
 * For example, we want to be able to link:
 * - image asset (file name)  => vulkan texture
 * - font glyph mesh cache    => SSBO
 * - text glyph instance data => vertex buffer
 * - font glyph page bitmap   => vulkan texture
 * - render texture           => vulkan texture
 * - etc...
 * The user either provides an asset file name, or raw asset data to this system.
 * The user is then provided a handle which can either be stored for later 
 * reuse, or only used on the frame it is requested & then discarded.
 * It makes sense for Resource handles derived from asset files to be of the 
 * transient variety, since file assets can be invalidated and changed at the 
 * start of each frame.
 * It also makes sense for non-file Resources (multimedia assets that are 
 * generated at runtime, such as glyph atlases) to have handles which are stored 
 * in some database for re-use in the future.
 * However, it is possible for _any_ Resource to be invalidated at the start of 
 * any given frame, because we want to retain the ability for the user to load a 
 * memory state in the platform layer at any time.
 * Given the above requirement, how to we ensure that Resource handles between 
 * memory state loads are actually usable?  For file assets, this is easy as the 
 * korl-assetCache is already capable of telling whether or not a file asset is 
 * invalid based on the file's last write using the platforms file system.  
 * What do we do about non-file Resources?
 * - we could maintain a hash of the raw asset data used to create the 
 *   multimedia asset
 * - The API could just be constructed in such a way that the Resource handle 
 *   can be invalidated at any time.  When a KORL memory state is loaded in the 
 *   platform layer, the korl-resource module simply invalidates all 
 *   non-file-backed resources, and we leave it up to the user of this module to 
 *   detect this condition any time a ResourceHandle is used so they can 
 *   "rebuild" the resource at any of those code points.
 * - What if the user is required to provide their own unique identifier for 
 *   each resource they make manually (like a unique string that gets hashed 
 *   when the resource is created)?  In this way, we can pair each 
 *   ResourceHandle with the user-provided unique identifier internally.  Then, 
 *   when a memory state gets loaded the user will have two types of 
 *   ResourceHandles:
 *   - ones which are contained in the korl-resource memory state that just got 
 *     loaded
 *   - ones which are not contained in the korl-resource memory
 *   What happens if the user tries to use a ResourceHandle which is not valid?  
 *   We _should_ be able to just consider this to be an error condition, since 
 *   we can expect _all_ the user's resource memory to be loaded with the rest 
 *   of the memory state!
 * *****************************************************************************
 * - Each Resource will either have a:
 *   - unique identifier
 *   - an asset file name string hash
 * - file resource data should be immutable
 * - non-file resource data should be mutable, exposed via "update" API
 * - changes to non-file resource data should be cached, and the resulting 
 *   encoded multimedia asset data should be generated from all the accumulated 
 *   changes in bulk at the end of each frame
 */
#pragma once
#include "korl-globalDefines.h"
#include "korl-interface-platform.h"
korl_internal void korl_resource_initialize(void);
korl_internal KORL_PLATFORM_RESOURCE_FROM_FILE(korl_resource_fromFile);
korl_internal Korl_Resource_Handle korl_resource_createVertexBuffer(const struct Korl_Vulkan_CreateInfoVertexBuffer* createInfo);
korl_internal Korl_Resource_Handle korl_resource_createTexture(const struct Korl_Vulkan_CreateInfoTexture* createInfo);
korl_internal void korl_resource_destroy(Korl_Resource_Handle handle);
korl_internal void korl_resource_update(Korl_Resource_Handle handle, const void* data, u$ dataBytes);
korl_internal void korl_resource_resize(Korl_Resource_Handle handle, u$ newByteSize);
korl_internal void korl_resource_flushUpdates(void);
korl_internal Korl_Vulkan_DeviceMemory_AllocationHandle korl_resource_getVulkanDeviceMemoryAllocationHandle(Korl_Resource_Handle handle);
korl_internal void korl_resource_saveStateWrite(void* memoryContext, u8** pStbDaSaveStateBuffer);
/** I don't like how this API requires us to do file I/O in modules outside of 
 * korl-file; maybe improve this in the future to use korl-file API instea of 
 * Win32 API?
 * KORL-ISSUE-000-000-069: savestate/file: contain filesystem API to korl-file? */
korl_internal bool korl_resource_saveStateRead(HANDLE hFile);
