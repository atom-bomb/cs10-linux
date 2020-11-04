/* cs10.h
 *
 * bits for talking to the cs10 as gleaned from the cs10 manual
 */

#ifndef CS10_H_INCLUDED
#define CS10_H_INCLUDED

#define LED_ON_VALUE             0x7f
#define LED_OFF_VALUE            0x00

#define HEX_TO_SSD_TABLE { 0x3f, /* 0 */ \
                           0x06, /* 1 */ \
                           0x5b, /* 2 */ \
                           0x4f, /* 3 */ \
                           0x66, /* 4 */ \
                           0x6d, /* 5 */ \
                           0x7d, /* 6 */ \
                           0x07, /* 7 */ \
                           0x7f, /* 8 */ \
                           0x6f, /* 9 */ \
                           0x77, /* A */ \
                           0x7c, /* b */ \
                           0x39, /* C */ \
                           0x5e, /* d */ \
                           0x79, /* E */ \
                           0x71, /* F */ \
                           0x00  /*   */ } ;

#define TRACK_TO_LED_ADDR(track) (track)
#define SELECT_LED_ADDR          0x08
#define LOCATE_LED_ADDR          0x09
#define MUTE_LED_ADDR            0x0a
#define SOLO_LED_ADDR            0x0b
#define DOWN_NULL_LED_ADDR       0x0c
#define UP_NULL_LED_ADDR         0x0d
#define LEFT_WHEEL_LED_ADDR      0x0e
#define RIGHT_WHEEL_LED_ADDR     0x0f
#define ONES_SSD_ADDR            0x10
#define TENS_SSD_ADDR            0x11
#define RECORD_LED_ADDR          0x12
#define TENS_DEC_LED_ADDR        0x13
#define ONES_DEC_LED_ADDR        0x14

#define BUTTON_DOWN_VALUE        0x7f
#define BUTTON_UP_VALUE          0x00

#define FIRST_BUTTON_ADDR        0x00
#define LAST_BUTTON_ADDR         0x1E

#define FIRST_TRACK_BUTTON_ADDR  0x00
#define LAST_TRACK_BUTTON_ADDR   0x07
#define BUTTON_ADDR_TO_TRACK(button) (button)

#define MODE_BUTTON_ADDR         0x08
#define SHIFT_BUTTON_ADDR        0x09

#define F1_BUTTON_ADDR           0x0A
#define F2_BUTTON_ADDR           0x0B
#define F3_BUTTON_ADDR           0x0C
#define F4_BUTTON_ADDR           0x0D
#define F5_BUTTON_ADDR           0x0E
#define F6_BUTTON_ADDR           0x0F
#define F7_BUTTON_ADDR           0x10
#define F8_BUTTON_ADDR           0x11
#define F9_BUTTON_ADDR           0x12

#define FIRST_F_BUTTON_ADDR      0x0A
#define LAST_F_BUTTON_ADDR       0x12

#define REW_BUTTON_ADDR          0x13
#define FF_BUTTON_ADDR           0x14
#define STOP_BUTTON_ADDR         0x15
#define PLAY_BUTTON_ADDR         0x16
#define RECORD_BUTTON_ADDR       0x17
#define LEFT_WHEEL_BUTTON_ADDR   0x18
#define RIGHT_WHEEL_BUTTON_ADDR  0x19
#define UP_BUTTON_ADDR           0x1A
#define DOWN_BUTTON_ADDR         0x1B
#define LEFT_BUTTON_ADDR         0x1C
#define RIGHT_BUTTON_ADDR        0x1D
#define FOOTSWITCH_ADDR          0x1E

#define FIRST_FADER_ADDR         0x40
#define LAST_FADER_ADDR          0x47
#define FADER_ADDR_TO_TRACK(fader) (fader - 0x40)

#define BOOST_KNOB_ADDR          0x48
#define FREQ_KNOB_ADDR           0x49
#define BW_KNOB_ADDR             0x4A
#define SEND1_KNOB_ADDR          0x4B
#define SEND2_KNOB_ADDR          0x4C
#define PAN_KNOB_ADDR            0x4D

#define FIRST_KNOB_ADDR          0x48
#define LAST_KNOB_ADDR           0x4D

#define WHEEL_ADDR               0x60

#define LED_SYSEX_PACKET_LENGTH  7
#define LED_SYSEX_PACKET(addr, val) \
  { 0xf0, 0x15, 0x15, 0x00, addr, val, 0xf7 }

#endif /* CS10_H_INCLUDED */
