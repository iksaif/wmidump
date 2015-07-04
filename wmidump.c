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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>


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

	out = '\0';
	return 0;
}

/*
 * Parse the _WDG method for the GUID data blocks
 */
static void parse_wdg(const void *data, size_t len)
{
	const struct guid_block *gblock = data;
	char guid_string[37];
	uint32_t i, total;

	total = len / sizeof(struct guid_block);

	for (i = 0; i < total; i++) {
		const struct guid_block *g = &gblock[i];

		wmi_gtoa(g->guid, guid_string);
		printf("%s:\n", guid_string);
		printf("\tobject_id: %c%c\n", g->object_id[0], g->object_id[1]);
		printf("\tnotify_id: %02X\n", g->notify_id);
		printf("\treserved: %02X\n", g->reserved);
		printf("\tinstance_count: %d\n", g->instance_count);
		printf("\tflags: %#x", g->flags);
		if (g->flags) {
			printf(" ");
			if (g->flags & ACPI_WMI_EXPENSIVE)
				printf("ACPI_WMI_EXPENSIVE ");
			if (g->flags & ACPI_WMI_METHOD)
				printf("ACPI_WMI_METHOD ");
			if (g->flags & ACPI_WMI_STRING)
				printf("ACPI_WMI_STRING ");
			if (g->flags & ACPI_WMI_EVENT)
				printf("ACPI_WMI_EVENT ");
		}
		printf("\n");
	}
}

static void *parse_ascii_wdg(const char *wdg, size_t *bytes)
{
	static int comment = 0;
	char *p = (char *)wdg;
	char *data = NULL;

	*bytes = 0;

	for (; *p; p++) {
		if (p[0] == '/' && p[1] == '*') {
			comment++;
			p++;
			continue;
		}
		if (p[0] == '*' && p[1] == '/') {
			comment--;
			p++;
			continue;
		}
		if (comment)
			continue;
		if (!isalnum(*p))
			continue;
		char c = strtol(p, &p, 16);
		(*bytes)++;
		data = realloc(data, *bytes);
		data[(*bytes) - 1] = c;
	}
	return data;
}

static void *read_wdg(int fd, size_t *len)
{
	void *data = NULL;
	char buf[1024];
	ssize_t bytes;

	*len = 0;

	while ((bytes = read(fd, buf, sizeof(buf))) > 0) {
		size_t offset = *len;
		*len += bytes;
		data = realloc(data, *len);
		memcpy((uint8_t *)data + offset, buf, bytes);
	}
	if (bytes < 0) {
		perror("read()");
		free(data);
		return NULL;
	}
	data = realloc(data, (*len) + 1);
	((char *)data)[*len] = '\0';
	return data;
}

int main(void)
{
	size_t len;
	void *data, *wdg;
	int err = 0;

	wdg = read_wdg(STDIN_FILENO, &len);
	if (!wdg) {
		err = -1;
		goto exit;
	}

	data = parse_ascii_wdg(wdg, &len);
	if (!data) {
		err = -1;
		goto free_wdg;
	}

	parse_wdg(data, len);
	free(data);
free_wdg:
	free(wdg);
exit:
	return err;
}
