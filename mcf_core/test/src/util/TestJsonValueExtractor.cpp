/**
 * Copyright (c) 2024 Accenture
 */
#include "gtest/gtest.h"
#include "mcf_core/util/JsonValueExtractor.h"

#include <sstream>

TEST(json_value_extractor, works)
{
    Json::Value config;
    auto reader = Json::CharReaderBuilder();

    std::istringstream stream(
        "{\"b\": true, \"i\": 1, \"f\": 2.0, \"s\": \"string\", \"iv\": [1, 2], \"fv\": [0.0, "
        "0.0], \"sv\": [\"a\", \"b\"], "
        "\"ivv\": [[1, 2], [3, 4]], \"fvv\": [[1.0, 0.0], [0.0, 1.0]]}",
        std::ios::in);
    std::string errors;
    Json::parseFromStream(reader, stream, &config, &errors);
    EXPECT_EQ(errors, "");

    mcf::util::json::JsonValueExtractor extractor("Extractor");
    EXPECT_EQ(extractor.extractConfigBool(config["b"], "b"), true);
    EXPECT_EQ(extractor.extractConfigInt(config["i"], "i"), 1);
    EXPECT_EQ(extractor.extractConfigString(config["s"], "s"), "string");
    EXPECT_EQ(extractor.extractConfigFloat(config["f"], "f"), 2.0);
    EXPECT_EQ(extractor.extractConfigIntVector(config["iv"], "iv"), std::vector<int>({1, 2}));
    EXPECT_EQ(
        extractor.extractConfigFloatVector(config["fv"], "fv"), std::vector<float>({0.0, 0.0}));
    EXPECT_EQ(
        extractor.extractConfigIntVector2d(config["ivv"], "ivv"),
        std::vector<std::vector<int> >({{1, 2}, {3, 4}}));
    EXPECT_EQ(
        extractor.extractConfigFloatVector2d(config["fvv"], "fvv"),
        std::vector<std::vector<float> >({{1.0, 0.0}, {0.0, 1.0}}));
    EXPECT_EQ(
        extractor.extractConfigStringVector(config["sv"], "sv"),
        std::vector<std::string>({"a", "b"}));
    EXPECT_EQ(
        extractor.extractConfigStringSet(config["sv"], "sv"), std::set<std::string>({"a", "b"}));

    // test template-based extractors
    EXPECT_EQ(extractor.extractConfigMember<bool>(config, "b"), true);
    EXPECT_EQ(extractor.extractConfigMember<int>(config, "i"), 1);
    EXPECT_EQ(extractor.extractConfigMember<std::string>(config, "s"), "string");
    EXPECT_EQ(extractor.extractConfigMember<float>(config, "f"), 2.0);
    EXPECT_EQ(
        extractor.extractConfigMember<std::vector<int> >(config, "iv"), std::vector<int>({1, 2}));
    EXPECT_EQ(
        extractor.extractConfigMember<std::vector<float> >(config, "fv"),
        std::vector<float>({0.0, 0.0}));
    EXPECT_EQ(
        extractor.extractConfigMember<std::vector<std::vector<int> > >(config, "ivv"),
        std::vector<std::vector<int> >({{1, 2}, {3, 4}}));
    EXPECT_EQ(
        extractor.extractConfigMember<std::vector<std::vector<float> > >(config, "fvv"),
        std::vector<std::vector<float> >({{1.0, 0.0}, {0.0, 1.0}}));
    EXPECT_EQ(
        extractor.extractConfigMember<std::vector<std::string> >(config, "sv"),
        std::vector<std::string>({"a", "b"}));
    EXPECT_EQ(
        extractor.extractConfigMember<std::set<std::string> >(config, "sv"),
        std::set<std::string>({"a", "b"}));

    // test throwing behaviour
    EXPECT_THROW(extractor.extractConfigBool(config["b_absent"], "error"), std::runtime_error);
    EXPECT_THROW(extractor.extractConfigInt(config["i_absent"], "error"), std::runtime_error);
    EXPECT_THROW(extractor.extractConfigString(config["s_absent"], "error"), std::runtime_error);
    EXPECT_THROW(extractor.extractConfigFloat(config["f_absent"], "error"), std::runtime_error);
    EXPECT_THROW(extractor.extractConfigIntVector(config["iv_absent"], "error"), std::runtime_error);
    EXPECT_THROW(extractor.extractConfigFloatVector(config["fv_absent"], "error"), std::runtime_error);
    EXPECT_THROW(
        extractor.extractConfigIntVector2d(config["ivv_absent"], "error"), std::runtime_error);
    EXPECT_THROW(
        extractor.extractConfigFloatVector2d(config["fvv_absent"], "error"), std::runtime_error);
    EXPECT_THROW(
        extractor.extractConfigStringVector(config["sv_absent"], "error"), std::runtime_error);
    EXPECT_THROW(extractor.extractConfigStringSet(config["sv_absent"], "error"), std::runtime_error);
    // throw on wrong type
    EXPECT_THROW(extractor.extractConfigBool(config["sv"], "error"), std::runtime_error);
    EXPECT_THROW(extractor.extractConfigInt(config["s"], "error"), std::runtime_error);
    EXPECT_THROW(extractor.extractConfigString(config["b"], "error"), std::runtime_error);
    EXPECT_THROW(extractor.extractConfigFloat(config["s"], "error"), std::runtime_error);
    EXPECT_THROW(extractor.extractConfigIntVector(config["sv"], "error"), std::runtime_error);
    EXPECT_THROW(extractor.extractConfigFloatVector(config["b"], "error"), std::runtime_error);
    EXPECT_THROW(extractor.extractConfigIntVector2d(config["iv"], "error"), std::runtime_error);
    EXPECT_THROW(extractor.extractConfigFloatVector2d(config["fv"], "error"), std::runtime_error);
    EXPECT_THROW(extractor.extractConfigStringVector(config["fvv"], "error"), std::runtime_error);
    EXPECT_THROW(extractor.extractConfigStringSet(config["ivv"], "error"), std::runtime_error);

    // test defaults
    EXPECT_EQ(extractor.extractConfigMember<bool>(config, "b_absent", true), true);
}
