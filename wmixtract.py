import sys
import re

files = {}

def dump_file(buf):
    name = buf['name']

    if name not in files:
        files[name] = 0
    else:
        files[name] += 1
        name = name + str(files[name])

    print ("Writing %s in %s and %s.bin" % (buf['name'], name, name))

    fp = open(name, "w")
    fp.write(",".join(buf['data']))

    fp = open(name + ".bin", "w")
    for byte in buf['data']:
        byte = byte.strip()
        if byte:
            fp.write(chr(int(byte, 16)))

def find_buffer(name, data):
    pattern = r"Name\s*\((%s),\s*Buffer\s*\(((0x)?[\dA-Fa-f]+)\)\s*{(.*?)}\s*\)" % name

    for group in re.findall(pattern, data, re.DOTALL):
        buf = {}
        buf['name'] = group[0]
        buf['size'] = group[1]
        buf['data'] = group[3]

        buf['data'] = re.sub(r"\s*/\*.*?\*/\s*", "", buf['data'])
        buf['data'] = re.split("\s*,\s*|\s*", buf['data'])

        yield buf

def main():
    if len(sys.argv) >= 2:
        if sys.argv[1] == '--help':
            print ('Usage: %s DSDT.dsl' % sys.argv[0])
            return 0
        stdin = open(sys.argv[1])
    else:
        stdin = sys.stdin

    data = stdin.read()

    for buf in find_buffer("_WDG", data):
        dump_file(buf)
    for buf in find_buffer("WQ[A-Z]{2}", data):
        dump_file(buf)

if __name__ == '__main__':
    ret = main()
    sys.exit(ret)
