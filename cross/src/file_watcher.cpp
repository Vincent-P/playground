// WIN32: https://docs.microsoft.com/en-us/windows/win32/fileio/obtaining-directory-change-notifications
// linux: inotify/select https://developer.ibm.com/tutorials/l-inotify/
#include "cross/file_watcher.h"

#ifdef __linux__

#include <fcntl.h>
#include <sys/inotify.h>
#include <unistd.h>

#elif defined(CROSS_WINDOWS)

#include <Windows.h>
#include "utils_win32.h"

#endif

#include <exo/prelude.h>
#include <exo/algorithms.h>

#include <array>
#include <tracy/Tracy.hpp>

namespace cross
{
/// --- Linux
#if defined(__linux__)

static FileWatcher create_internal()
{
    FileWatcher fw{};

    fw.inotify_fd = inotify_init();
    ASSERT(fw.inotify_fd > 0);

    int flags = fcntl(fw.inotify_fd, F_GETFL, 0);
    int res = fcntl(fw.inotify_fd, F_SETFL, flags | O_NONBLOCK) >= 0;
    ASSERT(res);

    fw.current_events.reserve(10);

    return fw;
}

static void destroy_internal(FileWatcher &fw)
{
    ASSERT(fw.inotify_fd > 0);
    fw.inotify_fd = -1;
}

static Watch add_watch_internal(FileWatcher &fw, const char *path)
{
    Watch watch;
    watch.path = path;
    watch.wd   = inotify_add_watch(fw.inotify_fd, path, IN_MODIFY);
    ASSERT(watch.wd > 0);
    fw.watches.push_back(std::move(watch));
    return fw.watches.back();
}

static void fetch_events_internal(FileWatcher &fw)
{
    std::array<u8, 2048> buffer;
    ssize_t sbread = read(fw.inotify_fd, buffer.data(), buffer.size());
    if (sbread <= 0)
    {
        return;
    }

    usize bread = sbread;

    u8 *p_buffer = buffer.data();
    usize offset = 0;

    while (offset + sizeof(inotify_event) < bread)
    {
        auto *p_event    = reinterpret_cast<inotify_event *>(ptr_offset(p_buffer, offset));
        usize event_size = offsetof(inotify_event, name) + p_event->len;

        Event event;
        event.wd     = p_event->wd;
        event.mask   = p_event->mask;
        event.cookie = p_event->cookie;
        event.name   = std::string{p_event->name};
        fw.current_events.push_back(std::move(event));

        offset += event_size;
    }
}

#elif defined(_WIN64)

static FileWatcher create_internal()
{
    FileWatcher fw{};
    fw.current_events.reserve(10);
    return fw;
}

static void destroy_internal(FileWatcher &fw)
{
    for (auto &watch : fw.watches)
    {
        (void)(watch); // TODO: custom assert, unused variable on release
        BOOL res = CloseHandle(watch.directory_handle);
        ASSERT(res);
    }
}

static Watch add_watch_internal(FileWatcher &fw, const char *path)
{
    static int last_wd = 0;
    Watch watch;
    watch.path = path;
    watch.wd   = last_wd++;

    watch.directory_handle = CreateFileA(path,
                                         GENERIC_READ,
                                         FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                         nullptr,
                                         OPEN_EXISTING,
                                         FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, // needed to get changes
                                         nullptr);

    watch.overlapped = {};
    watch.buffer     = {};

    DWORD notify_flags = 0;
    notify_flags |= FILE_NOTIFY_CHANGE_LAST_WRITE; // timestamp changed
    notify_flags |= FILE_NOTIFY_CHANGE_FILE_NAME; // renaming, creating or deleting a file

    BOOL res = ReadDirectoryChangesW(watch.directory_handle,
                                 watch.buffer.data(),
                                 static_cast<DWORD>(watch.buffer.size()),
                                 true,
                                 notify_flags,
                                 nullptr,
                                 &watch.overlapped,
                                 nullptr);
    ASSERT(res);

    fw.watches.push_back(std::move(watch));
    return fw.watches.back();
}

static void fetch_events_internal(FileWatcher &fw)
{
    for (auto &watch : fw.watches)
    {

        DWORD bread = 0;
        BOOL res    = GetOverlappedResult(watch.directory_handle, &watch.overlapped, &bread, false);
        if (!res)
        {
            auto error = GetLastError();
            ASSERT(error == ERROR_IO_INCOMPLETE);
            UNUSED(error);
        }

        u8 *p_buffer = watch.buffer.data();
        usize offset = 0;

        while (offset + sizeof(FILE_NOTIFY_INFORMATION) < bread)
        {
            auto *p_event = reinterpret_cast<PFILE_NOTIFY_INFORMATION>(ptr_offset(p_buffer, offset));
            offset += p_event->NextEntryOffset;

            Event event;
            event.wd = watch.wd;

            std::wstring wname{p_event->FileName, p_event->FileNameLength / sizeof(wchar_t)};
            event.name = utf16_to_utf8(wname);
            event.len = p_event->FileNameLength / sizeof(wchar_t);

            if (p_event->Action == FILE_ACTION_ADDED)
            {
                event.action = WatchEvent::FileAdded;
            }
            else if (p_event->Action == FILE_ACTION_REMOVED)
            {
                event.action = WatchEvent::FileRemoved;
            }
            else if (p_event->Action == FILE_ACTION_MODIFIED)
            {
                event.action = WatchEvent::FileChanged;
            }
            else if (p_event->Action == FILE_ACTION_RENAMED_NEW_NAME)
            {
                event.action = WatchEvent::FileRenamed;
            }
            else
            {
                break;
            }

            fw.current_events.push_back(std::move(event));

            if (p_event->NextEntryOffset == 0)
            {
                break;
            }
        }

        DWORD notify_flags = 0;
        notify_flags |= FILE_NOTIFY_CHANGE_LAST_WRITE; // timestamp changed
        notify_flags |= FILE_NOTIFY_CHANGE_FILE_NAME; // renaming, creating or deleting a file

        res = ReadDirectoryChangesW(watch.directory_handle,
                                 watch.buffer.data(),
                                 static_cast<DWORD>(watch.buffer.size()),
                                 true,
                                 notify_flags,
                                 nullptr,
                                 &watch.overlapped,
                                 nullptr);
        ASSERT(res);
    }
}
#endif

static const Watch &watch_from_event_internal(const FileWatcher &fw, const Event &event)
{
    return *std::find_if(std::begin(fw.watches), std::end(fw.watches), [&](const auto &watch) {
        return watch.wd == event.wd;
    });
}

FileWatcher FileWatcher::create() { return create_internal(); }

Watch FileWatcher::add_watch(const char *path) { return add_watch_internal(*this, path); }

void FileWatcher::update()
{
    ZoneScoped;

    fetch_events_internal(*this);

    for (const auto &event : current_events)
    {
        const auto &watch = watch_from_event_internal(*this, event);
        for (const auto &cb : callbacks)
        {
            cb(watch, event);
        }
    }

    current_events.clear();
}

void FileWatcher::on_file_change(const FileEventF &f) { callbacks.push_back(f); }

void FileWatcher::destroy() { destroy_internal(*this); }
}
