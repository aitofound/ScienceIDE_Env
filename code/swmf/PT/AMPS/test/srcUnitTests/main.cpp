//  Copyright (C) 2002 Regents of the University of Michigan, portions used with permission 
//  For more information, see http://csem.engin.umich.edu/tools/swmf



//the particle class
#include "pic.h"
#include "constants.h"

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <list>
#include <math.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <iostream>
#include <iostream>
#include <fstream>
#include <time.h>

#include <sys/time.h>
#include <sys/resource.h>

#include <gtest/gtest.h>



//$Id$

//TEST(AddFunction, HandlesNegativeNumbers) {
//    EXPECT_EQ(add(-2, -3), -5);  // Check if -2 + -3 equals -5
//}

// Custom test event listener to capture test results and generate a summary report
class SummaryReportListener : public ::testing::TestEventListener {
private:
    // Pointer to the default listener to forward calls
    std::unique_ptr<::testing::TestEventListener> default_listener_;

    // Variables to store test results
    int total_tests_ = 0;
    int passed_tests_ = 0;
    int failed_tests_ = 0;

    // Stream to store details of failed and passed tests
    std::ostringstream failed_tests_report_;
    std::ostringstream passed_tests_report_;

public:
    explicit SummaryReportListener(std::unique_ptr<::testing::TestEventListener> default_listener)
        : default_listener_(std::move(default_listener)) {}

    // Implement required methods

    // Test iteration methods (no-op in this case)
    void OnTestIterationStart(const ::testing::UnitTest& unit_test, int iteration) override {
        default_listener_->OnTestIterationStart(unit_test, iteration);
    }

    void OnTestIterationEnd(const ::testing::UnitTest& unit_test, int iteration) override {
        default_listener_->OnTestIterationEnd(unit_test, iteration);
    }

    // Environment setup/teardown methods (no-op in this case)
    void OnEnvironmentsSetUpStart(const ::testing::UnitTest& unit_test) override {
        default_listener_->OnEnvironmentsSetUpStart(unit_test);
    }

    void OnEnvironmentsSetUpEnd(const ::testing::UnitTest& unit_test) override {
        default_listener_->OnEnvironmentsSetUpEnd(unit_test);
    }

    void OnEnvironmentsTearDownStart(const ::testing::UnitTest& unit_test) override {
        default_listener_->OnEnvironmentsTearDownStart(unit_test);
    }

    void OnEnvironmentsTearDownEnd(const ::testing::UnitTest& unit_test) override {
        default_listener_->OnEnvironmentsTearDownEnd(unit_test);
    }

    // Test program methods
    void OnTestProgramStart(const ::testing::UnitTest& unit_test) override {
        default_listener_->OnTestProgramStart(unit_test);
    }

    void OnTestStart(const ::testing::TestInfo& test_info) override {
        default_listener_->OnTestStart(test_info);
    }

    void OnTestPartResult(const ::testing::TestPartResult& test_part_result) override {
        default_listener_->OnTestPartResult(test_part_result);
    }

    void OnTestEnd(const ::testing::TestInfo& test_info) override {
        total_tests_++;
        if (test_info.result()->Passed()) {
            passed_tests_++;
	    passed_tests_report_ << "Test Passed: " << test_info.test_case_name() << "." << test_info.name() << "\n";
        } else {
            failed_tests_++;
            failed_tests_report_ << "Test Failed: " << test_info.test_case_name() << "." << test_info.name() << "\n";
        }
        default_listener_->OnTestEnd(test_info);
    }

    void OnTestProgramEnd(const ::testing::UnitTest& unit_test) override {
        default_listener_->OnTestProgramEnd(unit_test);

        // Generate summary report
        std::ofstream report_file("test_summary_report.txt");
        if (report_file.is_open()) {
            report_file << "Google Test Summary Report\n";
            report_file << "===========================\n";
            report_file << "Total tests run: " << total_tests_ << "\n";
            report_file << "Tests passed: " << passed_tests_ << "\n";
            report_file << "Tests failed: " << failed_tests_ << "\n";

	    report_file << "\nDetails of passed tests:\n" << passed_tests_report_.str() << "\n";
            report_file << "Details of failed tests:\n" << failed_tests_report_.str() << "\n";
            report_file.close();
        } else {
            std::cerr << "Error: Unable to write test summary report.\n";
        }
    }
};


void amps_init();
void amps_time_step();

void pbuffer_test_for_linker(); 
void collisions_test_for_linker();
void idf_test_for_linker();
void split_merge_test_for_linker();
void distribution_test_for_linker();

int main(int argc,char **argv) {

  #ifdef LINK_BUFFER_TEST
  pbuffer_test_for_linker();
  #endif 

  #ifdef LINK_COLLISIONS_TEST
  collisions_test_for_linker();
  #endif
 
  #ifdef LINK_IDF_TEST
  idf_test_for_linker();
  #endif

  #ifdef LINK_SPLIT_MERGE_TEST
  split_merge_test_for_linker();
  #endif

  #ifdef LINK_DISTRIBUTION_TEST
  distribution_test_for_linker();
  #endif


  clock_t runtime =-clock();

PIC::Debugger::cGenericTimer t;
  
t.Start("main",__LINE__);
  amps_init();

t.SwitchTimeSegment(__LINE__,"first switch"); 


    // Init tests
    ::testing::InitGoogleTest(&argc, argv);

    // Replace the default test event listener with our custom listener
    auto& listeners = ::testing::UnitTest::GetInstance()->listeners();
    auto default_listener = listeners.Release(listeners.default_result_printer());
    listeners.Append(new SummaryReportListener(std::unique_ptr<::testing::TestEventListener>(default_listener)));


    // Run tests
    RUN_ALL_TESTS();

  MPI_Finalize();
  return EXIT_SUCCESS;
}
