/**************************************************************************************************
 *  This file is a part of Ultralight, an ultra-portable web-browser engine.                      *
 *                                                                                                *
 *  See <https://ultralig.ht> for licensing and more.                                             *
 *                                                                                                *
 *  (C) 2024 Ultralight, Inc.                                                                     *
 **************************************************************************************************/
#pragma once
#include <Ultralight/Defines.h>

namespace ultralight {

///
/// Scroll event representing a change in scroll state.
///
/// @see View::FireScrollEvent
///
class ScrollEvent {
 public:
  ///
  /// The scroll event granularity type
  ///
  enum Type {
    kType_ScrollByPixel, ///< The delta value will be interpreted as number of pixels to scroll.
    kType_ScrollByPage,  ///< The delta value will be interpreted as number of pages to scroll.
  };

  ///
  /// Scroll granularity type
  ///
  Type type;

  ///
  /// Horizontal scroll amount
  ///
  int delta_x;

  ///
  /// Vertical scroll amount
  ///
  int delta_y;
};

} // namespace ultralight
