/****************************************************************************
 * Copyright (C) 2016
 * by Fledge68
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you
 * must not claim that you wrote the original software. If you use
 * this software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and
 * must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 ***************************************************************************/
#include <unistd.h>
#include "PluginPrompt.hpp"
#include "language/gettext.h"
#include "themes/gettheme.h"
#include "themes/Resources.h"
#include "menu/menus.h"
#include "plugin/plugin.hpp"

PluginPrompt::PluginPrompt()
	: GuiWindow(0, 0)
{
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	trigB.SetButtonOnlyTrigger(-1, WPAD_BUTTON_B | WPAD_CLASSIC_BUTTON_B, PAD_BUTTON_B);

	btnOutline = Resources::GetImageData("button_dialogue_box.png");
	bgImgData = Resources::GetImageData("categoryPrompt.png");

	bgImg = new GuiImage(bgImgData);
	Append(bgImg);

	width = bgImg->GetWidth();
	height = bgImg->GetHeight();

	titleTxt = new GuiText(tr("Select Plugin"), 30, thColor("r=0 g=0 b=0 a=255 - plugin prompt title text color"));
	titleTxt->SetAlignment(thAlign("center - plugin prompt title text align hor"), thAlign("top - plugin prompt title text align ver"));
	titleTxt->SetPosition(thInt("0 - plugin prompt title text pos x"), thInt("10 - plugin prompt title text pos y"));
	Append(titleTxt);

	browser = new GuiPluginBrowser(thInt("396 - plugin browser width"),thInt("280 - plugin browser height"));
	browser->SetAlignment(thAlign("center - plugin browser align hor"), thAlign("top - plugin browser align ver"));
	browser->SetPosition(thInt("0 - plugin browser pos x"), thInt("45 - plugin browser pos y"));
	Append(browser);

	backImg = new GuiImage(btnOutline);
	backImg->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	backImg->SetScale(0.9f);
	backTxt = new GuiText(tr("Cancel"), 22, thColor("r=0 g=0 b=0 a=255 - plugin cancel button text color"));
	backBtn = new GuiButton(backImg->GetWidth()*0.9f, backImg->GetHeight()*0.9f);
	backBtn->SetImage(backImg);
	backBtn->SetLabel(backTxt);
	backBtn->SetAlignment(thAlign("center - plugin cancel button align hor"), thAlign("bottom - plugin cancel button align ver"));
	backBtn->SetPosition(thInt("110 - plugin cancel button pos x"), thInt("-20 - plugin cancel button pos y"));
	backBtn->SetSoundOver(btnSoundOver);
	backBtn->SetSoundClick(btnSoundClick);
	backBtn->SetTrigger(&trigA);
	backBtn->SetTrigger(&trigB);
	backBtn->SetEffectGrow();
	Append(backBtn);
}

PluginPrompt::~PluginPrompt()
{
	RemoveAll();
	delete browser;

	delete btnOutline;
	delete bgImgData;
	
	delete bgImg;
	delete titleTxt;
	delete backTxt;
	delete backImg;
	delete backBtn;
}

int PluginPrompt::Show()
{
	while(backBtn->GetState() != STATE_CLICKED)
	{
		usleep(10000);

		if (shutdown)
			Sys_Shutdown();
		else if (reset)
			Sys_Reboot();
			
		int pluginClicked = browser ? browser->GetClickedOption() : -1;

		if(pluginClicked >= 0 && pluginClicked < m_plugin.PluginsSize())
			return pluginClicked;
	}
	return -1;
}
