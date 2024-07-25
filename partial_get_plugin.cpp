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
#define PLUGIN_REPOSITORY "ssh://git@git.iter.org/imas/access-layer.git"
#define PLUGIN_DESCRIPTION "The 'partial_get' plugin builds low level partial requests from (HLI) user query."

#define QUERY_INCLUDES "includes"                                  //query 'includes' parameter (string) set in HLI
#define QUERY_EXCLUDES "excludes"                                  //query 'excludes' parameter (string) set in HLI
#define DEBUG_OPTION "debug"                                       //Enable full debug of the plugin (integer 0 or 1)
#define DEBUG_READ_REQUESTS_ONLY_OPTION "debug_read_requests_only" //Displays only READ requests when 'debug' is enabled

PartialGetPlugin::PartialGetPlugin()
    : aos_indices(), aos_excludes_indices(), aos_queries(), aos_excludes_queries(), aos_queries_index(), aos_excludes_queries_index(), display_only_debug_read_requests(false), query_includes(), query_excludes()
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

void PartialGetPlugin::setParameter(const char *parameter_name, int datatype, int dim, int *size, void *data)
{
    std::string parameter = parameter_name;
    if (dim == 1)
    {
        if (parameter == QUERY_INCLUDES)
        {
            char *p = strdup((char *)data);
            std::string includes = std::string(p);
            LOG_DEBUG << "setting includes:" << parameter.c_str();
            typedef boost::tokenizer<boost::char_separator<char>> tokenizer;
            boost::char_separator<char> sep{";"}; //separator between each include parameter
            tokenizer tok{includes, sep};
            for (const auto &t : tok)
            {
                query_includes.push_back(t);
            }
        }
        else if (parameter == QUERY_EXCLUDES)
        {
            char *p = strdup((char *)data);
            std::string excludes = std::string(p);
            LOG_DEBUG << "setting excludes:" << parameter.c_str();
            typedef boost::tokenizer<boost::char_separator<char>> tokenizer;
            boost::char_separator<char> sep{";"}; //separator between each exclude parameter
            tokenizer tok{excludes, sep};
            for (const auto &t : tok)
            {
                query_excludes.push_back(t);
            }
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

void PartialGetPlugin::begin_global_action(int pulseCtx, const char *dataobjectname, const char *datapath, int mode, int opCtx)
{
}

void PartialGetPlugin::begin_slice_action(int pulseCtx, const char *dataobjectname, int mode, double time, int interp, int opCtx)
{
    throw ALPluginException("PartialGetPlugin: slice operation not supported", LOG);
}

void PartialGetPlugin::begin_arraystruct_action(int ctx, int *aosctx, const char *fieldPath, const char *timeBasePath, int *arraySize)
{
    std::string field = fieldPath;

    //Creation of an AOS
    LLenv lle = Lowlevel::getLLenv(ctx);
    ArraystructContext *actx = lle.create(fieldPath, timeBasePath);
    *aosctx = Lowlevel::addLLenv(lle.backend, actx);
    lle.backend->beginArraystructAction(actx, arraySize);

    std::string aos_full_path;
    std::string fullDataObjectName;
    std::string rpath;
    getAOSPaths(actx, field, rpath, aos_full_path); //extracts some paths of the AOS (e.g. magnetics:0/flux_loop/position for the full path or flux_loop/position for the rpath )
    if (!display_only_debug_read_requests)
        LOG_DEBUG << "fieldPath= " << fieldPath << "; aos_full_path= " << aos_full_path.c_str() << "; rpath= " << rpath.c_str();

    //if 'includes' parameter is empty, all fields are included by default
    if (query_includes.size() == 0) {
        std::vector<int> requested_indices{-1}; //storing -1 means that all indices of this AOS are requested
        aos_indices[aos_full_path] = requested_indices;
    }
    
    for (int i = 0; i < query_excludes.size(); i++)
    {
        const std::string &query = query_excludes[i];
        std::string aos_query = setAOSQuery(actx, query, aos_full_path, i, aos_excludes_queries, aos_excludes_queries_index); //Stores the AOS query in the 'aos_queries' map
        if (!display_only_debug_read_requests && aos_query != "")
            LOG_DEBUG << "buildRequestedAOSIndices for 'excludes' query: " << aos_query;

        buildRequestedAOSIndices(field, aos_query, aos_full_path, i, aos_excludes_indices, false); //builds the requested AOS indices for the 'excludes' query
    }

    for (int i = 0; i < query_includes.size(); i++)
    {
        const std::string &query = query_includes[i];
        std::string aos_query = setAOSQuery(actx, query, aos_full_path, i, aos_queries, aos_queries_index); //Stores the AOS query in the 'aos_queries' map
        if (!display_only_debug_read_requests && aos_query != "")
            LOG_DEBUG << "buildRequestedAOSIndices for 'includes' query: " << aos_query;

        buildRequestedAOSIndices(field, aos_query, aos_full_path, i, aos_indices, true); //builds the requested AOS indices for the 'includes' query
    }

    if (*arraySize == 0)
    {
        LOG_DEBUG << "PartialGetPlugin::begin_arraystruct_action, excluding all elements (since size is 0) from AOS path=%s" << aos_full_path.c_str();
        lle.backend->endAction(actx);
    }
}

//Stores the AOS query in the 'aos_queries' map whose key is the aos full_path
std::string PartialGetPlugin::setAOSQuery(ArraystructContext *actx, const std::string &query,
                                          const std::string &aos_full_path, int query_index, std::map<std::string, std::string> &aos_queries_, std::map<std::string, int> &aos_queries_index_)
{
    std::string aos_query = ""; //empty value means that query is undefined for this AOS
    auto it = aos_queries_index_.find(aos_full_path);

    if (!display_only_debug_read_requests)
        LOG_DEBUG << "query=" << query << "; aos_full_path=" << aos_full_path << "; query_index = " << query_index;

    if (it != aos_queries_index_.end()) //this AOS path has been already visited and a query was found for this path
    {
        if (it->second != query_index)
        { //a different query already exists for this AOS path
            std::string message = "PartialGetPlugin: found at least 2 different queries for the same path: " + aos_full_path;
            throw ALPluginException(message.c_str(), LOG);
        }
        else
        { //it's the same query than found previously
            aos_query = it->second;
            return aos_query;
        }
    }

    getAOSQuery(actx, query, aos_query);

    if (aos_query != "")
    { //a user query is defined for this AOS path
        LOG_DEBUG << "adding aos_query =" << aos_query << "; query_index = " << query_index << " for path " << aos_full_path;
        aos_queries_[aos_full_path] = aos_query; //aos_queries map contains only non empty queries
        aos_queries_index_[aos_full_path] = query_index;
    }
    else
    {
        LOG_DEBUG << "found empty aos_query";
    }

    return aos_query;
}

//Extracts the query 'aos_query' of an AOS, for example 'flux_loop(1:5:2)' from a given user query 'query'
void PartialGetPlugin::getAOSQuery(ArraystructContext *actx, const std::string &query, std::string &aos_query)
{
    int position = -1;
    //Getting the position of the current AOS given by the actx pointer
    getAOSPosition(actx, &position); //position = 0 means first AOS, position = 1 means second nested AOS, and so on
    //printf("position=%d\n", position) ;
    typedef boost::tokenizer<boost::char_separator<char>> tokenizer;
    boost::char_separator<char> sep{"/"}; //separator between AOS in the query (e.g query "a(2:3:1)/b(3:7:1)" where a and b are AOSs)
    tokenizer tok{query, sep};
    std::vector<std::string> tokens;
    for (const auto &t : tok)
    {
        tokens.push_back(t);
        //std::cout << t << '\n';
    }
    if (tokens.size() > position)
    { //is there a query at the position of the current AOS?
        aos_query = tokens[position];

        if (aos_query == actx->getPath())
        {
            aos_query = actx->getPath() + "(:)";
        }
        else if (aos_query == actx->getPath() + "(:)")
        {
        }
        /*else if (aos_query == actx->getPath() + "(-1)") { //TODO
        }*/
        else
        {
            //printf("testing aos_query=%s for path=%s\n", aos_query.c_str(), actx->getPath().c_str()) ;
            std::string replacement = "";
            std::string p = "\\((\\d+):?(\\d+)?:?(\\d+)?\\)";
            std::regex pattern(p);
            std::string aos_path = std::regex_replace(aos_query, pattern, replacement);
            //printf("testing aos_path=%s for query=%s\n", aos_path.c_str(), aos_query.c_str()) ;
            if (actx->getPath() != aos_path)
            {
                LOG_DEBUG << "returning empty string since paths differ";
                aos_query = ""; //query has no effect for the current AOS path
            }
        }
    }
    else
    {
        //LOG_DEBUG << "returning empty string since position < tokens.size()";
        aos_query = ""; //query has no effect for the current AOS path
    }
}

void PartialGetPlugin::buildRequestedAOSIndices(const std::string &aos_name, const std::string &aos_query,
                                                const std::string &aos_full_path, int query_index, std::map<std::string, std::vector<int>> &aos_indices_, bool includes)
{

    //LOG_DEBUG << "getting request for " << aos_full_path << "; query_index=" << query_index << "; aos_query=" << aos_query;
    if (aos_indices_.find(aos_full_path) != aos_indices_.end()) //requested indices for this AOS path have been already set for this query index (AOS already visited)
        return;

    /*if (aos_query == aos_name + "(-1)") 
    return;*/
    //in case of an empty 'includes' query, we initialize aos_indices to -1, meaning we take all data by default
    /*if ((aos_query == "" && includes) || (aos_query == aos_name + "(:)"))
    {
        if (!display_only_debug_read_requests)
            LOG_DEBUG << "full request for " << aos_full_path << "; query_index=" << query_index << "; aos_query=" << aos_query;
        std::vector<int> requested_indices{-1}; //storing -1 means that all indices of this AOS are requested
        aos_indices_[aos_full_path] = requested_indices;
        return;
    }*/
    if (aos_query == "")
        return;

    if (!display_only_debug_read_requests)
        LOG_DEBUG << "partial request for " << aos_full_path << "; query_index=" << query_index << "; aos_query=" << aos_query;


    std::vector<int> requested_indices;
    getIndices(aos_name, aos_query, requested_indices);

    if (requested_indices.size() > 0)
    {
        aos_indices_[aos_full_path] = requested_indices;
    }
}

//Extract the AOS indices from the query on AOSs
void PartialGetPlugin::getIndices(const std::string &aos_name, const std::string &aos_query, std::vector<int> &indices)
{

    std::smatch matches;
    std::string p = "(" + aos_name + ")\\((\\d+):?(\\d+)?:?(\\d+)?\\)";
    std::regex pattern(p);

    if (std::regex_search(aos_query, matches, pattern))
    {
        //std::cout << "matches = " << matches.size() << std::endl;

        int minIndex = std::stoi(matches[2]);
        int maxIndex = minIndex + 1;

        if (matches[3] != "")
            maxIndex = std::stoi(matches[3]);

        int inc = 1;
        if (matches[4] != "")
            inc = std::stoi(matches[4]);

        if (!display_only_debug_read_requests)
            LOG_DEBUG << "minIndex = " << minIndex << ", maxIndex = " << maxIndex << ", inc=" << inc;

        std::vector<int> requested_indices;
        indices.push_back(minIndex);
        indices.push_back(maxIndex);
        indices.push_back(inc);
    }
    else
    {
        LOG_DEBUG << "no pattern found for " << aos_name.c_str();
    }
}

std::string PartialGetPlugin::getAOSName(const char *fieldPath)
{
    std::string field = fieldPath;
    boost::char_separator<char> sep("/");
    boost::tokenizer<boost::char_separator<char>> tokens(field, sep);
    std::string aos_name = *tokens.begin();
    return aos_name;
}

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
    LLenv lle = Lowlevel::getLLenv(ctx);
    Context *ctxt = lle.context;

    if (ctxt->getType() == CTX_ARRAYSTRUCT_TYPE)
    { //data field is located inside an AOS element

        std::vector<int> indices;
        int timedAOS_index = 0;
        getAOSIndices(ctxt, indices, &timedAOS_index);

        ArraystructContext *arctx = dynamic_cast<ArraystructContext *>(ctxt);

        if (requested_indices(arctx))
        {

            std::string full_path;
            std::string fullDataObjectName;
            LLplugin::getFullPath(ctx, fieldPath, full_path, fullDataObjectName);

            std::string debug = "";
            for (int i = 0; i < indices.size(); i++)
            {
                debug += std::to_string(indices[i]) + ";";
            }
            LOG_DEBUG << "PartialGetPlugin::read_data, including full path=%s" << full_path.c_str() << ", indices=" << debug.c_str();
            //std::cout  << "PartialGetPlugin::read_data, including full path=%s" << full_path.c_str() <<  ", indices=" << debug.c_str();
            al_status_t status = al_plugin_read_data(ctx, fieldPath, timeBasePath, data, datatype, dim, size);
            if (status.code != 0)
            {
                throw ALPluginException("Error calling al_plugin_read_data in PartialGetPlugin plugin.");
            }
            else
            {
                return 1;
            }
        }
        else
        {
            std::string full_path;
            std::string fullDataObjectName;
            LLplugin::getFullPath(ctx, fieldPath, full_path, fullDataObjectName);
            std::string debug = "";
            for (int i = 0; i < indices.size(); i++)
            {
                debug += "; " + std::to_string(indices[i]);
            }
            LOG_DEBUG << "PartialGetPlugin::read_data, excluding full path=" << full_path.c_str() << ", indices=" << debug.c_str();
            return 0;
        }
    }

    al_status_t status = al_plugin_read_data(ctx, fieldPath, timeBasePath, data, datatype, dim, size);
    if (status.code != 0)
    {
        throw ALPluginException("Error calling al_plugin_read_data in PartialGetPlugin plugin.");
    }
    return 1;
}

void PartialGetPlugin::write_data(int ctx, const char *fieldPath, const char *timeBasePath, void *data, int datatype, int dim, int *size)
{
    throw ALPluginException("PartialGetPlugin: write_data not supported", LOG);
}

void PartialGetPlugin::end_action(int ctx)
{
    LLenv lle = Lowlevel::getLLenv(ctx);
    Context *ctxt = lle.context;

    if (ctxt->getType() == CTX_OPERATION_TYPE)
    {
        aos_indices.clear();
        aos_queries.clear();
    }
}

//Returns true if the current AOS indices belong to the requested indices of the user query
bool PartialGetPlugin::requested_indices(ArraystructContext *arctx)
{

    std::string aos_full_path;
    std::string fullDataObjectName;
    std::string aos_name;
    ArraystructContext *p = arctx;

    while (p != NULL)
    {
        //we extract the exact name of the AOS
        aos_name = getAOSName(p->getPath().c_str()); //extracts the name of the AOS (it can differs from its path!)
        //we extract the path of the AOS
        std::string rpath;
        getAOSPaths(p, aos_name, rpath, aos_full_path); //extracts the paths of the AOS (e.g. magnetics:0/flux_loop/position for the full path)
        auto got = aos_indices.find(aos_full_path);
        auto got_excludes = aos_excludes_indices.find(aos_full_path);
        if (got != aos_indices.end())
        {
            auto &requested_indices = got->second;
            /*if (requested_indices.back() == -1) { //all indices requested for this AOS, filter of type "AOS(:)]"
                //p = p->getParent();
                //continue;
                if (got_excludes != aos_excludes_indices.end()) {
                    auto &requested_excludes_indices = got_excludes->second;
                    if (isRequested(requested_excludes_indices, p->getIndex())) //the AOS element index belongs to the excluded ones, so the data is ignored
                        return false;
                    else
                        continue;
                }
                else {
                    continue;
                }
            }
            else {*/
            if (!isRequested(requested_indices, p->getIndex())) {
                LOG_DEBUG << "PartialGetPlugin::excluding AOS full path (includes)=" << aos_full_path.c_str() << ", index=" << p->getIndex();
                return false;
            }
            else
            {
                /*for (const auto& entry : aos_excludes_indices) {
                std::cout << "Key: " << entry.first << std::endl;
                std::cout << "Values: ";
                for (int value : entry.second) {
                    std::cout << value << " ";
                }
                std::cout << std::endl;
                }*/
    
                //printf("aos_excludes_indices.size()=%d\n", aos_excludes_indices.size());
                //printf("0PartialGetPlugin::looking at excludes for AOS full path (excludes)=%s\n", aos_full_path.c_str(), p->getIndex());
                if (got_excludes != aos_excludes_indices.end())
                {   
                    //printf("1PartialGetPlugin::excluding AOS full path (excludes)=%s for index=%d\n", aos_full_path.c_str(), p->getIndex());
                    auto &requested_excludes_indices = got_excludes->second;
                    if (isRequested(requested_excludes_indices, p->getIndex())) {//the AOS element index belongs to the excluded ones, so the data is ignored 
                        LOG_DEBUG << "PartialGetPlugin::excluding AOS full path (excludes)=" << aos_full_path.c_str() << ", index=" << p->getIndex();
                        //printf("2PartialGetPlugin::excluding AOS full path (excludes)=%s for index=%d\n", aos_full_path.c_str(), p->getIndex());
                        return false;
                    }
                    else
                        LOG_DEBUG << "excludes found for " << aos_full_path.c_str() << " but no exclusion performed";
                }
                else
                {
                    //continue;
                    LOG_DEBUG << "no excludes found for " << aos_full_path.c_str();
                }
            }
            //}
        }
        else
        {
            return false;
        }
        p = p->getParent();
    }
    return true;
}

bool PartialGetPlugin::isRequested(const std::vector<int> &requested_indices, int index)
{
    if (requested_indices.size() == 1 && requested_indices.back() == -1)
        return true;
    int min = requested_indices[0];
    int max = requested_indices[1];
    int inc = requested_indices[2];
    bool requested = (index - min) % inc == 0 && index < max && index >= min;
    LOG_DEBUG << "index = " << index << " ; min=" << min << " ; max=" << max << " ; inc=" << inc << "; requested = " << requested;
    return requested;
}

//Returns the list of indices of the arrays of structures requested during a read_data() operation
void PartialGetPlugin::getAOSIndices(Context *ctx, std::vector<int> &indices, int *timedAOS_index)
{
    *timedAOS_index = -1;
    if (ctx->getType() == CTX_ARRAYSTRUCT_TYPE)
    {
        int i = 0;
        ArraystructContext *arrCtx = dynamic_cast<ArraystructContext *>(ctx);
        if (arrCtx->getTimed())
        {
            *timedAOS_index = i;
        }
        indices.push_back(arrCtx->getIndex());
        ArraystructContext *parent = arrCtx->getParent();
        while (parent != NULL)
        {
            i++;
            if (parent->getTimed())
            {
                *timedAOS_index = i;
            }
            indices.push_back(parent->getIndex());
            parent = parent->getParent();
        }
        std::reverse(indices.begin(), indices.end());
        if (*timedAOS_index != -1)
            *timedAOS_index = indices.size() - *timedAOS_index - 1;
    }
}

//Build the unique path of a given AOS (e.g. magnetics:0/flux_loop/position for the 'position' AOS of a magnetics IDS)
void PartialGetPlugin::getAOSPaths(ArraystructContext *c, const std::string &aos_name, std::string &rpath, std::string &full_path)
{

    std::string path = "";
    OperationContext *opctx = c->getOperationContext();
    std::string fullDataObjectName = opctx->getDataobjectName();
    size_t found_sep = fullDataObjectName.find("/");
    if (found_sep == std::string::npos)
        fullDataObjectName += ":0";
    else
        fullDataObjectName = std::regex_replace(fullDataObjectName, std::regex("/"), ":");

    c = c->getParent();
    if (c == NULL)
    {
        full_path = fullDataObjectName + "/" + aos_name;
        rpath = aos_name;
        return;
    }
    path = "";
    while (c != NULL)
    {
        path = c->getPath() + "/" + path;
        c = c->getParent();
    }

    full_path = fullDataObjectName + "/" + path + aos_name;
    rpath = path + aos_name;
}

void PartialGetPlugin::begin_timerange_action(int pulseCtx, const char *dataobjectname, int mode, double tmin, double tmax, std::vector<double> dtime, int interp, int opCtx)
{
}
