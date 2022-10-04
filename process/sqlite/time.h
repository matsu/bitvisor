/*
 * Copyright (c) 2009 Igel Co., Ltd
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the University of Tsukuba nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <core/types.h>
#include <core/printf.h>

typedef long long time_t;

struct tm {
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_wday;
	int tm_yday;
	int tm_isdst;
};

static time_t
tm_to_time (const struct tm *tm)
{
	int y = tm->tm_year - 100;
	int m = tm->tm_mon;
	if (m >= 2) {
		m -= 2;
	} else {
		m += 10;
		y--;
	}
	int y400r = y % 400;
	if (y < 0) {
		y400r += 400;
		y -= 400;
	}
	time_t y400q = y / 400;
	int y100r = y400r % 100;
	int y100q = y400r / 100;
	int y4r = y100r % 4;
	int y4q = y100r / 4;
	time_t d = (y400q * (365 * 400 + 97) + y100q * (365 * 100 + 24) +
		    y4q * (365 * 4 + 1) + y4r * 365 + (m * 306 + 5) / 10 +
		    tm->tm_mday - 1 + 60 + 365 * 30 + 7);
	return d * 86400 + tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec;
}

static struct tm *
real_gmtime (const time_t *timep)
{
	static struct tm ret;
	time_t t = *timep;
	time_t s = t % 86400;
	if (s < 0) {
		s += 86400;
		t -= 86400;
	}
	ret.tm_sec = s % 60;
	s /= 60;
	ret.tm_min = s % 60;
	s /= 60;
	ret.tm_hour = s % 24;

	time_t d = t / 86400;	/* Number of days since 1970-01-01 */
	d -= 365 * 30 + 7;	/* Number of days since 2000-01-01 */
	d -= 60;		/* Number of days since 2000-03-01 */
	int d400 = d % (365 * 400 + 97);
	if (d400 < 0) {
		d400 += 365 * 400 + 97;
		d -= 365 * 400 + 97;
	}
	int y400 = d / (365 * 400 + 97);
	int y100 = d400 / (365 * 100 + 24); /* 0-4 */
	if (y100 == 4)
		y100 = 3;
	int d100 = d400 - y100 * (365 * 100 + 24);
	int d4 = d100 % (365 * 4 + 1);
	int y4 = d100 / (365 * 4 + 1); /* 0-24 */
	int d1 = d4 % 365;	       /* 0-364 */
	int y1 = d4 / 365;	       /* 0-4 */
	if (y1 == 4) {
		y1 = 3;
		d1 += 365;
	}
	int m = (d1 * 10 + 5) / 306; /* 0-11 (Mar-Feb) */
	int dm = d1 - (m * 306 + 5) / 10;
	ret.tm_mday = dm + 1;
	ret.tm_mon = m > 9 ? m - 10 : m + 2;
	ret.tm_year = y400 * 400 + y100 * 100 + y4 * 4 + y1 + (m > 9) + 100;
	ret.tm_wday = (3 + d400) % 7;
	ret.tm_yday = m > 9 ? d1 - 306 : d1 + 59 + (!y1 && (y4 || !y100));
	ret.tm_isdst = 0;
	return &ret;
}

static inline struct tm *
gmtime (const time_t *timep)
{
	return real_gmtime (timep);
}

static size_t
real_strftime (char *s, size_t max, const char *format, const struct tm *tm)
{
	char *start = s;
	for(; *format; format++) {
		char c = *format;
		if (c != '%') {
		put:
			if (max < 2)
				break;
			*s++ = c;
			max--;
			continue;
		}
		do {
			format++;
			c = *format;
			if (c == '%')
				goto put;
		} while (c == 'E' || c == 'O');
		if (!c)
			break;
		int len = 0;
		int yearwday = (tm->tm_wday + 700 - tm->tm_yday) % 7;
		switch (c) {
		case 'a':
		case 'A':
			len = snprintf (s, max, "w%d", tm->tm_wday);
			break;
		case 'b':
		case 'B':
		case 'h':
			len = snprintf (s, max, "m%02d", tm->tm_mon + 1);
			break;
		case 'c':
			len = snprintf (s, max, "%04d-%02d-%02d"
					" %02d:%02d:%02d", tm->tm_year + 1900,
					tm->tm_mon + 1, tm->tm_mday,
					tm->tm_hour, tm->tm_min, tm->tm_sec);
			break;
		case 'C':
			len = snprintf (s, max, "%02d",
					tm->tm_year / 100 + 19);
			break;
		case 'd':
			len = snprintf (s, max, "%02d", tm->tm_mday);
			break;
		case 'D':
			len = snprintf (s, max, "%02d/%02d/%02d", tm->tm_mday,
					tm->tm_mon + 1, tm->tm_year % 100);
			break;
		case 'e':
			len = snprintf (s, max, "%2d", tm->tm_mday);
			break;
		case 'F':
		case 'x':
			len = snprintf (s, max, "%04d-%02d-%02d",
					tm->tm_year + 1900,
					tm->tm_mon + 1, tm->tm_mday);
			break;
		case 'H':
			len = snprintf (s, max, "%02d", tm->tm_hour);
			break;
		case 'I':
			len = snprintf (s, max, "%02d",
					(tm->tm_hour + 12 - 1) % 12 + 1);
			break;
		case 'j':
			len = snprintf (s, max, "%03d", tm->tm_yday + 1);
			break;
		case 'k':
			len = snprintf (s, max, "%2d", tm->tm_hour);
			break;
		case 'l':
			len = snprintf (s, max, "%2d",
					(tm->tm_hour + 12 - 1) % 12 + 1);
			break;
		case 'M':
			len = snprintf (s, max, "%02d", tm->tm_min);
			break;
		case 'm':
			len = snprintf (s, max, "%02d", tm->tm_mon + 1);
			break;
		case 'n':
			len = snprintf (s, max, "\n");
			break;
		case 'p':
			len = snprintf (s, max, "%s",
					tm->tm_hour < 12 ? "AM" : "PM");
			break;
		case 'P':
			len = snprintf (s, max, "%s",
					tm->tm_hour < 12 ? "am" : "pm");
			break;
		case 'r':
			len = snprintf (s, max, "%02d:%02d:%02d %s",
					(tm->tm_hour + 12 - 1) % 12 + 1,
					tm->tm_min, tm->tm_sec,
					tm->tm_hour < 12 ? "AM" : "PM");
			break;
		case 'R':
			len = snprintf (s, max, "%02d:%02d", tm->tm_hour,
					tm->tm_min);
			break;
		case 's':
			len = snprintf (s, max, "%lld", tm_to_time (tm));
			break;
		case 'S':
			len = snprintf (s, max, "%02d", tm->tm_sec);
			break;
		case 't':
			len = snprintf (s, max, "\t");
			break;
		case 'u':
			len = snprintf (s, max, "%d",
					(tm->tm_wday + 7 - 1) % 7 + 1);
			break;
		case 'U':
		case 'V':	/* FIXME */
			len = snprintf (s, max, "%02d",
					(tm->tm_yday + (yearwday + 7 - 1) % 7
					 + 1) / 7);
			break;
		case 'w':
			len = snprintf (s, max, "%d", tm->tm_wday);
			break;
		case 'W':
			len = snprintf (s, max, "%02d",
					(tm->tm_yday + (yearwday + 7 - 2) % 7
					 + 1) / 7);
			break;
		case 'X':
			len = snprintf (s, max, "%02d:%02d:%02d",
					tm->tm_hour, tm->tm_min, tm->tm_sec);
			break;
		case 'y':
		case 'g':	/* FIXME */
			len = snprintf (s, max, "%02d", tm->tm_year % 100);
			break;
		case 'Y':
		case 'G':	/* FIXME */
			len = snprintf (s, max, "%04d", tm->tm_year + 1900);
			break;
		case 'z':
			len = snprintf (s, max, "+%04d", 0);
			break;
		case 'Z':
			len = snprintf (s, max, "%s", "UTC");
			break;
		}
		if (max <= len)
			break;
		s += len;
		max -= len;
	}
	if (max >= 1)
		*s = '\0';
	return s - start;
}

static inline size_t
strftime (char *s, size_t max, const char *format, const struct tm *tm)
{
	return real_strftime (s, max, format, tm);
}
