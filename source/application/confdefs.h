/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 *  Copyright (C) 2015-2020, Rasmus Althoff <althoff@ct800.net>
 *
 *  This file is part of the CT800 (configuration and bitmasks).
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

 /*configuration parameter version.
 if the config changes in a way that is not downwards compatible,
 this parameter has to be incremented. after the update, the software
 will conclude that the device config is invalid, and it will switch
 to the default config. a possible backup game will be deleted because
 it contains the same unusable config.*/
#define CONF_VERSION          4ULL
#define CONF_VERSION_OFFSET   56U

/*
binary key:
36   32   28   24   20   16   12    8    4    0
 W WEEE EDDD DLLU UCCC XXXT TIII PFFM MMGG GSSB

B book: 0 = book off, 1 = book on (default)
S side: 0 = none, 1 = always white, 2 = always black (default), 3 = random
G game mode: 0 = time per move (default), 1 = game in, 2 = tournament, 3 = mate solver, 4 = analysis
M move in (in time-per-move-mode, only relevant for the computer)
    000 = 1s
    001 = 5s
    010 = 10s (default)
    011 = 15s
    100 = 30s
    101 = 60s
    110 = 120s
    111 = 180s
F = Fischer-Modus for the following time control modes.
    00 = off (default)
    01 = 10s
    10 = 20s
    11 = 30s
P = The player always gets 10s extra per move for entering the moves, 1 = on (default), 0 = off
NOTE: For the player, P and F add up.
I game in (in game-in-time mode, refers to the thinking time EACH player. P and F can apply.)
    000 = 10 min
    001 = 15 min
    010 = 20 min
    011 = 30 min (default)
    100 = 45 min
    101 = 60 min
    110 = 90 min
    111 = 120 min
T tournament levels (refers to the thinking time of EACH player. P and F can apply.)
    00 = 40 moves in 90 min, rest 30 min
    01 = 40 moves in 2h, rest 30 min
    10 = 40 moves in 2h, rest 1h
    11 = 40 moves in 2h, next 20 moves in 1h, rest 1h (classic tournament - default)
X problem mate solver, mate in XXX-1 moves:
    000 mate in 1
    001 mate in 2
    ...
    111 mate in 8
C clock management
    0 = underclock to 10% ( 18 MHz)
    1 = underclock to 25% ( 42 MHz)
    2 = underclock to 50% ( 84 MHz)
    3 = underclock to 70% (120 MHz)
    4 = regular      100% (168 MHz, default)
    5 = overclock to 130% (216 MHz)
    6 = overclock to 145% (240 MHz)
U user time factor:
    00 = same as computer (default)
    01 = double time
    10 = threefold time
    11 = fourfold time
L loudspeaker
    00 = off
    01 = keyboard click only
    10 = computer only
    11 = on (default)
D display contrast in 10% steps
    0 = 0% (minimum)
    5dec= 50% (default)
    10dec = 100% (maximum)
    the values 11dec to 15dec are not used.
E evaluation noise
    00 = off (default)
    01 = slight
    10 = medium
    11 = strong
W backlight active
    00 = off
    01 = on
    10 = auto (default)


NOTE: the text string definitons are NOT used in the HMI menu section;
they are included just for documentation purposes.*/

#define CFG_BOOK_MODE         (1ULL)
#define CFG_BOOK_OFF          (0ULL)
#define CFG_BOOK_ON           (1ULL)
#define CFG_BOOK_MODE_TXT     "use book:"
#define CFG_BOOK_OFF_TXT      "off"
#define CFG_BOOK_ON_TXT       "on "

#define CFG_COMP_SIDE_OFFSET    1U
#define CFG_COMP_SIDE_MODE      (3ULL << CFG_COMP_SIDE_OFFSET)
#define CFG_COMP_SIDE_NONE      (0ULL << CFG_COMP_SIDE_OFFSET)
#define CFG_COMP_SIDE_WHITE     (1ULL << CFG_COMP_SIDE_OFFSET)
#define CFG_COMP_SIDE_BLACK     (2ULL << CFG_COMP_SIDE_OFFSET)
#define CFG_COMP_SIDE_RND       (3ULL << CFG_COMP_SIDE_OFFSET)
#define CFG_COMP_SIDE_MODE_TXT  "computer side:"
#define CFG_COMP_SIDE_NONE_TXT  "none  "
#define CFG_COMP_SIDE_WHITE_TXT "white "
#define CFG_COMP_SIDE_BLACK_TXT "black "
#define CFG_COMP_SIDE_RND_TXT   "random"

#define CFG_GAME_OFFSET       3U
#define CFG_GAME_MODE         (7ULL << CFG_GAME_OFFSET)
#define CFG_GAME_MODE_TPM     (0ULL << CFG_GAME_OFFSET)
#define CFG_GAME_MODE_GMI     (1ULL << CFG_GAME_OFFSET)
#define CFG_GAME_MODE_TRN     (2ULL << CFG_GAME_OFFSET)
#define CFG_GAME_MODE_ANA     (3ULL << CFG_GAME_OFFSET)
#define CFG_GAME_MODE_MTI     (4ULL << CFG_GAME_OFFSET)
#define CFG_GAME_MODE_TXT     "time control:"
#define CFG_GAME_MODE_TPM_TXT "time per move"
#define CFG_GAME_MODE_GMI_TXT "game in"
#define CFG_GAME_MODE_TRN_TXT "tournament"
#define CFG_GAME_MODE_ANA_TXT "analysis"
#define CFG_GAME_MODE_MTI_TXT "mate in"

#define CFG_TPM_OFFSET    6U
#define CFG_TPM_MODE      (7ULL << CFG_TPM_OFFSET)
#define CFG_TPM_LV0       (0ULL << CFG_TPM_OFFSET)
#define CFG_TPM_LV1       (1ULL << CFG_TPM_OFFSET)
#define CFG_TPM_LV2       (2ULL << CFG_TPM_OFFSET)
#define CFG_TPM_LV3       (3ULL << CFG_TPM_OFFSET)
#define CFG_TPM_LV4       (4ULL << CFG_TPM_OFFSET)
#define CFG_TPM_LV5       (5ULL << CFG_TPM_OFFSET)
#define CFG_TPM_LV6       (6ULL << CFG_TPM_OFFSET)
#define CFG_TPM_LV7       (7ULL << CFG_TPM_OFFSET)
#define CFG_TPM_MODE_TXT  "time per move:"
#define CFG_TPM_LV0_TXT   "  1s"
#define CFG_TPM_LV1_TXT   "  5s"
#define CFG_TPM_LV2_TXT   " 10s"
#define CFG_TPM_LV3_TXT   " 20s"
#define CFG_TPM_LV4_TXT   " 30s"
#define CFG_TPM_LV5_TXT   " 60s"
#define CFG_TPM_LV6_TXT   "120s"
#define CFG_TPM_LV7_TXT   "180s"
#define CFG_TPM_LV0_TIME  1L
#define CFG_TPM_LV1_TIME  5L
#define CFG_TPM_LV2_TIME  10L
#define CFG_TPM_LV3_TIME  20L
#define CFG_TPM_LV4_TIME  30L
#define CFG_TPM_LV5_TIME  60L
#define CFG_TPM_LV6_TIME  120L
#define CFG_TPM_LV7_TIME  180L

#define CFG_FISCHER_OFFSET    9U
#define CFG_FISCHER_MODE      (3ULL << CFG_FISCHER_OFFSET)
#define CFG_FISCHER_OFF       (0ULL << CFG_FISCHER_OFFSET)
#define CFG_FISCHER_LV0       (1ULL << CFG_FISCHER_OFFSET)
#define CFG_FISCHER_LV1       (2ULL << CFG_FISCHER_OFFSET)
#define CFG_FISCHER_LV2       (3ULL << CFG_FISCHER_OFFSET)
#define CFG_FISCHER_MODE_TXT  "Fischer delay:"
#define CFG_FISCHER_OFF_TXT   "off"
#define CFG_FISCHER_LV0_TXT   "10s"
#define CFG_FISCHER_LV1_TXT   "20s"
#define CFG_FISCHER_LV2_TXT   "30s"
#define CFG_FISCHER_LV0_TIME  10L
#define CFG_FISCHER_LV1_TIME  20L
#define CFG_FISCHER_LV2_TIME  30L

#define CFG_PLAYER_BONUS_OFFSET   11U
#define CFG_PLAYER_BONUS_MODE     (1ULL << CFG_PLAYER_BONUS_OFFSET)
#define CFG_PLAYER_BONUS_OFF      (0ULL << CFG_PLAYER_BONUS_OFFSET)
#define CFG_PLAYER_BONUS_ON       (1ULL << CFG_PLAYER_BONUS_OFFSET)
#define CFG_PLAYER_BONUS_MODE_TXT "player bonus:"
#define CFG_PLAYER_BONUS_OFF_TXT  "off"
#define CFG_PLAYER_BONUS_ON_TXT   "10s"
#define CFG_PLAYER_BONUS_TIME     10L

#define CFG_GAME_IN_OFFSET    12U
#define CFG_GAME_IN_MODE      (7ULL << CFG_GAME_IN_OFFSET)
#define CFG_GAME_IN_LV0       (0ULL << CFG_GAME_IN_OFFSET)
#define CFG_GAME_IN_LV1       (1ULL << CFG_GAME_IN_OFFSET)
#define CFG_GAME_IN_LV2       (2ULL << CFG_GAME_IN_OFFSET)
#define CFG_GAME_IN_LV3       (3ULL << CFG_GAME_IN_OFFSET)
#define CFG_GAME_IN_LV4       (4ULL << CFG_GAME_IN_OFFSET)
#define CFG_GAME_IN_LV5       (5ULL << CFG_GAME_IN_OFFSET)
#define CFG_GAME_IN_LV6       (6ULL << CFG_GAME_IN_OFFSET)
#define CFG_GAME_IN_LV7       (7ULL << CFG_GAME_IN_OFFSET)
#define CFG_GAME_IN_MODE_TXT  "game in:"
#define CFG_GAME_IN_LV0_TXT   "  5min"
#define CFG_GAME_IN_LV1_TXT   " 10min"
#define CFG_GAME_IN_LV2_TXT   " 15min"
#define CFG_GAME_IN_LV3_TXT   " 20min"
#define CFG_GAME_IN_LV4_TXT   " 30min"
#define CFG_GAME_IN_LV5_TXT   " 45min"
#define CFG_GAME_IN_LV6_TXT   " 60min"
#define CFG_GAME_IN_LV7_TXT   " 90min"
#define CFG_GAME_IN_LV0_TIME   300L
#define CFG_GAME_IN_LV1_TIME   600L
#define CFG_GAME_IN_LV2_TIME   900L
#define CFG_GAME_IN_LV3_TIME  1200L
#define CFG_GAME_IN_LV4_TIME  1800L
#define CFG_GAME_IN_LV5_TIME  2700L
#define CFG_GAME_IN_LV6_TIME  3600L
#define CFG_GAME_IN_LV7_TIME  5400L

#define CFG_TRN_OFFSET        15U
#define CFG_TRN_MODE          (3ULL << CFG_TRN_OFFSET)
#define CFG_TRN_LV0           (0ULL << CFG_TRN_OFFSET)
#define CFG_TRN_LV1           (1ULL << CFG_TRN_OFFSET)
#define CFG_TRN_LV2           (2ULL << CFG_TRN_OFFSET)
#define CFG_TRN_LV3           (3ULL << CFG_TRN_OFFSET)
#define CFG_TRN_MODE_TXT      "tournament:"
#define CFG_TRN_LV0_TXT       "40/ 90+30"
#define CFG_TRN_LV1_TXT       "40/120+30"
#define CFG_TRN_LV2_TXT       "40/120+60"
#define CFG_TRN_LV3_TXT       "40/120,20/60+30"

#define CFG_TRN_LV0_M40       5400L
#define CFG_TRN_LV0_REST      1800L

#define CFG_TRN_LV1_M40       7200L
#define CFG_TRN_LV1_REST      1800L

#define CFG_TRN_LV2_M40       7200L
#define CFG_TRN_LV2_REST      3600L

#define CFG_TRN_LV3_M40       7200L
#define CFG_TRN_LV3_M60       3600L
#define CFG_TRN_LV3_REST      1800L

#define CFG_MTI_OFFSET        17U
#define CFG_MTI_MODE          (7ULL << CFG_MTI_OFFSET)
#define CFG_MTI_DEFAULT       (3ULL << CFG_MTI_OFFSET)
#define CFG_MTI_MAX           7
/*maximum 96 hours = 4 days for mate searching*/
#define CFG_MTI_TIME          (3600L * 96L)

/*maximum 9 hours for analysis mode*/
#define CFG_ANA_TIME          (3600L * 9L)

/*the frequency settings must be ordered from high to low because the
overclocking check is done with > CFG_CLOCK_100*/
#define CFG_CLOCK_OFFSET      20U
#define CFG_CLOCK_MODE        (7ULL << CFG_CLOCK_OFFSET)
#define CFG_CLOCK_145         (6ULL << CFG_CLOCK_OFFSET)
#define CFG_CLOCK_130         (5ULL << CFG_CLOCK_OFFSET)
#define CFG_CLOCK_100         (4ULL << CFG_CLOCK_OFFSET)
#define CFG_CLOCK_070         (3ULL << CFG_CLOCK_OFFSET)
#define CFG_CLOCK_050         (2ULL << CFG_CLOCK_OFFSET)
#define CFG_CLOCK_025         (1ULL << CFG_CLOCK_OFFSET)
#define CFG_CLOCK_010         (0ULL << CFG_CLOCK_OFFSET)
#define CFG_CLOCK_MAX         CFG_CLOCK_145
#define CFG_CLOCK_STEP        (1ULL << CFG_CLOCK_OFFSET)
#define CFG_CLOCK_MIN         CFG_CLOCK_010

#define CFG_USER_TIME_OFFET   23U
#define CFG_USER_TIME_MODE    (3ULL << CFG_USER_TIME_OFFET)
#define CFG_USER_TIME_LV0     (0ULL << CFG_USER_TIME_OFFET)
#define CFG_USER_TIME_LV1     (1ULL << CFG_USER_TIME_OFFET)
#define CFG_USER_TIME_LV2     (2ULL << CFG_USER_TIME_OFFET)
#define CFG_USER_TIME_LV3     (3ULL << CFG_USER_TIME_OFFET)

#define CFG_SPEAKER_OFFSET    25U
#define CFG_SPEAKER_MODE      (3ULL << CFG_SPEAKER_OFFSET)
#define CFG_SPEAKER_OFF       (0ULL << CFG_SPEAKER_OFFSET)
#define CFG_SPEAKER_CLICK     (1ULL << CFG_SPEAKER_OFFSET)
#define CFG_SPEAKER_COMP      (2ULL << CFG_SPEAKER_OFFSET)
#define CFG_SPEAKER_ON        (3ULL << CFG_SPEAKER_OFFSET)

#define CFG_DISP_OFFSET       27U
#define CFG_DISP_MODE         (15ULL << CFG_DISP_OFFSET)
#define CFG_DISP_DEFAULT      (5ULL << CFG_DISP_OFFSET)

#define CFG_NOISE_OFFSET      31U
#define CFG_NOISE_MODE        (15ULL << CFG_NOISE_OFFSET)
#define CFG_NOISE_OFF         (0ULL << CFG_NOISE_OFFSET)

#define CFG_LIGHT_OFFSET      35U
#define CFG_LIGHT_MODE        (3ULL << CFG_LIGHT_OFFSET)
#define CFG_LIGHT_OFF         (0ULL << CFG_LIGHT_OFFSET)
#define CFG_LIGHT_AUTO        (1ULL << CFG_LIGHT_OFFSET)
#define CFG_LIGHT_ON          (2ULL << CFG_LIGHT_OFFSET)

#define CFG_TIME_RELATED      (CFG_GAME_MODE | CFG_FISCHER_MODE | CFG_PLAYER_BONUS_MODE | CFG_GAME_IN_MODE | CFG_TRN_MODE | CFG_USER_TIME_MODE)

#define CFG_DEFAULT (CFG_BOOK_ON | CFG_COMP_SIDE_BLACK | CFG_GAME_MODE_TPM | CFG_TPM_LV2 | CFG_FISCHER_OFF | CFG_PLAYER_BONUS_ON | CFG_GAME_IN_LV4 | CFG_TRN_LV3 | CFG_MTI_DEFAULT | CFG_USER_TIME_LV0 | CFG_SPEAKER_ON | CFG_CLOCK_100 | CFG_DISP_DEFAULT | CFG_NOISE_OFF | CFG_LIGHT_AUTO | (CONF_VERSION << CONF_VERSION_OFFSET))

/* CFG_SET_OPT:
- first clears all bits for the thing that is to be configured.
- second sets the bits as to be configured.
*/
#define CFG_SET_OPT(_MODEMASK, _OPTVAL)  do {hw_config &= (~(_MODEMASK)); hw_config |= (_OPTVAL);} while (0)

/* CFG_GET_OPT gets the relevant configuration bits.*/
#define CFG_GET_OPT(_MODEMASK)           (hw_config & (_MODEMASK))

/* CFG_HAS_OPT tells whether an option is set.*/
#define CFG_HAS_OPT(_MODEMASK, _OPTVAL)  ((hw_config & (_MODEMASK)) == (_OPTVAL))

/* Default explained:
 use book
 display light in auto mode
 beeper on (click and computer sound)
 black side for new games - because game-in and tournament time settings work only from the starting position
 time mode is time per move
 soft time per move control (only in time-per-move mode)
 10s per move
 no Fischer clock for following modes (will become relevant if the user switches the time setting to game-in or tournament)
 10s player bonus always (will become relevant if the user switches the time setting to game-in or tournament)
 30 minutes per game (will become relevant if the user switches the time setting to game-in)
 classic tournament mode (will become relevant if the user switches the time setting to tournament).
 mate search depth 4 (only relevant in mate search mode)*/
