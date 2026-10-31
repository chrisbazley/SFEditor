/* SFEditor test: fighter performance.
 * Copyright (C) 2026 Christopher Bazley
 */
#undef NDEBUG

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

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

enum { ShipCount = ShipType_Fighter4 - ShipType_Fighter1 + 1,
       RecordSize = 32, WireSize = ShipCount * RecordSize, BufferSize = WireSize + 1 };

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

static void write_data(FighterPerformData const *data, unsigned char (*buffer)[BufferSize])
{
  Writer writer;
  assert(writer_mem_init(&writer, *buffer, sizeof(*buffer)));
  fighter_perform_write(data, &writer);
  assert(writer_ftell(&writer) == WireSize);
  assert(writer_fputc(165, &writer) == 165);
  assert(writer_destroy(&writer) == BufferSize);
}

static SFError read_data(FighterPerformData *data,
  unsigned char C23_CONST (*buffer)[BufferSize], size_t length)
{
  assert(length <= sizeof(*buffer));
  Reader reader;
  assert(reader_mem_init(&reader, *buffer, length));
  SFError const err = fighter_perform_read(data, &reader);
  if (!SFError_fail(err))
  {
    assert(reader_ftell(&reader) == WireSize);
    assert(reader_fgetc(&reader) == 165);
  }
  reader_destroy(&reader);
  return err;
}

static void check_word(unsigned char C23_CONST (*buffer)[BufferSize],
  size_t offset, int32_t value)
{
  for (size_t byte = 0; byte < 4; ++byte)
  {
    assert((*buffer)[offset + byte] == (((uint32_t)value >> (8 * byte)) & 255));
  }
}

static void change_word(unsigned char (*buffer)[BufferSize], size_t offset, int32_t value)
{
  assert(offset + 4 <= WireSize);
  Writer writer;
  assert(writer_mem_init(&writer, &(*buffer)[offset], 4));
  writer_fwrite_int32(value, &writer);
  assert(writer_destroy(&writer) == 4);
}

static void check_invalid(size_t offset, int32_t value, SFErrorType expected)
{
  for (unsigned ship = 0; ship < ShipCount; ++ship)
  {
    FighterPerformData data, loaded;
    init_data(&data, 63);
    unsigned char buffer[BufferSize];
    write_data(&data, &buffer);
    change_word(&buffer, ship * RecordSize + offset, value);
    assert(read_data(&loaded, &buffer, sizeof(buffer)).type == expected);
  }
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
    unsigned char buffer[BufferSize];
    write_data(&data, &buffer);
    for (unsigned ship = 0; ship < ShipCount; ++ship)
    {
      FighterPerform const expected = pattern(ship, phase);
      check_word(&buffer, ship * RecordSize + 0, expected.laser_prob);
      check_word(&buffer, ship * RecordSize + 4, expected.laser_type);
      check_word(&buffer, ship * RecordSize + 8, expected.engine);
      check_word(&buffer, ship * RecordSize + 12, expected.control);
      check_word(&buffer, ship * RecordSize + 16, expected.shields);
      check_word(&buffer, ship * RecordSize + 20, expected.missile_prob);
    }
    assert(!SFError_fail(read_data(&loaded, &buffer, sizeof(buffer))));
    for (ShipType type = ShipType_Fighter1; type <= ShipType_Fighter4; ++type)
    {
      FighterPerform const expected = pattern((unsigned)(type - ShipType_Fighter1), phase);
      check_fields(fighter_perform_get_ship(&loaded, type), &expected, FighterField_AllMustMatch);
    }
  }
}

static void test9(void)
{
  /* Ignored padding */

  FighterPerformData data, loaded;
  init_data(&data, 63);
  unsigned char buffer[BufferSize];
  write_data(&data, &buffer);
  for (size_t ship = 0; ship < ShipCount; ++ship)
  {
    for (size_t byte = 24; byte < 32; ++byte)
    {
      buffer[ship * RecordSize + byte] = (unsigned char)(165 + ship + byte);
    }
  }
  assert(!SFError_fail(read_data(&loaded, &buffer, sizeof(buffer))));
  for (ShipType type = ShipType_Fighter1; type <= ShipType_Fighter4; ++type)
  {
    FighterPerform const expected = pattern((unsigned)(type - ShipType_Fighter1), 63);
    check_fields(fighter_perform_get_ship(&loaded, type), &expected, FighterField_AllMustMatch);
  }
}

static void test10(void)
{
  /* Invalid shields */

  check_invalid(16, FighterPerformMinShields - 1, SFErrorType_BadShields);
  check_invalid(16, FighterPerformMaxShields + 1, SFErrorType_BadShields);
}

static void test11(void)
{
  /* Invalid engine */

  check_invalid(8, FighterPerformMinEngine - 1, SFErrorType_BadEngine);
  check_invalid(8, FighterPerformMaxEngine + 1, SFErrorType_BadEngine);
}

static void test12(void)
{
  /* Invalid laser probability */

  check_invalid(0, FighterPerformMinProb - 1, SFErrorType_BadLaserProb);
  check_invalid(0, FighterPerformMaxProb + 1, SFErrorType_BadLaserProb);
}

static void test13(void)
{
  /* Invalid missile probability */

  check_invalid(20, FighterPerformMinProb - 1, SFErrorType_BadMissileProb);
  check_invalid(20, FighterPerformMaxProb + 1, SFErrorType_BadMissileProb);
}

static void test14(void)
{
  /* Invalid laser type */

  check_invalid(4, FighterPerformMinLaserType - 1, SFErrorType_BadLaserType);
  check_invalid(4, FighterPerformMaxLaserType + 1, SFErrorType_BadLaserType);
}

static void test15(void)
{
  /* Invalid control */

  check_invalid(12, FighterPerformMinControl - 1, SFErrorType_BadControl);
  check_invalid(12, FighterPerformMaxControl + 1, SFErrorType_BadControl);
}

static void test16(void)
{
  /* Truncated performance data */

  FighterPerformData data;
  init_data(&data, 63);
  unsigned char buffer[BufferSize];
  write_data(&data, &buffer);
  for (size_t length = 0; length < WireSize - 8; ++length)
  {
    FighterPerformData loaded;
    assert(read_data(&loaded, &buffer, length).type == SFErrorType_ReadFail);
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
    { "Ignored padding", test9 },
    { "Invalid shields", test10 },
    { "Invalid engine", test11 },
    { "Invalid laser probability", test12 },
    { "Invalid missile probability", test13 },
    { "Invalid laser type", test14 },
    { "Invalid control", test15 },
    { "Truncated performance data", test16 },
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
