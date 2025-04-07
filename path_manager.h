#ifndef PATH_MANAGER_H
#define PATH_MANAGER_H 1

#include <iostream>
#include <vector>
#include <string>
#include <set>
#include <unordered_map>
#include <sstream>
#include <memory>
#include <algorithm>

using namespace std;

/**
 * Definition of the E space:
 * The E space represents the set of all valid paths as defined by the "include" rules 
 * minus the paths explicitly removed by the "exclude" rules. A path is considered 
 * to be in E if it matches at least one include query and does not match any exclude 
 * query. This concept is used by PathManager to determine whether a specific path 
 * or array is included in the managed set of paths.
 */

/**
 * @class StructureArray
 * @brief Represents an array-like structure with a name and index range.
 * 
 * This class defines an array element within a path, which can have a bounded range 
 * (e.g., 1:5:2) or be unbounded (e.g., (:)). It provides methods to check if an index 
 * belongs to the array and to retrieve its valid indices.
 */
class StructureArray {
public:
    string name;           // Name of the array (e.g., "profiles_1d", "ion").
    int start, end, step;  // Range parameters: start index, end index, and step size.
    bool is_unbounded;     // True if the array is unbounded (e.g., (:)), false otherwise.

    /**
     * @brief Constructs a StructureArray with specified parameters.
     * @param n Name of the array.
     * @param s Starting index of the range.
     * @param e Ending index of the range (inclusive).
     * @param st Step size between indices.
     * @param unbounded Indicates if the array has no bounds (default: false).
     */
    StructureArray(string n, int s, int e, int st, bool unbounded = false);

    /**
     * @brief Checks if a given index is within the array's range.
     * @param index The index to check.
     * @return True if the index is valid within the array's range, false otherwise.
     */
    bool contains(int index) const;

    /**
     * @brief Retrieves all valid indices in the array's range.
     * @return A set of integers representing all valid indices. For unbounded arrays, returns {-1}.
     */
    set<int> getIndices() const;

    /**
     * @brief Performs a binary search to check if an index is in the array's range.
     * @param index The index to check.
     * @return True if the index is within the range, false otherwise.
     * @note Optimized for bounded arrays; for unbounded arrays, checks if index >= 0.
     */
    bool binary_contains(int index) const;

    /**
     * @brief Comparison operator for sorting StructureArray objects.
     * @param other The other StructureArray to compare with.
     * @return True if this array is less than the other based on name and range.
     */
    bool operator<(const StructureArray& other) const;
};

/**
 * @class PathElement
 * @brief Represents a single component in a path, which may or may not be an array.
 * 
 * This class encapsulates an element of a path, such as "profiles_1d(1)" or "ion", 
 * and indicates whether it is an array with associated range information.
 */
class PathElement {
public:
    string name;                       // Name of the path element (e.g., "profiles_1d").
    bool is_array;                     // True if the element is an array, false if not.
    unique_ptr<StructureArray> array;  // Pointer to the array structure if is_array is true.

    /**
     * @brief Constructs a PathElement.
     * @param n Name of the element.
     * @param ia Indicates if the element is an array (default: false).
     * @param a Pointer to the StructureArray if it is an array (default: nullptr).
     */
    PathElement(string n, bool ia = false, StructureArray* a = nullptr);
};

/**
 * @class PathQuery
 * @brief Represents a parsed query string defining a set of paths.
 * 
 * This class parses a string like "profiles_1d(1:5:2)/ion(1:4:2)" into a sequence 
 * of PathElements, which can then be used to match specific paths.
 */
class PathQuery {
private:
    /**
     * @brief Parses a path string into a vector of PathElements.
     * @param path_str The string to parse (e.g., "profiles_1d(1:5:2)/ion").
     * @return A vector of PathElements representing the path structure.
     */
    vector<PathElement> parsePath(const string& path_str);

public:
    vector<PathElement> elements;  // List of elements in the parsed path query.

    /**
     * @brief Constructs a PathQuery from a string.
     * @param path_str The path string to parse into elements.
     */
    PathQuery(const string& path_str);
};

/**
 * @class PathManager
 * @brief Manages a set of paths defined by include and exclude rules, determining membership in E.
 * 
 * This class takes a set of include and exclude path queries and provides methods 
 * to check if specific paths or arrays are in the E space. It uses a cache to 
 * optimize lookups for array elements.
 */
class PathManager {
private:
    vector<PathQuery> includes;        // List of include rules defining the base set E.
    vector<PathQuery> excludes;        // List of exclude rules removing elements from E.
    unordered_map<string, set<StructureArray>> element_cache;  // Cache of array elements for optimization.

    /**
     * @brief Builds a cache of array elements from include and exclude queries.
     * 
     * Populates the element_cache with all array elements encountered in the 
     * include and exclude rules, enabling faster lookups during path matching.
     */
    void buildElementCache();

    /**
     * @brief Converts a path string into a vector of name-index pairs.
     * @param path The path string to convert (e.g., "profiles_1d(1)/ion(3)").
     * @return A vector of pairs where each pair contains a name and its index (-1 if not an array).
     */
    vector<pair<string, int>> pathToElements(const string& path) const;

    /**
     * @brief Checks if a path matches a query (include or exclude).
     * @param path_elements The parsed elements of the path to check.
     * @param query_elements The elements of the query to match against.
     * @param is_include True if matching an include query, false for exclude.
     * @return True if the path matches the query, false otherwise.
     */
    bool matchesQuery(const vector<pair<string, int>>& path_elements, 
                      const vector<PathElement>& query_elements, 
                      bool is_include) const;

public:
    /**
     * @brief Constructs a PathManager with include and exclude rules.
     * @param inc Vector of include path strings defining the initial set E.
     * @param exc Vector of exclude path strings removing elements from E.
     */
    PathManager(const vector<string>& inc, const vector<string>& exc);

    /**
     * @brief Checks if a specific path is in the E space.
     * @param path The path string to check (e.g., "profiles_1d(1)/ion(3)").
     * @return True if the path is in E (matches an include and no excludes), false otherwise.
     */
    bool isInSet(const string& path) const;

    /**
     * @brief Checks if an array path has at least one element in the E space.
     * @param array_path The array path to check (e.g., "profiles_1d(1)/ion").
     * @return True if there exists at least one path extending array_path that is in E, false otherwise.
     */
    bool is_included(const string& array_path) const;
};

#endif // PATH_MANAGER_HPP