/**
 * Copyright (c) 2024 Accenture
 */
#include "mcf_core/PluginLoader.h"

#include "mcf_core/LoggingMacros.h"

#include <algorithm>
#include <dlfcn.h>
#include <iostream>

namespace mcf
{

PluginLoader::PluginLoader() = default;

void PluginLoader::unloadHandle(void* handle)
{
    if (handle != nullptr)
    {
        int error = dlclose(handle);
        if (error != 0)
        {
            MCF_ERROR("Could not close shared object: {}", dlerror());
        }
    }
}

Plugin PluginLoader::load(const std::string& fileName)
{
    // check if the plugin has been already loaded
    const auto preLoaded = std::find_if(_pluginDescs.begin(),
                                        _pluginDescs.end(),
                                        [fileName](const PluginDescriptor& p)
                                        { return (p.filePath == fileName); });
    if (preLoaded != _pluginDescs.end())
    {
        // in this case, unload the library and then load it again
        auto handle = preLoaded->sharedObjectHandle;
        _pluginDescs.erase(preLoaded);
        unloadHandle(handle);
    }

    void* handle = dlopen(fileName.c_str(), RTLD_NOW);

    if (handle == nullptr)
    {
        throw PluginError(dlerror());
    }

    dlerror(); // clear any existing error

    // NOTE: Linux does not offer a "public" API for listing all symbols in an object
    void* symbol = dlsym(handle, PLUGIN_ENTRY_POINT);
    char* error = dlerror();

    if (error != nullptr)
    {
        throw PluginError(error);
    }

    // cast to function pointer
    auto (*func)() = reinterpret_cast<Plugin (*)()>(symbol);
    Plugin plugin    = func();

    _pluginDescs.push_back({fileName, handle, plugin});
    return plugin;
}

PluginLoader::~PluginLoader()
{
    MCF_INFO_NOFILELINE("Unloading libraries ...");

    // keep names and handles of libraries to be unloaded
    std::vector<void*> handles;
    std::vector<std::string> names;

    for (const auto& descriptor : _pluginDescs)
    {
        // TODO: clean this up, because it may throw an exception (in the destructor)
        names.push_back(descriptor.filePath);
        handles.push_back(descriptor.sharedObjectHandle);
    }

    // remove plugin descriptors
    // (Plugin instances must be deleted before unloading the shared lib, otherwise the
    // factory method in it will dangle and may cause a segfault when being destroyed.)
    MCF_INFO_NOFILELINE("... clearing plugin descriptors");
    _pluginDescs.clear();

    // unload all plugins, in opposite order of loading
    for (ssize_t i = names.size() - 1; i >= 0; --i)
    {
        MCF_INFO_NOFILELINE( "... unloading {}", names[i]);
        unloadHandle(handles[i]);
    }
    MCF_INFO_NOFILELINE("... done");
}
} // namespace mcf