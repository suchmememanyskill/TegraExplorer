# Copyright (c) 2021 bleck9999
# https://github.com/bleck9999/ts-minifier
# Version: 339183b2

import argparse
import itertools
import logging
from os import path
from string import ascii_letters, digits, hexdigits

auto_replace = False
verbose = False
stdlib = ['if', 'while', 'print', 'println', 'mountsys', 'mountemu', 'readsave', 'exit', 'break', 'dict', 'setpixel',
          'readdir', 'copyfile', 'mkdir', 'ncatype', 'pause', 'color', 'menu', 'emu', 'clear', 'timer', 'deldir',
          'fsexists', 'delfile', 'copydir', 'movefile', 'payload', 'readfile', 'writefile', 'setpixels', 'printpos',
          'emmcread', 'emmcwrite', 'emummcread', 'emummcwrite', 'escapepath', 'combinepath', 'cwd', 'power',
          'fuse_patched', 'fuse_hwtype']


class Code:
    def __init__(self, strings, comments, script):
        counter = 0
        strings_comments = sorted(strings + comments)

        bound = 0
        code = []
        for strcom in strings_comments:
            code.append((bound, strcom[0], script[bound:strcom[0]]))
            bound = strcom[1]
        code.append((bound, len(script), script[bound:]))
        self.sections = sorted(strings_comments + code)
        self.strings = strings
        self.comments = comments
        self.code = code
        self.varstrs = []
        self.rawcode = "".join([x[2] for x in sorted(self.code + self.strings)])


def isidentifier(s: str):
    for c in s:
        if c not in (ascii_letters + '_' + digits):
            return False
    return True


def iswhitespace(s: str):
    for c in s:
        if c not in (' ', '\t', '\n'):
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
    strscript = script.rawcode + ' '
    # the space will get removed after the second pass of whitespacent, but for now it prevents not detecting the
    # last identifier in a script (eg if script.rawcode was "a=12" the 12 wouldn't be detected without the trailing ' ')
    start = len(strscript) + 1
    for ch in range(len(strscript)):
        if (strscript[ch - 1] == '0' and strscript[ch] == 'x') and not quoted:
            hexxed = True
        elif isidentifier(strscript[ch]) and not (hexxed or quoted):
            if start > ch:
                start = ch
            else:
                pass
        elif hexxed and strscript[ch] not in hexdigits:
            hexxed = False
        elif strscript[ch] == '"':
            quoted = not quoted
        elif not quoted:
            if start != len(strscript) + 1 and not ismember:  # if we actually had an identifier before this char
                identifier = strscript[start:ch]  # and this isnt a member of anything
                if identifier in usages:
                    usages[identifier].append(start)
                elif identifier.isnumeric():  # numbers are legally valid identifiers because fuckyou
                    usages[identifier] = [start]
                    userobjects[identifier] = "INT"
                elif identifier == "0x":
                    pass
                elif strscript[ch] == '=' and strscript[ch + 1] != '=':
                    isfunc = strscript[ch + 1] == '{'
                    userobjects[identifier] = "func" if isfunc else "var"
                    usages[identifier] = [start]  # declaration is a usage because i cant be arsed
                else:  # not an assignment (or member) but also haven't seen this name before
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
                        for i, string in enumerate(script.strings):
                            if string[0] == ch + (script.comments[-1][1] if script.comments else 0) + 1:
                                script.varstrs.append(string)
                                script.strings.pop(i)
                                break
                    else:
                        ismember = False
                        pass
            elif strscript[ch] in ')}]':
                ismember = strscript[ch + 1] == '.'
            start = len(strscript) + 1

    return script, userobjects, usages


def minify(script: Code, userobjects, usages):
    # the space saved by an alias is the amount of characters currently used by calling the function (uses*len(func))
    # minus the amount of characters it would take to define an alias (len(alias)+len(func)+2), with the 2 being for the
    # equals and the whitespace needed for a definition
    # the same principle also applies to introducing a variable for string literals, though since a literal requires
    # having "s around it then it's uses*(len(str)+2) - (len(minName)+len(str)+4)
    #                                                                          ^ 2 for = and whitespace, 2 for ""
    #
    # obviously for a rename you're already defining it so it's just the difference between lengths multiplied by uses
    short_idents = [x for x in (ascii_letters + '_')] + [x[0] + x[1] for x in
                                                         itertools.product(ascii_letters + '_', repeat=2)]
    short_idents.pop(short_idents.index("if"))
    mcode = script.rawcode
    aliases = []
    logging.info("Renaming user functions and variables" if auto_replace else
                 "Checking user function and variable names")
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
                logging.info(f"{'Function' if otype == 'func' else 'Variable'} name {uo} could be shortened but "
                             f"no available names found (would save {uses} bytes)")
                continue
                # we assume that nobody is insane enough to exhaust all *2,808* 2 character names,
                # instead that uo is len 2 and all the 1 character names are in use (because of that we dont multiply
                # uses by anything as multiplying by a difference of 1 would be redundant)
            if not auto_replace:
                logging.warning(
                    f"{'Function' if otype == 'func' else 'Variable'} name {uo} could be shortened ({uo}->{minName}, "
                    f"would save {uses * (uolen - len(minName))} bytes)")
                continue
            else:
                logging.info(f"Renaming {'Function' if otype == 'func' else 'Variable'} {uo} to {minName} "
                             f"(saving {uses * (uolen - len(minName))} bytes)")
                diff = uolen - len(minName)

                # the foreach syntax is literally the worst part of ts
                if otype == "var":
                    struo = f'"{uo}"'
                    for varstr in script.varstrs:
                        if varstr[2] == struo:
                            logging.info(f"Replacing declaration of {varstr[2]} at {varstr[0]}-{varstr[1]}")
                            start = varstr[0] - (script.comments[-1][1] if script.comments else 0)
                            end = varstr[1] - (script.comments[-1][1] if script.comments else 0)
                            newend = start + len(minName)
                            mcode = mcode[:newend] + f'{minName}"' + (' ' * diff) + mcode[end:]

                # rather than just blindly str.replace()ing we're going to actually use the character indices that we stored
                prev = 0
                for bound in usages[uo]:
                    tmpcode += mcode[prev:bound] + minName + ' ' * diff
                    prev = bound + diff + len(minName)
                # actually shut up about "bound might be referenced before assignment" or show me what possible
                # execution path that could lead to usages[uo] being an empty list
                mcode = tmpcode + mcode[bound + diff + len(minName):]
    logging.info("Aliasing standard library functions" if auto_replace else
                 "Checking for standard library aliases")
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
                break
        # once again we assume it's only `if` that could trigger this message
        # 4 is the minimum amount of uses needed to save space, 1*(uses - 4) is the space it would save
        if not minName and uses > 4:
            logging.info(f"Standard library function {func} could be aliased but no available names found "
                         f"(would save {uses - 4} bytes)")
        else:
            if not savings:
                savings = uses * len(func) - (len(func) + len(minName) + 2)
            if savings > 0:
                userobjects[minName] = "TRP"
                if auto_replace:
                    logging.info(f"Aliasing standard library function {func} to {minName} (saving {savings} bytes)")
                    diff = len(func) - len(minName)
                    prev = 0
                    for bound in usages[func]:
                        tmpcode += mcode[prev:bound] + minName + ' ' * diff
                        prev = bound + diff + len(minName)
                    mcode = tmpcode + mcode[bound + diff + len(minName):]
                    aliases.append(f"{minName}={func} ")
                else:
                    logging.warning(f"Not aliasing standard library function {func} (would save {savings} bytes)")
            else:
                logging.info(f"Not aliasing standard library function {func} (would save {savings} bytes)")

    str_reuse = {}
    for string in script.strings:
        if string[2] in str_reuse:
            str_reuse[string[2]].append(string[0])
        else:
            str_reuse[string[2]] = [string[0]]
    logging.info("Introducing variables for reused literals" if auto_replace else
                 "Checking for reused literals")
    for string in str_reuse:
        tmpcode = ""
        candidates = short_idents
        minName = ""
        uses = len(str_reuse[string])
        if uses > 1:
            if len(string) == 2:
                candidates = short_idents[:53]
            for i in candidates:
                if i not in userobjects:
                    minName = i
                    break
            if not minName:
                savings = len(string) * uses - (len(string) + 5)  # 5 comes from id="{string}"
                logging.info(f"Could introduce variable for reused string {string} but no available names found "
                             f"(would save {savings} bytes)")
                continue
            # the quotation marks are included in string
            savings = uses * len(string) - (len(string) + len(minName) + 2)
            if savings > 0:
                userobjects[minName] = "TIV"
                if auto_replace:
                    # "duplicated code fragment" do i look like i give a shit
                    logging.info(f"Introducing variable {minName} with value {string} (saving {savings} bytes)")
                    diff = len(string) - len(minName)
                    prev = 0
                    for bound in str_reuse[string]:
                        bound -= script.comments[-1][1] if script.comments else 0
                        tmpcode += mcode[prev:bound] + minName + ' ' * diff
                        prev = bound + diff + len(minName)
                    mcode = tmpcode + mcode[bound + diff + len(minName):]
                    aliases.append(f"{minName}={string}")
                else:
                    logging.warning(f"Not introducing variable for string {string} reused {uses} times "
                                    f"(would save {savings} bytes)")
            else:
                logging.info(f"Not introducing variable for string {string} reused {uses} times "
                             f"(would save {savings} bytes)")
        else:
            logging.info(f"Not introducing variable for string {string} (only used once)")

    for uint in [x for x in userobjects]:
        if userobjects[uint] != "INT" or len(uint) < 2:
            continue
        candidates = short_idents
        uses = len(usages[uint])
        uilen = len(uint)
        minName = ""
        tmpcode = ""
        if uses > 1:
            if uilen == 2:
                candidates = short_idents[:53]
            for i in candidates:
                if i not in userobjects:
                    minName = i
                    break
            if not minName:
                # yet another case of "nobody could possibly use up all the 2 char names we hope"
                savings = uilen * uses - (uilen + 4)  # 4 comes from id={uint}<whitespace>
                logging.info(f"Could introduce variable for reused integer {uint} but no available names found "
                             f"(would save {savings} bytes)")
                continue
            savings = uilen * uses - (uilen + len(minName) + 2)
            if savings > 0:
                userobjects[minName] = "TIV"
                if auto_replace:
                    logging.info(f"Introducing variable {minName} with value {uint} (saving {savings} bytes)")
                    diff = len(uint) - len(minName)
                    prev = 0
                    for bound in usages[uint]:
                        tmpcode += mcode[prev:bound] + minName + ' ' * diff
                        prev = bound + diff + len(minName)
                    mcode = tmpcode + mcode[bound + diff + len(minName):]
                    aliases.append(f"{minName}={uint} ")
                else:
                    logging.warning(f"Not introducing variable for string {uint} reused {uses} times "
                                    f"(would save {savings} bytes)")
            else:
                logging.info(f"Not introducing variable for string {uint} reused {uses} times "
                             f"(would save {savings} bytes)")
        else:
            logging.info(f"Not introducing variable for int {uint} (only used once)")

    logging.info("Reintroducing REQUIREs")
    mcode = "".join([x[2] for x in script.comments]) + "".join(aliases) + mcode
    return mcode


def whitespacent(script: str):
    # also removes unneeded comments and pushes REQUIREs to the top of the file
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
                if line[part] and not iswhitespace(line[part]):
                    # turn lots of whitespace into one whitespace with one easy trick!
                    mcode += ' '.join(line[part].split()) + ' '
            else:
                mcode += f'"{line[part]}"'

            part += 1

    # tsv3 is still an absolute nightmare
    # so spaces should be preserved under two situations
    # 1. the subtraction operator which requires space between the right operand but only if the right operand is a literal
    # 2. between 2 characters that are valid identifiers (aA-zZ, _ or integers)
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
                                                           "\ndefault: .", default='.')
    argparser.add_argument("--auto-replace", action="store_true", default=False,
                           help="automatically replace reused functions, variables and strings instead of just warning\n"
                                "and attempt to generate shorter names for reused variables \ndefault: false")
    argparser.add_argument("-v", action="store_true", default=False,
                           help="prints even more information to the console than usual")

    args = argparser.parse_args()
    files = args.source
    dest = args.d
    auto_replace = args.auto_replace
    verbose = "INFO" if args.v else "WARNING"
    logging.basicConfig(level=verbose, format="{message}", style='{')

    print(f"Automatic replacement: {'ENABLED' if auto_replace else 'DISABLED'}")
    for file in files:
        print(f"\nMinifying {file}")
        with open(file, 'r') as f:
            logging.info("Stripping comments and whitespace (pass 1)")
            r = whitespacent(f.read())
            logging.info("Parsing file")
            r = parser(r)
            logging.info("Searching for optimisations")
            r = minify(r[0], r[1], r[2])
            logging.info("Stripping whitespace (pass 2)")
            r = whitespacent(r)
        file = path.splitext(path.basename(file))[0]
        if path.exists(path.join(dest, f"{file}.te")):
            f = open(path.join(dest, f"{file}_min.te"), 'w')
        else:
            f = open(path.join(dest, f"{file}.te"), 'w')
        logging.info(f"Writing to {f.name}")
        f.write(r)
        print("Done!")
        f.close()
