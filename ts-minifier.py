# Copyright (c) 2021 bleck9999
# https://github.com/bleck9999/ts-minifier
# Version: 1095d14a

import argparse
import itertools
from string import ascii_letters

auto_replace = False
stdlib = ['if', 'while', 'print', 'println', 'mountsys', 'mountemu', 'readsave', 'exit', 'break', 'dict', 'setpixel',
          'readdir', 'copyfile', 'mkdir', 'ncatype', 'pause', 'color', 'menu', 'emu', 'clear', 'timer', 'deldir',
          'fsexists', 'delfile', 'copydir', 'movefile', 'payload', 'readfile', 'writefile', 'setpixels', 'printpos',
          'emmcread', 'emmcwrite', 'emummcread', 'emummcwrite', 'escapepath', 'combinepath', 'cwd', 'power',
          'fuse_patched', 'fuse_hwtype']


class Code:
    def __init__(self, strings, comments, script):
        counter = 0
        strings_comments = sorted(strings + comments)
        bounds = [0] if strings_comments[0][0] != 0 else []
        for val in strings_comments:
            if counter and (bounds[counter - 1] == val[0]):
                bounds[counter - 1] = val[1]
            else:
                bounds += [val[0], val[1]]
                counter += 2
        bounds.append(len(script))
        code = []
        i = 2 if len(bounds) % 2 else 1
        while i < len(bounds):
            code.append((bounds[i - 1], bounds[i], script[bounds[i - 1]:bounds[i]]))
            i += 2
        self.sections = sorted(strings_comments + code)
        self.strings = strings
        self.comments = comments
        self.code = code
        self.rawcode = "".join([x[2] for x in sorted(self.code+self.strings)])

    def getafter(self, ch: int):
        ch += self.comments[-1][1] if self.comments else 0
        for strcom in self.strings:
            if strcom[0] >= ch:
                return strcom
        return None

    def nextch(self, ch: int, reverse: bool):
        rawcontent = self.rawcode
        if ((ch+1 >= len(rawcontent)) and not reverse) or \
                ((ch-1 < 0) and reverse):
            return ''
        return rawcontent[ch-1] if reverse else rawcontent[ch+1]


def isidentifier(s: str):
    for c in s:
        if c not in (ascii_letters + '_'):
            return False
    return True


def hascomment(s: str):
    quoted = False
    for c in range(len(s)):
        if s[c] == '"':
            quoted = not quoted
        if s[c] == '#' and not quoted:
            return c
    return None


def parser(script: str):
    comments = []  # [(start, end, content)]
    strings = []
    commented = False
    quoted = False
    strstart = -1
    commentstart = -1
    for c in range(len(script)):
        if script[c] == '#' and not quoted:
            commented = True
            commentstart = c
        elif (script[c] == '\n' and not quoted) and commented:
            comments.append((commentstart, c + 1, script[commentstart:c + 1]))
            commented = False
        elif script[c] == '"' and not commented:
            if not quoted:
                strstart = c
                quoted = True
            else:
                strings.append((strstart, c + 1, script[strstart:c + 1]))
                quoted = False

    script = Code(strings, comments, script)

    # couple notes:
    # we minify the script a little before parsing, so there is no unnecessary whitespace or comments
    # we are assuming the input script is valid syntax
    userobjects = {}
    usages = {}
    hexxed = False
    ismember = False
    quoted = False
    strscript = script.rawcode
    start = len(strscript) + 1
    for ch in range(len(strscript)):
        if (strscript[ch-1] == '0' and strscript[ch] == 'x') and not quoted:
            hexxed = True
        elif isidentifier(strscript[ch]) and not (hexxed or quoted):
            if start > ch:
                start = ch
            else:
                pass
        elif hexxed and strscript[ch].upper() not in "0123456789ABCDEF":
            hexxed = False
        elif strscript[ch] == '"':
            quoted = not quoted
        elif not quoted:
            if start != len(strscript)+1:  # if we actually had an identifier before this char
                identifier = strscript[start:ch]
                if identifier in usages:
                    usages[identifier].append(start)
                elif strscript[ch] == '=' and strscript[ch+1] != '=':
                    isfunc = script.nextch(ch, False) == '{'
                    userobjects[identifier] = "func" if isfunc else "var"
                    usages[identifier] = [start]  # declaration is a usage because i cant be arsed
                elif not ismember:  # not an assignment (or member) but also haven't seen this name before
                    usages[identifier] = [start]
                    # fuck it we are using a fucking list of fucking stdlib functions i just fucking cant im adding tsv3
                    # to the fucking esolangs wiki have a good day
                    if identifier not in stdlib:
                        userobjects[identifier] = "var"
            if strscript[ch] == '.':
                ismember = True
            elif strscript[ch] == '(':
                if ismember:
                    if "foreach" == strscript[start:ch]:  # array.foreach takes a variable name as an arg (blame meme)
                        name = script.getafter(ch)[2].replace('"', '')
                        if name in userobjects:
                            usages[name].append(start)
                        else:
                            # this is fucking disgusting
                            usages[name] = [script.getafter(ch)[0] + 1   # start index of the quote, +1 to account for "
                                            - (script.comments[-1][1]    # correct for strscript removing comments
                                            if script.comments else 0)]  # (if any are present at all)
                            userobjects[name] = "var"
                    else:
                        pass
            elif strscript[ch] == ')':
                ismember = script.nextch(ch, False) == '.'
            start = len(strscript) + 1

    return minify(script, userobjects, usages)


def minify(script: Code, userobjects, usages):
    # the space saved by an alias is the amount of characters currently used by calling the function (uses*len(func))
    # minus the amount of characters it would take to define an alias (len(alias)+len(func)+2), with the 2 being the
    # equals and the whitespace needed for a definition
    #
    # obviously for a rename you're already defining it so it's just the difference between lengths multiplied by uses
    short_idents = [x for x in (ascii_letters+'_')] + [x[0]+x[1] for x in itertools.product(ascii_letters+'_', repeat=2)]
    short_idents.pop(short_idents.index("if"))
    mcode = script.rawcode
    aliases = []
    for uo in [x for x in userobjects]:
        if userobjects[uo] not in ["var", "func"]:
            continue
        tmpcode = ""
        otype = userobjects[uo]
        uses = len(usages[uo])
        uolen = len(uo)
        if uolen > 1:
            candidates = short_idents
            minName = ''
            if uolen == 2:
                candidates = short_idents[:53]
            for i in candidates:
                if i not in userobjects:
                    minName = i
                    userobjects[minName] = "TRN"
                    break
            if not minName:
                print(f"{'Function' if otype == 'func' else 'Variable'} name {uo} could be shortened but no available "
                      f"names found (would save {uses} bytes)")
                continue
                # we assume that nobody is insane enough to exhaust all *2,808* 2 character names,
                # instead that uo is len 2 and all the 1 character names are in use (because of that we dont multiply
                # uses by anything as multiplying by a difference of 1 would be redundant)
            if not auto_replace:
                print(f"{'Function' if otype == 'func' else 'Variable'} name {uo} could be shortened ({uo}->{minName}, "
                      f"would save {uses*(uolen - len(minName))} bytes")
                continue
            else:
                print(f"Renaming {'Function' if otype == 'func' else 'Variable'} {uo} to {minName} "
                      f"(saving {uses*(uolen - len(minName))} bytes)")
                # rather than just blindly str.replace()ing we're going to actually use the character indices that we stored
                diff = uolen - len(minName)
                prev = 0
                # we're specifically looking for variables declared with the .foreach bullshit so we can ignore any functions
                if otype == "var":
                    for i, string in enumerate([x for x in script.strings]):
                        if string[2] == f'"{uo}"':
                            # you might think im forgetting to account for the shorter minName by leaving string[1]
                            # but you are wrong this is intentional
                            script.strings[i] = (string[0], string[1], f'"{minName}"')
                for bound in usages[uo]:
                    tmpcode += mcode[prev:bound] + minName + ' '*diff
                    prev = bound + diff + len(minName)
                # actually shut up about "bound might be referenced before assignment" or show me what possible
                # execution path that could lead to usages[uo] being an empty list
                mcode = tmpcode + mcode[bound+diff+len(minName):]
    for func in usages:
        tmpcode = ""
        candidates = short_idents
        minName = ''
        savings = 0
        uses = len(usages[func])
        if func in userobjects or uses < 2:  # we only want stdlib functions used more than once
            continue
        elif func == "if":
            candidates = short_idents[:53]
            savings = uses * 2 - 5  # the 5 is how many characters an alias declaration would use (a=if<space>)
        for i in candidates:
            if i not in userobjects:
                minName = i
                userobjects[minName] = "TRP"
                break
        # once again we assume it's only `if` that could trigger this message
        if not minName and (uses - 4) > 0:
            print(f"Standard library function {func} could be aliased but no available names found "
                  f"(would save {uses-4} bytes)")
        else:
            if not savings:
                savings = uses*len(func) - (len(func)+len(minName)+2)
            if savings <= 0 or not auto_replace:
                print(f"Not aliasing standard library function {func} (would save {savings} bytes)")
            else:
                print(f"Aliasing standard library function {func} to {minName} (saving {savings} bytes)")
                diff = len(func) - len(minName)
                prev = 0
                for bound in usages[func]:
                    tmpcode += mcode[prev:bound] + minName + ' ' * diff
                    prev = bound + diff + len(minName)
                mcode = tmpcode + mcode[bound + diff + len(minName):]
                aliases.append(f"{minName}={func} ")

    str_reuse = {}
    for string in script.strings:
        if string[2] in str_reuse:
            str_reuse[string[2]].append(string[0])
        else:
            str_reuse[string[2]] = [string[0]]
    for string in script.strings:
        tmpcode = ""
        candidates = short_idents
        minName = ""
        uses = len(str_reuse[string[2]])
        if auto_replace and (string[2].replace('"', '') in userobjects) and \
                (userobjects[string[2].replace('"', '')] == "var"):
            start = string[0] - (script.comments[-1][1] if script.comments else 0)
            end = string[1] - (script.comments[-1][1] if script.comments else 0)
            # newend is essentially start + len(minName) + 1 (+1 because we only exclude the trailing ")
            newend = start + len(string[2]) - 1
            if end == newend+1:
                # there are in theory two possible reasons for this
                # 1. minName and the original name of the replaced variable are the same length
                # 2. this string literal just happens to have the same content as a variable
                # however option 1 shouldn't happen because it shouldn't try to replace a variable if it doesn't save
                # any space, because of this we know it's option 2 and we should do nothing
                continue
            # you might be wondering why the +1 and -1 are there
            # so am i but removing them breaks things and i spent like 30 minutes trying to get this to work
            tmpcode = mcode[:newend] + '"' + mcode[newend+1:]
            tmpcode = tmpcode[:end-1] + ' ' + tmpcode[end:]
            mcode = tmpcode
        elif uses > 1 and len(string[2]) > 1:
            string = string[2]
            if len(string) == 2:
                candidates = short_idents[:53]
            for i in candidates:
                if i not in userobjects:
                    minName = i
                    userobjects[minName] = "TIV"
                    break
            # the quotation marks are included in string
            savings = uses * len(string) - (len(string) + len(minName) + 2)
            if savings <= 0 or not auto_replace:
                print(f"Not introducing variable for string {string} reused {uses} times (would save {savings} bytes)")
            else:
                # "duplicated code fragment" do i look like i give a shit
                print(f"Introducing variable {minName} with value {string} (saving {savings} bytes)")
                diff = len(string) - len(minName)
                prev = 0
                for bound in str_reuse[string]:
                    bound -= script.comments[-1][1] if script.comments else 0
                    tmpcode += mcode[prev:bound] + minName + ' ' * diff
                    prev = bound + diff + len(minName)
                mcode = tmpcode + mcode[bound + diff + len(minName):]
                aliases.append(f"{minName}={string}")

    print("Reintroducing REQUIREs")
    mcode = "".join([x[2] for x in script.comments]) + "".join(aliases) + mcode
    print("Stripping whitespace")
    return whitespacent(mcode)


def whitespacent(script: str):
    # also removes unneeded comments and push REQUIREs to the top of the file
    requires = ""
    mcode = ""
    for line in script.split(sep='\n'):
        start = hascomment(line)
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
        # one character cant contain an 8 character substring so it's not like it'll ever false positive
        if start != -1:
            line = line[:start]
        line = line.split(sep='"')

        if len(line) % 2 == 0:
            raise Exception("Unmatched quote or hard newline in string")
        part = 0
        while part < len(line):
            # all the odd numbered indexes should be inside quotes
            if part % 2 == 0:
                if not line[part]:
                    break
                # turn lots of whitespace into one whitespace with one easy trick!
                mcode += ' '.join(line[part].split()) + ' '
            else:
                mcode += f'"{line[part]}"'

            part += 1

    # tsv3 is still an absolute nightmare
    # so spaces have a couple edge cases
    # 1. the - operator which requires space between the right operand
    # yeah that's right only the right one
    # thanks meme
    # 2. between 2 characters that are either valid identifiers (aA-zZ or _) or integers
    inquote = False
    mmcode = ""
    index = 0
    newline = list(mcode)
    while index < (len(mcode) - 3):
        sec = mcode[index:index + 3]
        if sec[1] == '"':
            inquote = not inquote
        if (sec[1] == ' ') and not inquote:
            if (isidentifier(sec[0]) or sec[0].isnumeric()) and (isidentifier(sec[2]) or sec[2].isnumeric()):
                pass
            elif sec[0] == '-' and sec[2].isnumeric():
                pass
            else:
                newline[index + 1] = ''
        index += 1
    mmcode += ''.join(newline).strip()

    return requires + mmcode.strip().replace('\n', ' ')


if __name__ == '__main__':
    argparser = argparse.ArgumentParser(description="Minify tsv3 scripts, useful for embedding",
                                        formatter_class=argparse.RawTextHelpFormatter)
    argparser.add_argument("source", type=str, nargs='+', help="source files to minify")
    argparser.add_argument("-d", type=str, nargs='?', help="destination folder for minified scripts"
                                                           "\ndefault: ./", default='./')
    argparser.add_argument("--auto-replace", action="store_true", default=False,
                           help="automatically replace reused functions and variables instead of just warning\n"
                           "and attempt to generate shorter names for reused variables \ndefault: false")

    args = argparser.parse_args()
    files = args.source
    dest = args.d[:-1] if args.d[-1] == '/' else args.d
    auto_replace = args.auto_replace if args.auto_replace is not None else False

    for file in files:
        print(f"\nMinifying {file}")
        with open(file, 'r') as f:
            print("Stripping comments")
            r = parser(whitespacent(f.read()))
        file = file.split(sep='.')[0].split(sep='/')[-1]
        if dest != '.':
            f = open(f"{dest}/{file}.te", 'w')
        else:
            f = open(f"{dest}/{file}_min.te", 'w')
        f.write(r)
