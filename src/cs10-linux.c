/*****************************************************************************/

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <alsa/asoundlib.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>

#include "mmc.h"
#include "cs10.h"

/*****************************************************************************/

#define CS10_DEFAULT_SETTINGS_FILENAME "cs10-linux.dat"
#define CS10_DEFAULT_SETTINGS_PATH     "/.local/share/cs10/"
#define CS10_DEFAULT_SETTINGS_DIR      "/cs10/"

#define CS10_SEQUENCER_NAME    "default"
#define CS10_CLIENT_NAME       "cs10"
#define CS10_CONTROL_PORT_NAME "cs10-io"
#define CS10_MMC_IO_PORT_NAME  "mmc-io"

#define CS10_MIDI_CONTROL_CHANNEL 0

#define CS10_NUM_BANKS           4
#define CS10_NUM_PHYSICAL_TRACKS (LAST_FADER_ADDR - FIRST_FADER_ADDR + 1) 
#define CS10_NUM_VIRTUAL_TRACKS  (CS10_NUM_PHYSICAL_TRACKS * CS10_NUM_BANKS) 
#define CS10_NUM_KNOBS           (LAST_KNOB_ADDR - FIRST_KNOB_ADDR + 1)
#define CS10_NUM_F_BUTTONS       (LAST_F_BUTTON_ADDR - FIRST_F_BUTTON_ADDR + 1)

#define CS10_NUM_SAVED_STATES    CS10_NUM_F_BUTTONS
#define CS10_NUM_SAVED_POSITIONS CS10_NUM_F_BUTTONS

#define CS10_JOG_INTERVAL   500
#define CS10_JOG_THRESHOLD  4
#define CS10_JOG_DIVISOR    2

#define CS10_FADER_RESTORE_DELAY_US 5000


 static const unsigned int uiHexToSSDTable[] = HEX_TO_SSD_TABLE ; 

/*****************************************************************************/

typedef struct SMPTE_TIME_S {
  unsigned char flags ;
  unsigned char hours ;
  unsigned char minutes ;
  unsigned char seconds ;
  unsigned char frames ;
} smpte_time_t ;

typedef enum VIRTUAL_TRACK_CONTROL_E {
  ARMED_CONTROL,
  MUTE_CONTROL,
  SOLO_CONTROL,
  FADER_CONTROL,
  BOOST_CUT_CONTROL,
  FREQUENCY_CONTROL,
  BANDWDITH_CONTROL,
  SEND_ONE_CONTROL,
  SEND_TWO_CONTROL,
  PAN_CONTROL,
  NUM_VIRTUAL_TRACK_CONTROLS
} virtual_track_control_t ;

#define VIRTUAL_CONTROL_TO_KNOB_INDEX(control) \
   (control - BOOST_CUT_CONTROL)

#define KNOB_ADDR_TO_KNOB_INDEX(addr) \
   (addr - FIRST_KNOB_ADDR)

#define KNOB_ADDR_TO_VIRTUAL_CONTROL(addr) \
   (addr - FIRST_KNOB_ADDR + BOOST_CUT_CONTROL)

typedef enum MODE_E {
  SELECT_MODE,
  LOC_MODE,
  MUTE_MODE,
  SOLO_MODE,
  NULLIFY_MODE,
  NUM_MODES
} control_mode_t ;

typedef enum DISPLAY_MODE_E {
  SMPTE_DISPLAY_MODE,
  BANK_DISPLAY_MODE,
  NUM_DISPLAY_MODES
} display_mode_t ;

typedef enum SMPTE_DISPLAY_MODE_E  {
  SMPTE_DISPLAY_HOURS,
  SMPTE_DISPLAY_MINUTES,
  SMPTE_DISPLAY_SECONDS,
  SMPTE_DISPLAY_FRAMES,
  NUM_SMPTE_DISPLAY_MODES
} smpte_display_mode_t;

typedef struct CS10_TRACK_STATE_S {
  bool bArmed ;
  bool bMute ;
  bool bSolo ;
  unsigned int uiFader ;
  unsigned int uiKnob[CS10_NUM_KNOBS];
} cs10_track_state_t ;

typedef struct {
  cs10_track_state_t tsTrack[CS10_NUM_VIRTUAL_TRACKS] ;
} cs10_mixer_state_t ;

/*****************************************************************************/

struct CS10_S {
  bool            debug;

  char           *settings_filename;

  snd_seq_t      *pSeq ;

  int             hw_seq_client;
  int             hw_seq_port;

  int             iClientID ;
  int             iControlPortID ;
  int             iMMCPortID ;
  __sighandler_t  pOldSigHandler ;

  cs10_mixer_state_t csState ;

  smpte_time_t    tCurrentTime ;

  smpte_time_t    tQFTime ;
  unsigned char   ucQuarterFrameFlags ;

  smpte_time_t    tPlayFromTime ;
  smpte_time_t    tRecordFromTime ;

  cs10_mixer_state_t csSavedState[CS10_NUM_SAVED_STATES] ;
  smpte_time_t    tSavedPosition[CS10_NUM_SAVED_POSITIONS] ;

  display_mode_t  displayMode;
  smpte_display_mode_t smpteDisplayMode;
  unsigned char   display_ones;
  unsigned char   display_tens;

  unsigned int    uiBank ;
  control_mode_t  theMode ;
  unsigned int    uiSelectedTrack ;

  bool            bRecordKeyDown ;
  bool            bShiftKeyDown ;
  bool            bIgnoreRecordKeyUp ;

  bool            bJogging ;
  volatile int    iJogCount ;
} cs10 ;

/*****************************************************************************/


void
sighandler(
  int iSignal) {

  signal(SIGTERM, cs10.pOldSigHandler) ;
  exit(0) ;
} /* sighandler */

/*
 * cs10_fini
 *
 * cleanup system resources
 */
void
cs10_fini(void) {

  if (0 <= cs10.iControlPortID)
    snd_seq_delete_simple_port(cs10.pSeq, cs10.iControlPortID) ;

  if (0 <= cs10.iMMCPortID)
    snd_seq_delete_simple_port(cs10.pSeq, cs10.iMMCPortID) ;

  snd_seq_close(cs10.pSeq) ;
} /* cs10_fini */

/*
 * cs10_init
 *
 * allocate any system resources needed to run
 */
bool
cs10_init(void) {

  bool bRetValue = false ;

  if (0 == snd_seq_open(&cs10.pSeq, CS10_SEQUENCER_NAME,
                        SND_SEQ_OPEN_DUPLEX, 0)) {
    cs10.iClientID = snd_seq_client_id(cs10.pSeq) ;
    snd_seq_set_client_name(cs10.pSeq, CS10_CLIENT_NAME) ;

    cs10.iControlPortID = snd_seq_create_simple_port(cs10.pSeq,
        CS10_CONTROL_PORT_NAME,
        SND_SEQ_PORT_CAP_WRITE |
        SND_SEQ_PORT_CAP_READ |
        SND_SEQ_PORT_CAP_SUBS_WRITE |
        SND_SEQ_PORT_CAP_SUBS_READ,
        SND_SEQ_PORT_TYPE_MIDI_GENERIC |
        SND_SEQ_PORT_TYPE_APPLICATION) ;

    cs10.iMMCPortID = snd_seq_create_simple_port(cs10.pSeq,
        CS10_MMC_IO_PORT_NAME,
        SND_SEQ_PORT_CAP_READ |
        SND_SEQ_PORT_CAP_WRITE |
        SND_SEQ_PORT_CAP_SUBS_READ |
        SND_SEQ_PORT_CAP_SUBS_WRITE,
        SND_SEQ_PORT_TYPE_MIDI_GENERIC |
        SND_SEQ_PORT_TYPE_APPLICATION) ;

    bRetValue = true ;

    atexit(cs10_fini) ;

    cs10.pOldSigHandler = signal(SIGTERM, sighandler) ;
  } /* if open */

  return bRetValue ;
} /* cs10_init */

/*
 * cs10_save_settings
 *
 * save all of the settings to a file
 */
void
cs10_save_settings() {
  FILE *fp = NULL;

  if (cs10.settings_filename == NULL)
    return;

  fp = fopen(cs10.settings_filename, "w+");

  if (NULL != fp) {
    fwrite(cs10.csSavedState,
      sizeof(cs10_mixer_state_t), CS10_NUM_SAVED_STATES, fp);
    fwrite(cs10.tSavedPosition,
      sizeof(smpte_time_t), CS10_NUM_SAVED_POSITIONS, fp);
    fclose(fp);
  } /* if */
} /* cs10_save_settings */

/*
 * cs10_load_settings
 *
 * restore all of the settings from a file
 */
void
cs10_load_settings() {
  FILE *fp = NULL;

  if (cs10.settings_filename == NULL)
    return;

  fp = fopen(cs10.settings_filename, "r");

  if (NULL != fp) {
    fread(cs10.csSavedState,
      sizeof(cs10_mixer_state_t), CS10_NUM_SAVED_STATES, fp);
    fread(cs10.tSavedPosition,
      sizeof(smpte_time_t), CS10_NUM_SAVED_POSITIONS, fp);
    fclose(fp);
  } /* if */
} /* cs10_load_settings */

/*
 * cs10_set_led
 *
 * set CS10 LED status of uiAddr to uiValue
 */
bool
cs10_set_led(
  unsigned int uiAddr,
  unsigned int uiValue) {

  bool             bRetValue ;
  snd_seq_event_t  theEvent ;
  unsigned char    ucCommand[LED_SYSEX_PACKET_LENGTH] =
     LED_SYSEX_PACKET(uiAddr, uiValue) ;

  snd_seq_ev_clear(&theEvent) ;
  snd_seq_ev_set_dest(&theEvent, SND_SEQ_ADDRESS_SUBSCRIBERS, 0) ;
  snd_seq_ev_set_source(&theEvent, cs10.iControlPortID) ;
  snd_seq_ev_set_direct(&theEvent) ;

  snd_seq_ev_set_sysex(&theEvent, LED_SYSEX_PACKET_LENGTH, ucCommand) ;

  snd_seq_event_output(cs10.pSeq, &theEvent) ;
  snd_seq_drain_output(cs10.pSeq) ;

  return bRetValue ;
} /* cs10_set_led */

/*
 * cs10_display_number_dec
 *
 * display the given number on the cs10 seven segment display in base 10
 */
bool
cs10_display_number_dec(
  unsigned int uiNumber) {

  bool         bRetValue = true ;

  if (bRetValue = cs10_set_led(ONES_SSD_ADDR, uiHexToSSDTable[uiNumber % 10]))
   bRetValue = cs10_set_led(TENS_SSD_ADDR, uiHexToSSDTable[uiNumber / 10]) ;

  return bRetValue ;
} /* cs10_display_number_dec */

/*
 * cs10_display_bank
 *
 * display the bank number on the cs10 seven segment display
 */
void
cs10_display_bank() {

  cs10_set_led(ONES_SSD_ADDR, uiHexToSSDTable[
     cs10.uiBank % 10]);
  cs10_set_led(TENS_SSD_ADDR, 0) ;
} /* cs10_display_bank */

/*
 * cs10_display_time
 *
 * display part of the smpte time on the cs10 seven segment display
 */
void
cs10_display_time() {

  unsigned char data = 0;

  switch (cs10.smpteDisplayMode) {
    case SMPTE_DISPLAY_HOURS:
      data = cs10.tCurrentTime.hours;

      cs10_set_led(TENS_DEC_LED_ADDR, LED_ON_VALUE);
      cs10_set_led(ONES_DEC_LED_ADDR, LED_ON_VALUE);
      break;

    case SMPTE_DISPLAY_MINUTES:
      data = cs10.tCurrentTime.minutes;

      cs10_set_led(TENS_DEC_LED_ADDR, LED_ON_VALUE);
      cs10_set_led(ONES_DEC_LED_ADDR, LED_OFF_VALUE);
      break;

    case SMPTE_DISPLAY_SECONDS:
      data = cs10.tCurrentTime.seconds;

      cs10_set_led(TENS_DEC_LED_ADDR, LED_OFF_VALUE);
      cs10_set_led(ONES_DEC_LED_ADDR, LED_ON_VALUE);
      break;

    case SMPTE_DISPLAY_FRAMES:
      data = cs10.tCurrentTime.frames;

      cs10_set_led(TENS_DEC_LED_ADDR, LED_OFF_VALUE);
      cs10_set_led(ONES_DEC_LED_ADDR, LED_OFF_VALUE);
      break;

    default:
      break;
  } /* switch */

  cs10_set_led(ONES_SSD_ADDR, uiHexToSSDTable[data % 10]);
  cs10_set_led(TENS_SSD_ADDR, uiHexToSSDTable[data / 10]);

  cs10.display_ones = data % 10;
  cs10.display_tens = data / 10;
} /* cs10_display_time */

void
cs10_update_display_time() {
  unsigned char data = 0;

  if (cs10.displayMode == SMPTE_DISPLAY_MODE) {
    switch (cs10.smpteDisplayMode) {
      case SMPTE_DISPLAY_HOURS:
        data = cs10.tCurrentTime.hours;
        break;

      case SMPTE_DISPLAY_MINUTES:
        data = cs10.tCurrentTime.minutes;
        break;

      case SMPTE_DISPLAY_SECONDS:
        data = cs10.tCurrentTime.seconds;
        break;

      case SMPTE_DISPLAY_FRAMES:
        data = cs10.tCurrentTime.frames;
        break;

      default:
        break;
    } /* switch */

    unsigned char new_ones = data % 10;
    unsigned char new_tens = data / 10;

    if (new_ones != cs10.display_ones) {
      cs10_set_led(ONES_SSD_ADDR, uiHexToSSDTable[new_ones]);
      cs10.display_ones = new_ones;
    } /* if */

    if (new_tens != cs10.display_tens) {
      cs10_set_led(TENS_SSD_ADDR, uiHexToSSDTable[new_tens]);
      cs10.display_tens = new_tens;
    } /* if */
  } /* if */
} /* cs10_update_display_time */

/*
 * cs10_set_mode
 *
 * set mode LED and track LEDs to reflect theMode
 */
void
cs10_set_mode(
  control_mode_t theMode) {

  unsigned int uiTrack ;

  cs10_set_led(SELECT_LED_ADDR, LED_OFF_VALUE) ;
  cs10_set_led(LOCATE_LED_ADDR, LED_OFF_VALUE) ;
  cs10_set_led(MUTE_LED_ADDR, LED_OFF_VALUE) ;
  cs10_set_led(SOLO_LED_ADDR, LED_OFF_VALUE) ;

  switch (theMode) {
    case SELECT_MODE:
      cs10_set_led(SELECT_LED_ADDR, LED_ON_VALUE) ;

      cs10_set_led(DOWN_NULL_LED_ADDR, LED_OFF_VALUE);
      cs10_set_led(UP_NULL_LED_ADDR, LED_OFF_VALUE);
      cs10_set_led(LEFT_WHEEL_LED_ADDR, LED_OFF_VALUE);
      cs10_set_led(RIGHT_WHEEL_LED_ADDR, LED_OFF_VALUE);

      for (uiTrack = 0 ;
           uiTrack < CS10_NUM_PHYSICAL_TRACKS ;
           uiTrack++) {
        cs10_set_led(TRACK_TO_LED_ADDR(uiTrack), LED_OFF_VALUE) ;
      } /* for */

      cs10_set_led(TRACK_TO_LED_ADDR(cs10.uiSelectedTrack),
          LED_ON_VALUE) ;
      break ;

    case LOC_MODE:
      cs10_set_led(LOCATE_LED_ADDR, LED_ON_VALUE) ;

      cs10_set_led(DOWN_NULL_LED_ADDR, LED_OFF_VALUE);
      cs10_set_led(UP_NULL_LED_ADDR, LED_OFF_VALUE);
      cs10_set_led(LEFT_WHEEL_LED_ADDR, LED_OFF_VALUE);
      cs10_set_led(RIGHT_WHEEL_LED_ADDR, LED_OFF_VALUE);

      for (uiTrack = 0 ;
           uiTrack < CS10_NUM_PHYSICAL_TRACKS ;
           uiTrack++) {
        cs10_set_led(TRACK_TO_LED_ADDR(uiTrack),
            (cs10.csState.tsTrack[
               cs10.uiBank * CS10_NUM_PHYSICAL_TRACKS +
               uiTrack].bArmed ?
             LED_ON_VALUE : LED_OFF_VALUE)) ;
      } /* for */
      break ;

    case MUTE_MODE:
      cs10_set_led(MUTE_LED_ADDR, LED_ON_VALUE) ;

      cs10_set_led(DOWN_NULL_LED_ADDR, LED_OFF_VALUE);
      cs10_set_led(UP_NULL_LED_ADDR, LED_OFF_VALUE);
      cs10_set_led(LEFT_WHEEL_LED_ADDR, LED_OFF_VALUE);
      cs10_set_led(RIGHT_WHEEL_LED_ADDR, LED_OFF_VALUE);

      for (uiTrack = 0 ;
           uiTrack < CS10_NUM_PHYSICAL_TRACKS ;
           uiTrack++) {
        cs10_set_led(TRACK_TO_LED_ADDR(uiTrack),
            (cs10.csState.tsTrack[
               cs10.uiBank * CS10_NUM_PHYSICAL_TRACKS +
               uiTrack].bMute ?
             LED_ON_VALUE : LED_OFF_VALUE)) ;
      } /* for */
      break ;

    case SOLO_MODE:
      cs10_set_led(SOLO_LED_ADDR, LED_ON_VALUE) ;

      cs10_set_led(DOWN_NULL_LED_ADDR, LED_OFF_VALUE);
      cs10_set_led(UP_NULL_LED_ADDR, LED_OFF_VALUE);
      cs10_set_led(LEFT_WHEEL_LED_ADDR, LED_OFF_VALUE);
      cs10_set_led(RIGHT_WHEEL_LED_ADDR, LED_OFF_VALUE);

      for (uiTrack = 0 ;
           uiTrack < CS10_NUM_PHYSICAL_TRACKS ;
           uiTrack++) {
        cs10_set_led(TRACK_TO_LED_ADDR(uiTrack),
            (cs10.csState.tsTrack[
               cs10.uiBank * CS10_NUM_PHYSICAL_TRACKS +
               uiTrack].bSolo ?
             LED_ON_VALUE : LED_OFF_VALUE)) ;
      } /* for */
      break ;

    case NULLIFY_MODE:
      cs10_set_led(SELECT_LED_ADDR, LED_ON_VALUE) ;
      cs10_set_led(LOCATE_LED_ADDR, LED_ON_VALUE) ;
      cs10_set_led(MUTE_LED_ADDR, LED_ON_VALUE) ;
      cs10_set_led(SOLO_LED_ADDR, LED_ON_VALUE) ;

      cs10_set_led(DOWN_NULL_LED_ADDR, LED_ON_VALUE);
      cs10_set_led(UP_NULL_LED_ADDR, LED_ON_VALUE);

      cs10_set_led(LEFT_WHEEL_LED_ADDR, LED_ON_VALUE);
      cs10_set_led(RIGHT_WHEEL_LED_ADDR, LED_ON_VALUE);

      for (uiTrack = 0 ;
           uiTrack < CS10_NUM_PHYSICAL_TRACKS ;
           uiTrack++) {
        cs10_set_led(TRACK_TO_LED_ADDR(uiTrack), LED_OFF_VALUE) ;
      } /* for */

      cs10_set_led(TRACK_TO_LED_ADDR(cs10.uiSelectedTrack),
          LED_ON_VALUE) ;
      break ;
    default:
      break ;
  } /* break */
} /* cs10_set_mode */

/*
 * cs10_issue_mmc_command
 *
 * send uiCommand on cs10.iMMCPortID
 */
bool
cs10_issue_mmc_command(
  unsigned int uiCommand) {

  bool             bRetValue ;
  snd_seq_event_t  theEvent ;
  unsigned char    ucCommand[MMC_CMD_SYSEX_PACKET_LENGTH] =
     MMC_CMD_SYSEX_PACKET(MMC_DEVICEID_ALL, uiCommand) ;

  snd_seq_ev_clear(&theEvent) ;
  snd_seq_ev_set_dest(&theEvent, SND_SEQ_ADDRESS_SUBSCRIBERS, 0) ;
  snd_seq_ev_set_source(&theEvent, cs10.iMMCPortID) ;
  snd_seq_ev_set_direct(&theEvent) ;

  snd_seq_ev_set_sysex(&theEvent, MMC_CMD_SYSEX_PACKET_LENGTH, ucCommand) ;

  snd_seq_event_output(cs10.pSeq, &theEvent) ;
  snd_seq_drain_output(cs10.pSeq) ;

  return bRetValue ;
} /* cs10_issue_mmc_command */

/*
 * cs10_issue_mmc_step_command
 *
 * send mmc step command on cs10.iMMCPortID
 */
bool
cs10_issue_mmc_step_command(
  int iSteps) {

  bool             bRetValue = true ;
  snd_seq_event_t  theEvent ;
  unsigned char    ucCommand[MMC_STEP_SYSEX_PACKET_LENGTH] =
     MMC_STEP_SYSEX_PACKET(MMC_DEVICEID_ALL, iSteps) ;

  snd_seq_ev_clear(&theEvent) ;
  snd_seq_ev_set_dest(&theEvent, SND_SEQ_ADDRESS_SUBSCRIBERS, 0) ;
  snd_seq_ev_set_source(&theEvent, cs10.iMMCPortID) ;
  snd_seq_ev_set_direct(&theEvent) ;

  snd_seq_ev_set_sysex(&theEvent, MMC_STEP_SYSEX_PACKET_LENGTH, ucCommand) ;

  snd_seq_event_output(cs10.pSeq, &theEvent) ;
  snd_seq_drain_output(cs10.pSeq) ;

  return bRetValue ;
} /* cs10_issue_mmc_step_command */

/*
 * cs10_issue_mmc_goto_command
 *
 * send mmc goto command on cs10.iMMCPortID
 */
bool
cs10_issue_mmc_goto_command(
  smpte_time_t theTime) {

  bool             bRetValue = true ;
  snd_seq_event_t  theEvent ;
  unsigned char    ucCommand[MMC_GOTO_SYSEX_PACKET_LENGTH] =
     MMC_GOTO_SYSEX_PACKET(MMC_DEVICEID_ALL,
         theTime.hours, theTime.minutes, theTime.seconds, theTime.frames, 0) ;

  snd_seq_ev_clear(&theEvent) ;
  snd_seq_ev_set_dest(&theEvent, SND_SEQ_ADDRESS_SUBSCRIBERS, 0) ;
  snd_seq_ev_set_source(&theEvent, cs10.iMMCPortID) ;
  snd_seq_ev_set_direct(&theEvent) ;

  snd_seq_ev_set_sysex(&theEvent, MMC_GOTO_SYSEX_PACKET_LENGTH, ucCommand) ;

  snd_seq_event_output(cs10.pSeq, &theEvent) ;
  snd_seq_drain_output(cs10.pSeq) ;

  return bRetValue ;
} /* cs10_issue_mmc_goto_command */

/*
 * cs10_issue_virtual_control
 *
 * issue virtual control move on uiVirtualTrack's tcControl
 */
bool
cs10_issue_virtual_control(
  unsigned int uiVirtualTrack,
  virtual_track_control_t tcControl,
  unsigned int uiValue) {

  bool             bRetValue ;
  snd_seq_event_t  theEvent ;
  unsigned int     uiBank = uiVirtualTrack / CS10_NUM_PHYSICAL_TRACKS;
  unsigned int     uiPhysicalTrack = uiVirtualTrack % CS10_NUM_PHYSICAL_TRACKS;

  snd_seq_ev_clear(&theEvent) ;
  snd_seq_ev_set_dest(&theEvent, SND_SEQ_ADDRESS_SUBSCRIBERS, 0) ;
  snd_seq_ev_set_source(&theEvent, cs10.iMMCPortID) ;
  snd_seq_ev_set_direct(&theEvent) ;

  snd_seq_ev_set_controller(&theEvent, CS10_MIDI_CONTROL_CHANNEL + uiBank,
      (uiPhysicalTrack * NUM_VIRTUAL_TRACK_CONTROLS) + tcControl,
      uiValue) ;

  snd_seq_event_output(cs10.pSeq, &theEvent) ;
  snd_seq_drain_output(cs10.pSeq) ;

  return bRetValue ;
} /* cs10_issue_virtual_control */

/*
 * cs10_receive_virtual_control
 *
 * receive control state from a peer connected to the sequencer
 * update mixer state accordingly
 */
void
cs10_receive_virtual_control(
  unsigned int track,
  virtual_track_control_t control,
  unsigned int value) {

  switch (control) {
    case ARMED_CONTROL:
      cs10.csState.tsTrack[track].bArmed =
        (value ? true : false);
      cs10_set_mode(cs10.theMode) ;
      break;

    case MUTE_CONTROL:
      cs10.csState.tsTrack[track].bMute =
        (value ? true : false);
      cs10_set_mode(cs10.theMode) ;
      break;

    case SOLO_CONTROL:
      cs10.csState.tsTrack[track].bSolo =
        (value ? true : false);
      cs10_set_mode(cs10.theMode) ;
      break;

    case FADER_CONTROL:
      cs10.csState.tsTrack[track].uiFader = value ;
      break;

    case PAN_CONTROL:
    case SEND_ONE_CONTROL:
    case SEND_TWO_CONTROL:
    case BOOST_CUT_CONTROL:
    case FREQUENCY_CONTROL:
    case BANDWDITH_CONTROL:
      cs10.csState.tsTrack[track].uiKnob[
        VIRTUAL_CONTROL_TO_KNOB_INDEX(control)] = value ;
      break;

    default:
      break;
  } /* switch */
} /* cs10_receive_virtual_control */

/*
 * cs10_issue_control_state
 *
 * re-send the entire control state
 */
bool
cs10_issue_control_state(
  cs10_mixer_state_t *pState) {

  unsigned int uiTrack;
  unsigned int uiControl;

  for (uiTrack = 0 ;
       uiTrack < CS10_NUM_VIRTUAL_TRACKS ;
       uiTrack++) {
    for (uiControl = 0 ;
         uiControl < NUM_VIRTUAL_TRACK_CONTROLS ;
         uiControl++) {
      switch (uiControl) {
        /* XXX NB toggle states need to be sent relative to the state that
         * is being replaced
         */
        case ARMED_CONTROL:
          if (cs10.csState.tsTrack[uiTrack].bArmed !=
              pState->tsTrack[uiTrack].bArmed) {
            cs10_issue_virtual_control(uiTrack,
              ARMED_CONTROL, BUTTON_DOWN_VALUE);
            cs10_issue_virtual_control(uiTrack,
              ARMED_CONTROL, BUTTON_UP_VALUE);
          } /* if */
          break;

        case MUTE_CONTROL:
          if (cs10.csState.tsTrack[uiTrack].bMute !=
              pState->tsTrack[uiTrack].bMute) {
            cs10_issue_virtual_control(uiTrack,
              MUTE_CONTROL, BUTTON_DOWN_VALUE);
            cs10_issue_virtual_control(uiTrack,
              MUTE_CONTROL, BUTTON_UP_VALUE);
          } /* if */
          break;

        case SOLO_CONTROL:
          if (cs10.csState.tsTrack[uiTrack].bSolo !=
              pState->tsTrack[uiTrack].bSolo) {
            cs10_issue_virtual_control(uiTrack,
              SOLO_CONTROL, BUTTON_DOWN_VALUE);
            cs10_issue_virtual_control(uiTrack,
              SOLO_CONTROL, BUTTON_UP_VALUE);
          } /* if */
          break;

        /* XXX NB control states need to be sent in increments,
         * starting at the state that is being replaced
         */
        case FADER_CONTROL:
          {
            unsigned int uiTarget = pState->tsTrack[uiTrack].uiFader;

            while (cs10.csState.tsTrack[uiTrack].uiFader != uiTarget) {
              if (cs10.csState.tsTrack[uiTrack].uiFader > uiTarget)
                cs10.csState.tsTrack[uiTrack].uiFader--;
              else
                cs10.csState.tsTrack[uiTrack].uiFader++;

              cs10_issue_virtual_control(uiTrack, FADER_CONTROL,
                cs10.csState.tsTrack[uiTrack].uiFader);
#ifdef CS10_FADER_RESTORE_DELAY_US
              usleep(CS10_FADER_RESTORE_DELAY_US);
#endif
            }
          }
          break;

        case PAN_CONTROL:
        case SEND_ONE_CONTROL:
        case SEND_TWO_CONTROL:
        case BANDWDITH_CONTROL:
        case FREQUENCY_CONTROL:
        case BOOST_CUT_CONTROL:
          {
            unsigned int idx = VIRTUAL_CONTROL_TO_KNOB_INDEX(uiControl);
            unsigned int uiTarget = pState->tsTrack[uiTrack].uiKnob[idx];

            while (cs10.csState.tsTrack[uiTrack].uiKnob[idx] != uiTarget) {
              if (cs10.csState.tsTrack[uiTrack].uiKnob[idx] > uiTarget)
                cs10.csState.tsTrack[uiTrack].uiKnob[idx]--;
              else
                cs10.csState.tsTrack[uiTrack].uiKnob[idx]++;

              cs10_issue_virtual_control(uiTrack, uiControl,
                cs10.csState.tsTrack[uiTrack].uiKnob[idx]);
#ifdef CS10_FADER_RESTORE_DELAY_US
              usleep(CS10_FADER_RESTORE_DELAY_US);
#endif
            } /* while */
          }
          break;
      } /* switch */
    } /* for */
  } /* for */
} /* cs10_issue_control_state */

/*
 * cs10_handle_button
 *
 * do stuff based on uiButtonAddr and uiButtonVal
 */
void
cs10_handle_button(
  unsigned int uiButtonAddr,
  int uiButtonVal) {

  if (cs10.debug)
    fprintf(stderr, "%s %u %d\n",
      __FUNCTION__,
      uiButtonAddr,
      uiButtonVal);

  /* handle track buttons */
  if (((FIRST_TRACK_BUTTON_ADDR <= uiButtonAddr) &&
     (LAST_TRACK_BUTTON_ADDR >= uiButtonAddr)) &&
     (BUTTON_UP_VALUE == uiButtonVal)) {
    switch (cs10.theMode) {
      case NULLIFY_MODE:
      case SELECT_MODE:
        cs10_set_led(TRACK_TO_LED_ADDR(cs10.uiSelectedTrack),
            LED_OFF_VALUE) ;

        cs10.uiSelectedTrack =
          BUTTON_ADDR_TO_TRACK(uiButtonAddr) ;

        cs10_set_led(TRACK_TO_LED_ADDR(cs10.uiSelectedTrack),
            LED_ON_VALUE) ;
        break ;

      case LOC_MODE:
        cs10.csState.tsTrack[
          cs10.uiBank * CS10_NUM_PHYSICAL_TRACKS + 
          BUTTON_ADDR_TO_TRACK(uiButtonAddr)].bArmed =
          (cs10.csState.
           tsTrack[
             cs10.uiBank * CS10_NUM_PHYSICAL_TRACKS + 
             BUTTON_ADDR_TO_TRACK(uiButtonAddr)].bArmed == 0) ;

#if CS10_TOGGLE_BUTTONS
        cs10_issue_virtual_control(
          cs10.uiBank * CS10_NUM_PHYSICAL_TRACKS + 
          BUTTON_ADDR_TO_TRACK(uiButtonAddr),
          ARMED_CONTROL,
          (cs10.csState.
             tsTrack[
               cs10.uiBank * CS10_NUM_PHYSICAL_TRACKS + 
               BUTTON_ADDR_TO_TRACK(uiButtonAddr)].bArmed ?
             BUTTON_DOWN_VALUE : BUTTON_UP_VALUE)) ;
#else
        cs10_issue_virtual_control(
          cs10.uiBank * CS10_NUM_PHYSICAL_TRACKS + 
          BUTTON_ADDR_TO_TRACK(uiButtonAddr),
          ARMED_CONTROL, BUTTON_DOWN_VALUE);
        cs10_issue_virtual_control(
          cs10.uiBank * CS10_NUM_PHYSICAL_TRACKS + 
          BUTTON_ADDR_TO_TRACK(uiButtonAddr),
          ARMED_CONTROL, BUTTON_UP_VALUE);
#endif

        cs10_set_led(TRACK_TO_LED_ADDR(BUTTON_ADDR_TO_TRACK(uiButtonAddr)),
         (cs10.csState.
          tsTrack[
            cs10.uiBank * CS10_NUM_PHYSICAL_TRACKS + 
            BUTTON_ADDR_TO_TRACK(uiButtonAddr)].bArmed ?
          LED_ON_VALUE : LED_OFF_VALUE)) ;
        break ;

      case MUTE_MODE:
        cs10.csState.tsTrack[
          cs10.uiBank * CS10_NUM_PHYSICAL_TRACKS + 
          BUTTON_ADDR_TO_TRACK(uiButtonAddr)].bMute =
          (cs10.csState.
           tsTrack[
             cs10.uiBank * CS10_NUM_PHYSICAL_TRACKS + 
             BUTTON_ADDR_TO_TRACK(uiButtonAddr)].bMute == 0) ;

#if CS10_TOGGLE_BUTTONS
        cs10_issue_virtual_control(
          cs10.uiBank * CS10_NUM_PHYSICAL_TRACKS + 
          BUTTON_ADDR_TO_TRACK(uiButtonAddr),
          MUTE_CONTROL,
          (cs10.csState.
             tsTrack[
               cs10.uiBank * CS10_NUM_PHYSICAL_TRACKS + 
               BUTTON_ADDR_TO_TRACK(uiButtonAddr)].bMute ?
             BUTTON_DOWN_VALUE : BUTTON_UP_VALUE)) ;
#else
        cs10_issue_virtual_control(
          cs10.uiBank * CS10_NUM_PHYSICAL_TRACKS + 
          BUTTON_ADDR_TO_TRACK(uiButtonAddr),
          MUTE_CONTROL, BUTTON_DOWN_VALUE);
        cs10_issue_virtual_control(
          cs10.uiBank * CS10_NUM_PHYSICAL_TRACKS + 
          BUTTON_ADDR_TO_TRACK(uiButtonAddr),
          MUTE_CONTROL, BUTTON_UP_VALUE);
#endif

        cs10_set_led(TRACK_TO_LED_ADDR(BUTTON_ADDR_TO_TRACK(uiButtonAddr)),
         (cs10.csState.
          tsTrack[
            cs10.uiBank * CS10_NUM_PHYSICAL_TRACKS + 
            BUTTON_ADDR_TO_TRACK(uiButtonAddr)].bMute ?
          LED_ON_VALUE : LED_OFF_VALUE)) ;
        break ;

      case SOLO_MODE:
        cs10.csState.tsTrack[
          cs10.uiBank * CS10_NUM_PHYSICAL_TRACKS + 
          BUTTON_ADDR_TO_TRACK(uiButtonAddr)].bSolo =
          (cs10.csState.
           tsTrack[
             cs10.uiBank * CS10_NUM_PHYSICAL_TRACKS + 
             BUTTON_ADDR_TO_TRACK(uiButtonAddr)].bSolo == 0) ;

#if CS10_TOGGLE_BUTTONS
        cs10_issue_virtual_control(
          cs10.uiBank * CS10_NUM_PHYSICAL_TRACKS + 
          BUTTON_ADDR_TO_TRACK(uiButtonAddr),
          SOLO_CONTROL,
          (cs10.csState.
             tsTrack[
               cs10.uiBank * CS10_NUM_PHYSICAL_TRACKS + 
               BUTTON_ADDR_TO_TRACK(uiButtonAddr)].bSolo ?
             BUTTON_DOWN_VALUE : BUTTON_UP_VALUE)) ;
#else
        cs10_issue_virtual_control(
          cs10.uiBank * CS10_NUM_PHYSICAL_TRACKS + 
          BUTTON_ADDR_TO_TRACK(uiButtonAddr),
          SOLO_CONTROL, BUTTON_DOWN_VALUE);
        cs10_issue_virtual_control(
          cs10.uiBank * CS10_NUM_PHYSICAL_TRACKS + 
          BUTTON_ADDR_TO_TRACK(uiButtonAddr),
          SOLO_CONTROL, BUTTON_UP_VALUE);
#endif

        cs10_set_led(TRACK_TO_LED_ADDR(BUTTON_ADDR_TO_TRACK(uiButtonAddr)),
         (cs10.csState.
          tsTrack[
            cs10.uiBank * CS10_NUM_PHYSICAL_TRACKS + 
            BUTTON_ADDR_TO_TRACK(uiButtonAddr)].bSolo ?
          LED_ON_VALUE : LED_OFF_VALUE)) ;
        break ;

      default: 
        break ;
    } /* switch */
  } else
  if ((F1_BUTTON_ADDR <= uiButtonAddr) &&
     (F9_BUTTON_ADDR >= uiButtonAddr)) {
    if (cs10.bShiftKeyDown) {
      /* save/restore position */
      if (cs10.bRecordKeyDown) {
        cs10.bIgnoreRecordKeyUp = true ;
        cs10.tSavedPosition[uiButtonAddr - F1_BUTTON_ADDR] =
          cs10.tCurrentTime ;
        cs10_save_settings();
      } else {
        cs10_issue_mmc_goto_command(
            cs10.tSavedPosition[uiButtonAddr - F1_BUTTON_ADDR]) ;
      } /* !bRecordKeyDown */
    } else {
      /* save/restore fader settings */
      if (cs10.bRecordKeyDown) {
        cs10.bIgnoreRecordKeyUp = true ;
        memcpy(&cs10.csSavedState[uiButtonAddr - F1_BUTTON_ADDR],
               &cs10.csState, sizeof(cs10_mixer_state_t));
        cs10_save_settings();
      } else {
        /* send state out over midi seq */
        cs10_issue_control_state(
          &cs10.csSavedState[uiButtonAddr - F1_BUTTON_ADDR]);
        memcpy(&cs10.csState,
               &cs10.csSavedState[uiButtonAddr - F1_BUTTON_ADDR],
               sizeof(cs10_mixer_state_t));
        cs10_display_bank();
      } /* !bRecordKeyDown */
    } /* !bShiftKeyDown */
  } else
  switch (uiButtonAddr) {
    case SHIFT_BUTTON_ADDR:
      cs10.bShiftKeyDown = (BUTTON_DOWN_VALUE == uiButtonVal) ;
      break ;

    case REW_BUTTON_ADDR:
      if (BUTTON_UP_VALUE == uiButtonVal)
        if (cs10.bShiftKeyDown) {
          smpte_time_t tZero = {0, 0, 0, 0, 0} ;

          cs10_issue_mmc_goto_command(tZero) ;
        } else
          cs10_issue_mmc_command(MMC_COMMAND_REW) ;
      break ;

    case FF_BUTTON_ADDR:
      if (BUTTON_UP_VALUE == uiButtonVal)
        cs10_issue_mmc_command(MMC_COMMAND_FF) ;
      break;

    case STOP_BUTTON_ADDR:
      if (BUTTON_UP_VALUE == uiButtonVal)
        cs10_issue_mmc_command(MMC_COMMAND_STOP) ;
      break ;

    case PLAY_BUTTON_ADDR:
      if (BUTTON_UP_VALUE == uiButtonVal) {
        if (cs10.bShiftKeyDown)
          cs10_issue_mmc_goto_command(cs10.tPlayFromTime) ;
        else {
          cs10.tPlayFromTime = cs10.tCurrentTime ;
          cs10_issue_mmc_command(MMC_COMMAND_PLAY) ;
        } /* !bShiftKeyDown */
      } /* BUTTON_UP_VALUE */
      break ;

    case RECORD_BUTTON_ADDR:
      cs10.bRecordKeyDown = (BUTTON_DOWN_VALUE == uiButtonVal) ;

      if (!cs10.bRecordKeyDown) {
        if (cs10.bIgnoreRecordKeyUp)
          cs10.bIgnoreRecordKeyUp = false ;
        else {
          if (cs10.bShiftKeyDown)
            cs10_issue_mmc_goto_command(cs10.tRecordFromTime) ;
          else {
            cs10.tRecordFromTime = cs10.tCurrentTime ;
            cs10_issue_mmc_command(MMC_COMMAND_REC_PAUSE) ;
          } /* !bShiftKeyDown */
        } /* !bIgnoreRecordKeyUp */
      } /* !bRecordKeyDown */
      break ;

    case MODE_BUTTON_ADDR:
      if (BUTTON_UP_VALUE == uiButtonVal) {
        if (NUM_MODES == ++cs10.theMode)
          cs10.theMode = SELECT_MODE ;

        cs10_set_mode(cs10.theMode) ;
      } /* if */
      break ;

    case RIGHT_BUTTON_ADDR:
      if (BUTTON_UP_VALUE == uiButtonVal) {
        if (cs10.displayMode == BANK_DISPLAY_MODE) {
          if (++cs10.uiBank >= CS10_NUM_BANKS)
            cs10.uiBank = 0;
          cs10_display_bank();
          cs10_set_mode(cs10.theMode) ;
        } else {
          if (++cs10.smpteDisplayMode >= NUM_SMPTE_DISPLAY_MODES)
            cs10.smpteDisplayMode = 0;
          cs10_display_time();
        } /* else */
      } /* if */
      break;

    case LEFT_BUTTON_ADDR:
      if (BUTTON_UP_VALUE == uiButtonVal) {
        if (cs10.displayMode == BANK_DISPLAY_MODE) {
          if (cs10.uiBank-- == 0)
            cs10.uiBank = CS10_NUM_BANKS - 1;
          cs10_display_bank();
          cs10_set_mode(cs10.theMode) ;
        } else {
          if (cs10.smpteDisplayMode-- == 0)
            cs10.smpteDisplayMode = NUM_SMPTE_DISPLAY_MODES - 1;
          cs10_display_time();
        } /* else */
      } /* if */
      break;

    case UP_BUTTON_ADDR:
      if (BUTTON_UP_VALUE == uiButtonVal) {
        if (++cs10.displayMode == NUM_DISPLAY_MODES)
          cs10.displayMode = 0;

        if (cs10.displayMode == BANK_DISPLAY_MODE) {
          cs10_set_led(TENS_DEC_LED_ADDR, LED_OFF_VALUE);
          cs10_set_led(ONES_DEC_LED_ADDR, LED_OFF_VALUE);
          cs10_display_bank();
        } else {
          cs10_display_time();
        } /* else */
      } /* if */
      break;

    case DOWN_BUTTON_ADDR:
      if (BUTTON_UP_VALUE == uiButtonVal) {
        if (cs10.displayMode-- == 0)
          cs10.displayMode = NUM_DISPLAY_MODES - 1;

        if (cs10.displayMode == BANK_DISPLAY_MODE) {
          cs10_set_led(TENS_DEC_LED_ADDR, LED_OFF_VALUE);
          cs10_set_led(ONES_DEC_LED_ADDR, LED_OFF_VALUE);
          cs10_display_bank();
        } else {
          cs10_display_time();
        } /* else */
      } /* if */
      break;

    default:
      break ;
  } /* switch */
} /* cs10_handle_button */

/*
 * cs10_handle_fader
 *
 * do stuff based on uiFaderAddr and uiFaderVal
 */
void
cs10_handle_fader(
  unsigned int uiFaderAddr,
  int uiFaderVal) {

  if (cs10.debug)
    fprintf(stderr, "%s %u %d\n",
      __FUNCTION__,
      uiFaderAddr,
      uiFaderVal);

  if (NULLIFY_MODE == cs10.theMode) {
    if (uiFaderVal <
         cs10.csState.tsTrack[
         cs10.uiBank * CS10_NUM_PHYSICAL_TRACKS +
         FADER_ADDR_TO_TRACK(uiFaderAddr)].uiFader){
      cs10_set_led(DOWN_NULL_LED_ADDR, LED_OFF_VALUE);
      cs10_set_led(UP_NULL_LED_ADDR, LED_ON_VALUE);
    } else
    if (uiFaderVal >
         cs10.csState.tsTrack[
         cs10.uiBank * CS10_NUM_PHYSICAL_TRACKS +
         FADER_ADDR_TO_TRACK(uiFaderAddr)].uiFader){
      cs10_set_led(DOWN_NULL_LED_ADDR, LED_ON_VALUE);
      cs10_set_led(UP_NULL_LED_ADDR, LED_OFF_VALUE);
    } else {
      cs10_set_led(DOWN_NULL_LED_ADDR, LED_OFF_VALUE);
      cs10_set_led(UP_NULL_LED_ADDR, LED_OFF_VALUE);
    }
  } else {
    cs10.csState.
      tsTrack[
        cs10.uiBank * CS10_NUM_PHYSICAL_TRACKS +
        FADER_ADDR_TO_TRACK(uiFaderAddr)].
      uiFader = uiFaderVal;

    cs10_issue_virtual_control(
        cs10.uiBank * CS10_NUM_PHYSICAL_TRACKS +
        FADER_ADDR_TO_TRACK(uiFaderAddr),
        FADER_CONTROL,
        uiFaderVal) ;
  } /* else */
} /* cs10_handle_fader */

/*
 * cs10_handle_knob
 *
 * do stuff based on uiKnobAddr and uiKnobVal
 */
void
cs10_handle_knob(
  unsigned int uiKnobAddr,
  unsigned int uiKnobVal) {

  unsigned int idx = KNOB_ADDR_TO_KNOB_INDEX(uiKnobAddr);

  if (cs10.debug)
    fprintf(stderr, "%s %u %d\n",
      __FUNCTION__,
      uiKnobAddr,
      uiKnobVal);

  if (NULLIFY_MODE == cs10.theMode) {
    if (uiKnobVal <
         cs10.csState.tsTrack[
         cs10.uiBank * CS10_NUM_PHYSICAL_TRACKS +
         cs10.uiSelectedTrack].uiKnob[idx]) {
      cs10_set_led(LEFT_WHEEL_LED_ADDR, LED_OFF_VALUE);
      cs10_set_led(RIGHT_WHEEL_LED_ADDR, LED_ON_VALUE);
    } else
    if (uiKnobVal >
         cs10.csState.tsTrack[
         cs10.uiBank * CS10_NUM_PHYSICAL_TRACKS +
         cs10.uiSelectedTrack].uiKnob[idx]) {
      cs10_set_led(LEFT_WHEEL_LED_ADDR, LED_ON_VALUE);
      cs10_set_led(RIGHT_WHEEL_LED_ADDR, LED_OFF_VALUE);
    } else {
      cs10_set_led(LEFT_WHEEL_LED_ADDR, LED_OFF_VALUE);
      cs10_set_led(RIGHT_WHEEL_LED_ADDR, LED_OFF_VALUE);
    }
  } else {
    cs10.csState.
      tsTrack[
      cs10.uiBank * CS10_NUM_PHYSICAL_TRACKS +
      cs10.uiSelectedTrack].
      uiKnob[idx] = uiKnobVal ;

    cs10_issue_virtual_control(
        cs10.uiBank * CS10_NUM_PHYSICAL_TRACKS +
        cs10.uiSelectedTrack,
        KNOB_ADDR_TO_VIRTUAL_CONTROL(uiKnobAddr), uiKnobVal) ;
  } /* else */
} /* cs10_handle_knob */

/*
 * cs10_handle_wheel
 *
 * issue MMC step commands based on uiWheelVal
 */
void
cs10_handle_wheel(
  unsigned int uiWheelVal) {

  if (cs10.debug)
    fprintf(stderr, "%s %u\n",
      __FUNCTION__,
      uiWheelVal);

  /* add to cs10.iJogCount */
  cs10.iJogCount += (uiWheelVal & 0x40 ?
      0 - (((~uiWheelVal) & 0x7f) + 1) :
      uiWheelVal) ;

  if ((CS10_JOG_THRESHOLD < cs10.iJogCount) ||
      (-CS10_JOG_THRESHOLD > cs10.iJogCount)) {
    int iStepValue = (cs10.iJogCount / CS10_JOG_DIVISOR) ;

    iStepValue = (iStepValue > 0 ? iStepValue :
        -iStepValue | 0x40) & 0x7f ;

    cs10_issue_mmc_step_command(iStepValue) ;

    cs10.iJogCount = 0 ;
  } /* if */
} /* cs10_handle_wheel */

/*
 * cs10_receive_sysex
 *
 * receive a sysex message from the DAW or whatever
 * scrape out MTC or whatever
 */
void
cs10_receive_sysex(
  unsigned int length,
  unsigned char *data) {

  if (10 == length) {
    if ((0xf0 == data[0]) &&
        (0x7f == data[1]) &&
        (0x01 == data[3]) &&
        (0x01 == data[4])) {

      cs10.tCurrentTime.hours =
        data[5] ;
      cs10.tCurrentTime.minutes =
        data[6] ;
      cs10.tCurrentTime.seconds =
        data[7] ;
      cs10.tCurrentTime.frames =
        data[8] ;

      cs10_update_display_time();

      if (cs10.debug)
        fprintf(stderr, "%s %02d:%02d:%02d:%02d\n",
          __FUNCTION__,
          cs10.tCurrentTime.hours,
          cs10.tCurrentTime.minutes,
          cs10.tCurrentTime.seconds,
          cs10.tCurrentTime.frames) ;

    } else /* MTC full frame time */
    if ((0xf0 == data[0]) &&
        (0x7f == data[1]) &&
        (0x06 == data[3]) &&
        (0x44 == data[4]) &&
        (0x06 == data[5]) &&
        (0x01 == data[6])) {

      cs10.tCurrentTime.hours =
        data[7] ;
      cs10.tCurrentTime.minutes =
        data[8] ;
      cs10.tCurrentTime.seconds =
        data[9] ;
      cs10.tCurrentTime.frames =
        data[10] ;

      cs10_update_display_time();

      if (cs10.debug)
        fprintf(stderr, "%s MMC LOC %02d:%02d:%02d:%02d\n",
          __FUNCTION__,
          cs10.tCurrentTime.hours,
          cs10.tCurrentTime.minutes,
          cs10.tCurrentTime.seconds,
          cs10.tCurrentTime.frames) ;

    } else { /* MMC locate */
      if (cs10.debug) {
        unsigned int i;

        fprintf(stderr, "%s sysex", __FUNCTION__);
        for (i = 0;
             i < length ;
             i++) {
          fprintf(stderr, " %02x", data[i]);
        } /* for */
        fprintf(stderr, "\n");
      } /* if */
    } /* else */
  } /* length matches MTC full frame time */
} /* cs10_receive_sysex */

/*
 * cs10_receive_qframe
 *
 * receive qframe slices of time code and re-assemble them
 */
void
cs10_receive_qframe(
  unsigned char qframe_data) {

  unsigned char ucField = ((qframe_data & 0xf0) >> 4) ;
  unsigned char ucData = (qframe_data & 0x0f) ;

  if (cs10.debug)
    fprintf(stderr, "%s %x %x\n",
      __FUNCTION__, ucField, ucData); 

  switch(ucField) {
    case 0:
      cs10.tQFTime.frames = (cs10.tQFTime.frames & 0xf0) + ucData ;
      break ;

    case 1:
      cs10.tQFTime.frames = (cs10.tQFTime.frames & 0x0f) + (ucData << 4) ;
      break ;

    case 2:
      cs10.tQFTime.seconds = (cs10.tQFTime.seconds & 0xf0) + ucData ;
      break ;

    case 3:
      cs10.tQFTime.seconds = (cs10.tQFTime.seconds & 0x0f) + (ucData << 4) ;
      break ;

    case 4:
      cs10.tQFTime.minutes = (cs10.tQFTime.minutes & 0xf0) + ucData ;
      break ;

    case 5:
      cs10.tQFTime.minutes = (cs10.tQFTime.minutes & 0x0f) + (ucData << 4) ;
      break ;

    case 6:
      cs10.tQFTime.hours = (cs10.tQFTime.hours & 0xf0) + ucData ;
      break ;

    case 7:
      cs10.tQFTime.hours = (cs10.tQFTime.hours & 0x0f) +
        ((ucData << 4) & 0x01) ;
      cs10.tQFTime.flags = (ucData >> 1) ;
      break ;
  } /* switch */

  cs10.ucQuarterFrameFlags |= 1 << ucField ;

  if (0xff == cs10.ucQuarterFrameFlags) {
    cs10.tCurrentTime = cs10.tQFTime ;
    cs10.ucQuarterFrameFlags = 0 ;

    cs10_update_display_time();

    if (cs10.debug)
      fprintf(stderr, "%s %02d:%02d:%02d:%02d\n",
        __FUNCTION__,
        cs10.tCurrentTime.hours,
        cs10.tCurrentTime.minutes,
        cs10.tCurrentTime.seconds,
        cs10.tCurrentTime.frames) ;

  } /* if */
} /* cs10_receive_qframe */


/*
 * cs10_get_local_data_file
 *
 * wrangle whatever environment variables to get a path to the local data file
 */
void
cs10_get_local_data_file() {
  char *datadir = getenv("XDG_DATA_HOME");

  if (datadir == NULL) {
    char *homedir = getenv("HOME");
    int   data_path_len = 0;

    if (homedir == NULL) {
      homedir = getpwuid(getuid())->pw_dir;
    } /* if */

    if (homedir == NULL)
      return;

    data_path_len = strlen(homedir) +
      strlen(CS10_DEFAULT_SETTINGS_PATH) +
      strlen(CS10_DEFAULT_SETTINGS_FILENAME) + 1;

    cs10.settings_filename = malloc(data_path_len);

    if (cs10.settings_filename) {
      strcpy(cs10.settings_filename, homedir);
      strcat(cs10.settings_filename, CS10_DEFAULT_SETTINGS_PATH);

      mkdir(cs10.settings_filename, 0700);

      strcat(cs10.settings_filename, CS10_DEFAULT_SETTINGS_FILENAME);
    } /* if */
  } else {
    int data_path_len = strlen(datadir) +
      strlen(CS10_DEFAULT_SETTINGS_DIR) +
      strlen(CS10_DEFAULT_SETTINGS_FILENAME) + 1;

    cs10.settings_filename = malloc(data_path_len);

    if (cs10.settings_filename) {
      strcpy(cs10.settings_filename, datadir);
      strcat(cs10.settings_filename, CS10_DEFAULT_SETTINGS_DIR);

      mkdir(cs10.settings_filename, 0700);

      strcat(cs10.settings_filename, CS10_DEFAULT_SETTINGS_FILENAME);
    } /* if */
  } /* else */
} /* cs10_get_local_data_file */

static struct option long_opts[] = {
  { "verbose", no_argument, NULL, 'v'},
  { "file", required_argument, NULL, 'f'},
  { "port", required_argument, NULL, 'p'},
  { "help", no_argument, NULL, 'h'},
};

void
cs10_help_exit(
  int argc,
  char** argv) {

  fprintf(stderr, "%s options:\n", argv[0]);
  fprintf(stderr, "  --file, -f [path] to persistent data file\n");
  fprintf(stderr, "  --port, -p [client:port] of midi hardware interface\n");
  fprintf(stderr, "  --verbose, -v print debug information\n");
  fprintf(stderr, "  --help, -h show this help and exit\n");
  exit(0);
} /* cs10_help_exit */

int 
main(
  int argc,
  char** argv) { 

  char c;

  memset(&cs10, sizeof(cs10), 0) ;

  while ((c = getopt_long(argc, argv, "vf:p:h", long_opts, NULL)) != -1) {
    switch (c) {
      case 'v':
        /* verbose = true */
        cs10.debug = true;
        break;

      case 'f':
        /* filename = optarg */
        cs10.settings_filename = strdup(optarg); 
        break;

      case 'p':
        /* midi port = optarg */
        {
          char *startptr, *nextptr;
          unsigned long client_id = strtoul(optarg, &nextptr, 10);
          unsigned long port_id = 0;
          bool bad_param = false;

          if ((nextptr != optarg) &&
              (*nextptr = ':')) {
            startptr = nextptr + 1;
            port_id = strtoul(startptr, &nextptr, 10);
            if (nextptr != startptr) {
              cs10.hw_seq_client = client_id; 
              cs10.hw_seq_port = port_id; 
              if (cs10.debug)
                fprintf(stderr, "hw midi port %ld:%ld\n", client_id, port_id);
            } else
              bad_param = true;
          } else
            bad_param = true;

          if (bad_param) {
            fprintf(stderr, "bad parameter: %s\n", optarg);
            cs10_help_exit(argc, argv);
          } /* if */
        }
        break;

      case 'h':
        /* help exit */
        cs10_help_exit(argc, argv) ;
        break;

      default:
        break;
    } /* switch */
  } /* while */

  if (cs10.settings_filename == NULL)
    cs10_get_local_data_file();

  if (cs10.debug)
    fprintf(stderr, "using settings file %s\n", cs10.settings_filename);

  if (cs10_init()) {
    snd_seq_event_t *pNewEvent ;

    if (cs10.hw_seq_client) {
      if (cs10.debug)
        fprintf(stderr, "connect to %d:%d\n",
          cs10.hw_seq_client, cs10.hw_seq_port);
      snd_seq_connect_to(cs10.pSeq, 0, cs10.hw_seq_client, cs10.hw_seq_port);
      if (cs10.debug)
        fprintf(stderr, "connect from %d:%d\n",
          cs10.hw_seq_client, cs10.hw_seq_port);
      snd_seq_connect_from(cs10.pSeq, 0, cs10.hw_seq_client, cs10.hw_seq_port);
    } /* if */

    cs10_load_settings();
    cs10_set_mode(cs10.theMode) ;

    while (snd_seq_event_input(cs10.pSeq, &pNewEvent) >= 0) {
      if (SND_SEQ_EVENT_PORT_SUBSCRIBED == pNewEvent->type) {
        cs10_set_mode(cs10.theMode) ;
        continue;
      } /* else */

      if (pNewEvent->dest.port == cs10.iMMCPortID) {
        if (SND_SEQ_EVENT_SYSEX == pNewEvent->type) {
          cs10_receive_sysex(pNewEvent->data.ext.len,
                            (unsigned char*)pNewEvent->data.ext.ptr);
        } else /* SND_SEQ_EVENT_SYSEX */
        if (SND_SEQ_EVENT_QFRAME == pNewEvent->type) {
          cs10_receive_qframe(pNewEvent->data.control.value);
        } else /* SND_SEQ_EVENT_QFRAME */
        if (SND_SEQ_EVENT_CONTROLLER == pNewEvent->type) {
          if ((pNewEvent->data.control.param >= 0) &&
             (pNewEvent->data.control.param <=
                NUM_VIRTUAL_TRACK_CONTROLS * CS10_NUM_PHYSICAL_TRACKS)) {
            unsigned int event_track =
              (pNewEvent->data.control.param / NUM_VIRTUAL_TRACK_CONTROLS) +
              ((pNewEvent->data.control.channel - CS10_MIDI_CONTROL_CHANNEL) *
                CS10_NUM_PHYSICAL_TRACKS);
            unsigned int event_control =
              pNewEvent->data.control.param % NUM_VIRTUAL_TRACK_CONTROLS ;

            cs10_receive_virtual_control(event_track, 
              event_control, pNewEvent->data.control.value) ;
          } /* if */
        } /* SND_SEQ_EVENT_CONTROLLER */
      } /* iMMCPortID */

      if (pNewEvent->dest.port == cs10.iControlPortID) {
        if (SND_SEQ_EVENT_CONTROLLER == pNewEvent->type) {
          if ((FIRST_BUTTON_ADDR <= pNewEvent->data.control.param) &&
             (LAST_BUTTON_ADDR >= pNewEvent->data.control.param))
            cs10_handle_button(pNewEvent->data.control.param,
                               pNewEvent->data.control.value) ;
          else
          if ((FIRST_FADER_ADDR <= pNewEvent->data.control.param) &&
              (LAST_FADER_ADDR >= pNewEvent->data.control.param)) {
            cs10_handle_fader(pNewEvent->data.control.param,
                              pNewEvent->data.control.value) ;
          } else
          if ((FIRST_KNOB_ADDR <= pNewEvent->data.control.param) &&
              (LAST_KNOB_ADDR >= pNewEvent->data.control.param)) {
            cs10_handle_knob(pNewEvent->data.control.param,
                             pNewEvent->data.control.value) ;
          } else
          if (WHEEL_ADDR == pNewEvent->data.control.param) {
            cs10_handle_wheel(pNewEvent->data.control.value) ;
          } /* WHEEL_ADDR */
        } else { 
          /* pass on any non-controller events */
          snd_seq_ev_set_dest(pNewEvent, SND_SEQ_ADDRESS_SUBSCRIBERS, 0) ;
          snd_seq_ev_set_source(pNewEvent, cs10.iMMCPortID) ;
          snd_seq_ev_set_direct(pNewEvent) ;
          snd_seq_event_output(cs10.pSeq, pNewEvent) ;
          snd_seq_drain_output(cs10.pSeq) ;
        } /* if controller */
      } /* if msg to cs10 */
    } /* while */
  } /* pSeq */

  return 0 ;
} /* main */

