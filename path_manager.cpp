#include "path_manager.h"

StructureArray::StructureArray(string n, int s, int e, int st, bool unbounded) 
    : name(n), start(s), end(e), step(st), is_unbounded(unbounded) {}

bool StructureArray::contains(int index) const {
    if (is_unbounded) {
        return index >= 0; // Accept all non-negative indices
    }
    if (step == 0) {
        return index == start; // Special case: step = 0, only start index is valid
    }
    return index >= start && index <= end && 
           (index - start) % step == 0;
}

set<int> StructureArray::getIndices() const {
    set<int> indices;
    if (is_unbounded) {
        for (int i = 0; i <= 100; ++i) {
            indices.insert(i);
        }
    } else if (step == 0) {
        indices.insert(start);
    } else {
        for (int i = start; i <= end; i += step) {
            indices.insert(i);
        }
    }
    return indices;
}

bool StructureArray::operator<(const StructureArray& other) const {
    if (name != other.name) return name < other.name;
    if (is_unbounded != other.is_unbounded) return is_unbounded < other.is_unbounded;
    if (start != other.start) return start < other.start;
    if (end != other.end) return end < other.end;
    return step < other.step;
}

PathElement::PathElement(string n, bool ia, StructureArray* a) 
    : name(n), is_array(ia), array(a ? new StructureArray(*a) : nullptr) {}

vector<PathElement> PathQuery::parsePath(const string& path_str) {
    vector<PathElement> elements;
    stringstream ss(path_str);
    string part;
    
    while (getline(ss, part, '/')) {
        if (part.find('(') != string::npos) {
            string name = part.substr(0, part.find('('));
            string indices_str = part.substr(part.find('(') + 1, part.find(')') - part.find('(') - 1);
            
            if (indices_str == ":") {
                elements.emplace_back(name, true, new StructureArray(name, 0, 0, 1, true));
            } else {
                vector<int> indices;
                stringstream iss(indices_str);
                string num;
                while (getline(iss, num, ':')) {
                    indices.push_back(stoi(num));
                }
                elements.emplace_back(name, true, 
                    new StructureArray(name, indices[0], indices[1], indices[2]));
            }
        } else {
            elements.emplace_back(part);
        }
    }
    return elements;
}

PathQuery::PathQuery(const string& path_str) {
    elements = parsePath(path_str);
}

void PathManager::buildElementCache() {
    for (const auto& query : includes) {
        for (const auto& elem : query.elements) {
            if (elem.is_array) {
                element_cache[elem.name].insert(*elem.array);
            }
        }
    }
    for (const auto& query : excludes) {
        for (const auto& elem : query.elements) {
            if (elem.is_array) {
                element_cache[elem.name].insert(*elem.array);
            }
        }
    }
}

vector<pair<string, int>> PathManager::pathToElements(const string& path) const {
    vector<pair<string, int>> elements;
    stringstream ss(path);
    string part;
    
    while (getline(ss, part, '/')) {
        if (part.find('(') != string::npos) {
            string name = part.substr(0, part.find('('));
            int index = stoi(part.substr(part.find('(') + 1, part.find(')') - part.find('(') - 1));
            elements.push_back({name, index});
        } else {
            elements.push_back({part, -1});
        }
    }
    return elements;
}

bool PathManager::matchesQuery(const vector<pair<string, int>>& path_elements, 
                               const vector<PathElement>& query_elements, 
                               bool is_include) const {
    if (is_include && path_elements.size() > query_elements.size()) return false;
    if (!is_include && path_elements.size() < query_elements.size()) return false;

    for (size_t i = 0; i < min(path_elements.size(), query_elements.size()); ++i) {
        if (query_elements[i].name != path_elements[i].first) return false;
        if (query_elements[i].is_array && 
            !query_elements[i].array->contains(path_elements[i].second)) return false;
        if (!query_elements[i].is_array && path_elements[i].second != -1) return false;
    }

    return true;
}

PathManager::PathManager(const vector<string>& inc, const vector<string>& exc) {
    for (const auto& q : inc) includes.emplace_back(q);
    for (const auto& q : exc) excludes.emplace_back(q);
    buildElementCache();
}

bool PathManager::isInSet(const string& path) const {
    vector<pair<string, int>> path_elements = pathToElements(path);
    
    // If includes is empty, all paths are included by default unless excluded
    if (includes.empty()) {
        for (const auto& exclude : excludes) {
            if (matchesQuery(path_elements, exclude.elements, false)) {
                return false;
            }
        }
        return true;
    }

    // Check each include query
    for (const auto& include : includes) {
        // If path is shorter or equal, check for prefix match
        if (path_elements.size() <= include.elements.size()) {
            bool match = true;
            for (size_t i = 0; i < path_elements.size(); ++i) {
                const auto& path_elem = path_elements[i];
                const auto& query_elem = include.elements[i];
                if (path_elem.first != query_elem.name) {
                    match = false;
                    break;
                }
                if (query_elem.is_array) {
                    if (!query_elem.array->contains(path_elem.second)) {
                        match = false;
                        break;
                    }
                } else if (path_elem.second != -1) {
                    match = false;
                    break;
                }
            }
            if (match) {
                for (const auto& exclude : excludes) {
                    if (matchesQuery(path_elements, exclude.elements, false)) {
                        return false;
                    }
                }
                return true;
            }
        }
        // If path is longer, check if it extends a valid prefix
        else {
            bool prefix_match = true;
            for (size_t i = 0; i < include.elements.size(); ++i) {
                const auto& path_elem = path_elements[i];
                const auto& query_elem = include.elements[i];
                if (path_elem.first != query_elem.name) {
                    prefix_match = false;
                    break;
                }
                if (query_elem.is_array) {
                    if (!query_elem.array->contains(path_elem.second)) {
                        prefix_match = false;
                        break;
                    }
                } else if (path_elem.second != -1) {
                    prefix_match = false;
                    break;
                }
            }
            if (prefix_match) {
                for (const auto& exclude : excludes) {
                    if (matchesQuery(path_elements, exclude.elements, false)) {
                        return false;
                    }
                }
                return true;
            }
        }
    }

    return false;
}

bool PathManager::is_included(const string& array_name) const {
    // If includes is empty, all arrays are included unless fully excluded
    if (includes.empty()) {
        for (const auto& exclude : excludes) {
            for (const auto& elem : exclude.elements) {
                if (elem.name == array_name && elem.is_array && elem.array->is_unbounded) {
                    return false; // Fully excluded by an unbounded exclude like t(:)
                }
            }
        }
        return true; // Default inclusion unless fully excluded
    }

    // Check each include query for the array_name at any position
    for (const auto& include : includes) {
        bool array_found = false;
        string test_path;
        
        // Build a test path with the first valid index for each array
        for (size_t i = 0; i < include.elements.size(); ++i) {
            if (i > 0) test_path += "/";
            const auto& elem = include.elements[i];
            if (elem.is_array) {
                int index = elem.array->is_unbounded ? 0 : 
                           (elem.array->step == 0 ? elem.array->start : elem.array->start);
                test_path += elem.name + "(" + to_string(index) + ")";
            } else {
                test_path += elem.name;
            }
            if (elem.name == array_name) {
                array_found = true;
            }
        }

        // If array_name is in the path, check if the test path is in E
        if (array_found && isInSet(test_path)) {
            return true;
        }

        // If array_name is an array in the include, check its direct inclusion
        for (size_t i = 0; i < include.elements.size(); ++i) {
            if (include.elements[i].name == array_name && include.elements[i].is_array) {
                const auto& array = include.elements[i].array;
                if (array->is_unbounded) {
                    bool fully_excluded = false;
                    for (const auto& exclude : excludes) {
                        if (exclude.elements[0].name == array_name && exclude.elements[0].is_array) {
                            if (exclude.elements[0].array->is_unbounded) {
                                fully_excluded = true;
                                break;
                            }
                        }
                    }
                    if (!fully_excluded) return true;
                } else if (array->step == 0) {
                    string path = array_name + "(" + to_string(array->start) + ")";
                    if (isInSet(path)) return true;
                } else {
                    int test_index = array->start;
                    string path = array_name + "(" + to_string(test_index) + ")";
                    if (isInSet(path)) return true;
                    bool fully_excluded = false;
                    for (const auto& exclude : excludes) {
                        if (exclude.elements[0].name == array_name && exclude.elements[0].is_array) {
                            const auto& ex_array = exclude.elements[0].array;
                            if (!ex_array->is_unbounded && 
                                ex_array->start <= array->start && 
                                ex_array->end >= array->end && 
                                ex_array->step <= array->step) {
                                fully_excluded = true;
                                break;
                            }
                        }
                    }
                    if (!fully_excluded) return true;
                }
            }
        }
    }
    return false;
}