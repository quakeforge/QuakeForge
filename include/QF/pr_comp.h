/*  Copyright (C) 1996-1997  Id Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    See file, 'COPYING', for details.
*/

// this file is shared by QuakeForge and qfcc
#ifndef __QF_pr_comp_h
#define __QF_pr_comp_h

#include "QF/qtypes.h"

typedef double pr_double_t;
typedef float pr_float_t;
typedef int16_t pr_short_t;
typedef uint16_t pr_ushort_t;
typedef int32_t pr_int_t;
typedef uint32_t pr_uint_t;
typedef int64_t pr_long_t;
typedef uint64_t pr_ulong_t;
typedef pr_uint_t func_t;
typedef pr_int_t pr_string_t;
typedef pr_string_t string_t;//FIXME
typedef pr_uint_t pr_pointer_t;
typedef pr_pointer_t pointer_t;//FIXME

#define PR_VEC_TYPE(t,n,s) \
	typedef t n __attribute__ ((vector_size (s*sizeof (t))))

PR_VEC_TYPE (pr_int_t, pr_ivec2_t, 2);
PR_VEC_TYPE (pr_int_t, pr_ivec4_t, 4);
PR_VEC_TYPE (pr_uint_t, pr_uivec2_t, 2);
PR_VEC_TYPE (pr_uint_t, pr_uivec4_t, 4);
PR_VEC_TYPE (float, pr_vec2_t, 2);
PR_VEC_TYPE (float, pr_vec4_t, 4);
PR_VEC_TYPE (pr_long_t, pr_lvec2_t, 2);
PR_VEC_TYPE (pr_long_t, pr_lvec4_t, 4);
PR_VEC_TYPE (pr_ulong_t, pr_ulvec2_t, 2);
PR_VEC_TYPE (pr_ulong_t, pr_ulvec4_t, 4);
PR_VEC_TYPE (double, pr_dvec2_t, 2);
PR_VEC_TYPE (double, pr_dvec4_t, 4);

typedef enum {
	ev_void,
	ev_string,
	ev_float,
	ev_vector,
	ev_entity,
	ev_field,
	ev_func,
	ev_pointer,			// end of v6 types
	ev_quat,
	ev_integer,
	ev_uinteger,
	ev_short,			// value is embedded in the opcode
	ev_double,

	ev_invalid,			// invalid type. used for instruction checking
	ev_type_count		// not a type, gives number of types
} etype_t;

extern const pr_ushort_t pr_type_size[ev_type_count];
extern const char * const pr_type_name[ev_type_count];

#define	OFS_NULL		0
#define	OFS_RETURN		1
#define	OFS_PARM0		4		// leave 3 ofs for each parm to hold vectors
#define	OFS_PARM1		7
#define	OFS_PARM2		10
#define	OFS_PARM3		13
#define	OFS_PARM4		16
#define	OFS_PARM5		19
#define	OFS_PARM6		22
#define	OFS_PARM7		25
#define	RESERVED_OFS	28


typedef enum {
	OP_DONE_v6p,
	OP_MUL_F_v6p,
	OP_MUL_V_v6p,
	OP_MUL_FV_v6p,
	OP_MUL_VF_v6p,
	OP_DIV_F_v6p,
	OP_ADD_F_v6p,
	OP_ADD_V_v6p,
	OP_SUB_F_v6p,
	OP_SUB_V_v6p,

	OP_EQ_F_v6p,
	OP_EQ_V_v6p,
	OP_EQ_S_v6p,
	OP_EQ_E_v6p,
	OP_EQ_FN_v6p,

	OP_NE_F_v6p,
	OP_NE_V_v6p,
	OP_NE_S_v6p,
	OP_NE_E_v6p,
	OP_NE_FN_v6p,

	OP_LE_F_v6p,
	OP_GE_F_v6p,
	OP_LT_F_v6p,
	OP_GT_F_v6p,

	OP_LOAD_F_v6p,
	OP_LOAD_V_v6p,
	OP_LOAD_S_v6p,
	OP_LOAD_ENT_v6p,
	OP_LOAD_FLD_v6p,
	OP_LOAD_FN_v6p,

	OP_ADDRESS_v6p,

	OP_STORE_F_v6p,
	OP_STORE_V_v6p,
	OP_STORE_S_v6p,
	OP_STORE_ENT_v6p,
	OP_STORE_FLD_v6p,
	OP_STORE_FN_v6p,

	OP_STOREP_F_v6p,
	OP_STOREP_V_v6p,
	OP_STOREP_S_v6p,
	OP_STOREP_ENT_v6p,
	OP_STOREP_FLD_v6p,
	OP_STOREP_FN_v6p,

	OP_RETURN_v6p,
	OP_NOT_F_v6p,
	OP_NOT_V_v6p,
	OP_NOT_S_v6p,
	OP_NOT_ENT_v6p,
	OP_NOT_FN_v6p,
	OP_IF_v6p,
	OP_IFNOT_v6p,
	OP_CALL0_v6p,
	OP_CALL1_v6p,
	OP_CALL2_v6p,
	OP_CALL3_v6p,
	OP_CALL4_v6p,
	OP_CALL5_v6p,
	OP_CALL6_v6p,
	OP_CALL7_v6p,
	OP_CALL8_v6p,
	OP_STATE_v6p,
	OP_GOTO_v6p,
	OP_AND_v6p,
	OP_OR_v6p,

	OP_BITAND_v6p,
	OP_BITOR_v6p,	// end of v6 opcodes

	OP_ADD_S_v6p,
	OP_LE_S_v6p,
	OP_GE_S_v6p,
	OP_LT_S_v6p,
	OP_GT_S_v6p,

	OP_ADD_I_v6p,
	OP_SUB_I_v6p,
	OP_MUL_I_v6p,
	OP_DIV_I_v6p,
	OP_BITAND_I_v6p,
	OP_BITOR_I_v6p,
	OP_GE_I_v6p,
	OP_LE_I_v6p,
	OP_GT_I_v6p,
	OP_LT_I_v6p,
	OP_AND_I_v6p,
	OP_OR_I_v6p,
	OP_NOT_I_v6p,
	OP_EQ_I_v6p,
	OP_NE_I_v6p,
	OP_STORE_I_v6p,
	OP_STOREP_I_v6p,
	OP_LOAD_I_v6p,

	OP_CONV_IF_v6p,
	OP_CONV_FI_v6p,

	OP_BITXOR_F_v6p,
	OP_BITXOR_I_v6p,
	OP_BITNOT_F_v6p,
	OP_BITNOT_I_v6p,

	OP_SHL_F_v6p,
	OP_SHR_F_v6p,
	OP_SHL_I_v6p,
	OP_SHR_I_v6p,

	OP_REM_F_v6p,
	OP_REM_I_v6p,

	OP_LOADB_F_v6p,
	OP_LOADB_V_v6p,
	OP_LOADB_S_v6p,
	OP_LOADB_ENT_v6p,
	OP_LOADB_FLD_v6p,
	OP_LOADB_FN_v6p,
	OP_LOADB_I_v6p,
	OP_LOADB_P_v6p,

	OP_STOREB_F_v6p,
	OP_STOREB_V_v6p,
	OP_STOREB_S_v6p,
	OP_STOREB_ENT_v6p,
	OP_STOREB_FLD_v6p,
	OP_STOREB_FN_v6p,
	OP_STOREB_I_v6p,
	OP_STOREB_P_v6p,

	OP_ADDRESS_VOID_v6p,
	OP_ADDRESS_F_v6p,
	OP_ADDRESS_V_v6p,
	OP_ADDRESS_S_v6p,
	OP_ADDRESS_ENT_v6p,
	OP_ADDRESS_FLD_v6p,
	OP_ADDRESS_FN_v6p,
	OP_ADDRESS_I_v6p,
	OP_ADDRESS_P_v6p,

	OP_LEA_v6p,

	OP_IFBE_v6p,
	OP_IFB_v6p,
	OP_IFAE_v6p,
	OP_IFA_v6p,

	OP_JUMP_v6p,
	OP_JUMPB_v6p,

	OP_LT_U_v6p,
	OP_GT_U_v6p,
	OP_LE_U_v6p,
	OP_GE_U_v6p,

	OP_LOADBI_F_v6p,
	OP_LOADBI_V_v6p,
	OP_LOADBI_S_v6p,
	OP_LOADBI_ENT_v6p,
	OP_LOADBI_FLD_v6p,
	OP_LOADBI_FN_v6p,
	OP_LOADBI_I_v6p,
	OP_LOADBI_P_v6p,

	OP_STOREBI_F_v6p,
	OP_STOREBI_V_v6p,
	OP_STOREBI_S_v6p,
	OP_STOREBI_ENT_v6p,
	OP_STOREBI_FLD_v6p,
	OP_STOREBI_FN_v6p,
	OP_STOREBI_I_v6p,
	OP_STOREBI_P_v6p,

	OP_LEAI_v6p,

	OP_LOAD_P_v6p,
	OP_STORE_P_v6p,
	OP_STOREP_P_v6p,
	OP_NOT_P_v6p,
	OP_EQ_P_v6p,
	OP_NE_P_v6p,
	OP_LE_P_v6p,
	OP_GE_P_v6p,
	OP_LT_P_v6p,
	OP_GT_P_v6p,

	OP_MOVEI_v6p,
	OP_MOVEP_v6p,
	OP_MOVEPI_v6p,

	OP_SHR_U_v6p,

	OP_STATE_F_v6p,

	OP_ADD_Q_v6p,
	OP_SUB_Q_v6p,
	OP_MUL_Q_v6p,
	OP_MUL_QF_v6p,
	OP_MUL_FQ_v6p,
	OP_MUL_QV_v6p,
	OP_CONJ_Q_v6p,
	OP_NOT_Q_v6p,
	OP_EQ_Q_v6p,
	OP_NE_Q_v6p,
	OP_STORE_Q_v6p,
	OP_STOREB_Q_v6p,
	OP_STOREBI_Q_v6p,
	OP_STOREP_Q_v6p,
	OP_LOAD_Q_v6p,
	OP_LOADB_Q_v6p,
	OP_LOADBI_Q_v6p,
	OP_ADDRESS_Q_v6p,

	OP_RCALL0_v6p,
	OP_RCALL1_v6p,
	OP_RCALL2_v6p,
	OP_RCALL3_v6p,
	OP_RCALL4_v6p,
	OP_RCALL5_v6p,
	OP_RCALL6_v6p,
	OP_RCALL7_v6p,
	OP_RCALL8_v6p,

	OP_RETURN_V_v6p,

	OP_PUSH_S_v6p,
	OP_PUSH_F_v6p,
	OP_PUSH_V_v6p,
	OP_PUSH_ENT_v6p,
	OP_PUSH_FLD_v6p,
	OP_PUSH_FN_v6p,
	OP_PUSH_P_v6p,
	OP_PUSH_Q_v6p,
	OP_PUSH_I_v6p,
	OP_PUSH_D_v6p,

	OP_PUSHB_S_v6p,
	OP_PUSHB_F_v6p,
	OP_PUSHB_V_v6p,
	OP_PUSHB_ENT_v6p,
	OP_PUSHB_FLD_v6p,
	OP_PUSHB_FN_v6p,
	OP_PUSHB_P_v6p,
	OP_PUSHB_Q_v6p,
	OP_PUSHB_I_v6p,
	OP_PUSHB_D_v6p,

	OP_PUSHBI_S_v6p,
	OP_PUSHBI_F_v6p,
	OP_PUSHBI_V_v6p,
	OP_PUSHBI_ENT_v6p,
	OP_PUSHBI_FLD_v6p,
	OP_PUSHBI_FN_v6p,
	OP_PUSHBI_P_v6p,
	OP_PUSHBI_Q_v6p,
	OP_PUSHBI_I_v6p,
	OP_PUSHBI_D_v6p,

	OP_POP_S_v6p,
	OP_POP_F_v6p,
	OP_POP_V_v6p,
	OP_POP_ENT_v6p,
	OP_POP_FLD_v6p,
	OP_POP_FN_v6p,
	OP_POP_P_v6p,
	OP_POP_Q_v6p,
	OP_POP_I_v6p,
	OP_POP_D_v6p,

	OP_POPB_S_v6p,
	OP_POPB_F_v6p,
	OP_POPB_V_v6p,
	OP_POPB_ENT_v6p,
	OP_POPB_FLD_v6p,
	OP_POPB_FN_v6p,
	OP_POPB_P_v6p,
	OP_POPB_Q_v6p,
	OP_POPB_I_v6p,
	OP_POPB_D_v6p,

	OP_POPBI_S_v6p,
	OP_POPBI_F_v6p,
	OP_POPBI_V_v6p,
	OP_POPBI_ENT_v6p,
	OP_POPBI_FLD_v6p,
	OP_POPBI_FN_v6p,
	OP_POPBI_P_v6p,
	OP_POPBI_Q_v6p,
	OP_POPBI_I_v6p,
	OP_POPBI_D_v6p,

	OP_ADD_D_v6p,
	OP_SUB_D_v6p,
	OP_MUL_D_v6p,
	OP_MUL_QD_v6p,
	OP_MUL_DQ_v6p,
	OP_MUL_VD_v6p,
	OP_MUL_DV_v6p,
	OP_DIV_D_v6p,
	OP_REM_D_v6p,
	OP_GE_D_v6p,
	OP_LE_D_v6p,
	OP_GT_D_v6p,
	OP_LT_D_v6p,
	OP_NOT_D_v6p,
	OP_EQ_D_v6p,
	OP_NE_D_v6p,
	OP_CONV_FD_v6p,
	OP_CONV_DF_v6p,
	OP_CONV_ID_v6p,
	OP_CONV_DI_v6p,
	OP_STORE_D_v6p,
	OP_STOREB_D_v6p,
	OP_STOREBI_D_v6p,
	OP_STOREP_D_v6p,
	OP_LOAD_D_v6p,
	OP_LOADB_D_v6p,
	OP_LOADBI_D_v6p,
	OP_ADDRESS_D_v6p,

	OP_MOD_I_v6p,
	OP_MOD_F_v6p,
	OP_MOD_D_v6p,

	OP_MEMSETI_v6p,
	OP_MEMSETP_v6p,
	OP_MEMSETPI_v6p,
} pr_opcode_v6p_e;
#define OP_BREAK 0x8000

typedef struct v6p_opcode_s {
	const char *name;
	const char *opname;
	etype_t     type_a, type_b, type_c;
	unsigned int min_version;
	const char *fmt;
} v6p_opcode_t;

extern const v6p_opcode_t pr_v6p_opcodes[];
const v6p_opcode_t *PR_v6p_Opcode (pr_ushort_t opcode) __attribute__((const));
void PR_Opcode_Init (void);	// idempotent

typedef struct dstatement_s {
	pr_opcode_v6p_e op:16;
	pr_ushort_t a,b,c;
} GCC_STRUCT dstatement_t;

typedef struct ddef_s {
	pr_ushort_t type;			// if DEF_SAVEGLOBAL bit is set
								// the variable needs to be saved in savegames
	pr_ushort_t ofs;
	string_t    name;
} ddef_t;

typedef struct xdef_s {
	pointer_t   type;			///< pointer to type definition
	pointer_t   ofs;			///< 32-bit version of ddef_t.ofs
} xdef_t;

typedef struct pr_xdefs_s {
	pointer_t   xdefs;
	pr_int_t    num_xdefs;
} pr_xdefs_t;

typedef struct pr_def_s {
	pr_ushort_t type;
	pr_ushort_t size;			///< may not be correct
	pointer_t   ofs;
	string_t    name;
	pointer_t   type_encoding;
} pr_def_t;

typedef struct dparmsize_s {
	uint8_t     size:5;
	uint8_t     alignment:3;
} dparmsize_t;

#define	DEF_SAVEGLOBAL	(1<<15)

#define	MAX_PARMS	8

typedef struct dfunction_s {
	pr_int_t    first_statement;	// negative numbers are builtins
	pr_uint_t   parm_start;			// beginning of locals data space
	pr_uint_t   locals;				// total ints of parms + locals

	pr_uint_t   profile;			// runtime

	string_t    name;				// source function name
	string_t    file;				// source file defined in

	pr_int_t    numparms;			// -ve is varargs (1s comp of real count)
	dparmsize_t parm_size[MAX_PARMS];
} dfunction_t;

typedef union pr_type_u {
	float       float_var;
	string_t    string_var;
	func_t      func_var;
	pr_int_t    entity_var;
	float       vector_var;	// really [3], but this structure must be 32 bits
	float       quat_var;	// really [4], but this structure must be 32 bits
	pr_int_t    integer_var;
	pointer_t   pointer_var;
	pr_uint_t   uinteger_var;
} pr_type_t;

typedef struct pr_va_list_s {
	pr_int_t    count;
	pointer_t   list;			// pr_type_t
} pr_va_list_t;

#define PROG_VERSION_ENCODE(a,b,c)	\
	( (((0x##a) & 0x0ff) << 24)		\
	 |(((0x##b) & 0xfff) << 12)		\
	 |(((0x##c) & 0xfff) <<  0) )
#define	PROG_ID_VERSION	6
#define	PROG_VERSION	PROG_VERSION_ENCODE(0,fff,00a)

typedef struct dprograms_s {
	pr_uint_t   version;
	pr_uint_t   crc;			// check of header file

	pr_uint_t   ofs_statements;
	pr_uint_t   numstatements;	// statement 0 is an error

	pr_uint_t   ofs_globaldefs;
	pr_uint_t   numglobaldefs;

	pr_uint_t   ofs_fielddefs;
	pr_uint_t   numfielddefs;

	pr_uint_t   ofs_functions;
	pr_uint_t   numfunctions;	// function 0 is an empty

	pr_uint_t   ofs_strings;
	pr_uint_t   numstrings;		// first string is a null string

	pr_uint_t   ofs_globals;
	pr_uint_t   numglobals;

	pr_uint_t   entityfields;
} dprograms_t;

#endif//__QF_pr_comp_h
