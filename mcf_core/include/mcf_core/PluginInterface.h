/**
 * Copyright (c) 2024 Accenture
 */
#ifndef MCF_PLUGIN_INTERFACE_H
#define MCF_PLUGIN_INTERFACE_H
/**
 * @brief MCF plugin DSO interface.
 *
 * You should only include this file if you are intending to implement a DSO interface to be loaded
 * by MCF plugin system.
 */
#include "mcf_core/Plugin.h"

extern "C" {
mcf::Plugin initializePlugin();
}

#endif // MCF_PLUGIN_INTERFACE_H