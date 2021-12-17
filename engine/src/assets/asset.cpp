#include "assets/asset.h"
#include "assets/asset_constructors.h"

#include <exo/serializer.h>

// https://isocpp.org/wiki/faq/ctors#static-init-order-on-first-use
AssetConstructors &global_asset_constructors()
{
    static AssetConstructors *ac = new AssetConstructors();
    return *ac;
}

void Asset::serialize(exo::Serializer &serializer)
{
    serializer.serialize(this->uuid);
    serializer.serialize(this->name);
    serializer.serialize(this->dependencies);
}
