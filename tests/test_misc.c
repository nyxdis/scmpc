/**
 * test_misc.c: Tests for the cURL response buffer and time helpers.
 *
 * This file is part of scmpc. scmpc is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#include <string.h>

#include "audioscrobbler.h"
#include "misc.h"
#include "preferences.h"

/* normally defined in preferences.c and audioscrobbler.c */
struct prefs_t prefs;
gchar *buffer;

static void reset_buffer(void) {
  g_free(buffer);
  buffer = NULL;
}

/* libcurl delivers a body in chunks that are not NUL-terminated */
static void test_buffer_write_chunks(void) {
  const gchar *chunks[] = {"<lfm status=\"ok\"><scrobbles><ignor",
                           "edMessage code=\"0\"></ignoredMessage>",
                           "</scrobbles></lfm>"};
  gchar raw[128];
  gsize total = 0;

  reset_buffer();
  for (guint i = 0; i < G_N_ELEMENTS(chunks); i++) {
    gsize n = strlen(chunks[i]);
    memcpy(raw, chunks[i], n);
    memset(raw + n, 'X', 8); /* garbage instead of a terminator */
    g_assert_cmpuint(buffer_write(raw, 1, n, NULL), ==, n);
    total += n;
  }

  g_assert_nonnull(buffer);
  g_assert_cmpuint(strlen(buffer), ==, total);
  g_assert_nonnull(strstr(buffer, "<lfm status=\"ok\">"));
  g_assert_nonnull(strstr(buffer, "<ignoredMessage code=\"0\">"));
  g_assert_null(strchr(buffer, 'X'));
  reset_buffer();
}

static void test_buffer_write_nmemb(void) {
  reset_buffer();
  g_assert_cmpuint(buffer_write("abcdef", 2, 3, NULL), ==, 6);
  g_assert_cmpstr(buffer, ==, "abcdef");
  reset_buffer();
}

static void test_elapsed(void) {
  gint64 now = get_monotonic_time();

  g_assert_cmpint(elapsed(now), >=, 0);
  g_assert_cmpint(elapsed(now - 600), >=, 600);
  /* Last.fm timestamps are real UNIX time, after 2020-01-01 */
  g_assert_cmpint(get_time(), >, 1577836800);
}

int main(int argc, char **argv) {
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/misc/buffer_write/chunks", test_buffer_write_chunks);
  g_test_add_func("/misc/buffer_write/nmemb", test_buffer_write_nmemb);
  g_test_add_func("/misc/elapsed", test_elapsed);
  return g_test_run();
}
