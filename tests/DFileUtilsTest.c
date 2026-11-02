/* SFEditor test: compressed-file wrappers.
 * Copyright (C) 2026 Christopher Bazley
 */
#undef NDEBUG

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "Macros.h"
#include "Debug.h"
#include "FOpenCount.h"
#include "Reader.h"
#include "ReaderMem.h"
#include "Writer.h"
#include "WriterMem.h"
#include "Tests.h"
#include "HostSupport.h"
#include "../DFileData.h"
#include "../DFileUtils.h"

typedef struct
{
  DFile file;
  unsigned char bytes[64];
  size_t length;
  long minimum;
  SFError result;
  unsigned reads, writes;
} TestFile;

static SFError read_cb(DFile const *file, Reader *reader)
{
  TestFile *const ctx = CONTAINER_OF(file, TestFile, file);
  ++ctx->reads;
  if (reader_fread(ctx->bytes, 1, ctx->length, reader) != ctx->length)
  {
    return SFERROR(ReadFail);
  }
  return ctx->result;
}

static void write_cb(DFile const *file, Writer *writer)
{
  TestFile *const ctx = CONTAINER_OF(file, TestFile, file);
  ++ctx->writes;
  writer_fwrite(ctx->bytes, 1, ctx->length, writer);
}

static long minimum_cb(DFile const *file)
{
  TestFile const *const ctx = CONTAINER_OF(file, TestFile, file);
  return ctx->minimum;
}

static void init(TestFile *ctx, size_t length)
{
  assert(length <= sizeof(ctx->bytes));
  *ctx = (TestFile){.length = length, .minimum = (long)length,
    .result = SFERROR(OK)};
  dfile_init(&ctx->file, read_cb, write_cb, minimum_cb, NULL);
  for (size_t i = 0; i < length; ++i)
  {
    ctx->bytes[i] = (unsigned char)((i * 37) + (i / 3));
  }
}

static long encode(TestFile *ctx, unsigned char (*buffer)[128])
{
  Writer writer;
  assert(writer_mem_init(&writer, *buffer, sizeof(*buffer)));
  assert(!SFError_fail(write_compressed(&ctx->file, &writer)));
  assert(!writer_ferror(&writer));
  long const length = writer_ftell(&writer);
  assert(length >= 0);
  assert(writer_destroy(&writer) == length);
  return length;
}

static SFError decode(TestFile *ctx, unsigned char C23_CONST (*buffer)[128], size_t length)
{
  Reader reader;
  assert(reader_mem_init(&reader, *buffer, length));
  SFError const err = read_compressed(&ctx->file, &reader);
  reader_destroy(&reader);
  return err;
}

static void check_bytes(TestFile const *actual, TestFile const *expected)
{
  assert(actual->length == expected->length);
  for (size_t i = 0; i < actual->length; ++i)
  {
    assert(actual->bytes[i] == expected->bytes[i]);
  }
}

/* Refuse to overwrite an existing file in the test working directory. */
static void check_unused(char const *name)
{
  _Optional FILE *const file = fopen(name, "rb");
  assert(!file);
}

static void test1(void)
{
  /* Compressed stream round trip */

  static size_t const lengths[] = {0, 1, 7, 8, 9, 31, 63, 64};
  for (size_t i = 0; i < ARRAY_SIZE(lengths); ++i)
  {
    TestFile source, loaded;
    init(&source, lengths[i]);
    init(&loaded, lengths[i]);
    memset(loaded.bytes, 0, sizeof(loaded.bytes));
    unsigned char buffer[128];
    long const length = encode(&source, &buffer);

    assert(!SFError_fail(decode(&loaded, &buffer, length)));

    check_bytes(&loaded, &source);
    assert(source.writes == 1);
    assert(loaded.reads == 1);
    dfile_destroy(&loaded.file);
    dfile_destroy(&source.file);
  }
}

static void test2(void)
{
  /* Compressed output honours minimum size */

  TestFile source, loaded;
  init(&source, 17);
  source.minimum = 64;
  init(&loaded, 64);
  unsigned char buffer[128];
  long const length = encode(&source, &buffer);

  assert(!SFError_fail(decode(&loaded, &buffer, length)));

  for (size_t i = 0; i < loaded.length; ++i)
  {
    assert(loaded.bytes[i] == (i < source.length ? source.bytes[i] : 0));
  }
  dfile_destroy(&loaded.file);
  dfile_destroy(&source.file);
}

static void test3(void)
{
  /* Compressed size matches output */

  TestFile source;
  init(&source, 31);
  source.minimum = 64;
  unsigned char buffer[128];
  long const length = encode(&source, &buffer);

  assert(get_compressed_size(&source.file) == length);

  dfile_destroy(&source.file);
}

static void test4(void)
{
  /* Worst compressed size bounds output */

  for (size_t length = 0; length <= 64; ++length)
  {
    TestFile source;
    init(&source, length);
    unsigned char buffer[128];
    long const actual = encode(&source, &buffer);

    assert(worst_compressed_size(&source.file) >= actual);

    dfile_destroy(&source.file);
  }
}

static void test5(void)
{
  /* Compressed read propagates file error */

  TestFile source, loaded;
  init(&source, 64);
  init(&loaded, 64);
  loaded.result = SFERROR(BadNumAnims);
  unsigned char buffer[128];
  long const length = encode(&source, &buffer);

  assert(decode(&loaded, &buffer, length).type == SFErrorType_BadNumAnims);

  assert(loaded.reads == 1);
  dfile_destroy(&loaded.file);
  dfile_destroy(&source.file);
}

static void test6(void)
{
  /* Empty compressed input fails */

  TestFile loaded;
  init(&loaded, 64);
  unsigned char buffer[128] = {0};

  assert(decode(&loaded, &buffer, 0).type == SFErrorType_ReadFail);

  dfile_destroy(&loaded.file);
}

static void test7(void)
{
  /* Compressed write propagates output failure */

  TestFile source;
  init(&source, 64);
  unsigned char buffer[1];
  Writer writer;
  assert(writer_mem_init(&writer, buffer, sizeof(buffer)));

  assert(write_compressed(&source.file, &writer).type == SFErrorType_WriteFail);

  assert(writer_ferror(&writer));
  writer_destroy(&writer);
  dfile_destroy(&source.file);
}

static void test8(void)
{
  /* Compressed write leaves parent open */

  TestFile source;
  init(&source, 64);
  unsigned char buffer[128];
  Writer writer;
  assert(writer_mem_init(&writer, buffer, sizeof(buffer)));

  assert(!SFError_fail(write_compressed(&source.file, &writer)));

  assert(writer_fputc(42, &writer) == 42);
  assert(!writer_ferror(&writer));
  writer_destroy(&writer);
  dfile_destroy(&source.file);
}

static void test9(void)
{
  /* Compressed read leaves parent open */

  TestFile source, loaded;
  init(&source, 64);
  init(&loaded, 64);
  unsigned char buffer[128];
  long const length = encode(&source, &buffer);
  Reader reader;
  assert(reader_mem_init(&reader, buffer, length));

  assert(!SFError_fail(read_compressed(&loaded.file, &reader)));

  assert(reader_fseek(&reader, 0, SEEK_SET) == 0);
  assert(reader_ftell(&reader) == 0);
  reader_destroy(&reader);
  dfile_destroy(&loaded.file);
  dfile_destroy(&source.file);
}

static void test10(void)
{
  /* Compressed file round trip */

  char name[] = "SFEditor-DFileUtilsTest";
  check_unused(name);
  TestFile source, loaded;
  init(&source, 64);
  init(&loaded, 64);
  memset(loaded.bytes, 0, sizeof(loaded.bytes));
  unsigned const open_files = fopen_num();

  assert(!SFError_fail(save_compressed(&source.file, name)));
  assert(!SFError_fail(load_compressed(&loaded.file, name)));

  check_bytes(&loaded, &source);
  assert(fopen_num() == open_files);
  assert(remove(name) == 0);
  dfile_destroy(&loaded.file);
  dfile_destroy(&source.file);
}

static void test11(void)
{
  /* Compressed file read propagates file error */

  char name[] = "SFEditor-DFileUtilsTest";
  check_unused(name);
  TestFile source, loaded;
  init(&source, 64);
  init(&loaded, 64);
  assert(!SFError_fail(save_compressed(&source.file, name)));
  loaded.result = SFERROR(BadNumAnims);
  unsigned const open_files = fopen_num();

  assert(load_compressed(&loaded.file, name).type == SFErrorType_BadNumAnims);

  assert(fopen_num() == open_files);
  assert(remove(name) == 0);
  dfile_destroy(&loaded.file);
  dfile_destroy(&source.file);
}

static void test12(void)
{
  /* Compressed file open input failure */

  TestFile loaded;
  init(&loaded, 64);
  unsigned const open_files = fopen_num();

  assert(load_compressed(&loaded.file, "").type == SFErrorType_OpenInFail);

  assert(loaded.reads == 0);
  assert(fopen_num() == open_files);
  dfile_destroy(&loaded.file);
}

static void test13(void)
{
  /* Compressed file open output failure */

  TestFile source;
  init(&source, 64);
  char name[] = "";
  unsigned const open_files = fopen_num();

  assert(save_compressed(&source.file, name).type == SFErrorType_OpenOutFail);

  assert(source.writes == 0);
  assert(fopen_num() == open_files);
  dfile_destroy(&source.file);
}

static void test14(void)
{
  /* Unnamed file has empty leaf name */

  TestFile source;
  init(&source, 0);

  assert(strcmp(get_leaf_name(&source.file), "") == 0);

  dfile_destroy(&source.file);
}

static void test15(void)
{
  /* Saved file leaf name */

  TestFile source;
  init(&source, 0);
  int date[2] = {0, 0};
  assert(dfile_set_saved(&source.file, "ADFS::4.$.Missions.E.E_01", date));

  assert(strcmp(get_leaf_name(&source.file), "E_01") == 0);

  dfile_destroy(&source.file);
}

#ifdef FORTIFY
static void test16(void)
{
  /* Compressed read allocation failure */

  TestFile loaded;
  init(&loaded, 64);
  unsigned char buffer[128] = {0};
  Reader reader;
  assert(reader_mem_init(&reader, buffer, sizeof(buffer)));
  Fortify_SetNumAllocationsLimit(0);

  SFError const err = read_compressed(&loaded.file, &reader);

  Fortify_SetNumAllocationsLimit(ULONG_MAX);
  assert(err.type == SFErrorType_NoMem);
  assert(loaded.reads == 0);
  reader_destroy(&reader);
  dfile_destroy(&loaded.file);
}

#endif

#ifdef FORTIFY
static void test17(void)
{
  /* Compressed write allocation failure */

  TestFile source;
  init(&source, 64);
  unsigned char buffer[128];
  Writer writer;
  assert(writer_mem_init(&writer, buffer, sizeof(buffer)));
  Fortify_SetNumAllocationsLimit(0);

  SFError const err = write_compressed(&source.file, &writer);

  Fortify_SetNumAllocationsLimit(ULONG_MAX);
  assert(err.type == SFErrorType_NoMem);
  assert(source.writes == 0);
  writer_destroy(&writer);
  dfile_destroy(&source.file);
}

#endif

#ifdef FORTIFY
static void test18(void)
{
  /* Compressed size allocation failure */

  TestFile source;
  init(&source, 64);
  host_expect_error(SFErrorType_NoMem);
  Fortify_SetNumAllocationsLimit(0);

  long const size = get_compressed_size(&source.file);

  Fortify_SetNumAllocationsLimit(ULONG_MAX);
  host_check_error();
  assert(size == 0);
  assert(source.writes == 0);
  dfile_destroy(&source.file);
}

#endif

void DFileUtils_tests(void)
{
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Compressed stream round trip", test1 },
    { "Compressed output honours minimum size", test2 },
    { "Compressed size matches output", test3 },
    { "Worst compressed size bounds output", test4 },
    { "Compressed read propagates file error", test5 },
    { "Empty compressed input fails", test6 },
    { "Compressed write propagates output failure", test7 },
    { "Compressed write leaves parent open", test8 },
    { "Compressed read leaves parent open", test9 },
    { "Compressed file round trip", test10 },
    { "Compressed file read propagates file error", test11 },
    { "Compressed file open input failure", test12 },
    { "Compressed file open output failure", test13 },
    { "Unnamed file has empty leaf name", test14 },
    { "Saved file leaf name", test15 },
#ifdef FORTIFY
    { "Compressed read allocation failure", test16 },
#endif
#ifdef FORTIFY
    { "Compressed write allocation failure", test17 },
#endif
#ifdef FORTIFY
    { "Compressed size allocation failure", test18 },
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
