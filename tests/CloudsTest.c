/* SFEditor test: cloud colours.
 * Copyright (C) 2026 Christopher Bazley
 */
#undef NDEBUG

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "Macros.h"
#include "Debug.h"
#include "Reader.h"
#include "ReaderMem.h"
#include "Writer.h"
#include "WriterMem.h"
#include "Tests.h"
#include "../SFError.h"
#include "../Clouds.h"
#include "../CloudsData.h"

enum { Capacity = 64 };

static size_t write_data(CloudColData const *value, unsigned char (*buffer)[Capacity])
{
  Writer writer;
  assert(writer_mem_init(&writer, *buffer, sizeof(*buffer)));
  clouds_write(value, &writer);
  assert(!writer_ferror(&writer));
  long const length = writer_ftell(&writer);
  assert(length > 0);
  assert((size_t)length < sizeof(*buffer));
  /* Materialise any space skipped at the end, without assuming its size. */
  assert(writer_fputc(0, &writer) == 0);
  assert(writer_destroy(&writer) == length + 1);
  return (size_t)length;
}

static SFError read_data(CloudColData *value,
  unsigned char C23_CONST (*buffer)[Capacity], size_t length)
{
  assert(length <= sizeof(*buffer));
  Reader reader;
  assert(reader_mem_init(&reader, *buffer, length));
  SFError const err = clouds_read(value, &reader);
  reader_destroy(&reader);
  return err;
}

static void test1(void)
{
  /* Independent cloud colours */

  for (size_t index = 0; index < Clouds_NumColours; ++index)
  {
    for (int colour = 0; colour < 256; ++colour)
    {
      CloudColData clouds = {.colours = {17, 203}};
      clouds_set_colour(&clouds, index, colour);
      for (size_t other = 0; other < Clouds_NumColours; ++other)
      {
        int const expected = other == index ? colour : (other == 0 ? 17 : 203);
        assert(clouds_get_colour(&clouds, other) == expected);
      }
    }
  }
}

static void test2(void)
{
  /* Cloud colour round trip */

  for (int colour = 0; colour < 256; ++colour)
  {
    CloudColData clouds = {.colours = {0, 0}}, loaded;
    clouds_set_colour(&clouds, 0, colour);
    clouds_set_colour(&clouds, 1, 255 - colour);
    unsigned char buffer[Capacity];
    size_t const size = write_data(&clouds, &buffer);
    assert(!SFError_fail(read_data(&loaded, &buffer, size)));
    assert(clouds_get_colour(&loaded, 0) == colour);
    assert(clouds_get_colour(&loaded, 1) == 255 - colour);
  }
}

static void test3(void)
{
  /* Cloud colour truncation */

  CloudColData clouds = {.colours = {17, 203}};
  unsigned char buffer[Capacity];
  size_t const size = write_data(&clouds, &buffer);
  for (size_t length = 0; length < size; ++length)
  {
    CloudColData loaded;
    SFError const err = read_data(&loaded, &buffer, length);
    if (length == 0)
    {
      assert(SFError_fail(err));
    }
    if (!SFError_fail(err))
    {
      assert(clouds_get_colour(&loaded, 0) == 17);
      assert(clouds_get_colour(&loaded, 1) == 203);
    }
  }
}

void Clouds_tests(void)
{
  /* Clouds */
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Independent cloud colours", test1 },
    { "Cloud colour round trip", test2 },
    { "Cloud colour truncation", test3 },
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
