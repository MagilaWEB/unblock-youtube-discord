export module core.macro;

#ifndef NDEBUG
export inline constexpr bool debug = true;
#else
export inline constexpr bool debug = false;
#endif
