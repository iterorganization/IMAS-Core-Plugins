#ifndef PATH_MANAGER_HPP
#define PATH_MANAGER_HPP

#include <iostream>
#include <vector>
#include <string>
#include <set>
#include <unordered_map>
#include <sstream>
#include <memory>
#include <algorithm>

using namespace std;

class StructureArray {
public:
    string name;
    int start, end, step;
    bool is_unbounded;

    StructureArray(string n, int s, int e, int st, bool unbounded = false);

    bool contains(int index) const;
    set<int> getIndices() const;
    bool binary_contains(int index) const; // New method for binary search
    bool operator<(const StructureArray& other) const;
};

class PathElement {
public:
    string name;
    bool is_array;
    unique_ptr<StructureArray> array;

    PathElement(string n, bool ia = false, StructureArray* a = nullptr);
};

class PathQuery {
private:
    vector<PathElement> parsePath(const string& path_str);

public:
    vector<PathElement> elements;

    PathQuery(const string& path_str);
};

class PathManager {
private:
    vector<PathQuery> includes;
    vector<PathQuery> excludes;
    unordered_map<string, set<StructureArray>> element_cache;

    void buildElementCache();
    vector<pair<string, int>> pathToElements(const string& path) const;
    bool matchesQuery(const vector<pair<string, int>>& path_elements, 
                      const vector<PathElement>& query_elements, 
                      bool is_include) const;

public:
    PathManager(const vector<string>& inc, const vector<string>& exc);
    bool isInSet(const string& path) const;
    bool is_included(const string& array_path) const; // Updated declaration
};

#endif // PATH_MANAGER_HPP