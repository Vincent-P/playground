#pragma once
#include <exo/collections/vector.h>
#include <exo/maths/numerics.h>

#include <functional>
#include <string>

#if defined(PLATFORM_WINDOWS)
#include <array>
#include <wtypes.h> // HANDLE type
#endif

namespace cross
{
struct Watch
{

#if defined(PLATFORM_LINUX)

#elif defined(PLATFORM_WINDOWS)
	HANDLE     directory_handle;
	OVERLAPPED overlapped;

	std::array<u8, 2048> buffer;
#endif

	int         wd; /* Watch descriptor.  */
	std::string path;
};

enum struct WatchEventAction
{
	FileRenamed,
	FileChanged,
	FileRemoved,
	FileAdded
};

struct WatchEvent
{

#if defined(PLATFORM_LINUX)
	u32 mask;   /* Watch mask.  */
	u32 cookie; /* Cookie to synchronize two events.  */
#elif defined(PLATFORM_WINDOWS)

#endif

	int              wd;   /* Watch descriptor.  */
	std::string      name; /* filename. */
	usize            len;
	WatchEventAction action;
};

using FileEventF = std::function<void(const Watch &, const WatchEvent &)>;

struct FileWatcher
{
#if defined(PLATFORM_LINUX)
	int inotify_fd;
#elif defined(PLATFORM_WINDOWS)

#endif

	Vec<Watch>      watches;
	Vec<WatchEvent> current_events;
	Vec<FileEventF> callbacks;

	static FileWatcher create();

	Watch add_watch(const char *path);
	void  on_file_change(const FileEventF &f);

	void update();
	void destroy();
};
} // namespace cross
