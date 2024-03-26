#pragma once
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define E_EQ EXPECT_EQ
#define E_NE EXPECT_NE
#define E_LE EXPECT_LE
#define E_LT EXPECT_LT
#define E_GE EXPECT_GE
#define E_GT EXPECT_GT
#define E_THAT EXPECT_THAT
#define E_FALSE EXPECT_FALSE
#define E_TRUE EXPECT_TRUE
#define E_STREQ EXPECT_STREQ

#define FOR_I(n) for (int i = 0; i < n; ++i)
#define FOR_J(n) for (int j = 0; j < n; ++j)
#define FOR_K(n) for (int k = 0; k < n; ++k)