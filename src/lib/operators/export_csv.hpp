#pragma once

#include <memory>
#include <string>
#include <vector>

#include "abstract_read_only_operator.hpp"
#include "import_export/csv_writer.hpp"
#include "storage/column_visitable.hpp"
#include "utils/assert.hpp"

namespace opossum {

class ReferenceColumn;

/*
 * With the ExportCsv operator, selected tables of a database
 * can be exported to csv files. A valid input can either be
 * a table from the storage manager or a result of a different operator.
 *
 * Additionally to the main csv file, which contains the contents of the table,
 * a meta file is generated. This meta file contains further information,
 * such as the types of the columns in the table.
 *
 * Note: ExportCsv does not support null values at the moment
 */
class ExportCsv : public AbstractReadOnlyOperator {
 public:
  /*
   * Generates a new ExportCsv operator.
   * @param in          The input for this operator. Must be another operator,
   *                    whose output is used as output for the table. If exporting
   *                    a predefined table is wished, it must first be retrieved with
   *                    the gettable operator.
   * @param filename    Location and name of the output file. Additionally a file called <filename>.meta is created.
   */
  explicit ExportCsv(const std::shared_ptr<const AbstractOperator> in, const std::string& filename);

  // cannot move-assign because of const members
  ExportCsv& operator=(ExportCsv&&) = delete;

  /*
   * Executes the export process.
   * During this process, two files are created: <table_name>.csv and <table_name>.csv.meta
   * Currently, they are both csv files with a comma (,) as delimiter
   * and a quotation mark (") as quotation mark. As escape character, also a quotation mark is used (").
   * This definition is in line with RFC 4180
   *
   *
   * For explanation of the output format, consider the following example:
   * Given table, with name "example", chunk size 100:
   *  a (int) | b (string)            | c (float)
   *  -------------------------------------------
   *    1     | Hallo Welt            |  3.5
   *   102    | Du: sagtest: "Hi!"    |  4.0
   *   NULL   | Kekse                 |  5.0
   *
   * The generated files will look the following:
   *
   *  example.csv
   *
   *  a,b,c
   *  1,"Hallo Welt",3.5
   *  102,"Du sagtest:""Hi!""",4.0
   *  ,"Kekse",5.0
   *
   *  example.csv.meta:
   *
   *  "PropertyType","Key","Value"
   *  "ChunkSize",,100
   *  "ColumnType","a","int_null"
   *  "ColumnType","b","string"
   *  "ColumnType","c","float"
   *
   *  which resembles the following table of meta data:
   *
   *  PropertyType  | Key | Value
   *  ------------------------------
   *  ChunkSize     |     | 100
   *  ColumnType    |  a  | int_null
   *  ColumnType    |  b  | string
   *  ColumnType    |  c  | float
   *
   *  Returns the input table
   */
  std::shared_ptr<const Table> _on_execute() override;

  /*
   * Name of the operator is ExportCsv
   */
  const std::string name() const override;

 private:
  // Name of the output file
  const std::string _filename;

  static void _generate_meta_info_file(const std::shared_ptr<const Table>& table, const std::string& meta_file);
  static void _generate_content_file(const std::shared_ptr<const Table>& table, const std::string& csv_file);

  template <typename T>
  class ExportCsvVisitor;

  struct ExportCsvContext : ColumnVisitableContext {
    explicit ExportCsvContext(CsvWriter& csv_writer) : csv_writer(csv_writer) {}
    CsvWriter& csv_writer;
    ChunkOffset current_row;
  };
};
}  // namespace opossum
