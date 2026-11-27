# The Symbian port's Windows test host: the phone's PAL platform files. They are
# the generic ones - ClockGeneric, Sound (which is where PAL::systemBeep lives),
# the plain KillRing and the mbedTLS CryptoDigest - so the host takes them
# unchanged. Without this file the host got no PAL_SOURCES for its platform at
# all, and WebCore.lib was linked without CryptoDigest, KillRing, Clock and
# systemBeep.
include(${CMAKE_CURRENT_LIST_DIR}/PlatformSymbian.cmake)
