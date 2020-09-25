
//
// Author: Lorenz Pullwitt <memorysurfer@lorenz-pullwitt.de>
// Copyright 2016-2020
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
// You should have received a copy of the GNU General Public License
// along with this program; if not, you can find it here:
// https://www.gnu.org/licenses/old-licenses/gpl-2.0.html
//

#include <stdint.h>
#include <sys/time.h> // stopwatch

#pragma pack(push)
#pragma pack(4)
struct Chunk
{
  int64_t position;
  int32_t chunk_size;
};
#pragma pack(pop)

struct Stopwatch
{
  struct timeval *sw_time;
  struct timeval *sw_end;
  char **sw_text;
  int sw_c;
};

struct IndexedMemoryFile
{
  int filedesc;
  struct Chunk *chunks;
  int32_t chunk_count;
  int32_t *chunk_order;
  int32_t *delete_mark;
  int delete_end;
  struct Stopwatch sw;
  int stat_swap;
  int stats_gaps;
  int stats_gaps_space;
  char *stats_gaps_str;
};

void imf_init (struct IndexedMemoryFile *imf);
char imf_is_open (struct IndexedMemoryFile *imf);
int imf_create (struct IndexedMemoryFile *imf, const char *filename, int flags_mask);
int imf_open (struct IndexedMemoryFile *imf, const char *filename);
int imf_seek_unused (struct IndexedMemoryFile *imf, int32_t *index);
int32_t imf_get_size (struct IndexedMemoryFile *imf, int32_t index);
int imf_get (struct IndexedMemoryFile *imf, int32_t index, void *data);
int imf_delete (struct IndexedMemoryFile *imf, int32_t index);
int imf_put (struct IndexedMemoryFile *imf, int32_t index, void *data, int32_t data_size);
int imf_sync (struct IndexedMemoryFile *imf);
int imf_close (struct IndexedMemoryFile *imf);
int imf_get_length (struct IndexedMemoryFile *imf, int64_t *file_length);
/*
int imf_truncate (IndexedMemoryFile *imf);
*/
void imf_info_swaps (struct IndexedMemoryFile *imf);
int imf_info_gaps (struct IndexedMemoryFile *imf);
void sw_init (struct Stopwatch *sw);
int sw_start (char *sw_text, struct Stopwatch *sw);
int sw_stop (int sw_i, struct Stopwatch *sw);
int sw_info (char **sw_info_str, struct Stopwatch *sw);
void sw_free(struct Stopwatch *sw);
