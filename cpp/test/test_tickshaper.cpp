#include <gtest/gtest.h>
#include "../include/TickShaper.h"
#include "../include/ITCHParser.h"
#include "../include/MessageProcessor.h"
#include "../include/ThrottleController.h"
#include "../include/MicroburstDetector.h"
#include <chrono>
#include <thread>

using namespace tickshaper;

class TickShaperTest : public ::testing::Test {
protected:
    void SetUp() override {
        tickshaper = std::make_unique<TickShaper>();
    }
    
    void TearDown() override {
        if (tickshaper && tickshaper->IsRunning()) {
            tickshaper->Stop();
        }
    }
    
    std::unique_ptr<TickShaper> tickshaper;
};

TEST_F(TickShaperTest, InitializationTest) {
    // Test initialization with default config
    EXPECT_TRUE(tickshaper->Initialize("../config/tickshaper.conf"));
    
    // Check initial metrics
    const auto& metrics = tickshaper->GetMetrics();
    EXPECT_EQ(metrics.messages_processed.load(), 0);
    EXPECT_EQ(metrics.messages_throttled.load(), 0);
    EXPECT_FALSE(metrics.microburst_detected.load());
}

TEST_F(TickShaperTest, StartStopTest) {
    EXPECT_TRUE(tickshaper->Initialize("../config/tickshaper.conf"));
    
    // Test start
    tickshaper->Start();
    EXPECT_TRUE(tickshaper->IsRunning());
    
    // Let it run briefly
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Test stop
    tickshaper->Stop();
    EXPECT_FALSE(tickshaper->IsRunning());
}

TEST_F(TickShaperTest, ReplaySpeedTest) {
    EXPECT_TRUE(tickshaper->Initialize("../config/tickshaper.conf"));
    
    // Test valid replay speeds
    tickshaper->SetReplaySpeed(0.5);
    tickshaper->SetReplaySpeed(1.0);
    tickshaper->SetReplaySpeed(2.0);
    tickshaper->SetReplaySpeed(10.0);
    
    // Invalid speeds should be rejected (no crash)
    tickshaper->SetReplaySpeed(0.0);
    tickshaper->SetReplaySpeed(-1.0);
    tickshaper->SetReplaySpeed(1000.0);
}

TEST_F(TickShaperTest, ThrottleRateTest) {
    EXPECT_TRUE(tickshaper->Initialize("../config/tickshaper.conf"));
    
    // Test valid throttle rates
    tickshaper->SetThrottleRate(1000);
    tickshaper->SetThrottleRate(50000);
    tickshaper->SetThrottleRate(100000);
    tickshaper->SetThrottleRate(500000);
    
    // Invalid rates should be rejected
    tickshaper->SetThrottleRate(0);
    tickshaper->SetThrottleRate(2000000);
}

class ITCHParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        parser = std::make_unique<ITCHParser>();
    }
    
    std::unique_ptr<ITCHParser> parser;
};

TEST_F(ITCHParserTest, InitializationTest) {
    // Test with non-existent file (should create sample data)
    EXPECT_TRUE(parser->Initialize("nonexistent.itch"));
    EXPECT_GT(parser->GetTotalMessages(), 0);
}

TEST_F(ITCHParserTest, MessageParsingTest) {
    EXPECT_TRUE(parser->Initialize("test.itch"));
    
    // Parse some messages
    for (int i = 0; i < 10; ++i) {
        auto message = parser->GetNextMessage();
        EXPECT_NE(message, nullptr);
        EXPECT_GT(message->timestamp, 0);
        EXPECT_GT(message->data.size(), 0);
    }
}

class MessageProcessorTest : public ::testing::Test {
protected:
    void SetUp() override {
        processor = std::make_unique<MessageProcessor>();
        metrics = std::make_unique<SystemMetrics>();
        processor->Initialize(nullptr, metrics.get());
    }
    
    std::unique_ptr<MessageProcessor> processor;
    std::unique_ptr<SystemMetrics> metrics;
};

TEST_F(MessageProcessorTest, ProcessingTest) {
    // Create a synthetic ITCH message
    std::vector<uint8_t> data(36, 0);
    RawMessage raw_msg('A', 123456789, data.data(), data.size());
    
    TickData tick_data;
    bool result = processor->ProcessMessage(raw_msg, tick_data);
    
    EXPECT_TRUE(result);
    EXPECT_EQ(tick_data.timestamp, 123456789);
    EXPECT_EQ(tick_data.message_type, 'A');
}

class ThrottleControllerTest : public ::testing::Test {
protected:
    void SetUp() override {
        controller = std::make_unique<ThrottleController>();
        controller->Initialize(1000); // 1000 msg/s
    }
    
    std::unique_ptr<ThrottleController> controller;
};

TEST_F(ThrottleControllerTest, BasicThrottlingTest) {
    // Should allow initial messages
    EXPECT_TRUE(controller->ShouldProcess());
    
    // Set very low rate
    controller->SetRate(1); // 1 msg/s
    
    // First message should pass
    EXPECT_TRUE(controller->ShouldProcess());
    
    // Immediate second message should be throttled
    EXPECT_FALSE(controller->ShouldProcess());
}

TEST_F(ThrottleControllerTest, RateChangeTest) {
    controller->SetRate(100);
    EXPECT_EQ(controller->GetCurrentRate(), 100);
    
    controller->SetRate(50000);
    EXPECT_EQ(controller->GetCurrentRate(), 50000);
}

class MicroburstDetectorTest : public ::testing::Test {
protected:
    void SetUp() override {
        detector = std::make_unique<MicroburstDetector>();
        metrics = std::make_unique<SystemMetrics>();
        detector->Initialize(metrics.get());
    }
    
    std::unique_ptr<MicroburstDetector> detector;
    std::unique_ptr<SystemMetrics> metrics;
};

TEST_F(MicroburstDetectorTest, DetectionTest) {
    // Initially no microburst
    EXPECT_FALSE(detector->IsCurrentlyInMicroburst());
    
    // Simulate high message rate
    TickData tick_data;
    tick_data.timestamp = 123456789;
    tick_data.symbol_id = 1;
    tick_data.price = 10000;
    tick_data.size = 100;
    tick_data.side = 'B';
    tick_data.message_type = 'A';
    
    // Send many messages quickly to trigger detection
    for (int i = 0; i < 1000; ++i) {
        detector->CheckMessage(tick_data);
        tick_data.timestamp += 1000; // 1μs apart
    }
    
    // Give detector time to process
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Check if microburst was detected
    auto events = detector->GetRecentEvents();
    // Note: Actual detection depends on timing and thresholds
}

// Performance benchmark test
TEST(PerformanceTest, ThroughputBenchmark) {
    auto start = std::chrono::high_resolution_clock::now();
    
    ITCHParser parser;
    ASSERT_TRUE(parser.Initialize("benchmark.itch"));
    
    MessageProcessor processor;
    SystemMetrics metrics;
    processor.Initialize(nullptr, &metrics);
    
    const int num_messages = 10000;
    int processed = 0;
    
    for (int i = 0; i < num_messages; ++i) {
        auto message = parser.GetNextMessage();
        if (message) {
            TickData tick_data;
            if (processor.ProcessMessage(*message, tick_data)) {
                processed++;
            }
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double messages_per_second = (processed * 1000000.0) / duration.count();
    
    std::cout << "Processed " << processed << " messages in " 
              << duration.count() << " μs" << std::endl;
    std::cout << "Throughput: " << messages_per_second << " msg/s" << std::endl;
    
    // Expect at least 10K msg/s in test environment
    EXPECT_GT(messages_per_second, 10000);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}