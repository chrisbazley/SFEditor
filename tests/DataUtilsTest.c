/* SFEditor test: data utilities.
 * Copyright (C) 2026 Christopher Bazley
 */
#undef NDEBUG

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "Macros.h"
#include "Debug.h"
#include "Reader.h"
#include "ReaderMem.h"
#include "Tests.h"
#include "../SFError.h"
#include "../flex.h"
#include "../Utils.h"

static void test5(void)
{
  /* Flex fill values */

  void *block;
  assert(flex_alloc(&block, 128));

  for (unsigned colour = 0; colour <= UCHAR_MAX; ++colour)
  {
    for (size_t length = 0; length <= 64; ++length)
    {
      for (size_t i = 0; i < 128; ++i)
      {
        ((unsigned char *)block)[i] = (unsigned char)(colour + i + 1);
      }

      assert(memset_flex(&block, (unsigned char)colour, length) == &block);

      unsigned char const *const bytes = block;

      for (size_t i = 0; i < length; ++i)
      {
        assert(bytes[i] == (unsigned char)colour);
      }
    }
  }

  flex_free(&block);
}

static void test6(void)
{
  /* Flex fill preserves trailing bytes */

  void *block;
  assert(flex_alloc(&block, 128));

  for (size_t length = 0; length <= 64; ++length)
  {
    for (size_t i = 0; i < 128; ++i)
    {
      ((unsigned char *)block)[i] = (unsigned char)(i + 1);
    }

    memset_flex(&block, 0, length);

    unsigned char const *const bytes = block;

    for (size_t i = length; i < 128; ++i)
    {
      assert(bytes[i] == (unsigned char)(i + 1));
    }
  }

  flex_free(&block);
}

#ifdef FORTIFY
static void test13(void)
{
  /* Flex fill allocation bounds */

  for (size_t length = 0; length <= 64; ++length)
  {
    int const size = length > 0 ? (int)length : 1;
    void *block;
    assert(flex_alloc(&block, size));

    for (size_t i = 0; i < (size_t)size; ++i)
    {
      ((unsigned char *)block)[i] = (unsigned char)(i + 1);
    }

    memset_flex(&block, 0, length);

    /* Check Fortify's guards before and after the allocated block. */
    assert(!Fortify_CheckAllMemory());

    flex_free(&block);
  }
}

#endif

void DataUtils_tests(void)
{
  /* DataUtils */
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Flex fill values", test5 },
    { "Flex fill preserves trailing bytes", test6 },
#ifdef FORTIFY
    { "Flex fill allocation bounds", test13 },
#endif
  };

  for (size_t count = 0; count < ARRAY_SIZE(unit_tests); ++count)
  {
    DEBUGF("Test %zu/%zu : %s\n", 1 + count,
           ARRAY_SIZE(unit_tests), unit_tests[count].test_name);
    fflush(stdout);

    Fortify_EnterScope();
    unit_tests[count].test_func();
    Fortify_LeaveScope();
  }
}
