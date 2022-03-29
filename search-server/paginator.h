#pragma once
#include <iostream>
#include <vector>

using std::ostream;
using std::vector;

template<typename Iterator>
class IteratorRange{
public:
    IteratorRange(Iterator begin) 
    : begin_(begin) 
    {}

    IteratorRange(Iterator begin, Iterator end) 
    : begin_(begin), end_(end) 
    {}

    IteratorRange(Iterator begin, Iterator end, int size) 
    : begin_(begin), end_(end), size_(size) 
    {}
    
    auto begin() const {
        return begin_;
    }
    
    auto end() const {
        return end_;
    }
    int size() const {
        return size_;
    }
    
private:
    Iterator begin_;
    Iterator end_;
    int size_ = 0;
};

template<typename Iterator>
ostream& operator<<(ostream& out, const IteratorRange<Iterator>& doc) {
    for (auto iterator = doc.begin(); iterator != doc.end(); ++iterator) { 
        out << *iterator; 
    }
    return out;
}

template<typename Iterator>
class Paginator {
public:
    Paginator(Iterator range_begin, Iterator range_end, int page_size){
        for (auto left = distance(range_begin, range_end); left > 0;) {
            const auto current_page_size = min(page_size, static_cast<int>(left));
            const Iterator current_page_end = next(range_begin, current_page_size);
            pages_.push_back({range_begin, current_page_end});
            left -= current_page_size;
            range_begin = current_page_end;
        }             
    }
    
    auto begin() const {
        return pages_.begin();
    }
    auto end() const {
        return pages_.end();
    }
    
private:
    vector<IteratorRange<Iterator>> pages_;    
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}