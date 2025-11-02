/**************************************************************************************************
 *  This file is a part of Ultralight.                                                            *
 *                                                                                                *
 *  See <https://ultralig.ht> for licensing and more.                                             *
 *                                                                                                *
 *  (C) 2024 Ultralight, Inc.                                                                     *
 **************************************************************************************************/

// clang-format off
#pragma once
#pragma warning(disable : 4251)
#include <Ultralight/Defines.h>
#include <Ultralight/Geometry.h>
#include <Ultralight/Matrix.h>
#include <Ultralight/Bitmap.h>

namespace ultralight {

/// \cond ignore
///   This pragma pack(push, 1) command is important! 
///   GPU structs should not be padded with any bytes.
/// \endcond
#pragma pack(push, 1)

///
/// Render buffer description.
///
/// This structure describes a render buffer that can be used as a target for drawing commands.
///
/// @see GPUDriver::CreateRenderBuffer.
///
struct UExport RenderBuffer {
  uint32_t texture_id;     ///< The backing texture for this RenderBuffer
  uint32_t width;          ///< The width of the RenderBuffer texture
  uint32_t height;         ///< The height of the RenderBuffer texture
  bool has_stencil_buffer; ///< Currently unused, always false.
  bool has_depth_buffer;   ///< Currently unsued, always false.
};

///
/// Vertex layout for path vertices.
///
/// This struct is the in-memory layout for each path vertex (useful for synthesizing or modifying
/// your own vertex data).
///
struct Vertex_2f_4ub_2f {
  float pos[2];
  unsigned char color[4];
  float obj[2];
};

///
/// Vertex layout for quad vertices.
///
/// This struct is the in-memory layout for each quad vertex (useful for synthesizing or modifying
/// your own vertex data).
///
struct Vertex_2f_4ub_2f_2f_28f {
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
};

///
/// Vertex buffer formats.
///
/// This enumeration describes the format of a vertex buffer.
///
/// @note  Identifiers start with an underscore due to C++ naming rules.
///
/// @see VertexBuffer
///
enum class VertexBufferFormat : uint8_t {
  _2f_4ub_2f,        ///< Vertex_2f_4ub_2f (used for path rendering)
  _2f_4ub_2f_2f_28f, ///< Vertex_2f_4ub_2f_2f_28f (used for quad rendering)
};

///
/// Vertex buffer description.
///
/// @see GPUDriver::CreateGeometry
///
struct UExport VertexBuffer {
  VertexBufferFormat format; ///< The format of the vertex buffer.
  uint32_t size;             ///< The size of the vertex buffer in bytes.
  uint8_t* data;             ///< The raw vertex buffer data.
};

///
/// Vertex index type.
///
typedef uint32_t IndexType;

///
/// Index buffer description.
///
/// This structure describes an index buffer that can be used to index into a vertex buffer.
///
/// @note The index buffer is a simple array of IndexType values.
///
/// @see GPUDriver::CreateGeometry
///
struct UExport IndexBuffer {
  uint32_t size; ///< The size of the index buffer in bytes.
  uint8_t* data; ///< The raw index buffer data.
};

///
/// Shader program types.
///
/// Each of these correspond to a vertex/pixel shader pair. You can find stock shader code for
/// these in the `shaders` folder of the AppCore repo.
///
/// @see GPUState::shader_type
///
enum class ShaderType : uint8_t {
  Fill,     ///< Shader program for filling quad geometry.
  FillPath, ///< Shader program for filling tesselated path geometry.
  FilterBasic, ///< Shader program for basic CSS/SVG filters.
  FilterBlur,  ///< Shader program for blur CSS/SVG filters.
  FilterDropShadow, ///< Shader program for drop-shadow CSS/SVG filters.
};

///
/// The state of the GPU for a given draw command.
///
/// This structure describes the current state of the GPU for a given draw command.
///
/// @see Command::gpu_state
///
struct UExport GPUState {
  /// Viewport width in pixels
  uint32_t viewport_width;

  /// Viewport height in pixels
  uint32_t viewport_height;

  /// Transform matrix-- you should multiply this with the screen-space orthographic projection
  /// matrix then pass to the vertex shader.
  Matrix4x4 transform;

  /// Whether or not we should enable texturing for the current draw command.
  bool enable_texturing;

  /// Whether or not we should enable blending for the current draw command. If blending is
  /// disabled, any drawn pixels should overwrite existing. This is mainly used so we can modify
  /// alpha values of the RenderBuffer during scissored clears.
  bool enable_blend;

  /// The vertex/pixel shader program pair to use for the current draw command.
  ShaderType shader_type;

  /// The render buffer to use for the current draw command.
  uint32_t render_buffer_id;

  /// The texture id to bind to slot #1. (Will be 0 if none)
  uint32_t texture_1_id;

  /// The texture id to bind to slot #2. (Will be 0 if none)
  uint32_t texture_2_id;

  /// The texture id to bind to slot #3. (Will be 0 if none)
  uint32_t texture_3_id;

  /// The uniform integers (passed to the pixel shader via uniforms).
  int32_t uniform_integer[8];

  /// The uniform scalars (passed to the pixel shader via uniforms).
  float uniform_scalar[8];

  /// The uniform vectors (passed to the pixel shader via uniforms).
  vec4 uniform_vector[8];

  /// The clip size (passed to the pixel shader via uniforms).
  uint8_t clip_size;

  /// The clip stack (passed to the pixel shader via uniforms).
  Matrix4x4 clip[8];

  /// Whether or not scissor testing should be used for the current draw command.
  bool enable_scissor;

  /// The scissor rect to use for scissor testing (units in pixels)
  IntRect scissor_rect;
};

///
/// The types of commands.
///
/// This enumeration describes the type of command to execute on the GPU.
/// 
/// @see Command
///
enum class CommandType : uint8_t {
  ClearRenderBuffer, ///< Clear the specified render buffer.
  DrawGeometry,      ///< Draw the specified geometry to the specified render buffer.
};

///
/// A command to execute on the GPU.
///
/// This structure describes a command to be executed on the GPU.
///
/// Commands are dispatched to the GPU driver asynchronously via GPUDriver::UpdateCommandList(),
/// the GPU driver should consume these commands and execute them at an appropriate time.
///
/// @see CommandList
///
struct UExport Command {
  CommandType command_type; ///< The type of command to dispatch.
  GPUState gpu_state;       ///< The current GPU state.
  uint32_t geometry_id;     ///< The geometry ID to bind. (used with CommandType::DrawGeometry)
  uint32_t indices_count;   ///< The number of indices.   (used with CommandType::DrawGeometry)  
  uint32_t indices_offset;  ///< The index to start from. (used with CommandType::DrawGeometry)
};

///
/// List of commands to execute on the GPU.
///
/// @see GPUDriver::UpdateCommandList
///
struct UExport CommandList {
  uint32_t size;     ///< The number of commands in the list.
  Command* commands; ///< The raw command list data.
};

#pragma pack(pop)

///
/// User-defined GPU driver interface.
/// 
/// The library uses this to optionally render Views on the GPU (see ViewConfig::is_accelerated).
///
/// You can provide the library with your own GPU driver implementation so that all rendering is
/// performed using an existing GPU context (useful for game engines).
///
/// When a View is rendered on the GPU, you can retrieve the backing texture ID via
/// View::render_target().
/// 
/// ## Default Implementation
///
/// A platform-specific implementation of GPUDriver is provided for you when you call App::Create(),
/// (currently D3D11, Metal, and OpenGL). We recommend using these classes as a starting point for
/// your own implementation (available open-source in the AppCore repository on GitHub).
///
/// ## Setting the GPU Driver
///
/// When using Renderer::Create(), you can provide your own implementation of this
/// class via Platform::set_gpu_driver().
///
/// ## State Synchronization
///
/// During each call to Renderer::Render(), the library will update the state of the GPU driver
/// (textures, render buffers, geometry, command lists, etc.) to match the current state of the
/// library.
///
/// ### Detecting State Changes
///
/// The library will call BeginSynchronize() before any state is updated and EndSynchronize() after
/// all state is updated. All `Create` / `Update` / `Destroy` calls will be made between these two
/// calls.
///
/// This allows the GPU driver implementation to prepare the GPU for any state changes.
/// 
/// ## Drawing
///
/// All drawing is done via command lists (UpdateCommandList()) to allow asynchronous execution
/// of commands on the GPU.
///
/// The library will dispatch a list of commands to the GPU driver during state synchronization. The
/// GPU driver implementation should periodically consume the command list and execute the commands
/// at an appropriate time.
///
/// @see Platform::set_gpu_driver()
///
class UExport GPUDriver {
 public:
  virtual ~GPUDriver();

  ///
  /// Called before any state (eg, CreateTexture(), UpdateTexture(), DestroyTexture(), etc.) is
  /// updated during a call to Renderer::Render().
  ///
  /// This is a good time to prepare the GPU for any state updates.
  ///
  virtual void BeginSynchronize() = 0;

  ///
  /// Called after all state has been updated during a call to Renderer::Render().
  ///
  virtual void EndSynchronize() = 0;

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
  virtual uint32_t NextTextureId() = 0;

  ///
  /// Create a texture with a certain ID and optional bitmap.
  ///
  /// @param texture_id  The texture ID to use for the new texture.
  ///
  /// @param bitmap      The bitmap to initialize the texture with (can be empty).
  ///
  /// @note If the Bitmap is empty (Bitmap::IsEmpty), then a RTT Texture should be created instead.
  ///       This will be used as a backing texture for a new RenderBuffer.
  ///
  /// @warning A deep copy of the bitmap data should be made if you are uploading it to the GPU
  ///          asynchronously, it will not persist beyond this call.
  ///
  virtual void CreateTexture(uint32_t texture_id, RefPtr<Bitmap> bitmap) = 0;

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
  virtual void UpdateTexture(uint32_t texture_id, RefPtr<Bitmap> bitmap) = 0;

  ///
  /// Destroy a texture.
  ///
  /// @param texture_id  The texture to destroy.
  ///
  virtual void DestroyTexture(uint32_t texture_id) = 0;

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
  virtual uint32_t NextRenderBufferId() = 0;

  ///
  /// Create a render buffer with certain ID and buffer description.
  ///
  /// @param render_buffer_id  The render buffer ID to use for the new render buffer.
  ///
  /// @param buffer           The render buffer description.
  ///
  virtual void CreateRenderBuffer(uint32_t render_buffer_id, const RenderBuffer& buffer) = 0;

  ///
  /// Destroy a render buffer.
  ///
  /// @param render_buffer_id  The render buffer to destroy.
  ///
  virtual void DestroyRenderBuffer(uint32_t render_buffer_id) = 0;

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
  virtual uint32_t NextGeometryId() = 0;

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
  virtual void CreateGeometry(uint32_t geometry_id, const VertexBuffer& vertices,
                              const IndexBuffer& indices)
      = 0;

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
  virtual void UpdateGeometry(uint32_t geometry_id, const VertexBuffer& vertices,
                              const IndexBuffer& indices)
      = 0;

  ///
  /// Destroy geometry.
  ///
  /// @param geometry_id  The geometry to destroy.
  ///
  virtual void DestroyGeometry(uint32_t geometry_id) = 0;

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
  virtual void UpdateCommandList(const CommandList& list) = 0;
};

} // namespace ultralight

// clang-format on