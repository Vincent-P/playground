#pragma once
#include "types.hpp"
#include <functional>
#include <vector>

namespace my_app
{

struct Watch
{

#ifdef __linux__
    int wd; /* Watch descriptor.  */
#endif

    std::string path;
};

struct Event
{

#ifdef __linux__
    int wd;     /* Watch descriptor.  */
    u32 mask;   /* Watch mask.  */
    u32 cookie; /* Cookie to synchronize two events.  */
#endif

    std::string name; /* filename. */
};

using FileEventF = std::function<void(const Watch &, const Event &)>;

struct FileWatcher
{
#ifdef __linux__
    int inotify_fd;
#endif

    std::vector<Watch> watches;
    std::vector<Event> current_events;
    std::vector<FileEventF> callbacks;

    static FileWatcher create();

    Watch add_watch(const char *path);
    void on_file_change(FileEventF f);

    void update();
    void destroy();
};

} // namespace my_app
