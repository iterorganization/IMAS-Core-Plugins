#ifndef PARTIAL_GET_PLUGIN_H
#define PARTIAL_GET_PLUGIN_H 1

#include <string>
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <iostream>

#include "extended_access_layer_plugin.h"


class PartialGetPlugin: public extended_access_layer_plugin
{

  private:
    std::vector<std::string> query_includes; //the client 'includes' queries
    std::vector<std::string> query_excludes; //the client 'excludes' queries

    std::map<std::string, std::string> aos_queries; //key = aos full path, value = AOS query
    std::map<std::string, std::string> aos_excludes_queries; //key = aos full path, value = AOS query

    std::map<std::string, int> aos_queries_index; //key = aos full path, value = index of query in the 'query_includes' vector
    std::map<std::string, int> aos_excludes_queries_index; //key = aos full path, value = index of query in the 'query_excludes' vector 

    std::map<std::string, std::vector<int>> aos_indices; //key = aos path, value = list of requested indices for 'includes' queries
    std::map<std::string, std::vector<int>> aos_excludes_indices; //key = aos path, value = list of requested indices for 'excludes' queries
    
    std::vector<std::string> requested_includes_paths; //contains the requested paths (one path per include path) which have to be included
    std::vector<std::string> requested_excludes_paths; //contains the requested paths (one path per exclude path) which have to be excluded

    bool display_only_debug_read_requests;
    int operation; 

    /**
     * Splits a string in tokens separate by the 'delimiter' char
     */ 
    std::vector<std::string> split(const std::string& str, char delimiter);

    /**
     * Returns the 'aos_queries' list of AOS paths for which the user has defined a query
     */
    void findAOSPathsFromQuery(std::string &query, std::vector<std::string> &aos_queries);

    void findAOSPathsFromQuery2(std::string &query, std::map<std::string, std::string> &aos_queries);

    /** Checks if a given query is relevant for the current AOS path visited by the HLI iterator.
        The 'query' is a full (IMAS) path which may end with the name of the IDS or may end with the name of a structure (will result into data decimation using AOS in the first case or data decimation using structure(s) in the second case).
        Remember, actx->getPath() is a relative path which ends with the name of the AOS; the path may be exactly the name of the AOS or may start with 1 or several nested structures, like 'struct1/struct2/aos_name'
     */
    bool checkIfQueryMatchesCurrentPath(const std::string &query,  ArraystructContext *actx);

    /**
     * Stores the AOS query in the 'aos_queries' map whose key is the aos 'full_path' argument  
     */
    std::string setAOSQuery(ArraystructContext *actx, const std:: string &query, const std:: string &aos_full_path, int query_index, 
      std::map<std::string, std::string> &aos_queries_, std::map<std::string, int> &aos_queries_index_);

    /**
     * Builds the AOS requested indices 'aos_indices' from the query 'aos_query' and save these indices into a map with key='aos_full_path'. The 'query_index' is the index of
     * the query string in the  
    */
    void buildRequestedAOSIndices(const std::string &aos_name, const std::string &aos_query, const std::string &aos_full_path, 
        std::map<std::string, std::vector<int>> &aos_indices, bool includes);

    /**
     * Returns the requested AOS indices ('indices') from the query 'aos_query'
    */
    void getIndices(const std::string &aos_name, const std::string &aos_query, std::vector<int> &indices);

    /**
     * Returns true if the AOS element 'index' belongs to the requested indices 'requested_indices' of the user query
    */
    bool isRequested(const std::vector<int> &requested_indices, int index);

    /**
     * Returns the list of indices for AOSs elements requested during a read_data() operation
     */
    void getAOSIndices(Context * ctx, std::vector < int >&indices, int *timedAOS_index);

    /**
     * Returns the position of the current AOS in its path. Position = 0 means the highest parent in the nested AOSs hierarchy.
    */
    void getAOSPosition(ArraystructContext *actx, int *position);

    /**
     * Extracts the query 'aos_query' for an AOS, for example the string 'flux_loop(1:5:2)' from a given user query ('query')
    */
    void getAOSQuery(ArraystructContext *actx, const std::string &query, std::string &aos_query);

    /**
     * Builds the relative and absolute paths of a given AOS (e.g. 'magnetics:0/flux_loop/position' for the 'position' AOS of a magnetics IDS)
     */
    void getAOSPaths(ArraystructContext *c,  std::string &rpath, std::string &full_path);

    /**
     * Returns true if the AOSs elements indices belong to the requested indices 'aos_indices'
     */
    bool requested_indices(ArraystructContext *arctx, const std::string &full_path);

    /**
     * Builds all queries paths and store them in 'requested_includes_structures' and 'requested_excludes_structures'
    */ 
    void buildQueriesPaths();

    /**
     * Builds the queries on structure(s) from the client queries. Called by the 'buildStructuresQueries()' function. 
    */
    void buildRequestedPaths(const std::vector<std::string> &queries, std::vector<std::string> &result);

    /**
     * Checks if a dataset is included/excluded according to both 'requested_includes_structures' and 'requested_excludes_structures' queries
    */
    bool belongsToQueriesPaths(const std::string &full_path);

    bool belongsToExcludesAOSQueriesPaths(const std::string &full_path);

    /**
     * Removes all spaces found in "includes" and "excludes" queries and checks the queries for bad characters.
    */
    void cleanQueries(std::string &queries, bool includes);

  public:
    PartialGetPlugin();
    ~PartialGetPlugin();

    std::string getName() override;
    std::string getDescription() override;
    std::string getCommit() override;
    std::string getVersion() override;
    std::string getRepository() override;
    std::string getParameters() override;
    plugin::OPERATION node_operation(const std::string &path) override;

    std::string getReadbackName(const std::string &path, int* index) override;
    std::string getReadbackDescription(const std::string &path) override;
    std::string getReadbackCommit(const std::string &path) override;
    std::string getReadbackVersion(const std::string &path) override;
    std::string getReadbackRepository(const std::string &path) override;
    std::string getReadbackParameters(const std::string &path) override;

    /**
    * Sets the user parameters 'includes', 'excludes' and debug option as plugin parameters
    */
    void setParameter(const char* parameter_name, int datatype, int dim, int *size, void *data) override;
    
    void begin_arraystruct_action(int ctx, int *aosctx, const char* fieldPath, const char* timeBasePath, int *arraySize) override;
    void begin_global_action(int pulseCtx, const char* dataobjectname, const char* datapath, int mode, int opCtx) override;
    void begin_slice_action(int pulseCtx, const char* dataobjectname, int mode, double time, int interp, int opCtx) override;
    void begin_timerange_action(int pulseCtx, const char* dataobjectname, int mode, double tmin, double tmax, std::vector<double> dtime, int interp, int opCtx) override;
    void end_action(int ctx);
    int read_data(int ctx, const char* fieldPath, const char* timeBasePath, void **data, int datatype, int dim, int *size) override;
    void write_data(int ctx, const char* fieldPath, const char* timeBasePath, void *data, int datatype, int dim, int *size) override;
};

extern "C" extended_access_layer_plugin* create() {
  return new PartialGetPlugin;
}

extern "C" void destroy (extended_access_layer_plugin* al_plugin) {
  delete al_plugin;
}

#endif
