# -*- coding: utf-8 -*-
"""把 docs/技术说明-*.md 转成 Word（.docx）。

支持的子集：标题(#/##/###)、段落、无序列表(-)、有序列表(1.)、表格(| |)、
代码块(```)、图片(![alt](path))、引用(>)、分隔线(---)、行内 **粗体** 与 `代码`。

不引第三方 markdown 库，就这点语法，自己解析更可控。
"""
import os
import re
import sys
import io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")

from docx import Document
from docx.shared import Pt, Cm, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.oxml.ns import qn
from docx.oxml import OxmlElement

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MD = os.path.join(ROOT, "docs", "技术说明-2026-10-20.md")
OUT = os.path.join(ROOT, "docs", "技术说明-2026-10-20.docx")

BODY_FONT_CN = "微软雅黑"
BODY_FONT_EN = "Consolas"      # 代码块
MONO = "Consolas"


def set_font(run, size=None, bold=None, cn=BODY_FONT_CN, en=None, color=None):
    run.font.name = en or cn
    run._element.rPr.rFonts.set(qn("w:eastAsia"), cn)
    if size is not None:
        run.font.size = Pt(size)
    if bold is not None:
        run.font.bold = bold
    if color is not None:
        run.font.color.rgb = RGBColor(*color)


def add_page_number_footer(section):
    p = section.footer.paragraphs[0]
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    run = p.add_run()
    for tag, text in (("begin", None), (None, "PAGE"), ("end", None)):
        if tag:
            el = OxmlElement("w:fldChar")
            el.set(qn("w:fldCharType"), tag)
            run._r.append(el)
        else:
            el = OxmlElement("w:instrText")
            el.set(qn("xml:space"), "preserve")
            el.text = text
            run._r.append(el)
    set_font(run, 9)


INLINE = re.compile(r"(\*\*.+?\*\*|`[^`]+`)")


def add_rich(par, text, size=10.5, cn=BODY_FONT_CN):
    """处理 **粗体** 和 `行内代码`。"""
    text = text.replace("\\|", "|")
    for part in INLINE.split(text):
        if not part:
            continue
        if part.startswith("**") and part.endswith("**"):
            r = par.add_run(part[2:-2])
            set_font(r, size, bold=True, cn=cn)
        elif part.startswith("`") and part.endswith("`"):
            r = par.add_run(part[1:-1])
            set_font(r, size - 0.5, cn=cn, en=MONO)
            r.font.color.rgb = RGBColor(0xA3, 0x3B, 0x3B)
        else:
            r = par.add_run(part)
            set_font(r, size, cn=cn)


def style_doc(doc):
    n = doc.styles["Normal"]
    n.font.name = BODY_FONT_CN
    n.font.size = Pt(10.5)
    n._element.rPr.rFonts.set(qn("w:eastAsia"), BODY_FONT_CN)
    pf = n.paragraph_format
    pf.line_spacing = 1.4
    pf.space_after = Pt(4)

    sec = doc.sections[0]
    sec.page_width, sec.page_height = Cm(21.0), Cm(29.7)
    sec.left_margin = sec.right_margin = Cm(2.4)
    sec.top_margin = sec.bottom_margin = Cm(2.2)
    add_page_number_footer(sec)


def add_table(doc, rows):
    header = [c.strip() for c in rows[0].strip().strip("|").split("|")]
    body = []
    for line in rows[2:]:
        body.append([c.strip() for c in line.strip().strip("|").split("|")])
    ncol = len(header)
    t = doc.add_table(rows=1 + len(body), cols=ncol)
    t.style = "Table Grid"
    t.alignment = WD_TABLE_ALIGNMENT.CENTER
    for j, h in enumerate(header):
        cell = t.rows[0].cells[j]
        cell.text = ""
        par = cell.paragraphs[0]
        add_rich(par, h, size=9)
        for r in par.runs:
            r.font.bold = True
        shd = OxmlElement("w:shd")
        shd.set(qn("w:fill"), "EDF2F6")
        cell._tc.get_or_add_tcPr().append(shd)
    for i, row in enumerate(body, start=1):
        for j in range(ncol):
            txt = row[j] if j < len(row) else ""
            cell = t.rows[i].cells[j]
            cell.text = ""
            add_rich(cell.paragraphs[0], txt, size=9)
    doc.add_paragraph()


def main():
    lines = open(MD, encoding="utf-8").read().split("\n")
    doc = Document()
    style_doc(doc)

    i = 0
    in_code = False
    code_buf = []

    while i < len(lines):
        line = lines[i]
        s = line.rstrip()

        # 代码块
        if s.startswith("```"):
            if in_code:
                par = doc.add_paragraph()
                par.paragraph_format.left_indent = Cm(0.4)
                par.paragraph_format.line_spacing = 1.15
                r = par.add_run("\n".join(code_buf))
                set_font(r, 9, cn=BODY_FONT_CN, en=MONO)
                shd = OxmlElement("w:shd")
                shd.set(qn("w:fill"), "F5F5F5")
                par._p.get_or_add_pPr().append(shd)
                code_buf = []
                in_code = False
            else:
                in_code = True
            i += 1
            continue
        if in_code:
            code_buf.append(line)
            i += 1
            continue

        if not s:
            i += 1
            continue

        # 图片
        m = re.match(r"!\[(.*?)\]\((.*?)\)", s)
        if m:
            alt, path = m.group(1), m.group(2)
            img = os.path.normpath(os.path.join(os.path.dirname(MD), path))
            if os.path.exists(img):
                doc.add_picture(img, width=Cm(16.0))
                doc.paragraphs[-1].alignment = WD_ALIGN_PARAGRAPH.CENTER
                cap = doc.add_paragraph()
                cap.alignment = WD_ALIGN_PARAGRAPH.CENTER
                r = cap.add_run(alt)
                set_font(r, 9, cn=BODY_FONT_CN)
                r.font.color.rgb = RGBColor(0x66, 0x66, 0x66)
            else:
                p = doc.add_paragraph()
                add_rich(p, f"[缺图] {path}", size=10)
            i += 1
            continue

        # 标题
        if s.startswith("# "):
            p = doc.add_paragraph()
            r = p.add_run(s[2:])
            set_font(r, 20, bold=True)
            p.paragraph_format.space_before = Pt(0)
            p.paragraph_format.space_after = Pt(10)
            i += 1
            continue
        if s.startswith("## "):
            p = doc.add_paragraph()
            r = p.add_run(s[3:])
            set_font(r, 14, bold=True)
            r.font.color.rgb = RGBColor(0x1F, 0x4E, 0x79)
            p.paragraph_format.space_before = Pt(12)
            p.paragraph_format.space_after = Pt(6)
            i += 1
            continue
        if s.startswith("### "):
            p = doc.add_paragraph()
            r = p.add_run(s[4:])
            set_font(r, 12, bold=True)
            p.paragraph_format.space_before = Pt(8)
            i += 1
            continue

        # 分隔线
        if s.strip() == "---":
            p = doc.add_paragraph()
            pPr = p._p.get_or_add_pPr()
            bdr = OxmlElement("w:pBdr")
            bottom = OxmlElement("w:bottom")
            bottom.set(qn("w:val"), "single")
            bottom.set(qn("w:sz"), "6")
            bottom.set(qn("w:color"), "BBBBBB")
            bdr.append(bottom)
            pPr.append(bdr)
            i += 1
            continue

        # 表格
        if s.startswith("|") and i + 1 < len(lines) and re.match(r"^\|[\s:\-|]+\|$", lines[i + 1].strip()):
            block = []
            while i < len(lines) and lines[i].strip().startswith("|"):
                block.append(lines[i])
                i += 1
            add_table(doc, block)
            continue

        # 列表
        if s.startswith("- "):
            p = doc.add_paragraph(style="List Bullet")
            add_rich(p, s[2:])
            p.paragraph_format.line_spacing = 1.3
            p.paragraph_format.space_after = Pt(2)
            i += 1
            continue
        m = re.match(r"^(\d+)\.\s+(.*)$", s)
        if m:
            p = doc.add_paragraph(style="List Number")
            add_rich(p, m.group(2))
            p.paragraph_format.line_spacing = 1.3
            p.paragraph_format.space_after = Pt(2)
            i += 1
            continue

        # 引用
        if s.startswith("> "):
            p = doc.add_paragraph()
            p.paragraph_format.left_indent = Cm(0.6)
            add_rich(p, s[2:])
            i += 1
            continue

        # 普通段落
        p = doc.add_paragraph()
        add_rich(p, s)
        i += 1

    doc.save(OUT)

    # 回读自检
    d = Document(OUT)
    n_img = len(d.inline_shapes)
    print("已生成:", OUT)
    print("  段落 %d  表格 %d  图片 %d  字节 %d" % (
        len(d.paragraphs), len(d.tables), n_img, os.path.getsize(OUT)))
    for t in d.tables:
        assert len(t.rows) >= 2 and len(t.columns) >= 2


if __name__ == "__main__":
    main()
