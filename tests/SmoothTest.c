/* SFEditor test: texture groups and smoothing.
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
#include "Tests.h"
#include "../Smooth.h"
#include "../SmoothData.h"
#include "../MapEditCtx.h"
#include "../Map.h"
#include "../DFile.h"

enum { TileCount = 32, GridSize = 3 };

static struct
{
  MapEditContext map;
  unsigned char cells[GridSize][GridSize], before[GridSize][GridSize];
  unsigned writes;
  _Optional MapData *data;
} fixture;

static void redraw(MapArea const *area, struct EditSession *session)
{
  assert(session == fixture.map.session);
  assert(area->min.x == 1);
  assert(area->min.y == 1);
  assert(area->max.x == 1);
  assert(area->max.y == 1);
  ++fixture.writes;
}

/* The session pointer is passed back as an opaque callback cookie. */
static void smooth(MapTexGroups *groups, _Optional MapEditChanges *changes)
{
  assert(fixture.data);
  MapData *const data = &*fixture.data;
  for (MapCoord y = 0; y < GridSize; ++y)
  {
    for (MapCoord x = 0; x < GridSize; ++x)
    {
      map_set_tile(data, (MapPoint){x, y}, map_ref_from_num(fixture.cells[y][x]));
    }
  }

  MapTexGroups_smooth(&fixture.map, groups, (MapPoint){1, 1}, changes);

  for (MapCoord y = 0; y < GridSize; ++y)
  {
    for (MapCoord x = 0; x < GridSize; ++x)
    {
      fixture.cells[y][x] = map_ref_to_num(map_get_tile(data, (MapPoint){x, y}));
    }
  }
}

static void init_grid(unsigned char centre, unsigned char neighbour)
{
  for (size_t y = 0; y < GridSize; ++y)
  {
    for (size_t x = 0; x < GridSize; ++x)
    {
      fixture.cells[y][x] = neighbour;
    }
  }
  fixture.cells[1][1] = centre;
  memcpy(fixture.before, fixture.cells, sizeof(fixture.before));
  fixture.writes = 0;
  fixture.data = map_create_overlay();
  assert(fixture.data);
  fixture.map = (MapEditContext){
    .overlay = &*fixture.data,
    .redraw_cb = redraw,
    .session = (struct EditSession *)&fixture,
  };
}

static void check_grid(unsigned centre, unsigned writes)
{
  assert(fixture.writes == writes);
  assert(fixture.cells[1][1] == centre);

  for (size_t y = 0; y < GridSize; ++y)
  {
    for (size_t x = 0; x < GridSize; ++x)
    {
      if ((x == 1) && (y == 1))
      {
        continue;
      }
      assert(fixture.cells[y][x] == fixture.before[y][x]);
    }
  }
  assert(fixture.data);
  dfile_release(map_get_dfile(&*fixture.data));
  fixture.data = NULL;
}

static FILE *open_text(char const *text)
{
  _Optional FILE *const file = tmpfile();
  assert(file);
  assert(fputs(text, &*file) >= 0);
  rewind(&*file);
  return &*file;
}

static SFError read_text(MapTexGroups *groups, char const *text,
  char (*error)[MapTexGroupsErrBufferSize])
{
  FILE *const file = open_text(text);
  SFError const err = MapTexGroups_read(file, groups, TileCount, error);
  assert(fclose(file) == 0);
  return err;
}

static void load_text(MapTexGroups *groups, char const *text)
{
  char error[MapTexGroupsErrBufferSize];
  assert(!SFError_fail(read_text(groups, text, &error)));
  assert(error[0] == '\0');
}

static void expect_error(char const *text, SFErrorType type, char const *detail)
{
  MapTexGroups groups;
  MapTexGroups_init(&groups);
  char error[MapTexGroupsErrBufferSize];

  SFError const err = read_text(&groups, text, &error);

  assert(err.type == type);
  assert(strcmp(error, detail) == 0);
  MapTexGroups_free(&groups);
}

/* All sixteen combinations of two edge materials. Each tile's number
 * identifies the edge labels declared in the fixture, not a match score. */
static void load_patterns(MapTexGroups *groups)
{
  _Optional FILE *const file = tmpfile();
  assert(file);
  assert(fputs("StartGroup 0\n0:0,0,0,0,0\n", &*file) >= 0);

  for (unsigned part = 0; part < 16; ++part)
  {
    assert(fprintf(&*file, "%u:0,%d,%d,%d,%d\n", part + 1,
      (part & 1) != 0, (part & 2) != 0, (part & 4) != 0,
      (part & 8) != 0) > 0);
  }

  assert(fputs("19:1,0,0,0,0\nEndGroup\n"
    "StartGroup 1\n17:0,1,1,1,1\n18:0,0,0,0,0\nEndGroup\n", &*file) >= 0);
  rewind(&*file);
  char error[MapTexGroupsErrBufferSize];
  assert(!SFError_fail(MapTexGroups_read(&*file, groups, TileCount, &error)));
  assert(fclose(&*file) == 0);
}

static void edge_case(int candidate, int ideal, unsigned char expected)
{
  MapTexGroups groups;
  MapTexGroups_init(&groups);
  _Optional FILE *const file = tmpfile();
  assert(file);
  assert(fprintf(&*file, "StartGroup 0\n0:0,0,0,0,0\n"
    "1:0,%d,%d,%d,%d\nEndGroup\n"
    "StartGroup 1\n10:0,%d,%d,%d,%d\nEndGroup\n"
    "StartGroup 2\nEndGroup\n"
    "StartGroup 3\nSubGroup 1\nEndGroup\n"
    "StartGroup 4\nSubGroup 2\nEndGroup\n"
    "StartGroup 5\nSubGroup 1\nSubGroup 2\nEndGroup\n",
    candidate, candidate, candidate, candidate, ideal, ideal, ideal, ideal) > 0);
  rewind(&*file);
  char error[MapTexGroupsErrBufferSize];
  assert(!SFError_fail(MapTexGroups_read(&*file, &groups, TileCount, &error)));
  assert(fclose(&*file) == 0);
  init_grid(0, 10);

  smooth(&groups, NULL);

  check_grid(expected, expected == 0 ? 0 : 1);
  MapTexGroups_free(&groups);
}

static void test1(void)
{
  /* Initial texture group count */

  MapTexGroups groups;
  MapTexGroups_init(&groups);

  assert(MapTexGroups_get_count(&groups) == 0);

  MapTexGroups_free(&groups);
}

static void test2(void)
{
  /* Empty texture groups */

  MapTexGroups groups;
  MapTexGroups_init(&groups);

  load_text(&groups, "# Empty\n\n");

  assert(MapTexGroups_get_count(&groups) == 0);
  assert(MapTexGroups_get_group_of_tile(&groups, map_ref_from_num(0)) == UCHAR_MAX);
  MapTexGroups_free(&groups);
}

static void test3(void)
{
  /* Texture group membership */

  MapTexGroups groups;
  MapTexGroups_init(&groups);

  load_patterns(&groups);

  assert(MapTexGroups_get_count(&groups) == 2);
  assert(MapTexGroups_get_num_group_members(&groups, 0) == 18);
  for (int i = 0; i < 17; ++i)
  {
    MapRef const tile = MapTexGroups_get_group_member(&groups, 0, i);
    assert(map_ref_to_num(tile) == i);
    assert(MapTexGroups_get_group_of_tile(&groups, tile) == 0);
  }
  assert(map_ref_to_num(MapTexGroups_get_group_member(&groups, 0, 17)) == 19);
  assert(MapTexGroups_get_num_group_members(&groups, 1) == 2);
  assert(MapTexGroups_get_group_of_tile(&groups, map_ref_from_num(17)) == 1);
  assert(MapTexGroups_get_group_of_tile(&groups, map_ref_from_num(20)) == UCHAR_MAX);
  MapTexGroups_free(&groups);
}

static void test4(void)
{
  /* Undefined tiles join configured group */

  MapTexGroups groups;
  MapTexGroups_init(&groups);

  load_text(&groups, "UndefinedGroup 1\nStartGroup 0\n5:0,0,0,0,0\nEndGroup\n");

  assert(MapTexGroups_get_count(&groups) == 2);
  assert(MapTexGroups_get_num_group_members(&groups, 1) == TileCount - 1);
  int member = 0;
  for (unsigned char tile = 0; tile < TileCount; ++tile)
  {
    if (tile == 5)
    {
      assert(MapTexGroups_get_group_of_tile(&groups, map_ref_from_num(tile)) == 0);
      continue;
    }
    assert(MapTexGroups_get_group_of_tile(&groups, map_ref_from_num(tile)) == 1);
    assert(map_ref_to_num(MapTexGroups_get_group_member(&groups, 1, member)) == tile);
    ++member;
  }
  MapTexGroups_free(&groups);
}

static void test5(void)
{
  /* Reload replaces texture groups */

  MapTexGroups groups;
  MapTexGroups_init(&groups);
  load_patterns(&groups);

  load_text(&groups, "StartGroup 0\n7:0,0,0,0,0\nEndGroup\n");

  assert(MapTexGroups_get_count(&groups) == 1);
  assert(MapTexGroups_get_num_group_members(&groups, 0) == 1);
  assert(map_ref_to_num(MapTexGroups_get_group_member(&groups, 0, 0)) == 7);
  assert(MapTexGroups_get_group_of_tile(&groups, map_ref_from_num(0)) == UCHAR_MAX);
  MapTexGroups_free(&groups);
}

static void test6(void)
{
  /* Texture group whitespace */

  MapTexGroups groups;
  MapTexGroups_init(&groups);

  load_text(&groups, "# Comment\n\nStartGroup 0  \t\n3:0,0,0,0,0\nEndGroup   \t");

  assert(MapTexGroups_get_num_group_members(&groups, 0) == 1);
  assert(map_ref_to_num(MapTexGroups_get_group_member(&groups, 0, 0)) == 3);
  MapTexGroups_free(&groups);
}

static void test7(void)
{
  /* Unmatched group terminator */

  expect_error("StartGroup 0\nEndGroup\nEndGroup\n", SFErrorType_Unexp, "3");
}

static void test8(void)
{
  /* Nested texture group */

  expect_error("StartGroup 0\nEndGroup\nStartGroup 0\nStartGroup 0\n", SFErrorType_Unexp, "4");
}

static void test9(void)
{
  /* Missing texture group number */

  expect_error("StartGroup 0\nEndGroup\nStartGroup\n", SFErrorType_Mistake, "3");
}

static void test10(void)
{
  /* Out-of-range texture group */

  expect_error("StartGroup 0\nEndGroup\nStartGroup 255\n", SFErrorType_GroupRange, "3");
}

static void test11(void)
{
  /* Missing undefined group number */

  expect_error("StartGroup 0\nEndGroup\nUndefinedGroup\n", SFErrorType_Mistake, "3");
}

static void test12(void)
{
  /* Out-of-range undefined group */

  expect_error("StartGroup 0\nEndGroup\nUndefinedGroup 255\n", SFErrorType_GroupRange, "3");
}

static void test13(void)
{
  /* Undefined group inside definition */

  expect_error("StartGroup 0\nEndGroup\nStartGroup 0\nUndefinedGroup 0\n", SFErrorType_Unexp, "4");
}

static void test14(void)
{
  /* Subgroup outside definition */

  expect_error("StartGroup 0\nEndGroup\nSubGroup 0\n", SFErrorType_Unexp, "3");
}

static void test15(void)
{
  /* Missing subgroup number */

  expect_error("StartGroup 0\nEndGroup\nStartGroup 0\nSubGroup\n", SFErrorType_Mistake, "4");
}

static void test16(void)
{
  /* Out-of-range subgroup */

  expect_error("StartGroup 0\nEndGroup\nStartGroup 0\nSubGroup 255\n", SFErrorType_GroupRange, "4");
}

static void test17(void)
{
  /* Subgroup after tile */

  expect_error("StartGroup 0\nEndGroup\nStartGroup 0\n1:0,0,0,0,0\nSubGroup 0\n", SFErrorType_MixMem, "5");
}

static void test18(void)
{
  /* Tile after subgroup */

  expect_error("StartGroup 0\nEndGroup\nStartGroup 0\nSubGroup 0\n1:0,0,0,0,0\n", SFErrorType_MixMem, "5");
}

static void test19(void)
{
  /* Tile outside group */

  expect_error("StartGroup 0\nEndGroup\n1:0,0,0,0,0\n", SFErrorType_Mistake, "3");
}

static void test20(void)
{
  /* Incomplete smoothing tile */

  expect_error("StartGroup 0\nEndGroup\nStartGroup 0\n1:0,0\n", SFErrorType_Mistake, "4");
}

static void test21(void)
{
  /* Invalid smoothing lock flag */

  expect_error("StartGroup 0\nEndGroup\nStartGroup 0\n1:2,0,0,0,0\n", SFErrorType_Mistake, "4");
}

static void test22(void)
{
  /* Out-of-range smoothing tile */

  expect_error("StartGroup 0\nEndGroup\nStartGroup 0\n32:0,0,0,0,0\n", SFErrorType_NumRange, "4");
}

static void test23(void)
{
  /* Missing group terminator */

  expect_error("StartGroup 0\nEndGroup\nStartGroup 0\n", SFErrorType_EOF, "EndGroup\n");
}

static void test24(void)
{
  /* Out-of-range north edge */

  expect_error("StartGroup 0\nEndGroup\nStartGroup 0\n1:0,1,0,0,0\n", SFErrorType_GroupRange, "4");
}

static void test25(void)
{
  /* Out-of-range east edge */

  expect_error("StartGroup 0\nEndGroup\nStartGroup 0\n1:0,0,1,0,0\n", SFErrorType_GroupRange, "4");
}

static void test26(void)
{
  /* Out-of-range south edge */

  expect_error("StartGroup 0\nEndGroup\nStartGroup 0\n1:0,0,0,1,0\n", SFErrorType_GroupRange, "4");
}

static void test27(void)
{
  /* Out-of-range west edge */

  expect_error("StartGroup 0\nEndGroup\nStartGroup 0\n1:0,0,0,0,1\n", SFErrorType_GroupRange, "4");
}

static void test28(void)
{
  /* Smoothing edge combinations */

  MapTexGroups groups;
  MapTexGroups_init(&groups);
  load_patterns(&groups);
  static const MapPoint neighbours[] = {{1, 2}, {2, 1}, {1, 0}, {0, 1}};

  for (unsigned part = 0; part < 16; ++part)
  {
    init_grid(0, 18);
    for (size_t i = 0; i < ARRAY_SIZE(neighbours); ++i)
    {
      MapPoint const pos = neighbours[i];
      fixture.cells[pos.y][pos.x] = (part & (1u << i)) != 0 ? 17 : 18;
    }
    memcpy(fixture.before, fixture.cells, sizeof(fixture.before));

    smooth(&groups, NULL);

    check_grid(part == 0 ? 0 : part + 1, part == 0 ? 0 : 1);
  }
  MapTexGroups_free(&groups);
}

static void test29(void)
{
  /* Perfect tile remains unchanged */

  MapTexGroups groups;
  MapTexGroups_init(&groups);
  load_patterns(&groups);
  init_grid(16, 17);

  smooth(&groups, NULL);

  check_grid(16, 0);
  MapTexGroups_free(&groups);
}

static void test30(void)
{
  /* Locked tile remains unchanged */

  MapTexGroups groups;
  MapTexGroups_init(&groups);
  load_patterns(&groups);
  init_grid(19, 17);

  smooth(&groups, NULL);

  check_grid(19, 0);
  MapTexGroups_free(&groups);
}

static void test31(void)
{
  /* Undefined tile remains unchanged */

  MapTexGroups groups;
  MapTexGroups_init(&groups);
  load_patterns(&groups);
  init_grid(20, 17);

  smooth(&groups, NULL);

  check_grid(20, 0);
  MapTexGroups_free(&groups);
}

static void test32(void)
{
  /* Masked centre remains unchanged */

  MapTexGroups groups;
  MapTexGroups_init(&groups);
  load_patterns(&groups);
  init_grid(255, 17);

  smooth(&groups, NULL);

  check_grid(255, 0);
  MapTexGroups_free(&groups);
}

static void test33(void)
{
  /* Out-of-range centre remains unchanged */

  MapTexGroups groups;
  MapTexGroups_init(&groups);
  load_patterns(&groups);
  init_grid(100, 17);

  smooth(&groups, NULL);

  check_grid(100, 0);
  MapTexGroups_free(&groups);
}

static void test34(void)
{
  /* Masked neighbours allow fuzzy match */

  MapTexGroups groups;
  MapTexGroups_init(&groups);
  load_patterns(&groups);
  init_grid(0, 255);

  smooth(&groups, NULL);

  check_grid(0, 0);
  MapTexGroups_free(&groups);
}

static void test35(void)
{
  /* Out-of-range neighbours allow fuzzy match */

  MapTexGroups groups;
  MapTexGroups_init(&groups);
  load_patterns(&groups);
  init_grid(0, 100);

  smooth(&groups, NULL);

  check_grid(0, 0);
  MapTexGroups_free(&groups);
}

static void test36(void)
{
  /* Smoothing without definitions */

  MapTexGroups groups;
  MapTexGroups_init(&groups);
  init_grid(0, 17);

  smooth(&groups, NULL);

  check_grid(0, 0);
  MapTexGroups_free(&groups);
}

static void test37(void)
{
  /* Empty definitions prevent smoothing */

  MapTexGroups groups;
  MapTexGroups_init(&groups);
  load_text(&groups, "");
  init_grid(0, 17);

  smooth(&groups, NULL);

  check_grid(0, 0);
  MapTexGroups_free(&groups);
}

static void test38(void)
{
  /* Candidate subgroup match */

  edge_case(3, 1, 1);
}

static void test39(void)
{
  /* Neighbour subgroup match */

  edge_case(1, 3, 1);
}

static void test40(void)
{
  /* Overlapping supergroups match */

  edge_case(3, 5, 1);
}

static void test41(void)
{
  /* Disjoint supergroups avoid replacement */

  edge_case(3, 4, 0);
}

static void test42(void)
{
  /* Candidate subgroup mismatch */

  edge_case(3, 2, 0);
}

static void test43(void)
{
  /* Neighbour subgroup mismatch */

  edge_case(2, 3, 0);
}

static void test44(void)
{
  /* Unrelated edge avoids replacement */

  edge_case(2, 1, 0);
}

static void test45(void)
{
  /* Wildcard candidate improves mismatch */

  edge_case(255, 1, 1);
}

static void test46(void)
{
  /* Replacement stays in centre group */

  MapTexGroups groups;
  MapTexGroups_init(&groups);
  load_text(&groups, "StartGroup 0\n0:0,0,0,0,0\nEndGroup\n"
    "StartGroup 1\n1:0,1,1,1,1\nEndGroup\n");
  init_grid(0, 1);

  smooth(&groups, NULL);

  check_grid(0, 0);
  MapTexGroups_free(&groups);
}

static void test47(void)
{
  /* Smoothing forwards change counter */

  MapTexGroups groups;
  MapTexGroups_init(&groups);
  load_patterns(&groups);
  init_grid(0, 17);
  MapEditChanges changes;
  MapEditChanges_init(&changes);

  smooth(&groups, &changes);

  check_grid(16, 1);
  assert(changes.tiles_changed == 1);
  MapTexGroups_free(&groups);
}

static void test48(void)
{
  /* Supergroups have no tile members */

  MapTexGroups groups;
  MapTexGroups_init(&groups);

  load_text(&groups, "StartGroup 0\nEndGroup\nStartGroup 1\nSubGroup 0\nEndGroup\n");

  assert(MapTexGroups_get_num_group_members(&groups, 1) == 0);
  MapTexGroups_free(&groups);
}

static void test49(void)
{
  /* Highest texture group number */

  MapTexGroups groups;
  MapTexGroups_init(&groups);

  load_text(&groups, "StartGroup 254\n31:0,254,254,254,254\nEndGroup\n");

  assert(MapTexGroups_get_count(&groups) == 255);
  assert(MapTexGroups_get_group_of_tile(&groups, map_ref_from_num(31)) == 254);
  assert(MapTexGroups_get_num_group_members(&groups, 254) == 1);
  assert(map_ref_to_num(MapTexGroups_get_group_member(&groups, 254, 0)) == 31);
  MapTexGroups_free(&groups);
}

static void test50(void)
{
  /* Exact edges preferred over wildcards */

  MapTexGroups groups;
  MapTexGroups_init(&groups);
  load_text(&groups, "StartGroup 0\n0:0,0,0,0,0\n1:0,255,255,255,255\n"
    "2:0,1,1,1,1\nEndGroup\nStartGroup 1\n10:0,1,1,1,1\nEndGroup\n");
  init_grid(0, 10);

  smooth(&groups, NULL);

  check_grid(2, 1);
  MapTexGroups_free(&groups);
}

static void test51(void)
{
  /* Continuation preferred over wrong material */

  MapTexGroups groups;
  MapTexGroups_init(&groups);
  load_text(&groups, "StartGroup 0\n0:0,2,2,2,2\n1:0,0,0,0,0\nEndGroup\n"
    "StartGroup 1\n10:0,1,1,1,1\nEndGroup\nStartGroup 2\nEndGroup\n");
  init_grid(0, 10);

  smooth(&groups, NULL);

  check_grid(1, 1);
  MapTexGroups_free(&groups);
}

static void test52(void)
{
  /* Equally suitable replacements */

  MapTexGroups groups;
  MapTexGroups_init(&groups);
  load_text(&groups, "StartGroup 0\n0:0,0,0,0,0\n1:0,1,1,1,1\n"
    "2:0,1,1,1,1\nEndGroup\nStartGroup 1\n10:0,1,1,1,1\nEndGroup\n");
  init_grid(0, 10);

  smooth(&groups, NULL);

  unsigned char const replacement = fixture.cells[1][1];
  assert(replacement >= 1);
  assert(replacement <= 2);
  check_grid(replacement, 1);
  MapTexGroups_free(&groups);
}

#ifdef FORTIFY
static void test53(void)
{
  /* Smoothing allocation failures remain destructible */

  bool complete = false;

  for (unsigned long limit = 0; limit < 16; ++limit)
  {
    MapTexGroups groups;
    MapTexGroups_init(&groups);
    FILE *const file = open_text("UndefinedGroup 0\nStartGroup 0\n0:0,0,0,0,0\nEndGroup\n");
    char error[MapTexGroupsErrBufferSize];
    Fortify_SetNumAllocationsLimit(limit);

    SFError const err = MapTexGroups_read(file, &groups, TileCount, &error);

    Fortify_SetNumAllocationsLimit(ULONG_MAX);
    if (!SFError_fail(err))
    {
      complete = true;
      assert(MapTexGroups_get_num_group_members(&groups, 0) == TileCount);
    }
    else
    {
      switch (err.type)
      {
        case SFErrorType_NoMem:
        case SFErrorType_BadSeek:
          break;
        default:
          assert(!"Unexpected load error");
          break;
      }
    }
    assert(fclose(file) == 0);
    MapTexGroups_free(&groups);
    if (complete)
    {
      break;
    }
  }
  assert(complete);
}

#endif

void Smooth_tests(void)
{
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Initial texture group count", test1 },
    { "Empty texture groups", test2 },
    { "Texture group membership", test3 },
    { "Undefined tiles join configured group", test4 },
    { "Reload replaces texture groups", test5 },
    { "Texture group whitespace", test6 },
    { "Unmatched group terminator", test7 },
    { "Nested texture group", test8 },
    { "Missing texture group number", test9 },
    { "Out-of-range texture group", test10 },
    { "Missing undefined group number", test11 },
    { "Out-of-range undefined group", test12 },
    { "Undefined group inside definition", test13 },
    { "Subgroup outside definition", test14 },
    { "Missing subgroup number", test15 },
    { "Out-of-range subgroup", test16 },
    { "Subgroup after tile", test17 },
    { "Tile after subgroup", test18 },
    { "Tile outside group", test19 },
    { "Incomplete smoothing tile", test20 },
    { "Invalid smoothing lock flag", test21 },
    { "Out-of-range smoothing tile", test22 },
    { "Missing group terminator", test23 },
    { "Out-of-range north edge", test24 },
    { "Out-of-range east edge", test25 },
    { "Out-of-range south edge", test26 },
    { "Out-of-range west edge", test27 },
    { "Smoothing edge combinations", test28 },
    { "Perfect tile remains unchanged", test29 },
    { "Locked tile remains unchanged", test30 },
    { "Undefined tile remains unchanged", test31 },
    { "Masked centre remains unchanged", test32 },
    { "Out-of-range centre remains unchanged", test33 },
    { "Masked neighbours allow fuzzy match", test34 },
    { "Out-of-range neighbours allow fuzzy match", test35 },
    { "Smoothing without definitions", test36 },
    { "Empty definitions prevent smoothing", test37 },
    { "Candidate subgroup match", test38 },
    { "Neighbour subgroup match", test39 },
    { "Overlapping supergroups match", test40 },
    { "Disjoint supergroups avoid replacement", test41 },
    { "Candidate subgroup mismatch", test42 },
    { "Neighbour subgroup mismatch", test43 },
    { "Unrelated edge avoids replacement", test44 },
    { "Wildcard candidate improves mismatch", test45 },
    { "Replacement stays in centre group", test46 },
    { "Smoothing forwards change counter", test47 },
    { "Supergroups have no tile members", test48 },
    { "Highest texture group number", test49 },
    { "Exact edges preferred over wildcards", test50 },
    { "Continuation preferred over wrong material", test51 },
    { "Equally suitable replacements", test52 },
#ifdef FORTIFY
    { "Smoothing allocation failures remain destructible", test53 },
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
