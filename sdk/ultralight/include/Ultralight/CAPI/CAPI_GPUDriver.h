/**************************************************************************************************
 *  This file is a part of Ultralight, an ultra-portable web-browser engine.                      *
 *                                                                                                *
 *  See <https://ultralig.ht> for licensing and more.                                             *
 *                                                                                                *
 *  (C) 2024 Ultralight, Inc.                                                                     *
 **************************************************************************************************/

// clang-format off

///
/// @file CAPI_GPUDriver.h
///
/// User-defined GPU driver interface.
///
/// `#include <Ultralight/CAPI/CAPI_GPUDriver.h>`
///
/// The library uses this to optionally render Views on the GPU (see ulViewIsAccelerated()).
///
/// You can provide the library with your own GPU driver implementation so that all rendering is
/// performed using an existing GPU context (useful for game engines).
///
/// When a View is rendered on the GPU, you can retrieve the backing texture ID via
/// ulViewGetRenderTarget().
///
/// @see ulPlatformSetGPUDriver()
///
#ifndef ULTRALIGHT_CAPI_GPUDRIVER_H
#define ULTRALIGHT_CAPI_GPUDRIVER_H

#include <Ultralight/CAPI/CAPI_Defines.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 * GPUDriver
 *****************************************************************************/

///
/// Render buffer description.
///
/// This structure describes a render buffer that can be used as a target for drawing commands.
///
typedef struct {
  unsigned int texture_id; ///< The backing texture for this RenderBuffer
  unsigned int width;      ///< The width of the RenderBuffer texture
  unsigned int height;     ///< The height of the RenderBuffer texture
  bool has_stencil_buffer; ///< Currently unused, always false.
  bool has_depth_buffer;   ///< Currently unsued, always false.
} ULRenderBuffer;

/// \cond ignore
///   This pragma pack(push, 1) command is important! 
///   GPU structs should not be padded with any bytes.
/// \endcond
#pragma pack(push, 1)

///
/// Vertex layout for path vertices.
///
/// This struct is the in-memory layout for each path vertex (useful for synthesizing or modifying
/// your own vertex data).
///
typedef struct {
  float pos[2];
  unsigned char color[4];
  float obj[2];
} ULVertex_2f_4ub_2f;

///
/// Vertex layout for quad vertices.
///
/// This struct is the in-memory layout for each quad vertex (useful for synthesizing or modifying
/// your own vertex data).
///
typedef struct {
  float pos[2];
  unsigned char color[4];
  float tex[2];
  float obj[2];
  float data0[4];
  float data1[4];
  float data2[4];
  float data3[4];
  float data4[4];
  float data5[4];
  float data6[4];
} ULVertex_2f_4ub_2f_2f_28f;

///
/// End single-byte alignment.
///
#pragma pack(pop)

///
/// Vertex buffer formats.
///
/// This enumeration describes the format of a vertex buffer.
///
typedef enum {
  kVertexBufferFormat_2f_4ub_2f,        ///< Vertex_2f_4ub_2f (used for path rendering)
  kVertexBufferFormat_2f_4ub_2f_2f_28f, ///< Vertex_2f_4ub_2f_2f_28f (used for quad rendering)
} ULVertexBufferFormat;

///
/// Vertex buffer description.
///
/// @see ULGPUDriver::create_geometry
///
typedef struct {
  ULVertexBufferFormat format; ///< The format of the vertex buffer.
  unsigned int size;           ///< The size of the vertex buffer in bytes.
  unsigned char* data;         ///< The raw vertex buffer data.
} ULVertexBuffer;

///
/// Vertex index type.
///
typedef unsigned int ULIndexType;

///
/// Index buffer description.
///
/// This structure describes an index buffer that can be used to index into a vertex buffer.
///
/// @note The index buffer is a simple array of IndexType values.
///
typedef struct {
  unsigned int size;   ///< The size of the index buffer in bytes.
  unsigned char* data; ///< The raw index buffer data.
} ULIndexBuffer;

///
/// Shader program types, used with ULGPUState::shader_type
///
/// Each of these correspond to a vertex/pixel shader pair. You can find stock shader code for these
/// in the `shaders` folder of the AppCore repo.
///
typedef enum {
  kShaderType_Fill,     ///< Shader program for filling quad geometry.
  kShaderType_FillPath, ///< Shader program for filling tesselated path geometry.
} ULShaderType;

///
/// Raw 4x4 matrix as an array of floats in column-major order.
///
typedef struct {
  float data[16];
} ULMatrix4x4;

///
/// 4-component float vector
///
typedef struct {
  float value[4];
} ULvec4;

///
/// The state of the GPU for a given draw command.
///
/// This structure describes the current state of the GPU for a given draw command.
///
typedef struct {
  /// Viewport width in pixels
  unsigned int viewport_width;

  /// Viewport height in pixels
  unsigned int viewport_height;

  /// Transform matrix-- you should multiply this with the screen-space orthographic projection
  /// matrix then pass to the vertex shader.
  ULMatrix4x4 transform;

  /// Whether or not we should enable texturing for the current draw command.
  bool enable_texturing;

  /// Whether or not we should enable blending for the current draw command. If blending is
  /// disabled, any drawn pixels should overwrite existing. Mainly used so we can modify alpha
  /// values of the RenderBuffer during scissored clears.
  bool enable_blend;

  /// The vertex/pixel shader program pair to use for the current draw command. You should cast this
  /// to ShaderType to get the corresponding enum.
  unsigned char shader_type;

  /// The render buffer to use for the current draw command.
  unsigned int render_buffer_id;

  /// The texture id to bind to slot #1. (Will be 0 if none)
  unsigned int texture_1_id;

  /// The texture id to bind to slot #2. (Will be 0 if none)
  unsigned int texture_2_id;

  /// The texture id to bind to slot #3. (Will be 0 if none)
  unsigned int texture_3_id;

  /// The uniform scalars (passed to the pixel shader via uniforms).
  float uniform_scalar[8];

  /// The uniform vectors (passed to the pixel shader via uniforms).
  ULvec4 uniform_vector[8];

  /// The clip size (passed to the pixel shader via uniforms).
  unsigned char clip_size;

  /// The clip stack (passed to the pixel shader via uniforms).
  ULMatrix4x4 clip[8];

  /// Whether or not scissor testing should be used for the current draw command.
  bool enable_scissor;

  /// The scissor rect to use for scissor testing (units in pixels)
  ULIntRect scissor_rect;
} ULGPUState;

///
/// The types of commands.
///
/// This enumeration describes the type of command to execute on the GPU. Used with
/// ULCommand::command_type
///
typedef enum {
  kCommandType_ClearRenderBuffer, ///< Clear the specified render buffer.
  kCommandType_DrawGeometry,      ///< Draw the specified geometry to the specified render buffer.
} ULCommandType;

///
/// A command to execute on the GPU.
///
/// This structure describes a command to be executed on the GPU.
///
/// Commands are dispatched to the GPU driver asynchronously via ULGPUDriver::update_command_list,
/// the GPU driver should consume these commands and execute them at an appropriate time.
///
/// @see ULCommandList
///
typedef struct {
  unsigned char command_type;  ///< The type of command to dispatch.
  ULGPUState gpu_state;        ///< The current GPU state.
  unsigned int geometry_id;    ///< The geometry ID to bind. (used with kCommandType_DrawGeometry)
  unsigned int indices_count;  ///< The number of indices.   (used with kCommandType_DrawGeometry)
  unsigned int indices_offset; ///< The index to start from. (used with kCommandType_DrawGeometry)
} ULCommand;

///
/// List of commands to execute on the GPU.
///
/// @see ULGPUDriver::update_command_list
///
typedef struct {
  unsigned int size;   ///< The number of commands in the list.
  ULCommand* commands; ///< The raw command list data.
} ULCommandList;

///
/// Callback for users to implement ULGPUDriver::begin_synchronize.
///
/// Called before any state (eg, create_texture(), update_texture(), destroy_texture(), etc.) is
/// updated during a call to ulRender().
///
/// This is a good time to prepare the GPU for any state updates.
///
typedef void (*ULGPUDriverBeginSynchronizeCallback)();

///
/// Callback for users to implement ULGPUDriver::end_synchronize.
///
/// Called after all state has been updated during a call to ulRender().
///
typedef void (*ULGPUDriverEndSynchronizeCallback)();

///
/// Callback for users to implement ULGPUDriver::next_texture_id.
///
/// Get the next available texture ID.
///
/// This is used to generate a unique texture ID for each texture created by the library. The
/// GPU driver implementation is responsible for mapping these IDs to a native ID.
///
/// @note Numbering should start at 1, 0 is reserved for "no texture".
///
/// @return Returns the next available texture ID.
///
typedef unsigned int (*ULGPUDriverNextTextureIdCallback)();

///
/// Callback for users to implement ULGPUDriver::create_texture.
///
/// Create a texture with a certain ID and optional bitmap.
///
/// @param texture_id  The texture ID to use for the new texture.
///
/// @param bitmap      The bitmap to initialize the texture with (can be empty).
///
/// @note If the Bitmap is empty (ulBitmapIsEmpty()), then a RTT Texture should be created instead.
///       This will be used as a backing texture for a new RenderBuffer.
///
/// @warning A deep copy of the bitmap data should be made if you are uploading it to the GPU
///          asynchronously, it will not persist beyond this call.
///
typedef void (*ULGPUDriverCreateTextureCallback)(unsigned int texture_id, ULBitmap bitmap);

///
/// Callback for users to implement ULGPUDriver::update_texture.
///
/// Update an existing non-RTT texture with new bitmap data.
///
/// @param texture_id  The texture to update.
///
/// @param bitmap      The new bitmap data.
///
/// @warning A deep copy of the bitmap data should be made if you are uploading it to the GPU
///          asynchronously, it will not persist beyond this call.
///
typedef void (*ULGPUDriverUpdateTextureCallback)(unsigned int texture_id, ULBitmap bitmap);

///
/// Callback for users to implement ULGPUDriver::destroy_texture.
///
/// Destroy a texture.
///
/// @param texture_id  The texture to destroy.
///
typedef void (*ULGPUDriverDestroyTextureCallback)(unsigned int texture_id);

///
/// Callback for users to implement ULGPUDriver::next_render_buffer_id.
///
/// Get the next available render buffer ID.
///
/// This is used to generate a unique render buffer ID for each render buffer created by the
/// library. The GPU driver implementation is responsible for mapping these IDs to a native ID.
///
/// @note Numbering should start at 1, 0 is reserved for "no render buffer".
///
/// @return Returns the next available render buffer ID.
///
typedef unsigned int (*ULGPUDriverNextRenderBufferIdCallback)();

///
/// Callback for users to implement ULGPUDriver::create_render_buffer.
///
/// Create a render buffer with certain ID and buffer description.
///
/// @param render_buffer_id  The render buffer ID to use for the new render buffer.
///
/// @param buffer           The render buffer description.
///
typedef void (*ULGPUDriverCreateRenderBufferCallback)(unsigned int render_buffer_id,
                                                      ULRenderBuffer buffer);

///
/// Callback for users to implement ULGPUDriver::destroy_render_buffer.
///
/// Destroy a render buffer.
///
/// @param render_buffer_id  The render buffer to destroy.
///
typedef void (*ULGPUDriverDestroyRenderBufferCallback)(unsigned int render_buffer_id);

///
/// Callback for users to implement ULGPUDriver::next_geometry_id.
///
/// Get the next available geometry ID.
///
/// This is used to generate a unique geometry ID for each geometry created by the library. The
/// GPU driver implementation is responsible for mapping these IDs to a native ID.
///
/// @note Numbering should start at 1, 0 is reserved for "no geometry".
///
/// @return Returns the next available geometry ID.
///
typedef unsigned int (*ULGPUDriverNextGeometryIdCallback)();

///
/// Callback for users to implement ULGPUDriver::create_geometry.
///
/// Create geometry with certain ID and vertex/index data.
///
/// @param geometry_id  The geometry ID to use for the new geometry.
///
/// @param vertices     The vertex buffer data.
///
/// @param indices      The index buffer data.
///
/// @warning A deep copy of the vertex/index data should be made if you are uploading it to the
///          GPU asynchronously, it will not persist beyond this call.
///
typedef void (*ULGPUDriverCreateGeometryCallback)(unsigned int geometry_id, ULVertexBuffer vertices,
                                                  ULIndexBuffer indices);

///
/// Callback for users to implement ULGPUDriver::update_geometry.
///
/// Update existing geometry with new vertex/index data.
///
/// @param geometry_id  The geometry to update.
///
/// @param vertices     The new vertex buffer data.
///
/// @param indices      The new index buffer data.
///
/// @warning A deep copy of the vertex/index data should be made if you are uploading it to the
///          GPU asynchronously, it will not persist beyond this call.
///
typedef void (*ULGPUDriverUpdateGeometryCallback)(unsigned int geometry_id, ULVertexBuffer vertices,
                                                  ULIndexBuffer indices);

///
/// Callback for users to implement ULGPUDriver::destroy_geometry.
///
/// Destroy geometry.
///
/// @param geometry_id  The geometry to destroy.
///
typedef void (*ULGPUDriverDestroyGeometryCallback)(unsigned int geometry_id);

///
/// Callback for users to implement ULGPUDriver::update_command_list.
///
/// Update the pending command list with commands to execute on the GPU.
///
/// Commands are dispatched to the GPU driver asynchronously via this method. The GPU driver
/// implementation should consume these commands and execute them at an appropriate time.
///
/// @param list  The list of commands to execute.
///
/// @warning Implementations should make a deep copy of the command list, it will not persist
///          beyond this call.
///
typedef void (*ULGPUDriverUpdateCommandListCallback)(ULCommandList list);

/// 
/// User-defined GPU driver interface.
///
/// You should implement each of these callbacks, then pass an instance of this struct containing
/// your callbacks to ulPlatformSetGPUDriver().
///
typedef struct {
  ULGPUDriverBeginSynchronizeCallback begin_synchronize;
  ULGPUDriverEndSynchronizeCallback end_synchronize;
  ULGPUDriverNextTextureIdCallback next_texture_id;
  ULGPUDriverCreateTextureCallback create_texture;
  ULGPUDriverUpdateTextureCallback update_texture;
  ULGPUDriverDestroyTextureCallback destroy_texture;
  ULGPUDriverNextRenderBufferIdCallback next_render_buffer_id;
  ULGPUDriverCreateRenderBufferCallback create_render_buffer;
  ULGPUDriverDestroyRenderBufferCallback destroy_render_buffer;
  ULGPUDriverNextGeometryIdCallback next_geometry_id;
  ULGPUDriverCreateGeometryCallback create_geometry;
  ULGPUDriverUpdateGeometryCallback update_geometry;
  ULGPUDriverDestroyGeometryCallback destroy_geometry;
  ULGPUDriverUpdateCommandListCallback update_command_list;
} ULGPUDriver;

///
/// Sets up an orthographic projection matrix with a certain viewport width and height, multiplies
/// it by 'transform', and returns the result.
///
/// This should be used to calculate the model-view projection matrix for the vertex shaders using
/// the current ULGPUState.
///
/// The 'flip_y' can be optionally used to flip the Y coordinate-space. (Usually flip_y == true for
/// OpenGL)
///
ULExport ULMatrix4x4 ulApplyProjection(ULMatrix4x4 transform, float viewport_width,
                                       float viewport_height, bool flip_y);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // ULTRALIGHT_CAPI_GPUDRIVER_H

// clang-format on