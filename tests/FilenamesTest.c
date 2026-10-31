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

static SFError read_data(FilenamesData *value, unsigned char const *data, size_t size)
{
  Reader reader;
  assert(reader_mem_init(&reader, data, size));
  SFError const err = filenames_read(value, &reader);
  reader_destroy(&reader);
  return err;
}

enum { EncodedSize = BytesPerFilename * DataType_FilenamesCount };

static void make_name(char name[BytesPerFilename], DataType type, size_t length)
{
  for (size_t i = 0; i < length; ++i)
  {
    name[i] = (char)('A' + (i + 3 * (size_t)type) % 26);
  }
  name[length] = '\0';
}

static void write_names(FilenamesData const *names, unsigned char data[EncodedSize + 1])
{
  Writer writer;
  assert(writer_mem_init(&writer, data, EncodedSize + 1));
  filenames_write(names, &writer);
  assert(writer_ftell(&writer) == EncodedSize);
  assert(writer_fputc(165, &writer) == 165);
  assert(writer_destroy(&writer) == EncodedSize + 1);
}

static void fill_names(FilenamesData *names, size_t length)
{
  for (DataType type = DataType_First; type < DataType_FilenamesCount; ++type)
  {
    char name[BytesPerFilename];
    make_name(name, type, length);
    filenames_set(names, type, name);
  }
}

static void test1(void)
{
  /* Independent filename slots */

  FilenamesData names = {0};
  for (DataType type = DataType_First; type < DataType_Count; ++type)
  {
    char name[BytesPerFilename];
    make_name(name, type, BytesPerFilename - 1);
    filenames_set(&names, type, name);
    for (DataType other = DataType_First; other < DataType_Count; ++other)
    {
      char expected[BytesPerFilename];
      make_name(expected, other, other <= type ? BytesPerFilename - 1 : 0);
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
    make_name(expected, type, BytesPerFilename - 1);
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
    unsigned char data[EncodedSize + 1];
    write_names(&names, data);
    Reader reader;
    assert(reader_mem_init(&reader, data, sizeof(data)));
    assert(!SFError_fail(filenames_read(&loaded, &reader)));
    assert(reader_ftell(&reader) == EncodedSize);
    assert(reader_fgetc(&reader) == 165);
    reader_destroy(&reader);
    for (DataType type = DataType_First; type < DataType_FilenamesCount; ++type)
    {
      char expected[BytesPerFilename];
      make_name(expected, type, length);
      assert(strcmp(filenames_get(&loaded, type), expected) == 0);
    }
  }
}

static void test4(void)
{
  /* Filename truncation */

  FilenamesData names = {0};
  fill_names(&names, BytesPerFilename - 1);
  unsigned char data[EncodedSize + 1];
  write_names(&names, data);
  for (size_t length = 0; length < EncodedSize; ++length)
  {
    FilenamesData loaded = {0};
    assert(read_data(&loaded, data, length).type == SFErrorType_ReadFail);
  }
}

static void test5(void)
{
  /* Missing filename terminator */

  FilenamesData names = {0}, loaded = {0};
  fill_names(&names, BytesPerFilename - 1);
  unsigned char data[EncodedSize + 1];
  write_names(&names, data);
  /* Scrambling is XOR-based: change the decoded CR terminator to 12. */
  data[BytesPerFilename - 1] ^= 1;
  assert(read_data(&loaded, data, EncodedSize).type == SFErrorType_FilenameTooLong);
}

static void test6(void)
{
  /* Zero filename terminator */

  FilenamesData names = {0}, loaded = {0};
  fill_names(&names, BytesPerFilename - 1);
  unsigned char data[EncodedSize + 1];
  write_names(&names, data);
  /* Change the decoded CR to NUL without duplicating the scrambler. */
  data[BytesPerFilename - 1] ^= '\r';
  assert(!SFError_fail(read_data(&loaded, data, EncodedSize)));
  char expected[BytesPerFilename];
  make_name(expected, DataType_First, BytesPerFilename - 1);
  assert(strcmp(filenames_get(&loaded, DataType_First), expected) == 0);
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
    { "Filename truncation", test4 },
    { "Missing filename terminator", test5 },
    { "Zero filename terminator", test6 },
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
