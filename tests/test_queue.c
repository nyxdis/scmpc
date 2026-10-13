/**
 * test_queue.c: Tests for loading and saving the song cache.
 *
 * This file is part of scmpc. scmpc is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#include <string.h>
#include <unistd.h>

#include "mpd.h"
#include "preferences.h"
#include "queue.h"

/* normally defined in preferences.c, mpd.c and audioscrobbler.c */
struct prefs_t prefs;
struct mpd_t mpd;
gchar *buffer;

static gchar *tmpdir;

static void remove_tmpdir(void) {
  GDir *dir = g_dir_open(tmpdir, 0, NULL);
  const gchar *name;

  while (dir && (name = g_dir_read_name(dir))) {
    gchar *path = g_build_filename(tmpdir, name, NULL);
    unlink(path);
    g_free(path);
  }
  if (dir)
    g_dir_close(dir);
  rmdir(tmpdir);
  g_free(tmpdir);
}

static gchar *write_cache(const gchar *name, const gchar *contents) {
  gchar *path = g_build_filename(tmpdir, name, NULL);
  g_assert_true(g_file_set_contents(path, contents, -1, NULL));
  return path;
}

/* takes ownership of path and replaces the queue with its contents */
static void load(gchar *path) {
  if (prefs.cache_file)
    queue_cleanup();
  g_free(prefs.cache_file);
  prefs.cache_file = path;
  queue_init();
  queue_load();
}

static void test_long_line_without_newline(void) {
  gchar *title = g_strnfill(1000, 't');
  gchar *contents = g_strdup_printf("# BEGIN SONG\nartist: A\ntitle: %s\n"
                                    "album: B\nlength: 200\ntrack: 7\n"
                                    "date: 1700000000\n# END SONG",
                                    title);

  load(write_cache("long.cache", contents));
  g_assert_cmpuint(queue_get_length(), ==, 1);
  g_assert_cmpstr(queue_peek_head()->title, ==, title);
  g_assert_cmpuint(queue_peek_head()->track, ==, 7);
  g_assert_cmpint(queue_peek_head()->date, ==, 1700000000);
  g_free(title);
  g_free(contents);
}

static void test_fields_reset_between_songs(void) {
  load(write_cache("reset.cache",
                   "# BEGIN SONG\nartist: A\ntitle: One\nlength: 200\n"
                   "track: 9\ndate: 1700000000\n# END SONG\n"
                   "# BEGIN SONG\nartist: A\ntitle: Two\nlength: 200\n"
                   "# END SONG\n"));
  g_assert_cmpuint(queue_get_length(), ==, 2);
  g_assert_cmpuint(queue_peek_nth(1)->track, ==, 0);
  g_assert_cmpint(queue_peek_nth(1)->date, ==, 0);
}

static void test_invalid_songs_rejected(void) {
  load(write_cache("invalid.cache",
                   "# BEGIN SONG\ntitle: No artist\nlength: 200\n# END SONG\n"
                   "# BEGIN SONG\nartist: A\ntitle: Too short\nlength: 10\n"
                   "# END SONG\n"
                   "garbage line\n"));
  g_assert_cmpuint(queue_get_length(), ==, 0);
}

static void test_missing_file(void) {
  load(g_build_filename(tmpdir, "does-not-exist", NULL));
  g_assert_cmpuint(queue_get_length(), ==, 0);
}

static void test_save_round_trip(void) {
  gchar *saved, *contents;

  load(write_cache("in.cache",
                   "# BEGIN SONG\nartist: A\ntitle: Line\nbreak\n"
                   "album: \nlength: 200\ntrack: 3\ndate: 1700000000\n"
                   "# END SONG\n"));
  /* simulate a tag that contains a newline */
  g_free(queue_peek_head()->title);
  queue_peek_head()->title = g_strdup("Line\nbreak");

  saved = g_build_filename(tmpdir, "out.cache", NULL);
  g_free(prefs.cache_file);
  prefs.cache_file = g_strdup(saved);
  g_assert_true(queue_save(NULL));
  g_assert_true(g_file_get_contents(saved, &contents, NULL, NULL));
  g_assert_nonnull(strstr(contents, "title: Line break\n"));
  g_free(contents);

  load(g_strdup(saved));
  g_assert_cmpuint(queue_get_length(), ==, 1);
  g_assert_cmpstr(queue_peek_head()->title, ==, "Line break");
  g_assert_cmpuint(queue_peek_head()->track, ==, 3);
  g_free(saved);
}

static void test_queue_length_cap(void) {
  GString *contents = g_string_new("");

  for (gint i = 0; i < 5; i++)
    g_string_append_printf(contents,
                           "# BEGIN SONG\nartist: A\ntitle: %d\nlength: 200\n"
                           "date: %d\n# END SONG\n",
                           i, i);
  prefs.queue_length = 3;
  load(write_cache("cap.cache", contents->str));
  g_assert_cmpuint(queue_get_length(), ==, 3);
  g_assert_cmpstr(queue_peek_head()->title, ==, "2"); /* oldest dropped */
  prefs.queue_length = 500;
  g_string_free(contents, TRUE);
}

int main(int argc, char **argv) {
  gint ret;

  g_test_init(&argc, &argv, NULL);
  tmpdir = g_dir_make_tmp("scmpc-test-XXXXXX", NULL);
  g_assert_nonnull(tmpdir);
  prefs.queue_length = 500;

  g_test_add_func("/queue/load/long-line", test_long_line_without_newline);
  g_test_add_func("/queue/load/reset", test_fields_reset_between_songs);
  g_test_add_func("/queue/load/invalid", test_invalid_songs_rejected);
  g_test_add_func("/queue/load/missing", test_missing_file);
  g_test_add_func("/queue/save/round-trip", test_save_round_trip);
  g_test_add_func("/queue/length-cap", test_queue_length_cap);
  ret = g_test_run();

  queue_cleanup();
  g_free(prefs.cache_file);
  remove_tmpdir();
  return ret;
}
