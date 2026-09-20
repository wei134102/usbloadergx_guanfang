#include "GUI/gui.h"
#include "usbloader/disc.h"
#include "FileOperations/fileops.h"
#include "settings/CSettings.h"
#include "themes/CTheme.h"
#include "plugin/plugin.hpp"

/****************************************************************************
 * LoadCoverImage
 ***************************************************************************/
GuiImageData *LoadCoverImage(struct discHdr *header, bool Prefere3D, bool noCover)
{
	if (!header)
		return NULL;
	GuiImageData *Cover = NULL;

	if (Settings.GameDisplayType == DISP_PLUGIN || Settings.pluginMode)
	{
		bool flag = Settings.Plugin3DCvrs;
		char Title[64];
		char Path[256];

		const char *romPath = m_plugin.GetRomPath(header->title);
		if (!romPath || romPath[0] == '\0')
		{
			const char *base = strrchr(header->title, '/') ? strrchr(header->title, '/') + 1 : header->title;
			snprintf(Title, sizeof(Title), "%s", base);
		}
		else
		{
			const char *base = strrchr(romPath, '/') + 1;
			snprintf(Title, sizeof(Title), "%s", base ? base : romPath);
		}
		char *coverPath = flag ? Settings.covers_path : Settings.covers2d_path;
		const char *folderName = m_plugin.GetCoverFolderName(header->magic);
		snprintf(Path, sizeof(Path), "%s%s/%s.png", coverPath, folderName ? folderName : "unknown", Title);

		if (CheckFile(Path))
		{
			delete Cover;
			Cover = new (std::nothrow) GuiImageData(Path);
			if (Cover && Cover->GetImage())
				return Cover;
		}

		delete Cover;
		Cover = Resources::GetImageData(flag ? "nocover.png" : "nocoverFlat.png");
		if (Cover && !Cover->GetImage())
		{
			delete Cover;
			Cover = NULL;
		}
		return Cover;
	}

	char ID3[4] = {};
	char IDfull[7];
	char Path[255];
	bool flag = Prefere3D;

	memcpy(ID3, (char *)header->id, sizeof(ID3) - 1);
	snprintf(IDfull, sizeof(IDfull), "%s", (char *)header->id);

	for (int i = 0; i < 2; ++i)
	{
		char *coverPath = flag ? Settings.covers_path : Settings.covers2d_path;
		flag = !flag;
		//Load full id image
		snprintf(Path, sizeof(Path), "%s%s.png", coverPath, IDfull);

		if (!CheckFile(Path))
		{
			snprintf(Path, sizeof(Path), "%s%s.png", coverPath, ID3);
			if (!CheckFile(Path))
				continue;
		}

		delete Cover;
		Cover = new (std::nothrow) GuiImageData(Path);
		//Load short id image
		if (!Cover || !Cover->GetImage())
		{
			snprintf(Path, sizeof(Path), "%s%s.png", coverPath, ID3);
			delete Cover;
			Cover = new (std::nothrow) GuiImageData(Path);
		}
		if (Cover && Cover->GetImage())
			break;
	}
	//Load no image
	if (noCover && (!Cover || !Cover->GetImage()))
	{
		flag = Prefere3D;
		for (int i = 0; i < 2; ++i)
		{
			delete Cover;
			Cover = Resources::GetImageData(flag ? "nocover.png" : "nocoverFlat.png");
			if (Cover && Cover->GetImage())
				break;
			flag = !flag;
		}
	}
	if (Cover && !Cover->GetImage())
	{
		delete Cover;
		Cover = NULL;
	}
	return Cover;
}
