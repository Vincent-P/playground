#pragma once
#include "assets/importers/importer.h"

enum struct GLTFError
{
	FirstChunkNotJSON,
	SecondChunkNotBIN,
};

struct GLTFImporter final : Importer
{
	static constexpr u64 importer_id = 0x1;

	bool can_import_extension(std::span<std::string_view const> extensions) override;
	bool can_import_blob(std::span<u8 const> data) override;

	Result<CreateResponse>  create_asset(const CreateRequest &request) override;
	Result<ProcessResponse> process_asset(const ProcessRequest &request) override;
};
