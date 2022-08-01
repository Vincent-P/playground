#pragma once
#include "assets/importers/importer.h"

enum struct PNGErrors
{
	IhdrNotFound,
	CannotDecodeSize
};

struct PNGImporter final : Importer
{
	static constexpr u64 importer_id = 0x2;

	bool can_import_extension(std::span<std::string_view const> extensions) final;
	bool can_import_blob(std::span<u8 const> blob) final;

	Result<CreateResponse>  create_asset(const CreateRequest &request) override;
	Result<ProcessResponse> process_asset(const ProcessRequest &request) override;
};
