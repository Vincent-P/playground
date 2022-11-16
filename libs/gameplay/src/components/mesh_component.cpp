#include "gameplay/components/mesh_component.h"

#include "gameplay/contexts.h"

#include "assets/asset_id.h"
#include "assets/asset_manager.h"

void MeshComponent::load(LoadingContext &ctx)
{
	ctx.asset_manager->load_asset_async(this->mesh_asset);
	state = ComponentState::Loading;
}

void MeshComponent::unload(LoadingContext &ctx)
{
	ctx.asset_manager->unload_asset(this->mesh_asset);
	state = ComponentState::Unloaded;
}

void MeshComponent::update_loading(LoadingContext &ctx)
{
	if (ctx.asset_manager->is_fully_loaded(this->mesh_asset)) {
		this->state = ComponentState::Loaded;
	}
}

void MeshComponent::serialize(exo::Serializer &serializer)
{
	Super::serialize(serializer);
	exo::serialize(serializer, this->mesh_asset);
}
