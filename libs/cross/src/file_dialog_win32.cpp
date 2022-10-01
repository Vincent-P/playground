#include "cross/file_dialog.h"

#include "utils_win32.h"
#include <exo/macros/defer.h>
#include <exo/maths/numerics.h>

#include <shobjidl.h>
#include <windows.h>

namespace cross
{
Option<std::filesystem::path> file_dialog(Vec<std::pair<std::string, std::string>> extensions)
{
	// Convert the extensions list to windows' wonderful utf16 madness
	Vec<std::wstring> string_holder;
	string_holder.reserve(2 * extensions.size());
	Vec<COMDLG_FILTERSPEC> filters(extensions.size());
	for (uint i_filter = 0; i_filter < extensions.size(); i_filter++) {
		usize i_name_str = string_holder.size();
		string_holder.emplace_back(utils::utf8_to_utf16(extensions[i_filter].first));
		usize i_filter_str = string_holder.size();
		string_holder.emplace_back(utils::utf8_to_utf16(extensions[i_filter].second));

		filters[i_filter].pszName = string_holder[i_name_str].c_str();
		filters[i_filter].pszSpec = string_holder[i_filter_str].c_str();
	}

	// Call CoInitializeEx to initialize the COM library.
	HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if (!SUCCEEDED(hr)) {
		return {};
	}
	DEFER { CoUninitialize(); };

	// Call CoCreateInstance to create the Common Item Dialog object and get a pointer to the object's IFileOpenDialog
	// interface.
	IFileOpenDialog *pFileOpen = nullptr;
	hr                         = CoCreateInstance(CLSID_FileOpenDialog,
        nullptr,
        CLSCTX_ALL,
        IID_IFileOpenDialog,
        reinterpret_cast<void **>(&pFileOpen));
	if (!SUCCEEDED(hr)) {
		return {};
	}
	DEFER { pFileOpen->Release(); };

	// Set the file types
	hr = pFileOpen->SetFileTypes(static_cast<UINT>(filters.size()), filters.data());
	if (!SUCCEEDED(hr)) {
		return {};
	}

	// Call the object's Show method, which shows the dialog box to the user. This method blocks until the user
	// dismisses the dialog box.
	hr = pFileOpen->Show(nullptr);
	if (!SUCCEEDED(hr)) {
		return {};
	}

	// Call the object's GetResult method. This method returns a pointer to a second COM object, called a Shell item
	// object. The Shell item, which implements the IShellItem interface, represents the file that the user selected.
	IShellItem *pItem = nullptr;
	hr = hr = pFileOpen->GetResult(&pItem);
	if (!SUCCEEDED(hr)) {
		return {};
	}
	DEFER { pItem->Release(); };

	// Call the Shell item's GetDisplayName method. This method gets the file path, in the form of a string.
	PWSTR pszFilePath = nullptr;
	hr = hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
	if (!SUCCEEDED(hr)) {
		return {};
	}
	DEFER { CoTaskMemFree(pszFilePath); };

	return utils::utf16_to_utf8(pszFilePath);
}
} // namespace cross
