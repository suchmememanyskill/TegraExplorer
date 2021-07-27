# Copyright (c) 2021 bleck9999
# https://github.com/bleck9999/ts-minifier
# Version: b9ea46f3

import argparse
import re

# if is not included because it's already 2 characters
sub_funcs = {'while': "_h", 'print': "_p", 'println': "_l", 'mountsys': "_s", 'mountemu': "_e", 'readsave': "_r",
             'exit': "_q", 'break': "_b", 'dict': "_d", 'setpixel': "_y", 'readdir': "_i", 'copyfile': "_c",
             'mkdir': "_k", 'memory': "_m", 'ncatype': "_n", 'pause': "_w", 'color': "_a", 'menu': "__", 'emu': "_u",
             'clear': "_x", 'timer': "_t", 'deldir': "_g", 'fsexists': "_f", 'delfile': "_z", "copydir": "c_",
             "movefile": "_v", "payload": "_j", "readfile": "_o", "writefile": "w_", "setpixels": "y_", "printpos": "p_",
             "emmcread": "e_", "emmcwrite": "f_", "emummcread": "r_", "emummcwrite": "s_", "escapepath": "x_",
             "combinepath": "a_"}
replace_functions = False


def wantsumspace(s: str):
    for c in s.lower():
        if (ord(c) < 97 or ord(c) > 122) and (ord(c) != 95) and not (c.isnumeric()):
            return False
    return True


def commentstartswhere(s: str):
    quoted = False
    for c in range(len(s)):
        if s[c] == '"':
            quoted = not quoted
        if s[c] == '#' and not quoted:
            return c
    return None


def minify(script: str):
    # currently ts does not seem to allow 's to mark a quote
    # (https://github.com/suchmememanyskill/TegraExplorer/blob/tsv3/source/script/parser.c#L173)
    # im fine with that, it makes doing this a lot easier
    # strings = script.split(sep='"')
    str_reuse = {}
    requires = ""
    mcode = ""
    stl_counts = {}.fromkeys(sub_funcs, 0)
    # while part < len(strings):
    for line in script.split(sep='\n'):
        # maybe in future it'll shrink user defined names
        # dont hold out hope for that because `a.files.foreach("b") {println(b)}` is valid syntax
        # and i dont have the skill or patience to deal with that

        # # in theory all the even numbered indexes should be outside quotes, so we ignore any parts with an odd index
        # if part % 2 == 1:
        #     if strings[part] not in str_reuse:
        #         str_reuse[strings[part]] = 0
        #     else:
        #         str_reuse[strings[part]] += 1
        #     mcode += f'"{strings[part]}"'
        start = commentstartswhere(line)
        if start is None:
            start = -1

        if "REQUIRE " in line[start:]:
            requires += line[start:] + '\n'  # leave REQUIREs unmodified
            # comments are terminated by a newline so we need to add one back in

        # *deep breath*
        # slicing is exclusive on the right side of the colon so the "no comment" value of start=-1 would cut off
        # the last character of the line which would lead to several issues
        # however this is desirable when there *is* a comment, since it being exclusive means there isn't a trailing #
        # and if you're wondering about the above check that uses line[start:] this doesn't matter,
        # one character cant contain an 8 character substring
        if start != -1:
            line = line[:start]
        line = line.split(sep='"')

        if len(line) % 2 == 0:
            print("You appear to have string literals spanning multiple lines. Please seek professional help")
            raise Exception("Too much hatred")
        part = 0
        while part < len(line):
            # all the odd numbered indexes should be inside quotes
            if part % 2 == 0:
                if not line[part]:
                    break
                for s in sub_funcs:
                    stl_counts[s] += len(re.findall("(?<!\\.)%s\\(" % s, line[part]))
                mcode += line[part].replace('\t', '') + ' '
            else:
                if line[part] not in str_reuse:
                    str_reuse[line[part]] = 0
                else:
                    str_reuse[line[part]] += 1
                mcode += f'"{line[part]}"'

            part += 1


    # tsv3 is still an absolute nightmare
    # so spaces have a couple edge cases
    # 1. the - operator which requires space between the right operand
    # yeah that's right only the right one
    # thanks meme
    # 2. between 2 letters
    inquote = False
    mmcode = ""
    index = 0
    newline = list(mcode)
    while index < (len(mcode) - 3):
        sec = mcode[index:index + 3]
        if not inquote and sec[1] == '"':
            inquote = True
        elif inquote and sec[1] == '"':
            inquote = False
        if (sec[1] == ' ') and not inquote:
            if wantsumspace(sec[0]) and wantsumspace(sec[2]):
                pass
            elif sec[0] == '-' and sec[2].isnumeric():
                pass
            else:
                newline[index + 1] = ''
        index += 1
    mmcode += ''.join(newline).strip()

    for func in sub_funcs:
        # space saved here is given by n * (len(func) - len(min_func)) - (len(min_func)+1 + len(func))
        # as such with one usage space is always lost (len(func)-2 is never > len(func)+3) so dont even try
        if stl_counts[func] >= 2:
            savings = stl_counts[func] * (len(func) - 2) - (len(func) + 3)
            print(f"Replacing all {stl_counts[func]} usages of {func} would save {savings}byte{'s' if savings != 1 else ''}")
            if (savings < 0) or not replace_functions:
                print("Savings negative or automatic replacement disabled, continuing")
                continue
            func_min = sub_funcs[func]  # now here we have to assume nobody is using any of our substitute vars
            # should be a pretty safe assumption but knowing for sure would require about the same amount of effort
            # as it would to replace all user defined variables
            ucode = ""  # this is rather hacky
            sections = [0]
            for m in re.finditer(r"(?<!\.)%s\(" % func, mmcode):
                sections.append(m.span()[0])
                sections.append(m.span()[1])
            sections.append(len(mmcode))
            i = 2       # change rather to very
            while i < len(sections):
                ucode += mmcode[sections[i-2]:sections[i-1]] + func_min + '('
                i += 2
            ucode += mmcode[sections[i-2]:]

            ucode = f"{func_min}={func}\n" + ucode
            mmcode = ucode
            # a space isn't any shorter than \n so why not use \n

    for string, count in str_reuse.items():
        if count >= 2:
            # we can't auto replace strings without a full parser
            # unlike with the stdlib functions we cant make a lookup table ahead of time
            # and generating shorter names on the fly sounds like an absolute nightmare no thanks
            print(f'Warning: string "{string}" of len {len(string)} reused {count} times')

    return requires + mmcode.strip()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="Minify tsv3 scripts, useful for embedding",
                                     formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument("source", type=str, nargs='+', help="source files to minify")
    parser.add_argument("-d", type=str, nargs='?', help="destination folder for minified scripts"
                                                        "\ndefault: ./", default='./')
    parser.add_argument("--replace-functions", action="store_true", default=False,
                        help="automatically replace reused functions instead of just warning\ndefault: false")

    args = parser.parse_args()
    files = args.source
    dest = args.d[:-1] if args.d[-1] == '/' else args.d
    replace_functions = args.replace_functions if args.replace_functions is not None else False

    for file in files:
        print(f"Minifying {file}")
        with open(file, 'r') as f:
            r = minify(f.read())
        file = file.split(sep='.')[0].split(sep='/')[-1]
        if dest != '.':
            f = open(f"{dest}/{file}.te", 'w')
        else:
            f = open(f"{dest}/{file}_min.te", 'w')
        f.write(r)
