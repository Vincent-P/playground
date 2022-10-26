#include "gameplay/components/mesh_component.h"

#include <assets/asset_id.h>

void MeshComponent::serialize(exo::Serializer &serializer)
{
	Super::serialize(serializer);
	exo::serialize(serializer, this->mesh_asset);
}
