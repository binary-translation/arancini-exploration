#pragma once
#include <forward_list>
#include <iostream>
#include <vector>

template <class Base> class Destruct_Deleter {
  public:
    void operator()(Base *b) { b->~Base(); }
};

template <class Base> class Allocator {

  public:
    Allocator() {
        // Don't use make_unique to prevent zero-init
        std::unique_ptr<char[]> args =
            std::unique_ptr<char[]>(new char[ALLOCATION_SIZE]);
        pages_.emplace_front(std::move(args));
        current_page_ = pages_.front().get();
        limit_ = current_page_ + ALLOCATION_SIZE;
    }

    template <class T, typename = std::enable_if_t<std::is_base_of_v<Base, T>>>
    char *allocate() {
        if (current_page_ + sizeof(T) <= limit_) {
            char *ret = current_page_;
            current_page_ += sizeof(T);
            objects_.emplace_back((Base *)ret);
            return ret;
        } else if (sizeof(T) <= ALLOCATION_SIZE) {
            // New page
            // Don't use make_unique to prevent zero-init
            std::unique_ptr<char[]> args =
                std::unique_ptr<char[]>(new char[ALLOCATION_SIZE]);
            pages_.emplace_front(std::move(args));
            current_page_ = pages_.front().get();
            limit_ = current_page_ + ALLOCATION_SIZE;
            char *ret = current_page_;
            current_page_ += sizeof(T);
            objects_.emplace_back((Base *)ret);
            return ret;
        } else {
            return nullptr;
        }
    }

  private:
    static constexpr const int ALLOCATION_SIZE = 10200;

    std::forward_list<std::unique_ptr<char[]>> pages_;

    std::vector<std::unique_ptr<Base, Destruct_Deleter<Base>>> objects_;
    char *current_page_;
    char *limit_;
};
