/* SFEditor test: fighter performance.
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
#include "../FPerf.h"
#include "../FPerfData.h"

enum { ShipCount = ShipType_Fighter4 - ShipType_Fighter1 + 1, Capacity = 256 };

typedef enum
{
  FighterField_AllMustMatch,
  FighterField_Shields,
  FighterField_Engine,
  FighterField_LaserProb,
  FighterField_MissileProb,
  FighterField_LaserType,
  FighterField_Control,
}
FighterField;

static void check_fields(FighterPerform const *actual,
  FighterPerform const *expected, FighterField except)
{
  if (except != FighterField_Shields)
  {
    assert(fighter_perform_get_shields(actual) == expected->shields);
  }
  if (except != FighterField_Engine)
  {
    assert(fighter_perform_get_engine(actual) == expected->engine);
  }
  if (except != FighterField_LaserProb)
  {
    assert(fighter_perform_get_laser_prob(actual) == expected->laser_prob);
  }
  if (except != FighterField_MissileProb)
  {
    assert(fighter_perform_get_missile_prob(actual) == expected->missile_prob);
  }
  if (except != FighterField_LaserType)
  {
    assert(fighter_perform_get_laser_type(actual) == expected->laser_type);
  }
  if (except != FighterField_Control)
  {
    assert(fighter_perform_get_control(actual) == expected->control);
  }
}

static int pattern_value(int min, int max, unsigned offset, unsigned phase)
{
  unsigned const span = (unsigned)(max - min);
  return min + (int)((phase * span / 255 + offset) % (span + 1));
}

/* The first record reaches every field's limits at phases 0 and 255.
 * Other records use different values so that aliasing is visible. */
static FighterPerform pattern(unsigned ship, unsigned phase)
{
  return (FighterPerform){
    .shields = pattern_value(FighterPerformMinShields, FighterPerformMaxShields,
      ship, phase),
    .engine = pattern_value(FighterPerformMinEngine, FighterPerformMaxEngine,
      ship * 3, phase),
    .laser_prob = pattern_value(FighterPerformMinProb, FighterPerformMaxProb,
      ship * 5, phase),
    .missile_prob = pattern_value(FighterPerformMinProb, FighterPerformMaxProb,
      ship * 7, phase),
    .laser_type = pattern_value(FighterPerformMinLaserType, FighterPerformMaxLaserType,
      ship * 11, phase),
    .control = pattern_value(FighterPerformMinControl, FighterPerformMaxControl,
      ship * 13, phase),
  };
}

static void init_data(FighterPerformData *data, unsigned phase)
{
  for (unsigned ship = 0; ship < ShipCount; ++ship)
  {
    data->type[ship] = pattern(ship, phase);
  }
}

static size_t write_data(FighterPerformData const *value, unsigned char (*buffer)[Capacity])
{
  Writer writer;
  assert(writer_mem_init(&writer, *buffer, sizeof(*buffer)));
  fighter_perform_write(value, &writer);
  assert(!writer_ferror(&writer));
  long const length = writer_ftell(&writer);
  assert(length > 0);
  assert((size_t)length < sizeof(*buffer));
  /* Materialise any space skipped at the end, without assuming its size. */
  assert(writer_fputc(0, &writer) == 0);
  assert(writer_destroy(&writer) == length + 1);
  return (size_t)length;
}

static SFError read_data(FighterPerformData *value,
  unsigned char C23_CONST (*buffer)[Capacity], size_t length)
{
  assert(length <= sizeof(*buffer));
  Reader reader;
  assert(reader_mem_init(&reader, *buffer, length));
  SFError const err = fighter_perform_read(value, &reader);
  reader_destroy(&reader);
  return err;
}

static SFErrorType const expected_errors[] = {
  SFErrorType_BadLaserProb,
  SFErrorType_BadLaserType,
  SFErrorType_BadEngine,
  SFErrorType_BadControl,
  SFErrorType_BadShields,
  SFErrorType_BadMissileProb,
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
  /* Shields */

  static int const values[] = {FighterPerformMinShields, (FighterPerformMinShields + FighterPerformMaxShields) / 2, FighterPerformMaxShields};
  for (ShipType type = ShipType_Fighter1; type <= ShipType_Fighter4; ++type)
  {
    for (size_t i = 0; i < ARRAY_SIZE(values); ++i)
    {
      FighterPerformData data;
      init_data(&data, 63);
      FighterPerform *const record = fighter_perform_get_ship(&data, type);
      fighter_perform_set_shields(record, values[i]);
      assert(fighter_perform_get_shields(record) == values[i]);
      for (ShipType other = ShipType_Fighter1; other <= ShipType_Fighter4; ++other)
      {
        FighterPerform const expected = pattern((unsigned)(other - ShipType_Fighter1), 63);
        check_fields(fighter_perform_get_ship(&data, other), &expected,
          other == type ? FighterField_Shields : FighterField_AllMustMatch);
      }
    }
  }
}

static void test2(void)
{
  /* Engine */

  static int const values[] = {FighterPerformMinEngine, (FighterPerformMinEngine + FighterPerformMaxEngine) / 2, FighterPerformMaxEngine};
  for (ShipType type = ShipType_Fighter1; type <= ShipType_Fighter4; ++type)
  {
    for (size_t i = 0; i < ARRAY_SIZE(values); ++i)
    {
      FighterPerformData data;
      init_data(&data, 63);
      FighterPerform *const record = fighter_perform_get_ship(&data, type);
      fighter_perform_set_engine(record, values[i]);
      assert(fighter_perform_get_engine(record) == values[i]);
      for (ShipType other = ShipType_Fighter1; other <= ShipType_Fighter4; ++other)
      {
        FighterPerform const expected = pattern((unsigned)(other - ShipType_Fighter1), 63);
        check_fields(fighter_perform_get_ship(&data, other), &expected,
          other == type ? FighterField_Engine : FighterField_AllMustMatch);
      }
    }
  }
}

static void test3(void)
{
  /* Laser probability */

  static int const values[] = {FighterPerformMinProb, (FighterPerformMinProb + FighterPerformMaxProb) / 2, FighterPerformMaxProb};
  for (ShipType type = ShipType_Fighter1; type <= ShipType_Fighter4; ++type)
  {
    for (size_t i = 0; i < ARRAY_SIZE(values); ++i)
    {
      FighterPerformData data;
      init_data(&data, 63);
      FighterPerform *const record = fighter_perform_get_ship(&data, type);
      fighter_perform_set_laser_prob(record, values[i]);
      assert(fighter_perform_get_laser_prob(record) == values[i]);
      for (ShipType other = ShipType_Fighter1; other <= ShipType_Fighter4; ++other)
      {
        FighterPerform const expected = pattern((unsigned)(other - ShipType_Fighter1), 63);
        check_fields(fighter_perform_get_ship(&data, other), &expected,
          other == type ? FighterField_LaserProb : FighterField_AllMustMatch);
      }
    }
  }
}

static void test4(void)
{
  /* Missile probability */

  static int const values[] = {FighterPerformMinProb, (FighterPerformMinProb + FighterPerformMaxProb) / 2, FighterPerformMaxProb};
  for (ShipType type = ShipType_Fighter1; type <= ShipType_Fighter4; ++type)
  {
    for (size_t i = 0; i < ARRAY_SIZE(values); ++i)
    {
      FighterPerformData data;
      init_data(&data, 63);
      FighterPerform *const record = fighter_perform_get_ship(&data, type);
      fighter_perform_set_missile_prob(record, values[i]);
      assert(fighter_perform_get_missile_prob(record) == values[i]);
      for (ShipType other = ShipType_Fighter1; other <= ShipType_Fighter4; ++other)
      {
        FighterPerform const expected = pattern((unsigned)(other - ShipType_Fighter1), 63);
        check_fields(fighter_perform_get_ship(&data, other), &expected,
          other == type ? FighterField_MissileProb : FighterField_AllMustMatch);
      }
    }
  }
}

static void test5(void)
{
  /* Laser type */

  static int const values[] = {FighterPerformMinLaserType, (FighterPerformMinLaserType + FighterPerformMaxLaserType) / 2, FighterPerformMaxLaserType};
  for (ShipType type = ShipType_Fighter1; type <= ShipType_Fighter4; ++type)
  {
    for (size_t i = 0; i < ARRAY_SIZE(values); ++i)
    {
      FighterPerformData data;
      init_data(&data, 63);
      FighterPerform *const record = fighter_perform_get_ship(&data, type);
      fighter_perform_set_laser_type(record, values[i]);
      assert(fighter_perform_get_laser_type(record) == values[i]);
      for (ShipType other = ShipType_Fighter1; other <= ShipType_Fighter4; ++other)
      {
        FighterPerform const expected = pattern((unsigned)(other - ShipType_Fighter1), 63);
        check_fields(fighter_perform_get_ship(&data, other), &expected,
          other == type ? FighterField_LaserType : FighterField_AllMustMatch);
      }
    }
  }
}

static void test6(void)
{
  /* Control */

  static int const values[] = {FighterPerformMinControl, (FighterPerformMinControl + FighterPerformMaxControl) / 2, FighterPerformMaxControl};
  for (ShipType type = ShipType_Fighter1; type <= ShipType_Fighter4; ++type)
  {
    for (size_t i = 0; i < ARRAY_SIZE(values); ++i)
    {
      FighterPerformData data;
      init_data(&data, 63);
      FighterPerform *const record = fighter_perform_get_ship(&data, type);
      fighter_perform_set_control(record, values[i]);
      assert(fighter_perform_get_control(record) == values[i]);
      for (ShipType other = ShipType_Fighter1; other <= ShipType_Fighter4; ++other)
      {
        FighterPerform const expected = pattern((unsigned)(other - ShipType_Fighter1), 63);
        check_fields(fighter_perform_get_ship(&data, other), &expected,
          other == type ? FighterField_Control : FighterField_AllMustMatch);
      }
    }
  }
}

static void test7(void)
{
  /* Ship record lookup */

  FighterPerformData data;
  init_data(&data, 63);
  for (ShipType type = ShipType_Fighter1; type <= ShipType_Fighter4; ++type)
  {
    assert(fighter_perform_get_ship(&data, type) == &data.type[type - ShipType_Fighter1]);
  }
}

static void test8(void)
{
  /* Performance round trip */

  for (unsigned phase = 0; phase < 256; ++phase)
  {
    FighterPerformData data, loaded;
    init_data(&data, phase);
    unsigned char buffer[Capacity];
    size_t const size = write_data(&data, &buffer);
    assert(!SFError_fail(read_data(&loaded, &buffer, size)));
    for (ShipType type = ShipType_Fighter1; type <= ShipType_Fighter4; ++type)
    {
      FighterPerform const expected = pattern((unsigned)(type - ShipType_Fighter1), phase);
      check_fields(fighter_perform_get_ship(&loaded, type), &expected, FighterField_AllMustMatch);
    }
  }
}

static void test9(void)
{
  /* Corrupt performance data */

  FighterPerformData data;
  init_data(&data, 0);
  unsigned char buffer[Capacity];
  size_t const size = write_data(&data, &buffer);
  bool seen[ErrorCount] = {false};
  for (size_t offset = 0; offset < size; ++offset)
  {
    unsigned char const saved = buffer[offset];
    for (unsigned byte = 0; byte < 256; ++byte)
    {
      buffer[offset] = (unsigned char)byte;
      FighterPerformData loaded;
      note_error(read_data(&loaded, &buffer, size).type, &seen);
    }
    buffer[offset] = saved;
  }
  for (size_t i = 0; i < ErrorCount; ++i)
  {
    assert(seen[i]);
  }
}

static void test10(void)
{
  /* Truncated performance data */

  FighterPerformData data;
  init_data(&data, 63);
  unsigned char buffer[Capacity];
  size_t const size = write_data(&data, &buffer);
  for (size_t length = 0; length < size; ++length)
  {
    FighterPerformData loaded;
    SFError const err = read_data(&loaded, &buffer, length);
    if (length == 0)
    {
      assert(SFError_fail(err));
    }
    if (!SFError_fail(err))
    {
      for (ShipType type = ShipType_Fighter1; type <= ShipType_Fighter4; ++type)
      {
        FighterPerform const expected = pattern((unsigned)(type - ShipType_Fighter1), 63);
        check_fields(fighter_perform_get_ship(&loaded, type), &expected, FighterField_AllMustMatch);
      }
    }
  }
}

void FPerf_tests(void)
{
  /* FPerf */
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Shields", test1 },
    { "Engine", test2 },
    { "Laser probability", test3 },
    { "Missile probability", test4 },
    { "Laser type", test5 },
    { "Control", test6 },
    { "Ship record lookup", test7 },
    { "Performance round trip", test8 },
    { "Corrupt performance data", test9 },
    { "Truncated performance data", test10 },
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
