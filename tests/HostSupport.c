/* Minimal host facilities for the production loaders. No file parsing here.
 * Copyright (C) 2026 Christopher Bazley
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../flex.h"
#include "NoBudge.h"
#include "Hourglass.h"
#include "HostSupport.h"
#include "../SprMem.h"
#include "../MapTexBitm.h"
#include "../SFInit.h"

#ifdef FORTIFY
#include "fortify.h"
#endif

/* Non-Fortify builds still need a host implementation of Acorn Flex.
 * Delegate to the same tested allocator used by fortified builds. */
#ifndef FORTIFY
int flex_alloc(flex_ptr anchor, int size)
{
    return PseudoFlex_alloc(anchor, size, __FILE__, __LINE__);
}

void flex_free(flex_ptr anchor)
{
    PseudoFlex_free(anchor, __FILE__, __LINE__);
}

int flex_size(flex_ptr anchor)
{
    return PseudoFlex_size(anchor);
}

int flex_extend(flex_ptr anchor, int size)
{
    return PseudoFlex_extend(anchor, size, __FILE__, __LINE__);
}
#endif

/* Uniform palettes give every decoded tile a known brightness regardless
 * of its pixel indices. Run the loader against both extremes. */
static PaletteEntry host_palette[NumColours];
PaletteEntry const (*palette)[NumColours] =
    (PaletteEntry const (*)[NumColours])&host_palette;

void host_set_palette(bool bright)
{
    for (size_t i = 0; i < NumColours; ++i)
        host_palette[i] = bright ? 0xffffff00u : 0;

    palette = (PaletteEntry const (*)[NumColours])&host_palette;
}

void nobudge_register(size_t heap_ensure)
{
    (void)heap_ensure;
}

void nobudge_deregister(void)
{
}

_Optional _kernel_oserror *hourglass_on(void)
{
    return NULL;
}

_Optional _kernel_oserror *hourglass_off(void)
{
    return NULL;
}

_Optional _kernel_oserror *hourglass_percentage(int n)
{
    (void)n;
    return NULL;
}

bool SprMem_init(SprMem *sm, int size)
{
    if (size < (int)sizeof(SpriteAreaHeader))
        size = sizeof(SpriteAreaHeader);

    if (!flex_alloc(&sm->mem, size))
        return false;

    SpriteAreaHeader *const area = sm->mem;
    *area = (SpriteAreaHeader){
        .size = size,
        .sprite_count = 0,
        .first = sizeof(*area),
        .used = sizeof(*area)
    };

    return true;
}

bool SprMem_create_sprite(SprMem *sm, char const *name,
                         bool has_palette, Vertex size, int mode)
{
    /* Only the mode-13, unpaletted 16x16 tile path is needed by this loader. */
    if (has_palette || mode != MapTexModeNumber ||
        size.x != MapTexSize || size.y != MapTexSize)
        abort();

    SpriteAreaHeader *area = sm->mem;
    int const bytes = sizeof(SpriteHeader) + MapTexSize * MapTexSize;
    int const used = area->used;

    if (!flex_extend(&sm->mem, used + bytes))
        return false;

    area = sm->mem;
    SpriteHeader *const sprite = (SpriteHeader *)((char *)area + used);
    *sprite = (SpriteHeader){
        .size = bytes,
        .width = MapTexSize / 4 - 1,
        .height = MapTexSize - 1,
        .right_bit = 31,
        .image = sizeof(*sprite),
        .mask = sizeof(*sprite),
        .type = mode
    };
    strncpy(sprite->name, name, sizeof(sprite->name));

    *area = (SpriteAreaHeader){
        .size = used + bytes,
        .sprite_count = area->sprite_count + 1,
        .first = area->first,
        .used = used + bytes
    };

    return true;
}

_Optional SpriteHeader *SprMem_get_sprite_address(SprMem *sm, char const *name)
{
    SpriteAreaHeader const *const area = sm->mem;

    for (int offset = area->first; offset < area->used;) {
        SpriteHeader *const sprite = (SpriteHeader *)((char *)sm->mem + offset);

        if (!strncmp(sprite->name, name, sizeof(sprite->name)))
            return sprite;

        offset += sprite->size;
    }

    return NULL;
}

void SprMem_put_sprite_address(SprMem *sm, SpriteHeader *sprite)
{
    (void)sm;
    (void)sprite;
}

void SprMem_destroy(SprMem *sm)
{
    flex_free(&sm->mem);
}

/* Debug bitmap dumps are diagnostic output, not part of loading. */
bool SprMem_verify(SprMem const *sm)
{
    (void)sm;
    return true;
}

bool SprMem_save(SprMem const *sm, char const *name)
{
    (void)sm;
    (void)name;
    return true;
}
