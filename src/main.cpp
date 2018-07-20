#include "server.h"
#include "requests.h"
#include "backtrace.h"

#include <misc_log_ex.h>
#include <boost/program_options.hpp>
#include <boost/property_tree/ini_parser.hpp>
// #include <boost/tokenizer.hpp>
#include <boost/filesystem.hpp>
#include <csignal>

namespace po = boost::program_options;
using namespace std;

int main(int argc, const char** argv)
{
    graft::ConfigOpts opts;

    po::options_description desc("Allowed options");
    desc.add_options()
            ("help", "produce help message")
            ("config-file", po::value<string>(), "config filename (config.ini by default)")
            ("log-level", po::value<int>(), "log-level. (3 by default)");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help"))
    {
        std::cout << desc << std::endl;
        return EXIT_SUCCESS;
    }

    opts.config_filename = "./config.ini";
    if (vm.count("config-file"))
        opts.config_filename = vm["config-file"].as<string>();

    opts.log_level = 1;
    if (vm.count("log-level"))
        opts.log_level = vm["log-level"].as<int>();

    graft::GraftServer gserver;

    try
    {
        bool serve = true;
        while (serve)
        {
            bool res = gserver.init(opts);
            if(!res) return EXIT_FAILURE;

            serve = gserver.serve();
        }
    }
    catch (const std::exception & e)
    {
        std::cerr << "Exception thrown: " << e.what() << std::endl;
        return -1;
    }
    catch(...)
    {
        std::cerr << "Exception of unknown type!\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
