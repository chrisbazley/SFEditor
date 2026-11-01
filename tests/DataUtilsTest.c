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

static size_t fail_read(void *ptr, size_t size, Reader *reader)
{
  NOT_USED(ptr);
  NOT_USED(size);
  reader->error = true;

  return 0;
}

static void end_read(Reader *reader)
{
  NOT_USED(reader);
}

static void init_error_reader(Reader *reader)
{
  static ReaderFns const fns = {.fread_fn = fail_read, .term_fn = end_read};
  reader_internal_init(reader, &fns, reader);
}

static void test1(void)
{
  /* Empty string line count */

  int width = 0;
  assert(string_lcount("", &width) == 1);
  assert(width == 0);
}

static void test2(void)
{
  /* String line count */

  enum { StringCapacity = 128 };

  for (size_t lines = 0; lines + 1 < StringCapacity; ++lines)
  {
    char string[StringCapacity];
    memset(string, '\n', lines);
    string[lines] = '\0';

    int width = 0;
    assert(string_lcount(string, &width) == (int)lines + 1);
  }
}

static void test3(void)
{
  /* Maximum line width */

  for (size_t width = 0; width < 32; ++width)
  {
    char string[128];
    size_t pos = 0;

    for (size_t line = 0; line < 3; ++line)
    {
      size_t const length = line == 1 ? width : width / 2;
      memset(string + pos, 'A', length);
      pos += length;
      string[pos++] = '\n';
    }

    string[pos] = '\0';

    int maximum = 0;
    string_lcount(string, &maximum);
    assert(maximum == (int)width);
  }
}

static void test4(void)
{
  /* Existing maximum line width */

  int width = 20;
  string_lcount("ABC\nDEFG", &width);
  assert(width == 20);
}

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

static void test7(void)
{
  /* Exact stream length */

  unsigned char const data[] = {1};
  Reader reader;
  assert(reader_mem_init(&reader, data, sizeof(data)));

  assert(reader_fgetc(&reader) == 1);

  assert(check_trunc_or_ext(&reader, SFERROR(OK)).type == SFErrorType_OK);

  reader_destroy(&reader);
}

static void test8(void)
{
  /* Trailing stream data */

  unsigned char const data[] = {1};
  Reader reader;
  assert(reader_mem_init(&reader, data, sizeof(data)));

  assert(check_trunc_or_ext(&reader, SFERROR(OK)).type == SFErrorType_TooLong);

  reader_destroy(&reader);
}

static void test9(void)
{
  /* Truncated stream */

  unsigned char const data[] = {1};
  Reader reader;
  assert(reader_mem_init(&reader, data, sizeof(data)));

  assert(reader_fgetc(&reader) == 1);
  assert(reader_fgetc(&reader) == EOF);

  assert(check_trunc_or_ext(&reader, SFERROR(ReadFail)).type == SFErrorType_Trunc);

  reader_destroy(&reader);
}

static void test10(void)
{
  /* Existing parser error */

  unsigned char const data[] = {1};
  Reader reader;
  assert(reader_mem_init(&reader, data, sizeof(data)));

  assert(check_trunc_or_ext(&reader, SFERROR(BadNumTargetInfo)).type ==
         SFErrorType_BadNumTargetInfo);
  assert(reader_ftell(&reader) == 0);

  reader_destroy(&reader);
}

static void test11(void)
{
  /* Existing stream error */

  Reader reader;
  init_error_reader(&reader);

  assert(reader_fgetc(&reader) == EOF);
  assert(reader_ferror(&reader));

  assert(check_trunc_or_ext(&reader, SFERROR(OK)).type == SFErrorType_ReadFail);

  reader_destroy(&reader);
}

static void test12(void)
{
  /* Final read failure */

  Reader reader;
  init_error_reader(&reader);

  assert(!reader_ferror(&reader));
  assert(!reader_feof(&reader));

  assert(check_trunc_or_ext(&reader, SFERROR(OK)).type == SFErrorType_ReadFail);
  assert(reader_ferror(&reader));
  assert(!reader_feof(&reader));

  reader_destroy(&reader);
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
    { "Empty string line count", test1 },
    { "String line count", test2 },
    { "Maximum line width", test3 },
    { "Existing maximum line width", test4 },
    { "Flex fill values", test5 },
    { "Flex fill preserves trailing bytes", test6 },
#ifdef FORTIFY
    { "Flex fill allocation bounds", test13 },
#endif
    { "Exact stream length", test7 },
    { "Trailing stream data", test8 },
    { "Truncated stream", test9 },
    { "Existing parser error", test10 },
    { "Existing stream error", test11 },
    { "Final read failure", test12 },
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
