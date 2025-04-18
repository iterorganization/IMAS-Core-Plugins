#include "path_manager.h"

StructureArray::StructureArray(string n, int s, int e, int st, bool unbounded) 
    : name(n), start(s), end(e), step(st), is_unbounded(unbounded) {}

bool StructureArray::contains(int index) const {
    if (is_unbounded) {
        return index >= start && (step == 0 || (index - start) % step == 0);
    }
    if (step == 0) {
        return index == start;
    }
    return index >= start && index <= end && (index - start) % step == 0;
}

set<int> StructureArray::getIndices() const {
    set<int> indices;
    if (is_unbounded) {
        indices.insert(start); // Return the start index for unbounded arrays
    } else if (step == 0) {
        indices.insert(start);
    } else {
        for (int i = start; i <= end; i += step) {
            indices.insert(i);
        }
    }
    return indices;
}

bool StructureArray::binary_contains(int index) const {
    if (is_unbounded) {
        return index >= start && (step == 0 || (index - start) % step == 0);
    }
    if (step == 0) {
        return index == start;
    }
    if (index < start || index > end) {
        return false;
    }
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
                size_t pos = 0;
                size_t colon_pos = indices_str.find(':');
                if (colon_pos == string::npos) {
                    // Single index, e.g., "values(1)"
                    indices.push_back(stoi(indices_str));
                } else {
                    // Range or unbounded, e.g., "values(1:5)", "values(1:-1:2)", or "values(-1:-1:2)"
                    indices.push_back(stoi(indices_str.substr(0, colon_pos)));
                    size_t second_colon_pos = indices_str.find(':', colon_pos + 1);
                    if (second_colon_pos == string::npos) {
                        // Format "start:end"
                        indices.push_back(stoi(indices_str.substr(colon_pos + 1)));
                        indices.push_back(1); // Default step
                    } else {
                        // Format "start:end:step"
                        int end = stoi(indices_str.substr(colon_pos + 1, second_colon_pos - colon_pos - 1));
                        int step = stoi(indices_str.substr(second_colon_pos + 1));
                        indices.push_back(end);
                        indices.push_back(step);
                        // Handle unbounded cases
                        if (end == -1) {
                            if (indices[0] == -1) {
                                // Case (-1:-1:k) -> start=1, step=k, unbounded
                                indices[0] = 1;
                            }
                            // Case (i:-1:k) or (-1:-1:k) -> mark as unbounded
                            elements.emplace_back(name, true, 
                                new StructureArray(name, indices[0], 0, indices[2], true));
                            continue; // Skip the size check below
                        }
                    }
                }
                if (indices.size() == 1) {
                    elements.emplace_back(name, true, new StructureArray(name, indices[0], indices[0], 0));
                } else {
                    elements.emplace_back(name, true, 
                        new StructureArray(name, indices[0], indices[1], indices[2]));
                }
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
        if (query_elements[i].is_array) {
            if (query_elements[i].array->is_unbounded) {
                if (path_elements[i].second < 0) return false; // -1 in path not allowed
            } else if (!query_elements[i].array->contains(path_elements[i].second)) {
                return false;
            }
        } else if (path_elements[i].second != -1) {
            return false;
        }
    }
    return true;
}

PathManager::PathManager(const vector<string>& inc, const vector<string>& exc) {
    // Split each include query by ';' and create a PathQuery for each sub-query
    for (const auto& q : inc) {
        stringstream ss(q);
        string sub_query;
        while (getline(ss, sub_query, ';')) {
            if (!sub_query.empty()) { // Ignore empty sub-queries
                includes.emplace_back(sub_query);
            }
        }
    }

    // Check if exc contains at least one non-empty string before processing excludes
    bool has_non_empty_exclude = false;
    for (const auto& q : exc) {
        if (!q.empty()) {
            has_non_empty_exclude = true;
            break;
        }
    }

    // Only populate excludes if there is at least one non-empty string in exc
    if (has_non_empty_exclude) {
        for (const auto& q : exc) {
            // Split each exclude query by ';' and create a PathQuery for each sub-query
            stringstream ss(q);
            string sub_query;
            while (getline(ss, sub_query, ';')) {
                if (!sub_query.empty()) { // Skip empty sub-queries
                    excludes.emplace_back(sub_query);
                }
            }
        }
    }

    // Build the element cache based on the populated includes and excludes
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
        // Check if path matches the include query exactly or as a prefix
        bool match = true;
        if (path_elements.size() <= include.elements.size()) {
            for (size_t i = 0; i < path_elements.size(); ++i) {
                const auto& path_elem = path_elements[i];
                const auto& query_elem = include.elements[i];
                if (path_elem.first != query_elem.name) {
                    match = false;
                    break;
                }
                if (query_elem.is_array) {
                    if (path_elem.second < 0 || !query_elem.array->contains(path_elem.second)) {
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
        // Check if path is an extension of the include query
        if (path_elements.size() > include.elements.size()) {
            bool prefix_match = true;
            for (size_t i = 0; i < include.elements.size(); ++i) {
                const auto& path_elem = path_elements[i];
                const auto& query_elem = include.elements[i];
                if (path_elem.first != query_elem.name) {
                    prefix_match = false;
                    break;
                }
                if (query_elem.is_array) {
                    if (path_elem.second < 0 || !query_elem.array->contains(path_elem.second)) {
                        prefix_match = false; // Correction ici : utiliser prefix_match
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
                            int index = elem.array->is_unbounded ? elem.array->start : 
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
        // Case 2: array_path ends with the target array as in include (e.g., "ggd/electrons/density")
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
                    if (query_elem.is_array && path_elem.second != -1) {
                        if (!query_elem.array->contains(path_elem.second)) {
                            prefix_match = false;
                            break;
                        }
                    } else if (!query_elem.is_array && path_elem.second != -1) {
                        prefix_match = false;
                        break;
                    }
                }
                if (prefix_match) {
                    const auto& target_array_def = include.elements.back();
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
                    if (target_array_def.is_array) {
                        auto indices = target_array_def.array->getIndices();
                        for (int index : indices) {
                            string full_path = prefix_path.empty() ? 
                                target_array + "(" + to_string(index) + ")" :
                                prefix_path + "/" + target_array + "(" + to_string(index) + ")";
                            if (isInSet(full_path)) {
                                return true;
                            }
                        }
                    } else {
                        // Target is not an array, check the previous array element
                        set<int> indices;
                        size_t last_array_idx = 0;
                        for (size_t i = 0; i < include.elements.size() - 1; ++i) {
                            if (include.elements[i].is_array) {
                                indices = include.elements[i].array->getIndices();
                                last_array_idx = i;
                            }
                        }
                        if (!indices.empty()) {
                            // Build path up to the last array
                            string array_prefix;
                            for (size_t i = 0; i < last_array_idx; ++i) {
                                if (i > 0) array_prefix += "/";
                                if (prefix_elements.size() > i && prefix_elements[i].second != -1) {
                                    array_prefix += prefix_elements[i].first + "(" + 
                                                   to_string(prefix_elements[i].second) + ")";
                                } else {
                                    array_prefix += include.elements[i].name;
                                }
                            }
                            if (!array_prefix.empty()) array_prefix += "/";
                            array_prefix += include.elements[last_array_idx].name;
                            // Append all elements after last_array_idx up to target_array
                            string suffix_path;
                            for (size_t i = last_array_idx + 1; i < include.elements.size(); ++i) {
                                if (!suffix_path.empty()) suffix_path += "/";
                                suffix_path += include.elements[i].name;
                            }
                            for (int index : indices) {
                                string full_path = array_prefix + "(" + to_string(index) + ")";
                                if (!suffix_path.empty()) full_path += "/" + suffix_path;
                                if (isInSet(full_path)) {
                                    return true;
                                }
                            }
                        } else {
                            // No array, test the path directly
                            string full_path = prefix_path.empty() ? target_array : 
                                              prefix_path + "/" + target_array;
                            if (isInSet(full_path)) {
                                return true;
                            }
                        }
                    }
                }
            }
        }
        // Case 3: target_array is an intermediate array or an extension in include
        else {
            for (size_t i = 0; i < include.elements.size(); ++i) {
                if (include.elements[i].name == target_array && include.elements[i].is_array) {
                    if (prefix_elements.size() == i) {
                        bool prefix_match = true;
                        for (size_t j = 0; j < prefix_elements.size(); ++j) {
                            const auto& path_elem = prefix_elements[j];
                            const auto& query_elem = include.elements[j];
                            if (path_elem.first != query_elem.name) {
                                prefix_match = false;
                                break;
                            }
                            if (query_elem.is_array && path_elem.second != -1) {
                                if (!query_elem.array->contains(path_elem.second)) {
                                    prefix_match = false;
                                    break;
                                }
                            } else if (!query_elem.is_array && path_elem.second != -1) {
                                prefix_match = false;
                                break;
                            }
                        }
                        if (prefix_match) {
                            string prefix_path;
                            for (size_t j = 0; j < prefix_elements.size(); ++j) {
                                if (j > 0) prefix_path += "/";
                                if (prefix_elements[j].second != -1) {
                                    prefix_path += prefix_elements[j].first + "(" + 
                                                  to_string(prefix_elements[j].second) + ")";
                                } else {
                                    prefix_path += prefix_elements[j].first;
                                }
                            }
                            auto indices = include.elements[i].array->getIndices();
                            for (int index : indices) {
                                string base_path = prefix_path.empty() ? 
                                    target_array + "(" + to_string(index) + ")" :
                                    prefix_path + "/" + target_array + "(" + to_string(index) + ")";
                                // Check if any extension of base_path is in E
                                for (size_t k = i + 1; k < include.elements.size(); ++k) {
                                    string full_path = base_path;
                                    const auto& next_elem = include.elements[k];
                                    if (next_elem.is_array) {
                                        auto next_indices = next_elem.array->getIndices();
                                        for (int next_index : next_indices) {
                                            string extended_path = full_path + "/" + next_elem.name + "(" + 
                                                                  to_string(next_index) + ")";
                                            if (isInSet(extended_path)) {
                                                return true;
                                            }
                                        }
                                    } else {
                                        full_path += "/" + next_elem.name;
                                        if (isInSet(full_path)) {
                                            return true;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    break; // Stop searching once target_array is found
                }
            }
            // Case 4: array_path is an extension of include with specified indices
            if (prefix_elements.size() >= include.elements.size()) {
                bool prefix_match = true;
                for (size_t i = 0; i < include.elements.size(); ++i) {
                    const auto& path_elem = prefix_elements[i];
                    const auto& query_elem = include.elements[i];
                    if (path_elem.first != query_elem.name) {
                        prefix_match = false;
                        break;
                    }
                    if (query_elem.is_array && path_elem.second != -1) {
                        if (!query_elem.array->contains(path_elem.second)) {
                            prefix_match = false;
                            break;
                        }
                    } else if (!query_elem.is_array && path_elem.second != -1) {
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
                    if (isInSet(prefix_path + "/" + target_array)) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}