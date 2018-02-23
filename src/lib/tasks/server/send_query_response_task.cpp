#include "send_query_response_task.hpp"

#include "server/postgres_wire_handler.hpp"
#include "sql/sql_pipeline.hpp"

#include "SQLParserResult.h"

namespace opossum {

std::vector<ColumnDescription> SendQueryResponseTask::build_row_description(const std::shared_ptr<const Table> table) {
  std::vector<ColumnDescription> result;

  const auto& column_names = table->column_names();
  const auto& column_types = table->column_types();

  for (auto column_id = 0u; column_id < table->column_count(); ++column_id) {
    switch (column_types[column_id]) {
      case DataType::Int:
        result.emplace_back(ColumnDescription{column_names[column_id], /* object_id */ 23, /* type id */ 4});
        break;
      case DataType::Long:
        result.emplace_back(ColumnDescription{column_names[column_id], /* object_id */ 20, /* type id */ 8});
        break;
      case DataType::Float:
        result.emplace_back(ColumnDescription{column_names[column_id], /* object_id */ 700, /* type id */ 4});
        break;
      case DataType::Double:
        result.emplace_back(ColumnDescription{column_names[column_id], /* object_id */ 701, /* type id */ 8});
        break;
      case DataType::String:
        result.emplace_back(ColumnDescription{column_names[column_id], /* object_id */ 25, /* type id */ -1});
        break;
      default:
        Fail("Bad DataType");
    }
  }

  return result;
}

std::string SendQueryResponseTask::build_command_complete_message(hsql::StatementType statement_type,
                                                                  uint64_t row_count) {
  switch (statement_type) {
    case hsql::StatementType::kStmtSelect: {
      return "SELECT " + std::to_string(row_count);
    }
    case hsql::StatementType::kStmtInsert: {
      // 0 is ignored OID and 1 inserted row
      return "INSERT 0 1";
    }
    case hsql::StatementType::kStmtUpdate: {
      // We do not return how many rows are affected
      return "UPDATE 0";
    }
    case hsql::StatementType::kStmtDelete: {
      // We do not return how many rows are affected
      return "DELETE 0";
    }
    case hsql::StatementType::kStmtCreate: {
      // 0 rows retrieved (Postgres requires a CREATE TABLE statement to return SELECT)
      return "SELECT 0";
    }
    default: { throw std::logic_error("Unknown statement type. Server doesn't know how to complete query."); }
  }
}

std::string SendQueryResponseTask::build_execution_info_message(std::shared_ptr<SQLPipeline> sql_pipeline) {
  return "Compilation time (µs): " + std::to_string(sql_pipeline->compile_time_microseconds().count()) +
         "\nExecution time (µs): " + std::to_string(sql_pipeline->execution_time_microseconds().count());
}

void SendQueryResponseTask::_on_execute() {
  try {
    auto row_count = 0u;
    /*
    DataRow (B)
    Byte1('D')
    Identifies the message as a data row.

    Int32
    Length of message contents in bytes, including self.

    Int16
    The number of column values that follow (possibly zero).

    Next, the following pair of fields appear for each column:

    Int32
    The length of the column value, in bytes (this count does not include itself). Can be zero. As a special case,
    -1 indicates a NULL column value. No value bytes follow in the NULL case.

    Byte n
    The value of the column, in the format indicated by the associated format code. n is the above length.
    */
    const auto num_columns = _result_table->column_count();

    for (ChunkID chunk_id{0}; chunk_id < _result_table->chunk_count(); ++chunk_id) {
      const auto& chunk = _result_table->get_chunk(chunk_id);

      for (ChunkOffset chunk_offset{0}; chunk_offset < chunk->size(); ++chunk_offset) {
        std::vector<std::string> row_strings;
        row_strings.reserve(num_columns);

        for (ColumnID column_id{0}; column_id < num_columns; ++column_id) {
          const auto& column = chunk->get_column(column_id);
          row_strings.emplace_back(type_cast<std::string>((*column)[chunk_offset]));
        }

        ++row_count;

        // The result of this call (and thus possible exceptions) are ignored
        _connection->send_data_row(row_strings);
      }
    }
    _promise.set_value(row_count);
  } catch (std::exception& exception) {
    _promise.set_exception(boost::current_exception());
  }
}

}  // namespace opossum