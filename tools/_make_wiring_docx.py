# -*- coding: utf-8 -*-
"""生成《接线工程图与接线手册》Word 文档"""
import os
from docx import Document
from docx.shared import Pt, Cm, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH, WD_LINE_SPACING
from docx.enum.table import WD_TABLE_ALIGNMENT
from docx.oxml.ns import qn
from docx.oxml import OxmlElement
from PIL import Image

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
IMG = os.path.join(BASE, 'images')
OUT = os.path.join(BASE, '控制柜监测告警系统-接线工程图.docx')

doc = Document()
sec = doc.sections[0]
sec.page_width, sec.page_height = Cm(21), Cm(29.7)
sec.left_margin = sec.right_margin = Cm(2.5)
sec.top_margin = sec.bottom_margin = Cm(2.5)
USABLE = 16.0


def set_run(run, east='宋体', west='Times New Roman', size=12, bold=False, color=None):
    run.font.name = west
    run.font.size = Pt(size)
    run.font.bold = bold
    if color:
        run.font.color.rgb = RGBColor(*color)
    rPr = run._element.get_or_add_rPr()
    rf = rPr.find(qn('w:rFonts'))
    if rf is None:
        rf = OxmlElement('w:rFonts'); rPr.append(rf)
    rf.set(qn('w:eastAsia'), east); rf.set(qn('w:ascii'), west); rf.set(qn('w:hAnsi'), west)
    return run


def _ind(p, chars='200'):
    pPr = p._p.get_or_add_pPr()
    ind = pPr.find(qn('w:ind'))
    if ind is None:
        ind = OxmlElement('w:ind'); pPr.append(ind)
    ind.set(qn('w:firstLineChars'), chars)


def body(text, indent=True, size=12, before=0):
    p = doc.add_paragraph()
    p.paragraph_format.line_spacing_rule = WD_LINE_SPACING.ONE_POINT_FIVE
    p.paragraph_format.space_after = Pt(0)
    p.paragraph_format.space_before = Pt(before)
    p.alignment = WD_ALIGN_PARAGRAPH.JUSTIFY
    if indent:
        _ind(p)
    set_run(p.add_run(text), size=size)
    return p


def h1(text):
    p = doc.add_paragraph()
    p.paragraph_format.space_before = Pt(18); p.paragraph_format.space_after = Pt(8)
    set_run(p.add_run(text), east='黑体', west='SimHei', size=16, bold=True)
    return p


def h2(text):
    p = doc.add_paragraph()
    p.paragraph_format.space_before = Pt(12); p.paragraph_format.space_after = Pt(6)
    set_run(p.add_run(text), east='黑体', west='SimHei', size=13, bold=True)
    return p


def add_image(name, caption, max_w=USABLE, max_h=20.0):
    path = os.path.join(IMG, name)
    w, h = Image.open(path).size
    ratio = h / w
    width, height = max_w, max_w * ratio
    if height > max_h:
        height, width = max_h, max_h / ratio
    p = doc.add_paragraph(); p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.space_before = Pt(6); p.paragraph_format.space_after = Pt(2)
    p.add_run().add_picture(path, width=Cm(width), height=Cm(height))
    c = doc.add_paragraph(); c.alignment = WD_ALIGN_PARAGRAPH.CENTER
    c.paragraph_format.space_after = Pt(12)
    set_run(c.add_run(caption), size=10.5, color=(0x44, 0x44, 0x44))


def table(headers, rows, widths, fs=10):
    t = doc.add_table(rows=1, cols=len(headers))
    t.style = 'Table Grid'; t.alignment = WD_TABLE_ALIGNMENT.CENTER
    for i, htxt in enumerate(headers):
        c = t.rows[0].cells[i]; c.text = ''
        p = c.paragraphs[0]; p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        set_run(p.add_run(htxt), east='黑体', size=fs, bold=True)
        shd = OxmlElement('w:shd'); shd.set(qn('w:fill'), 'DCE9F7')
        c._tc.get_or_add_tcPr().append(shd)
    for row in rows:
        cells = t.add_row().cells
        for i, v in enumerate(row):
            cells[i].text = ''
            p = cells[i].paragraphs[0]
            p.alignment = WD_ALIGN_PARAGRAPH.CENTER if i == 0 else WD_ALIGN_PARAGRAPH.LEFT
            set_run(p.add_run(str(v)), size=fs)
    for r in t.rows:
        for i, wcm in enumerate(widths):
            r.cells[i].width = Cm(wcm)
    doc.add_paragraph().paragraph_format.space_after = Pt(6)


# ==================================================================
p = doc.add_paragraph(); p.alignment = WD_ALIGN_PARAGRAPH.CENTER
p.paragraph_format.space_after = Pt(4)
set_run(p.add_run('模拟控制柜健康监测与故障诊断系统'), east='黑体', west='SimHei', size=21, bold=True)
p = doc.add_paragraph(); p.alignment = WD_ALIGN_PARAGRAPH.CENTER
p.paragraph_format.space_after = Pt(4)
set_run(p.add_run('接 线 工 程 图 与 接 线 手 册'), east='黑体', west='SimHei', size=16)
p = doc.add_paragraph(); p.alignment = WD_ALIGN_PARAGRAPH.CENTER
p.paragraph_format.space_after = Pt(16)
set_run(p.add_run('主控：正点原子 MiniPro H750（STM32H750VB） ｜ 装配方式：全免焊 ｜ 2026.09'),
        east='楷体', size=11, color=(0x55, 0x55, 0x55))

warn = doc.add_paragraph(); warn.alignment = WD_ALIGN_PARAGRAPH.CENTER
warn.paragraph_format.space_after = Pt(14)
set_run(warn.add_run(
    '※ 重要：本手册引脚号为【推荐方案】，落地前请对照《MiniPro H750 引脚分配表》逐脚核对。'),
    east='黑体', size=11.5, bold=True, color=(0xC0, 0x39, 0x2B))

# ==================================================================
h1('一、接线总览图')
add_image('fig13_wiring.png', '图 1  项目接线总览图（什么模块接开发板哪个接口）')
body('按总线类型分组连接：I2C1 挂载 SHT30 #1 与 INA219（地址不同可共总线）；'
     'I2C2 单独挂载 SHT30 #2；DS18B20 走单总线并配 4.7kΩ 上拉；'
     'SPI1 作为共享总线，由 TFT、触摸控制器与 W5500 分片选访问；'
     'ESP-01S 走 USART2；加热 MOS、蜂鸣器与告警 LED 分别占用一路 PWM 或 GPIO。')

h1('二、开发板引脚占用图')
add_image('fig14_pinmap.png', '图 2  开发板引脚占用图（哪些脚能用、哪些禁占）')
body('图中四种颜色分别表示：禁占（板载 QSPI Flash 与晶振占用）、板载已占用（按键、RGBLED、'
     '板载串口、板载 TF 卡座）、本项目使用、空闲可用。')
body('需要特别强调的是最左侧一列：PB2、PB6、PD11~PD13、PE2 六个引脚被板载 16MB QSPI Flash 占用，'
     '而程序本身就运行在这片外部 Flash 上（映射地址 0x90000000），'
     '一旦被挪用将导致程序无法下载或运行异常。此外 PA9、PA10 通常被板载 CH340 串口占用，'
     'PA13、PA14 为 SWD 调试口必须保留。')

h1('三、接线前的三条铁律')
body('（1）每次上电前必须先查短路。用万用表通断档测量 5V 与 GND 之间是否导通，'
     '导通即存在短路，严禁上电。这是唯一能让主控板一次性报废的操作。', before=2)
body('（2）先控制电、后加热电。第一阶段只接控制部分（不接加热回路），确认串口与 I2C 正常后，'
     '再接入功率级。')
body('（3）不确定的线不接。宁可停下来查资料，也不要用"大概是这样"的态度上电。'
     '接错线的代价远高于多花十分钟核对。')

# ==================================================================
h1('四、分模块接线对照表')

h2('4.1 I2C1 总线（SHT30 #1 + INA219 共用）')
table(['模块', '接线（模块端 → 开发板端）', '备注'],
      [['SHT30 #1', 'VCC→3V3 · GND→GND · SCL→PB8 · SDA→PB9', 'I2C 地址 0x44；模块一般已带 4.7kΩ 上拉'],
       ['INA219', 'VCC→3V3 · GND→GND · SCL→PB8 · SDA→PB9', 'I2C 地址 0x40，与 SHT30 不冲突'],
       ['INA219', 'Vin+ → 加热电源侧（5V 经 PPTC/KSD9700 后）', '高边接法，顺序不可颠倒'],
       ['INA219', 'Vin- → 水泥电阻侧', '若读数为负说明 Vin+/Vin- 接反']],
      [2.4, 8.0, 5.6])

h2('4.2 I2C2 总线（SHT30 #2）')
table(['模块', '接线（模块端 → 开发板端）', '备注'],
      [['SHT30 #2', 'VCC→3V3 · GND→GND · SCL→PB10 · SDA→PB11', '独立总线，实现总线级故障隔离']],
      [2.4, 8.0, 5.6])

h2('4.3 单总线（DS18B20 负载本体温度）')
table(['模块', '接线（模块端 → 开发板端）', '备注'],
      [['DS18B20', '红(VCC)→3V3 · 黑(GND)→GND · 黄(DQ)→PB12', '不锈钢壳贴加热块本体'],
       ['上拉电阻', '4.7kΩ 跨接在 DQ(PB12) 与 3V3 之间', '必须加，否则读不出数据（插面包板即可）']],
      [2.4, 8.0, 5.6])

h2('4.4 SPI1 共享总线（TFT / 触摸 / W5500）')
table(['设备', '接线（模块端 → 开发板端）', '备注'],
      [['共用信号', 'SCK→PA5 · MISO→PA6 · MOSI→PA7', '三设备共用，靠片选区分'],
       ['TFT ILI9341', 'CS→PA4 · DC→PA2 · RST→PA3 · BL→PB0', 'BL 走 PB0（TIM3_CH3）可 PWM 调光'],
       ['TFT 供电', 'VCC→3V3（按模块标注）· GND→GND', '部分模块需 5V 供电，先看丝印'],
       ['W5500', 'CS→PB7 · INT→PC4 · RST→PC6', '仅共用 SCK/MISO/MOSI 三根'],
       ['触摸（电阻 XPT2046）', 'CS→PB1 · IRQ→PC5', '与 TFT 共用 SPI 总线，经典方案'],
       ['触摸（电容 FT6236）', 'SCL/SDA 接 I2C · INT→PC5 · RST→PC6', '电容屏走 I2C，地址通常 0x38，不占 SPI'],
       ['固件要求', 'SPI1 取锁必须带超时（50ms）', '禁止 portMAX_DELAY，防死锁']],
      [3.0, 7.4, 5.6])
body('※ 特别注意屏幕【形态】：必须是"带 PCB 转接板 + 2.54mm 排针"的模块，才能用杜邦线直连。'
     '若为"裸屏"（只有 FPC 排线、背面无转接板），必须额外配 FPC 转接座才能接线，'
     '而 FPC 座为 0.5mm 间距、需热风枪焊接，会直接破坏本项目的零焊接原则 —— '
     '建议更换为带转接板的模块，而不是硬着头皮去焊。')

h2('4.4.1 已选定 TFT 模块（TZT 2.4"/2.8" ILI9341 SPI 带触控）的 14 脚接线')
body('该模块带红色 PCB 转接板、2.54mm 排针（已焊）、ILI9341 驱动与 XPT2046 电阻触摸，'
     '可直插杜邦线；板载 U1/U2/U3 电平转换芯片，3.3V 与 5V 均可驱动。'
     '模块上另有一个 Micro SD 卡座与 TFT 共用 SPI —— 本项目不使用它'
     '（改用主控板背面自带的 TF 卡座），其片选在板上已上拉，一般不干扰。')
table(['模块丝印', '接到', '说明'],
      [['VCC', '3V3', '板载电平转换，5V 亦可'],
       ['GND', 'GND', '与主控共地'],
       ['CS', 'PA4', 'LCD 片选'],
       ['RESET', 'PA3', 'LCD 复位'],
       ['DC', 'PA2', '数据 / 命令选择'],
       ['SDI(MOSI)', 'PA7', 'SPI 数据输入'],
       ['SCK', 'PA5', 'SPI 时钟'],
       ['LED', 'PB0', '背光，TIM3_CH3 可 PWM 调光'],
       ['SDO(MISO)', 'PA6', 'SPI 数据输出'],
       ['T_CLK', 'PA5（与 SCK 并联）', '触摸时钟；模块内部不与 LCD 连通，需外部并联'],
       ['T_CS', 'PB1', '触摸片选'],
       ['T_DIN', 'PA7（与 MOSI 并联）', '触摸数据输入'],
       ['T_DO', 'PA6（与 MISO 并联）', '触摸数据输出'],
       ['T_IRQ', 'PC5', '触摸中断']],
      [3.0, 4.8, 8.2])
body('※ 以模块实物丝印为准：不同批次丝印排列顺序可能不同，关键看【信号名】而非物理位置，'
     '接线前请核对模块背面丝印。')
body('※ 电阻触摸屏的坐标是模拟量，首次使用需执行一次触摸校准：依次点击屏幕四角，'
     '固件记录原始采样值并做线性映射；校准系数建议存入板载 AT24C02 EEPROM，做到掉电不丢。')

h2('4.5 功率回路（加热驱动）')
table(['环节', '接线', '备注'],
      [['电源', '5V+ → PPTC(1.1A) → KSD9700(70°C) → INA219.Vin+', '全部用弹簧接线端子串接，免焊'],
       ['负载', 'INA219.Vin- → 水泥电阻 → IRLZ44N 漏极 D', '水泥电阻引线经弹簧端子'],
       ['回路', 'IRLZ44N 源极 S → GND（星型接地点）', '加热级建议独立 5V/2A 供电'],
       ['栅极驱动', 'PA8 → 100Ω → G（栅极）', 'PA8 为 TIM1_CH1，输出 1Hz 时间比例'],
       ['栅极下拉', '10kΩ 从 G 到 GND', '必加！防止上电期间栅极悬空误导通'],
       ['MOS 脚序', 'TO-220：左 G / 中 D / 右 S', 'D 在中间，接线前核对丝印']],
      [2.4, 8.0, 5.6])

h2('4.6 声光输出')
table(['设备', '接线（模块端 → 开发板端）', '备注'],
      [['无源蜂鸣器', 'VCC→5V · GND→GND · I/O→PB15', 'PB15 为 TIM12_CH2，PWM 变调实现三级告警音'],
       ['告警 LED', '5V → 220Ω → LED 正极；LED 负极 → PC13', '低电平点亮；5V-2V）/220Ω ≈ 13.6mA'],
       ['板载 RGBLED', '无需接线（PB4 红 / PE5 绿 / PE6 蓝）', '直接复用为状态指示灯'],
       ['对外接口', '预留 2P 端子做外部告警灯输出', '可选，方便面板安装']],
      [2.6, 7.8, 5.6])

h2('4.7 串口与调试')
table(['用途', '接线', '备注'],
      [['调试串口', '板载 CH340（PA9 TX / PA10 RX）', '直接插 USB 线，波特率 115200'],
       ['ESP-01S', 'ESP TX→PD6(RX) · ESP RX→PD5(TX) · CH_PD→3V3', 'TX/RX 交叉；GPIO0 悬空为运行、拉低为烧录'],
       ['ESP-01S 供电', '建议独立 3.3V + 100µF + 0.1µF 去耦', '峰值 300mA，板载 LDO 带不动会复位'],
       ['SWD 调试', 'SWDIO→PA13 · SWCLK→PA14 · GND', 'CubeMX 必须选 Serial Wire，不能选 JTAG 5-pin']],
      [2.4, 8.2, 5.4])

# ==================================================================
h1('五、杜邦线颜色规范')
body('建议按以下约定配色，好处是排查故障时不用逐根线追 —— 一眼就能看出哪根线属于哪条总线。')
table(['颜色', '用途', '说明'],
      [['红', '电源正极', '3V3 与 5V 建议用深红/橙红区分'],
       ['黑', '电源地', '所有 GND 统一黑色'],
       ['黄', 'I2C SCL / SPI SCK', '时钟线统一黄色'],
       ['绿', 'I2C SDA / SPI MOSI', '数据线统一绿色'],
       ['蓝', 'SPI MISO / 片选 CS', 'MISO 与 CS 统一蓝色'],
       ['白', '中断 / 复位 / 控制信号', 'INT、RST、DC 等'],
       ['棕 / 灰', 'PWM / GPIO 输出', '加热控制、蜂鸣器、告警灯']],
      [2.4, 5.0, 8.6])
body('⚠ 若现有杜邦线颜色不够，至少保证"电源正极"和"电源地"两色严格区分、'
     '绝不混用，其余可灵活处理。')

# ==================================================================
h1('六、上电前检查清单')
body('建议打印本页，每完成一步打勾。')
table(['#', '检查项', '通过判据'],
      [['1', '万用表通断档测 5V 与 GND', '不导通（无短路）'],
       ['2', '确认 KSD9700 常温状态', '导通（证明是常闭型）'],
       ['3', '核对 IRLZ44N 三脚顺序', '左 G / 中 D / 右 S'],
       ['4', '核对 INA219 的 Vin+ / Vin- 方向', 'Vin+ 靠电源侧'],
       ['5', '确认 PA13/PA14 未被占用', 'SWD 可连接'],
       ['6', '确认未误用 PB2 / PB6 / PD11～PD13 / PE2', 'QSPI 保留'],
       ['7', '先只上控制电、不接加热回路', '串口有输出，I2C 扫到 0x40 与 0x44'],
       ['8', '接入功率级后测回路电流', '万用表读数 ≈ 0.5A'],
       ['9', '比对 INA219 读数与万用表', '偏差 < 5%'],
       ['10', 'MOS 导通性验证', 'Vds < 0.1V 且外壳不烫']],
      [1.2, 7.0, 7.8])

# ==================================================================
h1('七、常见接线错误与后果')
table(['错误', '后果', '预防'],
      [['杜邦线硬插模块过孔（排针没焊）', '接触电阻不稳，电流读数乱跳，诊断逻辑误报', '模块必须用已焊排针，或用弹簧端子夹引线'],
       ['MOS 栅极未加下拉电阻', '上电瞬间栅极悬空，加热器可能被意外点着', 'G 到 GND 必须接 10kΩ'],
       ['INA219 放在低边', '总线电压读数≈0，R=U/I 算不出来', '必须放高边（电源侧）'],
       ['加热地与采样地共用一条地线', '地线压降污染电流采样，误差可达 40%', '星型接地 + 加热级独立供电'],
       ['把 MOS 或元件贴在加热铝块上', '热耦合导致参数漂移；MOS 金属背板带电有短路风险', '铝块只贴传感器，MOS 悬空放置'],
       ['先用 JTAG 后改 SWD', 'PA15 / PB4 被占用，红灯与 KEY1 失效', 'CubeMX 一开始就选 Serial Wire'],
       ['KSD9700 只是放在加热块附近', '它测的是自身壳温，不贴合就永远不动作', '用导热硅脂+铝箔胶带紧贴本体']],
      [4.6, 6.4, 5.0])

doc.save(OUT)
print('SAVED', os.path.basename(OUT))
print('SIZE_KB', round(os.path.getsize(OUT) / 1024, 1))
