#ifdef UNBLOCK_STATIC
	#define CORE_API
#else
	#ifdef CORE_EXPORTS
		#define CORE_API EXPORT
	#else
		#define CORE_API IMPORT
	#endif
#endif

#ifdef UNBLOCK_STATIC
	#define ENGINE_API
#else
	#ifdef ENGINE_EXPORTS
		#define ENGINE_API EXPORT
	#else
		#define ENGINE_API IMPORT
	#endif
#endif

#ifdef UNBLOCK_STATIC
	#define UNBLOCK_API
#else
	#ifdef UNBLOCK_EXPORTS
		#define UNBLOCK_API EXPORT
	#else
		#define UNBLOCK_API IMPORT
	#endif
#endif

#ifdef UNBLOCK_STATIC
	#define UI_API
#else
	#ifdef UI_EXPORTS
		#define UI_API EXPORT
	#else
		#define UI_API IMPORT
	#endif
#endif
