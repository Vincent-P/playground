#pragma once
#include "reflection/reflection.h"
#include "exo/serialization/serializer.h"

// Serialize an object from a reflection-based pointer
template <exo::MemberSerializable T>
void serialize(exo::Serializer &serializer, refl::BasePtr<T> &ptr)
{
	// Read the class id
	u64 class_id = ptr.typeinfo().class_id;
	exo::serialize(serializer, class_id);

	if (!serializer.is_writing) {
		// Check that we are loading into an empty ptr
		ASSERT(!ptr.get());

		// Check that the serialized type exists
		const auto *type_info = refl::get_type_info(class_id);
		ASSERT(type_info);

		// Construct the new object in heap-allocated memory
		void *memory     = malloc(type_info->size);
		auto *object_ptr = static_cast<T *>(type_info->placement_ctor(memory));
		ptr              = refl::BasePtr<T>(object_ptr, *type_info);
	}

	ptr->serialize(serializer);
}
