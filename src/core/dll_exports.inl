#ifdef CORE_EXPORTS
	#define CORE_API EXPORT
#else
	#define CORE_API IMPORT
#endif

#ifdef ENGINE_EXPORTS
	#define ENGINE_API EXPORT
#else
	#define ENGINE_API IMPORT
#endif

#ifdef UNBLOCK_EXPORTS
	#define UNBLOCK_API EXPORT
#else
	#define UNBLOCK_API IMPORT
#endif
