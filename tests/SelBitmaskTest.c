/* SFEditor test: selection bitmask.
 * Copyright (C) 2026 Christopher Bazley
 */
#undef NDEBUG

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>

#include "Macros.h"
#include "Debug.h"
#include "Tests.h"
#include "../SelBitmask.h"

enum { BitCount = sizeof(SelBitmaskType) * CHAR_BIT };

typedef struct
{
  size_t count;
  size_t indices[BitCount];
} IndexLog;

static void record_index(size_t index, void *arg)
{
  IndexLog *const log = arg;
  assert(log->count < ARRAY_SIZE(log->indices));
  log->indices[log->count++] = index;
}

static void check_log(IndexLog const *log, size_t const expected[], size_t count)
{
  assert(log->count == count);
  for (size_t i = 0; i < count; ++i)
  {
    assert(log->indices[i] == expected[i]);
  }
}

static void check_selection(SelectionBitmask const *selection,
  bool const expected[], size_t count)
{
  assert(selection->num_objects == count);
  size_t selected = 0;
  for (size_t i = 0; i < count; ++i)
  {
    assert(SelectionBitmask_is_selected(selection, i) == expected[i]);
    if (expected[i])
    {
      ++selected;
    }
  }
  assert(SelectionBitmask_size(selection) == selected);
  assert(SelectionBitmask_is_none(selection) == (selected == 0));
  assert(SelectionBitmask_is_all(selection) == (selected == count));
  for (size_t i = count; i < BitCount; ++i)
  {
    assert((selection->bitmask & ((SelBitmaskType)1 << i)) == 0);
  }
}

static void make_selection(SelectionBitmask *selection, size_t count)
{
  SelectionBitmask_init(selection, count, NULL, selection);
  for (size_t i = 0; i < count; ++i)
  {
    if (i % 3 == 1 || i + 1 == count)
    {
      SelectionBitmask_select(selection, i);
    }
  }
}

static void check_iterator(SelectionBitmask *selection,
  size_t const expected[], size_t count)
{
  SelectionBitmaskIter iter;
  size_t visited = 0;
  for (size_t index = SelectionBitmaskIter_get_first(&iter, selection);
       !SelectionBitmaskIter_done(&iter);
       index = SelectionBitmaskIter_get_next(&iter))
  {
    assert(visited < count);
    assert(index == expected[visited]);
    ++visited;
  }
  assert(visited == count);
}

/* Moving consumes the current selection. Model the index changes with arrays
 * so the expected result does not depend on the implementation's bit shifts. */
static void check_move(int direction)
{
  static bool const original[] = {true, true, false, true, false, true, true};
  for (size_t current = 0; current < ARRAY_SIZE(original); ++current)
  {
    if (!original[current])
    {
      continue;
    }
    for (size_t destination = 0; destination < ARRAY_SIZE(original); ++destination)
    {
      int const actual_direction = destination < current ? -1 : destination > current;
      if (actual_direction != direction)
      {
        continue;
      }
      DEBUGF("Move %zu to %zu\n", current, destination);
      SelectionBitmask selection;
      SelectionBitmask_init(&selection, ARRAY_SIZE(original), NULL, &selection);
      for (size_t i = 0; i < ARRAY_SIZE(original); ++i)
      {
        if (original[i])
        {
          SelectionBitmask_select(&selection, i);
        }
      }
      SelectionBitmaskIter iter;
      size_t index = SelectionBitmaskIter_get_first(&iter, &selection);
      while (index != current)
      {
        assert(!SelectionBitmaskIter_done(&iter));
        index = SelectionBitmaskIter_get_next(&iter);
      }
      bool expected[ARRAY_SIZE(original)] = {false};
      bool remaining[ARRAY_SIZE(original)] = {false};
      for (size_t i = 0; i < ARRAY_SIZE(original); ++i)
      {
        if (i == current)
        {
          continue;
        }
        size_t mapped = i < current ? i : i - 1;
        if (mapped >= destination)
        {
          ++mapped;
        }
        expected[mapped] = original[i];
        remaining[mapped] = i > current ? original[i] : false;
      }
      SelectionBitmaskIter_move_current(&iter, destination);
      check_selection(&selection, expected, ARRAY_SIZE(expected));
      IndexLog visited = {0};
      for (index = SelectionBitmaskIter_get_next(&iter);
           !SelectionBitmaskIter_done(&iter);
           index = SelectionBitmaskIter_get_next(&iter))
      {
        record_index(index, &visited);
      }
      size_t next = 0;
      for (size_t i = 0; i < ARRAY_SIZE(remaining); ++i)
      {
        if (remaining[i])
        {
          assert(next < visited.count);
          assert(visited.indices[next] == i);
          ++next;
        }
      }
      assert(visited.count == next);
    }
  }
}

static void test1(void)
{
  /* Initialise */
  for (size_t count = 0; count <= BitCount; ++count)
  {
    SelectionBitmask selection;
    SelectionBitmask_init(&selection, count, NULL, &selection);
    bool const expected[BitCount] = {false};
    check_selection(&selection, expected, count);
  }
}

static void test2(void)
{
  /* Select each bit */
  for (size_t index = 0; index < BitCount; ++index)
  {
    SelectionBitmask selection;
    SelectionBitmask_init(&selection, BitCount, NULL, &selection);
    SelectionBitmask_select(&selection, index);
    bool expected[BitCount] = {false};
    expected[index] = true;
    check_selection(&selection, expected, BitCount);
  }
}

static void test3(void)
{
  /* Select twice */
  IndexLog log = {0};
  SelectionBitmask selection;
  SelectionBitmask_init(&selection, BitCount, record_index, &log);
  SelectionBitmask_select(&selection, 7);
  SelectionBitmask_select(&selection, 7);
  assert(SelectionBitmask_size(&selection) == 1);
  assert(log.count == 1);
}

static void test4(void)
{
  /* Deselect */
  SelectionBitmask selection;
  SelectionBitmask_init(&selection, BitCount, NULL, &selection);
  SelectionBitmask_select(&selection, 7);
  SelectionBitmask_select(&selection, BitCount - 1);
  SelectionBitmask_deselect(&selection, 7);
  bool expected[BitCount] = {false};
  expected[BitCount - 1] = true;
  check_selection(&selection, expected, BitCount);
}

static void test5(void)
{
  /* Deselect unselected bit */
  IndexLog log = {0};
  SelectionBitmask selection;
  SelectionBitmask_init(&selection, BitCount, record_index, &log);
  SelectionBitmask_deselect(&selection, 7);
  assert(SelectionBitmask_is_none(&selection));
  assert(log.count == 0);
}

static void test6(void)
{
  /* Invert unselected bit */
  SelectionBitmask selection;
  SelectionBitmask_init(&selection, BitCount, NULL, &selection);
  SelectionBitmask_invert(&selection, 7, true);
  bool expected[BitCount] = {false};
  expected[7] = true;
  check_selection(&selection, expected, BitCount);
}

static void test7(void)
{
  /* Invert selected bit */
  SelectionBitmask selection;
  SelectionBitmask_init(&selection, BitCount, NULL, &selection);
  SelectionBitmask_select(&selection, 7);
  SelectionBitmask_invert(&selection, 7, true);
  bool expected[BitCount] = {false};
  check_selection(&selection, expected, BitCount);
}

static void test8(void)
{
  /* Clear */
  SelectionBitmask selection;
  SelectionBitmask_init(&selection, BitCount, NULL, &selection);
  SelectionBitmask_select(&selection, 0);
  SelectionBitmask_select(&selection, BitCount - 1);
  SelectionBitmask_clear(&selection);
  bool const expected[BitCount] = {false};
  check_selection(&selection, expected, BitCount);
}

static void test9(void)
{
  /* Clear empty selection */
  IndexLog log = {0};
  SelectionBitmask selection;
  SelectionBitmask_init(&selection, BitCount, record_index, &log);
  SelectionBitmask_clear(&selection);
  assert(SelectionBitmask_is_none(&selection));
  assert(log.count == 0);
}

static void test10(void)
{
  /* Select all */
  for (size_t count = 0; count < BitCount; ++count)
  {
    SelectionBitmask selection;
    SelectionBitmask_init(&selection, count, NULL, &selection);
    SelectionBitmask_select_all(&selection);
    bool expected[BitCount] = {false};
    for (size_t i = 0; i < count; ++i)
    {
      expected[i] = true;
    }
    check_selection(&selection, expected, count);
  }
}

static void test11(void)
{
  /* Select full-capacity bitset */
  SelectionBitmask selection;
  SelectionBitmask_init(&selection, BitCount, NULL, &selection);
  SelectionBitmask_select_all(&selection);
  assert(SelectionBitmask_size(&selection) == BitCount);
  for (size_t i = 0; i < BitCount; ++i)
  {
    assert(SelectionBitmask_is_selected(&selection, i));
  }
}

static void test12(void)
{
  /* Select all twice */
  IndexLog log = {0};
  SelectionBitmask selection;
  SelectionBitmask_init(&selection, BitCount, record_index, &log);
  SelectionBitmask_select_all(&selection);
  log = (IndexLog){0};
  SelectionBitmask_select_all(&selection);
  assert(log.count == 0);
}

static void test13(void)
{
  /* Selection redraw */
  IndexLog log = {0};
  SelectionBitmask selection;
  SelectionBitmask_init(&selection, BitCount, record_index, &log);
  SelectionBitmask_select(&selection, 7);
  static size_t const expected[] = {7};
  check_log(&log, expected, ARRAY_SIZE(expected));
}

static void test14(void)
{
  /* Deselection redraw */
  IndexLog log = {0};
  SelectionBitmask selection;
  SelectionBitmask_init(&selection, BitCount, record_index, &log);
  SelectionBitmask_select(&selection, 7);
  log = (IndexLog){0};
  SelectionBitmask_deselect(&selection, 7);
  static size_t const expected[] = {7};
  check_log(&log, expected, ARRAY_SIZE(expected));
}

static void test15(void)
{
  /* Clear redraw */
  IndexLog log = {0};
  SelectionBitmask selection;
  SelectionBitmask_init(&selection, BitCount, record_index, &log);
  SelectionBitmask_select(&selection, 7);
  SelectionBitmask_select(&selection, BitCount - 1);
  log = (IndexLog){0};
  SelectionBitmask_clear(&selection);
  static size_t const expected[] = {7, BitCount - 1};
  check_log(&log, expected, ARRAY_SIZE(expected));
}

static void test16(void)
{
  /* Inversion redraw */
  IndexLog log = {0};
  SelectionBitmask selection;
  SelectionBitmask_init(&selection, BitCount, record_index, &log);
  SelectionBitmask_invert(&selection, 7, true);
  assert(log.count == 1);
  assert(log.indices[0] == 7);
}

static void test17(void)
{
  /* Suppress inversion redraw */
  IndexLog log = {0};
  SelectionBitmask selection;
  SelectionBitmask_init(&selection, BitCount, record_index, &log);
  SelectionBitmask_invert(&selection, 7, false);
  assert(log.count == 0);
}

static void test18(void)
{
  /* Insert object */
  for (size_t count = 0; count < BitCount; ++count)
  {
    for (size_t index = 0; index <= count; ++index)
    {
      DEBUGF("Insert at %zu of %zu\n", index, count);
      SelectionBitmask selection;
      make_selection(&selection, count);
      bool expected[BitCount] = {false};
      for (size_t i = 0; i < count; ++i)
      {
        expected[i < index ? i : i + 1] = i % 3 == 1 || i + 1 == count;
      }
      SelectionBitmask_obj_inserted(&selection, index);
      check_selection(&selection, expected, count + 1);
    }
  }
}

static void test19(void)
{
  /* Delete object */
  for (size_t count = 1; count <= BitCount; ++count)
  {
    for (size_t index = 0; index < count; ++index)
    {
      DEBUGF("Delete at %zu of %zu\n", index, count);
      SelectionBitmask selection;
      make_selection(&selection, count);
      bool expected[BitCount] = {false};
      for (size_t i = 0; i < count; ++i)
      {
        if (i != index)
        {
          expected[i < index ? i : i - 1] = i % 3 == 1 || i + 1 == count;
        }
      }
      SelectionBitmask_obj_deleted(&selection, index);
      check_selection(&selection, expected, count - 1);
    }
  }
}

static void test20(void)
{
  /* Copy selection */
  SelectionBitmask a, b;
  make_selection(&a, BitCount);
  SelectionBitmask_init(&b, 3, NULL, &b);
  SelectionBitmask_copy(&b, &a);
  bool expected[BitCount] = {false};
  for (size_t i = 0; i < ARRAY_SIZE(expected); ++i)
  {
    expected[i] = i % 3 == 1 || i + 1 == ARRAY_SIZE(expected);
  }
  check_selection(&b, expected, BitCount);
}

static void test21(void)
{
  /* Copy independence */
  SelectionBitmask a, b;
  make_selection(&a, BitCount);
  SelectionBitmask_init(&b, 3, NULL, &b);
  SelectionBitmask_copy(&b, &a);
  SelectionBitmask_clear(&b);
  assert(SelectionBitmask_size(&a) == BitCount / 3 + (BitCount % 3 > 1));
}

static void test22(void)
{
  /* Copy callback */
  IndexLog a_log = {0}, b_log = {0};
  SelectionBitmask a, b;
  SelectionBitmask_init(&a, BitCount, NULL, &a_log);
  SelectionBitmask_init(&b, 3, record_index, &b_log);
  SelectionBitmask_copy(&b, &a);
  SelectionBitmask_select(&b, BitCount - 1);
  assert(a_log.count == 0);
  assert(b_log.count == 1);
  assert(b_log.indices[0] == BitCount - 1);
}

static void test23(void)
{
  /* Empty iterator */
  SelectionBitmask selection;
  SelectionBitmask_init(&selection, BitCount, NULL, &selection);
  size_t const expected[] = {0};
  check_iterator(&selection, expected, 0);
}

static void test24(void)
{
  /* Iterate */
  SelectionBitmask selection;
  SelectionBitmask_init(&selection, BitCount, NULL, &selection);
  static size_t const expected[] = {0, 7, 8, BitCount - 1};
  for (size_t i = 0; i < ARRAY_SIZE(expected); ++i)
  {
    SelectionBitmask_select(&selection, expected[i]);
  }
  check_iterator(&selection, expected, ARRAY_SIZE(expected));
}

static void test25(void)
{
  /* Iterate full selection */
  SelectionBitmask selection;
  SelectionBitmask_init(&selection, BitCount, NULL, &selection);
  SelectionBitmask_select_all(&selection);
  size_t expected[BitCount];
  for (size_t i = 0; i < ARRAY_SIZE(expected); ++i)
  {
    expected[i] = i;
  }
  check_iterator(&selection, expected, ARRAY_SIZE(expected));
}

static void test26(void)
{
  /* Visit selection */
  SelectionBitmask selection;
  SelectionBitmask_init(&selection, BitCount, NULL, &selection);
  SelectionBitmask_select(&selection, 7);
  SelectionBitmask_select(&selection, BitCount - 1);
  IndexLog log = {0};
  assert(SelectionBitmask_for_each(&selection, record_index, &log));
  static size_t const expected[] = {7, BitCount - 1};
  check_log(&log, expected, ARRAY_SIZE(expected));
}

static void test27(void)
{
  /* Visit empty selection */
  SelectionBitmask selection;
  SelectionBitmask_init(&selection, BitCount, NULL, &selection);
  IndexLog log = {0};
  assert(!SelectionBitmask_for_each(&selection, record_index, &log));
  assert(log.count == 0);
}

static void test28(void)
{
  /* Enumerate changes */
  SelectionBitmask a, b;
  SelectionBitmask_init(&a, BitCount, NULL, &a);
  SelectionBitmask_init(&b, BitCount, NULL, &b);
  IndexLog log = {0};
  SelectionBitmask_select(&a, 0);
  SelectionBitmask_select(&a, 7);
  SelectionBitmask_select(&b, 7);
  SelectionBitmask_select(&b, BitCount - 1);
  assert(SelectionBitmask_for_each_changed(&a, &b, record_index, &log));
  static size_t const expected[] = {0, BitCount - 1};
  check_log(&log, expected, ARRAY_SIZE(expected));
}

static void test29(void)
{
  /* Unchanged empty selections */
  SelectionBitmask a, b;
  SelectionBitmask_init(&a, BitCount, NULL, &a);
  SelectionBitmask_init(&b, BitCount, NULL, &b);
  IndexLog log = {0};
  assert(!SelectionBitmask_for_each_changed(&a, &b, record_index, &log));
  assert(log.count == 0);
}

static void test30(void)
{
  /* Unchanged full selections */
  SelectionBitmask a, b;
  SelectionBitmask_init(&a, BitCount, NULL, &a);
  SelectionBitmask_init(&b, BitCount, NULL, &b);
  IndexLog log = {0};
  SelectionBitmask_select_all(&a);
  SelectionBitmask_select_all(&b);
  assert(!SelectionBitmask_for_each_changed(&a, &b, record_index, &log));
  assert(log.count == 0);
}

static void test31(void)
{
  /* Unchanged sparse selections */
  SelectionBitmask a, b;
  SelectionBitmask_init(&a, BitCount, NULL, &a);
  SelectionBitmask_init(&b, BitCount, NULL, &b);
  IndexLog log = {0};
  SelectionBitmask_select(&a, 7);
  SelectionBitmask_select(&b, 7);
  assert(!SelectionBitmask_for_each_changed(&a, &b, record_index, &log));
  assert(log.count == 0);
}

static void test32(void)
{
  /* Delete current item */
  SelectionBitmask selection;
  SelectionBitmask_init(&selection, 6, NULL, &selection);
  SelectionBitmask_select(&selection, 0);
  SelectionBitmask_select(&selection, 2);
  SelectionBitmask_select(&selection, 4);
  SelectionBitmaskIter iter;
  IndexLog visited = {0};
  for (size_t index = SelectionBitmaskIter_get_first(&iter, &selection);
       !SelectionBitmaskIter_done(&iter);
       index = SelectionBitmaskIter_get_next(&iter))
  {
    record_index(index, &visited);
    SelectionBitmaskIter_del_current(&iter);
  }
  static size_t const expected[] = {0, 1, 2};
  check_log(&visited, expected, ARRAY_SIZE(expected));
  assert(selection.num_objects == 3);
  assert(SelectionBitmask_is_none(&selection));
}

static void test33(void)
{
  /* Move current earlier */
  check_move(-1);
}

static void test34(void)
{
  /* Move current later */
  check_move(1);
}

static void test35(void)
{
  /* Move current in place */
  check_move(0);
}

void SelBitmask_tests(void)
{
  /* SelBitmask */
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Initialise", test1 },
    { "Select each bit", test2 },
    { "Select twice", test3 },
    { "Deselect", test4 },
    { "Deselect unselected bit", test5 },
    { "Invert unselected bit", test6 },
    { "Invert selected bit", test7 },
    { "Clear", test8 },
    { "Clear empty selection", test9 },
    { "Select all", test10 },
    { "Select full-capacity bitset", test11 },
    { "Select all twice", test12 },
    { "Selection redraw", test13 },
    { "Deselection redraw", test14 },
    { "Clear redraw", test15 },
    { "Inversion redraw", test16 },
    { "Suppress inversion redraw", test17 },
    { "Insert object", test18 },
    { "Delete object", test19 },
    { "Copy selection", test20 },
    { "Copy independence", test21 },
    { "Copy callback", test22 },
    { "Empty iterator", test23 },
    { "Iterate", test24 },
    { "Iterate full selection", test25 },
    { "Visit selection", test26 },
    { "Visit empty selection", test27 },
    { "Enumerate changes", test28 },
    { "Unchanged empty selections", test29 },
    { "Unchanged full selections", test30 },
    { "Unchanged sparse selections", test31 },
    { "Delete current item", test32 },
    { "Move current earlier", test33 },
    { "Move current later", test34 },
    { "Move current in place", test35 },
  };

  for (size_t count = 0; count < ARRAY_SIZE(unit_tests); ++count)
  {
    DEBUGF("Test %zu/%zu : %s\n",
           1 + count,
           ARRAY_SIZE(unit_tests),
           unit_tests[count].test_name);

    Fortify_EnterScope();
    unit_tests[count].test_func();
    Fortify_LeaveScope();
  }
}
