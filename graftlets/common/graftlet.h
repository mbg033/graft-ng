#pragma once
#include "CMakeGraftletConfig.h"

#define MK_VER(major,minor) (((unsigned)major) << 8 | ((unsigned)minor))
#define VER_MAJOR(ver) (((unsigned)ver) >> 8)
#define VER_MINOR(ver) (((unsigned)ver) & 0xFF)

namespace graft
{

}//namespace graft
