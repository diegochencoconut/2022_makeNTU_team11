/*
 * Copyright 2018-2021 NXP.
 * This software is owned or controlled by NXP and may only be used strictly in accordance with the
 * license terms that accompany it. By expressly accepting such terms or by downloading, installing,
 * activating and/or otherwise using the software, you are agreeing that you have read, and that you
 * agree to comply with and are bound by, such license terms. If you do not agree to be bound by the
 * applicable license terms, then you may not retain, install, activate or otherwise use the software.
 */

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "event_groups.h"
#include "queue.h"
#include "task.h"
#include "timers.h"

/* NXP includes. */
#include "board.h"
#include "pdm_to_pcm_task.h"
#include "pdm_pcm_definitions.h"
#include "sln_pdm_mic.h"

#if USE_MQS
#include "semphr.h"
#include "ringbuffer.h"
#endif

#if defined(SLN_DSP_TOOLBOX_LIB)
#include "sln_dsp_toolbox.h"
#else
#define SLN_DSP
#include "sln_intelligence_toolbox.h"
#endif

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define AMP_DSP_STREAM  0U
#define MIC1_DSP_STREAM 1U
#define MIC2_DSP_STREAM 2U
#define MIC3_DSP_STREAM 3U

#define MIC3_START_IDX (SAI1_CH_COUNT * PCM_SINGLE_CH_SMPL_COUNT)

#define PDM_PCM_EVENT_TIMEOUT_MS 1000

#define EVT_PING_MASK (MIC1_PING_EVENT | MIC2_PING_EVENT | MIC3_PING_EVENT)
#define EVT_PONG_MASK (MIC1_PONG_EVENT | MIC2_PONG_EVENT | MIC3_PONG_EVENT)

#define PDM_PCM_EVENT_MASK (EVT_PING_MASK | EVT_PONG_MASK | AMP_REFERENCE_SIGNAL | PDM_ERROR_FLAG | AMP_ERROR_FLAG)

#if USE_MQS
/* Multiply the amplifier signal in order to adjust it according to the the microphones' signals.
 * Without this multiplication, the amplifier signal is too weak compared to the mics' input. */
#define AMP_SIGNAL_MULTIPLIER 10
#endif /* USE_MQS */
/*******************************************************************************
 * Global Vars
 ******************************************************************************/

#if SAI1_CH_COUNT
__attribute__((aligned(8))) uint32_t g_Sai1PdmPingPong[EDMA_TCD_COUNT][PDM_SAMPLE_COUNT * SAI1_CH_COUNT];

pdm_mic_config_t g_pdmMicSai1 = {SAI1,
                                 (USE_SAI1_RX_DATA0_MIC << 0U) | (USE_SAI1_RX_DATA1_MIC << 1U),
                                 PDM_SAMPLE_RATE_HZ,
                                 PDM_CAPTURE_SIZE_BYTES,
                                 PDM_SAMPLE_COUNT,
                                 kRtOutputFallingEdge};

__attribute__((aligned(32))) pdm_mic_handle_t g_pdmMicSai1Handle = {0U};
#endif

#if USE_SAI2_MIC
uint32_t g_Sai2PdmPingPong[EDMA_TCD_COUNT][PDM_SAMPLE_COUNT];

pdm_mic_config_t g_pdmMicSai2 = {
    SAI2, kMicChannel1, PDM_SAMPLE_RATE_HZ, PDM_CAPTURE_SIZE_BYTES, PDM_SAMPLE_COUNT, kRtOutputFallingEdge};

__attribute__((aligned(32))) pdm_mic_handle_t g_pdmMicSai2Handle = {0U};
#endif

#if USE_SAI1_RX_DATA0_MIC
uint32_t g_Sai1Mic0[EDMA_TCD_COUNT][PDM_SAMPLE_COUNT];
#endif
#if USE_SAI1_RX_DATA1_MIC
uint32_t g_Sai1Mic1[EDMA_TCD_COUNT][PDM_SAMPLE_COUNT];
#endif

static pcm_pcm_task_config_t s_config;
static EventGroupHandle_t s_PdmDmaEventGroup;
__attribute__((aligned(2))) static pcmPingPong_t s_pcmStream;
static int16_t s_ampOutput[PCM_SINGLE_CH_SMPL_COUNT * 2];
uint8_t *dspMemPool = NULL;

bool g_micsOn            = false;
bool g_decimationStarted = false;

#if USE_MQS
__attribute__((section(".data.$SRAM_DTC")))
__attribute__((aligned(2))) static int16_t s_AmpRXDataBuffer[PCM_AMP_SAMPLE_COUNT];
volatile static uint32_t s_pingPongTimestamp = 0;
#endif /* USE_MQS */

/*******************************************************************************
 * Prototypes
 ******************************************************************************/

#if USE_MQS
/*!
 * @brief * Read the loopback data (if exists) from the amplifier ringbuffer.
 *
 * @param type Ping or Pong in order to specify the downsampled (ping or pong) buffer.
 */
static void pdm_to_pcm_prepare_amp_data(pcm_event_t type);

/*!
 * @brief * Get the current ticks of the Loopback's timer.
 */
static void pdm_to_pcm_update_timestamp(void);
#endif /* USE_MQS */

/*******************************************************************************
 * Code
 ******************************************************************************/
#if USE_SAI2_MIC
void DMA0_DMA16_IRQHandler(void)
{
    PDM_MIC_DmaCallback(&g_pdmMicSai2Handle);
}
#endif

#if SAI1_CH_COUNT
void DMA1_DMA17_IRQHandler(void)
{
    PDM_MIC_DmaCallback(&g_pdmMicSai1Handle);
}
#endif

static int32_t pdm_to_pcm_dsp_init(uint8_t **memPool)
{
    int32_t dspStatus = kDspSuccess;

    dspStatus = SLN_DSP_Init(memPool, pvPortMalloc);

    if (kDspSuccess == dspStatus)
    {
        dspStatus = SLN_DSP_SetCaptureLength(memPool, PDM_SAMPLE_COUNT * PDM_CAPTURE_SIZE_BYTES);
    }

    if (kDspSuccess == dspStatus)
    {
        dspStatus = SLN_DSP_SetGainFactor(memPool, 3);
    }

    return dspStatus;
}

status_t pcm_to_pcm_set_config(pcm_pcm_task_config_t *config)
{
    status_t status = kStatus_Fail;

    if (NULL != config)
    {
        status = kStatus_Success;
    }

    if (kStatus_Success == status)
    {
        memcpy(&s_config, config, sizeof(s_config));

#if USE_TFA
        if (s_config.feedbackBuffer == NULL)
        {
            status = kStatus_InvalidArgument;
        }
#elif USE_MQS
        if ((s_config.loopbackRingBuffer == NULL) || (s_config.loopbackMutex == NULL) ||
            (s_config.updateTimestamp == NULL) || (s_config.getTimestamp == NULL))
        {
            status = kStatus_InvalidArgument;
        }
#endif /* USE_TFA */
    }

    return status;
}

pcm_pcm_task_config_t *pcm_to_pcm_get_config(void)
{
    return &s_config;
}

int32_t pdm_to_pcm_set_gain(uint8_t u8Gain)
{
    return SLN_DSP_SetGainFactor(&dspMemPool, u8Gain);
}

void pdm_to_pcm_stream_formatter(int16_t *pcmBuffer, pdm_pcm_input_event_t micEvent, uint8_t pcmFormat)
{
    uint32_t validEvents =
        (MIC1_PING_EVENT | MIC2_PING_EVENT | MIC3_PING_EVENT | MIC1_PONG_EVENT | MIC2_PONG_EVENT | MIC3_PONG_EVENT);

    if (validEvents & micEvent)
    {
        uint32_t idxStart    = 0;
        uint32_t idxEnd      = 0;
        uint32_t idxIter     = 0;
        uint32_t pingPongIdx = 0;

        bool isPingEvent = (micEvent & MIC1_PING_EVENT) || (micEvent & MIC2_PING_EVENT) || (micEvent & MIC3_PING_EVENT);
        bool isMicTwoEvent = (micEvent & MIC2_PING_EVENT) || (micEvent & MIC2_PONG_EVENT);
        bool isMicTreEvent = (micEvent & MIC3_PING_EVENT) || (micEvent & MIC3_PONG_EVENT);

        if (isPingEvent)
        {
            pingPongIdx = PCM_PING;
        }
        else
        {
            pingPongIdx = PCM_PONG;
        }

#if PDM_MIC_COUNT == 3
        if (isMicTreEvent)
        {
            idxStart = (pcmFormat) ? 2 : (2 * PCM_SINGLE_CH_SMPL_COUNT);
        }
#else
        isMicTwoEvent = isMicTreEvent;
#endif

        if (isMicTwoEvent)
        {
            idxStart = (pcmFormat) ? 1 : PCM_SINGLE_CH_SMPL_COUNT;
        }

        idxEnd = (pcmFormat) ? PCM_SAMPLE_COUNT : (PCM_SINGLE_CH_SMPL_COUNT + idxStart);

        idxIter = (pcmFormat) ? 2U : 1U;

        for (uint32_t idx = idxStart; idx < idxEnd; idx += idxIter)
        {
            s_pcmStream[pingPongIdx][idx] = *pcmBuffer;
            pcmBuffer++;
        }
    }
}

void pdm_to_pcm_set_task_handle(TaskHandle_t *handle)
{
    if (NULL != handle)
    {
        s_config.thisTask = handle;
    }
}

void pdm_to_pcm_set_audio_proces_task_handle(TaskHandle_t *handle)
{
    if (NULL != handle)
    {
        s_config.processingTask = handle;
    }
}

TaskHandle_t pdm_to_pcm_get_task_handle(void)
{
    return *(s_config.thisTask);
}

#if USE_TFA
EventGroupHandle_t pdm_to_pcm_get_event_group(void)
{
    return s_PdmDmaEventGroup;
}

void pdm_to_pcm_set_amp_loopback_buffer(uint8_t **buf)
{
    if ((NULL != buf) && (NULL != *buf))
    {
        s_config.feedbackBuffer = (int16_t *)(*buf);
    }
}

EventBits_t pdm_to_pcm_get_amp_loopback_event(void)
{
    return (EventBits_t)AMP_REFERENCE_SIGNAL;
}

EventBits_t pdm_to_pcm_get_amp_loopback_error_event(void)
{
    return (EventBits_t)AMP_ERROR_FLAG;
}

#elif USE_MQS
void pdm_to_pcm_set_loopback_mutex(SemaphoreHandle_t *mutex)
{
    s_config.loopbackMutex = *mutex;
}

void pdm_to_pcm_set_loopback_ring_buffer(ringbuf_t *ring_buf)
{
    s_config.loopbackRingBuffer = ring_buf;
}
#endif /* USE_TFA */

int16_t *pdm_to_pcm_get_amp_output(void)
{
    return s_ampOutput;
}

int16_t *pdm_to_pcm_get_pcm_output(void)
{
    return (int16_t *)s_pcmStream;
}

uint8_t **pdm_to_pcm_get_mempool(void)
{
    return &dspMemPool;
}

static volatile EventBits_t preProcessEvents  = 0U;
static volatile EventBits_t postProcessEvents = 0U;
static uint32_t u32AmpIndex                   = 1;

#if USE_MQS
static void pdm_to_pcm_prepare_amp_data(pcm_event_t type)
{
    uint32_t i                    = 0;
    uint32_t ampProcessDataSize   = 0;
    uint32_t ampPingPongBufferIdx = 0;
    uint32_t ampRingBuffOcc       = 0;
    static uint8_t ampOutputClean = 0;

    if ((s_config.loopbackMutex == NULL) || (s_config.loopbackRingBuffer == NULL) ||
        (s_config.updateTimestamp == NULL) || ((type != PCM_PING) && (type != PCM_PONG)))
    {
        return;
    }

    if (type == PCM_PING)
    {
        ampPingPongBufferIdx = 1;
    }
    else
    {
        ampPingPongBufferIdx = 0;
    }

    /* Read the loopback data from the amplifier`s ringbuffer.
     * Do not read more than 10ms of data. */
    xSemaphoreTake(s_config.loopbackMutex, portMAX_DELAY);

    s_config.updateTimestamp(s_pingPongTimestamp);

    ampRingBuffOcc = ringbuf_get_occupancy(s_config.loopbackRingBuffer);
    if (ampRingBuffOcc > PCM_AMP_DATA_SIZE_10_MS)
    {
        ampProcessDataSize = PCM_AMP_DATA_SIZE_10_MS;
    }
    else
    {
        ampProcessDataSize = ampRingBuffOcc;
    }

    ringbuf_read(s_config.loopbackRingBuffer, (uint8_t *)s_AmpRXDataBuffer, ampProcessDataSize);

    xSemaphoreGive(s_config.loopbackMutex);

    /* In case of need, add padding zeroes to form a 10ms chunk of data.
     * Downsample by 3 the data and place it in the downsampled buffer.
     * Amplify the data by the AMP_SIGNAL_MULTIPLIER factor.
     * In case there is no available data, clear the downsampled buffer. */
    if (ampProcessDataSize > 0)
    {
        memset(&((uint8_t *)s_AmpRXDataBuffer)[ampProcessDataSize], 0, (PCM_AMP_DATA_SIZE_10_MS - ampProcessDataSize));

        SLN_DSP_downsample_by_3(&dspMemPool, AMP_DSP_STREAM, s_AmpRXDataBuffer, PCM_AMP_SAMPLE_COUNT,
                                &s_ampOutput[ampPingPongBufferIdx * PCM_SINGLE_CH_SMPL_COUNT]);

        for (i = ampPingPongBufferIdx * PCM_SINGLE_CH_SMPL_COUNT;
             i < ((ampPingPongBufferIdx + 1) * PCM_SINGLE_CH_SMPL_COUNT); i++)
        {
            s_ampOutput[i] = s_ampOutput[i] * AMP_SIGNAL_MULTIPLIER;
        }

        ampOutputClean = 0;
    }
    else if (ampOutputClean == 0)
    {
        ampOutputClean = 1;
        memset(s_ampOutput, 0, sizeof(s_ampOutput));
    }
}

static void pdm_to_pcm_update_timestamp(void)
{
    if (s_config.getTimestamp != NULL)
    {
        s_pingPongTimestamp = s_config.getTimestamp();
    }
}
#endif /* USE_MQS */

void pdm_to_pcm_task(void *pvParameters)
{
    pdm_mic_status_t status = kPdmMicSuccess;
    int32_t dspStatus       = kDspSuccess;

    s_PdmDmaEventGroup = xEventGroupCreate();
    if (s_PdmDmaEventGroup == NULL)
    {
        configPRINTF(("Failed to create s_PdmDmaEventGroup\r\n"));
    }

#if SAI1_CH_COUNT
    g_pdmMicSai1Handle.eventGroup        = s_PdmDmaEventGroup;
    g_pdmMicSai1Handle.config            = &g_pdmMicSai1;
    g_pdmMicSai1Handle.dma               = DMA0;
    g_pdmMicSai1Handle.dmaChannel        = 1U;
    g_pdmMicSai1Handle.dmaIrqNum         = DMA1_DMA17_IRQn;
    g_pdmMicSai1Handle.dmaRequest        = (uint8_t)kDmaRequestMuxSai1Rx;
    g_pdmMicSai1Handle.pongFlag          = MIC1_PONG_EVENT;
    g_pdmMicSai1Handle.pingFlag          = MIC1_PING_EVENT;
    g_pdmMicSai1Handle.errorFlag         = PDM_ERROR_FLAG;
    g_pdmMicSai1Handle.pingPongBuffer[0] = (uint32_t *)(&g_Sai1PdmPingPong[0][0]);
    g_pdmMicSai1Handle.pingPongBuffer[1] = (uint32_t *)(&g_Sai1PdmPingPong[1][0]);
#if USE_MQS
    g_pdmMicSai1Handle.pdmMicUpdateTimestamp = pdm_to_pcm_update_timestamp;
#endif /* USE_MQS */
#endif

#if USE_SAI2_MIC
    g_pdmMicSai2Handle.eventGroup        = s_PdmDmaEventGroup;
    g_pdmMicSai2Handle.config            = &g_pdmMicSai2;
    g_pdmMicSai2Handle.dma               = DMA0;
    g_pdmMicSai2Handle.dmaChannel        = 0U;
    g_pdmMicSai2Handle.dmaIrqNum         = DMA0_DMA16_IRQn;
    g_pdmMicSai2Handle.dmaRequest        = (uint8_t)kDmaRequestMuxSai2Rx;
    g_pdmMicSai2Handle.pongFlag          = MIC3_PONG_EVENT;
    g_pdmMicSai2Handle.pingFlag          = MIC3_PING_EVENT;
    g_pdmMicSai2Handle.errorFlag         = PDM_ERROR_FLAG;
    g_pdmMicSai2Handle.pingPongBuffer[0] = (uint32_t *)(&g_Sai2PdmPingPong[0][0]);
    g_pdmMicSai2Handle.pingPongBuffer[1] = (uint32_t *)(&g_Sai2PdmPingPong[1][0]);
#endif

#if SAI1_CH_COUNT
    PDM_MIC_ConfigMic(&g_pdmMicSai1Handle);
#endif

#if USE_SAI2_MIC
    PDM_MIC_ConfigMic(&g_pdmMicSai2Handle);
#endif

#if USE_SAI1_RX_DATA0_MIC || USE_SAI1_RX_DATA1_MIC
    PDM_MIC_StartMic(&g_pdmMicSai1Handle);
#endif

#if USE_SAI2_MIC
    PDM_MIC_StartMic(&g_pdmMicSai2Handle);
#endif

#if USE_TFA
    if (NULL != s_config.feedbackInit)
    {
        status = s_config.feedbackInit();
    }
#endif /* USE_TFA */

    dspStatus = pdm_to_pcm_dsp_init(&dspMemPool);

    uint32_t *dspScratch = NULL;
    if (kDspSuccess == dspStatus)
    {
        dspStatus = SLN_DSP_create_scratch_buf(&dspMemPool, &dspScratch, pvPortMalloc);
    }

    if (kDspSuccess != dspStatus)
    {
        configPRINTF(("ERROR [%d]: DSP Toolbox initialization has failed!\r\n", dspStatus));
    }

    g_micsOn            = true;
    g_decimationStarted = true;

    for (;;)
    {
        preProcessEvents = xEventGroupWaitBits(s_PdmDmaEventGroup, PDM_PCM_EVENT_MASK, pdTRUE, pdFALSE,
                                               portTICK_PERIOD_MS * PDM_PCM_EVENT_TIMEOUT_MS);

        /* If no event group bit is set it means that the timeout was triggered */
        if ((preProcessEvents & PDM_PCM_EVENT_MASK) == 0)
        {
            /* The timeout is triggered so it means that we are not receiving any data from the mics.
             * This can be cause by an error in the SAI interface and we need to reset the mics to recover
             * or the mics are off, in which case we just continue.
             */
            if (true == g_micsOn)
            {
#ifndef NO_DEBUG_MICS
                configPRINTF(("SAI stopped working. Repairing it.\r\n"));
#endif
                pdm_to_pcm_mics_off();
                pdm_to_pcm_mics_on();
            }

            continue;
        }

#if USE_TFA
        if (preProcessEvents & AMP_ERROR_FLAG)
        {
#ifndef NO_DEBUG_MICS
            configPRINTF(("Loopback stopped working. Repairing it.\r\n"));
#endif

            pdm_to_pcm_mics_off();
            pdm_to_pcm_mics_on();
        }

        if (preProcessEvents & AMP_REFERENCE_SIGNAL)
        {
            dspStatus =
                SLN_DSP_downsample_by_3(&dspMemPool, AMP_DSP_STREAM, s_config.feedbackBuffer, PCM_AMP_SAMPLE_COUNT,
                                        &s_ampOutput[u32AmpIndex * PCM_SINGLE_CH_SMPL_COUNT]);

            if (u32AmpIndex >= 1)
            {
                u32AmpIndex = 0;
            }
            else
            {
                u32AmpIndex++;
            }

            postProcessEvents |= AMP_REFERENCE_SIGNAL;
            preProcessEvents &= ~AMP_REFERENCE_SIGNAL;
        }
#endif /* USE_TFA */

        if (preProcessEvents & MIC1_PING_EVENT)
        {
#if SAI1_CH_COUNT == 2

#if USE_MQS
            pdm_to_pcm_prepare_amp_data(PCM_PING);
#endif /* USE_MQS */

            if (kDspSuccess != SLN_DSP_pdm_to_pcm_multi_ch(&dspMemPool, 1U, 2U, &(g_Sai1PdmPingPong[0U][0U]),
                                                           &(s_pcmStream[0][0]), dspScratch))
            {
                configPRINTF(("PDM to PCM Conversion error: %d\r\n", kDspSuccess));
            }
#else
            /* Perform PDM to PCM Conversion */
            SLN_DSP_pdm_to_pcm(&dspMemPool, MIC1_DSP_STREAM, (uint8_t *)(&g_Sai1PdmPingPong[0U][0U]),
                               &(s_pcmStream[0U][0U]));

#endif

            postProcessEvents |= MIC1_PING_EVENT;
            postProcessEvents |= MIC2_PING_EVENT;

            preProcessEvents &= ~MIC1_PING_EVENT;
            preProcessEvents &= ~MIC2_PING_EVENT;
        }

        if (preProcessEvents & MIC1_PONG_EVENT)
        {
#if SAI1_CH_COUNT == 2

#if USE_MQS
            pdm_to_pcm_prepare_amp_data(PCM_PONG);
#endif /* USE_MQS */

            if (kDspSuccess != SLN_DSP_pdm_to_pcm_multi_ch(&dspMemPool, MIC1_DSP_STREAM, SAI1_CH_COUNT,
                                                           &(g_Sai1PdmPingPong[1U][0U]), &(s_pcmStream[1U][0U]),
                                                           dspScratch))
            {
                configPRINTF(("PDM to PCM Conversion error: %d\r\n", kDspSuccess));
            }

#else

            /* Perform PDM to PCM Conversion */
            SLN_DSP_pdm_to_pcm(&dspMemPool, MIC1_DSP_STREAM, (uint8_t *)(&g_Sai1PdmPingPong[1U][0U]),
                               &(s_pcmStream[1U][0U]));

#endif

            postProcessEvents |= MIC1_PONG_EVENT;
            postProcessEvents |= MIC2_PONG_EVENT;

            preProcessEvents &= ~MIC1_PONG_EVENT;
            preProcessEvents &= ~MIC2_PONG_EVENT;
        }

#if (USE_SAI2_MIC)
        if (preProcessEvents & MIC3_PING_EVENT)
        {
            GPIO_PinWrite(GPIO2, 12, 1U);

            /* Perform PDM to PCM Conversion */
            SLN_DSP_pdm_to_pcm(&dspMemPool, MIC3_DSP_STREAM, (uint8_t *)(&g_Sai2PdmPingPong[0U][0U]),
                               &(s_pcmStream[0U][MIC3_START_IDX]));

            postProcessEvents |= MIC3_PING_EVENT;
            preProcessEvents &= ~MIC3_PING_EVENT;

            GPIO_PinWrite(GPIO2, 12, 0U);
        }

        if (preProcessEvents & MIC3_PONG_EVENT)
        {
            /* Perform PDM to PCM Conversion */
            SLN_DSP_pdm_to_pcm(&dspMemPool, MIC3_DSP_STREAM, (uint8_t *)(&g_Sai2PdmPingPong[1U][0U]),
                               &(s_pcmStream[1U][MIC3_START_IDX]));

            postProcessEvents |= MIC3_PONG_EVENT;
            preProcessEvents &= ~MIC3_PONG_EVENT;
        }

        if (preProcessEvents & PDM_ERROR_FLAG)
        {
            configPRINTF(("[PDM-PCM] - Missed Event \r\n"));
            preProcessEvents &= ~PDM_ERROR_FLAG;
        }
#else
        postProcessEvents |= MIC3_PING_EVENT;
        postProcessEvents |= MIC3_PONG_EVENT;
#endif

        if (EVT_PING_MASK == (postProcessEvents & EVT_PING_MASK))
        {
            if (NULL == *(s_config.processingTask))
            {
                configPRINTF(("ERROR: Audio Processing Task Handle NULL!\r\n"));
            }
            else
            {
                xTaskNotify(*(s_config.processingTask), (1U << PCM_PING), eSetBits);
            }

            postProcessEvents &= ~(EVT_PING_MASK);
        }
        else if (EVT_PONG_MASK == (postProcessEvents & EVT_PONG_MASK))
        {
            if (NULL == *(s_config.processingTask))
            {
                configPRINTF(("ERROR: Audio Processing Task Handle NULL!\r\n"));
            }
            else
            {
                xTaskNotify(*(s_config.processingTask), (1U << PCM_PONG), eSetBits);
            }

            postProcessEvents &= ~(EVT_PONG_MASK);
        }
    }
}

void pdm_to_pcm_mics_off(void)
{
    /* Do nothing if already off or decimation not started */
    if ((true == g_micsOn) && (true == g_decimationStarted))
    {
#if SAI1_CH_COUNT
        PDM_MIC_StopMic(&g_pdmMicSai1Handle);
#endif

#if USE_SAI2_MIC
        PDM_MIC_StopMic(&g_pdmMicSai2Handle);
#endif

#if SAI1_CH_COUNT
        g_pdmMicSai1Handle.pingPongTracker = 0;
#endif

#if USE_SAI2_MIC
        g_pdmMicSai2Handle.pingPongTracker = 0;
#endif

        memset(s_pcmStream, 0, sizeof(pcmPingPong_t));

        /* amplifier loopback */
        if (NULL != s_config.feedbackDisable)
        {
            s_config.feedbackDisable();
        }

        /* update flag */
        g_micsOn = false;
    }
}

void pdm_to_pcm_mics_on(void)
{
    /* Do nothing if already on or decimation not started */
    if ((false == g_micsOn) && (true == g_decimationStarted))
    {
        if (s_PdmDmaEventGroup != NULL)
        {
            xEventGroupClearBits(s_PdmDmaEventGroup, 0x00FFFFFF);
        }
        preProcessEvents  = 0;
        postProcessEvents = 0;
        u32AmpIndex       = 1;

#if SAI1_CH_COUNT
        PDM_MIC_ConfigMic(&g_pdmMicSai1Handle);
#endif

#if USE_SAI2_MIC
        PDM_MIC_ConfigMic(&g_pdmMicSai2Handle);
#endif

#if USE_SAI1_RX_DATA0_MIC || USE_SAI1_RX_DATA1_MIC
        PDM_MIC_StartMic(&g_pdmMicSai1Handle);
#endif

#if USE_SAI2_MIC
        PDM_MIC_StartMic(&g_pdmMicSai2Handle);
#endif

        /* amplifier loopback */
        if (NULL != s_config.feedbackEnable)
        {
            s_config.feedbackEnable();
        }

        /* update flag */
        g_micsOn = true;
    }
}
