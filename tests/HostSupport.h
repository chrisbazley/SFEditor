/* Host facilities for SFEditor loading tests. */
#ifndef HostSupport_h
#define HostSupport_h
#include <stdbool.h>
#include "../SFError.h"
void host_set_palette(bool bright);
void host_expect_error(SFErrorType type);
void host_check_error(void);
void host_cancel_expected_error(void);
#endif
