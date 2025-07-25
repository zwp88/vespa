// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#pragma once

#include <vespa/searchlib/features/distancetopathfeature.h>
#include <vespa/searchlib/features/termdistancefeature.h>
#include <vespa/searchlib/test/ft_test_app_base.h>

class Test : public FtTestAppBase
{
public:
    Test();
    ~Test();

    static void setupForDotProductTest(FtFeatureTest & ft);

protected:
    void testFieldMatchBluePrint();
    void testFieldMatchExecutor();
    void testFieldMatchExecutorOutOfOrder();
    void testFieldMatchExecutorSegments();
    void testFieldMatchExecutorGaps();
    void testFieldMatchExecutorHead();
    void testFieldMatchExecutorTail();
    void testFieldMatchExecutorLongestSequence();
    void testFieldMatchExecutorMatches();
    void testFieldMatchExecutorCompleteness();
    void testFieldMatchExecutorOrderness();
    void testFieldMatchExecutorRelatedness();
    void testFieldMatchExecutorLongestSequenceRatio();
    void testFieldMatchExecutorEarliness();
    void testFieldMatchExecutorWeight();
    void testFieldMatchExecutorSignificance();
    void testFieldMatchExecutorImportance();
    void testFieldMatchExecutorOccurrence();
    void testFieldMatchExecutorAbsoluteOccurrence();
    void testFieldMatchExecutorWeightedOccurrence();
    void testFieldMatchExecutorWeightedAbsoluteOccurrence();
    void testFieldMatchExecutorSignificantOccurrence();
    void testFieldMatchExecutorUnweightedProximity();
    void testFieldMatchExecutorReverseProximity();
    void testFieldMatchExecutorAbsoluteProximity();
    void testFieldMatchExecutorMultiSegmentProximity();
    void testFieldMatchExecutorSegmentDistance();
    void testFieldMatchExecutorSegmentProximity();
    void testFieldMatchExecutorSegmentStarts();
    void testFieldMatchExecutorMoreThanASegmentLengthOfUnmatchedQuery();
    void testFieldMatchExecutorQueryRepeats();
    void testFieldMatchExecutorZeroCases();
    void testFieldMatchExecutorExceedingIterationLimit();
    void testFieldMatchExecutorRemaining();

    void assertAge(feature_t expAge, const std::string & attr, uint64_t now, uint64_t docTime);
    static void setupForAgeTest(FtFeatureTest & ft, int64_t docTime);
    static void setupForAttributeTest(FtFeatureTest &ft, bool setup_env = true);
    void assertCloseness(feature_t exp, const std::string & attr, double distance, double maxDistance = 0, double halfResponse = 0);
    static void setupForDistanceTest(FtFeatureTest & ft, const std::string & attrName,
                                     const std::vector<std::pair<int32_t, int32_t> > & positions, bool zcurve);
    void assert2DZDistance(feature_t exp, const std::string & positions,
                           int32_t xquery, int32_t yquery, uint32_t xAspect = 0, uint32_t hit_index = 0);
    void assertDistanceToPath(const std::vector<std::pair<int32_t, int32_t> > & pos, const std::string &path,
                              feature_t distance = search::features::DistanceToPathExecutor::DEFAULT_DISTANCE,
                              feature_t traveled = 1, feature_t product = 0);
    void assertDotProduct(feature_t exp, const std::string & vector, uint32_t docId = 1,
                          const std::string & attribute = "wsstr", const std::string & attributeOverride="");

    void assertFieldMatch(const std::string & spec, const std::string & query, const std::string & field,
                          const search::features::fieldmatch::Params * params = nullptr, uint32_t totalTermWeight = 0, feature_t totalSignificance = 0.0f);
    void assertFieldMatch(const std::string & spec, const std::string & query, const std::string & field,
                          uint32_t totalTermWeight);
    void assertFieldMatchTS(const std::string & spec, const std::string & query, const std::string & field,
                            feature_t totalSignificance);
    std::string getExpression(const std::string &parameter) const;
    void assertForeachOperation(feature_t exp, const std::string & cond, const std::string & op);
    void assertFreshness(feature_t expFreshness, const std::string & attr, uint32_t age, uint32_t maxAge = 0, double halfResponse = 0, bool logScale = false);
    bool assertTermDistance(const search::features::TermDistanceCalculator::Result & exp, const std::string & query,
                            const std::string & field, uint32_t docId = 1);
    bool assertTermDistance(const search::features::TermDistanceCalculator::Result & exp, const std::string & query,
                            const std::vector<std::string> &field, uint32_t docId,
                            std::optional<search::fef::ElementGap> element_gap,
                            std::optional<uint32_t> phrase_length);
    bool assertMatches(uint32_t output, const std::string & query, const std::string & field,
                       const std::string & feature = "matches(foo)", uint32_t docId = 1);

    search::fef::BlueprintFactory _factory;
};

class ProdFeaturesTest : public ::testing::Test,
                         public Test
{
protected:
    ProdFeaturesTest();
    ~ProdFeaturesTest();
};
