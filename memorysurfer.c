
//
// Author: Lorenz Pullwitt <memorysurfer@lorenz-pullwitt.de>
// Copyright 2016-2021
//
// This file is part of MemorySurfer.
//
// MemorySurfer is free software; you can redistribute it and/or
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

#include "imf/indexedmemoryfile.h"
#include "imf/sha1.h"
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <math.h>
#include <libgen.h> // basename
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h> // INT_MAX
#include <unistd.h> // unlink
#include <fcntl.h> // O_TRUNC / O_EXCL
#include <errno.h>

enum Field { F_UNKNOWN, F_FILENAME, F_FILE_TITLE, F_START_ACTION, F_FILE_ACTION, F_ARRANGE, F_CAT_NAME, F_MOVED_CAT, F_EDIT_ACTION, F_LEARN_ACTION, F_SEARCH_TXT, F_SEARCH_ACTION, F_CAT, F_CARD, F_MOV_CARD, F_LVL, F_Q, F_A, F_REVEAL_POS, F_TODO_MAIN, F_TODO_ALT, F_MTIME, F_PASSWORD, F_NEW_PASSWORD, F_TOKEN, F_EVENT, F_PAGE, F_MODE, F_TIMEOUT };
enum Action { A_END, A_NONE, A_FILE, A_WARNING, A_CREATE, A_NEW, A_OPEN_DLG, A_FILELIST, A_OPEN, A_CHANGE_PASSWD, A_WRITE_PASSWD, A_READ_PASSWD, A_CHECK_PASSWORD, A_AUTH_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_LOAD_CARDLIST, A_CHECK_RESUME, A_SLASH, A_VOID, A_FILE_EXTENSION, A_GATHER, A_UPLOAD, A_UPLOAD_REPORT, A_EXPORT, A_ASK_REMOVE, A_REMOVE, A_ASK_ERASE, A_ERASE, A_CLOSE, A_START_CAT, A_SELECT_CREATE_CAT, A_SELECT_CAT, A_SELECT_SEND_CAT, A_SELECT_ARRANGE, A_CAT_NAME, A_CREATE_CAT, A_RENAME_CAT, A_ASK_DELETE_CAT, A_DELETE_CAT, A_TOGGLE, A_MOVE_CAT, A_SELECT_EDIT_CAT, A_EDIT, A_SYNC_QA, A_INSERT, A_APPEND, A_ASK_DELETE_CARD, A_DELETE_CARD, A_PREVIOUS, A_NEXT, A_SCHEDULE, A_SET, A_ARRANGE, A_MOVE_CARD, A_SEND_CARD, A_SELECT_LEARN_CAT, A_SELECT_SEARCH_CAT, A_PREFERENCES, A_ABOUT, A_APPLY, A_SEARCH, A_QUESTION, A_SHOW, A_REVEAL, A_PROCEED, A_SUSPEND, A_RESUME, A_CHECK_FILE, A_LOGIN, A_HISTOGRAM, A_RETRIEVE_MTIME, A_MTIME_TEST, A_CARD_TEST, A_TEST_CAT_SELECTED, A_TEST_CAT_VALID, A_TEST_CAT };
enum Page { P_START, P_FILE, P_PASSWORD, P_NEW, P_OPEN, P_UPLOAD, P_UPLOAD_REPORT, P_EXPORT, P_START_CAT, P_CAT_NAME, P_SELECT_CREATE_CAT, P_SELECT_CAT, P_SELECT_SEND_CAT, P_SELECT_ARRANGE, P_SELECT_CARD_ARRANGE, P_SELECT_EDIT_CAT, P_EDIT, P_SELECT_LEARN_CAT, P_SELECT_SEARCH_CAT, P_SEARCH, P_PREFERENCES, P_ABOUT, P_LEARN, P_MSG, P_HISTOGRAM };
enum Block { B_END, B_START_HTML, B_HIDDEN_CAT, B_HIDDEN_ARRANGE, B_HIDDEN_CAT_NAME, B_HIDDEN_SEARCH_TXT, B_CLOSE_DIV, B_START, B_FILE, B_PASSWORD, B_NEW, B_OPEN, B_UPLOAD, B_UPLOAD_REPORT, B_EXPORT, B_START_CAT, B_CAT_NAME, B_SELECT_CREATE_CAT, B_SELECT_CAT, B_SELECT_SEND_CAT, B_SELECT_ARRANGE, B_SELECT_CARD_ARRANGE, B_SELECT_EDIT_CAT, B_EDIT, B_SELECT_LEARN_CAT, B_SELECT_SEARCH_CAT, B_SEARCH, B_PREFERENCES, B_ABOUT, B_LEARN, B_MSG, B_HISTOGRAM };
enum Mode { M_NONE = -1, M_DEFAULT, M_CHANGE_PASSWD, M_ASK, M_RATE };
enum Sequence { S_FILE, S_START_CAT, S_SELECT_CREATE_CAT, S_SELECT_ARRANGE, S_SELECT_MOVE_ARRANGE, S_CAT_NAME, S_SELECT_EDIT_CAT, S_SELECT_LEARN_CAT, S_SELECT_SEARCH_CAT, S_PREFERENCES, S_ABOUT, S_APPLY, S_NEW, S_FILELIST, S_WARNING, S_UPLOAD, S_LOGIN, S_ENTER, S_CHANGE, S_START, S_UPLOAD_REPORT, S_EXPORT, S_ASK_REMOVE, S_REMOVE, S_ASK_ERASE, S_ERASE, S_CLOSE, S_NONE, S_CREATE, S_GO_LOGIN, S_GO_CHANGE, S_SELECT_RENAME_CAT, S_RENAME_ENTER, S_RENAME_CAT, S_SELECT_MOVE_CAT, S_SELECT_DEST_CAT, S_MOVE_CAT, S_CREATE_CAT, S_SELECT_DELETE_CAT, S_ASK_DELETE_CAT, S_DELETE_CAT, S_SELECT_TOGGLE_CAT, S_TOGGLE, S_EDIT, S_INSERT, S_APPEND, S_ASK_DELETE_CARD, S_DELETE_CARD, S_PREVIOUS, S_NEXT, S_SCHEDULE, S_SET, S_ARRANGE, S_MOVE_CARD, S_SELECT_SEND_CAT, S_SEND_CARD, S_SEARCH_SYNCED, S_QUESTION, S_SHOW, S_REVEAL, S_PROCEED, S_SUSPEND, S_RESUME, S_SEARCH, S_HISTOGRAM, S_END };
enum Stage { T_NULL, T_URLENCODE, T_BOUNDARY_INIT, T_CONTENT, T_NAME, T_BOUNDARY_BEGIN, T_BOUNDARY_CHECK };

static enum Action action_seq[S_END+1][13] = {
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_FILE, A_END }, // S_FILE
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_START_CAT, A_END }, // S_START_CAT
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_SELECT_CREATE_CAT, A_END }, // S_SELECT_CREATE_CAT
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_TEST_CAT, A_SELECT_ARRANGE, A_END }, // S_SELECT_ARRANGE
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_SELECT_ARRANGE, A_END }, // S_SELECT_MOVE_ARRANGE
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_CAT_NAME, A_END }, // S_CAT_NAME
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_SELECT_EDIT_CAT, A_END }, // S_SELECT_EDIT_CAT
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_SELECT_LEARN_CAT, A_END }, // S_SELECT_LEARN_CAT
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_SELECT_SEARCH_CAT, A_END }, // S_SELECT_SEARCH_CAT
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_PREFERENCES, A_END }, // S_PREFERENCES
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_ABOUT, A_END }, // S_ABOUT
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_APPLY, A_GEN_TOK, A_END }, // S_APPLY
  { A_FILELIST, A_NEW, A_END }, // S_NEW
  { A_FILELIST, A_OPEN_DLG, A_END }, // S_FILELIST
  { A_SLASH, A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_WARNING, A_END }, // S_WARNING
  { A_SLASH, A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_UPLOAD, A_END }, // S_UPLOAD
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_PASSWD, A_GEN_TOK, A_NONE, A_END }, // S_LOGIN
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_CHECK_PASSWORD, A_CHANGE_PASSWD, A_WRITE_PASSWD, A_GEN_TOK, A_NONE, A_END }, // S_ENTER
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_PASSWD, A_CHANGE_PASSWD, A_WRITE_PASSWD, A_GEN_TOK, A_NONE, A_END }, // S_CHANGE
  { A_SLASH, A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_NONE, A_END }, // S_START
  { A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_UPLOAD_REPORT, A_END }, // S_UPLOAD_REPORT
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_EXPORT, A_END }, // S_EXPORT
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_RETRIEVE_MTIME, A_MTIME_TEST, A_ASK_REMOVE, A_END }, // S_ASK_REMOVE
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_REMOVE, A_CLOSE, A_END }, // S_REMOVE
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_RETRIEVE_MTIME, A_MTIME_TEST, A_ASK_ERASE, A_END }, // S_ASK_ERASE
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_ERASE, A_END }, // S_ERASE
  { A_CLOSE, A_END }, // S_CLOSE
  { A_NONE, A_END }, // S_NONE
  { A_SLASH, A_VOID, A_FILE_EXTENSION, A_GATHER, A_CREATE, A_LOGIN, A_END }, // S_CREATE
  { A_CHECK_FILE, A_GATHER, A_OPEN, A_READ_PASSWD, A_LOGIN, A_END }, // S_GO_LOGIN
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_LOGIN, A_END }, // S_GO_CHANGE
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_SELECT_CAT, A_END }, // S_SELECT_RENAME_CAT
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_CAT_NAME, A_END }, // S_RENAME_ENTER
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_RENAME_CAT, A_END }, // S_RENAME_CAT
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_SELECT_CAT, A_END }, // S_SELECT_MOVE_CAT
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_TEST_CAT, A_SELECT_CAT, A_END }, // S_SELECT_DEST_CAT
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_MOVE_CAT, A_END }, // S_MOVE_CAT
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_TEST_CAT, A_CREATE_CAT, A_END }, // S_CREATE_CAT
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_SELECT_CAT, A_END }, // S_SELECT_DELETE_CAT
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_TEST_CAT, A_ASK_DELETE_CAT, A_END }, // S_ASK_DELETE_CAT
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_TEST_CAT, A_DELETE_CAT, A_END }, // S_DELETE_CAT
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_SELECT_CAT, A_END }, // S_SELECT_TOGGLE_CAT
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_TEST_CAT, A_TOGGLE, A_END }, // S_TOGGLE
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_EDIT, A_END }, // S_EDIT
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_RETRIEVE_MTIME, A_MTIME_TEST, A_LOAD_CARDLIST, A_SYNC_QA, A_INSERT, A_END }, // S_INSERT
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_RETRIEVE_MTIME, A_MTIME_TEST, A_LOAD_CARDLIST, A_SYNC_QA, A_APPEND, A_END }, // S_APPEND
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_RETRIEVE_MTIME, A_MTIME_TEST, A_LOAD_CARDLIST, A_CARD_TEST, A_ASK_DELETE_CARD, A_END }, // S_ASK_DELETE_CARD
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_LOAD_CARDLIST, A_DELETE_CARD, A_END }, // S_DELETE_CARD
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_RETRIEVE_MTIME, A_MTIME_TEST, A_PREVIOUS, A_END }, // S_PREVIOUS
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_RETRIEVE_MTIME, A_MTIME_TEST, A_NEXT, A_END }, // S_NEXT
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_RETRIEVE_MTIME, A_MTIME_TEST, A_LOAD_CARDLIST, A_CARD_TEST, A_SYNC_QA, A_SCHEDULE, A_END }, // S_SCHEDULE
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_LOAD_CARDLIST, A_SYNC_QA, A_SET, A_END }, // S_SET
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_LOAD_CARDLIST, A_SYNC_QA, A_ARRANGE, A_END }, // S_ARRANGE
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_LOAD_CARDLIST, A_MOVE_CARD, A_END }, // S_MOVE_CARD
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_LOAD_CARDLIST, A_SYNC_QA, A_SELECT_SEND_CAT, A_END }, // S_SELECT_SEND_CAT
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_RETRIEVE_MTIME, A_MTIME_TEST, A_LOAD_CARDLIST, A_SEND_CARD, A_END }, // S_SEND_CARD
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_LOAD_CARDLIST, A_SYNC_QA, A_SEARCH, A_END }, // S_SEARCH_SYNCED
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_LOAD_CARDLIST, A_CHECK_RESUME, A_SYNC_QA, A_QUESTION, A_END }, // S_QUESTION
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_TEST_CAT_SELECTED, A_TEST_CAT_VALID, A_LOAD_CARDLIST, A_SHOW, A_CHECK_RESUME, A_END }, // S_SHOW
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_TEST_CAT, A_LOAD_CARDLIST, A_REVEAL, A_CHECK_RESUME, A_END }, // S_REVEAL
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_RETRIEVE_MTIME, A_MTIME_TEST, A_TEST_CAT, A_LOAD_CARDLIST, A_CARD_TEST, A_PROCEED, A_CHECK_RESUME, A_END }, // S_PROCEED
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_LOAD_CARDLIST, A_SUSPEND, A_CHECK_RESUME, A_END }, // S_SUSPEND
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_LOAD_CARDLIST, A_RESUME, A_END }, // S_RESUME
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_LOAD_CARDLIST, A_SEARCH, A_END }, // S_SEARCH
  { A_GATHER, A_OPEN, A_READ_PASSWD, A_AUTH_TOK, A_GEN_TOK, A_LOAD_CARDLIST, A_HISTOGRAM, A_END }, // S_HISTOGRAM
  { A_END } // S_END
};

static enum Block block_seq[P_HISTOGRAM+1][8] = {
  { B_START_HTML, B_HIDDEN_CAT, B_HIDDEN_ARRANGE, B_HIDDEN_CAT_NAME, B_HIDDEN_SEARCH_TXT, B_CLOSE_DIV, B_START, B_END }, // P_START
  { B_START_HTML, B_HIDDEN_CAT, B_HIDDEN_ARRANGE, B_HIDDEN_CAT_NAME, B_HIDDEN_SEARCH_TXT, B_CLOSE_DIV, B_FILE, B_END }, // P_FILE
  { B_START_HTML, B_HIDDEN_CAT, B_HIDDEN_ARRANGE, B_HIDDEN_CAT_NAME, B_HIDDEN_SEARCH_TXT, B_CLOSE_DIV, B_PASSWORD, B_END }, // P_PASSWORD
  { B_START_HTML, B_HIDDEN_CAT, B_HIDDEN_ARRANGE, B_HIDDEN_CAT_NAME, B_HIDDEN_SEARCH_TXT, B_CLOSE_DIV, B_NEW, B_END }, // P_NEW
  { B_START_HTML, B_HIDDEN_CAT, B_HIDDEN_ARRANGE, B_HIDDEN_CAT_NAME, B_HIDDEN_SEARCH_TXT, B_CLOSE_DIV, B_OPEN, B_END }, // P_OPEN
  { B_UPLOAD, B_END }, // P_UPLOAD
  { B_START_HTML, B_HIDDEN_CAT, B_HIDDEN_ARRANGE, B_HIDDEN_CAT_NAME, B_HIDDEN_SEARCH_TXT, B_CLOSE_DIV, B_UPLOAD_REPORT, B_END }, // P_UPLOAD_REPORT
  { B_EXPORT, B_END }, // P_EXPORT
  { B_START_HTML, B_HIDDEN_CAT, B_HIDDEN_ARRANGE, B_HIDDEN_CAT_NAME, B_HIDDEN_SEARCH_TXT, B_CLOSE_DIV, B_START_CAT, B_END }, // P_START_CAT
  { B_START_HTML, B_HIDDEN_CAT, B_HIDDEN_ARRANGE, B_HIDDEN_SEARCH_TXT, B_CLOSE_DIV, B_CAT_NAME, B_END }, // P_CAT_NAME
  { B_START_HTML, B_HIDDEN_ARRANGE, B_HIDDEN_CAT_NAME, B_HIDDEN_SEARCH_TXT, B_SELECT_CREATE_CAT, B_END }, // P_SELECT_CREATE_CAT
  { B_START_HTML, B_HIDDEN_ARRANGE, B_HIDDEN_CAT_NAME, B_HIDDEN_SEARCH_TXT, B_SELECT_CAT, B_END }, // P_SELECT_CAT
  { B_START_HTML, B_HIDDEN_ARRANGE, B_HIDDEN_CAT_NAME, B_HIDDEN_SEARCH_TXT, B_SELECT_SEND_CAT, B_END }, // P_SELECT_SEND_CAT
  { B_START_HTML, B_HIDDEN_CAT, B_HIDDEN_CAT_NAME, B_HIDDEN_SEARCH_TXT, B_SELECT_ARRANGE, B_END }, // P_SELECT_ARRANGE
  { B_START_HTML, B_HIDDEN_CAT, B_HIDDEN_CAT_NAME, B_HIDDEN_SEARCH_TXT, B_SELECT_CARD_ARRANGE, B_END }, // P_SELECT_CARD_ARRANGE
  { B_START_HTML, B_HIDDEN_ARRANGE, B_HIDDEN_CAT_NAME, B_HIDDEN_SEARCH_TXT, B_CLOSE_DIV, B_SELECT_EDIT_CAT, B_END }, // P_SELECT_EDIT_CAT
  { B_START_HTML, B_HIDDEN_CAT, B_HIDDEN_ARRANGE, B_HIDDEN_CAT_NAME, B_HIDDEN_SEARCH_TXT, B_CLOSE_DIV, B_EDIT, B_END }, // P_EDIT
  { B_START_HTML, B_HIDDEN_ARRANGE, B_HIDDEN_CAT_NAME, B_HIDDEN_SEARCH_TXT, B_CLOSE_DIV, B_SELECT_LEARN_CAT, B_END }, // P_SELECT_LEARN_CAT
  { B_START_HTML, B_HIDDEN_ARRANGE, B_HIDDEN_CAT_NAME, B_HIDDEN_SEARCH_TXT, B_CLOSE_DIV, B_SELECT_SEARCH_CAT, B_END }, // P_SELECT_SEARCH_CAT
  { B_START_HTML, B_HIDDEN_CAT, B_HIDDEN_ARRANGE, B_HIDDEN_CAT_NAME, B_CLOSE_DIV, B_SEARCH, B_END }, // P_SEARCH
  { B_START_HTML, B_HIDDEN_CAT, B_HIDDEN_ARRANGE, B_HIDDEN_CAT_NAME, B_HIDDEN_SEARCH_TXT, B_CLOSE_DIV, B_PREFERENCES, B_END }, // P_PREFERENCES
  { B_START_HTML, B_HIDDEN_CAT, B_HIDDEN_ARRANGE, B_HIDDEN_CAT_NAME, B_HIDDEN_SEARCH_TXT, B_CLOSE_DIV, B_ABOUT, B_END }, // P_ABOUT
  { B_START_HTML, B_HIDDEN_CAT, B_HIDDEN_ARRANGE, B_HIDDEN_CAT_NAME, B_HIDDEN_SEARCH_TXT, B_CLOSE_DIV, B_LEARN, B_END }, // P_LEARN
  { B_START_HTML, B_HIDDEN_CAT, B_HIDDEN_ARRANGE, B_HIDDEN_CAT_NAME, B_HIDDEN_SEARCH_TXT, B_MSG, B_END }, // P_MSG
  { B_START_HTML, B_HIDDEN_CAT, B_HIDDEN_ARRANGE, B_HIDDEN_CAT_NAME, B_HIDDEN_SEARCH_TXT, B_CLOSE_DIV, B_HISTOGRAM, B_END } // P_HISTOGRAM
};

static const char *DATA_PATH = "/var/www/memorysurfer";

static const char *ARRANGE[] = { "Before", "Below", "Behind" };

struct StringArray {
  int sa_c; // count
  char *sa_d; // data
  size_t sa_n;
};

#pragma pack(push)
#pragma pack(1)
struct Category {
  int32_t cat_cli; // card list index
  char cat_x;
  char unused;
  int16_t cat_n_sibling; // next
  int16_t cat_n_child;
  char cat_used;
  char cat_on;
};
#pragma pack(pop)

#pragma pack(push)
#pragma pack(1)
struct Card {
  int64_t card_time;
  int32_t card_strength;
  int32_t card_qai; // question/answer index
  char card_state;
};

struct Password {
  uint8_t pw_msg_digest[SHA1_HASH_SIZE];
  int8_t pw_flag; // password set?
  uint8_t tok_sec;
  uint8_t tok_count;
};
#pragma pack(pop)

struct MemorySurfer {
  struct IndexedMemoryFile imf;
  char *imf_filename;
  struct StringArray cat_sa;
  struct Category *cat_t; // tree
  int cat_a; // allocated
  int16_t n_first;
  int cat_i;
  int mov_cat_i; // moved
  int arrange;
  char *cat_name;
  struct Card *card_l;
  int card_a;
  int card_i;
  int mov_card_i;
  int cards_nel; // n eligible
  struct StringArray card_sa;
  time_t timestamp;
  int lvl; // level
  char *search_txt;
  int8_t srch_dir; // search
  int8_t can_resume;
  char *password;
  char *new_password;
  struct Password passwd;
};

static const int32_t SA_INDEX = 2; // StringArray
static const int32_t C_INDEX = 3; // Categories
static const int32_t PW_INDEX = 4; // Password

struct Multi
{
  char *delim_str[2];
  size_t delim_len[2];
  ssize_t nread;
  char *post_lp; // lineptr
  size_t post_n;
  int post_wp; // write pointer
  int post_fp; // found pointer
};

struct XML
{
  char *p_lineptr; // parse
  size_t n;
  int prev_cat_i;
  int parsed_cat_i;
};

struct WebMemorySurfer {
  struct MemorySurfer ms;
  enum Sequence seq;
  enum Page page;
  int from_page;
  enum Mode mode;
  enum Mode saved_mode;
  int timeout;
  struct Multi mult;
  struct XML xml;
  char *boundary_str;
  char *dbg_lp;
  size_t dbg_n;
  int dbg_wp;
  char *file_title_str;
  char **fl_v; // filelist vector
  int fl_c; // count
  struct StringArray qa_sa;
  int reveal_pos;
  int saved_reveal_pos;
  int sw_i;
  const char *static_msg;
  char *static_btn_main; // left
  char *static_btn_alt; // right
  int todo_main;
  int todo_alt;
  size_t html_n;
  char *html_lp;
  char *found_str;
  int32_t mtime[2];
  int hist_bucket[100]; // histogram
  int hist_max;
  int lvl_bucket[21];
  uint8_t tok_digest[SHA1_HASH_SIZE];
  char tok_str[41];
};

static int append_part(struct WebMemorySurfer *wms)
{
  int e;
  int i;
  char ch;
  e = 0;
  if (wms->dbg_wp + wms->mult.nread + 1 > wms->dbg_n) {
    wms->dbg_n = (wms->dbg_wp + wms->mult.nread + 1 + 120) & 0xfffffff8;
    e = wms->dbg_n > INT32_MAX;
    if (e == 0) {
      wms->dbg_lp = realloc(wms->dbg_lp, wms->dbg_n);
      e = wms->dbg_lp == NULL;
    }
  }
  if (e == 0) {
    for (i = 0; i < wms->mult.nread; i++) {
      ch = wms->mult.post_lp[i];
      wms->dbg_lp[wms->dbg_wp++] = ch;
    }
    wms->dbg_lp[wms->dbg_wp] = '\0';
  }
  return e;
}

static int percent2c(char *str, size_t len)
{
  int e;
  int rp; // read pos
  int wp; // write pos
  char r_ch; // read char
  unsigned char w_ch; // write char
  static const char table[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
  int i;
  e = 0;
  rp = 0;
  wp = 0;
  while (rp < len && e == 0)
  {
    r_ch = str[rp];
    if (r_ch == '+')
      str[wp] = ' ';
    else if (r_ch == '%') {
      if ((str[rp + 1] == '0') && (str[rp + 2] == 'D')) {
        if (str[rp + 3] == '%') {
          if ((str[rp + 4] == '0') && (str[rp + 5] == 'A')) {
            str[wp] = '\n';
            rp += 5;
          }
        }
      }
      else {
        e = -1;
        rp++;
        r_ch = str[rp];
        for (i = 0; i < 16 && e != 0; i++) {
          if (r_ch == table[i]) {
            w_ch = i * 16;
            e = 0;
          }
        }
        if (e == 0) {
          e = -1;
          rp++;
          r_ch = str[rp];
          for (i = 0; i < 16 && e != 0; i++) {
            if (r_ch == table[i]) {
              w_ch |= i;
              e = 0;
            }
          }
          if (e == 0) {
            if (w_ch < 32)
              e = w_ch != '\t';
            else
              e = w_ch == 127;
            if (e == 0)
              str[wp] = w_ch;
          }
        }
      }
    }
    else
      str[wp] = str[rp];
    rp++;
    wp++;
  }
  str[wp] = '\0';
  return e;
}

static int sa_set(struct StringArray *sa, int16_t sa_i, char *sa_str)
{
  int e;
  assert(sa_str != NULL);
  assert(sa_i >= 0);
  char *sa_d;
  char ch;
  size_t size;
  int pos_s; // size
  int i;
  int pos_w; // write
  int pos_r; // read
  int pos_n; // new
  int sa_c;
  size = strlen(sa_str) + 1;
  pos_s = 0;
  for (i = 0; i < sa->sa_c; i++) {
    if (i != sa_i) {
      do {
        size++;
        ch = sa->sa_d[pos_s++];
      }
      while (ch != '\0');
    }
    else {
        do {
          ch = sa->sa_d[pos_s++];
        }
        while (ch != '\0');
    }
  }
  sa_c = sa->sa_c;
  if (sa_i >= sa->sa_c) {
    size += sa_i - sa->sa_c;
    sa_c = sa_i + 1;
  }
  sa_d = malloc(size);
  e = sa_d == NULL;
  if (e == 0) {
    pos_w = 0;
    pos_r = 0;
    pos_n = 0;
    for (i = 0; i < sa_c; i++) {
      if (i != sa_i) {
        if (i < sa->sa_c) {
          do {
            ch = sa->sa_d[pos_r++];
            sa_d[pos_w++] = ch;
          }
          while (ch != '\0');
        }
        else {
          sa_d[pos_w++] = '\0';
        }
      }
      else {
        do {
          ch = sa_str[pos_n++];
          sa_d[pos_w++] = ch;
        }
        while (ch != '\0');
        if (i < sa->sa_c) {
          do {
            ch = sa->sa_d[pos_r++];
          }
          while (ch != '\0');
        }
      }
    }
    assert(pos_w == size);
    free(sa->sa_d);
    sa->sa_d = sa_d;
    sa->sa_n = size;
    sa->sa_c = sa_c;
  }
  return e;
}

static int multi_delim (struct Multi *mult)
{
  int e;
  int i;
  int ch;
  char *post_lp;
  size_t post_n;
  int cmp_i;
  int post_tp; // test pointer
  assert((mult->post_lp == NULL && mult->post_n == 0) || (mult->post_lp != NULL && mult->post_n > 0));
  for (i = 0; i < 2; i++) {
    if (mult->delim_str[i] != NULL)
      mult->delim_len[i] = strlen (mult->delim_str[i]);
    else
      mult->delim_len[i] = INT_MAX;
  }
  e = 0;
  mult->nread = -1;
  cmp_i = -1;
  mult->post_wp = 0;
  mult->post_fp = -1;
  do
  {
    if (mult->post_wp + 1 >= mult->post_n) {
      post_n = mult->post_wp + 120;
      e = post_n >= INT_MAX;
      if (e == 0) {
        post_lp = (char *) realloc(mult->post_lp, post_n);
        e = post_lp == NULL;
        if (e == 0) {
          mult->post_lp = post_lp;
          mult->post_n = post_n;
        }
      }
    }
    if (e == 0) {
      ch = fgetc(stdin);
      if (ch != EOF) {
        mult->post_lp[mult->post_wp] = ch;
        mult->post_wp++;
        for (i = 0; i < 2; i++) {
          post_tp = mult->post_wp - mult->delim_len[i];
          if (post_tp >= 0)
            cmp_i = memcmp(mult->post_lp + post_tp, mult->delim_str[i], mult->delim_len[i]);
          if (cmp_i == 0) {
            mult->post_fp = post_tp;
            break;
          }
        }
      }
      else
        break;
    }
  } while (mult->post_fp < 0 && e == 0);
  mult->post_lp[mult->post_wp] = '\0';
  if (mult->post_wp > 0)
    mult->nread = mult->post_wp;
  return e;
}

enum Tag { TAG_ROOT, TAG_MEMORYSURFER, TAG_CATEGORY, TAG_NAME, TAG_PAIR, TAG_TIME, TAG_STRENGTH, TAG_STATE, TAG_QUESTION, TAG_ANSWER };

static int
sa_length (struct StringArray *sa)
{
  int pos_l; // length
  int i;
  pos_l = 0;
  for (i = 0; i < sa->sa_c; i++)
  {
    while (sa->sa_d[pos_l++] != '\0');
  }
  return pos_l;
}

int xml_unescape (char *xml_str)
{
  int e;
  int rp;
  int wp;
  char ch;
  assert (xml_str != NULL);
  rp = 0;
  wp = 0;
  e = 0;
  do
  {
    ch = xml_str[rp++];
    if (ch == '&')
    {
      if (memcmp (xml_str + rp, "amp;", 4) == 0)
      {
        rp += 4;
        ch = '&';
      }
      else
      {
        e = memcmp (xml_str + rp, "lt;", 3);
        if (e == 0)
        {
          rp += 3;
          ch = '<';
        }
      }
    }
    xml_str[wp++] = ch;
  } while (ch != '\0' && e == 0);
  return e;
}

int parse_xml (struct WebMemorySurfer *wms, enum Tag tag, int parent_cat_i)
{
  int e;
  ssize_t nread;
  int do_flag;
  int cat_i;
  char *str;
  int len;
  int i;
  int cat_a;
  size_t size;
  int ref_cat_i;
  struct tm bd_time; // broken-down
  int a_n; // assignments
  time_t simple_time;
  int slash_f; // flag
  int32_t index;
  int32_t data_size;
  do_flag = 1;
  cat_i = -1;
  do
  {
    nread = getdelim (&wms->xml.p_lineptr, &wms->xml.n, '<', stdin);
    e = nread <= 0;
    if (e == 0)
    {
      if (do_flag != 0)
      {
        do_flag = 0;
        switch (tag)
        {
        case TAG_ROOT:
        case TAG_MEMORYSURFER:
          break;
        case TAG_CATEGORY:
          cat_i = 0;
          while (cat_i < wms->ms.cat_a && wms->ms.cat_t[cat_i].cat_used != 0)
            cat_i++;
          if (cat_i == wms->ms.cat_a)
          {
            cat_a = wms->ms.cat_a + 8;
            size = sizeof (struct Category) * cat_a;
            wms->ms.cat_t = realloc (wms->ms.cat_t, size);
            e = wms->ms.cat_t == NULL;
            if (e == 0)
            {
              for (i = wms->ms.cat_a; i < cat_a; i++)
                wms->ms.cat_t[i].cat_used = 0;
              wms->ms.cat_a = cat_a;
            }
          }
          if (e == 0)
          {
            ref_cat_i = -1;
            if (wms->xml.prev_cat_i != -1)
            {
              ref_cat_i = wms->xml.prev_cat_i;
              wms->ms.cat_t[ref_cat_i].cat_n_sibling = cat_i;
            }
            else if (parent_cat_i != -1)
            {
              ref_cat_i = parent_cat_i;
              wms->ms.cat_t[ref_cat_i].cat_n_child = cat_i;
            }
            wms->xml.prev_cat_i = -1;
            if (wms->xml.parsed_cat_i != -1)
            {
              e = imf_seek_unused (&wms->ms.imf, &index);
              if (e == 0)
              {
                assert (wms->ms.card_a >= 0);
                data_size = wms->ms.card_a * sizeof (struct Card);
                assert (wms->ms.card_l);
                e = imf_put (&wms->ms.imf, index, wms->ms.card_l, data_size);
                if (e == 0)
                {
                  wms->ms.cat_t[wms->xml.parsed_cat_i].cat_cli = index;
                }
              }
            }
            if (e == 0)
            {
              wms->ms.cat_t[cat_i].cat_cli = -1;
              wms->ms.cat_t[cat_i].cat_x = 1;
              wms->ms.cat_t[cat_i].cat_n_sibling = -1;
              wms->ms.cat_t[cat_i].cat_n_child = -1;
              wms->ms.cat_t[cat_i].cat_used = 1;
              wms->ms.cat_t[cat_i].cat_on = 1;
            }
          }
          wms->ms.card_a = 0;
          wms->ms.card_i = -1;
          wms->xml.parsed_cat_i = cat_i;
          break;
        case TAG_NAME:
          str = wms->xml.p_lineptr;
          str[nread - 1] = '\0';
          assert(cat_i < INT16_MAX);
          e = sa_set(&wms->ms.cat_sa, parent_cat_i, str);
          break;
        case TAG_PAIR:
          wms->ms.card_a++;
          size = wms->ms.card_a * sizeof (struct Card);
          wms->ms.card_l = realloc (wms->ms.card_l, size);
          e = wms->ms.card_l == NULL;
          if (e == 0)
          {
            wms->ms.card_i++;
          }
          break;
        case TAG_TIME:
          memset (&bd_time, 0, sizeof (bd_time));
          str = wms->xml.p_lineptr;
          str[nread - 1] = '\0';
          a_n = sscanf (str, "%d-%d-%dT%d:%d:%d", &bd_time.tm_year, &bd_time.tm_mon, &bd_time.tm_mday,
                                                  &bd_time.tm_hour, &bd_time.tm_min, &bd_time.tm_sec);
          e = a_n != 6;
          if (e == 0)
          {
            bd_time.tm_mon -= 1;
            bd_time.tm_year -= 1900;
            simple_time = timegm (&bd_time);
            e = simple_time == -1;
            if (e == 0)
            {
              wms->ms.card_l[wms->ms.card_i].card_time = simple_time;
            }
          }
          break;
        case TAG_STRENGTH:
          str = wms->xml.p_lineptr;
          str[nread - 1] = '\0';
          a_n = sscanf (str, "%d", &wms->ms.card_l[wms->ms.card_i].card_strength);
          e = a_n != 1;
          break;
        case TAG_STATE:
          str = wms->xml.p_lineptr;
          wms->ms.card_l[wms->ms.card_i].card_state = str[0] - '0';
          e = wms->ms.card_l[wms->ms.card_i].card_state < 0 || wms->ms.card_l[wms->ms.card_i].card_state > 3;
          break;
        case TAG_QUESTION:
          str = wms->xml.p_lineptr;
          str[nread - 1] = '\0';
          e = xml_unescape (str);
          if (e == 0)
            e = sa_set(&wms->ms.card_sa, 0, str);
          break;
        case TAG_ANSWER:
          str = wms->xml.p_lineptr;
          str[nread - 1] = '\0';
          e = xml_unescape (str);
          if (e == 0)
            e = sa_set(&wms->ms.card_sa, 1, str);
          break;
        }
      }
      nread = getdelim (&wms->xml.p_lineptr, &wms->xml.n, '>', stdin);
      e = nread <= 0;
      if (e == 0)
      {
        str = wms->xml.p_lineptr;
        len = nread - 1;
        slash_f = str[0] == '/';
        if (slash_f != 0)
        {
          str++;
          len--;
        }
        switch (len)
        {
        case 12:
          e = memcmp(str, "memorysurfer", 12);
          if (e == 0) {
            if (slash_f == 0) {
              e = tag != TAG_ROOT;
              if (e == 0)
                e = parse_xml (wms, TAG_MEMORYSURFER, parent_cat_i);
            }
            else
              e = tag != TAG_MEMORYSURFER;
          }
          break;
        case 8:
          if (memcmp (str, "strength", 8) == 0)
          {
            if (slash_f == 0)
            {
              e = tag != TAG_PAIR;
              if (e == 0)
              {
                e = parse_xml (wms, TAG_STRENGTH, parent_cat_i);
              }
            }
            else
            {
              e = tag != TAG_STRENGTH;
            }
          }
          else {
            e = memcmp(str, "question", 8);
            if (e == 0) {
              if (slash_f == 0)
              {
                e = tag != TAG_PAIR;
                if (e == 0)
                  e = parse_xml(wms, TAG_QUESTION, parent_cat_i);
              }
              else
                e = tag != TAG_QUESTION;
            }
          }
          break;
        case 6:
          e = memcmp (str, "answer", 6);
          if (e == 0)
          {
            if (slash_f == 0)
            {
              e = tag != TAG_PAIR;
              if (e == 0)
              {
                e = parse_xml (wms, TAG_ANSWER, parent_cat_i);
              }
            }
            else
            {
              e = tag != TAG_ANSWER;
            }
          }
          break;
        case 5:
          e = memcmp (str, "state", 5);
          if (e == 0)
          {
            if (slash_f == 0)
            {
              e = tag != TAG_PAIR;
              if (e == 0)
              {
                e = parse_xml (wms, TAG_STATE, parent_cat_i);
              }
            }
            else
            {
              e = tag != TAG_STATE;
            }
          }
          break;
        case 4:
          if (memcmp(str, "card", 4) == 0) {
            if (slash_f == 0) {
              e = tag != TAG_CATEGORY;
              if (e == 0)
                e = parse_xml(wms, TAG_PAIR, cat_i);
            }
            else {
              e = tag != TAG_PAIR;
              if (e == 0) {
                e = imf_seek_unused (&wms->ms.imf, &index);
                if (e == 0) {
                  data_size = sa_length (&wms->ms.card_sa);
                  e = imf_put (&wms->ms.imf, index, wms->ms.card_sa.sa_d, data_size);
                  if (e == 0)
                    wms->ms.card_l[wms->ms.card_i].card_qai = index;
                }
              }
            }
          }
          else if (memcmp (str, "time", 4) == 0)
          {
            if (slash_f == 0)
            {
              e = tag != TAG_PAIR;
              if (e == 0)
              {
                e = parse_xml (wms, TAG_TIME, parent_cat_i);
              }
            }
            else
            {
              e = tag != TAG_TIME;
            }
          }
          else if (memcmp(str, "name", 4) == 0) {
            if (slash_f == 0) {
              e = tag != TAG_CATEGORY;
              if (e == 0)
                e = parse_xml (wms, TAG_NAME, cat_i);
            }
            else
              e = tag != TAG_NAME;
          }
          else {
            e = memcmp(str, "deck", 4);
            if (e == 0) {
              if (slash_f == 0) {
                e = tag != TAG_MEMORYSURFER && tag != TAG_CATEGORY;
                if (e == 0)
                  e = parse_xml(wms, TAG_CATEGORY, cat_i);
              }
              else {
                e = tag != TAG_CATEGORY;
                if (e == 0) {
                  if (wms->xml.parsed_cat_i == cat_i) {
                    assert(wms->xml.parsed_cat_i != -1);
                    e = imf_seek_unused(&wms->ms.imf, &index);
                    if (e == 0) {
                      assert(wms->ms.card_a >= 0);
                      data_size = wms->ms.card_a * sizeof(struct Card);
                      assert(wms->ms.card_l);
                      e = imf_put(&wms->ms.imf, index, wms->ms.card_l, data_size);
                      if (e == 0) {
                        wms->ms.cat_t[wms->xml.parsed_cat_i].cat_cli = index;
                        wms->xml.parsed_cat_i = -1;
                      }
                    }
                  }
                  wms->xml.prev_cat_i = cat_i;
                }
              }
            }
          }
          break;
        default:
          e = -1;
          break;
        }
      }
    }
  } while (slash_f == 0 && e == 0 && tag != TAG_ROOT);
  return e;
}

int sa_load (struct StringArray *sa, struct IndexedMemoryFile *imf, int32_t index)
{
  int e;
  int32_t data_size;
  int pos_c; // count
  char *sa_d;
  data_size = imf_get_size (imf, index);
  e = 0;
  if (sa->sa_n < data_size)
  {
    sa_d = realloc (sa->sa_d, data_size);
    e = sa_d == NULL;
    if (e == 0)
    {
      sa->sa_d = sa_d;
      sa->sa_n = data_size;
    }
  }
  if (e == 0)
  {
    e = imf_get (imf, index, sa->sa_d);
    if (e == 0)
    {
      sa->sa_c = 0;
      pos_c = 0;
      while (pos_c < data_size)
      {
        sa->sa_c += !sa->sa_d[pos_c++];
      }
    }
  }
  return e;
}

int ms_open(struct MemorySurfer *ms)
{
  int e;
  int i;
  int32_t data_size;
  int16_t n_prev;
  e = ms->imf_filename == NULL || ms->cat_t != NULL || ms->cat_a != 0 || ms->n_first != -1;
  if (e == 0) {
    e = imf_open(&ms->imf, ms->imf_filename);
    if (e == 0) {
      e = sa_load(&ms->cat_sa, &ms->imf, SA_INDEX);
      if (e == 0) {
        data_size = imf_get_size(&ms->imf, C_INDEX);
        if (data_size > 0) {
          ms->cat_t = malloc(data_size);
          e = ms->cat_t == NULL;
        }
        if (e == 0) {
          ms->cat_a = data_size / sizeof(struct Category);
          e = imf_get(&ms->imf, C_INDEX, ms->cat_t);
          if (e == 0) {
            do {
              n_prev = -1;
              for (i = 0; i < ms->cat_a && n_prev == -1; i++)
                if (ms->cat_t[i].cat_used != 0)
                  if (ms->cat_t[i].cat_n_sibling == ms->n_first || ms->cat_t[i].cat_n_child == ms->n_first)
                    n_prev = i;
              if (n_prev != -1) {
                e = n_prev == ms->n_first;
                if (e == 0)
                  ms->n_first = n_prev;
                else
                  e = 0x02d3171a; // MSODHC MemorySurfer ms_open decks hierarchy (is) corrupted
              }
            }
            while (n_prev != -1 && e == 0);
          }
        }
      }
    }
  }
  else
    e = 0x052e8351; // WMSOAF (Web)MemorySurfer ms_open assert failed
  return e;
}

static int scan_hex(uint8_t *data, char *str, size_t len)
{
  int e;
  int i;
  char ch;
  int nibble[2];
  e = 0;
  while (len--) {
    i = 2;
    while (i--) {
      ch = str[len * 2 + i];
      if (ch >= 'a' && ch <= 'f')
        nibble[i] = ch - 'a' + 10;
      else if (ch >= '0' && ch <= '9')
        nibble[i] = ch - '0';
      else
        e = -1;
    }
    data[len] = nibble[0] << 4 | nibble[1];
  }
  return e;
}

int parse_post(struct WebMemorySurfer *wms) {
  int e;
  int a_n; // assignments
  char *str;
  enum Stage stage;
  int len;
  int deci; // decision
  enum Field field;
  size_t size;
  e = 0;
  wms->mult.delim_str[0] = "=";
  wms->mult.delim_str[1] = "--";
  stage = T_NULL;
  field = F_UNKNOWN;
  do
  {
    e = multi_delim(&wms->mult);
    if (wms->mult.nread > 0 && e == 0)
      e = append_part(wms);
    if (e == 0) {
      switch (stage)
      {
      case T_NULL:
        if (wms->mult.nread > 0) {
          if (wms->mult.post_fp > 0 && wms->mult.post_lp[wms->mult.post_fp] == '=') {
            stage = T_URLENCODE;
          }
          else {
            e = wms->mult.post_fp != 0 || wms->mult.nread != 2 || memcmp(wms->mult.post_lp, "--", 2) != 0;
            if (e == 0) {
              stage = T_BOUNDARY_INIT;
              wms->mult.delim_str[0] = "\r\n";
              wms->mult.delim_str[1] = NULL;
              break;
            }
            else
              break;
          }
        }
        else
          break;
      case T_URLENCODE:
        if (wms->mult.post_fp >= 0)
          str = wms->mult.post_lp + wms->mult.post_fp;
        else if (field != F_UNKNOWN)
          str = "&";
        else
          str = NULL;
        if (str != NULL) {
          if (memcmp (str, "=", 1) == 0) {
            assert(field == F_UNKNOWN);
            e = wms->mult.post_fp < 0;
            if (e == 0) {
              switch (wms->mult.post_fp)
              {
              case 1:
                if (memcmp(wms->mult.post_lp, "q", 1) == 0)
                  field = F_Q;
                else {
                  e = memcmp(wms->mult.post_lp, "a", 1);
                  if (e == 0)
                    field = F_A;
                }
                break;
              case 3:
                if (memcmp(wms->mult.post_lp, "cat", 3) == 0)
                  field = F_CAT;
                else {
                  e = memcmp(wms->mult.post_lp, "lvl", 3);
                  if (e == 0)
                    field = F_LVL;
                }
                break;
              case 4:
                if (memcmp (wms->mult.post_lp, "page", 4) == 0)
                  field = F_PAGE;
                else if (memcmp (wms->mult.post_lp, "mode", 4) == 0)
                  field = F_MODE;
                else {
                  e = memcmp (wms->mult.post_lp, "card", 4);
                  if (e == 0)
                    field = F_CARD;
                }
                break;
              case 5:
                if (memcmp(wms->mult.post_lp, "event", 5) == 0)
                  field = F_EVENT;
                else if (memcmp(wms->mult.post_lp, "token", 5) == 0)
                  field = F_TOKEN;
                else {
                  e = memcmp (wms->mult.post_lp, "mtime", 5);
                  if (e == 0)
                   field = F_MTIME;
                }
                break;
              case 7:
                if (memcmp(wms->mult.post_lp, "arrange", 7) == 0)
                  field = F_ARRANGE;
                else if (memcmp(wms->mult.post_lp, "mov-cat", 7) == 0)
                  field = F_MOVED_CAT;
                else {
                  e = memcmp(wms->mult.post_lp, "timeout", 7);
                  if (e == 0)
                    field = F_TIMEOUT;
                }
                break;
              case 8:
                if (memcmp (wms->mult.post_lp, "filename", 8) == 0)
                  field = F_FILENAME;
                else if (memcmp (wms->mult.post_lp, "cat_name", 8) == 0)
                  field = F_CAT_NAME;
                else if (memcmp (wms->mult.post_lp, "mov-card", 8) == 0)
                  field = F_MOV_CARD;
                else if (memcmp (wms->mult.post_lp, "todo_alt", 8) == 0)
                  field = F_TODO_ALT;
                else {
                  e = memcmp (wms->mult.post_lp, "password", 8);
                  if (e == 0)
                    field = F_PASSWORD;
                }
                break;
              case 9:
                e = memcmp(wms->mult.post_lp, "todo_main", 9);
                if (e == 0)
                  field = F_TODO_MAIN;
                break;
              case 10:
                if (memcmp(wms->mult.post_lp, "reveal-pos", 10) == 0)
                  field = F_REVEAL_POS;
                else if (memcmp(wms->mult.post_lp, "search_txt", 10) == 0)
                  field = F_SEARCH_TXT;
                else {
                  e = memcmp(wms->mult.post_lp, "file-title", 10);
                  if (e == 0)
                    field = F_FILE_TITLE;
                }
                break;
              case 11:
                if (memcmp (wms->mult.post_lp, "file_action", 11) == 0)
                  field = F_FILE_ACTION;
                else {
                  e = memcmp (wms->mult.post_lp, "edit_action", 11);
                  if (e == 0)
                    field = F_EDIT_ACTION;
                }
                break;
              case 12:
                if (memcmp (wms->mult.post_lp, "start_action", 12) == 0)
                  field = F_START_ACTION;
                else if (memcmp (wms->mult.post_lp, "learn_action", 12) == 0)
                  field = F_LEARN_ACTION;
                else {
                  e = memcmp (wms->mult.post_lp, "new-password", 12);
                  if (e == 0)
                    field = F_NEW_PASSWORD;
                }
                break;
              case 13:
                e = memcmp(wms->mult.post_lp, "search_action", 13);
                if (e == 0)
                  field = F_SEARCH_ACTION;
                break;
              default:
                e = -1;
                break;
              }
            }
            wms->mult.delim_str[0] = "&";
            wms->mult.delim_str[1] = NULL;
          }
          else {
            e = strncmp(str, "&", 1) != 0;
            if (e == 0) {
              switch (field)
              {
              case F_FILENAME:
                assert (wms->ms.imf_filename == NULL);
                wms->ms.imf_filename = malloc (wms->mult.nread);
                e = wms->ms.imf_filename == NULL;
                if (e == 0)
                {
                  assert (wms->mult.post_lp[wms->mult.nread - 1] == '&');
                  memcpy (wms->ms.imf_filename, wms->mult.post_lp, wms->mult.nread);
                  e = percent2c (wms->ms.imf_filename, wms->mult.nread - 1);
                }
                break;
              case F_FILE_TITLE:
                assert(wms->file_title_str == NULL);
                wms->file_title_str = malloc(wms->mult.nread);
                e = wms->file_title_str == NULL;
                if (e == 0) {
                  assert(wms->mult.post_lp[wms->mult.nread - 1] == '&');
                  memcpy(wms->file_title_str, wms->mult.post_lp, wms->mult.nread);
                  e = percent2c(wms->file_title_str, wms->mult.nread - 1);
                }
                break;
              case F_START_ACTION:
                if (memcmp (wms->mult.post_lp, "File", 4) == 0)
                {
                  wms->seq = S_FILE;
                }
                else if (memcmp (wms->mult.post_lp, "Categories", 10) == 0)
                {
                  wms->seq = S_START_CAT;
                }
                else if (memcmp (wms->mult.post_lp, "Edit", 4) == 0)
                {
                  wms->seq = S_SELECT_EDIT_CAT;
                }
                else if (memcmp (wms->mult.post_lp, "Learn", 5) == 0)
                {
                  wms->seq = S_SELECT_LEARN_CAT;
                }
                else if (memcmp (wms->mult.post_lp, "Search", 6) == 0)
                {
                  wms->seq = S_SELECT_SEARCH_CAT;
                }
                break;
              case F_FILE_ACTION:
                if (memcmp (wms->mult.post_lp, "New", 3) == 0)
                  wms->seq = S_NEW;
                else if (memcmp (wms->mult.post_lp, "Import", 6) == 0)
                  wms->seq = S_WARNING;
                else if (memcmp (wms->mult.post_lp, "Export", 6) == 0)
                  wms->seq = S_EXPORT;
                else if (memcmp (wms->mult.post_lp, "Close", 5) == 0)
                  wms->seq = S_CLOSE;
                else if (memcmp(wms->mult.post_lp, "Cancel", 6) == 0)
                  if (wms->file_title_str != NULL)
                    wms->seq = S_START;
                  else
                    wms->seq = S_NONE;
                else if (memcmp (wms->mult.post_lp, "Enter", 5) == 0)
                  wms->seq = S_ENTER;
                else if (memcmp (wms->mult.post_lp, "Login", 5) == 0)
                  wms->seq = S_LOGIN;
                else if (memcmp (wms->mult.post_lp, "Create", 6) == 0)
                  wms->seq = S_CREATE;
                else if (memcmp (wms->mult.post_lp, "Stop", 4) == 0) {
                  wms->seq = S_FILE;
                  if ((wms->from_page == P_PASSWORD && wms->saved_mode == M_DEFAULT) || wms->from_page == P_NEW)
                    wms->seq = S_CLOSE;
                }
                else if (memcmp(wms->mult.post_lp, "Password", 8) == 0)
                  wms->seq = S_GO_CHANGE;
                else if (memcmp(wms->mult.post_lp, "Change", 6) == 0)
                  wms->seq = S_CHANGE;
                else {
                  e = memcmp(wms->mult.post_lp, "OK", 2);
                  if (e == 0)
                    wms->seq = S_START;
                }
                break;
              case F_ARRANGE:
                assert (wms->ms.arrange == -1);
                a_n = sscanf (wms->mult.post_lp, "%d", &wms->ms.arrange);
                assert ((wms->ms.arrange >= 0) && (wms->ms.arrange <= 2));
                e = a_n != 1;
                break;
              case F_CAT_NAME:
                e = wms->ms.cat_name != NULL;
                if (e == 0) {
                  wms->ms.cat_name = malloc(wms->mult.nread);
                  e = wms->ms.cat_name == NULL;
                  if (e == 0) {
                    assert(wms->mult.post_lp[wms->mult.nread - 1] == '&');
                    memcpy(wms->ms.cat_name, wms->mult.post_lp, wms->mult.nread);
                    e = percent2c(wms->ms.cat_name, wms->mult.nread - 1);
                  }
                }
                if (e != 0)
                  e = 0x00729d2e; // WPPCN Web(MemorySurfer) parse_post (of) cat-name (failed)
                break;
              case F_MOVED_CAT:
                assert (wms->ms.mov_cat_i == -1);
                a_n = sscanf (wms->mult.post_lp, "%d", &wms->ms.mov_cat_i);
                e = a_n != 1;
                break;
              case F_EDIT_ACTION:
                if (strncmp(wms->mult.post_lp, "Append", 6) == 0)
                  wms->seq = S_APPEND;
                else if (strncmp(wms->mult.post_lp, "Delete", 6) == 0)
                  wms->seq = S_ASK_DELETE_CARD;
                else if (strncmp(wms->mult.post_lp, "Previous", 8) == 0)
                  wms->seq = S_PREVIOUS;
                else if (strncmp(wms->mult.post_lp, "Next", 4) == 0)
                  wms->seq = S_NEXT;
                else if (strncmp(wms->mult.post_lp, "Schedule", 8) == 0)
                  wms->seq = S_SCHEDULE;
                else if (strncmp(wms->mult.post_lp, "Stop", 4) == 0)
                  wms->seq = S_SELECT_EDIT_CAT;
                else if (strncmp(wms->mult.post_lp, "Search", 6) == 0)
                  wms->seq = S_SEARCH_SYNCED;
                else if (strncmp(wms->mult.post_lp, "Cancel", 6) == 0)
                  wms->seq = S_START;
                break;
              case F_LEARN_ACTION:
                if (memcmp (wms->mult.post_lp, "Learn", 5) == 0)
                  wms->seq = S_QUESTION;
                else if (memcmp (wms->mult.post_lp, "Suspend", 7) == 0)
                  wms->seq = S_SUSPEND;
                else if (memcmp (wms->mult.post_lp, "Stop", 4) == 0)
                  wms->seq = S_SELECT_LEARN_CAT;
                else if (memcmp (wms->mult.post_lp, "Proceed", 7) == 0)
                  wms->seq = S_PROCEED;
                else if (memcmp(wms->mult.post_lp, "Cancel", 6) == 0)
                  wms->seq = S_START;
                break;
              case F_SEARCH_TXT:
                assert (wms->ms.search_txt == NULL);
                wms->ms.search_txt = malloc (wms->mult.nread);
                e = wms->ms.search_txt == NULL;
                if (e == 0)
                {
                  assert (wms->mult.post_lp[wms->mult.nread - 1] == '&');
                  memcpy (wms->ms.search_txt, wms->mult.post_lp, wms->mult.nread);
                  e = percent2c (wms->ms.search_txt, wms->mult.nread - 1);
                }
                break;
              case F_SEARCH_ACTION:
                if (memcmp(wms->mult.post_lp, "Cancel", 6) == 0)
                  wms->seq = S_START;
                else if (memcmp(wms->mult.post_lp, "Reverse", 7) == 0) {
                  wms->seq = S_SEARCH;
                  wms->ms.srch_dir = -1;
                }
                else if (memcmp(wms->mult.post_lp, "Forward", 7) == 0) {
                  wms->seq = S_SEARCH;
                  wms->ms.srch_dir = 1;
                }
                else if (memcmp(wms->mult.post_lp, "Stop", 4) == 0)
                  wms->seq = S_SELECT_SEARCH_CAT;
                break;
              case F_CAT:
                assert (wms->ms.cat_i == -1);
                a_n = sscanf (wms->mult.post_lp, "%d", &wms->ms.cat_i);
                e = a_n != 1;
                break;
              case F_CARD:
                assert(wms->ms.card_i == -1);
                a_n = sscanf(wms->mult.post_lp, "%d", &wms->ms.card_i);
                e = a_n != 1;
                break;
              case F_MOV_CARD:
                assert(wms->ms.mov_card_i == -1);
                a_n = sscanf(wms->mult.post_lp, "%d", &wms->ms.mov_card_i);
                e = a_n != 1;
                break;
              case F_LVL:
                a_n = sscanf(wms->mult.post_lp, "%d", &wms->ms.lvl);
                e = a_n != 1;
                if (e == 0) {
                  e = wms->ms.lvl < 0 || wms->ms.lvl >= 21;
                  if (e != 0)
                    e =  0x21e1b31e; // WMSPPLA (Web)MemorySurfer parse_post lvl assert (failed)
                }
                break;
              case F_Q:
                assert (wms->mult.post_lp[wms->mult.nread - 1] == '&');
                e = percent2c(wms->mult.post_lp, wms->mult.nread - 1);
                if (e == 0)
                  e = sa_set(&wms->qa_sa, 0, wms->mult.post_lp);
                break;
              case F_A:
                if (wms->mult.nread > 0) {
                  len = wms->mult.nread;
                  if (wms->mult.post_lp[wms->mult.nread - 1] == '&')
                    len--;
                  e = percent2c(wms->mult.post_lp, len);
                }
                if (e == 0)
                  e = sa_set(&wms->qa_sa, 1, wms->mult.post_lp);
                break;
              case F_REVEAL_POS:
                e = wms->saved_reveal_pos != -1;
                if (e == 0) {
                  a_n = sscanf(wms->mult.post_lp, "%d", &wms->saved_reveal_pos);
                  e = a_n != 1 || wms->saved_reveal_pos < 0;
                }
                if (e == 1) {
                  e = 0x255dac21; // WMPPRPA WebMemorySurfer parse_post reveal-pos assert (failed)
                  wms->saved_reveal_pos = -1;
                }
                break;
              case F_TODO_MAIN:
                assert (wms->todo_main == -1);
                a_n = sscanf (wms->mult.post_lp, "%d", &wms->todo_main);
                e = a_n != 1;
                assert (wms->todo_main >= S_FILE && wms->todo_main <= S_END);
                break;
              case F_TODO_ALT:
                assert (wms->todo_alt == -1);
                a_n = sscanf (wms->mult.post_lp, "%d", &wms->todo_alt);
                e = a_n != 1;
                assert (wms->todo_alt >= S_FILE && wms->todo_alt <= S_END);
                break;
              case F_MTIME:
                assert(wms->mtime[0] == -1 && wms->mtime[1] == -1);
                a_n = sscanf(wms->mult.post_lp, "%8x%8x", &wms->mtime[1], &wms->mtime[0]);
                e = a_n != 2;
                assert(wms->mtime[0] >= 0 && wms->mtime[1] >= 0);
                break;
              case F_PASSWORD:
                assert (wms->ms.password == NULL);
                wms->ms.password = malloc (wms->mult.nread);
                e = wms->ms.password == NULL;
                if (e == 0)
                {
                  assert (wms->mult.post_lp[wms->mult.nread - 1] == '&');
                  memcpy (wms->ms.password, wms->mult.post_lp, wms->mult.nread);
                  e = percent2c (wms->ms.password, wms->mult.nread - 1);
                }
                break;
              case F_NEW_PASSWORD:
                assert(wms->ms.new_password == NULL);
                wms->ms.new_password = malloc(wms->mult.nread);
                e = wms->ms.new_password == NULL;
                if (e == 0) {
                  assert(wms->mult.post_lp[wms->mult.nread - 1] == '&');
                  memcpy(wms->ms.new_password, wms->mult.post_lp, wms->mult.nread);
                  e = percent2c(wms->ms.new_password, wms->mult.nread - 1);
                }
                break;
              case F_TOKEN:
                e = wms->mult.nread != 41;
                if (e == 0)
                  e = scan_hex(wms->tok_digest, wms->mult.post_lp, SHA1_HASH_SIZE);
                break;
              case F_EVENT:
                if (wms->mult.post_fp >= 0)
                  len = wms->mult.post_fp;
                else
                  len = wms->mult.post_wp;
                switch (len) {
                case 4:
                  if (strncmp(wms->mult.post_lp, "Stop", 4) == 0) {
                    if (wms->from_page == P_SELECT_CARD_ARRANGE)
                      wms->seq = S_EDIT;
                    else if (wms->from_page == P_SELECT_SEND_CAT) {
                      wms->ms.cat_i = wms->ms.mov_cat_i;
                      wms->ms.card_i = wms->ms.mov_card_i;
                      wms->seq = S_EDIT;
                    }
                    else
                      wms->seq = S_START_CAT;
                  }
                  else if (strncmp(wms->mult.post_lp, "Edit", 4) == 0)
                    wms->seq = S_EDIT;
                  else if (strncmp(wms->mult.post_lp, "Open", 4) == 0) {
                    if (wms->from_page == P_OPEN)
                      wms->seq = S_GO_LOGIN;
                    else {
                      e = wms->from_page != P_FILE;
                      if (e == 0)
                        wms->seq = S_FILELIST;
                    }
                  }
                  else if (strncmp(wms->mult.post_lp, "Move", 4) == 0) {
                    if (wms->from_page == P_START_CAT)
                      wms->seq = S_SELECT_MOVE_CAT;
                    else if (wms->from_page == P_SELECT_CAT)
                      wms->seq = S_SELECT_DEST_CAT;
                    else if (wms->from_page == P_SELECT_ARRANGE)
                      wms->seq = S_MOVE_CAT;
                    else if (wms->from_page == P_EDIT)
                      wms->seq = S_ARRANGE;
                    else {
                      e = wms->from_page != P_SELECT_CARD_ARRANGE;
                      if (e == 0)
                        wms->seq = S_MOVE_CARD;
                    }
                  }
                  else if (strncmp(wms->mult.post_lp, "Send", 4) == 0) {
                    if (wms->from_page == P_EDIT)
                      wms->seq = S_SELECT_SEND_CAT;
                    else {
                      e = wms->from_page != P_SELECT_SEND_CAT;
                      if (e == 0)
                        wms->seq = S_SEND_CARD;
                    }
                  }
                  else if (strncmp(wms->mult.post_lp, "Done", 4) == 0)
                    wms->seq = S_START;
                  else {
                    e = strncmp(wms->mult.post_lp, "Show", 4) != 0;
                    if (e == 0)
                      wms->seq = S_SHOW;
                  }
                  break;
                case 5:
                  if (strncmp(wms->mult.post_lp, "Apply", 5) == 0)
                    wms->seq = S_APPLY;
                  else if (strncmp(wms->mult.post_lp, "Erase", 5) == 0)
                    if (wms->from_page == P_FILE)
                      wms->seq = S_ASK_ERASE;
                    else {
                      e = wms->from_page != P_MSG;
                      if (e == 0) {
                        e = wms->todo_main == -1;
                        if (e == 0)
                          wms->seq = wms->todo_main;
                      }
                    }
                  else if (strncmp(wms->mult.post_lp, "About", 5) == 0)
                    wms->seq = S_ABOUT;
                  else {
                    e = strncmp(wms->mult.post_lp, "Start", 5);
                    if (e == 0)
                      wms->seq = S_NONE;
                  }
                  break;
                case 6:
                  if (strncmp(wms->mult.post_lp, "Create", 6) == 0) {
                    if (wms->from_page == P_START_CAT)
                      wms->seq = S_SELECT_CREATE_CAT;
                    else {
                      e = wms->from_page != P_CAT_NAME;
                      if (e == 0)
                        wms->seq = S_CREATE_CAT;
                    }
                  }
                  else if (strncmp(wms->mult.post_lp, "Cancel", 6) == 0) {
                    if (wms->from_page == P_PREFERENCES)
                      wms->seq = S_START;
                    else if (wms->from_page == P_MSG) {
                      e = wms->todo_alt == -1;
                      if (e == 0)
                        wms->seq = wms->todo_alt;
                    }
                    else {
                      e = wms->from_page != P_OPEN;
                      if (e == 0)
                        wms->seq = S_CLOSE;
                    }
                  }
                  else if (strncmp(wms->mult.post_lp, "Select", 6) == 0) {
                    if (wms->from_page == P_SELECT_CREATE_CAT)
                      wms->seq = S_SELECT_ARRANGE;
                    else if (wms->from_page == P_SELECT_CAT)
                      wms->seq = S_SELECT_MOVE_ARRANGE;
                    else {
                      e = wms->from_page != P_SELECT_ARRANGE;
                      if (e == 0)
                        wms->seq = S_CAT_NAME;
                    }
                  }
                  else if (strncmp(wms->mult.post_lp, "Delete", 6) == 0) {
                    if (wms->from_page == P_START_CAT)
                      wms->seq = S_SELECT_DELETE_CAT;
                    else if (wms->from_page == P_MSG) {
                      e = wms->todo_main == -1;
                      if (e == 0)
                        wms->seq = wms->todo_main;
                    }
                    else {
                      e = wms->from_page != P_SELECT_CAT;
                      if (e == 0)
                        wms->seq = S_ASK_DELETE_CAT;
                    }
                  }
                  else if (strncmp(wms->mult.post_lp, "Rename", 6) == 0) {
                    if (wms->from_page == P_START_CAT)
                      wms->seq = S_SELECT_RENAME_CAT;
                    else if (wms->from_page == P_SELECT_CAT)
                      wms->seq = S_RENAME_ENTER;
                    else {
                      e = wms->from_page != P_CAT_NAME;
                      if (e == 0)
                        wms->seq = S_RENAME_CAT;
                    }
                  }
                  else if (strncmp(wms->mult.post_lp, "Insert", 6) == 0) {
                    e = wms->from_page != P_EDIT;
                    if (e == 0)
                      wms->seq = S_INSERT;
                  }
                  else if (strncmp(wms->mult.post_lp, "Toggle", 6) == 0)
                    if (wms->from_page == P_START_CAT)
                      wms->seq = S_SELECT_TOGGLE_CAT;
                    else {
                      e = wms->from_page != P_SELECT_CAT;
                      if (e == 0)
                        wms->seq = S_TOGGLE;
                    }
                  else if (strncmp(wms->mult.post_lp, "Remove", 6) == 0)
                    if (wms->from_page == P_FILE)
                      wms->seq = S_ASK_REMOVE;
                    else {
                      e = wms->from_page != P_MSG;
                      if (e == 0) {
                        e = wms->todo_main == -1;
                        if (e == 0)
                          wms->seq = wms->todo_main;
                      }
                    }
                  else if (strncmp(wms->mult.post_lp, "Resume", 6) == 0)
                    wms->seq = S_RESUME;
                  else if (memcmp(wms->mult.post_lp, "Search", 6) == 0)
                    wms->seq = S_SEARCH;
                  else {
                    e = strncmp(wms->mult.post_lp, "Reveal", 6);
                    if (e == 0)
                      wms->seq = S_REVEAL;
                  }
                  break;
                default:
                  if (strncmp(wms->mult.post_lp, "Set", 3) == 0) {
                    e = wms->from_page != P_EDIT;
                    if (e == 0)
                      wms->seq = S_SET;
                  }
                  else if (strncmp(wms->mult.post_lp, "Preferences", 11) == 0)
                    wms->seq = S_PREFERENCES;
                  else if (strncmp(wms->mult.post_lp, "Histogram", 9) == 0)
                    wms->seq = S_HISTOGRAM;
                  else if (strncmp(wms->mult.post_lp, "Refresh", 7) == 0)
                    wms->seq = S_HISTOGRAM;
                  else {
                    e = strncmp(wms->mult.post_lp, "OK", 2);
                    if (e == 0) {
                      if (wms->from_page == P_FILE || wms->from_page == P_ABOUT) {
                        if (wms->file_title_str != NULL)
                          wms->seq = S_START;
                        else
                          wms->seq = S_NONE;
                      }
                      else {
                        e = wms->from_page != P_MSG;
                        if (e == 0) {
                          e = wms->todo_main == -1;
                          if (e == 0)
                            wms->seq = wms->todo_main;
                        }
                      }
                    }
                  }
                  break;
                }
                break;
              case F_PAGE:
                assert(wms->from_page == -1);
                a_n = sscanf(wms->mult.post_lp, "%d", &wms->from_page);
                e = a_n != 1;
                assert(wms->from_page >= P_START && wms->todo_main <= P_HISTOGRAM);
                break;
              case F_MODE:
                assert(wms->saved_mode == M_NONE);
                a_n = sscanf(wms->mult.post_lp, "%d", &wms->saved_mode);
                e = a_n != 1;
                assert(wms->saved_mode >= M_DEFAULT && wms->saved_mode <= M_RATE);
                break;
              case F_TIMEOUT:
                e = wms->from_page != P_PREFERENCES;
                if (e == 0) {
                  assert(wms->timeout == -1);
                  a_n = sscanf(wms->mult.post_lp, "%d", &wms->timeout);
                  e = a_n != 1;
                  if (e == 0)
                    assert(wms->timeout >= 0 && wms->timeout < 5);
                }
                break;
              default:
                e = -1;
                break;
              }
              field = F_UNKNOWN;
              wms->mult.delim_str[0] = "=";
              wms->mult.delim_str[1] = NULL;
            }
          }
        }
        break;
      case T_BOUNDARY_INIT:
        e = wms->mult.post_fp < 0;
        if (e == 0) {
          str = wms->mult.post_lp + wms->mult.post_fp;
          e = strncmp(str, "\r\n", 2) != 0;
          if (e == 0) {
            assert(wms->boundary_str == NULL);
            wms->boundary_str = malloc(wms->mult.nread - 1);
            e = wms->boundary_str == NULL;
            if (e == 0) {
              memcpy(wms->boundary_str, wms->mult.post_lp, wms->mult.nread - 2);
              wms->boundary_str[wms->mult.nread - 2] = '\0';
              stage = T_CONTENT;
              wms->mult.delim_str[0] = "; ";
              wms->mult.delim_str[1] = NULL;
            }
          }
        }
        break;
      case T_CONTENT:
        e = wms->mult.post_fp < 0;
        if (e == 0) {
          str = wms->mult.post_lp + wms->mult.post_fp;
          e = strncmp(str, "; ", 2) != 0;
          if (e == 0) {
            e = strncmp(wms->mult.post_lp, "Content-Disposition: form-data", 30) != 0;
            if (e == 0) {
              stage = T_NAME;
              wms->mult.delim_str[0] = "=\"";
            }
          }
        }
        break;
      case T_NAME:
        e = wms->mult.post_fp < 0;
        if (e == 0) {
          str = wms->mult.post_lp + wms->mult.post_fp;
          if (strncmp(str, "=\"", 2) == 0) {
            e = strncmp(wms->mult.post_lp, "name", 4) != 0;
            if (e == 0) {
              wms->mult.delim_str[0] = "\"\r\n\r\n";
              wms->mult.delim_str[1] = "\"; ";
            }
          }
          else if (strncmp(str, "\"\r\n\r\n", 5) == 0) {
            if (strncmp(wms->mult.post_lp, "file-title", 10) == 0) {
              field = F_FILE_TITLE;
              wms->mult.delim_str[0] = "\r\n";
            }
            else if (strncmp(wms->mult.post_lp, "token", 5) == 0) {
              field = F_TOKEN;
              wms->mult.delim_str[0] = "\r\n";
            }
            else
            {
              e = strncmp(wms->mult.post_lp, "file_action", 11) != 0;
              if (e == 0) {
                field = F_FILE_ACTION;
                wms->mult.delim_str[0] = "\r\n";
              }
            }
            wms->mult.delim_str[1] = NULL;
          }
          else if (memcmp (str, "\"; ", 3) == 0)
          {
            e = strncmp(wms->mult.post_lp, "file_action", 11) != 0;
            if (e == 0) {
              wms->mult.delim_str[0] = "Content-Type: text/xml\r\n\r\n";
              wms->mult.delim_str[1] = NULL;
            }
          }
          else if (strncmp(str, "\r\n", 2) == 0) {
            if (field == F_FILE_TITLE) {
              assert(wms->file_title_str == NULL);
              wms->file_title_str = malloc(wms->mult.nread - 1);
              e = wms->file_title_str == NULL;
              if (e == 0) {
                memcpy(wms->file_title_str, wms->mult.post_lp, wms->mult.nread - 2);
                wms->file_title_str[wms->mult.nread - 2] = '\0';
                stage = T_BOUNDARY_BEGIN;
                wms->mult.delim_str[0] = "--";
              }
            }
            else if (field == F_TOKEN) {
              e = wms->mult.nread != 42;
              if (e == 0) {
                e = scan_hex(wms->tok_digest, wms->mult.post_lp, SHA1_HASH_SIZE);
                stage = T_BOUNDARY_BEGIN;
                wms->mult.delim_str[0] = "--";
              }
            }
            else
            {
              e = field != F_FILE_ACTION;
              if (e == 0)
              {
                if (memcmp (wms->mult.post_lp, "Upload", 6) == 0)
                {
                  wms->seq = S_UPLOAD_REPORT;
                }
                else
                {
                  e = memcmp (wms->mult.post_lp, "Stop", 4);
                  if (e == 0)
                  {
                    wms->seq = S_FILE;
                  }
                }
                stage = T_BOUNDARY_BEGIN;
                wms->mult.delim_str[0] = "--";
              }
            }
          }
          else {
            e = strncmp(str, "Content-Type: text/xml\r\n\r\n", 26) != 0;
            if (e == 0) {
              assert(wms->file_title_str != NULL); // A_SLASH
              str = strchr(wms->file_title_str, '/');
              e = str != NULL;
              if (e == 0) {
                size = strlen(DATA_PATH) + strlen(wms->file_title_str) + 2; // A_GATHER
                str = malloc(size);
                e = str == NULL;
                if (e == 0) {
                  strcpy(str, DATA_PATH);
                  strcat(str, "/");
                  strcat(str, wms->file_title_str);
                  assert(wms->ms.imf_filename == NULL);
                  wms->ms.imf_filename = str;
                  e = ms_open(&wms->ms); // A_OPEN
                  if (e == 0) {
                    wms->xml.n = 0;
                    wms->xml.p_lineptr = NULL;
                    wms->xml.prev_cat_i = -1;
                    wms->xml.parsed_cat_i = -1;
                    e = parse_xml(wms, TAG_ROOT, -1);
                    if (e == 0) {
                      free (wms->xml.p_lineptr);
                      wms->xml.p_lineptr = NULL;
                      wms->xml.n = 0;
                      stage = T_BOUNDARY_BEGIN;
                      wms->mult.delim_str[0] = "\r\n--";
                    }
                  }
                }
              }
            }
          }
        }
        break;
      case T_BOUNDARY_BEGIN:
        e = wms->mult.post_fp < 0;
        if (e == 0) {
          str = wms->mult.post_lp + wms->mult.post_fp;
          len = wms->mult.post_wp - wms->mult.post_fp;
          e = strncmp(str, "--", 2) && (len < 4 || strncmp(str, "\r\n--", 4));
          if (e == 0) {
            stage = T_BOUNDARY_CHECK;
            wms->mult.delim_str[0] = "--\r\n";
            wms->mult.delim_str[1] = "\r\n";
          }
        }
        break;
      case T_BOUNDARY_CHECK:
        if (wms->mult.nread > 0) {
          e = wms->mult.post_fp < 0;
          if (e == 0) {
            str = wms->mult.post_lp + wms->mult.post_fp;
            len = wms->mult.post_wp - wms->mult.post_fp;
            deci = 0;
            if (strncmp(str, "\r\n", 2) == 0)
              deci = -1;
            if (len >= 4 && strncmp(str, "--\r\n", 4) == 0)
              deci = 1;
            e = deci == 0;
            if (e == 0) {
              assert(wms->boundary_str != NULL);
              len = strlen(wms->boundary_str);
              e = len != wms->mult.post_fp;
              if (e == 0) {
                e = strncmp(wms->mult.post_lp, wms->boundary_str, len) != 0;
                if (e == 0) {
                  if (deci < 0) {
                    stage = T_CONTENT;
                    wms->mult.delim_str[0] = "; ";
                    wms->mult.delim_str[1] = NULL;
                  }
                  else
                    wms->mult.delim_str[0] = NULL;
                }
              }
            }
          }
        }
        break;
      }
    }
  } while (wms->mult.nread != -1 && e == 0);
  return e;
}

static char *sa_get(struct StringArray *sa, int16_t sa_i)
{
  assert(sa_i >= 0);
  char *ret_str;
  char ch;
  int pos_g; // get
  int i;
  ret_str = NULL;
  if (sa_i < sa->sa_c) {
    pos_g = 0;
    for (i = 0; i != sa_i; i++) {
      do {
        ch = sa->sa_d[pos_g++];
      } while (ch != '\0');
    }
    assert(pos_g < sa->sa_n);
    ret_str = sa->sa_d + pos_g;
  }
  return ret_str;
}

enum { ESC_AMP = 1, ESC_LT = 2, ESC_QUOT = 4 };

static int xml_escape(char **xml_str_ptr, size_t *xml_n_ptr, char *str_text, int escape_mask)
{
  int e;
  char ch;
  int src;
  int dest;
  e = xml_str_ptr == NULL || xml_n_ptr == NULL;
  if (e == 0) {
    if (str_text == NULL)
      str_text = "";
    src = 0;
    dest = 0;
    do
    {
      ch = str_text[src++];
      while (((dest + 6) > *xml_n_ptr) && (e == 0)) {
        *xml_n_ptr = (*xml_n_ptr << 1) | 1;
        *xml_str_ptr = realloc (*xml_str_ptr, *xml_n_ptr);
        e = *xml_str_ptr == NULL;
      }
      switch (ch)
      {
      case '&':
        if ((escape_mask & ESC_AMP) != 0) {
          (*xml_str_ptr)[dest++] = '&';
          (*xml_str_ptr)[dest++] = 'a';
          (*xml_str_ptr)[dest++] = 'm';
          (*xml_str_ptr)[dest++] = 'p';
          (*xml_str_ptr)[dest++] = ';';
        }
        else
          (*xml_str_ptr)[dest++] = '&';
        break;
      case '<':
        if ((escape_mask & ESC_LT) != 0) {
          (*xml_str_ptr)[dest++] = '&';
          (*xml_str_ptr)[dest++] = 'l';
          (*xml_str_ptr)[dest++] = 't';
          (*xml_str_ptr)[dest++] = ';';
        }
        else
          (*xml_str_ptr)[dest++] = '<';
        break;
      case '"':
        if ((escape_mask & ESC_QUOT) != 0) {
          (*xml_str_ptr)[dest++] = '&';
          (*xml_str_ptr)[dest++] = 'q';
          (*xml_str_ptr)[dest++] = 'u';
          (*xml_str_ptr)[dest++] = 'o';
          (*xml_str_ptr)[dest++] = 't';
          (*xml_str_ptr)[dest++] = ';';
        }
        else
          (*xml_str_ptr)[dest++] = '"';
        break;
      default:
        (*xml_str_ptr)[dest++] = ch;
        break;
      }
    } while ((ch != '\0') && (e == 0));
  }
  return e;
}

enum HIERARCHY { H_CHILD, H_SIBLING };

void gen_html_cat (int16_t n_create, int indent_n, enum HIERARCHY hierarchy, struct WebMemorySurfer *wms)
{
  int e;
  char *c_str;
  char indent_str[32];
  int i;
  char *checked;
  for (i = 0; i < indent_n; i++)
  {
    indent_str[i] = '\t';
  }
  indent_str[i] = '\0';
  if (hierarchy == H_CHILD)
  {
    printf ("%s<ul>\n", indent_str);
  }
  checked = n_create == wms->ms.cat_i ? " checked" : "";
  c_str = sa_get (&wms->ms.cat_sa, n_create);
  e = xml_escape(&wms->html_lp, &wms->html_n, c_str, ESC_AMP | ESC_LT);
  assert (e == 0);
  printf ("%s\t<li><label><input type=\"radio\" name=\"cat\" value=\"%d\"%s>%s</label>%s\n",
    indent_str,
    n_create,
    checked,
    wms->html_lp,
    wms->ms.cat_t[n_create].cat_n_child == -1 ? "</li>" : "");
  if (wms->ms.cat_t[n_create].cat_n_child != -1)
  {
    if (wms->ms.cat_t[n_create].cat_x != 0)
    {
      gen_html_cat (wms->ms.cat_t[n_create].cat_n_child, indent_n + 1, H_CHILD, wms);
    }
    else
    {
      printf ("%s\t\t...\n", indent_str);
    }
    printf ("%s\t</li>\n", indent_str);
  }
  if (wms->ms.cat_t[n_create].cat_n_sibling != -1)
  {
    gen_html_cat (wms->ms.cat_t[n_create].cat_n_sibling, indent_n, H_SIBLING, wms);
  }
  if (hierarchy == H_CHILD)
  {
    printf ("%s</ul>\n", indent_str);
  }
}

enum
{
  STATE_ALARM = 0,
  STATE_SCHEDULED,
  STATE_NEW,
  STATE_SUSPENDED
};

enum
{
  SECONDS_MINUTE = 60,
  SECONDS_HOUR = SECONDS_MINUTE * 60,
  SECONDS_DAY = SECONDS_HOUR * 24,
  SECONDS_MONTH = SECONDS_DAY * 30,
  SECONDS_YEAR = SECONDS_DAY * 365
};

static void set_time_str(char *time_str, time_t time_set)
{
  int n_stored;
  char *str_cursor;
  int n_years;
  int n_month;
  int n_days;
  int n_hours;
  int n_minutes;
  int n_seconds;
  int valid_c; // count
  char *space_str;
  str_cursor = time_str;
  valid_c = 0;
  n_years = time_set / SECONDS_YEAR;
  if (n_years) {
    valid_c = 48;
    time_set -= SECONDS_YEAR * n_years;
  }
  n_month = time_set / SECONDS_MONTH;
  if (n_month) {
    if (valid_c == 0) valid_c = 24;
    time_set -= SECONDS_MONTH * n_month;
  }
  n_days = time_set / SECONDS_DAY;
  if (n_days) {
    if (valid_c == 0) valid_c = 12;
    time_set -= SECONDS_DAY * n_days;
  }
  n_hours = time_set / SECONDS_HOUR;
  if (n_hours) {
    if (valid_c == 0) valid_c = 6;
    time_set -= SECONDS_HOUR * n_hours;
  }
  n_minutes = time_set / SECONDS_MINUTE;
  if (n_minutes) {
    if (valid_c == 0) valid_c = 3;
    time_set -= SECONDS_MINUTE * n_minutes;
  }
  n_seconds = time_set;
  if (valid_c == 0) valid_c = 1;
  time_set -= n_seconds;
  if (valid_c & 32) {
    n_stored = sprintf(str_cursor, "%dY", n_years);
    str_cursor += n_stored;
  }
  if ((valid_c & 16) && n_month != 0) {
    space_str = valid_c & 32 ? " " : "";
    n_stored = sprintf(str_cursor, "%s%dM", space_str, n_month);
    str_cursor += n_stored;
  }
  if ((valid_c & 8) && n_days != 0) {
    space_str = valid_c & 16 ? " " : "";
    n_stored = sprintf(str_cursor, "%s%dD", space_str, n_days);
    str_cursor += n_stored;
  }
  if ((valid_c & 4) && n_hours != 0) {
    space_str = valid_c & 8 ? " " : "";
    n_stored = sprintf(str_cursor, "%s%dh", space_str, n_hours);
    str_cursor += n_stored;
  }
  if ((valid_c & 2) && n_minutes != 0) {
    space_str = valid_c & 4 ? " " : "";
    n_stored = sprintf(str_cursor, "%s%dm", space_str, n_minutes);
    str_cursor += n_stored;
  }
  if (valid_c & 1) {
    space_str = valid_c & 2 ? " " : "";
    if (n_seconds != 0 || (valid_c & 2) == 0)
      n_stored = sprintf(str_cursor, "%s%ds", space_str, n_seconds);
  }
}

struct XmlGenerator {
  FILE *w_stream; // write
  int indent;
  char indent_str[32];
  char *w_lineptr;
  size_t w_n;
};

void set_indent (char *indent_str, int indent, size_t max_size)
{
  size_t size;
  if (indent >= 0)
  {
    size = 2 + indent;
    if (size > max_size)
    {
      indent = max_size - 2;
    }
    indent_str[0] = '\n';
    memset (indent_str + 1, '\t', indent);
    indent_str[1 + indent] = '\0';
  }
}

static void sa_init(struct StringArray *sa) {
  sa->sa_c = 0;
  sa->sa_d = NULL;
  sa->sa_n = 0;
  assert(sizeof(char) == 1);
}

static void sa_free (struct StringArray *sa)
{
  sa->sa_c = 0;
  free (sa->sa_d);
  sa->sa_d = NULL;
  sa->sa_n = 0;
}

int gen_xml_category (int16_t cat_i, struct XmlGenerator *xg, struct MemorySurfer *ms)
{
  int e;
  struct Category *cat_ptr;
  char *c_str;
  int32_t data_size;
  struct Card *card_l;
  int card_a;
  int card_i;
  struct Card *card_ptr;
  struct StringArray card_sa;
  struct tm bd_time; // broken-down
  time_t card_time;
  char time_str[20]; // 1971-01-01T00:00:00
  int rv;
  char strength_str[16];
  char state_ch;
  char *q_str;
  char *a_str;
  e = fprintf (xg->w_stream, "%s<deck>", xg->indent_str) <= 0;
  if (e == 0)
  {
    xg->indent++;
    set_indent (xg->indent_str, xg->indent, sizeof (xg->indent_str));
    e = fprintf (xg->w_stream, "%s<name>", xg->indent_str) <= 0;
    if (e == 0)
    {
      cat_ptr = ms->cat_t + cat_i;
      c_str = sa_get (&ms->cat_sa, cat_i);
      e = xml_escape(&xg->w_lineptr, &xg->w_n, c_str, ESC_AMP | ESC_LT);
      if (e == 0)
      {
        e = fprintf (xg->w_stream, "%s</name>", xg->w_lineptr) <= 0;
        if (e == 0)
        {
          data_size = imf_get_size (&ms->imf, cat_ptr->cat_cli);
          card_l = malloc (data_size);
          e = card_l == NULL;
          if (e == 0)
          {
            e = imf_get (&ms->imf, cat_ptr->cat_cli, card_l);
            if (e == 0)
            {
              sa_init(&card_sa);
              card_a = data_size / sizeof (struct Card);
              card_i = 0;
              while ((card_i < card_a) && (e == 0))
              {
                card_ptr = card_l + card_i;
                e = sa_load (&card_sa, &ms->imf, card_ptr->card_qai);
                if (e == 0)
                {
                  e = fprintf (xg->w_stream, "%s<card>", xg->indent_str) <= 0;
                  if (e == 0)
                  {
                    xg->indent++;
                    set_indent (xg->indent_str, xg->indent, sizeof (xg->indent_str));
                    e = fprintf (xg->w_stream, "%s<time>", xg->indent_str) <= 0;
                    if (e == 0)
                    {
                      memset (&bd_time, 0, sizeof (bd_time));
                      assert ((card_ptr->card_time < INT32_MAX) || (sizeof (card_time) == 8));
                      card_time = card_ptr->card_time;
                      e = ! gmtime_r (&card_time, &bd_time);
                      if (e == 0)
                      {
                        bd_time.tm_mon += 1;
                        bd_time.tm_year += 1900;
                        rv = snprintf (time_str, sizeof (time_str), "%4d-%02d-%02dT%02d:%02d:%02d",
                            bd_time.tm_year, bd_time.tm_mon, bd_time.tm_mday,
                            bd_time.tm_hour, bd_time.tm_min, bd_time.tm_sec);
                        e = rv != 19;
                        if (e == 0)
                        {
                          e = fprintf (xg->w_stream, "%s</time>%s<strength>", time_str, xg->indent_str) <= 0;
                          if (e == 0)
                          {
                            rv = snprintf (strength_str, sizeof (strength_str), "%d", card_ptr->card_strength);
                            e = ! (rv < sizeof (strength_str));
                            if (e == 0)
                            {
                              state_ch = card_ptr->card_state + '0';
                              e = fprintf (xg->w_stream, "%s</strength>%s<state>%c</state>%s<question>",
                                strength_str, xg->indent_str, state_ch, xg->indent_str) <= 0;
                              if (e == 0)
                              {
                                q_str = sa_get (&card_sa, 0);
                                e = xml_escape(&xg->w_lineptr, &xg->w_n, q_str, ESC_AMP | ESC_LT);
                                if (e == 0)
                                {
                                  e = fprintf (xg->w_stream, "%s</question>%s<answer>", xg->w_lineptr, xg->indent_str) <= 0;
                                  if (e == 0)
                                  {
                                    a_str = sa_get (&card_sa, 1);
                                    e = xml_escape(&xg->w_lineptr, &xg->w_n, a_str, ESC_AMP | ESC_LT);
                                    if (e == 0)
                                    {
                                      e = fprintf (xg->w_stream, "%s</answer></card>", xg->w_lineptr) <= 0;
                                      if (e == 0)
                                      {
                                        xg->indent--;
                                        set_indent (xg->indent_str, xg->indent, sizeof (xg->indent_str));
                                        card_i++;
                                      }
                                    }
                                  }
                                }
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
              sa_free (&card_sa);
              if (e == 0)
              {
                if (ms->cat_t[cat_i].cat_n_child != -1)
                {
                  e = gen_xml_category (ms->cat_t[cat_i].cat_n_child, xg, ms);
                }
                if (e == 0)
                {
                  e = fputs ("</deck>", xg->w_stream) <= 0;
                  if (e == 0)
                  {
                    xg->indent--;
                    set_indent (xg->indent_str, xg->indent, sizeof (xg->indent_str));
                    if (ms->cat_t[cat_i].cat_n_sibling != -1)
                    {
                      e = gen_xml_category (ms->cat_t[cat_i].cat_n_sibling, xg, ms);
                    }
                  }
                }
              }
            }
            free (card_l);
          }
        }
      }
    }
  }
  return e;
}

static const uint8_t timeouts[5][2] = {{5,60},{15,60},{60,60},{90,120},{240,180}};

static time_t lvl_s[21] = { // level strength
  60, // 1m (0)
  120, // 2m (1)
  300, // 5m (2)
  900, // 15m (3)
  1800, // 30m (4)
  3600, // 1h (5)
  14400, // 4h (6)
  43200, // 12h (7)
  86400, // 1D (8)
  172800, // 2D (9)
  432000, // 5D (10)
  604800, // 7D (11)
  1209600, // 14D (12)
  2592000, // 1M (13)
  7776000,  // 3M (14)
  15552000, // 6M (15)
  31104000, // 1Y (16)
  62208000, // 2Y (17)
  155520000, // 5Y (18)
  311040000, // 10Y (19)
  622080000 // 20Y (20)
};

static int gen_html(struct WebMemorySurfer *wms) {
  int e;
  int rv;
  int i;
  int j;
  int lvl; // level
  int lvl_sel; // select
  char *q_str;
  char *a_str;
  time_t time_diff;
  char time_str[32];
  int x;
  int y;
  char *str;
  const char *dis_str; // disabled
  const char *attr_str; // attribute
  const char *header_str;
  const char *notice_str[2];
  const char *submit_str;
  const char *text_str;
  char *ext_str;
  char *dot;
  char *dup_str;
  char *f_basename; // file
  char title_str[64];
  struct XmlGenerator xg;
  char *sw_info_str;
  char mtime_str[17];
  struct stat file_stat;
  int nb; // number (of) bytes
  int dy;
  char path_str[10];
  static const char *TIMEOUTS[] = { "5 m", "15 m", "1 h", "3 h", "12 h" };
  size_t size;
  size_t len;
  enum Block bl;
  int bl_i; // block index
  e = 0;
  mtime_str[0] = '\0';
  if (wms->ms.imf_filename != NULL) {
    size = sizeof(struct stat);
    memset(&file_stat, 0, size);
    e = stat(wms->ms.imf_filename, &file_stat);
    if (e == 0) {
      assert(file_stat.st_mtim.tv_sec >= 0 && file_stat.st_mtim.tv_nsec >= 0);
      size = sizeof(mtime_str);
      nb = snprintf(mtime_str, size, "%08x%08x", (uint32_t) file_stat.st_mtim.tv_sec, (uint32_t) file_stat.st_mtim.tv_nsec);
      e = nb != 16;
    }
  }
  if (e == 0) {
    size = sizeof(title_str);
    assert(size >= 13);
    strcpy(title_str, "MemorySurfer - Welcome");
    if (wms->file_title_str != NULL) {
      e = xml_escape(&wms->html_lp, &wms->html_n, wms->file_title_str, ESC_AMP | ESC_LT);
      if (e == 0) {
        rv = snprintf(title_str, size, "MemorySurfer – %s", wms->html_lp);
        e = rv < 0 || rv >= size;
      }
    }
  }
  if (e == 0) {
    e = sw_stop(wms->sw_i, &wms->ms.imf.sw);
    if (e == 0)
      e = sw_info(&sw_info_str, &wms->ms.imf.sw);
  }
  assert(wms->tok_str[0] == '\0' || wms->tok_str[40] == '\0');
  if (e == 0) {
    bl_i = 0;
    while ((bl = block_seq[wms->page][bl_i++]) != B_END && e == 0)
    {
      switch (bl)
      {
      case B_START_HTML:
        rv = printf("Content-Type: text/html; charset=utf-8\n\n"
                    "<!DOCTYPE html>\n"
                    "<html lang=\"en\">\n"
                    "\t<head>\n"
                    "\t\t<meta charset=\"utf-8\">\n"
                    "\t\t<title>%s</title>\n"
                    "\t\t<meta name=\"viewport\" content=\"width=device-width\">\n"
                    "\t\t<meta name=\"description\" content=\"Open source software to efficiently memorize flashcards.\">\n"
                    "\t\t<link rel=\"shortcut icon\" href=\"/favicon.ico\">\n"
                    "\t\t<link rel=\"stylesheet\" type=\"text/css\" href=\"/ms.css\">\n"
                    "\t\t<link rel=\"license\" href=\"https://www.gnu.org/licenses/old-licenses/gpl-2.0.html\">\n"
                    "\t</head>\n"
                    "\t<body>\n"
                    "\t\t<form method=\"post\" action=\"memorysurfer.cgi\">\n"
                    "\t\t\t<div>\n"
                    "\t\t\t\t<input type=\"hidden\" name=\"page\" value=\"%d\">\n",
            title_str,
            wms->page);
        e = rv < 0;
        if (e == 0 && wms->mode >= M_DEFAULT && wms->mode <= M_RATE) {
          rv = printf("\t\t\t\t<input type=\"hidden\" name=\"mode\" value=\"%d\">\n", wms->mode);
          e = rv < 0;
        }
        if (e == 0 && wms->file_title_str != NULL) {
          e = xml_escape(&wms->html_lp, &wms->html_n, wms->file_title_str, ESC_AMP | ESC_QUOT);
          if (e == 0) {
            rv = printf("\t\t\t\t<input type=\"hidden\" name=\"file-title\" value=\"%s\">\n", wms->html_lp);
            e = rv < 0;
          }
        }
        if (e == 0 && strlen(wms->tok_str) == 40) {
          rv = printf("\t\t\t\t<input type=\"hidden\" name=\"token\" value=\"%s\">\n", wms->tok_str);
          e = rv < 0;
        }
        if (e == 0 && wms->ms.card_i >= 0) {
          rv = printf("\t\t\t\t<input type=\"hidden\" name=\"card\" value=\"%d\">\n", wms->ms.card_i);
          e = rv < 0;
        }
        if (e == 0 && strlen(mtime_str) == 16) {
          rv = printf("\t\t\t\t<input type=\"hidden\" name=\"mtime\" value=\"%s\">\n", mtime_str);
          e = rv < 0;
        }
        if (e == 0 && wms->reveal_pos >= 0) {
          rv = printf("\t\t\t\t<input type=\"hidden\" name=\"reveal-pos\" value=\"%d\">\n", wms->reveal_pos);
          e = rv < 0;
        }
        break;
      case B_HIDDEN_CAT:
        if (wms->ms.cat_i >= 0) {
          rv = printf("\t\t\t\t<input type=\"hidden\" name=\"cat\" value=\"%d\">\n", wms->ms.cat_i);
          e = rv < 0;
        }
        break;
      case B_HIDDEN_ARRANGE:
        if (wms->ms.arrange >= 0) {
          rv = printf("\t\t\t\t<input type=\"hidden\" name=\"arrange\" value=\"%d\">\n", wms->ms.arrange);
          e = rv < 0;
        }
        break;
      case B_HIDDEN_CAT_NAME:
        if (wms->ms.cat_name != NULL) {
          e = xml_escape(&wms->html_lp, &wms->html_n, wms->ms.cat_name, ESC_AMP | ESC_QUOT);
          if (e == 0) {
            rv = printf("\t\t\t\t<input type=\"hidden\" name=\"cat_name\" value=\"%s\">\n", wms->html_lp);
            e = rv < 0;
          }
        }
        break;
      case B_HIDDEN_SEARCH_TXT:
        if (wms->ms.search_txt != NULL && strlen(wms->ms.search_txt) > 0) {
          e = xml_escape(&wms->html_lp, &wms->html_n, wms->ms.search_txt, ESC_AMP | ESC_QUOT);
          if (e == 0) {
            rv = printf("\t\t\t\t<input type=\"hidden\" name=\"search_txt\" value=\"%s\">\n", wms->html_lp);
            e = rv < 0;
          }
        }
        break;
      case B_CLOSE_DIV:
        rv = printf("\t\t\t</div>\n");
        e = rv < 0;
        break;
      case B_START:
        dis_str = wms->file_title_str != NULL ? "" : " disabled";
        attr_str = wms->ms.n_first != -1 ? "" : " disabled";
        printf("\t\t\t<h1>Start</h1>\n"
               "\t\t\t<p><input type=\"submit\" name=\"start_action\" value=\"File\"></p>\n"
               "\t\t\t<p><input type=\"submit\" name=\"start_action\" value=\"Categories\"%s></p>\n"
               "\t\t\t<p><input type=\"submit\" name=\"start_action\" value=\"Edit\"%s></p>\n"
               "\t\t\t<p><input type=\"submit\" name=\"start_action\" value=\"Learn\"%s></p>\n"
               "\t\t\t<p><input type=\"submit\" name=\"start_action\" value=\"Search\"%s></p>\n"
               "\t\t\t<p><input type=\"submit\" name=\"event\" value=\"Preferences\"%s></p>\n"
               "\t\t\t<p><input type=\"submit\" name=\"event\" value=\"About\"></p>\n"
               "\t\t</form>\n"
               "\t\t<code>%s</code>\n"
               "\t</body>\n"
               "</html>\n",
          dis_str,
          attr_str,
          attr_str,
          attr_str,
          dis_str,
          sw_info_str);
        break;
      case B_FILE:
        if (wms->file_title_str != NULL) {
          dis_str = "";
          attr_str = " disabled";
        }
        else {
          dis_str = " disabled";
          attr_str = "";
        }
        printf("\t\t\t<h1>File</h1>\n"
               "\t\t\t<p><input type=\"submit\" name=\"file_action\" value=\"New\"%s></p>\n"
               "\t\t\t<p><input type=\"submit\" name=\"event\" value=\"Open\"%s></p>\n"
               "\t\t\t<p><input type=\"submit\" name=\"file_action\" value=\"Password\"%s></p>\n"
               "\t\t\t<p><input type=\"submit\" name=\"file_action\" value=\"Import\"%s></p>\n"
               "\t\t\t<p><input type=\"submit\" name=\"file_action\" value=\"Export\"%s></p>\n"
               "\t\t\t<p><input type=\"submit\" name=\"event\" value=\"Remove\"%s></p>\n"
               "\t\t\t<p><input type=\"submit\" name=\"event\" value=\"Erase\"%s></p>\n"
               "\t\t\t<p><input type=\"submit\" name=\"file_action\" value=\"Close\"%s></p>\n"
               "\t\t\t<p><input type=\"submit\" name=\"file_action\" value=\"Cancel\"></p>\n"
               "\t\t</form>\n"
               "\t\t<code>%s</code>\n"
               "\t</body>\n"
               "</html>\n",
          attr_str,
          attr_str,
          dis_str,
          dis_str,
          dis_str,
          dis_str,
          dis_str,
          dis_str,
          sw_info_str);
        break;
      case B_PASSWORD:
        assert(wms->ms.passwd.pw_flag >= 0);
        header_str = NULL;
        notice_str[0] = NULL;
        notice_str[1] = NULL;
        x = 0;
        y = 0;
        if (wms->ms.passwd.pw_flag > 0) {
          x = 1;
          if (wms->mode != M_CHANGE_PASSWD) {
            header_str = "Enter the password to login";
            submit_str = "Login";
          }
          else {
            assert(wms->seq == S_GO_CHANGE);
            header_str = "Change password";
            notice_str[0] = "Enter the current password";
            notice_str[1] = "Enter a new password";
            submit_str = "Change";
            y = 1;
          }
        }
        else {
          assert(wms->ms.passwd.pw_flag == 0);
          header_str = "Define a (initial) password (for this file) (may be empty)";
          submit_str = "Enter";
          y = 1;
        }
        assert(wms->mode != M_NONE && header_str != NULL);
        printf("\t\t\t<h1>%s</h1>\n", header_str);
        if (notice_str[0] != NULL) printf("\t\t\t<p>%s</p>\n", notice_str[0]);
        if (x != 0) printf("\t\t\t<p><input type=\"text\" name=\"password\" value=\"\" size=25></p>\n");
        if (notice_str[1] != NULL) printf("\t\t\t<p>%s</p>\n", notice_str[1]);
        if (y != 0) printf("\t\t\t<p><input type=\"text\" name=\"new-password\" value=\"\" size=25></p>\n");
        printf("\t\t\t<p><input type=\"submit\" name=\"file_action\" value=\"%s\">\n"
               "\t\t\t\t<input type=\"submit\" name=\"file_action\" value=\"Stop\"></p>\n"
               "\t\t</form>\n"
               "\t</body>\n"
               "</html>\n",
          submit_str);
        break;
      case B_NEW:
        assert(wms->seq == S_NEW);
        size = 14; // 'new-9999.imsf\0'
        str = malloc(size);
        e = str == NULL;
        if (e == 0) {
          i = 0;
          do {
            if (i > 0)
              rv = snprintf(str, size, "new-%d.imsf", i);
            else
              rv = snprintf(str, size, "new.imsf");
            e = rv < 0 || rv >= size;
            if (e == 0) {
              x = 0;
              for (j = 0; x == 0 && j < wms->fl_c; j++)
                x = strcmp(wms->fl_v[j], str) == 0;
              i++;
              e = i > 9999;
            }
          } while (e == 0 && x == 1);
          if (e == 0) {
            ext_str = rindex(str, '.');
            assert(ext_str != NULL);
            *ext_str = '\0';
            printf("\t\t\t<h1>Create a New file</h1>\n"
                   "\t\t\t<p><input type=\"text\" name=\"file-title\" value=\"%s\" size=25>.imsf</p>\n"
                   "\t\t\t<p><input type=\"submit\" name=\"file_action\" value=\"Create\">\n"
                   "\t\t\t<input type=\"submit\" name=\"file_action\" value=\"Stop\"></p>\n"
                   "\t\t</form>\n"
                   "\t\t<code>%s</code>\n"
                   "\t</body>\n"
                   "</html>\n",
                str,
                sw_info_str);
          }
          free(str);
        }
        break;
      case B_OPEN:
        rv = printf("\t\t\t<h1>Open file</h1>\n"
                    "\t\t\t<ul>\n");
        e = rv < 0;
        for (i = 0; e == 0 && i < wms->fl_c; i++) {
          e = xml_escape(&wms->html_lp, &wms->html_n, wms->fl_v[i], ESC_AMP | ESC_QUOT);
          if (e == 0) {
            rv = printf("\t\t\t\t<li><label><input type=\"radio\" name=\"file-title\" value=\"%s\">", wms->html_lp);
            e = rv < 0;
            if (e == 0) {
              e = xml_escape(&wms->html_lp, &wms->html_n, wms->fl_v[i], ESC_AMP | ESC_LT);
              rv = printf("%s</label></li>\n", wms->html_lp);
              e = rv < 0;
            }
          }
        }
        printf("\t\t\t</ul>\n"
               "\t\t\t<p><input type=\"submit\" name=\"event\" value=\"Open\">\n"
               "\t\t\t\t<input type=\"submit\" name=\"event\" value=\"Cancel\"></p>\n"
               "\t\t</form>\n"
               "\t\t<code>%s</code>\n"
               "\t</body>\n"
               "</html>\n",
          sw_info_str);
        break;
      case B_UPLOAD:
        assert(wms->file_title_str != NULL && strlen(wms->tok_str) == 40);
        printf("Content-Type: text/html; charset=utf-8\n\n"
               "<!DOCTYPE html>\n"
               "<html lang=\"en\">\n"
               "\t<head>\n"
               "\t\t<meta charset=\"utf-8\">\n"
               "\t\t<title>%s</title>\n"
               "\t\t<meta name=\"viewport\" content=\"width=device-width\">\n"
               "\t\t<link rel=\"shortcut icon\" href=\"/favicon.ico\">\n"
               "\t\t<link rel=\"stylesheet\" type=\"text/css\" href=\"/ms.css\">\n"
               "\t\t<link rel=\"license\" href=\"https://www.gnu.org/licenses/old-licenses/gpl-2.0.html\">\n"
               "\t</head>\n"
               "\t<body>\n"
               "\t\t<form enctype=\"multipart/form-data\" method=\"post\" action=\"memorysurfer.cgi\">\n"
               "\t\t\t<div>\n"
               "\t\t\t\t<input type=\"hidden\" name=\"file-title\" value=\"%s\">\n"
               "\t\t\t\t<input type=\"hidden\" name=\"token\" value=\"%s\">\n"
               "\t\t\t</div>\n"
               "\t\t\t<h1>Upload</h1>\n"
               "\t\t\t<p>Choose a (previously exported .XML) File to upload (which will be used for the Import)</p>\n"
               "\t\t\t<p><input type=\"file\" name=\"file_action\"></p>\n"
               "\t\t\t<p><input type=\"submit\" name=\"file_action\" value=\"Upload\">\n"
               "\t\t\t\t<input type=\"submit\" name=\"file_action\" value=\"Stop\"></p>\n"
               "\t\t</form>\n"
               "\t</body>\n"
               "</html>\n",
          title_str,
          wms->file_title_str,
          wms->tok_str);
        break;
      case B_UPLOAD_REPORT:
        printf("\t\t\t<h1>XML File imported</h1>\n"
               "\t\t\t<p><input type=\"submit\" name=\"file_action\" value=\"OK\"></p>\n"
               "\t\t</form>\n"
               "\t\t<code>%s; chunks=%d, swaps=%d, avg=%d</code>\n"
               "\t</body>\n"
               "</html>\n",
          sw_info_str,
          wms->ms.imf.chunk_count,
          wms->ms.imf.stat_swap,
          wms->ms.imf.stat_swap / wms->ms.imf.chunk_count);
        break;
      case B_EXPORT:
        e = wms->ms.imf_filename == NULL;
        if (e == 0)
        {
          dup_str = strdup (wms->ms.imf_filename);
          e = dup_str == NULL;
          if (e == 0)
          {
            f_basename = basename (dup_str);
            len = strlen(f_basename);
            e = len <= 5;
            if (e == 0)
            {
              dot = strrchr (dup_str, '.');
              e = strcmp (dot, ".imsf") != 0;
              if (e == 0)
              {
                strcpy (dot, ".xml");
                printf("Content-Disposition: attachment; filename=\"%s\"\n"
                       "Content-Type: text/xml; charset=utf-8\n\n",
                  f_basename);
                xg.w_stream = stdout;
                e = fputs ("<memorysurfer>", xg.w_stream) <= 0;
                if (e == 0)
                {
                  xg.indent = 1;
                  set_indent (xg.indent_str, xg.indent, sizeof (xg.indent_str));
                  xg.w_lineptr = NULL;
                  xg.w_n = 0;
                  e = gen_xml_category (wms->ms.n_first, &xg, &wms->ms);
                  if (e == 0)
                  {
                    e = fputs ("</memorysurfer>", xg.w_stream) <= 0;
                  }
                  free (xg.w_lineptr);
                }
              }
            }
            free (dup_str);
          }
        }
        break;
      case B_START_CAT:
        assert(wms->file_title_str != NULL && strlen(wms->tok_str) == 40);
        dis_str = wms->ms.n_first >= 0 ? "" : " disabled";
        printf("\t\t\t<h1>Categories</h1>\n"
               "\t\t\t<p><input type=\"submit\" name=\"event\" value=\"Create\"></p>\n"
               "\t\t\t<p><input type=\"submit\" name=\"event\" value=\"Rename\"%s></p>\n"
               "\t\t\t<p><input type=\"submit\" name=\"event\" value=\"Move\"%s></p>\n"
               "\t\t\t<p><input type=\"submit\" name=\"event\" value=\"Delete\"%s></p>\n"
               "\t\t\t<p><input type=\"submit\" name=\"event\" value=\"Toggle\"%s></p>\n"
               "\t\t\t<p><input type=\"submit\" name=\"file_action\" value=\"Cancel\"></p>\n"
               "\t\t</form>\n"
               "\t</body>\n"
               "</html>\n",
            dis_str,
            dis_str,
            dis_str,
            dis_str);
        break;
      case B_SELECT_CREATE_CAT:
      case B_SELECT_CAT:
      case B_SELECT_SEND_CAT:
        assert(wms->file_title_str != NULL && strlen(wms->tok_str) == 40);
        if (wms->seq == S_SELECT_DEST_CAT || wms->seq == S_SELECT_SEND_CAT) {
          assert(wms->ms.cat_i >= 0);
          printf("\t\t\t\t<input type=\"hidden\" name=\"mov-cat\" value=\"%d\">\n",
            wms->ms.cat_i);
        }
        if (wms->seq == S_SELECT_SEND_CAT) {
          assert(wms->ms.card_i >= 0);
          printf("\t\t\t\t<input type=\"hidden\" name=\"mov-card\" value=\"%d\">\n",
            wms->ms.card_i);
        }
        if (wms->seq == S_SELECT_CREATE_CAT) {
          notice_str[0] = "where to create the new category to";
          submit_str = "Select";
        }
        else if (wms->seq == S_SELECT_DEST_CAT) {
          notice_str[0] = "where to move";
          submit_str = "Select";
        }
        else if (wms->seq == S_SELECT_RENAME_CAT) {
          notice_str[0] = "to rename";
          submit_str = "Rename";
        }
        else if (wms->seq == S_SELECT_MOVE_CAT) {
          notice_str[0] = "to move";
          submit_str = "Move";
        }
        else if (wms->seq == S_SELECT_DELETE_CAT) {
          notice_str[0] = "to delete";
          submit_str = "Delete";
        }
        else if (wms->seq == S_SELECT_TOGGLE_CAT) {
          notice_str[0] = "to Collapse / Expand";
          submit_str = "Toggle";
        }
        else {
          assert(wms->seq == S_SELECT_SEND_CAT);
          notice_str[0] = "to move the card to";
          submit_str = "Send";
        }
        assert(strlen(mtime_str) == 16);
        printf("\t\t\t</div>\n"
               "\t\t\t<h1>Select the category %s</h1>\n",
          notice_str[0]);
        gen_html_cat(wms->ms.n_first, 3, H_CHILD, wms);
        printf("\t\t\t<p><input type=\"submit\" name=\"event\" value=\"%s\">\n"
               "\t\t\t\t<input type=\"submit\" name=\"event\" value=\"Stop\"></p>\n"
               "\t\t</form>\n"
               "\t\t<code>%s</code>\n"
               "\t</body>\n"
               "</html>\n",
          submit_str,
          sw_info_str);
        break;
      case B_SELECT_ARRANGE:
      case B_SELECT_CARD_ARRANGE:
        assert(wms->file_title_str != NULL && strlen(wms->tok_str) == 40);
        if (wms->ms.mov_card_i >= 0)
          printf("\t\t\t\t<input type=\"hidden\" name=\"mov-card\" value=\"%d\">\n",
            wms->ms.mov_card_i);
        if (wms->seq == S_SELECT_ARRANGE)
          submit_str = "Select";
        else if (wms->seq == S_SELECT_MOVE_ARRANGE) {
          assert(wms->ms.mov_cat_i >= 0);
          printf("\t\t\t\t<input type=\"hidden\" name=\"mov-cat\" value=\"%d\">\n",
            wms->ms.mov_cat_i);
          submit_str = "Move";
        }
        else {
          assert(wms->seq == S_ARRANGE);
          submit_str = "Move";
        }
        printf("\t\t\t</div>\n"
               "\t\t\t<h1>Select how to arrange</h1>\n"
               "\t\t\t<p>\n");
        for (i = 0; i < 3; i++)
          if (wms->seq != S_ARRANGE || i != 1)
            printf("\t\t\t\t<label><input type=\"radio\" name=\"arrange\" value=\"%d\"%s>%s</label>\n",
              i,
              i == wms->ms.arrange ? " checked" : "",
              ARRANGE[i]);
        printf("\t\t\t</p>\n"
               "\t\t\t<p><input type=\"submit\" name=\"event\" value=\"%s\">\n"
               "\t\t\t\t<input type=\"submit\" name=\"event\" value=\"Stop\"></p>\n"
               "\t\t</form>\n"
               "\t\t<code>%s</code>\n"
               "\t</body>\n"
               "</html>\n",
          submit_str,
          sw_info_str);
        break;
      case B_CAT_NAME:
        assert(wms->file_title_str != NULL && strlen(wms->tok_str) == 40);
        if (wms->seq == S_SELECT_CREATE_CAT || wms->seq == S_CAT_NAME) {
          notice_str[0] = "Enter the name of the category to create.";
          text_str = wms->ms.cat_name != NULL ? wms->ms.cat_name : "new category name";
          submit_str = "Create";
        }
        else {
          assert(wms->seq == S_RENAME_ENTER && wms->ms.cat_i >= 0 && wms->ms.cat_i < wms->ms.cat_a);
          notice_str[0] = "Enter the name to rename the category to.";
          text_str = sa_get(&wms->ms.cat_sa, wms->ms.cat_i);
          submit_str = "Rename";
        }
        printf("\t\t\t<h1>%s</h1>\n"
               "\t\t\t<p><input type=\"text\" name=\"cat_name\" value=\"%s\" size=25></p>\n"
               "\t\t\t<p><input type=\"submit\" name=\"event\" value=\"%s\">\n"
               "\t\t\t\t<input type=\"submit\" name=\"event\" value=\"Stop\"></p>\n"
               "\t\t</form>\n"
               "\t\t<code>%s</code>\n"
               "\t</body>\n"
               "</html>\n",
          notice_str[0],
          text_str,
          submit_str,
          sw_info_str);
        break;
      case B_SELECT_EDIT_CAT:
        assert(wms->file_title_str != NULL && strlen(wms->tok_str) == 40);
        printf("\t\t\t<h1>Select a category to edit</h1>\n");
        gen_html_cat (wms->ms.n_first, 3, H_CHILD, wms);
        printf("\t\t\t<p><input type=\"submit\" name=\"event\" value=\"Edit\">\n"
               "\t\t\t\t<input type=\"submit\" name=\"learn_action\" value=\"Learn\">\n"
               "\t\t\t\t<input type=\"submit\" name=\"edit_action\" value=\"Cancel\"></p>\n"
               "\t\t</form>\n"
               "\t\t<code>%s</code>\n"
               "\t</body>\n"
               "</html>\n",
          sw_info_str);
        break;
      case B_EDIT:
        q_str = sa_get(&wms->ms.card_sa, 0);
        a_str = sa_get(&wms->ms.card_sa, 1);
        e = xml_escape(&wms->html_lp, &wms->html_n, q_str, ESC_AMP | ESC_LT);
        if (e == 0) {
          assert(strlen(mtime_str) == 16 && wms->file_title_str != NULL && strlen(wms->tok_str) == 40);
          printf("\t\t\t<p><input type=\"submit\" name=\"event\" value=\"Insert\"%s>\n"
                 "\t\t\t\t<input type=\"submit\" name=\"edit_action\" value=\"Append\">\n"
                 "\t\t\t\t<input type=\"submit\" name=\"edit_action\" value=\"Delete\"%s>\n"
                 "\t\t\t\t<input type=\"submit\" name=\"edit_action\" value=\"Previous\"%s>\n"
                 "\t\t\t\t<input type=\"submit\" name=\"edit_action\" value=\"Next\"%s></p>\n"
                 "\t\t\t<div><textarea name=\"q\" rows=\"10\" cols=\"46\"%s>%s</textarea></div>\n"
                 "\t\t\t<p><input type=\"submit\" name=\"edit_action\" value=\"Schedule\"%s>\n"
                 "\t\t\t\t<input type=\"submit\" name=\"event\" value=\"Set\"%s>\n"
                 "\t\t\t\t<input type=\"submit\" name=\"event\" value=\"Move\"%s>\n"
                 "\t\t\t\t<input type=\"submit\" name=\"event\" value=\"Send\"%s></p>\n",
            wms->ms.card_a > 0 ? "" : " disabled",
            wms->ms.card_a > 0 ? "" : " disabled",
            wms->ms.card_a > 0 && wms->ms.card_i > 0 ? "" : " disabled",
            wms->ms.card_a > 0 && wms->ms.card_i + 1 < wms->ms.card_a ? "" : " disabled",
            q_str != NULL ? "" : " disabled",
            wms->html_lp,
            wms->ms.card_i >= 0 && wms->ms.card_a > 0 && wms->ms.card_i < wms->ms.card_a && wms->ms.card_l[wms->ms.card_i].card_state >= STATE_NEW ? "" : " disabled",
            wms->ms.card_i != wms->ms.mov_card_i ? "" : " disabled",
            wms->ms.mov_card_i != -1 && wms->ms.card_i != wms->ms.mov_card_i ? "" : " disabled",
            wms->ms.card_i != -1 ? "" : " disabled");
          e = xml_escape(&wms->html_lp, &wms->html_n, a_str, ESC_AMP | ESC_LT);
          if (e == 0) {
            printf("\t\t\t<div><textarea name=\"a\" rows=\"10\" cols=\"46\"%s>%s</textarea></div>\n"
                   "\t\t\t<p><input type=\"submit\" name=\"learn_action\" value=\"Learn\">\n"
                   "\t\t\t\t<input type=\"submit\" name=\"edit_action\" value=\"Search\">\n"
                   "\t\t\t\t<input type=\"submit\" name=\"edit_action\" value=\"Stop\"></p>\n",
              a_str != NULL ? "" : " disabled",
              wms->html_lp);
            imf_info_swaps(&wms->ms.imf);
            printf("\t\t\t<div>\n");
            if (wms->ms.mov_card_i >= 0)
              printf("\t\t\t\t<input type=\"hidden\" name=\"mov-card\" value=\"%d\">\n",
                wms->ms.mov_card_i);
            printf("\t\t\t</div>\n"
                   "\t\t</form>\n"
                   "\t\t<code>%s; chunks=%d, swaps=%d, avg=%d</code>\n"
                   "\t</body>\n"
                   "</html>\n",
              sw_info_str,
              wms->ms.imf.chunk_count,
              wms->ms.imf.stat_swap,
              wms->ms.imf.stat_swap / wms->ms.imf.chunk_count);
          }
        }
        break;
      case B_SELECT_LEARN_CAT:
        assert(wms->file_title_str != NULL && strlen(wms->tok_str) == 40);
        printf("\t\t\t<h1>Select a category to learn</h1>\n");
        gen_html_cat(wms->ms.n_first, 3, H_CHILD, wms);
        printf("\t\t\t<p><input type=\"submit\" name=\"learn_action\" value=\"Learn\">\n"
               "\t\t\t<input type=\"submit\" name=\"event\" value=\"Edit\">\n"
               "\t\t\t\t<input type=\"submit\" name=\"learn_action\" value=\"Cancel\"></p>\n"
               "\t\t</form>\n"
               "\t\t<code>%s</code>\n"
               "\t</body>\n"
               "</html>\n",
          sw_info_str);
        break;
      case B_SELECT_SEARCH_CAT:
        assert(wms->file_title_str != NULL && strlen(wms->tok_str) == 40);
        printf("\t\t\t<h1>Select a category to search</h1>\n");
        gen_html_cat (wms->ms.n_first, 3, H_CHILD, wms);
        printf("\t\t\t<p><input type=\"submit\" name=\"event\" value=\"Search\">\n"
               "\t\t\t<input type=\"submit\" name=\"search_action\" value=\"Cancel\"></p>\n"
               "\t\t</form>\n"
               "\t\t<code>%s</code>\n"
               "\t</body>\n"
               "</html>\n",
          sw_info_str);
        break;
      case B_SEARCH:
        e = xml_escape(&wms->html_lp, &wms->html_n, wms->ms.search_txt, ESC_AMP | ESC_QUOT);
        if (e == 0) {
          assert(wms->file_title_str != NULL && strlen(wms->tok_str) == 40);
          printf("\t\t\t<p><input type=\"text\" name=\"search_txt\" value=\"%s\" size=25></p>\n", wms->html_lp);
          q_str = sa_get(&wms->ms.card_sa, 0);
          a_str = sa_get(&wms->ms.card_sa, 1);
          e = xml_escape(&wms->html_lp, &wms->html_n, q_str, ESC_AMP | ESC_LT);
          if (e == 0) {
            printf("\t\t\t<div><textarea rows=\"10\" cols=\"46\"%s>%s</textarea></div>\n"
                   "\t\t\t<p><input type=\"submit\" name=\"search_action\" value=\"Reverse\"%s>\n"
                   "\t\t\t<input type=\"submit\" name=\"search_action\" value=\"Forward\"%s></p>\n",
              wms->found_str != NULL ? " readonly" : " disabled",
              wms->html_lp,
              wms->ms.card_a > 0 ? "" : " disabled",
              wms->ms.card_a > 0 ? "" : " disabled");
            e = xml_escape(&wms->html_lp, &wms->html_n, a_str, ESC_AMP | ESC_LT);
            if (e == 0) {
              printf("\t\t\t<div><textarea rows=\"10\" cols=\"46\"%s>%s</textarea></div>\n"
                     "\t\t\t<p><input type=\"submit\" name=\"event\" value=\"Edit\">\n"
                     "\t\t\t\t<input type=\"submit\" name=\"learn_action\" value=\"Learn\"%s>\n"
                     "\t\t\t\t<input type=\"submit\" name=\"search_action\" value=\"Stop\"></p>\n"
                     "\t\t</form>\n"
                     "\t\t<code>%s</code>\n"
                     "\t</body>\n"
                     "</html>\n",
                wms->found_str != NULL ? " readonly" : " disabled",
                wms->html_lp,
                wms->ms.card_a > 0 ? "" : " disabled",
                sw_info_str);
            }
          }
        }
        break;
      case B_PREFERENCES:
        assert(wms->file_title_str != NULL && strlen(wms->tok_str) == 40);
        printf("\t\t\t<h1>Preferences</h1>\n"
               "\t\t\t<p>Timeout</p>\n"
               "\t\t\t<div>\n");
        j = -1;
        for (i = 0; i < 5 && j == -1; i++)
          if (timeouts[i][0]*timeouts[i][1] >= wms->ms.passwd.tok_count*wms->ms.passwd.tok_sec)
            j = i;
        if (j == -1)
          j = 1;
        for (i = 0; i < 5; i++) {
          attr_str = i == j ? " checked" : "";
          printf("\t\t\t\t<label><input type=\"radio\" name=\"timeout\" value=\"%d\"%s>%s</label>\n",
              i,
              attr_str,
              TIMEOUTS[i]);
        }
        printf("\t\t\t</div>\n"
               "\t\t\t<p><input type=\"submit\" name=\"event\" value=\"Apply\">\n"
               "\t\t\t\t<input type=\"submit\" name=\"event\" value=\"Cancel\"></p>\n"
               "\t\t</form>\n"
               "\t\t<code>%s</code>\n"
               "\t</body>\n"
               "</html>\n",
          sw_info_str);
        break;
      case B_ABOUT:
        rv = printf("\t\t\t<h1>About MemorySurfer v1.0.1.6</h1>\n"
                    "\t\t\t<p>Author: Lorenz Pullwitt</p>\n"
                    "\t\t\t<p>Copyright 2016-2021</p>\n"
                    "\t\t\t<p>Send bugs and suggestions to\n"
                    "<a href=\"mailto:memorysurfer@lorenz-pullwitt.de\">memorysurfer@lorenz-pullwitt.de</a></p>\n"
                    "\t\t\t<cite>MemorySurfer is free software; you can redistribute it and/or\n"
                    "modify it under the terms of the GNU General Public License\n"
                    "as published by the Free Software Foundation; either version 2\n"
                    "of the License, or (at your option) any later version.</cite>\n"
                    "\t\t\t<cite>This program is distributed in the hope that it will be useful,\n"
                    "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
                    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the\n"
                    "<a href=\"https://www.gnu.org/licenses/\">GNU\302\240General\302\240Public\302\240License</a> for more details.</cite>\n"
                    "\t\t\t<p><input type=\"submit\" name=\"event\" value=\"OK\"></p>\n"
                    "\t\t</form>\n"
                    "\t\t<code>%s</code>\n"
                    "\t</body>\n"
                    "</html>\n",
            sw_info_str);
        e = rv < 0;
        break;
      case B_LEARN:
        q_str = sa_get(&wms->ms.card_sa, 0);
        a_str = sa_get(&wms->ms.card_sa, 1);
        e = q_str == NULL || a_str == NULL || (wms->mode != M_ASK && wms->mode != M_RATE) || strlen(mtime_str) != 16 || wms->file_title_str == NULL || strlen(wms->tok_str) != 40;
        if (e == 0) {
          e = xml_escape(&wms->html_lp, &wms->html_n, q_str, ESC_AMP | ESC_LT);
          if (e == 0) {
            rv = printf("\t\t\t<p><input type=\"submit\" name=\"learn_action\" value=\"Proceed\"%s>\n"
                        "\t\t\t\t<input type=\"submit\" name=\"event\" value=\"Show\"%s>\n"
                        "\t\t\t\t<input type=\"submit\" name=\"event\" value=\"Reveal\"%s>\n"
                        "\t\t\t\t<input type=\"submit\" name=\"event\" value=\"Histogram\"></p>\n"
                        "\t\t\t<div><textarea rows=\"10\" cols=\"46\" readonly>%s</textarea></div>\n"
                        "\t\t\t<table>\n",
                wms->mode != M_RATE ? " disabled" : "",
                wms->mode != M_ASK ? " disabled" : "",
                wms->mode != M_ASK ? " disabled" : "",
                wms->html_lp);
            e = rv < 0;
          }
          if (e == 0) {
            if (wms->mode == M_ASK) {
              for (y = 0; y < 3 && e == 0; y++) {
                rv = printf ("\t\t\t\t<tr>\n");
                e = rv < 0;
                for (x = 0; x < 2 && e == 0; x++) {
                  rv = printf("\t\t\t\t\t<td><label><input type=\"radio\" disabled>Level</label></td>\n");
                  e = rv < 0;
                }
                if (e == 0) {
                  rv = printf("\t\t\t\t</tr>\n");
                  e = rv < 0;
                }
              }
            } else {
              assert(wms->mode == M_RATE || wms->ms.card_i != -1);
              if (wms->ms.card_l[wms->ms.card_i].card_state == STATE_NEW) {
                time_diff = lvl_s[1];
                lvl = 0;
              } else {
                time_diff = wms->ms.timestamp - wms->ms.card_l[wms->ms.card_i].card_time;
                for (i = 0; i < 21; i++)
                  if (lvl_s[i] >= wms->ms.card_l[wms->ms.card_i].card_strength)
                    break;
                lvl = i;
              }
              j = lvl - 2;
              if (j < 0)
                j = 0;
              for (i = 0; i < 21; i++)
                if (lvl_s[i] >= time_diff)
                  break;
              lvl_sel = i;
              if (lvl_sel > j + 5)
                lvl_sel = j + 5;
              for (y = 0; y < 3 && e == 0; y++) {
                rv = printf("\t\t\t\t<tr>\n");
                e = rv < 0;
                for (x = 0; x < 2 && e == 0; x++) {
                  i = j + x + y * 2;
                  attr_str = "";
                  if (i == lvl)
                    attr_str = " checked";
                  if (i == lvl_sel)
                    attr_str = " autofocus";
                  set_time_str(time_str, lvl_s[i]);
                  rv = printf("\t\t\t\t\t<td><label><input type=\"radio\" name=\"lvl\" value=\"%d\"%s>Level %d (%s)</label></td>\n",
                      i, attr_str,
                      i, time_str);
                  e = rv < 0;
                }
                if (e == 0) {
                  rv = printf("\t\t\t\t</tr>\n");
                  e = rv < 0;
                }
              }
            }
          }
          if (e == 0) {
            assert(wms->mode == M_ASK && wms->reveal_pos == -1 ? wms->ms.cards_nel >= 0 : 1);
            e = xml_escape(&wms->html_lp, &wms->html_n, a_str, ESC_AMP | ESC_LT);
            if (e == 0) {
              rv = printf("\t\t\t</table>\n"
                          "\t\t\t<div><textarea rows=\"10\" cols=\"46\"%s>%s</textarea></div>\n"
                          "\t\t\t<p><input type=\"submit\" name=\"event\" value=\"Edit\">\n"
                          "\t\t\t\t<input type=\"submit\" name=\"event\" value=\"Search\">\n"
                          "\t\t\t\t<input type=\"submit\" name=\"learn_action\" value=\"Stop\">\n"
                          "\t\t\t\t<input type=\"submit\" name=\"learn_action\" value=\"Suspend\"%s>\n"
                          "\t\t\t\t<input type=\"submit\" name=\"event\" value=\"Resume\"%s></p>\n"
                          "\t\t</form>\n"
                          "\t\t<code>%s; nel: %d; html_n: %zu</code>\n"
                          "\t</body>\n"
                          "</html>\n",
                  wms->mode == M_RATE || wms->reveal_pos > 0 ? " readonly" : " disabled",
                  wms->mode == M_RATE || wms->reveal_pos > 0 ? wms->html_lp : "",
                  wms->ms.card_l[wms->ms.card_i].card_state != STATE_SUSPENDED ? "" : " disabled",
                  wms->ms.can_resume != 0 ? "" : " disabled",
                  sw_info_str,
                  wms->ms.cards_nel,
                  wms->html_n);
              e = rv < 0;
            }
          }
        }
        if (e != 0)
          e = 0x0583cf25; // WMSBLN (Web)MemorySurfer B_LEARN failed
        break;
      case B_MSG:
        assert(wms->static_msg != NULL && wms->static_btn_main != NULL);
        assert(wms->todo_main >= S_FILE && wms->todo_main <= S_END);
        rv = printf("\t\t\t\t<input type=\"hidden\" name=\"todo_main\" value=\"%d\">\n", wms->todo_main);
        e = rv < 0;
        if (e == 0 && wms->todo_alt >= S_FILE) {
          assert(wms->todo_alt <= S_END);
          rv = printf("\t\t\t\t<input type=\"hidden\" name=\"todo_alt\" value=\"%d\">\n", wms->todo_alt);
          e = rv < 0;
        }
        if (e == 0) {
          rv = printf("\t\t\t</div>\n"
                      "\t\t\t<h1>%s</h1>\n"
                      "\t\t\t<p>\n"
                      "\t\t\t\t<input type=\"submit\" name=\"event\" value=\"%s\">\n",
              wms->static_msg,
              wms->static_btn_main);
          e = rv < 0;
        }
        if (e == 0 && wms->static_btn_alt != NULL) {
          rv = printf("\t\t\t\t<input type=\"submit\" name=\"event\" value=\"%s\">\n", wms->static_btn_alt);
          e = rv < 0;
        }
        if (e == 0) {
          rv = printf("\t\t\t</p>\n"
                      "\t\t</form>\n"
                      "\t\t<code>%s</code>\n"
                      "\t</body>\n"
                      "</html>\n",
              sw_info_str);
          e = rv < 0;
        }
        break;
      case B_HISTOGRAM:
        wms->html_lp[0] = '\0';
        strcat(wms->html_lp, "M1 61");
        nb = 5;
        y = 0;
        for (i = 0; i < 100 && e == 0; i++) {
          e = nb + 3 * sizeof(path_str) + 1 > wms->html_n;
          if (e == 0) {
            dy = y - wms->hist_bucket[i];
            if (dy != 0) {
              nb += sprintf(path_str, "v%d", dy);
              strcat(wms->html_lp, path_str);
              y -= dy;
            }
            strcat(wms->html_lp, "h1");
            nb += 2;
          }
        }
        if (e == 0) {
          sprintf(path_str, "v%d", y);
          strcat(wms->html_lp, path_str);
          strcat(wms->html_lp, "z");
        }
        e = imf_info_gaps(&wms->ms.imf);
        notice_str[0] = e == 0 ? wms->ms.imf.stats_gaps_str : "error";
        rv = printf("\t\t\t<h1>Histogram and Table</h1>\n"
                    "\t\t\t<p>Retention</p>\n"
                    "\t\t\t<svg viewbox=\"0 0 101 62\">\n"
                    "\t\t\t\t<path d=\"%s\" />\n"
                    "\t\t\t</svg>\n"
                    "\t\t\t<p>Strength</p>\n"
                    "\t\t\t<table>\n",
            wms->html_lp);
        e = rv < 0;
        for (i = 0; i < 21 && e == 0; i++) {
          rv = printf("\t\t\t\t<tr><td>%d</td><td>%d</td></tr>\n", i, wms->lvl_bucket[i]);
          e = rv < 0;
        }
        if (e == 0) {
          rv = printf("\t\t\t</table>\n"
                      "\t\t\t<p><input type=\"submit\" name=\"event\" value=\"Edit\">\n"
                      "\t\t\t\t<input type=\"submit\" name=\"learn_action\" value=\"Learn\">\n"
                      "\t\t\t\t<input type=\"submit\" name=\"event\" value=\"Search\">\n"
                      "\t\t\t\t<input type=\"submit\" name=\"event\" value=\"Refresh\">\n"
                      "\t\t\t\t<input type=\"submit\" name=\"event\" value=\"Done\"></p>\n"
                      "\t\t</form>\n"
                      "\t\t<code>%s; gaps=%d, gaps_size=%d, Details: %s</code>\n"
                      "\t</body>\n"
                      "</html>\n",
              sw_info_str,
              wms->ms.imf.stats_gaps,
              wms->ms.imf.stats_gaps_space,
              notice_str[0]);
          e = rv < 0;
        }
        break;
      default:
        e = 0x011e23b9; // WMSGHA (Web)MemorySurfer gen_html assert (failed)
        break;
      }
    }
    free(sw_info_str);
  }
  return e;
}

int ms_init(struct MemorySurfer *ms)
{
  int e;
  size_t size;
  imf_init(&ms->imf);
  ms->imf_filename = NULL;
  sa_init(&ms->cat_sa);
  assert(sizeof(struct Category) == 12);
  ms->cat_t = NULL;
  ms->cat_a = 0;
  ms->n_first = -1;
  ms->cat_i = -1;
  ms->mov_cat_i = -1;
  ms->arrange = -1;
  ms->cat_name = NULL;
  ms->card_l = NULL;
  ms->card_a = 0;
  ms->card_i = -1;
  ms->mov_card_i = -1;
  ms->cards_nel = -1;
  sa_init(&ms->card_sa);
  ms->timestamp = time(NULL);
  e = ms->timestamp == -1;
  if (e == 0) {
    ms->search_txt = NULL;
    ms->srch_dir = 0;
    ms->can_resume = 0;
    ms->password = NULL;
    ms->new_password = NULL;
    size = sizeof(ms->passwd.pw_msg_digest);
    memset(&ms->passwd.pw_msg_digest, -1, size);
    ms->passwd.pw_flag = -1;
    ms->passwd.tok_sec = 0;
    ms->passwd.tok_count = 0;
  }
  return e;
}

int wms_init(struct WebMemorySurfer *wms)
{
  int e;
  e = ms_init(&wms->ms);
  if (e == 0) {
    assert(A_END == 0);
    wms->seq = S_END;
    wms->page = P_START;
    wms->from_page = -1;
    wms->mode = M_NONE;
    wms->saved_mode = M_NONE;
    wms->timeout = -1;
    wms->mult.post_lp = NULL;
    wms->mult.post_n = 0;
    wms->boundary_str = NULL;
    wms->dbg_n = 720;
    wms->dbg_lp = malloc(wms->dbg_n);
    e = wms->dbg_lp == NULL;
    if (e == 0) {
      wms->dbg_lp[0] = '\0';
      wms->dbg_wp = 0;
      wms->file_title_str = NULL;
      wms->fl_v = NULL;
      wms->fl_c = 0;
      sa_init(&wms->qa_sa);
      wms->reveal_pos = -1;
      wms->saved_reveal_pos = -1;
      wms->sw_i = -1;
      wms->static_msg = NULL;
      wms->static_btn_main = NULL;
      wms->static_btn_alt = NULL;
      wms->todo_main = -1;
      wms->todo_alt = -1;
      wms->html_n = 500;
      wms->html_lp = malloc(wms->html_n);
      e = wms->html_lp == NULL;
      if (e == 0) {
        memset(wms->mtime, -1, sizeof(wms->mtime));
        memset(wms->tok_digest, -1, sizeof(wms->tok_digest));
        wms->tok_str[0] = '\0';
      }
    }
  }
  return e;
}

void ms_free (struct MemorySurfer *ms)
{
  free (ms->imf_filename);
  sa_free (&ms->cat_sa);
  free (ms->cat_t);
  ms->cat_t = NULL;
  ms->cat_a = 0;
  free (ms->cat_name);
  ms->cat_name = NULL;
  free(ms->card_l);
  ms->card_l = NULL;
  ms->card_a = 0;
  ms->card_i = -1;
  sa_free (&ms->card_sa);
  free (ms->search_txt);
  free (ms->password);
  free(ms->new_password);
  char b;
  b = imf_is_open (&ms->imf);
  if (b)
  {
    imf_close (&ms->imf);
  }
  sw_free(&ms->imf.sw);
}

void wms_free (struct WebMemorySurfer *wms)
{
  int i;
  free(wms->html_lp);
  for (i = 0; i < wms->fl_c; i++) {
    assert(wms->fl_v[i] != NULL);
    free(wms->fl_v[i]);
  }
  free(wms->fl_v);
  sa_free(&wms->qa_sa);
  free(wms->mult.post_lp);
  free(wms->boundary_str);
  free(wms->dbg_lp);
  free(wms->file_title_str);
  ms_free(&wms->ms);
}

int ms_create (struct MemorySurfer *ms, int flags_mask)
{
  int e;
  size_t size;
  assert (ms->imf_filename != NULL);
  e = imf_create (&ms->imf, ms->imf_filename, flags_mask);
  if (e == 0)
  {
    e = imf_put (&ms->imf, SA_INDEX, "", 0);
    if (e == 0)
    {
      e = imf_put (&ms->imf, C_INDEX, "", 0);
      if (e == 0) {
        if (ms->passwd.pw_flag < 0) {
          ms->passwd.pw_flag = 0;
          assert(ms->passwd.tok_sec == 0);
          ms->passwd.tok_sec = 60;
          assert(ms->passwd.tok_count == 0);
          ms->passwd.tok_count = 10;
        }
        else {
          assert(ms->passwd.pw_flag == 1);
        }
        size = sizeof(struct Password);
        e = imf_put(&ms->imf, PW_INDEX, &ms->passwd, size);
        if (e == 0)
          e = imf_sync (&ms->imf);
      }
    }
  }
  return e;
}

static int ms_get_card_sa(struct MemorySurfer *ms) {
  int e;
  assert(ms->card_l != NULL);
  e = 0;
  if (ms->card_i != -1) {
    assert(ms->card_i >= 0 && ms->card_i < ms->card_a);
    e = sa_load(&ms->card_sa, &ms->imf, ms->card_l[ms->card_i].card_qai);
  }
  return e;
}

static int RANK = 6;

static int ms_determine_card(struct MemorySurfer *ms)
{
  int e;
  time_t time_diff;
  int32_t card_strength_thr; // threshold
  double reten; // retention
  double reten_state[4];
  time_t state_time_diff[4];
  int sel_card[4]; // selected
  int card_state;
  int card_i;
  if (ms->card_a > 0) {
    ms->timestamp = time(NULL);
    e = ms->timestamp == -1;
    if (e == 0) {
      card_strength_thr = lvl_s[20];
      ms->card_i = -1;
      ms->cards_nel = 0;
      for (card_state = 0; card_state <= STATE_SUSPENDED; card_state++) {
        reten_state[card_state] = 1.0;
        state_time_diff[card_state] = INT32_MAX;
        sel_card[card_state] = -1;
      }
      for (card_i = 0; card_i < ms->card_a && e == 0; card_i++) {
        time_diff = ms->timestamp - ms->card_l[card_i].card_time;
        reten = exp(-(double)time_diff / ms->card_l[card_i].card_strength);
        if (reten <= 0.36788) { // 1/e
          card_state = ms->card_l[card_i].card_state;
          switch (card_state)
          {
          case STATE_SCHEDULED:
            ms->cards_nel++;
            if (ms->card_l[card_i].card_strength <= card_strength_thr) {
              if (card_strength_thr > lvl_s[RANK]) {
                if (ms->card_l[card_i].card_strength <= lvl_s[RANK]) {
                  card_strength_thr = lvl_s[RANK];
                  reten_state[STATE_SCHEDULED] = 1.0;
                }
              }
              if (reten < reten_state[STATE_SCHEDULED] || (reten == reten_state[STATE_SCHEDULED] && time_diff < state_time_diff[STATE_SCHEDULED])) {
                reten_state[STATE_SCHEDULED] = reten;
                state_time_diff[STATE_SCHEDULED] = time_diff;
                sel_card[STATE_SCHEDULED] = card_i;
              }
            }
            break;
          case STATE_ALARM:
          case STATE_NEW:
          case STATE_SUSPENDED:
            if (reten < reten_state[card_state]) {
              reten_state[card_state] = reten;
              sel_card[card_state] = card_i;
            }
            break;
          default:
            e = 0x0009100c; // MSDCA MemorySurfer ms_determine_card assert (failed)
          }
        }
      }
      if (e == 0) {
        if (sel_card[STATE_SCHEDULED] != -1)
          ms->card_i = sel_card[STATE_SCHEDULED];
        else if (sel_card[STATE_NEW] != -1)
          ms->card_i = sel_card[STATE_NEW];
        else if (sel_card[STATE_SUSPENDED] != -1)
          ms->card_i = sel_card[STATE_SUSPENDED];
        if (ms->card_i != -1)
          e = ms_get_card_sa (ms);
        else
          e = -1;
      }
    }
  }
  else
    e = -1;
  return e;
}

int sa_cmp (struct StringArray *sa_ls, struct StringArray *sa_rs)
{
  int eq; // equal
  int i;
  int j;
  int n;
  char ch_ls; // left side
  char ch_rs; // right
  n = sa_ls->sa_c;
  eq = n == sa_rs->sa_c;
  if (eq != 0)
  {
    i = 0;
    j = 0;
    while (i < n && eq != 0)
    {
      do
      {
        ch_ls = sa_ls->sa_d[j];
        ch_rs = sa_rs->sa_d[j];
        eq = ch_ls == ch_rs;
        j++;
      } while (eq != 0 && ch_ls != '\0');
      i++;
    }
  }
  return eq;
}

static void
sa_move (struct StringArray *sa_dest, struct StringArray *sa_src)
{
  free (sa_dest->sa_d);
  sa_dest->sa_d = sa_src->sa_d;
  sa_src->sa_d = NULL;
  sa_dest->sa_c = sa_src->sa_c;
  sa_src->sa_c = 0;
  sa_dest->sa_n = sa_src->sa_n;
  sa_src->sa_n = 0;
}

int ms_modify_qa (struct StringArray *sa, struct MemorySurfer *ms)
{
  int e;
  int eq;
  int32_t data_size;
  struct Card *card_ptr;
  e = 0;
  eq = sa_cmp (sa, &ms->card_sa);
  if (eq == 0)
  {
    sa_move (&ms->card_sa, sa);
    data_size = sa_length (&ms->card_sa);
    card_ptr = ms->card_l + ms->card_i;
    e = imf_put (&ms->imf, card_ptr->card_qai, ms->card_sa.sa_d, data_size) == 0 ? 1 : -1;
  }
  return e;
}

int ms_load_card_list (struct MemorySurfer *ms)
{
  int e;
  struct Category *cat_ptr;
  int32_t data_size;
  cat_ptr = ms->cat_t + ms->cat_i;
  assert (cat_ptr->cat_used != 0);
  data_size = imf_get_size (&ms->imf, cat_ptr->cat_cli);
  assert (data_size >= 0);
  assert (ms->card_l == NULL);
  ms->card_l = malloc (data_size);
  e = ms->card_l == NULL;
  if (e == 0)
  {
    e = imf_get (&ms->imf, cat_ptr->cat_cli, ms->card_l);
    if (e == 0)
    {
      assert (ms->card_a == 0);
      ms->card_a = data_size / sizeof (struct Card);
    }
  }
  return e;
}

void str_tolower (char *str)
{
  int i;
  char ch;
  static uint8_t map[256] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
    0x40, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, // A - O
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, // P - Z
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f,
    0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf,
    0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
    0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf,
    0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
    0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff };
  assert (str != NULL);
  i = 0;
  while (ch = str[i], ch != '\0')
  {
    str[i++] = map[(uint8_t)ch];
  }
}

static int ms_close (struct MemorySurfer *ms) {
  int e;
  assert(ms->imf_filename != NULL);
  e = imf_close(&ms->imf);
  if (e == 0) {
    sa_free(&ms->cat_sa);
    assert((ms->cat_a == 0 && ms->cat_t == NULL) || (ms->cat_a > 0 && ms->cat_t != NULL));
    free(ms->cat_t);
    ms->cat_t = NULL;
    ms->cat_a = 0;
    ms->n_first = -1;
  }
  return e;
}

static void print_hex(char *str, uint8_t *data, size_t len)
{
  int i;
  int nibble[2];
  char ch;
  str[len * 2] = '\0';
  while (len--) {
    nibble[1] = data[len] & 0xf;
    nibble[0] = data[len] >> 4;
    i = 2;
    while (i--) {
      ch = nibble[i];
      if (ch >= 10)
        ch += 'a' - 10;
      else
        ch += '0';
      str[len * 2 + i] = ch;
    }
  }
}

static void e2str(int e, char *e_str) {
  int i;
  char ch;
  if (e == 0)
    strcpy(e_str, "0");
  else if (e == 1)
    strcpy(e_str, "1");
  else if (e < 0)
    strcpy(e_str, "< 0");
  else {
    i = 0;
    while (e != 0) {
      ch = e % 27;
      if (ch != 0) {
        ch += '@';
        e_str[i++] = ch;
      }
      e /= 27;
    }
    e_str[i] = '\0';
  }
}

static int gen_err_msg(struct WebMemorySurfer *wms, char *e_str) {
  int e;
  char *err_str;
  size_t len;
  e = 0;
  err_str = strerror(errno);
  len = 4 + strlen(err_str) + 4 + strlen(e_str);
  if (wms->dbg_wp + len + 1 > wms->dbg_n) {
    wms->dbg_n = (wms->dbg_wp + len + 1 + 120) & 0xfffffff8;
    e = wms->dbg_n > INT32_MAX;
    if (e == 0) {
      wms->dbg_lp = realloc(wms->dbg_lp, wms->dbg_n);
      e = wms->dbg_lp == NULL;
    }
  }
  if (e == 0) {
    strcat(wms->dbg_lp, "<br>");
    strcat(wms->dbg_lp, err_str);
    strcat(wms->dbg_lp, "<br>");
    strcat(wms->dbg_lp, e_str);
    wms->dbg_wp += len;
  }
  return e;
}

size_t utf8_char_len(const char *s) {
  size_t len;
  const uint8_t *b;
  b = (const uint8_t *)s;
  if ((*b & 0x80) == 0)
    len = 1;
  else if ((*b & 0xE0) == 0xC0)
    len = 2;
  else if ((*b & 0xF0) == 0xE0)
    len = 3;
  else if ((*b & 0xF8) == 0xF0)
    len = 4;
  else
    len = 0;
  return len;
}

size_t utf8_strcspn(const char *s, const char *reject, size_t *n) {
  int e;
  size_t len;
  int i;
  int j;
  int found;
  e = s == NULL || reject == NULL || n == NULL;
  if (e == 0) {
    *n = 0;
    do {
      i = 0;
      do {
        len = utf8_char_len(reject + i);
        e = len == 0;
        if (e == 0) {
          j = 0;
          found = 1;
          while (s[*n + j] != '\0' && j < len && found == 1) {
            found = s[*n + j] == reject[i + j];
            j++;
          }
          if (reject[i] != '\0')
            i += len;
          else
            found = -1;
        }
      } while (found == 0 && e == 0);
      if (found <= 0)
        ++*n;
    } while (found <= 0 && e == 0);
  }
  return e;
}

int main(int argc, char *argv[])
{
  int e; // error
  int saved_e;
  struct WebMemorySurfer *wms;
  char *dbg_filename;
  FILE *dbg_stream;
  int rv; // return value
  size_t size;
  size_t len;
  int cat_i;
  int i;
  int qa_err;
  char *qa_str;
  int search_card_i;
  char *lwr_search_txt; // lower case
  struct stat file_stat;
  int mtime_test;
  int act_i; // action index
  enum Action act_c; // current
  int32_t data_size;
  size_t n;
  DIR *dirp;
  char *fl_pn; // filelist pathname
  size_t fl_pn_alloc_size;
  size_t fl_pn_size;
  struct dirent *dirent;
  char *ext_str;
  int card_i;
  time_t time_diff;
  double retention;
  char *str;
  struct Sha1Context sha1;
  uint8_t message_digest[SHA1_HASH_SIZE];
  uint32_t mod_time;
  int cat_a;
  int16_t n_parent;
  int16_t n_prev;
  struct Card card;
  int32_t index;
  struct Card *mov_card_l;
  int mov_card_a;
  void *dest;
  void *src;
  struct Category *cat_ptr;
  struct Card *card_ptr;
  char e_str[8]; // JTLWQNE + '\0' (0x7fffffff)
  char msg[64];
  dbg_stream = NULL;
  size = sizeof(struct WebMemorySurfer);
  wms = malloc(size);
  e = wms == NULL;
  if (e == 0) {
    e = wms_init(wms);
    if (e == 0) {
      wms->sw_i = sw_start("main", &wms->ms.imf.sw);
      e = wms->sw_i != 0;
      if (e == 0) {
        size = strlen(DATA_PATH) + 10 + 1;
        dbg_filename = malloc(size);
        e = dbg_filename == NULL;
        if (e == 0) {
          rv = snprintf(dbg_filename, size, "%s%s", DATA_PATH, "/debug.csv");
          e = rv < 0 || rv >= size;
          if (e == 0) {
            dbg_stream = fopen(dbg_filename, "a");
            if (dbg_stream == NULL) {
              rv = fprintf(stderr, "can't open \"%s\"\n", dbg_filename);
              e = rv < 0;
            }
          }
          free(dbg_filename);
        }
        else
          e = 0x00344f24; // WMDLF Web(MemorySurfer) malloc debug log file (failed)
      }
      else
        e = 0x00020585; // WSSF Web(MemorySurfer) starting stopwatch failed
      if (e == 0) {
        e = parse_post(wms);
        if (e == 1)
          e = 0x00014618; // WMFD Web(MemorySurfer) malformed form data
      }
      if (e == 0) {
        mtime_test = -1;
        act_i = 0;
        assert(wms->seq <= S_END);
        while ((act_c = action_seq[wms->seq][act_i++]) != A_END && e == 0)
        {
          switch (act_c)
          {
          case A_END:
            assert (0);
            break;
          case A_NONE:
            wms->page = P_START;
            break;
          case A_FILE:
            wms->page = P_FILE;
            break;
          case A_WARNING:
            wms->static_msg = "Warning: Before importing, the content of the current file is erased (and rebuild during the import).";
            wms->static_btn_main = "Erase";
            wms->static_btn_alt = "Cancel";
            wms->todo_main = S_UPLOAD;
            wms->todo_alt = S_FILE;
            wms->page = P_MSG;
            break;
          case A_CREATE:
            e = ms_create(&wms->ms, O_EXCL);
            if (e != 0) {
              assert(wms->file_title_str != NULL);
              free(wms->file_title_str);
              wms->file_title_str = NULL;
              wms->static_msg = strerror(e);
              wms->static_btn_main = "OK";
              wms->todo_main = S_NONE;
              wms->page = P_MSG;
            }
            break;
          case A_NEW:
            wms->page = P_NEW;
            break;
          case A_OPEN_DLG:
            wms->page = P_OPEN;
            break;
          case A_FILELIST:
            dirp = opendir(DATA_PATH);
            e = dirp == NULL;
            if (e == 0) {
              fl_pn = NULL;
              fl_pn_alloc_size = 0;
              do {
                dirent = readdir(dirp);
                if (dirent != NULL) {
                  fl_pn_size = strlen(DATA_PATH) + strlen(dirent->d_name) + 2;
                  if (fl_pn_size > fl_pn_alloc_size) {
                    fl_pn = realloc(fl_pn, fl_pn_size);
                    e = fl_pn == NULL;
                    if (e == 0)
                      fl_pn_alloc_size = fl_pn_size;
                  }
                  if (e == 0) {
                    strcpy(fl_pn, DATA_PATH);
                    strcat(fl_pn, "/");
                    strcat(fl_pn, dirent->d_name);
                    e = stat(fl_pn, &file_stat);
                    if (e == 0) {
                      if (file_stat.st_mode & S_IFREG) {
                        ext_str = rindex(fl_pn, '.');
                        if (ext_str != NULL && strcmp(ext_str, ".imsf") == 0) {
                          size = sizeof(char*) * (wms->fl_c + 1);
                          wms->fl_v = realloc(wms->fl_v, size);
                          e = wms->fl_v == NULL;
                          if (e == 0) {
                            size = strlen(dirent->d_name) + 1;
                            wms->fl_v[wms->fl_c] = malloc(size);
                            e = wms->fl_v[wms->fl_c] == NULL;
                            if (e == 0) {
                              strcpy(wms->fl_v[wms->fl_c], dirent->d_name);
                              wms->fl_c++;
                            }
                          }
                        }
                      }
                    }
                  }
                }
              } while (dirent != NULL && e == 0);
              free(fl_pn);
              e = closedir(dirp);
            }
            break;
          case A_OPEN:
            if (wms->ms.imf_filename != NULL) {
              e = ms_open(&wms->ms);
              if (e != 0) {
                free(wms->file_title_str);
                wms->file_title_str = NULL;
                e2str(e, e_str);
                gen_err_msg(wms, e_str);
                wms->static_msg = wms->dbg_lp;
                wms->static_btn_main = "OK"; // "Retry"
                wms->todo_main = S_NONE; // S_SCHEDULE S_SELECT_CREATE_CAT S_GO_LOGIN S_EDIT S_DELETE_CARD S_INSERT S_APPEND S_PREVIOUS S_NEXT S_SELECT_SEARCH_CAT
                wms->page = P_MSG;
              }
            }
            else
              e = wms->seq != S_FILE && wms->seq != S_ABOUT;
            break;
          case A_CHANGE_PASSWD:
            free(wms->ms.password);
            assert(wms->ms.new_password != NULL);
            wms->ms.password = wms->ms.new_password;
            wms->ms.new_password = NULL;
            break;
          case A_WRITE_PASSWD:
            e = sha1_reset(&sha1);
            if (e == 0) {
              assert(wms->ms.password != NULL);
              len = strlen(wms->ms.password);
              e = sha1_input(&sha1, (uint8_t*) wms->ms.password, len);
              if (e == 0) {
                e = sha1_result(&sha1, wms->ms.passwd.pw_msg_digest);
                if (e == 0) {
                  wms->ms.passwd.pw_flag = 1;
                  size = sizeof(struct Password);
                  e = imf_put(&wms->ms.imf, PW_INDEX, &wms->ms.passwd, size);
                  if (e == 0)
                    e = imf_sync (&wms->ms.imf);
                }
              }
            }
            break;
          case A_READ_PASSWD:
            if (wms->ms.imf_filename != NULL) {
              assert(wms->ms.passwd.pw_flag == -1);
              data_size = imf_get_size (&wms->ms.imf, PW_INDEX);
              e = data_size != sizeof(struct Password);
              if (e == 0)
                e = imf_get (&wms->ms.imf, PW_INDEX, &wms->ms.passwd);
              if (e != 0) {
                wms->static_msg = "Read of password hash failed";
                wms->static_btn_main = "OK";
                wms->todo_main = S_GO_LOGIN;
                wms->page = P_MSG;
              }
            }
            else
              e = wms->seq != S_FILE && wms->seq != S_ABOUT;
            break;
          case A_CHECK_PASSWORD:
            e = wms->ms.passwd.pw_flag != 0;
            if (e != 0) {
              wms->static_msg = "A password is already set";
              wms->static_btn_main = "OK";
              wms->todo_main = S_GO_LOGIN;
              wms->page = P_MSG;
            }
            break;
          case A_AUTH_PASSWD:
            assert(wms->ms.passwd.pw_flag > 0);
            e = sha1_reset(&sha1);
            if (e == 0) {
              assert(wms->ms.password != NULL);
              len = strlen(wms->ms.password);
              e = sha1_input(&sha1, (uint8_t*) wms->ms.password, len);
              if (e == 0) {
                e = sha1_result(&sha1, message_digest);
                if (e == 0)
                  e = memcmp(wms->ms.passwd.pw_msg_digest, message_digest, SHA1_HASH_SIZE);
              }
            }
            if (e != 0) {
              sleep (1);
              wms->static_msg = "Invalid password";
              wms->static_btn_main = "OK";
              wms->todo_main = S_GO_LOGIN;
              wms->page = P_MSG;
            }
            break;
          case A_AUTH_TOK:
            if (wms->ms.imf_filename != NULL) {
              assert(wms->ms.passwd.pw_flag > 0);
              assert(wms->ms.passwd.tok_sec > 0 && wms->ms.passwd.tok_count > 0);
              mod_time = wms->ms.timestamp / wms->ms.passwd.tok_sec * wms->ms.passwd.tok_sec;
              for (i = 0; i < wms->ms.passwd.tok_count; i++) {
                e = sha1_reset(&sha1);
                assert(e == 0);
                e = sha1_input(&sha1, (uint8_t*) wms->ms.passwd.pw_msg_digest, SHA1_HASH_SIZE);
                if (e == 0) {
                  e = sha1_input(&sha1, (uint8_t*) &mod_time, sizeof(uint32_t));
                  if (e == 0) {
                    e = sha1_result(&sha1, message_digest);
                    if (e == 0) {
                      rv = memcmp(wms->tok_digest, message_digest, SHA1_HASH_SIZE);
                      e = rv != 0;
                      if (e == 0)
                        break;
                    }
                  }
                }
                mod_time -= wms->ms.passwd.tok_sec;
              }
              if (e != 0) {
                sleep (1);
                wms->static_msg = "Invalid session token";
                wms->static_btn_main = "OK";
                wms->todo_main = S_GO_LOGIN;
                wms->page = P_MSG;
              }
            }
            else
              e = wms->seq != S_FILE && wms->seq != S_ABOUT;
            break;
          case A_GEN_TOK:
            if (wms->ms.imf_filename != NULL) {
              assert(wms->ms.passwd.pw_flag > 0);
              e = sha1_reset(&sha1);
              if (e == 0) {
                e = sha1_input(&sha1, (uint8_t*) wms->ms.passwd.pw_msg_digest, SHA1_HASH_SIZE);
                if (e == 0) {
                  assert(wms->ms.passwd.tok_sec > 0);
                  mod_time = wms->ms.timestamp / wms->ms.passwd.tok_sec * wms->ms.passwd.tok_sec;
                  e = sha1_input(&sha1, (uint8_t*) &mod_time, sizeof(uint32_t));
                  if (e == 0) {
                    e = sha1_result(&sha1, message_digest);
                    if (e == 0)
                      print_hex(wms->tok_str, message_digest, 20);
                  }
                }
              }
            }
            else
              e = wms->seq != S_FILE && wms->seq != S_ABOUT;
            break;
          case A_RETRIEVE_MTIME:
            e = mtime_test != -1;
            if (e == 0) {
              mtime_test = 1;
              if (wms->mtime[0] != 0 || wms->mtime[1] != 0) {
                mtime_test = 0;
                if (wms->mtime[0] >= 0 && wms->mtime[1] >= 0) {
                  if (wms->ms.imf_filename != NULL) {
                    memset(&file_stat, 0, sizeof(struct stat));
                    e = stat(wms->ms.imf_filename, &file_stat);
                    if (e == 0) {
                      assert(file_stat.st_mtim.tv_sec >= 0 && file_stat.st_mtim.tv_nsec >= 0);
                      mtime_test = wms->mtime[0] == file_stat.st_mtim.tv_nsec && wms->mtime[1] == file_stat.st_mtim.tv_sec;
                    }
                  }
                }
              }
            }
            else
              e = 0x00e72d74; // WMSMAA (Web)MemorySurfer main assert A (failed)
            break;
          case A_MTIME_TEST:
            e = mtime_test == -1;
            if (e == 0) {
              e = mtime_test == 0;
              if (e != 0) {
                wms->static_msg = "Error: Invalid mtime value";
                wms->static_btn_main = "OK";
                wms->todo_main = S_SELECT_EDIT_CAT; // S_EDIT S_QUESTION
                wms->page = P_MSG;
              }
            }
            else
              e = 0x01c21fdf; // WMSMAB (Web)MemorySurfer main assert B (failed)
            break;
          case A_CARD_TEST:
            e = wms->ms.card_a <= 0 ? 0x19b4a393 : 0; // WMSMACA (Web)MemorySurfer main assert Card (test) (failed)
            if (e == 0) {
              e = wms->ms.card_i < 0 || wms->ms.card_i >= wms->ms.card_a;
              if (e != 0) {
                wms->static_msg = "Warning: Invalid card";
                wms->static_btn_main = "OK";
                wms->todo_main = S_NONE;
                wms->page = P_MSG;
              }
            }
            break;
          case A_TEST_CAT_SELECTED:
            e = wms->ms.cat_i < 0;
            if (e == 1) {
              wms->static_msg = "Please select a category to learn";
              wms->static_btn_main = "OK";
              wms->todo_main = S_SELECT_LEARN_CAT;
              wms->page = P_MSG;
            }
            break;
          case A_TEST_CAT_VALID:
            e = wms->ms.cat_i >= wms->ms.cat_a || wms->ms.cat_t[wms->ms.cat_i].cat_used == 0;
            if (e == 1) {
              wms->ms.cat_i = -1;
              wms->static_msg = "Warning: Invalid category";
              wms->static_btn_main = "OK";
              wms->todo_main = S_SELECT_LEARN_CAT;
              wms->page = P_MSG;
            }
            break;
          case A_TEST_CAT:
            e = wms->ms.cat_i < 0 || wms->ms.cat_i >= wms->ms.cat_a || wms->ms.cat_t[wms->ms.cat_i].cat_used == 0;
            if (e != 0 && wms->seq == S_CREATE_CAT)
              e = wms->ms.cat_i != -1 || wms->ms.n_first != -1;
            if (e != 0) {
              wms->static_msg = "Warning: Invalid category";
              wms->static_btn_main = "OK";
              wms->todo_main = S_START_CAT;
              wms->page = P_MSG;
              if (wms->from_page == P_SELECT_CREATE_CAT) {
                wms->static_msg = "Please select a category were to arrange the new category to";
                wms->todo_main = S_SELECT_CREATE_CAT;
              }
              else if (wms->from_page == P_SELECT_CAT) {
                if (wms->seq == S_SELECT_DEST_CAT) {
                  wms->static_msg = "Please select a category to move";
                  wms->todo_main = S_SELECT_MOVE_CAT;
                }
                else {
                  assert(wms->seq == S_ASK_DELETE_CAT);
                  wms->static_msg = "Please select a category to delete";
                  wms->todo_main = S_SELECT_DELETE_CAT;
                }
              }
            }
            break;
          case A_SLASH:
            e = wms->file_title_str == NULL;
            if (e == 0) {
              str = strchr(wms->file_title_str, '/');
              e = str != NULL;
              if (e != 0) {
                free(wms->file_title_str);
                wms->file_title_str = NULL;
                wms->static_msg = "Error: Slash ('/')";
                wms->static_btn_main = "OK";
                wms->todo_main = S_FILE;
                wms->page = P_MSG;
              }
            }
            else
              e = 0x029d124a; // WMSMAC (Web)MemorySurfer main assert C (failed)
            break;
          case A_VOID:
            assert(wms->file_title_str != NULL);
            len = strlen(wms->file_title_str);
            e = len == 0;
            if (e != 0) {
              free(wms->file_title_str);
              wms->file_title_str = NULL;
              wms->static_msg = "Error: No filename";
              wms->static_btn_main = "OK";
              wms->todo_main = S_FILE;
              wms->page = P_MSG;
            }
            break;
          case A_FILE_EXTENSION:
            assert(wms->file_title_str != NULL);
            ext_str = rindex(wms->file_title_str, '.');
            if (ext_str == NULL || strcmp(ext_str, ".imsf") != 0) {
              size = strlen(wms->file_title_str) + 6; // + ".imsf" + '\0'
              str = malloc(size);
              e = str == NULL;
              if (e == 0) {
                strcpy(str, wms->file_title_str);
                strcat(str, ".imsf");
                free(wms->file_title_str);
                wms->file_title_str = str;
              }
            }
            break;
          case A_GATHER:
            if (wms->file_title_str != NULL) {
              size = strlen(DATA_PATH) + strlen(wms->file_title_str) + 2; // + '/' + '\0'
              str = malloc(size);
              e = str == NULL;
              if (e == 0) {
                strcpy(str, DATA_PATH);
                strcat(str, "/");
                strcat(str, wms->file_title_str);
                assert(wms->ms.imf_filename == NULL);
                wms->ms.imf_filename = str;
              }
            }
            else
              e = wms->seq != S_FILE && wms->seq != S_ABOUT;
            break;
          case A_UPLOAD:
            e = ms_close (&wms->ms);
            if (e == 0)
            {
              e = ms_create (&wms->ms, O_TRUNC);
              if (e == 0)
              {
                wms->page = P_UPLOAD;
              }
            }
            break;
          case A_UPLOAD_REPORT:
            data_size = sa_length(&wms->ms.cat_sa);
            e = imf_put(&wms->ms.imf, SA_INDEX, wms->ms.cat_sa.sa_d, data_size);
            if (e == 0) {
              data_size = sizeof(struct Category) * wms->ms.cat_a;
              e = imf_put(&wms->ms.imf, C_INDEX, wms->ms.cat_t, data_size);
              if (e == 0) {
                e = imf_sync(&wms->ms.imf);
                if (e == 0)
                  wms->page = P_UPLOAD_REPORT;
              }
            }
            break;
          case A_EXPORT:
            wms->page = P_EXPORT;
            break;
          case A_LOAD_CARDLIST:
            e = wms->ms.cat_i < 0;
            if (e == 0) {
              assert(wms->ms.cat_i >= 0);
              e = wms->ms.cat_i >= wms->ms.cat_a || wms->ms.cat_t[wms->ms.cat_i].cat_used == 0;
              if (e == 0) {
                assert(wms->ms.cat_i < wms->ms.cat_a && wms->ms.cat_t[wms->ms.cat_i].cat_used != 0);
                e = ms_load_card_list(&wms->ms);
              }
              else {
                wms->ms.cat_i = -1;
                wms->static_msg = "Warning: Invalid category";
                wms->static_btn_main = "OK";
                wms->todo_main = S_NONE; // S_SELECT_EDIT_CAT S_SELECT_SEARCH_CAT S_SELECT_LEARN_CAT S_DELETE_CARD S_APPEND
                wms->page = P_MSG;
              }
            }
            else {
              assert (wms->ms.cat_i == -1);
              wms->static_msg = "Please select a category";
              wms->static_btn_main = "OK";
              wms->todo_main = S_START; // S_NONE S_SELECT_EDIT_CAT S_SELECT_SEARCH_CAT S_SELECT_LEARN_CAT S_DELETE_CARD
              wms->page = P_MSG;
            }
            break;
          case A_CHECK_RESUME:
            if (wms->ms.card_a > 0)
              for (card_i = 0; card_i < wms->ms.card_a && wms->ms.can_resume == 0; card_i++)
                if (wms->ms.card_l[card_i].card_state == STATE_SUSPENDED)
                  wms->ms.can_resume = 1;
            break;
          case A_ASK_REMOVE:
            wms->static_msg = "Remove file of the file system?";
            wms->static_btn_main = "Remove";
            wms->static_btn_alt = "Cancel";
            wms->todo_main = S_REMOVE;
            wms->todo_alt = S_FILE;
            wms->page = P_MSG;
            break;
          case A_REMOVE:
            e = ms_close(&wms->ms);
            if (e == 0) {
              e = unlink(wms->ms.imf_filename);
              if (e == 0) {
                assert(wms->ms.imf_filename != NULL);
                free(wms->ms.imf_filename);
                wms->ms.imf_filename = NULL;
                wms->tok_str[0] = '\0';
              }
            }
            break;
          case A_ASK_ERASE:
            wms->static_msg = "Erase all decks & cards?";
            wms->static_btn_main = "Erase";
            wms->static_btn_alt = "Cancel";
            wms->todo_main = S_ERASE;
            wms->todo_alt = S_FILE;
            wms->page = P_MSG;
            break;
          case A_ERASE:
            e = ms_close(&wms->ms);
            if (e == 0) {
              e = ms_create(&wms->ms, O_TRUNC);
              if (e == 0) {
                wms->ms.cat_i = -1;
                wms->page = P_FILE;
              }
            }
            break;
          case A_CLOSE:
            free(wms->file_title_str);
            wms->file_title_str = NULL;
            wms->ms.cat_i = -1;
            wms->page = P_FILE;
            break;
          case A_START_CAT:
            wms->page = P_START_CAT;
            break;
          case A_SELECT_CREATE_CAT:
            if (wms->ms.n_first >= 0)
              wms->page = P_SELECT_CREATE_CAT;
            else
              wms->page = P_CAT_NAME;
            break;
          case A_SELECT_CAT:
            wms->page = P_SELECT_CAT;
            break;
          case A_SELECT_SEND_CAT:
            wms->page = P_SELECT_SEND_CAT;
            break;
          case A_SELECT_ARRANGE:
            wms->page = P_SELECT_ARRANGE;
            break;
          case A_CAT_NAME:
            wms->page = P_CAT_NAME;
            break;
          case A_CREATE_CAT:
            e = wms->ms.cat_name == NULL;
            if (e == 0) {
              len = strlen(wms->ms.cat_name);
              e = len == 0;
            }
            if (e == 0) {
              if (wms->ms.cat_i >= 0)
                e = wms->ms.cat_i >= wms->ms.cat_a || wms->ms.cat_t[wms->ms.cat_i].cat_used == 0;
              else {
                e = wms->ms.cat_i != -1 || wms->ms.n_first != -1;
                if (e == 0)
                  wms->ms.arrange = -1;
              }
              if (e == 0) {
                cat_i = 0;
                while (cat_i < wms->ms.cat_a && wms->ms.cat_t[cat_i].cat_used != 0)
                  cat_i++;
                if (cat_i == wms->ms.cat_a) {
                  cat_a = wms->ms.cat_a + 7;
                  e = cat_a > INT16_MAX;
                  if (e == 0) {
                    size = sizeof(struct Category) * cat_a;
                    wms->ms.cat_t = realloc(wms->ms.cat_t, size);
                    e = wms->ms.cat_t == NULL;
                    if (e == 0) {
                      for (i = wms->ms.cat_a; i < cat_a; i++)
                        wms->ms.cat_t[i].cat_used = 0;
                      wms->ms.cat_a = cat_a;
                    }
                  }
                }
                if (e == 0) {
                  assert(cat_i < wms->ms.cat_a && wms->ms.cat_t[cat_i].cat_used == 0);
                  e = imf_seek_unused(&wms->ms.imf, &index);
                  if (e == 0) {
                    e = imf_put(&wms->ms.imf, index, "", 0);
                    if (e == 0) {
                      wms->ms.cat_t[cat_i].cat_cli = index;
                      e = sa_set(&wms->ms.cat_sa, cat_i, wms->ms.cat_name);
                      if (e == 0) {
                        wms->ms.cat_t[cat_i].cat_x = 1;
                        switch (wms->ms.arrange)
                        {
                        case 0: // Before
                          n_prev = -1;
                          n_parent = -1;
                          for (i = 0; i < wms->ms.cat_a && n_prev == -1 && n_parent == -1; i++)
                            if (wms->ms.cat_t[i].cat_used != 0) {
                              if (wms->ms.cat_t[i].cat_n_sibling == wms->ms.cat_i)
                                n_prev = i;
                              if (wms->ms.cat_t[i].cat_n_child == wms->ms.cat_i)
                                n_parent = i;
                            }
                          if (n_prev != -1) {
                            assert(wms->ms.cat_t[n_prev].cat_n_sibling == wms->ms.cat_i);
                            wms->ms.cat_t[n_prev].cat_n_sibling = cat_i;
                          }
                          else {
                            if (wms->ms.n_first == wms->ms.cat_i)
                              wms->ms.n_first = cat_i;
                            else {
                              assert(n_parent != -1);
                              wms->ms.cat_t[n_parent].cat_n_child = cat_i;
                            }
                          }
                          wms->ms.cat_t[cat_i].cat_n_sibling = wms->ms.cat_i;
                          wms->ms.cat_t[cat_i].cat_n_child = -1;
                          break;
                        case 1: // Below
                          wms->ms.cat_t[cat_i].cat_n_sibling = wms->ms.cat_t[wms->ms.cat_i].cat_n_child;
                          wms->ms.cat_t[wms->ms.cat_i].cat_n_child = cat_i;
                          wms->ms.cat_t[cat_i].cat_n_child = -1;
                          break;
                        case 2: // Behind
                          wms->ms.cat_t[cat_i].cat_n_sibling = wms->ms.cat_t[wms->ms.cat_i].cat_n_sibling;
                          wms->ms.cat_t[wms->ms.cat_i].cat_n_sibling = cat_i;
                          wms->ms.cat_t[cat_i].cat_n_child = -1;
                          break;
                        case -1:
                          e = wms->ms.n_first != -1;
                          if (e == 0) {
                            wms->ms.cat_t[cat_i].cat_n_sibling = -1;
                            wms->ms.cat_t[cat_i].cat_n_child = -1;
                            assert(cat_i == 0);
                            wms->ms.n_first = cat_i;
                          }
                          break;
                        default:
                          e = -1;
                        }
                        if (e == 0) {
                          wms->ms.cat_t[cat_i].cat_used = 1;
                          wms->ms.cat_t[cat_i].cat_on = 1;
                          data_size = sa_length(&wms->ms.cat_sa);
                          e = imf_put(&wms->ms.imf, SA_INDEX, wms->ms.cat_sa.sa_d, data_size);
                          if (e == 0) {
                            data_size = sizeof(struct Category) * wms->ms.cat_a;
                            e = imf_put(&wms->ms.imf, C_INDEX, wms->ms.cat_t, data_size);
                            if (e == 0) {
                              e = imf_sync(&wms->ms.imf);
                              if (e == 0) {
                                wms->ms.cat_i = cat_i;
                                wms->page = P_START_CAT;
                              }
                            }
                          }
                        }
                        else {
                          wms->static_msg = "Please select how to arrange the new category";
                          wms->static_btn_main = "OK";
                          wms->todo_main = S_SELECT_ARRANGE;
                          wms->page = P_MSG;
                        }
                      }
                    }
                  }
                }
              }
            }
            else {
              wms->static_msg = "Please enter a name for the new category";
              wms->static_btn_main = "OK";
              wms->todo_main = S_SELECT_CREATE_CAT;
              wms->page = P_MSG;
            }
            break;
          case A_RENAME_CAT:
            assert(wms->ms.cat_i >= 0 && wms->ms.cat_i < wms->ms.cat_a && wms->ms.cat_t[wms->ms.cat_i].cat_used != 0);
            e = sa_set (&wms->ms.cat_sa, wms->ms.cat_i, wms->ms.cat_name);
            if (e == 0) {
              data_size = sa_length(&wms->ms.cat_sa);
              e = imf_put(&wms->ms.imf, SA_INDEX, wms->ms.cat_sa.sa_d, data_size);
              if (e == 0) {
                e = imf_sync(&wms->ms.imf);
                if (e == 0)
                  wms->page = P_START_CAT;
              }
            }
            break;
          case A_ASK_DELETE_CAT:
            assert(wms->ms.cat_t[wms->ms.cat_i].cat_used != 0);
            if (wms->ms.cat_t[wms->ms.cat_i].cat_n_child == -1) {
              wms->static_msg = "Delete Category?";
              wms->static_btn_main = "Delete";
              wms->static_btn_alt = "Cancel";
              wms->todo_main = S_DELETE_CAT;
              wms->todo_alt = S_START_CAT;
              wms->page = P_MSG;
            }
            else {
              wms->static_msg = "A category to delete must be a leaf";
              wms->static_btn_main = "OK";
              wms->todo_main = S_SELECT_DELETE_CAT;
              wms->page = P_MSG;
            }
            break;
          case A_DELETE_CAT:
            assert(wms->ms.cat_t[wms->ms.cat_i].cat_used != 0);
            n_prev = -1;
            n_parent = -1;
            i = 0;
            do {
              if (wms->ms.cat_t[i].cat_used != 0) {
                if (wms->ms.cat_t[i].cat_n_sibling == wms->ms.cat_i)
                  n_prev = i;
                if (wms->ms.cat_t[i].cat_n_child == wms->ms.cat_i)
                  n_parent = i;
              }
              i++;
            } while (i < wms->ms.cat_a && n_prev == -1 && n_parent == -1);
            if (n_prev != -1)
              wms->ms.cat_t[n_prev].cat_n_sibling = wms->ms.cat_t[wms->ms.cat_i].cat_n_sibling;
            else if (n_parent != -1)
              wms->ms.cat_t[n_parent].cat_n_child = wms->ms.cat_t[wms->ms.cat_i].cat_n_sibling;
            else {
              e = wms->ms.n_first != wms->ms.cat_i;
              if (e == 0)
                wms->ms.n_first = wms->ms.cat_t[wms->ms.cat_i].cat_n_sibling;
            }
            if (e == 0) {
              e = ms_load_card_list(&wms->ms);
              if (e == 0) {
                card_i = 0;
                while (card_i < wms->ms.card_a && e == 0) {
                  e = imf_delete (&wms->ms.imf, wms->ms.card_l[card_i].card_qai);
                  card_i++;
                }
                if (e == 0)
                  e = imf_delete (&wms->ms.imf, wms->ms.cat_t[wms->ms.cat_i].cat_cli);
              }
            }
            if (e == 0) {
              wms->ms.cat_t[wms->ms.cat_i].cat_used = 0;
              data_size = sizeof(struct Category) * wms->ms.cat_a;
              e = imf_put(&wms->ms.imf, C_INDEX, wms->ms.cat_t, data_size);
              if (e == 0) {
                e = imf_sync(&wms->ms.imf);
                if (e == 0) {
                  wms->ms.cat_i = -1;
                  wms->page = P_START_CAT;
                }
              }
            }
            break;
          case A_TOGGLE:
            assert(wms->ms.cat_i >= 0 && wms->ms.cat_i < wms->ms.cat_a && wms->ms.cat_t[wms->ms.cat_i].cat_used != 0);
            wms->ms.cat_t[wms->ms.cat_i].cat_x = wms->ms.cat_t[wms->ms.cat_i].cat_x == 0 ? 1 : 0;
            data_size = sizeof(struct Category) * wms->ms.cat_a;
            e = imf_put(&wms->ms.imf, C_INDEX, wms->ms.cat_t, data_size);
            if (e == 0) {
              e = imf_sync(&wms->ms.imf);
              if (e == 0)
                wms->page = P_START_CAT;
            }
            break;
          case A_MOVE_CAT:
            assert (wms->ms.cat_t[wms->ms.cat_i].cat_used != 0 && wms->ms.cat_t[wms->ms.mov_cat_i].cat_used != 0);
            n_prev = -1;
            n_parent = wms->ms.cat_i;
            do {
              if (n_prev != -1)
                cat_i = n_prev;
              if (n_parent != -1)
                cat_i = n_parent;
              e = wms->ms.mov_cat_i == n_parent;
              if (e != 0)
                break;
              n_prev = -1;
              n_parent = -1;
              for (i = 0; i < wms->ms.cat_a && n_parent == -1 && n_prev == -1; i++)
                if (wms->ms.cat_t[i].cat_used != 0) {
                  if (wms->ms.cat_t[i].cat_n_sibling == cat_i)
                    n_prev = i;
                  if (wms->ms.cat_t[i].cat_n_child == cat_i)
                    n_parent = i;
                }
            } while (n_parent != -1 || n_prev != -1);
            if (e == 0) {
              if (wms->ms.arrange != 0 || wms->ms.cat_t[wms->ms.mov_cat_i].cat_n_sibling != wms->ms.cat_i) {
                if (wms->ms.n_first != wms->ms.mov_cat_i) {
                  n_prev = -1;
                  n_parent = -1;
                  for (i = 0; i < wms->ms.cat_a && n_prev == -1 && n_parent == -1; i++)
                    if (wms->ms.cat_t[i].cat_used != 0) {
                      if (wms->ms.cat_t[i].cat_n_sibling == wms->ms.mov_cat_i)
                        n_prev = i;
                      if (wms->ms.cat_t[i].cat_n_child == wms->ms.mov_cat_i)
                        n_parent = i;
                    }
                  if (n_prev != -1)
                    wms->ms.cat_t[n_prev].cat_n_sibling = wms->ms.cat_t[wms->ms.mov_cat_i].cat_n_sibling;
                  if (n_parent != -1)
                    wms->ms.cat_t[n_parent].cat_n_child = wms->ms.cat_t[wms->ms.mov_cat_i].cat_n_sibling;
                }
                else
                  wms->ms.n_first = wms->ms.cat_t[wms->ms.mov_cat_i].cat_n_sibling;
                assert (wms->ms.arrange != -1);
                switch (wms->ms.arrange)
                {
                case 0: // Before
                  i = 0;
                  n_prev = -1;
                  n_parent = -1;
                  do {
                    if (wms->ms.cat_t[i].cat_used != 0) {
                      if (wms->ms.cat_t[i].cat_n_sibling == wms->ms.cat_i)
                        n_prev = i;
                      if (wms->ms.cat_t[i].cat_n_child == wms->ms.cat_i)
                        n_parent = i;
                    }
                    i++;
                  } while (i < wms->ms.cat_a && n_prev == -1 && n_parent == -1);
                  if (n_prev != -1) {
                    assert(wms->ms.cat_t[n_prev].cat_n_sibling == wms->ms.cat_i);
                    wms->ms.cat_t[n_prev].cat_n_sibling = wms->ms.mov_cat_i;
                  }
                  else {
                    if (wms->ms.n_first == wms->ms.cat_i)
                      wms->ms.n_first = wms->ms.mov_cat_i;
                    else {
                      assert(n_parent != -1);
                      wms->ms.cat_t[n_parent].cat_n_child = wms->ms.mov_cat_i;
                    }
                  }
                  wms->ms.cat_t[wms->ms.mov_cat_i].cat_n_sibling = wms->ms.cat_i;
                  break;
                case 1: // Below
                  wms->ms.cat_t[wms->ms.mov_cat_i].cat_n_sibling = wms->ms.cat_t[wms->ms.cat_i].cat_n_child;
                  wms->ms.cat_t[wms->ms.cat_i].cat_n_child = wms->ms.mov_cat_i;
                  break;
                case 2: // Behind
                  wms->ms.cat_t[wms->ms.mov_cat_i].cat_n_sibling = wms->ms.cat_t[wms->ms.cat_i].cat_n_sibling;
                  wms->ms.cat_t[wms->ms.cat_i].cat_n_sibling = wms->ms.mov_cat_i;
                  break;
                }
                data_size = sizeof (struct Category) * wms->ms.cat_a;
                e = imf_put (&wms->ms.imf, C_INDEX, wms->ms.cat_t, data_size);
                if (e == 0) {
                  e = imf_sync (&wms->ms.imf);
                  if (e == 0) {
                    wms->ms.cat_i = wms->ms.mov_cat_i;
                    wms->page = P_START_CAT;
                  }
                }
              }
            }
            else {
              wms->static_msg = "Invalid topology";
              wms->static_btn_main = "OK";
              wms->todo_main = S_START_CAT;
              wms->page = P_MSG;
            }
            break;
          case A_SELECT_EDIT_CAT:
            wms->ms.card_i = -1;
            wms->page = P_SELECT_EDIT_CAT;
            break;
          case A_EDIT:
            if (wms->ms.cat_i >= 0)
            {
              if ((wms->ms.cat_i < wms->ms.cat_a) && (wms->ms.cat_t[wms->ms.cat_i].cat_used != 0))
              {
                e = ms_load_card_list (&wms->ms);
                if (e == 0)
                {
                  assert (wms->ms.card_i >= -1 && wms->ms.card_a >= 0);
                  if (wms->ms.card_i < 0 && wms->ms.card_a > 0)
                  {
                    wms->ms.card_i = 0;
                  }
                  if (wms->ms.card_i >= wms->ms.card_a)
                  {
                    wms->ms.card_i = wms->ms.card_a - 1;
                  }
                  if (wms->ms.card_i >= 0)
                  {
                    e = ms_get_card_sa (&wms->ms);
                  }
                  wms->page = P_EDIT;
                }
              }
              else
              {
                wms->ms.cat_i = -1;
                wms->static_msg = "Warning: Invalid category";
                wms->static_btn_main = "OK";
                wms->todo_main = S_SELECT_EDIT_CAT;
                wms->page = P_MSG;
              }
            }
            else
            {
              wms->static_msg = "Please select a category to edit";
              wms->static_btn_main = "OK";
              wms->todo_main = S_SELECT_EDIT_CAT;
              wms->page = P_MSG;
            }
            break;
          case A_SYNC_QA:
            qa_err = sa_get(&wms->qa_sa, 0) == NULL;
            qa_err |= sa_get(&wms->qa_sa, 1) == NULL;
            if (qa_err == 0) {
              e = ms_get_card_sa(&wms->ms);
              if (e == 0) {
                e = ms_modify_qa(&wms->qa_sa, &wms->ms);
                if (e > 0) {
                  e = mtime_test == 0;
                  if (e == 0)
                    e = imf_sync(&wms->ms.imf);
                  else {
                    wms->static_msg = "Error: Invalid mtime value";
                    wms->static_btn_main = "OK";
                    wms->todo_main = S_SELECT_EDIT_CAT; // S_EDIT S_QUESTION
                    wms->page = P_MSG;
                  }
                }
              }
            }
            break;
          case A_INSERT:
            e = sa_set(&wms->ms.card_sa, 0, "");
            if (e == 0) {
              e = sa_set(&wms->ms.card_sa, 1, "");
              if (e == 0) {
                e = imf_seek_unused(&wms->ms.imf, &index);
                if (e == 0) {
                  data_size = sa_length(&wms->ms.card_sa);
                  e = imf_put(&wms->ms.imf, index, wms->ms.card_sa.sa_d, data_size);
                  if (e == 0) {
                    wms->ms.card_a++;
                    data_size = wms->ms.card_a * sizeof(struct Card);
                    wms->ms.card_l = realloc(wms->ms.card_l, data_size);
                    e = wms->ms.card_l == NULL;
                    if (e == 0) {
                      src = wms->ms.card_l + wms->ms.card_i;
                      dest = wms->ms.card_l + wms->ms.card_i + 1;
                      n = wms->ms.card_a - wms->ms.card_i - 1;
                      assert(n > 0);
                      size = n * sizeof(struct Card);
                      memmove(dest, src, size);
                      wms->ms.card_l[wms->ms.card_i].card_time = time(NULL);
                      e = wms->ms.card_l[wms->ms.card_i].card_time == -1;
                      if (e == 0) {
                        wms->ms.card_l[wms->ms.card_i].card_strength = 60;
                        wms->ms.card_l[wms->ms.card_i].card_qai = index;
                        wms->ms.card_l[wms->ms.card_i].card_state = STATE_NEW;
                        e = imf_put(&wms->ms.imf, wms->ms.cat_t[wms->ms.cat_i].cat_cli, wms->ms.card_l, data_size);
                        if (e == 0) {
                          e = imf_sync (&wms->ms.imf);
                          if (e == 0)
                            wms->page = P_EDIT;
                        }
                      }
                    }
                  }
                }
              }
            }
            break;
          case A_APPEND:
            e = sa_set(&wms->ms.card_sa, 0, "");
            if (e == 0) {
              e = sa_set(&wms->ms.card_sa, 1, "");
              if (e == 0) {
                e = imf_seek_unused(&wms->ms.imf, &index);
                if (e == 0) {
                  data_size = sa_length(&wms->ms.card_sa);
                  e = imf_put(&wms->ms.imf, index, wms->ms.card_sa.sa_d, data_size);
                  if (e == 0) {
                    wms->ms.card_i = wms->ms.card_a;
                    wms->ms.card_a++;
                    data_size = wms->ms.card_a * sizeof(struct Card);
                    wms->ms.card_l = realloc(wms->ms.card_l, data_size);
                    e = wms->ms.card_l == NULL;
                    if (e == 0) {
                      card_ptr = wms->ms.card_l + wms->ms.card_i;
                      card_ptr->card_time = time (NULL);
                      e = card_ptr->card_time == -1;
                      if (e == 0) {
                        card_ptr->card_strength = 60;
                        card_ptr->card_qai = index;
                        card_ptr->card_state = STATE_NEW;
                        cat_ptr = wms->ms.cat_t + wms->ms.cat_i;
                        e = imf_put(&wms->ms.imf, cat_ptr->cat_cli, wms->ms.card_l, data_size);
                        if (e == 0) {
                          e = imf_sync(&wms->ms.imf);
                          if (e == 0)
                            wms->page = P_EDIT;
                        }
                      }
                    }
                  }
                }
              }
            }
            break;
          case A_ASK_DELETE_CARD:
            wms->static_msg = "Delete Item?";
            wms->static_btn_main = "Delete";
            wms->static_btn_alt = "Cancel";
            wms->todo_main = S_DELETE_CARD;
            wms->todo_alt = S_EDIT;
            wms->page = P_MSG;
            break;
          case A_DELETE_CARD:
            if ((wms->ms.card_a > 0) && (wms->ms.card_i >= 0) && (wms->ms.card_i < wms->ms.card_a))
            {
              card_ptr = wms->ms.card_l + wms->ms.card_i;
              e = imf_delete (&wms->ms.imf, card_ptr->card_qai);
              if (e == 0)
              {
                wms->ms.card_a--;
                dest = wms->ms.card_l + wms->ms.card_i;
                src = wms->ms.card_l + wms->ms.card_i + 1;
                n = wms->ms.card_a - wms->ms.card_i;
                if (n > 0)
                {
                  n *= sizeof(struct Card);
                  memmove (dest, src, n);
                }
                if (wms->ms.card_i == wms->ms.card_a)
                {
                  wms->ms.card_i--;
                }
                data_size = wms->ms.card_a * sizeof (struct Card);
                cat_ptr = wms->ms.cat_t + wms->ms.cat_i;
                e = imf_put (&wms->ms.imf, cat_ptr->cat_cli, wms->ms.card_l, data_size);
                if (e == 0)
                {
                  e = imf_sync (&wms->ms.imf);
                  if (e == 0)
                  {
                    e = ms_get_card_sa (&wms->ms);
                    if (e == 0)
                    {
                      wms->page = P_EDIT;
                    }
                  }
                }
              }
            }
            break;
          case A_PREVIOUS:
            e = ms_load_card_list (&wms->ms);
            {
              qa_err = sa_get(&wms->qa_sa, 0) == NULL;
              qa_err |= sa_get(&wms->qa_sa, 1) == NULL;
              if (qa_err == 0)
              {
                e = ms_get_card_sa (&wms->ms);
                if (e == 0)
                {
                  e = ms_modify_qa(&wms->qa_sa, &wms->ms);
                  if (e > 0)
                  {
                    e = imf_sync (&wms->ms.imf);
                  }
                }
              }
              if (wms->ms.card_a > 0) {
                wms->ms.card_i--;
                if (wms->ms.card_i < 0)
                  wms->ms.card_i = 0;
                if (wms->ms.card_i >= wms->ms.card_a)
                  wms->ms.card_i = wms->ms.card_a - 1;
              }
              else
                wms->ms.card_i = -1;
              if (wms->ms.card_i != -1) {
                e = ms_get_card_sa(&wms->ms);
                if (e == 0)
                  wms->page = P_EDIT;
              }
            }
            break;
          case A_NEXT:
            e = ms_load_card_list (&wms->ms);
            {
              qa_err = sa_get(&wms->qa_sa, 0) == NULL;
              qa_err |= sa_get(&wms->qa_sa, 1) == NULL;
              if (qa_err == 0)
              {
                e = ms_get_card_sa (&wms->ms);
                if (e == 0)
                {
                  e = ms_modify_qa(&wms->qa_sa, &wms->ms);
                  if (e > 0)
                  {
                    e = imf_sync (&wms->ms.imf);
                  }
                }
              }
              if (wms->ms.card_a > 0) {
                wms->ms.card_i++;
                if (wms->ms.card_i < 0)
                  wms->ms.card_i = 0;
                if (wms->ms.card_i >= wms->ms.card_a)
                  wms->ms.card_i = wms->ms.card_a - 1;
              }
              else
                wms->ms.card_i = -1;
              if (wms->ms.card_i != -1) {
                e = ms_get_card_sa(&wms->ms);
                if (e == 0)
                  wms->page = P_EDIT;
              }
            }
            break;
          case A_SCHEDULE:
            e = wms->ms.card_l[wms->ms.card_i].card_state < STATE_NEW;
            if (e == 0) {
              wms->ms.card_l[wms->ms.card_i].card_state = STATE_SCHEDULED;
              data_size = wms->ms.card_a * sizeof(struct Card);
              index = wms->ms.cat_t[wms->ms.cat_i].cat_cli;
              e = imf_put(&wms->ms.imf, index, wms->ms.card_l, data_size);
              if (e == 0) {
                e = imf_sync(&wms->ms.imf);
                if (e == 0) {
                  e = ms_get_card_sa(&wms->ms);
                  if (e == 0)
                    wms->page = P_EDIT;
                }
              }
            }
            break;
          case A_SET:
            wms->ms.mov_card_i = wms->ms.card_i;
            wms->page = P_EDIT;
            break;
          case A_ARRANGE:
            wms->page = P_SELECT_CARD_ARRANGE;
            break;
          case A_MOVE_CARD:
            e = wms->ms.mov_card_i < 0 || wms->ms.mov_card_i >= wms->ms.card_a;
            if (e == 0) {
              size = sizeof(struct Card);
              memcpy(&card, wms->ms.card_l + wms->ms.mov_card_i, size);
              dest = wms->ms.card_l + wms->ms.mov_card_i;
              src = dest + sizeof(struct Card);
              n = wms->ms.card_a - wms->ms.mov_card_i - 1;
              if (n > 0) {
                size = n * sizeof(struct Card);
                memmove(dest, src, size);
              }
              e = wms->ms.card_i < 0 || wms->ms.card_i >= wms->ms.card_a;
              if (e == 0) {
                card_i = wms->ms.card_i;
                if (card_i > wms->ms.mov_card_i)
                  card_i--;
                if (wms->ms.arrange == 2)
                  card_i++;
                else
                  e = wms->ms.arrange != 0;
              }
              if (e == 0) {
                src = wms->ms.card_l + card_i;
                dest = src + sizeof(struct Card);
                n = wms->ms.card_a - card_i - 1;
                if (n > 0) {
                  size = n * sizeof(struct Card);
                  memmove(dest, src, size);
                }
                size = sizeof(struct Card);
                memcpy(wms->ms.card_l + card_i, &card, size);
                data_size = wms->ms.card_a * sizeof(struct Card);
                e = imf_put(&wms->ms.imf, wms->ms.cat_t[wms->ms.cat_i].cat_cli, wms->ms.card_l, data_size);
                if (e == 0) {
                  e = imf_sync(&wms->ms.imf);
                  if (e == 0) {
                    wms->ms.card_i = card_i;
                    e = ms_get_card_sa(&wms->ms);
                    if (e == 0) {
                      wms->ms.mov_card_i = -1;
                      wms->page = P_EDIT;
                    }
                  }
                }
              }
            }
            break;
          case A_SEND_CARD:
            e = wms->ms.mov_cat_i == wms->ms.cat_i;
            if (e == 0) {
              e = wms->ms.mov_cat_i < 0 || wms->ms.mov_cat_i >= wms->ms.cat_a || wms->ms.cat_t[wms->ms.mov_cat_i].cat_used == 0;
              if (e == 0) {
                index = wms->ms.cat_t[wms->ms.mov_cat_i].cat_cli;
                data_size = imf_get_size(&wms->ms.imf, index);
                e = data_size <= 0;
                if (e == 0) {
                  mov_card_a = data_size / sizeof(struct Card);
                  e = wms->ms.mov_card_i < 0 || wms->ms.mov_card_i >= mov_card_a;
                  if (e == 0) {
                    mov_card_l = malloc(data_size);
                    e = mov_card_l == NULL;
                    if (e == 0) {
                      e = imf_get(&wms->ms.imf, index, mov_card_l);
                      if (e == 0) {
                        size = sizeof(struct Card);
                        memcpy(&card, mov_card_l + wms->ms.mov_card_i, size);
                        mov_card_a--;
                        dest = mov_card_l + wms->ms.mov_card_i;
                        src = mov_card_l + wms->ms.mov_card_i + 1;
                        n = mov_card_a - wms->ms.mov_card_i;
                        if (n > 0) {
                          size = n * sizeof(struct Card);
                          memmove(dest, src, size);
                        }
                        data_size = mov_card_a * sizeof(struct Card);
                        e = imf_put(&wms->ms.imf, index, mov_card_l, data_size);
                      }
                      free(mov_card_l);
                    }
                  }
                }
              }
              if (e == 0) {
                wms->ms.card_i = wms->ms.card_a;
                wms->ms.card_a++;
                data_size = wms->ms.card_a * sizeof(struct Card);
                wms->ms.card_l = realloc(wms->ms.card_l, data_size);
                e = wms->ms.card_l == NULL;
                if (e == 0) {
                  src = &card;
                  dest = wms->ms.card_l + wms->ms.card_i;
                  size = sizeof(struct Card);
                  memcpy(dest, src, size);
                  index = wms->ms.cat_t[wms->ms.cat_i].cat_cli;
                  e = imf_put(&wms->ms.imf, index, wms->ms.card_l, data_size);
                }
              }
              if (e == 0)
                e = imf_sync(&wms->ms.imf);
              if (e == 0) {
                e = ms_get_card_sa(&wms->ms);
                if (e == 0)
                  wms->page = P_EDIT;
              }
            }
            else {
              wms->static_msg = "Error: Src == Dest";
              wms->static_btn_main = "OK";
              wms->todo_main = S_EDIT;
              wms->page = P_MSG;
            }
            break;
          case A_SELECT_LEARN_CAT:
            wms->ms.card_i = -1;
            wms->page = P_SELECT_LEARN_CAT;
            break;
          case A_SELECT_SEARCH_CAT:
            wms->ms.card_i = -1;
            wms->page = P_SELECT_SEARCH_CAT;
            break;
          case A_PREFERENCES:
            wms->page = P_PREFERENCES;
            break;
          case A_ABOUT:
            wms->page = P_ABOUT;
            break;
          case A_APPLY:
            assert(wms->timeout >= 0 && wms->timeout < 5);
            wms->ms.passwd.tok_count = timeouts[wms->timeout][0];
            wms->ms.passwd.tok_sec = timeouts[wms->timeout][1];
            size = sizeof(struct Password);
            e = imf_put(&wms->ms.imf, PW_INDEX, &wms->ms.passwd, size);
            if (e == 0) {
              e = imf_sync (&wms->ms.imf);
              if (e == 0)
                wms->page = P_START;
            }
            break;
          case A_SEARCH:
            wms->found_str = NULL;
            if (wms->ms.card_a > 0)
            {
              assert ((wms->ms.card_i >= -1) && (wms->ms.card_a > 0));
              if (wms->ms.card_i == -1)
                wms->ms.card_i = 0;
              search_card_i = wms->ms.card_i;
              if (wms->ms.search_txt == NULL) {
                wms->ms.search_txt = malloc(1);
                e = wms->ms.search_txt == NULL;
                if (e == 0)
                  wms->ms.search_txt[0] = '\0';
              }
              if (e == 0)
              {
                size = strlen (wms->ms.search_txt) + 1;
                lwr_search_txt = malloc (size);
                e = lwr_search_txt == NULL;
                if (e == 0) {
                  strcpy(lwr_search_txt, wms->ms.search_txt);
                  str_tolower (lwr_search_txt);
                  do
                  {
                    wms->ms.card_i += wms->ms.srch_dir;
                    if (wms->ms.card_i == wms->ms.card_a)
                    {
                      wms->ms.card_i = 0;
                    }
                    if (wms->ms.card_i < 0)
                    {
                      wms->ms.card_i = wms->ms.card_a - 1;
                    }
                    assert(wms->ms.card_i >= 0 && wms->ms.card_i < wms->ms.card_a);
                    e = ms_get_card_sa(&wms->ms);
                    if (e == 0) {
                      qa_str = sa_get(&wms->ms.card_sa, 0);
                      str_tolower(qa_str);
                      wms->found_str = strstr(qa_str, lwr_search_txt);
                      if (wms->found_str == NULL)
                      {
                        qa_str = sa_get(&wms->ms.card_sa, 1);
                        str_tolower(qa_str);
                        wms->found_str = strstr (qa_str, lwr_search_txt);
                      }
                    }
                  } while ((wms->found_str == NULL) && (wms->ms.card_i != search_card_i) && (e == 0));
                  e = ms_get_card_sa(&wms->ms);
                  free(lwr_search_txt);
                }
              }
            }
            wms->page = P_SEARCH;
            break;
          case A_QUESTION:
            if (e == 0) {
              e = ms_determine_card(&wms->ms);
              if (e == 0) {
                wms->page = P_LEARN;
                wms->mode = M_ASK;
              } else if (e < 0) {
                wms->static_msg = "Notification: No card eligible for repetition.";
                wms->static_btn_main = "OK";
                wms->todo_main = S_SELECT_LEARN_CAT;
                wms->page = P_MSG;
              }
            }
            break;
          case A_SHOW:
            assert(wms->ms.card_a > 0 && wms->ms.timestamp > 0 && wms->ms.card_i != -1);
            e = ms_get_card_sa (&wms->ms);
            if (e == 0) {
              wms->page = P_LEARN;
              wms->mode = M_RATE;
            }
            break;
          case A_REVEAL:
            e = ms_get_card_sa(&wms->ms);
            if (e == 0) {
              qa_str = sa_get(&wms->ms.card_sa, 1);
              len = strlen(qa_str);
              e = len > INT32_MAX;
              if (e == 0) {
                assert(wms->saved_reveal_pos < 0 || wms->saved_reveal_pos <= len);
                i = wms->saved_reveal_pos < 0 ? 0 : wms->saved_reveal_pos;
                e = utf8_strcspn(qa_str + i, ",·.-", &n);
                if (e == 0) {
                  wms->reveal_pos = i + n;
                  if (wms->reveal_pos < len) {
                    len = utf8_char_len(qa_str + wms->reveal_pos);
                    e = len == 0;
                    if (e == 0) {
                      wms->reveal_pos += len;
                      size = wms->reveal_pos + 10 + 1; // "---more---" + '\0'
                      str = malloc(size);
                      e = str == NULL;
                      if (e == 0) {
                        strncpy(str, qa_str, wms->reveal_pos);
                        str[wms->reveal_pos] = '\0';
                        strcat(str, "---more---");
                        e = sa_set(&wms->ms.card_sa, 1, str);
                        free(str);
                        wms->page = P_LEARN;
                        wms->mode = M_ASK;
                      }
                    }
                  }
                  else {
                    assert(wms->reveal_pos == len);
                    wms->reveal_pos = -1;
                    wms->page = P_LEARN;
                    wms->mode = M_RATE;
                  }
                }
              }
            }
            break;
          case A_PROCEED:
            assert(mtime_test != 0);
            assert(wms->ms.cat_i >= 0 && wms->ms.cat_i < wms->ms.cat_a && wms->ms.cat_t[wms->ms.cat_i].cat_used != 0);
            assert(wms->ms.timestamp > 0);
            assert(wms->ms.card_i >= 0 && wms->ms.card_i < wms->ms.card_a);
            card_ptr = wms->ms.card_l + wms->ms.card_i;
            if (card_ptr->card_state == STATE_NEW || card_ptr->card_state == STATE_SUSPENDED)
              card_ptr->card_state = STATE_SCHEDULED;
            card_ptr->card_strength = lvl_s[wms->ms.lvl]; // S = -t / log(R)
            card_ptr->card_time = time(NULL);
            e = card_ptr->card_time == -1;
            if (e == 0) {
              assert(card_ptr->card_state == STATE_SCHEDULED || card_ptr->card_state == STATE_SUSPENDED);
              data_size = wms->ms.card_a * sizeof(struct Card);
              e = imf_put(&wms->ms.imf, wms->ms.cat_t[wms->ms.cat_i].cat_cli, wms->ms.card_l, data_size);
              if (e == 0) {
                e = imf_sync(&wms->ms.imf);
                if (e == 0) {
                  e = ms_determine_card(&wms->ms);
                  if (e == 0) {
                    wms->page = P_LEARN;
                    wms->mode = M_ASK;
                  } else if (e < 0) {
                    wms->static_msg = "Notification: No card eligible for repetition.";
                    wms->static_btn_main = "OK";
                    wms->todo_main = S_SELECT_LEARN_CAT;
                    wms->page = P_MSG;
                  }
                }
              }
            }
            break;
          case A_SUSPEND:
            assert(wms->ms.card_a > 0);
            e = wms->ms.card_i < 0 || wms->ms.card_i >= wms->ms.card_a;
            if (e == 0) {
              assert(wms->ms.card_l[wms->ms.card_i].card_state == STATE_SCHEDULED || wms->ms.card_l[wms->ms.card_i].card_state == STATE_NEW);
              wms->ms.card_l[wms->ms.card_i].card_state = STATE_SUSPENDED;
              data_size = wms->ms.card_a * sizeof(struct Card);
              index = wms->ms.cat_t[wms->ms.cat_i].cat_cli;
              e = imf_put(&wms->ms.imf, index, wms->ms.card_l, data_size);
              if (e == 0) {
                e = imf_sync(&wms->ms.imf);
                if (e == 0) {
                  e = ms_determine_card(&wms->ms);
                  if (e == 0) {
                    wms->page = P_LEARN;
                    wms->mode = M_ASK;
                  } else if (e < 0) {
                    wms->static_msg = "Notification: No card eligible for repetition.";
                    wms->static_btn_main = "OK";
                    wms->todo_main = S_SELECT_LEARN_CAT;
                    wms->page = P_MSG;
                  }
                }
              }
            }
            else {
              wms->static_msg = "Warning: Invalid card";
              wms->static_btn_main = "OK";
              wms->todo_main = S_NONE;
              wms->page = P_MSG;
            }
            break;
          case A_RESUME:
            e = wms->ms.card_a <= 0;
            if (e == 0) {
              n = 0;
              for (card_i = 0; card_i < wms->ms.card_a; card_i++)
                if (wms->ms.card_l[card_i].card_state == STATE_SUSPENDED) {
                  wms->ms.card_l[card_i].card_state = STATE_SCHEDULED;
                  n++;
                }
              e = n == 0;
              if (e == 0) {
                data_size = wms->ms.card_a * sizeof(struct Card);
                index = wms->ms.cat_t[wms->ms.cat_i].cat_cli;
                e = imf_put(&wms->ms.imf, index, wms->ms.card_l, data_size);
                if (e == 0) {
                  e = imf_sync(&wms->ms.imf);
                  if (e == 0) {
                    e = ms_determine_card(&wms->ms);
                    if (e == 0) {
                      wms->page = P_LEARN;
                      wms->mode = M_ASK;
                    } else if (e < 0) {
                      wms->static_msg = "Notification: No card eligible for repetition.";
                      wms->static_btn_main = "OK";
                      wms->todo_main = S_SELECT_LEARN_CAT;
                      wms->page = P_MSG;
                    }
                  }
                }
              }
            }
            break;
          case A_CHECK_FILE:
            e = wms->file_title_str == NULL;
            if (e != 0) {
              wms->static_msg = "Please select a file.";
              wms->static_btn_main = "OK";
              wms->todo_main = S_FILELIST;
              wms->page = P_MSG;
            }
            break;
          case A_LOGIN:
            wms->page = P_PASSWORD;
            wms->mode = wms->seq == S_GO_CHANGE ? M_CHANGE_PASSWD : M_DEFAULT;
            break;
          case A_HISTOGRAM:
            for (i = 0; i < 100; i++)
              wms->hist_bucket[i] = 0;
            for (i = 0; i < 21; i++)
              wms->lvl_bucket[i] = 0;
            if (wms->ms.card_a > 0) {
              wms->ms.timestamp = time(NULL);
              e = wms->ms.timestamp == -1;
              if (e == 0) {
                for (card_i = 0; card_i < wms->ms.card_a; card_i++)
                  if (wms->ms.card_l[card_i].card_state == STATE_SCHEDULED) {
                    time_diff = wms->ms.timestamp - wms->ms.card_l[card_i].card_time;
                    retention = exp (- ((double)(time_diff) / wms->ms.card_l[card_i].card_strength));
                    i = retention * 100;
                    assert(i >= 0 && i < 100);
                    wms->hist_bucket[i]++;
                    for (i = 0; i < 21; i++)
                      if (lvl_s[i] >= wms->ms.card_l[card_i].card_strength)
                        break;
                    wms->lvl_bucket[i]++;
                  }
                wms->hist_max = 0;
                for (i = 0; i < 100; i++)
                  if (wms->hist_bucket[i] > wms->hist_max)
                    wms->hist_max = wms->hist_bucket[i];
                wms->page = P_HISTOGRAM;
              }
            }
            break;
          }
        }
        free(wms->ms.password);
        wms->ms.password = NULL;
      }
      e2str(e, e_str);
      rv = fprintf(dbg_stream != NULL ? dbg_stream : stderr, "%ld %s \"%s\"\n",
          wms->ms.timestamp,
          e_str,
          wms->dbg_lp);
      saved_e = e;
      e = rv < 0;
      if (e == 0 && dbg_stream != NULL)
        e = fclose(dbg_stream);
      if (saved_e != 0 && wms->page != P_MSG) {
        free(wms->file_title_str);
        wms->file_title_str = NULL;
        size = sizeof(msg);
        rv = snprintf(msg, size, "Unexpected error (%s)", e_str);
        e = rv < 0 || rv >= size;
        wms->static_msg = msg;
        wms->static_btn_main = "OK";
        wms->todo_main = S_NONE;
        wms->page = P_MSG;
      }
      if (e == 0 || wms->page == P_MSG)
        e = gen_html(wms);
      if (e != 0 && saved_e != 0)
        e = saved_e;
      wms_free(wms);
    }
    else
      e = 0x00339332; // WMSIF wms_init failed
    free(wms);
  }
  else
    e = 0x00021047; // WMWF Web(MemorySurfer) malloc (for) wms failed (failed)
  if (e != 0) {
    e2str(e, e_str);
    fprintf(stderr, "unreported error: %s\n", e_str);
  }
  return e;
}
