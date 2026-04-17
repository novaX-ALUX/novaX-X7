## X7.SCH Hardware Pin Map — Altium SchDoc 바이너리 추출 기반

> Source: X7.SCH (Altium Designer, 26-Jan-2026), OLE Compound 바이너리에서 텍스트 추출
> MCU: STM32F407VGT6 (U8, LQFP100)
> BoxerV22 회로도(T14-PCB.pdf)와 **동일 설계** 확인됨

---

### 부품 목록 (X7.SCH에서 추출된 고유 IC/모듈)

| 부품 | 파트넘버 | 수량 | 비고 |
|------|---------|------|------|
| MCU | STM32F407VGT6 | 1 | LQFP100 |
| EEPROM | M24256-BRMN6TP | 1 | 256Kbit=32KB, I2C, PB8(SCL)/PB9(SDA) |
| 충전 IC | IP2326 | 1 | 2S 리포 충전, USB-C J28에서 VBUS2 입력 |
| 5V 부스트 | CN3903 | 1 | VBAT→+5V |
| 3.3V LDO | AZ1117CR-3.3TRG1 | 1 | +5V→+3.3V (1A) |
| SD LDO | ME6232C33M5G | 1 | SD 카드 전용 3.3V |
| 코프로세서 LDO | ME6209A33M3G | 1 | STC15W408AS 전용 3.3V |
| 코프로세서 | STC15W408AS-35I-SOP16 | 1 | LED1~7 구동, SW5/7/10/12/15/17 |
| 오디오 앰프 | AD8002D | 1 | 듀얼 Op-Amp |
| USB ESD | SR05.TCT | 2 | J13(데이터) + J28(충전) 각 1개 |
| UART 인버터 | SN74LVC1G04DBVR | 4 | U9/U11(S.PORT), U12/U13(외장모듈) |
| UART 버퍼 | SN74LVC1G126DBVR | 1 | U10(S.PORT DIR) |
| P-FET | AO3407-CN | 4 | Q2(내부모듈), Q9(외장모듈), Q12/Q13(배터리) |
| NPN | SS8050 | 10 | Q1(햅틱), Q3~Q8(전원/LED), Q10(트레이너), Q11(배터리) |
| PNP | SS8550 | 1 | Q15(배터리 경로) |
| USB-C | TYPEC-304J-BCP16 | 2 | J13(데이터) + J28(충전) |
| TVS | SMFJ5.0CA | 8 | ESD/서지 보호 |
| Schottky | B5819W | 5 | 역류 방지 |
| LED | LED4 (tri-color) | 1 | 绿红蓝 (PA7/PE13/PE2) |
| LED | LED (오렌지) | 7 | LED1~7, 코프로세서 구동 |

---

### MCU 전체 핀맵

#### PA 포트

| GPIO | Pin# | 기능 | 서브시스템 | 넷 연결 확인 |
|------|------|------|-----------|-------------|
| PA0 | 23 | ADC1_IN0 — 스틱 | ADC | VREF+/VREF- 기준, 51R+1nF 필터 |
| PA1 | 24 | ADC1_IN1 — 스틱 | ADC | VREF+/VREF- 기준, 51R+1nF 필터 |
| PA2 | 25 | ADC1_IN2 — 스틱 | ADC | VREF+/VREF- 기준, 51R+1nF 필터 |
| PA3 | 26 | ADC1_IN3 — 스틱 | ADC | VREF+/VREF- 기준, 51R+1nF 필터 |
| PA4 | 29 | DAC1 오디오 출력 | Audio | →R(220R)→AD8002D +IN |
| PA5 | 30 | ADC1_IN5 — 6-POS 스위치 | ADC | R(10K)+R(10K) 분압, C(100nF) |
| PA6 | 31 | ADC1_IN6 — POT2 | ADC | R(220R)+C(100nF) 필터 |
| PA7 | 32 | LED GREEN (LED4A) | LED | R(220R) 직접 구동 |
| PA8 | 67 | Trainer Detect | Trainer | R(100R)+R(100K) pullup+C(100nF) |
| PA9 | 68 | USB VBUS sense | USB | USB OTG FS VBUS |
| PA10 | 69 | KEY_ENTER (SW3) | Button | R(220R)+C(100nF) 디바운스 |
| PA11 | 70 | USB DM | USB | R(22R)+SR05.TCT ESD |
| PA12 | 71 | USB DP | USB | R(22R)+SR05.TCT ESD |
| PA13 | 72 | SWDIO | SWD | NC1 10핀 헤더 |
| PA14 | 76 | SWCLK | SWD | R(10K) pullup |
| PA15 | 77 | LCD CS (SPI3) | LCD | R(33R) 시리즈 |

#### PB 포트

| GPIO | Pin# | 기능 | 서브시스템 | 넷 연결 확인 |
|------|------|------|-----------|-------------|
| PB0 | 35 | ADC1_IN8 — POT1 | ADC | R(220R)+C(100nF) 필터 |
| PB1 | 36 | 내부 모듈 Boot | Int Module | Q5(SS8050), R(2.2K)+R(2K) |
| PB2 | 37 | BOOT1 | System | LOW 고정 |
| PB3 | 89 | Haptic PWM (TIM2_CH2) | Haptic | R(2.2K)→Q1(SS8050)→M1, +5V |
| PB4 | 90 | KEY_SYS | Button | GPIO Input |
| PB5 | 91 | Charger Status | Power | R(10K) pullup, Q8(SS8050) |
| PB6 | 92 | USART1_TX (내부 CRSF) | Int Module | 직결 |
| PB7 | 93 | USART1_RX (내부 CRSF) | Int Module | 직결 |
| PB8 | 95 | I2C1_SCL (EEPROM) | I2C | R(4.7K) pullup to 3.3V |
| PB9 | 96 | I2C1_SDA (EEPROM) | I2C | R(4.7K) pullup to 3.3V |
| PB10 | 47 | USART3_TX (BLE/AUX) | BLE | J36 pin2 |
| PB11 | 48 | USART3_RX (BLE/AUX) | BLE | J36 pin3 |
| PB12 | 51 | SD CS (SPI2_NSS) | SD | R(33R) 시리즈 |
| PB13 | 52 | SD SCK (SPI2_SCK) | SD | R(33R) 시리즈 |
| PB14 | 53 | SD MISO (SPI2_MISO) | SD | R(33R) 시리즈 |
| PB15 | 54 | SD MOSI (SPI2_MOSI) | SD | R(33R) 시리즈 |

#### PC 포트

| GPIO | Pin# | 기능 | 서브시스템 | 넷 연결 확인 |
|------|------|------|-----------|-------------|
| PC0 | 15 | ADC1_IN10 — VBAT | ADC | R(499K)+R(160K) 분압, C(100nF) |
| PC1 | 16 | TRIM_T1_INC | Trim | GPIO Input (STM32 직결) |
| PC2 | 17 | TRIM_T3_INC | Trim | GPIO Input (STM32 직결) |
| PC3 | 18 | TRIM_T3_DEC | Trim | GPIO Input (STM32 직결) |
| PC4 | 33 | 내부 모듈 전원 | Int Module | Q3(SS8050)→Q2(AO3407) P-FET |
| PC5 | 34 | KEY_EXIT | Button | R(220R)+C(100nF) 디바운스 |
| PC6 | 63 | USART6_TX (외장 모듈) | Ext Module | →U13(SN74LVC1G04) 인버터→J27 |
| PC7 | 64 | USART6_RX (외장 모듈) | Ext Module | J27→U12(SN74LVC1G04) 인버터→ |
| PC8 | 65 | Trainer OUT (TIM3_CH3) | Trainer | Q10(SS8050) open-collector |
| PC9 | 66 | Trainer IN (TIM3_CH4) | Trainer | R(2K)+R(100K) pullup |
| PC10 | 78 | LCD SCK (SPI3_SCK) | LCD | R(33R) 시리즈 |
| PC11 | 79 | LCD DC/A0 | LCD | R(33R) 시리즈, GPIO |
| PC12 | 80 | LCD MOSI (SPI3_MOSI) | LCD | R(33R) 시리즈 |
| PC13 | 7 | Switch SA (2POS) | Switch | J1/J37 (6-pin) |
| PC14 | 8 | LSE OSC32_IN | Clock | Y2 32.768kHz |
| PC15 | 9 | LSE OSC32_OUT | Clock | |

#### PD 포트

| GPIO | Pin# | 기능 | 서브시스템 | 넷 연결 확인 |
|------|------|------|-----------|-------------|
| PD0 | 81 | PWR_ON 래치 | Power | R(2.2K)→Q4(SS8050), R(2K) GND |
| PD1 | 82 | PWR_SWITCH 입력 | Power | SW11, R(4.7K)+R(47K), C(100nF) |
| PD2 | 83 | KEY_TELE | Button | R(220R)+C(100nF) |
| PD3 | 84 | KEY_PAGEUP | Button | R(220R)+C(100nF) |
| PD4 | 85 | S.PORT DIR | Telemetry | →U10(SN74LVC1G126) OE, R(10K) pulldown |
| PD5 | 86 | S.PORT TX (USART2) | Telemetry | →U9(SN74LVC1G04) 인버터 |
| PD6 | 87 | S.PORT RX (USART2) | Telemetry | ←U11(SN74LVC1G04) 인버터, R(10K) pullup |
| PD7 | 88 | KEY_PAGEDN | Button | R(220R)+C(100nF) |
| PD8 | 55 | 외장 모듈 전원 | Ext Module | Q7(SS8050)→Q9(AO3407) P-FET, F3 퓨즈 |
| PD9 | 56 | SD Detect | SD | GPIO Input |
| PD10 | 57 | EEPROM WP | I2C | →M24256 WC# (Write Protect) |
| PD11 | 58 | Switch SC_H (3POS) | Switch | J11 (3-pin), R(220R)+C(100nF) |
| PD12 | 59 | LCD RST | LCD | R(33R) 시리즈 |
| PD13 | 60 | LCD 백라이트 PWM | LCD | Q6(SS8050), R(2K)/R(2.2K) |
| PD14 | 61 | (미확인) | — | 회로도에서 넷 라벨 미추출 |
| PD15 | 62 | TRIM_T1_DEC | Trim | GPIO Input (STM32 직결) |

#### PE 포트

| GPIO | Pin# | 기능 | 서브시스템 | 넷 연결 확인 |
|------|------|------|-----------|-------------|
| PE0 | 97 | Switch SC_L (3POS) | Switch | J11 (3-pin), R(220R)+C(100nF) |
| PE1 | 98 | Switch SF (2POS) | Switch | R(220R)+C(100nF) |
| PE2 | 1 | LED BLUE (LED4C) | LED | Q14(SS8050) 트랜지스터 구동 |
| PE3 | 2 | TRIM_T4_DEC | Trim | GPIO Input (STM32 직결) |
| PE4 | 3 | TRIM_T4_INC | Trim | GPIO Input (STM32 직결) |
| PE5 | 4 | TRIM_T2_INC | Trim | GPIO Input (STM32 직결) |
| PE6 | 5 | TRIM_T2_DEC | Trim | GPIO Input (STM32 직결) |
| PE7 | 38 | Switch SB_H (3POS) | Switch | J29 (3-pin), R(220R)+C(100nF) |
| PE8 | 39 | Switch SD (2POS) | Switch | J2 (PS-22F03), R(220R)+C(100nF) |
| PE9 | 40 | Rotary Encoder A | Encoder | J38 (ALPS 3-pin) |
| PE10 | 41 | Rotary Encoder B | Encoder | J38 (ALPS 3-pin) |
| PE11 | 42 | KEY_MDL | Button | R(220R)+C(100nF) |
| PE12 | 43 | Audio AMP Mute | Audio | →AD8002D SHUTDOWN(pin1) |
| PE13 | 44 | LED RED (LED4B) | LED | R(2.2K)+R(220R) 저전류 직접 구동 |
| PE14 | 45 | Switch SE (2POS) | Switch | SW1 (PS-22F03)+J32, R(220R)+C(100nF) |
| PE15 | 46 | Switch SB_L (3POS) | Switch | J29 (3-pin), R(220R)+C(100nF) |

---

### BoxerV22 대비 X7.SCH 차이점

**X7.SCH와 BoxerV22는 동일한 Boxer V22 하드웨어 설계 기반이다.**
X7.SCH에서 추출된 부품 목록, 넷 라벨, MCU 핀맵이 BoxerV22.md/BoxerV22.pdf와 완전히 일치한다.

확인된 동일 사항:
- MCU: STM32F407VGT6 (LQFP100) — 동일
- EEPROM: M24256-BRMN6TP (PB8/PB9, I2C1) — 동일
- 코프로세서: STC15W408AS + ME6209A33M3G — 동일
- 충전: IP2326 + 듀얼 USB-C (TYPEC-304J-BCP16 x2) — 동일
- 전원: CN3903(5V) + AZ1117CR-3.3(3.3V) + ME6232C33M5G(SD) — 동일
- 인버터: SN74LVC1G04 x4 + SN74LVC1G126 x1 — 동일
- 보호: SMFJ5.0CA x8 + B5819W x5 + SR05.TCT x2 — 동일
- 오디오: AD8002D + PA4(DAC) + PE12(Mute) — 동일
- 모든 GPIO 핀맵 — 동일

X7.SCH 바이너리에서 추출 불가능한 항목:
- PD14(pin 61): 넷 라벨이 추출되지 않음 (미확인)
- 일부 수동 부품의 Designator(R#, C# 번호): 바이너리 구조상 매칭 제한적
- HSE 크리스탈 주파수: Y_4/Y_4_1 심볼만 확인, 주파수 값 미추출

---

### 결론

X7.SCH는 BoxerV22(T14-PCB)와 **동일한 회로 설계**이며,
Altium Designer에서 작성된 X7.SCH가 원본이고,
BoxerV22.md는 이 회로도의 PDF 추출 분석 문서다.
