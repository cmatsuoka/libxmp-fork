#include "test.h"

/* This input caused an out-of-bounds reads in the Fuchs Tracker
 * loader due to a faulty bounds check on the pattern length.
 */

TEST(test_fuzzer_prowizard_fuchs_pattern_length)
{
	xmp_context opaque;
	int ret;

	opaque = xmp_create_context();
	ret = xmp_load_module(opaque, "data/f/prowizard_fuchs_pattern_length.xz");
	fail_unless(ret == -XMP_ERROR_LOAD, "module load");

	xmp_free_context(opaque);
}
END_TEST
