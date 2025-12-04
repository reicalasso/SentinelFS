#pragma once

/**
 * @file Commands.h
 * @brief Convenience header that includes all IPC command handlers
 * 
 * This file provides a single include point for all command handler classes.
 * Include this in IPCHandler.cpp to access all command functionality.
 */

#include "CommandHandler.h"
#include "StatusCommands.h"
#include "PeerCommands.h"
#include "ConfigCommands.h"
#include "FileCommands.h"
#include "TransferCommands.h"
#include "RelayCommands.h"
