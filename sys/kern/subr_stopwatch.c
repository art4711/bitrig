/*
 * Copyright (c) 2012 Artur Grabowski <art@blahonga.org>
 * Copyright (c) 2012 Thordur Bjornsson <thib@secnorth.net>
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
#include <sys/time.h>
#include <sys/stopwatch.h>

void
stopwatch_reset(struct stopwatch *sw)
{
	sw->t.sec = sw->t.frac = 0;
}

void
stopwatch_start(struct stopwatch *sw)
{
	struct bintime bt;

	bintime(&bt);
	bintime_sub(&sw->t, &bt);
}

void
stopwatch_stop(struct stopwatch *sw)
{
	struct bintime bt;

	bintime(&bt);
	bintime_add(&sw->t, &bt);
}

void
stopwatch_handover(struct stopwatch *from, struct stopwatch *to)
{
	struct bintime bt;

	bintime(&bt);
	bintime_add(&from->t, &bt);
	bintime_sub(&to->t, &bt);
}

void
stopwatch_snapshot(struct stopwatch *from, struct stopwatch *to)
{
	struct bintime bt;

	bintime(&bt);
	*to = *from;
	bintime_add(&to->t, &bt);
}

void
stopwatch_to_ts(struct stopwatch *sw, struct timespec *ts)
{
	bintime2timespec(&sw->t, ts);
}

void
stopwatch_to_tv(struct stopwatch *sw, struct timeval *tv)
{
	bintime2timeval(&sw->t, tv);
}

int
stopwatch_to_s(struct stopwatch *sw)
{
	return sw->t.sec;
}

#ifndef _KERNEL
double
stopwatch_to_ns(struct stopwatch *sw)
{
	return (double)sw->t;
}
#endif
