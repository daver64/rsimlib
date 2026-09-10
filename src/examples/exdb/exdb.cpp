#include "sl.h"

#include "rdb.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace
{
	void remove_database(const std::string &filename)
	{
		std::error_code error;
		if (!std::filesystem::remove(filename, error) && error)
		{
			std::fprintf(stderr, "Unable to remove temporary database '%s': %s\n",
				filename.c_str(), error.message().c_str());
		}
	}

	struct Product
	{
		int id = 0;
		std::string name;
		std::string category;
		double price = 0.0;
		int stock = 0;
	};

	std::vector<Product> create_database(const std::string &filename)
	{
		rdb::Database database(filename);
		database.execute("DROP TABLE IF EXISTS products;");
		database.execute(
			"CREATE TABLE products ("
			"id INTEGER PRIMARY KEY, "
			"name TEXT NOT NULL, "
			"category TEXT NOT NULL, "
			"price REAL NOT NULL, "
			"stock INTEGER NOT NULL"
			");");

		const std::vector<Product> products = {
			{0, "Copper mug", "Kitchen", 18.50, 24},
			{0, "Canvas backpack", "Travel", 64.00, 11},
			{0, "Desk lamp", "Office", 39.95, 8},
			{0, "Field notebook", "Stationery", 7.25, 42},
			{0, "Wool blanket", "Home", 82.00, 5},
			{0, "Wireless mouse", "Office", 29.99, 17},
		};

		rdb::Database::Transaction transaction(database);
		auto insert = database.prepare(
			"INSERT INTO products (name, category, price, stock) VALUES (?, ?, ?, ?);");
		for (const Product &product : products)
		{
			insert->bind(1, product.name);
			insert->bind(2, product.category);
			insert->bind(3, product.price);
			insert->bind(4, product.stock);
			insert->step();
			insert->reset();
		}
		transaction.commit();

		std::vector<Product> rows;
		auto select = database.prepare(
			"SELECT id, name, category, price, stock FROM products ORDER BY id;");
		select->forEachRow([&rows](rdb::Statement &row)
		{
			rows.push_back({row.getInt(0), row.getText(1), row.getText(2),
				row.getDouble(3), row.getInt(4)});
		});
		return rows;
	}

	void process_input(bool &running)
	{
		sl::Event event;
		while (sl::poll_event(&event))
		{
			if (event.type() == sl::Event::Type::quit ||
				(event.type() == sl::Event::Type::key_down &&
				 event.key() == sl::Event::Key::escape))
			{
				running = false;
			}
			sl::display_handle_event(event);
		}
	}

	void draw_table(const std::string &filename, const std::vector<Product> &products)
	{
		sl::clear_to_colour(sl::screen, {24, 29, 38});
		const sl::Colour heading{235, 220, 155};
		const sl::Colour text{218, 226, 235};
		const sl::Colour muted{145, 160, 178};

		sl::gprintf(32, 24, heading, "SQLite database example");
		sl::gprintf(32, 48, muted, "File: %s", filename.c_str());
		sl::gprintf(32, 72, muted, "Rows: %d    Press Escape to exit", static_cast<int>(products.size()));
		sl::gprintf(32, 116, heading, "ID   NAME                  CATEGORY       PRICE     STOCK");
		sl::gprintf(32, 134, muted, "----------------------------------------------------------");

		int y = 158;
		for (const Product &product : products)
		{
			sl::gprintf(32, y, text, "%-4d %-21s %-14s $%7.2f %5d",
				product.id, product.name.c_str(), product.category.c_str(),
				product.price, product.stock);
			y += 24;
		}

		sl::show_video_bitmap();
		sl::end_frame();
	}
}

int main(int argc, char *argv[])
{
	constexpr const char *database_filename = "exdb.sqlite";
	std::vector<Product> products;
	try
	{
		products = create_database(database_filename);
	}
	catch (const rdb::SQLiteException &exception)
	{
		std::fprintf(stderr, "Database error: %s\n", exception.what());
		remove_database(database_filename);
		return -1;
	}

	if (!sl::configure_graphics_backend_from_args(argc, argv) ||
		!sl::set_gfx_mode(sl::GFX_AUTODETECT_WINDOWED, 900, 420))
	{
		remove_database(database_filename);
		return -1;
	}

	bool running = true;
	while (running)
	{
		process_input(running);
		draw_table(database_filename, products);
	}

	sl::shutdown();
	remove_database(database_filename);
	return 0;
}

