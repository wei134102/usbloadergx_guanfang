
#include "GUI/gui.h"
#include "wpad.h"

#include <unistd.h>
#include "settings/CSettings.h"
#include "main.h"
#include "themes/CTheme.h"
#include "utils/tools.h"
#include "menu.h"
#include "plugin/plugin.hpp"
#include "plugin/PluginBrowser.hpp"

#include <string.h>
#include <sstream>

#define GAMESELECTSIZE	  30

GuiPluginBrowser::GuiPluginBrowser(int w, int h)// int offset
	: scrollBar(h-10)
{
	width = w;//396
	height = h;//280
	pagesize = thInt("9 - plugin list browser page size");
	selectable = true;
	listOffset = 0;
	//listOffset = LIMIT(offset, 0, MAX(0, gameList.size()-pagesize));
	selectedItem = 0;

	m_plugin.createPluginsList();
	
	trigA = new GuiTrigger;
	trigA->SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	scrollBar.SetParent(this);
	scrollBar.SetAlignment(thAlign("right - plugin browser scrollbar align hor"), thAlign("top - plugin browser scrollbar align ver"));
	scrollBar.SetPosition(thInt("0 - plugin browser scrollbar pos x"), thInt("5 - plugin browser scrollbar pos y"));
	scrollBar.SetButtonScroll(WPAD_BUTTON_B | WPAD_CLASSIC_BUTTON_B);
	scrollBar.SetPageSize(pagesize);
	scrollBar.SetSelectedItem(selectedItem);
	scrollBar.SetSelectedIndex(listOffset);
	scrollBar.SetEntrieCount(m_plugin.PluginsSize());
	scrollBar.listChanged.connect(this, &GuiPluginBrowser::onListChange);

	bgPlugins = Resources::GetImageData("bg_options.png");
	bgPluginsImg = new GuiImage(bgPlugins);
	bgPluginsImg->SetParent(this);
	bgPluginsImg->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);

	bgPluginsEntry = Resources::GetImageData("bg_options_entry.png");

	maxTextWidth = bgPluginsImg->GetWidth() - scrollBar.GetWidth() - 38;

	plugin = new GuiButton *[pagesize];
	pluginTxt = new GuiText *[pagesize];
	pluginTxtOver = new GuiText *[pagesize];
	pluginBg = new GuiImage *[pagesize];

	for (int i = 0; i < pagesize; ++i)
	{
		pluginTxt[i] = new GuiText((char *) NULL, 20, thColor("r=0 g=0 b=0 a=255 - plugin browser list text color"));
		pluginTxt[i]->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
		pluginTxt[i]->SetPosition(24, 0);
		pluginTxt[i]->SetMaxWidth(maxTextWidth, DOTTED);

		pluginTxtOver[i] = new GuiText((char *) NULL, 20, thColor("r=0 g=0 b=0 a=255 - plugin browser list text color over"));
		pluginTxtOver[i]->SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
		pluginTxtOver[i]->SetPosition(24, 0);
		pluginTxtOver[i]->SetMaxWidth(maxTextWidth, SCROLL_HORIZONTAL);

		pluginBg[i] = new GuiImage(bgPluginsEntry);

		plugin[i] = new GuiButton(width - scrollBar.GetWidth(), GAMESELECTSIZE);
		plugin[i]->SetParent(this);
		plugin[i]->SetLabel(pluginTxt[i]);
		plugin[i]->SetLabelOver(pluginTxtOver[i]);
		plugin[i]->SetImageOver(pluginBg[i]);
		plugin[i]->SetPosition(5, GAMESELECTSIZE * i + 4);
		plugin[i]->SetRumble(false);
		plugin[i]->SetTrigger(trigA);
		plugin[i]->SetSoundClick(btnSoundClick);
		plugin[i]->SetVisible(false);
		plugin[i]->SetState(STATE_DISABLED);
	}
	UpdateListEntries();
}

/**
 * Destructor for the GuiGameList class.
 */
GuiPluginBrowser::~GuiPluginBrowser()
{
	delete bgPluginsImg;
	delete bgPlugins;
	delete bgPluginsEntry;

	delete trigA;

	for (int i = 0; i < pagesize; ++i)
	{
		delete pluginTxt[i];
		delete pluginTxtOver[i];
		delete pluginBg[i];
		delete plugin[i];
	}
	delete[] plugin;
	delete[] pluginTxt;
	delete[] pluginTxtOver;
	delete[] pluginBg;
}

void GuiPluginBrowser::SetFocus(int f)
{
	LOCK( this );
	if (!m_plugin.PluginsSize()) return;

	for (int i = 0; i < pagesize; ++i)
		plugin[i]->ResetState();

	if (f == 1) plugin[selectedItem]->SetState(STATE_SELECTED);
}

void GuiPluginBrowser::ResetState()
{
	LOCK( this );
	if (state != STATE_DISABLED)
	{
		state = STATE_DEFAULT;
		stateChan = -1;
	}

	for (int i = 0; i < pagesize; ++i)
	{
		plugin[i]->ResetState();
	}
}

int GuiPluginBrowser::GetClickedOption()
{
	int found = -1;
	for (int i = 0; i < pagesize; ++i)
	{
		if (plugin[i]->GetState() == STATE_CLICKED)
		{
			plugin[i]->SetState(STATE_SELECTED);
			found = listOffset + i;
			break;
		}
	}
	return found;
}

void GuiPluginBrowser::onListChange(int SelItem, int SelInd)
{
	selectedItem = SelItem;
	listOffset = SelInd;
	UpdateListEntries();
}

void GuiPluginBrowser::setListOffset(int off)
{
	LOCK(this);
	listOffset = LIMIT(off, 0, MAX(0, m_plugin.PluginsSize()-pagesize));
}

void GuiPluginBrowser::SetSelectedOption(int ind)
{
	LOCK(this);
	selectedItem = LIMIT(ind, 0, MIN(pagesize, MAX(0, m_plugin.PluginsSize()-1)));
}

/**
 * Draw the button on screen
 */
void GuiPluginBrowser::Draw()
{
	LOCK( this );
	if (!this->IsVisible() || !m_plugin.PluginsSize()) return;

	bgPluginsImg->Draw();

	for (int i = 0, next = listOffset; i < pagesize; ++i, ++next)
	{
		if (next < m_plugin.PluginsSize())
			plugin[i]->Draw();
	}

	scrollBar.Draw();

	this->UpdateEffects();
}

void GuiPluginBrowser::UpdateListEntries()
{
	for (int i = 0, next = listOffset; i < pagesize; ++i, ++next)
	{
		if (next < m_plugin.PluginsSize())
		{
			if (plugin[i]->GetState() == STATE_DISABLED)
			{
				plugin[i]->SetVisible(true);
				plugin[i]->SetState(STATE_DEFAULT);
			}
			pluginTxt[i]->SetText(m_plugin.GetPluginName(next).c_str());// plugin displayname
			pluginTxt[i]->SetPosition(24, 0);
			pluginTxtOver[i]->SetText(m_plugin.GetPluginName(next).c_str());
			pluginTxtOver[i]->SetPosition(24, 0);
		}
		else
		{
			plugin[i]->SetVisible(false);
			plugin[i]->SetState(STATE_DISABLED);
		}
	}
}

void GuiPluginBrowser::Update(GuiTrigger * t)
{
	LOCK( this );
	if (state == STATE_DISABLED || !t || !m_plugin.PluginsSize()) return;

	static int pressedChan = -1;

	if((t->wpad.btns_d & (WPAD_BUTTON_B | WPAD_BUTTON_DOWN | WPAD_BUTTON_UP | WPAD_BUTTON_LEFT | WPAD_BUTTON_RIGHT |
						  WPAD_CLASSIC_BUTTON_B | WPAD_CLASSIC_BUTTON_UP | WPAD_CLASSIC_BUTTON_DOWN | WPAD_CLASSIC_BUTTON_LEFT | WPAD_CLASSIC_BUTTON_RIGHT)) ||
		(t->pad.btns_d & (PAD_BUTTON_UP | PAD_BUTTON_DOWN)))
		pressedChan = t->chan;

	// update the location of the scroll box based on the position in the option list
	scrollBar.Update(t);

	if(pressedChan == -1 || (!t->wpad.btns_h && !t->pad.btns_h))
	{
		for (int i = 0, next = listOffset; i < pagesize; ++i, ++next)
		{
			if (next >= m_plugin.PluginsSize())
				break;

			if (i != selectedItem && plugin[i]->GetState() == STATE_SELECTED)
				plugin[i]->ResetState();
			else if (i == selectedItem && plugin[i]->GetState() == STATE_DEFAULT)
				plugin[selectedItem]->SetState(STATE_SELECTED, -1);

			plugin[i]->Update(t);

			if (plugin[i]->GetState() == STATE_SELECTED)
				selectedItem = i;
		}
	}

	if(pressedChan == t->chan && !t->wpad.btns_d && !t->wpad.btns_h)
		pressedChan = -1;

	scrollBar.SetPageSize(pagesize);
	scrollBar.SetSelectedItem(selectedItem);
	scrollBar.SetSelectedIndex(listOffset);
	scrollBar.SetEntrieCount(m_plugin.PluginsSize());

	if (updateCB) updateCB(this);
}

