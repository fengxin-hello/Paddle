add_subdirectory(garbage_collector)
add_subdirectory(interpreter)
add_subdirectory(workqueue)

set(STANDALONE_EXECUTOR_SRCS
    feed_fetch_utils.cc interpretercore.cc new_executor_defs.cc
    standalone_executor.cc program_interpreter.cc new_ir_interpreter.cc)

set(STANDALONE_EXECUTOR_DEPS
    interpreter
    interpretercore_garbage_collector
    workqueue
    pd_dialect
    pd_op_to_kernel_pass
    phi_kernel_adaptor
    program_translator
    ir)

cc_library(
  standalone_executor
  SRCS ${STANDALONE_EXECUTOR_SRCS}
  DEPS ${STANDALONE_EXECUTOR_DEPS})

cc_library(
  staticgraph_executor_statistics
  SRCS executor_statistics.cc
  DEPS enforce glog phi)
