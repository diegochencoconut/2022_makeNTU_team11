/*
 * Copyright 2021 NXP.
 * This software is owned or controlled by NXP and may only be used strictly in accordance with the
 * license terms that accompany it. By expressly accepting such terms or by downloading, installing,
 * activating and/or otherwise using the software, you are agreeing that you have read, and that you
 * agree to comply with and are bound by, such license terms. If you do not agree to be bound by the
 * applicable license terms, then you may not retain, install, activate or otherwise use the software.d
 */

#ifndef SLN_LOCAL_VOICE_H_
#define SLN_LOCAL_VOICE_H_

#include <stdint.h>
#include <string.h>
#include "sln_asr.h"

#if defined(__cplusplus)
extern "C" {
#endif

// languages
#define MULTILINGUAL            (1)
#define IMXRT105S               (0) // Not supported yet
#define MAX_INSTALLED_LANGUAGES (4)

#if MULTILINGUAL
#if defined(SLN_LOCAL2_RD)
#define MAX_CONCURRENT_LANGUAGES 3
#elif defined(SLN_LOCAL2_IOT)
#define MAX_CONCURRENT_LANGUAGES 4
#endif // defined(SLN_LOCAL2_RD)
#else
#define MAX_CONCURRENT_LANGUAGES (1)
#endif // MULTILINGUAL

// applications
#define ENABLE_IOT                    (1)
#define ENABLE_ELEVATOR               (1)
#define ENABLE_AUDIO                  (1)
#define ENABLE_WASH                   (1)
#define ENABLE_LED                    (1)
#define ENABLE_DIALOGIC_1             (1)
#define ENABLE_DIALOGIC_2_TEMPERATURE (1)
#define ENABLE_DIALOGIC_2_TIMER       (1)
#define ENABLE_NORMAL                 (1)
#define ENABLE_CONDITION              (1)
#define ENABLE_TEMPERATURE            (1)
#define ENABLE_FLOAT_NUM              (1)
#define ENABLE_CONFIRM                (1)
#define ENABLE_MEAL                   (1)
#define ENABLE_CONFIRM_MEAL           (1)
#define NUM_APPS_EN                                                                               \
    (ENABLE_IOT + ENABLE_ELEVATOR + ENABLE_AUDIO + ENABLE_WASH + ENABLE_LED + ENABLE_DIALOGIC_1 + \
     ENABLE_DIALOGIC_2_TEMPERATURE + ENABLE_DIALOGIC_2_TIMER) // LED and Dialog demos are with only English
#define NUM_APPS (ENABLE_IOT + ENABLE_ELEVATOR + ENABLE_AUDIO + ENABLE_WASH)
//uhua
#define NUM_APPS_ZH                   (14)
//uhua

// groups: base, ww, cmd_iot, cmd_elevator, and so on
#define NUM_GROUPS_EN (NUM_APPS_EN + 3) // add 1 for base group, add 1 for wake word, add 1 for the mapID
#define NUM_GROUPS    (NUM_APPS + 3)
#define NUM_GROUPS_ZH (18)
#define MAX_GROUPS    (18)
//((NUM_GROUPS > NUM_GROUPS_EN) ? NUM_GROUPS : NUM_GROUPS_EN)

#define NUM_INFERENCES_WW (MAX_INSTALLED_LANGUAGES) // WW in multiple languages

#define k_nMaxTime (300)

#define TIMEOUT_TIME_IN_MS 8000 // the response waiting time in ASR session

// Shell Commands Related
#define ASR_SHELL_COMMANDS_FILE_NAME "asr_shell_commands.dat"

#if defined(SLN_LOCAL2_RD)
// Addresses of the sound files
#define AUDIO_EN_01_FILE_ADDR 0x60E00000
#define AUDIO_EN_02_FILE_ADDR 0x60E25000
#define AUDIO_EN_03_FILE_ADDR 0x60F28000
#define AUDIO_EN_04_FILE_ADDR 0x60F4D000
#define AUDIO_EN_05_FILE_ADDR 0x60F72000
#define AUDIO_EN_06_FILE_ADDR 0x60F97000

#define AUDIO_ZH_01_FILE_ADDR 0x60E4A000
#define AUDIO_ZH_02_FILE_ADDR 0x60E6F000

#define AUDIO_DE_01_FILE_ADDR 0x60E94000
#define AUDIO_DE_02_FILE_ADDR 0x60EB9000

#define AUDIO_FR_01_FILE_ADDR 0x60EDE000
#define AUDIO_FR_02_FILE_ADDR 0x60F03000

// Sizes of the sound files
#define AUDIO_EN_01_FILE_SIZE 46220
#define AUDIO_EN_02_FILE_SIZE 68780
#define AUDIO_EN_03_FILE_SIZE 149324
#define AUDIO_EN_04_FILE_SIZE 127532
#define AUDIO_EN_05_FILE_SIZE 134592
#define AUDIO_EN_06_FILE_SIZE 124940

#define AUDIO_ZH_01_FILE_SIZE 35276
#define AUDIO_ZH_02_FILE_SIZE 88364

#define AUDIO_DE_01_FILE_SIZE 48908
#define AUDIO_DE_02_FILE_SIZE 110348

#define AUDIO_FR_01_FILE_SIZE 35904
#define AUDIO_FR_02_FILE_SIZE 75596
#elif defined(SLN_LOCAL2_IOT)
#define AUDIO_EN_01_FILE "audio_en_01_begin.dat"
#define AUDIO_EN_02_FILE "audio_en_02_begin.dat"
#define AUDIO_EN_03_FILE "audio_en_03_begin.dat"
#define AUDIO_EN_04_FILE "audio_en_04_begin.dat"
#define AUDIO_EN_05_FILE "audio_en_05_begin.dat"
#define AUDIO_EN_06_FILE "audio_en_06_begin.dat"

#define AUDIO_ZH_01_FILE "audio_zh_01_begin.dat"
#define AUDIO_ZH_02_FILE "audio_zh_02_begin.dat"

#define AUDIO_DE_01_FILE "audio_de_01_begin.dat"
#define AUDIO_DE_02_FILE "audio_de_02_begin.dat"

#define AUDIO_FR_01_FILE "audio_fr_01_begin.dat"
#define AUDIO_FR_02_FILE "audio_fr_02_begin.dat"
#endif

#if defined(SLN_LOCAL2_RD)
// Structure used for sending audio file data to the task that plays it
typedef struct audio_file
{
    uint8_t *fileAddr;
    uint32_t fileSize;
} audio_file_t;
#endif

// Out-of-box demo languages. Developers can add more language. Note that the runtime max number is up to four
// languages.
typedef enum _asr_languages
{
    UNDEFINED_LANGUAGE = 0,
    ASR_ENGLISH        = (1U << 0U),
    ASR_CHINESE        = (1U << 1U),
    ASR_GERMAN         = (1U << 2U),
    ASR_FRENCH         = (1U << 3U),
#if defined(SLN_LOCAL2_IOT)
    ASR_ALL_LANG = (ASR_ENGLISH | ASR_CHINESE | ASR_GERMAN | ASR_FRENCH)
#elif defined(SLN_LOCAL2_RD)
    ASR_ALL_LANG = (ASR_ENGLISH | ASR_GERMAN | ASR_FRENCH)
#endif
    // DEVELOPERS: add more languages here with the form ASR_XXXXX
} asr_language_t;

// ASR events. Currently only ASR_SESSION_STARTED and ASR_SESSION_ENDED are used.
typedef enum _asr_events
{
    ASR_SESSION_STARTED,
    ASR_SESSION_ENDED,
    //    ASR_COMMAND_DETECTED,
    //    ASR_COMMAND_NOT_RECOGNIZED,
    ASR_SESSION_TIMEOUT,
    //    ASR_CANCELLED,
} asr_events_t;

// ASR inference engines that cover
// 1) wake word engine,
// 2) LED control (demo #1),
// 3) voice commands in multiple languages (demo #2),
// 4) dialogic commands (demo #3)
typedef enum _asr_inference
{
    UNDEFINED_INFERENCE            = 0,
    ASR_WW                         = (1U << 0U),
    ASR_CMD_IOT                    = (ENABLE_IOT << 1U),
    ASR_CMD_ELEVATOR               = (ENABLE_ELEVATOR << 2U),
    ASR_CMD_AUDIO                  = (ENABLE_AUDIO << 3U),
    ASR_CMD_WASH                   = (ENABLE_WASH << 4U),
    ASR_CMD_LED                    = (ENABLE_LED << 5U),
    ASR_CMD_DIALOGIC_1             = (ENABLE_DIALOGIC_1 << 6U),
    ASR_CMD_DIALOGIC_2_TEMPERATURE = (ENABLE_DIALOGIC_2_TEMPERATURE << 7U),
    ASR_CMD_DIALOGIC_2_TIMER       = (ENABLE_DIALOGIC_2_TIMER << 8U),
	ASR_CMD_NORMAL                 = (ENABLE_NORMAL << 9U),
	ASR_CMD_CONDITION              = (ENABLE_CONDITION << 10U),
	ASR_CMD_TEMPERATURE            = (ENABLE_TEMPERATURE << 11U),
	ASR_CMD_FLOAT_NUM              = (ENABLE_FLOAT_NUM << 12U),
	ASR_CMD_CONFIRM                = (ENABLE_CONFIRM << 13U),
	ASR_CMD_MEAL                   = (ENABLE_MEAL << 14U),
	ASR_CMD_CONFIRM_MEAL           = (ENABLE_CONFIRM_MEAL << 15U)
} asr_inference_t;

// type for the LED control (demo #1) application.
// DEVELOPERS: customize this type and use it within oob_demo_control_t.
typedef enum _oob_led
{
    LED_RED = 0,
    LED_GREEN,
    LED_BLUE,
    CYCLE_FAST,
    CYCLE_SLOW,
    UNDEFINED_COMMAND
} oob_led_t;

typedef enum _oob_dialog
{
    RESPONSE_1_TEMPERATURE = 0,
    RESPONSE_1_TIMER,
    RESPONSE_2_TEMPERATURE,
    RESPONSE_2_TIMER,
	RESPONSE_FINE,
} oob_dialog_t;

typedef struct _oob_demo_control
{
    asr_language_t language;
    oob_led_t ledCmd;
    oob_dialog_t dialogRes;
    // DEVELOPERS: similarly with oob_led_t, add / customize each demo application
    // define oob_iot_t or oob_elevator_t or oob_audio_t or oob_wash_t for the demo #2 types
    // define oob_dialogic_cmd_t for the demo #3 type.
} oob_demo_control_t;

struct asr_language_model;   // will be used to install the selected languages.
struct asr_inference_engine; // will be used to install and set the selected WW/CMD inference engines.

struct asr_language_model
{
    asr_language_t iWhoAmI; // language types for language model. A model is language specific.
    uint8_t
        nGroups; // base, group1 (ww), group2 (commands set 1), group3 (commands set 2), ..., groupN (commands set N)
    unsigned char *addrBin;                        // model binary address
    unsigned char *addrGroup[MAX_GROUPS];          // addresses for base, group1, group2, ...
    unsigned char *addrGroupMapID[MAX_GROUPS - 1]; // addresses for mapIDs for group1, group2, ...
    struct asr_language_model *next;               // pointer to next language model in this linked list
};

struct asr_inference_engine
{
    asr_inference_t iWhoAmI_inf;   // inference types for WW engine or CMD engine
    asr_language_t iWhoAmI_lang;   // language for inference engine
    void *handler;                 // model handler
    uint8_t nGroups;               // the number of groups for an inference engine. Default is 2 and it's enough.
    unsigned char *addrGroup[2];   // base + keyword group. default nGroups is 2
    unsigned char *addrGroupMapID; // mapID group. default nGroups is 1
    char **idToKeyword;            // the string list
    unsigned char *memPool;        // memory pool in ram for inference engine
    uint32_t memPoolSize;          // memory pool size
    struct asr_inference_engine
        *next; // pointer to next inference engine, if this is linked list. The end of "next" should be NULL.
};

typedef struct _asr_control
{
    struct asr_language_model *langModel;      // linked list
    struct asr_inference_engine *infEngineWW;  // linked list
    struct asr_inference_engine *infEngineCMD; // not linked list
    uint32_t sampleCount;                      // to measure the waiting response time
    asr_result_t result;                       // results of the command processing
} asr_control_t;

typedef enum _app_flash_status
{
    READ_SUCCESS  = (1U << 0U),
    READ_FAIL     = (1U << 1U),
    READ_READY    = (1U << 2U),
    WRITE_SUCCESS = (1U << 3U),
    WRITE_FAIL    = (1U << 4U),
    WRITE_READY   = (1U << 5U),
} app_flash_status_t;

typedef enum _asr_mute
{
    ASR_MUTE_OFF = 0,
    ASR_MUTE_ON,
} asr_mute_t;

typedef enum _asr_followup
{
    ASR_FOLLOWUP_OFF = 0,
    ASR_FOLLOWUP_ON,
} asr_followup_t;

typedef enum _asr_ptt
{
    ASR_PTT_OFF = 0,
    ASR_PTT_ON,
} asr_ptt_t;

typedef enum _asr_cmd_res
{
    ASR_CMD_RES_OFF = 0,
    ASR_CMD_RES_ON,
} asr_cmd_res_t;

typedef enum _asr_cfg_demo
{
    ASR_CFG_DEMO_NO_CHANGE = (1U << 0U), // OOB demo type or languages unchanged
    ASR_CFG_CMD_INFERENCE_ENGINE_CHANGED =
        (1U << 1U),                             // OOB demo type (iot, elevator, audio, wash, led, dialog) changed
    ASR_CFG_DEMO_LANGUAGE_CHANGED = (1U << 2U), // OOB language type changed
} asr_cfg_demo_t;

typedef struct _app_asr_shell_commands
{
    app_flash_status_t status;
    asr_cfg_demo_t asrCfg;
    uint32_t volume; // 0 ~ 100
    asr_mute_t mute;
    uint32_t timeout; // in millisecond
    asr_followup_t followup;
    asr_inference_t demo;        // demo types: LED (demo #1) / iot, elevator, audio, wash (demo #2) / dialog (demo #3)
    asr_language_t multilingual; // runtime language types (demo #2 and #3)
    asr_ptt_t ptt;
    asr_cmd_res_t cmdresults;
} app_asr_shell_commands_t;

/////////////////////////////////////////////////

void local_voice_task(void *arg);

#if defined(__cplusplus)
}
#endif

#endif /* SLN_LOCAL_VOICE_H_ */
