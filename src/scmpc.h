/**
 * scmpc.h: The front end of the program.
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


#ifndef HAVE_SCMPC_H
#define HAVE_SCMPC_H

#include <glib.h>

/**
 * Exit a running scmpc instance
 */
void kill_scmpc(void);

/**
 * Exit the current scmpc instance
 */
void scmpc_shutdown(void);

/**
 * Check if a song is eligible for submission and add it to the queue
 */
gboolean scmpc_check(gpointer data);

#endif /* HAVE_SCMPC_H */
