/*
 *  One libc symbol that psd_sdk references and solver_minimal_1 does not
 *  export. A side module whose imports are not all resolved never instantiates
 *  at all - the filter simply never registers - so it has to be defined here
 *  (the approach libape and libjxr already take).
 *
 *  psd_sdk reaches it through PSD_ASSERT, which its release build still emits.
 */

#include <cstdio>
#include <cstdlib>

extern "C" void __assert_fail(const char *expr, const char *file, unsigned int line, const char *func)
{
	std::fprintf(stderr, "[PSDDec] assertion failed: %s at %s:%u in %s\n",
	             expr ? expr : "?", file ? file : "?", line, func ? func : "?");
	std::abort();
}
