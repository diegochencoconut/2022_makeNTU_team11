/*
 * Copyright 2018-2021 NXP.
 * This software is owned or controlled by NXP and may only be used strictly in accordance with the
 * license terms that accompany it. By expressly accepting such terms or by downloading, installing,
 * activating and/or otherwise using the software, you are agreeing that you have read, and that you
 * agree to comply with and are bound by, such license terms. If you do not agree to be bound by the
 * applicable license terms, then you may not retain, install, activate or otherwise use the software.d
 */

#include "audio_processing_task.h"

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "board.h"
#include "event_groups.h"
#include "limits.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

/* Freescale includes. */
#include "fsl_sai.h"
#include "fsl_sai_edma.h"
#include "pdm_pcm_definitions.h"

/* Local app include. */
#include "sln_local_voice.h"
#include "sln_dsp_toolbox.h"
#include "sln_afe.h"

/* LED include. */
#include "sln_RT10xx_RGB_LED_driver.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define RUN_GENERATED_TEST (0U)
#define BUFFER_SIZE        (PCM_SAMPLE_COUNT * 3)
#define BUFFER_NUM         (4)

#define AUDIO_QUEUE_NUM_ITEMS      75U
#define AUDIO_QUEUE_WATERMARK      15U
#define AUDIO_QUEUE_ITEM_LEN_BYTES (PCM_SAMPLE_SIZE_BYTES * PCM_SINGLE_CH_SMPL_COUNT)
#define AUDIO_QUEUE_WTRMRK_BYTES   (AUDIO_QUEUE_WATERMARK * AUDIO_QUEUE_ITEM_LEN_BYTES)
#define AUDIO_QUEUE_LENGTH_BYTES   (AUDIO_QUEUE_NUM_ITEMS * AUDIO_QUEUE_ITEM_LEN_BYTES)

#define AFE_BLOCKS_TO_ACCUMULATE (3)

/*******************************************************************************
 * Global Vars
 ******************************************************************************/

static TaskHandle_t s_appTask;

static SemaphoreHandle_t s_pushCtr;

static uint8_t *s_afe_mem_pool;
static uint8_t s_afeAudioOut[PCM_SINGLE_CH_SMPL_COUNT * PCM_SAMPLE_SIZE_BYTES] __attribute__((aligned(4)));

int16_t g_audio_out[PCM_SINGLE_CH_SMPL_COUNT * AFE_BLOCKS_TO_ACCUMULATE];
int16_t *g_out_ptr            = g_audio_out;
uint8_t g_accumulated_samples = 0;

QueueHandle_t g_xSampleQueue = NULL;

#if defined(SLN_LOCAL2_RD)
SDK_ALIGN(uint8_t __attribute__((section(".data.$SRAM_DTC"))) g_externallyAllocatedMem[AFE_MEM_SIZE_2MICS], 8);
#elif defined(SLN_LOCAL2_IOT)
SDK_ALIGN(uint8_t __attribute__((section(".data.$SRAM_DTC"))) g_externallyAllocatedMem[AFE_MEM_SIZE_3MICS], 8);
#endif

static TaskHandle_t s_thisTaskHandle = NULL;
static pcmPingPong_t *s_micInputStream;
static int16_t *s_ampInputStream;
static uint32_t s_numItems    = 0;
static uint32_t s_waterMark   = 0;
static uint32_t s_outputIndex = 0;

uint8_t *cloud_buffer;
uint32_t cloud_buffer_len = 0;
/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/

/*! @brief Called to reset the index's to ensure old mic data isn't resent. */
static void audio_processing_reset_mic_capture_buffers()
{
    s_numItems    = 0;
    s_waterMark   = 0;
    s_outputIndex = 0;
}

void audio_processing_set_app_task_handle(TaskHandle_t *handle)
{
    if ((NULL != handle) && (NULL != *handle))
    {
        s_appTask = *handle;
    }
}

void audio_processing_set_task_handle(TaskHandle_t *handle)
{
    s_thisTaskHandle = *handle;
}

TaskHandle_t audio_processing_get_task_handle(void)
{
    return s_thisTaskHandle;
}

void audio_processing_set_mic_input_buffer(int16_t **buf)
{
    if ((NULL != buf) && (NULL != *buf))
    {
        s_micInputStream = (pcmPingPong_t *)(*buf);
    }
}

void audio_processing_set_amp_input_buffer(int16_t **buf)
{
    if ((NULL != buf) && (NULL != *buf))
    {
        s_ampInputStream = (int16_t *)(*buf);
    }
}

void audio_processing_task(void *pvParameters)
{
    uint8_t pingPongIdx     = 0;
    uint8_t *cleanAudioBuff = NULL;
    int32_t status          = 0;
    uint8_t pingPongAmpIdx  = 0;

    uint32_t taskNotification = 0U;
    uint32_t currentEvent     = 0U;

    sln_afe_configuration_params_t afeConfig;

    s_pushCtr = xSemaphoreCreateCounting(2, 0);

    /* Make sure we memset the buffer to zero */
    audio_processing_reset_mic_capture_buffers();

    afeConfig.postProcessedGain = 0x0600;
    afeConfig.numberOfMics      = PDM_MIC_COUNT;
    afeConfig.afeMemBlock       = g_externallyAllocatedMem;
    afeConfig.afeMemBlockSize   = sizeof(g_externallyAllocatedMem);

    cleanAudioBuff = &s_afeAudioOut[0];

    status = SLN_AFE_Init(&s_afe_mem_pool, pvPortMalloc, &afeConfig);
    if (status != kAfeSuccess)
    {
        configPRINTF(("ERROR [%d]: AFE engine initialization has failed!\r\n", status));
    }

    g_xSampleQueue = xQueueCreate(5, PCM_SINGLE_CH_SMPL_COUNT * AFE_BLOCKS_TO_ACCUMULATE * sizeof(short));
    if (g_xSampleQueue == NULL)
    {
        configPRINTF(("Could not create queue for AFE to ASR communication. Audio processing task failed!\r\n"));
        RGB_LED_SetColor(LED_COLOR_RED);
        vTaskDelete(NULL);
    }

    while (1)
    {
        // Suspend waiting to be activated when receiving PDM mic data after Decimation
        xTaskNotifyWait(0U, ULONG_MAX, &taskNotification, portMAX_DELAY);

        // Figure out if it's a PING or PONG buffer received
        if (taskNotification & (1U << PCM_PING))
        {
            pingPongIdx    = 1U;
            pingPongAmpIdx = 1U;
            currentEvent   = (1U << PCM_PING);
        }

        if (taskNotification & (1U << PCM_PONG))
        {
            pingPongIdx    = 0U;
            pingPongAmpIdx = 0U;
            currentEvent   = (1U << PCM_PONG);
        }

        // Process microphone streams
        int16_t *pcmIn = (int16_t *)((*s_micInputStream)[pingPongIdx]);

        // Run mic streams through the AFE
        SLN_AFE_Process_Audio(&s_afe_mem_pool, pcmIn, &s_ampInputStream[pingPongAmpIdx * PCM_SINGLE_CH_SMPL_COUNT],
                              cleanAudioBuff);

        // Pass output of AFE to wake word
        memcpy((uint8_t *)g_out_ptr, (uint8_t *)cleanAudioBuff, PCM_SINGLE_CH_SMPL_COUNT * 2);
        g_out_ptr += PCM_SINGLE_CH_SMPL_COUNT;
        g_accumulated_samples++;

        // If we've accumulated enough audio, send it to ASR
        if (g_accumulated_samples == AFE_BLOCKS_TO_ACCUMULATE)
        {
            // Ship it out to ASR
            if (xQueueSendToBack(g_xSampleQueue, g_audio_out, 0) == errQUEUE_FULL)
            //            if(xQueueSendToBack(g_xSampleQueue, cleanAudioBuff, 0) == errQUEUE_FULL)
            {
                RGB_LED_SetColor(LED_COLOR_PURPLE);
            }

            g_out_ptr             = g_audio_out;
            g_accumulated_samples = 0;
        }

        taskNotification &= ~currentEvent;
    }
}
