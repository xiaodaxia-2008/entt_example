#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#include <spdlog/spdlog.h>

#include <spdlog/fmt/ostr.h>
#include <spdlog/fmt/ranges.h>

#include <boost/pfr.hpp>

#include <entt/entt.hpp>

#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>

#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

// formatter for entt::entity
template <>
struct fmt::formatter<entt::entity>
{
    constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const entt::entity &e, FormatContext &ctx) const
    {
        if (e == entt::null)
        {
            return format_to(ctx.out(), "<entity null>");
        }

        return format_to(ctx.out(), "<entity {}>", fmt::underlying(e));
    }
};

template <size_t N>
struct StringLiteral
{
    constexpr StringLiteral(const char (&str)[N]) { std::copy_n(str, N, value); }

    char value[N];
};

template <size_t N>
StringLiteral(const char (&)[N]) -> StringLiteral<N>;

// formatter for general structs
FMT_BEGIN_NAMESPACE
template <StringLiteral NAME = "">
struct basic_struct_formatter
{
    constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }

    template <typename T, typename FormatContext>
    auto format(const T &t, FormatContext &ctx)
    {
        auto out = ctx.out();
        out = format_to(out, "{}{{", NAME.value);
        boost::pfr::for_each_field(t, [&out](const auto &field)
                                   { out = fmt::format_to(out, "{}, ", field); });
        out = format_to(out, "}}");
        return out;
    }
};
FMT_END_NAMESPACE

struct BasicInfo
{
    std::string name;
};

struct Position
{
    float x;
    float y;
};

struct Velocity
{
    float x;
    float y;
};

struct Shape
{
    float width;
    float height;
};

struct RelationShip
{
    entt::entity parent;
    std::vector<entt::entity> children;
};

template <class Archive, class T>
    requires std::is_same_v<T, BasicInfo> || std::is_same_v<T, Position> || std::is_same_v<T, Velocity> ||
             std::is_same_v<T, Shape> || std::is_same_v<T, RelationShip>
void serialize(Archive &archive, T &val)
{
    boost::pfr::for_each_field(val, [&archive](auto &field)
                               { archive(field); });
}

// clang-format off
template <> struct fmt::formatter<Position> : fmt::basic_struct_formatter<"Position"> {};
template <> struct fmt::formatter<Velocity> : fmt::basic_struct_formatter<"Velocity"> {};
template <> struct fmt::formatter<Shape> : fmt::basic_struct_formatter<"Shape"> {};
template <> struct fmt::formatter<RelationShip> : fmt::basic_struct_formatter<"RelationShip"> {};
template <> struct fmt::formatter<BasicInfo> : fmt::basic_struct_formatter<"BasicInfo"> {};
// clang-format on

void Init(entt::registry &registry)
{
    std::vector<entt::entity> entities;
    for (auto i = 0; i < 10; ++i)
    {
        entities.push_back(registry.create());
    }

    for (auto entity : entities)
    {
        registry.destroy(entity);
    }

    entt::entity entity1 = registry.create();
    SPDLOG_DEBUG("entity1: {}", entity1);
    registry.emplace<BasicInfo>(entity1, fmt::format("{}", entity1));
    registry.emplace<Position>(entity1, 1.0f, 2.0f);
    registry.emplace<Velocity>(entity1, 3.0f, 4.0f);

    auto entity2 = registry.create();
    SPDLOG_DEBUG("entity2: {}", entity2);
    registry.emplace<BasicInfo>(entity2, fmt::format("{}", entity2));
    registry.emplace<Position>(entity2, 5.0f, 6.0f);
    registry.emplace<Shape>(entity2, 5.0f, 6.0f);

    registry.emplace<RelationShip>(entity1, entt::null,
                                   std::vector<entt::entity>{entity2});
    registry.emplace<RelationShip>(entity2, entity1, std::vector<entt::entity>{});
}

int main(int argc, char **argv)
{
    spdlog::set_level(spdlog::level::debug);

    entt::registry registry;

    Init(registry);

    {
        SPDLOG_DEBUG("registry");
        for (auto &&[entity, pos, info, rel] :
             registry.view<Position, BasicInfo, RelationShip>().each())
        {
            SPDLOG_DEBUG("entity: {}", entity);
            SPDLOG_DEBUG("{}", info);
            SPDLOG_DEBUG("{}", pos);
            SPDLOG_DEBUG("{}", rel);
        }
    }

    {
        std::ofstream ofs("output.json");
        cereal::JSONOutputArchive output(ofs);
        entt::snapshot{registry}
            .entities(output)
            .component<BasicInfo, Position, Velocity, Shape, RelationShip>(output);
    }

    entt::registry registry2;
    {
        std::ifstream ifs("output.json");
        cereal::JSONInputArchive input(ifs);
        entt::snapshot_loader{registry2}
            .entities(input)
            .component<BasicInfo, Position, Velocity, Shape, RelationShip>(input);
    }

    {
        SPDLOG_DEBUG("registry2");
        for (auto &&[entity, pos, info, rel] :
             registry2.view<Position, BasicInfo, RelationShip>().each())
        {
            SPDLOG_DEBUG("entity: {}", entity);
            SPDLOG_DEBUG("{}", info);
            SPDLOG_DEBUG("{}", pos);
            SPDLOG_DEBUG("{}", rel);
        }
    }

    entt::registry r3;
    {
        std::ifstream ifs("output.json");
        cereal::JSONInputArchive input(ifs);
        entt::continuous_loader loader{r3};
        loader.entities(input)
            .component<BasicInfo, Position, Velocity, Shape, RelationShip>(input, &RelationShip::parent, &RelationShip::children);
    }
    {
        SPDLOG_DEBUG("registry3");
        for (auto &&[entity, pos, info, rel] :
             r3.view<Position, BasicInfo, RelationShip>().each())
        {
            SPDLOG_DEBUG("entity: {}", entity);
            SPDLOG_DEBUG("{}", info);
            SPDLOG_DEBUG("{}", pos);
            SPDLOG_DEBUG("{}", rel);
        }
    }

    // entt::continuous_loader loader{registry};

    return 0;
}
