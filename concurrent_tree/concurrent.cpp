#include "concurrent.h"

namespace sync {
    void ConcurrentTree::left_rotate(Node* node) {
        while (true) {
            Node* first = node->right.load(std::memory_order_relaxed);
            if (!first) return; // No value to rotate
            Node* second = first->left.load(std::memory_order_relaxed);
            Node* grandparent = node->parent.load(std::memory_order_relaxed);

            // This is the set that will mutate under the rotation, which we lock from other concurrent threads.
            // left needs be included only if we will mutate its parent.
            std::vector<Node*> locked = { grandparent, node, first };
            if (second) locked.push_back(second);
            lock_adress_order(locked);

            begin_write(node);
            begin_write(first);
            begin_write(grandparent); // if they exist
            begin_write(second); // if they exist

            // Nodes still valid, grandparent is still related to node.
            bool valid = (node->right.load(std::memory_order_relaxed) == first)
                && (first->left.load(std::memory_order_relaxed) == second)
                && (node->parent.load(std::memory_order_relaxed) == grandparent)
                && (!grandparent || grandparent->left.load(std::memory_order_relaxed) == node || grandparent->right.load(std::memory_order_relaxed) == node);

            // Rollback if the state of write is invalid, i.e. another thread beat this one to execution.
            if (!valid) {
                end_write(second);
                end_write(grandparent);
                end_write(first);
                end_write(node);
                unlock_reverse(locked);
                continue;
            }

            // Left rotation performed:
            node->right.store(second, std::memory_order_relaxed);
            if (second) second->parent.store(node, std::memory_order_relaxed);
            first->parent.store(grandparent, std::memory_order_relaxed);
            if (!grandparent) root.store(first, std::memory_order_release);
            else if (grandparent->left.load(std::memory_order_relaxed) == node) grandparent->left.store(first, std::memory_order_relaxed);
            else grandparent->right.store(first, std::memory_order_release);
            first->left.store(node, std::memory_order_relaxed);
            node->parent.store(first, std::memory_order_relaxed);

            // Finish writes and release nodes. Order does not matter, here its done inside-out
            end_write(second);
            end_write(grandparent);
            end_write(first);
            end_write(node);
            unlock_reverse(locked);
            return;
        }
    }

    void ConcurrentTree::right_rotate(Node* node) {
        while (true) {
            Node* first = node->left.load(std::memory_order_relaxed);
            if (!first) return; // No value to rotate
            Node* second = first->right.load(std::memory_order_relaxed);
            Node* grandparent = node->parent.load(std::memory_order_relaxed);

            // This is the set that will mutate under the rotation, which we lock from other concurrent threads.
            // left needs be included only if we will mutate its parent.
            std::vector<Node*> locked = { grandparent, node, first };
            if (second) locked.push_back(second);
            lock_adress_order(locked);

            begin_write(node);
            begin_write(first);
            begin_write(grandparent); // if they exist
            begin_write(second); // if they exist

            // Nodes still valid, grandparent is still related to node.
            bool valid = (node->left.load(std::memory_order_relaxed) == first)
                && (first->right.load(std::memory_order_relaxed) == second)
                && (node->parent.load(std::memory_order_relaxed) == grandparent)
                && (!grandparent || grandparent->left.load(std::memory_order_relaxed) == node || grandparent->right.load(std::memory_order_relaxed) == node);

            // Rollback if the state of write is invalid, i.e. another thread beat this one to execution.
            if (!valid) {
                end_write(second);
                end_write(grandparent);
                end_write(first);
                end_write(node);
                unlock_reverse(locked);
                continue;
            }

            // Right rotation performed:
            node->left.store(second, std::memory_order_relaxed);
            if (second) second->parent.store(node, std::memory_order_relaxed);
            first->parent.store(grandparent, std::memory_order_relaxed);
            if (!grandparent) root.store(first, std::memory_order_release);
            else if (grandparent->left.load(std::memory_order_relaxed) == node) grandparent->left.store(first, std::memory_order_relaxed);
            else grandparent->right.store(first, std::memory_order_release);
            first->right.store(node, std::memory_order_relaxed);
            node->parent.store(first, std::memory_order_relaxed);

            // Finish writes and release nodes. Order does not matter, here its done inside-out
            end_write(second);
            end_write(grandparent);
            end_write(first);
            end_write(node);
            unlock_reverse(locked);
            return;
        }
    }

    std::vector<unsigned char> ConcurrentTree::get(std::vector<unsigned char> key) {
        while (Node* current = root.load(std::memory_order_acquire)) {
            // Iterate over the tree with optimistic search
            while (true) {
                // If touched tree edge with no match return NULL
                if (!current) return NULL_VALUE;
                uint64_t version_first = current->version.load(std::memory_order_acquire);
                // Other thread acting on the node, reset to root
                if (version_first & 1u) break;
                Node* next;
                if (current->key == key) {
                    auto value = std::atomic_load(&current->value);
                    uint64_t version_second = current->version.load(std::memory_order_acquire);
                    if (version_first == version_second && !(version_second & 1u)) return *value;
                    continue;
                }
                else if (current->key < key) next = current->right.load(std::memory_order_relaxed);
                else next = current->left.load(std::memory_order_relaxed);
                uint64_t version_second = current->version.load(std::memory_order_acquire);
                // Other tree raced, reset to root
                if (version_first != version_second || version_second & 1u) break;
                // Iterate
                current = next;
            }
        }
        // If tree is uninitialized, first while will eval to false, therefore we return NULL.
        return NULL_VALUE;
    }

    void ConcurrentTree::put(const std::vector<unsigned char>& key, const std::vector<unsigned char>& value) {
        // For this to work, we have to keep the value and key of the node immutable.
        Node* node = new Node(key, value);

        while (true) {
            // Empty Tree
            Node* _root = root.load(std::memory_order_acquire);
            if (!_root) {
                if (root.compare_exchange_strong(_root, node, std::memory_order_release, std::memory_order_acquire)) {
                    node->color.store(BLACK, std::memory_order_relaxed);
                    return;
                }
                continue; // Other thread raced.
            }

            Node* current = _root;
            Node* parent = nullptr;
            bool go_right = false;

            // Optimistic search, this loop will iterate through the tree until the place of insertion and reset to root
            // when someone else raced.
            while (true) {
                uint64_t version_first = current->version.load(std::memory_order_acquire);
                // Write in process from other thread
                if (version_first & 1u) {
                    current = root.load(std::memory_order_acquire);
                    if (!current) break;
                    parent = nullptr;
                    continue;
                }
                if (current->key == key) { // If key is the same, replace value.
                    auto new_value = std::make_shared<const std::vector<unsigned char>>(value);
                    std::unique_lock<std::mutex> lock(current->lock); // serialize writers on this node.
                    begin_write(current);
                    std::atomic_store(&current->value, new_value);
                    end_write(current);
                    return;
                }
                parent = current;
                go_right = current->key < key;
                Node* next = go_right ? current->right.load(std::memory_order_acquire) : current->left.load(std::memory_order_acquire);

                uint64_t version_second = current->version.load(std::memory_order_acquire);
                if (version_first != version_second || version_second & 1) { // Some thread raced, changing versions.
                    current = root.load(std::memory_order_acquire);
                    parent = nullptr;
                    continue;
                }
                if (!next) break; // found insertion parent.
                current = next;
            }
            if (!parent) continue; // Tree changed; Retry the operation.

            std::unique_lock<std::mutex> lock(parent->lock);
            begin_write(parent);

            Node* observed = go_right ? parent->right.load(std::memory_order_acquire) : parent->left.load(std::memory_order_acquire);
            if (observed != nullptr) { // lost race for the node.
                end_write(parent);
                lock.unlock();
                continue;
            }

            node->parent.store(parent, std::memory_order_relaxed);
            if (go_right) parent->right.store(node, std::memory_order_relaxed);
            else parent->left.store(node, std::memory_order_relaxed);
            end_write(parent);
            lock.unlock();
            fixInsert(node); // Rebalancing of RED / BLACK tree after insertion.
            return;
        }
    }

    void ConcurrentTree::fixInsert(Node* node) {
        while (true) {
            Node* parent = node->parent.load(std::memory_order_relaxed);
            if (!parent || parent->color.load(std::memory_order_relaxed) == BLACK) break;

            Node* grandparent = parent->parent.load(std::memory_order_relaxed);
            if (!grandparent) break;

            bool parent_is_left = (grandparent->left.load(std::memory_order_relaxed) == parent);
            Node* uncle = parent_is_left
                ? grandparent->right.load(std::memory_order_relaxed)
                : grandparent->left.load(std::memory_order_relaxed);

            // Case I: recolor if uncle is RED
            if (uncle && uncle->color.load(std::memory_order_relaxed) == RED) {
                std::vector<Node*> locked{ grandparent, parent, uncle };
                lock_adress_order(locked);

                begin_write(grandparent);
                begin_write(parent);
                begin_write(uncle);

                bool valid = (parent->parent.load(std::memory_order_relaxed) == grandparent)
                    && ((grandparent->left.load(std::memory_order_relaxed) == parent)
                        || (grandparent->right.load(std::memory_order_relaxed) == parent))
                    && (uncle == (parent_is_left ? grandparent->right.load(std::memory_order_relaxed) : grandparent->left.load(std::memory_order_relaxed)))
                    && (parent->color.load(std::memory_order_relaxed) == RED)
                    && (uncle->color.load(std::memory_order_relaxed) == RED);

                if (valid) {
                    parent->color.store(BLACK, std::memory_order_relaxed);
                    uncle->color.store(BLACK, std::memory_order_relaxed);
                    grandparent->color.store(RED, std::memory_order_relaxed);
                }

                end_write(uncle);
                end_write(parent);
                end_write(grandparent);
                unlock_reverse(locked);

                if (valid) {
                    node = grandparent;
                    continue;
                }
                else continue; // retry this step
            }

            // Case II : Uncle BLACK, left / right rotations
            if (parent_is_left) {
                if (parent->right.load(std::memory_order_relaxed) == node) {
                    left_rotate(parent);// Left Rotation around parent
                    node = parent;
                    parent = node->parent.load(std::memory_order_relaxed);
                    grandparent = parent ? parent->parent.load(std::memory_order_relaxed) : nullptr;
                    if (!parent || !grandparent) break;
                }
                right_rotate(grandparent);// Right Rotate around grand parent

                std::vector<Node*> locked{ parent, grandparent };
                lock_adress_order(locked);
                begin_write(parent);
                begin_write(grandparent);

                bool valid = (grandparent->parent.load(std::memory_order_relaxed) == parent)
                    && ((parent->left.load(std::memory_order_relaxed) == grandparent) ||
                        (parent->right.load(std::memory_order_relaxed) == grandparent));

                if (valid) {
                    parent->color.store(BLACK, std::memory_order_relaxed);
                    grandparent->color.store(RED, std::memory_order_relaxed);
                }

                end_write(grandparent);
                end_write(parent);
                unlock_reverse(locked);

                if (!valid) continue;
            }
            else {
                if (parent->left.load(std::memory_order_relaxed) == node) {
                    right_rotate(parent);
                    node = parent;
                    parent = node->parent.load(std::memory_order_relaxed);
                    grandparent = parent ? parent->parent.load(std::memory_order_relaxed) : nullptr;
                    if (!parent || !grandparent) break;
                }
                left_rotate(grandparent);

                std::vector<Node*> locked{ parent, grandparent };
                lock_adress_order(locked);
                begin_write(parent);
                begin_write(grandparent);

                bool valid = (grandparent->parent.load(std::memory_order_relaxed) == parent)
                    && ((parent->left.load(std::memory_order_relaxed) == grandparent)
                        || (parent->right.load(std::memory_order_relaxed) == grandparent));

                if (valid) {
                    parent->color.store(BLACK, std::memory_order_relaxed);
                    grandparent->color.store(RED, std::memory_order_relaxed);
                }

                end_write(grandparent);
                end_write(parent);
                unlock_reverse(locked);

                if (!valid) continue;
            }

            break; // Rotation path finished
        }

        // Root must be BLACK
        Node* _root = root.load(std::memory_order_acquire);
        if (_root) {
            std::unique_lock<std::mutex> locked(_root->lock);
            begin_write(_root);
            _root->color.store(BLACK, std::memory_order_relaxed);
            end_write(_root);
        }
    }
    void ConcurrentTree::deleteTree(Node* node) {
        if (node != nullptr) {
            deleteTree(node->left);
            deleteTree(node->right);
            delete node;
        }
    }

    void ConcurrentTree::printHelper(Node* root, bool last, std::string indent)
    {
        if (root != nullptr) {
            std::cout << indent;
            if (last) {
                std::cout << "R----";
                indent += "     ";
            }
            else {
                std::cout << "L----";
                indent += "|    ";
            }
            std::string sColor
                = (root->color == RED) ? "RED" : "BLACK";
            std::cout << parse(root->key) << " : " << parse(*(root->value)) << "(" << sColor << ")"
                << std::endl;
            printHelper(root->left, false, indent);
            printHelper(root->right, true, indent);
        }
    }

    void ConcurrentTree::printTree() {
        if (root == nullptr) {
            std::cout << "empty";
        }
        else {
            printHelper(root, true);
        }
    }

    void ConcurrentTree::printList() {
        inOrder(root, [&](const auto& k, const auto& v) {
            std::cout << parse(k) << " : " << parse(v) << '\n';
        });
    }

    template <typename Visitor, class... Args> 
    void ConcurrentTree::inOrder(Node* node, Visitor&& visit, Args&&... args) const {
        if (!node) return;
        inOrder(node->left, visit, args...);
        std::invoke(visit, node->key, *(node->value), args...);
        inOrder(node->right, visit, args...);
    }
}

