#pragma once

#include <string>

namespace arancini::ir {
enum metadata_kind { text_value, numeric_value };

class metadata {
public:
	metadata(metadata_kind kind)
		: kind_(kind)
	{
	}

	metadata_kind kind() const { return kind_; }

private:
	metadata_kind kind_;
};

class text_value_metadata : public metadata {
public:
	text_value_metadata(const std::string &value)
		: metadata(metadata_kind::text_value)
		, value_(value)
	{
	}

	const std::string &value() const { return value_; }

private:
	std::string value_;
};

class numeric_value_metadata : public metadata {
public:
	using numeric_type = unsigned long long;

	numeric_value_metadata(numeric_type value)
		: metadata(metadata_kind::numeric_value)
		, value_(value)
	{
	}

	numeric_type value() const { return value_; }

private:
	numeric_type value_;
};
} // namespace arancini::ir
