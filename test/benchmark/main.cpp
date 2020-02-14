
#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>

#include "params.hpp"


benchmark_params global_benchmark_params;


int main(int argc, char* argv[])
{
    Catch::Session session;

    auto cli
        = session.cli()
        | Catch::clara::Opt(global_benchmark_params.num_threads, "n")
        ["--num-threads"]
        ("number of threads to use in thread_squad benchmarks");

    session.cli(cli);

    int returnCode = session.applyCommandLine( argc, argv );
    if (returnCode != 0) return returnCode;

    return session.run();
}
