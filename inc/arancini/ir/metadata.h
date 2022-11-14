#pragma once

#include <string>

namespace arancini::ir {
enum metadata_kind { text_value };

class metadata {
public:
	metadata(metadata_kind kind)
		: kind_(kind)
	{
	}

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

private:
	std::string value_;
};
} // namespace arancini::ir
