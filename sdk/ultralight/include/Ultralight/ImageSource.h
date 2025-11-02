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
#include <Ultralight/String.h>

namespace ultralight {

class ImageSourceListener;
class ImageSourceProviderListener;

///
/// User-defined image source to display custom images on a web-page.
///
/// This API allows you to composite your own images into a web-page. This is useful for displaying
/// in-game textures, external image assets, or other custom content.
///
/// ## ImageSource File Format
///
/// To use an ImageSource, you must first create an `.imgsrc` file containing a string identifying
/// the image source. This string will be used to lookup the ImageSource from ImageSourceProvider
/// when it is loaded on a web-page.
///
/// The file format is as follows:
///
/// ```
/// IMGSRC-V1
/// <identifier>
/// ```
///
/// You can use the `.imgsrc` file anywhere in your web-page that typically accepts an image URL.
/// For example:
///
/// ```html
/// <img src="my_custom_image.imgsrc" />
/// ```
///
/// ## Creating from a GPU Texture
///
/// To composite your own GPU texture on a web-page, you should first reserve a texture ID from
/// GPUDriver::NextTextureId() and then create an ImageSource from that texture ID. Next, you should
/// register the ImageSource with ImageSourceProvider using the identifier from the `.imgsrc` file.
///
/// When the image element is drawn on the web-page, the library will draw geometry using the
/// specified texture ID and UV coordinates. You should bind your own texture when the specified
/// texture ID is used.
///
//// @note If the GPU renderer is not enabled for the View or pixel data is needed for other
///        purposes, the library will sample the backing bitmap instead.
///
/// ## Creating from a Bitmap
///
/// To composite your own bitmap on a web-page, you should create an ImageSource from a Bitmap.
/// Next, you should register the ImageSource with ImageSourceProvider using the identifier from
/// the `.imgsrc` file.
///
/// When the image element is drawn on the web-page, the library will sample this bitmap directly.
///
/// ## Invalidating Images
///
/// If you modify the texture or bitmap pixels after creating the ImageSource, you should call
/// ImageSource::Invalidate() to notify the library that the image should be redrawn.
///
class UExport ImageSource : public RefCounted {
 public:
  ///
  /// Create an ImageSource from a GPU texture with optional backing bitmap.
  ///
  /// @param  width  The width of the image in pixels (used for layout).
  ///
  /// @param  height  The height of the image in pixels (used for layout).
  ///
  /// @param  texture_id  The GPU texture identifier to bind when drawing the quad for this image.
  ///                     This should be non-zero and obtained from GPUDriver::NextTextureId().
  ///
  /// @param  texture_uv  The UV coordinates of the texture.
  ///
  /// @param  bitmap  Optional backing bitmap for this image source. This is used when drawing
  ///                 the image using the CPU renderer or when pixel data is needed for other
  ///                 purposes. You should update this bitmap when the texture changes.
  ///
  /// @return  A new ImageSource instance.
  ///
  static RefPtr<ImageSource> CreateFromTexture(uint32_t width, uint32_t height, uint32_t texture_id,
                                               const Rect& texture_uv,
                                               RefPtr<Bitmap> bitmap = nullptr);

  ///
  /// Create an ImageSource from a Bitmap.
  ///
  /// @param  bitmap  The backing bitmap for this image source.
  ///
  /// @return  A new ImageSource instance.
  ///
  static RefPtr<ImageSource> CreateFromBitmap(RefPtr<Bitmap> bitmap);

  ///
  /// Get the width of the image in pixels.
  ///
  virtual uint32_t width() const = 0;

  ///
  /// Get the height of the image in pixels.
  ///
  virtual uint32_t height() const = 0;

  ///
  /// Get the GPU texture identifier to bind when drawing the quad for this image.
  ///
  /// @note This will be zero (0) if the image source was created from a bitmap.
  ///
  virtual uint32_t texture_id() const = 0;

  ///
  /// Get the UV coordinates of the texture.
  ///
  virtual Rect texture_uv() const = 0;

  ///
  /// Get the backing bitmap for this image source.
  ///
  virtual RefPtr<Bitmap> bitmap() = 0;

  ///
  /// Invalidate the image.
  ///
  /// This will notify the library that the image has changed and should be redrawn.
  ///
  virtual void Invalidate() = 0;

  ///
  /// Add a listener to the image source.
  ///
  /// @param  listener  The listener to add.
  ///
  virtual void AddListener(ImageSourceListener* listener) = 0;

  ///
  /// Remove a listener from the image source.
  ///
  /// @param  listener  The listener to remove.
  ///
  virtual void RemoveListener(ImageSourceListener* listener) = 0;

 protected:
  ImageSource() = default;
  virtual ~ImageSource() = default;
  ImageSource(const ImageSource&) = delete;
  void operator=(const ImageSource&) = delete;
};

///
/// Listener for ImageSource events.
///
/// This is used to notify listeners when the image source is invalidated.
///
class UExport ImageSourceListener {
 public:
  virtual ~ImageSourceListener() = default;

  ///
  /// Called when the image source is invalidated.
  ///
  /// @param  image_source  The image source that was invalidated.
  ///
  virtual void OnInvalidateImageSource(ImageSource* image_source) = 0;
};

///
/// Maps image sources to string identifiers.
///
/// This is used to lookup ImageSource instances when they are requested by a web-page.
///
class UExport ImageSourceProvider {
 public:
  ///
  /// Get the ImageSourceProvider singleton.
  ///
  static ImageSourceProvider& instance();

  ///
  /// Get an ImageSource by its identifier.
  ///
  /// @param  id  The identifier of the image source.
  ///
  /// @return  The ImageSource instance or nullptr if not found.
  ///
  virtual RefPtr<ImageSource> GetImageSource(const String& id) = 0;

  ///
  /// Add an ImageSource to the provider.
  ///
  /// @param  id  The identifier of the image source.
  ///
  /// @param  image_source  The ImageSource instance.
  ///
  virtual void AddImageSource(const String& id, RefPtr<ImageSource> image_source) = 0;

  ///
  /// Remove an ImageSource from the provider.
  ///
  /// @param  id  The identifier of the image source.
  ///
  virtual void RemoveImageSource(const String& id) = 0;

  ///
  /// Add a listener to the provider.
  ///
  /// @param  listener  The listener to add.
  ///
  virtual void AddListener(ImageSourceProviderListener* listener) = 0;

  ///
  /// Remove a listener from the provider.
  ///
  /// @param  listener  The listener to remove.
  ///
  virtual void RemoveListener(ImageSourceProviderListener* listener) = 0;

 protected:
  virtual ~ImageSourceProvider() = default;
};

///
/// Listener for ImageSourceProvider events.
///
/// This is used to notify listeners when an ImageSource is added or removed from the provider.
///
class ImageSourceProviderListener {
 public:
  virtual ~ImageSourceProviderListener() = default;

  ///
  /// Called when an ImageSource is added to the provider.
  ///
  /// @param  id  The identifier of the image source.
  ///
  /// @param  image_source  The ImageSource instance.
  ///
  virtual void OnAddImageSource(const String& id, RefPtr<ImageSource> image_source) = 0;

  ///
  /// Called when an ImageSource is removed from the provider.
  ///
  /// @param  id  The identifier of the image source.
  ///
  virtual void OnRemoveImageSource(const String& id) = 0;
};

} // namespace ultralight
