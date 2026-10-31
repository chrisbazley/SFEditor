/* SFEditor: loading tests using the original game data.
 * Copyright (C) 2026 Christopher Bazley
 */
#undef NDEBUG

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "HostSupport.h"
#include "Tests.h"
#include "Reader.h"
#include "ReaderGKey.h"
#include "../DFile.h"
#include "../Map.h"
#include "../Obj.h"
#include "../PolyCol.h"
#include "../HillCol.h"
#include "../Mission.h"
#include "../MapTexBDat.h"
#include "../ObjGfxMesh.h"
#include "../ObjGfxMeshData.h"

#ifdef FORTIFY
#include "fortify.h"
#endif
#ifdef USE_OPTIONAL
#include "Optional.h"
#else
#define _Optional
#endif

static _Optional DFile *create_file(char const *const kind)
{
    if (!strcmp(kind, "map")) {
        _Optional MapData *const p = map_create_base();

        return p ? map_get_dfile(&*p) : NULL;
    }

    if (!strcmp(kind, "objects")) {
        _Optional ObjectsData *const p = objects_create_base();

        return p ? objects_get_dfile(&*p) : NULL;
    }

    if (!strcmp(kind, "colours")) {
        _Optional PolyColData *const p = polycol_create();

        return p ? polycol_get_dfile(&*p) : NULL;
    }

    if (!strcmp(kind, "hills")) {
        _Optional HillColData *const p = hillcol_create();

        return p ? hillcol_get_dfile(&*p) : NULL;
    }

    if (!strcmp(kind, "mission")) {
        _Optional MissionData *const p = mission_create();

        return p ? mission_get_dfile(&*p) : NULL;
    }

    return NULL;
}

static bool load(char const *const kind, Reader *const reader)
{
    SFError err = SFERROR(OK);

    if (!strcmp(kind, "tiles-dark") || !strcmp(kind, "tiles-bright")) {
        MapTexBitmaps tiles;
        MapTexBitmaps_init(&tiles);
        err = MapTexBitmaps_read(&tiles, reader);

        if (!SFError_fail(err)) {
            bool const bright = !strcmp(kind, "tiles-bright");

            for (int i = 0; i < tiles.count; ++i) {
                if (MapTexBitmaps_is_bright(&tiles,
                        map_ref_from_num((unsigned char)i)) != bright) {
                    fprintf(stderr, "Incorrect brightness for tile %d\n", i);
                    err = SFERROR(AlreadyReported);
                    break;
                }
            }
        }

        MapTexBitmaps_free(&tiles);
    } else if (!strcmp(kind, "graphics")) {
        ObjGfxMeshes meshes;
        ObjGfxMeshes_init(&meshes);
        err = ObjGfxMeshes_read(&meshes, reader);
        ObjGfxMeshes_free(&meshes);
    } else {
        _Optional DFile *const file = create_file(kind);

        if (!file) {
            fprintf(stderr, "Cannot create %s model\n", kind);
            return false;
        }

        err = dfile_read(&*file, reader);
        dfile_release(&*file);
    }

    if (SFError_fail(err)) {
        static char const *const errors[] = {
#define DECLARE_ERROR(name) #name,
#include "../DeclErrors.h"
#undef DECLARE_ERROR
        };

        fprintf(stderr, "Loader returned %s\n", errors[err.type]);
        return false;
    }

    return !reader_ferror(reader);
}

static void load_file(char const *kind, char const *filename)
{
    host_set_palette(!strcmp(kind, "tiles-bright"));
    printf("Loading %s: %s\n", kind, filename);
    fflush(stdout);

#ifdef FORTIFY
    unsigned long const allocated = Fortify_GetCurrentAllocation();
#endif
    _Optional FILE *const input = fopen(filename, "rb");
    assert(input);
    Reader reader;
    assert(reader_gkey_init(&reader, 9, &*input));
    assert(load(kind, &reader));
    reader_destroy(&reader);
    assert(!fclose(&*input));

#ifdef FORTIFY
    assert(!Fortify_CheckAllMemory());
    assert(Fortify_GetCurrentAllocation() == allocated);
#endif
}

static void load_files(char const *kind, char const *directory,
                       char const *const names[], size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        char filename[256];
        int const length = snprintf(filename, sizeof(filename),
                                    "%s/%s", directory, names[i]);
        assert(length > 0);
        assert((size_t)length < sizeof(filename));
        load_file(kind, filename);
    }
}

static void test1(void)
{
    /* Base maps */
    static char const *const names[] = {
        "Academy1", "Basic", "Blank", "Chem1", "Cyber1",
        "Cyber2", "Death1", "Death2", "Earth1", "Earth2",
        "Earth3", "Sea", "Warrior1", "Warrior2",
    };

    load_files("map", "LandScapes/Base/SprScape", names, ARRAY_SIZE(names));
}

static void test2(void)
{
    /* Base objects */
    static char const *const names[] = {
        "Academy1", "Basic", "Basic2", "Blank", "Chem1",
        "Death1", "Earth1", "Earth2", "Earth3", "Warrior1",
        "Warrior2",
    };

    load_files("objects", "LandScapes/Base/FxdObj", names, ARRAY_SIZE(names));
}

static void test3(void)
{
    /* Polygon graphics */
    static char const *const names[] = {
        "Chemical", "CyberZ", "Earth1", "Earth2", "Space",
        "Warrior",
    };

    load_files("graphics", "LandScapes/Graphics", names, ARRAY_SIZE(names));
}

static void test4(void)
{
    /* Dark tile bitmaps */
    static char const *const names[] = {
        "Chem", "Cyber", "Death", "Earth", "Ice",
        "Space", "Warrior",
    };

    load_files("tiles-dark", "LandScapes/Sprites", names, ARRAY_SIZE(names));
}

static void test5(void)
{
    /* Bright tile bitmaps */
    static char const *const names[] = {
        "Chem", "Cyber", "Death", "Earth", "Ice",
        "Space", "Warrior",
    };

    load_files("tiles-bright", "LandScapes/Sprites", names, ARRAY_SIZE(names));
}

static void test6(void)
{
    /* Polygon colours */
    static char const *const names[] = {
        "Default", "FastLaser", "FastShip", "Normal", "RedShip",
    };

    load_files("colours", "LandScapes/Palette", names, ARRAY_SIZE(names));
}

static void test7(void)
{
    /* Hill colours */
    static char const *const names[] = {
        "Chem", "Earth", "Earth2", "Ice", "Saturn",
        "Warrior",
    };

    load_files("hills", "LandScapes/Hill", names, ARRAY_SIZE(names));
}

static void test8(void)
{
    /* Missions */
    static char const levels[] = "EMH";
    for (size_t level = 0; level < sizeof(levels) - 1; ++level) {
        for (int mission = 1; mission <= 36; ++mission) {
            char filename[256];
            int const length = snprintf(filename, sizeof(filename),
                "LandScapes/Missions/%c/%c_%02d", levels[level], levels[level], mission);
            assert(length > 0);
            assert((size_t)length < sizeof(filename));
            load_file("mission", filename);
        }
    }
}

void LoadGameData_tests(void)
{
    /* LoadGameData */
    static const struct
    {
        char const *test_name;
        void (*test_func)(void);
    }
    unit_tests[] =
    {
        { "Base maps", test1 },
        { "Base objects", test2 },
        { "Polygon graphics", test3 },
        { "Dark tile bitmaps", test4 },
        { "Bright tile bitmaps", test5 },
        { "Polygon colours", test6 },
        { "Hill colours", test7 },
        { "Missions", test8 },
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
