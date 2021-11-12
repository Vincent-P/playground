// automatically generated by the FlatBuffers compiler, do not modify


#ifndef FLATBUFFERS_GENERATED_EXO_ENGINE_SCHEMAS_EXO_H_
#define FLATBUFFERS_GENERATED_EXO_ENGINE_SCHEMAS_EXO_H_

#include "flatbuffers/flatbuffers.h"

namespace engine {
namespace schemas {
namespace exo {

struct float2;

struct float4;

struct float4x4;

struct UUID;

FLATBUFFERS_MANUALLY_ALIGNED_STRUCT(4) float2 FLATBUFFERS_FINAL_CLASS {
 private:
  float v_[2];

 public:
  float2()
      : v_() {
  }
  float2(flatbuffers::span<const float, 2> _v) {
    flatbuffers::CastToArray(v_).CopyFromSpan(_v);
  }
  const flatbuffers::Array<float, 2> *v() const {
    return &flatbuffers::CastToArray(v_);
  }
};
FLATBUFFERS_STRUCT_END(float2, 8);

FLATBUFFERS_MANUALLY_ALIGNED_STRUCT(4) float4 FLATBUFFERS_FINAL_CLASS {
 private:
  float v_[4];

 public:
  float4()
      : v_() {
  }
  float4(flatbuffers::span<const float, 4> _v) {
    flatbuffers::CastToArray(v_).CopyFromSpan(_v);
  }
  const flatbuffers::Array<float, 4> *v() const {
    return &flatbuffers::CastToArray(v_);
  }
};
FLATBUFFERS_STRUCT_END(float4, 16);

FLATBUFFERS_MANUALLY_ALIGNED_STRUCT(4) float4x4 FLATBUFFERS_FINAL_CLASS {
 private:
  float v_[16];

 public:
  float4x4()
      : v_() {
  }
  float4x4(flatbuffers::span<const float, 16> _v) {
    flatbuffers::CastToArray(v_).CopyFromSpan(_v);
  }
  const flatbuffers::Array<float, 16> *v() const {
    return &flatbuffers::CastToArray(v_);
  }
};
FLATBUFFERS_STRUCT_END(float4x4, 64);

FLATBUFFERS_MANUALLY_ALIGNED_STRUCT(4) UUID FLATBUFFERS_FINAL_CLASS {
 private:
  uint32_t v_[4];

 public:
  UUID()
      : v_() {
  }
  UUID(flatbuffers::span<const uint32_t, 4> _v) {
    flatbuffers::CastToArray(v_).CopyFromSpan(_v);
  }
  const flatbuffers::Array<uint32_t, 4> *v() const {
    return &flatbuffers::CastToArray(v_);
  }
};
FLATBUFFERS_STRUCT_END(UUID, 16);

}  // namespace exo
}  // namespace schemas
}  // namespace engine

#endif  // FLATBUFFERS_GENERATED_EXO_ENGINE_SCHEMAS_EXO_H_