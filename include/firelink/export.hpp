#ifndef FIRELINK_EXPORT_H
#define FIRELINK_EXPORT_H

#if defined(_WIN32) || defined(__CYGWIN__)
  #ifdef BUILD_FIRELINK
    #define FIRELINK_API __declspec(dllexport)
  #else
    #define FIRELINK_API __declspec(dllimport)
  #endif
  #define FIRELINK_HIDDEN
#elif defined(__GNUC__) || defined(__clang__)
    #define FIRELINK_API __attribute__((visibility("default")))
    #define FIRELINK_HIDDEN __attribute__((visibility("hidden")))
#else
    #define FIRELINK_API
    #define FIRELINK_HIDDEN
#endif

#define FIRELINK_CLASS_API FIRELINK_API

#endif /* FIRELINK_EXPORT_H */
