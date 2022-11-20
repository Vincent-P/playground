#include "assets/importers/gltf_importer.h"
#include "assets/asset_manager.h"
#include "assets/importers/importer.h"
#include "assets/material.h"
#include "assets/mesh.h"
#include "assets/subscene.h"
#include "assets/texture.h"
#include "cross/mapped_file.h"
#include "exo/collections/span.h"
#include "exo/format.h"
#include "exo/maths/pointer.h"
#include "exo/memory/scope_stack.h"
#include "exo/memory/string_repository.h"
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/prettywriter.h>

// -- glTF data utils

namespace gltf
{
enum struct ComponentType : i32
{
	Byte          = 5120,
	UnsignedByte  = 5121,
	Short         = 5122,
	UnsignedShort = 5123,
	UnsignedInt   = 5125,
	Float         = 5126,
	Invalid
};

inline u32 size_of(ComponentType type)
{
	switch (type) {
	case ComponentType::Byte:
	case ComponentType::UnsignedByte:
		return 1;

	case ComponentType::Short:
	case ComponentType::UnsignedShort:
		return 2;

	case ComponentType::UnsignedInt:
	case ComponentType::Float:
		return 4;

	default:
		ASSERT(false);
		return 4;
	}
}
enum struct ChunkType : u32
{
	Json   = 0x4E4F534A,
	Binary = 0x004E4942,
	Invalid,
};

struct Chunk
{
	u32       length;
	ChunkType type;
};
static_assert(sizeof(Chunk) == 2 * sizeof(u32));

struct Header
{
	u32   magic;
	u32   version;
	u32   length;
	Chunk first_chunk;
};

struct Accessor
{
	ComponentType component_type   = ComponentType::Invalid;
	u32           count            = 0;
	u32           nb_component     = 0;
	u32           bufferview_index = 0;
	u32           byte_offset      = 0;
	float         min_float;
	float         max_float;
};

struct BufferView
{
	u32 i_buffer    = u32_invalid;
	u32 byte_offset = 0;
	u32 byte_length = 1;
	u32 byte_stride = 0;
};

static Accessor get_accessor(const rapidjson::Value &object)
{
	const auto &accessor = object.GetObj();

	Accessor res = {};

	// technically not required but it doesn't make sense not to have one
	res.bufferview_index = accessor["bufferView"].GetUint();
	res.byte_offset      = 0;

	if (accessor.HasMember("byteOffset")) {
		res.byte_offset = accessor["byteOffset"].GetUint();
	}

	res.component_type = ComponentType(accessor["componentType"].GetInt());

	res.count = accessor["count"].GetUint();

	auto type = exo::StringView(accessor["type"].GetString());
	if (type == "SCALAR") {
		res.nb_component = 1;
	} else if (type == "VEC2") {
		res.nb_component = 2;
	} else if (type == "VEC3") {
		res.nb_component = 3;
	} else if (type == "VEC4" || type == "MAT2") {
		res.nb_component = 4;
	} else if (type == "MAT3") {
		res.nb_component = 9;
	} else if (type == "MAT4") {
		res.nb_component = 16;
	} else {
		ASSERT(false);
	}

	return res;
}

static BufferView get_bufferview(const rapidjson::Value &object)
{
	const auto &bufferview = object.GetObj();
	BufferView  res        = {};
	if (bufferview.HasMember("byteOffset")) {
		res.byte_offset = bufferview["byteOffset"].GetUint();
	}
	res.byte_length = bufferview["byteLength"].GetUint();
	if (bufferview.HasMember("byteStride")) {
		res.byte_stride = bufferview["byteStride"].GetUint();
	}
	if (bufferview.HasMember("buffer")) {
		res.i_buffer = bufferview["buffer"].GetUint();
	}
	return res;
}
} // namespace gltf

// -- glTF data utils end

struct ImporterContext
{
	ImporterApi               &api;
	const exo::Path           &main_path; // path of the gltf file
	SubScene                  *new_scene;
	const rapidjson::Document &j_document;
	Vec<cross::MappedFile>     files;
	Vec<exo::Span<const u8>>   buffers;
	const void                *binary_chunk       = nullptr;
	u32                        i_unnamed_mesh     = 0;
	u32                        i_unnamed_material = 0;
	u32                        i_unnamed_texture  = 0;
	AssetId                    main_id;
	Vec<AssetId>               material_ids;
	Vec<AssetId>               mesh_ids;
	Vec<AssetId>               texture_ids;

	Vec<uint>   indices;
	Vec<float4> positions;
	Vec<float2> uvs;

	[[nodiscard]] exo::Path relative_to_absolute_path(exo::StringView relative_path_str) const
	{
		auto dir = exo::Path::remove_filename(this->main_path);
		return exo::Path::join(dir, relative_path_str);
	}

	template <typename T>
	AssetId create_id(exo::StringView subname)
	{
		const exo::String copy = this->main_id.name + exo::StringView{"_"} + subname;
		return AssetId::create<T>(copy);
	}
};

static exo::Span<const u8> get_binary_data(
	ImporterContext &ctx, const gltf::BufferView &view, const gltf::Accessor &accessor, usize i_element = 0)

{
	const usize byte_stride =
		view.byte_stride > 0 ? view.byte_stride : gltf::size_of(accessor.component_type) * accessor.nb_component;

	const usize offset = view.byte_offset + accessor.byte_offset + i_element * byte_stride;

	const u8 *source = nullptr;
	if (view.i_buffer == u32_invalid) {
		source = reinterpret_cast<const u8 *>(ctx.binary_chunk);
	} else {
		source = ctx.buffers[view.i_buffer].data();
		ASSERT(ctx.buffers[view.i_buffer].len() >= view.byte_offset + view.byte_length);
		ASSERT(offset < view.byte_offset + view.byte_length);
	}
	ASSERT(source != nullptr);
	return exo::Span{exo::ptr_offset(source, offset), view.byte_length};
}

static void import_buffers(ImporterContext &ctx)
{
	if (!ctx.j_document.HasMember("buffers")) {
		return;
	}

	const auto &j_buffers = ctx.j_document["buffers"].GetArray();
	for (u32 i_mesh = 0; i_mesh < j_buffers.Size(); i_mesh += 1) {
		const auto &j_buffer = j_buffers[i_mesh];

		auto relative_path = exo::StringView{j_buffer["uri"].GetString()};
		auto absolute_path = ctx.relative_to_absolute_path(relative_path);
		auto bytelength    = j_buffer["byteLength"].GetUint();

		ctx.files.push(cross::MappedFile::open(absolute_path.view()).value());

		auto file_content = ctx.files.last().content();
		ASSERT(file_content.len() == bytelength);
		ctx.buffers.push(file_content);
	}
}

static void import_meshes(ImporterContext &ctx)
{
	const auto &j_accessors   = ctx.j_document["accessors"].GetArray();
	const auto &j_bufferviews = ctx.j_document["bufferViews"].GetArray();

	if (!ctx.j_document.HasMember("meshes")) {
		return;
	}

	exo::ScopeStack scope;

	const auto &j_meshes = ctx.j_document["meshes"].GetArray();
	ctx.mesh_ids.resize(j_meshes.Size());
	// Generate new UUID for the meshes if needed
	for (u32 i_mesh = 0; i_mesh < j_meshes.Size(); i_mesh += 1) {
		const auto &j_mesh = j_meshes[i_mesh];

		exo::String mesh_name;
		if (j_mesh.HasMember("name")) {
			mesh_name = j_mesh["name"].GetString();
		} else {
			mesh_name = exo::formatf(scope, "Mesh%u", ctx.i_unnamed_mesh);
			ctx.i_unnamed_mesh += 1;
		}
		auto mesh_uuid       = ctx.create_id<Mesh>(mesh_name);
		ctx.mesh_ids[i_mesh] = mesh_uuid;
		auto *new_mesh       = ctx.api.create_asset<Mesh>(mesh_uuid);
		ctx.positions.clear();
		ctx.uvs.clear();
		ctx.indices.clear();

		if (j_mesh.HasMember("name")) {
			new_mesh->name = exo::tls_string_repository->intern(j_mesh["name"].GetString());
		}

		for (auto &j_primitive : j_mesh["primitives"].GetArray()) {
			ASSERT(j_primitive.HasMember("attributes"));
			const auto &j_attributes = j_primitive["attributes"].GetObj();
			ASSERT(j_attributes.HasMember("POSITION"));

			new_mesh->submeshes.push();
			auto &new_submesh = new_mesh->submeshes.last();

			new_submesh.index_count  = 0;
			new_submesh.first_vertex = static_cast<u32>(ctx.positions.len());
			new_submesh.first_index  = static_cast<u32>(ctx.indices.len());
			new_submesh.material     = {};

			// -- Attributes
			ASSERT(j_primitive.HasMember("indices"));
			{
				auto j_accessor = j_accessors[j_primitive["indices"].GetUint()].GetObj();
				auto accessor   = gltf::get_accessor(j_accessor);
				auto bufferview = gltf::get_bufferview(j_bufferviews[accessor.bufferview_index]);

				// Copy the data from the binary buffer
				ctx.indices.reserve(accessor.count);
				for (usize i_index = 0; i_index < usize(accessor.count); i_index += 1) {
					u32 index = u32_invalid;
					if (accessor.component_type == gltf::ComponentType::UnsignedShort) {
						index =
							new_submesh.first_vertex +
							*reinterpret_cast<const u16 *>(get_binary_data(ctx, bufferview, accessor, i_index).data());
					} else if (accessor.component_type == gltf::ComponentType::UnsignedInt) {
						index =
							new_submesh.first_vertex +
							*reinterpret_cast<const u32 *>(get_binary_data(ctx, bufferview, accessor, i_index).data());
					} else {
						ASSERT(false);
					}
					ctx.indices.push(index);
				}

				new_submesh.index_count = accessor.count;
			}

			usize vertex_count = 0;
			{
				auto j_accessor = j_accessors[j_attributes["POSITION"].GetUint()].GetObj();
				auto accessor   = gltf::get_accessor(j_accessor);
				vertex_count    = accessor.count;
				auto bufferview = gltf::get_bufferview(j_bufferviews[accessor.bufferview_index]);

				// Copy the data from the binary buffer
				ctx.positions.reserve(accessor.count);
				for (usize i_position = 0; i_position < usize(accessor.count); i_position += 1) {
					float4 new_position = {1.0f};

					if (accessor.component_type == gltf::ComponentType::UnsignedShort) {
						const auto *components = reinterpret_cast<const u16 *>(
							get_binary_data(ctx, bufferview, accessor, i_position).data());
						new_position = {float(components[0]), float(components[1]), float(components[2]), 1.0f};
					} else if (accessor.component_type == gltf::ComponentType::Float) {
						const auto *components = reinterpret_cast<const float *>(
							get_binary_data(ctx, bufferview, accessor, i_position).data());
						new_position = {float(components[0]), float(components[1]), float(components[2]), 1.0f};
					} else {
						ASSERT(false);
					}

					ctx.positions.push(new_position);
				}
			}

			if (j_attributes.HasMember("TEXCOORD_0")) {
				auto j_accessor = j_accessors[j_attributes["TEXCOORD_0"].GetUint()].GetObj();
				auto accessor   = gltf::get_accessor(j_accessor);
				ASSERT(accessor.count == vertex_count);
				auto bufferview = gltf::get_bufferview(j_bufferviews[accessor.bufferview_index]);

				// Copy the data from the binary buffer
				ctx.uvs.reserve(accessor.count);
				for (usize i_uv = 0; i_uv < usize(accessor.count); i_uv += 1) {
					float2 new_uv = {1.0f};

					if (accessor.component_type == gltf::ComponentType::UnsignedShort) {
						const auto *components =
							reinterpret_cast<const u16 *>(get_binary_data(ctx, bufferview, accessor, i_uv).data());
						new_uv = {float(components[0]), float(components[1])};
					} else if (accessor.component_type == gltf::ComponentType::Float) {
						const auto *components =
							reinterpret_cast<const float *>(get_binary_data(ctx, bufferview, accessor, i_uv).data());
						new_uv = {float(components[0]), float(components[1])};
					} else {
						ASSERT(false);
					}

					ctx.uvs.push(new_uv);
				}
			} else {
				ctx.uvs.reserve(vertex_count);
				for (usize i = 0; i < vertex_count; i += 1) {
					ctx.uvs.push(float2(0.0f, 0.0f));
				}
			}

			if (j_primitive.HasMember("material")) {
				const u32 i_material = j_primitive["material"].GetUint();
				new_submesh.material = ctx.material_ids[i_material];
				new_mesh->add_dependency_checked(new_submesh.material);
			}
		}

		auto positions_bytes          = exo::span_to_bytes<float4>(ctx.positions);
		new_mesh->positions_hash      = ctx.api.save_blob(positions_bytes);
		new_mesh->positions_byte_size = positions_bytes.len();

		auto uvs_bytes          = exo::span_to_bytes<float2>(ctx.uvs);
		new_mesh->uvs_hash      = ctx.api.save_blob(uvs_bytes);
		new_mesh->uvs_byte_size = uvs_bytes.len();

		auto indices_bytes          = exo::span_to_bytes<uint>(ctx.indices);
		new_mesh->indices_hash      = ctx.api.save_blob(indices_bytes);
		new_mesh->indices_byte_size = indices_bytes.len();

		ctx.new_scene->add_dependency_checked(new_mesh->uuid);
	}
}

static void import_nodes(ImporterContext &ctx)
{
	auto get_transform = [](auto j_node) {
		float4x4 transform = float4x4::identity();

		if (j_node.HasMember("matrix")) {
			usize const i      = 0;
			auto        matrix = j_node["matrix"].GetArray();
			ASSERT(matrix.Size() == 16);

			for (u32 i_element = 0; i_element < matrix.Size(); i_element += 1) {
				transform.at(i % 4, i / 4) = static_cast<float>(matrix[i_element].GetDouble());
			}
		}

		if (j_node.HasMember("translation")) {
			auto     translation_factors = j_node["translation"].GetArray();
			float4x4 translation         = float4x4::identity();
			translation.at(0, 3)         = static_cast<float>(translation_factors[0].GetDouble());
			translation.at(1, 3)         = static_cast<float>(translation_factors[1].GetDouble());
			translation.at(2, 3)         = static_cast<float>(translation_factors[2].GetDouble());
			transform                    = translation;
		}

		if (j_node.HasMember("rotation")) {
			auto   rotation   = j_node["rotation"].GetArray();
			float4 quaternion = {0.0f};
			quaternion.x      = static_cast<float>(rotation[0].GetDouble());
			quaternion.y      = static_cast<float>(rotation[1].GetDouble());
			quaternion.z      = static_cast<float>(rotation[2].GetDouble());
			quaternion.w      = static_cast<float>(rotation[3].GetDouble());

			transform = transform * float4x4({
										1.0f - 2.0f * quaternion.y * quaternion.y - 2.0f * quaternion.z * quaternion.z,
										2.0f * quaternion.x * quaternion.y - 2.0f * quaternion.z * quaternion.w,
										2.0f * quaternion.x * quaternion.z + 2.0f * quaternion.y * quaternion.w,
										0.0f,
										2.0f * quaternion.x * quaternion.y + 2.0f * quaternion.z * quaternion.w,
										1.0f - 2.0f * quaternion.x * quaternion.x - 2.0f * quaternion.z * quaternion.z,
										2.0f * quaternion.y * quaternion.z - 2.0f * quaternion.x * quaternion.w,
										0.0f,
										2.0f * quaternion.x * quaternion.z - 2.0f * quaternion.y * quaternion.w,
										2.0f * quaternion.y * quaternion.z + 2.0f * quaternion.x * quaternion.w,
										1.0f - 2.0f * quaternion.x * quaternion.x - 2.0f * quaternion.y * quaternion.y,
										0.0f,
										0.0f,
										0.0f,
										0.0f,
										1.0f,
									});
		}

		if (j_node.HasMember("scale")) {
			auto     scale_factors = j_node["scale"].GetArray();
			float4x4 scale         = {};
			scale.at(0, 0)         = static_cast<float>(scale_factors[0].GetDouble());
			scale.at(1, 1)         = static_cast<float>(scale_factors[1].GetDouble());
			scale.at(2, 2)         = static_cast<float>(scale_factors[2].GetDouble());
			scale.at(3, 3)         = 1.0f;

			transform = transform * scale;
		}

		return transform;
	};

	const u32   i_scene  = ctx.j_document.HasMember("scene") ? ctx.j_document["scene"].GetUint() : 0;
	const auto &j_scenes = ctx.j_document["scenes"].GetArray();
	const auto &j_scene  = j_scenes[i_scene].GetObj();
	const auto &j_roots  = j_scene["nodes"].GetArray();

	for (const auto &j_root : j_roots) {
		ctx.new_scene->roots.push(j_root.GetUint());
	}

	const auto &j_nodes = ctx.j_document["nodes"].GetArray();

	ctx.new_scene->transforms.reserve(j_nodes.Size());
	ctx.new_scene->meshes.reserve(j_nodes.Size());
	ctx.new_scene->children.reserve(j_nodes.Size());
	ctx.new_scene->names.reserve(j_nodes.Size());

	for (const auto &j_node : j_nodes) {
		const auto &j_node_obj = j_node.GetObj();

		ctx.new_scene->transforms.push(get_transform(j_node_obj));

		if (j_node.HasMember("mesh")) {
			auto i_mesh = j_node["mesh"].GetUint();
			ctx.new_scene->meshes.push(ctx.mesh_ids[i_mesh]);
		} else {
			ctx.new_scene->meshes.push();
		}

		ctx.new_scene->names.push();
		if (j_node.HasMember("name")) {
			ctx.new_scene->names.last() = exo::tls_string_repository->intern(j_node["name"].GetString());
		} else {
			ctx.new_scene->names.last() = "No name";
		}

		ctx.new_scene->children.push();
		if (j_node.HasMember("children")) {
			const auto &j_children = j_node["children"].GetArray();
			ctx.new_scene->children.last().reserve(j_children.Size());
			for (const auto &j_child : j_children) {
				ctx.new_scene->children.last().push(j_child.GetUint());
			}
		}
	}
}

static void import_materials(ImporterContext &ctx)
{
	const auto &j_document = ctx.j_document;

	if (!ctx.j_document.HasMember("materials")) {
		return;
	}

	exo::ScopeStack scope;
	const auto     &j_materials = ctx.j_document["materials"].GetArray();
	ctx.material_ids.resize(j_materials.Size());
	for (u32 i_material = 0; i_material < j_materials.Size(); i_material += 1) {
		const auto &j_material = j_materials[i_material];

		exo::String material_name;
		if (j_material.HasMember("name")) {
			material_name = j_material["name"].GetString();
		} else {
			material_name = exo::formatf(scope, "Material%u", ctx.i_unnamed_material);
			ctx.i_unnamed_material += 1;
		}
		auto  material_uuid          = ctx.create_id<Material>(material_name);
		auto *new_material           = ctx.api.create_asset<Material>(material_uuid);
		ctx.material_ids[i_material] = material_uuid;

		if (j_material.HasMember("name")) {
			new_material->name = exo::tls_string_repository->intern(j_material["name"].GetString());
		}

		auto load_texture = [&](auto &json_object, const char *texture_name) -> u32 {
			if (json_object.HasMember(texture_name)) {
				const auto &j_texture_desc  = json_object[texture_name];
				u32 const   i_texture_index = j_texture_desc["index"].GetUint();
				const auto &j_texture       = j_document["textures"][i_texture_index];
				u32         i_texture       = u32_invalid;

				if (j_texture.HasMember("extensions")) {
					for (const auto &j_extension : j_texture["extensions"].GetObj()) {
						if (exo::StringView(j_extension.name.GetString()) == exo::StringView("KHR_texture_basisu")) {
							i_texture = j_extension.value["source"].GetUint();
							break;
						}
					}
				}

				if (i_texture == u32_invalid) {
					i_texture = j_texture["source"].GetUint();
				}

				ASSERT(i_texture != u32_invalid);
				return i_texture;
			}
			return u32_invalid;
		};

		if (auto i_normal_texture = load_texture(j_material, "normalTexture"); i_normal_texture != u32_invalid) {
			new_material->normal_texture = ctx.texture_ids[i_normal_texture];
			new_material->dependencies.push(new_material->normal_texture);
		}

		if (j_material.HasMember("pbrMetallicRoughness")) {
			const auto &j_pbr = j_material["pbrMetallicRoughness"].GetObj();

			if (auto i_base_color = load_texture(j_pbr, "baseColorTexture"); i_base_color != u32_invalid) {
				new_material->base_color_texture = ctx.texture_ids[i_base_color];
				new_material->dependencies.push(new_material->base_color_texture);

// TODO: implement KHR_texture_transform
#if 0
				if (j_base_color_texture.HasMember("extensions") &&
					j_base_color_texture["extensions"].HasMember("KHR_texture_transform")) {
					const auto &extension = j_base_color_texture["extensions"]["KHR_texture_transform"];
					if (extension.HasMember("offset")) {
						new_material->uv_transform.offset[0] = extension["offset"].GetArray()[0].GetFloat();
						new_material->uv_transform.offset[1] = extension["offset"].GetArray()[1].GetFloat();
					}
					if (extension.HasMember("scale")) {
						new_material->uv_transform.scale[0] = extension["scale"].GetArray()[0].GetFloat();
						new_material->uv_transform.scale[1] = extension["scale"].GetArray()[1].GetFloat();
					}
					if (extension.HasMember("rotation")) {
						new_material->uv_transform.rotation = extension["rotation"].GetFloat();
					}
				}
#endif
			}

			if (auto i_metallic_roughness = load_texture(j_pbr, "metallicRoughnessTexture");
				i_metallic_roughness != u32_invalid) {
				new_material->metallic_roughness_texture = ctx.texture_ids[i_metallic_roughness];
				new_material->dependencies.push(new_material->metallic_roughness_texture);
			}

			if (j_pbr.HasMember("baseColorFactor")) {
				new_material->base_color_factor = {1.0, 1.0, 1.0, 1.0};
				for (u32 i = 0; i < 4; i += 1) {
					new_material->base_color_factor[i] = j_pbr["baseColorFactor"].GetArray()[i].GetFloat();
				}
			}

			if (j_pbr.HasMember("metallicFactor")) {
				new_material->metallic_factor = j_pbr["metallicFactor"].GetFloat();
			}

			if (j_pbr.HasMember("roughnessFactor")) {
				new_material->roughness_factor = j_pbr["roughnessFactor"].GetFloat();
			}
		}
	}
}

static void import_textures(ImporterContext &ctx)
{
	if (!ctx.j_document.HasMember("images")) {
		return;
	}
	exo::ScopeStack scope;

	const auto &j_images = ctx.j_document["images"].GetArray();

	ctx.texture_ids.resize(j_images.Size());
	for (u32 i_image = 0; i_image < j_images.Size(); i_image += 1) {
		const auto &j_image = j_images[i_image];

		exo::String texture_name;
		if (j_image.HasMember("name")) {
			texture_name = j_image["name"].GetString();
		} else {
			texture_name = exo::formatf(scope, "Texture%u", ctx.i_unnamed_texture);
			ctx.i_unnamed_texture += 1;
		}
		ctx.texture_ids[i_image] = ctx.create_id<Texture>(texture_name);

		auto *new_texture = ctx.api.retrieve_asset<Texture>(ctx.texture_ids[i_image]);
		if (j_image.HasMember("name")) {
			new_texture->name = texture_name;
		}
	}
}

bool GLTFImporter::can_import_extension(exo::Span<const exo::StringView> extensions)
{
	for (const auto &extension : extensions) {
		if (extension == exo::StringView{".gltf"}) {
			return true;
		}
	}
	return false;
}

bool GLTFImporter::can_import_blob(exo::Span<const u8> data)
{
	ASSERT(data.len() > 4);
	return data[0] == 'g' && data[1] == 'l' && data[2] == 'T' && data[3] == 'F';
}

Result<CreateResponse> GLTFImporter::create_asset(const CreateRequest &request)
{
	CreateResponse response{};
	if (request.asset.is_valid()) {
		response.new_id = request.asset;
	} else {
		response.new_id = AssetId::create<SubScene>(request.path.filename());
	}

	auto file = cross::MappedFile::open(request.path.view()).value();
	auto file_content_str =
		exo::StringView{reinterpret_cast<const char *>(file.content().data()), file.content().len()};
	rapidjson::Document document;
	document.Parse(file_content_str.data(), file_content_str.len());

	if (document.HasParseError()) {
		return Err<Asset *>(AssetErrors::ParsingError);
	}

	exo::ScopeStack scope;

	u32 i_unnamed_texture = 0;

	if (document.HasMember("images")) {
		const auto &j_images = document["images"].GetArray();

		for (u32 i_image = 0; i_image < j_images.Size(); i_image += 1) {
			const auto &j_image = j_images[i_image];

			// glb images have a bufferView pointing to the binary chunk,
			// gltf images have an uri field pointing to the path

			// Generate texture id
			exo::String texture_name;
			if (j_image.HasMember("name")) {
				texture_name = j_image["name"].GetString();
			} else {
				texture_name = exo::formatf(scope, "Texture%u", i_unnamed_texture);
				i_unnamed_texture += 1;
			}

			const exo::String copy          = response.new_id.name + exo::StringView{"_"} + texture_name;
			auto              texture_id    = AssetId::create<Texture>(copy);
			auto              relative_path = j_image["uri"].GetString();
			auto              absolute_path = exo::Path::replace_filename(request.path, relative_path);

			response.dependencies_id.push(texture_id);
			response.dependencies_paths.push(absolute_path);
		}
	}

	return Ok(std::move(response));
}

Result<ProcessResponse> GLTFImporter::process_asset(const ProcessRequest &request)
{
	auto file = cross::MappedFile::open(request.path.view()).value();
	auto file_content_str =
		exo::StringView{reinterpret_cast<const char *>(file.content().data()), file.content().len()};
	rapidjson::Document document;
	document.Parse(file_content_str.data(), file_content_str.len());

	if (document.HasParseError()) {
		return Err<Asset *>(AssetErrors::ParsingError);
	}

	auto *new_scene = request.importer_api.create_asset<SubScene>(request.asset);

	ImporterContext ctx = {
		.api        = request.importer_api,
		.main_path  = request.path,
		.new_scene  = new_scene,
		.j_document = document,
		.main_id    = request.asset,
	};

	import_buffers(ctx);
	import_textures(ctx);
	import_materials(ctx);
	import_meshes(ctx);
	import_nodes(ctx);

	ProcessResponse response{};
	response.products.push(request.asset);
	for (const auto &mesh : ctx.mesh_ids) {
		response.products.push(mesh);
	}
	for (const auto &material : ctx.material_ids) {
		response.products.push(material);
	}
	return Ok(std::move(response));
}

#if 0
Result<Asset *> GLTFImporter::import(ImporterApi &api, exo::UUID resource_uuid, exo::Span<u8 const> data)
{
	const auto &header = *reinterpret_cast<const Header *>(data.data());
	if (header.first_chunk.type != ChunkType::Json) {
		return Err<Asset *>(GLTFError::FirstChunkNotJSON);
	}

	const u8 *first_chunk_data = reinterpret_cast<const u8 *>(exo::ptr_offset(&header.first_chunk, sizeof(Chunk)));

	exo::StringView    json_content{reinterpret_cast<const char *>(first_chunk_data), header.first_chunk.length};
	rapidjson::Document document;
	document.Parse(json_content.data(), json_content.size());
	if (document.HasParseError()) {
		return Err<Asset *>(AssetErrors::ParsingError);
	}

	ASSERT(sizeof(Header) + header.first_chunk.length < header.length);
	const auto *binary_chunk =
		reinterpret_cast<const Chunk *>(exo::ptr_offset(first_chunk_data, header.first_chunk.length));
	if (binary_chunk->type != ChunkType::Binary) {
		return Err<Asset *>(GLTFError::SecondChunkNotBIN);
	}
	const auto *binary_chunk_data = reinterpret_cast<const u8 *>(exo::ptr_offset(binary_chunk, sizeof(Chunk)));

	auto *new_scene = api.load_or_create_asset<SubScene>(resource_uuid);

	ImporterContext ctx = {
		.api           = api,
		.new_scene     = new_scene,
		.j_document    = document,
		.binary_chunk  = binary_chunk_data,
	};

	import_textures(ctx);
	import_materials(ctx);
	import_meshes(ctx);
	import_nodes(ctx);

	return Ok<Asset *>(new_scene);
}
#endif
