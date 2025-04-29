#include "partial_get_plugin.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <regex>
#include <string>
#include "simple_logger.h"
#include <boost/tokenizer.hpp>

#include <sstream>
#include <vector>
#include <stdexcept>
#include <algorithm>

#define PLUGIN_NAME "partial_get"
#define PLUGIN_VERSION "1.0.0"
#define PLUGIN_REPOSITORY "ssh://git.iter.org/projects/imas/al-plugins.git"
#define PLUGIN_DESCRIPTION "The 'partial_get' plugin builds low level partial requests from (HLI) user query."

#define QUERY_INCLUDES "includes"                                  // query 'includes' parameter (string) set in HLI
#define QUERY_EXCLUDES "excludes"                                  // query 'excludes' parameter (string) set in HLI
#define DEBUG_OPTION "debug"                                       // Enable full debug of the plugin (integer 0 or 1)
#define DEBUG_READ_REQUESTS_ONLY_OPTION "debug_read_requests_only" // Displays only READ requests when 'debug' is enabled

#define USE_INDICES_FORTRAN_CONVENTION 1

#include "path_manager.h"

PartialGetPlugin::PartialGetPlugin()
    : display_only_debug_read_requests(false),
      query_includes(), query_excludes()
{
    std::clog.setstate(std::ios_base::failbit);
}

PartialGetPlugin::~PartialGetPlugin()
{
}

std::string PartialGetPlugin::getName()
{
    return PLUGIN_NAME;
}

std::string PartialGetPlugin::getCommit()
{
    return "";
}

std::string PartialGetPlugin::getDescription()
{
    return PLUGIN_DESCRIPTION;
}

std::string PartialGetPlugin::getVersion()
{
    return PLUGIN_VERSION;
}

std::string PartialGetPlugin::getRepository()
{
    return PLUGIN_REPOSITORY;
}

std::string PartialGetPlugin::getParameters()
{
    std::string parameters = "";
    for (int i = 0; i < query_includes.size(); i++)
    {
        parameters += "Include " + std::to_string(i) + " : " + query_includes[i] + "; ";
    }
    for (int i = 0; i < query_excludes.size(); i++)
    {
        parameters += "Exclude " + std::to_string(i) + " : " + query_excludes[i] + "; ";
    }
    return parameters;
}

plugin::OPERATION PartialGetPlugin::node_operation(const std::string &path)
{
    return plugin::OPERATION::GET_ONLY;
}

std::string PartialGetPlugin::getReadbackName(const std::string &path, int *index)
{
    return "";
}

std::string PartialGetPlugin::getReadbackDescription(const std::string &path)
{
    return "";
}

std::string PartialGetPlugin::getReadbackCommit(const std::string &path)
{
    return "";
}

std::string PartialGetPlugin::getReadbackVersion(const std::string &path)
{
    return "";
}

std::string PartialGetPlugin::getReadbackRepository(const std::string &path)
{
    return "";
}

std::string PartialGetPlugin::getReadbackParameters(const std::string &path)
{
    return "";
}

/**
 * Sets the user parameters 'includes', 'excludes' and debug option as plugin parameters
 */
void PartialGetPlugin::setParameter(const char *parameter_name, int datatype, int dim, int *size, void *data)
{
    LOG_DEBUG << "Calling setParameter...";
    LOG_DEBUG << "parameter_name=" << parameter_name << ", dim=" << dim;

    std::string parameter = parameter_name;

    if (dim == 1)
    {
        if (parameter == QUERY_INCLUDES)
        {
            query_includes.clear();
            char *p = strdup((char *)data);
            std::string includes = std::string(p);
            cleanQueries(includes, true);
            LOG_DEBUG << "setting includes:" << parameter.c_str();
	        splitQueryIntoSubQueries(includes, query_includes);
            free(p);
        }
        else if (parameter == QUERY_EXCLUDES)
        {
            query_excludes.clear();
            char *p = strdup((char *)data);
            std::string excludes = std::string(p);
            cleanQueries(excludes, false);
            LOG_DEBUG << "setting excludes:" << parameter.c_str();
            splitQueryIntoSubQueries(excludes, query_excludes);
            free(p);
        }

    }
    else if (parameter == DEBUG_OPTION && dim == 0)
    {
        int *p_value = (int *)data;
        if (*p_value == 1)
        {
            std::clog.clear();
        }
        else
        {
            std::clog.setstate(std::ios_base::failbit);
        }
    }
    else if (parameter == DEBUG_READ_REQUESTS_ONLY_OPTION && dim == 0)
    {
        int *p_value = (int *)data;
        display_only_debug_read_requests = *p_value;
    }
    else
    {
        std::string message = "PartialGetPlugin: parameter " + parameter + " not supported";
        throw ALPluginException(message.c_str(), LOG);
    }

}


void PartialGetPlugin::begin_arraystruct_action(int ctx, int *aosctx, const char *fieldPath, const char *timeBasePath, int *arraySize)
{
    // Creation of an AOS
    LLenv lle = Lowlevel::getLLenv(ctx);
    ArraystructContext *actx = lle.create(fieldPath, timeBasePath);
    *aosctx = Lowlevel::addLLenv(lle.backend, actx);
    lle.backend->beginArraystructAction(actx, arraySize);

    // Initialize PathManager with predefined include and exclude queries
    PathManager manager(query_includes, query_excludes);

    //Checking if the AOS is included
    std::string aos_full_path;
    getAOSPaths(actx, aos_full_path, true);

    // Use PathManager to check if the full path of the array of structures (AOS) has at least one element in the set E
    bool is_included = manager.is_included(aos_full_path);

    // If no elements of the AOS are included according to PathManager, set array size to 0 to exclude all elements
    if (!is_included) {
        *arraySize = 0;
        LOG_DEBUG << "PartialGetPlugin::begin_arraystruct_action, all elements excluded for AOS path=" << aos_full_path.c_str();
    }
    // If at least one element is included, proceed with the original array size
    else {
        LOG_DEBUG << "PartialGetPlugin::begin_arraystruct_action, all elements included for AOS path=" << aos_full_path.c_str();
    }
}

int PartialGetPlugin::read_data(int ctx, const char *fieldPath, const char *timeBasePath,
                                void **data, int datatype, int dim, int *size)
{
    // If no include or exclude queries are defined, PathManager is bypassed and all data is returned
    if (query_includes.size() == 0 && query_excludes.size() == 0)
    {                                                                                 
        al_plugin_read_data(ctx, fieldPath, timeBasePath, data, datatype, dim, size); 
        return 1;
    }

    LLenv lle = Lowlevel::getLLenv(ctx);
    Context *ctxt = lle.context;

    std::string full_path;

    getFullPath(ctx, fieldPath,  full_path);

    // Initialize PathManager with predefined include and exclude queries
    PathManager manager(query_includes, query_excludes);

    // Use PathManager to check if the full path is explicitly included in the set E
    bool is_included = manager.isInSet(full_path);

    // If the path is included according to PathManager, read and return the data
    if (is_included) {
        LOG_DEBUG << "PartialGetPlugin::read_data, including full path=" << full_path.c_str();
        al_plugin_read_data(ctx, fieldPath, timeBasePath, data, datatype, dim, size);
        return 1;
    }
    
    // If the path is not included, skip reading the data and return 0
    LOG_DEBUG << "PartialGetPlugin::read_data, excluding full path=" << full_path.c_str();
    return 0;
}

// Build the full path of a given AOS (e.g. flux_loop(2)) for the 'flux_loop' AOS of a magnetics IDS)
void PartialGetPlugin::getAOSPaths(ArraystructContext *c, std::string &full_path, bool array_only)
{
    int aos_index;
    std::string path = "";

    if (!array_only) {
        if (USE_INDICES_FORTRAN_CONVENTION == 1)
            aos_index = c->getIndex() + 1;
        else
            aos_index = c->getIndex();
        path = c->getPath() + "(" + std::to_string(aos_index) + ")";
    }

    else {
        path = c->getPath();
    }

    c = c->getParent();

    while (c != NULL)
    {
        if (USE_INDICES_FORTRAN_CONVENTION == 1)
            aos_index = c->getIndex() + 1;
        else
            aos_index = c->getIndex();
        path = c->getPath() + "(" + std::to_string(aos_index) + ")/" + path;
        c = c->getParent();
    }
    full_path = path;
}


void PartialGetPlugin::getFullPath(int ctxID, const char* fieldPath,  std::string &full_path) {

  LLenv lle = Lowlevel::getLLenv(ctxID);
  std::string path = "";
  OperationContext *opctx = nullptr;
  if (lle.context->getType() == CTX_ARRAYSTRUCT_TYPE) {
    ArraystructContext *c =dynamic_cast<ArraystructContext*> (lle.context);
    opctx = c->getOperationContext();
    path = c->getPath();
    int aos_index;

    if (USE_INDICES_FORTRAN_CONVENTION == 1)
        aos_index = c->getIndex() + 1;
    else
        aos_index = c->getIndex();

    path = c->getPath() + "(" + std::to_string(aos_index) + ")";

    c = c->getParent();
    while (c != NULL && c->getType() == CTX_ARRAYSTRUCT_TYPE) {
       if (USE_INDICES_FORTRAN_CONVENTION == 1)
            aos_index = c->getIndex() + 1;
       else
            aos_index = c->getIndex();
      path = c->getPath() + "(" + std::to_string(aos_index) + ")/" + path;
      c = c->getParent();
    } 
  }
  if (path == "")
    full_path = std::string(fieldPath);
  else
    full_path = path + "/" + std::string(fieldPath);
}

void PartialGetPlugin::splitQueryIntoSubQueries(const std::string &query, std::vector<std::string> &subqueries) {

    std::stringstream ss(query);
    std::string subquery;
    
    while (std::getline(ss, subquery, ';')) 
        subqueries.push_back(subquery);
}

void PartialGetPlugin::cleanQueries(std::string &query, bool includes) {
    // Remove all whitespace from the string
    query.erase(std::remove_if(query.begin(), query.end(), ::isspace), query.end());

    // Check if the string is empty after removing whitespace
    if (query.empty()) {
        return;
    }

    // Split the string into subqueries separated by ';'
    std::vector<std::string> subqueries;
    splitQueryIntoSubQueries(query, subqueries);

    // Function to process a path element
    auto processPathElement = [&includes](const std::string& element) -> std::string {
        // Check for invalid separators like '.' within the path element
        if (element.find('.') != std::string::npos) {
            throw ALPluginException(includes ?
                "Invalid include query: path element contains invalid separator '.' instead of '/'" :
                "Invalid exclude query: path element contains invalid separator '.' instead of '/'");
        }

        // Regular expression to match different query formats: name or name(indices)
        std::regex queryPattern(R"(^([^(]*)(?:\(([^)]*)\))?$)");
        std::smatch matches;

        if (!std::regex_match(element, matches, queryPattern)) {
            throw ALPluginException(includes ?
                "Invalid include query: path element has incorrect format (expected 'name' or 'name(indices)')" :
                "Invalid exclude query: path element has incorrect format (expected 'name' or 'name(indices)')");
        }

        std::string name = matches[1].str();
        std::string indices = (matches.size() > 2 && matches[2].matched) ? matches[2].str() : "";

        // Case where there are no parentheses: return just the name
        if (!matches[2].matched) {
            return name;
        }

        // Case where parentheses are present but empty: throw an exception
        if (indices.empty()) {
            throw ALPluginException(includes ?
                "Invalid include query: empty content in parentheses is not allowed" :
                "Invalid exclude query: empty content in parentheses is not allowed");
        }

        // Do not modify T(:) - special case for selecting all elements
        if (indices == ":") {
            return element; // Return the original element like name(:)
        }

        // Normalize index formats: (i), (i:j), (i:j:k), (:j), (i:), (::k)
        std::stringstream newIndices;
        newIndices << name << "("; // Start constructing the normalized string

        // Count the number of ':' characters in the indices part
        int colonCount = std::count(indices.begin(), indices.end(), ':');

        try {
            if (colonCount == 0) {
                // Format: (i) -> normalize to (i:i:0)
                int i = std::stoi(indices);
                if (i <= 0) {
                    throw ALPluginException(includes ?
                        "Invalid include query: Index i must be greater than zero for format (i)" :
                        "Invalid exclude query: Index i must be greater than zero for format (i)");
                }
                newIndices << i << ":" << i << ":0"; // Use 0 for step k as it represents a single element
            }
            else if (colonCount == 1) {
                // Format can be (i:j), (i:), or (:j)
                size_t colonPos = indices.find(':');
                std::string firstPart = indices.substr(0, colonPos);
                std::string secondPart = indices.substr(colonPos + 1);

                if (!firstPart.empty() && !secondPart.empty()) {
                    // Format: (i:j) -> normalize to (i:j:1)
                    int i = std::stoi(firstPart);
                    int j = std::stoi(secondPart);

                    if (i <= 0 || j <= 0) {
                        throw ALPluginException(includes ?
                            "Invalid include query: Indices must be greater than zero for format (start:stop)" :
                            "Invalid exclude query: Indices must be greater than zero for format (start:stop)");
                    }
                    if (j < i) {
                        throw ALPluginException(includes ?
                            "Invalid include query: stop must be >= start for format (start:stop)" :
                            "Invalid exclude query: stop must be >= start for format (start:stop)");
                    }
                    newIndices << i << ":" << j << ":1"; // Default step is 1
                } else if (!firstPart.empty() && secondPart.empty()) {
                    // Format: (i:) -> normalize to (i:-1:1)
                    int i = std::stoi(firstPart);
                    if (i <= 0) {
                        throw ALPluginException(includes ?
                            "Invalid include query: Index start must be greater than zero for format (start:)" :
                            "Invalid exclude query: Index start must be greater than zero for format (start:)");
                    }
                    newIndices << i << ":-1:1"; // Corrected to match (i:-1:1)
                } else if (firstPart.empty() && !secondPart.empty()) {
                    // Format: (:j) -> normalize to (1:j:1)
                    int j = std::stoi(secondPart);
                    if (j <= 0) {
                        throw ALPluginException(includes ?
                            "Invalid include query: Index stop must be greater than zero for format (:stop)" :
                            "Invalid exclude query: Index stop must be greater than zero for format (:stop)");
                    }
                    newIndices << "1:" << j << ":1"; // Start from 1, end at j, step 1
                } else {
                    // This case (:) should have been caught earlier, but added for completeness
                    throw ALPluginException(includes ?
                        "Invalid include query: unexpected format with one colon" :
                        "Invalid exclude query: unexpected format with one colon");
                }
            }
            else if (colonCount == 2) {
                // Format can be (i:j:k) or (::k)
                size_t firstColon = indices.find(':');
                size_t secondColon = indices.find(':', firstColon + 1);
                std::string i_str = indices.substr(0, firstColon);
                std::string j_str = indices.substr(firstColon + 1, secondColon - firstColon - 1);
                std::string k_str = indices.substr(secondColon + 1);

                if (i_str.empty() && j_str.empty() && !k_str.empty()) {
                    // Format: (::k) -> normalize to (1:-1:k)
                    int k = std::stoi(k_str);
                    if (k < 0) {
                        throw ALPluginException(includes ?
                            "Invalid include query: Index step must be positive or zero for format (::step)" :
                            "Invalid exclude query: Index step must be positive or zero for format (::step)");
                    }
                    newIndices << "1:-1:" << k; // Corrected to match (1:-1:k)
                }
                else if (!i_str.empty() && !j_str.empty() && !k_str.empty()) {
                    // Format: (i:j:k) -> validate and keep as is
                    int i = std::stoi(i_str);
                    int j = std::stoi(j_str);
                    int k = std::stoi(k_str);

                    // Validate indices according to original logic
                    if (i <= 0 || j <= 0) {
                        throw ALPluginException(includes ?
                            "Invalid include query: Indices start and stop must be greater than zero for format (start:stop:step)" :
                            "Invalid exclude query: Indices start and stop must be greater than zero for format (start:stop:step)");
                    }
                    if (k < 0) {
                        throw ALPluginException(includes ?
                            "Invalid include query: Index step must be positive or zero for format (start:stop:step)" :
                            "Invalid exclude query: Index step must be positive or zero for format (start:stop:step)");
                    }
                    if (j < i) {
                        throw ALPluginException(includes ?
                            "Invalid include query: stop must be >= start for format (start:stop:step)" :
                            "Invalid exclude query: stop must be >= start for format (start:stop:step)");
                    }
                    // Indices are valid, use them directly
                    newIndices << i << ":" << j << ":" << k;
                } else {
                    // Other formats with two colons (e.g., i::k, :j:k) are considered invalid
                    throw ALPluginException(includes ?
                        "Invalid include query: unsupported format with two colons" :
                        "Invalid exclude query: unsupported format with two colons");
                }
            }
            else {
                // More than two ':' characters
                throw ALPluginException(includes ?
                    "Invalid include query: incorrect format, too many colons" :
                    "Invalid exclude query: incorrect format, too many colons");
            }
        } catch (const std::invalid_argument& e) {
            // Catch errors from std::stoi
            throw ALPluginException(includes ?
                "Invalid include query: index is not a valid integer" :
                "Invalid exclude query: index is not a valid integer");
        } catch (const std::out_of_range& e) {
            // Catch errors from std::stoi if number is too large
            throw ALPluginException(includes ?
                "Invalid include query: index is out of range" :
                "Invalid exclude query: index is out of range");
        }
        // Catch the specific runtime_errors thrown above for index validation
        catch (const ALPluginException& e) {
            throw; // Re-throw the specific validation error
        }

        newIndices << ")"; // Close the parentheses
        return newIndices.str(); // Return the normalized string e.g., name(i:j:k)
    };

    // Process each subquery
    std::vector<std::string> cleanedSubqueries;
    cleanedSubqueries.reserve(subqueries.size());

    for (const auto& sq : subqueries) {
        // Check for invalid separators like '.' used between path elements
        if (sq.find('.') != std::string::npos) {
            throw ALPluginException(includes ?
                "Invalid include query: subquery contains invalid separator '.' instead of '/'" :
                "Invalid exclude query: subquery contains invalid separator '.' instead of '/'");
        }

        // Split the path into elements separated by '/'
        std::vector<std::string> pathElements;
        std::stringstream pathSs(sq);
        std::string pathElement;
        std::vector<std::string> processedElements;

        while (std::getline(pathSs, pathElement, '/')) {
            if (!pathElement.empty()) { // Handle potential empty strings from consecutive slashes "//"
                processedElements.push_back(processPathElement(pathElement));
            } else if (!processedElements.empty()) {
                // If pathElement is empty BUT we already have elements, it implies "//"
                // Current behavior: treat "a//b" as "a/b"
            } else {
                // Path starts with '/', ignore leading empty element
            }
        }

        // Reconstruct the processed path for the subquery
        std::string processedSq;
        for (size_t i = 0; i < processedElements.size(); ++i) {
            processedSq += processedElements[i];
            if (i < processedElements.size() - 1) {
                processedSq += "/";
            }
        }
        if (!processedSq.empty()) {
            cleanedSubqueries.push_back(processedSq);
        }
    }

    // Reconstruct the final query string from cleaned subqueries
    query = "";
    for (size_t i = 0; i < cleanedSubqueries.size(); ++i) {
        query += cleanedSubqueries[i];
        if (i < cleanedSubqueries.size() - 1) {
            query += ";";
        }
    }
}

void PartialGetPlugin::write_data(int ctx, const char *fieldPath, const char *timeBasePath, void *data, int datatype, int dim, int *size)
{
    throw ALPluginException("PartialGetPlugin: write_data() not supported", LOG);
}

void PartialGetPlugin::end_action(int ctx)
{
}

void PartialGetPlugin::begin_global_action(int pulseCtx, const char *dataobjectname, const char *datapath, int mode, int opCtx)
{
    this->operation = mode;
}

void PartialGetPlugin::begin_slice_action(int pulseCtx, const char *dataobjectname, int mode, double time, int interp, int opCtx)
{
    this->operation = mode;
}

void PartialGetPlugin::begin_timerange_action(int pulseCtx, const char *dataobjectname, int mode, double tmin, double tmax, std::vector<double> dtime, int interp, int opCtx)
{
    this->operation = mode;
}
