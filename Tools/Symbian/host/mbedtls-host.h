/* The end of mbedtls_config.h on the Windows test host (build-net.ps1 appends
 * it to the phone's mbedtls_config.h from the symbian-platform SDK): the
 * phone's own additions, then what Windows has instead of the phone's Symbian
 * glue. Code that includes mbedTLS's headers needs nothing else. */
#include "symbian_mbedtls/config.h"

/* TLS 1.3, which the phone turns on in shims/mbedtls-config.h. */
#define MBEDTLS_SSL_PROTO_TLS1_3

/* Entropy from Windows (BCryptGenRandom) instead of the phone's Math::Random(). */
#undef MBEDTLS_NO_PLATFORM_ENTROPY
#undef MBEDTLS_ENTROPY_HARDWARE_ALT

/* The phone's zeroize comes from its Symbian glue. */
#undef MBEDTLS_PLATFORM_ZEROIZE_ALT
