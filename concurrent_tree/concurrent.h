#pragma once
#define _SILENCE_CXX20_OLD_SHARED_PTR_ATOMIC_SUPPORT_DEPRECATION_WARNING

#include <iostream>
#include <vector>
#include <span>
#include <mutex>
#include <functional>
#include <algorithm>
#include <new>
#include <variant>

inline std::string parse(std::vector<unsigned char> data) {
    return std::string(data.begin(), data.end());
}

inline std::vector<unsigned char> data(std::string key) {
    return std::vector<unsigned char>(key.begin(), key.end());
}

namespace sync {

    constexpr uint8_t BLACK = 0;
    constexpr uint8_t RED = 1;

    struct alignas(64) Node {
        const std::vector<unsigned char> key;
        std::shared_ptr<const std::vector<unsigned char>> value;
        std::atomic<Node*> left;
        std::atomic <Node*> right;
        std::atomic<Node*> parent;
        std::atomic<uint64_t> version;
        std::atomic<uint8_t> color;
        std::mutex lock;
        Node(std::vector<unsigned char> key, std::vector<unsigned char> value,
            Node* _left = nullptr, Node* _right = nullptr, Node* _parent = nullptr,
            uint64_t _version = 0, uint8_t _color = RED) : value(std::make_shared<const std::vector<unsigned char>>(std::move(value))), key(std::move(key)) {
            left.store(_left, std::memory_order_relaxed);
            right.store(_right, std::memory_order_relaxed);
            parent.store(_parent, std::memory_order_relaxed);
            version.store(_version, std::memory_order_relaxed);
            color.store(_color, std::memory_order_relaxed);
        };
    };

    class ConcurrentTree {
    public:
        const std::vector<unsigned char> NULL_VALUE{ {} };
        ConcurrentTree() : root(nullptr) {};
        ~ConcurrentTree() { deleteTree(root.load(std::memory_order_acquire)); }
        void put(const std::vector<unsigned char>& key, const std::vector<unsigned char>& value);
        std::vector<unsigned char> get(std::vector<unsigned char> key);
        void printList();
        void printTree();
    private:
        std::atomic<Node*> root{nullptr};

        static inline void begin_write(Node* node) {
            if (!node) return;
            node->version.fetch_add(1, std::memory_order_acq_rel);
        }

        static inline void end_write(Node* node) {
            if (!node) return;
            node->version.fetch_add(1, std::memory_order_release);
        }

        static void lock_adress_order(std::vector<Node*>& nodes) {
            nodes.erase(std::remove(nodes.begin(), nodes.end(), nullptr), nodes.end());
            std::sort(nodes.begin(), nodes.end(), std::less<Node*>{});
            nodes.erase(std::unique(nodes.begin(), nodes.end()), nodes.end());
            for (Node* node : nodes) node->lock.lock();
        }

        static void unlock_reverse(std::vector<Node*>& nodes) {
            for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
                (*it)->lock.unlock();
            }
        }

        static inline uint8_t load_color(Node* node) {
            return node ? node->color.load(std::memory_order_relaxed) : BLACK;
        }

        void left_rotate(Node* node_root);
        void right_rotate(Node* node_root);
        void fixInsert(Node* node_leaf);
        void deleteTree(Node* node);
        void printHelper(Node* root, bool last, std::string indent = "");
        template <class Visitor, class... Args>
        void inOrder(Node* node, Visitor&& visitor, Args&&... args) const;
    };
}