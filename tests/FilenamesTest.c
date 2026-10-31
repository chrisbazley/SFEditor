/* SFEditor test: mission filenames.
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
#include "../Filenames.h"
#include "../FilenamesData.h"

enum { Capacity = 256 };

static SFError read_data(FilenamesData *value,
  unsigned char C23_CONST (*buffer)[Capacity], size_t length)
{
  assert(length <= sizeof(*buffer));
  Reader reader;
  assert(reader_mem_init(&reader, *buffer, length));
  SFError const err = filenames_read(value, &reader);
  reader_destroy(&reader);
  return err;
}

static void make_name(char (*name)[BytesPerFilename], DataType type, size_t length)
{
  for (size_t i = 0; i < length; ++i)
  {
    (*name)[i] = (char)('A' + (i + 3 * (size_t)type) % 26);
  }
  (*name)[length] = '\0';
}

static size_t write_names(FilenamesData const *value, unsigned char (*buffer)[Capacity])
{
  Writer writer;
  assert(writer_mem_init(&writer, *buffer, sizeof(*buffer)));
  filenames_write(value, &writer);
  assert(!writer_ferror(&writer));
  long const length = writer_ftell(&writer);
  assert(length > 0);
  assert((size_t)length < sizeof(*buffer));
  /* Materialise any space skipped at the end, without assuming its size. */
  assert(writer_fputc(0, &writer) == 0);
  assert(writer_destroy(&writer) == length + 1);
  return (size_t)length;
}

static void fill_names(FilenamesData *names, size_t length)
{
  for (DataType type = DataType_First; type < DataType_FilenamesCount; ++type)
  {
    char name[BytesPerFilename];
    make_name(&name, type, length);
    filenames_set(names, type, name);
  }
}

static SFErrorType const expected_errors[] = {
  SFErrorType_FilenameTooLong,
};

enum { ErrorCount = ARRAY_SIZE(expected_errors) };

static void note_error(SFErrorType error, bool (*seen)[ErrorCount])
{
  if (error == SFErrorType_OK)
  {
    return;
  }
  for (size_t i = 0; i < ErrorCount; ++i)
  {
    if (error == expected_errors[i])
    {
      (*seen)[i] = true;
      return;
    }
  }
  fprintf(stderr, "Unexpected read error %d\n", (int)error);
  assert(false);
}

static void test1(void)
{
  /* Independent filename slots */

  FilenamesData names = {0};
  for (DataType type = DataType_First; type < DataType_Count; ++type)
  {
    char name[BytesPerFilename];
    make_name(&name, type, BytesPerFilename - 1);
    filenames_set(&names, type, name);
    for (DataType other = DataType_First; other < DataType_Count; ++other)
    {
      char expected[BytesPerFilename];
      make_name(&expected, other, other <= type ? BytesPerFilename - 1 : 0);
      assert(strcmp(filenames_get(&names, other), expected) == 0);
    }
  }
}

static void test2(void)
{
  /* Filename replacement */

  FilenamesData names = {0};
  fill_names(&names, BytesPerFilename - 1);
  filenames_set(&names, DataType_BaseMap, "A");
  assert(strcmp(filenames_get(&names, DataType_BaseMap), "A") == 0);
  filenames_set(&names, DataType_BaseMap, "");
  assert(strcmp(filenames_get(&names, DataType_BaseMap), "") == 0);
  for (DataType type = DataType_OverlayMap; type < DataType_FilenamesCount; ++type)
  {
    char expected[BytesPerFilename];
    make_name(&expected, type, BytesPerFilename - 1);
    assert(strcmp(filenames_get(&names, type), expected) == 0);
  }
}

static void test3(void)
{
  /* Filename round trip */

  for (size_t length = 0; length < BytesPerFilename; ++length)
  {
    FilenamesData names = {0}, loaded = {0};
    fill_names(&names, length);
    unsigned char buffer[Capacity];
    size_t const size = write_names(&names, &buffer);
    assert(!SFError_fail(read_data(&loaded, &buffer, size)));
    for (DataType type = DataType_First; type < DataType_FilenamesCount; ++type)
    {
      char expected[BytesPerFilename];
      make_name(&expected, type, length);
      assert(strcmp(filenames_get(&loaded, type), expected) == 0);
    }
  }
}

static void test4(void)
{
  /* Corrupt filename data */

  FilenamesData names = {0};
  fill_names(&names, BytesPerFilename - 1);
  unsigned char buffer[Capacity];
  size_t const size = write_names(&names, &buffer);
  bool seen[ErrorCount] = {false};
  for (size_t offset = 0; offset < size; ++offset)
  {
    unsigned char const saved = buffer[offset];
    for (unsigned byte = 0; byte < 256; ++byte)
    {
      buffer[offset] = (unsigned char)byte;
      FilenamesData loaded = {0};
      note_error(read_data(&loaded, &buffer, size).type, &seen);
    }
    buffer[offset] = saved;
  }
  for (size_t i = 0; i < ErrorCount; ++i)
  {
    assert(seen[i]);
  }
}

static void test5(void)
{
  /* Filename truncation */

  FilenamesData names = {0};
  fill_names(&names, BytesPerFilename - 1);
  unsigned char buffer[Capacity];
  size_t const size = write_names(&names, &buffer);
  for (size_t length = 0; length < size; ++length)
  {
    FilenamesData loaded = {0};
    SFError const err = read_data(&loaded, &buffer, length);
    if (length == 0)
    {
      assert(SFError_fail(err));
    }
    if (!SFError_fail(err))
    {
      for (DataType type = DataType_First; type < DataType_FilenamesCount; ++type)
      {
        char expected[BytesPerFilename];
        make_name(&expected, type, BytesPerFilename - 1);
        assert(strcmp(filenames_get(&loaded, type), expected) == 0);
      }
    }
  }
}

static void test6(void)
{
  /* Eight-bit filename round trip */

  FilenamesData names = {0}, loaded = {0};
  char const name[] = "\x80\xff";
  filenames_set(&names, DataType_BaseMap, name);
  unsigned char buffer[Capacity];
  size_t const size = write_names(&names, &buffer);
  assert(!SFError_fail(read_data(&loaded, &buffer, size)));
  assert(strcmp(filenames_get(&loaded, DataType_BaseMap), name) == 0);
}

void Filenames_tests(void)
{
  /* Filenames */
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Independent filename slots", test1 },
    { "Filename replacement", test2 },
    { "Filename round trip", test3 },
    { "Corrupt filename data", test4 },
    { "Filename truncation", test5 },
    { "Eight-bit filename round trip", test6 },
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
