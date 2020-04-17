/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 *  Copyright (C) 2015-2020, Rasmus Althoff <althoff@ct800.net>
 *  Copyright (C) 2010-2014, George Georgopoulos
 *
 *  This file is part of CT800/NGPlay (common definitions).
 *
 *  CT800/NGPlay is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  any later version.
 *
 *  CT800/NGPlay is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with CT800/NGPlay. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#define VERSION_INFO_STARTUP            "   CT800  V1.40   "
#define VERSION_INFO_DIALOGUE_LINE_1    "CT800 V1.40"
#define VERSION_INFO_DIALOGUE_LINE_2    "free SW: GPLv3+"
#define VERSION_INFO_DIALOGUE_LINE_3    "(c) 2020  by"
#define VERSION_INFO_DIALOGUE_LINE_4    "Rasmus Althoff"

/*different PCB IDs read on PA5-7. GND is 1, open 0. MSB on PA7.
  used to cope with minor hardware differences with only one SW.*/
/*original hardware*/
#define HW_PCB_ID_ORIGINAL 0U
/*open board with 12 V supply voltage*/
#define HW_PCB_ID_VD       1U
/*IDs 2-7 still free, treated like 0*/

#define Abs(a)             (((a) >= 0) ? (a) : -(a))
#define Max(a,b)           (((a) >= (b)) ? (a) : (b))
#define MAXMV              200
#define MAXCAPTMV          64
#define CHECKLISTLEN       64
#define START_DEPTH        2
#define ID_WINDOW_DEPTH    (START_DEPTH+2)
#define ID_WINDOW_SIZE     50
#define PV_KEEP_DEPTH      4
#define PRE_DEPTH          1
#define NULL_START_DEPTH   2
#define MAX_DEPTH          23
#define MAX_QIESC_DEPTH    10
#define MAX_PLIES          502
#define MAX_STACK          (MAX_PLIES+MAX_DEPTH+MAX_QIESC_DEPTH)
#define IID_DEPTH          5
#define LMR_MOVES          4
#define LMR_DEPTH_LIMIT    3
#define PV_ADD_DEPTH       1
#define SORT_THRESHOLD     (EASY_THRESHOLD * 2)
#define EASY_THRESHOLD     200
#define EASY_MARGIN_DOWN   (-50)
#define EASY_MARGIN_UP     50
#define EASY_DEPTH         4
/*shifted 8 bit constants are better on ARM*/
#define INFINITY_          (16384 + 8192)
#define NO_RESIGN          (INFINITY_ + 1024)
#define INF_MATE_1         (INFINITY_ - 1)
#define MATE_CUTOFF        (INFINITY_ - 1024)
#define RESIGN_EVAL        950
#define CONTEMPT_VAL       (-30)
#define CONTEMPT_END       64
#define TERMINAL_NODE      (-1)
#define LIGHT_SQ           1
#define DARK_SQ            2
#define TWO_COLOUR         3
#define PAWN_V             100
#define KNIGHT_V           320
#define BISHOP_V           325
#define ROOK_V             510
#define QUEEN_V            960
#define DELTAMARGIN        200
#define ADVANTAGE_MARGIN   50
#define WINNING_MARGIN     300
#define EG_WINNING_MARGIN  400
#define PV_CHANGE_THRESH   50
#define EG_PIECES          6
#define NULL_PIECES        6
/*15 book matches is the limit - the book format has only 4 bits for the
  number of match moves per position. currently, up to 8 are used.*/
#define MAX_BOOK_MATCH     12
#define NO_LEVEL           (-1)
#define NO_MATE_CHECK      0
#define MATE_CHECK         1
#define QUEENING           0
#define UNDERPROM          1
#define QS_NO_CHECKS       0
#define QS_CHECKS          1
#define QS_CHECK_DEPTH     4
#define QS_RECAPT_DEPTH    5 /*must be greater than QS_CHECK_DEPTH*/
#define HIGH_EVAL_NOISE    30

#define NO_ACTION_PLIES    40
#define FIFTY_MOVES_FULL   85

#define MAX_TT             0x0FFFUL
#define PMAX_TT            0x07FFUL
#define CLUSTER_SIZE       3
#define MAX_AGE_CNT        1

/*undo-function with timekeeping for the last 40 plies only*/
#define MAX_TIME_UNDO      43
#define MAX_MOVE_REDO      (MAX_PLIES+2)
#define MOVING_PIECE_SIZE  (MAX_PLIES+4)

/*now for some GCC specific attributes, builtins and pragmas.
this is encapulated via defines so that switching to another
compiler will be easier if need be.*/

/*some definitions for inlining.*/
#define ALWAYS_INLINE      __attribute__((always_inline))
#define NEVER_INLINE       __attribute__((noinline))

/*some more function attributes*/
#define FUNC_INTERRUPT     __attribute__((interrupt))
#define FUNC_NAKED         __attribute__((naked))
#define FUNC_USED          __attribute__((used))
#define FUNC_HOT           __attribute__((hot))
/*GCSE optimisation causes higher stack usage in Negascout()*/
#ifdef NOGCSE_NOT_AVAIL
    #define FUNC_NOGCSE
#else
    #define FUNC_NOGCSE    __attribute__((optimize ("no-gcse")))
#endif

/*unused variables for matching prototypes*/
#define VAR_UNUSED         __attribute__((unused))

/*alignment*/
#define ALIGN_4            __attribute__((aligned (4)))

/*compiler barrier for the TimeUp handling*/
#define COMPILER_BARRIER   __asm__ volatile("" : : : "memory")

/*telling LIKELY/UNLIKELY branches*/
#define LIKELY(x)          __builtin_expect(!!(x),1)
#define UNLIKELY(x)        __builtin_expect(!!(x),0)

/*status flags for play and opening book (gflags)*/
#define WKMOVED            1
#define WRA1MOVED          2
#define WRH1MOVED          4
#define BKMOVED            8
#define BRA8MOVED          16
#define BRH8MOVED          32

/*status flags for play (gflags)*/
#define WCASTLED           64
#define BCASTLED           128
#define BLACK_MOVED        256
#define HASHFLAGS          (WKMOVED | WRA1MOVED | WRH1MOVED | BKMOVED | BRA8MOVED | BRH8MOVED | BLACK_MOVED)
#define ALLFLAGS           (HASHFLAGS | WCASTLED | BCASTLED)
#define FLAGRESET          0

/*status flags for the opening book (gflags)*/
#define CASTL_FLAGS        63
#define BLACK_TO_MOVE      64

/*board files definitions for the pawn masking*/
#define ALL_FILES_FREE     0x00U
#define NO_FILES_FREE      0xFFU
#define H_FILE             0x80U
#define G_FILE             0x40U
#define F_FILE             0x20U
#define E_FILE             0x10U
#define D_FILE             0x08U
#define C_FILE             0x04U
#define B_FILE             0x02U
#define A_FILE             0x01U

#define MIDDLE_FILES       (C_FILE | D_FILE | E_FILE | F_FILE)
#define CENTRE_FILES       (D_FILE | E_FILE)
#define KINGSIDE_FILES     (F_FILE | G_FILE | H_FILE)
#define QUEENSIDE_FILES    (A_FILE | B_FILE | C_FILE)
#define NOT_CENTRE_FILES   (KINGSIDE_FILES | QUEENSIDE_FILES)
#define EDGE_FILES         (A_FILE | H_FILE)
#define FLANK_FILES        (B_FILE | C_FILE | F_FILE | G_FILE)
#define QUEEN_SIDE         (A_FILE | B_FILE | C_FILE | D_FILE)
#define KING_SIDE          (E_FILE | F_FILE | G_FILE | H_FILE)

#define BOARD_A_FILE       1
#define BOARD_B_FILE       2
#define BOARD_C_FILE       3
#define BOARD_D_FILE       4
#define BOARD_E_FILE       5
#define BOARD_F_FILE       6
#define BOARD_G_FILE       7
#define BOARD_H_FILE       8

/*extra score in centipawns in middlegame for pawns on DE / FC files*/
#define PAWN_DE_VAL       10
#define PAWN_FC_VAL        5

/*castling options in the position editor*/
#define CST_NONE           0
#define CST_WH_KING        1
#define CST_WH_QUEEN       2
#define CST_BL_KING        4
#define CST_BL_QUEEN       8

/*the play states for the Play() loop in play.c*/
enum E_PST_STATE {
    PST_NEW_WH_MOVE,
    PST_PLAY_WHITE,
    PST_POST_WHITE,
    PST_PLAY_BLACK,
    PST_POST_BLACK,
    PST_EXIT
};

/*computer search answer states*/
enum E_COMP_RESULT {
    COMP_MOVE_FOUND,
    COMP_MATE,
    COMP_STALE,
    COMP_MAT_DRAW,
    COMP_FIFTY_MOVES,
    COMP_THREE_REP,
    COMP_RESIGN,
    COMP_STACK_FULL,
    COMP_NO_MOVE
};

/*time control after move*/
enum E_TIME_CONTROL {TIME_BOTH_OK, TIME_WHITE_LOST, TIME_BLACK_LOST};

/*intermediate time control during moves*/
enum E_TIME_INT_CHECK {TIME_OK, TIME_FAIL};

/*whether to trigger a search display toggle on time update*/
enum E_TIME_DISP {NO_DISP_TOGGLE, DISP_TOGGLE};

/*which search info display mode. for more modes, also
  SAVE_SEARCH_DISP, Hmi_Load_Status() and Hmi_Save_Status()
  must be modified wth more bits.*/
enum E_SEARCH_DISP {DISP_NORM, DISP_ALT};

/*return values for Get_User_Input() when the user can enter a move*/
enum E_HMI_INPUT {
    HMI_ENTER_PLY,
    HMI_UNDO_MOVE,
    HMI_REDO_MOVE,
    HMI_COMP_GO,
    HMI_NEW_GAME,
    HMI_NEW_POS,
    HMI_GENERAL_INPUT
};

/*user input during dialogues*/
enum E_HMI_USER {
    HMI_USER_OK,
    HMI_USER_CANCEL,
    HMI_USER_FLIP,
    HMI_USER_DISP,
    HMI_USER_INVALID
};

/*return values from the menu*/
enum E_HMI_MENU {
    HMI_MENU_OK,
    HMI_MENU_LEAVE,
    HMI_MENU_NEW_GAME,
    HMI_MENU_NEW_POS,
    HMI_MENU_INVALID
};

/*position display via menu or directly*/
enum E_HMI_MEN_POS {HMI_MENU_MODE_POS, HMI_MENU_MODE_MEN};

/*dialogue mode/caption options*/
enum E_HMI_DIALOGUE {
    HMI_QUESTION,
    HMI_INFO,
    HMI_MULTI_STAT,
    HMI_MONO_STAT,
    HMI_PV,
    HMI_POS_SEL,
    HMI_NO_FEEDBACK
};

/*whether the move shall be confirmed in the move list or nor.*/
enum E_HMI_CONF {HMI_CONFIRM, HMI_NO_CONFIRM};

/*whether a dialogue shall restore the screen afterwards or not*/
enum E_HMI_REST_MODE {HMI_RESTORE, HMI_NO_RESTORE, HMI_BOARD_MENU};

/*dialogues without timeout*/
#define HMI_NO_TIMEOUT     (-1)

#define HMI_MOVE_POS       65

/*for the "pretty print" move display*/
enum E_HMI_CHECK {HMI_CHECK_STATUS_NONE, HMI_CHECK_STATUS_CHECK, HMI_CHECK_STATUS_MATE};

/*for signalling OK, error beeps and the like*/
enum E_HMI_MSG {
    HMI_MSG_OK,
    HMI_MSG_ERROR,
    HMI_MSG_FAILURE,
    HMI_MSG_ATT,
    HMI_MSG_MOVE,
    HMI_MSG_TEST
};

enum E_CLK_FORCE {CLK_ALLOW_LOW, CLK_FORCE_HIGH, CLK_FORCE_AUTO};

/*power-on self test results*/
#define HW_SYSTEM_OK       0U
#define HW_ROM_FAIL        (1U << 0)
#define HW_RAM_FAIL        (1U << 1)
#define HW_XTAL_FAIL       (1U << 2)
#define HW_KEYS_FAIL       (1U << 3)

/*system reset causes. useful for fault analysis.*/
#define HW_SYSRESET_POWER  (1U << 0)
#define HW_SYSRESET_WDG    (1U << 1)
#define HW_SYSRESET_PIN    (1U << 2)
#define HW_SYSRESET_SW     (1U << 3)

/*charsets*/
enum E_HW_CHARSET {HW_CHARSET_NORM, HW_CHARSET_CGOL};

/*whether a screen is a dialogue and has to be beautified on ARM
  or raw display data*/
enum E_HW_DISP {HW_DISP_RAW, HW_DISP_DIALOGUE};

/*display modes*/
enum E_HW_DISP_ONOFF {HW_DISP_OFF, HW_DISP_ON};

/*keypad*/
enum HW_KEYBD_MODE {
    HW_KEYBD_MODE_INIT,
    HW_KEYBD_MODE_USER,
    HW_KEYBD_MODE_COMP,
    HW_KEYBD_MODE_ENT
};

/*HW signal handler*/
enum E_HW_MSG {
    HW_MSG_INIT,
    HW_MSG_LED_GREEN_ON,
    HW_MSG_LED_RED_ON,
    HW_MSG_LED_BACK_ON,
    HW_MSG_LED_BACK_OFF,
    HW_MSG_LED_BACK_FADE,
    HW_MSG_LED_BACK_INHIB,
    HW_MSG_LED_BACK_ALLOW,
    HW_MSG_BEEP_ON
};

#define HW_MSG_NO_DURATION 0

enum E_HW_MSG_PARAM {
    HW_MSG_PARAM_NONE,
    HW_MSG_PARAM_CLICK,
    HW_MSG_PARAM_BEEP,
    HW_MSG_PARAM_MOVE,
    HW_MSG_PARAM_ERROR,
    HW_MSG_PARAM_BACK_CONF,
    HW_MSG_PARAM_BACK_FORCE
};

#define PIGS_DO_NOT_FLY    1U

/*milliseconds per second*/
#define MILLISECONDS       1000L

/*maximum systime: 24 days, that is the limit because the system time
is in milliseconds and an int32_t. shifted 8 bit constants are more
efficient in the ARM instruction set, that's why 7F.
no action takes that long. note that the IO timers for keyboard, LEDs
and buzzer still will work because they are countdown software timers.
besides, the system time is reset to 0 before each move.*/
#define MAX_SYS_TIME       0x7F000000L

/*define battery levels.
there is a 20k/10k voltage divisor that divides the input voltage by 3.
there are four batteries in series so that the total voltage is 4 times the cell voltage.
and 3.3V total voltage correspond to 4095 decimal from the ADC.
Similarly for a 47k/10k divider (10/57), useful for up to 12V input voltage.

example calculation for the shutdown voltage:
4*1.0V = 4.0V. Divided by 3: 4.0V/3 = 1.33V. ADC reading: 1.33V/3.3V*4095 = 1655 decimal.*/

/*immediate shutdown shall happen at 1.0V per battery, which is still safe for NiMH batteries.*/
enum {
    BATTERY_SHUTDOWN_LEVEL,
    BATTERY_HI_TO_LO_LEVEL,
    BATTERY_STARTUP_VOLTAGE,
    BATTERY_LO_TO_HI_LEVEL,
    BATTERY_NORM_LEVEL,
    BATTERY_LOW_VALID_LEVEL,
    BATTERY_HIGH_VALID_LEVEL,
    BATTERY_LEVEL_ENTRIES
};

#define BATTERY_SHUTDOWN_LEVEL_20K   1655UL
#define BATTERY_SHUTDOWN_LEVEL_47K    871UL

/*when going from high state to low state, assume 1.06V.*/
#define BATTERY_HI_TO_LO_LEVEL_20K   1754UL
#define BATTERY_HI_TO_LO_LEVEL_47K    923UL

/*minimum voltage when starting a new game, assume 1.10V.*/
#define BATTERY_STARTUP_VOLTAGE_20K  1820UL
#define BATTERY_STARTUP_VOLTAGE_47K   958UL

/*when going from low state to high state (slight hystereris), assume 1.15V.*/
#define BATTERY_LO_TO_HI_LEVEL_20K   1903UL
#define BATTERY_LO_TO_HI_LEVEL_47K   1001UL

/*regular level is 1.25V*/
#define BATTERY_NORM_LEVEL_20K       2068UL
#define BATTERY_NORM_LEVEL_47K       1089UL

/*3.6V total input voltage, i.e. 0.9V per cell.
  Values lower than that will be clipped upwards to this value.*/
#define BATTERY_LOW_VALID_LEVEL_20K  1492UL
#define BATTERY_LOW_VALID_LEVEL_47K   784UL

/*5.4V total input voltage, i.e. 1.35V per cell.
  Values higher than that will be clipped downwards to this value.*/
#define BATTERY_HIGH_VALID_LEVEL_20K 2232UL
#define BATTERY_HIGH_VALID_LEVEL_47K 1176UL

/*possible logical battery states, bitwise*/
#define BATTERY_HIGH       (1u << 0)
#define BATTERY_LOW        (1u << 1)
#define BATTERY_SHUTDOWN   (1u << 2)

#define BATTERY_CONF_HIGH  (1u << 0)
#define BATTERY_CONF_LOW   (1u << 1)

enum E_BAT_CHECK {BAT_NO_CHECK, BAT_CHECK};

/*for the battery monitoring*/
enum E_WHOSE_TURN {USER_TURN, COMP_TURN};

/*the duration of the load stress test before a new game, in ms.
must be a multiple of 16 because of the progress bar and of
10 because of the system time resolution.*/
#define BAT_MON_DELAY      960L

/*2000ms boot screen - minimum is 320ms to get the random generator seed
  initialised using the LSB of the battery reading (every 10ms),
  and 640ms for the battery monitoring.*/
#define BOOT_SCREEN_DELAY  2000L

/*the boot screen must be long enough for the battery check.*/
#if (BOOT_SCREEN_DELAY < BAT_MON_DELAY)
    #error "boot screen delay too short!"
#endif

/*for the board display*/
#define HMI_BLACK_BOTTOM   0U
#define HMI_WHITE_BOTTOM   1U

/*in milliseconds*/
#define LED_SHORT          500UL
#define LED_LONG           1000UL
#define LED_BOOT           2000UL

#define BEEP_CLICK         10UL
#define BEEP_SHORT         130UL
#define BEEP_LONG          260UL

#define BEEP_MOVE          330UL
#define BEEP_MOVE_OFF      200UL
#define BEEP_MOVE_ON       130UL

#define BACKLIGHT_FADE     500UL
#define BACKLIGHT_MOVE     15000UL
#define BACKLIGHT_KEY      15000UL
#define BACKLIGHT_ANA      30000UL
#define BACKLIGHT_REQ      30000UL
#define BACKLIGHT_POS      30000UL

#define BAT_ANNOUNCE_LINE_1   "batteries low,"
#define BAT_ANNOUNCE_LINE_2   "change them."

#define BAT_SHUTDOWN_TIMEOUT  (15L * 60L * 1000L)
#define BAT_SHUTDOWN_LINE_1   "batteries failing,"
#define BAT_SHUTDOWN_LINE_2   "switch device off!"

#define DISP_CURSOR_OFF    (-1)
#define DISP_CURSOR_ON       1

/* the keypad:
 1  2  3  4
 5  6  7  8
 9 10 11 12
13 14 15 16

1-8: keys A1-H8.

9: menu
10: info (PV etc)
11: pos display
12: go (force move)

13: arrow backwards (undo)
14: arrow forwards (redo)
15: cancel (CL)
16: enter (ENT)
*/

/*the raw keys as returned by the keyboard driver*/
enum E_KEY {
    KEY_NONE,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,
    KEY_10,
    KEY_11,
    KEY_12,
    KEY_13,
    KEY_14,
    KEY_15,
    KEY_16,
    KEY_ERROR,
    /*virtual key*/
    KEY_V_0
};

/*the key mapping*/
#define KEY_A1    KEY_1
#define KEY_B2    KEY_2
#define KEY_C3    KEY_3
#define KEY_D4    KEY_4
#define KEY_E5    KEY_5
#define KEY_F6    KEY_6
#define KEY_G7    KEY_7
#define KEY_H8    KEY_8

/*keys A1-D4 double-serve as promotion keys*/
#define KEY_PROM_QUEEN         KEY_A1
#define KEY_PROM_ROOK          KEY_B2
#define KEY_PROM_BISHOP        KEY_C3
#define KEY_PROM_KNIGHT        KEY_D4

#define KEY_MENU               KEY_9
#define KEY_INFO               KEY_10
#define KEY_POS_DISP           KEY_11
#define KEY_GO                 KEY_12

#define KEY_UNDO               KEY_13
#define KEY_REDO               KEY_14
#define KEY_CL                 KEY_15
#define KEY_ENT                KEY_16
/*virtual key for clearing an entered coordinate file*/
#define KEY_V_FCL              KEY_V_0

/*key mapping within the menu*/
#define KEY_MENU_NEW_GAME      KEY_A1
#define KEY_MENU_FILE          KEY_B2
#define KEY_MENU_POS           KEY_C3
#define KEY_MENU_TIME          KEY_D4
#define KEY_MENU_MISC          KEY_E5
#define KEY_MENU_INFO          KEY_F6

#define KEY_MENU_FILE_LOAD     KEY_A1
#define KEY_MENU_FILE_SAVE     KEY_B2
#define KEY_MENU_FILE_ERASE    KEY_C3
#define KEY_MENU_FILE_BOOK     KEY_D4
#define KEY_MENU_FILE_RESET    KEY_E5

#define KEY_MENU_POS_VIEW      KEY_A1
#define KEY_MENU_POS_EDIT      KEY_B2
#define KEY_MENU_POS_MOVELIST  KEY_C3

#define KEY_MENU_BONI_FISCHER  KEY_A1
#define KEY_MENU_BONI_PLAYER   KEY_B2
#define KEY_MENU_BONI_FACTOR   KEY_C3

#define KEY_MENU_GAME_MODE     KEY_A1
#define KEY_MENU_TIME_DETAILS  KEY_B2
#define KEY_MENU_TIME_BONI     KEY_C3

#define KEY_MENU_MISC_COL      KEY_A1
#define KEY_MENU_MISC_NOISE    KEY_B2
#define KEY_MENU_MISC_SPEED    KEY_C3
#define KEY_MENU_MISC_DISP     KEY_D4
#define KEY_MENU_MISC_LIGHT    KEY_E5
#define KEY_MENU_MISC_SPEAKER  KEY_F6

#define KEY_MENU_PLUS          KEY_REDO
#define KEY_MENU_MINUS         KEY_UNDO

/*for various situations, make clear to the get-char-function which keys are acceptable.*/

#define KEY_A1_MASK        (1UL << KEY_A1)
#define KEY_B2_MASK        (1UL << KEY_B2)
#define KEY_C3_MASK        (1UL << KEY_C3)
#define KEY_D4_MASK        (1UL << KEY_D4)
#define KEY_E5_MASK        (1UL << KEY_E5)
#define KEY_F6_MASK        (1UL << KEY_F6)
#define KEY_G7_MASK        (1UL << KEY_G7)
#define KEY_H8_MASK        (1UL << KEY_H8)

#define KEY_SQUARES_MASK (KEY_A1_MASK | KEY_B2_MASK | KEY_C3_MASK | KEY_D4_MASK | KEY_E5_MASK | KEY_F6_MASK | KEY_G7_MASK | KEY_H8_MASK)

#define KEY_PROM_QUEEN_MASK     (1UL << KEY_PROM_QUEEN)
#define KEY_PROM_ROOK_MASK      (1UL << KEY_PROM_ROOK)
#define KEY_PROM_BISHOP_MASK    (1UL << KEY_PROM_BISHOP)
#define KEY_PROM_KNIGHT_MASK    (1UL << KEY_PROM_KNIGHT)

#define KEY_PROM_ALL_MASK (KEY_PROM_QUEEN_MASK | KEY_PROM_ROOK_MASK | KEY_PROM_BISHOP_MASK | KEY_PROM_KNIGHT_MASK)

#define KEY_ENT_MASK       (1UL << KEY_ENT)
#define KEY_CL_MASK        (1UL << KEY_CL)
#define KEY_POS_MASK       (1UL << KEY_POS)
#define KEY_ENT_CL_MASK    (KEY_ENT_MASK | KEY_CL_MASK)
#define KEY_ENT_POS_MASK   (KEY_ENT_MASK | KEY_POS_MASK)

/*for dynamic system speed changes*/
/*speed. what HIGH is depends on the system configuration.*/
enum E_SYS_SPEED {SYSTEM_SPEED_LOW, SYSTEM_SPEED_HIGH};
/*mode can be user, with keypad enabled and 1 ms systick (but the system
  clock resolution is still 10 ms!), or computer with only GO and light
  key enabled, and 10 ms systick.*/
enum E_SYS_MODE {SYSTEM_MODE_KEEP, SYSTEM_MODE_USER, SYSTEM_MODE_COMP};

/*for the keyboard handler, which can enter CPU sleep until next systick
  if no keyboard data have been arriving.*/
enum E_WAIT_SLEEP {SLEEP_ALLOWED, SLEEP_FORBIDDEN};

/*states of the position editor*/
enum E_POS_EDIT_STATE {
    POS_ENTER_WKING,
    POS_ENTER_WQUEENS,
    POS_ENTER_WROOKS,
    POS_ENTER_WBISHOPS,
    POS_ENTER_WKNIGHTS,
    POS_ENTER_WPAWNS,
    POS_ENTER_BKING,
    POS_ENTER_BQUEENS,
    POS_ENTER_BROOKS,
    POS_ENTER_BBISHOPS,
    POS_ENTER_BKNIGHTS,
    POS_ENTER_BPAWNS,
    POS_PIECES_ENTERED,
    POS_ENTER_TURN,
    POS_ENTER_EP,
    POS_ENTER_CASTL,
    POS_ENTER_VIEW,
    POS_ENTER_FINISHED,
    POS_ENTER_CANCELLED
};

/*fake piece counts*/
enum {POS_PCS_UNDO = 97, POS_PCS_REDO, POS_PCS_LEAVE};

/*return codes for checking an entered position*/
enum E_POS_STATE {
    POS_OK,
    POS_TOO_MANY_PIECES,
    POS_KING_INVALID,
    POS_CHECKS_INVALID,
    POS_TOO_MANY_MOVES,
    POS_TOO_MANY_CAPTS,
    POS_TOO_MANY_CHECKS
};

/*for the info dialogue with the position evaluation*/
enum E_POS_EVAL {EVAL_INVALID, EVAL_BOOK, EVAL_MOVE};

enum E_SAVE_TYPE {HW_MANUAL_SAVE, HW_AUTO_SAVE};

enum E_AUTOSAVE {HW_AUTOSAVE_OFF, HW_AUTOSAVE_ON};

enum E_FILEOP {HW_FILEOP_FAILED, HW_FILEOP_OK_AUTO, HW_FILEOP_OK_MAN};

/*for converting time to strings*/
enum E_UT_LEAD_ZEROS {UT_NO_LEAD_ZEROS, UT_LEAD_ZEROS};
enum E_UT_ROUND {UT_TIME_ROUND_CEIL, UT_TIME_ROUND_FLOOR};

/*during search, the user cancellation is driven via simulating
a timeout. in order to know whether a timeout actually is a real
one, the timeout flag is set to different values.*/
enum E_TIMEOUT {TM_NO_TIMEOUT, TM_TIMEOUT, TM_USER_CANCEL};


/*use this for debug tests of stack usage, both on PC and ARM.
suggested stack measurement method:
- enter the following position: white king e1, white pawn e2, black king e8, white to move.
- set the time mode:
    on the PC: time per move, give 180s. Press 'x' when the move is there. The relevant
    stack usage is reported after the 4th move by white (e2-e3).
    on ARM: use the analysis mode, wait until depth 20 is reached.
    - press GO. wait for the answer move and confirm the stack usage dialogue box.
    - do this until 4. e2-e3 is the played move.
the dialogue box that is displayed now contains the maximum stack usage.
add up about 2k for the quescence recursion, whic isn't triggered in this little test.
then add at least 1k as reserve e.g. for interrupts or register spill.

Note: this is only an empirical analysis, i.e. a rough estimation; for a serious
analysis, see documentation/doc_software/stack_analysis.
*/

//#define DEBUG_STACK

/* for the PC version only: enhanced debug output possible with the defines below*/
//#define PC_VERSION

#ifdef PC_VERSION

#define SECONDS_PASSED     ((Time_Passed())/1000.0)

/*activate the node counter*/
//#define G_NODES

/*show the board in verbose PC version*/
#define SHOW_BOARD

/*verbose output in PC version*/
#define PC_PRINTF

#define CCM_RAM
#define FLASH_ROM
#define DATA_SECTION

#else /* ARM-VERSION - for forcing variables into different memory sections*/
/*64 kB core coupled memory. only suited for data, but code is not loaded
  into RAM anywhere in this project, and not for DMA, but DMA is not used
  in this project.
  Only for zero initialised data because the init loop in boot_stm32f405.c
  does not contain ROM-RAM copy code for CCM variables.*/
#define CCM_RAM            __attribute__((section(".ccmram")))

/*1 MB of flash ROM. const alone only tells the compiler that the variable
  is not being written programatically, but to be sure it really ends up in
  ROM, force the compiler to link it there.*/
#define FLASH_ROM          __attribute__((section(".rodata")))

/*128 kB of main memory, also for non-zero initialised data.*/
#define DATA_SECTION       __attribute__((section(".data")))
#endif

/*this is practically equivalent to using the sizeof operator during compile
time - a bit tricky because sizeof and #if cannot be combined directly. Taken
from the Linux kernel.
ISO C doesn't allow 0 length arrays, so the mapping goes to 1 and -1. Since
the array is declared as extern, it doesn't cost memory.*/
#define BUILD_ASSERT(condition, assertlabel, message) extern int assertlabel[((!!(condition)) * 2) - 1]

/*piece types for the board and MVV/LVA*/
enum
{
    NO_PIECE,
    WPAWN =  2, WKNIGHT, WBISHOP, WROOK, WQUEEN, WKING,
    BPAWN = 12, BKNIGHT, BBISHOP, BROOK, BQUEEN, BKING,
    PIECEMAX
};

/*fake "piece type" of the en passant square for the position editor*/
#define POSEDIT_EPSQ_TYPE  (PIECEMAX+1)

enum E_COLOUR {WHITE = 1, NONE = 3, BLACK = 10};

#define WKING_CHAR         'K'
#define WQUEEN_CHAR        'Q'
#define WROOK_CHAR         'R'
#define WBISHOP_CHAR       'B'
#define WKNIGHT_CHAR       'N'
#define WPAWN_CHAR         'P'
#define BKING_CHAR         'k'
#define BQUEEN_CHAR        'q'
#define BROOK_CHAR         'r'
#define BBISHOP_CHAR       'b'
#define BKNIGHT_CHAR       'n'
#define BPAWN_CHAR         'p'

enum E_DISP_CASE {MIXEDCASE, UPPERCASE};

enum {NORMAL, CASTL, PROMOT};
enum {NO_FLAG, EXACT, CHECK_ALPHA, CHECK_BETA};
enum {CUT_NODE, PV_NODE};

enum
{
    A1=21, B1, C1, D1, E1, F1, G1, H1,
    A2=31, B2, C2, D2, E2, F2, G2, H2,
    A3=41, B3, C3, D3, E3, F3, G3, H3,
    A4=51, B4, C4, D4, E4, F4, G4, H4,
    A5=61, B5, C5, D5, E5, F5, G5, H5,
    A6=71, B6, C6, D6, E6, F6, G6, H6,
    A7=81, B7, C7, D7, E7, F7, G7, H7,
    A8=91, B8, C8, D8, E8, F8, G8, H8, ENDSQ
};

#define FILE_DIFF (B1 - A1)
#define RANK_DIFF (A2 - A1)

/*for the opening do-moves-function.*/

enum E_BP_SQUARE
{
    BP_A1, BP_B1, BP_C1, BP_D1, BP_E1, BP_F1, BP_G1, BP_H1,
    BP_A2, BP_B2, BP_C2, BP_D2, BP_E2, BP_F2, BP_G2, BP_H2,
    BP_A3, BP_B3, BP_C3, BP_D3, BP_E3, BP_F3, BP_G3, BP_H3,
    BP_A4, BP_B4, BP_C4, BP_D4, BP_E4, BP_F4, BP_G4, BP_H4,
    BP_A5, BP_B5, BP_C5, BP_D5, BP_E5, BP_F5, BP_G5, BP_H5,
    BP_A6, BP_B6, BP_C6, BP_D6, BP_E6, BP_F6, BP_G6, BP_H6,
    BP_A7, BP_B7, BP_C7, BP_D7, BP_E7, BP_F7, BP_G7, BP_H7,
    BP_A8, BP_B8, BP_C8, BP_D8, BP_E8, BP_F8, BP_G8, BP_H8, BP_NOSQUARE
};

#define BP_FILE_DIFF       (BP_B1 - BP_A1)
#define BP_RANK_DIFF       (BP_A2 - BP_A1)
#define BP_UP_RIGHT        (BP_RANK_DIFF + BP_FILE_DIFF)
#define BP_UP_LEFT         (BP_RANK_DIFF - BP_FILE_DIFF)
#define BP_FILE(square)    (square % BP_RANK_DIFF)
#define BP_RANK(square)    (square / BP_RANK_DIFF)
#define BP_STATUS_FLAGS    64
#define BP_COL_MASK        (0x07U << 0)
#define BP_RANK_MASK       (0x07U << 3)
#define BP_MV_MASK         (BP_RANK_MASK | BP_COL_MASK)

/*for accessing the KPK endgame table in an efficient, but still
endianess-proof manner*/
struct bytewise
{
    uint8_t byte_0;
    uint8_t byte_1;
    uint8_t byte_2;
    uint8_t byte_3;
};

typedef union
{
    struct bytewise bytes;
    uint32_t u;
} ENTRY_32;


#define MV_NO_MOVE_MASK    0x00000000UL
#define MV_NO_MOVE_CMASK   0x0000U
/*regular search*/
#define MVV_LVA_MATE_1     127
#define MVV_LVA_PV         126
#define MVV_LVA_HASH       125
#define MVV_LVA_THREAT     110
#define MVV_LVA_CSTL_SHORT  91
#define MVV_LVA_CSTL_LONG   85
#define MVV_LVA_KILLER_0     2
#define MVV_LVA_KILLER_1     1
#define MVV_LVA_TACTICAL     0
#define MVV_LVA_ILLEGAL   -126
/*mate searcher*/
#define MVV_LVA_CHECK      126
/*50 moves resort*/
#define MVV_LVA_50_OK      125
#define MVV_LVA_50_NOK    -124
struct mvdata
{
    uint8_t flag;   /*0:NULL, 1:piece move, >1: pawn move  2=WPAWN or 12=BPAWN : simple pawn move, else stores promotion piece */
    uint8_t from;   /*in 10x12 mailbox format*/
    uint8_t to;     /*in 10x12 mailbox format*/
    int8_t mvv_lva; /* stores value for move ordering. >0 captures or pawn promotion,  <0 non captures */
};

/*for this union, u must have the same size as the struct!*/
typedef union
{
    struct mvdata m;
    uint32_t u;
} MOVE;

typedef uint16_t CMOVE; /*compressed move*/

/*for move lines like PV*/
typedef struct t_line
{
    int16_t line_len;               /*Number of moves in the line.*/
    CMOVE line_cmoves[MAX_DEPTH+2]; /*The line in compressed move format.*/
} LINE;

typedef struct piece_st
{
    struct piece_st *next;
    struct piece_st *prev;
    int8_t type;
    int8_t xy;
    int8_t mobility; /* for pieces : number of moves - max_per piece/2 , for pawns: passed pawn evaluation */
} PIECE;

/*struct for the main move stack.
  packed structure on Cortex-M4 because this is a 32 bit processor,
  so even uint64_t needs only 4 byte alignment. This saves 4 bytes
  padding per entry, or about 2kB in total.*/
typedef struct __attribute__((packed, aligned(4))) mvst
{
    uint64_t mv_pos_hash;
    uint64_t mv_pawn_hash;
    MOVE move;
    PIECE *captured;
    int16_t material;
    int8_t capt;
    uint8_t special;   /* values: NORMAL,CASTL,PROMOT */
} MVST;

/*general notes on the hash tables: of course, if would be nice to store the full 64bit
position hash and not just the upper 32bits, but with 4096*2+2048 entries, that would require
additional (!) 20kB of RAM. That isn't left over, so the hash table sizes would have to be reduced.
So only 32bit are saved, 6 more bits get fiddled into some free space within the entries, and the
index serves also as part of the hash sum. All in all, that gives around 48bits of hash without
wasting 20kB.*/

typedef struct tt_st
{
    /*if the cashed entry is not a terminal node, it is even more valuable because it represents a whole
    subtree. if it is a terminal node, then the move entry will be set to 0. so if the move entry
    is not zero, a hash collision would be more severe - that's why, if there is a stored move, it is
    checked for pseudo legality when looking up the entry. if we have a hash collision, it is not
    probable that the right kind of piece is at the from-square for that move, and that the to-square is
    either free or occupied by an enemy piece.*/
    CMOVE cmove;
    uint16_t pos_hash_upper_h;
    uint16_t pos_hash_upper_l;
    int16_t value;
    /*the flag holds the values 1, 2 or 3 in the lowest two bits, depending on whether the stored value
    is an exact evaluation, an alpha- or a beta bound. 0 is not used, so that these bit also work as validity
    bits since the table is cleared before starting the move calculation.
    that means the upper 6 bits are still free, so 6 more bits from the 64 bit position hash are stuffed
    in here. the lowest 12 bits work as index, with a cluster length of 3, so that effectively, 10 bits
    from the position hash are indirectly stored here via the index. All in all, that gives a stored
    position hash length of 32+6+10=48 bits.*/
    uint8_t flag;
    /*the lower 6 bits of depth contain the actual depth. The upper 2 bits contain the hash clear
    counter which is increased upon every search so that before each search, the oldest 25% of
    the entries are cleared.*/
    uint8_t depth;
#if (MAX_DEPTH > 63)
    #error "Review the TT_ST data structure with depth (too high) and clear counter!"
#endif
#if (MAX_AGE_CNT > 3)
    #error "Review the TT_ST data structure with clear counter (too high) and depth!"
#endif
} TT_ST;


typedef struct tt_ptt_st
{
    /*here, we use only 32bits from the position hash plus 11 bits indirectly from the index,
    so that the effective stored hash length becomes 43. 5 more hash bits get fiddled into the
    value variable, which is only used in the range of +/-511. Then we are at 48 bits of hash.*/
    uint32_t pawn_hash_upper;
#define PTT_VALUE_BITS     0x01FFUL
#define PTT_SIGN_BIT       0x0200UL
#define PTT_MG_BIT         0x0400UL
#define PTT_HASH_BITS      0xF800UL
    /*the lowest 9 bits of the value hold the absolute value, the 10th is for the sign.
    1 bit is for storing whether the pawn structure eval was only middlegame (MG) or
    with the endgame modifications.
    this gives 5 more bits to stuff in a bigger portion of the pawn hash value, as to
    minimise hash collisions.

    Of course, the bit twiddling could be done with a signed 16bit integer as well, but
    the C standard doesn't specify how a signed int is implemented. On ARM as well as
    on x86, it is the two's complement, but the unsigned int is specified (though not
    in the byte order, of course)*/
    uint16_t value;
    uint8_t w_pawn_mask; /*files with white pawns. LSB is the A file.*/
    uint8_t b_pawn_mask;
} TT_PTT_ST;

/*make this a separate table to avoid padding issues (or performance penalty).*/
typedef struct tt_ptt_rook_st
{
    uint8_t w_rook_files;
    uint8_t b_rook_files;
} TT_PTT_ROOK_ST;

typedef struct t_game_info
{
    int depth;
    int eval;
    int last_valid_eval;
    enum E_POS_EVAL valid;
} GAME_INFO;

typedef struct t_move_redo_stack
{
    CMOVE buffer[MAX_MOVE_REDO];
    int32_t index;
} MOVE_REDO_STACK;

typedef struct t_starting_pos
{
    uint32_t gflags; /*castling flags.*/
    uint32_t epsquare;
    uint8_t board[64]; /*holds the piece standing on a square or 0. a1 = index 0, h1 = index 7, h8 = index 63.*/
} STARTING_POS;

/*opening book position keeping*/
typedef struct t_pos
{
    uint8_t board[65]; /*holds the piece standing on a square or 0. a1 = index 0, h1 = index 7, h8 = index 63.
                            index 64 is the flags for castling rights.*/
} BPOS;

typedef struct t_time_keeping
{
    int32_t time_used_buffer[MAX_TIME_UNDO]; /*a stack buffer for keeping track of the used time*/
    int32_t time_used_index; /*current index for the buffer above*/
    int32_t remaining_white_time, remaining_black_time; /*in seconds*/
    int32_t next_time_control_move_number; /*for tournament settings*/
    int32_t next_full_second; /*used for the display of the thinking time to avoid frequent div and modulo operations*/
    int32_t dialogue_conf_time; /*holds confirmation times for dialogues during a ply*/
} TIME_KEEPING;

#define SAVE_BLACK_START    (1U << 0)
#define SAVE_SEARCH_DISP    (1U << 1)

typedef struct t_backup_game
{
    uint64_t hw_config;
    int16_t mv_stack_p;
    uint8_t blackstart_searchdisp;
    uint8_t game_started_from_0;
    uint8_t computer_side;
    uint8_t autosave;
    int16_t dynamic_resign_threshold;
    STARTING_POS starting_pos;
    GAME_INFO game_info;
    CMOVE movelist[MAX_PLIES+1];
    uint8_t moving_piece[MOVING_PIECE_SIZE];
    LINE GlobalPV;
    MOVE player_move;
    MOVE_REDO_STACK move_redo_stack;
    TIME_KEEPING time_keeping;
    int32_t menu_start_time;
    int32_t start_time;
    int32_t stop_time;
    uint32_t randomness_state;
} BACKUP_GAME;

typedef struct t_sys_backup
{
    BACKUP_GAME backup_game;
    uint32_t crc_32;
} SYS_BACKUP;

/*the following typedefs are just for eval.c. their point is
to split the static eval function without having to pass around tons
of variables, just struct pointers.

note: these structs must consist of 32bit integers only due to a
zeroing hack in eval.c where a uint32_t pointer is used to init
them instead of Util_Memzero - benchmarking showed that this speeds
up the eval function noticeably because it is used extremely often.

32bit int isn't always needed, but on ARM, that is faster than using some
int8_t.*/
typedef struct t_pawn_info
{
    int32_t w_pawns;
    int32_t b_pawns;
    int32_t all_pawns;
    int32_t w_isolani;
    int32_t b_isolani;
    int32_t w_outpassed;
    int32_t b_outpassed;
    int32_t deval_pawn_majority;
    uint32_t w_passed_rows[8];
    uint32_t b_passed_rows[8];
    uint32_t w_pawn_mask;
    uint32_t b_pawn_mask;
    uint32_t w_rook_files;
    uint32_t b_rook_files;
    uint32_t w_passed_pawns;
    uint32_t b_passed_pawns;
    uint32_t w_passed_mask;
    uint32_t b_passed_mask;
    uint32_t w_d_pawnmask;
    uint32_t b_d_pawnmask;
    int32_t w_passed_mobility;
    int32_t b_passed_mobility;
    int32_t extra_pawn_val;
} PAWN_INFO;

typedef struct t_piece_info
{
    int32_t white_pieces;
    int32_t black_pieces;
    int32_t all_pieces;
    int32_t all_queens;
    int32_t all_rooks;
    int32_t w_queens;
    int32_t w_rooks;
    int32_t w_bishops;
    int32_t w_knights;
    int32_t b_queens;
    int32_t b_rooks;
    int32_t b_bishops;
    int32_t b_knights;
    int32_t all_minor_pieces;
    int32_t w_bishop_colour;
    int32_t b_bishop_colour;
} PIECE_INFO;
