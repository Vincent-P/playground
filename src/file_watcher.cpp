// WIN32: https://docs.microsoft.com/en-us/windows/win32/fileio/obtaining-directory-change-notifications
// linux: inotify/select https://developer.ibm.com/tutorials/l-inotify/
#include "file_watcher.hpp"

#ifdef __linux__
#include <fcntl.h>
#include <sys/inotify.h>
#include <unistd.h>
#endif

#include <array>
#include <algorithm>
#include <cassert>
#include <iostream>

#include "types.hpp"

namespace my_app
{

/// --- Linux
#ifdef __linux__

static FileWatcher create_internal()
{
    FileWatcher fw{};

    fw.inotify_fd = inotify_init();
    assert(fw.inotify_fd > 0);

    int flags = fcntl(fw.inotify_fd, F_GETFL, 0);
    assert(fcntl(fw.inotify_fd, F_SETFL, flags | O_NONBLOCK) >= 0);

    fw.current_events.reserve(10);

    return fw;
}

static void destroy_internal(FileWatcher& fw)
{
    assert(fw.inotify_fd > 0);
    fw.inotify_fd = -1;
}

static Watch add_watch_internal(FileWatcher &fw, const char* path)
{
    Watch watch;
    watch.path = path;
    watch.wd = inotify_add_watch(fw.inotify_fd, path, IN_MODIFY);
    assert(watch.wd > 0);
    fw.watches.push_back(std::move(watch));
    return fw.watches.back();
}

static void fetch_events_internal(FileWatcher &fw)
{
    std::array<u8, 2048> buffer;
    ssize_t sbread = read(fw.inotify_fd, buffer.data(), buffer.size());
    if (sbread <= 0) {
	return;
    }

    usize bread = sbread;

    u8 *p_buffer = buffer.data();
    usize offset = 0;

    while (offset + sizeof(inotify_event) < bread) {
	auto *p_event    = reinterpret_cast<inotify_event *>(ptr_offset(p_buffer, offset));
	usize event_size = offsetof(inotify_event, name) + p_event->len;

	Event event;
	event.wd     = p_event->wd;
	event.mask   = p_event->mask;
	event.cookie = p_event->cookie;
	event.name   = std::string{p_event->name, p_event->len};
	fw.current_events.push_back(std::move(event));

	offset += event_size;
    }
}

static const Watch& watch_from_event_internal(const FileWatcher &fw, const Event &event)
{
    return *std::find_if(std::begin(fw.watches), std::end(fw.watches), [&](const auto &watch) { return watch.wd == event.wd; });
}

#endif

FileWatcher FileWatcher::create()
{
    return create_internal();
}

Watch FileWatcher::add_watch(const char *path)
{
    return add_watch_internal(*this, path);
}

void FileWatcher::update()
{
    fetch_events_internal(*this);

    for (const auto &event : current_events) {
        auto &watch = watch_from_event_internal(*this, event);
        for (const auto &cb : callbacks) {
            cb(watch, event);
        }
    }

    current_events.clear();
}

void FileWatcher::on_file_change(FileEventF f)
{
    callbacks.push_back(f);
}

void FileWatcher::destroy()
{
    destroy_internal(*this);
}

} // namespace my_app
