//
//  x65dsasm.cpp
//  
//
//  Created by Carl-Henrik Skårstedt on 9/23/15.
//
//
//  x65 companion disassembler
//
//
// The MIT License (MIT)
//
// Copyright (c) 2015 Carl-Henrik Skårstedt
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software
// and associated documentation files (the "Software"), to deal in the Software without restriction,
// including without limitation the rights to use, copy, modify, merge, publish, distribute,
// sublicense, and/or sell copies of the Software, and to permit persons to whom the Software
// is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or
// substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
// INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
// PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
// FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
// Details, source and documentation at https://github.com/Sakrac/x65.
//
// "struse.h" can be found at https://github.com/Sakrac/struse, only the header file is required.
//

//#include <stdio.h>
//#include <stdlib.h>
//#include <vector>

static const char* aAddrModeFmt[] = {
  "%s ($%02x,x)",     // 00
  "%s $%02x",       // 01
  "%s #$%02x",      // 02
  "%s $%04x",       // 03
  "%s ($%02x),y",     // 04
  "%s $%02x,x",     // 05
  "%s $%04x,y",     // 06
  "%s $%04x,x",     // 07
  "%s ($%04x)",     // 08
  "%s A",         // 09
  "%s ",          // 0a
  "%s ($%02x)",     // 0b
  "%s ($%04x,x)",     // 0c
  "%s $%02x, $%04x",    // 0d
  "%s [$%02x]",     // 0e
  "%s [$%02x],y",     // 0f
  "%s $%06x",       // 10
  "%s $%06x,x",     // 11
  "%s $%02x,s",     // 12
  "%s ($%02x,s),y",   // 13
  "%s [$%04x]",     // 14
  "%s $%02x,$%02x",   // 15

  "%s $%02x,y",     // 16
  "%s ($%02x,y)",     // 17

  "%s #$%02x",      // 18
  "%s #$%02x",      // 19

  "%s $%04x",       // 1a
  "%s $%04x",       // 1b
};

enum AddressModes {
  // address mode bit index

  // 6502

  AM_ZP_REL_X,  // 0 ($12,x)
  AM_ZP,      // 1 $12
  AM_IMM,     // 2 #$12
  AM_ABS,     // 3 $1234
  AM_ZP_Y_REL,  // 4 ($12),y
  AM_ZP_X,    // 5 $12,x
  AM_ABS_Y,   // 6 $1234,y
  AM_ABS_X,   // 7 $1234,x
  AM_REL,     // 8 ($1234)
  AM_ACC,     // 9 A
  AM_NON,     // a

          // 65C02

  AM_ZP_REL,    // b ($12)
  AM_REL_X,   // c ($1234,x)
  AM_ZP_ABS,    // d $12, *+$12

          // 65816

  AM_ZP_REL_L,  // e [$02]
  AM_ZP_REL_Y_L,  // f [$00],y
  AM_ABS_L,   // 10 $bahilo
  AM_ABS_L_X,   // 11 $123456,x
  AM_STK,     // 12 $12,s
  AM_STK_REL_Y, // 13 ($12,s),y
  AM_REL_L,   // 14 [$1234]
  AM_BLK_MOV,   // 15 $12,$34

  AM_ZP_Y,    // 16 stx/ldx
  AM_ZP_REL_Y,  // 17 sax/lax/ahx

  AM_IMM_DBL_A, // 18 #$12/#$1234
  AM_IMM_DBL_I, // 19 #$12/#$1234

  AM_BRANCH,    // 1a beq $1234
  AM_BRANCH_L,  // 1b brl $1234

  AM_COUNT,
};

enum MNM_Base {
  mnm_brk,
  mnm_ora,
  mnm_cop,
  mnm_tsb,
  mnm_asl,
  mnm_php,
  mnm_phd,
  mnm_bpl,
  mnm_trb,
  mnm_clc,
  mnm_inc,
  mnm_tcs,
  mnm_jsr,
  mnm_and,
  mnm_bit,
  mnm_rol,
  mnm_plp,
  mnm_pld,
  mnm_bmi,
  mnm_sec,
  mnm_dec,
  mnm_tsc,
  mnm_rti,
  mnm_eor,
  mnm_wdm,
  mnm_mvp,
  mnm_lsr,
  mnm_pha,
  mnm_phk,
  mnm_jmp,
  mnm_bvc,
  mnm_mvn,
  mnm_cli,
  mnm_phy,
  mnm_tcd,
  mnm_rts,
  mnm_adc,
  mnm_per,
  mnm_stz,
  mnm_ror,
  mnm_rtl,
  mnm_bvs,
  mnm_sei,
  mnm_ply,
  mnm_tdc,
  mnm_bra,
  mnm_sta,
  mnm_brl,
  mnm_sty,
  mnm_stx,
  mnm_dey,
  mnm_txa,
  mnm_phb,
  mnm_bcc,
  mnm_tya,
  mnm_txs,
  mnm_txy,
  mnm_ldy,
  mnm_lda,
  mnm_ldx,
  mnm_tay,
  mnm_tax,
  mnm_plb,
  mnm_bcs,
  mnm_clv,
  mnm_tsx,
  mnm_tyx,
  mnm_cpy,
  mnm_cmp,
  mnm_rep,
  mnm_iny,
  mnm_dex,
  mnm_wai,
  mnm_bne,
  mnm_pei,
  mnm_cld,
  mnm_phx,
  mnm_stp,
  mnm_cpx,
  mnm_sbc,
  mnm_sep,
  mnm_inx,
  mnm_nop,
  mnm_xba,
  mnm_beq,
  mnm_pea,
  mnm_sed,
  mnm_plx,
  mnm_xce,
  mnm_inv,
  mnm_pla,

  mnm_wdc_and_illegal_instructions,

  mnm_bbs0 = mnm_wdc_and_illegal_instructions,
  mnm_bbs1,
  mnm_bbs2,
  mnm_bbs3,
  mnm_bbs4,
  mnm_bbs5,
  mnm_bbs6,
  mnm_bbs7,
  mnm_bbr0,
  mnm_bbr1,
  mnm_bbr2,
  mnm_bbr3,
  mnm_bbr4,
  mnm_bbr5,
  mnm_bbr6,
  mnm_bbr7,

  mnm_ahx,
  mnm_anc,
  mnm_aac,
  mnm_alr,
  mnm_axs,
  mnm_dcp,
  mnm_isc,
  mnm_lax,
  mnm_lax2,
  mnm_rla,
  mnm_rra,
  mnm_sre,
  mnm_sax,
  mnm_slo,
  mnm_xaa,
  mnm_arr,
  mnm_tas,
  mnm_shy,
  mnm_shx,
  mnm_las,
  mnm_sbi,

  mnm_count
};

const char *zsMNM[mnm_count] {
  "brk",
  "ora",
  "cop",
  "tsb",
  "asl",
  "php",
  "phd",
  "bpl",
  "trb",
  "clc",
  "inc",
  "tcs",
  "jsr",
  "and",
  "bit",
  "rol",
  "plp",
  "pld",
  "bmi",
  "sec",
  "dec",
  "tsc",
  "rti",
  "eor",
  "wdm",
  "mvp",
  "lsr",
  "pha",
  "phk",
  "jmp",
  "bvc",
  "mvn",
  "cli",
  "phy",
  "tcd",
  "rts",
  "adc",
  "per",
  "stz",
  "ror",
  "rtl",
  "bvs",
  "sei",
  "ply",
  "tdc",
  "bra",
  "sta",
  "brl",
  "sty",
  "stx",
  "dey",
  "txa",
  "phb",
  "bcc",
  "tya",
  "txs",
  "txy",
  "ldy",
  "lda",
  "ldx",
  "tay",
  "tax",
  "plb",
  "bcs",
  "clv",
  "tsx",
  "tyx",
  "cpy",
  "cmp",
  "rep",
  "iny",
  "dex",
  "wai",
  "bne",
  "pei",
  "cld",
  "phx",
  "stp",
  "cpx",
  "sbc",
  "sep",
  "inx",
  "nop",
  "xba",
  "beq",
  "pea",
  "sed",
  "plx",
  "xce",
  "???",
  "pla",
  "bbs0",
  "bbs1",
  "bbs2",
  "bbs3",
  "bbs4",
  "bbs5",
  "bbs6",
  "bbs7",
  "bbr0",
  "bbr1",
  "bbr2",
  "bbr3",
  "bbr4",
  "bbr5",
  "bbr6",
  "bbr7",
  "ahx",
  "anc",
  "aac",
  "alr",
  "axs",
  "dcp",
  "isc",
  "lax",
  "lax2",
  "rla",
  "rra",
  "sre",
  "sax",
  "slo",
  "xaa",
  "arr",
  "tas",
  "shy",
  "shx",
  "las",
  "sbi",
};

struct dismnm {
  MNM_Base mnemonic;
  unsigned char addrMode;
  unsigned char arg_size;
};

struct dismnm a65C02_ops[256] = {
  { mnm_brk, AM_NON, 0},
  { mnm_ora, AM_ZP_REL_X, 1 },
  { mnm_inv, AM_NON, 0 },
  { mnm_inv, AM_NON, 0 },
  { mnm_tsb, AM_ZP, 1 },
  { mnm_ora, AM_ZP, 1 },
  { mnm_asl, AM_ZP, 1 },
  { mnm_inv, AM_NON, 0 },
  { mnm_php, AM_NON, 0 },
  { mnm_ora, AM_IMM, 1 },
  { mnm_asl, AM_NON, 0 },
  { mnm_inv, AM_NON, 0 },
  { mnm_tsb, AM_ABS, 2 },
  { mnm_ora, AM_ABS, 2 },
  { mnm_asl, AM_ABS, 2 },
  { mnm_bbr0, AM_ZP_ABS, 2 },
  { mnm_bpl, AM_BRANCH, 1 },
  { mnm_ora, AM_ZP_Y_REL, 1 },
  { mnm_ora, AM_ZP_REL, 1 },
  { mnm_inv, AM_NON, 0 },
  { mnm_trb, AM_ZP, 1 },
  { mnm_ora, AM_ZP_X, 1 },
  { mnm_asl, AM_ZP_X, 1 },
  { mnm_inv, AM_NON, 0 },
  { mnm_clc, AM_NON, 0 },
  { mnm_ora, AM_ABS_Y, 2 },
  { mnm_inc, AM_NON, 0 },
  { mnm_inv, AM_NON, 0 },
  { mnm_trb, AM_ABS, 2 },
  { mnm_ora, AM_ABS_X, 2 },
  { mnm_asl, AM_ABS_X, 2 },
  { mnm_bbr1, AM_ZP_ABS, 2 },
  { mnm_jsr, AM_ABS, 2 },
  { mnm_and, AM_ZP_REL_X, 1 },
  { mnm_inv, AM_NON, 0 },
  { mnm_inv, AM_NON, 0 },
  { mnm_bit, AM_ZP, 1 },
  { mnm_and, AM_ZP, 1 },
  { mnm_rol, AM_ZP, 1 },
  { mnm_inv, AM_NON, 0 },
  { mnm_plp, AM_NON, 0 },
  { mnm_and, AM_IMM, 1 },
  { mnm_rol, AM_NON, 0 },
  { mnm_inv, AM_NON, 0 },
  { mnm_bit, AM_ABS, 2 },
  { mnm_and, AM_ABS, 2 },
  { mnm_rol, AM_ABS, 2 },
  { mnm_bbr2, AM_ZP_ABS, 2 },
  { mnm_bmi, AM_BRANCH, 1 },
  { mnm_and, AM_ZP_Y_REL, 1 },
  { mnm_and, AM_ZP_REL, 1 },
  { mnm_inv, AM_NON, 0 },
  { mnm_bit, AM_ZP_X, 1 },
  { mnm_and, AM_ZP_X, 1 },
  { mnm_rol, AM_ZP_X, 1 },
  { mnm_inv, AM_NON, 0 },
  { mnm_sec, AM_NON, 0 },
  { mnm_and, AM_ABS_Y, 2 },
  { mnm_dec, AM_NON, 0 },
  { mnm_inv, AM_NON, 0 },
  { mnm_bit, AM_ABS_X, 2 },
  { mnm_and, AM_ABS_X, 2 },
  { mnm_rol, AM_ABS_X, 2 },
  { mnm_bbr3, AM_ZP_ABS, 2 },
  { mnm_rti, AM_NON, 0 },
  { mnm_eor, AM_ZP_REL_X, 1 },
  { mnm_inv, AM_NON, 0 },
  { mnm_inv, AM_NON, 0 },
  { mnm_inv, AM_NON, 0 },
  { mnm_eor, AM_ZP, 1 },
  { mnm_lsr, AM_ZP, 1 },
  { mnm_inv, AM_NON, 0 },
  { mnm_pha, AM_NON, 0 },
  { mnm_eor, AM_IMM, 1 },
  { mnm_lsr, AM_NON, 0 },
  { mnm_inv, AM_NON, 0 },
  { mnm_jmp, AM_ABS, 2 },
  { mnm_eor, AM_ABS, 2 },
  { mnm_lsr, AM_ABS, 2 },
  { mnm_bbr4, AM_ZP_ABS, 2 },
  { mnm_bvc, AM_BRANCH, 1 },
  { mnm_eor, AM_ZP_Y_REL, 1 },
  { mnm_eor, AM_ZP_REL, 1 },
  { mnm_inv, AM_NON, 0 },
  { mnm_inv, AM_NON, 0 },
  { mnm_eor, AM_ZP_X, 1 },
  { mnm_lsr, AM_ZP_X, 1 },
  { mnm_inv, AM_NON, 0 },
  { mnm_cli, AM_NON, 0 },
  { mnm_eor, AM_ABS_Y, 2 },
  { mnm_phy, AM_NON, 0 },
  { mnm_inv, AM_NON, 0 },
  { mnm_inv, AM_NON, 0 },
  { mnm_eor, AM_ABS_X, 2 },
  { mnm_lsr, AM_ABS_X, 2 },
  { mnm_bbr5, AM_ZP_ABS, 2 },
  { mnm_rts, AM_NON, 0 },
  { mnm_adc, AM_ZP_REL_X, 1 },
  { mnm_inv, AM_NON, 0 },
  { mnm_inv, AM_NON, 0 },
  { mnm_stz, AM_ZP, 1 },
  { mnm_adc, AM_ZP, 1 },
  { mnm_ror, AM_ZP, 1 },
  { mnm_inv, AM_NON, 0 },
  { mnm_pla, AM_NON, 0 },
  { mnm_adc, AM_IMM, 1 },
  { mnm_ror, AM_NON, 0 },
  { mnm_inv, AM_NON, 0 },
  { mnm_jmp, AM_REL, 2 },
  { mnm_adc, AM_ABS, 2 },
  { mnm_ror, AM_ABS, 2 },
  { mnm_bbr6, AM_ZP_ABS, 2 },
  { mnm_bvs, AM_BRANCH, 1 },
  { mnm_adc, AM_ZP_Y_REL, 1 },
  { mnm_adc, AM_ZP_REL, 1 },
  { mnm_inv, AM_NON, 0 },
  { mnm_stz, AM_ZP_X, 1 },
  { mnm_adc, AM_ZP_X, 1 },
  { mnm_ror, AM_ZP_X, 1 },
  { mnm_inv, AM_NON, 0 },
  { mnm_sei, AM_NON, 0 },
  { mnm_adc, AM_ABS_Y, 2 },
  { mnm_ply, AM_NON, 0 },
  { mnm_inv, AM_NON, 0 },
  { mnm_jmp, AM_REL_X, 2 },
  { mnm_adc, AM_ABS_X, 2 },
  { mnm_ror, AM_ABS_X, 2 },
  { mnm_bbr7, AM_ZP_ABS, 2 },
  { mnm_bra, AM_BRANCH, 1 },
  { mnm_sta, AM_ZP_REL_X, 1 },
  { mnm_inv, AM_NON, 0 },
  { mnm_inv, AM_NON, 0 },
  { mnm_sty, AM_ZP, 1 },
  { mnm_sta, AM_ZP, 1 },
  { mnm_stx, AM_ZP, 1 },
  { mnm_inv, AM_NON, 0 },
  { mnm_dey, AM_NON, 0 },
  { mnm_bit, AM_IMM, 1 },
  { mnm_txa, AM_NON, 0 },
  { mnm_inv, AM_NON, 0 },
  { mnm_sty, AM_ABS, 2 },
  { mnm_sta, AM_ABS, 2 },
  { mnm_stx, AM_ABS, 2 },
  { mnm_bbs0, AM_ZP_ABS, 2 },
  { mnm_bcc, AM_BRANCH, 1 },
  { mnm_sta, AM_ZP_Y_REL, 1 },
  { mnm_sta, AM_ZP_REL, 1 },
  { mnm_inv, AM_NON, 0 },
  { mnm_sty, AM_ZP_X, 1 },
  { mnm_sta, AM_ZP_X, 1 },
  { mnm_stx, AM_ZP_Y, 1 },
  { mnm_inv, AM_NON, 0 },
  { mnm_tya, AM_NON, 0 },
  { mnm_sta, AM_ABS_Y, 2 },
  { mnm_txs, AM_NON, 0 },
  { mnm_inv, AM_NON, 0 },
  { mnm_stz, AM_ABS, 2 },
  { mnm_sta, AM_ABS_X, 2 },
  { mnm_stz, AM_ABS_X, 2 },
  { mnm_bbs1, AM_ZP_ABS, 2 },
  { mnm_ldy, AM_IMM, 1 },
  { mnm_lda, AM_ZP_REL_X, 1 },
  { mnm_ldx, AM_IMM, 1 },
  { mnm_inv, AM_NON, 0 },
  { mnm_ldy, AM_ZP, 1 },
  { mnm_lda, AM_ZP, 1 },
  { mnm_ldx, AM_ZP, 1 },
  { mnm_inv, AM_NON, 0 },
  { mnm_tay, AM_NON, 0 },
  { mnm_lda, AM_IMM, 1 },
  { mnm_tax, AM_NON, 0 },
  { mnm_inv, AM_NON, 0 },
  { mnm_ldy, AM_ABS, 2 },
  { mnm_lda, AM_ABS, 2 },
  { mnm_ldx, AM_ABS, 2 },
  { mnm_bbs2, AM_ZP_ABS, 2 },
  { mnm_bcs, AM_BRANCH, 1 },
  { mnm_lda, AM_ZP_Y_REL, 1 },
  { mnm_lda, AM_ZP_REL, 1 },
  { mnm_inv, AM_NON, 0 },
  { mnm_ldy, AM_ZP_X, 1 },
  { mnm_lda, AM_ZP_X, 1 },
  { mnm_ldx, AM_ZP_Y, 1 },
  { mnm_inv, AM_NON, 0 },
  { mnm_clv, AM_NON, 0 },
  { mnm_lda, AM_ABS_Y, 2 },
  { mnm_tsx, AM_NON, 0 },
  { mnm_inv, AM_NON, 0 },
  { mnm_ldy, AM_ABS_X, 2 },
  { mnm_lda, AM_ABS_X, 2 },
  { mnm_ldx, AM_ABS_Y, 2 },
  { mnm_bbs3, AM_ZP_ABS, 2 },
  { mnm_cpy, AM_IMM, 1 },
  { mnm_cmp, AM_ZP_REL_X, 1 },
  { mnm_inv, AM_NON, 0 },
  { mnm_inv, AM_NON, 0 },
  { mnm_cpy, AM_ZP, 1 },
  { mnm_cmp, AM_ZP, 1 },
  { mnm_dec, AM_ZP, 1 },
  { mnm_inv, AM_NON, 0 },
  { mnm_iny, AM_NON, 0 },
  { mnm_cmp, AM_IMM, 1 },
  { mnm_dex, AM_NON, 0 },
  { mnm_wai, AM_NON, 0 },
  { mnm_cpy, AM_ABS, 2 },
  { mnm_cmp, AM_ABS, 2 },
  { mnm_dec, AM_ABS, 2 },
  { mnm_bbs4, AM_ZP_ABS, 2 },
  { mnm_bne, AM_BRANCH, 1 },
  { mnm_cmp, AM_ZP_Y_REL, 1 },
  { mnm_cmp, AM_ZP_REL, 1 },
  { mnm_inv, AM_NON, 0 },
  { mnm_inv, AM_NON, 0 },
  { mnm_cmp, AM_ZP_X, 1 },
  { mnm_dec, AM_ZP_X, 1 },
  { mnm_inv, AM_NON, 0 },
  { mnm_cld, AM_NON, 0 },
  { mnm_cmp, AM_ABS_Y, 2 },
  { mnm_phx, AM_NON, 0 },
  { mnm_stp, AM_NON, 0 },
  { mnm_inv, AM_NON, 0 },
  { mnm_cmp, AM_ABS_X, 2 },
  { mnm_dec, AM_ABS_X, 2 },
  { mnm_bbs5, AM_ZP_ABS, 2 },
  { mnm_cpx, AM_IMM, 1 },
  { mnm_sbc, AM_ZP_REL_X, 1 },
  { mnm_inv, AM_NON, 0 },
  { mnm_inv, AM_NON, 0 },
  { mnm_cpx, AM_ZP, 1 },
  { mnm_sbc, AM_ZP, 1 },
  { mnm_inc, AM_ZP, 1 },
  { mnm_inv, AM_NON, 0 },
  { mnm_inx, AM_NON, 0 },
  { mnm_sbc, AM_IMM, 1 },
  { mnm_nop, AM_NON, 0 },
  { mnm_inv, AM_NON, 0 },
  { mnm_cpx, AM_ABS, 2 },
  { mnm_sbc, AM_ABS, 2 },
  { mnm_inc, AM_ABS, 2 },
  { mnm_bbs6, AM_ZP_ABS, 2 },
  { mnm_beq, AM_BRANCH, 1 },
  { mnm_sbc, AM_ZP_Y_REL, 1 },
  { mnm_sbc, AM_ZP_REL, 1 },
  { mnm_inv, AM_NON, 0 },
  { mnm_inv, AM_NON, 0 },
  { mnm_sbc, AM_ZP_X, 1 },
  { mnm_inc, AM_ZP_X, 1 },
  { mnm_inv, AM_NON, 0 },
  { mnm_sed, AM_NON, 0 },
  { mnm_sbc, AM_ABS_Y, 2 },
  { mnm_plx, AM_NON, 0 },
  { mnm_inv, AM_NON, 0 },
  { mnm_inv, AM_NON, 0 },
  { mnm_sbc, AM_ABS_X, 2 },
  { mnm_inc, AM_ABS_X, 2 },
  { mnm_bbs7, AM_ZP_ABS, 2 },
};
