#ifndef PARTIAL_GET_PLUGIN_H
#define PARTIAL_GET_PLUGIN_H 1

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <iostream>

#include "access_layer_plugin.h"


class PartialGetPlugin: public access_layer_plugin
{

  private:
    std::vector<std::string> query_includes;
    std::vector<std::string> query_excludes;
    std::map<std::string, std::vector<int>> aos_indices; //key = aos path, value = list of requested indices 
    std::map<std::string, std::vector<int>> aos_excludes_indices; //key = aos path, value = list of requested indices for 'excludes' queries
    std::map<std::string, std::string> aos_queries; //key = aos full path, value = AOS query
    std::map<std::string, int> aos_queries_index; //key = aos full path, value = index of query in the 'queries' vector
    std::map<std::string, std::string> aos_excludes_queries; //key = aos full path, value = AOS query
    std::map<std::string, int> aos_excludes_queries_index; //key = aos full path, value = index of query in the 'queries' vector    
    bool display_only_debug_read_requests;

    std::string setAOSQuery(ArraystructContext *actx, const std:: string &query, const std:: string &aos_full_path, int query_index, 
    std::map<std::string, std::string> &aos_queries_, std::map<std::string, int> &aos_queries_index_);
    void buildRequestedAOSIndices(const std::string &aos_name, const std::string &aos_query, const std::string &aos_full_path, 
      int query_index, std::map<std::string, std::vector<int>> &aos_indices, bool includes);
    bool isRequested(const std::vector<int> &requested_indices, int index);
    void getIndices(const std::string &aos_name, const std::string &aos_query, std::vector<int> &indices);
    std::string getAOSName(const char *fieldPath);
    void getAOSIndices(Context * ctx, std::vector < int >&indices, int *timedAOS_index);
    void getAOSPosition(ArraystructContext *actx, int *position);
    void getAOSQuery(ArraystructContext *actx, const std::string &query, std::string &aos_query);
    void getAOSPaths(ArraystructContext *c,  const std::string &aos_name, std::string &rpath, std::string &full_path);
    bool requested_indices(ArraystructContext *arctx);

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

    void setParameter(const char* parameter_name, int datatype, int dim, int *size, void *data) override;
    void begin_arraystruct_action(int ctx, int *aosctx, const char* fieldPath, const char* timeBasePath, int *arraySize) override;
    void begin_global_action(int pulseCtx, const char* dataobjectname, const char* datapath, int mode, int opCtx) override;
    void begin_slice_action(int pulseCtx, const char* dataobjectname, int mode, double time, int interp, int opCtx) override;
    void begin_timerange_action(int pulseCtx, const char* dataobjectname, int mode, double tmin, double tmax, std::vector<double> dtime, int interp, int opCtx) override;
    void end_action(int ctx);
    int read_data(int ctx, const char* fieldPath, const char* timeBasePath, void **data, int datatype, int dim, int *size) override;
    void write_data(int ctx, const char* fieldPath, const char* timeBasePath, void *data, int datatype, int dim, int *size) override;
};

extern "C" access_layer_plugin* create() {
  return new PartialGetPlugin;
}

extern "C" void destroy (access_layer_plugin* al_plugin) {
  delete al_plugin;
}

#endif
