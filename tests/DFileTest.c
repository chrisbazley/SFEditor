/* SFEditor test: generic file lifecycle.
 * Copyright (C) 2026 Christopher Bazley
 */
#undef NDEBUG

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "Macros.h"
#include "Debug.h"
#include "Reader.h"
#include "ReaderMem.h"
#include "Tests.h"
#include "../SFError.h"
#include "Writer.h"
#include "WriterMem.h"
#include "StrDict.h"
#include "../DFile.h"
#include "../DFileData.h"

/* Callback counters remain valid after the stack-owned file is destroyed. */
typedef struct
{
  DFile file;
  SFError read_result;
  long min_size;
  unsigned reads, writes, queries, destroys;
} TestFile;

static SFError read_cb(DFile const *file, Reader *reader)
{
  TestFile *const ctx = CONTAINER_OF(file, TestFile, file);
  ++ctx->reads;
  assert(reader_fgetc(reader) == 'R');
  return ctx->read_result;
}

static void write_cb(DFile const *file, Writer *writer)
{
  TestFile *const ctx = CONTAINER_OF(file, TestFile, file);
  ++ctx->writes;
  writer_fputc('W', writer);
  writer_fseek(writer, 7, SEEK_CUR);
  writer_fputc('Z', writer);
}

static long min_size_cb(DFile const *file)
{
  TestFile *const ctx = CONTAINER_OF(file, TestFile, file);
  ++ctx->queries;
  return ctx->min_size;
}

static void destroy_cb(DFile const *file)
{
  TestFile *const ctx = CONTAINER_OF(file, TestFile, file);
  dfile_destroy(&ctx->file);
  ++ctx->destroys;
}

static void init_file(TestFile *ctx)
{
  *ctx = (TestFile){.read_result = SFERROR(OK), .min_size = 37};
  dfile_init(&ctx->file, read_cb, write_cb, min_size_cb, destroy_cb);
}

static void check_metadata(DFile const *file, _Optional char const *name,
  int C23_CONST (*date)[2], bool modified)
{
  _Optional char const *const actual = dfile_get_name(file);

  if (name)
  {
    assert(actual);
    assert(strcmp(&*actual, &*name) == 0);
  }
  else
  {
    assert(actual == NULL);
  }

  int const *const actual_date = dfile_get_date(file);
  for (size_t i = 0; i < ARRAY_SIZE(*date); ++i)
  {
    assert(actual_date[i] == (*date)[i]);
  }

  assert(dfile_get_modified(file) == modified);
}

static void destroy_dict(StrDict *dict)
{
  strdict_destroy(dict, (StrDictDestructorFn *)NULL, dict);
}

static void test1(void)
{
  /* Initial file metadata */

  TestFile ctx;
  init_file(&ctx);
  int date[2] = {0, 0};

  check_metadata(&ctx.file, NULL, &date, false);

  dfile_release(&ctx.file);
  assert(ctx.destroys == 1);
}

static void test2(void)
{
  /* File saved metadata */

  TestFile ctx;
  init_file(&ctx);
  int date[2] = {0x12345678, -12345};

  assert(dfile_set_saved(&ctx.file, "Saved", date));

  check_metadata(&ctx.file, "Saved", &date, false);

  dfile_release(&ctx.file);
}

static void test3(void)
{
  /* Saved file name is copied */

  TestFile ctx;
  init_file(&ctx);
  char name[] = "Original";
  int date[2] = {13, 17};
  assert(dfile_set_saved(&ctx.file, name, date));

  name[0] = 'X';

  check_metadata(&ctx.file, "Original", &date, false);

  dfile_release(&ctx.file);
}

static void test4(void)
{
  /* Saved file date is copied */

  TestFile ctx;
  init_file(&ctx);
  int date[2] = {13, 17};
  assert(dfile_set_saved(&ctx.file, "Saved", date));

  date[0] = 99;
  date[1] = 101;

  int expected[2] = {13, 17};
  check_metadata(&ctx.file, "Saved", &expected, false);

  dfile_release(&ctx.file);
}

static void test5(void)
{
  /* Modified file preserves saved identity */

  TestFile ctx;
  init_file(&ctx);
  int date[2] = {13, 17};
  assert(dfile_set_saved(&ctx.file, "Saved", date));

  dfile_set_modified(&ctx.file);

  check_metadata(&ctx.file, "Saved", &date, true);

  dfile_release(&ctx.file);
}

static void test6(void)
{
  /* Saving clears modified state */

  TestFile ctx;
  init_file(&ctx);
  dfile_set_modified(&ctx.file);
  int date[2] = {13, 17};

  assert(dfile_set_saved(&ctx.file, "Saved", date));

  check_metadata(&ctx.file, "Saved", &date, false);

  dfile_release(&ctx.file);
}

static void test7(void)
{
  /* Replacing saved metadata */

  TestFile ctx;
  init_file(&ctx);
  int first[2] = {3, 5}, second[2] = {7, 11};
  assert(dfile_set_saved(&ctx.file, "First", first));

  assert(dfile_set_saved(&ctx.file, "Second", second));

  check_metadata(&ctx.file, "Second", &second, false);

  dfile_release(&ctx.file);
}

static void test8(void)
{
  /* Clearing saved identity */

  TestFile ctx;
  init_file(&ctx);
  int first[2] = {3, 5}, second[2] = {7, 11};
  assert(dfile_set_saved(&ctx.file, "Saved", first));

  assert(dfile_set_saved(&ctx.file, NULL, second));

  check_metadata(&ctx.file, NULL, &second, true);

  dfile_release(&ctx.file);
}

static void test9(void)
{
  /* Claimed file survives release */

  TestFile ctx;
  init_file(&ctx);
  dfile_claim(&ctx.file);

  dfile_release(&ctx.file);

  assert(ctx.destroys == 0);

  dfile_release(&ctx.file);
  assert(ctx.destroys == 1);
}

static void test10(void)
{
  /* Multiple file claims */

  TestFile ctx;
  init_file(&ctx);

  for (unsigned i = 0; i < 16; ++i)
  {
    dfile_claim(&ctx.file);
  }

  for (unsigned i = 0; i < 16; ++i)
  {
    dfile_release(&ctx.file);
    assert(ctx.destroys == 0);
  }

  dfile_release(&ctx.file);
  assert(ctx.destroys == 1);
}

static void test11(void)
{
  /* File read callback */

  TestFile ctx;
  init_file(&ctx);
  unsigned char const buffer[] = {'R', 'S'};
  Reader reader;
  assert(reader_mem_init(&reader, buffer, sizeof(buffer)));

  assert(!SFError_fail(dfile_read(&ctx.file, &reader)));

  assert(ctx.reads == 1);
  assert(reader_ftell(&reader) == 1);
  assert(reader_fgetc(&reader) == 'S');

  reader_destroy(&reader);
  dfile_release(&ctx.file);
}

static void test12(void)
{
  /* File read error propagation */

  TestFile ctx;
  init_file(&ctx);
  ctx.read_result = SFERROR(BadMissionType);
  unsigned char const buffer[] = {'R'};
  Reader reader;
  assert(reader_mem_init(&reader, buffer, sizeof(buffer)));

  SFError const err = dfile_read(&ctx.file, &reader);

  assert(err.type == ctx.read_result.type);
  assert(err.loc == ctx.read_result.loc);
  assert(ctx.reads == 1);

  reader_destroy(&reader);
  dfile_release(&ctx.file);
}

static void test13(void)
{
  /* File without read callback */

  DFile file;
  dfile_init(&file, NULL, NULL, NULL, NULL);
  unsigned char const buffer[] = {'R'};
  Reader reader;
  assert(reader_mem_init(&reader, buffer, sizeof(buffer)));

  assert(!SFError_fail(dfile_read(&file, &reader)));

  assert(reader_ftell(&reader) == 0);

  reader_destroy(&reader);
  dfile_release(&file);
}

static void test14(void)
{
  /* File write callback */

  TestFile ctx;
  init_file(&ctx);
  unsigned char buffer[16] = {0};
  Writer writer;
  assert(writer_mem_init(&writer, buffer, sizeof(buffer)));

  dfile_write(&ctx.file, &writer);

  assert(!writer_ferror(&writer));
  assert(writer_ftell(&writer) == 9);
  assert(ctx.writes == 1);
  assert(buffer[0] == 'W');
  assert(buffer[8] == 'Z');

  writer_destroy(&writer);
  dfile_release(&ctx.file);
}

static void test15(void)
{
  /* File write error propagation */

  TestFile ctx;
  init_file(&ctx);
  unsigned char buffer[16];
  Writer writer;
  assert(writer_mem_init(&writer, buffer, 0));

  dfile_write(&ctx.file, &writer);

  assert(writer_ferror(&writer));
  assert(ctx.writes == 1);

  writer_destroy(&writer);
  dfile_release(&ctx.file);
}

static void test16(void)
{
  /* File without write callback */

  DFile file;
  dfile_init(&file, NULL, NULL, NULL, NULL);
  unsigned char buffer[16];
  Writer writer;
  assert(writer_mem_init(&writer, buffer, sizeof(buffer)));

  dfile_write(&file, &writer);

  assert(writer_ftell(&writer) == 0);
  assert(!writer_ferror(&writer));

  writer_destroy(&writer);
  dfile_release(&file);
}

static void test17(void)
{
  /* File minimum size callback */

  TestFile ctx;
  init_file(&ctx);

  assert(dfile_get_min_size(&ctx.file) == 37);

  assert(ctx.queries == 1);
  assert(ctx.writes == 0);

  dfile_release(&ctx.file);
}

static void test18(void)
{
  /* File minimum size from writer */

  TestFile ctx;
  init_file(&ctx);
  dfile_init(&ctx.file, NULL, write_cb, NULL, destroy_cb);

  assert(dfile_get_min_size(&ctx.file) == 9);

  assert(ctx.writes == 1);
  assert(ctx.queries == 0);

  dfile_release(&ctx.file);
}

static void test19(void)
{
  /* Empty file minimum size */

  DFile file;
  dfile_init(&file, NULL, NULL, NULL, NULL);

  assert(dfile_get_min_size(&file) == 0);

  dfile_release(&file);
}

static void test20(void)
{
  /* Shared file lookup claims a reference */

  StrDict dict;
  strdict_init(&dict);
  TestFile ctx;
  init_file(&ctx);
  int date[2] = {13, 17};
  assert(dfile_set_saved(&ctx.file, "Shared", date));
  assert(dfile_set_shared(&ctx.file, &dict));

  _Optional DFile *const found = dfile_find_shared(&dict, "sHaReD");

  assert(found == &ctx.file);
  dfile_release(&ctx.file);
  assert(ctx.destroys == 0);
  assert(found);
  check_metadata(&*found, "Shared", &date, false);

  dfile_release(&*found);
  assert(ctx.destroys == 1);
  assert(dfile_find_shared(&dict, "Shared") == NULL);
  destroy_dict(&dict);
}

static void test21(void)
{
  /* Missing shared file */

  StrDict dict;
  strdict_init(&dict);

  assert(dfile_find_shared(&dict, "Missing") == NULL);

  destroy_dict(&dict);
}

static void test22(void)
{
  /* Default destruction removes shared file */

  StrDict dict;
  strdict_init(&dict);
  DFile file;
  dfile_init(&file, NULL, NULL, NULL, NULL);
  int date[2] = {13, 17};
  assert(dfile_set_saved(&file, "Shared", date));
  assert(dfile_set_shared(&file, &dict));

  dfile_release(&file);

  assert(dfile_find_shared(&dict, "Shared") == NULL);

  destroy_dict(&dict);
}

#ifdef FORTIFY
static void test23(void)
{
  /* Saved metadata allocation rollback */

  TestFile ctx;
  init_file(&ctx);
  int first[2] = {3, 5}, second[2] = {7, 11};
  assert(dfile_set_saved(&ctx.file, "Original", first));
  dfile_set_modified(&ctx.file);

  Fortify_SetNumAllocationsLimit(0);
  bool const saved = dfile_set_saved(&ctx.file, "Replacement", second);
  Fortify_SetNumAllocationsLimit(ULONG_MAX);

  assert(!saved);
  check_metadata(&ctx.file, "Original", &first, true);

  dfile_release(&ctx.file);
}

static void test24(void)
{
  /* Shared file allocation rollback */

  StrDict dict;
  strdict_init(&dict);
  TestFile ctx;
  init_file(&ctx);
  int date[2] = {13, 17};
  assert(dfile_set_saved(&ctx.file, "Shared", date));

  Fortify_SetNumAllocationsLimit(0);
  bool const shared = dfile_set_shared(&ctx.file, &dict);
  Fortify_SetNumAllocationsLimit(ULONG_MAX);

  assert(!shared);
  assert(dfile_find_shared(&dict, "Shared") == NULL);
  check_metadata(&ctx.file, "Shared", &date, false);
  assert(dfile_set_shared(&ctx.file, &dict));

  dfile_release(&ctx.file);
  assert(ctx.destroys == 1);
  destroy_dict(&dict);
}

#endif

void DFile_tests(void)
{
  /* DFile */
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Initial file metadata", test1 },
    { "File saved metadata", test2 },
    { "Saved file name is copied", test3 },
    { "Saved file date is copied", test4 },
    { "Modified file preserves saved identity", test5 },
    { "Saving clears modified state", test6 },
    { "Replacing saved metadata", test7 },
    { "Clearing saved identity", test8 },
    { "Claimed file survives release", test9 },
    { "Multiple file claims", test10 },
    { "File read callback", test11 },
    { "File read error propagation", test12 },
    { "File without read callback", test13 },
    { "File write callback", test14 },
    { "File write error propagation", test15 },
    { "File without write callback", test16 },
    { "File minimum size callback", test17 },
    { "File minimum size from writer", test18 },
    { "Empty file minimum size", test19 },
    { "Shared file lookup claims a reference", test20 },
    { "Missing shared file", test21 },
    { "Default destruction removes shared file", test22 },
#ifdef FORTIFY
    { "Saved metadata allocation rollback", test23 },
    { "Shared file allocation rollback", test24 },
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
