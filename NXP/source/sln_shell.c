/*
 * Copyright 2018-2021 NXP.
 * This software is owned or controlled by NXP and may only be used strictly in accordance with the
 * license terms that accompany it. By expressly accepting such terms or by downloading, installing,
 * activating and/or otherwise using the software, you are agreeing that you have read, and that you
 * agree to comply with and are bound by, such license terms. If you do not agree to be bound by the
 * applicable license terms, then you may not retain, install, activate or otherwise use the software.d
 */

#include "sln_shell.h"
#include "mbedtls/base64.h"

#include "IndexCommands.h"
#include "audio_processing_task.h"
#include "sln_local_voice.h"
#include "sln_app_fwupdate.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

#if (defined(SERIAL_MANAGER_NON_BLOCKING_MODE) && (SERIAL_MANAGER_NON_BLOCKING_MODE > 0U))
#define SLN_SERIAL_MANAGER_RECEIVE_BUFFER_LEN 2048U
#endif

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
extern void *pvPortCalloc(size_t nmemb, size_t xSize);

#if USE_WIFI_CONNECTION
static shell_status_t sln_print_handler(shell_handle_t shellHandle, int32_t argc, char **argv);
static shell_status_t sln_erase_handler(shell_handle_t shellHandle, int32_t argc, char **argv);
static shell_status_t sln_setup_handler(shell_handle_t shellHandle, int32_t argc, char **argv);
#endif
static shell_status_t sln_reset_handler(shell_handle_t shellHandle, int32_t argc, char **argv);
static shell_status_t sln_commands_handler(shell_handle_t shellHandle, int32_t argc, char **argv);
static shell_status_t sln_changeto_handler(shell_handle_t shellHandle, int32_t argc, char **argv);
static shell_status_t sln_volume_handler(shell_handle_t shellHandle, int32_t argc, char **argv);
static shell_status_t sln_mute_handler(shell_handle_t shellHandle, int32_t argc, char **argv);
static shell_status_t sln_timeout_handler(shell_handle_t shellHandle, int32_t argc, char **argv);
static shell_status_t sln_followup_handler(shell_handle_t shellHandle, int32_t argc, char **argv);
#if MULTILINGUAL
static shell_status_t sln_multilingual_handler(shell_handle_t shellHandle, int32_t argc, char **argv);
#endif
static shell_status_t sln_ptt_handler(shell_handle_t shellHandle, int32_t argc, char **argv);
static shell_status_t sln_cmdresults_handler(shell_handle_t shellHandle, int32_t argc, char **argv);
static shell_status_t sln_updateotw_handler(shell_handle_t shellHandle, int32_t argc, char **argv);
static shell_status_t sln_updateota_handler(shell_handle_t shellHandle, int32_t argc, char **argv);
static shell_status_t sln_version_handler(shell_handle_t shellHandle, int32_t argc, char **argv);

/*******************************************************************************
 * Variables
 ******************************************************************************/
#if USE_WIFI_CONNECTION
SHELL_COMMAND_DEFINE(print,
                     "\r\n\"print\": Print the WiFi Network Credentials currently stored in flash.\r\n",
                     sln_print_handler,
                     0);

SHELL_COMMAND_DEFINE(erase,
                     "\r\n\"erase\": Erase the current WiFi Network credentials from flash.\r\n",
                     sln_erase_handler,
                     0);

SHELL_COMMAND_DEFINE(setup,
                     "\r\n\"setup\": Setup the WiFi Network Credentials\r\n"
                     "         Usage:\r\n"
                     "             setup SSID [PASSWORD] \r\n"
                     "         Parameters:\r\n"
                     "         SSID:       The wireless network name\r\n"
                     "         PASSWORD:   The password for the wireless network\r\n"
                     "                     For open networks it is not needed\r\n",
                     /* if more than two parameters, it'll take just the first two of them */
                     sln_setup_handler,
                     SHELL_IGNORE_PARAMETER_COUNT);
#endif
SHELL_COMMAND_DEFINE(reset, "\r\n\"reset\": Resets the MCU.\r\n", sln_reset_handler, 0);

SHELL_COMMAND_DEFINE(commands,
                     "\r\n\"commands\": List available voice commands for selected demo.\r\n",
                     sln_commands_handler,
                     0);

SHELL_COMMAND_DEFINE(changeto,
                     "\r\n\"changeto\": Change the command set\r\n"
                     "         Usage:\r\n"
                     "            changeto <param> \r\n"
                     "         Parameters\r\n"
                     "            elevator: Elevator control\r\n"
                     "            iot: IoT\r\n"
                     "            audio: Audio control\r\n"
                     "            wash: Washing machine\r\n"
                     "            led: LED control (auto-enabling English)\r\n"
                     "            dialog: Dialogic commands for oven (auto-enabling English)\r\n"
	                 "            cmkuan: your best helper",
                     sln_changeto_handler,
                     1);

SHELL_COMMAND_DEFINE(volume,
                     "\r\n\"volume\": Set speaker volume (0 - 100). Save in flash memory.\r\n"
                     "         Usage:\r\n"
                     "            volume N \r\n"
                     "         Parameters\r\n"
                     "            N between 0 and 100\r\n",
                     sln_volume_handler,
                     SHELL_IGNORE_PARAMETER_COUNT);

SHELL_COMMAND_DEFINE(mute,
                     "\r\n\"mute\": Set microphones state (on / off). Save in flash memory.\r\n"
                     "         Usage:\r\n"
                     "            mute on (or off)\r\n"
                     "         Parameters\r\n"
                     "            on or off\r\n",
                     sln_mute_handler,
                     1);

SHELL_COMMAND_DEFINE(timeout,
                     "\r\n\"timeout\": Set command waiting time (in ms). Save in flash memory.\r\n"
                     "         Usage:\r\n"
                     "            timeout N \r\n"
                     "         Parameters\r\n"
                     "            N milliseconds\r\n",
                     sln_timeout_handler,
                     SHELL_IGNORE_PARAMETER_COUNT);

SHELL_COMMAND_DEFINE(followup,
                     "\r\n\"followup\": Set follow-up mode (on / off). Save in flash memory.\r\n"
                     "         Usage:\r\n"
                     "            followup on (or off) \r\n"
                     "         Parameters\r\n"
                     "            on or off\r\n",
                     sln_followup_handler,
                     1);
#if MULTILINGUAL
SHELL_COMMAND_DEFINE(multilingual,
                     "\r\n\"multilingual\": Select language model(s). Save in flash memory.\r\n"
                     "         Usage:\r\n"
                     "            multilingual language_code1 up to language_code4 \r\n"
                     "         Parameters\r\n"
                     "            language_codes - en, zh, de, fr\r\n",
                     sln_multilingual_handler,
                     SHELL_IGNORE_PARAMETER_COUNT);
#endif
SHELL_COMMAND_DEFINE(ptt,
                     "\r\n\"ptt\": Set push-to-talk mode (on / off). Save in flash memory.\r\n"
                     "         Usage:\r\n"
                     "            ptt on (or off) \r\n"
                     "         Parameters\r\n"
                     "            on or off\r\n",
                     sln_ptt_handler,
                     1);

SHELL_COMMAND_DEFINE(cmdresults,
                     "\r\n\"cmdresults\": Print the command detection results in console.\r\n"
                     "         Usage:\r\n"
                     "            cmdresults on (or off) \r\n"
                     "         Parameters\r\n"
                     "            on or off\r\n",
                     sln_cmdresults_handler,
                     1);

SHELL_COMMAND_DEFINE(updateotw,
                     "\r\n\"updateotw\": Restarts the board in the OTW update mode.\r\n",
                     sln_updateotw_handler,
                     0);

SHELL_COMMAND_DEFINE(updateota,
                     "\r\n\"updateota\": Restarts the board in the OTA update mode.\r\n",
                     sln_updateota_handler,
                     0);

SHELL_COMMAND_DEFINE(version, "\r\n\"version\": Print firmware version\r\n", sln_version_handler, 0);

extern app_asr_shell_commands_t appAsrShellCommands;
extern TaskHandle_t appTaskHandle;

static uint8_t s_shellHandleBuffer[SHELL_HANDLE_SIZE];
static shell_handle_t s_shellHandle;

static uint8_t s_serialHandleBuffer[SERIAL_MANAGER_HANDLE_SIZE];
static serial_handle_t s_serialHandle = &s_serialHandleBuffer[0];

#if (defined(SERIAL_MANAGER_NON_BLOCKING_MODE) && (SERIAL_MANAGER_NON_BLOCKING_MODE > 0U))
__attribute__((section(".ocram_data"))) __attribute__((aligned(8)))
uint8_t readRingBuffer[SLN_SERIAL_MANAGER_RECEIVE_BUFFER_LEN];
#endif

static EventGroupHandle_t s_ShellEventGroup;
#if USE_WIFI_CONNECTION
static wifi_cred_t s_wifi_cred = {0};
#endif
static TaskHandle_t s_appInitTask      = NULL;
static shell_heap_trace_t s_heap_trace = {0};

/*******************************************************************************
 * Code
 ******************************************************************************/

static const char *getAppType(void)
{
    /* Find the current running bank by checking the ResetISR Address in the vector table (which is loaded into
     * DTC) */
    uint32_t runningFromBankA =
        (((*(uint32_t *)(APPLICATION_RESET_ISR_ADDRESS)) & APP_VECTOR_TABLE_APP_A) == APP_VECTOR_TABLE_APP_A);
    uint32_t runningFromBankb =
        (((*(uint32_t *)(APPLICATION_RESET_ISR_ADDRESS)) & APP_VECTOR_TABLE_APP_B) == APP_VECTOR_TABLE_APP_B);

    if (runningFromBankA)
        return JSON_FILEPATH_APPA;
    if (runningFromBankb)
        return JSON_FILEPATH_APPB;
    return JSON_FILEPATH_APPUNK;
}

static shell_status_t isNumber(char *arg)
{
    int32_t status = kStatus_SHELL_Success;
    uint32_t i;

    for (i = 0; arg[i] != NULL; i++)
    {
        if (!isdigit(arg[i]))
        {
            status = kStatus_SHELL_Error;
        }
    }

    return status;
}

static void USB_DeviceClockInit(void)
{
#if defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0U)
    usb_phy_config_struct_t phyConfig = {
        BOARD_USB_PHY_D_CAL,
        BOARD_USB_PHY_TXCAL45DP,
        BOARD_USB_PHY_TXCAL45DM,
    };
#endif
#if defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0U)
    if (CONTROLLER_ID == kSerialManager_UsbControllerEhci0)
    {
        CLOCK_EnableUsbhs0PhyPllClock(kCLOCK_Usbphy480M, 480000000U);
        CLOCK_EnableUsbhs0Clock(kCLOCK_Usb480M, 480000000U);
    }
    else
    {
        CLOCK_EnableUsbhs1PhyPllClock(kCLOCK_Usbphy480M, 480000000U);
        CLOCK_EnableUsbhs1Clock(kCLOCK_Usb480M, 480000000U);
    }
    USB_EhciPhyInit(CONTROLLER_ID, BOARD_XTAL0_CLK_HZ, &phyConfig);
#endif
}

static shell_status_t sln_reset_handler(shell_handle_t shellHandle, int32_t argc, char **argv)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xEventGroupSetBitsFromISR(s_ShellEventGroup, RESET_EVENT, &xHigherPriorityTaskWoken);

    return kStatus_SHELL_Success;
}

static shell_status_t sln_commands_handler(shell_handle_t shellHandle, int32_t argc, char **argv)
{
    unsigned int cmd_number     = get_cmd_number(appAsrShellCommands.demo);
    asr_language_t lang         = ASR_ENGLISH;
    asr_language_t multilingual = ASR_ENGLISH;

#if MULTILINGUAL
    if ((appAsrShellCommands.demo != ASR_CMD_LED) && (appAsrShellCommands.demo != ASR_CMD_DIALOGIC_1))
    {
        multilingual = appAsrShellCommands.multilingual;
    }
    configPRINTF(("Available commands in selected languages:\r\n"));
#endif

    while (multilingual != UNDEFINED_LANGUAGE)
    {
        if ((multilingual & lang) != UNDEFINED_LANGUAGE)
        {
            char **cmdString = get_cmd_strings(lang, appAsrShellCommands.demo);

            for (unsigned int i = 0; i < cmd_number; i++)
            {
                configPRINTF(("%s\r\n", cmdString[i]));
            }
            multilingual &= ~lang;
        }
        lang <<= 1U;
    }

    return kStatus_SHELL_Success;
}

static shell_status_t sln_changeto_handler(shell_handle_t shellHandle, int32_t argc, char **argv)
{
    int32_t status = kStatus_SHELL_Success;
    char *str;

    if (argc != 2)
    {
        SHELL_Printf(
            s_shellHandle,
            "\r\nIncorrect command parameter(s). Enter \"help\" to view a list of available commands.\r\n\r\n");
        status = kStatus_SHELL_Error;
    }
    else
    {
        str = argv[1];

        if (strcmp(str, "audio") == 0)
        {
            appAsrShellCommands.demo   = ASR_CMD_AUDIO;
            appAsrShellCommands.status = WRITE_READY;
            appAsrShellCommands.asrCfg = ASR_CFG_CMD_INFERENCE_ENGINE_CHANGED;
            configPRINTF(("Changing to Audio commands demo.\r\n"));
        }
        else if (strcmp(str, "iot") == 0)
        {
            appAsrShellCommands.demo   = ASR_CMD_IOT;
            appAsrShellCommands.status = WRITE_READY;
            appAsrShellCommands.asrCfg = ASR_CFG_CMD_INFERENCE_ENGINE_CHANGED;
            configPRINTF(("Changing to IoT commands demo.\r\n"));
        }
        else if (strcmp(str, "elevator") == 0)
        {
            appAsrShellCommands.demo   = ASR_CMD_ELEVATOR;
            appAsrShellCommands.status = WRITE_READY;
            appAsrShellCommands.asrCfg = ASR_CFG_CMD_INFERENCE_ENGINE_CHANGED;
            configPRINTF(("Changing to Elevator commands demo.\r\n"));
        }
        else if (strcmp(str, "wash") == 0)
        {
            appAsrShellCommands.demo   = ASR_CMD_WASH;
            appAsrShellCommands.status = WRITE_READY;
            appAsrShellCommands.asrCfg = ASR_CFG_CMD_INFERENCE_ENGINE_CHANGED;
            configPRINTF(("Changing to Washing Machine commands demo.\r\n"));
        }
        else if (strcmp(str, "led") == 0)
        {
            appAsrShellCommands.demo         = ASR_CMD_LED;
            appAsrShellCommands.multilingual = ASR_ENGLISH;
            appAsrShellCommands.status       = WRITE_READY;
            appAsrShellCommands.asrCfg       = ASR_CFG_CMD_INFERENCE_ENGINE_CHANGED | ASR_CFG_DEMO_LANGUAGE_CHANGED;
            configPRINTF(("Changing to LED commands demo. English only activated.\r\n"));
        }
        else if (strcmp(str, "dialog") == 0)
        {
            appAsrShellCommands.demo         = ASR_CMD_DIALOGIC_1;
            appAsrShellCommands.multilingual = ASR_ENGLISH;
            appAsrShellCommands.status       = WRITE_READY;
            appAsrShellCommands.asrCfg       = ASR_CFG_CMD_INFERENCE_ENGINE_CHANGED | ASR_CFG_DEMO_LANGUAGE_CHANGED;
            configPRINTF(("Changing to Dialogic commands demo. English only activated.\r\n"));
        }
        else if (strcmp(str, "cmkuan") == 0)
		{
			appAsrShellCommands.demo         = ASR_CMD_NORMAL;
			appAsrShellCommands.multilingual = ASR_CHINESE;
			appAsrShellCommands.status       = WRITE_READY;
			appAsrShellCommands.asrCfg       = ASR_CFG_CMD_INFERENCE_ENGINE_CHANGED | ASR_CFG_DEMO_LANGUAGE_CHANGED;
			configPRINTF(("Changing to cmkuan commands demo. Chinese only activated.\r\n"));
		}
        else
        {
            configPRINTF(("Invalid input.\r\n"));
            status = kStatus_SHELL_Error;
        }

        // notify main task
        xTaskNotifyFromISR(appTaskHandle, kDefault, eSetBits, NULL);
    }

    return status;
}

#if MULTILINGUAL
static shell_status_t sln_multilingual_handler(shell_handle_t shellHandle, int32_t argc, char **argv)
{
    int32_t status = kStatus_SHELL_Success;
    int32_t language_limit;
    asr_language_t multilingual;

    int idx;

    if ((argc - 1) > MAX_CONCURRENT_LANGUAGES)
    {
        SHELL_Printf(s_shellHandle,
                     "\r\nIncorrect command parameter(s). Please enter up to %d languages at a time.\r\n\r\n",
                     MAX_CONCURRENT_LANGUAGES);
        status = kStatus_SHELL_Error;
    }
    else
    {
        if (argc == 1)
        {
            multilingual = appAsrShellCommands.multilingual;

            if (multilingual != UNDEFINED_LANGUAGE)
            {
                configPRINTF(("Enabled language(s):"));
                if ((multilingual & ASR_ENGLISH) != UNDEFINED_LANGUAGE)
                {
                    configPRINTF((" English"));
                }
                if ((multilingual & ASR_CHINESE) != UNDEFINED_LANGUAGE)
                {
                    configPRINTF((" Chinese"));
                }
                if ((multilingual & ASR_GERMAN) != UNDEFINED_LANGUAGE)
                {
                    configPRINTF((" German"));
                }
                if ((multilingual & ASR_FRENCH) != UNDEFINED_LANGUAGE)
                {
                    configPRINTF((" French"));
                }
                configPRINTF((".\r\n"));
            }
        }
        else
        {
            multilingual = UNDEFINED_LANGUAGE;

            if ((appAsrShellCommands.demo == ASR_CMD_LED) || (appAsrShellCommands.demo == ASR_CMD_DIALOGIC_1) ||
                (appAsrShellCommands.ptt == ASR_PTT_ON))
            {
                // PTT, LED & DIALOG demos work only with English language.
                appAsrShellCommands.multilingual = ASR_ENGLISH;
                appAsrShellCommands.status       = WRITE_READY;
                appAsrShellCommands.asrCfg       = ASR_CFG_DEMO_LANGUAGE_CHANGED;

                if (appAsrShellCommands.ptt == ASR_PTT_ON)
                {
                    configPRINTF(("English only is auto-enabled in Push-to-Talk mode.\r\n"));
                }
                else
                {
                    configPRINTF(("English only is auto-enabled for current demo type.\r\n"));
                }
            }
            else
            {
                configPRINTF(("Enabling "));

                language_limit = (argc - 1 > MAX_CONCURRENT_LANGUAGES) ? MAX_CONCURRENT_LANGUAGES + 1 : argc;

                for (idx = 1; idx < language_limit; idx++)
                {
                    if (strcmp(argv[idx], "en") == 0 && !(multilingual & ASR_ENGLISH))
                    {
                        multilingual |= ASR_ENGLISH;
                        configPRINTF(("English "));
                    }
                    else if (strcmp(argv[idx], "zh") == 0 && !(multilingual & ASR_CHINESE))
                    {
                        multilingual |= ASR_CHINESE;
                        configPRINTF(("Chinese "));
                    }
                    else if (strcmp(argv[idx], "de") == 0 && !(multilingual & ASR_GERMAN))
                    {
                        multilingual |= ASR_GERMAN;
                        configPRINTF(("German "));
                    }
                    else if (strcmp(argv[idx], "fr") == 0 && !(multilingual & ASR_FRENCH))
                    {
                        multilingual |= ASR_FRENCH;
                        configPRINTF(("French "));
                    }
                    else
                    {
                        configPRINTF(("ERROR(arg: %s) ", argv[idx]));
                        status = kStatus_SHELL_Error;
                    }
                }
                configPRINTF(("language(s).\r\n"));
            }

            if (status == kStatus_SHELL_Success)
            {
                appAsrShellCommands.multilingual = multilingual;
                appAsrShellCommands.status       = WRITE_READY;
                appAsrShellCommands.asrCfg       = ASR_CFG_DEMO_LANGUAGE_CHANGED;
            }
            else
            {
                SHELL_Printf(s_shellHandle,
                             "\r\nIncorrect/duplicated command parameter(s). Enter \"help\" to view a list of "
                             "available commands.\r\n\r\n");
            }

            // notify main task
            xTaskNotifyFromISR(appTaskHandle, kDefault, eSetBits, NULL);
        }
    }

    return status;
}
#endif

static shell_status_t sln_volume_handler(shell_handle_t shellHandle, int32_t argc, char **argv)
{
    int32_t status = kStatus_SHELL_Success;

    if (argc > 2)
    {
        SHELL_Printf(
            s_shellHandle,
            "\r\nIncorrect command parameter(s). Enter \"help\" to view a list of available commands.\r\n\r\n");
        status = kStatus_SHELL_Error;
    }
    else
    {
        if (argc == 1)
        {
            configPRINTF(("Speaker volume set to %d.\r\n", appAsrShellCommands.volume));
        }
        else if (argc == 2 && (isNumber(argv[1]) == kStatus_SHELL_Success) && atoi(argv[1]) >= 0 &&
                 atoi(argv[1]) <= 100)
        {
            appAsrShellCommands.volume = (uint32_t)atoi(argv[1]);
            appAsrShellCommands.status = WRITE_READY;
            configPRINTF(("Setting speaker volume to %d.\r\n", appAsrShellCommands.volume));

            // notify main task
            xTaskNotifyFromISR(appTaskHandle, kVolumeUpdate, eSetBits, NULL);
        }
        else
        {
            configPRINTF(("Invalid volume value. Set between 0 to 100.\r\n", appAsrShellCommands.volume));
            status = kStatus_SHELL_Error;
        }
    }

    return status;
}

static shell_status_t sln_followup_handler(shell_handle_t shellHandle, int32_t argc, char **argv)
{
    int32_t status = kStatus_SHELL_Success;
    char *str;

    if (argc != 2)
    {
        SHELL_Printf(
            s_shellHandle,
            "\r\nIncorrect command parameter(s). Enter \"help\" to view a list of available commands.\r\n\r\n");
        status = kStatus_SHELL_Error;
    }
    else
    {
        str = argv[1];

        if (strcmp(str, "on") == 0)
        {
            appAsrShellCommands.followup = ASR_FOLLOWUP_ON;
            appAsrShellCommands.status   = WRITE_READY;
            configPRINTF(("Setting ASR Follow-Up mode on.\r\n"));
        }
        else if (strcmp(str, "off") == 0)
        {
            appAsrShellCommands.followup = ASR_FOLLOWUP_OFF;
            appAsrShellCommands.status   = WRITE_READY;
            configPRINTF(("Setting ASR Follow-Up mode off.\r\n"));
        }
        else
        {
            configPRINTF(("Invalid input.\r\n"));
            status = kStatus_SHELL_Error;
        }

        // notify main task
        xTaskNotifyFromISR(appTaskHandle, kDefault, eSetBits, NULL);
    }

    return status;
}

static shell_status_t sln_timeout_handler(shell_handle_t shellHandle, int32_t argc, char **argv)
{
    int32_t status = kStatus_SHELL_Success;

    if (argc > 2)
    {
        SHELL_Printf(
            s_shellHandle,
            "\r\nIncorrect command parameter(s). Enter \"help\" to view a list of available commands.\r\n\r\n");
        status = kStatus_SHELL_Error;
    }
    else
    {
        if (argc == 1)
        {
            configPRINTF(("Timeout value set to %d.\r\n", appAsrShellCommands.timeout));
        }
        else if (argc == 2 && (isNumber(argv[1]) == kStatus_SHELL_Success) && atoi(argv[1]) >= 0)
        {
            appAsrShellCommands.timeout = (uint32_t)atoi(argv[1]);
            appAsrShellCommands.status  = WRITE_READY;
            configPRINTF(("Setting command waiting time to %d ms.\r\n", appAsrShellCommands.timeout));

            // notify main task
            xTaskNotifyFromISR(appTaskHandle, kDefault, eSetBits, NULL);
        }
        else
        {
            configPRINTF(("Invalid waiting time %d ms.\r\n", appAsrShellCommands.timeout));
            status = kStatus_SHELL_Error;
        }
    }

    return status;
}

static shell_status_t sln_mute_handler(shell_handle_t shellHandle, int32_t argc, char **argv)
{
    int32_t status = kStatus_SHELL_Success;
    char *str;

    if (argc != 2)
    {
        SHELL_Printf(
            s_shellHandle,
            "\r\nIncorrect command parameter(s). Enter \"help\" to view a list of available commands.\r\n\r\n");
        status = kStatus_SHELL_Error;
    }
    else
    {
        str = argv[1];

        if (strcmp(str, "on") == 0)
        {
            appAsrShellCommands.mute   = ASR_MUTE_ON;
            appAsrShellCommands.status = WRITE_READY;
            configPRINTF(("Setting mute on.\r\n"));

            if (appAsrShellCommands.ptt == ASR_PTT_ON)
            {
                // deactivate PTT mode when muted
                appAsrShellCommands.ptt = ASR_PTT_OFF;
                configPRINTF(("Disabling ASR Push-To-Talk mode.\r\n"));
            }

            // notify main task
            xTaskNotifyFromISR(appTaskHandle, kMicUpdate, eSetBits, NULL);
        }
        else if (strcmp(str, "off") == 0)
        {
            appAsrShellCommands.mute   = ASR_MUTE_OFF;
            appAsrShellCommands.status = WRITE_READY;
            configPRINTF(("Setting mute off.\r\n"));

            // notify main task
            xTaskNotifyFromISR(appTaskHandle, kMicUpdate, eSetBits, NULL);
        }
        else
        {
            configPRINTF(("Invalid input.\r\n"));
            status = kStatus_SHELL_Error;
        }
    }

    return status;
}

static shell_status_t sln_ptt_handler(shell_handle_t shellHandle, int32_t argc, char **argv)
{
    int32_t status = kStatus_SHELL_Success;
    char *str;

    if (argc != 2)
    {
        SHELL_Printf(
            s_shellHandle,
            "\r\nIncorrect command parameter(s). Enter \"help\" to view a list of available commands.\r\n\r\n");
        status = kStatus_SHELL_Error;
    }
    else
    {
        str = argv[1];

        if (strcmp(str, "on") == 0)
        {
            if (appAsrShellCommands.mute == ASR_MUTE_OFF)
            {
                appAsrShellCommands.ptt          = ASR_PTT_ON;
                appAsrShellCommands.multilingual = ASR_ENGLISH;
                appAsrShellCommands.status       = WRITE_READY;
                appAsrShellCommands.asrCfg       = ASR_CFG_DEMO_LANGUAGE_CHANGED;
                configPRINTF(("Setting ASR Push-To-Talk mode on. English only activated.\r\n"));

                // notify main task
                xTaskNotifyFromISR(appTaskHandle, kMicUpdate, eSetBits, NULL);
            }
            else
            {
                // cannot activate PTT when muted
                configPRINTF(("Mics are muted! Turn mute off before activating ASR Push-To-Talk mode.\r\n"));
            }
        }
        else if (strcmp(str, "off") == 0)
        {
            appAsrShellCommands.ptt    = ASR_PTT_OFF;
            appAsrShellCommands.status = WRITE_READY;
            configPRINTF(("Setting ASR Push-To-Talk mode off.\r\n"));

            // notify main task
            xTaskNotifyFromISR(appTaskHandle, kMicUpdate, eSetBits, NULL);
        }
        else
        {
            configPRINTF(("Invalid input.\r\n"));
            status = kStatus_SHELL_Error;
        }
    }

    return status;
}

static shell_status_t sln_cmdresults_handler(shell_handle_t shellHandle, int32_t argc, char **argv)
{
    int32_t status = kStatus_SHELL_Success;
    char *str;

    if (argc != 2)
    {
        SHELL_Printf(
            s_shellHandle,
            "\r\nIncorrect command parameter(s). Enter \"help\" to view a list of available commands.\r\n\r\n");
        status = kStatus_SHELL_Error;
    }
    else
    {
        str = argv[1];

        if (strcmp(str, "on") == 0)
        {
            appAsrShellCommands.cmdresults = ASR_CMD_RES_ON;
            configPRINTF(("Setting command results printing to on.\r\n"));
        }
        else if (strcmp(str, "off") == 0)
        {
            appAsrShellCommands.cmdresults = ASR_CMD_RES_OFF;
            configPRINTF(("Setting command results printing to off.\r\n"));
        }
        else
        {
            configPRINTF(("Invalid input.\r\n"));
            status = kStatus_SHELL_Error;
        }
    }

    return status;
}

#if USE_WIFI_CONNECTION
static shell_status_t sln_print_handler(shell_handle_t shellHandle, int32_t argc, char **argv)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xEventGroupSetBitsFromISR(s_ShellEventGroup, PRINT_EVENT, &xHigherPriorityTaskWoken);

    return kStatus_SHELL_Success;
}

static shell_status_t sln_erase_handler(shell_handle_t shellHandle, int32_t argc, char **argv)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xEventGroupSetBitsFromISR(s_ShellEventGroup, ERASE_EVENT, &xHigherPriorityTaskWoken);

    return kStatus_SHELL_Success;
}

static shell_status_t sln_setup_handler(shell_handle_t shellHandle, int32_t argc, char **argv)
{
    int32_t status = kStatus_SHELL_Success;

    if (argc == 1 || argc > 3)
    {
        SHELL_Printf(
            s_shellHandle,
            "\r\nIncorrect command parameter(s). Enter \"help\" to view a list of available commands.\r\n\r\n");
        status = kStatus_SHELL_Error;
    }
    else
    {
        char *kWiFiName     = NULL;
        char *kWiFiPassword = NULL;

        kWiFiName = argv[1];
        if (argc > 2)
        {
            kWiFiPassword = argv[2];
        }

        uint32_t name_len = strlen(kWiFiName);
        uint32_t pass_len = kWiFiPassword ? strlen(kWiFiPassword) : 0;

        if (name_len == 0)
        {
            SHELL_Printf(
                s_shellHandle,
                "\r\nIncorrect command parameter(s). Enter \"help\" to view a list of available commands.\r\n\r\n");
            status = kStatus_SHELL_Error;
        }

        if (name_len <= sizeof(s_wifi_cred.ssid.value))
        {
            memcpy(s_wifi_cred.ssid.value, kWiFiName, name_len);
            s_wifi_cred.ssid.length = name_len;
        }
        else
        {
            status = kStatus_SHELL_Error;
        }

        if (pass_len + 1 <= sizeof(s_wifi_cred.password.value))
        {
            if (pass_len != 0)
            {
                memcpy(s_wifi_cred.password.value, kWiFiPassword, pass_len + 1);
            }
            else
            {
                s_wifi_cred.password.value[0] = '\0';
            }
            s_wifi_cred.password.length = pass_len;
        }
        else
        {
            status = kStatus_SHELL_Error;
        }

        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xEventGroupSetBitsFromISR(s_ShellEventGroup, SETUP_EVENT, &xHigherPriorityTaskWoken);
    }

    return status;
}
#endif

static shell_status_t sln_updateota_handler(shell_handle_t shellHandle, int32_t argc, char **argv)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    FWUpdate_set_SLN_OTA();

    SHELL_Printf(s_shellHandle, "\r\nReseting the board in OTA update mode\r\n");
    xEventGroupSetBitsFromISR(s_ShellEventGroup, RESET_EVENT, &xHigherPriorityTaskWoken);

    return kStatus_SHELL_Success;
}

static shell_status_t sln_updateotw_handler(shell_handle_t shellHandle, int32_t argc, char **argv)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    FWUpdate_set_SLN_OTW();

    SHELL_Printf(s_shellHandle, "\r\nReseting the board in OTW update mode\r\n");
    xEventGroupSetBitsFromISR(s_ShellEventGroup, RESET_EVENT, &xHigherPriorityTaskWoken);

    return kStatus_SHELL_Success;
}

static shell_status_t sln_version_handler(shell_handle_t shellHandle, int32_t argc, char **argv)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xEventGroupSetBitsFromISR(s_ShellEventGroup, VERSION_EVT, &xHigherPriorityTaskWoken);

    return kStatus_SHELL_Success;
}

int log_shell_printf(const char *formatString, ...)
{
    va_list ap;
    char logbuf[configLOGGING_MAX_MESSAGE_LENGTH] = {0};

    va_start(ap, formatString);
    vsnprintf(logbuf, configLOGGING_MAX_MESSAGE_LENGTH, formatString, ap);

    va_end(ap);

    SHELL_Write(s_shellHandle, logbuf, strlen(logbuf));

    return 0;
}

int sln_shell_init(void)
{
    status_t status = 0;
    serial_manager_config_t serialConfig;

    serial_port_usb_cdc_config_t usbCdcConfig = {
        .controllerIndex = (serial_port_usb_cdc_controller_index_t)CONTROLLER_ID,
    };

    s_ShellEventGroup = xEventGroupCreate();

    static volatile uint8_t usb_clock_initialized = 0;
    if (!usb_clock_initialized)
    {
        usb_clock_initialized = 1;
        USB_DeviceClockInit();
    }

    /* Init Serial Manager for USB CDC */
    serialConfig.type = kSerialPort_UsbCdc;
#if (defined(SERIAL_MANAGER_NON_BLOCKING_MODE) && (SERIAL_MANAGER_NON_BLOCKING_MODE > 0U))
    serialConfig.ringBuffer     = &readRingBuffer[0];
    serialConfig.ringBufferSize = SLN_SERIAL_MANAGER_RECEIVE_BUFFER_LEN;
#endif
    serialConfig.portConfig = &usbCdcConfig;

    status = SerialManager_Init(s_serialHandle, &serialConfig);
    if (status != kStatus_SerialManager_Success)
    {
        return (int32_t)status;
    }

    /* Init SHELL */
    s_shellHandle = &s_shellHandleBuffer[0];
    SHELL_Init(s_shellHandle, s_serialHandle, "SHELL>> ");

    /* Add the SLN commands to the commands list */
#if USE_WIFI_CONNECTION
    SHELL_RegisterCommand(s_shellHandle, SHELL_COMMAND(print));
    SHELL_RegisterCommand(s_shellHandle, SHELL_COMMAND(setup));
    SHELL_RegisterCommand(s_shellHandle, SHELL_COMMAND(erase));
#endif
    SHELL_RegisterCommand(s_shellHandle, SHELL_COMMAND(reset));
    SHELL_RegisterCommand(s_shellHandle, SHELL_COMMAND(commands));
    SHELL_RegisterCommand(s_shellHandle, SHELL_COMMAND(changeto));
    SHELL_RegisterCommand(s_shellHandle, SHELL_COMMAND(volume));
    SHELL_RegisterCommand(s_shellHandle, SHELL_COMMAND(mute));
    SHELL_RegisterCommand(s_shellHandle, SHELL_COMMAND(timeout));
    SHELL_RegisterCommand(s_shellHandle, SHELL_COMMAND(followup));
#if MULTILINGUAL
    SHELL_RegisterCommand(s_shellHandle, SHELL_COMMAND(multilingual));
#endif
    SHELL_RegisterCommand(s_shellHandle, SHELL_COMMAND(ptt));
    SHELL_RegisterCommand(s_shellHandle, SHELL_COMMAND(cmdresults));
    SHELL_RegisterCommand(s_shellHandle, SHELL_COMMAND(updateotw));
    SHELL_RegisterCommand(s_shellHandle, SHELL_COMMAND(updateota));
    SHELL_RegisterCommand(s_shellHandle, SHELL_COMMAND(version));

    return status;
}

void sln_shell_task(void *arg)
{
    volatile EventBits_t shellEvents = 0U;
    status_t status                  = 0;

    SHELL_Printf(s_shellHandle, "Howdy! Type \"help\" to see what this shell can do!\r\n");
    SHELL_Printf(s_shellHandle, "SHELL>> ");

    while (1)
    {
        shellEvents = xEventGroupWaitBits(s_ShellEventGroup, 0x00FFFFFF, pdTRUE, pdFALSE, portMAX_DELAY);

#if USE_WIFI_CONNECTION
        if (shellEvents & PRINT_EVENT)
        {
            wifi_cred_t wifi_cred = {0};

            /* Stores the WiFi SSID as a printable string. Added one extra byte for the NULL terminator. */
            char ssid_str[SSID_NAME_SIZE + 1] = {0};

            status = wifi_credentials_flash_get(&wifi_cred);
            if (!status)
            {
                if (check_valid_credentials(&wifi_cred))
                {
                    SHELL_Printf(s_shellHandle, "Found no credentials in flash\r\n");
                    SHELL_Printf(s_shellHandle, "SHELL>> ");
                }
                else
                {
                    SHELL_Printf(s_shellHandle, "These are the credentials:\r\n");

                    strncpy(ssid_str, (char *)wifi_cred.ssid.value, SSID_NAME_SIZE);
                    SHELL_Printf(s_shellHandle, "Wifi name: %s\r\n", ssid_str);

                    SHELL_Printf(s_shellHandle, "Wifi password: %s\r\n", wifi_cred.password.value);
                    SHELL_Printf(s_shellHandle, "SHELL>> ");
                }
            }
            else
            {
                SHELL_Printf(s_shellHandle, "Failed to read wifi credentials from flash, error code %d\r\n", status);
                SHELL_Printf(s_shellHandle, "SHELL>> ");
            }
        }

        if (shellEvents & SETUP_EVENT)
        {
            status = wifi_credentials_flash_set(&s_wifi_cred);
            if (!status)
            {
                SHELL_Printf(s_shellHandle, "Credentials saved\r\n");
                NVIC_SystemReset();
            }
            else
            {
                SHELL_Printf(s_shellHandle, "Failed to write wifi credentials in flash, error code %d\r\n", status);
                SHELL_Printf(s_shellHandle, "SHELL>> ");
            }
        }

        if (shellEvents & ERASE_EVENT)
        {
            status = wifi_credentials_flash_reset();
            if (!status)
            {
                SHELL_Printf(s_shellHandle, "Credentials erased\r\n");
                NVIC_SystemReset();
            }
            else
            {
                SHELL_Printf(s_shellHandle, "Failed to wipe wifi credentials from flash, error code %d\r\n", status);
                SHELL_Printf(s_shellHandle, "SHELL>> ");
            }
        }
#endif
        if (shellEvents & VERSION_EVT)
        {
            SHELL_Printf(s_shellHandle, "Firmware version: %d.%d.%d, App type: %s\r\n", APP_MAJ_VER, APP_MIN_VER,
                         APP_BLD_VER, getAppType());
            SHELL_Printf(s_shellHandle, "SHELL>> ");
        }

        if (shellEvents & RESET_EVENT)
        {
            /* this rather drastic approach is used for when one wants to use another
             * wifi network after successfully connecting to another one previously */
            NVIC_SystemReset();
        }
    }

    while (SerialManager_Deinit(s_serialHandle) == kStatus_SerialManager_Busy)
    {
        /* should happen pretty quickly after the call of g_shellCommandexit.pFuncCallBack,
         * just need to wait till the read and write handles are closed */
    }

    /* clean event group */
    vEventGroupDelete(s_ShellEventGroup);

    vTaskDelete(NULL);
}

void sln_shell_set_app_init_task_handle(TaskHandle_t *handle)
{
    if (NULL != handle)
    {
        s_appInitTask = *handle;
    }
}

void sln_shell_trace_malloc(void *ptr, size_t size)
{
    if (s_shellHandle)
    {
        if (s_heap_trace.enable)
        {
            if (size >= s_heap_trace.threshold)
            {
                SHELL_Printf(s_shellHandle, "[TRACE] Allocated %d bytes to 0x%X\r\n", size, (int)ptr);
                SHELL_Printf(s_shellHandle, "SHELL>> ");
            }
        }
    }
}

void sln_shell_trace_free(void *ptr, size_t size)
{
    if (s_shellHandle)
    {
        if (s_heap_trace.enable)
        {
            if (size >= s_heap_trace.threshold)
            {
                SHELL_Printf(s_shellHandle, "[TRACE] De-allocated %d bytes from 0x%X\r\n", size, (int)ptr);
                SHELL_Printf(s_shellHandle, "SHELL>> ");
            }
        }
    }
}
#if !(SDK_DEBUGCONSOLE)
int DbgConsole_Printf(const char *formatString, ...)
{
    va_list ap;
    char logbuf[configLOGGING_MAX_MESSAGE_LENGTH] = {0};

    va_start(ap, formatString);
    vsnprintf(logbuf, configLOGGING_MAX_MESSAGE_LENGTH, formatString, ap);

    va_end(ap);

    return 0;
}
#endif

/* Need to generate a unique ID for the client ID */
void APP_GetUniqueID(char **uniqueID)
{
    uint64_t u64UniqueIDRaw = (uint64_t)((uint64_t)OCOTP->CFG1 << 32ULL) | OCOTP->CFG0;
    uint32_t cIdLen         = 0;

    mbedtls_base64_encode(NULL, 0, &cIdLen, (const unsigned char *)&u64UniqueIDRaw, sizeof(uint64_t));

    *uniqueID = (char *)pvPortMalloc(cIdLen + 1);
    if (*uniqueID)
    {
        memset(*uniqueID, 0, cIdLen + 1);
        uint32_t outputLen = 0;
        mbedtls_base64_encode((unsigned char *)*uniqueID, cIdLen, &outputLen, (const unsigned char *)&u64UniqueIDRaw,
                              sizeof(uint64_t));
    }
    else
    {
        SHELL_Printf(s_shellHandle, "Failed to allocate memory for Unique ID\r\n");
    }
}
