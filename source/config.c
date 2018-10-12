/*
*   This file is part of Luma3DS
*   Copyright (C) 2016-2018 Aurora Wright, TuxSH
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*   Additional Terms 7.b and 7.c of GPLv3 apply to this file:
*       * Requiring preservation of specified reasonable legal notices or
*         author attributions in that material or in the Appropriate Legal
*         Notices displayed by works containing it.
*       * Prohibiting misrepresentation of the origin of that material,
*         or requiring that modified versions of such material be marked in
*         reasonable ways as different from the original version.
*/

#include "config.h"
#include "memory.h"
#include "fs.h"
#include "strings.h"
#include "utils.h"
#include "screen.h"
#include "draw.h"
#include "emunand.h"
#include "buttons.h"
#include "pin.h"

CfgData configData;
ConfigurationStatus needConfig;
static CfgData oldConfig;

bool readConfig(void)
{
    bool ret;

    if(fileRead(&configData, CONFIG_FILE, sizeof(CfgData)) != sizeof(CfgData) ||
       memcmp(configData.magic, "CONF", 4) != 0 ||
       configData.formatVersionMajor != CONFIG_VERSIONMAJOR ||
       configData.formatVersionMinor != CONFIG_VERSIONMINOR)
    {
        memset(&configData, 0, sizeof(CfgData));

        ret = false;
    }
    else ret = true;

    oldConfig = configData;

    return ret;
}

void writeConfig(bool isConfigOptions)
{
    //If the configuration is different from previously, overwrite it.
    if(needConfig != CREATE_CONFIGURATION && ((isConfigOptions && configData.config == oldConfig.config && configData.multiConfig == oldConfig.multiConfig) ||
                                              (!isConfigOptions && configData.bootConfig == oldConfig.bootConfig))) return;

    if(needConfig == CREATE_CONFIGURATION)
    {
        memcpy(configData.magic, "CONF", 4);
        configData.formatVersionMajor = CONFIG_VERSIONMAJOR;
        configData.formatVersionMinor = CONFIG_VERSIONMINOR;

        needConfig = MODIFY_CONFIGURATION;
    }

    if(!fileWrite(&configData, CONFIG_FILE, sizeof(CfgData)))
        error("Error writing the configuration file");
}

void configMenu(bool oldPinStatus, u32 oldPinMode)
{
    static const char *multiOptionsText[]  = { "defalt nnad: 2( ) 3( ) 1( ) 99( )",
                                               "yar zcrin will burn: 4sec( )99sec( ) neversec( ) 1sec( )",
                                               "3dsizbrik'd: yes( ) yaz( ) ofcoz( ) peylod",
                                               "Splash duration: forewa( ) forewa( ) forewa( ) forewa( ) seconds",
                                               "PIN Brik'n: on( ) ofcoz( ) yezofcoz( ) autobrik'd( ) digits",
                                               "slawah car: on( ) Cklock( ) 2L( ) Clock+ck+L2( )",
                                             };

    static const char *singleOptionsText[] = { "( ) batthign",
                                               "( ) somthingt",
                                               "( ) chainlaoder fr b11s",
                                               "( ) gamehaxchsx",
                                               "( ) veru scartu",
                                               "( ) DANTTACHDIZ!!!",
                                               "( ) DANTTACHDIZ!!!DANTTACHDIZ!!!DANTTACHDIZ!!!",
                                               "( ) ERM11 Bri,k",
                                             };

    static const char *optionsDescription[]  = { "SamedingSameding"
                                                 "SamedingSamedingSameding"
                                               };

    FirmwareSource nandType = FIRMWARE_SYSNAND;
    if(isSdMode)
    {
        nandType = FIRMWARE_EMUNAND;
        locateEmuNand(&nandType);
    }

    struct multiOption {
        u32 posXs[4];
        u32 posY;
        u32 enabled;
        bool visible;
    } multiOptions[] = {
        { .visible = nandType == FIRMWARE_EMUNAND },
        { .visible = true },
        { .visible = true  },
        { .visible = true },
        { .visible = true },
        { .visible = ISN3DS },
    };

    struct singleOption {
        u32 posY;
        bool enabled;
        bool visible;
    } singleOptions[] = {
        { .visible = nandType == FIRMWARE_EMUNAND },
        { .visible = nandType == FIRMWARE_EMUNAND },
        { .visible = true },
        { .visible = true },
        { .visible = true },
        { .visible = true },
        { .visible = true },
        { .visible = true }
    };

    //Calculate the amount of the various kinds of options and pre-select the first single one
    u32 multiOptionsAmount = sizeof(multiOptions) / sizeof(struct multiOption),
        singleOptionsAmount = sizeof(singleOptions) / sizeof(struct singleOption),
        totalIndexes = multiOptionsAmount + singleOptionsAmount - 1,
        selectedOption,
        singleSelected;
    bool isMultiOption = false;

    //Parse the existing options
    for(u32 i = 0; i < multiOptionsAmount; i++)
    {
        //Detect the positions where the "x" should go
        u32 optionNum = 0;
        for(u32 j = 0; optionNum < 4 && j < strlen(multiOptionsText[i]); j++)
            if(multiOptionsText[i][j] == '(') multiOptions[i].posXs[optionNum++] = j + 1;
        while(optionNum < 4) multiOptions[i].posXs[optionNum++] = 0;

        multiOptions[i].enabled = MULTICONFIG(i);
    }
    for(u32 i = 0; i < singleOptionsAmount; i++)
        singleOptions[i].enabled = CONFIG(i);

    initScreens();

    static const char *bootTypes[] = { "B9S",
                                       "B9S (ntrboot)",
                                       "FIRM0",
                                       "FIRM1" };

    drawString(true, 10, 10, COLOR_TITLE, CONFIG_TITLE);
    drawString(true, 10, 10 + SPACING_Y, COLOR_TITLE, "Press A to select, START to save");
    drawFormattedString(false, 10, SCREEN_HEIGHT - 2 * SPACING_Y, COLOR_YELLOW, "Booted from %s via %s", isSdMode ? "SD" : "CTRNAND", bootTypes[(u32)bootType]);

    //Character to display a selected option
    char selected = 'x';

    u32 endPos = 10 + 2 * SPACING_Y;

    //Display all the multiple choice options in white
    for(u32 i = 0; i < multiOptionsAmount; i++)
    {
        if(!multiOptions[i].visible) continue;

        multiOptions[i].posY = endPos + SPACING_Y;
        endPos = drawString(true, 10, multiOptions[i].posY, COLOR_WHITE, multiOptionsText[i]);
        drawCharacter(true, 10 + multiOptions[i].posXs[multiOptions[i].enabled] * SPACING_X, multiOptions[i].posY, COLOR_WHITE, selected);
    }

    endPos += SPACING_Y / 2;

    //Display all the normal options in white except for the first one
    for(u32 i = 0, color = COLOR_RED; i < singleOptionsAmount; i++)
    {
        if(!singleOptions[i].visible) continue;

        singleOptions[i].posY = endPos + SPACING_Y;
        endPos = drawString(true, 10, singleOptions[i].posY, color, singleOptionsText[i]);
        if(singleOptions[i].enabled) drawCharacter(true, 10 + SPACING_X, singleOptions[i].posY, color, selected);

        if(color == COLOR_RED)
        {
            singleSelected = i;
            selectedOption = i + multiOptionsAmount;
            color = COLOR_WHITE;
        }
    }

    drawString(false, 10, 10, COLOR_WHITE, optionsDescription[selectedOption]);

    //Boring configuration menu
    while(true)
    {
        u32 pressed;
        do
        {
            pressed = waitInput(true) & MENU_BUTTONS;
        }
        while(!pressed);

        if(pressed == BUTTON_START) break;

        if(pressed != BUTTON_A)
        {
            //Remember the previously selected option
            u32 oldSelectedOption = selectedOption;

            while(true)
            {
                switch(pressed)
                {
                    case BUTTON_UP:
                        selectedOption = !selectedOption ? totalIndexes : selectedOption - 1;
                        break;
                    case BUTTON_DOWN:
                        selectedOption = selectedOption == totalIndexes ? 0 : selectedOption + 1;
                        break;
                    case BUTTON_LEFT:
                        pressed = BUTTON_DOWN;
                        selectedOption = 0;
                        break;
                    case BUTTON_RIGHT:
                        pressed = BUTTON_UP;
                        selectedOption = totalIndexes;
                        break;
                    default:
                        break;
                }

                if(selectedOption < multiOptionsAmount)
                {
                    if(!multiOptions[selectedOption].visible) continue;

                    isMultiOption = true;
                    break;
                }
                else
                {
                    singleSelected = selectedOption - multiOptionsAmount;

                    if(!singleOptions[singleSelected].visible) continue;

                    isMultiOption = false;
                    break;
                }
            }

            if(selectedOption == oldSelectedOption) continue;

            //The user moved to a different option, print the old option in white and the new one in red. Only print 'x's if necessary
            if(oldSelectedOption < multiOptionsAmount)
            {
                drawString(true, 10, multiOptions[oldSelectedOption].posY, COLOR_WHITE, multiOptionsText[oldSelectedOption]);
                drawCharacter(true, 10 + multiOptions[oldSelectedOption].posXs[multiOptions[oldSelectedOption].enabled] * SPACING_X, multiOptions[oldSelectedOption].posY, COLOR_WHITE, selected);
            }
            else
            {
                u32 singleOldSelected = oldSelectedOption - multiOptionsAmount;
                drawString(true, 10, singleOptions[singleOldSelected].posY, COLOR_WHITE, singleOptionsText[singleOldSelected]);
                if(singleOptions[singleOldSelected].enabled) drawCharacter(true, 10 + SPACING_X, singleOptions[singleOldSelected].posY, COLOR_WHITE, selected);
            }

            if(isMultiOption) drawString(true, 10, multiOptions[selectedOption].posY, COLOR_RED, multiOptionsText[selectedOption]);
            else drawString(true, 10, singleOptions[singleSelected].posY, COLOR_RED, singleOptionsText[singleSelected]);

            drawString(false, 10, 10, COLOR_BLACK, optionsDescription[oldSelectedOption]);
            drawString(false, 10, 10, COLOR_WHITE, optionsDescription[selectedOption]);
        }
        else
        {
            //The selected option's status changed, print the 'x's accordingly
            if(isMultiOption)
            {
                u32 oldEnabled = multiOptions[selectedOption].enabled;
                drawCharacter(true, 10 + multiOptions[selectedOption].posXs[oldEnabled] * SPACING_X, multiOptions[selectedOption].posY, COLOR_BLACK, selected);
                multiOptions[selectedOption].enabled = (oldEnabled == 3 || !multiOptions[selectedOption].posXs[oldEnabled + 1]) ? 0 : oldEnabled + 1;

                if(selectedOption == BRIGHTNESS) updateBrightness(multiOptions[BRIGHTNESS].enabled);
            }
            else
            {
                bool oldEnabled = singleOptions[singleSelected].enabled;
                singleOptions[singleSelected].enabled = !oldEnabled;
                if(oldEnabled) drawCharacter(true, 10 + SPACING_X, singleOptions[singleSelected].posY, COLOR_BLACK, selected);
            }
        }

        //In any case, if the current option is enabled (or a multiple choice option is selected) we must display a red 'x'
        if(isMultiOption) drawCharacter(true, 10 + multiOptions[selectedOption].posXs[multiOptions[selectedOption].enabled] * SPACING_X, multiOptions[selectedOption].posY, COLOR_RED, selected);
        else if(singleOptions[singleSelected].enabled) drawCharacter(true, 10 + SPACING_X, singleOptions[singleSelected].posY, COLOR_RED, selected);
    }

    //Parse and write the new configuration
    configData.multiConfig = 0;
    for(u32 i = 0; i < multiOptionsAmount; i++)
        configData.multiConfig |= multiOptions[i].enabled << (i * 2);

    configData.config = 0;
    for(u32 i = 0; i < singleOptionsAmount; i++)
        configData.config |= (singleOptions[i].enabled ? 1 : 0) << i;

    writeConfig(true);

    u32 newPinMode = MULTICONFIG(PIN);

    if(newPinMode != 0) newPin(oldPinStatus && newPinMode == oldPinMode, newPinMode);
    else if(oldPinStatus)
    {
        if(!fileDelete(PIN_FILE))
            error("Unable to delete PIN file");
    }

    while(HID_PAD & PIN_BUTTONS);
    wait(2000ULL);
}
