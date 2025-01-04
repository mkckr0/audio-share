import csv
from pathlib import PurePath
import sys

try:
    strings_path = sys.argv[1]
    src = sys.argv[2]
    dst_dir = sys.argv[3]
except:
    print("arg error, usage: generate_rc.py strings.csv src.rc dst_dir")
    exit()

class Language:
    name: str
    primary_lang: str
    sub_lang: str
    comment: str
    str_map: dict[str, str]
    
    def __init__(self, name: str, primary_lang: str, sub_lang: str, comment: str):
        self.name = name
        self.primary_lang = primary_lang
        self.sub_lang = sub_lang
        self.comment = comment
        self.str_map = {}

# read csv data
csv_array = []
with open(strings_path, "r", encoding="utf_8_sig") as file:
    reader = csv.reader(file)
    for row_index, row in enumerate(reader):
        csv_array.append(row)

# build lang map
lang_map: dict[str, Language] = {}
for idx, name in enumerate(csv_array[0][1:]):
    col = idx + 1
    lang = Language(name, csv_array[1][col], csv_array[2][col], csv_array[3][col])
    for idx in range(5, len(csv_array)):
        en = csv_array[idx][1]
        other = csv_array[idx][col]
        if en != '#':
            lang.str_map[en] = other
    lang_map[lang.name] = lang

# read en resource
with open(src, "r", encoding="utf8") as file:
    base_rc = file.read()

en_US = lang_map["en-US"]
for lang in lang_map.values():
    if lang.name == 'en-US':
        continue
    trans_rc = base_rc
    trans_rc = trans_rc.replace(f"{en_US.primary_lang}, {en_US.sub_lang}", f"{lang.primary_lang}, {lang.sub_lang}")
    trans_rc = trans_rc.replace(en_US.comment, lang.comment)
    for en, target in lang.str_map.items():
        trans_rc = trans_rc.replace(f'"{en}"', f'"{target}"')
    dst = PurePath(dst_dir).joinpath(f"{PurePath(src).stem}_{lang.name}.rc")
    with open(dst, "w", encoding="utf8") as file:
        file.write(trans_rc)
    print(f"{dst}")