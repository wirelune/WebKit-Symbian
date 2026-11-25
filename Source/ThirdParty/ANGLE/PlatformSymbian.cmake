# Symbian^3: only the shader translator, for WebGL on the system's OpenGL ES
# 2.0 (USE_ANGLE_EGL and USE_ANGLE_WEBGL are off in OptionsSymbian.cmake).
#
# On __SYMBIAN32__ the Khronos headers mark functions IMPORT_C, a macro of
# Symbian's e32def.h, which the translator does not include.
list(APPEND ANGLE_DEFINITIONS IMPORT_C=)
