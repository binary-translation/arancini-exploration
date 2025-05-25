#pragma once

#include <memory>
#include <string>
#include <vector>

enum idl_ast_node_type {
    IANT_ROOT,
    IANT_DEFS,
    IANT_LIBDEF,
    IANT_CCDEF,
    IANT_FNDEF,
    IANT_TYPEDEF,
    IANT_PARAMS,
    IANT_PARAM,
    IANT_ATTRS,
    IANT_ATTR,
    IANT_NONE
};

struct idl_ast_node {
    idl_ast_node_type ty{IANT_NONE};
    std::vector<idl_ast_node> children;
    int nr_children{};

    std::string value;
    int is_const{};
    int width{};
    int tc{};

    explicit idl_ast_node(idl_ast_node_type ty) : ty(ty) {}

    idl_ast_node() = default;

    [[maybe_unused]] void add_child(idl_ast_node &&child) {
        children.push_back(std::move(child));
        nr_children++;
    }
};