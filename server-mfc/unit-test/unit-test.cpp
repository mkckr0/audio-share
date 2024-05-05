#include "pch.h"
#include "CppUnitTest.h"
#include "../audio-share-server/util.hpp"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace unittest
{
	TEST_CLASS(test_split_string)
	{
	public:
		
        TEST_METHOD(split_string_0) {
            Assert::IsTrue(std::vector<std::string>{"1", "2", "3"} == util::split_string("1.2.3", '.'));
        }

        TEST_METHOD(split_string_1) {
            Assert::IsTrue(std::vector<std::string>{"2", "3"} == util::split_string(".2.3", '.'));
        }

        TEST_METHOD(split_string_2) {
            Assert::IsTrue(std::vector<std::string>{} == util::split_string("", '.'));
        }

        TEST_METHOD(split_string_3) {
            Assert::IsTrue(std::vector<std::string>{} == util::split_string(".", '.'));
        }
	};

    TEST_CLASS(test_is_newer_version)
    {
    public:
        TEST_METHOD(is_newer_version_arg0) {
            try
            {
                util::is_newer_version("3.2.2", "v0.0.1");
            }
            catch (const std::exception&)
            {
                return;
            }

            Assert::Fail();
        }

        TEST_METHOD(is_newer_version_arg1) {
            try
            {
                util::is_newer_version("v3.2.", "v0.0.");
            }
            catch (const std::exception&)
            {
                return;
            }

            Assert::Fail();
        }

        TEST_METHOD(is_newer_version_version0) {
            Assert::IsTrue(util::is_newer_version("v0.0.17", "v0.0.9"));
        }

        TEST_METHOD(is_newer_version_version1) {
            Assert::IsTrue(util::is_newer_version("v0.1.0", "v0.0.17"));
        }

        TEST_METHOD(is_newer_version_version2) {
            Assert::IsTrue(util::is_newer_version("v0.17.0", "v0.9.17"));
        }

        TEST_METHOD(is_newer_version_version3) {
            Assert::IsTrue(util::is_newer_version("v12.17.0", "v1.0.0"));
        }

        TEST_METHOD(is_newer_version_version4) {
            Assert::IsFalse(util::is_newer_version("v12.17.0", "v12.17.0"));
        }
    };
}
