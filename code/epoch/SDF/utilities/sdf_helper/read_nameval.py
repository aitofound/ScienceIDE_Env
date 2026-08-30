import re
import sdf_helper as sh


def parse_name_val(str, delim='=', include_strings=False):
    if str.find(delim) == -1:
        return None

    pair = str.split(delim)
    if len(pair) != 2:
        return None

    val = None
    try:
        val = int(pair[1].strip())
    except:
        try:
            val = float(pair[1].strip())
        except:
            if include_strings:
                val = pair[1].strip()

    if val is not None:
        return (pair[0].strip(), val)
    else:
        return None


def extr_handled_ok(str):
    reg = re.compile('Element (.*) handled OK')
    res = re.search(reg, str)
    try:
        return res.groups()[0]
    except:
        return ''


def get_next_cnt(vals, key):
    # Get next incremental counter to make 'key' unique

    got = False
    new_key = key
    cnt = 0
    while not got:
        try:
            cnt += 1
            new_key = key + '_' + str(cnt)
        except:
            got = True
    return new_key


def read_nameval(dir='', pref='', file='const.status'):
    # pref is a prefix, for instance './Data_run1/prefix_deck.status'
    # Later entries will override earlier ones

    if not dir:
        try:
            dir = sh.get_wkdir()
        except:
            pass

    name = dir + '/' + pref + file

    const_vals = {}
    try:
        with open(name, 'r') as infile:
            for line in infile:
                if line.strip():
                    nv = parse_name_val(line)
                    if nv:
                        const_vals[nv[0]] = nv[1]
    except IOError:
        print('File '+name+' cannot be opened')

    return const_vals


def read_deck_file_all(dir='', pref='', file='deck.status'):

    # pref is a prefix, for instance './Data_run1/prefix_deck.status'
    # This will try and extract all the things mentioned, including those that
    # are strings
    # Duplicates are now possible (because of context) so we add a running
    # counter if required
    # Mostly this is not useful, but it can be helpful sometimes

    if not dir:
        try:
            dir = sh.get_wkdir()
        except:
            pass

    const_vals = {}
    try:
        with open(file, 'r') as infile:
            for line in infile:
                line_seg = extr_handled_ok(line)
                if line_seg.strip():
                    nv = parse_name_val(line_seg, include_strings=True)
                    key = get_next_cnt(const_vals, nv[0])
                    if nv:
                        const_vals[key] = nv[1]
    except:
        pass

    return const_vals


def pretty_print_struct(const):
    from json import dumps

    print(dumps(const, sort_keys=True, indent=4))
