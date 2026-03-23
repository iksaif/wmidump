#!/bin/sh
# Integration tests for wmidump
WMIDUMP="${WMIDUMP:-./wmidump}"
PASS=0
FAIL=0

assert_output() {
    name="$1"
    input="$2"
    expected="$3"

    actual=$(printf '%s' "$input" | "$WMIDUMP")
    if [ "$actual" = "$expected" ]; then
        PASS=$((PASS + 1))
        printf "  PASS: %s\n" "$name"
    else
        FAIL=$((FAIL + 1))
        printf "  FAIL: %s\n" "$name"
        printf "    expected:\n%s\n" "$expected"
        printf "    got:\n%s\n" "$actual"
    fi
}

assert_empty() {
    name="$1"
    input="$2"

    actual=$(printf '%s' "$input" | "$WMIDUMP")
    if [ -z "$actual" ]; then
        PASS=$((PASS + 1))
        printf "  PASS: %s\n" "$name"
    else
        FAIL=$((FAIL + 1))
        printf "  FAIL: %s (expected empty output)\n" "$name"
        printf "    got:\n%s\n" "$actual"
    fi
}

printf "Running wmidump tests...\n"

# Test 1: Basic WDG parsing with multiple GUID blocks
assert_output "multiple GUID blocks" \
    "0xD0, 0x5E, 0x84, 0x97, 0x6D, 0x4E, 0xDE, 0x11, 0x8A, 0x39, 0x08, 0x00, 0x20, 0x0C, 0x9A, 0x66, 0x42, 0x43, 0x01, 0x02, 0xA0, 0x47, 0x67, 0x46, 0xEC, 0x70, 0xDE, 0x11, 0x8A, 0x39, 0x08, 0x00, 0x20, 0x0C, 0x9A, 0x66, 0x42, 0x44, 0x01, 0x02, 0x72, 0x0F, 0xBC, 0xAB, 0xA1, 0x8E, 0xD1, 0x11, 0x00, 0xA0, 0xC9, 0x06, 0x29, 0x10, 0x00, 0x00, 0xD2, 0x00, 0x01, 0x08, 0x21, 0x12, 0x90, 0x05, 0x66, 0xD5, 0xD1, 0x11, 0xB2, 0xF0, 0x00, 0xA0, 0xC9, 0x06, 0x29, 0x10, 0x4D, 0x4F, 0x01, 0x00" \
    "97845ED0-4E6D-11DE-8A39-0800200C9A66:
	object_id: BC
	instance_count: 1
	flags: 0x2 ACPI_WMI_METHOD
466747A0-70EC-11DE-8A39-0800200C9A66:
	object_id: BD
	instance_count: 1
	flags: 0x2 ACPI_WMI_METHOD
ABBC0F72-8EA1-11D1-00A0-C90629100000:
	notify_id: 0xD2
	instance_count: 1
	flags: 0x8 ACPI_WMI_EVENT
05901221-D566-11D1-B2F0-00A0C9062910:
	object_id: MO
	instance_count: 1
	flags: 0"

# Test 2: Single GUID block with method flag
assert_output "single method GUID" \
    "D0 5E 84 97 6D 4E DE 11 8A 39 08 00 20 0C 9A 66 42 43 01 02" \
    "97845ED0-4E6D-11DE-8A39-0800200C9A66:
	object_id: BC
	instance_count: 1
	flags: 0x2 ACPI_WMI_METHOD"

# Test 3: Input without 0x prefix (bare hex)
assert_output "bare hex without 0x prefix" \
    "D0 5E 84 97 6D 4E DE 11 8A 39 08 00 20 0C 9A 66 42 43 01 02" \
    "97845ED0-4E6D-11DE-8A39-0800200C9A66:
	object_id: BC
	instance_count: 1
	flags: 0x2 ACPI_WMI_METHOD"

# Test 4: Event flag (ACPI_WMI_EVENT = 0x8) - prints notify_id instead of object_id
assert_output "event GUID shows notify_id" \
    "72 0F BC AB A1 8E D1 11 00 A0 C9 06 29 10 00 00 D2 00 01 08" \
    "ABBC0F72-8EA1-11D1-00A0-C90629100000:
	notify_id: 0xD2
	instance_count: 1
	flags: 0x8 ACPI_WMI_EVENT"

# Test 5: No flags set
assert_output "no flags" \
    "21 12 90 05 66 D5 D1 11 B2 F0 00 A0 C9 06 29 10 4D 4F 01 00" \
    "05901221-D566-11D1-B2F0-00A0C9062910:
	object_id: MO
	instance_count: 1
	flags: 0"

# Test 6: Block comments are stripped
assert_output "block comments stripped" \
    "/* 0000 */ 0xD0, 0x5E, 0x84, 0x97, 0x6D, 0x4E, 0xDE, 0x11, /* foo */ 0x8A, 0x39, 0x08, 0x00, 0x20, 0x0C, 0x9A, 0x66, 0x42, 0x43, 0x01, 0x02" \
    "97845ED0-4E6D-11DE-8A39-0800200C9A66:
	object_id: BC
	instance_count: 1
	flags: 0x2 ACPI_WMI_METHOD"

# Test 7: Line comments are stripped
INPUT_WITH_LINE_COMMENT="// this is a comment
D0 5E 84 97 6D 4E DE 11 8A 39 08 00 20 0C 9A 66 42 43 01 02"
assert_output "line comments stripped" \
    "$INPUT_WITH_LINE_COMMENT" \
    "97845ED0-4E6D-11DE-8A39-0800200C9A66:
	object_id: BC
	instance_count: 1
	flags: 0x2 ACPI_WMI_METHOD"

# Test 8: Empty input produces no output
assert_empty "empty input" ""

# Test 9: Multiple flags combined (EXPENSIVE | METHOD = 0x3)
assert_output "multiple flags" \
    "D0 5E 84 97 6D 4E DE 11 8A 39 08 00 20 0C 9A 66 42 43 01 03" \
    "97845ED0-4E6D-11DE-8A39-0800200C9A66:
	object_id: BC
	instance_count: 1
	flags: 0x3 ACPI_WMI_EXPENSIVE ACPI_WMI_METHOD"

# Test 10: STRING flag (0x4)
assert_output "string flag" \
    "D0 5E 84 97 6D 4E DE 11 8A 39 08 00 20 0C 9A 66 42 43 01 04" \
    "97845ED0-4E6D-11DE-8A39-0800200C9A66:
	object_id: BC
	instance_count: 1
	flags: 0x4 ACPI_WMI_STRING"

printf "\nResults: %d passed, %d failed\n" "$PASS" "$FAIL"

if [ "$FAIL" -ne 0 ]; then
    exit 1
fi
