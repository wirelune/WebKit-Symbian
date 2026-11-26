# x86-windows without the debug builds: the host is built RelWithDebInfo, and
# vcpkg's debug libraries would only double the CI's longest step.
set(VCPKG_TARGET_ARCHITECTURE x86)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_BUILD_TYPE release)
