
#ifndef LIBPORTALQT_EXPORT_H
#define LIBPORTALQT_EXPORT_H

#ifdef LIBPORTALQT_STATIC_DEFINE
#  define LIBPORTALQT_EXPORT
#  define LIBPORTALQT_NO_EXPORT
#else
#  ifndef LIBPORTALQT_EXPORT
#    ifdef LibPortalQt_EXPORTS
        /* We are building this library */
#      define LIBPORTALQT_EXPORT __attribute__((visibility("default")))
#    else
        /* We are using this library */
#      define LIBPORTALQT_EXPORT __attribute__((visibility("default")))
#    endif
#  endif

#  ifndef LIBPORTALQT_NO_EXPORT
#    define LIBPORTALQT_NO_EXPORT __attribute__((visibility("hidden")))
#  endif
#endif

#ifndef LIBPORTALQT_DEPRECATED
#  define LIBPORTALQT_DEPRECATED __attribute__ ((__deprecated__))
#endif

#ifndef LIBPORTALQT_DEPRECATED_EXPORT
#  define LIBPORTALQT_DEPRECATED_EXPORT LIBPORTALQT_EXPORT LIBPORTALQT_DEPRECATED
#endif

#ifndef LIBPORTALQT_DEPRECATED_NO_EXPORT
#  define LIBPORTALQT_DEPRECATED_NO_EXPORT LIBPORTALQT_NO_EXPORT LIBPORTALQT_DEPRECATED
#endif

#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef LIBPORTALQT_NO_DEPRECATED
#    define LIBPORTALQT_NO_DEPRECATED
#  endif
#endif

#endif
