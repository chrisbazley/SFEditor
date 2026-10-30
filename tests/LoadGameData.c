/* SFEditor: loading tests using the original game data.
 * Copyright (C) 2026 Christopher Bazley
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "HostSupport.h"
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

#ifdef FORTIFY
static bool fortify_error;

static void fortify_output(char const *text)
{
    fputs(text, stderr);
    fortify_error = true;
}
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

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s kind file...\n", argv[0]);
        return EXIT_FAILURE;
    }

#ifdef FORTIFY
    Fortify_SetOutputFunc(fortify_output);
#endif
    host_set_palette(!strcmp(argv[1], "tiles-bright"));
    bool success = true;

    for (int i = 2; i < argc; ++i) {
        printf("Loading %s: %s\n", argv[1], argv[i]);
        fflush(stdout);

#ifdef FORTIFY
        unsigned long const allocated = Fortify_GetCurrentAllocation();
#endif
        _Optional FILE *const input = fopen(argv[i], "rb");

        if (!input) {
            perror(argv[i]);
            success = false;
            continue;
        }

        Reader reader;
        if (!reader_gkey_init(&reader, 9, &*input)) {
            fprintf(stderr, "Cannot initialise decompressor\n");
            success = false;
        } else {
            if (!load(argv[1], &reader)) {
                fprintf(stderr, "Failed to load %s\n", argv[i]);
                success = false;
            }

            reader_destroy(&reader);
        }

        if (fclose(&*input))
            success = false;

#ifdef FORTIFY
        if (Fortify_CheckAllMemory() ||
            Fortify_GetCurrentAllocation() != allocated || fortify_error) {
            fprintf(stderr, "Memory check failed after %s\n", argv[i]);
            success = false;
        }
#endif
    }

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
