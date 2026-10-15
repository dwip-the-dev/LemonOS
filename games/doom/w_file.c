//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	WAD I/O functions for LemonOS.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "doomtype.h"
#include "m_argv.h"
#include "w_file.h"

extern char _binary_assets_doom1_wad_start[];
extern char _binary_assets_doom1_wad_end[];

static wad_file_class_t lemon_wad_file;

static wad_file_t *Lemon_OpenFile(char *path)
{
    (void)path;
    wad_file_t *wad = (wad_file_t *)malloc(sizeof(wad_file_t));
    if (!wad) return NULL;

    wad->file_class = &lemon_wad_file;
    wad->mapped = (byte *)_binary_assets_doom1_wad_start;
    wad->length = (unsigned int)(_binary_assets_doom1_wad_end - _binary_assets_doom1_wad_start);

    printf("LemonOS: Mapped embedded WAD at %p (%u bytes)\n", wad->mapped, wad->length);
    return wad;
}

static void Lemon_CloseFile(wad_file_t *wad)
{
    if (wad) {
        free(wad);
    }
}

static size_t Lemon_Read(wad_file_t *wad, unsigned int offset,
                         void *buffer, size_t buffer_len)
{
    if (!wad || !wad->mapped) return 0;
    if (offset >= wad->length) return 0;
    if (offset + buffer_len > wad->length) {
        buffer_len = wad->length - offset;
    }
    memcpy(buffer, wad->mapped + offset, buffer_len);
    return buffer_len;
}

static wad_file_class_t lemon_wad_file = 
{
    Lemon_OpenFile,
    Lemon_CloseFile,
    Lemon_Read
};

wad_file_t *W_OpenFile(char *path)
{
    return lemon_wad_file.OpenFile(path);
}

void W_CloseFile(wad_file_t *wad)
{
    wad->file_class->CloseFile(wad);
}

size_t W_Read(wad_file_t *wad, unsigned int offset,
              void *buffer, size_t buffer_len)
{
    return wad->file_class->Read(wad, offset, buffer, buffer_len);
}
