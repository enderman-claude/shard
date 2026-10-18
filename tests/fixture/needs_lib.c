/* Links against libm, so the ELF has a DT_NEEDED entry that the loader must reject. */
#include <math.h>
double needs_lib_sin(double x) { return sin(x); }
