// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include <vespa/vespalib/component/vtag.h>
#include <vespa/vespalib/net/connection_auth_context.h>
#include <vespa/vespalib/net/http/state_server.h>
#include <vespa/vespalib/net/http/simple_health_producer.h>
#include <vespa/vespalib/net/http/simple_metrics_producer.h>
#include <vespa/vespalib/net/http/simple_component_config_producer.h>
#include <vespa/vespalib/net/http/state_explorer.h>
#include <vespa/vespalib/net/http/slime_explorer.h>
#include <vespa/vespalib/net/http/generic_state_handler.h>
#include <vespa/vespalib/util/stringfmt.h>
#include <vespa/vespalib/util/host_name.h>
#include <vespa/vespalib/process/process.h>
#include <sys/stat.h>
#include <sstream>
#include <vespa/vespalib/gtest/gtest.h>

using namespace vespalib;

//-----------------------------------------------------------------------------

std::string root_path = "/state/v1/";
std::string short_root_path = "/state/v1";
std::string metrics_path = "/state/v1/metrics";
std::string health_path = "/state/v1/health";
std::string config_path = "/state/v1/config";
std::string version_path = "/state/v1/version";

std::string total_metrics_path = "/metrics/total";

std::string unknown_path = "/this/path/is/not/known";
std::string unknown_state_path = "/state/v1/this/path/is/not/known";
std::string my_path = "/my/path";

std::string host_tag = "HOST";
std::map<std::string,std::string> empty_params;

//-----------------------------------------------------------------------------

std::string run_cmd(const std::string &cmd) {
    std::string out;
    EXPECT_TRUE(Process::run(cmd.c_str(), out));
    return out;
}

std::string getPage(int port, const std::string &path, const std::string &extra_params = "") {
    return run_cmd(make_string("curl -s %s 'http://localhost:%d%s'", extra_params.c_str(), port, path.c_str()));
}

std::string getFull(int port, const std::string &path) { return getPage(port, path, "-D -"); }

std::pair<std::string, std::string>
get_body_and_content_type(const JsonGetHandler &handler,
                          const std::string &host,
                          const std::string &path,
                          const std::map<std::string,std::string> &params)
{
    net::ConnectionAuthContext dummy_ctx(net::tls::PeerCredentials(), net::tls::CapabilitySet::all());
    auto res = handler.get(host, path, params, dummy_ctx);
    if (res.ok()) {
        return {std::string(res.payload()), std::string(res.content_type())};
    }
    return {};
}

std::string get_json(const JsonGetHandler &handler,
                          const std::string &host,
                          const std::string &path,
                          const std::map<std::string,std::string> &params)
{
    return get_body_and_content_type(handler, host, path, params).first;
}

//-----------------------------------------------------------------------------

struct DummyHandler : JsonGetHandler {
    std::string result;
    DummyHandler(const std::string &result_in) : result(result_in) {}
    Response get(const std::string &, const std::string &,
                 const std::map<std::string,std::string> &,
                 const net::ConnectionAuthContext &) const override
    {
        if (!result.empty()) {
            return Response::make_ok_with_json(result);
        } else {
            return Response::make_not_found();
        }
    }
};

//-----------------------------------------------------------------------------

TEST(StateServerTest, require_that_unknown_url_returns_404_response) {
    HttpServer f1(0);
    std::string expect("HTTP/1.1 404 Not Found\r\n"
                       "Connection: close\r\n"
                       "\r\n");
    std::string actual = getFull(f1.port(), unknown_path);
    EXPECT_EQ(expect, actual);
}

TEST(StateServerTest, require_that_handler_can_return_a_404_response) {
    DummyHandler f1("");
    HttpServer f2(0);
    auto token = f2.repo().bind(my_path, f1);
    std::string expect("HTTP/1.1 404 Not Found\r\n"
                       "Connection: close\r\n"
                       "\r\n");
    std::string actual = getFull(f2.port(), my_path);
    EXPECT_EQ(expect, actual);
}

TEST(StateServerTest, require_that_non_empty_known_url_returns_expected_headers) {
    DummyHandler f1("[123]");
    HttpServer f2(0);
    auto token = f2.repo().bind(my_path, f1);
    std::string expect("HTTP/1.1 200 OK\r\n"
                            "Connection: close\r\n"
                            "Content-Type: application/json\r\n"
                            "Content-Length: 5\r\n"
                            "X-XSS-Protection: 1; mode=block\r\n"
                            "X-Frame-Options: DENY\r\n"
                            "Content-Security-Policy: default-src 'none'; frame-ancestors 'none'\r\n"
                            "X-Content-Type-Options: nosniff\r\n"
                            "Cache-Control: no-store\r\n"
                            "Pragma: no-cache\r\n"
                            "\r\n"
                            "[123]");
    std::string actual = getFull(f2.port(), my_path);
    EXPECT_EQ(expect, actual);
}

TEST(StateServerTest, require_that_handler_is_selected_based_on_longest_matching_url_prefix) {
    DummyHandler f1("[1]");
    DummyHandler f2("[2]");
    DummyHandler f3("[3]");
    HttpServer f4(0);
    auto token2 = f4.repo().bind("/foo/bar", f2);
    auto token1 = f4.repo().bind("/foo", f1);
    auto token3 = f4.repo().bind("/foo/bar/baz", f3);
    int port = f4.port();
    EXPECT_EQ("", getPage(port, "/fox"));
    EXPECT_EQ("[1]", getPage(port, "/foo"));
    EXPECT_EQ("[1]", getPage(port, "/foo/fox"));
    EXPECT_EQ("[2]", getPage(port, "/foo/bar"));
    EXPECT_EQ("[2]", getPage(port, "/foo/bar/fox"));
    EXPECT_EQ("[3]", getPage(port, "/foo/bar/baz"));
    EXPECT_EQ("[3]", getPage(port, "/foo/bar/baz/fox"));
}

struct EchoHost : JsonGetHandler {
    ~EchoHost() override;
    Response get(const std::string &host, const std::string &,
                 const std::map<std::string,std::string> &,
                 const net::ConnectionAuthContext &) const override
    {
        return Response::make_ok_with_json("[\"" + host + "\"]");
    }
};

EchoHost::~EchoHost() = default;

TEST(StateServerTest, require_that_host_is_passed_correctly) {
    EchoHost f1;
    HttpServer f2(0);
    auto token = f2.repo().bind(my_path, f1);
    EXPECT_EQ(make_string("%s:%d", HostName::get().c_str(), f2.port()), f2.host());
    std::string default_result = make_string("[\"%s\"]", f2.host().c_str());
    std::string localhost_result = make_string("[\"%s:%d\"]", "localhost", f2.port());
    std::string silly_result = "[\"sillyserver\"]";
    EXPECT_EQ(localhost_result, run_cmd(make_string("curl -s http://localhost:%d/my/path", f2.port())));
    EXPECT_EQ(silly_result, run_cmd(make_string("curl -s http://localhost:%d/my/path -H \"Host: sillyserver\"", f2.port())));
    EXPECT_EQ(default_result, run_cmd(make_string("curl -s http://localhost:%d/my/path -H \"Host:\"", f2.port())));
}

struct SamplingHandler : JsonGetHandler {
    mutable std::mutex my_lock;
    mutable std::string my_host;
    mutable std::string my_path;
    mutable std::map<std::string,std::string> my_params;
    ~SamplingHandler() override;
    Response get(const std::string &host, const std::string &path,
                 const std::map<std::string,std::string> &params,
                 const net::ConnectionAuthContext &) const override
    {
        {
            auto guard = std::lock_guard(my_lock);
            my_host = host;
            my_path = path;
            my_params = params;
        }
        return Response::make_ok_with_json("[]");
    }
};

SamplingHandler::~SamplingHandler() = default;

TEST(StateServerTest, require_that_request_parameters_can_be_inspected)
{
    SamplingHandler f1;
    HttpServer f2(0);
    auto token = f2.repo().bind("/foo", f1);
    EXPECT_EQ("[]", getPage(f2.port(), "/foo?a=b&x=y&z"));
    {
        auto guard = std::lock_guard(f1.my_lock);
        EXPECT_EQ(f1.my_path, "/foo");
        EXPECT_EQ(f1.my_params.size(), 3u);
        EXPECT_EQ(f1.my_params["a"], "b");
        EXPECT_EQ(f1.my_params["x"], "y");
        EXPECT_EQ(f1.my_params["z"], "");
        EXPECT_EQ(f1.my_params.size(), 3u); // "z" was present
    }
}

TEST(StateServerTest, require_that_request_path_is_dequoted)
{
    SamplingHandler f1;
    HttpServer f2(0);
    auto token = f2.repo().bind("/[foo]", f1);
    EXPECT_EQ("[]", getPage(f2.port(), "/%5bfoo%5D"));
    {
        auto guard = std::lock_guard(f1.my_lock);
        EXPECT_EQ(f1.my_path, "/[foo]");
        EXPECT_EQ(f1.my_params.size(), 0u);
    }
}

//-----------------------------------------------------------------------------

TEST(StateServerTest, require_that_the_state_server_wires_the_appropriate_url_prefixes)
{
    SimpleHealthProducer f1;
    SimpleMetricsProducer f2;
    SimpleComponentConfigProducer f3;
    StateServer f4(0, f1, f2, f3);
    f2.setTotalMetrics("{}", MetricsProducer::ExpositionFormat::JSON); // avoid empty result
    int port = f4.getListenPort();
    EXPECT_TRUE(getFull(port, short_root_path).find("HTTP/1.1 200 OK") == 0);
    EXPECT_TRUE(getFull(port, total_metrics_path).find("HTTP/1.1 200 OK") == 0);
    EXPECT_TRUE(getFull(port, unknown_path).find("HTTP/1.1 404 Not Found") == 0);
}

TEST(StateServerTest, require_that_the_state_server_exposes_the_state_api_handler_repo)
{
    SimpleHealthProducer f1;
    SimpleMetricsProducer f2;
    SimpleComponentConfigProducer f3;
    StateServer f4(0, f1, f2, f3);
    int port = f4.getListenPort();
    std::string page1 = getPage(port, root_path);
    auto token = f4.repo().add_root_resource("state/v1/custom");
    std::string page2 = getPage(port, root_path);
    EXPECT_NE(page1, page2);
    token.reset();
    std::string page3 = getPage(port, root_path);
    EXPECT_EQ(page3, page1);
}

//-----------------------------------------------------------------------------

TEST(StateServerTest, require_that_json_handlers_can_be_removed_from_repo)
{
    DummyHandler f1("[1]");
    DummyHandler f2("[2]");
    DummyHandler f3("[3]");
    JsonHandlerRepo f4;
    auto token1 = f4.bind("/foo", f1);
    auto token2 = f4.bind("/foo/bar", f2);
    auto token3 = f4.bind("/foo/bar/baz", f3);
    std::map<std::string,std::string> params;
    EXPECT_EQ("[1]", get_json(f4, "", "/foo", params));
    EXPECT_EQ("[2]", get_json(f4, "", "/foo/bar", params));
    EXPECT_EQ("[3]", get_json(f4, "", "/foo/bar/baz", params));
    token2.reset();
    EXPECT_EQ("[1]", get_json(f4, "", "/foo", params));
    EXPECT_EQ("[1]", get_json(f4, "", "/foo/bar", params));
    EXPECT_EQ("[3]", get_json(f4, "", "/foo/bar/baz", params));
}

TEST(StateServerTest, require_that_json_handlers_can_be_shadowed)
{
    DummyHandler f1("[1]");
    DummyHandler f2("[2]");
    DummyHandler f3("[3]");
    JsonHandlerRepo f4;
    auto token1 = f4.bind("/foo", f1);
    auto token2 = f4.bind("/foo/bar", f2);
    std::map<std::string,std::string> params;
    EXPECT_EQ("[1]", get_json(f4, "", "/foo", params));
    EXPECT_EQ("[2]", get_json(f4, "", "/foo/bar", params));
    auto token3 = f4.bind("/foo/bar", f3);
    EXPECT_EQ("[3]", get_json(f4, "", "/foo/bar", params));
    token3.reset();
    EXPECT_EQ("[2]", get_json(f4, "", "/foo/bar", params));
}

TEST(StateServerTest, require_that_root_resources_can_be_tracked)
{
    JsonHandlerRepo f1;
    EXPECT_TRUE(std::vector<std::string>({}) == f1.get_root_resources());
    auto token1 = f1.add_root_resource("/health");
    EXPECT_TRUE(std::vector<std::string>({"/health"}) == f1.get_root_resources());
    auto token2 = f1.add_root_resource("/config");
    EXPECT_TRUE(std::vector<std::string>({"/health", "/config"}) == f1.get_root_resources());
    auto token3 = f1.add_root_resource("/custom/foo");
    EXPECT_TRUE(std::vector<std::string>({"/health", "/config", "/custom/foo"}) == f1.get_root_resources());    
    token2.reset();
    EXPECT_TRUE(std::vector<std::string>({"/health", "/custom/foo"}) == f1.get_root_resources());
}

//-----------------------------------------------------------------------------

TEST(StateServerTest, require_that_state_api_responds_to_the_expected_paths)
{
    SimpleHealthProducer f1;
    SimpleMetricsProducer f2;
    SimpleComponentConfigProducer f3;
    StateApi f4(f1, f2, f3);
    f2.setTotalMetrics("{}", MetricsProducer::ExpositionFormat::JSON); // avoid empty result
    EXPECT_TRUE(!get_json(f4, host_tag, short_root_path, empty_params).empty());
    EXPECT_TRUE(!get_json(f4, host_tag, root_path, empty_params).empty());
    EXPECT_TRUE(!get_json(f4, host_tag, health_path, empty_params).empty());
    EXPECT_TRUE(!get_json(f4, host_tag, metrics_path, empty_params).empty());
    EXPECT_TRUE(!get_json(f4, host_tag, config_path, empty_params).empty());
    EXPECT_TRUE(!get_json(f4, host_tag, version_path, empty_params).empty());
    EXPECT_TRUE(!get_json(f4, host_tag, total_metrics_path, empty_params).empty());
    EXPECT_TRUE(get_json(f4, host_tag, unknown_path, empty_params).empty());
    EXPECT_TRUE(get_json(f4, host_tag, unknown_state_path, empty_params).empty());
}

TEST(StateServerTest, require_that_top_level_urls_are_generated_correctly)
{
    SimpleHealthProducer f1;
    SimpleMetricsProducer f2;
    SimpleComponentConfigProducer f3;
    StateApi f4(f1, f2, f3);
    EXPECT_EQ("{\"resources\":["
                 "{\"url\":\"http://HOST/state/v1/health\"},"
                 "{\"url\":\"http://HOST/state/v1/metrics\"},"
                 "{\"url\":\"http://HOST/state/v1/config\"},"
                 "{\"url\":\"http://HOST/state/v1/version\"}]}",
                 get_json(f4, host_tag, root_path, empty_params));
    EXPECT_EQ(get_json(f4, host_tag, root_path, empty_params),
                 get_json(f4, host_tag, short_root_path, empty_params));
}

TEST(StateServerTest, require_that_top_level_resource_list_can_be_extended)
{
    SimpleHealthProducer f1;
    SimpleMetricsProducer f2;
    SimpleComponentConfigProducer f3;
    StateApi f4(f1, f2, f3);
    auto token = f4.repo().add_root_resource("/state/v1/custom");
    EXPECT_EQ("{\"resources\":["
                 "{\"url\":\"http://HOST/state/v1/health\"},"
                 "{\"url\":\"http://HOST/state/v1/metrics\"},"
                 "{\"url\":\"http://HOST/state/v1/config\"},"
                 "{\"url\":\"http://HOST/state/v1/version\"},"
                 "{\"url\":\"http://HOST/state/v1/custom\"}]}",
                 get_json(f4, host_tag, root_path, empty_params));
}

TEST(StateServerTest, require_that_health_resource_works_as_expected)
{
    SimpleHealthProducer f1;
    SimpleMetricsProducer f2;
    SimpleComponentConfigProducer f3;
    StateApi f4(f1, f2, f3);
    EXPECT_EQ("{\"status\":{\"code\":\"up\"}}",
                 get_json(f4, host_tag, health_path, empty_params));
    f1.setFailed("FAIL MSG");
    EXPECT_EQ("{\"status\":{\"code\":\"down\",\"message\":\"FAIL MSG\"}}",
                 get_json(f4, host_tag, health_path, empty_params));
}

TEST(StateServerTest, require_that_metrics_resource_works_as_expected)
{
    SimpleHealthProducer f1;
    SimpleMetricsProducer f2;
    SimpleComponentConfigProducer f3;
    StateApi f4(f1, f2, f3);
    EXPECT_EQ("{\"status\":{\"code\":\"up\"}}",
                 get_json(f4, host_tag, metrics_path, empty_params));
    f1.setFailed("FAIL MSG");
    EXPECT_EQ("{\"status\":{\"code\":\"down\",\"message\":\"FAIL MSG\"}}",
                 get_json(f4, host_tag, metrics_path, empty_params));
    f1.setOk();
    f2.setMetrics(R"({"foo":"bar"})", MetricsProducer::ExpositionFormat::JSON);
    f2.setMetrics(R"(cool_stuff{hello="world"} 1 23456)", MetricsProducer::ExpositionFormat::Prometheus);

    auto result = get_body_and_content_type(f4, host_tag, metrics_path, empty_params);
    EXPECT_EQ(R"({"status":{"code":"up"},"metrics":{"foo":"bar"}})", result.first);
    EXPECT_EQ("application/json", result.second);

    result = get_body_and_content_type(f4, host_tag, metrics_path, {{"format", "json"}}); // Explicit JSON
    EXPECT_EQ(R"({"status":{"code":"up"},"metrics":{"foo":"bar"}})", result.first);
    EXPECT_EQ("application/json", result.second);

    result = get_body_and_content_type(f4, host_tag, metrics_path, {{"format", "prometheus"}}); // Explicit Prometheus
    EXPECT_EQ(R"(cool_stuff{hello="world"} 1 23456)", result.first);
    EXPECT_EQ("text/plain; version=0.0.4", result.second);
}

TEST(StateServerTest, require_that_config_resource_works_as_expected)
{
    SimpleHealthProducer f1;
    SimpleMetricsProducer f2;
    SimpleComponentConfigProducer f3;
    StateApi f4(f1, f2, f3);
    EXPECT_EQ("{\"config\":{}}",
                 get_json(f4, host_tag, config_path, empty_params));
    f3.addConfig(SimpleComponentConfigProducer::Config("foo", 3));
    EXPECT_EQ("{\"config\":{\"generation\":3,\"foo\":{\"generation\":3}}}",
                 get_json(f4, host_tag, config_path, empty_params));
    f3.addConfig(SimpleComponentConfigProducer::Config("foo", 4));
    f3.addConfig(SimpleComponentConfigProducer::Config("bar", 4, "error"));
    EXPECT_EQ("{\"config\":{\"generation\":4,\"bar\":{\"generation\":4,\"message\":\"error\"},\"foo\":{\"generation\":4}}}",
                 get_json(f4, host_tag, config_path, empty_params));
    f3.removeConfig("bar");
    EXPECT_EQ("{\"config\":{\"generation\":4,\"foo\":{\"generation\":4}}}",
                 get_json(f4, host_tag, config_path, empty_params));
}

TEST(StateServerTest, version_resource_yields_current_version_number)
{
    SimpleHealthProducer f1;
    SimpleMetricsProducer f2;
    SimpleComponentConfigProducer f3;
    StateApi f4(f1, f2, f3);
    std::ostringstream os;
    os << "{\"version\":\"" << vespalib::Vtag::currentVersion.toString() << "\"}";
    EXPECT_EQ(os.str(),
                 get_json(f4, host_tag, version_path, empty_params));
}

TEST(StateServerTest, require_that_state_api_also_can_return_total_metric)
{
    SimpleHealthProducer f1;
    SimpleMetricsProducer f2;
    SimpleComponentConfigProducer f3;
    StateApi f4(f1, f2, f3);
    f2.setTotalMetrics(R"({"foo":"bar"})", MetricsProducer::ExpositionFormat::JSON);
    f2.setTotalMetrics(R"(cool_stuff{hello="world"} 1 23456)", MetricsProducer::ExpositionFormat::Prometheus);
    EXPECT_EQ(R"({"foo":"bar"})",
                 get_json(f4, host_tag, total_metrics_path, empty_params));
    EXPECT_EQ(R"(cool_stuff{hello="world"} 1 23456)",
                 get_json(f4, host_tag, total_metrics_path, {{"format", "prometheus"}}));
}

TEST(StateServerTest, require_that_custom_handlers_can_be_added_to_the_state_server)
{
    SimpleHealthProducer f1;
    SimpleMetricsProducer f2;
    SimpleComponentConfigProducer f3;
    StateApi f4(f1, f2, f3);
    DummyHandler f5("[123]");
    EXPECT_EQ("", get_json(f4, host_tag, my_path, empty_params));
    auto token = f4.repo().bind(my_path, f5);
    EXPECT_EQ("[123]", get_json(f4, host_tag, my_path, empty_params));
    token.reset();
    EXPECT_EQ("", get_json(f4, host_tag, my_path, empty_params));
}

struct EchoConsumer : MetricsProducer {
    static constexpr const char* to_string(ExpositionFormat format) noexcept {
        switch (format) {
        case ExpositionFormat::JSON: return "JSON";
        case ExpositionFormat::Prometheus: return "Prometheus";
        }
        abort();
    }

    static std::string stringify_params(const std::string &consumer, ExpositionFormat format) {
        // Not semantically meaningful output if format == Prometheus, but doesn't really matter here.
        return vespalib::make_string(R"(["%s", "%s"])", to_string(format), consumer.c_str());
    }

    ~EchoConsumer() override;
    std::string getMetrics(const std::string &consumer, ExpositionFormat format) override {
        return stringify_params(consumer, format);
    }
    std::string getTotalMetrics(const std::string &consumer, ExpositionFormat format) override {
        return stringify_params(consumer, format);
    }
};

EchoConsumer::~EchoConsumer() = default;

TEST(StateServerTest, require_that_empty_v1_metrics_consumer_defaults_to_statereporter)
{
    SimpleHealthProducer f1;
    EchoConsumer f2;
    SimpleComponentConfigProducer f3;
    StateApi f4(f1, f2, f3);
    EXPECT_EQ(R"({"status":{"code":"up"},"metrics":["JSON", "statereporter"]})",
                 get_json(f4, host_tag, metrics_path, empty_params));
    EXPECT_EQ(R"(["Prometheus", "statereporter"])",
                 get_json(f4, host_tag, metrics_path, {{"format", "prometheus"}}));
}

TEST(StateServerTest, require_that_empty_total_metrics_consumer_defaults_to_the_empty_string)
{
    SimpleHealthProducer f1;
    EchoConsumer f2;
    SimpleComponentConfigProducer f3;
    StateApi f4(f1, f2, f3);
    EXPECT_EQ(R"(["JSON", ""])", get_json(f4, host_tag, total_metrics_path, empty_params));
}

TEST(StateServerTest, require_that_metrics_consumer_is_passed_correctly)
{
    SimpleHealthProducer f1;
    EchoConsumer f2;
    SimpleComponentConfigProducer f3;
    StateApi f4(f1, f2, f3);
    std::map<std::string,std::string> my_params;
    my_params["consumer"] = "ME";
    EXPECT_EQ(R"({"status":{"code":"up"},"metrics":["JSON", "ME"]})", get_json(f4, host_tag, metrics_path, my_params));
    EXPECT_EQ(R"(["JSON", "ME"])", get_json(f4, host_tag, total_metrics_path, my_params));
    my_params["format"] = "prometheus";
    EXPECT_EQ(R"(["Prometheus", "ME"])", get_json(f4, host_tag, total_metrics_path, my_params));
}

void check_json(const std::string &expect_json, const std::string &actual_json) {
    Slime expect_slime;
    Slime actual_slime;
    EXPECT_TRUE(slime::JsonFormat::decode(expect_json, expect_slime) > 0);
    EXPECT_TRUE(slime::JsonFormat::decode(actual_json, actual_slime) > 0);
    EXPECT_EQ(expect_slime, actual_slime);
}

TEST(StateServerTest, require_that_generic_state_can_be_explored) {
    std::string json_model =
        "{"
        "  foo: 'bar',"
        "  cnt: 123,"
        "  engine: {"
        "    up: 'yes',"
        "    stats: {"
        "      latency: 5,"
        "      qps: 100"
        "    }"
        "  },"
        "  list: {"
        "    one: {"
        "      size: {"
        "        value: 1"
        "      }"
        "    },"
        "    two: {"
        "      size: 2"
        "    }"
        "  }"
        "}";
    std::string json_root =
        "{"
        "  full: true,"
        "  foo: 'bar',"
        "  cnt: 123,"
        "  engine: {"
        "    up: 'yes',"
        "    url: 'http://HOST/state/v1/engine'"
        "  },"
        "  list: {"
        "    one: {"
        "      size: {"
        "        value: 1,"
        "        url: 'http://HOST/state/v1/list/one/size'"
        "      }"
        "    },"
        "    two: {"
        "      size: 2,"
        "      url: 'http://HOST/state/v1/list/two'"
        "    }"
        "  }"
        "}";
    std::string json_engine =
        "{"
        "  full: true,"
        "  up: 'yes',"
        "  stats: {"
        "    latency: 5,"
        "    qps: 100,"
        "    url: 'http://HOST/state/v1/engine/stats'"
        "  }"
        "}";
    std::string json_engine_stats =
        "{"
        "  full: true,"
        "  latency: 5,"
        "  qps: 100"
        "}";
    std::string json_list =
        "{"
        "  one: {"
        "    size: {"
        "      value: 1,"
        "      url: 'http://HOST/state/v1/list/one/size'"
        "    }"
        "  },"
        "  two: {"
        "    size: 2,"
        "    url: 'http://HOST/state/v1/list/two'"
        "  }"
        "}";
    std::string json_list_one =
        "{"
        "  size: {"
        "    value: 1,"
        "    url: 'http://HOST/state/v1/list/one/size'"
        "  }"
        "}";
    std::string json_list_one_size = "{ full: true, value: 1 }";
    std::string json_list_two = "{ full: true, size: 2 }";
    //-------------------------------------------------------------------------
    Slime slime_state;
    EXPECT_TRUE(slime::JsonFormat::decode(json_model, slime_state) > 0);
    SlimeExplorer slime_explorer(slime_state.get());
    GenericStateHandler state_handler(short_root_path, slime_explorer);
    EXPECT_EQ("", get_json(state_handler, host_tag, unknown_path, empty_params));
    EXPECT_EQ("", get_json(state_handler, host_tag, unknown_state_path, empty_params));
    check_json(json_root, get_json(state_handler, host_tag, root_path, empty_params));
    check_json(json_engine, get_json(state_handler, host_tag, root_path + "engine", empty_params));
    check_json(json_engine_stats, get_json(state_handler, host_tag, root_path + "engine/stats", empty_params));
    check_json(json_list, get_json(state_handler, host_tag, root_path + "list", empty_params));
    check_json(json_list_one, get_json(state_handler, host_tag, root_path + "list/one", empty_params));
    check_json(json_list_one_size, get_json(state_handler, host_tag, root_path + "list/one/size", empty_params));
    check_json(json_list_two, get_json(state_handler, host_tag, root_path + "list/two", empty_params));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    mkdir("var", S_IRWXU);
    mkdir("var/run", S_IRWXU);
    auto res = RUN_ALL_TESTS();
    rmdir("var/run");
    rmdir("var");
    return res;
}
