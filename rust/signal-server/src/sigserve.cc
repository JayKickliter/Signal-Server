#include "sigserve.h"

extern int handle_args(int argc, char *argv[]);

namespace sigserve_wrapper {

int entry_point(int argc, char *argv[]) { return ::handle_args(argc, argv); }

}  // namespace sigserve_wrapper
