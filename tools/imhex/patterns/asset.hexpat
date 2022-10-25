fn round_up_to_alignment(u64 len, u64 align) {
	u64 mask = align - 1;
	return (len + mask) & ~mask;

};

struct AssetId
{
	u64 name_len;
	char name[round_up_to_alignment(name_len, 4)];
	u64 name_hash;
};

struct Asset
{
	AssetId id;
	u64 name_len;
	char name[round_up_to_alignment(name_len, 4)];
	u64 deps_len;
	AssetId deps[deps_len];
};


