/* SFEditor test: polygonal object vertex arithmetic.
 * Copyright (C) 2026 Christopher Bazley
 */
#undef NDEBUG

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "Macros.h"
#include "Debug.h"
#include "Reader.h"
#include "ReaderMem.h"
#include "Tests.h"
#include "../SFError.h"
#include "../ObjVertex.h"

static void check_vertex(Vertex3D actual, Vertex3D expected)
{
  assert(actual.x == expected.x);
  assert(actual.y == expected.y);
  assert(actual.z == expected.z);
}

static UnitVectors basis(void)
{
  return (UnitVectors){
    {16, 32, 64},
    {48, 80, 112},
    {128, 144, 160},
  };
}

static void make_vertices(ObjVertices *vertices, unsigned char count, ObjVertex step)
{
  assert(count > 0);

  /* One count byte followed by x/y/z bytes for each vertex. */
  static unsigned char buffer[1 + (3 * ObjVertexMax)];
  buffer[0] = count;

  for (int i = 0; i < count; ++i)
  {
    int const offset = 1 + (3 * i);
    buffer[offset] = step.x;
    buffer[offset + 1] = step.y;
    buffer[offset + 2] = step.z;
  }

  Reader reader;
  assert(reader_mem_init(&reader, buffer, 1 + (3 * count)));

  obj_vertices_init(vertices);
  int loaded_count;
  assert(!SFError_fail(obj_vertices_read(vertices, &reader, &loaded_count)));
  assert(loaded_count == count);

  reader_destroy(&reader);
}

static void test1(void)
{
  /* Empty vertex destruction */

  ObjVertices vertices;
  obj_vertices_init(&vertices);
  obj_vertices_free(&vertices);
}

static void test2(void)
{
  /* Vertex count */

  for (unsigned char count = ObjVertexMax; count > 0; --count)
  {
    ObjVertices vertices;
    make_vertices(&vertices, count, (ObjVertex){
      RelCoord_Zero, RelCoord_Zero, RelCoord_Zero,
    });

    assert(obj_vertices_get_count(&vertices) == count);

    obj_vertices_free(&vertices);
  }
}

static void test3(void)
{
  /* Unit vector identity scaling */

  UnitVectors const input = basis();
  UnitVectors output;
  obj_vertices_scale_unit(&output, &input, 0);

  check_vertex(output.x, input.x);
  check_vertex(output.y, input.y);
  check_vertex(output.z, input.z);
}

static void test4(void)
{
  /* Unit vector division */

  for (int shift = 0; shift <= 5; ++shift)
  {
    long const factor = 1L << shift;
    UnitVectors const input = {
      {3 * factor, -5 * factor, 7 * factor},
      {-11 * factor, 13 * factor, 17 * factor},
      {19 * factor, 23 * factor, -29 * factor},
    };
    UnitVectors output;
    obj_vertices_scale_unit(&output, &input, shift);

    check_vertex(output.x, (Vertex3D){3, -5, 7});
    check_vertex(output.y, (Vertex3D){-11, 13, 17});
    check_vertex(output.z, (Vertex3D){19, 23, -29});
  }
}

static void test5(void)
{
  /* Negative vector rounding */

  UnitVectors const input = {
    {-1, -17, -33},
    {1, 17, 33},
    {0, -2, 2},
  };
  UnitVectors output;
  obj_vertices_scale_unit(&output, &input, 1);

  check_vertex(output.x, (Vertex3D){-1, -9, -17});
  check_vertex(output.y, (Vertex3D){0, 8, 16});
  check_vertex(output.z, (Vertex3D){0, -1, 1});
}

static void test6(void)
{
  /* In-place unit scaling */

  UnitVectors vectors = {
    {32, -64, 96},
    {-128, 160, -192},
    {224, -256, 288},
  };
  obj_vertices_scale_unit(&vectors, &vectors, 5);

  check_vertex(vectors.x, (Vertex3D){1, -2, 3});
  check_vertex(vectors.y, (Vertex3D){-4, 5, -6});
  check_vertex(vectors.z, (Vertex3D){7, -8, 9});
}

static void test7(void)
{
  /* Zero relative movement */

  UnitVectors const unit = basis();
  Vertex3D vertex = {-123, 456, -789};
  obj_vertices_add_scaled_unit(&vertex, &unit, (ObjVertex){
    RelCoord_Zero, RelCoord_Zero, RelCoord_Zero,
  });

  check_vertex(vertex, (Vertex3D){-123, 456, -789});
}

static void test8(void)
{
  /* Relative multiplication */

  static const struct
  {
    RelCoord code;
    long factor;
  } cases[] = {
    {.code = RelCoord_SubMul32, .factor = -32},
    {.code = RelCoord_SubMul16, .factor = -16},
    {.code = RelCoord_SubMul8, .factor = -8},
    {.code = RelCoord_SubMul4, .factor = -4},
    {.code = RelCoord_SubMul2, .factor = -2},
    {.code = RelCoord_SubUnit, .factor = -1},
    {.code = RelCoord_AddUnit, .factor = 1},
    {.code = RelCoord_AddMul2, .factor = 2},
    {.code = RelCoord_AddMul4, .factor = 4},
    {.code = RelCoord_AddMul8, .factor = 8},
    {.code = RelCoord_AddMul16, .factor = 16},
    {.code = RelCoord_AddMul32, .factor = 32},
  };
  UnitVectors const unit = basis();

  for (size_t i = 0; i < ARRAY_SIZE(cases); ++i)
  {
    for (int axis = 0; axis < 3; ++axis)
    {
      ObjVertex step = {
        RelCoord_Zero, RelCoord_Zero, RelCoord_Zero,
      };
      Vertex3D move;

      switch (axis)
      {
        case 0:
          step.x = cases[i].code;
          move = unit.x;
          break;

        case 1:
          step.y = cases[i].code;
          move = unit.y;
          break;

        default:
          step.z = cases[i].code;
          move = unit.z;
          break;
      }

      Vertex3D vertex = {17, -29, 43};
      obj_vertices_add_scaled_unit(&vertex, &unit, step);

      check_vertex(vertex, (Vertex3D){
        17 + move.x * cases[i].factor,
        -29 + move.y * cases[i].factor,
        43 + move.z * cases[i].factor,
      });
    }
  }
}

static void test9(void)
{
  /* Relative division */

  static const struct
  {
    long divisor;
    RelCoord code;
    int sign;
  } cases[] = {
    {.code = RelCoord_SubDiv2, .divisor = 2, .sign = -1},
    {.code = RelCoord_SubDiv4, .divisor = 4, .sign = -1},
    {.code = RelCoord_SubDiv8, .divisor = 8, .sign = -1},
    {.code = RelCoord_SubDiv16, .divisor = 16, .sign = -1},
    {.code = RelCoord_AddDiv16, .divisor = 16, .sign = 1},
    {.code = RelCoord_AddDiv8, .divisor = 8, .sign = 1},
    {.code = RelCoord_AddDiv4, .divisor = 4, .sign = 1},
    {.code = RelCoord_AddDiv2, .divisor = 2, .sign = 1},
  };
  UnitVectors const unit = basis();

  for (size_t i = 0; i < ARRAY_SIZE(cases); ++i)
  {
    Vertex3D vertex = {17, -29, 43};
    obj_vertices_add_scaled_unit(&vertex, &unit, (ObjVertex){
      cases[i].code, cases[i].code, cases[i].code,
    });

    check_vertex(vertex, (Vertex3D){
      17 + (unit.x.x + unit.y.x + unit.z.x) / cases[i].divisor * cases[i].sign,
      -29 + (unit.x.y + unit.y.y + unit.z.y) / cases[i].divisor * cases[i].sign,
      43 + (unit.x.z + unit.y.z + unit.z.z) / cases[i].divisor * cases[i].sign,
    });
  }
}

static void test10(void)
{
  /* Mixed relative factors */

  UnitVectors const unit = basis();
  Vertex3D vertex = {17, -29, 43};
  obj_vertices_add_scaled_unit(&vertex, &unit, (ObjVertex){
    RelCoord_AddMul2, RelCoord_SubDiv4, RelCoord_AddDiv16,
  });

  check_vertex(vertex, (Vertex3D){45, 24, 153});
}

static void test11(void)
{
  /* Cumulative vertex coordinates */

  for (unsigned char count = ObjVertexMax; count > 0; --count)
  {
    ObjVertices vertices;
    make_vertices(&vertices, count, (ObjVertex){
      RelCoord_AddUnit, RelCoord_SubUnit, RelCoord_AddDiv2,
    });

    UnitVectors const unit = {
      {16, 0, 0},
      {0, 32, 0},
      {0, 0, 64},
    };
    Vertex3D const centre = {1000, 2000, 3000};
    static Vertex3D output[ObjVertexMax];
    obj_vertices_to_coords(&vertices, &centre, &unit, &output);

    for (int i = 0; i < count; ++i)
    {
      check_vertex(output[i], (Vertex3D){
        1000 + 16L * (i + 1),
        2000 - 32L * (i + 1),
        3000 + 32L * (i + 1),
      });
    }

    obj_vertices_free(&vertices);
  }
}

static void test12(void)
{
  /* Untouched coordinate output */

  for (unsigned char count = ObjVertexMax; count > 0; --count)
  {
    ObjVertices vertices;
    make_vertices(&vertices, count, (ObjVertex){
      RelCoord_Zero, RelCoord_Zero, RelCoord_Zero,
    });

    static Vertex3D output[ObjVertexMax];
    for (int i = 0; i < ObjVertexMax; ++i)
    {
      output[i] = (Vertex3D){111, -222, 333};
    }

    UnitVectors const unit = basis();
    Vertex3D const centre = {7, -13, 23};
    obj_vertices_to_coords(&vertices, &centre, &unit, &output);

    for (int i = count; i < ObjVertexMax; ++i)
    {
      check_vertex(output[i], (Vertex3D){111, -222, 333});
    }

    obj_vertices_free(&vertices);
  }
}

void ObjVertex_tests(void)
{
  /* ObjVertex */
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Empty vertex destruction", test1 },
    { "Vertex count", test2 },
    { "Unit vector identity scaling", test3 },
    { "Unit vector division", test4 },
    { "Negative vector rounding", test5 },
    { "In-place unit scaling", test6 },
    { "Zero relative movement", test7 },
    { "Relative multiplication", test8 },
    { "Relative division", test9 },
    { "Mixed relative factors", test10 },
    { "Cumulative vertex coordinates", test11 },
    { "Untouched coordinate output", test12 },
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
