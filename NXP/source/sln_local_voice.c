/*
 * Copyright 2021 NXP.
 * This software is owned or controlled by NXP and may only be used strictly in accordance with the
 * license terms that accompany it. By expressly accepting such terms or by downloading, installing,
 * activating and/or otherwise using the software, you are agreeing that you have read, and that you
 * agree to comply with and are bound by, such license terms. If you do not agree to be bound by the
 * applicable license terms, then you may not retain, install, activate or otherwise use the software.d
 */

#include "sln_local_voice.h"
#include "sln_local_voice_model.h"

#include "IndexToCommand_en.h"
#include "IndexToCommand_zh.h"
#include "IndexToCommand_de.h"
#include "IndexToCommand_fr.h"
#include "fsl_debug_console.h"
#include "sln_flash.h"
#include "sln_flash_mgmt.h"
#include "sln_cfg_file.h"
#include "sln_RT10xx_RGB_LED_driver.h"
#include "audio_processing_task.h"
#include "sln_amplifier.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define NUM_SAMPLES_AFE_OUTPUT (480)

/*
 * For more details regarding WWs and CMDs memory space make sure to check
 * Section 5.1.2 from the Developer's Guide
 */
#define WAKE_WORD_MEMPOOL_SIZE    (50 * 1024)
#define ZH_WAKE_WORD_MEMPOOL_SIZE (90 * 1024)
#define COMMAND_MEMPOOL_SIZE      (90 * 1024)

/*******************************************************************************
 * Variables
 ******************************************************************************/
SDK_ALIGN(uint8_t __attribute__((section(".bss.$SRAM_DTC"))) g_memPoolWLang1[WAKE_WORD_MEMPOOL_SIZE], 8);
SDK_ALIGN(uint8_t __attribute__((section(".bss.$SRAM_OC_CACHEABLE"))) g_memPoolCmd[COMMAND_MEMPOOL_SIZE], 8);

#if MULTILINGUAL
// NOTE: Chinese with Tone Recognition model takes larger memory pool than the other languages.
// Make sure Chinese model is placed in g_memPoolWLang2 with ZH_WAKE_WORD_MEMPOOL_SIZE.
// Also, make sure Chinese model is installed in the same order for install_language() and install_inference_engine().
SDK_ALIGN(uint8_t __attribute__((section(".bss.$SRAM_OC_CACHEABLE"))) g_memPoolWLang2[ZH_WAKE_WORD_MEMPOOL_SIZE], 8);
SDK_ALIGN(uint8_t __attribute__((section(".bss.$SRAM_DTC"))) g_memPoolWLang3[WAKE_WORD_MEMPOOL_SIZE], 8);
SDK_ALIGN(uint8_t __attribute__((section(".bss.$SRAM_OC_CACHEABLE"))) g_memPoolWLang4[WAKE_WORD_MEMPOOL_SIZE], 8);
#endif

extern QueueHandle_t g_xSampleQueue;
extern TaskHandle_t appTaskHandle;
extern oob_demo_control_t oob_demo_control;
extern bool g_SW1Pressed;
extern int g_bypass_voice_engine;

struct asr_language_model g_asrLangModel[MAX_INSTALLED_LANGUAGES] = {0};
struct asr_inference_engine g_asrInfWW[NUM_INFERENCES_WW]         = {0};
struct asr_inference_engine g_asrInfCMD                           = {0};
asr_control_t g_asrControl                                        = {0};
app_asr_shell_commands_t appAsrShellCommands                      = {};

/*******************************************************************************
 * Code
 ******************************************************************************/

/*!
 * @brief Utility function to extract indices from bitwise variable. this is used for asr_inference_t.
 */
static unsigned int decode_bitshift(unsigned int x)
{
    unsigned int y = 1; // starting from index 1 (not 0)
    while (x >>= 1)
        y++;
    return y;
}

/*!
 * @brief Utility function to break model binary pack into multiple groups where each group will represent an
 * inference engine combined with base model.
 */
static signed int unpackBin(unsigned char lpbyBin[], unsigned char *lppbyModel[], int32_t nMaxNumModel)
{
    unsigned int *lpnBin     = (unsigned int *)lpbyBin;
    signed int nNumBin       = lpnBin[0];
    unsigned int *lpnBinSize = lpnBin + 1;
    signed int i;

    lppbyModel[0] = (unsigned char *)(lpnBinSize + nNumBin);
    for (i = 1; i < nNumBin; i++)
    {
        if (i >= nMaxNumModel)
            break;
        lppbyModel[i] = lppbyModel[i - 1] + lpnBinSize[i - 1];
    }

    return i;
}

/*!
 * @brief Language model installation.
 */
int32_t install_language(asr_control_t *pAsrCtrl,
                         struct asr_language_model *pLangModel,
                         asr_language_t lang,
                         unsigned char *pAddrBin,
                         uint8_t nGroups)
{
    sln_asr_local_states_t status = kAsrLocalSuccess;
    uint8_t nGroupsMapID          = nGroups - 2; // -1 cause there is no MapID group for base model,
                                        // -1 cause the nGroups contains 1 more section with the no of mapID binaries

    if (lang && pAddrBin && nGroups)
    {
        pLangModel->iWhoAmI = lang;
        pLangModel->addrBin = pAddrBin;
        pLangModel->nGroups = nGroups;
        pLangModel->next    = pAsrCtrl->langModel;
        pAsrCtrl->langModel = pLangModel;

        if ((status = unpackBin(pAddrBin, pLangModel->addrGroup, nGroups)) <
            nGroups) // unpack group addresses from model binary
        {
            configPRINTF(("Invalid bin. Error Code: %d.\r\n", status));
        }
        else
        {
            if ((status = unpackBin(pLangModel->addrGroup[nGroups - 1], pLangModel->addrGroupMapID, nGroupsMapID)) <
                (nGroupsMapID)) // unpack group addresses from mapID binary
            {
                configPRINTF(("Invalid bin. Error Code: %d.\r\n", status));
            }
        }
    }
    else
        status = 1;

    return status;
}

/*!
 * @brief Inference engine installation.
 */
uint32_t install_inference_engine(asr_control_t *pAsrCtrl,
                                  struct asr_inference_engine *pInfEngine,
                                  asr_language_t lang,
                                  asr_inference_t infType,
                                  char **idToString,
                                  unsigned char *addrMemPool,
                                  uint32_t sizeMemPool)
{
    sln_asr_local_states_t status = kAsrLocalSuccess;

    if (pAsrCtrl && pInfEngine && lang && infType && addrMemPool && sizeMemPool)
    {
        pInfEngine->iWhoAmI_inf  = infType;
        pInfEngine->iWhoAmI_lang = lang;
        pInfEngine->handler      = NULL;
        pInfEngine->nGroups      = 2;
        pInfEngine->idToKeyword  = idToString;
        pInfEngine->memPool      = addrMemPool;
        pInfEngine->memPoolSize  = sizeMemPool;
        if (infType == ASR_WW)
        {                                                  // linked list for WW engines
            pInfEngine->next      = pAsrCtrl->infEngineWW; // the end of pInfEngine->next should be NULL
            pAsrCtrl->infEngineWW = pInfEngine;
        }
        else
        { // linked list for CMD engines. Dialog demo needs a linked list of CMD engines.
            pInfEngine->next       = pAsrCtrl->infEngineCMD;
            pAsrCtrl->infEngineCMD = pInfEngine;
        }
    }
    else
        status = kAsrLocalInstallFailed;

    return status;
}

/*!
 * @brief Checks memory pool size for WW / CMD engines.
 */
void verify_inference_handler(struct asr_inference_engine *p)
{
    sln_asr_local_states_t status = kAsrLocalSuccess;
    int32_t mem_usage;
    int32_t mem_size_ww;

    mem_usage = SLN_ASR_LOCAL_Verify(p->addrGroup[0], (unsigned char **)&p->addrGroup[1], 1, k_nMaxTime);

    if (p->iWhoAmI_lang == ASR_CHINESE)
    {
        mem_size_ww = ZH_WAKE_WORD_MEMPOOL_SIZE;
    }
    else
    {
        mem_size_ww = WAKE_WORD_MEMPOOL_SIZE;
    }

    if ((p->iWhoAmI_inf == ASR_WW) && (mem_usage > mem_size_ww))
    {
        configPRINTF(("Memory size %d for WW exceeds the memory pool %d!\r\n", mem_usage, WAKE_WORD_MEMPOOL_SIZE));
        status = kAsrLocalOutOfMemory;
    }
    else if ((p->iWhoAmI_inf != ASR_WW) && (mem_usage > COMMAND_MEMPOOL_SIZE))
    {
        configPRINTF(("Memory size %d for CMD exceeds the memory pool %d!\r\n", mem_usage, COMMAND_MEMPOOL_SIZE));
        status = kAsrLocalOutOfMemory;
    }

    if (status != kAsrLocalSuccess)
    {
        RGB_LED_SetColor(LED_COLOR_ORANGE);
        while (1)
        {
            vTaskDelay(1000);
        }
    }
}

/*!
 * @brief Handler should be set with valid
 *  p->addrGroup[0] (base model address) and
 *  p->addrGroup[1] (application group such as WW, CMD for IoT, etc)
 */
void set_inference_handler(struct asr_inference_engine *p)
{
    int status = kAsrLocalSuccess;

    p->handler = SLN_ASR_LOCAL_Init(p->addrGroup[0], (unsigned char **)&p->addrGroup[1], 1, k_nMaxTime, p->memPool,
                                    p->memPoolSize, (signed int *)&status);

    if (status != kAsrLocalSuccess)
    {
        configPRINTF(("Could not initialize ASR engine. Please check language settings or license limitations!\r\n"));
        RGB_LED_SetColor(LED_COLOR_RED);
        while (1)
        {
            vTaskDelay(1000);
        }
    }

    if ((status = SLN_ASR_LOCAL_Set_CmdMapID(p->handler, &p->addrGroupMapID, 1)) != kAsrLocalSuccess)
    {
        configPRINT_STRING("Fail to set map id! - %d\r\n", status);
    }
}

/*!
 * @brief Handler should be reset after WW or CMDs are detected.
 */
void reset_inference_handler(struct asr_inference_engine *p)
{
    SLN_ASR_LOCAL_Reset(p->handler);
}

/*!
 * @brief Initialize WW inference engine from the installed language models.
 *  After, pInfEngine should be a linked list of the installed languages.
 */
void init_WW_engine(asr_control_t *pAsrCtrl, asr_inference_t infType)
{
    struct asr_inference_engine *pInfEngine;
    struct asr_language_model *pLang;
    int idx       = decode_bitshift(ASR_WW); // decode the bitwise ASR_WW which is 1.
    int idx_mapID = idx - 1;                 // the index for mapIDs starts from 0 instead of 1

    pInfEngine = pAsrCtrl->infEngineWW;
    for (pLang = pAsrCtrl->langModel; pLang != NULL; pLang = pLang->next)
    {
        if ((infType == ASR_CMD_LED) || (infType == ASR_CMD_DIALOGIC_1))
        {
            // only activate English for these demos
            if (pLang->iWhoAmI != ASR_ENGLISH)
            {
                continue;
            }
        }
        pInfEngine->addrGroup[0]   = pLang->addrGroup[0];              // language model's base
        pInfEngine->addrGroup[1]   = pLang->addrGroup[idx];            // language model's wake word group
        pInfEngine->addrGroupMapID = pLang->addrGroupMapID[idx_mapID]; // language model's wake word mapID group

        verify_inference_handler(pInfEngine); // verify inference handler, checking mem pool size
        set_inference_handler(pInfEngine);    // set inf engine to ww mode for each language.

        if ((infType == ASR_CMD_LED) || (infType == ASR_CMD_DIALOGIC_1))
        {
            // exit loop after activating English
            if (pLang->iWhoAmI == ASR_ENGLISH)
            {
                break;
            }
        }

        pInfEngine = pInfEngine->next; // the end of pInfEngine->next should be NULL.
    }
}

/*!
 * @brief Initialize CMD inference engine from the installed language models.
 *  After, pInfEngine does not need to be a linked list for Demo #1 and #2 but does for Demo #3 (dialog).
 */
void init_CMD_engine(asr_control_t *pAsrCtrl, asr_inference_t infType)
{
    struct asr_inference_engine *pInfEngine;
    struct asr_language_model *pLang;
    int idx       = decode_bitshift(infType); // decode the bitwise infType variable.
    int idx_mapID = idx - 1;                  // the index for mapIDs starts from 0 instead of 1

    pInfEngine = pAsrCtrl->infEngineCMD;
    if ((infType == ASR_CMD_LED) || (infType == ASR_CMD_DIALOGIC_1))
    {
        for (pLang = pAsrCtrl->langModel; pLang != NULL; pLang = pLang->next)
        {
            if (pLang->iWhoAmI == ASR_ENGLISH)
            {
                pInfEngine->addrGroup[0] = pLang->addrGroup[0]; // language model's base
                pInfEngine->addrGroup[1] =
                    pLang->addrGroup[idx]; // language model's infType group which is ASR_CMD_LED.
                pInfEngine->addrGroupMapID =
                    pLang->addrGroupMapID[idx_mapID]; // the selected language model's mapID group
            }
        }
    }
    else
    {
        pLang                    = pAsrCtrl->langModel; // langModel for CMD inf engine is selected when WW is detected.
        pInfEngine->addrGroup[0] = pLang->addrGroup[0]; // the selected language model's base
        pInfEngine->addrGroup[1] = pLang->addrGroup[idx];              // the selected language model's infType group
        pInfEngine->addrGroupMapID = pLang->addrGroupMapID[idx_mapID]; // the selected language model's mapID group
    }

    verify_inference_handler(pInfEngine); // verify inference handler, checking mem pool size
    set_inference_handler(pInfEngine);    // set inf engine to ww mode for each language.
}

/*!
 * @brief Set language WW recognition engines.
 */
void set_WW_engine(asr_control_t *pAsrCtrl)
{
    struct asr_inference_engine *pInf = pAsrCtrl->infEngineWW;

    for (pInf = pAsrCtrl->infEngineWW; pInf != NULL; pInf = pInf->next)
    {
        set_inference_handler(pInf);
    }
}

/*!
 * @brief Reset language WW recognition engines.
 */
void reset_WW_engine(asr_control_t *pAsrCtrl)
{
    struct asr_inference_engine *pInf = pAsrCtrl->infEngineWW;

    for (pInf = pAsrCtrl->infEngineWW; pInf != NULL; pInf = pInf->next)
    {
        reset_inference_handler(pInf);
    }
}

/*!
 * @brief Set specific language CMD recognition engine, post WW detection.
 */
void set_CMD_engine(asr_control_t *pAsrCtrl, asr_language_t langType, asr_inference_t infCMDType, char **cmdString)
{
    struct asr_language_model *pLang;
    struct asr_inference_engine *pInf = pAsrCtrl->infEngineCMD;
    int idx                           = decode_bitshift(infCMDType); // decode the bitwise infType variable
    int idx_mapID                     = idx - 1;                     // the index for mapIDs starts from 0 instead of 1

    for (pLang = pAsrCtrl->langModel; pLang != NULL; pLang = pLang->next)
    {
        if (pLang->iWhoAmI == langType)
        {
            pInf->iWhoAmI_inf  = infCMDType;
            pInf->iWhoAmI_lang = langType;
            pInf->addrGroup[0] = pLang->addrGroup[0]; // base model. should be same with WW's base
            if (pLang->addrGroup[idx] != NULL)
            {
                pInf->addrGroup[1]   = pLang->addrGroup[idx];
                pInf->addrGroupMapID = pLang->addrGroupMapID[idx_mapID];
            }
            set_inference_handler(pInf);
            pInf->idToKeyword = cmdString;
            break; // exit for loop, once pInf is set with the intended language
        }
    }
}

/*!
 * @brief Reset specific language CMD recognition engine.
 */
void reset_CMD_engine(asr_control_t *pAsrCtrl)
{
    struct asr_inference_engine *pInf = pAsrCtrl->infEngineCMD;

    reset_inference_handler(pInf);
}

/*!
 * @brief Process audio stream to detect wake words or commands.
 */
int asr_process_audio_buffer(void *handler, int16_t *audBuff, uint16_t bufSize, asr_inference_t infType)
{
    int status = 0;
    // reset values
    g_asrControl.result.keywordID[0] = 0xFFFF;
    g_asrControl.result.keywordID[1] = 0xFFFF;
    g_asrControl.result.cmdMapID     = 0xFF;

    status = SLN_ASR_LOCAL_Process(handler, audBuff, bufSize, &g_asrControl.result);

    return status;
}

char *asr_get_string_by_id(struct asr_inference_engine *pInfEngine, int32_t id)
{
    return pInfEngine->idToKeyword[id];
}

char **get_cmd_string(asr_language_t asrLang, asr_inference_t infCMDType)
{
    char **retString = NULL;

    switch (asrLang)
    {
        case ASR_ENGLISH:
            if (infCMDType == ASR_CMD_IOT)
                retString = cmd_iot_en;
            else if (infCMDType == ASR_CMD_ELEVATOR)
                retString = cmd_elevator_en;
            else if (infCMDType == ASR_CMD_AUDIO)
                retString = cmd_audio_en;
            else if (infCMDType == ASR_CMD_WASH)
                retString = cmd_wash_en;
            else if (infCMDType == ASR_CMD_LED)
                retString = cmd_led_en;
            else if (infCMDType == ASR_CMD_DIALOGIC_1)
                retString = cmd_dialogic_1_en;
            break;
        case ASR_CHINESE:
            if (infCMDType == ASR_CMD_IOT)
                retString = cmd_iot_zh;
            else if (infCMDType == ASR_CMD_ELEVATOR)
                retString = cmd_elevator_zh;
            else if (infCMDType == ASR_CMD_AUDIO)
                retString = cmd_audio_zh;
            else if (infCMDType == ASR_CMD_WASH)
                retString = cmd_wash_zh;
            else if (infCMDType == ASR_CMD_NORMAL)
            	retString = cmd_normal_zh;
            break;
        case ASR_GERMAN:
            if (infCMDType == ASR_CMD_IOT)
                retString = cmd_iot_de;
            else if (infCMDType == ASR_CMD_ELEVATOR)
                retString = cmd_elevator_de;
            else if (infCMDType == ASR_CMD_AUDIO)
                retString = cmd_audio_de;
            else if (infCMDType == ASR_CMD_WASH)
                retString = cmd_wash_de;
            break;
        case ASR_FRENCH:
            if (infCMDType == ASR_CMD_IOT)
                retString = cmd_iot_fr;
            else if (infCMDType == ASR_CMD_ELEVATOR)
                retString = cmd_elevator_fr;
            else if (infCMDType == ASR_CMD_AUDIO)
                retString = cmd_audio_fr;
            else if (infCMDType == ASR_CMD_WASH)
                retString = cmd_wash_fr;
            break;
        default:
            retString = cmd_led_en;
            break;
    }

    return retString;
}

void initialize_asr(void)
{
    asr_inference_t demoType                     = appAsrShellCommands.demo;
    asr_language_t lang[MAX_INSTALLED_LANGUAGES] = {UNDEFINED_LANGUAGE};
    asr_language_t langShell                     = 0;

#if MULTILINGUAL
    // make sure LED & DIALOG demos have ASR_ENGLISH as default enabled.
    if ((appAsrShellCommands.demo == ASR_CMD_LED) || (appAsrShellCommands.demo == ASR_CMD_DIALOGIC_1) ||
        (appAsrShellCommands.multilingual == UNDEFINED_LANGUAGE))
#endif
    {
        appAsrShellCommands.multilingual = ASR_ENGLISH;
        oob_demo_control.language        = ASR_ENGLISH;
    }

    langShell = appAsrShellCommands.multilingual;
    lang[0]   = langShell & ASR_ENGLISH; // first language
    lang[1]   = langShell & ASR_CHINESE; // second
    lang[2]   = langShell & ASR_GERMAN;  // third
    lang[3]   = langShell & ASR_FRENCH;  // fourth

    // NULL to ensure the end of linked list.
    g_asrControl.langModel    = NULL;
    g_asrControl.infEngineWW  = NULL;
    g_asrControl.infEngineCMD = NULL;

//#if MULTILINGUAL
    // install multilingual
    install_language(&g_asrControl, &g_asrLangModel[3], lang[3], (unsigned char *)&oob_demo_fr_begin, NUM_GROUPS);
    install_language(&g_asrControl, &g_asrLangModel[2], lang[2], (unsigned char *)&oob_demo_de_begin, NUM_GROUPS);
    install_language(&g_asrControl, &g_asrLangModel[1], lang[1], (unsigned char *)&oob_demo_zh_begin, NUM_GROUPS_ZH);
//#endif

    install_language(&g_asrControl, &g_asrLangModel[0], lang[0], (unsigned char *)&oob_demo_en_begin, NUM_GROUPS_EN);

#if MULTILINGUAL
    install_inference_engine(&g_asrControl, &g_asrInfWW[3], lang[3], ASR_WW, ww_fr, &g_memPoolWLang4[0],
                             WAKE_WORD_MEMPOOL_SIZE); // ww language4
    install_inference_engine(&g_asrControl, &g_asrInfWW[2], lang[2], ASR_WW, ww_de, &g_memPoolWLang3[0],
                             WAKE_WORD_MEMPOOL_SIZE); // ww language3
    install_inference_engine(&g_asrControl, &g_asrInfWW[1], lang[1], ASR_WW, ww_zh, &g_memPoolWLang2[0],
                             ZH_WAKE_WORD_MEMPOOL_SIZE); // ww language2
#endif

    install_inference_engine(&g_asrControl, &g_asrInfWW[0], lang[0], ASR_WW, ww_en, &g_memPoolWLang1[0],
                             WAKE_WORD_MEMPOOL_SIZE); // ww language1

    // CMD inference engine will be reset with detected language after WW is detected
    install_inference_engine(&g_asrControl, &g_asrInfCMD, ASR_ENGLISH, demoType, cmd_led_en, &g_memPoolCmd[0],
                             COMMAND_MEMPOOL_SIZE); // commands, setting up with defaults

    // init
    init_WW_engine(&g_asrControl, demoType);
    init_CMD_engine(&g_asrControl, demoType);
    oob_demo_control.ledCmd = UNDEFINED_COMMAND;
}

void print_asr_session(int status)
{
    switch (status)
    {
        case ASR_SESSION_STARTED:
            //configPRINTF(("\r\n[ASR] Session started\r\n"));
            //PRINTF("\r\n[ASR] Session started\r\n");
            configPRINTF(("2012\r\n"));
            PRINTF("2012\r\n");
            break;
        case ASR_SESSION_ENDED:
            //configPRINTF(("[ASR] Session ended\r\n"));
            //PRINTF("[ASR] Session ended\r\n");
        	configPRINTF(("-10000\r\n"));
            PRINTF("-10000\r\n");
            break;
        case ASR_SESSION_TIMEOUT:
            //configPRINTF(("[ASR] Timed out waiting for response\r\n"));
            //PRINTF("[ASR] Timed out waiting for response\r\n");
            configPRINTF(("-777\r\n"));
            PRINTF("-777\r\n");
            break;
    }
}

/*!
 * @brief ASR main task
 */
void local_voice_task(void *arg)
{
    int16_t pi16Sample[NUM_SAMPLES_AFE_OUTPUT];
    uint32_t len          = 0;
    uint32_t statusFlash  = 0;
    asr_events_t asrEvent = ASR_SESSION_ENDED;
    struct asr_inference_engine *pInfWW;
    struct asr_inference_engine *pInfCMD;
    char **cmdString;

    // Read Shell Commands Parameters from flash memory. If not available, initialize and write into flash memory.
    statusFlash = SLN_FLASH_MGMT_Read(ASR_SHELL_COMMANDS_FILE_NAME, (uint8_t *)&appAsrShellCommands, &len);
    if (statusFlash != SLN_FLASH_MGMT_OK)
    {
        configPRINTF(("Failed reading local demo configuration from flash memory.\r\n"));
    }

    if (appAsrShellCommands.status != WRITE_SUCCESS)
    {
        appAsrShellCommands.demo         = ASR_CMD_LED;
        appAsrShellCommands.followup     = ASR_FOLLOWUP_OFF;
        appAsrShellCommands.multilingual = ASR_ENGLISH;
        appAsrShellCommands.mute         = ASR_MUTE_OFF;
        appAsrShellCommands.ptt          = ASR_PTT_OFF;
        appAsrShellCommands.timeout      = TIMEOUT_TIME_IN_MS;
        appAsrShellCommands.volume       = 55;
        appAsrShellCommands.status       = WRITE_SUCCESS;
        appAsrShellCommands.asrCfg       = ASR_CFG_DEMO_NO_CHANGE;
        statusFlash = SLN_FLASH_MGMT_Save(ASR_SHELL_COMMANDS_FILE_NAME, (uint8_t *)&appAsrShellCommands,
                                          sizeof(app_asr_shell_commands_t));
        if ((statusFlash == SLN_FLASH_MGMT_EOVERFLOW) || (statusFlash == SLN_FLASH_MGMT_EOVERFLOW2))
        {
            statusFlash = SLN_FLASH_MGMT_Erase(ASR_SHELL_COMMANDS_FILE_NAME);
            statusFlash = SLN_FLASH_MGMT_Save(ASR_SHELL_COMMANDS_FILE_NAME, (uint8_t *)&appAsrShellCommands,
                                              sizeof(app_asr_shell_commands_t));
            if (statusFlash != SLN_FLASH_MGMT_OK)
            {
                configPRINTF(("Failed writing local demo configuration in flash memory.\r\n"));
            }
        }
        else if (statusFlash != SLN_FLASH_MGMT_OK)
        {
            configPRINTF(("Failed writing local demo configuration in flash memory.\r\n"));
        }
    }

    initialize_asr();
    // We need to reset asrCfg state so we won't remember an unprocessed demo change that was saved in flash
    appAsrShellCommands.asrCfg = ASR_CFG_DEMO_NO_CHANGE;

    while (!g_xSampleQueue)
        vTaskDelay(10);

    while (1)
    {
        if (xQueueReceive(g_xSampleQueue, pi16Sample, portMAX_DELAY) != pdPASS)
        {
            configPRINTF(("Could not receive from the queue\r\n"));
        }

        // bypass while playing audio clip
        if (g_bypass_voice_engine > 0)
        {
            g_bypass_voice_engine -= NUM_SAMPLES_AFE_OUTPUT;
            continue;
        }
        g_bypass_voice_engine = 0;

        // push-to-talk
        if (g_SW1Pressed == true && asrEvent == ASR_SESSION_ENDED && appAsrShellCommands.ptt == ASR_PTT_ON)
        {
            g_SW1Pressed             = false;
            g_asrControl.sampleCount = 0;

            // only English CMD
            cmdString = get_cmd_string(ASR_ENGLISH, appAsrShellCommands.demo);
            set_CMD_engine(&g_asrControl, ASR_ENGLISH, appAsrShellCommands.demo, cmdString);
            oob_demo_control.language = ASR_ENGLISH;

            asrEvent = ASR_SESSION_STARTED;
            print_asr_session(asrEvent); // print ASR session status

            xTaskNotify(appTaskHandle, kWakeWordDetected, eSetBits);
        }

        // continue listening to wake words in the selected languages. pInfWW is language specific.
        if (asrEvent == ASR_SESSION_ENDED && appAsrShellCommands.ptt == ASR_PTT_OFF)
        {
            for (pInfWW = g_asrControl.infEngineWW; pInfWW != NULL; pInfWW = pInfWW->next)
            {
                if (asr_process_audio_buffer(pInfWW->handler, pi16Sample, NUM_SAMPLES_AFE_OUTPUT,
                                             pInfWW->iWhoAmI_inf) == kAsrLocalDetected)
                {
                    if (asr_get_string_by_id(pInfWW, g_asrControl.result.keywordID[0]) != NULL)
                    {
                        asrEvent = ASR_SESSION_STARTED;
                        print_asr_session(asrEvent);
                        configPRINTF(("0\r\n"));
                        /*configPRINTF(("[ASR] Wake Word: %s(%d) - MapID(%d)\r\n",
                                      asr_get_string_by_id(pInfWW, g_asrControl.result.keywordID[0]),
                                      g_asrControl.result.keywordID[0], g_asrControl.result.cmdMapID));*/
                        if (appAsrShellCommands.cmdresults == ASR_CMD_RES_ON)
                        {
                            configPRINTF(("      Trust: %d, SGDiff: %d\r\n", g_asrControl.result.trustScore,
                                          g_asrControl.result.SGDiffScore));
                        }
                        /*PRINTF("[ASR] Wake Word: %s(%d) \r\n",
                               asr_get_string_by_id(pInfWW, g_asrControl.result.keywordID[0]),
                               g_asrControl.result.keywordID[0]);*/
                        PRINTF("0\r\n");

                        if (appAsrShellCommands.demo ==
                            ASR_CMD_LED) // only English CMD for LED demo, multi-lingual WW is possible.
                        {
                            cmdString = cmd_led_en;
                            set_CMD_engine(&g_asrControl, ASR_ENGLISH, ASR_CMD_LED, cmdString);
                        }
                        else if (appAsrShellCommands.demo &
                                 (ASR_CMD_DIALOGIC_1 | ASR_CMD_DIALOGIC_2_TEMPERATURE |
                                  ASR_CMD_DIALOGIC_2_TIMER)) // only English CMD for Dialog demo. the current set up
                                                             // is to starts over from CMD_1 whenever WW is
                                                             // detected.
                        {
                            cmdString = cmd_dialogic_1_en;
                            set_CMD_engine(&g_asrControl, ASR_ENGLISH, ASR_CMD_DIALOGIC_1, cmdString);
                        }
                        else if (appAsrShellCommands.demo ==
                                                    (ASR_CMD_NORMAL | ASR_CMD_CONDITION| ASR_CMD_TEMPERATURE | ASR_CMD_FLOAT_NUM | ASR_CMD_CONFIRM | ASR_CMD_MEAL)) // only English CMD for LED demo, multi-lingual WW is possible.
						{
							cmdString = cmd_normal_zh;
							set_CMD_engine(&g_asrControl, ASR_CHINESE, ASR_CMD_NORMAL, cmdString);
						}
                        else
                        {
                            cmdString = get_cmd_string(pInfWW->iWhoAmI_lang, appAsrShellCommands.demo);
                            set_CMD_engine(&g_asrControl, pInfWW->iWhoAmI_lang, appAsrShellCommands.demo, cmdString);
                        }

                        oob_demo_control.language = pInfWW->iWhoAmI_lang;

                        reset_WW_engine(&g_asrControl);

                        // Notify App Task Wake Word Detected
                        xTaskNotify(appTaskHandle, kWakeWordDetected, eSetBits);
                        break; // exit for loop
                    }          // end of if (asr_get_string_by_id(pInfWW, g_asrControl.keywordID[0]) != NULL)
                } // end of if (asr_process_audio_buffer(pInfWW->handler, pi16Sample, NUM_SAMPLES_AFE_OUTPUT,
                  // pInfWW->iWhoAmI_inf))
            }     // end of for (pInfWW = g_asrControl.infEngineWW; pInfWW != NULL; pInfWW = pInfWW->next)
        }         // end of if (asrEvent == ASR_SESSION_ENDED)
        // now we are getting into command detection. It must detect a command within the waiting time.
        else if (asrEvent == ASR_SESSION_STARTED)
        {
            pInfCMD = g_asrControl.infEngineCMD;

            if (asr_process_audio_buffer(pInfCMD->handler, pi16Sample, NUM_SAMPLES_AFE_OUTPUT, pInfCMD->iWhoAmI_inf) ==
                kAsrLocalDetected)
            {
                if (asr_get_string_by_id(pInfCMD, g_asrControl.result.keywordID[1]) != NULL)
                {
                	configPRINTF(("%d\r\n",g_asrControl.result.cmdMapID));
                    /*configPRINTF(("[ASR] Command: %s(%d) - MapID(%d)\r\n",
                                  asr_get_string_by_id(pInfCMD, g_asrControl.result.keywordID[1]),
                                  g_asrControl.result.keywordID[1], g_asrControl.result.cmdMapID));*/
                    if (appAsrShellCommands.cmdresults == ASR_CMD_RES_ON)
                    {
                        configPRINTF(("      Trust: %d, SGDiff: %d\r\n", g_asrControl.result.trustScore,
                                      g_asrControl.result.SGDiffScore));
                    }
                    /*PRINTF("[ASR] Command: %s(%d) \r\n",
                           asr_get_string_by_id(pInfCMD, g_asrControl.result.keywordID[1]),
                           g_asrControl.result.keywordID[1]);*/
                    PRINTF("%d \r\n",g_asrControl.result.cmdMapID);

                    g_asrControl.sampleCount = 0;

                    if (appAsrShellCommands.followup)
                    {
                        asrEvent = ASR_SESSION_STARTED;
                    }
                    else
                    {
                        asrEvent = ASR_SESSION_ENDED;
                    }

                    // Notify App Task Command Detected
                    switch (pInfCMD->iWhoAmI_inf)
                    {
                        case ASR_CMD_LED:
                            oob_demo_control.ledCmd = g_asrControl.result.keywordID[1];
                            xTaskNotify(appTaskHandle, kCommandLED, eSetBits);
                            break;
                        case ASR_CMD_IOT:
                        case ASR_CMD_ELEVATOR:
                        case ASR_CMD_AUDIO:
                        case ASR_CMD_WASH:
                            xTaskNotify(appTaskHandle, kCommandGeneric, eSetBits);
                            break;
                        case ASR_CMD_DIALOGIC_1:
                            if (g_asrControl.result.keywordID[1] >= 0 &&
                                g_asrControl.result.keywordID[1] <= 3) // set preheat, set bake, set broil
                            {
                                oob_demo_control.dialogRes = RESPONSE_1_TEMPERATURE; //  audio playback.
                                cmdString                  = cmd_dialogic_2_temperature_en;
                                set_CMD_engine(&g_asrControl, ASR_ENGLISH, ASR_CMD_DIALOGIC_2_TEMPERATURE, cmdString);
                            }
                            else if (g_asrControl.result.keywordID[1] == 4) // set timer
                            {
                                oob_demo_control.dialogRes = RESPONSE_1_TIMER; //  audio playback.
                                cmdString                  = cmd_dialogic_2_timer_en;
                                set_CMD_engine(&g_asrControl, ASR_ENGLISH, ASR_CMD_DIALOGIC_2_TIMER, cmdString);
                            }

                            asrEvent =
                                ASR_SESSION_STARTED; // moving to listen to commands 2 within the same ASR session
                            xTaskNotify(appTaskHandle, kCommandDialog, eSetBits);
                            break;
                        case ASR_CMD_DIALOGIC_2_TEMPERATURE:
                            oob_demo_control.dialogRes = RESPONSE_2_TEMPERATURE; //  audio playback.
                            asrEvent = ASR_SESSION_ENDED; // now finishing the ASR session and setting to dialogic cmd 2
                            xTaskNotify(appTaskHandle, kCommandDialog, eSetBits);
                            break;
                        case ASR_CMD_DIALOGIC_2_TIMER:
                            oob_demo_control.dialogRes = RESPONSE_2_TIMER; //  audio playback.
                            asrEvent = ASR_SESSION_ENDED; // now finishing the ASR session and setting to dialogic cmd 2
                            xTaskNotify(appTaskHandle, kCommandDialog, eSetBits);
                            break;

                        //uhua
                        case ASR_CMD_NORMAL:
                        	if (g_asrControl.result.keywordID[1] == 0) // set preheat, set bake, set broil
							{

								//configPRINTF(("%d",oob_demo_control.dialogRes));
								//PRINTF("%d",oob_demo_control.dialogRes);
								cmdString                  = cmd_condition_zh;
								set_CMD_engine(&g_asrControl, ASR_CHINESE, ASR_CMD_CONDITION, cmdString);
								asrEvent = ASR_SESSION_STARTED;
								xTaskNotify(appTaskHandle, kCommandGeneric, eSetBits);
							}
							else if (g_asrControl.result.keywordID[1] == 1) // set timer
							{
								cmdString                  = cmd_meal_zh;
								set_CMD_engine(&g_asrControl, ASR_CHINESE, ASR_CMD_MEAL, cmdString);
								asrEvent = ASR_SESSION_STARTED;
								xTaskNotify(appTaskHandle, kCommandGeneric, eSetBits);
							}
							else if (g_asrControl.result.keywordID[1] <= 4) // set timer
							{
								asrEvent = ASR_SESSION_ENDED;
								xTaskNotify(appTaskHandle, kCommandGeneric, eSetBits);
							}
                        	break;
                        case ASR_CMD_CONDITION:
                        	if(g_asrControl.result.keywordID[1] >= 0 && g_asrControl.result.keywordID[1] <= 3)
                        	{
								//configPRINTF(("%d",oob_demo_control.dialogRes));
								//PRINTF("%d",oob_demo_control.dialogRes);
								cmdString                  = cmd_temperature_zh;
								set_CMD_engine(&g_asrControl, ASR_CHINESE, ASR_CMD_TEMPERATURE, cmdString);
								asrEvent = ASR_SESSION_STARTED;
								xTaskNotify(appTaskHandle, kCommandGeneric, eSetBits);
                        	}
                        	break;
                        case ASR_CMD_TEMPERATURE:
							if(g_asrControl.result.keywordID[1] >= 0 && g_asrControl.result.keywordID[1] <= 11)
							{
								//configPRINTF(("%d",oob_demo_control.dialogRes));
								//PRINTF("%d",oob_demo_control.dialogRes);
								cmdString                  = cmd_float_num_zh;
								set_CMD_engine(&g_asrControl, ASR_CHINESE, ASR_CMD_FLOAT_NUM, cmdString);
								asrEvent = ASR_SESSION_STARTED;
								xTaskNotify(appTaskHandle, kCommandGeneric, eSetBits);
							}
							break;
                        case ASR_CMD_FLOAT_NUM:
                        	if(g_asrControl.result.keywordID[1] >= 0 && g_asrControl.result.keywordID[1] <= 9)
							{
								//configPRINTF(("%d",oob_demo_control.dialogRes));
								//PRINTF("%d",oob_demo_control.dialogRes);
								cmdString                  = cmd_confirm_zh;
								set_CMD_engine(&g_asrControl, ASR_CHINESE, ASR_CMD_CONFIRM, cmdString);
								asrEvent = ASR_SESSION_STARTED;
								xTaskNotify(appTaskHandle, kCommandGeneric, eSetBits);
							}
                        	break;
                        case ASR_CMD_CONFIRM:
                        	if(g_asrControl.result.keywordID[1] == 1)
							{
								//configPRINTF(("%d",oob_demo_control.dialogRes));
								//PRINTF("%d",oob_demo_control.dialogRes);
								cmdString                  = cmd_temperature_zh;
								set_CMD_engine(&g_asrControl, ASR_CHINESE, ASR_CMD_TEMPERATURE, cmdString);
								asrEvent = ASR_SESSION_STARTED;
								xTaskNotify(appTaskHandle, kCommandGeneric, eSetBits);
							}
                        	else if(g_asrControl.result.keywordID[1] == 0)
                        	{
                        		asrEvent = ASR_SESSION_ENDED;
                        		xTaskNotify(appTaskHandle, kCommandGeneric, eSetBits);
                        	}
							break;
                        case ASR_CMD_MEAL:
							cmdString                  = cmd_confirm_meal_zh;
							set_CMD_engine(&g_asrControl, ASR_CHINESE, ASR_CMD_CONFIRM_MEAL, cmdString);
							asrEvent = ASR_SESSION_STARTED;
                        	xTaskNotify(appTaskHandle, kCommandGeneric, eSetBits);
                        	break;
                        case ASR_CMD_CONFIRM_MEAL:
                        	if(g_asrControl.result.keywordID[1] == 1)
							{
								//configPRINTF(("%d",oob_demo_control.dialogRes));
								//PRINTF("%d",oob_demo_control.dialogRes);
								cmdString                  = cmd_meal_zh;
								set_CMD_engine(&g_asrControl, ASR_CHINESE, ASR_CMD_MEAL, cmdString);
								asrEvent = ASR_SESSION_STARTED;
								xTaskNotify(appTaskHandle, kCommandGeneric, eSetBits);
							}
                        	else if(g_asrControl.result.keywordID[1] == 0)
                        	{
                        		asrEvent = ASR_SESSION_ENDED;
                        		xTaskNotify(appTaskHandle, kCommandGeneric, eSetBits);
                        	}
                        	break;
                        //uhua
                        default:
                            xTaskNotify(appTaskHandle, kCommandGeneric, eSetBits);
                            break;
                    }

                    if (asrEvent == ASR_SESSION_ENDED)
                    {
                        print_asr_session(asrEvent);
                    }

                    reset_CMD_engine(&g_asrControl);
                    set_WW_engine(&g_asrControl);

                } // end of asr_get_string()
            }     // end of asr_process_audio_buffer()

            // calculate waiting time.
            g_asrControl.sampleCount += NUM_SAMPLES_AFE_OUTPUT;
            if (g_asrControl.sampleCount > 16000 / 1000 * appAsrShellCommands.timeout)
            {
                g_asrControl.sampleCount = 0;

                reset_CMD_engine(&g_asrControl);

                asrEvent = ASR_SESSION_ENDED;
                print_asr_session(ASR_SESSION_TIMEOUT);
                print_asr_session(asrEvent);

                set_WW_engine(&g_asrControl);

                // Notify App Task Timeout
                xTaskNotify(appTaskHandle, kTimeOut, eSetBits);
            }
        } // end of else if (asrEvent == ASR_SESSION_STARTED)

        // reinitialize the ASR engine if language set was changed
        if (appAsrShellCommands.asrCfg & ASR_CFG_DEMO_LANGUAGE_CHANGED)
        {
            initialize_asr();
            appAsrShellCommands.asrCfg &= ~ASR_CFG_DEMO_LANGUAGE_CHANGED;
        }
    } // end of while
}
