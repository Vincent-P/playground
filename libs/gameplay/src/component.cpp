#include "gameplay/component.h"

#include "exo/serialization/serializer.h"
#include "exo/serialization/string_serializer.h"
#include "exo/serialization/uuid_serializer.h"
#include "reflection/reflection_serializer.h"

void SpatialComponent::set_local_transform(const float4x4 &new_transform)
{
	local_transform = new_transform;
	this->update_world_transform();
}

void SpatialComponent::set_local_bounds(const exo::AABB &new_bounds)
{
	local_bounds = new_bounds;
	// TODO: compute world_bounds
}

void SpatialComponent::update_world_transform()
{
	world_transform = local_transform;
	auto p          = parent;
	while (p.get() != nullptr) {
		world_transform = p->local_transform * world_transform;
		p               = p->parent;
	}

	for (auto child : children) {
		child->update_world_transform();
	}
}

void BaseComponent::serialize(exo::Serializer &serializer)
{
	exo::serialize(serializer, this->uuid);
	exo::serialize(serializer, this->name);
}

void SpatialComponent::serialize(exo::Serializer &serializer)
{
	Super::serialize(serializer);

	exo::serialize(serializer, this->local_transform);
	exo::serialize(serializer, this->local_bounds.min);
	exo::serialize(serializer, this->local_bounds.max);

	exo::serialize(serializer, this->children);
}
