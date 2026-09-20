#ifndef _GUIPLUGINBROWSER_H_
#define _GUIPLUGINBROWSER_H_

#include "GUI/gui.h"
#include "GUI/gui_scrollbar.hpp"

class GuiPluginBrowser : public GuiElement, public sigslot::has_slots<>// GUIGameBrowser
{
	public:
		GuiPluginBrowser(int w, int h);
		virtual ~GuiPluginBrowser();
		int GetClickedOption();
		int GetSelectedOption() { return listOffset+selectedItem; }
		void SetSelectedOption(int ind);
		void setListOffset(int off);
		int getListOffset() const { return listOffset; }
		void ResetState();
		void SetFocus(int f);
		void Draw();
		void Update(GuiTrigger * t);
	protected:
		void onListChange(int SelItem, int SelInd);
		void UpdateListEntries();
		int selectedItem;
		int listOffset;
		int pagesize;
		int maxTextWidth;

		GuiButton ** plugin;
		GuiText ** pluginTxt;
		GuiText ** pluginTxtOver;
		GuiImage ** pluginBg;

		GuiImage * bgPluginsImg;

		GuiImageData * bgPlugins;
		GuiImageData * bgPluginsEntry;

		GuiTrigger * trigA;

		GuiScrollbar scrollBar;
};
#endif
