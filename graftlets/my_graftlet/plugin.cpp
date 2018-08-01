#include "graftlet.h"

#include <iostream>
//#include "CMakeGraftletConfig.h"
//#include <boost/config.hpp> // for BOOST_SYMBOL_EXPORT
#include <boost/dll.hpp>
#include <boost/dll/alias.hpp> // for BOOST_DLL_ALIAS

#define API extern "C" BOOST_SYMBOL_EXPORT

namespace graft
{

API void my_function_x() noexcept
{
    std::cout << "\nthe version " << PROJECT_VERSION_MAJOR << '.' << PROJECT_VERSION_MINOR << "\n";
}

} //namespace graft

BOOST_DLL_ALIAS(graft::my_function_x, my_function);
