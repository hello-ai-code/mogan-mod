/******************************************************************************
 * md4c_bridge.cpp
 * Bridge to compile md4c.c as C inside a C++ translation unit.
 * This avoids C/C++ incompatibilities (void* casts for malloc/realloc).
 ******************************************************************************/

extern "C" {
    /* Disable asserts to keep md4c lean */
    #define MD4C_USE_ASSERT 0
    #include "md4c.c"
}
