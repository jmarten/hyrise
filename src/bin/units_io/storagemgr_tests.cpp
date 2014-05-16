// Copyright (c) 2012 Hasso-Plattner-Institut fuer Softwaresystemtechnik GmbH. All rights reserved.
#include "testing/test.h"

#include <io/shortcuts.h>
#include <io/StorageManager.h>
#include <storage/MutableVerticalTable.h>
#include "storage/Store.h"
#include "helper/checked_cast.h"
#include <access/InsertScan.h>
#include <access/tx/Commit.h>
#include "io/TransactionManager.h"
#include <access/MergeTable.h>


namespace hyrise {
namespace io {

class StorageManagerTests : public ::hyrise::Test {

 public:
  StorageManagerTests() { sm = StorageManager::getInstance(); }

  virtual void SetUp() { sm->removeAll(); }

  StorageManager* sm;
};

TEST_F(StorageManagerTests, base) {
  StorageManager* sm2;
  sm2 = StorageManager::getInstance();

  ASSERT_TRUE(sm == sm2);
}

TEST_F(StorageManagerTests, load_table_multiple_contexts) {
  {
    StorageManager* sm = StorageManager::getInstance();
    sm->loadTableFile("LINXXS", "lin_xxs.tbl");
  }

  {
    StorageManager* sm = StorageManager::getInstance();
    ASSERT_TRUE((bool)sm->getTable("LINXXS"));
    hyrise::storage::atable_ptr_t t = sm->getTable("LINXXS");
  }

  {
    StorageManager* sm = StorageManager::getInstance();
    sm->removeTable("LINXXS");
  }
  StorageManager* sm = StorageManager::getInstance();

  ASSERT_EQ(0u, sm->getTableNames().size());

  sm->removeAll();
}

TEST_F(StorageManagerTests, load_table) {
  sm->loadTableFile("LINXXS", "lin_xxs.tbl");

  hyrise::storage::atable_ptr_t tbl = sm->getTable("LINXXS");
  hyrise::storage::atable_ptr_t ref = Loader::shortcuts::load("test/lin_xxs.tbl");
  ASSERT_TRUE(tbl->contentEquals(ref));
  sm->removeTable("LINXXS");
  ASSERT_EQ(0u, sm->getTableNames().size());
}

TEST_F(StorageManagerTests, load_table_header_data) {
  sm->loadTableFileWithHeader("HEADERDATA", "header/data.tbl", "header/header.data");

  hyrise::storage::atable_ptr_t tbl = sm->getTable("HEADERDATA");
  hyrise::storage::atable_ptr_t ref =
      Loader::shortcuts::loadWithHeader("test/header/data.tbl", "test/header/header.data");

  ASSERT_TRUE(tbl->contentEquals(ref));

  sm->removeTable("HEADERDATA");

  ASSERT_EQ(0u, sm->getTableNames().size());
}

TEST_F(StorageManagerTests, load_persist_and_recover_table) {

  sm->loadTableFile("LINXXS", "lin_xxs.tbl");
  ASSERT_TRUE(sm->exists("LINXXS"));
  sm->persistTable("LINXXS");
  sm->removeTable("LINXXS");
  ASSERT_FALSE(sm->exists("LINXXS"));
  sm->recoverTable("LINXXS");
  ASSERT_TRUE(sm->exists("LINXXS"));
}

TEST_F(StorageManagerTests, persist_and_recover_table_with_delta) {
  auto rows = Loader::shortcuts::load("test/alltypes.tbl");
  auto orig = Loader::shortcuts::load("test/alltypes.tbl");
  auto expected = Loader::shortcuts::load("test/alltypes.tbl");

  const std::string tablename = "DUMP_TEST";
  auto sm = StorageManager::getInstance();

  orig->setName(tablename);
  sm->add(tablename, orig);

  // Make sure, that table is in MAIN
  access::MergeTable mt;
  mt.addInput(sm->getTable("DUMP_TEST"));
  mt.execute();

  // insert some rows, which remain in the delta.
  access::InsertScan is;
  auto ctx = tx::TransactionManager::getInstance().buildContext();
  is.setTXContext(ctx);
  is.addInput(orig);
  is.setInputData(rows);
  is.execute();
  access::Commit c;
  c.addInput(is.getResultTable());
  c.setTXContext(ctx);
  c.execute();

  // dump table with delta to disk and delete it from StorageManager
  sm->persistTable(tablename, "", true);
  sm->removeTable(tablename);
  ASSERT_FALSE(sm->exists(tablename));

  // load table back into StorageManager
  sm->loadTableWithDelta(tablename, "");
  ASSERT_TRUE(sm->exists(tablename));

  // Compare resulting table.
  auto table = sm->getTable(tablename);
  auto store = std::dynamic_pointer_cast<hyrise::storage::Store>(table);
  ASSERT_EQ(store->size(), 8);
  ASSERT_EQ(store->getDeltaTable()->size(), 4);
  ASSERT_EQ(store->getMainTable()->size(), 4);
  ASSERT_EQ(store->columnCount(), orig->columnCount());
  
  ASSERT_TABLE_EQUAL(store, orig);
  ASSERT_TABLE_EQUAL(store->getMainTable(), expected);
  ASSERT_TABLE_EQUAL(store->getDeltaTable(), expected);

  sm->removeTable(tablename);
}

}
}  // namespace hyrise::io
