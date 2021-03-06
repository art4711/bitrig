/*	$OpenBSD: map.c,v 1.27 2012/05/29 19:53:10 gilles Exp $	*/

/*
 * Copyright (c) 2008 Gilles Chehade <gilles@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/param.h>
#include <sys/socket.h>

#include <event.h>
#include <imsg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smtpd.h"
#include "log.h"

struct map_backend *map_backend_lookup(enum map_src);

extern struct map_backend map_backend_static;

extern struct map_backend map_backend_db;
extern struct map_backend map_backend_stdio;

struct map_backend *
map_backend_lookup(enum map_src source)
{
	switch (source) {
	case S_NONE:
		return &map_backend_static;

	case S_DB:
		return &map_backend_db;

	case S_PLAIN:
		return &map_backend_stdio;

	default:
		fatalx("invalid map type");
	}
	return NULL;
}

struct map *
map_findbyname(const char *name)
{
	struct map	*m;

	TAILQ_FOREACH(m, env->sc_maps, m_entry) {
		if (strcmp(m->m_name, name) == 0)
			break;
	}
	return (m);
}

struct map *
map_find(objid_t id)
{
	struct map	*m;

	TAILQ_FOREACH(m, env->sc_maps, m_entry) {
		if (m->m_id == id)
			break;
	}
	return (m);
}

void *
map_lookup(objid_t mapid, char *key, enum map_kind kind)
{
	void *hdl = NULL;
	char *ret = NULL;
	struct map *map;
	struct map_backend *backend = NULL;

	map = map_find(mapid);
	if (map == NULL)
		return NULL;

	backend = map_backend_lookup(map->m_src);
	hdl = backend->open(map);
	if (hdl == NULL) {
		log_warn("map_lookup: can't open %s", map->m_config);
		return NULL;
	}

	ret = backend->lookup(hdl, key, kind);

	backend->close(hdl);
	return ret;
}

int
map_compare(objid_t mapid, char *key, enum map_kind kind,
    int (*func)(char *, char *))
{
	void *hdl = NULL;
	struct map *map;
	struct map_backend *backend = NULL;
	int ret;

	map = map_find(mapid);
	if (map == NULL)
		return 0;

	backend = map_backend_lookup(map->m_src);
	hdl = backend->open(map);
	if (hdl == NULL) {
		log_warn("map_lookup: can't open %s", map->m_config);
		return 0;
	}

	ret = backend->compare(hdl, key, kind, func);

	backend->close(hdl);
	return ret;	
}
