/**
 * test_audioscrobbler.c: Tests for parsing Last.fm error responses.
 *
 * The module is included directly so its static functions and connection
 * state can be tested. None of these tests make network requests.
 *
 * This file is part of scmpc. scmpc is free software; you can redistribute
 * it and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 */

#include "../src/audioscrobbler.c"

#include <glib/gstdio.h>

/* normally defined in preferences.c and mpd.c */
struct prefs_t prefs;
struct mpd_t mpd;

static void parse(const gchar *xml) {
  gchar *response = g_strdup(xml);
  as_parse_error(response);
  g_free(response);
}

static void test_bad_auth(void) {
  as_conn.status = CONNECTED;
  parse("<lfm status=\"failed\">\n"
        "<error code=\"4\">Authentication Failed</error></lfm>");
  g_assert_cmpint(as_conn.status, ==, BADAUTH);
}

static void test_missing_error_element(void) {
  as_conn.status = CONNECTED;
  parse("<lfm status=\"failed\">\n</lfm>");
  parse("<lfm status=\"failed\"><error code=\"6\"");
  g_assert_cmpint(as_conn.status, ==, CONNECTED);
}

/* an expired session asks for a new login, which is throttled here so no
 * request is made; the response must not be touched afterwards */
static void test_expired_session_throttled(void) {
  as_conn.status = CONNECTED;
  as_conn.last_auth = get_monotonic_time();
  buffer = g_strdup("<lfm status=\"failed\">\n<error code=\"9\">Invalid "
                    "session key - Please re-authenticate</error></lfm>");
  as_parse_error(buffer);
  g_free(buffer);
  buffer = NULL;
  g_assert_cmpint(as_conn.status, ==, CONNECTED);
}

/* a failed first login, e.g. before the network is up at boot, is retried
 * on the next submission, but at most once a minute; API_URL points at a
 * closed local port, so attempts fail immediately without leaving the host */
static void test_retry_failed_login(void) {
  gchar *path = g_build_filename(g_get_tmp_dir(), "scmpc-retry.cache", NULL);

  g_assert_true(g_file_set_contents(
      path, "# BEGIN SONG\nartist: A\ntitle: T\nlength: 200\n# END SONG\n", -1,
      NULL));
  prefs.cache_file = path;
  prefs.queue_length = 500;
  queue_init();
  queue_load();
  g_assert_cmpuint(queue_get_length(), ==, 1);

  as_conn.status = DISCONNECTED;
  as_conn.last_auth = 0;
  as_conn.last_attempt = 0;
  as_check_submit();
  g_assert_cmpint(as_conn.last_attempt, >, 0); /* tried to log in */
  g_assert_cmpint(as_conn.status, ==, DISCONNECTED);

  as_conn.last_attempt = get_monotonic_time() - 30;
  as_check_submit();
  g_assert_cmpint(elapsed(as_conn.last_attempt), >=, 30); /* throttled */

  as_conn.last_attempt = get_monotonic_time() - 61;
  as_check_submit();
  g_assert_cmpint(elapsed(as_conn.last_attempt), <, 5); /* tried again */

  g_assert_cmpuint(queue_get_length(), ==, 1); /* nothing was lost */
  queue_cleanup();
  g_unlink(path);
  g_free(path);
}

static void test_xml_text_after(void) {
  gchar *text;

  text = xml_text_after("<session><key>abc123</key></session>", "<key>");
  g_assert_cmpstr(text, ==, "abc123");
  g_free(text);

  g_assert_null(xml_text_after("<session></session>", "<key>"));

  text = xml_text_after("<key>unterminated", "<key>");
  g_assert_cmpstr(text, ==, "unterminated");
  g_free(text);
}

int main(int argc, char **argv) {
  g_test_init(&argc, &argv, NULL);
  /* the code under test reports Last.fm errors with g_warning() */
  g_log_set_always_fatal(G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);

  prefs.as_username = g_strdup("user");
  prefs.as_password = g_strdup("password");
  prefs.as_password_hash = g_strdup("");
  g_assert_true(as_connection_init());

  g_test_add_func("/audioscrobbler/error/bad-auth", test_bad_auth);
  g_test_add_func("/audioscrobbler/error/missing-element",
                  test_missing_error_element);
  g_test_add_func("/audioscrobbler/error/expired-session",
                  test_expired_session_throttled);
  g_test_add_func("/audioscrobbler/xml-text-after", test_xml_text_after);
  g_test_add_func("/audioscrobbler/retry-failed-login",
                  test_retry_failed_login);
  return g_test_run();
}
