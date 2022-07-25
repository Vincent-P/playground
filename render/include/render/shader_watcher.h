#pragma once

#define SHADER_FOLDER OUT_DIR "/" TARGET_NAME "/shaders/"
#define SHADER_PATH(shader) SHADER_FOLDER shader

#define WATCH_LIB_SHADER(watcher) watcher.add_watch(SHADER_FOLDER);
