#pragma once

#include <exo/collections/vector.h>
#include <exo/maths/numerics.h>
#include <exo/path.h>
#include <exo/result.h>

#include "exo/collections/span.h"
#include <string_view>

#include "assets/asset_id.h"

struct Asset;
struct ImporterApi;

struct CreateRequest
{
	AssetId   asset;
	exo::Path path;
};

struct CreateResponse
{
	AssetId        new_id;
	Vec<AssetId>   dependencies_id;    // must be created/processed before this one
	Vec<exo::Path> dependencies_paths; // must be created/processed before this one
};

struct ProcessRequest
{
	AssetId      asset;
	exo::Path    path;
	ImporterApi &importer_api;
};

struct ProcessResponse
{
	// jobs
	Vec<AssetId> products;
};

struct Importer
{
	virtual bool can_import_extension(exo::Span<std::string_view const> extensions) = 0;
	virtual bool can_import_blob(exo::Span<u8 const> data)                          = 0;

	virtual Result<CreateResponse>  create_asset(const CreateRequest &request)   = 0;
	virtual Result<ProcessResponse> process_asset(const ProcessRequest &request) = 0;
};
