/* SFEditor test: objects editing operations.
 * Copyright (C) 2026 Christopher Bazley
 */
#undef NDEBUG

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "Macros.h"
#include "Debug.h"
#include "Reader.h"
#include "ReaderGKey.h"
#include "Tests.h"
#include "../ObjGfxMesh.h"
#include "../ObjGfxMeshData.h"

static void load_meshes(ObjGfxMeshes *data)
{
  _Optional FILE *const file = fopen("LandScapes/Graphics/Earth1", "rb");
  assert(file);
  Reader reader;
  assert(reader_gkey_init(&reader, 9, &*file));
  assert(!SFError_fail(ObjGfxMeshes_read(data, &reader)));
  assert(!reader_ferror(&reader));
  reader_destroy(&reader);
  assert(fclose(&*file) == 0);
}

static void test1(void)
{
  /* Collision cache reset after initialisation */

  static ObjGfxMeshes data;
  ObjGfxMeshes_init(&data);
  load_meshes(&data);
  MapPoint const loaded = ObjGfxMeshes_get_max_collision_size(&data);
  assert(loaded.x > 0);
  assert(loaded.y > 0);
  ObjGfxMeshes_free(&data);

  ObjGfxMeshes_init(&data);

  MapPoint const empty = ObjGfxMeshes_get_max_collision_size(&data);
  assert(empty.x == 0);
  assert(empty.y == 0);
  ObjGfxMeshes_free(&data);
}

void ObjectsEdit_tests(void)
{
  static const struct
  {
    char const *test_name;
    void (*test_func)(void);
  }
  unit_tests[] =
  {
    { "Collision cache reset after initialisation", test1 },
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
