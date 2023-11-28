#include "camera_plugin.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "simple_logger.h"

Camera_plugin::Camera_plugin()
{
}

Camera_plugin::~Camera_plugin()
{
}

std::string Camera_plugin::getName() {
    return "camera";
}

std::string Camera_plugin::getCommit() {
    return "";
}

std::string Camera_plugin::getVersion() {
    return "1.0.0";
}

std::string Camera_plugin::getRepository() {
    return "ssh://git@git.iter.org/imas/access-layer-plugins.git";
}

std::string Camera_plugin::getParameters() {
    return "";
}

std::string Camera_plugin::getReadbackName(const std::string &path, int* index) {
    return "";
}

std::string Camera_plugin::getReadbackCommit(const std::string &path) {
    return "";
}

std::string Camera_plugin::getReadbackVersion(const std::string &path) {
    return "";
}

std::string Camera_plugin::getReadbackRepository(const std::string &path) {
    return "";
}

std::string Camera_plugin::getReadbackParameters(const std::string &path) {
    return "";
}

plugin::OPERATION Camera_plugin::node_operation(const std::string &path) {
    return plugin::OPERATION::PUT_ONLY;
}

void Camera_plugin::setParameter(const char* parameter_name, int datatype, int dim, int *size, void *data) {
}

void Camera_plugin::begin_global_action(int pulseCtx, const char* dataobjectname, const char* datapath, int mode, int opCtx) {
}

void Camera_plugin::begin_slice_action(int pulseCtx, const char* dataobjectname, int mode, double time, int interp, int opCtx) {
}

void Camera_plugin::begin_arraystruct_action(int ctx, int *aosctx, const char* fieldPath, const char* timeBasePath, int *arraySize) {
    LLenv lle = Lowlevel::getLLenv(ctx);
    ArraystructContext* actx = lle.create(fieldPath, timeBasePath);
    *aosctx = Lowlevel::addLLenv(lle.backend, actx); 
    *arraySize = 3;
    printf("Camera_plugin::sizing AOS with path=%s to %d...\n", actx->getPath().c_str(), *arraySize);
    lle.backend->beginArraystructAction(actx, arraySize); //calling backend  
}

int Camera_plugin::read_data(int ctx, const char* fieldPath, const char* timeBasePath, 
                void **data, int datatype, int dim, int *size) {
    return 0; 
}


void Camera_plugin::write_data(int ctx, const char* fieldPath, const char* timeBasePath, void *data, int datatype, int dim, int *size) {
    std::string field = fieldPath;
    if (field == "name") {
        LLenv lle = Lowlevel::getLLenv(ctx);
        ArraystructContext *arctx = dynamic_cast<ArraystructContext *>(lle.context);
        printf("Camera_plugin::writing the name of the flux_loop %d\n", arctx->getIndex());
        std::string flux_loop_name = "loop_" +  std::to_string(arctx->getIndex());
        void* ptrData = (void *) (flux_loop_name.c_str());
        int arrayOfSizes[1] = {(int)flux_loop_name.size()};
        al_plugin_write_data(ctx, fieldPath, timeBasePath, ptrData, CHAR_DATA, 1, arrayOfSizes);
    }
    else {
        std::string message = std::string("Unexpected call to camera_plugin. Writing node '") + field +  std::string("' is not supported.");
        throw ALPluginException(message.c_str());
    }
        
}

void Camera_plugin::end_action(int ctx) {}

