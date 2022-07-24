#pragma once

#include <exo/collections/vector.h>
#include <exo/result.h>
#include <exo/uuid.h>

#include <rapidjson/fwd.h>

#include "assets/subscene.h"

struct AssetManager;

enum struct GLTFError
{
	FirstChunkNotJSON,
	SecondChunkNotBIN,
};

struct GLTFImporter
{
	struct Settings
	{
		u32  i_scene                                 = 0;
		bool apply_transform                         = false;
		bool remove_degenerate_triangles             = false;
		bool operator==(const Settings &other) const = default;
	};

	struct Data
	{
		Settings       settings;
		Vec<exo::UUID> mesh_uuids;
		Vec<exo::UUID> texture_uuids;
		Vec<exo::UUID> material_uuids;
	};

	bool            can_import(const void *file_data, usize file_len);
	Result<Asset *> import(AssetManager *asset_manager,
		exo::UUID                        resource_uuid,
		const void                      *file_data,
		usize                            file_len,
		void                            *import_settings = nullptr);

	// Importer data
	void *create_default_importer_data();
	void *read_data_json(const rapidjson::Value &j_data);
	void  write_data_json(rapidjson::GenericPrettyWriter<rapidjson::FileWriteStream> &writer, const void *data);
};
