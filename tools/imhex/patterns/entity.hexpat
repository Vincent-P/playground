u64 error out;

fn class_hash(str name) {
	u64 hash = 5381;
	for (u64 i = 0, i < builtin::std::string::length(name), i = i + 1) {
		char c = builtin::std::string::at(name, i);
		hash = ((hash << 5) + hash) + u64(c); /* hash * 33 + c */
	}
	return hash;
};


struct AssetId
{
	u64 name_len;
	char name[name_len];

	u64 name_hash;
};

struct BaseComponent
{
	u128 uuid;

	u64 name_len;
	char name[name_len];
};

using Component;
struct SpatialComponent : BaseComponent
{
	float local_transform[16];
	float local_bounds[6];

	u64 children_len;
	Component children[children_len];
};

struct MeshComponent : SpatialComponent
{
	AssetId mesh_asset;
};

struct CameraComponent : SpatialComponent {};
struct EditorCameraComponent : BaseComponent {};
struct CameraInputComponent : BaseComponent {};

struct Component
{
	u64 class_id;
	if (class_id == class_hash("SpatialComponent")) {
		SpatialComponent spatial;
	}
	else if (class_id == class_hash("MeshComponent")) {
		MeshComponent mesh;
	}
	else if (class_id == class_hash("CameraComponent")) {
		CameraComponent camera;
	}
	else if (class_id == class_hash("EditorCameraComponent")) {
		EditorCameraComponent editor_camera;
	}
	else if (class_id == class_hash("CameraInputComponent")) {
		CameraInputComponent camera_input;
	}
	else {
		u32 unknown;
		error = error + 1;
	}
};

struct Entity
{
	u128 uuid;

	u64 name_len;
	char name[name_len];

	s32 state;

	u64 components_len;
	Component components[components_len];

	u128 root_component_uuid;

	u64 attached_entities_len;
	u128 attached_entities[attached_entities_len];

	u128 parent_entity;

	bool is_attached_to_parent;
};

struct EntityDebug
{
	u128 uuid;

	u64 name_len;
	char name[name_len];

	s32 state;

	u64 components_len;
	Component components[components_len];

	u128 root_component_uuid;

	u64 attached_entities_len;
	u128 attached_entities[attached_entities_len];
	u128 parent_entity;

	bool is_attached_to_parent;
};

struct EntityWorld
{
	u64 entities_len;
	Entity entities[entities_len];
};

EntityWorld world @ 0x00;