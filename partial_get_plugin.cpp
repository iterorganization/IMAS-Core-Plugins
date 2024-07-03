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


#define QUERY "query" //Name of the parameter to pass a query (string) from HLI 
#define NB_QUERIES_OPTION "nb_queries" //Number of queries (strings), default to 1
#define DEBUG_OPTION "debug" //Enable full debug of the plugin (integer 0 or 1)
#define DEBUG_READ_REQUESTS_ONLY_OPTION "debug_read_requests_only" //Displays only READ requests when 'debug' is enabled


PartialGetPlugin::PartialGetPlugin()
:aos_indices(), aos_queries(), aos_queries_index(), display_only_debug_read_requests(false), nb_queries(1), queries()
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

std::string PartialGetPlugin::getDescription() {
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
    std::string parameters = "nb_queries=" + std::to_string(nb_queries) + ";";
    for (int i = 0; i < queries.size(); i++) {
        parameters += "Query " + std::to_string(i) + " : " + queries[i] + "; ";
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

std::string PartialGetPlugin::getReadbackVersion(const std::string &path){
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
        if (parameter == QUERY) {
            char* p = strdup((char*) data);
            std::string query = std::string(p);
            queries.push_back(query);
            LOG_DEBUG << "adding query:" << parameter.c_str() << " to: " << query.c_str();
        }
    }
    else if (parameter == DEBUG_OPTION && dim == 0) {
            int *p_value = (int*) data;
            if (*p_value == 1) {
                std::clog.clear();
            }
            else {
                std::clog.setstate(std::ios_base::failbit); 
            }
    }
    else if (parameter == DEBUG_READ_REQUESTS_ONLY_OPTION && dim == 0) {
            int *p_value = (int*) data;
            display_only_debug_read_requests = *p_value;
    }
    else if (parameter == NB_QUERIES_OPTION && dim == 0) {
            int *p_value = (int*) data;
            nb_queries = *p_value;
    }
    else {
        std::string message = "PartialGetPlugin: parameter " + parameter  + " not supported";
        throw ALPluginException(message.c_str(), LOG);
    }
}

void PartialGetPlugin::begin_global_action(int pulseCtx, const char *dataobjectname, const char *datapath, int mode, int opCtx)
{
    if (queries.size() != nb_queries) {
        std::string message = "PartialGetPlugin: bad number of query strings (number of expected queries = " + std::to_string(nb_queries) + ")";
        throw ALPluginException(message.c_str(), LOG);
    }
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
    
    for (int i = 0; i < queries.size(); i++) {
        const std::string &query = queries[i];
        std::string aos_query = setAOSQuery(actx, query, aos_full_path, i); //Stores the AOS query in the 'aos_queries' map 
        if (!display_only_debug_read_requests && aos_query != "")
            LOG_DEBUG << "buildRequestedAOSIndices for " << aos_query;
        buildRequestedAOSIndices(field, aos_query, aos_full_path, i); //builds the requested AOS indices by the user for the given query
        if (aos_query == field + "(-1)") {
            *arraySize = 0;
        }
    }

    if (*arraySize == 0) {
        LOG_DEBUG  << "PartialGetPlugin::begin_arraystruct_action, excluding all elements (since size is 0) from AOS path=%s" << aos_full_path.c_str();
        lle.backend->endAction(actx);
    }
    
}

//Stores the AOS query in the 'aos_queries' map whose key is the aos full_path
std::string PartialGetPlugin::setAOSQuery(ArraystructContext *actx, const std:: string &query, const std:: string &aos_full_path, int query_index) {

    std::string aos_query = ""; //empty value means that query is undefined for this AOS
    auto it = aos_queries_index.find(aos_full_path);

    if (!display_only_debug_read_requests)
        LOG_DEBUG << "query=" << query << "; aos_full_path=" << aos_full_path << "; query_index = " << query_index;

    if (it != aos_queries_index.end()) { //a query already exists for this AOS path
        std::string message = "PartialGetPlugin: found at least 2 different queries for the same path: " + aos_full_path;
        throw ALPluginException(message.c_str(), LOG);
    }

    getAOSQuery(actx, query, aos_query);

    if (aos_query != "") { //a user query is defined for this AOS path
        LOG_DEBUG << "adding aos_query =" << aos_query << "; query_index = " << query_index << " for path " << aos_full_path;
        aos_queries[aos_full_path] = aos_query; //aos_queries map contains only non empty queries
    }
    else {
        LOG_DEBUG << "found empty aos_query";
    }
    
    return aos_query;
}

//Extracts the query 'aos_query' of an AOS, for example 'flux_loop(1:5:2)' from a given user query 'query'
void PartialGetPlugin::getAOSQuery(ArraystructContext *actx, const std::string &query, std::string &aos_query) {
    int position = -1;
    getAOSPosition(actx, &position); //position = 0 means first AOS, position = 1 means second nested AOS, and so on
    //printf("position=%d\n", position) ;
    typedef boost::tokenizer<boost::char_separator<char>> tokenizer;
    boost::char_separator<char> sep{"/"};
    tokenizer tok{query, sep};
    std::vector<std::string> tokens;
    for (const auto &t : tok) {
        tokens.push_back(t);
        //std::cout << t << '\n';
    }
    if (tokens.size() > position) {
        aos_query = tokens[position];
    
        if (aos_query == actx->getPath()) {
            aos_query = actx->getPath() + "(:)";
        }
        else if (aos_query == actx->getPath() + "(:)") {
        }
        else if (aos_query == actx->getPath() + "(-1)") { //TODO
        }
        else {
            //printf("testing aos_query=%s for path=%s\n", aos_query.c_str(), actx->getPath().c_str()) ;
            std::string replacement = "";
            std::string p = "\\((\\d+):?(\\d+)?:?(\\d+)?\\)";
            std::regex pattern(p);
            std::string aos_path  = std::regex_replace(aos_query, pattern, replacement);
            //printf("testing aos_path=%s for query=%s\n", aos_path.c_str(), query.c_str()) ;
            if (actx->getPath() != aos_path) {
                LOG_DEBUG << "returning empty string since paths differ";
                aos_query = "";
            }
        }
    }
    else {
        //LOG_DEBUG << "returning empty string since position < tokens.size()";
        aos_query = "";
    }
}

void PartialGetPlugin::buildRequestedAOSIndices(const std::string &aos_name, const std::string &aos_query, const std::string &aos_full_path, int query_index) {
  
  //LOG_DEBUG << "getting request for " << aos_full_path << "; query_index=" << query_index << "; aos_query=" << aos_query;
  if (aos_indices.find(aos_full_path) != aos_indices.end()) //these indices have been already found
    return;

  if (aos_query == aos_name + "(-1)") 
    return;

  if (aos_query == "" || (aos_query == aos_name + "(:)")) {
        if (!display_only_debug_read_requests)
            LOG_DEBUG << "full request for " << aos_full_path << "; query_index=" << query_index << "; aos_query=" << aos_query;
        std::vector<int> requested_indices{-1}; //storing -1 means that all indices of this AOS are requested
        aos_indices[aos_full_path] = requested_indices;
        //aos_queries_index[aos_full_path] = -1;
        return;
  }

  if (!display_only_debug_read_requests)
    LOG_DEBUG << "partial request for " << aos_full_path << "; query_index=" << query_index << "; aos_query=" << aos_query;

  std::smatch matches;
  std::string p = "(" + aos_name + ")\\((\\d+):?(\\d+)?:?(\\d+)?\\)";
  std::regex pattern(p);

  if (std::regex_search(aos_query, matches, pattern)) {
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
    requested_indices.push_back(minIndex);
    requested_indices.push_back(maxIndex);
    requested_indices.push_back(inc);

    aos_indices[aos_full_path] = requested_indices;
    //aos_queries_index[aos_full_path] = query_index;
  }

  else {
      LOG_DEBUG << "no pattern found for " << aos_name.c_str();
  }
}

std::string PartialGetPlugin::getAOSName(const char *fieldPath) {
    std::string field = fieldPath;
    boost::char_separator<char> sep("/");
    boost::tokenizer< boost::char_separator<char>> tokens(field, sep);
    std::string aos_name = *tokens.begin();
    return aos_name;
}

void PartialGetPlugin::getAOSPosition(ArraystructContext *actx, int *position) {
    *position = 0;
    ArraystructContext *parent = actx->getParent();
    while (parent != NULL) {
        (*position)++;
        parent = parent->getParent();
    }
}

int PartialGetPlugin::read_data(int ctx, const char *fieldPath, const char *timeBasePath,
                                void **data, int datatype, int dim, int *size)
{
    LLenv lle = Lowlevel::getLLenv(ctx);
    Context *ctxt = lle.context;

    if (ctxt->getType() == CTX_ARRAYSTRUCT_TYPE) { //data field is located inside an AOS element

        std::vector < int > indices;
        int timedAOS_index = 0;
        getAOSIndices(ctxt, indices, &timedAOS_index);

        ArraystructContext *arctx = dynamic_cast<ArraystructContext *>(ctxt);

        if (requested_indices(arctx)) {

            std::string full_path;
            std::string fullDataObjectName;
            LLplugin::getFullPath(ctx, fieldPath, full_path, fullDataObjectName);

            std::string debug = "";
            for (int i = 0; i < indices.size(); i++) {
                debug += "; " + std::to_string(indices[i]);
            }
            LOG_DEBUG  << "PartialGetPlugin::read_data, including full path=%s" << full_path.c_str() <<  ", indices=" << debug.c_str();
            //std::cout  << "PartialGetPlugin::read_data, including full path=%s" << full_path.c_str() <<  ", indices=" << debug.c_str();
            al_status_t status = al_plugin_read_data(ctx, fieldPath, timeBasePath, data, datatype, dim, size);
            if (status.code != 0) {
                throw ALPluginException("Error calling al_plugin_read_data in PartialGetPlugin plugin.");
            }
            else {
                return 1;
            }
        }
        else
        {
            std::string full_path;
            std::string fullDataObjectName;
            LLplugin::getFullPath(ctx, fieldPath, full_path, fullDataObjectName);
            std::string debug = "";
            for (int i = 0; i < indices.size(); i++) {
                debug += "; " + std::to_string(indices[i]);
            }
            LOG_DEBUG  << "PartialGetPlugin::read_data, excluding full path=" << full_path.c_str() << ", indices=" << debug.c_str();
            return 0;
        }
        
    }

    al_status_t status = al_plugin_read_data(ctx, fieldPath, timeBasePath, data, datatype, dim, size);
    if (status.code != 0) {
            throw ALPluginException("Error calling al_plugin_read_data in PartialGetPlugin plugin.");
    }
    return 1;
}

void PartialGetPlugin::write_data(int ctx, const char *fieldPath, const char *timeBasePath, void *data, int datatype, int dim, int *size)
{
    throw ALPluginException("PartialGetPlugin: write_data not supported", LOG);
}

void PartialGetPlugin::end_action(int ctx) {
    LLenv lle = Lowlevel::getLLenv(ctx);
    Context *ctxt = lle.context;

    if (ctxt->getType() == CTX_OPERATION_TYPE) {
        aos_indices.clear();
        aos_queries.clear(); 
    }

}

//Returns true if the current AOS indices belong to the requested indices of the user query
bool PartialGetPlugin::requested_indices(ArraystructContext *arctx) {

    std::string aos_full_path;
    std::string fullDataObjectName;
    std::string aos_name;
    ArraystructContext *p = arctx;

    while (p != NULL) {
        //we extract the exact name of the AOS
        aos_name = getAOSName(p->getPath().c_str()); //extracts the name of the AOS (it can differs from its path!)
        //we extract the path of the AOS
        std::string rpath;
        getAOSPaths(p, aos_name, rpath, aos_full_path); //extracts the paths of the AOS (e.g. magnetics:0/flux_loop/position for the full path)
        auto got = aos_indices.find(aos_full_path);
        if (got != aos_indices.end()) {
            auto& requested_indices = got->second;
            if (requested_indices.back() == -1) { //all indices requested for this AOS, filter of type "AOS(:)]"
                p = p->getParent();
                continue;
            }
            int index = p->getIndex();
            int min = requested_indices[0];
            int max = requested_indices[1];
            int inc = requested_indices[2];
            bool requested = (index - min) % inc == 0 && index < max;
            if (!requested)
                return false;
        }
        else {
            return false;
        }
        p = p->getParent();
    }
    return true;
}
//Returns the list of indices of the arrays of structures requested during a read_data() operation
void PartialGetPlugin::getAOSIndices(Context * ctx, std::vector < int >&indices, int *timedAOS_index)
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
        while (parent != NULL) {
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

//Build the unique path of a given AOS (e.g. magnetics:0/flux_loop/position for the 'position' AOS of a magnetics IDS)
void PartialGetPlugin::getAOSPaths(ArraystructContext *c,  const std::string &aos_name, std::string &rpath, std::string &full_path) {

  std::string path = "";
  OperationContext *opctx = c->getOperationContext();
  std::string fullDataObjectName = opctx->getDataobjectName();
  size_t found_sep = fullDataObjectName.find("/");
  if (found_sep == std::string::npos) 
        fullDataObjectName += ":0";
  else
        fullDataObjectName = std::regex_replace(fullDataObjectName, std::regex("/"), ":");

    c = c->getParent();
    if (c==NULL) {
        full_path = fullDataObjectName + "/" + aos_name;
        rpath = aos_name;
        return;
    }
    path = "";    
    while (c != NULL) {
      path = c->getPath() + "/" + path;
      c = c->getParent();
    } 

    full_path = fullDataObjectName + "/" + path + aos_name;
    rpath = path + aos_name;
}

void PartialGetPlugin::begin_timerange_action(int pulseCtx, const char* dataobjectname, int mode, double tmin, double tmax, std::vector<double> dtime, int interp, int opCtx)  {
}
