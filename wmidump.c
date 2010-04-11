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

/**
 * wmi_parse_hexbyte - Convert a ASCII hex number to a byte
 * @src:  Pointer to at least 2 characters to convert.
 *
 * Convert a two character ASCII hex string to a number.
 *
 * Return:  0-255  Success, the byte was parsed correctly
 *          -1     Error, an invalid character was supplied
 */
static int wmi_parse_hexbyte(const uint8_t *src)
{
	unsigned int x; /* For correct wrapping */
	int h;

	/* high part */
	x = src[0];
	if (x - '0' <= '9' - '0') {
		h = x - '0';
	} else if (x - 'a' <= 'f' - 'a') {
		h = x - 'a' + 10;
	} else if (x - 'A' <= 'F' - 'A') {
		h = x - 'A' + 10;
	} else {
		return -1;
	}
	h <<= 4;

	/* low part */
	x = src[1];
	if (x - '0' <= '9' - '0')
		return h | (x - '0');
	if (x - 'a' <= 'f' - 'a')
		return h | (x - 'a' + 10);
	if (x - 'A' <= 'F' - 'A')
		return h | (x - 'A' + 10);
	return -1;
}

/**
 * wmi_swap_bytes - Rearrange GUID bytes to match GUID binary
 * @src:   Memory block holding binary GUID (16 bytes)
 * @dest:  Memory block to hold byte swapped binary GUID (16 bytes)
 *
 * Byte swap a binary GUID to match it's real GUID value
 */
static void wmi_swap_bytes(uint8_t *src, uint8_t *dest)
{
	int i;

	for (i = 0; i <= 3; i++)
		memcpy(dest + i, src + (3 - i), 1);

	for (i = 0; i <= 1; i++)
		memcpy(dest + 4 + i, src + (5 - i), 1);

	for (i = 0; i <= 1; i++)
		memcpy(dest + 6 + i, src + (7 - i), 1);

	memcpy(dest + 8, src + 8, 8);
}

/**
 * wmi_parse_guid - Convert GUID from ASCII to binary
 * @src:   36 char string of the form fa50ff2b-f2e8-45de-83fa-65417f2f49ba
 * @dest:  Memory block to hold binary GUID (16 bytes)
 *
 * N.B. The GUID need not be NULL terminated.
 *
 * Return:  'true'   @dest contains binary GUID
 *          'false'  @dest contents are undefined
 */
static bool wmi_parse_guid(const uint8_t *src, uint8_t *dest)
{
	static const int size[] = { 4, 2, 2, 2, 6 };
	int i, j, v;

	if (src[8]  != '-' || src[13] != '-' ||
		src[18] != '-' || src[23] != '-')
		return false;

	for (j = 0; j < 5; j++, src++) {
		for (i = 0; i < size[j]; i++, src += 2, *dest++ = v) {
			v = wmi_parse_hexbyte(src);
			if (v < 0)
				return false;
		}
	}

	return true;
}

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
		*len += bytes;
		data = realloc(data, *len);
		memcpy(data, buf, bytes);
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

int main(int argc, char *argv[])
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
