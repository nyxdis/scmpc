/**
 * misc.c: Misc helper functions
 *
 * ==================================================================
 * Copyright (c) 2009-2013 Christoph Mende <mende.christoph@gmail.com>
 * Based on Jonathan Coome's work on scmpc
 *
 * This file is part of scmpc.
 *
 * scmpc is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * scmpc is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with scmpc; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 * ==================================================================
 */

#include <string.h>

#include "audioscrobbler.h"
#include "misc.h"
#include "preferences.h"

/**
 * Log file resource
 */
static FILE *log_file;

void open_log(const gchar *filename) {
  if (!prefs.fork) {
    log_file = stdout;
    return;
  }

  log_file = fopen(filename, "a");
  if (!log_file) {
    fputs("Unable to open log file for writing,"
          " logging to stdout\n",
          stderr);
    log_file = stdout;
  }
}

void scmpc_log(G_GNUC_UNUSED const gchar *log_domain, GLogLevelFlags log_level,
               const gchar *message, G_GNUC_UNUSED gpointer user_data) {
  gchar *ts;
  const gchar format[] = "%Y-%m-%d %H:%M:%S  ";

  /* strip G_LOG_FLAG_FATAL and G_LOG_FLAG_RECURSION before comparing, or
   * g_error() and other fatal messages compare as less important */
  GLogLevelFlags level = log_level & G_LOG_LEVEL_MASK;

  if (level > prefs.log_level)
    return;

  /* when logging to a file, also show errors to whoever started scmpc;
   * once daemonised stderr is /dev/null and this is a no-op */
  if (log_file != stdout && level <= G_LOG_LEVEL_CRITICAL) {
    fputs(message, stderr);
    fputs("\n", stderr);
  }

  GDateTime *datetime = g_date_time_new_now_local();
  ts = g_date_time_format(datetime, format);
  fputs(ts, log_file);
  g_free(ts);
  g_date_time_unref(datetime);

  fputs(message, log_file);
  fputs("\n", log_file);
  fflush(log_file);
}

gsize buffer_write(void *input, gsize size, gsize nmemb,
                   G_GNUC_UNUSED void *buf) {
  gsize len = size * nmemb;
  gsize old = buffer ? strlen(buffer) : 0;

  /* libcurl delivers the body in chunks that are not NUL-terminated,
   * so append to what has already arrived instead of replacing it */
  buffer = g_realloc(buffer, old + len + 1);
  memcpy(buffer + old, input, len);
  buffer[old + len] = '\0';
  return len;
}

gint64 get_time(void) { return g_get_real_time() / G_USEC_PER_SEC; }

gint64 get_monotonic_time(void) {
  return g_get_monotonic_time() / G_USEC_PER_SEC;
}

gint64 elapsed(gint64 since) { return get_monotonic_time() - since; }
