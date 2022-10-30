#include "gameplay/components/mesh_component.h"

#include "gameplay/contexts.h"

#include <assets/asset_id.h>
#include <assets/asset_manager.h>

void MeshComponent::load(LoadingContext &ctx)
{
	//
	Super::load(ctx);
}

void MeshComponent::unload(LoadingContext &ctx)
{
	//
	Super::unload(ctx);
}

void MeshComponent::update_loading(LoadingContext &ctx)
{
	auto asset = ctx.asset_manager->load_asset(this->mesh_asset);
	if (asset.is_valid()) {
		this->state = ComponentState::Loaded;
	}
}

void MeshComponent::serialize(exo::Serializer &serializer)
{
	Super::serialize(serializer);
	exo::serialize(serializer, this->mesh_asset);
}
