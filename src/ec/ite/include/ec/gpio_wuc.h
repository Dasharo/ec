// SPDX-License-Identifier: GPL-3.0-only

// IT5570E GPIO → WUC → INTC mapping table
//
// For each GPIO pin, three macros define the interrupt routing:
//   _GPIO_WUC_GROUP_<PORT><PIN>  WUC group number (0 = no WUC / no INTC routing)
//   _GPIO_WUC_BIT_<PORT><PIN>    Bit position within WUEMR/WUESR (0-7)
//   _GPIO_WUC_IRQ_<PORT><PIN>    INTC IRQ number (= IER_reg * 8 + IER_bit)
//
// Source: IT5570E datasheet Table 7-6 "WUC Input Assignments"
// WUC bit formula: bit = (WUxx - group_start) where group_start = (group * 10) for
// groups 1-8, or (group * 8 + offset) for groups 9-14 (WU88-WU135).
// IRQ formula: IRQ = IER_reg * 8 + IER_bit
//
// These macros are used by the GPIO() initializer macro in gpio.h to populate
// the wuc_group, wuc_bit, and irq fields of struct Gpio.

#ifndef _EC_GPIO_WUC_H
#define _EC_GPIO_WUC_H

// clang-format off

// -------------------------------------------------------------------------
// Port A
// -------------------------------------------------------------------------
// GPA0: WU91 (Group 9, bit 3), WKO[91] → INT96 → IER12[0]
#define _GPIO_WUC_GROUP_A0   9
#define _GPIO_WUC_BIT_A0     3
#define _GPIO_WUC_IRQ_A0     96

// GPA1: WU92 (Group 9, bit 4), WKO[92] → INT97 → IER12[1]
#define _GPIO_WUC_GROUP_A1   9
#define _GPIO_WUC_BIT_A1     4
#define _GPIO_WUC_IRQ_A1     97

// GPA2: WU93 (Group 9, bit 5), WKO[93] → INT98 → IER12[2]
#define _GPIO_WUC_GROUP_A2   9
#define _GPIO_WUC_BIT_A2     5
#define _GPIO_WUC_IRQ_A2     98

// GPA3: WU80 (Group 8, bit 0), WKO[80] → INT88 → IER11[0]
#define _GPIO_WUC_GROUP_A3   8
#define _GPIO_WUC_BIT_A3     0
#define _GPIO_WUC_IRQ_A3     88

// GPA4: WU81 (Group 8, bit 1), WKO[81] → INT89 → IER11[1]
#define _GPIO_WUC_GROUP_A4   8
#define _GPIO_WUC_BIT_A4     1
#define _GPIO_WUC_IRQ_A4     89

// GPA5: WU82 (Group 8, bit 2), WKO[82] → INT90 → IER11[2]
#define _GPIO_WUC_GROUP_A5   8
#define _GPIO_WUC_BIT_A5     2
#define _GPIO_WUC_IRQ_A5     90

// GPA6: WU83 (Group 8, bit 3), WKO[83] → INT91 → IER11[3]
#define _GPIO_WUC_GROUP_A6   8
#define _GPIO_WUC_BIT_A6     3
#define _GPIO_WUC_IRQ_A6     91

// GPA7: WU100 (Group 10, bit 4), WKO[100] → INT105 → IER13[1]
#define _GPIO_WUC_GROUP_A7   10
#define _GPIO_WUC_BIT_A7     4
#define _GPIO_WUC_IRQ_A7     105

// -------------------------------------------------------------------------
// Port B
// -------------------------------------------------------------------------
// GPB0: WU101 (Group 10, bit 5), WKO[101] → INT106 → IER13[2]
#define _GPIO_WUC_GROUP_B0   10
#define _GPIO_WUC_BIT_B0     5
#define _GPIO_WUC_IRQ_B0     106

// GPB1: WU102 (Group 10, bit 6), WKO[102] → INT107 → IER13[3]
#define _GPIO_WUC_GROUP_B1   10
#define _GPIO_WUC_BIT_B1     6
#define _GPIO_WUC_IRQ_B1     107

// GPB2: WU84 (Group 8, bit 4), WKO[84] → INT92 → IER11[4]
#define _GPIO_WUC_GROUP_B2   8
#define _GPIO_WUC_BIT_B2     4
#define _GPIO_WUC_IRQ_B2     92

// GPB3: WU25/PWRSW (Group 2, bit 5), WKO[25] → INT14 → IER1[6]
// Note: GPB3 is also WU103 (Group 10, bit 7, INT108) but WU25 is the primary entry.
#define _GPIO_WUC_GROUP_B3   2
#define _GPIO_WUC_BIT_B3     5
#define _GPIO_WUC_IRQ_B3     14

// GPB4: WU94 (Group 9, bit 6), WKO[94] → INT99 → IER12[3]
#define _GPIO_WUC_GROUP_B4   9
#define _GPIO_WUC_BIT_B4     6
#define _GPIO_WUC_IRQ_B4     99

// GPB5: WU104 (Group 11, bit 0), WKO[104] → INT109 → IER13[5]
#define _GPIO_WUC_GROUP_B5   11
#define _GPIO_WUC_BIT_B5     0
#define _GPIO_WUC_IRQ_B5     109

// GPB6: WU105 (Group 11, bit 1), WKO[105] → INT110 → IER13[6]
#define _GPIO_WUC_GROUP_B6   11
#define _GPIO_WUC_BIT_B6     1
#define _GPIO_WUC_IRQ_B6     110

// GPB7: no WUC capability
#define _GPIO_WUC_GROUP_B7   0
#define _GPIO_WUC_BIT_B7     0
#define _GPIO_WUC_IRQ_B7     0

// -------------------------------------------------------------------------
// Port C
// -------------------------------------------------------------------------
// GPC0: WU85 (Group 8, bit 5), WKO[85] → INT93 → IER11[5]
#define _GPIO_WUC_GROUP_C0   8
#define _GPIO_WUC_BIT_C0     5
#define _GPIO_WUC_IRQ_C0     93

// GPC1: WU107 (Group 11, bit 3), WKO[107] → INT112 → IER14[0]
#define _GPIO_WUC_GROUP_C1   11
#define _GPIO_WUC_BIT_C1     3
#define _GPIO_WUC_IRQ_C1     112

// GPC2: WU95 (Group 9, bit 7), WKO[95] → INT100 → IER12[4]
#define _GPIO_WUC_GROUP_C2   9
#define _GPIO_WUC_BIT_C2     7
#define _GPIO_WUC_IRQ_C2     100

// GPC3: WU108 (Group 11, bit 4), WKO[108] → INT113 → IER14[1]
#define _GPIO_WUC_GROUP_C3   11
#define _GPIO_WUC_BIT_C3     4
#define _GPIO_WUC_IRQ_C3     113

// GPC4: WU22 (Group 2, bit 2), WKO[22] → INT21 → IER2[5]
#define _GPIO_WUC_GROUP_C4   2
#define _GPIO_WUC_BIT_C4     2
#define _GPIO_WUC_IRQ_C4     21

// GPC5: WU109 (Group 11, bit 5), WKO[109] → INT114 → IER14[2]
#define _GPIO_WUC_GROUP_C5   11
#define _GPIO_WUC_BIT_C5     5
#define _GPIO_WUC_IRQ_C5     114

// GPC6: WU23 (Group 2, bit 3), WKO[23] → INT6 → IER0[6]
#define _GPIO_WUC_GROUP_C6   2
#define _GPIO_WUC_BIT_C6     3
#define _GPIO_WUC_IRQ_C6     6

// GPC7: WU86 (Group 8, bit 6), WKO[86] → INT94 → IER11[6]
#define _GPIO_WUC_GROUP_C7   8
#define _GPIO_WUC_BIT_C7     6
#define _GPIO_WUC_IRQ_C7     94

// -------------------------------------------------------------------------
// Port D
// -------------------------------------------------------------------------
// GPD0: WU20 (Group 2, bit 0), WKO[20] → INT1 → IER0[1]
#define _GPIO_WUC_GROUP_D0   2
#define _GPIO_WUC_BIT_D0     0
#define _GPIO_WUC_IRQ_D0     1

// GPD1: WU21 (Group 2, bit 1), WKO[21] → INT31 → IER3[7]
#define _GPIO_WUC_GROUP_D1   2
#define _GPIO_WUC_BIT_D1     1
#define _GPIO_WUC_IRQ_D1     31

// GPD2: WU24 (Group 2, bit 4), WKO[24] → INT17 → IER2[1]
#define _GPIO_WUC_GROUP_D2   2
#define _GPIO_WUC_BIT_D2     4
#define _GPIO_WUC_IRQ_D2     17

// GPD3: WU110 (Group 11, bit 6), WKO[110] → INT115 → IER14[3]
#define _GPIO_WUC_GROUP_D3   11
#define _GPIO_WUC_BIT_D3     6
#define _GPIO_WUC_IRQ_D3     115

// GPD4: WU111 (Group 11, bit 7), WKO[111] → INT116 → IER14[4]
#define _GPIO_WUC_GROUP_D4   11
#define _GPIO_WUC_BIT_D4     7
#define _GPIO_WUC_IRQ_D4     116

// GPD5: WU112 (Group 12, bit 0), WKO[112] → INT117 → IER14[5]
#define _GPIO_WUC_GROUP_D5   12
#define _GPIO_WUC_BIT_D5     0
#define _GPIO_WUC_IRQ_D5     117

// GPD6: WU113 (Group 12, bit 1), WKO[113] → INT118 → IER14[6]
#define _GPIO_WUC_GROUP_D6   12
#define _GPIO_WUC_BIT_D6     1
#define _GPIO_WUC_IRQ_D6     118

// GPD7: WU87 (Group 8, bit 7), WKO[87] → INT95 → IER11[7]
#define _GPIO_WUC_GROUP_D7   8
#define _GPIO_WUC_BIT_D7     7
#define _GPIO_WUC_IRQ_D7     95

// -------------------------------------------------------------------------
// Port E
// -------------------------------------------------------------------------
// GPE0: WU70 (Group 7, bit 0), WKO[70] → INT72 → IER9[0]
#define _GPIO_WUC_GROUP_E0   7
#define _GPIO_WUC_BIT_E0     0
#define _GPIO_WUC_IRQ_E0     72

// GPE1: WU71 (Group 7, bit 1), WKO[71] → INT73 → IER9[1]
#define _GPIO_WUC_GROUP_E1   7
#define _GPIO_WUC_BIT_E1     1
#define _GPIO_WUC_IRQ_E1     73

// GPE2: WU72 (Group 7, bit 2), WKO[72] → INT74 → IER9[2]
#define _GPIO_WUC_GROUP_E2   7
#define _GPIO_WUC_BIT_E2     2
#define _GPIO_WUC_IRQ_E2     74

// GPE3: WU73 (Group 7, bit 3), WKO[73] → INT75 → IER9[3]
#define _GPIO_WUC_GROUP_E3   7
#define _GPIO_WUC_BIT_E3     3
#define _GPIO_WUC_IRQ_E3     75

// GPE4: WU114 (Group 12, bit 2), WKO[114] → INT119 → IER14[7]
#define _GPIO_WUC_GROUP_E4   12
#define _GPIO_WUC_BIT_E4     2
#define _GPIO_WUC_IRQ_E4     119

// GPE5: WU40 (Group 4, bit 0) — WUC input but no INTC routing
#define _GPIO_WUC_GROUP_E5   0
#define _GPIO_WUC_BIT_E5     0
#define _GPIO_WUC_IRQ_E5     0

// GPE6: WU45 (Group 4, bit 5) — WUC input but no INTC routing
#define _GPIO_WUC_GROUP_E6   0
#define _GPIO_WUC_BIT_E6     0
#define _GPIO_WUC_IRQ_E6     0

// GPE7: WU46 (Group 4, bit 6) — WUC input but no INTC routing
#define _GPIO_WUC_GROUP_E7   0
#define _GPIO_WUC_BIT_E7     0
#define _GPIO_WUC_IRQ_E7     0

// -------------------------------------------------------------------------
// Port F
// -------------------------------------------------------------------------
// GPF0: WU96 (Group 10, bit 0), WKO[96] → INT101 → IER12[5]
#define _GPIO_WUC_GROUP_F0   10
#define _GPIO_WUC_BIT_F0     0
#define _GPIO_WUC_IRQ_F0     101

// GPF1: WU97 (Group 10, bit 1), WKO[97] → INT102 → IER12[6]
#define _GPIO_WUC_GROUP_F1   10
#define _GPIO_WUC_BIT_F1     1
#define _GPIO_WUC_IRQ_F1     102

// GPF2: WU98 (Group 10, bit 2), WKO[98] → INT103 → IER12[7]
#define _GPIO_WUC_GROUP_F2   10
#define _GPIO_WUC_BIT_F2     2
#define _GPIO_WUC_IRQ_F2     103

// GPF3: WU99 (Group 10, bit 3), WKO[99] → INT104 → IER13[0]
#define _GPIO_WUC_GROUP_F3   10
#define _GPIO_WUC_BIT_F3     3
#define _GPIO_WUC_IRQ_F3     104

// GPF4: WU64 (Group 6, bit 4), WKO[64] → INT52 → IER6[4]
#define _GPIO_WUC_GROUP_F4   6
#define _GPIO_WUC_BIT_F4     4
#define _GPIO_WUC_IRQ_F4     52

// GPF5: WU65 (Group 6, bit 5), WKO[65] → INT53 → IER6[5]
#define _GPIO_WUC_GROUP_F5   6
#define _GPIO_WUC_BIT_F5     5
#define _GPIO_WUC_IRQ_F5     53

// GPF6: WU66 (Group 6, bit 6), WKO[66] → INT54 → IER6[6]
#define _GPIO_WUC_GROUP_F6   6
#define _GPIO_WUC_BIT_F6     6
#define _GPIO_WUC_IRQ_F6     54

// GPF7: WU67 (Group 6, bit 7), WKO[67] → INT55 → IER6[7]
#define _GPIO_WUC_GROUP_F7   6
#define _GPIO_WUC_BIT_F7     7
#define _GPIO_WUC_IRQ_F7     55

// -------------------------------------------------------------------------
// Port G
// -------------------------------------------------------------------------
// GPG0: WU115 (Group 12, bit 3), WKO[115] → INT120 → IER15[0]
#define _GPIO_WUC_GROUP_G0   12
#define _GPIO_WUC_BIT_G0     3
#define _GPIO_WUC_IRQ_G0     120

// GPG1: WU116 (Group 12, bit 4), WKO[116] → INT121 → IER15[1]
#define _GPIO_WUC_GROUP_G1   12
#define _GPIO_WUC_BIT_G1     4
#define _GPIO_WUC_IRQ_G1     121

// GPG2: WU117 (Group 12, bit 5), WKO[117] → INT122 → IER15[2]
#define _GPIO_WUC_GROUP_G2   12
#define _GPIO_WUC_BIT_G2     5
#define _GPIO_WUC_IRQ_G2     122

// GPG3: no WUC capability
#define _GPIO_WUC_GROUP_G3   0
#define _GPIO_WUC_BIT_G3     0
#define _GPIO_WUC_IRQ_G3     0

// GPG4: no WUC capability
#define _GPIO_WUC_GROUP_G4   0
#define _GPIO_WUC_BIT_G4     0
#define _GPIO_WUC_IRQ_G4     0

// GPG5: no WUC capability
#define _GPIO_WUC_GROUP_G5   0
#define _GPIO_WUC_BIT_G5     0
#define _GPIO_WUC_IRQ_G5     0

// GPG6: WU118 (Group 12, bit 6), WKO[118] → INT123 → IER15[3]
#define _GPIO_WUC_GROUP_G6   12
#define _GPIO_WUC_BIT_G6     6
#define _GPIO_WUC_IRQ_G6     123

// GPG7: no WUC capability
#define _GPIO_WUC_GROUP_G7   0
#define _GPIO_WUC_BIT_G7     0
#define _GPIO_WUC_IRQ_G7     0

// -------------------------------------------------------------------------
// Port H
// -------------------------------------------------------------------------
// GPH0: WU60 (Group 6, bit 0), WKO[60] → INT48 → IER6[0]
#define _GPIO_WUC_GROUP_H0   6
#define _GPIO_WUC_BIT_H0     0
#define _GPIO_WUC_IRQ_H0     48

// GPH1: WU61 (Group 6, bit 1), WKO[61] → INT49 → IER6[1]
#define _GPIO_WUC_GROUP_H1   6
#define _GPIO_WUC_BIT_H1     1
#define _GPIO_WUC_IRQ_H1     49

// GPH2: WU62 (Group 6, bit 2), WKO[62] → INT50 → IER6[2]
#define _GPIO_WUC_GROUP_H2   6
#define _GPIO_WUC_BIT_H2     2
#define _GPIO_WUC_IRQ_H2     50

// GPH3: WU63 (Group 6, bit 3), WKO[63] → INT51 → IER6[3]
#define _GPIO_WUC_GROUP_H3   6
#define _GPIO_WUC_BIT_H3     3
#define _GPIO_WUC_IRQ_H3     51

// GPH4: WU88 (Group 9, bit 0), WKO[88] → INT85 → IER10[5]
#define _GPIO_WUC_GROUP_H4   9
#define _GPIO_WUC_BIT_H4     0
#define _GPIO_WUC_IRQ_H4     85

// GPH5: WU89 (Group 9, bit 1), WKO[89] → INT86 → IER10[6]
#define _GPIO_WUC_GROUP_H5   9
#define _GPIO_WUC_BIT_H5     1
#define _GPIO_WUC_IRQ_H5     86

// GPH6: WU90 (Group 9, bit 2), WKO[90] → INT87 → IER10[7]
#define _GPIO_WUC_GROUP_H6   9
#define _GPIO_WUC_BIT_H6     2
#define _GPIO_WUC_IRQ_H6     87

// GPH7: WU127 (Group 13, bit 7), WKO[127] → INT148 → IER18[4]
#define _GPIO_WUC_GROUP_H7   13
#define _GPIO_WUC_BIT_H7     7
#define _GPIO_WUC_IRQ_H7     148

// -------------------------------------------------------------------------
// Port I
// -------------------------------------------------------------------------
// GPI0: WU119 (Group 12, bit 7), WKO[119] → INT124 → IER15[4]
#define _GPIO_WUC_GROUP_I0   12
#define _GPIO_WUC_BIT_I0     7
#define _GPIO_WUC_IRQ_I0     124

// GPI1: WU120 (Group 13, bit 0), WKO[120] → INT125 → IER15[5]
#define _GPIO_WUC_GROUP_I1   13
#define _GPIO_WUC_BIT_I1     0
#define _GPIO_WUC_IRQ_I1     125

// GPI2: WU121 (Group 13, bit 1), WKO[121] → INT126 → IER15[6]
#define _GPIO_WUC_GROUP_I2   13
#define _GPIO_WUC_BIT_I2     1
#define _GPIO_WUC_IRQ_I2     126

// GPI3: WU122 (Group 13, bit 2), WKO[122] → INT127 → IER15[7]
#define _GPIO_WUC_GROUP_I3   13
#define _GPIO_WUC_BIT_I3     2
#define _GPIO_WUC_IRQ_I3     127

// GPI4: WU74 (Group 7, bit 4), WKO[74] → INT76 → IER9[4]
#define _GPIO_WUC_GROUP_I4   7
#define _GPIO_WUC_BIT_I4     4
#define _GPIO_WUC_IRQ_I4     76

// GPI5: WU75 (Group 7, bit 5), WKO[75] → INT77 → IER9[5]
#define _GPIO_WUC_GROUP_I5   7
#define _GPIO_WUC_BIT_I5     5
#define _GPIO_WUC_IRQ_I5     77

// GPI6: WU76 (Group 7, bit 6), WKO[76] → INT78 → IER9[6]
#define _GPIO_WUC_GROUP_I6   7
#define _GPIO_WUC_BIT_I6     6
#define _GPIO_WUC_IRQ_I6     78

// GPI7: WU77 (Group 7, bit 7), WKO[77] → INT79 → IER9[7]
#define _GPIO_WUC_GROUP_I7   7
#define _GPIO_WUC_BIT_I7     7
#define _GPIO_WUC_IRQ_I7     79

// -------------------------------------------------------------------------
// Port J
// -------------------------------------------------------------------------
// GPJ0: WU128 (Group 14, bit 0), WKO[128] → INT128 → IER16[0]
#define _GPIO_WUC_GROUP_J0   14
#define _GPIO_WUC_BIT_J0     0
#define _GPIO_WUC_IRQ_J0     128

// GPJ1: WU129 (Group 14, bit 1), WKO[129] → INT129 → IER16[1]
#define _GPIO_WUC_GROUP_J1   14
#define _GPIO_WUC_BIT_J1     1
#define _GPIO_WUC_IRQ_J1     129

// GPJ2: WU130 (Group 14, bit 2), WKO[130] → INT130 → IER16[2]
#define _GPIO_WUC_GROUP_J2   14
#define _GPIO_WUC_BIT_J2     2
#define _GPIO_WUC_IRQ_J2     130

// GPJ3: WU131 (Group 14, bit 3), WKO[131] → INT131 → IER16[3]
#define _GPIO_WUC_GROUP_J3   14
#define _GPIO_WUC_BIT_J3     3
#define _GPIO_WUC_IRQ_J3     131

// GPJ4: WU132 (Group 14, bit 4), WKO[132] → INT132 → IER16[4]
#define _GPIO_WUC_GROUP_J4   14
#define _GPIO_WUC_BIT_J4     4
#define _GPIO_WUC_IRQ_J4     132

// GPJ5: WU133 (Group 14, bit 5), WKO[133] → INT133 → IER16[5]
#define _GPIO_WUC_GROUP_J5   14
#define _GPIO_WUC_BIT_J5     5
#define _GPIO_WUC_IRQ_J5     133

// GPJ6: WU134 (Group 14, bit 6), WKO[134] → INT134 → IER16[6]
#define _GPIO_WUC_GROUP_J6   14
#define _GPIO_WUC_BIT_J6     6
#define _GPIO_WUC_IRQ_J6     134

// GPJ7: WU135 (Group 14, bit 7), WKO[135] → INT135 → IER16[7]
#define _GPIO_WUC_GROUP_J7   14
#define _GPIO_WUC_BIT_J7     7
#define _GPIO_WUC_IRQ_J7     135

// -------------------------------------------------------------------------
// Port M — no WUC capability on any pin
// -------------------------------------------------------------------------
#define _GPIO_WUC_GROUP_M0   0
#define _GPIO_WUC_BIT_M0     0
#define _GPIO_WUC_IRQ_M0     0

#define _GPIO_WUC_GROUP_M1   0
#define _GPIO_WUC_BIT_M1     0
#define _GPIO_WUC_IRQ_M1     0

#define _GPIO_WUC_GROUP_M2   0
#define _GPIO_WUC_BIT_M2     0
#define _GPIO_WUC_IRQ_M2     0

#define _GPIO_WUC_GROUP_M3   0
#define _GPIO_WUC_BIT_M3     0
#define _GPIO_WUC_IRQ_M3     0

#define _GPIO_WUC_GROUP_M4   0
#define _GPIO_WUC_BIT_M4     0
#define _GPIO_WUC_IRQ_M4     0

#define _GPIO_WUC_GROUP_M5   0
#define _GPIO_WUC_BIT_M5     0
#define _GPIO_WUC_IRQ_M5     0

#define _GPIO_WUC_GROUP_M6   0
#define _GPIO_WUC_BIT_M6     0
#define _GPIO_WUC_IRQ_M6     0

// clang-format on

#endif // _EC_GPIO_WUC_H
