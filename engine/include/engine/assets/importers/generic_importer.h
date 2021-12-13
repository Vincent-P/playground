#pragma once

#include <exo/maths/numerics.h>
#include <exo/result.h>
#include <type_traits>
#include <memory>
#include <exo/os/uuid.h>

#include <rapidjson/fwd.h>

struct Asset;
struct AssetManager;

template <typename T>
concept ImporterConcept = requires(T importer)
{
    // clang-format off
    { importer.can_import((const void*)nullptr, (usize)0) } -> std::same_as<bool>;
    { importer.import((AssetManager*)nullptr, os::UUID{}, (const void*)nullptr, (usize)0, (void*)nullptr)} -> std::same_as<Result<Asset*>>;
    { importer.create_default_importer_data() } -> std::same_as<void*>;

    {importer.read_data_json(*(const rapidjson::Value*)nullptr) } -> std::same_as<void*>;
    {importer.write_data_json(*((rapidjson::GenericPrettyWriter<rapidjson::FileWriteStream>*)nullptr), (const void *)nullptr)} -> std::same_as<void>;
    // clang-format on
};

// Type erase any types that implement the ImporterConcept.
struct GenericImporter
{
    GenericImporter() = default;

    template<typename ImporterType>
    GenericImporter(ImporterType && importer)
    : value_accessor{ std::make_unique<wrapper<ImporterType>>(std::forward<ImporterType>(importer)) }
    {}

    bool can_import(const void* file_data, usize file_size) { return value_accessor->can_import(file_data, file_size); }
    Result<Asset*> import(AssetManager* m, os::UUID r, const void* d, usize s, void* i) { return value_accessor->import(m, r, d, s, i); }
    void *create_default_importer_data() { return value_accessor->create_default_importer_data(); }
    void *read_data_json(const rapidjson::Value &j_data) { return value_accessor->read_data_json(j_data); }
    void write_data_json(rapidjson::GenericPrettyWriter<rapidjson::FileWriteStream> &writer, const void *data) { value_accessor->write_data_json(writer, data); }

private:
    struct model
    {
        virtual ~model() = default;
        virtual bool can_import(const void*, usize) = 0;
        virtual Result<Asset*> import(AssetManager*, os::UUID, const void*, usize, void*) = 0;
        virtual void *create_default_importer_data() = 0;
        virtual void *read_data_json(const rapidjson::Value &j_data) = 0;
        virtual void write_data_json(rapidjson::GenericPrettyWriter<rapidjson::FileWriteStream> &writer, const void *data) = 0;
    };

    template <ImporterConcept T>
    struct wrapper final : model
    {
        using Settings = typename T::Settings;
        wrapper(T && arg)
        : value{std::forward<decltype(arg)>(arg)}
        {}
        ~wrapper() override {}
        bool can_import(const void* file_data, usize file_size) final { return value.can_import(file_data, file_size); }
        Result<Asset*> import(AssetManager* m, os::UUID r, const void* d, usize s, void* i) final { return value.import(m, r, d, s, i); }
        void *create_default_importer_data() final { return value.create_default_importer_data(); }
        void *read_data_json(const rapidjson::Value &j_data) final { return value.read_data_json(j_data); }
        void write_data_json(rapidjson::GenericPrettyWriter<rapidjson::FileWriteStream> &writer, const void *data) final { value.write_data_json(writer, data); }
    private:
        // decay removes references and const, allows to get the "expected" type T
        std::decay_t<T> value;
    };

    std::unique_ptr<model> value_accessor;
};

static_assert(ImporterConcept<GenericImporter>);
