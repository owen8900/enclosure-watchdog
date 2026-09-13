# -*- coding: utf-8 -*-
"""生成《项目策划书》Word 文档"""
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
OUT = os.path.join(BASE, '控制柜监测告警系统-项目策划书.docx')

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
    rf.set(qn('w:eastAsia'), east)
    rf.set(qn('w:ascii'), west)
    rf.set(qn('w:hAnsi'), west)
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


def kv(label, text, size=11.5):
    p = doc.add_paragraph()
    p.paragraph_format.line_spacing_rule = WD_LINE_SPACING.ONE_POINT_FIVE
    p.paragraph_format.space_after = Pt(0)
    p.alignment = WD_ALIGN_PARAGRAPH.JUSTIFY
    _ind(p)
    set_run(p.add_run(label), east='黑体', west='SimHei', size=size, bold=True)
    set_run(p.add_run(text), size=size)
    return p


def h1(text):
    p = doc.add_paragraph()
    p.paragraph_format.space_before = Pt(18)
    p.paragraph_format.space_after = Pt(8)
    set_run(p.add_run(text), east='黑体', west='SimHei', size=16, bold=True)
    return p


def h2(text):
    p = doc.add_paragraph()
    p.paragraph_format.space_before = Pt(12)
    p.paragraph_format.space_after = Pt(6)
    set_run(p.add_run(text), east='黑体', west='SimHei', size=13.5, bold=True)
    return p


def h3(text):
    p = doc.add_paragraph()
    p.paragraph_format.space_before = Pt(9)
    p.paragraph_format.space_after = Pt(4)
    set_run(p.add_run(text), east='黑体', west='SimHei', size=12, bold=True)
    return p


def add_image(name, caption, max_w=USABLE, max_h=19.0):
    path = os.path.join(IMG, name)
    w, h = Image.open(path).size
    ratio = h / w
    width, height = max_w, max_w * ratio
    if height > max_h:
        height = max_h
        width = height / ratio
    p = doc.add_paragraph()
    p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.space_before = Pt(6)
    p.paragraph_format.space_after = Pt(2)
    p.add_run().add_picture(path, width=Cm(width), height=Cm(height))
    c = doc.add_paragraph()
    c.alignment = WD_ALIGN_PARAGRAPH.CENTER
    c.paragraph_format.space_after = Pt(12)
    set_run(c.add_run(caption), size=10.5, color=(0x44, 0x44, 0x44))


def table(headers, rows, widths, fs=10.5):
    t = doc.add_table(rows=1, cols=len(headers))
    t.style = 'Table Grid'
    t.alignment = WD_TABLE_ALIGNMENT.CENTER
    for i, htxt in enumerate(headers):
        c = t.rows[0].cells[i]
        c.text = ''
        p = c.paragraphs[0]
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        set_run(p.add_run(htxt), east='黑体', size=fs, bold=True)
        shd = OxmlElement('w:shd')
        shd.set(qn('w:fill'), 'DCE9F7')
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
# 封面
# ==================================================================
for _ in range(3):
    doc.add_paragraph()
p = doc.add_paragraph(); p.alignment = WD_ALIGN_PARAGRAPH.CENTER
p.paragraph_format.space_after = Pt(6)
set_run(p.add_run('模拟控制柜健康监测与故障诊断系统'), east='黑体', west='SimHei', size=24, bold=True)
p = doc.add_paragraph(); p.alignment = WD_ALIGN_PARAGRAPH.CENTER
p.paragraph_format.space_after = Pt(30)
set_run(p.add_run('项  目  策  划  书'), east='黑体', west='SimHei', size=20)
for lab, val in [('主控平台', '正点原子 MiniPro H750（STM32H750VB，480MHz Cortex-M7）'),
                 ('实时系统', 'FreeRTOS 多任务架构'),
                 ('方案版本', 'v0.3（免焊接方案）'),
                 ('项目周期', '2026.09 — 2027.07'),
                 ('编制日期', '2026 年 9 月')]:
    p = doc.add_paragraph(); p.alignment = WD_ALIGN_PARAGRAPH.CENTER
    p.paragraph_format.space_after = Pt(6)
    set_run(p.add_run(f'{lab}：{val}'), size=12)
doc.add_page_break()

# ==================================================================
h1('一、项目概述')

h2('1.1 项目背景')
body('工业控制柜内部的电气元件（断路器、接触器、加热器、接线端子）长期处于高温与大电流工况，'
     '其失效通常不是突发的，而是先表现为温升异常、回路电流漂移、元件阻值退化等渐进特征。'
     '传统方案多采用单一温度阈值报警，只能在故障已经发生之后才动作，缺乏对异常趋势的识别能力，'
     '也无法区分"真实过热"与"传感器自身失效"。')
body('本项目以模拟控制柜为对象（5V 安全电压下以 10Ω/10W 水泥电阻等效发热负载），'
     '构建一套具备多源数据融合、故障诊断与健康度趋势预测能力的监测系统，'
     '在安全、低成本的前提下完整复现工业设备监测的核心技术链路。')

h2('1.2 项目目标')
kv('【技术目标】', '构建端-边-云三层架构的设备健康监测系统，实现多源传感采集、'
   '五类故障的自动判别、三级分级告警、Modbus TCP 工业协议对接与 MQTT 上云可视化；'
   '固件在 72 小时连续运行下零看门狗复位。')
kv('【参赛目标】', '以同一套技术内核支撑 2 个重点学科竞赛（嵌入式芯片与系统设计竞赛、'
   '全国大学生计算机设计大赛），产出完整的作品、申报材料与演示视频。')
kv('【能力目标】', '完整经历一次"从需求到交付"的嵌入式工程实践，'
   '形成可复用的驱动层、任务框架与调试方法论，并沉淀一份工程日志。')

h2('1.3 技术指标')
table(['指标项', '目标值', '验证方式'],
      [['连续运行', '72 小时零看门狗复位', '长时间跑测 + 串口日志统计'],
       ['告警响应', '采样到声光输出 < 500ms', '示波器 / 秒表实测'],
       ['电流测量误差', '< 5%（INA219 出厂标定）', '与万用表读数比对'],
       ['冷启动到可监测', '< 3 秒', '上电计时'],
       ['断网续传', '数据不丢、不重、不乱序', '拔网线后回插，核对日志序号'],
       ['资源占用', 'RAM / Flash < 80%', '编译报告 + 运行时统计'],
       ['任务栈余量', '全部任务 ≥ 30%', 'uxTaskGetStackHighWaterMark()'],
       ['故障识别', '4 类注入故障全部正确判别', '故障注入测试记录']],
      [3.2, 5.4, 7.4])

# ==================================================================
h1('二、总体技术方案')

h2('2.1 系统总体架构')
add_image('fig1_architecture.png', '图 1  系统总体架构与数据流（端 - 边 - 云）')
body('感知层以 100ms 周期采集环境温度、负载本体温度、局部温度与回路电流；'
     '边缘控制层由 STM32H750 完成数据融合、故障诊断、告警决策、本地显示与加热控制；'
     '通信层提供 W5500 以太网主链路与 ESP-01S 无线备用链路；'
     '应用层通过 Modbus TCP 接入 SCADA/PLC/组态软件，同时经 MQTT 上云由可视化大屏完成趋势分析。')

h2('2.2 功率级与安全设计')
add_image('fig3_power.png', '图 2  加热功率级接线与三重安全链')
body('加热回路自 5V 电源起，依次串联 PPTC 自恢复保险丝（1.1A）、常闭型温控开关 KSD9700（70°C）、'
     'INA219 电流传感器（内置 0.1Ω 精密分流）、10Ω/10W 水泥电阻与 IRLZ44N 低边开关管，回到星型接地点。'
     'MOS 栅极由 PA8 经 100Ω 串阻驱动并配置 10kΩ 下拉，防止上电期间栅极悬空导致加热误启动。')
body('系统设置三重独立保护：软件看门狗与告警状态机、硬件温控开关、回路自恢复保险丝。'
     '任何异常情形一律执行失效安全（fail-safe）动作，无条件切断加热。')

# ==================================================================
h1('三、项目阶段划分')

h2('3.1 阶段划分总览')
add_image('fig9_gantt.png', '图 3  项目阶段划分总览（甘特图）')

h2('3.2 阶段摘要')
table(['阶段', '名称', '时间', '关键里程碑'],
      [['P0', '硬件自检与工具链打通', '2026.09下–10上', '串口 / RGBLED / TFT 三项跑通；引脚表定稿'],
       ['P1', '外设逐个点亮', '2026.10–11初', 'SHT30 / INA219 / DS18B20 / TF卡 / W5500 五个 demo'],
       ['—', '软考中级（项目挂起）', '2026.11', '—'],
       ['P2', 'RTOS 任务架构', '2026.12', '8 任务框架建成；连续采集 1h 零复位'],
       ['P3', '告警与故障诊断', '2027.01', '状态机 + 趋势预警；故障注入全部通过'],
       ['P4', '通信与上云', '2027.02', 'Modbus TCP 从站 + MQTT + 断网续传'],
       ['P5', '结构装配与安全链', '2027.02下–03上', '整机装配完成、线束整理、安全链实测'],
       ['P6', '大屏与参赛材料', '2027.03', '可视化大屏 + 申报书 + 演示视频'],
       ['P7', '参赛', '2027.04–07', '蓝桥杯(4月) / 省赛答辩 / 嵌入式设计师(5月)']],
      [1.5, 4.4, 3.1, 7.0])

h2('3.3 各阶段任务书')

phases = [
    ('P0  硬件自检与工具链打通（2026.09 下 – 10 上）',
     '打通 STM32H750 开发链路，证明工具链可用。这是全项目唯一可能"卡死"的环节。',
     '① 下载正点原子资料包 A 盘，提取《MiniPro H750 引脚分配表》与原理图；'
     '② 安装 Keil MDK 与 QSPI Flash 下载算法；'
     '③ 依次跑通 M1 串口打印、M2 板载 RGBLED 闪烁、M3 TFT 显示；'
     '④ 建立代码仓库并提交可运行工程。',
     '① 可运行的工程模板；② 定稿的引脚分配表；③ 三个里程碑的验证截图与串口日志。',
     '三项里程碑全部跑通；工程可重复编译下载；引脚表与原理图核对无冲突。'),

    ('P1  外设逐个点亮（2026.10 – 11 初）',
     '让每个外设都有独立、可验证的驱动程序，为后续集成打好基础。',
     '① SHT30 双路 I2C 读数；② INA219 电流/电压读数并与万用表比对；'
     '③ DS18B20 单总线读数（含 4.7kΩ 上拉）；④ 板载 TF 卡 FatFS 读写；'
     '⑤ W5500 网络（ping 通 / DHCP）。',
     '五个独立 demo 工程 + 一份实测数据记录表。',
     '每个 demo 单独可运行且读数合理；INA219 与万用表偏差 < 5%。'),

    ('P2  RTOS 任务架构（2026.12）',
     '把五个独立 demo 整合进 FreeRTOS 多任务框架，建立稳定的采集链路。',
     '① 移植 FreeRTOS 并配置 tick / 优先级 / 栈溢出检测；'
     '② 划分 8 个任务并确定优先级；③ 建立队列、互斥量（SPI 共享总线）与任务通知机制；'
     '④ 接入 IWDG 看门狗；⑤ 用 uxTaskGetStackHighWaterMark 调优栈大小。',
     '可长时间运行的采集固件 + 任务优先级表 + 栈水位记录。',
     '连续采集 1 小时零复位；所有任务栈余量 ≥ 30%。'),

    ('P3  告警与故障诊断（2027.01 寒假）',
     '实现项目核心算法：三级告警状态机与多源融合故障诊断。',
     '① 实现带迟滞与去抖的状态机；② 实现六条诊断判据；'
     '③ 接入无源蜂鸣器变调与 RGBLED 三色输出；④ 设计并执行故障注入测试。',
     '诊断算法模块 + 故障注入测试报告（含截图/录像）。',
     '四类注入故障（拔传感器 / 拔网线 / 断加热丝 / 短接回路）全部正确判别；'
     '过热保护演示成功触发并正确锁存。'),

    ('P4  通信与上云（2027.02 寒假）',
     '打通工业协议与云端链路，形成端-边-云闭环。',
     '① W5500 上移植 lwIP；② 实现 Modbus TCP 从站（502）并定义寄存器映射；'
     '③ 实现 MQTT 客户端与主题设计；④ 实现 TF 卡环形日志与断网续传。',
     '通信模块 + 寄存器映射表 + 上位机/云平台截图。',
     '上位机可实时读取寄存器；云端可见数据；拔网线后数据补传不丢不重。'),

    ('P5  结构装配与安全链（2027.02 下 – 03 上）',
     '把面包板原型变成可展示的整机，并完成安全链实测。',
     '① 用 M3 铜柱架空导热铝片并固定发热负载；② 传感器贴合（DS18B20 + KSD9700）；'
     '③ 亚克力盒装配与扎带理线；④ 安全链实测（温控开关动作、保险丝、看门狗）。',
     '整机实物 + 接线照片 + 安全链测试记录。',
     '整机上电自检通过；线束整齐无松动；三重保护逐一验证有效。'),

    ('P6  大屏与参赛材料（2027.03）',
     '产出比赛与求职所需的全部材料。',
     '① 云端可视化大屏（趋势曲线 / 告警看板 / 历史查询）；'
     '② 演示视频（分镜脚本 → 拍摄 → 剪辑，含故障注入 30 秒高光片段）；'
     '③ 各比赛申报书与答辩 PPT；④ 项目文档整理。',
     '大屏、演示视频、申报书、答辩 PPT、项目技术文档。',
     '演示视频 ≤ 6 分钟且逻辑完整；申报书技术方案无漏洞；'
     '答辩可应对"多源融合具体怎么融合"等追问。'),

    ('P7  参赛（2027.04 – 07）',
     '完成报名、材料提交与答辩，争取省赛及以上奖项。',
     '① 3 月完成报名与资格确认；② 4 月蓝桥杯省赛；③ 5 月嵌入式设计师软考；'
     '④ 5–6 月作品提交与省赛答辩；⑤ 6 月起转入秋招准备。',
     '参赛记录、奖项证书。',
     '至少 1 项省级及以上奖项；材料可完整复用于求职。'),
]

for title, goal, tasks, dels, acc in phases:
    h3(title)
    kv('【阶段目标】', goal)
    kv('【主要任务】', tasks)
    kv('【交付物】', dels)
    kv('【验收标准】', acc)

h2('3.4 阶段门（Stage-Gate）规则')
body('阶段之间设置验收门，未通过不得进入下一阶段，以防问题被带入后续环节而指数级放大。'
     '具体包括：P0 未跑通三项里程碑不得开始外设调试；P1 任一外设读数不合理不得进入 RTOS 整合；'
     'P2 未达到 1 小时零复位不得开始算法开发；P3 故障注入未全过不得进行通信集成；'
     'P4 未打通双协议不得开始整机装配；P5 安全链未验证不得进行长时间压力测试。')

# ==================================================================
h1('四、制作流程')

h2('4.1 软件开发流程')
add_image('fig10_sw_flow.png', '图 4  软件开发流程（自底向上，逐层验证）')
body('软件采用自底向上、逐层验证的推进方式，共分六层。'
     '底层为硬件抽象层与芯片配置（含 MPU 与 Cache 配置），其上为外设驱动层，'
     '再上为实时系统层（FreeRTOS 任务框架），然后是应用逻辑层（采集调度、告警状态机、故障诊断、加热控制），'
     '接着是通信与展示层，最后是验证层。')
body('核心原则是：上一层未通过验证，绝不进入下一层。'
     '每一层都必须具备可独立运行的验证手段 —— 例如外设驱动层用独立 demo 验证，'
     '实时系统层用 1 小时连续运行验证，应用逻辑层用故障注入验证。'
     '这样做的价值在于：问题出现时可被严格限定在当前层，避免"整个系统都不对但不知道错在哪"的困境。')

h2('4.2 硬件制作流程')
add_image('fig11_hw_flow.png', '图 5  硬件制作流程（4 个板块 · 12 个步骤）')
body('硬件制作分为四个板块：板块 A 为工具链与最小系统验证（步骤 1–5），此阶段不需要任何新增元件；'
     '板块 B 为控制电路搭建（步骤 6–7），只搭控制部分、不接功率级；'
     '板块 C 为功率回路搭建（步骤 8–10），涉及首次上电加热负载；'
     '板块 D 为整机装配与验证（步骤 11–12）。')
body('贯穿全过程的两条铁律：一是"先软件后硬件、先控制后功率"，任何一步都不越级；'
     '二是"每一次上电前必须先查短路" —— 用万用表通断档确认 5V 与 GND 之间不存在短路，'
     '这是唯一能让主控板一次性报废的操作。')

h2('4.3 软硬件联调流程')
add_image('fig12_integration.png', '图 6  软硬件联调流程（9 个步骤，逐级放行）')
body('联调阶段采用"逐级放行"策略：每一步都定义了明确的操作与通过判据，'
     '未达到判据即停止并回到上一步排查，绝不带着疑点继续往下走。'
     '顺序上从"只上控制电"开始，确认串口与 I2C 正常后再接入功率级；'
     '先用万用表实测电流并与 INA219 读数比对完成标定，再进行闭环控制与告警测试；'
     '最后依次执行故障注入、过热保护演示与 72 小时压力测试。')

# ==================================================================
h1('五、固件与算法设计')

h2('5.1 任务架构')
add_image('fig5_tasks.png', '图 7  固件软件架构（FreeRTOS 8 任务）')
table(['任务', '优先级', '周期', '栈', '职责'],
      [['heater_ctrl', '8', '10ms', '512w', '1Hz 时间比例控制 + 电流互锁'],
       ['alarm_fsm', '7', '100ms', '768w', '告警状态机 + 迟滞 + 趋势预警'],
       ['sensor', '6', '100ms', '768w', '读 2×SHT30 + INA219 + DS18B20'],
       ['diagnose', '5', '1s', '1024w', '多源融合判据 + R=U/I 老化趋势'],
       ['net_w5500', '4', '事件', '2048w', 'Modbus TCP 从站 + MQTT'],
       ['ui_lvgl', '3', '200ms', '2048w', 'TFT 局部刷新'],
       ['storage', '2', '1s', '1024w', 'FatFS 环形日志 + 断网续传'],
       ['monitor', '1', '1s', '512w', '栈水位 / 喂狗 / 健康自检']],
      [2.8, 1.6, 1.8, 1.6, 8.2])

h2('5.2 三级告警状态机')
add_image('fig6_statemachine.png', '图 8  三级告警状态机（含迟滞与故障锁存）')
body('触发阈值 45°C / 60°C / 75°C（均需持续 3 秒确认以滤除毛刺），'
     '解除阈值下移 5°C 形成迟滞带；硬件故障（温控开关动作、过流、传感器失效）进入故障锁存态，'
     '禁止自动恢复加热，需人工复位 —— 这是针对 KSD9700 自动复位特性必须做的设计。')

h2('5.3 多源融合故障诊断')
add_image('fig7_diagnosis.png', '图 9  多源融合故障诊断判据流程')
body('诊断算法以 1 秒为周期，依次执行六条判据，可区分加热回路断路、短路过流、'
     'MOS 击穿直通、传感器漂移、散热异常五类故障，并给出元件老化趋势预警。'
     '本阶段还将引入一阶热模型残差分析（C·dT/dt = P − k·(T − T_amb)），'
     '以模型预测值与实测值的残差作为判据，替代单纯的阈值比较，'
     '使"多源融合"具备理论依据而非多个 if 的堆叠。')

# ==================================================================
h1('六、关键技术难点与解决方案')
table(['序号', '技术难点', '解决方案'],
      [['1', 'STM32H750 片内 Flash 仅 128KB，固件远超此限',
        '使用 QSPI Flash 下载算法将程序烧至外部 Flash（0x90000000）并以 XIP 方式运行'],
       ['2', '出厂板载 Flash 为 BY25Q128（博雅），OpenOCD 支持不完整',
        '改用 Keil MDK 或 STM32CubeIDE 配合正点原子官方下载算法'],
       ['3', 'H7 的 D-Cache 与 DMA 一致性问题',
        'DMA 收发前后执行 Cache 清洁/失效；或将缓冲置于 non-cacheable 区'],
       ['4', '通用 SPI 屏幕没有现成驱动可用',
        '自研 ILI9341 SPI 驱动，含初始化序列、局部刷新区机制与触摸读取'],
       ['5', '3.3V 驱动 IRF520 会停在线性区，发热且使电流读数漂移',
        '改用 IRLZ44N 逻辑电平 MOS（Vgs(th) 1~2V），并实测 Vds 验证完全导通'],
       ['6', '面包板接触电阻影响 0.1Ω 分流测量精度',
        '改用 INA219 模块（板载精密分流 + 螺钉端子），消除自制测量链路'],
       ['7', '加热电流在地线上产生压降，污染电流采样',
        '星型接地 + 加热级独立供电 + INA219 高边接法'],
       ['8', 'KSD9700 自动复位会导致"加热-断开-再加热"振荡',
        '固件对硬件故障执行锁存，禁止自动恢复加热，需人工复位'],
       ['9', 'kHz 级 PWM 会使 INA219 采到脉冲电流',
        '采用 1Hz 时间比例控制，在导通期稳定采样，同时降低开关损耗']],
      [1.3, 5.6, 9.1])

# ==================================================================
h1('七、风险管理')
table(['风险项', '等级', '应对措施'],
      [['时间被证书考试挤占（软考/蓝桥杯/嵌入式设计师）', '高',
        '阶段划分已避开 2026.11 与 2027.4–5；锁定 2 个比赛，不贪多'],
       ['H750 工具链卡壳超过 2 周', '高',
        'P0 单独立项并设 2 周上限；卡壳立即求助官方论坛与技术支持'],
       ['TFT 为通用 SPI 屏需自研驱动', '中',
        '预留 1 周缓冲；必要时先降级为点阵字符屏/串口屏完成联调'],
       ['故障诊断被评委质疑为"阈值堆砌"', '中',
        '引入一阶热模型残差分析作为判据，准备理论推导与实测曲线'],
       ['整机在演示现场出现接触不良', '中',
        '扎带固定线束；关键节点改用弹簧端子；准备备用跳线'],
       ['独自参赛导致材料/视频工作量过大', '中',
        '材料模板化、一次生产多处分发；优先保证 2 个核心比赛的质量'],
       ['赛后阶段动力下降', '低',
        '把工程日志与面试话术作为 P6 的正式交付物，与求职直接绑定']],
      [6.2, 1.4, 8.4])

# ==================================================================
h1('八、资源需求与预算')
table(['类别', '内容', '预算'],
      [['主控与显示', '正点原子 MiniPro H750 + 2.8" TFT', '已购'],
       ['传感器', 'SHT30×2、DS18B20×2、INA219、ADS1115（备用）', '已购'],
       ['通信', 'W5500、ESP-01S', '已购'],
       ['功率级', 'IRLZ44N、10Ω/10W 水泥电阻×5、PPTC 1.1A、KSD9700 70°C', '已购'],
       ['结构', '亚克力盒 30×20×20、M3 铜柱、导热铝片、扎带、弹簧端子', '已购'],
       ['工具', '数字万用表、面包板、杜邦线', '已购'],
       ['待补', '第二个 5V/2A 电源、M3 尼龙柱、公对公杜邦线', '约 35 元']],
      [3.0, 10.0, 3.0])
body('本项目以"零焊接"为装配原则：功率回路各元件引线通过按压式快速接线端子串接，'
     'INA219 使用自带螺钉端子接线，MOS 管与栅极电阻直接插入面包板，各功能模块以杜邦线互联。'
     '全部硬件已完成采购，无需额外投入。')

# ==================================================================
h1('九、预期成果')
body('（1）一套可运行的模拟控制柜健康监测系统整机，具备多源采集、故障诊断、'
     '分级告警、工业协议通信与云端可视化的完整能力，并通过 72 小时连续运行验证。')
body('（2）一套可复用的嵌入式工程资产：模块化驱动层、FreeRTOS 任务框架、'
     '故障诊断算法模块与技术文档，可作为后续项目的基础。')
body('（3）完整的参赛与求职材料：技术方案文档、申报书、演示视频、答辩 PPT、'
     '以及一份记录全部调试过程的工程日志。')
body('（4）一份可写入求职简历的核心项目经历，并具备应对 5 层技术追问的能力 ——'
     '包括任务调度设计、总线保护、故障诊断理论依据、断网续传一致性与平台特性处理。')

# ==================================================================
h1('十、参赛规划')
table(['定位', '比赛', '评审形式', '目标'],
      [['主攻', '全国大学生嵌入式芯片与系统设计竞赛', '初赛线上材料评审', '省级及以上奖项'],
       ['强推', '全国大学生计算机设计大赛（物联网应用）', '省赛线上评审 + 视频', '省级奖项'],
       ['可选', '挑战杯大学生课外学术科技作品竞赛', '省赛线下答辩', '视指导老师情况'],
       ['顺手', '全国高校计算机能力挑战赛', '全程线上', '热身，不计主力']],
      [1.8, 7.0, 4.2, 3.0])
body('筛选原则：比赛必须在 2027 年 9 月秋招之前拿到奖项，否则写不进求职简历；'
     '同时因 2027 年 7 月需赴广州参加培训，优先选择线上评审的形式。'
     '针对线上评审，评委只能依据材料与视频打分，因此演示视频的质量被提到最高优先级。')

doc.save(OUT)
print('SAVED', os.path.basename(OUT))
print('SIZE_KB', round(os.path.getsize(OUT) / 1024, 1))
print('PARAS', len(doc.paragraphs))
