// Pour MSVC, qui ne connaît ni __inline__ ni __builtin_expect de GCC : imposé à la
// compilation de city-c.c, que l'on garde tel qu'il est venu. Écrit pour Trait64.
#pragma once
#define __inline__ __inline
#define __builtin_expect(x, expected) (x)
