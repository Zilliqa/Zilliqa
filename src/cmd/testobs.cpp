/*
 * Copyright (C) 2019 Zilliqa
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <iostream>
#include <chrono>
#include <map>
#include <memory>
#include <thread>
#include <vector>

#include <opentelemetry/sdk/metrics/view/meter_selector.h>
#include <opentelemetry/sdk/metrics/view/view.h>
#include "opentelemetry/context/context.h"
#include "opentelemetry/metrics/provider.h"

#include "libUtils/Metrics.h"

namespace metrics_api = opentelemetry::metrics;
namespace metrics_sdk = opentelemetry::sdk::metrics;
namespace nostd = opentelemetry::nostd;
namespace common = opentelemetry::common;


//
// Tests using in class Static like original examples.
//


namespace test {

    std::map<std::string, std::string> get_random_attr() {
        const std::vector<std::pair<std::string, std::string>> labels = {{"key1", "value1"},
                                                                         {"key2", "value2"},
                                                                         {"key3", "value3"},
                                                                         {"key4", "value4"},
                                                                         {"key5", "value5"}};
        return std::map<std::string, std::string>{labels[rand() % (labels.size() - 1)],
                                                  labels[rand() % (labels.size() - 1)]};
    }


    class MeasurementFetcher {
    public:
        static void Fetcher(opentelemetry::metrics::ObserverResult observer_result, void * /* state */) {
            std::cout << "static callback - Fetcher" << std::endl;
            if (std::holds_alternative<
                    std::shared_ptr<opentelemetry::metrics::ObserverResultT<double>>>(observer_result)) {
                dValue = (rand() % 999) + ((rand() % 299) / 1000.0);
                std::get<std::shared_ptr<opentelemetry::metrics::ObserverResultT<double>>>(
                        observer_result)
                        ->Observe(dValue);
            } else {
                lValue = (rand() % 999) + ((rand() % 299) / 1000.0);
                std::get<std::shared_ptr<opentelemetry::metrics::ObserverResultT<long>>>(
                        observer_result)
                        ->Observe(lValue);
            }
        }

        static void CounterFetcher(opentelemetry::metrics::ObserverResult observer_result, void * /* state */) {
            std::cout << "static callback - CounterFetcher" << std::endl;
            if (std::holds_alternative<
                    std::shared_ptr<opentelemetry::metrics::ObserverResultT<double>>>(observer_result)) {
                dValue += ((rand() % 299) / 1000.0);
                std::get<std::shared_ptr<opentelemetry::metrics::ObserverResultT<double>>>(
                        observer_result)
                        ->Observe(dValue, get_random_attr());
            } else {
                dValue += ((rand() % 299) / 1000.0);
                std::get<std::shared_ptr<opentelemetry::metrics::ObserverResultT<long>>>(
                        observer_result)
                        ->Observe(dValue, get_random_attr());
            }
        }

        static void Int64GaugeFetcher(opentelemetry::metrics::ObserverResult observer_result, void * /* state */) {
            std::cout << "static callback - Int64GaugeCounterFetcher" << std::endl;
            if (std::holds_alternative<
                    std::shared_ptr<opentelemetry::metrics::ObserverResultT<double>>>(observer_result)) {
                std::cout << "requested a double when its an uint64" << std::endl;
            } else {
                lValue = (rand() % 10);
                std::get<std::shared_ptr<opentelemetry::metrics::ObserverResultT<long>>>(
                        observer_result)
                        ->Observe(lValue, get_random_attr());
            }
        }

        static double dValue;
        static uint64_t lValue;
    };
};

double test::MeasurementFetcher::dValue = 0.0;
uint64_t test::MeasurementFetcher::lValue = 0.0;

void TestDoubleSimpleGauge() {

    std::shared_ptr<opentelemetry::metrics::ObservableInstrument>
            testGauge = Metrics::GetMeter()->CreateDoubleObservableGauge("zilliqa.double.gauage.noview.observable","A Double Observable Gauge","ms");


    testGauge->AddCallback(test::MeasurementFetcher::Fetcher, nullptr);

    for (uint32_t i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    testGauge->RemoveCallback(test::MeasurementFetcher::Fetcher, nullptr);
}

void TestSimplei64Gauge() {

    std::shared_ptr<opentelemetry::metrics::ObservableInstrument>
            testGauge = Metrics::GetMeter()->CreateInt64ObservableGauge("zilliqa.i64.gaauge.noview.observable","A I64 Observable Gauge","barrels");


    testGauge->AddCallback(test::MeasurementFetcher::Int64GaugeFetcher, nullptr);

    for (uint32_t i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    testGauge->RemoveCallback(test::MeasurementFetcher::Int64GaugeFetcher, nullptr);
}

void TestSimpleDoubleCounter() {

    std::shared_ptr<opentelemetry::metrics::ObservableInstrument>
            testCounter = Metrics::GetMeter()->CreateDoubleObservableCounter("zilliqa.double.counter.noview.observable","A Double Observable Gauge","seconds");

    testCounter->AddCallback(test::MeasurementFetcher::CounterFetcher, nullptr);

    for (uint32_t i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    testCounter->RemoveCallback(test::MeasurementFetcher::CounterFetcher, nullptr);
}

void TestSimpleObservableI64Counter() {

    std::shared_ptr<opentelemetry::metrics::ObservableInstrument>
            testCounter = Metrics::GetMeter()->CreateInt64ObservableCounter("zilliqa.i64.counter.noview.observable","A I64 Observable Counter","minutes");

    testCounter->AddCallback(test::MeasurementFetcher::CounterFetcher, nullptr);

    for (uint32_t i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    testCounter->RemoveCallback(test::MeasurementFetcher::CounterFetcher, nullptr);
}


void TestSimpleDoubleCounterSync() {

    zil::metrics::doubleCounter_t testCounter = Metrics::GetMeter()->CreateDoubleCounter("zilliqa.Double.counter.noview.synchronous","A Synchronous Double Counter","nanoseconds");

    for (uint32_t i = 0; i < 10; ++i) {
        testCounter->Add(((rand() % 299) / 1000.0), test::get_random_attr());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void TestSimpleI64CounterSync() {

    zil::metrics::uint64Counter_t testCounter = Metrics::GetMeter()->CreateUInt64Counter("zilliqa.i64.counter.noview.synchronous","A sync I64 Counter","hours");

    for (uint32_t i = 0; i < 10; ++i) {
        testCounter->Add(((rand() % 299)));
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void TestSimpleDoubleHistogram() {

    std::string name = "zilliqa.double";
    std::string description = {"A view with boundaries"};
    std::string hname = "zilliqa.double.hist";

    std::list<double> list{0, 1, 2, 3};


    Metrics::GetInstance().AddCounterHistogramView(name, hname, list, description);

    auto histogram_counter = Metrics::GetMeter()->CreateDoubleHistogram(name , "The Metric", "ds");

    auto context = opentelemetry::context::Context{};
    for (uint32_t i = 0; i < 10000; ++i) {
        double val = (rand() % 3);
        std::map<std::string, std::string> labels = test::get_random_attr();
        auto labelkv = opentelemetry::common::KeyValueIterableView<decltype(labels)>{labels};
        histogram_counter->Record(val, labelkv, context);
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
}



//
// This version can define its own callback not owned by anybody.
//

void TestSimpleDoubleObservableCounterLamda() {

    std::shared_ptr<opentelemetry::metrics::ObservableInstrument>
            testCounter  = Metrics::GetMeter()->CreateDoubleObservableCounter("zilliqa.Double.lambda.observable","A Doule observale mit Lambda","ms");;

    struct counter_t {
        double first_count{0.0};
        double second_count{0.0};
        double third_count{0.0};
    };
    counter_t p;

    const auto lamda = [](opentelemetry::metrics::ObserverResult observer_result, void *state) {

        counter_t *params = static_cast<counter_t *>(state);
        if (std::holds_alternative<std::shared_ptr<opentelemetry::metrics::ObserverResultT<double>>>(observer_result)) {

            std::get<std::shared_ptr<opentelemetry::metrics::ObserverResultT<double>>>(
                    observer_result)
                    ->Observe(params->first_count, {{"evm", "1"}});
            std::get<std::shared_ptr<opentelemetry::metrics::ObserverResultT<double>>>(
                    observer_result)
                    ->Observe(params->second_count, {{"evm", "2"}});
            std::get<std::shared_ptr<opentelemetry::metrics::ObserverResultT<double>>>(
                    observer_result)
                    ->Observe(params->third_count, {{"evm", "3"}});
        }
    };

    // Pass a struct with lots of values that you want to set
    // Give the address of struct, must stay in scope or you are doomed.

    testCounter->AddCallback(lamda, static_cast<void *>(&p));

    for (uint32_t i = 0; i < 10; ++i) {

        p.first_count += 1;
        p.second_count += 2;
        p.third_count += 3;

        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    // Must supply original pointer and exact params or it don't match and is not deleted, leads to UB
    //

    testCounter->RemoveCallback(lamda, static_cast<void *>(&p));

}

//
// Observble Counter I64 with Lambda.
//

void TestSimpleUint64ObservableCounterLamda() {

    std::shared_ptr<opentelemetry::metrics::ObservableInstrument>
            testCounter= Metrics::GetMeter()->CreateInt64ObservableCounter("zilliqa.Double.lambda.observable","A I64 observable Counter mit Lambda","ms");

    struct counter_t {
        int64_t first_count{0};
        int64_t second_count{0};
        int64_t third_count{0};
    };
    counter_t p;

    const auto worker = [](opentelemetry::metrics::ObserverResult observer_result, void *state) {

        counter_t *params = static_cast<counter_t *>(state);
        if (std::holds_alternative<std::shared_ptr<opentelemetry::metrics::ObserverResultT<double>>>(observer_result)) {
            std::cout << "should not be a double" << std::endl;
        } else {
            std::get<std::shared_ptr<opentelemetry::metrics::ObserverResultT<int64_t>>>(
                    observer_result)
                    ->Observe(params->first_count, {{"evm", "1"}});
            std::get<std::shared_ptr<opentelemetry::metrics::ObserverResultT<int64_t>>>(
                    observer_result)
                    ->Observe(params->second_count, {{"evm", "2"}});
            std::get<std::shared_ptr<opentelemetry::metrics::ObserverResultT<int64_t>>>(
                    observer_result)
                    ->Observe(params->third_count, {{"evm", "3"}});
        }
    };

    testCounter->AddCallback(worker, static_cast<void *>(&p));


    for (uint32_t i = 0; i < 10; ++i) {

        p.first_count += 1;
        p.second_count += 2;
        p.third_count += 3;

        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    testCounter->RemoveCallback(worker, static_cast<void *>(&p));
}


int main() {
    Metrics::GetInstance();

    // These int64 Histograms fail because the default boundaries only support double.
    std::cout << "Testing Histogram" << std::endl;
    //TestSimpleHistogram();
    TestSimpleDoubleHistogram();

    while (1)
        std::this_thread::sleep_for(std::chrono::milliseconds(250));

    std::cout << "Testing Double Counter" << std::endl;
    TestSimpleDoubleCounter();

    std::cout << "Testing Dbl Counter " << std::endl;
    TestSimpleDoubleCounter();

    std::cout << "Testing Dbl Counter Sync" << std::endl;
    TestSimpleDoubleCounterSync();

    std::cout << "Testing Simple Gauge" << std::endl;
    TestDoubleSimpleGauge();

    std::cout << "Testing Docuble Counter" << std::endl;
    TestSimpleDoubleCounter();

    std::cout << "Testing Histogram" << std::endl;
    TestSimpleDoubleHistogram();

    std::cout << "Testing i64 Gauge" << std::endl;
    TestSimplei64Gauge();

    std::cout << "Testing i64 Counter Sync" << std::endl;
    TestSimpleI64CounterSync();

    std::cout << "Testing i 64 ObservableCounter" << std::endl;
    TestSimpleObservableI64Counter();

    std::cout << "Testing lamda" << std::endl;
    TestSimpleDoubleObservableCounterLamda();

    std::cout << "Testing CounterLambda" << std::endl;

    TestSimpleUint64ObservableCounterLamda();


    for (int i = 0; i < 1000; i++) {
        std::cout << "Sleeping to make sure the is no more activity" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    std::shared_ptr<metrics_api::MeterProvider> none;
    metrics_api::Provider::SetMeterProvider(none);

    return 0;
}
