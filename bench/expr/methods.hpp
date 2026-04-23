#pragma once

#include "method_interface.hpp"
#include "method_hardcoded.hpp"
#include "method_plain_loop4.hpp"
#include "method_chunk_pipeline.hpp"
#include "method_graph_fused.hpp"
#include "method_vm.hpp"
#ifdef _WIN32
#include "method_opencl_dynamic.hpp"
#endif
