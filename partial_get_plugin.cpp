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

    PathManager manager(query_includes, query_excludes);

    //Checking if the AOS is included
    std::string aos_full_path;
    getAOSPaths(actx, aos_full_path, true);
    //LOG_DEBUG <<  "aos_full_path= " << aos_full_path;
    //LOG_DEBUG <<  "manager.is_included(aos_full_path)= " << manager.is_included(aos_full_path);
    bool is_included = manager.is_included(aos_full_path);

    if (!is_included) {
        *arraySize = 0;
        LOG_DEBUG << "PartialGetPlugin::begin_arraystruct_action, all elements excluded for AOS path=" << aos_full_path.c_str();
    }
    else {
        LOG_DEBUG << "PartialGetPlugin::begin_arraystruct_action, all elements included for AOS path=" << aos_full_path.c_str();
    }
}

int PartialGetPlugin::read_data(int ctx, const char *fieldPath, const char *timeBasePath,
                                void **data, int datatype, int dim, int *size)
{
    if (query_includes.size() == 0 && query_excludes.size() == 0)
    {                                                                                 // we return all data if there is no 'includes' and no 'excludes' query
        al_plugin_read_data(ctx, fieldPath, timeBasePath, data, datatype, dim, size); // in case of exception, the LL will catch it
        return 1;
    }

    LLenv lle = Lowlevel::getLLenv(ctx);
    Context *ctxt = lle.context;

    std::string full_path;

    getFullPath(ctx, fieldPath,  full_path);
    PathManager manager(query_includes, query_excludes);

    bool is_included = manager.isInSet(full_path);
    if (is_included) {
        LOG_DEBUG << "PartialGetPlugin::read_data, including full path=" << full_path.c_str();
        al_plugin_read_data(ctx, fieldPath, timeBasePath, data, datatype, dim, size);
        return 1;
    }
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
        // Regular expression to match different query formats
        std::regex queryPattern(R"(^([^(]*)(?:\(([^)]*)\))?$)");
        std::smatch matches;

        if (!std::regex_match(element, matches, queryPattern)) {
            throw std::runtime_error(includes ? 
                "Invalid include query: incorrect path element format" : 
                "Invalid exclude query: incorrect path element format");
        }

        std::string name = matches[1].str();
        std::string indices = (matches.size() > 2 && matches[2].matched) ? matches[2].str() : "";

        // Case where there are no indices
        if (indices.empty()) {
            return name;
        }

        // Normalize formats: (i), (i:j), (i:j:k)
        std::stringstream newIndices;
        newIndices << name << "(";

        // Do not modify T(:)
        if (indices == ":") {
            return element;
        }

        // Count the number of ':'
        int colonCount = std::count(indices.begin(), indices.end(), ':');

        if (colonCount == 0) {
            // Format (i)
            try {
                int i = std::stoi(indices);
                if (i <= 0) {
                    throw std::runtime_error(includes ? 
                        "Invalid include query: non-positive index" : 
                        "Invalid exclude query: non-positive index");
                }
                newIndices << i << ":" << i << ":0";
            } catch (const std::invalid_argument&) {
                throw std::runtime_error(includes ? 
                    "Invalid include query: invalid index" : 
                    "Invalid exclude query: invalid index");
            }
        }
        else if (colonCount == 1) {
            // Format (i:j)
            size_t firstColon = indices.find(':');
            try {
                int i = std::stoi(indices.substr(0, firstColon));
                int j = std::stoi(indices.substr(firstColon + 1));

                if (i <= 0 || j <= 0) {
                    throw std::runtime_error(includes ? 
                        "Invalid include query: non-positive index" : 
                        "Invalid exclude query: non-positive index");
                }
                if (j < i) {
                    throw std::runtime_error(includes ? 
                        "Invalid include query: j must be >= i" : 
                        "Invalid exclude query: j must be >= i");
                }
                newIndices << i << ":" << j << ":1";
            } catch (const std::invalid_argument&) {
                throw std::runtime_error(includes ? 
                    "Invalid include query: invalid index" : 
                    "Invalid exclude query: invalid index");
            }
        }
        else if (colonCount == 2) {
            // Format (i:j:k)
            size_t firstColon = indices.find(':');
            size_t secondColon = indices.find(':', firstColon + 1);
            try {
                int i = std::stoi(indices.substr(0, firstColon));
                int j = std::stoi(indices.substr(firstColon + 1, secondColon - firstColon - 1));
                int k = std::stoi(indices.substr(secondColon + 1));

                if (i <= 0 || j <= 0 || k < 0) {
                    throw std::runtime_error(includes ? 
                        "Invalid include query: invalid index" : 
                        "Invalid exclude query: invalid index");
                }
                if (j < i) {
                    throw std::runtime_error(includes ? 
                        "Invalid include query: j must be >= i" : 
                        "Invalid exclude query: j must be >= i");
                }
                newIndices << i << ":" << j << ":" << k;
            } catch (const std::invalid_argument&) {
                throw std::runtime_error(includes ? 
                    "Invalid include query: invalid index" : 
                    "Invalid exclude query: invalid index");
            }
        }
        else {
            // Too many ':' or other format issues
            throw std::runtime_error(includes ? 
                "Invalid include query: incorrect format" : 
                "Invalid exclude query: incorrect format");
        }

        newIndices << ")";
        return newIndices.str();
    };

    // Process each subquery
    for (auto& sq : subqueries) {
        // Split the paths
        std::vector<std::string> pathElements;
        std::stringstream pathSs(sq);
        std::string pathElement;

        while (std::getline(pathSs, pathElement, '/')) {
            pathElements.push_back(processPathElement(pathElement));
        }

        // Reconstruct the path
        sq = "";
        for (size_t i = 0; i < pathElements.size(); ++i) {
            sq += pathElements[i];
            if (i < pathElements.size() - 1) {
                sq += "/";
            }
        }
    }

    // Reconstruct the query string
    query = "";
    for (size_t i = 0; i < subqueries.size(); ++i) {
        query += subqueries[i];
        if (i < subqueries.size() - 1) {
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
