#include "load_server_file_task.hpp"

#include "storage/storage_manager.hpp"
#include "utils/load_table.hpp"

namespace opossum {

void LoadServerFileTask::_on_execute() {
  try {
    const auto table = load_table(_file_name, Chunk::MAX_SIZE);
    StorageManager::get().add_table(_table_name, table);
    //  _session->pipeline_info("Successfully loaded " + _table_name);
    _promise.set_value();
  } catch (const std::exception& exception) {
    _promise.set_exception(exception);
  }
}

}  // namespace opossum
