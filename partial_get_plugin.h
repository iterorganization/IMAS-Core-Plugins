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

    bool display_only_debug_read_requests;
    int operation; 

    void splitQueryIntoSubQueries(const std::string &query, std::vector<std::string> &subqueries);
    void getFullPath(int ctxID, const char* fieldPath,  std::string &full_path) ;
    void getAOSPaths(ArraystructContext *c, std::string &full_path, bool array_only=false);

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
