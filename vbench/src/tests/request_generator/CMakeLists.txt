# Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
vespa_add_executable(vbench_request_generator_test_app TEST
    SOURCES
    request_generator_test.cpp
    DEPENDS
    vbench_test
    vespa_vbench
    GTest::gtest
)
vespa_add_test(NAME vbench_request_generator_test_app COMMAND vbench_request_generator_test_app)
