#include "server.h"

#include <boost/config.hpp> // for BOOST_SYMBOL_EXPORT
#include <boost/dll/import.hpp> // for import_alias

void test_graftlet(int argc, const char** argv)
{
    boost::filesystem::path lib_dir(argv[0]);
    lib_dir.remove_filename();

    auto my_function_x = boost::dll::import<void()>(
                lib_dir / "graftlets/libmy_graftlet", "my_function_x",
                boost::dll::load_mode::append_decorations
                );
    my_function_x();

    auto my_function = boost::dll::import_alias<void()>(
                lib_dir / "graftlets/libmy_graftlet", "my_function",
                boost::dll::load_mode::append_decorations
                );
    my_function();

    try
    {//invalid test
        auto my_bla = boost::dll::import<void(std::string&)>(
                    lib_dir / "graftlets/libmy_graftlet", "my_function_x",
                    boost::dll::load_mode::append_decorations
                    );
        std::string s("sss");
        my_bla(s);
    }
    catch(std::exception& ex)
    {
        std::cout << "\n exception:" << ex.what();
    }

    try
    {
        auto my_bla = boost::dll::import_alias<void(std::string&)>(
                    lib_dir / "graftlets/libmy_graftlet", "xxx",
                    boost::dll::load_mode::append_decorations
                    );
    }
    catch(std::exception& ex)
    {
        std::cout << "\n exception:" << ex.what();
    }
}

int main(int argc, const char** argv)
{
    test_graftlet(argc, argv);
    return 0;

    try
    {
        graft::GraftServer gserver;
        bool res = gserver.init(argc, argv);
        if(!res) return -1;
        gserver.serve();
    } catch (const std::exception & e) {
        std::cerr << "Exception thrown: " << e.what() << std::endl;
        return -1;
    }
    catch(...) {
        std::cerr << "Exception of unknown type!\n";
        return -1;
    }

    return 0;
}
