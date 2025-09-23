/**************************************************************************************************
 *  This file is a part of Ultralight.                                                            *
 *                                                                                                *
 *  See <https://ultralig.ht> for licensing and more.                                             *
 *                                                                                                *
 *  (C) 2024 Ultralight, Inc.                                                                     *
 **************************************************************************************************/
///
/// @file StringSTL.h
///
/// STL compatibility header for ultralight::String.
///
/// `#include <Ultralight/StringSTL.h>`
///
/// This optional header provides utility functions for converting between ultralight::String,
/// std::string, and std::string_view. It also provides support for using ultralight::String
/// with standard library containers and stream operators.
///
/// @pre This header requires C++17 or later.
///
/// ## Example
///
/// ```cpp
/// #include <Ultralight/StringSTL.h>
/// #include <string>
/// #include <iostream>
///
/// ultralight::String myStr("Hello, world!");
///
/// // Convert ultralight::String to std::string
/// std::string stdStr = ultralight::Convert(myStr);
///
/// // Convert std::string to ultralight::String
/// ultralight::String backToUl = ultralight::Convert(stdStr);
///
/// // Print ultralight::String to std::cout
/// std::cout << myStr << std::endl;
/// ```
///
#pragma once
#include <Ultralight/String.h>
#include <string>
#include <string_view>
#include <type_traits>

namespace ultralight {

///
/// Trait to check if a type is a supported string-like type.
///
template <typename T>
struct is_string_type : std::false_type {};

template <> struct is_string_type<String> : std::true_type {};
template <> struct is_string_type<std::string> : std::true_type {};
template <> struct is_string_type<std::string_view> : std::true_type {};
template <> struct is_string_type<const char*> : std::true_type {};

///
/// Convert between string types.
///
/// This function provides efficient conversion between different string types.
/// It supports ultralight::String, std::string, std::string_view, and const char*.
///
/// The following type conversions are automatically deduced (no template argument needed):
/// - ultralight::String -> std::string
/// - std::string -> ultralight::String
/// - std::string_view -> ultralight::String
/// - const char* -> ultralight::String
///
/// For explicit conversion to std::string_view, use Convert<std::string_view>().
///
/// @tparam To The target string type (optional, deduced in common cases)
/// @tparam From The source string type (optional)
///
/// @param from The string to convert
///
/// @return The converted string in the target type
///
/// ## Example
///
/// ```cpp
/// ultralight::String myStr("Hello, world!");
///
/// // ultralight::String -> std::string
/// std::string stdStr = ultralight::Convert(myStr);
///
/// // ultralight::String -> std::string_view
/// std::string_view svStr = ultralight::Convert<std::string_view>(myStr);
///
/// // std::string -> ultralight::String
/// ultralight::String backToUl = ultralight::Convert(stdStr);
///
/// // std::string_view -> ultralight::String
/// ultralight::String fromView = ultralight::Convert(std::string_view("View"));
/// ```
///
template <typename To = void, typename From>
auto Convert(const From& from) {
    static_assert(is_string_type<std::remove_cv_t<std::remove_reference_t<From>>>::value,
                  "Convert only supports String, std::string, std::string_view, and const char*");

    if constexpr (std::is_same_v<To, void>) {
        // Automatic deduction
        if constexpr (std::is_same_v<From, String>) {
            return std::string(from.utf8().data(), from.utf8().length());
        } else if constexpr (std::is_same_v<From, std::string> || 
                             std::is_same_v<From, std::string_view>) {
            return String(from.data(), from.length());
        } else if constexpr (std::is_same_v<From, const char*>) {
            return String(from);  // String constructor handles null-termination
        } else {
            // This case should never be reached due to the static_assert
            return From{};
        }
    } else {
        // Explicit conversion
        static_assert(is_string_type<To>::value,
                      "Convert only supports String, std::string, std::string_view, and const char*");

        if constexpr (std::is_same_v<To, From>) {
            return from;
        } else if constexpr (std::is_same_v<To, String>) {
            if constexpr (std::is_same_v<From, const char*>) {
                return String(from);  // String constructor handles null-termination
            } else {
                return String(from.data(), from.length());
            }
        } else if constexpr (std::is_same_v<To, std::string>) {
            if constexpr (std::is_same_v<From, String>) {
                return std::string(from.utf8().data(), from.utf8().length());
            } else if constexpr (std::is_same_v<From, const char*>) {
                return std::string(from);  // std::string constructor handles null-termination
            } else {
                return std::string(from);
            }
        } else if constexpr (std::is_same_v<To, std::string_view>) {
            if constexpr (std::is_same_v<From, String>) {
                return std::string_view(from.utf8().data(), from.utf8().length());
            } else if constexpr (std::is_same_v<From, const char*>) {
                return std::string_view(from);  // std::string_view constructor handles null-termination
            } else {
                return std::string_view(from);
            }
        } else if constexpr (std::is_same_v<To, const char*>) {
            static_assert(!std::is_same_v<To, const char*>, 
                          "Direct conversion to const char* is not supported due to ownership issues. "
                          "Convert to String, std::string, or std::string_view instead.");
        } else {
            // This case should never be reached due to the static_assert
            return To{};
        }
    }
}

} // namespace ultralight

namespace std {

///
/// Hash specialization for ultralight::String
///
template<>
struct hash<ultralight::String> {
    size_t operator()(const ultralight::String& str) const {
        return str.Hash();
    }
};

} // namespace std

///
/// Stream output operator for ultralight::String.
///
/// @param os The output stream.
//
/// @param str The string to output.
///
/// @return The output stream.
///
inline std::ostream& operator<<(std::ostream& os, const ultralight::String& str) {
    return os << ultralight::Convert<std::string_view>(str);
}

///
/// Stream input operator for ultralight::String.
///
/// @param is The input stream.
///
/// @param str The string to input into.
///
/// @return The input stream.
///
inline std::istream& operator>>(std::istream& is, ultralight::String& str) {
    std::string temp;
    is >> temp;
    str = ultralight::Convert<ultralight::String>(temp);
    return is;
}