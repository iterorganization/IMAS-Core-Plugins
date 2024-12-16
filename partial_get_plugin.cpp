#include "partial_get_plugin.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <regex>
#include <string>
#include "simple_logger.h"
#include <boost/tokenizer.hpp>

#define PLUGIN_NAME "partial_get"
#define PLUGIN_VERSION "1.0.0"
#define PLUGIN_REPOSITORY "ssh://git.iter.org/projects/imas/al-plugins.git"
#define PLUGIN_DESCRIPTION "The 'partial_get' plugin builds low level partial requests from (HLI) user query."

#define QUERY_INCLUDES "includes"                                  // query 'includes' parameter (string) set in HLI
#define QUERY_EXCLUDES "excludes"                                  // query 'excludes' parameter (string) set in HLI
#define DEBUG_OPTION "debug"                                       // Enable full debug of the plugin (integer 0 or 1)
#define DEBUG_READ_REQUESTS_ONLY_OPTION "debug_read_requests_only" // Displays only READ requests when 'debug' is enabled

PartialGetPlugin::PartialGetPlugin()
    : aos_indices(), aos_excludes_indices(), aos_queries(), aos_excludes_queries(),
      aos_queries_index(), aos_excludes_queries_index(), display_only_debug_read_requests(false),
      query_includes(), query_excludes(),requested_includes_paths(), requested_excludes_paths()
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
            aos_indices.clear();
            aos_queries.clear();
            aos_queries_index.clear();
            char *p = strdup((char *)data);
            std::string includes = std::string(p);
            cleanQueries(includes, true);
            LOG_DEBUG << "setting includes:" << parameter.c_str();
            typedef boost::tokenizer<boost::char_separator<char>> tokenizer;
            boost::char_separator<char> sep{";"}; // separator between each include parameter
            tokenizer tok{includes, sep};
            for (const auto &t : tok)
            {
                query_includes.push_back(t);
            }
            free(p);
        }
        else if (parameter == QUERY_EXCLUDES)
        {
            query_excludes.clear();
            aos_excludes_indices.clear();
            aos_excludes_queries.clear();
            aos_excludes_queries_index.clear();
            char *p = strdup((char *)data);
            std::string excludes = std::string(p);
            cleanQueries(excludes, false);
            LOG_DEBUG << "setting excludes:" << parameter.c_str();
            typedef boost::tokenizer<boost::char_separator<char>> tokenizer;
            boost::char_separator<char> sep{";"}; // separator between each exclude parameter
            tokenizer tok{excludes, sep};
            for (const auto &t : tok)
            {
                query_excludes.push_back(t);
            }
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

    buildQueriesPaths();
}


void PartialGetPlugin::begin_arraystruct_action(int ctx, int *aosctx, const char *fieldPath, const char *timeBasePath, int *arraySize)
{
    std::string field = fieldPath;

    // Creation of an AOS
    LLenv lle = Lowlevel::getLLenv(ctx);
    ArraystructContext *actx = lle.create(fieldPath, timeBasePath);
    *aosctx = Lowlevel::addLLenv(lle.backend, actx);
    lle.backend->beginArraystructAction(actx, arraySize);

    std::string aos_full_path;
    std::string rpath;
    getAOSPaths(actx, rpath, aos_full_path); // extracts some paths of the AOS (e.g. magnetics:0/flux_loop/position for the full path or flux_loop/position for the rpath )

    if (!display_only_debug_read_requests)
        LOG_DEBUG << "fieldPath= " << fieldPath << "; aos_full_path= " << aos_full_path.c_str() << "; rpath= " << rpath.c_str();

    if (query_includes.size() == 0) //if 'includes' parameter is empty, all fields are included by default
    {
        std::vector<int> requested_indices{-1}; // storing -1 means that all indices of this AOS are requested
        aos_indices[aos_full_path] = requested_indices;
    }

    for (int i = 0; i < query_excludes.size(); i++)
    {
        const std::string &query = query_excludes[i];
        if (!checkIfQueryMatchesCurrentPath(query, actx)) // actx pointer is never NULL
            continue;
        std::string aos_query = setAOSQuery(actx, query, aos_full_path, i, aos_excludes_queries, aos_excludes_queries_index); // Stores the AOS query in the 'aos_queries' map
        if (!display_only_debug_read_requests && aos_query != "")
            LOG_DEBUG << "buildRequestedAOSIndices for 'excludes' query: " << aos_query;

        buildRequestedAOSIndices(field, aos_query, aos_full_path, aos_excludes_indices, false); // builds the requested AOS indices for the 'excludes' query
    }

    for (int i = 0; i < query_includes.size(); i++)
    {
        const std::string &query = query_includes[i];
        if (!checkIfQueryMatchesCurrentPath(query, actx))
        {
            LOG_DEBUG << "query = " << query << " has NO impact for path: " << actx->getPath();
            continue;
        }
        LOG_DEBUG << "aos path = " << actx->getPath() << " has an associated (token) query " << query;
        std::string aos_query = setAOSQuery(actx, query, aos_full_path, i, aos_queries, aos_queries_index); // Stores the AOS query in the 'aos_queries' map
        if (!display_only_debug_read_requests && aos_query != "")
            LOG_DEBUG << "buildRequestedAOSIndices for 'includes' query: " << aos_query;

        buildRequestedAOSIndices(field, aos_query, aos_full_path, aos_indices, true); // builds the requested AOS indices for the 'includes' query
    }

    if (*arraySize == 0)
    {
        LOG_DEBUG << "PartialGetPlugin::begin_arraystruct_action, excluding all elements (since size is 0) from AOS path=%s" << aos_full_path.c_str();
    }
}

// Stores the AOS query in the 'aos_queries' map whose key is the aos full_path
std::string PartialGetPlugin::setAOSQuery(ArraystructContext *actx, const std::string &query,
    const std::string &aos_full_path, int query_index, std::map<std::string, std::string> &aos_queries_, std::map<std::string, int> &aos_queries_index_)
{
    std::string aos_query = ""; // empty value means that query is undefined for this AOS

    auto it = aos_queries_index_.find(aos_full_path);

    if (!display_only_debug_read_requests)
        LOG_DEBUG << "query=" << query << "; aos_full_path=" << aos_full_path << "; query_index = " << query_index;

    if (it != aos_queries_index_.end()) // this AOS path has been already visited and a query was found for this path
    {
        if (it->second != query_index) //A query has been already found for this AOS path
        { // a different query already exists for this AOS path
            //Checking if this query is the same, otherwise it's an error
            std::string current_aos_query = "";
            const std::string &current_query = query_includes[query_index];
            getAOSQuery(actx, current_query, current_aos_query);
            if (current_aos_query != "") {
                auto found = aos_queries_.find(aos_full_path);
                if (found->second != current_aos_query) {
                    std::string message = "PartialGetPlugin: found at least 2 different queries for the same AOS: " + aos_full_path;
                    throw ALPluginException(message.c_str(), LOG);
                }
            }
        }
        else
        { // this AOS has been already visited, it's exactly the same query than found previously
            auto found = aos_queries_.find(aos_full_path);
            if (found == aos_queries_.end()) {
                std::string message = "PartialGetPlugin: unexpected error, AOS query not found for: " + aos_full_path;
                throw ALPluginException(message.c_str(), LOG);
            }
            return found->second;
        }
        
        auto found = aos_queries_.find(aos_full_path);
        if (found == aos_queries_.end()) {
            std::string message = "PartialGetPlugin: unexpected error, AOS query not found for: " + aos_full_path;
            throw ALPluginException(message.c_str(), LOG);
        }
        return found->second;
    }

    getAOSQuery(actx, query, aos_query);
    LOG_DEBUG << "--> aos_query =" << aos_query << " for path " << aos_full_path;

    if (aos_query != "")
    { // a user query is defined for this AOS path
        LOG_DEBUG << "adding aos_query =" << aos_query << "; query_index = " << query_index << " for path " << aos_full_path;
        aos_queries_[aos_full_path] = aos_query; // aos_queries map contains only non empty queries
        aos_queries_index_[aos_full_path] = query_index;
    }
    else
    {
        LOG_DEBUG << "found empty aos_query";
    }

    return aos_query;
}

// Extracts the query 'aos_query' of an AOS, for example the string 'flux_loop(1:5:2)' from a given user query 'query'
void PartialGetPlugin::getAOSQuery(ArraystructContext *actx, const std::string &query, std::string &aos_query)
{
    // Getting the position of the current AOS given by the actx pointer
    int position = -1;
    getAOSPosition(actx, &position); // position = 0 means first AOS, position = 1 means second nested AOS, and so on
    typedef boost::tokenizer<boost::char_separator<char>> tokenizer;
    boost::char_separator<char> sep{"/"}; // separator between AOS in the query (e.g query "a(2:3:1)/b(3:7:1)" where a and b are AOSs)
    tokenizer tok{query, sep};
    std::vector<std::string> tokens;

    for (const auto &t : tok)
        tokens.push_back(t);

    if (tokens.size() > position)
    { // is there a query at the position of the current AOS?
        aos_query = tokens[position];

        if (aos_query == actx->getPath())
        {
            aos_query = actx->getPath() + "(:)";
        }
        else if (aos_query == actx->getPath() + "(:)")
        {
        }
        else
        {
            // printf("testing aos_query=%s for path=%s\n", aos_query.c_str(), actx->getPath().c_str()) ;
            std::string replacement = "";
            std::string p = "\\((\\d+):(\\d+):(\\d+)\\)";
            std::regex pattern(p);
            std::string aos_path = std::regex_replace(aos_query, pattern, replacement);
            // printf("testing aos_path=%s for query=%s\n", aos_path.c_str(), aos_query.c_str()) ;
            if (actx->getPath() != aos_path)
            {
                LOG_DEBUG << "returning empty string since paths differ";
                aos_query = ""; // query has no effect for the current AOS path
            }
        }
    }
    else
    {
        // LOG_DEBUG << "returning empty string since position < tokens.size()";
        aos_query = ""; // query has no effect for the current AOS path
    }
}

// Builds the AOS requested indices from the query 'aos_query' and save its indices into a map with key='aos_full_path'
void PartialGetPlugin::buildRequestedAOSIndices(const std::string &aos_name, const std::string &aos_query,
    const std::string &aos_full_path, std::map<std::string, std::vector<int>> &aos_indices_, bool includes)
{
    if (aos_indices_.find(aos_full_path) != aos_indices_.end()) // requested indices for this AOS path have been already set for this query index (AOS already visited)
        return;

    if (aos_query == "")
        return;

    if (aos_query == aos_name + "(:)") // all indices of this AOS are requested
    {
        std::vector<int> requested_indices{-1}; // storing -1 means that all indices of this AOS are requested
        aos_indices[aos_full_path] = requested_indices;
        return;
    }

    std::vector<int> requested_indices;
    getIndices(aos_name, aos_query, requested_indices);
    aos_indices_[aos_full_path] = requested_indices;
}

// Extract the requested AOS indices from the query 'aos_query'
void PartialGetPlugin::getIndices(const std::string &aos_name, const std::string &aos_query, std::vector<int> &indices)
{
    std::smatch matches;
    std::string p = "(" + aos_name + ")\\((\\d+):(\\d+):(\\d+)\\)";
    std::regex pattern(p);

    if (std::regex_search(aos_query, matches, pattern))
    {
        // std::cout << "matches = " << matches.size() << std::endl;

        int minIndex = std::stoi(matches[2]);
        int maxIndex = std::stoi(matches[3]);

        if (minIndex < 1 || maxIndex < 1) {
            char message[200];
            snprintf(message, 200, "Min and max index should be > 1 for AOS %s. Please check the query.\n", aos_name.c_str());
            throw ALPluginException(message);
        }
        if (maxIndex < minIndex) {
            char message[200];
            snprintf(message, 200, "Max index should be >= to min index for AOS %s. Please check the query.\n", aos_name.c_str());
            throw ALPluginException(message);
        }
        int inc = std::stoi(matches[4]);
        
        if (!display_only_debug_read_requests)
            LOG_DEBUG << "minIndex = " << minIndex << ", maxIndex = " << maxIndex << ", inc=" << inc;

        std::vector<int> requested_indices;
        indices.push_back(minIndex);
        indices.push_back(maxIndex);
        indices.push_back(inc);
    }
    else
    {
        LOG_DEBUG << "expected pattern not found for " << aos_name.c_str();
        char message[200];
        snprintf(message, 200, "Expected pattern '(d+:d+:d+)' not found for AOS %s. Please check the query syntax.\n", aos_name.c_str());
        throw ALPluginException(message);
    }
}

// Returns the position of the current AOS. Position = 0 is the highest one in the nested AOSs hierarchy.
void PartialGetPlugin::getAOSPosition(ArraystructContext *actx, int *position)
{
    *position = 0;
    ArraystructContext *parent = actx->getParent();
    while (parent != NULL)
    {
        (*position)++;
        parent = parent->getParent();
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
    std::string fullDataObjectName;
    LLplugin::getFullPath(ctx, fieldPath, full_path, fullDataObjectName);

    if (ctxt->getType() == CTX_ARRAYSTRUCT_TYPE) // data field is located inside an AOS element
    { 
        std::vector<int> indices;
        int timedAOS_index = 0;
        getAOSIndices(ctxt, indices, &timedAOS_index); // getting the current indices of each AOS

        ArraystructContext *arctx = dynamic_cast<ArraystructContext *>(ctxt);

        std::string debug = "";
        for (int i = 0; i < indices.size(); i++)
        {
            debug += std::to_string(indices[i]) + ";";
        }

        if (requested_indices(arctx, full_path)) // the current AOS indices are included in the requested AOS indices
        {
            if (belongsToQueriesPaths(full_path))
            {
                LOG_DEBUG << "PartialGetPlugin::read_data, including full path=%s" << full_path.c_str() << ", indices=" << debug.c_str();
                al_plugin_read_data(ctx, fieldPath, timeBasePath, data, datatype, dim, size); // in case of exception, the LL will catch it
                return 1;
            }
            else
                return 0;
        }
        else
        { // the current AOS indices are NOT included in the requested AOS indices, we just display a debug message
            LOG_DEBUG << "PartialGetPlugin::read_data, excluding full path=" << full_path.c_str() << ", indices=" << debug.c_str();
            return 0;
        }
    }

    if (!belongsToQueriesPaths(full_path))
        return 0;

    al_plugin_read_data(ctx, fieldPath, timeBasePath, data, datatype, dim, size);

    return 1;
}

// Returns true if the AOSs elements indices belong to the requested indices 'aos_indices'
bool PartialGetPlugin::requested_indices(ArraystructContext *arctx, const std::string &full_path)
{
    std::string aos_full_path;
    std::string fullDataObjectName;
    ArraystructContext *p = arctx;

    bool is_requested = false;

    while (p != NULL)
    {
        std::string rpath;
        getAOSPaths(p, rpath, aos_full_path); // extracts the paths of the AOS (e.g. magnetics:0/flux_loop/position for the full path)
        auto got = aos_indices.find(aos_full_path);

        if (got != aos_indices.end())
        {
            auto &requested_indices = got->second;
            if (!isRequested(requested_indices, p->getIndex()))
            {
                LOG_DEBUG << "PartialGetPlugin::excluding full path (includes)=" << full_path.c_str() << ", fortran_index=" << p->getIndex() + 1;
                return false;
            }
            else // Ok, the current AOS is in the includes, we check now if there is an 'exclude' request which excludes these data
            {
                if (belongsToExcludesAOSQueriesPaths(full_path)) {
                    getAOSPaths(arctx, rpath, aos_full_path); //we need the most inner AOS path for such exclude query, so we use arctx pointer
                    auto got_excludes = aos_excludes_indices.find(aos_full_path); 
                    if (got_excludes != aos_excludes_indices.end())
                    {
                        auto &requested_excludes_indices = got_excludes->second;
                        if (isRequested(requested_excludes_indices, p->getIndex())) // the AOS element index belongs to the excluded ones, so the data is ignored
                        { 
                            LOG_DEBUG << "PartialGetPlugin::excluding full path (excludes)=" << full_path.c_str() << ", fortran_index=" << p->getIndex() + 1;
                            return false;
                        }
                        else {
                            LOG_DEBUG << "excludes found for " << aos_full_path << " but no exclusion performed";
                        }
                    }
                    else {
                        LOG_DEBUG << "no excludes found for AOS path= " << arctx->getPath() << " while visiting full_path= " << full_path ;
                    }
                }
                else
                {
                    //LOG_DEBUG << "ne exclude query found for the dataset full path"; 
                }
            }
            is_requested = true;
        }
        else
        {
            /**This AOS is not present in the query, however we still need to look at all of 
             * its parents to check if it is requested or not*/
        }
        p = p->getParent(); // we need to check the parent too
    }
    return is_requested;
}

// Check if an AOS index belongs to the requested_indices vector passed in argument
bool PartialGetPlugin::isRequested(const std::vector<int> &requested_indices, int index)
{
    int fortran_index_conv = index + 1;

    if (requested_indices.size() == 1 && requested_indices.back() == -1) {
        LOG_DEBUG << "index = " << fortran_index_conv << " ; requested index is -1, so returning true";
        return true;
    }
    
    int min = requested_indices[0];
    int max = requested_indices[1];
    int inc = requested_indices[2];
    bool requested;
    if (inc != 0) {
        requested = (fortran_index_conv - min) % inc == 0 && fortran_index_conv <= max && fortran_index_conv >= min;
    }
    else {
        requested = fortran_index_conv <= max && fortran_index_conv >= min;
    }
    LOG_DEBUG << "index = " << fortran_index_conv << " ; min=" << min << " ; max=" << max << " ; inc=" << inc << "; requested = " << requested;
    return requested;
}

// Build all queries paths
void PartialGetPlugin::buildQueriesPaths()
{
    buildRequestedPaths(query_includes, requested_includes_paths);
    buildRequestedPaths(query_excludes, requested_excludes_paths);

    //Checking if an 'excludes' quey is a subset of at least one 'includes' query
    for (int i = 0; i < requested_excludes_paths.size(); i++)
    {
        auto &exclude_path = requested_excludes_paths[i];

        bool is_exclude_query_valid = false;

        if (query_includes.size() == 0) {
            is_exclude_query_valid = true;
        }
        else {
            for (int j = 0; j < requested_includes_paths.size(); j++)
            {
                auto include_path = requested_includes_paths[j];

                 if (!include_path.empty() && include_path.back() == '&') 
                    include_path.pop_back();

                if (exclude_path.find(include_path) == std::string::npos)
                {
                    continue; //the exclude query is not a subset (a subpath) of any include query path
                }
                else
                {
                    is_exclude_query_valid = true;
                    break;
                }
            }
        }
        
        if (!is_exclude_query_valid)
        {
            char message[200];
            if(exclude_path.back() == '&') 
                exclude_path.pop_back();
            snprintf(message, 200, "The 'excludes' query:'%s' is not a subset of any 'includes' query.\n", exclude_path.c_str());
            throw ALPluginException(message);
        }
    }

}

// Build the queries paths. It consists in removing all indices which appear in each query string , keep only AOSs and structures names to build all possible paths
void PartialGetPlugin::buildRequestedPaths(const std::vector<std::string> &queries, std::vector<std::string> &result)
{
    result.clear();

    for (int i = 0; i < queries.size(); i++)
    {
        std::string query = queries[i];

        bool aos_query = false;
        if(query.back() == ')') aos_query = true;

        std::string from = ")/";
        std::string to = ")&";
        size_t start_pos = 0;
        while ((start_pos = query.find(from, start_pos)) != std::string::npos)
        {
            query.replace(start_pos, from.length(), to);
            start_pos += to.length();
        }

        std::replace(query.begin(), query.end(), '/', '&');

        std::smatch matches;
        std::string p = "\\(:\\)"; //we remove all patterns like (:) because we are only interested in the path 
        std::regex pattern(p);
        if (std::regex_search(query, matches, pattern)) {
            LOG_DEBUG << "Pattern " <<  p << " found in query: " << query;   
            query = std::regex_replace(query, pattern, "");
        }

        p = "\\(\\d+:\\d+:\\d+\\)"; //we remove all paterns like (?:?:?) because we are only interested in the path 
        std::regex pattern2(p);
        if (std::regex_search(query, matches, pattern2)) {
            LOG_DEBUG << "Pattern " <<  p << " found in query: " << query;
            query = std::regex_replace(query, pattern2, "");
        } 
        else {
            LOG_DEBUG << "Pattern " <<  p << " not found in : " << query;    
        }  

        std::replace(query.begin(), query.end(), '&', '/');

        //if query is an AOS query (ending with ")"), we add an '&' to mark it as an AOS path
        if(aos_query) 
            query += "&";

        result.push_back(query);
        LOG_DEBUG << "Path added: " << query;    
    }
}

// Check if it exists a 'excludes' query with path included in the fill path of a dataset
bool PartialGetPlugin::belongsToExcludesAOSQueriesPaths(const std::string &full_path)
 {
     for (int i = 0; i < requested_excludes_paths.size(); i++) {
        auto exclude_path = requested_excludes_paths[i];
        if (exclude_path.empty() || exclude_path.back() != '&') 
            continue; 
        exclude_path.pop_back();
        if (full_path.find(exclude_path) != std::string::npos) {
            LOG_DEBUG << "Found AOS exclude path=: " << exclude_path; 
            return true;
        }
     }
     LOG_DEBUG << "No AOS path found"; 
     return false;
 }

// Check if a dataset path is included/excluded according to both 'excludes' and 'includes' queries paths
bool PartialGetPlugin::belongsToQueriesPaths(const std::string &full_path)
 {
    if (requested_excludes_paths.size() == 0 && requested_includes_paths.size() == 0)
        return true;

     for (int i = 0; i < requested_excludes_paths.size(); i++) {
        auto &exclude_path = requested_excludes_paths[i];
        if (!exclude_path.empty() && exclude_path.back() == '&') 
            continue; //we ignore queries on AOSs since these queries have been already taken into account using query indices
        if (full_path.find(exclude_path) != std::string::npos) {
            LOG_DEBUG << "path: " << full_path << " is excluded from 'excludes' queries paths by the excluded path: " << exclude_path;   
            return false;
        }
     }

     bool isRequested = false;
     if (query_includes.size() == 0) {
        isRequested = true;
     }
     else {
        for (int i = 0; i < requested_includes_paths.size(); i++) {
            auto &include_path = requested_includes_paths[i];
            if (!include_path.empty() && include_path.back() == '&')
                include_path.pop_back();
            //LOG_DEBUG << "include_path: " << include_path;   
            if (full_path.find(include_path) != std::string::npos) {
                isRequested = true;
                break;

            }
        }
     }
     
     if (!isRequested)
        LOG_DEBUG << "path: " << full_path << " is not included in 'includes' queries paths";   
     return isRequested;
 }

// Returns the list of indices for AOSs requested during a read_data() operation
void PartialGetPlugin::getAOSIndices(Context *ctx, std::vector<int> &indices, int *timedAOS_index)
{
    *timedAOS_index = -1;
    if (ctx->getType() == CTX_ARRAYSTRUCT_TYPE) {
        int i = 0;
        ArraystructContext *arrCtx = dynamic_cast < ArraystructContext * >(ctx);
        if (arrCtx->getTimed()) {
            *timedAOS_index = i;
        }
        indices.push_back(arrCtx->getIndex());
        ArraystructContext *parent = arrCtx->getParent();
        while (parent != NULL)
        {
            i++;
            if (parent->getTimed()) {
                *timedAOS_index = i;
            }
            indices.push_back(parent->getIndex());
            parent = parent->getParent();
        }
        std::reverse(indices.begin(), indices.end());
        if (*timedAOS_index != -1)
            *timedAOS_index =  indices.size() - *timedAOS_index - 1;
    }
}

// Build the relative path and the absolute path of a given AOS (e.g. magnetics:0/flux_loop/position for the 'position' AOS of a magnetics IDS)
void PartialGetPlugin::getAOSPaths(ArraystructContext *c, std::string &rpath, std::string &full_path)
{
    std::string path = "";
    OperationContext *opctx = c->getOperationContext();
    std::string fullDataObjectName = opctx->getDataobjectName();
    size_t found_sep = fullDataObjectName.find("/");
    if (found_sep == std::string::npos)
        fullDataObjectName += ":0";
    else
        fullDataObjectName = std::regex_replace(fullDataObjectName, std::regex("/"), ":");

    rpath = c->getPath();
    path = rpath;
    c = c->getParent();

    while (c != NULL)
    {
        path = c->getPath() + "/" + path;
        c = c->getParent();
    }
    full_path = fullDataObjectName + "/" + path;
}

// Splits a string into tokens separate by the 'delimiter' char, then the tokens are returned
std::vector<std::string> PartialGetPlugin::split(const std::string &str, char delimiter)
{
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter))
    {
        tokens.push_back(token);
    }
    return tokens;
}

void PartialGetPlugin::cleanQueries(std::string &query, bool includes) {

    std::regex special_chars_re("[&\\[\\]\\*\\?]");
    bool detect_bad_characters = std::regex_search(query, special_chars_re);
    if (detect_bad_characters) {
        std::string query_type = "includes";
        if (!includes)
            query_type = "excludes";
        std::string message = "PartialGetPlugin: found a bad character ('&' or '[' or ']' or '*' or '?') in the '" + query_type + "' queries.";
        throw ALPluginException(message.c_str(), LOG);
    }

    //Clean the query for redondant and useless characters
    std::regex space_re("\\s+");
    query = std::regex_replace(query, space_re, ""); 
    std::regex semic_re(R"(;{2,})");
    query = std::regex_replace(query, semic_re, ";"); 
    std::regex slash_re(R"(;/*)");
    query = std::regex_replace(query, slash_re, ";"); 
    std::regex slash_re2(R"(\*/;)");
    query = std::regex_replace(query, slash_re2, ";");
}

//Returns the list of AOS paths ('aos_queries') for which the user has defined a query (e.g.'flux_loop(2:10:3)/position(0:1:1)' contains 2 AOS queries)
//The query string is expected to be an AOS query, e.g. a query containing AOS indices "(i:j:k)"
void PartialGetPlugin::findAOSPathsFromQuery(std::string &query, std::vector<std::string> &aos_queries)
{
    std::string from = ")/";
    std::string to = ")&";
    size_t start_pos = 0;
    while ((start_pos = query.find(from, start_pos)) != std::string::npos)
    {
        query.replace(start_pos, from.length(), to);
        start_pos += to.length();
    }

    aos_queries = split(query, '&'); //aos_queries will be empty if there is no AOS indices in the 'query' string
    for (int i = 0; i < aos_queries.size(); i++)
    {
        std::string &aos_query = aos_queries[i];
        auto query_parts = split(aos_query, '(');
        aos_query = query_parts[0];
    }
}

// Check if a given query is relevant for the current AOS path visited by the HLI iterator
// query is a full path which may end with the name of the IDS or may end with the name of a structure (will result into data decimation using AOS in first case, and data decimation using structure in the second case )
// remember, actx->getPath() is a relative path which ends with the name of the AOS; the path may be exactly the name of the AOS or may start from 1 or several nested structures, like 'struct1/struct2/aos_name'
bool PartialGetPlugin::checkIfQueryMatchesCurrentPath(const std::string &query, ArraystructContext *actx)
{
    //Check if the query is related to an AOS, otherwise we ignore it
    if (query.find("(") == std::string::npos || query.find(")") == std::string::npos) {
        return false;
    }
    std::vector<std::string> aos_queries;
    std::string query_copy = query;
    findAOSPathsFromQuery(query_copy, aos_queries);
    if (aos_queries.size() == 0)
        return false;

    int AOSposition;
    getAOSPosition(actx, &AOSposition);

    if (AOSposition >= aos_queries.size())
       return false;

    if (aos_queries[AOSposition] + "(:)" == query)
        return true;

    if (actx->getPath() != aos_queries[AOSposition])
        return false;

    // now we check that all ancestors of the current AOS is in the query
    ArraystructContext *parent = actx->getParent();
    int AOSpos;
    while (parent != NULL)
    {
        getAOSPosition(parent, &AOSpos);
        if (parent->getPath() != aos_queries[AOSpos])
            return false;
        parent = parent->getParent();
    }

    if (actx->getTimed() && (operation == SLICE_OP || operation == TIMERANGE_OP) ) {
        LOG_WARNING << "Partial request on timed array of structures is ignored for slice and time range operations.";
        return false;
    }

    LOG_DEBUG << "the sub query: " << aos_queries[AOSposition] << " matches the current AOS path: " << actx->getPath();
    return true;
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
