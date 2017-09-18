/*----------------------------------------------------------------------------*/
/* Copyright (c) FIRST 2017. All Rights Reserved.                             */
/* Open Source Software - may be modified and shared by FRC teams. The code   */
/* must be accompanied by the FIRST BSD license file in the root directory of */
/* the project.                                                               */
/*----------------------------------------------------------------------------*/

#include "support/HttpUtil.h"

#include "gtest/gtest.h"

namespace wpi {

TEST(HttpHeaderParserTest, FeedStartLineOnly) {
  HttpHeaderParser parser(true);
  EXPECT_TRUE(parser.Feed("GET / HTTP/1.1\r\n\r\n").empty());
  EXPECT_TRUE(parser.IsDone());
  EXPECT_FALSE(parser.HasError());
  EXPECT_EQ(parser.GetStartLine(), "GET / HTTP/1.1");
}

TEST(HttpHeaderParserTest, FeedExact) {
  HttpHeaderParser parser(true);
  EXPECT_TRUE(parser.Feed("GET / HTTP/1.1\r\nField: Value\r\n\r\n").empty());
  EXPECT_TRUE(parser.IsDone());
  EXPECT_FALSE(parser.HasError());
  EXPECT_EQ(parser.GetStartLine(), "GET / HTTP/1.1");
  llvm::SmallString<32> buf;
  EXPECT_EQ(parser.GetHeader("Field", buf), "Value");
  EXPECT_TRUE(parser.GetHeader("Foo", buf).empty());
}

TEST(HttpHeaderParserTest, FeedPartial) {
  HttpHeaderParser parser(true);
  EXPECT_TRUE(parser.Feed("GET / HTTP/1.").empty());
  EXPECT_FALSE(parser.IsDone());
  EXPECT_TRUE(parser.Feed("1\r\nField:").empty());
  EXPECT_FALSE(parser.IsDone());
  EXPECT_TRUE(parser.Feed(" Value\r\n\r").empty());
  EXPECT_FALSE(parser.IsDone());
  EXPECT_TRUE(parser.Feed("\n").empty());
  EXPECT_TRUE(parser.IsDone());
  EXPECT_FALSE(parser.HasError());
  EXPECT_EQ(parser.GetStartLine(), "GET / HTTP/1.1");
  llvm::SmallString<32> buf;
  EXPECT_EQ(parser.GetHeader("Field", buf), "Value");
}

TEST(HttpHeaderParserTest, FeedTrailing) {
  HttpHeaderParser parser(true);
  EXPECT_EQ(parser.Feed("GET / HTTP/1.1\r\nField: Value\r\n\r\nabc"), "abc");
  EXPECT_TRUE(parser.IsDone());
  EXPECT_FALSE(parser.HasError());
}

TEST(HttpHeaderParserTest, LineTrailing) {
  HttpHeaderParser parser(true);
  parser.Feed("GET / HTTP/1.1\r\nField:\tValue   \r\n\r\n");
  EXPECT_TRUE(parser.IsDone());
  EXPECT_FALSE(parser.HasError());
  llvm::SmallString<32> buf;
  EXPECT_EQ(parser.GetHeader("Field", buf), "Value");
}

TEST(HttpHeaderParserTest, LineFolding) {
  HttpHeaderParser parser(true);
  parser.Feed("GET / HTTP/1.1\r\nField: Value\r\n   More\r\n\r\n");
  EXPECT_TRUE(parser.IsDone());
  EXPECT_FALSE(parser.HasError());
  llvm::SmallString<32> buf;
  EXPECT_EQ(parser.GetHeader("Field", buf), "Value More");
}

TEST(HttpHeaderParserTest, Reset) {
  HttpHeaderParser parser(true);
  parser.Feed("GET / HTTP/1.1\r\nField1: Value1\r\n\r\n");
  EXPECT_TRUE(parser.IsDone());
  parser.Reset(true);
  EXPECT_FALSE(parser.IsDone());
  parser.Feed("GET / HTTP/1.1\r\nField2: Value2\r\n\r\n");
  EXPECT_TRUE(parser.IsDone());
  llvm::SmallString<32> buf;
  EXPECT_TRUE(parser.GetHeader("Field1", buf).empty());
  EXPECT_EQ(parser.GetHeader("Field2", buf), "Value2");
}

TEST(HttpHeaderParserTest, ErrorFieldWhitespace) {
  HttpHeaderParser parser(true);
  parser.Feed("GET / HTTP/1.1\r\nField : Value\r\n\r\n");
  EXPECT_TRUE(parser.IsDone());
  EXPECT_TRUE(parser.HasError());
}

TEST(HttpHeaderParserTest, ErrorNoPrevFolding) {
  HttpHeaderParser parser(true);
  parser.Feed("GET / HTTP/1.1\r\n Value\r\n\r\n");
  EXPECT_TRUE(parser.IsDone());
  EXPECT_TRUE(parser.HasError());
}

TEST(HttpHeaderParserTest, NoStartLine) {
  HttpHeaderParser parser(false);
  parser.Feed("Field: Value\r\n\r\n");
  EXPECT_TRUE(parser.IsDone());
  EXPECT_FALSE(parser.HasError());
  EXPECT_TRUE(parser.GetStartLine().empty());
  llvm::SmallString<32> buf;
  EXPECT_EQ(parser.GetHeader("Field", buf), "Value");
}

TEST(HttpMultipartScannerTest, FeedExact) {
  HttpMultipartScanner scanner("foo");
  EXPECT_TRUE(scanner.Feed("abcdefg---\r\n--foo\r\n").empty());
  EXPECT_TRUE(scanner.IsDone());
  EXPECT_TRUE(scanner.GetSkipped().empty());
}

TEST(HttpMultipartScannerTest, FeedPartial) {
  HttpMultipartScanner scanner("foo");
  EXPECT_TRUE(scanner.Feed("abcdefg--").empty());
  EXPECT_FALSE(scanner.IsDone());
  EXPECT_TRUE(scanner.Feed("-\r\n").empty());
  EXPECT_FALSE(scanner.IsDone());
  EXPECT_TRUE(scanner.Feed("--foo\r").empty());
  EXPECT_FALSE(scanner.IsDone());
  EXPECT_TRUE(scanner.Feed("\n").empty());
  EXPECT_TRUE(scanner.IsDone());
}

TEST(HttpMultipartScannerTest, FeedTrailing) {
  HttpMultipartScanner scanner("foo");
  EXPECT_EQ(scanner.Feed("abcdefg---\r\n--foo\r\nxyz"), "xyz");
}

TEST(HttpMultipartScannerTest, FeedPadding) {
  HttpMultipartScanner scanner("foo");
  EXPECT_EQ(scanner.Feed("abcdefg---\r\n--foo    \r\nxyz"), "xyz");
  EXPECT_TRUE(scanner.IsDone());
}

TEST(HttpMultipartScannerTest, SaveSkipped) {
  HttpMultipartScanner scanner("foo", true);
  scanner.Feed("abcdefg---\r\n--foo\r\n");
  EXPECT_EQ(scanner.GetSkipped(), "abcdefg---\r\n--foo\r\n");
}

TEST(HttpMultipartScannerTest, Reset) {
  HttpMultipartScanner scanner("foo", true);

  scanner.Feed("abcdefg---\r\n--foo\r\n");
  EXPECT_TRUE(scanner.IsDone());
  EXPECT_EQ(scanner.GetSkipped(), "abcdefg---\r\n--foo\r\n");

  scanner.Reset("bar", true);
  EXPECT_FALSE(scanner.IsDone());

  scanner.Feed("--foo\r\n--bar\r\n");
  EXPECT_TRUE(scanner.IsDone());
  EXPECT_EQ(scanner.GetSkipped(), "--foo\r\n--bar\r\n");
}

}  // namespace wpi
