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

static SFError read_data(CloudColData *value, unsigned char const *data, size_t size)
{
  Reader reader;
  assert(reader_mem_init(&reader, data, size));
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
    unsigned char data[Clouds_NumColours];
    Writer writer;
    assert(writer_mem_init(&writer, data, sizeof(data)));
    clouds_write(&clouds, &writer);
    assert(writer_destroy(&writer) == sizeof(data));
    assert(data[0] == colour);
    assert(data[1] == 255 - colour);
    assert(!SFError_fail(read_data(&loaded, data, sizeof(data))));
    assert(clouds_get_colour(&loaded, 0) == colour);
    assert(clouds_get_colour(&loaded, 1) == 255 - colour);
  }
}

static void test3(void)
{
  /* Cloud colour truncation */

  unsigned char const data[Clouds_NumColours] = {17, 203};
  for (size_t length = 0; length < sizeof(data); ++length)
  {
    CloudColData clouds;
    assert(read_data(&clouds, data, length).type == SFErrorType_ReadFail);
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
