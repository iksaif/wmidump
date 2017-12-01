/*
 *  Copyright (C) 2010 Corentin Chary <corentincj@iksaif.net>
 *
 *  Stolen from drivers/platform/x86/wmi.c:
 *   Copyright (C) 2007-2008 Carlos Corbacho <carlos@strangeworlds.co.uk>
 *
 *  GUID parsing code from ldm.c is:
 *   Copyright (C) 2001,2002 Richard Russon <ldm@flatcap.org>
 *   Copyright (c) 2001-2007 Anton Altaparmakov
 *   Copyright (C) 2001,2002 Jakob Kemi <jakob.kemi@telia.com>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or (at
 *  your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <sys/types.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * If the GUID data block is marked as expensive, we must enable and
 * explicitily disable data collection.
 */
#define ACPI_WMI_EXPENSIVE   0x1
#define ACPI_WMI_METHOD      0x2	/* GUID is a method */
#define ACPI_WMI_STRING      0x4	/* GUID takes & returns a string */
#define ACPI_WMI_EVENT       0x8	/* GUID is an event */

struct guid_block {
	char guid[16];
	union {
		char object_id[2];
		struct {
			unsigned char notify_id;
			unsigned char reserved;
		};
	};
	uint8_t instance_count;
	uint8_t flags;
};

/*
 * Convert a raw GUID to the ACII string representation
 */
static int wmi_gtoa(const char *in, char *out)
{
	int i;

	for (i = 3; i >= 0; i--)
		out += sprintf(out, "%02X", in[i] & 0xFF);

	out += sprintf(out, "-");
	out += sprintf(out, "%02X", in[5] & 0xFF);
	out += sprintf(out, "%02X", in[4] & 0xFF);
	out += sprintf(out, "-");
	out += sprintf(out, "%02X", in[7] & 0xFF);
	out += sprintf(out, "%02X", in[6] & 0xFF);
	out += sprintf(out, "-");
	out += sprintf(out, "%02X", in[8] & 0xFF);
	out += sprintf(out, "%02X", in[9] & 0xFF);
	out += sprintf(out, "-");

	for (i = 10; i <= 15; i++)
		out += sprintf(out, "%02X", in[i] & 0xFF);

	*out = '\0';
	return 0;
}

/*
 * Parse the _WDG method for the GUID data blocks
 */
static void parse_wdg(const char *data, size_t len)
{
	char guid_string[37];
	union {
		const char *s;
		const struct guid_block *g;
	} ptr;
	const char *end;

	/* Round downwards to multiple of sizeof(struct guid_block). */
	end = data + len - (len % sizeof(struct guid_block));
	for (ptr.s = data; ptr.s < end; ptr.g++) {
		wmi_gtoa(ptr.g->guid, guid_string);
		printf("%s:\n", guid_string);
		printf("\tobject_id: %c%c\n",
		    ptr.g->object_id[0], ptr.g->object_id[1]);
		printf("\tnotify_id: %02X\n", ptr.g->notify_id);
		printf("\treserved: %02X\n", ptr.g->reserved);
		printf("\tinstance_count: %d\n", ptr.g->instance_count);
		printf("\tflags: %#x", ptr.g->flags);
		if (ptr.g->flags) {
			printf(" ");
			if (ptr.g->flags & ACPI_WMI_EXPENSIVE)
				printf("ACPI_WMI_EXPENSIVE ");
			if (ptr.g->flags & ACPI_WMI_METHOD)
				printf("ACPI_WMI_METHOD ");
			if (ptr.g->flags & ACPI_WMI_STRING)
				printf("ACPI_WMI_STRING ");
			if (ptr.g->flags & ACPI_WMI_EVENT)
				printf("ACPI_WMI_EVENT ");
		}
		printf("\n");
	}
}

static char *parse_ascii_wdg(const char *wdg, size_t *bytes)
{
	char *end;
	char *data = NULL;
	long lval;
	size_t lno = 1;
	size_t cno = 1;
	int comment = 0;

	*bytes = 0;

	for (; *wdg; wdg++) {
		if (*wdg == '\n') {
			lno++;
			cno = 1;
		} else {
			cno++;
		}

		/* Handle multiline comments */
		if (wdg[0] == '/' && wdg[1] == '*') {
			comment++;
			wdg++;
			continue;
		}
		if (wdg[0] == '*' && wdg[1] == '/') {
			comment--;
			wdg++;
			continue;
		}
		if (comment)
			continue;

		/* Handle trailing comments. */
		if (wdg[0] == '/' && wdg[1] == '/') {
			for (wdg += 2; *wdg != '\n' && *wdg != '\0'; wdg++)
				continue;
			if (*wdg == '\0')
				break;
			continue;
		}

		if (!isalnum(*wdg))
			continue;
		if (wdg[0] != '0' || wdg[1] != 'x')
			errx(1, "<stdin>:%ld:%ld: expected hex prefix, "
			    "got `%c%c'",
			    lno, cno, wdg[0], wdg[1]);

		errno = 0;
		lval = strtol(wdg, &end, 16);
		if (lval < 0 || lval > UCHAR_MAX ||
		    (errno == ERANGE && (lval == LONG_MAX || lval == LONG_MIN)))
			errx(1, "<stdin>:%ld:%ld: invalid hex number",
			    lno, cno);
		wdg += end - wdg;

		(*bytes)++;
		data = realloc(data, *bytes);
		if (data == NULL)
			err(1, NULL);
		data[(*bytes) - 1] = lval;
	}
	return data;
}

static char *read_wdg(int fd, size_t *len)
{
	char buf[1024];
	char *data = NULL;
	ssize_t bytes;

	*len = 0;

	while ((bytes = read(fd, buf, sizeof(buf))) > 0) {
		data = realloc(data, *len + bytes);
		if (data == NULL)
			err(1, NULL);
		memcpy(data + *len, buf, bytes);
		*len += bytes;
	}
	if (bytes < 0) {
		perror("read()");
		free(data);
		return NULL;
	}
	data = realloc(data, (*len) + 1);
	if (data == NULL)
		err(1, NULL);
	data[*len] = '\0';
	return data;
}

int main(void)
{
	char *data = NULL;
	char *wdg = NULL;
	size_t len;
	int err = 0;

	wdg = read_wdg(STDIN_FILENO, &len);
	if (!wdg) {
		err = 1;
		goto done;
	}

	data = parse_ascii_wdg(wdg, &len);
	if (!data) {
		err = 1;
		goto done;
	}

	parse_wdg(data, len);

done:
	free(data);
	free(wdg);
	return err;
}
