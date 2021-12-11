#include "assets/asset.h"
#include "assets/asset_constructors.h"

// https://isocpp.org/wiki/faq/ctors#static-init-order-on-first-use
AssetConstructors &global_asset_constructors()
{
    static AssetConstructors *ac = new AssetConstructors();
    return *ac;
}

template <>
void Serializer::serialize<Asset>(Asset &data)
{
    serialize(data.uuid);
    serialize(data.name);
    serialize(data.dependencies);
}
