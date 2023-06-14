#ifndef SIGSERVE_DB020C9E
#define SIGSERVE_DB020C9E
#include "rust/cxx.h"

namespace sigserve_wrapper {

    struct Report;

int init(const char *sdf_path, bool debug);
Report handle_args(int argc, char *argv[]);

}

#endif /* SIGSERVE_DB020C9E */
