# -*- coding: utf-8 -*-
r"""
CubeMX 点完 GENERATE CODE 之后跑这一条：把"每次都会发生的那几件事"一次查完并修掉。

用法（仓库根目录下）:  python scripts\post_generate.py

六项检查，幂等——没毛病就报 OK 且不动任何文件：
  1. startup_stm32h750xx.s 还在不在           缺了从 git 取回
  2. .uvprojx 里 Startup 组还在不在            缺了插回去（插在 ::CMSIS 组之前）
  3. ffconf.h 的 _USE_MKFS / _USE_STRFUNC 是 0？ 被改回默认就退回上一版
  4. main.c 的 BOM / UTF-8 / 行尾              坏了先从 git 取回，再把这次
                                               CubeMX 该加的 include 与 MX_xxx_Init() 补回
  5. USER CODE 区里的关键代码还在不在
  6. code/Drivers/BSP 下的模块有没有全挂进工程

为什么需要它：CubeMX 的 RTE 里没有 Device:Startup 组件，所以它每次都会删掉启动文件
并丢掉那个组；ffconf.h 每次都按默认重写；它的内嵌 JVM 还可能按 GBK 写回 main.c。
这些由它的设计决定，改不掉，所以用一条幂等命令兜住。
"""
import io
import os
import re
import subprocess
import sys

sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MAIN = os.path.join(REPO, r"code\Core\Src\main.c")
UV = os.path.join(REPO, r"code\MDK-ARM\CabinetGuardian.uvprojx")
FFCONF = os.path.join(REPO, r"code\FATFS\Target\ffconf.h")
STARTUP_REL = r"code/Drivers/CMSIS/Device/ST/STM32H7xx/Source/Templates/arm/startup_stm32h750xx.s"
STARTUP = os.path.join(REPO, STARTUP_REL.replace("/", "\\"))
CRLF = "\r\n"

# 写在 USER CODE 区里的关键代码，少一个就说明又被 CubeMX 吞了
LANDMARKS = ["touch_marker_tick", "touch_poll", "ui_refresh", "w5500_net_up",
             "w5500_watch_dump", "ds18b20_selftest", "ds18b20_poll",
             "i2c_bus_selftest", "i2c_bus_poll", "bsp_log_set"]

failures = []


def git(*args):
    return subprocess.run(["git"] + list(args), cwd=REPO,
                          capture_output=True).stdout.decode("utf-8", "replace")


def report(tag, state, detail=""):
    print("[%-5s] %-28s %s" % (state, tag, detail))
    if state == "FAIL":
        failures.append(tag)


# ---- 1. 启动文件 ----------------------------------------------------------
if os.path.exists(STARTUP):
    report("startup_stm32h750xx.s", "OK")
else:
    git("checkout", "HEAD", "--", STARTUP_REL)
    ok = os.path.exists(STARTUP)
    report("startup_stm32h750xx.s", "FIXED" if ok else "FAIL", "从 git 取回")

# ---- 2. uvprojx 的 Startup 组 --------------------------------------------
uv = open(UV, "rb").read().decode("utf-8", "replace")
if "<GroupName>Startup</GroupName>" in uv:
    report("uvprojx Startup 组", "OK")
else:
    block = (
        "        <Group>\r\n"
        "          <GroupName>Startup</GroupName>\r\n"
        "          <Files>\r\n"
        "            <File>\r\n"
        "              <FileName>startup_stm32h750xx.s</FileName>\r\n"
        "              <FileType>2</FileType>\r\n"
        "              <FilePath>..\\Drivers\\CMSIS\\Device\\ST\\STM32H7xx\\Source\\Templates\\arm\\startup_stm32h750xx.s</FilePath>\r\n"
        "            </File>\r\n"
        "          </Files>\r\n"
        "        </Group>\r\n")
    i = uv.index("<GroupName>::CMSIS</GroupName>")
    j = uv.rindex("        <Group>", 0, i)
    open(UV, "wb").write((uv[:j] + block + uv[j:]).encode("utf-8"))
    uv = open(UV, "rb").read().decode("utf-8", "replace")
    report("uvprojx Startup 组", "FIXED" if "<GroupName>Startup</GroupName>" in uv else "FAIL")

# ---- 3. ffconf.h 的两个开关 ----------------------------------------------
def ffdefine(text, name):
    m = re.search(r"^#define\s+%s\s+([0-9]+)" % name, text, re.M)
    return m.group(1) if m else "?"


raw = open(FFCONF, "rb").read().decode("utf-8", "replace")
bad = [n for n in ("_USE_MKFS", "_USE_STRFUNC") if ffdefine(raw, n) != "0"]
if not bad:
    report("ffconf.h 开关", "OK", "_USE_MKFS=0 _USE_STRFUNC=0")
else:
    git("checkout", "HEAD", "--", "code/FATFS/Target/ffconf.h")
    raw = open(FFCONF, "rb").read().decode("utf-8", "replace")
    left = [n for n in bad if ffdefine(raw, n) != "0"]
    report("ffconf.h 开关", "FIXED" if not left else "FAIL", "退回上一版: " + ",".join(bad))

# ---- 4. main.c 编码 ------------------------------------------------------
cur = open(MAIN, "rb").read()
why = []
if cur[:3] != b"\xef\xbb\xbf":
    why.append("丢 BOM")
try:
    text = cur.decode("utf-8")
    if re.search(r"(?<!\r)\n", text):
        why.append("有裸 LF")
except UnicodeDecodeError:
    why.append("UTF-8 解码失败")

if not why:
    report("main.c 编码", "OK")
else:
    broken = [l.rstrip() for l in cur.decode("ascii", "replace").splitlines()]
    head = git("show", "HEAD:code/Core/Src/main.c").splitlines()
    add_inc = [l for l in broken if l.startswith('#include "') and l not in head]
    add_call = [l for l in broken if "MX_" in l and l.endswith("_Init();") and l not in head]

    git("checkout", "HEAD", "--", "code/Core/Src/main.c")
    lines = open(MAIN, "rb").read().decode("utf-8-sig").replace(CRLF, "\n").split("\n")
    for inc in add_inc:
        lines.insert(max(i for i, l in enumerate(lines) if l.startswith("#include ")) + 1, inc)
    for call in add_call:
        lines.insert(max(i for i, l in enumerate(lines) if "_Init();" in l) + 1, call)

    out = CRLF.join(lines)
    open(MAIN, "wb").write((out if out.endswith(CRLF) else out + CRLF).encode("utf-8-sig"))
    report("main.c 编码", "FIXED", "%s；补回 %s" % ("/".join(why), (add_inc + add_call) or "无"))

# ---- 5. USER CODE 里的关键代码 -------------------------------------------
txt = open(MAIN, "rb").read().decode("utf-8-sig", "replace")
missing = [k for k in LANDMARKS if k not in txt]
report("USER CODE 关键代码", "OK" if not missing else "FAIL",
       "" if not missing else "缺: " + ",".join(missing))

# ---- 6. BSP 模块挂进工程 --------------------------------------------------
bsp = os.path.join(REPO, r"code\Drivers\BSP")
mods = sorted(d for d in os.listdir(bsp) if os.path.isdir(os.path.join(bsp, d)))
uv = open(UV, "rb").read().decode("utf-8", "replace")
nogroup = [m for m in mods if "<GroupName>BSP/%s</GroupName>" % m not in uv]
report("BSP 模块挂进工程", "OK" if not nogroup else "FAIL",
       "%d 个模块" % len(mods) if not nogroup else "没挂上: " + ",".join(nogroup))

print()
if failures:
    print("有 %d 项没搞定: %s —— 先别编译，把这份输出发出来。" % (len(failures), ", ".join(failures)))
    sys.exit(1)
print("全部通过，可以编译了。")
