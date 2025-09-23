/**************************************************************************************************
 *  This file is a part of Ultralight.                                                            *
 *                                                                                                *
 *  See <https://ultralig.ht> for licensing and more.                                             *
 *                                                                                                *
 *  (C) 2024 Ultralight, Inc.                                                                     *
 **************************************************************************************************/
#pragma once
#include <Ultralight/Defines.h>
#include <Ultralight/Bitmap.h>
#include <Ultralight/Geometry.h>

namespace ultralight {

#pragma pack(push, 1)

///
/// Offscreen render target, used when rendering Views via the GPU renderer.
///
/// When a View is rendered via the GPU renderer (see View::is_accelerated), it will be rendered to
/// an offscreen render target (View::render_target) that you can display in your application.
///
/// This is intended to be used with a custom GPUDriver implementation in a game or similar 
/// application.
///
/// ## Displaying the Render Target
///
/// To display the View's render target, you should:
///
///  1. Retrieve the underlying texture via RenderTarget::texture_id().
///  2. Bind the texture using your custom GPUDriver implementation.
///  3. Draw a textured quad with the provided UV coordinates (RenderTarget::uv_coords()).
///
struct UExport RenderTarget {
  ///
  /// Whether this target is empty (null texture)
  ///
  bool is_empty;

  ///
  /// The viewport width (in device coordinates).
  ///
  uint32_t width;

  ///
  /// The viewport height (in device coordinates).
  ///
  uint32_t height;

  ///
  /// The GPUDriver-specific texture ID (you should bind the texture using your custom GPUDriver
  /// implementation before drawing a quad).
  ///
  uint32_t texture_id;

  ///
  /// The texture width (in pixels). This may be padded.
  ///
  uint32_t texture_width;

  ///
  /// The texture height (in pixels). This may be padded.
  ///
  uint32_t texture_height;

  ///
  /// The pixel format of the texture.
  ///
  BitmapFormat texture_format;

  ///
  /// UV coordinates of the texture (this is needed because the texture may be padded).
  ///
  Rect uv_coords;

  ///
  /// The GPUDriver-specific render buffer ID.
  ///
  uint32_t render_buffer_id;

  RenderTarget();
};

#pragma pack(pop)

} // namespace ultralight
