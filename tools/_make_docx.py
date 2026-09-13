# -*- coding: utf-8 -*-
"""生成《项目步骤图与流程图》Word 文档"""
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
OUT = os.path.join(BASE, '控制柜监测告警系统-项目步骤图与流程图.docx')

doc = Document()

# 页面 A4
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


def body(text, indent=True, size=12):
    p = doc.add_paragraph()
    p.paragraph_format.line_spacing_rule = WD_LINE_SPACING.ONE_POINT_FIVE
    p.paragraph_format.space_after = Pt(0)
    p.alignment = WD_ALIGN_PARAGRAPH.JUSTIFY
    if indent:
        pPr = p._p.get_or_add_pPr()
        ind = pPr.find(qn('w:ind'))
        if ind is None:
            ind = OxmlElement('w:ind'); pPr.append(ind)
        ind.set(qn('w:firstLineChars'), '200')
    set_run(p.add_run(text), size=size)
    return p


def h1(text):
    p = doc.add_paragraph()
    p.paragraph_format.space_before = Pt(16)
    p.paragraph_format.space_after = Pt(8)
    set_run(p.add_run(text), east='黑体', west='SimHei', size=16, bold=True)
    return p


def h2(text):
    p = doc.add_paragraph()
    p.paragraph_format.space_before = Pt(10)
    p.paragraph_format.space_after = Pt(6)
    set_run(p.add_run(text), east='黑体', west='SimHei', size=13.5, bold=True)
    return p


def add_image(name, caption, max_w=USABLE, max_h=19.5):
    path = os.path.join(IMG, name)
    w, h = Image.open(path).size
    ratio = h / w
    width = max_w
    height = width * ratio
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


def table(headers, rows, widths=None):
    t = doc.add_table(rows=1, cols=len(headers))
    t.style = 'Table Grid'
    t.alignment = WD_TABLE_ALIGNMENT.CENTER
    hdr = t.rows[0].cells
    for i, htxt in enumerate(headers):
        hdr[i].text = ''
        p = hdr[i].paragraphs[0]
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        set_run(p.add_run(htxt), east='黑体', size=10.5, bold=True)
        shd = OxmlElement('w:shd')
        shd.set(qn('w:fill'), 'DCE9F7')
        hdr[i]._tc.get_or_add_tcPr().append(shd)
    for row in rows:
        cells = t.add_row().cells
        for i, v in enumerate(row):
            cells[i].text = ''
            p = cells[i].paragraphs[0]
            p.alignment = WD_ALIGN_PARAGRAPH.CENTER if i == 0 else WD_ALIGN_PARAGRAPH.LEFT
            set_run(p.add_run(str(v)), size=10.5)
    if widths:
        for r in t.rows:
            for i, wcm in enumerate(widths):
                r.cells[i].width = Cm(wcm)
    doc.add_paragraph().paragraph_format.space_after = Pt(6)


# ============================================================
# 封面标题
# ============================================================
p = doc.add_paragraph(); p.alignment = WD_ALIGN_PARAGRAPH.CENTER
p.paragraph_format.space_after = Pt(4)
set_run(p.add_run('模拟控制柜健康监测与故障诊断系统'), east='黑体', west='SimHei', size=22, bold=True)

p = doc.add_paragraph(); p.alignment = WD_ALIGN_PARAGRAPH.CENTER
p.paragraph_format.space_after = Pt(4)
set_run(p.add_run('—— 项目步骤图与流程图'), east='黑体', west='SimHei', size=15)

p = doc.add_paragraph(); p.alignment = WD_ALIGN_PARAGRAPH.CENTER
p.paragraph_format.space_after = Pt(16)
set_run(p.add_run('主控：正点原子 MiniPro H750（STM32H750VB） ｜ 实时系统：FreeRTOS ｜ 版本 v0.3（零焊接方案）'),
        east='楷体', size=11, color=(0x55, 0x55, 0x55))

# ============================================================
h1('一、系统总体方案')

h2('1.1 项目概述')
body('本系统面向"模拟电气控制柜"场景，构建一套集多源传感采集、边缘故障诊断、分级声光告警、工业协议通信与云端可视化于一体的设备健康监测系统。'
     '系统以 STM32H750VB（480MHz Cortex-M7，1MB RAM，16MB 外部 QSPI Flash）为主控，运行 FreeRTOS 实时操作系统；'
     '通过双路 SHT30 温湿度传感器、DS18B20 负载本体温度探头与 INA219 高边电流传感器采集设备运行状态；'
     '由逻辑电平 MOS 管（IRLZ44N）驱动 10Ω/10W 水泥电阻模拟发热负载与故障热源；'
     '最终通过 W5500 以太网或 ESP-01S 无线模块，以 Modbus TCP 从站与 MQTT 双协议对外通信。')
body('与传统"温度超限即报警"的简易方案不同，本系统通过温度、电流、输出占空比三类信号的交叉判据，'
     '可区分加热回路断路、短路过流、MOS 击穿直通、传感器漂移、散热异常等五类故障；'
     '并利用 R = U / I 的长期趋势监测水泥电阻阻值漂移，实现从"故障报警"到"健康度退化预警"的升级。')

h2('1.2 系统总体架构与数据流')
add_image('fig1_architecture.png', '图1  系统总体架构与数据流（端 - 边 - 云）')
body('系统采用"端 - 边 - 云"三层结构：感知层以 100ms 周期采集环境温度、负载本体温度、局部温度与回路电流；'
     '边缘控制层由 STM32H750 完成数据融合、故障诊断、告警决策、本地显示与加热控制；'
     '通信层提供 W5500 以太网主链路与 ESP-01S 无线备用链路；'
     '应用层通过 Modbus TCP 接入 SCADA/PLC/组态软件，同时经 MQTT 上云，由可视化大屏完成趋势分析。')

h2('1.3 硬件结构与总线分配')
add_image('fig2_hardware.png', '图2  硬件结构图与总线分配')
body('主控外围按总线类型划分：I2C1 挂载 INA219（0x40）与 SHT30 #1（0x44），I2C2 挂载 SHT30 #2；'
     'DS18B20 采用单总线并配 4.7kΩ 上拉；SPI1 作为共享总线，由 TFT、W5500 分片选访问，'
     '固件中以互斥量保护并设置 50ms 取锁超时，避免任何任务长期阻塞总线。')
body('需特别注意：板载 16MB QSPI Flash 占用 PB2、PB6、PD11~PD13、PE2 共 6 个引脚，'
     '程序本身即运行于该外部 Flash（映射地址 0x90000000），这些引脚绝不可挪用；'
     '此外由于板上将 PA15 与 PB4 用作 GPIO，调试接口必须选择 SWD（Serial Wire）而非 JTAG 5-pin。')

# ============================================================
h1('二、功率级与安全设计')

h2('2.1 加热功率级接线')
add_image('fig3_power.png', '图3  加热功率级接线与三重安全链')
body('加热回路自 5V 电源起，依次串联自恢复保险丝 PPTC（1.1A）、常闭型温控开关 KSD9700（70°C）、'
     'INA219 电流传感器（内置 0.1Ω 精密分流电阻），再经 10Ω/10W 水泥电阻与 IRLZ44N 低边开关管回到星型接地点。'
     'MOS 栅极由 PA8（TIM1_CH1）经 100Ω 串阻驱动，并配置 10kΩ 下拉电阻 —— 后者用于保证上电至 GPIO 初始化期间'
     '栅极不被悬空，避免加热器被意外点着。')
body('加热控制采用 1Hz 慢速时间比例方式（如 30% 占空比即导通 300ms、关断 700ms），'
     '而非 kHz 级 PWM。其好处有三：一是使 INA219 能在导通期稳定采样，保证电流测量准确；'
     '二是 MOSFET 开关损耗趋近于零；三是避免高频开关干扰 I2C/SPI 总线。'
     '10W 水泥电阻的热时间常数为数十秒量级，完全不需要高频调制。')

h2('2.2 三重安全冗余与失效安全')
body('系统设置三重独立保护：软件层由独立看门狗 IWDG 与告警状态机负责，检测到紧急状态立即拉低 PWM；'
     '硬件层由 KSD9700 双金属片温控开关在 70°C 时物理断开加热回路，即使软件完全失效仍能切断加热；'
     '回路层由 PPTC 自恢复保险丝承担短路保护。')
body('任何异常情形（传感器无响应、I2C 总线锁死、看门狗复位、过流）一律执行失效安全（fail-safe）动作，'
     '即无条件切断加热 —— 宁可误停，不可炸机。需特别强调：KSD9700 为自动复位型，'
     '温度回落约 55°C 后会自行闭合，因此固件必须对硬件故障进行锁存，禁止自动恢复加热，'
     '否则将出现"加热 - 断开 - 再加热"的振荡循环。')

# ============================================================
h1('三、实施步骤与进度安排')

h2('3.1 项目实施步骤流程')
add_image('fig4_steps.png', '图4  项目实施步骤流程（P0 → P7）')

h2('3.2 阶段任务与里程碑')
table(
    ['阶段', '名称', '时间', '里程碑'],
    [['P0', '硬件自检与工具链打通', '2026.09下–10上', '串口打印 / RGBLED / TFT 三个里程碑；引脚表定稿'],
     ['P1', '外设逐个点亮', '2026.10（3周）', 'SHT30 / INA219 / DS18B20 / TF卡 / W5500 五个独立 demo'],
     ['—', '软考中级（项目挂起）', '2026.11', '—'],
     ['P2', 'RTOS 任务架构', '2026.12（3周）', '8 任务 + 队列 + SPI 互斥量；采集 1h 零复位'],
     ['P3', '告警与故障诊断', '2027.01 寒假', '告警状态机 + 迟滞 + 趋势预警 + 故障注入全过'],
     ['P4', '通信与上云', '2027.02 寒假', 'Modbus TCP 从站 + MQTT + 断网续传'],
     ['P5', '结构装配与安全链', '2027.02（1周）', '亚克力装配 + 温控开关 + 保险丝 + 线束整理'],
     ['P6', '大屏与参赛材料', '2027.03（4周）', '可视化大屏 + 多套申报书 + 演示视频'],
     ['P7', '参赛', '2027.04–07', '蓝桥杯(4月) / 报名 / 省赛答辩 / 嵌入式设计师(5月)']],
    widths=[1.6, 4.2, 3.2, 7.0])
body('其中 P0 为最关键环节：STM32H750 片内 Flash 仅 128KB，而 FreeRTOS + lwIP + FatFS + 图形库的固件体积远超此限，'
     '必须借助 QSPI Flash 下载算法将程序烧录至外部 Flash（0x90000000）并以 XIP 方式运行。'
     '此外，出厂板载 Flash 为 BY25Q128（博雅），OpenOCD 对其支持不完整，应使用 Keil MDK 或 STM32CubeIDE 配合官方下载算法。')

# ============================================================
h1('四、固件软件设计')

h2('4.1 任务架构')
add_image('fig5_tasks.png', '图5  固件软件架构（FreeRTOS 8 任务）')
body('固件划分为 8 个任务，按实时性要求分配优先级。加热控制任务周期最短（10ms）且优先级最高，'
     '以保证时间比例控制的时基精度；告警状态机与采集任务居中；通信、显示与存储任务优先级较低但栈需求更大。')

h2('4.2 任务清单')
table(
    ['任务', '优先级', '周期', '栈', '职责'],
    [['heater_ctrl', '8', '10ms', '512w', '1Hz 时间比例控制 + 双通道电流互锁'],
     ['alarm_fsm', '7', '100ms', '768w', '告警状态机 + 迟滞 + 趋势预警 + 故障码'],
     ['sensor', '6', '100ms', '768w', 'I2C 读 2×SHT30 + INA219；单总线读 DS18B20'],
     ['diagnose', '5', '1s', '1024w', '多源融合判据 + R=U/I 老化趋势分析'],
     ['net_w5500', '4', '事件', '2048w', 'Modbus TCP 从站 + MQTT 客户端（lwIP）'],
     ['ui_lvgl', '3', '200ms', '2048w', '环形缓冲区日志 + 断网补传'],
     ['storage', '2', '1s', '1024w', 'FatFS 环形日志 + 断网续传'],
     ['monitor', '1', '1s', '512w', '栈水位 / CPU 统计 / 喂狗 / 健康自检']],
    widths=[2.8, 1.6, 1.8, 1.6, 8.2])
body('任务间通信使用队列传递数据、互斥量保护 SPI1 共享总线、任务通知用于中断唤醒。'
     '中断服务程序只执行 xTaskNotifyFromISR，绝不在其中访问 SPI/I2C，也绝不调用任何阻塞 API。'
     '所有任务栈大小均通过 uxTaskGetStackHighWaterMark() 实测调优，保证剩余水位不低于 30%。')

# ============================================================
h1('五、核心算法设计')

h2('5.1 三级告警状态机')
add_image('fig6_statemachine.png', '图6  三级告警状态机（含迟滞与故障锁存）')
body('告警状态机设置正常、预警、告警、紧急四个温度相关状态，并以故障锁存状态承接一切硬件异常。'
     '触发阈值分别为 45°C、60°C、75°C（均需持续 3 秒确认以滤除单点毛刺），'
     '解除阈值则下移 5°C 形成迟滞带（40°C、55°C），避免温度在阈值附近波动时反复报警。'
     '三级告警分别对应板载 RGB 指示灯的三色输出与无源蜂鸣器的不同音调。')

h2('5.2 多源融合故障诊断')
add_image('fig7_diagnosis.png', '图7  多源融合故障诊断判据流程')
body('诊断算法以 1 秒为周期采集环境温度、负载温度、局部温度、回路电流、负载电压与输出占空比，'
     '依次执行六条判据：当占空比大于零而电流近似为零并持续 2 秒时，判定加热回路断路；'
     '当电流超过 2.5A 时判定短路过流并立即切断；当占空比为零而电流仍不为零时判定 MOS 击穿直通；'
     '当两路温度读数偏差超过 5°C 时判定传感器漂移；当电流正常而温升速率超限时判定散热异常；'
     '当由 R = V / I 得到的负载阻值在滑动窗口内持续上升时，给出元件老化趋势预警。')
body('这一设计将传统的单阈值报警升级为多源交叉判据，是本项目的主要技术特色；'
     '其中阻值趋势判据利用了水泥电阻长期高温后阻值缓慢漂移的物理特性，'
     '使系统具备"健康度退化预测"能力，而非仅仅在故障发生后才报警。')

# ============================================================
h1('六、硬件制作步骤')
add_image('fig8_build.png', '图8  硬件制作步骤（12 步）')
body('制作流程按"先软件、后硬件；先控制、后功率"的顺序展开。步骤 1~5 属于工具链与基础外设验证，'
     '不需要任何新增元件；步骤 6 开始搭建控制电路；步骤 7 之后才涉及加热功率回路。'
     '在接通加热回路之前，必须用万用表通断档确认 5V 与 GND 之间不存在短路，'
     '并确认 KSD9700 在常温下处于导通状态（验证其为常闭型）。')
body('全部装配过程采用免焊接方案：功率回路的各元件引线通过按压式快速接线端子串接，'
     'INA219 使用自带螺钉端子接线，MOS 管与栅极电阻直接插入面包板，各功能模块以杜邦线互联。')

# ============================================================
h1('七、关键参数汇总')

h2('7.1 电源预算（5V / 2.5A）')
table(
    ['负载项', '电流'],
    [['H750 主板（480MHz M7）', '约 250 mA'],
     ['2.8" TFT（含背光）', '约 130 mA'],
     ['W5500 以太网模块', '约 150 mA'],
     ['SHT30×2 + INA219 + 触摸控制器', '＜ 5 mA'],
     ['无源蜂鸣器 + 指示灯', '约 90 mA'],
     ['加热通道 A（10Ω 水泥电阻）', '500 mA'],
     ['合计（单通道加热）', '约 1.13 A（余量 1.37 A）']],
    widths=[10.0, 6.0])

h2('7.2 温度与电流阈值')
table(
    ['参数', '数值', '说明'],
    [['预警触发 / 解除', '45°C / 40°C', '迟滞 5°C，需持续 3s 确认'],
     ['告警触发 / 解除', '60°C / 55°C', '迟滞 5°C，需持续 3s 确认'],
     ['紧急触发', '75°C', '立即切断加热并锁存'],
     ['硬件过温保护', '70°C', 'KSD9700 物理断开加热回路'],
     ['断路判定', 'duty>0 且 I<0.05A 持续 2s', '加热丝断 / 端子松 / 保险丝熔断'],
     ['过流判定', 'I > 2.5A', '立即切断 MOS'],
     ['分流电阻', '0.1Ω / 1%（INA219 板载）', '0.5A 时压降 50mV'],
     ['加热控制方式', '1Hz 时间比例控制', '占空比 30% 即导通 300ms']],
    widths=[4.0, 5.4, 6.6])

doc.save(OUT)
print('SAVED', os.path.basename(OUT))
print('SIZE_KB', round(os.path.getsize(OUT) / 1024, 1))
print('PARAGRAPHS', len(doc.paragraphs))
