#pragma once

#include "rdb.h"
#include <sol/sol.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>

namespace rdb
{
    /**
     * @brief Register Lua bindings for rdb into the given Lua state view.
     */
    inline void register_lua(sol::state_view &lua,
                             std::shared_ptr<std::unordered_map<std::string, std::shared_ptr<rdb::Database>>> db_map = nullptr)
    {
        if (!db_map)
        {
            db_map = std::make_shared<std::unordered_map<std::string, std::shared_ptr<rdb::Database>>>();
        }

        sol::table rdb_tbl = lua.create_named_table("rdb");
        rdb_tbl.set_function("connect", [db_map](const std::string &id, const std::string &driver_str, const std::string &conn_str)
        {
            if (id.empty() || db_map->find(id) != db_map->end())
                return false;
            try
            {
                auto db = std::make_shared<rdb::Database>(conn_str);
                db_map->emplace(id, db);
                return true;
            }
            catch (const std::exception &e)
            {
                std::cerr << "rdb.connect exception: " << e.what() << "\n";
                return false;
            }
        });

        rdb_tbl.set_function("disconnect", [db_map](const std::string &id)
        {
            auto it = db_map->find(id);
            if (it == db_map->end()) return false;
            db_map->erase(it);
            return true;
        });

        rdb_tbl.set_function("execute", [db_map](const std::string &id, const std::string &sql)
        {
            auto it = db_map->find(id);
            if (it == db_map->end()) return false;
            try
            {
                it->second->execute(sql);
                return true;
            }
            catch (...)
            {
                return false;
            }
        });

        rdb_tbl.set_function("query", [db_map](sol::this_state state, const std::string &id, const std::string &sql)
        {
            sol::state_view lua_state(state);
            sol::table rows = lua_state.create_table();
            auto it = db_map->find(id);
            if (it == db_map->end()) return rows;
            try
            {
                auto stmt = it->second->prepare(sql);
                if (!stmt) return rows;
                int row_idx = 1;
                while (stmt->step())
                {
                    sol::table row = lua_state.create_table();
                    int col_count = sqlite3_column_count(stmt->get());
                    for (int col = 0; col < col_count; ++col)
                    {
                        const char *name_str = sqlite3_column_name(stmt->get(), col);
                        std::string col_name = name_str ? name_str : "";
                        if (sqlite3_column_type(stmt->get(), col) == SQLITE_NULL)
                        {
                            row[col_name] = sol::nil;
                        }
                        else if (sqlite3_column_type(stmt->get(), col) == SQLITE_INTEGER)
                        {
                            row[col_name] = stmt->getInt(col);
                        }
                        else if (sqlite3_column_type(stmt->get(), col) == SQLITE_FLOAT)
                        {
                            row[col_name] = stmt->getDouble(col);
                        }
                        else
                        {
                            row[col_name] = stmt->getText(col);
                        }
                    }
                    rows[row_idx++] = row;
                }
                return rows;
            }
            catch (...)
            {
                return rows;
            }
        });
    }
} // namespace rdb
