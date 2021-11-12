// automatically generated by the FlatBuffers compiler, do not modify


#ifndef FLATBUFFERS_GENERATED_SUBSCENE_ENGINE_SCHEMAS_H_
#define FLATBUFFERS_GENERATED_SUBSCENE_ENGINE_SCHEMAS_H_

#include "flatbuffers/flatbuffers.h"

#include "schemas/asset_generated.h"
#include "schemas/exo_generated.h"

namespace engine {
namespace schemas {

struct EntityChildren;
struct EntityChildrenBuilder;

struct SubScene;
struct SubSceneBuilder;

struct EntityChildren FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  typedef EntityChildrenBuilder Builder;
  enum FlatBuffersVTableOffset FLATBUFFERS_VTABLE_UNDERLYING_TYPE {
    VT_CHILDREN = 4
  };
  const flatbuffers::Vector<uint32_t> *children() const {
    return GetPointer<const flatbuffers::Vector<uint32_t> *>(VT_CHILDREN);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffset(verifier, VT_CHILDREN) &&
           verifier.VerifyVector(children()) &&
           verifier.EndTable();
  }
};

struct EntityChildrenBuilder {
  typedef EntityChildren Table;
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_children(flatbuffers::Offset<flatbuffers::Vector<uint32_t>> children) {
    fbb_.AddOffset(EntityChildren::VT_CHILDREN, children);
  }
  explicit EntityChildrenBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  flatbuffers::Offset<EntityChildren> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<EntityChildren>(end);
    return o;
  }
};

inline flatbuffers::Offset<EntityChildren> CreateEntityChildren(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<flatbuffers::Vector<uint32_t>> children = 0) {
  EntityChildrenBuilder builder_(_fbb);
  builder_.add_children(children);
  return builder_.Finish();
}

inline flatbuffers::Offset<EntityChildren> CreateEntityChildrenDirect(
    flatbuffers::FlatBufferBuilder &_fbb,
    const std::vector<uint32_t> *children = nullptr) {
  auto children__ = children ? _fbb.CreateVector<uint32_t>(*children) : 0;
  return engine::schemas::CreateEntityChildren(
      _fbb,
      children__);
}

struct SubScene FLATBUFFERS_FINAL_CLASS : private flatbuffers::Table {
  typedef SubSceneBuilder Builder;
  enum FlatBuffersVTableOffset FLATBUFFERS_VTABLE_UNDERLYING_TYPE {
    VT_TRANSFORMS = 4,
    VT_MESHES = 6,
    VT_CHILDREN = 8,
    VT_ROOTS = 10
  };
  const flatbuffers::Vector<const engine::schemas::exo::float4x4 *> *transforms() const {
    return GetPointer<const flatbuffers::Vector<const engine::schemas::exo::float4x4 *> *>(VT_TRANSFORMS);
  }
  const flatbuffers::Vector<const engine::schemas::exo::UUID *> *meshes() const {
    return GetPointer<const flatbuffers::Vector<const engine::schemas::exo::UUID *> *>(VT_MESHES);
  }
  const flatbuffers::Vector<flatbuffers::Offset<engine::schemas::EntityChildren>> *children() const {
    return GetPointer<const flatbuffers::Vector<flatbuffers::Offset<engine::schemas::EntityChildren>> *>(VT_CHILDREN);
  }
  const flatbuffers::Vector<uint32_t> *roots() const {
    return GetPointer<const flatbuffers::Vector<uint32_t> *>(VT_ROOTS);
  }
  bool Verify(flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyOffset(verifier, VT_TRANSFORMS) &&
           verifier.VerifyVector(transforms()) &&
           VerifyOffset(verifier, VT_MESHES) &&
           verifier.VerifyVector(meshes()) &&
           VerifyOffset(verifier, VT_CHILDREN) &&
           verifier.VerifyVector(children()) &&
           verifier.VerifyVectorOfTables(children()) &&
           VerifyOffset(verifier, VT_ROOTS) &&
           verifier.VerifyVector(roots()) &&
           verifier.EndTable();
  }
};

struct SubSceneBuilder {
  typedef SubScene Table;
  flatbuffers::FlatBufferBuilder &fbb_;
  flatbuffers::uoffset_t start_;
  void add_transforms(flatbuffers::Offset<flatbuffers::Vector<const engine::schemas::exo::float4x4 *>> transforms) {
    fbb_.AddOffset(SubScene::VT_TRANSFORMS, transforms);
  }
  void add_meshes(flatbuffers::Offset<flatbuffers::Vector<const engine::schemas::exo::UUID *>> meshes) {
    fbb_.AddOffset(SubScene::VT_MESHES, meshes);
  }
  void add_children(flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<engine::schemas::EntityChildren>>> children) {
    fbb_.AddOffset(SubScene::VT_CHILDREN, children);
  }
  void add_roots(flatbuffers::Offset<flatbuffers::Vector<uint32_t>> roots) {
    fbb_.AddOffset(SubScene::VT_ROOTS, roots);
  }
  explicit SubSceneBuilder(flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  flatbuffers::Offset<SubScene> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = flatbuffers::Offset<SubScene>(end);
    return o;
  }
};

inline flatbuffers::Offset<SubScene> CreateSubScene(
    flatbuffers::FlatBufferBuilder &_fbb,
    flatbuffers::Offset<flatbuffers::Vector<const engine::schemas::exo::float4x4 *>> transforms = 0,
    flatbuffers::Offset<flatbuffers::Vector<const engine::schemas::exo::UUID *>> meshes = 0,
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<engine::schemas::EntityChildren>>> children = 0,
    flatbuffers::Offset<flatbuffers::Vector<uint32_t>> roots = 0) {
  SubSceneBuilder builder_(_fbb);
  builder_.add_roots(roots);
  builder_.add_children(children);
  builder_.add_meshes(meshes);
  builder_.add_transforms(transforms);
  return builder_.Finish();
}

inline flatbuffers::Offset<SubScene> CreateSubSceneDirect(
    flatbuffers::FlatBufferBuilder &_fbb,
    const std::vector<engine::schemas::exo::float4x4> *transforms = nullptr,
    const std::vector<engine::schemas::exo::UUID> *meshes = nullptr,
    const std::vector<flatbuffers::Offset<engine::schemas::EntityChildren>> *children = nullptr,
    const std::vector<uint32_t> *roots = nullptr) {
  auto transforms__ = transforms ? _fbb.CreateVectorOfStructs<engine::schemas::exo::float4x4>(*transforms) : 0;
  auto meshes__ = meshes ? _fbb.CreateVectorOfStructs<engine::schemas::exo::UUID>(*meshes) : 0;
  auto children__ = children ? _fbb.CreateVector<flatbuffers::Offset<engine::schemas::EntityChildren>>(*children) : 0;
  auto roots__ = roots ? _fbb.CreateVector<uint32_t>(*roots) : 0;
  return engine::schemas::CreateSubScene(
      _fbb,
      transforms__,
      meshes__,
      children__,
      roots__);
}

inline const engine::schemas::SubScene *GetSubScene(const void *buf) {
  return flatbuffers::GetRoot<engine::schemas::SubScene>(buf);
}

inline const engine::schemas::SubScene *GetSizePrefixedSubScene(const void *buf) {
  return flatbuffers::GetSizePrefixedRoot<engine::schemas::SubScene>(buf);
}

inline const char *SubSceneIdentifier() {
  return "SBSC";
}

inline bool SubSceneBufferHasIdentifier(const void *buf) {
  return flatbuffers::BufferHasIdentifier(
      buf, SubSceneIdentifier());
}

inline bool VerifySubSceneBuffer(
    flatbuffers::Verifier &verifier) {
  return verifier.VerifyBuffer<engine::schemas::SubScene>(SubSceneIdentifier());
}

inline bool VerifySizePrefixedSubSceneBuffer(
    flatbuffers::Verifier &verifier) {
  return verifier.VerifySizePrefixedBuffer<engine::schemas::SubScene>(SubSceneIdentifier());
}

inline void FinishSubSceneBuffer(
    flatbuffers::FlatBufferBuilder &fbb,
    flatbuffers::Offset<engine::schemas::SubScene> root) {
  fbb.Finish(root, SubSceneIdentifier());
}

inline void FinishSizePrefixedSubSceneBuffer(
    flatbuffers::FlatBufferBuilder &fbb,
    flatbuffers::Offset<engine::schemas::SubScene> root) {
  fbb.FinishSizePrefixed(root, SubSceneIdentifier());
}

}  // namespace schemas
}  // namespace engine

#endif  // FLATBUFFERS_GENERATED_SUBSCENE_ENGINE_SCHEMAS_H_