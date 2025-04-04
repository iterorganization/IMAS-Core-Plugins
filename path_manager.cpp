#include "path_manager.h"

StructureArray::StructureArray(string n, int s, int e, int st, bool unbounded) 
    : name(n), start(s), end(e), step(st), is_unbounded(unbounded) {}

bool StructureArray::contains(int index) const {
    if (is_unbounded) {
        return index >= 0;
    }
    if (step == 0) {
        return index == start;
    }
    return index >= start && index <= end && (index - start) % step == 0;
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

// Binary search to check if an index belongs to the range
bool StructureArray::binary_contains(int index) const {
    if (is_unbounded) {
        return index >= 0;
    }
    if (step == 0) {
        return index == start;
    }
    if (index < start || index > end) {
        return false;
    }
    // Calculate the sequence length
    int count = (end - start) / step + 1;
    vector<int> indices(count);
    for (int i = 0; i < count; ++i) {
        indices[i] = start + i * step;
    }
    return binary_search(indices.begin(), indices.end(), index);
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
    
    if (includes.empty()) {
        for (const auto& exclude : excludes) {
            if (matchesQuery(path_elements, exclude.elements, false)) {
                return false;
            }
        }
        return true;
    }

    for (const auto& include : includes) {
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
                    if (!query_elem.array->binary_contains(path_elem.second)) {
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
                    if (!query_elem.array->binary_contains(path_elem.second)) {
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

bool PathManager::is_included(const string& array_path) const {
    vector<pair<string, int>> path_elements = pathToElements(array_path);
    string target_array = path_elements.back().first; // Last element is the target array
    vector<pair<string, int>> prefix_elements(path_elements.begin(), path_elements.end() - 1);

    // If includes is empty, check if the array is not fully excluded
    if (includes.empty()) {
        for (const auto& exclude : excludes) {
            if (exclude.elements.back().name == target_array && exclude.elements.back().is_array) {
                if (exclude.elements.back().array->is_unbounded) {
                    return false;
                }
            }
        }
        return true;
    }

    // Check each include query
    for (const auto& include : includes) {
        // Case 1: array_path is just the target array (e.g., "profiles_1d")
        if (path_elements.size() == 1 && path_elements[0].second == -1) {
            for (size_t i = 0; i < include.elements.size(); ++i) {
                if (include.elements[i].name == target_array && include.elements[i].is_array) {
                    auto indices = include.elements[i].array->getIndices();
                    string prefix_path;
                    for (size_t j = 0; j < i; ++j) {
                        if (j > 0) prefix_path += "/";
                        const auto& elem = include.elements[j];
                        if (elem.is_array) {
                            int index = elem.array->is_unbounded ? 0 : 
                                       (elem.array->step == 0 ? elem.array->start : elem.array->start);
                            prefix_path += elem.name + "(" + to_string(index) + ")";
                        } else {
                            prefix_path += elem.name;
                        }
                    }
                    for (int index : indices) {
                        string full_path = prefix_path.empty() ? 
                            target_array + "(" + to_string(index) + ")" :
                            prefix_path + "/" + target_array + "(" + to_string(index) + ")";
                        if (isInSet(full_path)) {
                            return true;
                        }
                    }
                }
            }
        }
        // Case 2: array_path ends with the target array as in include (e.g., "profiles_1d(1)/ion")
        else if (!include.elements.empty() && include.elements.back().name == target_array) {
            if (prefix_elements.size() <= include.elements.size() - 1) {
                bool prefix_match = true;
                for (size_t i = 0; i < prefix_elements.size(); ++i) {
                    const auto& path_elem = prefix_elements[i];
                    const auto& query_elem = include.elements[i];
                    if (path_elem.first != query_elem.name) {
                        prefix_match = false;
                        break;
                    }
                    if (query_elem.is_array) {
                        if (!query_elem.array->binary_contains(path_elem.second)) {
                            prefix_match = false;
                            break;
                        }
                    } else if (path_elem.second != -1) {
                        prefix_match = false;
                        break;
                    }
                }
                if (prefix_match) {
                    const auto& target_array_def = include.elements.back();
                    if (target_array_def.is_array) {
                        auto indices = target_array_def.array->getIndices();
                        string prefix_path;
                        for (size_t i = 0; i < prefix_elements.size(); ++i) {
                            if (i > 0) prefix_path += "/";
                            if (prefix_elements[i].second != -1) {
                                prefix_path += prefix_elements[i].first + "(" + 
                                              to_string(prefix_elements[i].second) + ")";
                            } else {
                                prefix_path += prefix_elements[i].first;
                            }
                        }
                        for (int index : indices) {
                            string full_path = prefix_path.empty() ? 
                                target_array + "(" + to_string(index) + ")" :
                                prefix_path + "/" + target_array + "(" + to_string(index) + ")";
                            if (isInSet(full_path)) {
                                return true;
                            }
                        }
                    }
                    return false; // If target_array is not an array, no indices to check
                }
            }
        }
        // Case 3: target_array is not in include, check if prefix is in E only if it matches fully
        else if (prefix_elements.size() == include.elements.size()) {
            bool prefix_match = true;
            for (size_t i = 0; i < prefix_elements.size(); ++i) {
                const auto& path_elem = prefix_elements[i];
                const auto& query_elem = include.elements[i];
                if (path_elem.first != query_elem.name) {
                    prefix_match = false;
                    break;
                }
                if (query_elem.is_array) {
                    if (!query_elem.array->binary_contains(path_elem.second)) {
                        prefix_match = false;
                        break;
                    }
                } else if (path_elem.second != -1) {
                    prefix_match = false;
                    break;
                }
            }
            if (prefix_match) {
                string prefix_path;
                for (size_t i = 0; i < prefix_elements.size(); ++i) {
                    if (i > 0) prefix_path += "/";
                    if (prefix_elements[i].second != -1) {
                        prefix_path += prefix_elements[i].first + "(" + 
                                      to_string(prefix_elements[i].second) + ")";
                    } else {
                        prefix_path += prefix_elements[i].first;
                    }
                }
                if (isInSet(prefix_path)) {
                    return true;
                }
            }
        }
    }
    return false;
}