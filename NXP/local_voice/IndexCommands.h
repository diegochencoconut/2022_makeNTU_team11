/*
 * Copyright 2021 NXP.
 * This software is owned or controlled by NXP and may only be used strictly in accordance with the
 * license terms that accompany it. By expressly accepting such terms or by downloading, installing,
 * activating and/or otherwise using the software, you are agreeing that you have read, and that you
 * agree to comply with and are bound by, such license terms. If you do not agree to be bound by the
 * applicable license terms, then you may not retain, install, activate or otherwise use the software.
 */

#ifndef INDEXCOMMANDS_H_
#define INDEXCOMMANDS_H_

#include "sln_local_voice.h"

/* These defines are currently used only for displaying in the commands shell */
#define NUMBER_OF_WW            4
#define NUMBER_OF_AUDIO_CMDS    10
#define NUMBER_OF_ELEVATOR_CMDS 10
#define NUMBER_OF_IOT_CMDS      8
#define NUMBER_OF_WASH_CMDS     5
#define NUMBER_OF_LED_CMDS      5
#define NUMBER_OF_DIALOGIC_CMDS 5

static char *cmd_iot_zh[] = {"温度升高", "温度降低", "打开窗帘", "关上窗帘", "开灯", "关灯", "亮一点", "暗一点"};

static char *cmd_elevator_zh[] = {"一楼", "二楼", "三楼", "四楼", "五楼", "大堂", "上行", "下行", "开门", "关门"};

static char *cmd_audio_zh[] = {"打开", "关掉",   "播放",   "暂停",     "开始",
                               "停止", "下一首", "上一曲", "提高音量", "音量减小"};

static char *cmd_wash_zh[] = {"精致洗", "正常清洗", "强力洗", "洗白", "取消"};

static char *cmd_iot_fr[] = {
    "Augmenter Température", "Diminuer Température", "Monter Fenêtre", "Baisser Fenêtre", "Allumer", "Eteindre",
    "Augmenter Luminosité",  "Diminuer Luminosité"};

static char *cmd_elevator_fr[] = {"Premier Etage",   "Deuxième Etage",    "Troisième Etage", "Quatrième Etage",
                                  "Cinquième Etage", "Entrée Principale", "Monter",          "Descendre",
                                  "Ouvrir Porte",    "Fermer Porte"};

static char *cmd_audio_fr[] = {"Allumer",          "Eteindre",      "Lecture",        "Pause",
                               "Démarrage",        "Arrêt",         "Piste Suivante", "Piste Précédente",
                               "Augmenter Volume", "Baisser Volume"};

static char *cmd_wash_fr[] = {"Lavage Délicat", "Lavage Normal", "Lavage en Profondeur", "Lavage Blanc", "Annuler"};

static char *cmd_iot_en[] = {"Temperature Up", "Temperature Down", "Window Up", "Window Down",
                             "Turn On",        "Turn Off",         "Brighter",  "Darker"};

static char *cmd_elevator_en[] = {"Floor One",  "Floor Two", "Floor Three", "Floor Four", "Floor Five",
                                  "Main Lobby", "Going Up",  "Going Down",  "Open Door",  "Close Door"};

static char *cmd_audio_en[] = {"Turn On", "Turn Off",   "Play",           "Pause",     "Start",
                               "Stop",    "Next Track", "Previous Track", "Volume Up", "Volume Down"};

static char *cmd_wash_en[] = {"Wash Delicate", "Wash Normal", "Wash Heavy Duty", "Wash Whites", "Cancel"};

static char *cmd_led_en[] = {"L, E, D, Red", "L, E, D, Green", "L, E, D, Blue", "Cycle Fast", "Cycle Slow"};

static char *cmd_dialogic_1_en[] = {"Set Preheat", "Set Pre Heat", "Set Bake", "Set Broil", "Set Timer"};

static char *cmd_iot_de[] = {"Temperatur erhöhen", "Temperatur verringern", "Fenster hoch", "Fenster runter",
                             "anschalten",         "Ausschalten",           "heller",       "dunkler"};

static char *cmd_elevator_de[] = {"Etage eins", "Etage zwei", "Etage drei",   "Etage vier",    "Etage fünf",
                                  "Hauptlobby", "Hochfahren", "Runterfahren", "Öffne die Tür", "Schließe die Tür"};

static char *cmd_audio_de[] = {"anschalten",
                               "ausschalten",
                               "abspielen",
                               "Pause",
                               "Anfang",
                               "halt",
                               "nächstes Lied",
                               "vorheriges Lied",
                               "Lautstärke erhöhen",
                               "Lautstärke verringern"};

static char *cmd_wash_de[] = {"Feinwäsche", "Normalwäsche", "stark verschmutze Wäsche", "Weißwäsche", "abbrechen"};

char **get_cmd_strings(asr_language_t asrLang, asr_inference_t infCMDType)
{
    char **retString = NULL;

    switch (asrLang)
    {
        case ASR_CHINESE:
            if (infCMDType == ASR_CMD_IOT)
                retString = cmd_iot_zh;
            else if (infCMDType == ASR_CMD_ELEVATOR)
                retString = cmd_elevator_zh;
            else if (infCMDType == ASR_CMD_AUDIO)
                retString = cmd_audio_zh;
            else if (infCMDType == ASR_CMD_WASH)
                retString = cmd_wash_zh;
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
        default:
            retString = cmd_led_en;
            break;
    }

    return retString;
}

unsigned int get_cmd_number(asr_inference_t infCMDType)
{
    unsigned int cmd_number = 0;

    if (infCMDType == ASR_CMD_IOT)
        cmd_number = NUMBER_OF_IOT_CMDS;
    else if (infCMDType == ASR_CMD_ELEVATOR)
        cmd_number = NUMBER_OF_ELEVATOR_CMDS;
    else if (infCMDType == ASR_CMD_AUDIO)
        cmd_number = NUMBER_OF_AUDIO_CMDS;
    else if (infCMDType == ASR_CMD_WASH)
        cmd_number = NUMBER_OF_WASH_CMDS;
    else if (infCMDType == ASR_CMD_LED)
        cmd_number = NUMBER_OF_LED_CMDS;
    else if (infCMDType == ASR_CMD_DIALOGIC_1)
        cmd_number = NUMBER_OF_DIALOGIC_CMDS;

    return cmd_number;
}

#endif /* INDEXCOMMANDS_H_ */
