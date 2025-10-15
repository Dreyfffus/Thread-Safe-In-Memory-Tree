#Thread Safe In Memory Tree

This is a project for the JetBrains YouTrackDB internship position. It is a thread safe binary tree that implements 2 methods:
`ConcurrentTree::get()` and `ConcurrentTree::put()`.

##Implementation
- `sync::Node` : This is the struct that represents the data in the in-memory tree. It stores key : value pairs as dynamic arrays of bytes. Thread safety comes from use of atomic pointers and Node locking through `std::mutex` and version checking.
- `sync::ConcurrentTree` : this is the class of the data structure, implemented as a Red Black Tree with an owning pointer to the root of the tree. The tree is built from `sync::Node`s.
- `sync::ConcurrentTree::get(key)` : this is an optimistic read algorithm that returns the value of the Node with the key specified, or `NULL_VALUE' if the key could not be found. It may be prone to delay if concurrent since it restarts the read if the current tree path is being accesed by other threads.
- `sync::ConcurrentTree::put(key, value)` : it uses optimistic traversal to find the insertion place of the key value pair. If the key of the new value is the same, it replaces the value of the existing node. After insertion it rebalances the tree ensuring thread safety.
- `sync::ConcurrentTree::printList()` : Prints the values currently in the tree inOrder.
- `sync::ConcurrentTree::printTree()` : Prints the values currently in the tree in a Tree Format.

##How to Build
This is a Visual Studio C++ project. The project is built into a .sln project using **premake5**. The build scripts for the project are found in `Scripts/Setup-Windows.bat` for Windows platforms and `Scripts/Setup-Linux.sh` for Linux and Mac. Premake binaries are provided in the project. The project space includes 2 different projects: `ConcurrentTree`, which builds into a static library and contains the source code for the data structure and `ConcurrentTest` which is Test project that compiles into a .dll referencing `ConcurrentTree` containing unit tests for the data structure.