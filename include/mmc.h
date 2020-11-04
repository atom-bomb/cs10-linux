/* mmc.h
 *
 * here lies all of the mmc sysex crap i've been able to figure out without
 * actually having a midi spec.
 */

#ifndef MMC_H_INCLUDED
#define MMC_H_INCLUDED

#define MMC_COMMAND_STOP        0x01
#define MMC_COMMAND_PLAY        0x02
#define MMC_COMMAND_DEF_PLAY    0x03
#define MMC_COMMAND_FF          0x04
#define MMC_COMMAND_REW         0x05
#define MMC_COMMAND_PUNCH_IN    0x06
#define MMC_COMMAND_PUNCH_OUT   0x07
#define MMC_COMMAND_REC_PAUSE   0x08
#define MMC_COMMAND_PAUSE       0x09
#define MMC_COMMAND_EJECT       0x0A
#define MMC_COMMAND_CHASE       0x0B
#define MMC_COMMAND_ERR_RESET   0x0C
#define MMC_COMMAND_MMC_RESET   0x0D

#define MMC_DEVICEID_ALL        0x7f

#define MMC_CMD_SYSEX_PACKET_LENGTH  6
#define MMC_CMD_SYSEX_PACKET(deviceid, cmd) \
  { 0xf0, 0x7f, deviceid, 0x06, cmd, 0xf7 }

#define MMC_GOTO_SYSEX_PACKET_LENGTH 13
#define MMC_GOTO_SYSEX_PACKET(deviceid, hour, minute, second, frame, subframe) \
  { 0xf0, 0x7f, deviceid, 0x06, 0x44, 0x06, 0x01, \
    hour, minute, second, frame, subframe, 0xf7 }

#define MMC_LOC_SYSEX_PACKET_LENGTH 9
#define MMC_LOC_SYSEX_PACKET(deviceid, loc) \
  { 0xf0, 0x7f, deviceid, 0x06, 0x44, 0x02, 0x00, (0x08 + loc), 0xf7 }

/* 8 potential tracks mask1 = 60, mask2 = 3f */
#define MMC_TRACK_ENABLE_SYSEX_PACKET_LENGTH 11 
#define MMC_TRACK_ENABLE_SYSEX_PACKET(deviceid, mask1, mask2) \
  { 0xf0, 0x7f, deviceid, 0x06, 0x40, 0x04, 0x4f, 0x02, \
    mask1, mask2, 0xf7 }

/* speed = 00 10 00 -> 07 00 00, 01 00 00 = normal speed */
#define MMC_SHUTTLE_SYSEX_PACKET_LENGTH 10
#define MMC_SHUTTLE_SYSEX_PACKET(deviceid, speed1, speed2, speed3) \
  { 0xf0, 0x7f, deviceid, 0x06, 0x47, 0x03, speed1, speed2, speed3, 0xf7 }

#define MMC_STEP_SYSEX_PACKET_LENGTH 8
#define MMC_STEP_SYSEX_PACKET(deviceid, steps) \
  { 0xf0, 0x7f, deviceid, 0x06, 0x48, 0x01, \
    steps, 0xf7 }

#endif /* MMC_H_INCLUDED */
