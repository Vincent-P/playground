#pragma once

#include <exo/path.h>
#include <exo/serialization/serializer.h>

namespace exo
{
inline void serialize(Serializer &serializer, Path &path)
{
	if (serializer.is_writing) {
		auto path_string = path.view();
		auto path_c_str  = path_string.data();
		exo::serialize(serializer, path_c_str);
	} else {
		const char *path_string = "";
		exo::serialize(serializer, path_string);
		path = exo::Path::from_string(path_string);
	}
}
} // namespace exo
