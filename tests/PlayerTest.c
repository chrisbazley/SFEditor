/* SFEditor test: player equipment.
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
#include "../Player.h"
#include "../PlayerData.h"

enum { Capacity = 64 };

typedef enum
{
  PlayerField_AllMustMatch,
  PlayerField_Coords,
  PlayerField_Direction,
  PlayerField_ShipType,
  PlayerField_EquipEnabled,
  PlayerField_LaserType,
  PlayerField_Engine,
  PlayerField_Control,
  PlayerField_Shields,
  PlayerField_ATA,
  PlayerField_ATG,
  PlayerField_Mines,
  PlayerField_Bombs,
  PlayerField_MegaLaser,
  PlayerField_MultiATA,
}
PlayerField;

static void check_fields(PlayerData const *actual,
  PlayerData const *expected, PlayerField except)
{
  if (except != PlayerField_Coords)
  {
    assert(actual->coords.x == expected->coords.x);
    assert(actual->coords.y == expected->coords.y);
    assert(actual->coords.z == expected->coords.z);
  }
  if (except != PlayerField_Direction)
  {
    assert(actual->direction == expected->direction);
  }
  if (except != PlayerField_ShipType)
  {
    assert(player_get_ship_type(actual) == expected->ship_type);
  }
  if (except != PlayerField_EquipEnabled)
  {
    assert(player_get_equip_enabled(actual) == expected->equip_enabled);
  }
  if (except != PlayerField_LaserType)
  {
    assert(player_get_laser_type(actual) == expected->laser_type);
  }
  if (except != PlayerField_Engine)
  {
    assert(player_get_engine(actual) == expected->engine);
  }
  if (except != PlayerField_Control)
  {
    assert(player_get_control(actual) == expected->control);
  }
  if (except != PlayerField_Shields)
  {
    assert(player_get_shields(actual) == expected->shields);
  }
  if (except != PlayerField_ATA)
  {
    assert(player_get_ata(actual) == expected->ata);
  }
  if (except != PlayerField_ATG)
  {
    assert(player_get_atg(actual) == expected->atg);
  }
  if (except != PlayerField_Mines)
  {
    assert(player_get_mines(actual) == expected->mines);
  }
  if (except != PlayerField_Bombs)
  {
    assert(player_get_bombs(actual) == expected->bombs);
  }
  if (except != PlayerField_MegaLaser)
  {
    assert(player_get_mega_laser(actual) == expected->mega_laser);
  }
  if (except != PlayerField_MultiATA)
  {
    assert(player_get_multi_ata(actual) == expected->multi_ata);
  }
}

static PlayerData const initial = {
  .state = PlayerDataState_Write, .coords = {.x = 0, .y = 1, .z = 2},
  .direction = ShipDirection_NE, .docked_in.ship = NULL,
  .ship_type = ShipType_Player, .equip_enabled = false,
  .laser_type = 5, .engine = 6, .control = 7, .shields = 8,
  .ata = 9, .atg = 10, .mines = 11, .bombs = 12, .mega_laser = 13, .multi_ata = 14,
};

static size_t write_data(PlayerData const *value, unsigned char (*buffer)[Capacity])
{
  Writer writer;
  assert(writer_mem_init(&writer, *buffer, sizeof(*buffer)));
  player_write(value, &writer);
  assert(!writer_ferror(&writer));
  long const length = writer_ftell(&writer);
  assert(length > 0);
  assert((size_t)length < sizeof(*buffer));
  /* Materialise any space skipped at the end, without assuming its size. */
  assert(writer_fputc(0, &writer) == 0);
  assert(writer_destroy(&writer) == length + 1);
  return (size_t)length;
}

static SFError read_data(PlayerData *value,
  unsigned char C23_CONST (*buffer)[Capacity], size_t length)
{
  assert(length <= sizeof(*buffer));
  Reader reader;
  assert(reader_mem_init(&reader, *buffer, length));
  SFError const err = player_read(value, &reader);
  reader_destroy(&reader);
  return err;
}

static SFErrorType const expected_errors[] = {
  SFErrorType_BadPlayerDir,
  SFErrorType_BadEnableEquip,
  SFErrorType_BadPlayerLaserType,
  SFErrorType_BadPlayerEngine,
  SFErrorType_BadPlayerControl,
  SFErrorType_BadPlayerShields,
  SFErrorType_BadSpecialType,
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
  /* Ship type */

  for (int value = 0; value <= ShipType_Fighter4; ++value)
  {
    PlayerData player = initial;
    player_set_ship_type(&player, (ShipType)value);
    assert(player_get_ship_type(&player) == (ShipType)value);
    check_fields(&player, &initial, PlayerField_ShipType);
  }
}

static void test2(void)
{
  /* Equipment flag */

  for (int value = 0; value <= 1; ++value)
  {
    PlayerData player = initial;
    player_set_equip_enabled(&player, value != 0);
    assert(player_get_equip_enabled(&player) == value);
    check_fields(&player, &initial, PlayerField_EquipEnabled);
  }
}

static void test3(void)
{
  /* Laser type */

  for (int value = 0; value <= PlayerMaxLaserType; ++value)
  {
    PlayerData player = initial;
    player_set_laser_type(&player, value);
    assert(player_get_laser_type(&player) == value);
    check_fields(&player, &initial, PlayerField_LaserType);
  }
}

static void test4(void)
{
  /* Engine */

  for (int value = 0; value <= PlayerMaxEngine; ++value)
  {
    PlayerData player = initial;
    player_set_engine(&player, value);
    assert(player_get_engine(&player) == value);
    check_fields(&player, &initial, PlayerField_Engine);
  }
}

static void test5(void)
{
  /* Control */

  for (int value = 0; value <= PlayerMaxControl; ++value)
  {
    PlayerData player = initial;
    player_set_control(&player, value);
    assert(player_get_control(&player) == value);
    check_fields(&player, &initial, PlayerField_Control);
  }
}

static void test6(void)
{
  /* Shields */

  for (int value = 0; value <= PlayerMaxShields; ++value)
  {
    PlayerData player = initial;
    player_set_shields(&player, value);
    assert(player_get_shields(&player) == value);
    check_fields(&player, &initial, PlayerField_Shields);
  }
}

static void test7(void)
{
  /* Air missiles */

  for (int value = 0; value <= PlayerMaxATA; ++value)
  {
    PlayerData player = initial;
    player_set_ata(&player, value);
    assert(player_get_ata(&player) == value);
    check_fields(&player, &initial, PlayerField_ATA);
  }
}

static void test8(void)
{
  /* Ground missiles */

  for (int value = 0; value <= PlayerMaxATG; ++value)
  {
    PlayerData player = initial;
    player_set_atg(&player, value);
    assert(player_get_atg(&player) == value);
    check_fields(&player, &initial, PlayerField_ATG);
  }
}

static void test9(void)
{
  /* Mines */

  for (int value = 0; value <= PlayerMaxMines; ++value)
  {
    PlayerData player = initial;
    player_set_mines(&player, value);
    assert(player_get_mines(&player) == value);
    check_fields(&player, &initial, PlayerField_Mines);
  }
}

static void test10(void)
{
  /* Bombs */

  for (int value = 0; value <= PlayerMaxBombs; ++value)
  {
    PlayerData player = initial;
    player_set_bombs(&player, value);
    assert(player_get_bombs(&player) == value);
    check_fields(&player, &initial, PlayerField_Bombs);
  }
}

static void test11(void)
{
  /* Mega laser */

  for (int value = 0; value <= PlayerMaxMegaLaser; ++value)
  {
    PlayerData player = initial;
    player_set_mega_laser(&player, value);
    assert(player_get_mega_laser(&player) == value);
    check_fields(&player, &initial, PlayerField_MegaLaser);
  }
}

static void test12(void)
{
  /* Multi missiles */

  for (int value = 0; value <= PlayerMaxMultiATA; ++value)
  {
    PlayerData player = initial;
    player_set_multi_ata(&player, value);
    assert(player_get_multi_ata(&player) == value);
    check_fields(&player, &initial, PlayerField_MultiATA);
  }
}

static void test13(void)
{
  /* Player round trip */

  for (unsigned v = 0; v < 256; ++v)
  {
    PlayerData player = initial, loaded = initial;
    player.coords = (CoarsePoint3d){.x = (CoarseCoord)v, .y = (CoarseCoord)(255 - v),
                                   .z = (CoarseCoord)(v ^ 85)};
    player.direction = (ShipDirection)(v % 8);
    player_set_ship_type(&player, (ShipType)(v % 5));
    player_set_equip_enabled(&player, v % 2 != 0);
    player_set_laser_type(&player, (int)(v % 8));
    player_set_engine(&player, (int)(v % 17));
    player_set_control(&player, (int)((v + 3) % 17));
    player_set_shields(&player, (int)(v % 13));
    player_set_ata(&player, (int)(v));
    player_set_atg(&player, (int)(255 - v));
    player_set_mines(&player, (int)(v ^ 85));
    player_set_bombs(&player, (int)(v ^ 170));
    player_set_mega_laser(&player, (int)((v + 7) % 256));
    player_set_multi_ata(&player, (int)((v + 13) % 256));
    unsigned char buffer[Capacity];
    size_t const size = write_data(&player, &buffer);
    assert(!SFError_fail(read_data(&loaded, &buffer, size)));
    check_fields(&loaded, &player, PlayerField_AllMustMatch);
  }
}

static void test14(void)
{
  /* Corrupt player data */

  unsigned char buffer[Capacity];
  size_t const size = write_data(&initial, &buffer);
  bool seen[ErrorCount] = {false};
  for (size_t offset = 0; offset < size; ++offset)
  {
    unsigned char const saved = buffer[offset];
    for (unsigned byte = 0; byte < 256; ++byte)
    {
      buffer[offset] = (unsigned char)byte;
      PlayerData loaded = initial;
      note_error(read_data(&loaded, &buffer, size).type, &seen);
    }
    buffer[offset] = saved;
  }
  for (size_t i = 0; i < ErrorCount; ++i)
  {
    assert(seen[i]);
  }
}

static void test15(void)
{
  /* Player truncation */

  unsigned char buffer[Capacity];
  size_t const size = write_data(&initial, &buffer);
  for (size_t length = 0; length < size; ++length)
  {
    PlayerData loaded = initial;
    SFError const err = read_data(&loaded, &buffer, length);
    if (length == 0)
    {
      assert(SFError_fail(err));
    }
    if (!SFError_fail(err))
    {
      check_fields(&loaded, &initial, PlayerField_AllMustMatch);
    }
  }
}

void Player_tests(void)
{
  /* Player */
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Ship type", test1 },
    { "Equipment flag", test2 },
    { "Laser type", test3 },
    { "Engine", test4 },
    { "Control", test5 },
    { "Shields", test6 },
    { "Air missiles", test7 },
    { "Ground missiles", test8 },
    { "Mines", test9 },
    { "Bombs", test10 },
    { "Mega laser", test11 },
    { "Multi missiles", test12 },
    { "Player round trip", test13 },
    { "Corrupt player data", test14 },
    { "Player truncation", test15 },
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
