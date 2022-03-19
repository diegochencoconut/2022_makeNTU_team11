/*
 * Copyright 2019-2021 NXP.
 * This software is owned or controlled by NXP and may only be used strictly in accordance with the
 * license terms that accompany it. By expressly accepting such terms or by downloading, installing,
 * activating and/or otherwise using the software, you are agreeing that you have read, and that you
 * agree to comply with and are bound by, such license terms. If you do not agree to be bound by the
 * applicable license terms, then you may not retain, install, activate or otherwise use the software.
 */

#include <time.h>

/* Board includes */
#include "board.h"
#include "fsl_debug_console.h"
#include "pin_mux.h"

/* FreeRTOS kernel includes */
#include "FreeRTOS.h"
#include "task.h"

/* Driver includes */
#include "fsl_dmamux.h"
#include "fsl_edma.h"
#include "fsl_iomuxc.h"

/* RGB LED driver header */
#include "sln_RT10xx_RGB_LED_driver.h"

/* Shell includes */
#include "sln_shell.h"

/* Network includes */
#include "sln_tcp_server.h"

/* Application headers */
#include "sln_local_voice.h"
#include "switch.h"

/* Flash includes */
#include "sln_flash.h"
#include "sln_file_table.h"

/* Crypto includes */
#include "ksdk_mbedtls.h"

/* Audio processing includes */
#include "audio_samples.h"
#include "audio_processing_task.h"
#include "pdm_to_pcm_task.h"
#include "sln_amplifier.h"
#include "pdm_pcm_definitions.h"

#include "clock_config.h"
/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define pdm_to_pcm_task_PRIORITY       (configMAX_PRIORITIES - 2)
#define audio_processing_task_PRIORITY (configMAX_PRIORITIES - 1)

#if defined(SLN_LOCAL2_RD)
#define audio_play_task_NAME     "AudioPlay"
#define audio_play_task_PRIORITY 4
#define audio_play_task_STACK    512

#define STREAMER_PACKET_SIZE PCM_AMP_DATA_SIZE_20_MS
#endif

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
void SysTick_DelayTicks(uint32_t n);
static void next_demo();
/*******************************************************************************
 * Variables
 ******************************************************************************/
TaskHandle_t appTaskHandle              = NULL;
TaskHandle_t xAudioProcessingTaskHandle = NULL;
TaskHandle_t xPdmToPcmTaskHandle        = NULL;
TaskHandle_t appInitDummyNullHandle     = NULL;
bool taskPlaying;
uint8_t isRecording;
bool g_SW1Pressed                   = false;
oob_demo_control_t oob_demo_control = {0};
extern app_asr_shell_commands_t appAsrShellCommands;
int g_bypass_voice_engine = 0;

#if defined(SLN_LOCAL2_RD)
static uint8_t s_streamerPoolIdx          = 0;
static volatile uint8_t s_streamerPoolCnt = AMP_WRITE_SLOTS;
static uint8_t s_streamerPools[STREAMER_PACKET_SIZE * AMP_WRITE_SLOTS];

static SemaphoreHandle_t s_audioPlayMutex;
static uint8_t s_audioIsPlaying;
#endif

/*******************************************************************************
 * Callbacks
 ******************************************************************************/
void switch_callback(int32_t button_nr, int32_t state)
{
    if (button_nr == SWITCH_SW1)
    {
        if (state == SWITCH_PRESSED)
        {
            g_SW1Pressed = true;
        }
        else
        {
            g_SW1Pressed = false;
        }
    }

    if (button_nr == SWITCH_SW2)
    {
        if (state == SWITCH_PRESSED)
        {
            next_demo();
        }
    }
}

/*******************************************************************************
 * Code
 ******************************************************************************/

void SysTick_DelayTicks(uint32_t n)
{
    vTaskDelay(n);
}

#if defined(SLN_LOCAL2_RD)
static void audio_play_task(void *arg)
{
    uint8_t *sound           = ((audio_file_t *)arg)->fileAddr;
    uint32_t soundSize       = ((audio_file_t *)arg)->fileSize;
    uint32_t soundDataPlayed = 0;
    uint32_t packetSize      = 0;

    s_streamerPoolCnt = AMP_WRITE_SLOTS;
    s_streamerPoolIdx = 0;

    while ((soundSize - soundDataPlayed) >= STREAMER_PACKET_SIZE)
    {
        /* In case there is an empty slot, send a new packet to the Amplifier */
        if (s_streamerPoolCnt > 0)
        {
            if (soundSize - soundDataPlayed >= STREAMER_PACKET_SIZE)
            {
                packetSize = STREAMER_PACKET_SIZE;
            }
            else
            {
                packetSize = soundSize - soundDataPlayed;
            }

            memcpy(&s_streamerPools[s_streamerPoolIdx * STREAMER_PACKET_SIZE], &sound[soundDataPlayed], packetSize);

            if (SLN_AMP_WriteNoWait(&s_streamerPools[s_streamerPoolIdx * STREAMER_PACKET_SIZE], packetSize) !=
                kStatus_Success)
            {
                configPRINTF(("[WARNING] The sound could not be played. AMP error.\r\n"));
            }

            s_streamerPoolCnt--;

            // to prevent false positive while playing audio when Speaker and mics are close. 2 for 16bit, 3 for 48Khz
            // to 16KHz
            g_bypass_voice_engine += packetSize / (2 * 3);

            s_streamerPoolIdx = (s_streamerPoolIdx + 1) % AMP_WRITE_SLOTS;
            soundDataPlayed += packetSize;
        }
    }

    xSemaphoreTake(s_audioPlayMutex, portMAX_DELAY);
    s_audioIsPlaying = 0;
    xSemaphoreGive(s_audioPlayMutex);

    vTaskDelete(NULL);
}

static status_t audio_play_clip(uint8_t *fileAddr, uint32_t fileSize)
{
    static audio_file_t audio_file;
    status_t status = kStatus_Success;

    xSemaphoreTake(s_audioPlayMutex, portMAX_DELAY);
    if (s_audioIsPlaying)
    {
        configPRINTF(("[WARNING] Previous audio is still playing.\r\n"));
        status = kStatus_Fail;
    }
    else
    {
        s_audioIsPlaying = 1;
    }
    xSemaphoreGive(s_audioPlayMutex);

    if (status == kStatus_Success)
    {
        audio_file.fileAddr = fileAddr;
        audio_file.fileSize = fileSize;

        if (xTaskCreate(audio_play_task, audio_play_task_NAME, audio_play_task_STACK, (void *)(&audio_file),
                        audio_play_task_PRIORITY, NULL) != pdPASS)
        {
            PRINTF("xTaskCreate audio_play_task failed!\r\n");

            xSemaphoreTake(s_audioPlayMutex, portMAX_DELAY);
            s_audioIsPlaying = 0;
            xSemaphoreGive(s_audioPlayMutex);

            status = kStatus_Fail;
        }
    }

    return status;
}
#elif defined(SLN_LOCAL2_IOT)
static status_t audio_play_clip(const char *file)
{
    uint8_t *audio;
    uint32_t audio_len;
    status_t status = kStatus_Success;

    if (file == NULL)
    {
        configPRINTF(("[WARNING] The sound file name is NULL.\r\n"));
        status = kStatus_Fail;
    }
    else
    {
        if ((status = SLN_FLASH_MGMT_ReadDataPtr(file, (const uint8_t **)&audio, &audio_len)) != kStatus_Success)
        {
            configPRINTF(("[WARNING] The sound could not be read from flash.\r\n"));
        }
        else if ((status = SLN_AMP_Write(audio, audio_len)) != kStatus_Success)
        {
            configPRINTF(("[WARNING] The sound could not be played. AMP error.\r\n"));
        }

        // to prevent false positive while playing audio when Speaker and mics are close. 2 for 16bit, 3 for 48Khz to
        // 16KHz
        g_bypass_voice_engine += audio_len / (2 * 3);
    }

    return status;
}
#endif

static void next_demo()
{
    // Only switch the demo when the last demo switch was processed
    if (!(appAsrShellCommands.asrCfg & ASR_CFG_CMD_INFERENCE_ENGINE_CHANGED) &&
        !(appAsrShellCommands.asrCfg & ASR_CFG_DEMO_LANGUAGE_CHANGED))
    {
        switch (appAsrShellCommands.demo)
        {
            case ASR_CMD_LED:
                appAsrShellCommands.demo         = ASR_CMD_IOT;
                appAsrShellCommands.multilingual = ASR_ALL_LANG;
                break;
            case ASR_CMD_IOT:
                appAsrShellCommands.demo         = ASR_CMD_DIALOGIC_1;
                appAsrShellCommands.multilingual = ASR_ENGLISH;
                break;
            case ASR_CMD_DIALOGIC_1:
                appAsrShellCommands.demo         = ASR_CMD_LED;
                appAsrShellCommands.multilingual = ASR_ENGLISH;
                break;

            default:
                appAsrShellCommands.demo         = ASR_CMD_DIALOGIC_1;
                appAsrShellCommands.multilingual = ASR_ENGLISH;
                break;
        }

        appAsrShellCommands.status = WRITE_READY;
        appAsrShellCommands.asrCfg |= ASR_CFG_CMD_INFERENCE_ENGINE_CHANGED | ASR_CFG_DEMO_LANGUAGE_CHANGED;

        // Notify the app task to start processing the new demo configuration
        xTaskNotifyFromISR(appTaskHandle, kDefault, eSetBits, NULL);
    }
}

static status_t demo_play_clip(uint8_t *sound, uint32_t soundSize)
{
    status_t ret = kStatus_Success;

#if defined(SLN_LOCAL2_RD)
    ret = audio_play_clip((uint8_t *)sound, soundSize);
#elif defined(SLN_LOCAL2_IOT)
    ret = SLN_AMP_Write((uint8_t *)sound, soundSize);
#endif
    return ret;
}

static status_t announce_demo(asr_inference_t demo)
{
    status_t ret = kStatus_Success;

    switch (demo)
    {
        case ASR_CMD_AUDIO:
            ret = demo_play_clip((uint8_t *)audio_demo_clip, sizeof(audio_demo_clip));
            break;
        case ASR_CMD_IOT:
            ret = demo_play_clip((uint8_t *)smart_home_demo_clip, sizeof(smart_home_demo_clip));
            break;
        case ASR_CMD_ELEVATOR:
            ret = demo_play_clip((uint8_t *)elevator_demo_clip, sizeof(elevator_demo_clip));
            break;
        case ASR_CMD_WASH:
            ret = demo_play_clip((uint8_t *)wash_demo_clip, sizeof(wash_demo_clip));
            break;
        case ASR_CMD_LED:
            ret = demo_play_clip((uint8_t *)led_demo_clip, sizeof(led_demo_clip));
            break;
        case ASR_CMD_DIALOGIC_1:
            ret = demo_play_clip((uint8_t *)dialog_demo_clip, sizeof(dialog_demo_clip));
            break;
        default:
            PRINTF("No such demo!\r\n");
            break;
    }

    return ret;
}

static void led_state_success(void)
{
    RGB_LED_SetColor(LED_COLOR_OFF);
    RGB_LED_SetColor(LED_COLOR_GREEN);
    vTaskDelay(200);
    RGB_LED_SetColor(LED_COLOR_OFF);
}

static void led_state_error(void)
{
    RGB_LED_SetColor(LED_COLOR_OFF);
    RGB_LED_SetColor(LED_COLOR_RED);
}

static void led_state_listening(void)
{
    RGB_LED_SetColor(LED_COLOR_OFF);
    RGB_LED_SetColor(LED_COLOR_BLUE);
}

static void led_state_timeout(void)
{
    RGB_LED_SetColor(LED_COLOR_OFF);
    RGB_LED_SetColor(LED_COLOR_PURPLE);
    vTaskDelay(200);
    RGB_LED_SetColor(LED_COLOR_OFF);
}

void appTask(void *arg)
{
    amplifier_status_t ret;
    uint32_t statusFlash  = 0;

    sln_shell_set_app_init_task_handle(&appInitDummyNullHandle);
#if defined(SLN_LOCAL2_RD)
    s_audioPlayMutex = xSemaphoreCreateMutex();

    ret = SLN_AMP_Init(&s_streamerPoolCnt);
#elif defined(SLN_LOCAL2_IOT)
    ret = SLN_AMP_Init(NULL);
#endif
    if (ret != kStatus_Success)
    {
        PRINTF("SLN_AMP_Init failed!\r\n");
    }

    audio_processing_set_app_task_handle(&appTaskHandle);

    int16_t *micBuf = pdm_to_pcm_get_pcm_output();
    audio_processing_set_mic_input_buffer(&micBuf);

    int16_t *ampBuf = pdm_to_pcm_get_amp_output();
    audio_processing_set_amp_input_buffer(&ampBuf);

    audio_processing_set_task_handle(&xAudioProcessingTaskHandle);

    // Create audio processing task
    if (xTaskCreate(audio_processing_task, "Audio_processing_task", 1536U, NULL, audio_processing_task_PRIORITY,
                    &xAudioProcessingTaskHandle) != pdPASS)
    {
        PRINTF("Audio processing task creation failed!\r\n");
        RGB_LED_SetColor(LED_COLOR_RED);
        vTaskDelete(NULL);
    }

    // Set PDM to PCM config
    pcm_pcm_task_config_t config = {0};
    config.thisTask              = &xPdmToPcmTaskHandle;
    config.processingTask        = &xAudioProcessingTaskHandle;
    config.feedbackEnable        = SLN_AMP_LoopbackEnable;
    config.feedbackDisable       = SLN_AMP_LoopbackDisable;
#if USE_TFA
    config.feedbackInit   = SLN_AMP_Read;
    config.feedbackBuffer = (int16_t *)SLN_AMP_GetLoopBackBuffer();
#elif USE_MQS
    config.loopbackRingBuffer = SLN_AMP_GetRingBuffer();
    config.loopbackMutex      = SLN_AMP_GetLoopBackMutex();
    config.updateTimestamp    = SLN_AMP_UpdateTimestamp;
    config.getTimestamp       = SLN_AMP_GetTimestamp;
#endif

    pcm_to_pcm_set_config(&config);

#if USE_TFA
    // Set loopback event bit for AMP
    EventBits_t loopBackEvent = pdm_to_pcm_get_amp_loopback_event();
    SLN_AMP_SetLoopBackEventBits(loopBackEvent);
    SLN_AMP_SetLoopBackErrorEventBits(pdm_to_pcm_get_amp_loopback_error_event());
#endif /* USE_TFA */

    // Set default sound playback and use it as an initial chime
    SLN_AMP_SetDefaultAudioData((uint8_t *)tone_boot, sizeof(tone_boot));

    // Create pdm to pcm task
    if (xTaskCreate(pdm_to_pcm_task, "pdm_to_pcm_task", 1024U, NULL, pdm_to_pcm_task_PRIORITY, &xPdmToPcmTaskHandle) !=
        pdPASS)
    {
        PRINTF("PDM to PCM processing task creation failed!\r\n");
        RGB_LED_SetColor(LED_COLOR_RED);
        vTaskDelete(NULL);
    }

    pdm_to_pcm_set_task_handle(&xPdmToPcmTaskHandle);

#if USE_TFA
    // Pass loopback event group to AMP
    EventGroupHandle_t ampLoopBackEventGroup = NULL;
    while (1)
    {
        ampLoopBackEventGroup = pdm_to_pcm_get_event_group();
        if (ampLoopBackEventGroup != NULL)
        {
            break;
        }
        vTaskDelay(1);
    }
    SLN_AMP_SetLoopBackEventGroup(&ampLoopBackEventGroup);
#endif /* USE_TFA */

    RGB_LED_SetColor(LED_COLOR_OFF);

    SLN_AMP_SetVolume(appAsrShellCommands.volume);
    announce_demo(appAsrShellCommands.demo);

    if (ret != kStatus_Success)
    {
        PRINTF("Could not play default sound!\r\n");
        led_state_error();
    }

    if (appAsrShellCommands.mute == ASR_MUTE_ON)
    {
        // close mics and amp feedback loop
        pdm_to_pcm_mics_off();
        // show the user that device does not respond
        RGB_LED_SetColor(LED_COLOR_ORANGE);
    }
    else if (appAsrShellCommands.ptt == ASR_PTT_ON)
    {
        configPRINTF(
            ("ASR Push-To-Talk mode is enabled. English only is active.\r\n"
             "Press SW1 to input a command or disable PTT mode.\r\n"));
        // show the user that device will wake up only on SW1 press
        RGB_LED_SetColor(LED_COLOR_CYAN);
    }

    uint32_t u32UniqueIDRaw1 = (uint32_t)OCOTP->CFG1;
    uint32_t u32UniqueIDRaw0 = (uint32_t)OCOTP->CFG0;
    configPRINTF(("\r\nu64UniqueIDRaw: 0x%x 0x%x.\r\n", u32UniqueIDRaw1, u32UniqueIDRaw0));
    PRINTF("\r\nu64UniqueIDRaw: 0x%x 0x%x.\r\n", u32UniqueIDRaw1, u32UniqueIDRaw0);

    // Initialize the buttons
    SWITCH_Init();
    SWITCH_RegisterCallback(switch_callback);

    uint32_t taskNotification = 0;
    while (1)
    {
        xTaskNotifyWait(0xffffffffU, 0xffffffffU, &taskNotification, portMAX_DELAY);

        switch (taskNotification)
        {
            case kMicUpdate:
            {
                if (appAsrShellCommands.mute == ASR_MUTE_ON)
                {
                    // close mics and amp feedback loop
                    pdm_to_pcm_mics_off();
                    // show the user that device does not respond
                    RGB_LED_SetColor(LED_COLOR_ORANGE);
                }
                else
                {
                    // reopen mics and amp feedback loop
                    pdm_to_pcm_mics_on();
                    // turn off the orange led
                    RGB_LED_SetColor(LED_COLOR_OFF);
                }
                break;
            }
            case kVolumeUpdate:
            {
                SLN_AMP_SetVolume(appAsrShellCommands.volume);
                break;
            }
            case kWakeWordDetected:
            {
                if (oob_demo_control.language == ASR_ENGLISH)
                {
// play audio "can i help you?" in English
#if defined(SLN_LOCAL2_RD)
                    ret = audio_play_clip((uint8_t *)AUDIO_EN_02_FILE_ADDR, AUDIO_EN_02_FILE_SIZE);
#elif defined(SLN_LOCAL2_IOT)
                    ret = audio_play_clip(AUDIO_EN_02_FILE);
#endif
                }
                else if (oob_demo_control.language == ASR_CHINESE)
                {
// play audio "can i help you?" in Chinese
#if defined(SLN_LOCAL2_RD)
                    ret = audio_play_clip((uint8_t *)AUDIO_ZH_02_FILE_ADDR, AUDIO_ZH_02_FILE_SIZE);
#elif defined(SLN_LOCAL2_IOT)
                    ret = audio_play_clip(AUDIO_ZH_02_FILE);
#endif
                }
                else if (oob_demo_control.language == ASR_GERMAN)
                {
// play audio "can i help you?" in German
#if defined(SLN_LOCAL2_RD)
                    ret = audio_play_clip((uint8_t *)AUDIO_DE_02_FILE_ADDR, AUDIO_DE_02_FILE_SIZE);
#elif defined(SLN_LOCAL2_IOT)
                    ret = audio_play_clip(AUDIO_DE_02_FILE);
#endif
                }
                else if (oob_demo_control.language == ASR_FRENCH)
                {
// play audio "can i help you?" in French
#if defined(SLN_LOCAL2_RD)
                    ret = audio_play_clip((uint8_t *)AUDIO_FR_02_FILE_ADDR, AUDIO_FR_02_FILE_SIZE);
#elif defined(SLN_LOCAL2_IOT)
                    ret = audio_play_clip(AUDIO_FR_02_FILE);
#endif
                }
                else
                {
                    configPRINTF(("undefined language to play.\r\n"));
                    ret = kStatus_Fail;
                }

                if (ret != kStatus_Success)
                {
                    PRINTF("Could not play wake word detected sound!\r\n");
                    led_state_error();
                }
                else
                {
                    led_state_listening();
                }
                break;
            }
            case kCommandLED:
            {
// play audio "OK" in English Accent
#if defined(SLN_LOCAL2_RD)
                ret = audio_play_clip((uint8_t *)AUDIO_EN_01_FILE_ADDR, AUDIO_EN_01_FILE_SIZE);
#elif defined(SLN_LOCAL2_IOT)
                ret = audio_play_clip(AUDIO_EN_01_FILE);
#endif
                if (ret != kStatus_Success)
                {
                    PRINTF("Could not play command LED sound!\r\n");
                    led_state_error();
                }
                else
                {
                    RGB_LED_SetColor(LED_COLOR_OFF);

                    if (oob_demo_control.ledCmd == LED_RED)
                    {
                        RGB_LED_SetColor(LED_COLOR_RED);
                        vTaskDelay(300);
                    }
                    else if (oob_demo_control.ledCmd == LED_GREEN)
                    {
                        RGB_LED_SetColor(LED_COLOR_GREEN);
                        vTaskDelay(300);
                    }
                    else if (oob_demo_control.ledCmd == LED_BLUE)
                    {
                        RGB_LED_SetColor(LED_COLOR_BLUE);
                        vTaskDelay(300);
                    }
                    else if (oob_demo_control.ledCmd == CYCLE_FAST)
                    {
                        for (int i = 0; i < 3; i++)
                        {
                            RGB_LED_SetColor(LED_COLOR_RED);
                            vTaskDelay(200);
                            RGB_LED_SetColor(LED_COLOR_OFF);
                            RGB_LED_SetColor(LED_COLOR_GREEN);
                            vTaskDelay(200);
                            RGB_LED_SetColor(LED_COLOR_OFF);
                            RGB_LED_SetColor(LED_COLOR_BLUE);
                            vTaskDelay(200);
                        }
                    }
                    else if (oob_demo_control.ledCmd == CYCLE_SLOW)
                    {
                        for (int i = 0; i < 3; i++)
                        {
                            RGB_LED_SetColor(LED_COLOR_RED);
                            vTaskDelay(400);
                            RGB_LED_SetColor(LED_COLOR_OFF);
                            RGB_LED_SetColor(LED_COLOR_GREEN);
                            vTaskDelay(400);
                            RGB_LED_SetColor(LED_COLOR_OFF);
                            RGB_LED_SetColor(LED_COLOR_BLUE);
                            vTaskDelay(400);
                        }
                    }
                    RGB_LED_SetColor(LED_COLOR_OFF);
                }
                break;
            }
            case kCommandGeneric:
            {
                if (oob_demo_control.language == ASR_ENGLISH)
                {
// play audio "OK" in English Accent
#if defined(SLN_LOCAL2_RD)
                    ret = audio_play_clip((uint8_t *)AUDIO_EN_01_FILE_ADDR, AUDIO_EN_01_FILE_SIZE);
#elif defined(SLN_LOCAL2_IOT)
                    ret = audio_play_clip(AUDIO_EN_01_FILE);
#endif
                }
                else if (oob_demo_control.language == ASR_CHINESE)
                {
// play audio "OK" in Chinese
#if defined(SLN_LOCAL2_RD)
                    ret = audio_play_clip((uint8_t *)AUDIO_ZH_01_FILE_ADDR, AUDIO_ZH_01_FILE_SIZE);
#elif defined(SLN_LOCAL2_IOT)
                    ret = audio_play_clip(AUDIO_ZH_01_FILE);
#endif
                }
                else if (oob_demo_control.language == ASR_GERMAN)
                {
// play audio "OK" in German Accent
#if defined(SLN_LOCAL2_RD)
                    ret = audio_play_clip((uint8_t *)AUDIO_DE_01_FILE_ADDR, AUDIO_DE_01_FILE_SIZE);
#elif defined(SLN_LOCAL2_IOT)
                    ret = audio_play_clip(AUDIO_DE_01_FILE);
#endif
                }
                else if (oob_demo_control.language == ASR_FRENCH)
                {
// play audio "OK" in French Accent
#if defined(SLN_LOCAL2_RD)
                    ret = audio_play_clip((uint8_t *)AUDIO_FR_01_FILE_ADDR, AUDIO_FR_01_FILE_SIZE);
#elif defined(SLN_LOCAL2_IOT)
                    ret = audio_play_clip(AUDIO_FR_01_FILE);
#endif
                }
                else
                {
                    configPRINTF(("undefined language to play.\r\n"));
                    ret = kStatus_Fail;
                }

                if (ret != kStatus_Success)
                {
                    PRINTF("Could not play command generic sound!\r\n");
                    led_state_error();
                }
                else
                {
                    led_state_success();
                }
                break;
            }
            case kCommandDialog:
                if (oob_demo_control.dialogRes == RESPONSE_1_TEMPERATURE)
                {
// play audio "say the temperature to be set" in English
#if defined(SLN_LOCAL2_RD)
                    ret = audio_play_clip((uint8_t *)AUDIO_EN_03_FILE_ADDR, AUDIO_EN_03_FILE_SIZE);
#elif defined(SLN_LOCAL2_IOT)
                    ret = audio_play_clip(AUDIO_EN_03_FILE);
#endif
                }
                else if (oob_demo_control.dialogRes == RESPONSE_1_TIMER)
                {
// play audio "say the time to be set" in English
#if defined(SLN_LOCAL2_RD)
                    ret = audio_play_clip((uint8_t *)AUDIO_EN_04_FILE_ADDR, AUDIO_EN_04_FILE_SIZE);
#elif defined(SLN_LOCAL2_IOT)
                    ret = audio_play_clip(AUDIO_EN_04_FILE);
#endif
                }
                else if (oob_demo_control.dialogRes == RESPONSE_2_TEMPERATURE)
                {
// play audio "temperature has been set" in English
#if defined(SLN_LOCAL2_RD)
                    ret = audio_play_clip((uint8_t *)AUDIO_EN_05_FILE_ADDR, AUDIO_EN_05_FILE_SIZE);
#elif defined(SLN_LOCAL2_IOT)
                    ret = audio_play_clip(AUDIO_EN_05_FILE);
#endif
                }
                else if (oob_demo_control.dialogRes == RESPONSE_2_TIMER)
                {
// play audio "timer has been set" in English
#if defined(SLN_LOCAL2_RD)
                    ret = audio_play_clip((uint8_t *)AUDIO_EN_06_FILE_ADDR, AUDIO_EN_06_FILE_SIZE);
#elif defined(SLN_LOCAL2_IOT)
                    ret = audio_play_clip(AUDIO_EN_06_FILE);
#endif
                }
                else if (oob_demo_control.dialogRes == RESPONSE_FINE)
				{

// play audio "timer has been set" in English
#if defined(SLN_LOCAL2_RD)
					ret = audio_play_clip((uint8_t *)AUDIO_EN_06_FILE_ADDR, AUDIO_EN_06_FILE_SIZE);
#elif defined(SLN_LOCAL2_IOT)
					ret = audio_play_clip(AUDIO_EN_06_FILE);
#endif
                }

                if (ret != kStatus_Success)
                {
                    PRINTF("Could not play command dialog sound!\r\n");
                    led_state_error();
                }
                else
                {
                    led_state_success();
                }
                break;
            case kTimeOut:
                // play timeout tone
#if defined(SLN_LOCAL2_RD)
                ret = audio_play_clip((uint8_t *)tone_timeout, sizeof(tone_timeout));
#elif defined(SLN_LOCAL2_IOT)
                ret = SLN_AMP_Write((uint8_t *)tone_timeout, sizeof(tone_timeout));
#endif
                if (ret != kStatus_Success)
                {
                    PRINTF("Could not play default sound!\r\n");
                    led_state_error();
                }
                else
                {
                    led_state_timeout();
                }

                break;
            default:
                break;
        }

        if (appAsrShellCommands.ptt == ASR_PTT_ON)
        {
            // show the user that device will wake up only on SW1 press
            RGB_LED_SetColor(LED_COLOR_CYAN);
        }

        if (appAsrShellCommands.asrCfg & ASR_CFG_CMD_INFERENCE_ENGINE_CHANGED)
        {
            announce_demo(appAsrShellCommands.demo);
            appAsrShellCommands.asrCfg &= ~ASR_CFG_CMD_INFERENCE_ENGINE_CHANGED;
        }

        // check the status of shell commands and flash file system
        if (appAsrShellCommands.status == WRITE_READY)
        {
            appAsrShellCommands.status = WRITE_SUCCESS;
            statusFlash = SLN_FLASH_MGMT_Save(ASR_SHELL_COMMANDS_FILE_NAME, (uint8_t *)&appAsrShellCommands,
                                              sizeof(app_asr_shell_commands_t));
            if ((statusFlash == SLN_FLASH_MGMT_EOVERFLOW) || (statusFlash == SLN_FLASH_MGMT_EOVERFLOW2))
            {
                statusFlash = SLN_FLASH_MGMT_Erase(ASR_SHELL_COMMANDS_FILE_NAME);
                statusFlash = SLN_FLASH_MGMT_Save(ASR_SHELL_COMMANDS_FILE_NAME, (uint8_t *)&appAsrShellCommands,
                                                  sizeof(app_asr_shell_commands_t));
                if (statusFlash != SLN_FLASH_MGMT_OK)
                {
                    configPRINTF(("Failed to write in flash memory.\r\n"));
                }
            }
            else if (statusFlash != SLN_FLASH_MGMT_OK)
            {
                configPRINTF(("Failed to write in flash memory.\r\n"));
            }

            if (statusFlash == SLN_FLASH_MGMT_OK)
            {
                configPRINTF(("Updated Shell command parameter in flash memory.\r\n"));
            }
        }

        taskNotification = 0;
    }
}

/*!
 * @brief Main function
 */
void main(void)
{
    /* Enable additional fault handlers */
    SCB->SHCSR |= (SCB_SHCSR_BUSFAULTENA_Msk | /*SCB_SHCSR_USGFAULTENA_Msk |*/ SCB_SHCSR_MEMFAULTENA_Msk);

    /* Init board hardware */
    /* Relocate Vector Table */
#if RELOCATE_VECTOR_TABLE
    BOARD_RelocateVectorTableToRam();
#endif

    BOARD_ConfigMPU();
    BOARD_InitBootPins();
    BOARD_BootClockRUN();

    /* Setup Crypto HW */
    CRYPTO_InitHardware();

    /* Set flash management callbacks */
    sln_flash_mgmt_cbs_t flash_mgmt_cbs = {pdm_to_pcm_mics_off, pdm_to_pcm_mics_on};
    SLN_FLASH_MGMT_SetCbs(&flash_mgmt_cbs);

    /* Initialize Flash to allow writing */
    SLN_Flash_Init();

    /* Initialize flash management */
    SLN_FLASH_MGMT_Init((sln_flash_entry_t *)g_fileTable, false);

    /*
     * AUDIO PLL setting: Frequency = Fref * (DIV_SELECT + NUM / DENOM)
     *                              = 24 * (32 + 77/100)
     *                              = 786.48 MHz
     */
    const clock_audio_pll_config_t audioPllConfig = {
        .loopDivider = 32,  /* PLL loop divider. Valid range for DIV_SELECT divider value: 27~54. */
        .postDivider = 1,   /* Divider after the PLL, should only be 1, 2, 4, 8, 16. */
        .numerator   = 77,  /* 30 bit numerator of fractional loop divider. */
        .denominator = 100, /* 30 bit denominator of fractional loop divider */
    };

    CLOCK_InitAudioPll(&audioPllConfig);

    CLOCK_SetMux(kCLOCK_Sai1Mux, BOARD_PDM_SAI_CLOCK_SOURCE_SELECT);
    CLOCK_SetDiv(kCLOCK_Sai1PreDiv, BOARD_PDM_SAI_CLOCK_SOURCE_PRE_DIVIDER);
    CLOCK_SetDiv(kCLOCK_Sai1Div, BOARD_PDM_SAI_CLOCK_SOURCE_DIVIDER);
    CLOCK_EnableClock(kCLOCK_Sai1);

    CLOCK_SetMux(kCLOCK_Sai2Mux, BOARD_PDM_SAI_CLOCK_SOURCE_SELECT);
    CLOCK_SetDiv(kCLOCK_Sai2PreDiv, BOARD_PDM_SAI_CLOCK_SOURCE_PRE_DIVIDER);
    CLOCK_SetDiv(kCLOCK_Sai2Div, BOARD_PDM_SAI_CLOCK_SOURCE_DIVIDER);
    CLOCK_EnableClock(kCLOCK_Sai2);

    edma_config_t dmaConfig = {0};

    EDMA_GetDefaultConfig(&dmaConfig);
    EDMA_Init(DMA0, &dmaConfig);

    DMAMUX_Init(DMAMUX);

    RGB_LED_Init();
    RGB_LED_SetColor(LED_COLOR_GREEN);

    sln_shell_init();

    TCP_OTA_Server_Start();

    xTaskCreate(appTask, "APP_Task", 512, NULL, configMAX_PRIORITIES - 4, &appTaskHandle);
    xTaskCreate(sln_shell_task, "Shell_Task", 1024, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(local_voice_task, "Local_Voice_Task", 4096, NULL, configMAX_PRIORITIES - 4, NULL);

    /* Run RTOS */
    vTaskStartScheduler();

    /* Should not reach this statement */
    while (1)
        ;
}
