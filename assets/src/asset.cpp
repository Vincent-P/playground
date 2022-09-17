#include "assets/asset.h"
#include "assets/asset_constructors.h"

#include <exo/serializer.h>
#include <exo/string_serializer.h>

#include "assets/asset_id.h"
// https://isocpp.org/wiki/faq/ctors#static-init-order-on-first-use
AssetConstructors &global_asset_constructors()
{
	static auto *ac = new AssetConstructors();
	return *ac;
}

void Asset::serialize(exo::Serializer &serializer)
{
	exo::serialize(serializer, this->uuid);
	exo::serialize(serializer, this->name);
	exo::serialize(serializer, this->dependencies);
}
