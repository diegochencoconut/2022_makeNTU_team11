/*
 * Copyright 2019-2021 NXP
 * All rights reserved.
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "FreeRTOS.h"
#include "event_groups.h"
#include "queue.h"
#include "task.h"
#include "timers.h"

#include "board.h"
#include "fsl_dmamux.h"
#include "fsl_edma.h"
#include "fsl_sai.h"
#include "fsl_sai_edma.h"
#include "fsl_lpi2c.h"
#include "fsl_codec_common.h"
#include "pdm_pcm_definitions.h"
#include "sln_amplifier.h"

#if USE_MQS
#include "fsl_gpt.h"
#include "semphr.h"
#include "ringbuffer.h"
#endif /* USE_MQS */

#if USE_AUDIO_SPEAKER
#include "composite.h"
#endif

#include "sln_flash_mgmt.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
/*! @brief AMPLIFIER SEND Task settings */
#define AMPLIFIER_SEND_TASK_NAME       "amplifier_send_task"
#define AMPLIFIER_SEND_TASK_STACK_SIZE 1024
#define AMPLIFIER_SEND_TASK_PRIORITY   configMAX_PRIORITIES - 1

#if USE_MQS
#define PCM_AMP_DMA_CHUNK_SIZE 2 * PCM_AMP_SAMPLE_COUNT *PCM_SAMPLE_SIZE_BYTES

/* For 24Mhz and PS=200, 1 tick ~= 8.4us.
 * In this configuration, AMP_LOOPBACK_GPT will overflow after around 10 hours */
#define AMP_LOOPBACK_GPT          GPT2
#define AMP_LOOPBACK_GPT_FREQ_MHZ 24
#define AMP_LOOPBACK_GPT_PS       200

/* Convert Loopback GPT ticks to microseconds */
#define AMP_LOOPBACK_GPT_TICKS_TO_US(ticks) ((ticks * AMP_LOOPBACK_GPT_PS) / AMP_LOOPBACK_GPT_FREQ_MHZ)

#define AMP_LOOPBACK_NEW_SYNC_MAX_WAIT_MS 100

/* During loopback enable, delay the barge-in start in order to give time
 * to the pdm_to_pcm_task to restart its activities. */
#define AMP_LOOPBACK_START_DELAY_MS 15

typedef enum _loopback_state
{
    kLoopbackDisabled,
    kLoopbackNeedSync,
    kLoopbackEnabled,
} loopback_state_t;

#elif USE_TFA
#define PCM_AMP_DMA_CHUNK_SIZE 0x80000
#endif /* USE_MQS */

#define PCM_AMP_DMA_TX_COMPLETE_EVT_BIT 1
#define PCM_AMP_AUDIO_ABORT_EVT_BIT     2

#define WAIT_SAI_RX_FEF_FLAG_CLEAR  3
#define WAIT_SAI_TX_FEF_FLAG_CLEAR  3
#define LOOPBACK_STOP_SCHEDULE_WAIT 20
/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/
static TaskHandle_t s_AmplifierSendTaskHandle;
static EventGroupHandle_t s_DmaTxComplete;
static codec_handle_t codecHandle;

#if USE_TFA
static EventGroupHandle_t s_LoopBackEventGroup;
static EventBits_t s_LoopBackEvent;
static EventBits_t s_LoopBackErrorEvent;
__attribute__((section(".data.$SRAM_DTC")))
__attribute__((aligned(2))) static uint8_t s_AmpRxDmaBuffer[PCM_AMP_SAMPLE_COUNT * PCM_SAMPLE_SIZE_BYTES];
__attribute__((section(".data.$SRAM_DTC")))
__attribute__((aligned(2))) static uint8_t s_AmpRxDataBuffer[PCM_AMP_SAMPLE_COUNT * PCM_SAMPLE_SIZE_BYTES];

#elif USE_MQS
static SemaphoreHandle_t s_LoopBackMutex         = NULL;
static ringbuf_t *s_AmpRxDataRingBuffer          = NULL;
static volatile uint32_t s_PdmPcmTimestamp       = -1;
static SemaphoreHandle_t s_LoopBackStateMutex    = NULL;
static volatile loopback_state_t s_LoopbackState = kLoopbackEnabled;
#endif /* USE_TFA */

/* The DMA Handle for audio amplifier SAI3 */
edma_handle_t s_AmpDmaTxHandle   = {0};
sai_edma_handle_t s_AmpTxHandler = {0};
#if USE_TFA
edma_handle_t s_AmpDmaRxHandle   = {0};
sai_edma_handle_t s_AmpRxHandler = {0};
#endif /* USE_TFA */

static volatile uint32_t u32AudioLength = 0;
static uint8_t *pu8AudioPlay            = NULL;
static uint32_t startAudioLength        = 0;
static uint8_t *startAudioPlay          = NULL;
static volatile uint8_t loop            = 0;

#if USE_AUDIO_SPEAKER
extern usb_device_composite_struct_t g_composite;
extern uint8_t audioPlayDataBuff[AUDIO_SPEAKER_DATA_WHOLE_BUFFER_LENGTH * FS_ISO_OUT_ENDP_PACKET_SIZE];

USB_DMA_NONINIT_DATA_ALIGN(USB_DATA_ALIGN_SIZE)
static uint8_t audioPlayDMATempBuff[FS_ISO_OUT_ENDP_PACKET_SIZE];
#endif

static volatile uint8_t *pu8BufferPool   = NULL;
static uint8_t *pDefaultAudioData        = NULL;
static uint32_t s_DefaultAudioDataLength = 0;

#if USE_TFA
static volatile bool s_LoopBackRunning       = false;
static volatile bool s_LoopBackStopScheduled = false;
#endif /* USE_TFA */

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

/*******************************************************************************
 * Code
 ******************************************************************************/
void SAI_UserTxIRQHandler(void)
{
    uint8_t i;

    SAI_TxClearStatusFlags(BOARD_AMP_SAI, kSAI_FIFOErrorFlag);
    /* Give time FEF flag to be cleared */
    for (i = 0; i < WAIT_SAI_TX_FEF_FLAG_CLEAR; i++)
    {
        if ((BOARD_AMP_SAI->TCSR & kSAI_FIFOErrorFlag) == 0)
        {
            break;
        }
    }
}

void SAI_UserRxIRQHandler(void)
{
#if USE_TFA
    if (s_LoopBackRunning)
    {
        /* SAI FIFO error was triggered, which may happen due to a call to SLN_AMP_LoopbackDisable
         * or due to an internal error.
         * In case it was intentionally shut down by the application by calling SLN_AMP_LoopbackDisable,
         * no actions should be taken.
         * In case it was an internal error, notify the pdm_to_pcm_task to restart the mics and the loopback.
         * Mics and loopback should be kept in sync => should be started at the same time.
         */
        s_LoopBackRunning = false;
        if (s_LoopBackStopScheduled == false)
        {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;

            SAI_TransferTerminateReceiveEDMA(BOARD_AMP_SAI, &s_AmpRxHandler);

            if (NULL != s_LoopBackEventGroup)
            {
                xEventGroupSetBitsFromISR(s_LoopBackEventGroup, s_LoopBackErrorEvent, &xHigherPriorityTaskWoken);
            }
        }
    }
    else
    {
        uint8_t i;

        /* Loopback mechanism is disabled. Just clear the FEF flag */
        SAI_RxClearStatusFlags(BOARD_AMP_SAI, kSAI_FIFOErrorFlag);

        /* Give time FEF flag to be cleared */
        for (i = 0; i < WAIT_SAI_RX_FEF_FLAG_CLEAR; i++)
        {
            if ((BOARD_AMP_SAI->RCSR & kSAI_FIFOErrorFlag) == 0)
            {
                break;
            }
        }
    }
#elif USE_MQS
    SAI_RxClearStatusFlags(BOARD_AMP_SAI, kSAI_FIFOErrorFlag);
#endif /* USE_TFA */
}

#if USE_MQS
/*
 * In case of NOT being already synchronized, calculate the delay between the last Ping/Pong event and the current call.
 * Add this delay as zeroes to the ringbuffer.
 * After synchronization (if was needed), start the playback and place the playback data into the ringbuffer.
 */
static status_t SLN_AMP_RxCallback(uint8_t *data, uint32_t length, sai_transfer_t *write_xfer)
{
    status_t status          = kStatus_Success;
    uint16_t i               = 0;
    uint8_t dummyZero        = 0;
    uint32_t ringbufOcc      = 0;
    uint32_t slnAmpTimestamp = 0;
    uint32_t delayTicks      = 0;
    uint32_t delayUs         = 0;
    uint32_t delayBytes      = 0;

    if ((s_LoopBackStateMutex == NULL) || (s_LoopBackMutex == NULL) || (s_AmpRxDataRingBuffer == NULL) ||
        (s_PdmPcmTimestamp == -1))
    {
        /* Loopback is not ready, just send the sound chunk to dma */
        status = SAI_TransferSendEDMA(BOARD_AMP_SAI, &s_AmpTxHandler, write_xfer);
        return status;
    }

    xSemaphoreTake(s_LoopBackStateMutex, portMAX_DELAY);
    if (s_LoopbackState == kLoopbackNeedSync)
    {
        /* Give time to pdm_to_pcm_task to restart its activities */
        vTaskDelay(AMP_LOOPBACK_START_DELAY_MS);

        /* Wait for all the registered AMP packets to be played.
         * This is needed for a new synchronization to be performed after a loopback disable-enable. */
        for (i = 0; i < AMP_LOOPBACK_NEW_SYNC_MAX_WAIT_MS; i++)
        {
            if ((*pu8BufferPool) == AMP_WRITE_SLOTS)
            {
                s_LoopbackState = kLoopbackEnabled;
                break;
            }
            vTaskDelay(1);
        }
    }

    if (s_LoopbackState == kLoopbackEnabled)
    {
        xSemaphoreTake(s_LoopBackMutex, portMAX_DELAY);

        status = SAI_TransferSendEDMA(BOARD_AMP_SAI, &s_AmpTxHandler, write_xfer);
        if (status == kStatus_Success)
        {
            /* Check if the current packet is the first one of a playback session.
             * If it is the first packet, add delay data to the ringbuffer. */
            ringbufOcc = ringbuf_get_occupancy(s_AmpRxDataRingBuffer);
            if (ringbufOcc == 0)
            {
                slnAmpTimestamp = GPT_GetCurrentTimerCount(AMP_LOOPBACK_GPT);

                /* Add the delay data in order to sync the microphones with the amplifier. */
                if (slnAmpTimestamp > s_PdmPcmTimestamp)
                {
                    delayTicks = slnAmpTimestamp - s_PdmPcmTimestamp;
                }
                else
                {
                    delayTicks = (UINT32_MAX - s_PdmPcmTimestamp) + slnAmpTimestamp;
                }

                delayUs = AMP_LOOPBACK_GPT_TICKS_TO_US(delayTicks) + AMP_LOOPBACK_CONST_DELAY_US;

                /* Delay in bytes should be multiple of 4. Number 4 is selected because it is needed
                 * to keep samples grouped by 2(positive and negative) and one sample is an int16 (on 2 bytes). */
                delayBytes = (delayUs * PCM_AMP_DATA_SIZE_1_MS) / 1000;
                delayBytes = delayBytes - (delayBytes % 4);

                if (delayBytes > AMP_LOOPBACK_MAX_DELAY_BYTES)
                {
                    /* Should not happen, but better safe */
                    configPRINTF(("WARNING: loopback desync of %d packets\r\n",
                                  delayBytes - (AMP_LOOPBACK_MAX_DELAY_BYTES - (AMP_LOOPBACK_MAX_DELAY_BYTES % 4))));
                    delayBytes = AMP_LOOPBACK_MAX_DELAY_BYTES - (AMP_LOOPBACK_MAX_DELAY_BYTES % 4);
                }

                for (i = 0; i < delayBytes; i++)
                {
                    ringbuf_write(s_AmpRxDataRingBuffer, &dummyZero, 1);
                }
            }

            /* Place the data in the ringbuffer. This data will be used for barge-in.
             * It should not happen, but skip the packet if the ring buffer is full. */
            if (ringbufOcc + length <= AMP_LOOPBACK_RINGBUF_SIZE)
            {
                ringbuf_write(s_AmpRxDataRingBuffer, data, length);
            }
            else
            {
                configPRINTF(("Failed to write data to the loopback ringbuffer. data len = %d, free space = %d\r\n",
                              length, AMP_LOOPBACK_RINGBUF_SIZE - ringbufOcc));
            }
        }

        xSemaphoreGive(s_LoopBackMutex);
    }
    else
    {
        status = SAI_TransferSendEDMA(BOARD_AMP_SAI, &s_AmpTxHandler, write_xfer);
    }

    xSemaphoreGive(s_LoopBackStateMutex);

    return status;
}
#elif USE_TFA
static void SLN_AMP_RxCallback(I2S_Type *base, sai_edma_handle_t *handle, status_t status, void *userData)
{
    static uint8_t firstTime = 1;

    if (!firstTime)
    {
        BaseType_t xHigherPriorityTaskWoken;
        xHigherPriorityTaskWoken = pdFALSE;

        sai_transfer_t sSaiRxBuffer = {0};

        memcpy(s_AmpRxDataBuffer, s_AmpRxDmaBuffer, PCM_AMP_SAMPLE_COUNT * PCM_SAMPLE_SIZE_BYTES);

        if (NULL != s_LoopBackEventGroup)
        {
            xEventGroupSetBitsFromISR(s_LoopBackEventGroup, s_LoopBackEvent, &xHigherPriorityTaskWoken);
        }

        /* Read the next 48KHz, Stereo, 16Bit sample */
        sSaiRxBuffer.data     = s_AmpRxDmaBuffer;
        sSaiRxBuffer.dataSize = PCM_AMP_SAMPLE_COUNT * PCM_SAMPLE_SIZE_BYTES;

        if (s_LoopBackStopScheduled == false)
        {
            SAI_TransferReceiveEDMA(base, &s_AmpRxHandler, &sSaiRxBuffer);
        }
    }
    firstTime = 0;
}
#endif /* USE_MQS */

static void SLN_AMP_TxCallback(I2S_Type *base, sai_edma_handle_t *handle, status_t status, void *userData)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xEventGroupSetBitsFromISR(s_DmaTxComplete, PCM_AMP_DMA_TX_COMPLETE_EVT_BIT, &xHigherPriorityTaskWoken);

    if (pu8BufferPool != NULL)
    {
        (*pu8BufferPool)++;
    }

#if USE_AUDIO_SPEAKER
    sai_transfer_t xfer = {0};
    if ((g_composite.audioUnified.audioSendTimes >= g_composite.audioUnified.usbRecvTimes) &&
        (g_composite.audioUnified.startPlayHalfFull == 1))
    {
        g_composite.audioUnified.startPlayHalfFull      = 0;
        g_composite.audioUnified.speakerDetachOrNoInput = 1;
    }
    if (g_composite.audioUnified.startPlayHalfFull)
    {
        xfer.dataSize = FS_ISO_OUT_ENDP_PACKET_SIZE;
        xfer.data     = audioPlayDataBuff + g_composite.audioUnified.tdWriteNumberPlay;
        g_composite.audioUnified.audioSendCount += FS_ISO_OUT_ENDP_PACKET_SIZE;
        g_composite.audioUnified.audioSendTimes++;
        g_composite.audioUnified.tdWriteNumberPlay += FS_ISO_OUT_ENDP_PACKET_SIZE;
        if (g_composite.audioUnified.tdWriteNumberPlay >=
            AUDIO_SPEAKER_DATA_WHOLE_BUFFER_LENGTH * FS_ISO_OUT_ENDP_PACKET_SIZE)
        {
            g_composite.audioUnified.tdWriteNumberPlay = 0;
        }
    }
    else
    {
        xfer.dataSize = FS_ISO_OUT_ENDP_PACKET_SIZE;
        xfer.data     = audioPlayDMATempBuff;
    }

    SAI_TransferSendEDMA(base, handle, &xfer);
#endif
}

#if USE_MQS
static void SLN_AMP_VolAndDiffInputControl(void *data, uint32_t len)
{
    uint32_t i;
    int16_t *data16 = (int16_t *)data;
    float volume    = ((mqs_config_t *)(codecHandle.codecConfig->codecDevConfig))->volume;
    uint32_t len16  = len / sizeof(int16_t);

    for (i = 0; i < len16 - 1; i += 2)
    {
        // the volume is decreased by multiplying the samples with values between 0 and 1
        data16[i]     = (int16_t)(data16[i] * volume);
        data16[i + 1] = -data16[i];
    }
}
#endif /* USE_MQS */

void SLN_AMP_SetDefaultAudioData(uint8_t *data, uint32_t len)
{
    if (data)
    {
        pDefaultAudioData        = data;
        s_DefaultAudioDataLength = len;
    }
}

amplifier_status_t SLN_AMP_WriteDefault(void)
{
#if USE_SIGNAL_TEST_SIGNAL
    PlaybackSine();
#else
    sai_transfer_t xfer_codec = {0};

    // Take advantage of 32 being power of 2 to replace % to reduce # of instructions
    size_t overFlow = s_DefaultAudioDataLength & 31U;
    if (overFlow)
    {
        xfer_codec.dataSize = s_DefaultAudioDataLength - (overFlow);
    }
    xfer_codec.data = pDefaultAudioData;
    SAI_TransferSendEDMA(BOARD_AMP_SAI, &s_AmpTxHandler, &xfer_codec);
#endif
    return 0;
}

void audio_send_task(void *pvParameters)
{
    sai_transfer_t write_xfer = {0};
    EventBits_t events;

    /* Chop into PCM_AMP_DMA_CHUNK_SIZE chunks so the DMA can handle it. */
    while (1)
    {
        events = xEventGroupWaitBits(s_DmaTxComplete, PCM_AMP_AUDIO_ABORT_EVT_BIT | PCM_AMP_DMA_TX_COMPLETE_EVT_BIT,
                                     pdTRUE, pdFALSE, portMAX_DELAY);

        /* We are assured that we waited for all the SAI_TransferSendEDMA to finish and we can stop safely. */
        if (u32AudioLength == 0)
        {
            break;
        }

        // Abort
        if (events & 2)
        {
            SAI_TransferTerminateSendEDMA(BOARD_AMP_SAI, &s_AmpTxHandler);
            u32AudioLength = 0;
            pu8AudioPlay   = NULL;
            loop           = 0;
            break;
        }
        else
        {
            if (u32AudioLength > PCM_AMP_DMA_CHUNK_SIZE)
            {
                write_xfer.dataSize = PCM_AMP_DMA_CHUNK_SIZE;
                write_xfer.data     = pu8AudioPlay;

#if USE_TFA
                SAI_TransferSendEDMA(BOARD_AMP_SAI, &s_AmpTxHandler, &write_xfer);

#elif USE_MQS
                SLN_AMP_VolAndDiffInputControl(write_xfer.data, write_xfer.dataSize);
                SLN_AMP_RxCallback(write_xfer.data, write_xfer.dataSize, &write_xfer);
#endif /* USE_TFA */

                pu8AudioPlay += PCM_AMP_DMA_CHUNK_SIZE;
                u32AudioLength -= PCM_AMP_DMA_CHUNK_SIZE;
            }
            else
            {
                write_xfer.dataSize = u32AudioLength;
                write_xfer.data     = pu8AudioPlay;

#if USE_TFA
                SAI_TransferSendEDMA(BOARD_AMP_SAI, &s_AmpTxHandler, &write_xfer);

#elif USE_MQS
                SLN_AMP_VolAndDiffInputControl(write_xfer.data, write_xfer.dataSize);
                SLN_AMP_RxCallback(write_xfer.data, write_xfer.dataSize, &write_xfer);
#endif /* USE_TFA */

                if (loop)
                {
                    u32AudioLength = startAudioLength;
                    pu8AudioPlay   = startAudioPlay;
                }
                else
                {
                    u32AudioLength = 0;
                    pu8AudioPlay   = NULL;
                }
            }
        }
    }

    vTaskDelete(NULL);
}

amplifier_status_t SLN_AMP_Write(uint8_t *data, uint32_t length)
{
    sai_transfer_t write_xfer     = {0};
    amplifier_status_t eAmpStatus = 1;

    if (0 == u32AudioLength)
    {
        pu8AudioPlay = data;
        if (length)
        {
            u32AudioLength = length - (length % 32);
        }
        else
        {
            u32AudioLength = length;
        }

        /* Chop into 512KB chunks so the DMA can handle it. */
        if (u32AudioLength > PCM_AMP_DMA_CHUNK_SIZE)
        {
            write_xfer.dataSize = PCM_AMP_DMA_CHUNK_SIZE;
            write_xfer.data     = pu8AudioPlay;

#if USE_TFA
            SAI_TransferSendEDMA(BOARD_AMP_SAI, &s_AmpTxHandler, &write_xfer);

#elif USE_MQS
            SLN_AMP_VolAndDiffInputControl(write_xfer.data, write_xfer.dataSize);
            SLN_AMP_RxCallback(write_xfer.data, write_xfer.dataSize, &write_xfer);
#endif /* USE_TFA */

            pu8AudioPlay += PCM_AMP_DMA_CHUNK_SIZE;
            u32AudioLength -= PCM_AMP_DMA_CHUNK_SIZE;

            if (xTaskCreate(audio_send_task, AMPLIFIER_SEND_TASK_NAME, AMPLIFIER_SEND_TASK_STACK_SIZE, NULL,
                            AMPLIFIER_SEND_TASK_PRIORITY, &s_AmplifierSendTaskHandle) == pdPASS)
            {
                eAmpStatus = 0;
            }
            else
            {
                configPRINTF(("Failed to create amplifier_send_task for SLN_AMP_Write!\r\n"));
            }
        }
        else
        {
            write_xfer.dataSize = u32AudioLength;
            write_xfer.data     = pu8AudioPlay;

#if USE_TFA
            SAI_TransferSendEDMA(BOARD_AMP_SAI, &s_AmpTxHandler, &write_xfer);

#elif USE_MQS
            SLN_AMP_VolAndDiffInputControl(write_xfer.data, write_xfer.dataSize);
            SLN_AMP_RxCallback(write_xfer.data, write_xfer.dataSize, &write_xfer);
#endif /* USE_TFA */

            u32AudioLength = 0;
            pu8AudioPlay   = NULL;

            eAmpStatus = 0;
        }
    }
    return eAmpStatus;
}

amplifier_status_t SLN_AMP_WriteLoop(uint8_t *data, uint32_t length)
{
    sai_transfer_t write_xfer     = {0};
    amplifier_status_t eAmpStatus = 1;

    if (0 == u32AudioLength)
    {
        pu8AudioPlay = data;
        if (length)
        {
            u32AudioLength = length - (length % 32);
        }
        else
        {
            u32AudioLength = length;
        }

        startAudioLength = u32AudioLength;
        startAudioPlay   = data;
        loop             = 1;

        /* Chop into 512KB chunks so the DMA can handle it. */
        if (u32AudioLength > PCM_AMP_DMA_CHUNK_SIZE)
        {
            write_xfer.dataSize = PCM_AMP_DMA_CHUNK_SIZE;
            write_xfer.data     = pu8AudioPlay;

#if USE_TFA
            SAI_TransferSendEDMA(BOARD_AMP_SAI, &s_AmpTxHandler, &write_xfer);

#elif USE_MQS
            SLN_AMP_VolAndDiffInputControl(write_xfer.data, write_xfer.dataSize);
            SLN_AMP_RxCallback(write_xfer.data, write_xfer.dataSize, &write_xfer);
#endif /* USE_TFA */

            pu8AudioPlay += PCM_AMP_DMA_CHUNK_SIZE;
            u32AudioLength -= PCM_AMP_DMA_CHUNK_SIZE;

            /* Clear all the bits before creating the task */
            xEventGroupClearBits(s_DmaTxComplete, PCM_AMP_DMA_TX_COMPLETE_EVT_BIT | PCM_AMP_AUDIO_ABORT_EVT_BIT);

            if (xTaskCreate(audio_send_task, AMPLIFIER_SEND_TASK_NAME, AMPLIFIER_SEND_TASK_STACK_SIZE, NULL,
                            AMPLIFIER_SEND_TASK_PRIORITY, &s_AmplifierSendTaskHandle) == pdPASS)
            {
                eAmpStatus = 0;
            }
            else
            {
                configPRINTF(("Failed to create amplifier_send_task for SLN_AMP_WriteLoop!\r\n"));
            }
        }
        else
        {
            write_xfer.dataSize = u32AudioLength;
            write_xfer.data     = pu8AudioPlay;

#if USE_TFA
            SAI_TransferSendEDMA(BOARD_AMP_SAI, &s_AmpTxHandler, &write_xfer);

#elif USE_MQS
            SLN_AMP_VolAndDiffInputControl(write_xfer.data, write_xfer.dataSize);
            SLN_AMP_RxCallback(write_xfer.data, write_xfer.dataSize, &write_xfer);
#endif /* USE_TFA */

            u32AudioLength = 0;
            pu8AudioPlay   = NULL;

            eAmpStatus = 0;
        }
    }
    return eAmpStatus;
}

amplifier_status_t SLN_AMP_WriteNoWait(uint8_t *data, uint32_t length)
{
    uint32_t total_len;
    uint8_t *ptr              = data;
    sai_transfer_t write_xfer = {0};
    status_t ret              = 0;

    if (length)
    {
        total_len = length - (length % 32);
    }
    else
    {
        total_len = length;
    }

    /* Chop into 512KB chunks so the DMA can handle it. */
    write_xfer.dataSize = total_len;
    write_xfer.data     = ptr;

#if USE_TFA
    ret = SAI_TransferSendEDMA(BOARD_AMP_SAI, &s_AmpTxHandler, &write_xfer);

#elif USE_MQS
    SLN_AMP_VolAndDiffInputControl(write_xfer.data, write_xfer.dataSize);
    ret = SLN_AMP_RxCallback(write_xfer.data, write_xfer.dataSize, &write_xfer);
#endif /* USE_TFA */

    return ret;
}

amplifier_status_t SLN_AMP_WriteBlocking(uint8_t *data, uint32_t length)
{
    uint32_t total_len;
    uint8_t *ptr              = data;
    sai_transfer_t write_xfer = {0};

    if (length)
    {
        total_len = length - (length % 32);
    }
    else
    {
        total_len = length;
    }

    /* Chop into 512KB chunks so the DMA can handle it. */
    while (total_len)
    {
        if (total_len > PCM_AMP_DMA_CHUNK_SIZE)
        {
            write_xfer.dataSize = PCM_AMP_DMA_CHUNK_SIZE;
            write_xfer.data     = ptr;

#if USE_TFA
            SAI_TransferSendEDMA(BOARD_AMP_SAI, &s_AmpTxHandler, &write_xfer);

#elif USE_MQS
            SLN_AMP_VolAndDiffInputControl(write_xfer.data, write_xfer.dataSize);
            SLN_AMP_RxCallback(write_xfer.data, write_xfer.dataSize, &write_xfer);
#endif /* USE_TFA */

            ptr += PCM_AMP_DMA_CHUNK_SIZE;
            total_len -= PCM_AMP_DMA_CHUNK_SIZE;
        }
        else
        {
            write_xfer.dataSize = total_len;
            write_xfer.data     = ptr;

#if USE_TFA
            SAI_TransferSendEDMA(BOARD_AMP_SAI, &s_AmpTxHandler, &write_xfer);

#elif USE_MQS
            SLN_AMP_VolAndDiffInputControl(write_xfer.data, write_xfer.dataSize);
            SLN_AMP_RxCallback(write_xfer.data, write_xfer.dataSize, &write_xfer);
#endif /* USE_TFA */

            total_len = 0;
        }

        xEventGroupWaitBits(s_DmaTxComplete, PCM_AMP_DMA_TX_COMPLETE_EVT_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
    }

    return 0;
}

amplifier_status_t SLN_AMP_AbortWrite(void)
{
    xEventGroupSetBits(s_DmaTxComplete, PCM_AMP_AUDIO_ABORT_EVT_BIT);
    return 0;
}

amplifier_status_t SLN_AMP_Read(void)
{
#if USE_TFA
    sai_transfer_t xfer = {0};
    xfer.data           = s_AmpRxDmaBuffer;
    xfer.dataSize       = sizeof(s_AmpRxDmaBuffer);

    SAI_TransferReceiveEDMA(BOARD_AMP_SAI, &s_AmpRxHandler, &xfer);
    s_LoopBackRunning = true;
#endif /* USE_TFA */
    return 0;
}

#if USE_MQS
static void SLN_AMP_StartLoopbackTimer(void)
{
    gpt_config_t gpt;

    /* The timer is set with a 24M clock. */
    CLOCK_SetMux(kCLOCK_PerclkMux, 1);
    CLOCK_SetDiv(kCLOCK_PerclkDiv, 0);

    GPT_GetDefaultConfig(&gpt);
    gpt.divider = AMP_LOOPBACK_GPT_PS;
    GPT_Init(AMP_LOOPBACK_GPT, &gpt);

    GPT_StartTimer(AMP_LOOPBACK_GPT);
}
#endif /* USE_MQS */

amplifier_status_t SLN_AMP_Init(volatile uint8_t *pu8BuffNum)
{
    status_t ret = kStatus_Fail;

    sai_init_handle_t saiInitHandle = {
        .amp_dma_tx_handle = &s_AmpDmaTxHandle,
        .amp_sai_tx_handle = &s_AmpTxHandler,
        .sai_tx_callback   = SLN_AMP_TxCallback,
#if USE_TFA
        .amp_dma_rx_handle = &s_AmpDmaRxHandle,
        .amp_sai_rx_handle = &s_AmpRxHandler,
        .sai_rx_callback   = SLN_AMP_RxCallback,
#endif /* USE_TFA */
    };

    pu8BufferPool   = pu8BuffNum;
    s_DmaTxComplete = xEventGroupCreate();

#if USE_TFA
    BOARD_Codec_I2C_Init();
#endif /* USE_TFA */

    BOARD_SAI_Init(saiInitHandle);

    xEventGroupWaitBits(s_DmaTxComplete, PCM_AMP_DMA_TX_COMPLETE_EVT_BIT, pdTRUE, pdTRUE, portMAX_DELAY);

    ret = CODEC_Init(&codecHandle, (codec_config_t *)BOARD_GetBoardCodecConfig());

#if USE_MQS
    s_AmpRxDataRingBuffer =
        (ringbuf_t *)(((mqs_config_t *)(codecHandle.codecConfig->codecDevConfig))->s_AmpRxDataRingBuffer);

    s_LoopBackMutex = xSemaphoreCreateMutex();
    if (s_LoopBackMutex == NULL)
    {
        configPRINTF(("Failed to create s_LoopBackMutex\r\n"));
    }

    s_LoopBackStateMutex = xSemaphoreCreateMutex();
    if (s_LoopBackMutex == NULL)
    {
        configPRINTF(("Failed to create s_LoopBackStateMutex\r\n"));
    }

    SLN_AMP_StartLoopbackTimer();
#endif /* USE_MQS */

    return ret;
}

#if USE_TFA
uint8_t *SLN_AMP_GetLoopBackBuffer(void)
{
    return s_AmpRxDataBuffer;
}

void SLN_AMP_SetLoopBackEventGroup(EventGroupHandle_t *eventGroup)
{
    if ((NULL != eventGroup) && (NULL != *eventGroup))
    {
        s_LoopBackEventGroup = *eventGroup;
    }
}

void SLN_AMP_SetLoopBackEventBits(EventBits_t event)
{
    s_LoopBackEvent = event;
}

void SLN_AMP_SetLoopBackErrorEventBits(EventBits_t event)
{
    s_LoopBackErrorEvent = event;
}

#elif USE_MQS
SemaphoreHandle_t SLN_AMP_GetLoopBackMutex(void)
{
    return s_LoopBackMutex;
}

ringbuf_t *SLN_AMP_GetRingBuffer(void)
{
    return s_AmpRxDataRingBuffer;
}

uint32_t SLN_AMP_GetTimestamp(void)
{
    return GPT_GetCurrentTimerCount(AMP_LOOPBACK_GPT);
}

void SLN_AMP_UpdateTimestamp(uint32_t timestamp)
{
    s_PdmPcmTimestamp = timestamp;
}
#endif /* USE_TFA */

void SLN_AMP_SetVolume(uint8_t volume)
{
    /* Set Volume 0(min) to 100 (max) */
    CODEC_SetVolume(&codecHandle, kCODEC_PlayChannelLeft0 | kCODEC_PlayChannelRight0, (uint32_t)volume);
}

void *SLN_AMP_GetAmpTxHandler()
{
    return &s_AmpTxHandler;
}

void SLN_AMP_Abort(void)
{
    /* Stop playback. This will flush the SAI transmit buffers. */
    SAI_TransferTerminateSendEDMA(BOARD_AMP_SAI, &s_AmpTxHandler);
}

void SLN_AMP_LoopbackEnable(void)
{
#if USE_TFA
    if (s_LoopBackRunning == false)
    {
        SLN_AMP_Read();
    }

    DMAMUX_EnableChannel(DMAMUX, 3U);
    NVIC_EnableIRQ(SAI3_RX_IRQn);

#elif USE_MQS
    xSemaphoreTake(s_LoopBackStateMutex, portMAX_DELAY);
    s_LoopbackState = kLoopbackNeedSync;
    xSemaphoreGive(s_LoopBackStateMutex);
#endif /* USE_TFA */
}

void SLN_AMP_LoopbackDisable(void)
{
#if USE_TFA
    uint8_t i;

    s_LoopBackStopScheduled = true;
    /* Wait until RX FIFO error (FEF) flag is set meaning that the SAI read is no more executing.
     * FEF flag will trigger SAI_RxUserIRQHandler which will set s_loopback_running to false.
     */
    for (i = 0; i < LOOPBACK_STOP_SCHEDULE_WAIT; i++)
    {
        if (s_LoopBackRunning == false)
        {
            break;
        }
        vTaskDelay(10);
    }

    /* In case SAI_RxUserIRQHandler was triggered, manually stop SAI reads. */
    if (s_LoopBackRunning == true)
    {
        SAI_TransferTerminateReceiveEDMA(BOARD_AMP_SAI, &s_AmpRxHandler);
        vTaskDelay(30);
        s_LoopBackRunning = false;
    }

    s_LoopBackStopScheduled = false;

    NVIC_DisableIRQ(SAI3_RX_IRQn);
    DMAMUX_DisableChannel(DMAMUX, 3U);

#elif USE_MQS
    xSemaphoreTake(s_LoopBackStateMutex, portMAX_DELAY);

    /* Clear the loopback ringbuffer for a future clean start */
    xSemaphoreTake(s_LoopBackMutex, portMAX_DELAY);
    ringbuf_clear(s_AmpRxDataRingBuffer);
    xSemaphoreGive(s_LoopBackMutex);

    s_LoopbackState = kLoopbackDisabled;
    xSemaphoreGive(s_LoopBackStateMutex);
#endif /* USE_TFA */
}
