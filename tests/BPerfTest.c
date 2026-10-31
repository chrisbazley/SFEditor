/* SFEditor test: big ship performance.
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
#include "../BPerf.h"
#include "../BPerfData.h"

enum { ShipCount = ShipType_Big3 - ShipType_Big1 + 1,
       RecordSize = 32, WireSize = ShipCount * RecordSize, BufferSize = WireSize + 1 };

enum { RemoteShieldMarker = 999999 };

typedef enum
{
  BigField_AllMustMatch,
  BigField_Shields,
  BigField_LaserProb,
  BigField_MissileProb,
  BigField_ShipProb,
  BigField_LaserType,
  BigField_ShipCount,
  BigField_RemoteShield,
}
BigField;

static void check_fields(BigPerform const *actual,
  BigPerform const *expected, BigField except)
{
  if (except != BigField_Shields)
  {
    assert(big_perform_get_shields(actual) == expected->shields);
  }
  if (except != BigField_LaserProb)
  {
    assert(big_perform_get_laser_prob(actual) == expected->laser_prob);
  }
  if (except != BigField_MissileProb)
  {
    assert(big_perform_get_missile_prob(actual) == expected->missile_prob);
  }
  if (except != BigField_ShipProb)
  {
    assert(big_perform_get_ship_prob(actual) == expected->ship_prob);
  }
  if (except != BigField_LaserType)
  {
    assert(big_perform_get_laser_type(actual) == expected->laser_type);
  }
  if (except != BigField_ShipCount)
  {
    assert(big_perform_get_ship_count(actual) == expected->ship_count);
  }
  if (except != BigField_RemoteShield)
  {
    assert(big_perform_has_remote_shield(actual) == expected->remote_shield);
  }
}

static int pattern_value(int min, int max, unsigned offset, unsigned phase)
{
  unsigned const span = (unsigned)(max - min);
  return min + (int)((phase * span / 255 + offset) % (span + 1));
}

/* The first record reaches every field's limits at phases 0 and 255.
 * Other records use different values so that aliasing is visible. */
static BigPerform pattern(unsigned ship, unsigned phase)
{
  return (BigPerform){
    .shields = pattern_value(BigPerformMinShields, BigPerformMaxShields,
      ship, phase),
    .laser_prob = pattern_value(BigPerformMinProb, BigPerformMaxProb,
      ship * 3, phase),
    .missile_prob = pattern_value(BigPerformMinProb, BigPerformMaxProb,
      ship * 5, phase),
    .ship_prob = pattern_value(BigPerformMinProb, BigPerformMaxProb,
      ship * 7, phase),
    .laser_type = pattern_value(BigPerformMinLaserType, BigPerformMaxLaserType,
      ship * 11, phase),
    .ship_count = pattern_value(BigPerformMinShips, BigPerformMaxShips,
      ship * 13, phase),
    .remote_shield = false,
  };
}

static void init_data(BigPerformData *data, unsigned phase)
{
  for (unsigned ship = 0; ship < ShipCount; ++ship)
  {
    data->type[ship] = pattern(ship, phase);
  }
}

static void write_data(BigPerformData const *data, unsigned char (*buffer)[BufferSize])
{
  Writer writer;
  assert(writer_mem_init(&writer, *buffer, sizeof(*buffer)));
  big_perform_write(data, &writer);
  assert(writer_ftell(&writer) == WireSize);
  assert(writer_fputc(165, &writer) == 165);
  assert(writer_destroy(&writer) == BufferSize);
}

static SFError read_data(BigPerformData *data,
  unsigned char C23_CONST (*buffer)[BufferSize], size_t length)
{
  assert(length <= sizeof(*buffer));
  Reader reader;
  assert(reader_mem_init(&reader, *buffer, length));
  SFError const err = big_perform_read(data, &reader);
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
    BigPerformData data, loaded;
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

  static int const values[] = {BigPerformMinShields, (BigPerformMinShields + BigPerformMaxShields) / 2, BigPerformMaxShields};
  for (ShipType type = ShipType_Big1; type <= ShipType_Big3; ++type)
  {
    for (size_t i = 0; i < ARRAY_SIZE(values); ++i)
    {
      BigPerformData data;
      init_data(&data, 63);
      BigPerform *const record = big_perform_get_ship(&data, type);
      big_perform_set_shields(record, values[i]);
      assert(big_perform_get_shields(record) == values[i]);
      for (ShipType other = ShipType_Big1; other <= ShipType_Big3; ++other)
      {
        BigPerform const expected = pattern((unsigned)(other - ShipType_Big1), 63);
        check_fields(big_perform_get_ship(&data, other), &expected,
          other == type ? BigField_Shields : BigField_AllMustMatch);
      }
    }
  }
}

static void test2(void)
{
  /* Laser probability */

  static int const values[] = {BigPerformMinProb, (BigPerformMinProb + BigPerformMaxProb) / 2, BigPerformMaxProb};
  for (ShipType type = ShipType_Big1; type <= ShipType_Big3; ++type)
  {
    for (size_t i = 0; i < ARRAY_SIZE(values); ++i)
    {
      BigPerformData data;
      init_data(&data, 63);
      BigPerform *const record = big_perform_get_ship(&data, type);
      big_perform_set_laser_prob(record, values[i]);
      assert(big_perform_get_laser_prob(record) == values[i]);
      for (ShipType other = ShipType_Big1; other <= ShipType_Big3; ++other)
      {
        BigPerform const expected = pattern((unsigned)(other - ShipType_Big1), 63);
        check_fields(big_perform_get_ship(&data, other), &expected,
          other == type ? BigField_LaserProb : BigField_AllMustMatch);
      }
    }
  }
}

static void test3(void)
{
  /* Missile probability */

  static int const values[] = {BigPerformMinProb, (BigPerformMinProb + BigPerformMaxProb) / 2, BigPerformMaxProb};
  for (ShipType type = ShipType_Big1; type <= ShipType_Big3; ++type)
  {
    for (size_t i = 0; i < ARRAY_SIZE(values); ++i)
    {
      BigPerformData data;
      init_data(&data, 63);
      BigPerform *const record = big_perform_get_ship(&data, type);
      big_perform_set_missile_prob(record, values[i]);
      assert(big_perform_get_missile_prob(record) == values[i]);
      for (ShipType other = ShipType_Big1; other <= ShipType_Big3; ++other)
      {
        BigPerform const expected = pattern((unsigned)(other - ShipType_Big1), 63);
        check_fields(big_perform_get_ship(&data, other), &expected,
          other == type ? BigField_MissileProb : BigField_AllMustMatch);
      }
    }
  }
}

static void test4(void)
{
  /* Launch probability */

  static int const values[] = {BigPerformMinProb, (BigPerformMinProb + BigPerformMaxProb) / 2, BigPerformMaxProb};
  for (ShipType type = ShipType_Big1; type <= ShipType_Big3; ++type)
  {
    for (size_t i = 0; i < ARRAY_SIZE(values); ++i)
    {
      BigPerformData data;
      init_data(&data, 63);
      BigPerform *const record = big_perform_get_ship(&data, type);
      big_perform_set_ship_prob(record, values[i]);
      assert(big_perform_get_ship_prob(record) == values[i]);
      for (ShipType other = ShipType_Big1; other <= ShipType_Big3; ++other)
      {
        BigPerform const expected = pattern((unsigned)(other - ShipType_Big1), 63);
        check_fields(big_perform_get_ship(&data, other), &expected,
          other == type ? BigField_ShipProb : BigField_AllMustMatch);
      }
    }
  }
}

static void test5(void)
{
  /* Laser type */

  static int const values[] = {BigPerformMinLaserType, (BigPerformMinLaserType + BigPerformMaxLaserType) / 2, BigPerformMaxLaserType};
  for (ShipType type = ShipType_Big1; type <= ShipType_Big3; ++type)
  {
    for (size_t i = 0; i < ARRAY_SIZE(values); ++i)
    {
      BigPerformData data;
      init_data(&data, 63);
      BigPerform *const record = big_perform_get_ship(&data, type);
      big_perform_set_laser_type(record, values[i]);
      assert(big_perform_get_laser_type(record) == values[i]);
      for (ShipType other = ShipType_Big1; other <= ShipType_Big3; ++other)
      {
        BigPerform const expected = pattern((unsigned)(other - ShipType_Big1), 63);
        check_fields(big_perform_get_ship(&data, other), &expected,
          other == type ? BigField_LaserType : BigField_AllMustMatch);
      }
    }
  }
}

static void test6(void)
{
  /* Hangar capacity */

  static int const values[] = {BigPerformMinShips, (BigPerformMinShips + BigPerformMaxShips) / 2, BigPerformMaxShips};
  for (ShipType type = ShipType_Big1; type <= ShipType_Big3; ++type)
  {
    for (size_t i = 0; i < ARRAY_SIZE(values); ++i)
    {
      BigPerformData data;
      init_data(&data, 63);
      BigPerform *const record = big_perform_get_ship(&data, type);
      big_perform_set_ship_count(record, values[i]);
      assert(big_perform_get_ship_count(record) == values[i]);
      for (ShipType other = ShipType_Big1; other <= ShipType_Big3; ++other)
      {
        BigPerform const expected = pattern((unsigned)(other - ShipType_Big1), 63);
        check_fields(big_perform_get_ship(&data, other), &expected,
          other == type ? BigField_ShipCount : BigField_AllMustMatch);
      }
    }
  }
}

static void test7(void)
{
  /* Remote shield enable */

  for (ShipType type = ShipType_Big1; type <= ShipType_Big3; ++type)
  {
    BigPerformData data;
    init_data(&data, 63);
    BigPerform *const record = big_perform_get_ship(&data, type);
    big_perform_set_remote_shield(record);
    assert(big_perform_has_remote_shield(record));
    for (ShipType other = ShipType_Big1; other <= ShipType_Big3; ++other)
    {
      BigPerform const expected = pattern((unsigned)(other - ShipType_Big1), 63);
      check_fields(big_perform_get_ship(&data, other), &expected,
        other == type ? BigField_RemoteShield : BigField_AllMustMatch);
    }
  }
}

static void test8(void)
{
  /* Normal shields disable remote generator */

  static int const values[] = {BigPerformMinShields, 5000, BigPerformMaxShields};
  for (ShipType type = ShipType_Big1; type <= ShipType_Big3; ++type)
  {
    for (size_t i = 0; i < ARRAY_SIZE(values); ++i)
    {
      BigPerform record = pattern((unsigned)(type - ShipType_Big1), 63);
      big_perform_set_remote_shield(&record);
      big_perform_set_shields(&record, values[i]);
      assert(big_perform_get_shields(&record) == values[i]);
      BigPerform const expected = pattern((unsigned)(type - ShipType_Big1), 63);
      check_fields(&record, &expected, BigField_Shields);
    }
  }
}

static void test9(void)
{
  /* Ship record lookup */

  BigPerformData data;
  init_data(&data, 63);
  for (ShipType type = ShipType_Big1; type <= ShipType_Big3; ++type)
  {
    assert(big_perform_get_ship(&data, type) == &data.type[type - ShipType_Big1]);
  }
}

static void test10(void)
{
  /* Performance round trip */

  for (unsigned phase = 0; phase < 256; ++phase)
  {
    BigPerformData data, loaded;
    init_data(&data, phase);
    unsigned char buffer[BufferSize];
    write_data(&data, &buffer);
    for (unsigned ship = 0; ship < ShipCount; ++ship)
    {
      BigPerform const expected = pattern(ship, phase);
      check_word(&buffer, ship * RecordSize + 0, expected.laser_prob);
      check_word(&buffer, ship * RecordSize + 4, expected.laser_type);
      check_word(&buffer, ship * RecordSize + 16, expected.shields);
      check_word(&buffer, ship * RecordSize + 20, expected.missile_prob);
      check_word(&buffer, ship * RecordSize + 24, expected.ship_prob);
      check_word(&buffer, ship * RecordSize + 28, expected.ship_count);
    }
    assert(!SFError_fail(read_data(&loaded, &buffer, sizeof(buffer))));
    for (ShipType type = ShipType_Big1; type <= ShipType_Big3; ++type)
    {
      BigPerform const expected = pattern((unsigned)(type - ShipType_Big1), phase);
      check_fields(big_perform_get_ship(&loaded, type), &expected, BigField_AllMustMatch);
    }
  }
}

static void test11(void)
{
  /* Remote shield round trip */

  for (unsigned phase = 0; phase < 256; ++phase)
  {
    BigPerformData data, loaded;
    init_data(&data, phase);
    for (ShipType type = ShipType_Big1; type <= ShipType_Big3; ++type)
    {
      big_perform_set_remote_shield(big_perform_get_ship(&data, type));
    }
    unsigned char buffer[BufferSize];
    write_data(&data, &buffer);
    for (unsigned ship = 0; ship < ShipCount; ++ship)
    {
      BigPerform const expected = pattern(ship, phase);
      check_word(&buffer, ship * RecordSize + 0, expected.laser_prob);
      check_word(&buffer, ship * RecordSize + 4, expected.laser_type);
      check_word(&buffer, ship * RecordSize + 16, RemoteShieldMarker);
      check_word(&buffer, ship * RecordSize + 20, expected.missile_prob);
      check_word(&buffer, ship * RecordSize + 24, expected.ship_prob);
      check_word(&buffer, ship * RecordSize + 28, expected.ship_count);
    }
    assert(!SFError_fail(read_data(&loaded, &buffer, sizeof(buffer))));
    for (ShipType type = ShipType_Big1; type <= ShipType_Big3; ++type)
    {
      BigPerform *const record = big_perform_get_ship(&loaded, type);
      assert(big_perform_get_shields(record) == BigPerformMaxShields);
      check_fields(record, &data.type[type - ShipType_Big1], BigField_Shields);
    }
  }
}

static void test12(void)
{
  /* Ignored padding */

  BigPerformData data, loaded;
  init_data(&data, 63);
  unsigned char buffer[BufferSize];
  write_data(&data, &buffer);
  for (size_t ship = 0; ship < ShipCount; ++ship)
  {
    for (size_t byte = 8; byte < 16; ++byte)
    {
      buffer[ship * RecordSize + byte] = (unsigned char)(165 + ship + byte);
    }
  }
  assert(!SFError_fail(read_data(&loaded, &buffer, sizeof(buffer))));
  for (ShipType type = ShipType_Big1; type <= ShipType_Big3; ++type)
  {
    BigPerform const expected = pattern((unsigned)(type - ShipType_Big1), 63);
    check_fields(big_perform_get_ship(&loaded, type), &expected, BigField_AllMustMatch);
  }
}

static void test13(void)
{
  /* Invalid shields */

  check_invalid(16, BigPerformMinShields - 1, SFErrorType_BadShields);
  check_invalid(16, BigPerformMaxShields + 1, SFErrorType_BadShields);
  check_invalid(16, RemoteShieldMarker - 1, SFErrorType_BadShields);
  check_invalid(16, RemoteShieldMarker + 1, SFErrorType_BadShields);
}

static void test14(void)
{
  /* Invalid laser probability */

  check_invalid(0, BigPerformMinProb - 1, SFErrorType_BadLaserProb);
  check_invalid(0, BigPerformMaxProb + 1, SFErrorType_BadLaserProb);
}

static void test15(void)
{
  /* Invalid missile probability */

  check_invalid(20, BigPerformMinProb - 1, SFErrorType_BadMissileProb);
  check_invalid(20, BigPerformMaxProb + 1, SFErrorType_BadMissileProb);
}

static void test16(void)
{
  /* Invalid launch probability */

  check_invalid(24, BigPerformMinProb - 1, SFErrorType_BadShipProb);
  check_invalid(24, BigPerformMaxProb + 1, SFErrorType_BadShipProb);
}

static void test17(void)
{
  /* Invalid laser type */

  check_invalid(4, BigPerformMinLaserType - 1, SFErrorType_BadLaserType);
  check_invalid(4, BigPerformMaxLaserType + 1, SFErrorType_BadLaserType);
}

static void test18(void)
{
  /* Invalid hangar capacity */

  check_invalid(28, BigPerformMinShips - 2, SFErrorType_BadNumShips);
  check_invalid(28, BigPerformMaxShips + 1, SFErrorType_BadNumShips);
}

static void test19(void)
{
  /* Legacy negative ship count */

  for (ShipType changed = ShipType_Big1; changed <= ShipType_Big3; ++changed)
  {
    BigPerformData data, loaded;
    init_data(&data, 63);
    unsigned char buffer[BufferSize];
    write_data(&data, &buffer);
    change_word(&buffer, (size_t)(changed - ShipType_Big1) * RecordSize + 28, -1);
    assert(!SFError_fail(read_data(&loaded, &buffer, sizeof(buffer))));
    for (ShipType type = ShipType_Big1; type <= ShipType_Big3; ++type)
    {
      BigPerform *const record = big_perform_get_ship(&loaded, type);
      BigPerform const expected = pattern((unsigned)(type - ShipType_Big1), 63);
      if (type == changed)
      {
        assert(big_perform_get_ship_count(record) == 0);
        check_fields(record, &expected, BigField_ShipCount);
      }
      else
      {
        check_fields(record, &expected, BigField_AllMustMatch);
      }
    }
  }
}

static void test20(void)
{
  /* Truncated performance data */

  BigPerformData data;
  init_data(&data, 63);
  unsigned char buffer[BufferSize];
  write_data(&data, &buffer);
  for (size_t length = 0; length < WireSize; ++length)
  {
    BigPerformData loaded;
    assert(read_data(&loaded, &buffer, length).type == SFErrorType_ReadFail);
  }
}

void BPerf_tests(void)
{
  /* BPerf */
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Shields", test1 },
    { "Laser probability", test2 },
    { "Missile probability", test3 },
    { "Launch probability", test4 },
    { "Laser type", test5 },
    { "Hangar capacity", test6 },
    { "Remote shield enable", test7 },
    { "Normal shields disable remote generator", test8 },
    { "Ship record lookup", test9 },
    { "Performance round trip", test10 },
    { "Remote shield round trip", test11 },
    { "Ignored padding", test12 },
    { "Invalid shields", test13 },
    { "Invalid laser probability", test14 },
    { "Invalid missile probability", test15 },
    { "Invalid launch probability", test16 },
    { "Invalid laser type", test17 },
    { "Invalid hangar capacity", test18 },
    { "Legacy negative ship count", test19 },
    { "Truncated performance data", test20 },
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
