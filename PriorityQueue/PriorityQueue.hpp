#pragma once
namespace priorityqueue {

template <typename T, typename Compare>
class priority_queue : public std::priority_queue<T, std::vector<T>, Compare> {
public:
    bool remove(const T& value) {
        auto it = std::find(this->c.begin(), this->c.end(), value);

        if (it == this->c.end()) {
            return false;
        }
        if (it == this->c.begin()) {
            this->pop();
        } else {
            this->c.erase(it);
            std::make_heap(this->c.begin(), this->c.end(), this->comp);
        }
        return true;
    }
};

}  // namespace priorityqueue