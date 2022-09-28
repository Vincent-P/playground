#pragma once
#include "assets/importers/importer.h"

enum struct KTX2Errors
{
	CreateFailed,
	TranscodeFailed,
};

struct LibKtxError
{
	int error_code;
};

struct KTX2Importer final : Importer
{
	static constexpr u64 importer_id = 0x3;

	bool can_import_extension(std::span<std::string_view const> extensions) final;
	bool can_import_blob(std::span<u8 const> blob) final;

	virtual Result<CreateResponse>  create_asset(const CreateRequest &request) override;
	virtual Result<ProcessResponse> process_asset(const ProcessRequest &request) override;
};
