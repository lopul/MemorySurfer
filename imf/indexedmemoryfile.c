
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

#include "indexedmemoryfile.h"
#include "sha1.h"
#include <malloc.h>
#include <string.h>
#include <fcntl.h>
#include <limits.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h> // abs

const int32_t INITIAL_CHUNKS = 8;

void imf_init (struct IndexedMemoryFile *imf)
{
  imf->filedesc = -1;
  imf->chunks = NULL;
  imf->chunk_count = 0;
  imf->chunk_order = NULL;
  imf->delete_mark = NULL;
  imf->delete_end = 0;
  sw_init (&imf->sw);
  imf->stat_swap = -1;
  imf->stats_gaps = -1;
  imf->stats_gaps_space = -1;
  imf->stats_gaps_str = NULL;
}

struct GapStats
{
  int32_t gs_size;
  int32_t gs_n;
};

char imf_is_open (struct IndexedMemoryFile *imf)
{
  return (imf->chunk_count > 0) && (imf->filedesc != -1);
}

static int
imf_read (struct IndexedMemoryFile *imf, void *data, int64_t position, int32_t data_size)
{
  int e;
  off_t off;
  off_t cur_pos;
  ssize_t ssize;
  struct Sha1Context sha1;
  uint8_t message_digest_data[SHA1_HASH_SIZE];
  uint8_t message_digest[SHA1_HASH_SIZE];
  off = position;
  cur_pos = lseek (imf->filedesc, off, SEEK_SET);
  e = cur_pos == -1;
  if (e == 0)
  {
    assert (sizeof (data_size) <= sizeof (size_t));
    ssize = read (imf->filedesc, data, data_size);
    e = ssize == -1;
    if (e == 0)
    {
      e = sha1_reset (&sha1);
      if (e == 0)
      {
        e = sha1_input (&sha1, data, data_size);
        if (e == 0)
        {
          e = sha1_result (&sha1, message_digest_data);
          if (e == 0)
          {
            ssize = read (imf->filedesc, message_digest, SHA1_HASH_SIZE);
            e = ssize == -1;
            if (e == 0)
            {
              e = memcmp (message_digest_data, message_digest, SHA1_HASH_SIZE);
            }
          }
        }
      }
    }
  }
  return e;
}

static int
imf_write (struct IndexedMemoryFile *imf, const void *data, int64_t position, int32_t data_size)
{
  int e;
  off_t off;
  off_t cur_pos;
  ssize_t ssize;
  struct Sha1Context sha1;
  uint8_t message_digest[SHA1_HASH_SIZE];
  off = position;
  cur_pos = lseek (imf->filedesc, off, SEEK_SET);
  e = cur_pos == -1;
  if (e == 0)
  {
    assert (sizeof (data_size) <= sizeof (size_t));
    ssize = write (imf->filedesc, data, data_size);
    e = ssize == -1;
    if (e == 0)
    {
      e = sha1_reset (&sha1);
      if (e == 0)
      {
        e = sha1_input (&sha1, data, data_size);
        if (e == 0)
        {
          e = sha1_result (&sha1, message_digest);
          if (e == 0)
          {
            ssize = write (imf->filedesc, message_digest, SHA1_HASH_SIZE);
            e = ssize == -1;
          }
        }
      }
    }
  }
  return e;
}

static int
imf_alloc_chunks (struct IndexedMemoryFile *imf)
{
  int e;
  int i;
  int32_t data_size;
  int32_t increase;
  int32_t chunk_count;
  struct Chunk *chunks;
  int32_t *chunk_order;
  size_t size;
  increase = imf->chunk_count;
  if (increase > 128)
  {
    increase = 128;
  }
  chunk_count = imf->chunk_count + increase;
  assert (chunk_count < ((0x7FFFFFFF - SHA1_HASH_SIZE) / sizeof (struct Chunk) - 2));
  data_size = sizeof (struct Chunk) * chunk_count;
  chunks = realloc (imf->chunks, data_size);
  e = chunks == NULL;
  if (e == 0)
  {
    size = sizeof (int32_t) * chunk_count;
    chunk_order = realloc (imf->chunk_order, size);
    e = chunk_order == NULL;
    if (e == 0)
    {
      imf->chunks = chunks;
      imf->chunk_order = chunk_order;
      for (i = imf->chunk_count; i < chunk_count; i++)
      {
        chunks[i].position = INT64_MAX;
        chunks[i].chunk_size = 0;
        chunk_order[i] = i;
      }
      imf->chunk_count = chunk_count;
    }
  }
  return e;
}

static int
imf_find_space (struct IndexedMemoryFile *imf, int64_t *position, int32_t chunk_size)
{
  int e;
  int i;
  int64_t l_offset;
  int64_t r_offset;
  int64_t space;
  int64_t lg_pos; // large gap
  int32_t lg_chunk_size;
  lg_pos = -1;
  e = -1;
  l_offset = 0;
  for (i = 0; i < imf->chunk_count && e != 0; i++)
  {
    r_offset = imf->chunks[imf->chunk_order[i]].position;
    space = r_offset - l_offset;
    assert (space >= 0); // make sure no overlapping has happend
    if (space == chunk_size)
    {
      *position = l_offset;
      e = 0;
    }
    else if (space > chunk_size * 2)
    {
      if ((lg_pos == -1) || (lg_pos >= 0 && lg_chunk_size > space))
      {
        lg_pos = l_offset;
        lg_chunk_size = space;
      }
    }
    l_offset = r_offset + imf->chunks[imf->chunk_order[i]].chunk_size;
  }
  if ((e != 0) && (lg_pos != -1))
  {
    assert (lg_pos >= 0);
    *position = lg_pos;
    e = 0;
  }
  return e;
}

static void imf_sort_order(struct IndexedMemoryFile *imf) {
  int32_t tmp;
  int i;
  int j;
  int64_t i_pos;
  int64_t j_pos;
  if (imf->chunk_count > 1) {
    i = 1;
    while (i < imf->chunk_count) {
      tmp = imf->chunk_order[i];
      j = i - 1;
      i_pos = imf->chunks[tmp].position;
      do {
        j_pos = imf->chunks[imf->chunk_order[j]].position;
        if (j_pos > i_pos) {
          imf->chunk_order[j + 1] = imf->chunk_order[j];
        } else {
          break;
        }
        j--;
      } while (j >= 0);
      imf->chunk_order[j + 1] = tmp;
      i++;
    }
  }
}

int
imf_create (struct IndexedMemoryFile *imf, const char *filename, int flags_mask)
{
  int e;
  int filedesc;
  int chunk_count;
  int32_t data_size;
  struct Chunk *chunks;
  size_t size;
  int32_t *chunk_order;
  int64_t position;
  int i;
  int32_t chunk_size;
  struct flock fl;
  assert (imf->filedesc == -1 && imf->chunk_count == 0 && imf->chunks == NULL);
  filedesc = open (filename, O_CREAT | flags_mask | O_RDWR, S_IRUSR | S_IWUSR);
  e = filedesc == -1;
  if (e == 0)
  {
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0; // Lock entire file
    e = fcntl (filedesc, F_SETLK, &fl);
    if (e == 0)
    {
      chunk_count = INITIAL_CHUNKS;
      data_size = sizeof (struct Chunk) * chunk_count;
      chunks = malloc (data_size);
      e = chunks == NULL;
      if (e == 0)
      {
        size = sizeof (int32_t) * chunk_count;
        chunk_order = malloc (size);
        e = chunk_order == NULL;
        if (e == 0)
        {
          imf->filedesc = filedesc;
          imf->chunks = chunks;
          imf->chunk_count = chunk_count;
          imf->chunk_order = chunk_order;
          for (i = 0; i < imf->chunk_count; i++)
          {
            imf->chunks[i].position = INT64_MAX;
            imf->chunks[i].chunk_size = 0;
            imf->chunk_order[i] = i;
          }
          chunk_size = 2 * sizeof (struct Chunk) + SHA1_HASH_SIZE;
          e = imf_find_space (imf, &position, chunk_size);
          if (e == 0)
          {
            imf->chunks[0].position = position;
            imf->chunks[0].chunk_size = chunk_size;
            e = imf_sync (imf);
          }
        }
      }
    }
  }
  return e;
}

int imf_open(struct IndexedMemoryFile *imf, const char *filename) {
  int e;
  int filedesc;
  struct flock fl;
  int32_t data_size;
  struct Chunk *chunks;
  int32_t chunk_count;
  int32_t *chunk_order;
  int i;
  int sw_i;
  sw_i = sw_start("imf_open", &imf->sw);
  e = sw_i < 0;
  if (e == 0) {
    e = imf->filedesc != -1 || imf->chunk_count != 0 ? 0x052e5e3e : 0; // IMFOAF - imf_open assert (failed)
    if (e == 0) {
      filedesc = open(filename, O_RDWR, 0);
      e = filedesc == -1 ? 0x059fe56c : 0; // IMFOOF - imf_open open (failed)
      if (e == 0) {
        fl.l_type = F_WRLCK;
        fl.l_whence = SEEK_SET;
        fl.l_start = 0;
        fl.l_len = 0; // Lock entire file
        e = fcntl(filedesc, F_SETLK, &fl) ? 0x0556e9f3 : 0; // IMFOFF - imf_open fcntl (failed)
        if (e == 0) {
          data_size = sizeof(struct Chunk) * 2;
          chunks = malloc(data_size);
          e = chunks == NULL;
          if (e == 0) {
            imf->filedesc = filedesc;
            e = imf_read(imf, chunks, 0, data_size);
            if (e == 0) {
              chunk_count = (chunks[1].chunk_size - SHA1_HASH_SIZE) / sizeof(struct Chunk) + 2;
              data_size = sizeof(struct Chunk) * chunk_count;
              chunks = realloc(chunks, data_size);
              e = chunks == NULL;
              if (e == 0) {
                data_size = chunks[1].chunk_size - SHA1_HASH_SIZE;
                e = imf_read(imf, chunks + 2, chunks[1].position, data_size);
                if (e == 0) {
                  data_size = sizeof(int32_t) * chunk_count;
                  chunk_order = malloc(data_size);
                  e = chunk_order == NULL;
                  if (e == 0) {
                    imf->chunks = chunks;
                    imf->chunk_count = chunk_count;
                    imf->chunk_order = chunk_order;
                    for (i = 0; i < imf->chunk_count; i++) {
                      imf->chunk_order[i] = i;
                    }
                    imf_sort_order(imf);
                  }
                }
              }
            }
          }
        }
      }
    }
    e = sw_stop(sw_i, &imf->sw);
  }
  return e;
}

int imf_seek_unused (struct IndexedMemoryFile *imf, int32_t *index)
{
  int e;
  int32_t i;
  int32_t chunk_size;
  assert (imf->chunk_count > 0);
  for (i = 0; i < imf->chunk_count; i++)
  {
    chunk_size = imf->chunks[i].chunk_size;
    e = chunk_size != 0;
    if (e == 0)
    {
      *index = i;
      break;
    }
  }
  assert (e == 0);
  // alloc at least one "ending chunk"
  if (i == (imf->chunk_count - 1))
  {
    e = imf_alloc_chunks (imf);
  }
  return e;
}

int32_t imf_get_size (struct IndexedMemoryFile *imf, int32_t index)
{
  int32_t data_size;
  assert (index >= 0 && index < imf->chunk_count && imf->chunks[index].chunk_size > 0);
  data_size = imf->chunks[index].chunk_size - SHA1_HASH_SIZE;
  return data_size;
}

int imf_get (struct IndexedMemoryFile *imf, int32_t index, void *data)
{
  int e;
  int32_t data_size;
  int64_t position;
  data_size = imf_get_size (imf, index);
  position = imf->chunks[index].position;
  e = imf_read (imf, data, position, data_size);
  return e;
}

static int
imf_prepare_free(struct IndexedMemoryFile *imf)
{
  int e;
  size_t size;
  int32_t *delete_mark;
  size = sizeof(int32_t) * (imf->delete_end + 1);
  delete_mark = realloc(imf->delete_mark, size);
  e = delete_mark == NULL;
  if (e == 0)
    imf->delete_mark = delete_mark;
  return e;
}

static void
imf_free(struct IndexedMemoryFile *imf, int32_t index)
{
  assert(imf->chunks[index].chunk_size != 0);
  imf->delete_mark[imf->delete_end++] = index;
}

int imf_delete(struct IndexedMemoryFile *imf, int32_t index)
{
  int e;
  e = imf_prepare_free(imf);
  if (e == 0)
    imf_free(imf, index);
  return e;
}

int imf_put (struct IndexedMemoryFile *imf, int32_t index, void *data, int32_t data_size)
{
  int e;
  int64_t position;
  int32_t chunk_size;
  int32_t free_index;
  assert (index > 1);
  chunk_size = data_size + SHA1_HASH_SIZE;
  e = imf_find_space (imf, &position, chunk_size);
  if (e == 0)
  {
    e = imf_write (imf, data, position, data_size);
    if (e == 0)
    {
      e = imf->chunks[index].chunk_size > 0;
      if (e != 0)
      {
        e = imf_seek_unused (imf, &free_index);
        if (e == 0)
        {
          e = imf_prepare_free (imf);
          if (e == 0)
          {
            imf->chunks[free_index].position = imf->chunks[index].position;
            imf->chunks[free_index].chunk_size = imf->chunks[index].chunk_size;
            imf_free (imf, free_index);
          }
        }
      }
      if (e == 0)
      {
        imf->chunks[index].position = position;
        imf->chunks[index].chunk_size = chunk_size;
        imf_sort_order (imf);
      }
    }
  }
  return e;
}

int imf_sync(struct IndexedMemoryFile *imf)
{
  int e;
  int i;
  int32_t del_index;
  int64_t position;
  int32_t data_size;
  int64_t space;
  void *data;
  int32_t chunk_size;
  int32_t index;
  int32_t r_index; // right
  e = 0;
  for (i = 1; i < (imf->chunk_count - 1) && e == 0; i++) {
    index = imf->chunk_order[i];
    if (index != 1) {
      r_index = imf->chunk_order[i+1];
      assert(r_index < imf->chunk_count);
      chunk_size = imf->chunks[index].chunk_size;
      space = imf->chunks[r_index].position - imf->chunks[index].position - chunk_size;
      assert(space >= 0);
      if (space > 0) {
        if (space < chunk_size) {
          data_size = imf_get_size(imf, index);
          data = malloc(data_size);
          e = data == NULL;
          if (e == 0) {
            e = imf_get(imf, index, data);
            if (e == 0) {
              e = imf_put(imf, index, data, data_size);
              i = INT32_MAX - 1;
            }
            free(data);
          }
        }
      }
    }
  }
  if (e == 0) {
    data_size = sizeof (struct Chunk) * (imf->chunk_count - 2);
    e = imf_find_space (imf, &position, data_size + SHA1_HASH_SIZE);
    if (e == 0)
    {
      imf->chunks[1].position = position;
      imf->chunks[1].chunk_size = data_size + SHA1_HASH_SIZE;
      // free the deleted chunks
      if (imf->delete_end > 0)
      {
        for (i = 0; i < imf->delete_end; i++)
        {
          del_index = imf->delete_mark[i];
          imf->chunks[del_index].position = INT64_MAX;
          imf->chunks[del_index].chunk_size = 0;
        }
        imf->delete_end = 0;
      }
      imf_sort_order (imf);
      e = imf_write (imf, imf->chunks + 2, position, data_size);
      if (e == 0)
      {
        data_size = sizeof (struct Chunk) * 2;
        assert (imf->chunks[0].position == 0);
        e = imf_write (imf, imf->chunks, 0, data_size);
        if (e == 0)
        {
          e = fsync (imf->filedesc);
        }
      }
    }
  }
  return e;
}

int imf_close(struct IndexedMemoryFile *imf) {
  int e;
  e = imf == NULL || imf->filedesc == -1;
  if (e == 0) {
    e = close(imf->filedesc);
    if (e == 0) {
      imf->filedesc = -1;
      free(imf->chunks);
      imf->chunks = NULL;
      imf->chunk_count = 0;
      free(imf->chunk_order);
      imf->chunk_order = NULL;
      free(imf->delete_mark);
      imf->delete_mark = NULL;
      imf->delete_end = 0;
      free(imf->stats_gaps_str);
      imf->stats_gaps_str = NULL;
    }
  }
  if (e != 0)
    e = 0x0000f927; // IMFC - imf_close (failed)
  return e;
}

// file_length returns the lenght of the used part in the file.
int
imf_get_length (struct IndexedMemoryFile *imf, int64_t *file_length)
{
  int status;
  int i;
  int32_t chunk_size;
  assert (file_length != NULL);
  status = (imf->chunk_count <= 0);
  if (status == 0)
  {
    imf_sort_order(imf);
    i = imf->chunk_count;
    while (i > 0)
    {
      i--;
      chunk_size = imf->chunks[imf->chunk_order[i]].chunk_size;
      status = (chunk_size == 0);
      if (status == 0)
      {
        *file_length = imf->chunks[imf->chunk_order[i]].position + chunk_size;
        break;
      }
    }
  }
  return status;
}
/*
// truncates the file
int
imf_truncate (IndexedMemoryFile *imf)
{
  int status;
  int64_t file_length;
  status = imf_get_length(imf, &file_length);
  if (status == 0)
  {
    status = ftruncate (imf->filedesc, file_length);
  }
  return status;
}
*/
void imf_info_swaps (struct IndexedMemoryFile *imf)
{
  int i;
  int dist;
  int abs_dist;
  int tot_abs_dist;
  tot_abs_dist = 0;
  for (i = 0; i < imf->chunk_count; i++)
  {
    dist = i - imf->chunk_order[i];
    abs_dist = abs (dist);
    tot_abs_dist += abs_dist;
  }
  assert ((tot_abs_dist & 1) == 0);
  imf->stat_swap = tot_abs_dist / 2;
}

int imf_info_gaps (struct IndexedMemoryFile *imf)
{
  int e;
  int rv; // return value
  int s; // stop
  struct GapStats* gstats;
  int gs_a; // allocated
  int gs_i;
  int i;
  int64_t l_offset;
  int64_t r_offset;
  int64_t space;
  size_t size;
  char gap_str[15]; // 9999x999999999/0
  e = 0;
  gstats = NULL;
  gs_a = 0;
  l_offset = 0;
  imf->stats_gaps = 0;
  imf->stats_gaps_space = 0;
  s = 0;
  for (i = 0; i < imf->chunk_count && e == 0 && s == 0; i++)
  {
    r_offset = imf->chunks[imf->chunk_order[i]].position;
    s = r_offset == INT64_MAX;
    if (s == 0)
    {
      space = r_offset - l_offset;
      assert (space >= 0);
      if (space > 0)
      {
        imf->stats_gaps++;
        imf->stats_gaps_space += space;
        e = -1;
        gs_i = 0;
        while (gs_i < gs_a && e != 0)
        {
          if (gstats[gs_i].gs_size == space)
          {
            e = 0;
          }
          else
          {
            gs_i++;
          }
        }
        if (e == 0)
        {
          gstats[gs_i].gs_n++;
        }
        else
        {
          gs_a++;
          size = sizeof (struct GapStats) * gs_a;
          gstats = realloc (gstats, size);
          e = gstats == NULL;
          if (e == 0)
          {
            gstats[gs_i].gs_size = space;
            gstats[gs_i].gs_n = 1;
          }
        }
      }
      l_offset = r_offset + imf->chunks[imf->chunk_order[i]].chunk_size;
    }
  }
  for (gs_i = 0; gs_i < gs_a && e == 0; gs_i++)
  {
    size = sizeof (gap_str);
    rv = snprintf (gap_str, size, "%dx%d", gstats[gs_i].gs_n, gstats[gs_i].gs_size);
    e = rv < 0 || rv >= size;
    if (e == 0)
    {
      size = 0;
      if (imf->stats_gaps_str != NULL)
      {
        size = strlen (imf->stats_gaps_str) + 2;
      }
      size += strlen (gap_str) + 1;
      imf->stats_gaps_str = realloc (imf->stats_gaps_str, size);
      e = imf->stats_gaps_str == NULL;
      if (e == 0)
      {
        if (gs_i == 0)
        {
          strcpy (imf->stats_gaps_str, gap_str);
        }
        else
        {
          strcat (imf->stats_gaps_str, ", ");
          strcat (imf->stats_gaps_str, gap_str);
        }
      }
    }
  }
  free(gstats);
  return e;
}

void sw_init (struct Stopwatch *sw)
{
  sw->sw_time = NULL;
  sw->sw_end = NULL;
  sw->sw_text = NULL;
  sw->sw_c = 0;
}

int sw_start (char *sw_text, struct Stopwatch *sw)
{
  int e;
  int ret;
  int sw_i;
  size_t size;
  assert(sw_text != NULL && sw->sw_c >= 0);
  sw_i = sw->sw_c++;
  size = sizeof(char*) * sw->sw_c;
  sw->sw_text = realloc(sw->sw_text, size);
  e = sw->sw_text == NULL;
  if (e == 0) {
    size = strlen(sw_text) + 1;
    sw->sw_text[sw_i] = malloc(size);
    e = sw->sw_text[sw_i] == NULL;
    if (e == 0) {
      strcpy(sw->sw_text[sw_i], sw_text);
      size = sizeof(struct timeval) * sw->sw_c;
      sw->sw_time = realloc(sw->sw_time, size);
      e = sw->sw_time == NULL;
      if (e == 0) {
        sw->sw_end = realloc(sw->sw_end, size);
        e = sw->sw_end == NULL;
        if (e == 0) {
          e = gettimeofday(&sw->sw_time[sw_i], NULL);
        }
      }
    }
  }
  ret = e == 0 ? sw_i : -1;
  return ret;
}

int sw_stop (int sw_i, struct Stopwatch *sw)
{
  int e;
  assert(sw->sw_c > 0 && sw_i < sw->sw_c);
  e = gettimeofday(&sw->sw_end[sw_i], NULL);
  return e;
}

int sw_info (char **sw_info_str, struct Stopwatch *sw)
{
  int e;
  int i;
  size_t size_a; // allocated
  size_t size_p; // printed
  int off;
  time_t tv_sec;
  suseconds_t tv_usec;
  char *comma_str;
  assert (sw_info_str != NULL);
  *sw_info_str = NULL;
  e = 0;
  size_a = 0;
  size_p = 0;
  off = 0;
  for (i = 0; i < sw->sw_c && e == 0; i++)
  {
    tv_sec = sw->sw_end[i].tv_sec - sw->sw_time[i].tv_sec;
    tv_usec = sw->sw_end[i].tv_usec - sw->sw_time[i].tv_usec;
    if (tv_usec < 0) {
      tv_sec--;
      tv_usec += 1000000;
    }
    comma_str = i + 1 < sw->sw_c ? ", " : "";
    do
    {
      if (size_a < off + size_p + 1)
      {
        size_a = off + size_p + 1;
        *sw_info_str = realloc (*sw_info_str, size_a);
        e = *sw_info_str == NULL;
      }
      if (e == 0)
      {
        size_p = snprintf (*sw_info_str + off, size_a - off, "%s: %d.%06d%s", sw->sw_text[i], (int) tv_sec, (int) tv_usec, comma_str);
      }
    } while (((size_a - off) < size_p) && (e == 0));
    off += size_p;
    size_p = 0;
  }
  return e;
}

void sw_free(struct Stopwatch *sw)
{
  int i;
  free(sw->sw_time);
  sw->sw_time = NULL;
  free(sw->sw_end);
  sw->sw_end = NULL;
  for (i = 0; i < sw->sw_c; i++)
    free(sw->sw_text[i]);
  sw->sw_c = 0;
  free(sw->sw_text);
  sw->sw_text = NULL;
}
